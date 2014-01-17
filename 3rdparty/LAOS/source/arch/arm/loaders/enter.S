/*
 * Copyright (C) 2012-2013 Gil Mendes
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file
 * @brief		ARM kernel entry code.
 */

#include <arm/asm.h>
#include <laos.h>

#if CONFIG_LAOS_LOADER_LAOS

#define ENTRY_ARGS_TRANSITION_TTBR	0
#define ENTRY_ARGS_VIRT			4
#define ENTRY_ARGS_KERNEL_TTBR		8
#define ENTRY_ARGS_SP			12
#define ENTRY_ARGS_ENTRY		16
#define ENTRY_ARGS_TAGS			20
#define ENTRY_ARGS_TRAMPOLINE		24

#if __ARM_ARCH >= 7
# define ISB	isb
#else
# define ISB	mcr	p15, 0, r0, c7, c5, 4
#endif

/** Enter an ARM LAOS kernel.
 * @param args		Entry arguments structure. */
FUNCTION_START(laos_arch_enter32)
	/* Ensure that IRQs/FIQs are disabled. */
	cpsid	if

	/* Write the transition L1 table address to TTBR0. */
	ldr	r4, [r0, #ENTRY_ARGS_TRANSITION_TTBR]
	mcr	p15, 0, r4, c2, c0, 0

	/* Ensure that TTBCR is configured to always use TTBR0. */
	mov	r4, #0
	mcr	p15, 0, r4, c2, c0, 2

	/* Enable full access to all domains. */
	mvn	r4, #0
	mcr	p15, 0, r4, c3, c0, 0

	/* Enable the I/D caches (SCTLR.I and SCTLR.C). */
	mrc	p15, 0, r4, c1, c0, 0
	orr	r4, r4, #(1<<12)
	orr	r4, r4, #(1<<2)
	mcr	p15, 0, r4, c1, c0, 0

	/* Invalidate I/D TLB. */
	mcr	p15, 0, r0, c8, c7, 0

	/* Enable the MMU by setting SCTLR.M to 1. */
	orr	r4, r4, #(1<<0)
	mcr	p15, 0, r4, c1, c0, 0
	ISB

	/* Calculate the trampoline address. */
	ldr	r4, [r0, #ENTRY_ARGS_VIRT]
	add	r4, r4, #ENTRY_ARGS_TRAMPOLINE
	mov	pc, r4

SYMBOL(laos_trampoline32)
	/* Write the main L1 table address to TTBR0. */
	ldr	r4, [r0, #ENTRY_ARGS_KERNEL_TTBR]
	mcr	p15, 0, r4, c2, c0, 0
	ISB

	/* Set the stack pointer. */
	ldr	sp, [r0, #ENTRY_ARGS_SP]

	/* Clear the stack frame pointer (R11). */
	mov	fp, #0

	/* Call the kernel. */
	ldr	r4, [r0, #ENTRY_ARGS_ENTRY]
	ldr	r1, [r0, #ENTRY_ARGS_TAGS]
	ldr	r0, =LAOS_MAGIC
	mov	pc, r4
1:	b	1b

SYMBOL(laos_trampoline32_size)
	.long	. - laos_trampoline32 + 8
FUNCTION_END(laos_arch_enter32)

#endif /* CONFIG_LAOS_LOADER_LAOS */
