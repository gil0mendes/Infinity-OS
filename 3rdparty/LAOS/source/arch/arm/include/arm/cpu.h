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
 * @brief		ARM CPU definitions.
 */

#ifndef __ARM_CPU_H
#define __ARM_CPU_H

/** Definitions of bits in SCTLR (System Control Register). */
#define ARM_SCTLR_M		(1<<0)	/**< MMU enable. */
#define ARM_SCTLR_A		(1<<1)	/**< Alignment fault checking enable. */
#define ARM_SCTLR_C		(1<<2)	/**< Data and unified caches enable. */
#define ARM_SCTLR_Z		(1<<11)	/**< Branch prediction enable. */
#define ARM_SCTLR_I		(1<<12)	/**< Instruction cache enable. */
#define ARM_SCTLR_V		(1<<13)	/**< Hivecs enable. */

/** Definitions of mode bits in CPSR. */
#define ARM_MODE_USR		0x10	/**< User. */
#define ARM_MODE_FIQ		0x11	/**< FIQ. */
#define ARM_MODE_IRQ		0x12	/**< IRQ. */
#define ARM_MODE_SVC		0x13	/**< Supervisor. */
#define ARM_MODE_ABT		0x17	/**< Abort. */
#define ARM_MODE_UND		0x1B	/**< Undefined. */

#ifndef __ASM__

/** Execute a data memory barrier. */
static inline void arm_dmb(void) {
#if __ARM_ARCH >= 7
	__asm__ volatile("dmb" ::: "memory");
#else
	__asm__ volatile("mcr p15, 0, %0, c7, c10, 5" :: "r"(0) : "memory");
#endif
}

/** Execute a data synchronization barrier. */
static inline void arm_dsb(void) {
#if __ARM_ARCH >= 7
	__asm__ volatile("dsb" ::: "memory");
#else
	__asm__ volatile("mcr p15, 0, %0, c7, c10, 4" :: "r"(0) : "memory");
#endif
}

/** Execute an instruction synchronization barrier. */
static inline void arm_isb(void) {
#if __ARM_ARCH >= 7
	__asm__ volatile("isb" ::: "memory");
#else
	__asm__ volatile("mcr p15, 0, %0, c7, c5, 4" :: "r"(0) : "memory");
#endif
}

#endif /* __ASM__ */
#endif /* __ARM_CPU_H */
