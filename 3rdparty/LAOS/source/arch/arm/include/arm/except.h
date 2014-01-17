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
 * @brief		ARM exception handling definitions.
 */

#ifndef __ARM_EXCEPT_H
#define __ARM_EXCEPT_H

#include <types.h>

/** Structure defining an interrupt stack frame. */
typedef struct interrupt_frame {
	unsigned long sp;			/**< SP/R13 from previous mode. */
	unsigned long lr;			/**< LR/R14 from previous mode. */
	unsigned long r0;			/**< R0. */
	unsigned long r1;			/**< R1. */
	unsigned long r2;			/**< R2. */
	unsigned long r3;			/**< R3. */
	unsigned long r4;			/**< R4. */
	unsigned long r5;			/**< R5. */
	unsigned long r6;			/**< R6. */
	unsigned long r7;			/**< R7. */
	unsigned long r8;			/**< R8. */
	unsigned long r9;			/**< R9. */
	unsigned long r10;			/**< R10. */
	unsigned long r11;			/**< R11. */
	unsigned long r12;			/**< R12. */
	unsigned long pc;			/**< PC/R15 from previous mode. */
	unsigned long spsr;			/**< Saved Program Status Register. */
} __packed interrupt_frame_t;

/** Exception vector table indexes. */
#define ARM_VECTOR_COUNT		8	/**< Number of exception vectors. */
#define ARM_VECTOR_RESET		0	/**< Reset. */
#define ARM_VECTOR_UNDEFINED		1	/**< Undefined Instruction. */
#define ARM_VECTOR_SYSCALL		2	/**< Supervisor Call. */
#define ARM_VECTOR_PREFETCH_ABORT	3	/**< Prefetch Abort. */
#define ARM_VECTOR_DATA_ABORT		4	/**< Data Abort. */
#define ARM_VECTOR_IRQ			6	/**< IRQ. */
#define ARM_VECTOR_FIQ			7	/**< FIQ. */

extern void arm_undefined(void);
extern void arm_prefetch_abort(void);
extern void arm_data_abort(void);

extern void arm_undefined_handler(interrupt_frame_t *frame);
extern void arm_prefetch_abort_handler(interrupt_frame_t *frame);
extern void arm_data_abort_handler(interrupt_frame_t *frame);

#endif /* __ARM_EXCEPT_H */
