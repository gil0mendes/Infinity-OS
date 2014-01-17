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

#ifdef LAOS_LOAD_ELF64
# define elf_ehdr_t	Elf64_Ehdr
# define elf_phdr_t	Elf64_Phdr
# define elf_shdr_t	Elf64_Shdr
# define elf_addr_t	Elf64_Addr
# define ELF_BITS	64
# define FUNC(name)	laos_elf64_##name
#else
# define elf_ehdr_t	Elf32_Ehdr
# define elf_phdr_t	Elf32_Phdr
# define elf_shdr_t	Elf32_Shdr
# define elf_addr_t	Elf32_Addr
# define ELF_BITS	32
# define FUNC(name)	laos_elf32_##name
#endif

/** Iterate over note sections in an ELF file. */
static bool FUNC(note_iterate)(laos_loader_t *loader, laos_note_cb_t cb) {
	elf_phdr_t *phdrs;
	elf_note_t *note;
	const char *name;
	size_t i, offset;
	void *buf, *desc;
	elf_ehdr_t ehdr;

	if(!file_read(loader->kernel, &ehdr, sizeof(ehdr), 0))
		return false;

	phdrs = kmalloc(sizeof(*phdrs) * ehdr.e_phnum);
	if(!file_read(loader->kernel, phdrs, ehdr.e_phnum * ehdr.e_phentsize, ehdr.e_phoff))
		return false;

	for(i = 0; i < ehdr.e_phnum; i++) {
		if(phdrs[i].p_type != ELF_PT_NOTE)
			continue;

		buf = kmalloc(phdrs[i].p_filesz);
		if(!file_read(loader->kernel, buf, phdrs[i].p_filesz, phdrs[i].p_offset))
			return false;

		for(offset = 0; offset < phdrs[i].p_filesz; ) {
			note = (elf_note_t *)(buf + offset);
			offset += sizeof(elf_note_t);
			name = (const char *)(buf + offset);
			offset += ROUND_UP(note->n_namesz, 4);
			desc = buf + offset;
			offset += ROUND_UP(note->n_descsz, 4);

			if(strcmp(name, "LAOS") == 0) {
				if(!cb(note, desc, loader)) {
					kfree(buf);
					kfree(phdrs);
					return true;
				}
			}
		}

		kfree(buf);
	}

	kfree(phdrs);
	return true;
}

/** Load an ELF kernel image. */
static void FUNC(load_kernel)(laos_loader_t *loader, laos_itag_load_t *load) {
	elf_addr_t virt_base = 0, virt_end = 0;
	phys_ptr_t phys = 0;
	elf_phdr_t *phdrs;
	elf_ehdr_t ehdr;
	ptr_t dest;
	size_t i;

	if(!file_read(loader->kernel, &ehdr, sizeof(ehdr), 0))
		boot_error("Could not read kernel image");

	phdrs = kmalloc(sizeof(*phdrs) * ehdr.e_phnum);
	if(!file_read(loader->kernel, phdrs, ehdr.e_phnum * ehdr.e_phentsize, ehdr.e_phoff))
		boot_error("Could not read kernel image");

	/* If not loading at a fixed location, we allocate a single block of
	 * physical memory to load at. */
	if(!(load->flags & LAOS_LOAD_FIXED)) {
		/* Calculate the total load size of the kernel. */
		for(i = 0; i < ehdr.e_phnum; i++) {
			if(phdrs[i].p_type != ELF_PT_LOAD)
				continue;

			if(virt_base == 0 || virt_base > phdrs[i].p_vaddr)
				virt_base = phdrs[i].p_vaddr;
			if(virt_end < (phdrs[i].p_vaddr + phdrs[i].p_memsz))
				virt_end = phdrs[i].p_vaddr + phdrs[i].p_memsz;
		}

		phys = allocate_kernel(loader, load, virt_base, virt_end);
	}

	/* Load in the image data. */
	for(i = 0; i < ehdr.e_phnum; i++) {
		if(phdrs[i].p_type != ELF_PT_LOAD)
			continue;

		/* If loading at a fixed location, we have to allocate space. */
		if(load->flags & LAOS_LOAD_FIXED) {
			allocate_segment(loader, load, phdrs[i].p_vaddr, phdrs[i].p_paddr,
				phdrs[i].p_memsz, i);
			dest = phdrs[i].p_paddr;
		} else {
			dest = phys + (phdrs[i].p_vaddr - virt_base);
		}

		if(!file_read(loader->kernel, (void *)dest, phdrs[i].p_filesz, phdrs[i].p_offset))
			boot_error("Could not read kernel image");

		/* Clear BSS sections. */
		memset((void *)(dest + (ptr_t)phdrs[i].p_filesz), 0,
			phdrs[i].p_memsz - phdrs[i].p_filesz);
	}

	loader->entry = ehdr.e_entry;
}

/** Load additional sections from an ELF kernel image. */
static void FUNC(load_sections)(laos_loader_t *loader) {
	laos_tag_sections_t *tag;
	laos_tag_core_t *core;
	elf_shdr_t *shdr;
	elf_ehdr_t ehdr;
	phys_ptr_t addr;
	size_t size, i;
	void *dest;

	if(!file_read(loader->kernel, &ehdr, sizeof(ehdr), 0))
		boot_error("Could not read kernel image");

	size = ehdr.e_shnum * ehdr.e_shentsize;

	tag = laos_allocate_tag(loader, LAOS_TAG_SECTIONS, sizeof(*tag) + size);
	tag->num = ehdr.e_shnum;
	tag->entsize = ehdr.e_shentsize;
	tag->shstrndx = ehdr.e_shstrndx;

	if(!file_read(loader->kernel, tag->sections, size, ehdr.e_shoff))
		boot_error("Could not read kernel image");

	core = (laos_tag_core_t *)((ptr_t)loader->tags_phys);

	/* Iterate through the headers and load in additional loadable sections. */
	for(i = 0; i < ehdr.e_shnum; i++) {
		shdr = (elf_shdr_t *)&tag->sections[i * ehdr.e_shentsize];

		if(shdr->sh_flags & ELF_SHF_ALLOC || shdr->sh_addr || !shdr->sh_size
			|| (shdr->sh_type != ELF_SHT_PROGBITS
				&& shdr->sh_type != ELF_SHT_NOBITS
				&& shdr->sh_type != ELF_SHT_SYMTAB
				&& shdr->sh_type != ELF_SHT_STRTAB)) {
			continue;
		}

		/* Allocate memory to load the section data to. Try to make it
		 * contiguous with the kernel image. */
		phys_memory_alloc(ROUND_UP(shdr->sh_size, PAGE_SIZE), 0,
			core->kernel_phys, 0, PHYS_MEMORY_ALLOCATED, 0,
			&addr);
		shdr->sh_addr = addr;

		/* Load in the section data. */
		dest = (void *)((ptr_t)addr);
		if(shdr->sh_type == ELF_SHT_NOBITS) {
			memset(dest, 0, shdr->sh_size);
		} else {
			if(!file_read(loader->kernel, dest, shdr->sh_size, shdr->sh_offset))
				boot_error("Could not read kernel image");
		}

		dprintf("laos: loaded ELF section %zu to 0x%" PRIxPHYS " (size: %zu)\n",
			i, addr, (size_t)shdr->sh_size);
	}
}

#undef elf_ehdr_t
#undef elf_phdr_t
#undef elf_shdr_t
#undef elf_addr_t
#undef ELF_BITS
#undef FUNC
