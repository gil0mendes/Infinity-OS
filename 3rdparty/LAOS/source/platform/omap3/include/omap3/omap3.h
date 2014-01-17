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
 * @brief		OMAP3 core definitions.
 */

#ifndef __OMAP3_OMAP3_H
#define __OMAP3_OMAP3_H

/** Physical memory region definitions. */
#define OMAP3_GPRAM_BASE	0x00000000	/**< GPMC (Q0) base. */
#define OMAP3_GPRAM_SIZE	0x40000000	/**< GPMC size (1GB). */
#define OMAP3_L4_BASE		0x48000000	/**< L4-Core base. */
#define OMAP3_L4_SIZE		0x01000000	/**< L4-Core size (16MB). */
#define OMAP3_L4_WAKEUP_BASE	0x48300000	/**< L4-Wakeup base. */
#define OMAP3_L4_WAKEUP_SIZE	0x00040000	/**< L4-Wakeup size (256KB). */
#define OMAP3_L4_PER_BASE	0x49000000	/**< L4-Peripheral base. */
#define OMAP3_L4_PER_SIZE	0x00100000	/**< L4-Peripheral size (1MB). */
#define OMAP3_L4_EMU_BASE	0x54000000	/**< L4-Emulation base. */
#define OMAP3_L4_EMU_SIZE	0x00800000	/**< L4-Emulation size (8MB). */
#define OMAP3_SDRAM_BASE	0x80000000	/**< SDMC (Q2) base. */
#define OMAP3_SDRAM_SIZE	0x40000000	/**< SDMC size (1GB). */

/** UART base addresses. */
#define OMAP3_UART1_BASE	(OMAP3_L4_BASE + 0x6A000)
#define OMAP3_UART2_BASE	(OMAP3_L4_BASE + 0x6C000)
#define OMAP3_UART3_BASE	(OMAP3_L4_PER_BASE + 0x20000)

/** DSS base address. */
#define OMAP3_DSS_BASE		(OMAP3_L4_BASE + 0x50400)

#endif /* __OMAP3_OMAP3_H */
