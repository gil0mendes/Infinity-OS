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
 * @brief		OMAP3 UART console implementation.
 */

#include <omap3/omap3.h>
#include <omap3/uart.h>

#include <console.h>
#include <loader.h>

/** UART port definitions. */
static volatile uint8_t *uarts[] = {
	(volatile uint8_t *)OMAP3_UART1_BASE,
	(volatile uint8_t *)OMAP3_UART2_BASE,
	(volatile uint8_t *)OMAP3_UART3_BASE,
};

/** Read a register from a UART.
 * @param port		Port to read from.
 * @param reg		Register to read.
 * @return		Value read. */
static inline uint8_t uart_read_reg(int port, int reg) {
	return uarts[port][reg << 2];
}

/** Write a register to a UART.
 * @param port		Port to read from.
 * @param reg		Register to read.
 * @param value		Value to write. */
static inline void uart_write_reg(int port, int reg, uint8_t value) {
	uarts[port][reg << 2] = value;
}

/** Initialise a UART port.
 * @param port		Port number to initialise.
 * @param baud		Baud rate. */
static void uart_init_port(int port, int baud) {
	uint16_t divisor;

	/* Disable UART. */
	uart_write_reg(port, UART_MDR1_REG, 0x7);

	/* Enable access to IER_REG (configuration mode B for EFR_REG). */
	uart_write_reg(port, UART_LCR_REG, 0xBF);
	uart_write_reg(port, UART_EFR_REG, uart_read_reg(port, UART_EFR_REG) | (1<<4));

	/* Disable interrupts and sleep mode (operational mode). */
	uart_write_reg(port, UART_LCR_REG, 0);
	uart_write_reg(port, UART_IER_REG, 0);

	/* Clear and enable FIFOs. Must be done when clock is not running, so
	 * set DLL_REG and DLH_REG to 0. All done in configuration mode A. */
	uart_write_reg(port, UART_LCR_REG, (1<<7));
	uart_write_reg(port, UART_DLL_REG, 0);
	uart_write_reg(port, UART_DLH_REG, 0);
	uart_write_reg(port, UART_FCR_REG, (1<<0) | (1<<1) | (1<<2));

	/* Now program the divisor to set the baud rate.
	 *  Baud rate = (functional clock / 16) / N */
	divisor = (UART_CLOCK / 16) / baud;
	uart_write_reg(port, UART_DLL_REG, divisor & 0xff);
	uart_write_reg(port, UART_DLH_REG, (divisor >> 8) & 0x3f);

	/* Configure for 8N1 (8-bit, no parity, 1 stop-bit), and switch to
	 * operational mode. */
	uart_write_reg(port, UART_LCR_REG, 0x3);

	/* Enable RTS/DTR. */
	uart_write_reg(port, UART_MCR_REG, (1<<0) | (1<<1));

	/* Enable UART in 16x mode. */
	uart_write_reg(port, UART_MDR1_REG, 0);
}

/** Write a character to a UART.
 * @param port		Port to write.
 * @param ch		Character to write. */
static void uart_putch(int port, unsigned char ch) {
	/* Wait for the TX FIFO to be empty. */
	while(!(uart_read_reg(port, UART_LSR_REG) & (1<<6))) {}

	/* Write the character. */
	uart_write_reg(port, UART_THR_REG, ch);
}

/** Write a character to the UART console.
 * @param ch		Character to write. */
static void uart_console_putch(char ch) {
	uart_putch(DEBUG_UART, ch);
}

/** UART console object. */
static console_t uart_console = {
	.putch = uart_console_putch,
};

/** Initialise the UART console. */
void uart_init(void) {
	/* Initialise the debug port and set it as the debug console. */
	uart_init_port(DEBUG_UART, 115200);
	debug_console = &uart_console;
	uart_console_putch('\n');
}
