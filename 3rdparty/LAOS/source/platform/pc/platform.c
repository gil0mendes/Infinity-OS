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
 * @brief		PC platform startup code.
 */

#include <arch/io.h>

#include <x86/descriptor.h>

#include <lib/string.h>

#include <pc/console.h>
#include <pc/multiboot.h>
#include <pc/vbe.h>

#include <config.h>
#include <loader.h>
#include <memory.h>
#include <tar.h>
#include <time.h>

extern void platform_init(void);

/** Main function of the PC loader. */
void platform_init(void) {
	/* Set up console output. */
	console_init();

	/* Initialize the architecture. */
	arch_init();

	/* Initialize hardware. */
	memory_init();
	disk_init();
	vbe_init();

	/* Parse information from Multiboot. */
	multiboot_init();

	/* Call the main function. */
	loader_main();
}

/** Reboot the system. */
void platform_reboot(void) {
	uint8_t val;

	/* Try the keyboard controller. */
	do {
		val = in8(0x64);
		if(val & (1<<0)) {
			in8(0x60);
		}
	} while(val & (1<<1));
	out8(0x64, 0xfe);
	spin(5000);

	/* Fall back on a triple fault. */
	lidt(0, 0);
	__asm__ volatile("ud2");
}
