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
 * @brief		VBE structures/definitions.
 */

#ifndef __PLATFORM_VBE_H
#define __PLATFORM_VBE_H

#include <lib/list.h>

/** VBE information structure. */
typedef struct vbe_info {
	char     vbe_signature[4];
	uint8_t  vbe_version_minor;
	uint8_t  vbe_version_major;
	uint32_t oem_string_ptr;
	uint32_t capabilities;
	uint32_t video_mode_ptr;
	uint16_t total_memory;
	uint16_t oem_software_rev;
	uint32_t oem_vendor_name_ptr;
	uint32_t oem_product_name_ptr;
	uint32_t oem_product_rev_ptr;
	uint8_t  reserved[222];
	uint8_t  oem_data[256];
} __packed vbe_info_t;

/** VBE mode information structure. */
typedef struct vbe_mode_info {
	uint16_t mode_attributes;
	uint8_t  wina_attributes;
	uint8_t  winb_attributes;
	uint16_t win_granularity;
	uint16_t win_size;
	uint16_t wina_segment;
	uint16_t winb_segment;
	uint32_t win_func_ptr;
	uint16_t bytes_per_scan_line;

	/* VBE 1.2 */
	uint16_t x_resolution;
	uint16_t y_resolution;
	uint8_t  x_char_size;
	uint8_t  y_char_size;
	uint8_t  num_planes;
	uint8_t  bits_per_pixel;
	uint8_t  num_banks;
	uint8_t  memory_model;
	uint8_t  bank_size;
	uint8_t  num_image_pages;
	uint8_t  reserved1;

	/* Direct colour fields */
	uint8_t  red_mask_size;
	uint8_t  red_field_position;
	uint8_t  green_mask_size;
	uint8_t  green_field_position;
	uint8_t  blue_mask_size;
	uint8_t  blue_field_position;
	uint8_t  rsvd_mask_size;
	uint8_t  rsvd_field_position;
	uint8_t  direct_color_mode_info;

	/* VBE 2.0 */
	uint32_t phys_base_ptr;
	uint32_t reserved2;
	uint16_t reserved3;

	/* VBE 3.0 */
	uint16_t lin_bytes_per_scan_line;
	uint8_t  bnk_num_image_pages;
	uint8_t  lin_num_image_pages;
	uint8_t  lin_red_mask_size;
	uint8_t  lin_red_field_position;
	uint8_t  lin_green_mask_size;
	uint8_t  lin_green_field_position;
	uint8_t  lin_blue_mask_size;
	uint8_t  lin_blue_field_position;
	uint8_t  lin_rsvd_mask_size;
	uint8_t  lin_rsvd_field_position;
	uint8_t  max_pixel_clock;

	uint8_t  reserved4[189];
} __packed vbe_mode_info_t;

/** Structure describing a VBE video mode. */
typedef struct vbe_mode {
	list_t header;			/**< Link to mode list. */

	uint16_t id;			/**< ID of the mode. */
	vbe_mode_info_t info;		/**< Mode information. */
} vbe_mode_t;

/** VBE function definitions. */
#define VBE_FUNCTION_CONTROLLER_INFO	0x4F00	/**< Return VBE Controller Information. */
#define VBE_FUNCTION_MODE_INFO		0x4F01	/**< Return VBE Mode Information. */
#define VBE_FUNCTION_SET_MODE		0x4F02	/**< Set VBE Mode. */

extern vbe_info_t vbe_info;
extern list_t vbe_modes;
extern vbe_mode_t *default_vbe_mode;

extern void vbe_mode_set(vbe_mode_t *mode);
extern vbe_mode_t *vbe_mode_find(uint16_t width, uint16_t height, uint8_t depth);

extern void vbe_init(void);

#endif /* __PLATFORM_VBE_H */
