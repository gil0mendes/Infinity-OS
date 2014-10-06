/*
 * Copyright (C) 2009-2013 Gil Mendes
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		RTLD image management.
 *
 * @todo		Report missing library/symbol names back to the creator
 *			of the process.
 * @todo		When the API is implemented, need to wrap calls in a
 *			lock.
 */

#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/private/image.h>
#include <kernel/vm.h>

#include <alloca.h>
#include <stdlib.h>
#include <string.h>

#define _GNU_SOURCE
#include <link.h>

#include "libkernel.h"

/** Expected path to libkernel. */
#define LIBKERNEL_PATH		"/system/lib/libkernel.so"

extern char _end[];

/** Array of directories to search for libraries in. */
static const char *library_search_dirs[] = {
	".",
	"/system/lib",
	NULL
};

/** List of loaded images. */
LIST_DEFINE(loaded_images);

/** Image structure representing the kernel library. */
rtld_image_t libkernel_image = {
	.name = "libkernel.so",
	.path = LIBKERNEL_PATH,
	.refcount = 0,
	.state = RTLD_IMAGE_LOADED,
};

/** Pointer to the application image. */
rtld_image_t *application_image;

/** Check if a library is already loaded.
 * @param name		Name of library.
 * @return		Pointer to image structure if found. */
static rtld_image_t *rtld_image_lookup(const char *name) {
	rtld_image_t *image;

	LIST_FOREACH_SAFE(&loaded_images, iter) {
		image = list_entry(iter, rtld_image_t, header);
		if(strcmp(image->name, name) == 0)
			return image;
	}

	return NULL;
}

/** Check if a path exists.
 * @param path		Path to check.
 * @return		True if exists, false if not. */
static bool path_exists(const char *path) {
	handle_t handle;
	status_t ret;

	dprintf("  trying %s... ", path);

	/* Attempt to open it to see if it is there. */
	ret = kern_fs_open(path, FILE_ACCESS_READ, 0, 0, &handle);
	if(ret != STATUS_SUCCESS) {
		dprintf("returned %d\n", ret);
		return false;
	}

	dprintf("success!\n");
	kern_handle_close(handle);
	return true;
}

/** Search for a library and then load it.
 * @param name		Name of library to load.
 * @param req		Image that requires this library.
 * @param imagep	Where to store pointer to image structure.
 * @return		STATUS_SUCCESS if loaded, STATUS_MISSING_LIBRARY if not
 *			found or another status code for failures. */
static status_t load_library(const char *name, rtld_image_t *req, rtld_image_t **imagep) {
	char buf[FS_PATH_MAX];
	rtld_image_t *exist;
	size_t i;

	/* Check if it's already loaded. */
	exist = rtld_image_lookup(name);
	if(exist) {
		if(exist->state == RTLD_IMAGE_LOADING) {
			dprintf("rtld: cyclic dependency on %s detected!\n", exist->name);
			return STATUS_MALFORMED_IMAGE;
		}

		dprintf("rtld: increasing reference count on %s (%p)\n", exist->name, exist);
		exist->refcount++;
		return STATUS_SUCCESS;
	}

	/* Look for the library in the search paths. */
	for(i = 0; library_search_dirs[i]; i++) {
		strcpy(buf, library_search_dirs[i]);
		strcat(buf, "/");
		strcat(buf, name);
		if(path_exists(buf))
			return rtld_image_load(buf, req, ELF_ET_DYN, NULL, imagep);
	}

	printf("rtld: could not find required library %s (required by %s)\n", name, req->name);
	return STATUS_MISSING_LIBRARY;
}

/** Handle an ELF_PT_LOAD program header.
 * @param image		Image being loaded.
 * @param phdr		Program header.
 * @param handle	Handle to the file.
 * @param path		Path to the file.
 * @param i		Index of the program header.
 * @return		Status code describing result of the operation. */
static status_t do_load_phdr(rtld_image_t *image, elf_phdr_t *phdr, handle_t handle,
	const char *path, size_t i)
{
	uint32_t access = 0;
	elf_addr_t start, end;
	offset_t offset;
	status_t ret;
	size_t size;

	/* Work out the access flags to use. */
	if(phdr->p_flags & ELF_PF_R)
		access |= VM_ACCESS_READ;
	if(phdr->p_flags & ELF_PF_W)
		access |= VM_ACCESS_WRITE;
	if(phdr->p_flags & ELF_PF_X)
		access |= VM_ACCESS_EXECUTE;
	if(!access) {
		dprintf("rtld: %s: program header %zu has no protection flags\n", path, i);
		return STATUS_MALFORMED_IMAGE;
	}

	/* Map the BSS if required. */
	if(phdr->p_memsz > phdr->p_filesz) {
		start = (elf_addr_t)image->load_base + ROUND_DOWN(phdr->p_vaddr
			+ phdr->p_filesz, page_size);
		end = (elf_addr_t)image->load_base + ROUND_UP(phdr->p_vaddr
			+ phdr->p_memsz, page_size);
		size = end - start;

		/* Must be writable to be able to clear later. */
		if(!(access & VM_ACCESS_WRITE)) {
			dprintf("rtld: %s: program header %zu should be writable\n",
				path, i);
			return STATUS_MALFORMED_IMAGE;
		}

		/* Create an anonymous region for it. */
		ret = kern_vm_map((void **)&start, size, VM_ADDRESS_EXACT, access,
			VM_MAP_PRIVATE, INVALID_HANDLE, 0, NULL);
		if(ret != STATUS_SUCCESS) {
			dprintf("rtld: %s: unable to create anonymous BSS region (%d)\n",
				path, ret);
			return ret;
		}
	}

	if(phdr->p_filesz == 0)
		return STATUS_SUCCESS;

	/* Load the data. */
	start = (elf_addr_t)image->load_base + ROUND_DOWN(phdr->p_vaddr, page_size);
	end = (elf_addr_t)image->load_base + ROUND_UP(phdr->p_vaddr + phdr->p_filesz, page_size);
	size = end - start;
	offset = ROUND_DOWN(phdr->p_offset, page_size);
	dprintf("rtld: %s: loading header %zu to [%p,%p)\n", path, i, start, start + size);

	/* Map the data in. Set the private flag if mapping as writeable. */
	ret = kern_vm_map((void **)&start, size, VM_ADDRESS_EXACT, access,
		(access & VM_ACCESS_WRITE) ? VM_MAP_PRIVATE : 0, handle,
		offset, NULL);
	if(ret != STATUS_SUCCESS) {
		dprintf("rtld: %s: unable to map file data into memory (%d)\n", path, ret);
		return ret;
	}

	/* Clear out BSS. */
	if(phdr->p_filesz < phdr->p_memsz) {
		start = (elf_addr_t)image->load_base + phdr->p_vaddr + phdr->p_filesz;
		size = phdr->p_memsz - phdr->p_filesz;
		dprintf("rtld: %s: clearing BSS for %zu at [%p,%p)\n", path, i,
			start, start + size);
		memset((void *)start, 0, size);
	}

	return STATUS_SUCCESS;
}

/** Load an image into memory.
 * @param path		Path to image file.
 * @param req		Image that requires this image. This is used to work
 *			out where to place the new image in the image list.
 * @param type		Required ELF type.
 * @param entryp	Where to store entry point for binary, if type is
 *			ELF_ET_EXEC.
 * @param imagep	Where to store pointer to image structure.
 * @return		Status code describing result of the operation. */
status_t rtld_image_load(const char *path, rtld_image_t *req, int type, void **entryp,
	rtld_image_t **imagep)
{
	handle_t handle;
	size_t bytes, size, i;
	elf_ehdr_t ehdr;
	rtld_image_t *image = NULL;
	elf_phdr_t *phdrs;
	char *interp = NULL;
	const char *dep;
	image_info_t info;
	status_t ret;

	/* Try to open the image. */
	ret = kern_fs_open(path, FILE_ACCESS_READ | FILE_ACCESS_EXECUTE, 0, 0, &handle);
	if(ret != STATUS_SUCCESS)
		return ret;

	/* Read in its header and ensure that it is valid. */
	ret = kern_file_read(handle, &ehdr, sizeof(ehdr), 0, &bytes);
	if(ret != STATUS_SUCCESS) {
		goto fail;
	} else if(bytes != sizeof(ehdr)) {
		ret = STATUS_UNKNOWN_IMAGE;
		goto fail;
	} else if(strncmp((const char *)ehdr.e_ident, ELF_MAGIC, strlen(ELF_MAGIC)) != 0) {
		dprintf("rtld: %s: not a valid ELF file\n", path);
		ret = STATUS_UNKNOWN_IMAGE;
		goto fail;
	} else if(ehdr.e_ident[ELF_EI_CLASS] != ELF_CLASS) {
		dprintf("rtld: %s: incorrect ELF class\n", path);
		ret = STATUS_UNKNOWN_IMAGE;
		goto fail;
	} else if(ehdr.e_ident[ELF_EI_DATA] != ELF_ENDIAN) {
		dprintf("rtld: %s: incorrect endianness\n", path);
		ret = STATUS_UNKNOWN_IMAGE;
		goto fail;
	} else if(ehdr.e_machine != ELF_MACHINE) {
		dprintf("rtld: %s: not for this machine\n", path);
		ret = STATUS_UNKNOWN_IMAGE;
		goto fail;
	} else if(ehdr.e_ident[ELF_EI_VERSION] != 1 || ehdr.e_version != 1) {
		dprintf("rtld: %s: not correct ELF version\n", path);
		ret = STATUS_UNKNOWN_IMAGE;
		goto fail;
	} else if(ehdr.e_type != type) {
		dprintf("rtld: %s: incorrect ELF file type\n", path);
		ret = STATUS_UNKNOWN_IMAGE;
		goto fail;
	} else if(ehdr.e_phentsize != sizeof(elf_phdr_t)) {
		dprintf("rtld: %s: bad program header size\n", path);
		ret = STATUS_MALFORMED_IMAGE;
		goto fail;
	}

	/* Create a structure to track information about the image. */
	image = malloc(sizeof(rtld_image_t));
	if(!image) {
		ret = STATUS_NO_MEMORY;
		goto fail;
	}
	memset(image, 0, sizeof(rtld_image_t));

	/* Don't particularly care if we can't duplicate the path string, its
	 * not important (only for debugging purposes). */
	image->path = strdup(path);
	list_init(&image->header);

	/* Read in the program headers. Use alloca() because our malloc()
	 * implementation does not support freeing, so we'd be wasting valuable
	 * heap space. */
	size = ehdr.e_phnum * ehdr.e_phentsize;
	phdrs = alloca(size);
	ret = kern_file_read(handle, phdrs, size, ehdr.e_phoff, &bytes);
	if(ret != STATUS_SUCCESS) {
		goto fail;
	} else if(bytes != size) {
		ret = STATUS_MALFORMED_IMAGE;
		goto fail;
	}

	/* If loading a library, find out exactly how much space we need for
	 * all the LOAD headers, and allocate a chunk of memory for them. For
	 * executables, just put the load base as NULL. */
	if(ehdr.e_type == ELF_ET_DYN) {
		for(i = 0, image->load_size = 0; i < ehdr.e_phnum; i++) {
			if(phdrs[i].p_type == ELF_PT_LOAD)
				continue;

			if((phdrs[i].p_vaddr + phdrs[i].p_memsz) > image->load_size) {
				image->load_size = ROUND_UP(phdrs[i].p_vaddr
					+ phdrs[i].p_memsz, page_size);
			}
		}

		/* Allocate a chunk of memory for it. */
		ret = kern_vm_map(&image->load_base, image->load_size, VM_ADDRESS_ANY,
			VM_ACCESS_READ, VM_MAP_PRIVATE, INVALID_HANDLE, 0, NULL);
		if(ret != STATUS_SUCCESS) {
			dprintf("rtld: %s: unable to allocate memory (%d)\n", path, ret);
			goto fail;
		}
	} else {
		image->load_base = NULL;
		image->load_size = 0;
	}

	/* Load all of the LOAD headers, and save the address of the dynamic
	 * section if we find it. */
	for(i = 0; i < ehdr.e_phnum; i++) {
		switch(phdrs[i].p_type) {
		case ELF_PT_LOAD:
			ret = do_load_phdr(image, &phdrs[i], handle, path, i);
			if(ret != STATUS_SUCCESS)
				goto fail;

			/* Assume the first LOAD header in the image covers the
			 * EHDR and the PHDRs. */
			if(!image->ehdr && !image->phdrs) {
				image->ehdr = image->load_base
					+ ROUND_DOWN(phdrs[i].p_vaddr, page_size);
				image->phdrs = image->load_base
					+ ROUND_DOWN(phdrs[i].p_vaddr, page_size)
					+ ehdr.e_phoff;
				image->num_phdrs = ehdr.e_phnum;
			}

			break;
		case ELF_PT_INTERP:
			if(ehdr.e_type == ELF_ET_EXEC) {
				interp = alloca(phdrs[i].p_filesz + 1);
				ret = kern_file_read(handle, interp, phdrs[i].p_filesz,
					phdrs[i].p_offset, NULL);
				if(ret != STATUS_SUCCESS)
					goto fail;

				interp[phdrs[i].p_filesz] = 0;
			} else if(ehdr.e_type == ELF_ET_DYN) {
				dprintf("rtld: %s: library requires an interpreter!\n",
					path);
				ret = STATUS_MALFORMED_IMAGE;
				goto fail;
			}
			break;
		case ELF_PT_DYNAMIC:
			image->dyntab = (elf_dyn_t *)((elf_addr_t)image->load_base
				+ phdrs[i].p_vaddr);
			break;
		case ELF_PT_TLS:
			if(!phdrs[i].p_memsz) {
				break;
			} else if(image->tls_memsz) {
				/* TODO: Is this right? */
				dprintf("rtld: %s: multiple TLS segments not allowed\n",
					path);
				ret = STATUS_MALFORMED_IMAGE;
				goto fail;
			}

			/* Set the module ID. For the main executable, this
			 * must be APPLICATION_TLS_ID. */
			if(ehdr.e_type == ELF_ET_EXEC) {
				image->tls_module_id = APPLICATION_TLS_ID;
			} else {
				image->tls_module_id = tls_alloc_module_id();
			}

			/* Record information about the initial TLS image. */
			image->tls_image = (void *)((elf_addr_t)image->load_base
				+ phdrs[i].p_vaddr);
			image->tls_filesz = phdrs[i].p_filesz;
			image->tls_memsz = phdrs[i].p_memsz;
			image->tls_align = phdrs[i].p_align;
			image->tls_offset = tls_tp_offset(image);

			dprintf("rtld: %s: got TLS segment at %p (filesz: %zu, memsz: "
				"%zu, align: %zu)\n", path, image->tls_image,
				image->tls_filesz, image->tls_memsz, image->tls_align);
			break;
		case ELF_PT_NOTE:
		case ELF_PT_PHDR:
			break;
		case ELF_PT_GNU_EH_FRAME:
		case ELF_PT_GNU_STACK:
			// FIXME: Handle stack.
			break;
		default:
			dprintf("rtld: %s: program header %zu has unhandled type %u\n",
				path, phdrs[i].p_type);
			ret = STATUS_MALFORMED_IMAGE;
			goto fail;
		}
	}

	/* If loading an executable, check that it has libkernel as its
	 * interpreter. This is to prevent someone from attempting to run a
	 * non-Kiwi application. */
	if(ehdr.e_type == ELF_ET_EXEC) {
		if(!interp || strcmp(interp, LIBKERNEL_PATH) != 0) {
			printf("rtld: %s: not a Kiwi application\n", path);
			ret = STATUS_MALFORMED_IMAGE;
			goto fail;
		}
	}

	/* Check that there was a DYNAMIC header. */
	if(!image->dyntab) {
		dprintf("rtld: %s: could not find DYNAMIC section\n", path);
		ret = STATUS_MALFORMED_IMAGE;
		goto fail;
	}

	/* Fill in our dynamic table and do address fixups. We copy some of the
	 * table entries we need into a table indexed by tag for easy access. */
	for(i = 0; image->dyntab[i].d_tag != ELF_DT_NULL; i++) {
		if(image->dyntab[i].d_tag >= ELF_DT_NUM || image->dyntab[i].d_tag == ELF_DT_NEEDED)
			continue;

		/* Do address fixups. */
		switch(image->dyntab[i].d_tag) {
		case ELF_DT_HASH:
		case ELF_DT_PLTGOT:
		case ELF_DT_STRTAB:
		case ELF_DT_SYMTAB:
		case ELF_DT_JMPREL:
		case ELF_DT_REL_TYPE:
			image->dyntab[i].d_un.d_ptr += (elf_addr_t)image->load_base;
			break;
		}

		image->dynamic[image->dyntab[i].d_tag] = image->dyntab[i].d_un.d_ptr;
	}

	/* Set name and loading state, and fill out hash information.
	 * FIXME: Just use basename of path for application, and for library
	 * if SONAME not set. */
	if(type == ELF_ET_DYN) {
		image->name = (const char *)(image->dynamic[ELF_DT_SONAME]
			+ image->dynamic[ELF_DT_STRTAB]);
	} else {
		image->name = "<application>";
	}
	image->state = RTLD_IMAGE_LOADING;
	rtld_symbol_init(image);

	/* Check if the image is already loaded. */
	if(type == ELF_ET_DYN) {
		if(rtld_image_lookup(image->name)) {
			printf("rtld: %s: image with same name already loaded\n", path);
			ret = STATUS_ALREADY_EXISTS;
			goto fail;
		}
	}

	/* Add the library into the library list before checking dependencies
	 * so that we can check if something has a cyclic dependency. */
	if(req) {
		list_add_before(&req->header, &image->header);
	} else {
		list_append(&loaded_images, &image->header);
	}

	/* Load libraries that we depend on. */
	for(i = 0; image->dyntab[i].d_tag != ELF_DT_NULL; i++) {
		if(image->dyntab[i].d_tag != ELF_DT_NEEDED)
			continue;

		dep = (const char *)(image->dyntab[i].d_un.d_ptr + image->dynamic[ELF_DT_STRTAB]);
		dprintf("rtld: %s: dependency on %s, loading...\n", path, dep);
		ret = load_library(dep, image, NULL);
		if(ret != STATUS_SUCCESS)
			goto fail;
	}

	/* We can now perform relocations. */
	ret = rtld_image_relocate(image);
	if(ret != STATUS_SUCCESS)
		goto fail;

	/* We are loaded. Set the state to loaded and return required info. */
	image->refcount = 1;
	image->state = RTLD_IMAGE_LOADED;

	/* Register the image with the kernel. FIXME: See above about basename.
	 * TODO: Load in full symbol table if possible as the dynsym table only
	 * contains exported symbols (perhaps only for debug builds). */
	info.name = (type == ELF_ET_DYN) ? image->name : image->path;
	info.load_base = image->load_base;
	info.load_size = image->load_size;
	info.symtab = (void *)image->dynamic[ELF_DT_SYMTAB];
	info.sym_entsize = image->dynamic[ELF_DT_SYMENT];
	info.sym_size = image->h_nchain * info.sym_entsize;
	info.strtab = (void *)image->dynamic[ELF_DT_STRTAB];

	ret = kern_image_register(&info, &image->id);
	if(ret != STATUS_SUCCESS) {
		printf("rtld: failed to register image with kernel (%d)\n", ret);
		goto fail;
	}

	if(entryp)
		*entryp = (void *)ehdr.e_entry;
	if(imagep)
		*imagep = image;

	kern_handle_close(handle);
	return STATUS_SUCCESS;
fail:
	if(image) {
		if(image->load_base)
			kern_vm_unmap(image->load_base, image->load_size);

		list_remove(&image->header);
		free(image);
	}
	kern_handle_close(handle);
	return ret;
}

/** Unload an image from memory.
 * @param image		Image to unload. */
void rtld_image_unload(rtld_image_t *image) {
#if 0
	void (*func)(void);
	rtld_image_t *dep;
	const char *name;
	size_t i;

	if(--image->refcount > 0) {
		dprintf("RTLD: Decreased reference count of %p(%s)\n", image, image->name);
		return;
	}

	/* Call the FINI function. */
	if(image->dynamic[ELF_DT_FINI]) {
		func = (void (*)(void))(image->load_base + image->dynamic[ELF_DT_FINI]);
		dprintf("RTLD: Calling FINI function %p...\n", func);
		func();
	}

	/* Unload all dependencies of the library. */
	for(i = 0; image->dyntab[i].d_tag != ELF_DT_NULL; i++) {
		if(image->dyntab[i].d_tag != ELF_DT_NEEDED) {
			continue;
		}

		name = (const char *)(image->dyntab[i].d_un.d_ptr + image->dynamic[ELF_DT_STRTAB]);
		LIST_FOREACH(&rtld_loaded_images, iter) {
			dep = list_entry(iter, rtld_image_t, header);

			if(strcmp(dep->name, name) == 0) {
				rtld_image_unload(dep);
			}
		}
	}

	dprintf("RTLD: Unloaded image %p(%s)\n", image, image->name);
	if(image->load_base) {
		kern_vm_unmap(image->load_base, image->load_size);
	}
	list_remove(&image->header);
	free(image);
#endif
	printf("rtld: not implemented\n");
}

/** Initialise the runtime loader.
 * @param args		Process arguments structure pointer.
 * @param dry_run	Whether this is a dry run.
 * @return		Entry point for the program. */
void *rtld_init(process_args_t *args, bool dry_run) {
	rtld_image_t *image;
	image_info_t info;
	status_t ret;
	void *entry;

	/* Finish setting up the libkernel image structure. */
	libkernel_image.load_size = ROUND_UP(
		(elf_addr_t)_end - (elf_addr_t)libkernel_image.load_base,
		page_size);
	rtld_symbol_init(&libkernel_image);
	list_init(&libkernel_image.header);
	list_append(&loaded_images, &libkernel_image.header);

	/* Register the image with the kernel. */
	info.name = libkernel_image.name;
	info.load_base = libkernel_image.load_base;
	info.load_size = libkernel_image.load_size;
	info.symtab = (void *)libkernel_image.dynamic[ELF_DT_SYMTAB];
	info.sym_entsize = libkernel_image.dynamic[ELF_DT_SYMENT];
	info.sym_size = libkernel_image.h_nchain * info.sym_entsize;
	info.strtab = (void *)libkernel_image.dynamic[ELF_DT_STRTAB];

	ret = kern_image_register(&info, &libkernel_image.id);
	if(ret != STATUS_SUCCESS) {
		printf("rtld: failed to register libkernel image (%d)\n", ret);
		kern_process_exit(ret);
	}

	/* Load the program. */
	dprintf("rtld: loading program %s...\n", args->path);
	ret = rtld_image_load(args->path, NULL, ELF_ET_EXEC, &entry, &application_image);
	if(ret != STATUS_SUCCESS) {
		dprintf("rtld: failed to load binary (%d)\n", ret);
		kern_process_exit(ret);
	}

	/* Print out the image list if required. */
	if(libkernel_debug || dry_run) {
		dprintf("rtld: final image list:\n");
		LIST_FOREACH(&loaded_images, iter) {
			image = list_entry(iter, rtld_image_t, header);
			if(image->path) {
				printf("  %s => %s (%p)\n", image->name, image->path,
					image->load_base);
			} else {
				printf("  %s (%p)\n", image->name, image->load_base);
			}
		}
	}

	return entry;
}

/* TODO: Move this out of here. */
__export int dl_iterate_phdr(int (*callback)(struct dl_phdr_info *, size_t, void *), void *data) {
	rtld_image_t *image;
	struct dl_phdr_info info;
	int ret;

	// FIXME: lock.

	LIST_FOREACH(&loaded_images, iter) {
		image = list_entry(iter, rtld_image_t, header);

		info.dlpi_addr = (elf_addr_t)image->load_base;
		info.dlpi_name = image->name;
		info.dlpi_phdr = image->phdrs;
		info.dlpi_phnum = image->num_phdrs;

		ret = callback(&info, sizeof(info), data);
		if(ret != 0)
			return ret;
	}

	return 0;
}
