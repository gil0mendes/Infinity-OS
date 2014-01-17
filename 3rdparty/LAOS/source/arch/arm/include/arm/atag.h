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
 * @brief		ARM boot format definitions.
 *
 * On ARM, the loader is loaded using the Linux kernel boot procedure, via
 * another loader such as U-Boot. It is the job of the loader to set up the
 * environment to be as expected by the kernel.
 *
 * Reference:
 *  - Booting ARM Linux
 *    http://www.simtec.co.uk/products/SWLINUX/files/booting_article.html
 */

#ifndef __ARM_ATAG_H
#define __ARM_ATAG_H

#include <types.h>

/** Boot tag types. */
#define ATAG_NONE	0x00000000	/**< Final tag in the list. */
#define ATAG_CORE	0x54410001	/**< First tag in the list. */
#define ATAG_MEM	0x54410002	/**< Physical memory range (multiple allowed). */
#define ATAG_VIDEOTEXT	0x54410003	/**< VGA text type display information. */
#define ATAG_RAMDISK	0x54410004	/**< How the ramdisk will be used in kernel. */
#define ATAG_INITRD2	0x54420005	/**< Location of ramdisk image (physical address). */
#define ATAG_SERIAL	0x54410006	/**< Board serial number. */
#define ATAG_REVISION	0x54410007	/**< Board revision. */
#define ATAG_VIDEOLFB	0x54410008	/**< Framebuffer type display information. */
#define ATAG_CMDLINE	0x54410009	/**< Command line (null-terminated). */
#define ATAG_ACORN	0x41000101	/**< Acorn RiscPC-specific information. */
#define ATAG_MEMCLK	0x41000402	/**< Footbridge memory clock. */

/** Tag header structure. */
typedef struct atag_header {
	uint32_t size;			/**< Size of the tag data (in 32-bit words). */
	uint32_t tag;			/**< Type of the tag. */
} atag_header_t;

/** ATAG_CORE structure. */
typedef struct atag_core {
	uint32_t flags;			/**< Flags (bit 0 = read-only). */
	uint32_t pagesize;		/**< System page size (usually 4K). */
	uint32_t rootdev;		/**< Root device number. */
} atag_core_t;

/** ATAG_MEM structure. */
typedef struct atag_mem {
	uint32_t size;			/**< Size of the area. */
	uint32_t start;			/**< Physical start address. */
} atag_mem_t;

/** ATAG_VIDEOTEXT structure. */
typedef struct atag_videotext {
	uint8_t x;
	uint8_t y;
	uint16_t video_page;
	uint8_t video_mode;
	uint8_t video_cols;
	uint16_t video_ega_bx;
	uint8_t video_lines;
	uint8_t video_isvga;
	uint16_t video_points;
} atag_videotext_t;

/** ATAG_RAMDISK structure. */
typedef struct atag_ramdisk {
	uint32_t flags;			/**< Flags (bit 0 = load, bit 1 = prompt). */
	uint32_t size;			/**< Decompressed size in kB. */
	uint32_t start;			/**< Starting block of floppy-based RAM disk image. */
} atag_ramdisk_t;

/** ATAG_INITRD2 structure. */
typedef struct atag_initrd {
	uint32_t start;			/**< Physical start address. */
	uint32_t size;			/**< Size of compressed ramdisk image in bytes. */
} atag_initrd_t;

/** ATAG_SERIAL structure. */
typedef struct atag_serial {
	uint32_t low;
	uint32_t high;
} atag_serial_t;

/** ATAG_REVISION structure. */
typedef struct atag_revision {
	uint32_t rev;
} atag_revision_t;

/** ATAG_VIDEOLFB structure. */
typedef struct atag_videolfb {
	uint16_t lfb_width;
	uint16_t lfb_height;
	uint16_t lfb_depth;
	uint16_t lfb_linelength;
	uint32_t lfb_base;
	uint32_t lfb_size;
	uint8_t red_size;
	uint8_t red_pos;
	uint8_t green_size;
	uint8_t green_pos;
	uint8_t blue_size;
	uint8_t blue_pos;
	uint8_t rsvd_size;
	uint8_t rsvd_pos;
} atag_videolfb_t;

/** ATAG_CMDLINE structure. */
typedef struct atag_cmdline {
	char cmdline[1];
} atag_cmdline_t;

/** ATAG_ACORN structure. */
typedef struct atag_acorn {
	uint32_t memc_control_reg;
	uint32_t vram_pages;
	uint8_t sounddefault;
	uint8_t adfsdrives;
} atag_acorn_t;

/** ATAG_MEMCLK structure. */
typedef struct atag_memclk {
	uint32_t fmemclk;
} atag_memclk_t;

/** Full tag structure. */
typedef struct atag {
	atag_header_t hdr;

	union {
		atag_core_t core;
		atag_mem_t mem;
		atag_videotext_t videotext;
		atag_ramdisk_t ramdisk;
		atag_initrd_t initrd;
		atag_serial_t serialnr;
		atag_revision_t revision;
		atag_videolfb_t videolfb;
		atag_cmdline_t cmdline;
		atag_acorn_t acorn;
		atag_memclk_t memclk;
	};
} atag_t;

extern atag_t *atag_list;

/** Iterate over the ATAG list. */
#define ATAG_ITERATE(var, _type)	\
	for(atag_t *var = atag_list; var->hdr.tag != ATAG_NONE; \
			var = (atag_t *)((ptr_t)var + (var->hdr.size * 4))) \
		if(var->hdr.tag == _type)

#endif /* __ARM_ATAG_H */
