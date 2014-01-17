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
 * @brief		OMAP3 UART definitions.
 */

#ifndef __OMAP3_UART_H
#define __OMAP3_UART_H

/** Port to use for debug output. FIXME: This is only correct for BeagleBoard. */
#define DEBUG_UART	2

/** UART port definitions. */
#define UART_RHR_REG		0	/**< Receive Holding Register. */
#define UART_THR_REG		0	/**< Transmit Holding Register. */
#define UART_DLL_REG		0	/**< Divisor Latches Low. */
#define UART_DLH_REG		1	/**< Divisor Latches High. */
#define UART_IER_REG		1	/**< Interrupt Enable Register. */
#define UART_EFR_REG		2	/**< Enhanced Feature Register. */
#define UART_FCR_REG		2	/**< FIFO Control Register. */
#define UART_LCR_REG		3	/**< Line Control Register. */
#define UART_MCR_REG		4	/**< Modem Control Register. */
#define UART_LSR_REG		5	/**< Line Status Register. */
#define UART_MDR1_REG		8	/**< Mode Definition Register 1. */

/** Base clock rate (48MHz). */
#define UART_CLOCK		48000000

extern void uart_init(void);

#endif /* __OMAP3_UART_H */
