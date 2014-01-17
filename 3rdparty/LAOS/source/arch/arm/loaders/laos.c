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
 * @brief		ARM LAOS kernel loader.
 */

#include <arm/mmu.h>

#include <loaders/laos.h>

#include <platform/loader.h>

#include <elf.h>
#include <laos.h>
#include <loader.h>
#include <memory.h>

/** Entry arguments for the kernel. */
typedef struct entry_args {
	uint32_t transition_ttbr;	/**< Transition address space. */
	uint32_t virt;			/**< Virtual location of trampoline. */
	uint32_t kernel_ttbr;		/**< Kernel address space. */
	uint32_t sp;			/**< Stack pointer for the kernel. */
	uint32_t entry;			/**< Entry point for kernel. */
	uint32_t tags;			/**< Tag list virtual address. */

	char trampoline[];
} entry_args_t;

extern void laos_arch_enter32(entry_args_t *args) __noreturn;

extern char laos_trampoline32[];
extern size_t laos_trampoline32_size;

/** Check a kernel image and determine the target type.
 * @param loader	LAOS loader data structure. */
void laos_arch_check(laos_loader_t *loader) {
	if(!elf_check(loader->kernel, ELFCLASS32, ELFDATA2LSB, ELF_EM_ARM))
		boot_error("Kernel image is not for this architecture");

	loader->target = TARGET_TYPE_32BIT;
}

/** Validate kernel load parameters.
 * @param loader	LAOS loader data structure.
 * @param load		Load image tag. */
void laos_arch_load_params(laos_loader_t *loader, laos_itag_load_t *load) {
	if(!(load->flags & LAOS_LOAD_FIXED) && !load->alignment) {
		/* Set default alignment parameters. Just try 1MB alignment
		 * as that allows the kernel to be mapped using sections. */
		load->alignment = load->min_alignment = 0x100000;
	}
}

/** Perform architecture-specific setup tasks.
 * @param loader	LAOS loader data structure. */
void laos_arch_setup(laos_loader_t *loader) {
	laos_tag_pagetables_t *tag;
	target_ptr_t virt;
	uint32_t *l1, *l2;
	phys_ptr_t phys;

	/* Allocate a 1MB temporary mapping region for the kernel. */
	if(!allocator_alloc(&loader->alloc, 0x100000, 0x100000, &virt))
		boot_error("Unable to allocate temporary mapping region");

	/* Create a second level table to cover the region. */
	phys_memory_alloc(PAGE_SIZE, PAGE_SIZE, 0, 0, PHYS_MEMORY_PAGETABLES, 0, &phys);
	memset((void *)phys, 0, PAGE_SIZE);

	/* Insert it into the first level table, then point its last entry to
	 * itself. */
	l1 = (uint32_t *)loader->mmu->l1;
	l1[virt / 0x100000] = phys | (1<<0);
	l2 = (uint32_t *)phys;
	l2[255] = phys | (1<<1) | (1<<4);

	/* Add the pagetables tag. */
	tag = laos_allocate_tag(loader, LAOS_TAG_PAGETABLES, sizeof(*tag));
	tag->l1 = loader->mmu->l1;
	tag->mapping = virt;
}

/** Enter a loaded LAOS kernel.
 * @param loader	LAOS loader data structure. */
__noreturn void laos_arch_enter(laos_loader_t *loader) {
	entry_args_t *args;

	/* Store information for the entry code. */
	args = (void *)((ptr_t)loader->trampoline_phys);
	args->transition_ttbr = loader->transition->l1;
	args->virt = loader->trampoline_virt;
	args->kernel_ttbr = loader->mmu->l1;
	args->sp = loader->stack_virt + loader->stack_size;
	args->entry = loader->entry;
	args->tags = loader->tags_virt;

	/* Copy the trampoline and call the entry code. */
	memcpy(args->trampoline, laos_trampoline32, laos_trampoline32_size);
	laos_arch_enter32(args);
}
