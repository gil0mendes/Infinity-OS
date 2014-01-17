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
 * @brief		PL011 UART console implementation.
 *
 * Reference:
 *  - PrimeCell UART (PL011) Technical Reference Manual
 *    http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0183g/index.html
 */

#include <pl011/pl011.h>

#include <console.h>
#include <loader.h>

/** Baud rate to use. */
#define BAUD_RATE	115200

/** Mapping of the PL011 registers. */
static volatile uint32_t *pl011_mapping;

/** Write a character to the PL011 console.
 * @param ch		Character to write. */
static void pl011_console_putch(char ch) {
	/* Wait until the transmit FIFO has space. */
	while(pl011_mapping[PL011_REG_FR] & PL011_FR_TXFF);

	pl011_mapping[PL011_REG_DR] = ch;
}

/** PL011 UART debug console. */
static console_t pl011_console = {
	.putch = pl011_console_putch,
};

/** Initialise the PL011 UART as the debug console.
 * @param mapping	Address of PL011 registers.
 * @param clock_rate	Base clock rate. */
void pl011_init(ptr_t mapping, uint32_t clock_rate) {
	uint32_t divider, fraction;

	pl011_mapping = (volatile uint32_t *)mapping;

	/* Disable the UART while we configure it. */
	pl011_mapping[PL011_REG_CR] = 0;

	/* Calculate the baud rate divisor registers. See PL011 Reference
	 * Manual, page 3-10.
	 *  Baud Rate Divisor = UARTCLK / (16 * Baud Rate)
	 * This is split into an integer and a fractional part.
	 *  FBRD = Round((64 * (UARTCLK % (16 * Baud Rate))) / (16 * Baud Rate))
	 */
	divider = clock_rate / (16 * BAUD_RATE);
	fraction = (8 * (clock_rate % (16 * BAUD_RATE))) / BAUD_RATE;
	fraction = (fraction >> 1) + (fraction & 1);

	pl011_mapping[PL011_REG_IBRD] = divider;
	pl011_mapping[PL011_REG_FBRD] = fraction;

	/* Initialize the line control register (8N1, FIFOs enabled). Note that
	 * a write to the LCR is required for an IBRD/FBRD change to take
	 * effect. */
	pl011_mapping[PL011_REG_LCRH] = PL011_LCRH_FEN | PL011_LCRH_WLEN8;

	/* Enable the UART. */
	pl011_mapping[PL011_REG_CR] = PL011_CR_UARTEN | PL011_CR_TXE | PL011_CR_RXE;

	/* Set the debug console. */
	debug_console = &pl011_console;
}
