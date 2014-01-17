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
 * @brief		ARM architecture initialization functions.
 */

#include <arm/atag.h>
#include <arm/except.h>

#include <loader.h>

/** Address of the ATAG list. */
atag_t *atag_list = NULL;

/** Handle an undefined instruction exception.
 * @param frame		Interrupt frame. */
void arm_undefined_handler(interrupt_frame_t *frame) {
	internal_error("Undefined Instruction exception:\n"
		"R0:   0x%08lx  R1: 0x%08lx  R2:  0x%08lx  R3:  0x%08lx\n"
		"R4:   0x%08lx  R5: 0x%08lx  R6:  0x%08lx  R7:  0x%08lx\n"
		"R8:   0x%08lx  R9: 0x%08lx  R10: 0x%08lx  R11: 0x%08lx\n"
		"R12:  0x%08lx  SP: 0x%08lx  LR:  0x%08lx  PC:  0x%08lx\n"
		"SPSR: 0x%08lx", frame->r0, frame->r1, frame->r2, frame->r3,
		frame->r4, frame->r5, frame->r6, frame->r7, frame->r8,
		frame->r9, frame->r10, frame->r11, frame->r12, frame->sp,
		frame->lr, frame->pc, frame->spsr);
}

/** Handle a prefetch abort exception.
 * @param frame		Interrupt frame. */
void arm_prefetch_abort_handler(interrupt_frame_t *frame) {
	unsigned long ifsr, ifar;

	/* Get the IFSR and IFAR. */
	__asm__ volatile("mrc p15, 0, %0, c5, c0, 1" : "=r"(ifsr));
	__asm__ volatile("mrc p15, 0, %0, c6, c0, 2" : "=r"(ifar));

	internal_error("Prefetch Abort exception\n"
		"R0:   0x%08lx  R1:   0x%08lx  R2:   0x%08lx  R3:  0x%08lx\n"
		"R4:   0x%08lx  R5:   0x%08lx  R6:   0x%08lx  R7:  0x%08lx\n"
		"R8:   0x%08lx  R9:   0x%08lx  R10:  0x%08lx  R11: 0x%08lx\n"
		"R12:  0x%08lx  SP:   0x%08lx  LR:   0x%08lx  PC:  0x%08lx\n"
		"SPSR: 0x%08lx  IFSR: 0x%08lx  IFAR: 0x%08lx", frame->r0,
		frame->r1, frame->r2, frame->r3, frame->r4, frame->r5,
		frame->r6, frame->r7, frame->r8, frame->r9, frame->r10,
		frame->r11, frame->r12, frame->sp, frame->lr, frame->pc,
		frame->spsr, ifsr, ifar);
}

/** Handle a data abort exception.
 * @param frame		Interrupt frame. */
void arm_data_abort_handler(interrupt_frame_t *frame) {
	unsigned long dfsr, dfar;

	/* Get the DFSR and DFAR. */
	__asm__ volatile("mrc p15, 0, %0, c5, c0, 0" : "=r"(dfsr));
	__asm__ volatile("mrc p15, 0, %0, c6, c0, 0" : "=r"(dfar));

	internal_error("Data Abort exception\n"
		"R0:   0x%08lx  R1:   0x%08lx  R2:   0x%08lx  R3:  0x%08lx\n"
		"R4:   0x%08lx  R5:   0x%08lx  R6:   0x%08lx  R7:  0x%08lx\n"
		"R8:   0x%08lx  R9:   0x%08lx  R10:  0x%08lx  R11: 0x%08lx\n"
		"R12:  0x%08lx  SP:   0x%08lx  LR:   0x%08lx  PC:  0x%08lx\n"
		"SPSR: 0x%08lx  DFSR: 0x%08lx  DFAR: 0x%08lx", frame->r0,
		frame->r1, frame->r2, frame->r3, frame->r4, frame->r5,
		frame->r6, frame->r7, frame->r8, frame->r9, frame->r10,
		frame->r11, frame->r12, frame->sp, frame->lr, frame->pc,
		frame->spsr, dfsr, dfar);
}

/** Install an exception handler.
 * @param vector	Vector number.
 * @param addr		Address of handler. */
static void install_handler(size_t num, void (*addr)(void)) {
	/* Store the opcode for the following instruction:
	 *  ldr pc, [pc, #((ARM_VECTOR_COUNT * 4) - 8)]
	 * 8 is subtracted because "When executing an ARM instruction, PC reads
	 * as the address of the current instruction plus 8." This will load
	 * the address of the handler that we store after the vector table. */
	((unsigned long *)0)[num] = 0xe59ff000 | ((ARM_VECTOR_COUNT * 4) - 8);
	((unsigned long *)0)[num + ARM_VECTOR_COUNT] = (ptr_t)addr;
}

/** Perform early architecture initialization.
 * @param atags		ATAG list from the firmware/U-Boot. */
void arch_init(atag_t *atags) {
	/* Install exception handlers. */
	install_handler(ARM_VECTOR_UNDEFINED, arm_undefined);
	install_handler(ARM_VECTOR_PREFETCH_ABORT, arm_prefetch_abort);
	install_handler(ARM_VECTOR_DATA_ABORT, arm_data_abort);

	atag_list = atags;

	/* Verify that the list is valid: it must begin with an ATAG_CORE tag. */
	if(atag_list->hdr.tag != ATAG_CORE)
		internal_error("ATAG list is not valid (%p)", atag_list);
}
