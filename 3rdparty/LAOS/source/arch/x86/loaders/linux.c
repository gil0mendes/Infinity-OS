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
 * @brief		x86 Linux kernel loader.
 *
 * Currently we only support the 32-bit boot protocol, all 2.6 series and later
 * kernels support this as far as I know.
 *
 * Reference:
 *  - The Linux/x86 Boot Protocol
 *    http://lxr.linux.no/linux/Documentation/x86/boot.txt
 */

#include <arch/page.h>

#include <x86/linux.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <loaders/linux.h>

#include <assert.h>
#include <fs.h>
#include <loader.h>
#include <memory.h>

extern __noreturn void linux_arch_enter(ptr_t entry, ptr_t params, ptr_t sp);

/** Allocate memory to load the kernel to.
 * @param params	Kernel parameters structure.
 * @param prot_size	Calculated protected mode kernel size.
 * @param physp		Where to store the allocated address.
 * @return		Whether successful in allocating memory. */
static bool allocate_kernel(linux_params_t *params, size_t prot_size, phys_ptr_t *physp) {
	size_t align, min_align, load_size;
	phys_ptr_t pref_addr;
	bool relocatable;

	if(params->hdr.version >= 0x0205 && params->hdr.relocatable_kernel) {
		relocatable = true;
		align = params->hdr.kernel_alignment;
		if(params->hdr.version >= 0x020a) {
			min_align = 1 << params->hdr.min_alignment;
			pref_addr = params->hdr.pref_address;
		} else {
			min_align = align;
			pref_addr = ROUND_UP(LINUX_BZIMAGE_ADDR, align);
		}
	} else {
		relocatable = false;
		align = 0;
		min_align = 0;
		pref_addr = LINUX_BZIMAGE_ADDR;
	}

	/* Determine the load size. For protocol 2.10+ there is a hint in the
	 * header which contains the amount of memory that the kernel requires
	 * to decompress itself, so use that if possible. Otherwise, just use
	 * the size of the protected mode kernel in the file. FIXME: Account
	 * for decompression by allocating more space in older kernels? */
	if(params->hdr.version >= 0x020a) {
		assert(params->hdr.init_size >= prot_size);
		load_size = ROUND_UP(params->hdr.init_size, PAGE_SIZE);
	} else {
		load_size = ROUND_UP(prot_size, PAGE_SIZE);
	}

	/* First try the preferred address. */
	if(phys_memory_alloc(load_size, 0, pref_addr, pref_addr + load_size,
		PHYS_MEMORY_ALLOCATED, PHYS_ALLOC_CANFAIL, physp))
	{
		dprintf("linux: loading kernel to preferred address 0x%" PRIxPHYS " "
			"(load_size: 0x%zx)\n", pref_addr, load_size);
		return true;
	}

	/* Failed that, if we're relocatable we can try to find some space
	 * anywhere to load to. */
	if(relocatable) {
		/* Iterate down in powers of 2 until we reach the minimum alignment. */
		while(!phys_memory_alloc(load_size, align, 0x100000, 0, PHYS_MEMORY_ALLOCATED,
			PHYS_ALLOC_CANFAIL, physp))
		{
			align >>= 1;
			if(align < min_align || align < PAGE_SIZE)
				return false;
		}

		params->hdr.kernel_alignment = align;

		dprintf("linux: loading kernel to 0x%" PRIxPHYS " (pref_addr: 0x%" PRIxPHYS ", "
			"align: 0x%zx, min_align: 0x%zx, load_size: 0x%zx, relocatable: %d)\n",
			*physp, pref_addr, align, min_align, load_size, relocatable);
		return true;
	} else {
		return false;
	}
}

/** Load a Linux kernel.
 * @param kernel	Handle to kernel image.
 * @param initrd	Handle to initrd, if any.
 * @param cmdline	Kernel command line. */
void linux_arch_load(file_handle_t *kernel, file_handle_t *initrd, const char *cmdline) {
	size_t cmdline_size, prot_size, prot_offset, initrd_size;
	phys_ptr_t load_addr, initrd_max;
	linux_params_t *params;
	linux_header_t header;
	const char *tok;
	bool quiet;

	STATIC_ASSERT(sizeof(linux_params_t) == PAGE_SIZE);
	STATIC_ASSERT(offsetof(linux_params_t, hdr) == LINUX_HEADER_OFFSET);

	quiet = (tok = strstr(cmdline, "quiet")) && (tok[5] == ' ' || tok[5] == 0);

	/* Read in the kernel header. */
	if(!file_read(kernel, &header, sizeof(linux_header_t), LINUX_HEADER_OFFSET))
		boot_error("Failed to read kernel header");

	/* Check that this is a valid kernel image and that the version is
	 * sufficient. We require at least protocol 2.03, earlier kernels don't
	 * support the 32-bit boot protocol. */
	if(header.boot_flag != 0xAA55 || header.header != LINUX_MAGIC_SIGNATURE) {
		boot_error("File is not a Linux kernel image");
	} else if(header.version < 0x0203) {
		boot_error("Kernel version too old");
	} else if(!(header.loadflags & LINUX_LOADFLAG_LOADED_HIGH)) {
		boot_error("zImage kernels not supported");
	}

	/* Calculate the maximum command line size. */
	if(header.version >= 0x0206) {
		cmdline_size = header.cmdline_size;
	} else {
		cmdline_size = 255;
	}

	/* Allocate memory for the parameters data (the "zero page"). */
	phys_memory_alloc(sizeof(linux_params_t) + ROUND_UP(cmdline_size, PAGE_SIZE),
		PAGE_SIZE, 0x10000, 0x90000, PHYS_MEMORY_RECLAIMABLE, 0, &load_addr);
	params = (linux_params_t *)(ptr_t)load_addr;

	/* Ensure that the parameters page is cleared and copy in the setup
	 * header. */
	memset(params, 0, sizeof(*params));
	memcpy(&params->hdr, &header, 0x11 + header.relative_end);

	/* Start populating required fields in the header. */
	params->hdr.type_of_loader = 0xFF;	// TODO: Get a boot loader ID assigned.
	params->hdr.loadflags |= LINUX_LOADFLAG_CAN_USE_HEAP | ((quiet) ? LINUX_LOADFLAG_QUIET : 0);
	// FIXME: Not sure what this needs to be set to when using the 32-bit
	// boot protocol, as far as I'm aware it doesn't even get used. I'm
	// just setting it to what GRUB does for now.
	params->hdr.heap_end_ptr = 0x8E00;

	/* Copy in the command line data. */
	params->hdr.cmd_line_ptr = (uint32_t)&params[1];
	strncpy((char *)params->hdr.cmd_line_ptr, cmdline, cmdline_size);
	((char *)params->hdr.cmd_line_ptr)[cmdline_size] = 0;

	/* Now we can load the 32-bit kernel. */
	prot_offset = (((params->hdr.setup_sects) ? params->hdr.setup_sects : 4) + 1) * 512;
	prot_size = file_size(kernel) - prot_offset;

	if(!quiet)
		kprintf("Loading Linux kernel...\n");

	if(!allocate_kernel(params, prot_size, &load_addr))
		boot_error("You do not have enough memory available");

	params->hdr.code32_start = load_addr + (params->hdr.code32_start - LINUX_BZIMAGE_ADDR);

	/* Read in the kernel image. */
	if(!file_read(kernel, (void *)(ptr_t)load_addr, prot_size, prot_offset))
		boot_error("Failed to read kernel image");

	/* Load in the initrd. */
	if(initrd) {
		initrd_size = file_size(initrd);
		initrd_max = (params->hdr.version >= 0x0203)
			? params->hdr.initrd_addr_max
			: 0x37FFFFFF;

		/* It is recommended that the initrd be loaded as high as
		 * possible, ask for highest available address. */
		phys_memory_alloc(ROUND_UP(initrd_size, PAGE_SIZE), PAGE_SIZE, 0x100000,
			initrd_max + 1, PHYS_MEMORY_MODULES, PHYS_ALLOC_HIGH,
			&load_addr);

		dprintf("linux: loading initrd to 0x%" PRIxPHYS " (size: 0x%zx, "
			"initrd_max: 0x%" PRIxPHYS ")\n", load_addr, initrd_size,
			initrd_max);
		if(!quiet)
			kprintf("Loading initrd...\n");

		if(!file_read(initrd, (void *)(ptr_t)load_addr, initrd_size, 0))
			boot_error("Failed to read initrd");

		params->hdr.ramdisk_image = load_addr;
		params->hdr.ramdisk_size = initrd_size;
	} else {
		params->hdr.ramdisk_image = 0;
		params->hdr.ramdisk_size = 0;
	}

	/* Perform pre-boot tasks. */
	loader_preboot();

	/* Call into platform code to do environment setup (usually done by the
	 * real-mode bootstrap when using the 16-bit boot protocol). */
	linux_platform_load(params);

	/* Not actually necessary as Linux doesn't want a LAOS memory map,
	 * but it dumps the internal memory map for debugging purposes. */
	memory_finalize();

	/* Start the kernel. Stack is positioned points below the parameters. */
	dprintf("linux: kernel entry point at 0x%x, params at %p\n", params->hdr.code32_start, params);
	linux_arch_enter(params->hdr.code32_start, (ptr_t)params, (ptr_t)params);
}

#if CONFIG_LAOS_UI

/** Add architecture-specific Linux configuration options.
 * @param window	Configuration window. */
void linux_arch_configure(ui_window_t *window) {
	linux_platform_configure(window);
}

#endif
