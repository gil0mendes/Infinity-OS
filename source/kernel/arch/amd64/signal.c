/*
 * Copyright (C) 2010-2011 Gil Mendes
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
 * @brief		AMD64 signal dispatcher.
 *
 * @todo		Save and restore FPU context.
 */

#include <arch/frame.h>
#include <arch/stack.h>

#include <x86/cpu.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/safe.h>

#include <proc/process.h>
#include <proc/signal.h>

#include <assert.h>
#include <status.h>

/** Signal frame structure. */
typedef struct signal_frame {
	void *retaddr;			/**< Return address. */
	siginfo_t info;			/**< Signal information. */
	ucontext_t context;		/**< Previous context. */
} signal_frame_t;

/** FLAGS values to restore. */
#define RESTORE_FLAGS	(X86_FLAGS_CF | X86_FLAGS_PF | X86_FLAGS_AF \
	| X86_FLAGS_ZF | X86_FLAGS_SF | X86_FLAGS_TF | X86_FLAGS_DF \
	| X86_FLAGS_OF | X86_FLAGS_AC)

/** Set up the user interrupt frame to execute a signal handler.
 * @param action	Signal action.
 * @param info		Signal information.
 * @param mask		Previous signal mask.
 * @return		Status code describing result of the operation. */
status_t arch_signal_setup_frame(sigaction_t *action, siginfo_t *info, sigset_t mask) {
	signal_frame_t frame;
	intr_frame_t *iframe;
	status_t ret;
	ptr_t dest;

	/* Get the user interrupt frame. This is stored upon every entry to the
	 * kernel from user mode in the architecture thread data. */
	iframe = curr_thread->arch.user_iframe;
	assert(iframe->cs & 3);

	/* Work out where to place the frame. */
	if(action->sa_flags & SA_ONSTACK && !(curr_thread->signal_stack.ss_flags & SS_DISABLE)) {
		/* No need to obey the red zone here, this is a dedicated stack
		 * that nothing else should be using. */
		dest = (ptr_t)curr_thread->signal_stack.ss_sp + curr_thread->signal_stack.ss_size;
	} else {
		/* We must not clobber the red zone (128 bytes below the stack
		 * pointer). */
		dest = iframe->sp - 128;
	}

	/* Make space for the frame on the stack and ensure that ABI alignment
	 * requirements are maintained - ((RSP + 8) % 16) == 0 upon entry to
	 * the handler. */
	dest = ROUND_DOWN(dest, 8) - sizeof(signal_frame_t);
	if((dest % 16) == 0)
		dest -= 8;

	/* Set up the frame structure. This is copied onto the user-mode stack
	 * below with memcpy_to_user(). */
	memset(&frame, 0, sizeof(frame));
	memcpy(&frame.info, info, sizeof(frame.info));
	frame.context.uc_sigmask = mask;
	frame.context.uc_stack.ss_sp = (void *)iframe->sp;
	frame.context.uc_stack.ss_size = USTACK_SIZE;
	frame.context.uc_mcontext.rax = iframe->ax;
	frame.context.uc_mcontext.rbx = iframe->bx;
	frame.context.uc_mcontext.rcx = iframe->cx;
	frame.context.uc_mcontext.rdx = iframe->dx;
	frame.context.uc_mcontext.rdi = iframe->di;
	frame.context.uc_mcontext.rsi = iframe->si;
	frame.context.uc_mcontext.rbp = iframe->bp;
	frame.context.uc_mcontext.r8 = iframe->r8;
	frame.context.uc_mcontext.r9 = iframe->r9;
	frame.context.uc_mcontext.r10 = iframe->r10;
	frame.context.uc_mcontext.r11 = iframe->r11;
	frame.context.uc_mcontext.r12 = iframe->r12;
	frame.context.uc_mcontext.r13 = iframe->r13;
	frame.context.uc_mcontext.r14 = iframe->r14;
	frame.context.uc_mcontext.r15 = iframe->r15;
	frame.context.uc_mcontext.rip = iframe->ip;
	frame.context.uc_mcontext.rflags = iframe->flags;
	frame.context.uc_mcontext.rsp = iframe->sp;

	/* Set the return address on the frame. When the handler is installed,
	 * libkernel sets a private field in the sigaction structure pointing
	 * to its wrapper for kern_signal_return(). This solution isn't all
	 * that nice, but it's the best compared to the alternatives:
	 *  - Have the kernel lookup the kern_signal_return symbol in libkernel.
	 *    This is a huge pain in the arse to do.
	 *  - Copy code to call kern_signal_return() onto the stack. This would
	 *    require the stack to be executable.
	 * This method is also what is used by Linux x86_64. */
	frame.retaddr = action->sa_restorer;

	/* Copy across the frame. */
	ret = memcpy_to_user((void *)dest, &frame, sizeof(frame));
	if(ret != STATUS_SUCCESS)
		return ret;

	/* We have definitely succeeded. We can now modify the interrupt frame
	 * to return to the handler. */
	iframe->ip = (ptr_t)action->sa_sigaction;
	iframe->sp = dest;

	/* Pass arguments to the handler. */
	iframe->di = info->si_signo;
	if(action->sa_flags & SA_SIGINFO) {
		iframe->si = dest + offsetof(signal_frame_t, info);
		iframe->dx = dest + offsetof(signal_frame_t, context);
	}

	/* We must return from system calls via the IRET path because we have
	 * modified the frame. */
	curr_thread->arch.flags |= ARCH_THREAD_IFRAME_MODIFIED;
	return STATUS_SUCCESS;
}

/** Restore previous context after returning from a signal handler.
 * @param maskp		Where to store restored signal mask.
 * @return		Status code describing result of the operation. */
status_t arch_signal_restore_frame(sigset_t *maskp) {
	signal_frame_t frame;
	intr_frame_t *iframe;
	status_t ret;

	/* Get the user interrupt frame. */
	iframe = curr_thread->arch.user_iframe;
	assert(iframe->cs & 3);

	/* The stack pointer should point at frame + sizeof(void *) due to the
	 * return address being popped. Copy it back. */
	ret = memcpy_from_user(&frame, (void *)(iframe->sp - sizeof(void *)), sizeof(frame));
	if(ret != STATUS_SUCCESS)
		return ret;

	/* Save the mask from the frame. */
	*maskp = frame.context.uc_sigmask;

	/* Restore the context. */
	iframe->ax = frame.context.uc_mcontext.rax;
	iframe->bx = frame.context.uc_mcontext.rbx;
	iframe->cx = frame.context.uc_mcontext.rcx;
	iframe->dx = frame.context.uc_mcontext.rdx;
	iframe->di = frame.context.uc_mcontext.rdi;
	iframe->si = frame.context.uc_mcontext.rsi;
	iframe->bp = frame.context.uc_mcontext.rbp;
	iframe->r8 = frame.context.uc_mcontext.r8;
	iframe->r9 = frame.context.uc_mcontext.r9;
	iframe->r10 = frame.context.uc_mcontext.r10;
	iframe->r11 = frame.context.uc_mcontext.r11;
	iframe->r12 = frame.context.uc_mcontext.r12;
	iframe->r13 = frame.context.uc_mcontext.r13;
	iframe->r14 = frame.context.uc_mcontext.r14;
	iframe->r15 = frame.context.uc_mcontext.r15;
	iframe->ip = frame.context.uc_mcontext.rip;
	iframe->flags &= ~RESTORE_FLAGS;
	iframe->flags |= frame.context.uc_mcontext.rflags & RESTORE_FLAGS;
	iframe->sp = frame.context.uc_mcontext.rsp;
	return STATUS_SUCCESS;
}
