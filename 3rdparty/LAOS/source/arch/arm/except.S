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
 * @brief		ARM exception handlers.
 */

#include <arch/page.h>

#include <arm/asm.h>
#include <arm/cpu.h>

.section .text, "ax", %progbits

/** Switch into supervisor mode and create an interrupt frame on the stack.
 * @param offset	Exception-dependent offset of the LR value. */
.macro EXCEPTION_ENTER offset
	/* Subtract the exception-dependent offset from LR so that we return
	 * to the correct place. */
	sub	lr, lr, #\offset

	/* Save LR and SPSR to the supervisor mode stack. */
	srsdb	sp!, #ARM_MODE_SVC

	/* Switch to supervisor mode, with IRQs and FIQs disabled. */
	cpsid	if, #ARM_MODE_SVC

	/* Save remaining registers. */
	stmfd	sp!, { r0-r12 }

	/* Save SP and LR. */
	add	r0, sp, #60
	stmfd 	sp!, { r0, lr }

	/* Set R0 to the current SP as the argument to the handler. */
	mov	r0, sp
.endm

/** Undefined Instruction exception handler. */
FUNCTION_START(arm_undefined)
	EXCEPTION_ENTER 4
	bl	arm_undefined_handler
FUNCTION_END(arm_undefined)

/** Prefetch Abort exception handler. */
FUNCTION_START(arm_prefetch_abort)
	EXCEPTION_ENTER 4
	bl	arm_prefetch_abort_handler
FUNCTION_END(arm_prefetch_abort)

/** Data Abort exception handler. */
FUNCTION_START(arm_data_abort)
	EXCEPTION_ENTER 8
	bl	arm_data_abort_handler
FUNCTION_END(arm_data_abort)
