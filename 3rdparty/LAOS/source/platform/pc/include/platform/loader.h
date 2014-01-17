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
 * @brief		PC platform core definitions.
 */

#ifndef __PLATFORM_LOADER_H
#define __PLATFORM_LOADER_H

/** Memory layout definitions. */
#define LOADER_LOAD_ADDR	0x10000		/**< Load address of the boot loader. */
#define MULTIBOOT_LOAD_ADDR	0x100000	/**< Load address for Multiboot. */
#define MULTIBOOT_LOAD_OFFSET	0xF0000		/**< Load offset for Multiboot. */

/** Segment defintions. */
#define SEGMENT_CS		0x08		/**< Code segment. */
#define SEGMENT_DS		0x10		/**< Data segment. */
#define SEGMENT_CS16		0x18		/**< 16-bit code segment. */
#define SEGMENT_DS16		0x20		/**< 16-bit code segment. */
#define SEGMENT_CS64		0x28		/**< 64-bit code segment. */
#define SEGMENT_DS64		0x30		/**< 64-bit data segment. */

/** LAOS log buffer address. */
#define LAOS_LOG_BUFFER	0x1F00000
#define LAOS_LOG_SIZE		0x100000

#ifndef __ASM__

extern void platform_reboot(void);

#endif /* __ASM__ */

#endif /* __PLATFORM_LOADER_H */
