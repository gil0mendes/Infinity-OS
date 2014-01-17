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
 * @brief		OMAP3 memory management.
 */

#include <arm/atag.h>

#include <lib/utility.h>

#include <omap3/omap3.h>

#include <loader.h>
#include <memory.h>

/** Detect memory regions. */
void platform_memory_detect(void) {
	phys_ptr_t start, end;

	/* Iterate through all ATAG_MEM tags and add the regions they describe. */
	ATAG_ITERATE(tag, ATAG_MEM) {
		if(tag->mem.size) {
			/* Cut the region short if it is not page-aligned. */
			start = ROUND_UP(tag->mem.start, PAGE_SIZE);
			end = ROUND_DOWN(tag->mem.start + tag->mem.size, PAGE_SIZE);
			phys_memory_add(start, end - start, PHYS_MEMORY_FREE);
		}
	}

	/* Mark any supplied boot image as internal, the memory taken by it is
	 * no longer used once the kernel is entered. */
	ATAG_ITERATE(tag, ATAG_INITRD2) {
		if(tag->initrd.size) {
			/* Ensure the whole region is covered if it is not
			 * page-aligned. */
			start = ROUND_DOWN(tag->initrd.start, PAGE_SIZE);
			end = ROUND_UP(tag->initrd.start + tag->initrd.size, PAGE_SIZE);
			phys_memory_add(start, end - start, PHYS_MEMORY_INTERNAL);
		}
	}

	/* Mark the region between the start of SDRAM and our load address as
	 * internal, as U-Boot puts things like the ATAG list here. */
	phys_memory_add(OMAP3_SDRAM_BASE, ROUND_DOWN((ptr_t)__start, PAGE_SIZE)
		- OMAP3_SDRAM_BASE, PHYS_MEMORY_INTERNAL);
}
