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
 * @brief		PC platform LAOS loader.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <loaders/laos.h>

#include <pc/bios.h>
#include <pc/console.h>
#include <pc/vbe.h>
#include <pc/memory.h>

#include <config.h>
#include <memory.h>
#include <ui.h>

/** Default video mode parameters. */
static laos_itag_video_t default_video_itag = {
	.types = LAOS_VIDEO_VGA,
	.width = 0,
	.height = 0,
	.bpp = 0,
};

/** Parse the video mode string.
 * @param string	Mode string.
 * @param modep		Where to store VBE mode, if any.
 * @return		Video mode type, or 0 if invalid/not found. */
static uint32_t parse_video_mode(const char *string, vbe_mode_t **modep) {
	uint16_t width = 0, height = 0;
	char *dup, *orig, *tok;
	uint8_t depth = 0;

	if(strcmp(string, "vga") == 0)
		return LAOS_VIDEO_VGA;

	dup = orig = kstrdup(string);

	if((tok = strsep(&dup, "x")))
		width = strtol(tok, NULL, 0);
	if((tok = strsep(&dup, "x")))
		height = strtol(tok, NULL, 0);
	if((tok = strsep(&dup, "x")))
		depth = strtol(tok, NULL, 0);

	kfree(orig);

	if(width && height) {
		*modep = vbe_mode_find(width, height, depth);
		return (*modep) ? LAOS_VIDEO_LFB : 0;
	}

	return 0;
}

#if CONFIG_LAOS_UI

/** Create the UI chooser.
 * @param loader	LAOS loader data structure.
 * @param types		Supported video mode types.
 * @param entry		Environment entry to modify. */
static void create_mode_chooser(laos_loader_t *loader, uint32_t types, value_t *entry) {
	ui_entry_t *chooser;
	vbe_mode_t *mode;
	value_t value;
	char buf[16];

	value.type = VALUE_TYPE_STRING;
	value.string = buf;

	/* There will only be more than 1 choice if using VBE. */
	if(!(types & LAOS_VIDEO_LFB) || list_empty(&vbe_modes))
		return;

	chooser = ui_chooser_create("Video Mode", entry);

	if(types & LAOS_VIDEO_VGA) {
		sprintf(buf, "vga");
		ui_chooser_insert(chooser, "VGA", &value);
	}

	LIST_FOREACH(&vbe_modes, iter) {
		mode = list_entry(iter, vbe_mode_t, header);

		sprintf(buf, "%ux%ux%u", mode->info.x_resolution,
			mode->info.y_resolution,
			mode->info.bits_per_pixel);
		ui_chooser_insert(chooser, NULL, &value);
	}

	ui_list_insert(loader->config, chooser, false);
}

#endif

/** Determine the default video mode.
 * @param tag		Video mode tag.
 * @param modep		Where to store default mode.
 * @return		Type of the mode, or 0 on failure. */
static uint32_t get_default_mode(laos_itag_video_t *tag, vbe_mode_t **modep) {
	vbe_mode_t *mode;

	if(tag->types & LAOS_VIDEO_LFB) {
		/* If the kernel specifies a preferred mode, try to find it. */
		mode = (tag->width && tag->height)
			? vbe_mode_find(tag->width, tag->height, tag->bpp)
			: NULL;
		if(!mode)
			mode = default_vbe_mode;

		
		if(mode) {
			*modep = mode;
			return LAOS_VIDEO_LFB;
		}
	}

	if(tag->types & LAOS_VIDEO_VGA)
		return LAOS_VIDEO_VGA;

	return 0;
}

/** Parse video parameters in a LAOS image.
 * @param loader	LAOS loader data structure. */
void laos_platform_video_init(laos_loader_t *loader) {
	laos_itag_video_t *tag;
	vbe_mode_t *mode = NULL;
	value_t *entry, value;
	uint32_t type = 0;
	char buf[16];

	tag = laos_itag_find(loader, LAOS_ITAG_VIDEO);
	if(!tag)
		tag = &default_video_itag;

	/* If the kernel doesn't want anything, we don't need to do anything. */
	if(!tag->types) {
		environ_remove(current_environ, "video_mode");
		return;
	}

	/* Check if we already have a valid video mode set in the environment. */
	entry = environ_lookup(current_environ, "video_mode");
	if(entry && entry->type == VALUE_TYPE_STRING)
		type = tag->types & parse_video_mode(entry->string, &mode);

	/* Get the default mode if there wasn't or it is not valid. */
	if(!type)
		type = get_default_mode(tag, &mode);

	/* If there's still nothing, we can't give video information to the
	 * kernel so just remove the environment variable and return. */
	if(!type) {
		environ_remove(current_environ, "video_mode");
		return;
	}

	/* Save the mode in a properly formatted string. */
	switch(type) {
	case LAOS_VIDEO_LFB:
		sprintf(buf, "%ux%ux%u", mode->info.x_resolution,
			mode->info.y_resolution,
			mode->info.bits_per_pixel);
		break;
	case LAOS_VIDEO_VGA:
		sprintf(buf, "vga");
		break;
	}

	value.type = VALUE_TYPE_STRING;
	value.string = buf;
	entry = environ_insert(current_environ, "video_mode", &value);

	#if CONFIG_LAOS_UI
	/* Add a video mode chooser to the UI. */
	create_mode_chooser(loader, tag->types, entry);
	#endif
}

/** Set the video mode.
 * @param loader	LAOS loader data structure. */
static void set_video_mode(laos_loader_t *loader) {
	vbe_mode_t *mode = NULL;
	laos_tag_video_t *tag;
	value_t *entry;
	uint32_t type;

	/* In laos_platform_video_init(), we ensure that the video_mode
	 * environment variable contains a valid mode, so we just need to
	 * grab that and set it. */
	entry = environ_lookup(current_environ, "video_mode");
	if(!entry)
		return;

	type = parse_video_mode(entry->string, &mode);

	/* Create the tag. */
	tag = laos_allocate_tag(loader, LAOS_TAG_VIDEO, sizeof(*tag));
	tag->type = type;

	/* Set the mode and save information. */
	switch(type) {
	case LAOS_VIDEO_VGA:
		tag->vga.cols = 80;
		tag->vga.lines = 25;
		vga_cursor_position(&tag->vga.x, &tag->vga.y);
		tag->vga.mem_phys = VGA_MEM_BASE;
		tag->vga.mem_size = ROUND_UP(tag->vga.cols * tag->vga.lines * 2, PAGE_SIZE);
		tag->vga.mem_virt = laos_allocate_virtual(loader, tag->vga.mem_phys,
			tag->vga.mem_size);
		break;
	case LAOS_VIDEO_LFB:
		tag->lfb.flags = (mode->info.memory_model == 4) ? LAOS_LFB_INDEXED : LAOS_LFB_RGB;
		tag->lfb.width = mode->info.x_resolution;
		tag->lfb.height = mode->info.y_resolution;
		tag->lfb.bpp = mode->info.bits_per_pixel;
		tag->lfb.pitch = (vbe_info.vbe_version_major >= 3)
			? mode->info.lin_bytes_per_scan_line
			: mode->info.bytes_per_scan_line;

		if(tag->lfb.flags & LAOS_LFB_RGB) {
			tag->lfb.red_size = mode->info.red_mask_size;
			tag->lfb.red_pos = mode->info.red_field_position;
			tag->lfb.green_size = mode->info.green_mask_size;
			tag->lfb.green_pos = mode->info.green_field_position;
			tag->lfb.blue_size = mode->info.blue_mask_size;
			tag->lfb.blue_pos = mode->info.blue_field_position;
		} else if(tag->lfb.flags & LAOS_LFB_INDEXED) {
			boot_error("TODO: Indexed video modes");
		}

		/* Map the framebuffer. */
		tag->lfb.fb_phys = mode->info.phys_base_ptr;
		tag->lfb.fb_size = ROUND_UP(tag->lfb.height * tag->lfb.pitch, PAGE_SIZE);
		tag->lfb.fb_virt = laos_allocate_virtual(loader, tag->lfb.fb_phys,
			tag->lfb.fb_size);

		/* Set the mode. */
		vbe_mode_set(mode);
		break;
	}
}

/** Add E820 memory map tags.
 * @param loader	LAOS loader data structure. */
static void add_e820_tags(laos_loader_t *loader) {
	laos_tag_t *tag;
	bios_regs_t regs;

	bios_regs_init(&regs);

	do {
		regs.eax = 0xE820;
		regs.edx = E820_SMAP;
		regs.ecx = 64;
		regs.edi = BIOS_MEM_BASE;
		bios_interrupt(0x15, &regs);

		/* If CF is set, the call was not successful. BIOSes are
		 * allowed to return a non-zero continuation value in EBX and
		 * return an error on next call to indicate that the end of the
		 * list has been reached. */
		if(regs.eflags & X86_FLAGS_CF)
			break;

		/* Create a tag for the entry. */
		tag = laos_allocate_tag(loader, LAOS_TAG_E820, sizeof(*tag) + regs.ecx);
		memcpy(&tag[1], (void *)BIOS_MEM_BASE, regs.ecx);
	} while(regs.ebx != 0);
}

/** Perform platform-specific setup for a LAOS kernel.
 * @param loader	LAOS loader data structure. */
void laos_platform_setup(laos_loader_t *loader) {
	/* Set the video mode. */
	set_video_mode(loader);

	/* Add a copy of the E820 memory map. */
	add_e820_tags(loader);
}
