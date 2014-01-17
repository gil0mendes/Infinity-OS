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
 * @brief		OMAP3 loader entry point.
 */

#include <arch/page.h>

#include <arm/asm.h>

#include <platform/loader.h>

/* Ensure that this code is linked in first. */
.section .init.text, "ax", %progbits

/** Main entry point for the loader. */
FUNCTION_START(_start)
	/* We are loaded using the ARM Linux boot procedure.
	 * Processor state upon entry:
	 *  - SVC (supervisor) mode with IRQ and FIQ interrupts disabled.
	 *  - MMU off.
	 *  - Data cache off.
	 *  - Instruction cache on or off.
	 * Registers:
	 *  R0 - 0
	 *  R1 - Machine type.
	 *  R2 - Physical address of ATAG list.
	 */

	/* Load the stack address. We don't care about setting stacks for other
	 * modes here, we shouldn't get any exceptions or interrupts while
	 * we're in the loader. */
	ldr	sp, =loader_stack + PAGE_SIZE

	/* Save the ATAG list pointer (R4 is callee-save). */
	mov	r4, r2

	/* Clear out the BSS section. */
	ldr	r0, =__bss_start
	mov	r1, #0
	ldr	r2, =__bss_end
	sub	r2, r2, r0
	bl	memset

	/* Call the main function. */
	mov	r0, r4
	b	platform_init
FUNCTION_END(_start)

.section .data, "aw", %progbits

/** Stack.
 * @note		Must not be placed in .bss, as it is cleared by memset
 *			whose stack would be overwritten. */
.align PAGE_WIDTH
SYMBOL(loader_stack)
	.fill	PAGE_SIZE
