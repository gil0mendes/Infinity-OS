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
 * @brief		OMAP3 platform startup code.
 */

#include <arm/atag.h>

#include <omap3/omap3.h>
#include <omap3/uart.h>

#include <loader.h>
#include <memory.h>
#include <tar.h>

extern void platform_init(atag_t *atags);

/** Main function of the OMAP3 loader.
 * @param atags		ATAG list from U-Boot. */
void platform_init(atag_t *atags) {
	/* Set up the UART for debug output. */
	uart_init();

	dprintf("loader: loaded, ATAGs at %p\n", atags);

	/* Initialize the architecture. */
	arch_init(atags);

	/* The boot image is passed to us as an initial ramdisk. */
	ATAG_ITERATE(tag, ATAG_INITRD2) {
		tar_mount((void *)tag->initrd.start, tag->initrd.size);
		break;
	}

	/* Initialize hardware. */
	memory_init();

	/* Call the main function. */
	loader_main();
}
