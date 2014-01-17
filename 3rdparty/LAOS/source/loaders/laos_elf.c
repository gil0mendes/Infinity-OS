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
 * @brief		LAOS ELF loading functions.
 */

#include <lib/utility.h>

#include <loaders/laos.h>

#include <elf.h>
#include <memory.h>
#include <mmu.h>

/** Allocate memory for the kernel image.
 * @param loader	LAOS loader data structure.
 * @param load		Image load parameters.
 * @param virt_base	Virtual base address.
 * @param virt_end	Virtual end address.
 * @return		Physical address allocated for kernel. */
static phys_ptr_t allocate_kernel(laos_loader_t *loader, laos_itag_load_t *load,
	target_ptr_t virt_base, target_ptr_t virt_end)
{
	laos_tag_core_t *core;
	target_size_t size;
	phys_ptr_t ret;
	size_t align;

	size = ROUND_UP(virt_end - virt_base, PAGE_SIZE);

	/* Try to find some space to load to. Iterate down in powers of 2 unti
	 * we reach the minimum alignment. */
	align = load->alignment;
	while(!phys_memory_alloc(size, align, 0, 0, PHYS_MEMORY_ALLOCATED, PHYS_ALLOC_CANFAIL, &ret)) {
		align >>= 1;
		if(align < load->min_alignment || align < PAGE_SIZE)
			boot_error("You do not have enough memory available");
	}

	dprintf("LAOS: loading kernel to 0x%" PRIxPHYS " (alignment: 0x%" PRIxPHYS
		", min_alignment: 0x%" PRIxPHYS ", size: 0x%" PRIx64 ", virt_base: 0x%"
		PRIx64 ")\n", ret, load->alignment, load->min_alignment, size,
		virt_base);

	/* Map in the kernel image. */
	laos_map_virtual(loader, virt_base, ret, size);

	core = (laos_tag_core_t *)((ptr_t)loader->tags_phys);
	core->kernel_phys = ret;
	return ret;
}

/** Allocate memory for a single segment.
 * @param loader	LAOS loader data structure.
 * @param load		Image load parameters.
 * @param virt		Virtual load address.
 * @param phys		Physical load address.
 * @param size		Total load size.
 * @param idx		Segment index. */
static void allocate_segment(laos_loader_t *loader, laos_itag_load_t *load,
	target_ptr_t virt, phys_ptr_t phys, target_size_t size, size_t idx)
{
	phys_ptr_t ret;

	size = ROUND_UP(size, PAGE_SIZE);

	/* Allocate the exact physical address specified. */
	phys_memory_alloc(size, 0, phys, phys + size, PHYS_MEMORY_ALLOCATED, 0, &ret);

	dprintf("LAOS: loading segment %zu to 0x%" PRIxPHYS " (size: 0x%" PRIx64
		", virt: 0x%" PRIx64 ")\n", idx, phys, size, virt);

	/* Map the address range. */
	laos_map_virtual(loader, virt, phys, size);
}

#if CONFIG_LAOS_HAVE_LOADER_LAOS32
# define LAOS_LOAD_ELF32
# include "laos_elfxx.h"
# undef LAOS_LOAD_ELF32
#endif

#if CONFIG_LAOS_HAVE_LOADER_LAOS64
# define LAOS_LOAD_ELF64
# include "laos_elfxx.h"
# undef LAOS_LOAD_ELF64
#endif

/** Iterate over LAOS ELF notes.
 * @param loader	LAOS loader data structure.
 * @param cb		Callback function.
 * @return		Whether the file is an ELF file. */
bool laos_elf_note_iterate(laos_loader_t *loader, laos_note_cb_t cb) {
	#if CONFIG_LAOS_HAVE_LOADER_LAOS32
	if(elf_check(loader->kernel, ELFCLASS32, 0, 0))
		return laos_elf32_note_iterate(loader, cb);
	#endif
	#if CONFIG_LAOS_HAVE_LOADER_LAOS64
	if(elf_check(loader->kernel, ELFCLASS64, 0, 0))
		return laos_elf64_note_iterate(loader, cb);
	#endif

	return false;
}

/** Load an ELF kernel image.
 * @param loader	LAOS loader data structure.
 * @param load		Image load parameters. */
void laos_elf_load_kernel(laos_loader_t *loader, laos_itag_load_t *load) {
	#if CONFIG_LAOS_HAVE_LOADER_LAOS32
	if(loader->target == TARGET_TYPE_32BIT)
		laos_elf32_load_kernel(loader, load);
	#endif
	#if CONFIG_LAOS_HAVE_LOADER_LAOS64
	if(loader->target == TARGET_TYPE_64BIT)
		laos_elf64_load_kernel(loader, load);
	#endif
}

/** Load additional sections for an ELF kernel image.
 * @param loader	LAOS loader data structure. */
void laos_elf_load_sections(laos_loader_t *loader) {
	#if CONFIG_LAOS_HAVE_LOADER_LAOS32
	if(loader->target == TARGET_TYPE_32BIT)
		laos_elf32_load_sections(loader);
	#endif
	#if CONFIG_LAOS_HAVE_LOADER_LAOS64
	if(loader->target == TARGET_TYPE_64BIT)
		laos_elf64_load_sections(loader);
	#endif
}
