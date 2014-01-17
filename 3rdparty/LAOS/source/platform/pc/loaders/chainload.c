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
 * @brief		PC chainload loader type.
 */

#include <arch/io.h>

#include <pc/bios.h>
#include <pc/disk.h>

#include <config.h>
#include <loader.h>

extern void chain_loader_enter(uint8_t id, ptr_t part) __noreturn;

/** Details of where to load stuff to. */
#define CHAINLOAD_ADDR		0x7C00
#define CHAINLOAD_SIZE		512
#define PARTITION_TABLE_ADDR	0x7BE
#define PARTITION_TABLE_OFFSET	446
#define PARTITION_TABLE_SIZE	64

/** Load a chainload entry.
 * @note		Assumes the disk has an MSDOS partition table. */
static __noreturn void chain_loader_load(void) {
	disk_t *disk, *parent;
	ptr_t part_addr = 0;
	file_handle_t *file;
	char *path;

	if(current_device->type != DEVICE_TYPE_DISK)
		boot_error("Cannot chainload from non-disk device");

	disk = (disk_t *)current_device;
	parent = disk_parent(disk);

	path = current_environ->data;
	if(path) {
		/* Loading from a file. */
		file = file_open(path, NULL);
		if(!file)
			boot_error("Could not read boot file");

		/* Read in the boot sector. */
		if(!file_read(file, (void *)CHAINLOAD_ADDR, CHAINLOAD_SIZE, 0))
			boot_error("Could not read boot sector");

		file_close(file);
	} else {
		/* Loading the boot sector from the disk. */
		if(!disk_read(disk, (void *)CHAINLOAD_ADDR, CHAINLOAD_SIZE, 0))
			boot_error("Could not read boot sector");
	}

	/* Check if this is a valid boot sector. */
	if(*(uint16_t *)(CHAINLOAD_ADDR + 510) != 0xAA55)
		boot_error("Boot sector is missing signature");

	dprintf("loader: chainloading from device %s (BIOS drive: 0x%x)\n",
		current_device->name, parent->id);

	/* If booting a partition, we must give partition information to it. */
	if(parent != disk) {
		if(!disk_read(parent, (void *)PARTITION_TABLE_ADDR, PARTITION_TABLE_SIZE,
			PARTITION_TABLE_OFFSET))
		{
			boot_error("Could not read partition table");
		}

		part_addr = PARTITION_TABLE_ADDR + (disk->id << 4);
	}

	/* Perform pre-boot tasks. */
	loader_preboot();

	/* Drop to real mode and jump to the boot sector. */
	chain_loader_enter(parent->id, part_addr);
}

/** Chainload loader type. */
static loader_type_t chain_loader_type = {
	.load = chain_loader_load,
};

/** Chainload another boot sector.
 * @param args		Arguments for the command.
 * @return		Whether successful. */
static bool config_cmd_chainload(value_list_t *args) {
	char *path = NULL;

	if(args->count != 0 && args->count != 1) {
		dprintf("config: chainload: invalid arguments\n");
		return false;
	}

	if(args->count == 1) {
		if(args->values[0].type != VALUE_TYPE_STRING) {
			dprintf("config: chainload: invalid arguments\n");
			return false;
		}

		path = kstrdup(args->values[0].string);
	}

	current_environ->loader = &chain_loader_type;
	current_environ->data = path;
	return true;
}

BUILTIN_COMMAND("chainload", config_cmd_chainload);
