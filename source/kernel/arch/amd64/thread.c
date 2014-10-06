/*
 * Copyright (C) 2009-2014 Gil Mendes
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		AMD64 thread functions.
 */

#include <arch/frame.h>
#include <arch/stack.h>

#include <x86/cpu.h>
#include <x86/descriptor.h>
#include <x86/fpu.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/aspace.h>
#include <mm/safe.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <assert.h>
#include <cpu.h>
#include <status.h>

/** FLAGS values to restore. */
#define RESTORE_FLAGS	(X86_FLAGS_CF | X86_FLAGS_PF | X86_FLAGS_AF \
	| X86_FLAGS_ZF | X86_FLAGS_SF | X86_FLAGS_TF | X86_FLAGS_DF \
	| X86_FLAGS_OF | X86_FLAGS_AC)

extern void amd64_context_switch(ptr_t new_rsp, ptr_t *old_rsp);
extern void amd64_context_restore(ptr_t new_rsp);

/** Initialize AMD64-specific thread data.
 * @param thread	Thread to initialize.
 * @param stack		Base of the kernel stack for the thread.
 * @param entry		Entry point for the thread. */
void arch_thread_init(thread_t *thread, void *stack, void (*entry)(void)) {
	unsigned long *sp;

	thread->arch.parent = thread;
	thread->arch.flags = 0;
	thread->arch.tls_base = 0;
	thread->arch.fpu_count = 0;

	/* Point the RSP for SYSCALL entry at the top of the stack. */
	thread->arch.kernel_rsp = (ptr_t)stack + KSTACK_SIZE;

	/* Initialize the kernel stack. First value is a fake return address to
	 * make the backtrace end correctly and maintain ABI alignment
	 * requirements: ((RSP - 8) % 16) == 0 on entry to a function. */
	sp = (unsigned long *)thread->arch.kernel_rsp;
	*--sp = 0;			/* Fake return address for backtrace. */
	*--sp = (ptr_t)entry;		/* RIP/Return address. */
	*--sp = 0;			/* RBP. */
	*--sp = 0;			/* RBX. */
	*--sp = 0;			/* R12. */
	*--sp = 0;			/* R13. */
	*--sp = 0;			/* R14. */
	*--sp = 0;			/* R15. */

	/* Save the stack pointer for arch_thread_switch(). */
	thread->arch.saved_rsp = (ptr_t)sp;
}

/** Clean up AMD64-specific thread data.
 * @param thread	Thread to clean up. */
void arch_thread_destroy(thread_t *thread) {
	/* Nothing happens. */
}

/** Clone the current thread.
 * @param thread	New thread to clone into.
 * @param frame		Frame to prepare for new thread to enter user mode with
 *			arch_thread_user_enter(). */
void arch_thread_clone(thread_t *thread, intr_frame_t *frame) {
	thread->arch.flags = curr_thread->arch.flags & ARCH_THREAD_HAVE_FPU;
	thread->arch.tls_base = curr_thread->arch.tls_base;

	if(x86_fpu_state()) {
		/* FPU is currently enabled so the latest state may not have
		 * been saved. */
		x86_fpu_save(thread->arch.fpu);
	} else if(curr_thread->flags & ARCH_THREAD_HAVE_FPU) {
		memcpy(thread->arch.fpu, curr_thread->arch.fpu, sizeof(thread->arch.fpu));
	}

	/* Duplicate the user interrupt frame. This should be valid as we
	 * only get here via a system call. */
	memcpy(frame, curr_thread->arch.user_iframe, sizeof(*frame));

	/* The new thread should return success from the system call. */
	frame->ax = STATUS_SUCCESS;
}

/** Switch to another thread.
 * @param thread	Thread to switch to.
 * @param prev		Thread that was previously running. */
void arch_thread_switch(thread_t *thread, thread_t *prev) {
	bool fpu_enabled;

	/* Save the current FPU state, if any. */
	fpu_enabled = x86_fpu_state();
	if(likely(prev)) {
		if(fpu_enabled) {
			x86_fpu_save(prev->arch.fpu);
		} else {
			prev->arch.fpu_count = 0;
		}
	}

	/* Store the current CPU pointer and then point the GS register to the
	 * new thread's architecture data. The load of curr_cpu will load from
	 * the previous thread's architecture data. */
	thread->arch.cpu = curr_cpu;
	x86_write_msr(X86_MSR_GS_BASE, (ptr_t)&thread->arch);

	/* Set the RSP0 field in the TSS to point to the new thread's
	 * kernel stack. */
	curr_cpu->arch.tss.rsp0 = thread->arch.kernel_rsp;

	/* Set the FS base address to the TLS segment base. */
	x86_write_msr(X86_MSR_FS_BASE, thread->arch.tls_base);

	/* Handle the FPU state. */
	if(thread->arch.flags & ARCH_THREAD_FREQUENT_FPU) {
		/* The FPU is being frequently used by the new thread, load the
		 * new state immediately so that the thread doesn't have to
		 * incur a fault before it can use the FPU again. */
		if(!fpu_enabled)
			x86_fpu_enable();

		x86_fpu_restore(thread->arch.fpu);
	} else {
		/* Disable the FPU. We switch the FPU state on demand in the
		 * new thread, to remove the overhead of loading it now when
		 * it is not likely that the FPU will be needed by the thread. */
		if(fpu_enabled)
			x86_fpu_disable();
	}

	/* Switch to the new context. */
	if(likely(prev)) {
		amd64_context_switch(thread->arch.saved_rsp, &prev->arch.saved_rsp);
	} else {
		/* Initial thread switch, don't have a previous thread. */
		amd64_context_restore(thread->arch.saved_rsp);
	}
}

/** Set the TLS address for the current thread.
 * @param addr		TLS address. */
void arch_thread_set_tls_addr(ptr_t addr) {
	/* The AMD64 ABI uses the FS segment register to access the TLS data.
	 * Save the address to be written to the FS base upon each thread
	 * switch. */
	curr_thread->arch.tls_base = addr;
	x86_write_msr(X86_MSR_FS_BASE, addr);
}

/** Prepare an interrupt frame to enter user mode.
 * @param frame		Frame to prepare.
 * @param entry		Entry function.
 * @param sp		Stack pointer.
 * @param arg		First argument to function. */
void arch_thread_user_setup(intr_frame_t *frame, ptr_t entry, ptr_t sp, ptr_t arg) {
	/* Correctly align the stack pointer for ABI requirements. */
	sp -= sizeof(unsigned long);

	/* Clear out the frame to zero all GPRs. */
	memset(frame, 0, sizeof(*frame));

	frame->di = arg;
	frame->ip = entry;
	frame->cs = USER_CS | 0x3;
	frame->flags = X86_FLAGS_IF | X86_FLAGS_ALWAYS1;
	frame->sp = sp;
	frame->ss = USER_DS | 0x3;
}

/** Prepare to execute a user mode interrupt.
 * @param interrupt	Interrupt to execute.
 * @param ipl		Previous IPL.
 * @return		Status code describing result of the operation. */
status_t arch_thread_interrupt_setup(thread_interrupt_t *interrupt, unsigned ipl) {
	intr_frame_t *frame;
	ptr_t data_addr, state_addr, ret_addr;
	thread_state_t state;
	status_t ret;

	frame = curr_thread->arch.user_iframe;
	assert(frame->cs & 3);

	/* Work out where to place stuff on the user stack. We must not clobber
	 * the red zone (128 bytes below the stack pointer). Ensure that we
	 * satisfy ABI constraints - ((RSP + 8) % 16) == 0 upon entry to the
	 * handler. */
	data_addr = round_down(frame->sp - 128 - interrupt->size, 16);
	state_addr = round_down(data_addr - sizeof(state), 16);
	ret_addr = state_addr - 8;

	if(interrupt->size) {
		/* Copy interrupt data. */
		ret = memcpy_to_user((void *)data_addr, interrupt + 1, interrupt->size);
		if(ret != STATUS_SUCCESS)
			return ret;
	}

	/* Save the thread state. TODO: FPU context. */
	state.context.rax = frame->ax;
	state.context.rbx = frame->bx;
	state.context.rcx = frame->cx;
	state.context.rdx = frame->dx;
	state.context.rdi = frame->di;
	state.context.rsi = frame->si;
	state.context.rbp = frame->bp;
	state.context.rsp = frame->sp;
	state.context.r8 = frame->r8;
	state.context.r9 = frame->r9;
	state.context.r10 = frame->r10;
	state.context.r11 = frame->r11;
	state.context.r12 = frame->r12;
	state.context.r13 = frame->r13;
	state.context.r14 = frame->r14;
	state.context.r15 = frame->r15;
	state.context.rflags = frame->flags;
	state.context.rip = frame->ip;
	state.ipl = ipl;

	ret = memcpy_to_user((void *)state_addr, &state, sizeof(state));
	if(ret != STATUS_SUCCESS)
		return ret;

	/* Write return address. */
	ret = write_user((ptr_t *)ret_addr, curr_proc->thread_restore);
	if(ret != STATUS_SUCCESS)
		return ret;

	/* Modify the interrupt frame to return to the handler. */
	frame->ip = interrupt->handler;
	frame->sp = ret_addr;
	frame->di = data_addr;
	frame->si = state_addr;

	/* We must return from system calls via the IRET path because we have
	 * modified the frame. */
	curr_thread->arch.flags |= ARCH_THREAD_IFRAME_MODIFIED;
	return STATUS_SUCCESS;
}

/** Restore previous state after returning from a user mode interrupt.
 * @param iplp		Where to store previous IPL.
 * @return		Status code describing result of the operation. */
status_t arch_thread_interrupt_restore(unsigned *iplp) {
	intr_frame_t *frame;
	thread_state_t state;
	status_t ret;

	frame = curr_thread->arch.user_iframe;
	assert(frame->cs & 3);

	/* The stack pointer should point at the state structure due to the
	 * return address being popped. Copy it back. */
	ret = memcpy_from_user(&state, (void *)frame->sp, sizeof(state));
	if(ret != STATUS_SUCCESS)
		return ret;

	/* Save the IPL to restore. */
	*iplp = state.ipl;

	/* Restore the context. */
	frame->ax = state.context.rax;
	frame->bx = state.context.rbx;
	frame->cx = state.context.rcx;
	frame->dx = state.context.rdx;
	frame->di = state.context.rdi;
	frame->si = state.context.rsi;
	frame->bp = state.context.rbp;
	frame->sp = state.context.rsp;
	frame->r8 = state.context.r8;
	frame->r9 = state.context.r9;
	frame->r10 = state.context.r10;
	frame->r11 = state.context.r11;
	frame->r12 = state.context.r12;
	frame->r13 = state.context.r13;
	frame->r14 = state.context.r14;
	frame->r15 = state.context.r15;
	frame->flags &= ~RESTORE_FLAGS;
	frame->flags |= state.context.rflags & RESTORE_FLAGS;
	frame->ip = state.context.rip;

	/* Same as above. */
	curr_thread->arch.flags |= ARCH_THREAD_IFRAME_MODIFIED;
	return STATUS_SUCCESS;
}
