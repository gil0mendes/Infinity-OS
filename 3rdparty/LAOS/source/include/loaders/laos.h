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
 * @brief		LAOS loader definitions.
 */

#ifndef __LOADERS_LAOS_H
#define __LOADERS_LAOS_H

#include <lib/allocator.h>
#include <lib/list.h>

#include <elf.h>
#include <laos.h>
#include <mmu.h>
#include <target.h>
#include <ui.h>

/** Data for the LAOS loader. */
typedef struct laos_loader {
	file_handle_t *kernel;		/**< Handle to the kernel image. */
	value_t modules;		/**< Modules to load. */

	/** Kernel image information. */
	target_type_t target;		/**< Target operation mode of the kernel. */
	list_t itags;			/**< Image tag list. */
	laos_itag_image_t *image;	/**< Image definition tag. */
	uint32_t log_magic;		/**< Magic number for the log buffer. */

	/** Environment for the kernel. */
	target_ptr_t entry;		/**< Kernel entry point. */
	phys_ptr_t tags_phys;		/**< Physical address of tag list. */
	target_ptr_t tags_virt;		/**< Virtual address of tag list. */
	mmu_context_t *mmu;		/**< MMU context for the kernel. */
	allocator_t alloc;		/**< Virtual address space allocator. */
	list_t mappings;		/**< Virtual memory mapping information. */
	target_ptr_t stack_virt;	/**< Base of stack set up for the kernel. */
	target_ptr_t stack_size;	/**< Size of stack set up for the kernel. */
	mmu_context_t *transition;	/**< Kernel transition address space. */
	phys_ptr_t trampoline_phys;	/**< Page containing kernel entry trampoline. */
	target_ptr_t trampoline_virt;	/**< Virtual address of trampoline page. */

	#if CONFIG_LAOS_UI
	ui_window_t *config;		/**< Configuration window. */
	#endif
} laos_loader_t;

/** Image tag header structure. */
typedef struct laos_itag {
	list_t header;			/**< List header. */
	uint32_t type;			/**< Type of the tag. */
} __aligned(8) laos_itag_t;

/** Iterate over all tags of a certain type in the LAOS image tag list.
 * @note		Hurray for language abuse. */
#define LAOS_ITAG_ITERATE(_loader, _type, _vtype, _vname) \
	LIST_FOREACH(&(_loader)->itags, __##_vname) \
		for(_vtype *_vname = (_vtype *)&list_entry(__##_vname, laos_itag_t, header)[1]; \
			list_entry(__##_vname, laos_itag_t, header)->type == _type && _vname; \
			_vname = NULL)

/** Find a tag in the image tag list.
 * @param loader	Loader to find in.
 * @param type		Type of tag to find.
 * @return		Pointer to tag, or NULL if not found. */
static inline void *laos_itag_find(laos_loader_t *loader, uint32_t type) {
	laos_itag_t *itag;

	LIST_FOREACH(&loader->itags, iter) {
		itag = list_entry(iter, laos_itag_t, header);
		if(itag->type == type)
			return &itag[1];
	}

	return NULL;
}

/** ELF note type (structure is the same for both ELF32 and ELF64). */
typedef Elf32_Note elf_note_t;

/** LAOS ELF note iteration callback.
 * @param note		Note header.
 * @param desc		Note data.
 * @param loader	LAOS loader data structure.
 * @return		Whether to continue iteration. */
typedef bool (*laos_note_cb_t)(elf_note_t *note, void *desc, laos_loader_t *loader);

extern void *laos_allocate_tag(laos_loader_t *loader, uint32_t type, size_t size);

extern laos_vaddr_t laos_allocate_virtual(laos_loader_t *loader, laos_paddr_t phys,
	laos_vaddr_t size);
extern void laos_map_virtual(laos_loader_t *loader, laos_vaddr_t addr,
	laos_paddr_t phys, laos_vaddr_t size);

extern bool laos_elf_note_iterate(laos_loader_t *loader, laos_note_cb_t cb);
extern void laos_elf_load_kernel(laos_loader_t *loader, laos_itag_load_t *load);
extern void laos_elf_load_sections(laos_loader_t *loader);

extern void laos_arch_check(laos_loader_t *loader);
extern void laos_arch_load_params(laos_loader_t *loader, laos_itag_load_t *load);
extern void laos_arch_setup(laos_loader_t *loader);
extern void laos_arch_enter(laos_loader_t *loader) __noreturn;

extern void laos_platform_video_init(laos_loader_t *loader);
extern void laos_platform_setup(laos_loader_t *loader);

#endif /* __LOADERS_LAOS_H */
