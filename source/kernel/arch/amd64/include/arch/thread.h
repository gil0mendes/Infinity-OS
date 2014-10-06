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
 * @brief		AMD64-specific thread definitions.
 */

#ifndef __ARCH_THREAD_H
#define __ARCH_THREAD_H

#ifndef __ASM__

#include <types.h>

struct cpu;
struct intr_frame;
struct thread;

/** x86-specific thread structure.
 * @note		The GS register is pointed to the copy of this structure
 *			for the current thread. It is used to access per-CPU
 *			data, and also to easily access per-thread data from
 *			assembly code. If changing the layout of this structure,
 *			be sure to updated the offset definitions below. */
typedef struct __packed arch_thread {
	struct cpu *cpu;			/**< Current CPU pointer, for curr_cpu. */
	struct thread *parent;			/**< Pointer to containing thread, for curr_thread. */

	/** SYSCALL/SYSRET data. */
	ptr_t kernel_rsp;			/**< RSP for kernel entry via SYSCALL. */
	ptr_t user_rsp;				/**< Temporary storage for user RSP. */

	/** Saved context switch stack pointer. */
	ptr_t saved_rsp;

	struct intr_frame *user_iframe;		/**< Frame from last user-mode entry. */
	unsigned long flags;			/**< Flags for the thread. */
	ptr_t tls_base;				/**< TLS base address. */

	/** Number of consecutive runs that the FPU is used for. */
	unsigned fpu_count;

	/** FPU context save point. */
	char fpu[512] __aligned(16);
} arch_thread_t;

/** Get the current thread structure pointer.
 * @return		Pointer to current thread structure. */
static inline struct thread *arch_curr_thread(void) {
	struct thread *addr;
	__asm__("mov %%gs:8, %0" : "=r"(addr));
	return addr;
}

#endif /* __ASM__ */

/** Flags for arch_thread_t. */
#define ARCH_THREAD_IFRAME_MODIFIED	(1<<0)	/**< Interrupt frame was modified. */
#define ARCH_THREAD_HAVE_FPU		(1<<1)	/**< Thread has an FPU state saved. */
#define ARCH_THREAD_FREQUENT_FPU	(1<<2)	/**< FPU is frequently used by the thread. */

/** Offsets in arch_thread_t. */
#define ARCH_THREAD_OFF_KERNEL_RSP	0x10
#define ARCH_THREAD_OFF_USER_RSP	0x18
#define ARCH_THREAD_OFF_USER_IFRAME	0x28
#define ARCH_THREAD_OFF_FLAGS		0x30

#endif /* __ARCH_THREAD_H */
