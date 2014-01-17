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
 * @brief		x86 port I/O functions.
 */

#ifndef __ARCH_IO_H
#define __ARCH_IO_H

#include <types.h>

/** Read 8 bits from a port.
 * @param port		Port to read from.
 * @return		Value read. */
static inline uint8_t in8(uint16_t port) {
	uint8_t rv;

	__asm__ volatile("inb %1, %0" : "=a"(rv) : "dN"(port));
	return rv;
}

/** Write 8 bits to a port.
 * @param port		Port to write to.
 * @param data		Value to write. */
static inline void out8(uint16_t port, uint8_t data) {
	__asm__ volatile("outb %1, %0" : : "dN"(port), "a"(data));
}

/** Read 16 bits from a port.
 * @param port		Port to read from.
 * @return		Value read. */
static inline uint16_t in16(uint16_t port) {
	uint16_t rv;

	__asm__ volatile("inw %1, %0" : "=a"(rv) : "dN"(port));
	return rv;
}

/** Write 16 bits to a port.
 * @param port		Port to write to.
 * @param data		Value to write. */
static inline void out16(uint16_t port, uint16_t data) {
	__asm__ volatile("outw %1, %0" : : "dN"(port), "a"(data));
}

/** Read 32 bits from a port.
 * @param port		Port to read from.
 * @return		Value read. */
static inline uint32_t in32(uint16_t port) {
	uint32_t rv;

	__asm__ volatile("inl %1, %0" : "=a"(rv) : "dN"(port));
	return rv;
}

/** Write 32 bits to a port.
 * @param port		Port to write to.
 * @param data		Value to write. */
static inline void out32(uint16_t port, uint32_t data) {
	__asm__ volatile("outl %1, %0" : : "dN"(port), "a"(data));
}

/** Write an array of 16 bits to a port.
 * @param port		Port to write to.
 * @param count		Number of 16 byte values to write.
 * @param buf		Buffer to write from. */
static inline void out16s(uint16_t port, size_t count, const uint16_t *buf) {
	__asm__ volatile("rep outsw" : "=c"(count), "=S"(buf) : "d"(port), "0"(count), "1"(buf) : "memory");
}

/** Read an array of 16 bits from a port.
 * @param port		Port to read from.
 * @param count		Number of 16 byte values to read.
 * @param buf		Buffer to read into. */
static inline void in16s(uint16_t port, size_t count, uint16_t *buf) {
	__asm__ volatile("rep insw" : "=c"(count), "=D"(buf) : "d"(port), "0"(count), "1"(buf) : "memory");
}

#endif /* __ARCH_IO_H */
