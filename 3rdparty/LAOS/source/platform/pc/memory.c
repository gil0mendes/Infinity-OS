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
 * @brief		PC memory detection code.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <pc/bios.h>
#include <pc/memory.h>

#include <loader.h>
#include <memory.h>

/** Detect physical memory. */
void platform_memory_detect(void) {
	e820_entry_t *mmap = (e820_entry_t *)BIOS_MEM_BASE;
	phys_ptr_t start, end;
	size_t count = 0, i;
	bios_regs_t regs;

	bios_regs_init(&regs);

	/* Obtain a memory map using interrupt 15h, function E820h. */
	do {
		regs.eax = 0xE820;
		regs.edx = E820_SMAP;
		regs.ecx = 64;
		regs.edi = BIOS_MEM_BASE + (count * sizeof(e820_entry_t));
		bios_interrupt(0x15, &regs);

		/* If CF is set, the call was not successful. BIOSes are
		 * allowed to return a non-zero continuation value in EBX and
		 * return an error on next call to indicate that the end of the
		 * list has been reached. */
		if(regs.eflags & X86_FLAGS_CF)
			break;

		count++;
	} while(regs.ebx != 0);

	/* FIXME: Should handle BIOSen that don't support this. */
	if(count == 0)
		boot_error("BIOS does not support E820 memory map");

	/* Iterate over the obtained memory map and add the entries to the
	 * PMM. */
	for(i = 0; i < count; i++) {
		/* The E820 memory map can contain regions that aren't
		 * page-aligned. This presents a problem for us - we want to
		 * provide the kernel with a list of regions that are all
		 * page-aligned. Therefore, we must handle non-aligned regions
		 * according to the type they are. For free memory regions,
		 * we round start up and end down, to ensure that the region
		 * doesn't get resized to memory we shouldn't accessed. If
		 * this results in a zero-length entry, then we ignore it.
		 * Otherwise, we round start down, and end up, so we don't
		 * finish up with a zero-length region. This ensures that all
		 * reserved regions in the original map are included in the
		 * map provided to the kernel. */
		if(mmap[i].type == E820_TYPE_FREE || mmap[i].type == E820_TYPE_ACPI_RECLAIM) {
			start = ROUND_UP(mmap[i].start, PAGE_SIZE);
			end = ROUND_DOWN(mmap[i].start + mmap[i].length, PAGE_SIZE);
		} else {
			start = ROUND_DOWN(mmap[i].start, PAGE_SIZE);
			end = ROUND_UP(mmap[i].start + mmap[i].length, PAGE_SIZE);
		}

		/* What we did above may have made the region too small, warn
		 * and ignore it if this is the case. */
		if(end <= start) {
			dprintf("memory: broken memory map entry: [0x%" PRIx64 ",0x%"
				PRIx64 ") (%" PRIu32 ")\n", mmap[i].start,
				mmap[i].start + mmap[i].length, mmap[i].type);
			continue;
		}

		/* We only care about free ranges here. */
		if(mmap[i].type != E820_TYPE_FREE)
			continue;

		/* Ensure that the BIOS data area is not marked as free. BIOSes
		 * don't mark it as reserved in the memory map as it can be
		 * overwritten if it is no longer needed, but it may be needed
		 * by the kernel, for example to call BIOS interrupts. */
		if(start == 0) {
			start = PAGE_SIZE;
			if(start >= end)
				continue;
		}

		/* Add the range to the physical memory manager. */
		phys_memory_add(start, end - start, PHYS_MEMORY_FREE);
	}

	/* Mark the memory area we use for BIOS calls as internal. */
	phys_memory_protect(BIOS_MEM_BASE, BIOS_MEM_SIZE + PAGE_SIZE);
}
