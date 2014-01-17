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
 * @brief		PC platform Linux loader.
 */

#include <x86/linux.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <pc/bios.h>
#include <pc/console.h>
#include <pc/memory.h>
#include <pc/vbe.h>

#include <loader.h>
#include <memory.h>

/** Get memory information.
 * @param params	Kernel boot paramters page.
 * @return		Whether any method succeeded. */
static bool get_memory_info(linux_params_t *params) {
	bool success = false;
	uint8_t count = 0;
	bios_regs_t regs;

	/* First try function AX=E820h. */
	bios_regs_init(&regs);
	do {
		regs.eax = 0xE820;
		regs.edx = E820_SMAP;
		regs.ecx = 20;
		regs.edi = BIOS_MEM_BASE;
		bios_interrupt(0x15, &regs);

		/* If CF is set, the call was not successful. BIOSes are
		 * allowed to return a non-zero continuation value in EBX and
		 * return an error on next call to indicate that the end of the
		 * list has been reached. */
		if(regs.eflags & X86_FLAGS_CF)
			break;

		memcpy(&params->e820_map[count], (void *)BIOS_MEM_BASE, sizeof(params->e820_map[count]));
		count++;
	} while(regs.ebx != 0 && count < ARRAY_SIZE(params->e820_map));

	if((params->e820_entries = count))
		success = true;

	/* Try function AX=E801h. */
	bios_regs_init(&regs);
	regs.eax = 0xE801;
	bios_interrupt(0x15, &regs);
	if(!(regs.eflags & X86_FLAGS_CF)) {
		if(regs.cx || regs.dx) {
			regs.ax = regs.cx;
			regs.bx = regs.dx;
		}

		/* Maximum value is 15MB. */
		if(regs.ax <= 0x3C00) {
			success = true;
			if(regs.ax == 0x3C00) {
				params->alt_mem_k += (regs.bx << 6) + regs.ax;
			} else {
				params->alt_mem_k = regs.ax;
			}
		}
	}

	/* Finally try AH=88h. */
	bios_regs_init(&regs);
	regs.eax = 0x8800;
	bios_interrupt(0x15, &regs);
	if(!(regs.eflags & X86_FLAGS_CF)) {
		/* Why this is under screen_info is beyond me... */
		params->screen_info.ext_mem_k = regs.ax;
		success = true;
	}

	return success;
}

/** Get APM BIOS information.
 * @param params	Kernel boot paramters page. */
static void get_apm_info(linux_params_t *params) {
	bios_regs_t regs;

	bios_regs_init(&regs);
	regs.eax = 0x5300;
	bios_interrupt(0x15, &regs);
	if(regs.eflags & X86_FLAGS_CF || regs.bx != 0x504D || !(regs.cx & (1<<1)))
		return;

	/* Connect 32-bit interface. */
	regs.eax = 0x5304;
	bios_interrupt(0x15, &regs);
	regs.eax = 0x5303;
	bios_interrupt(0x15, &regs);
	if(regs.eflags & X86_FLAGS_CF)
		return;

	params->apm_bios_info.cseg = regs.ax;
	params->apm_bios_info.offset = regs.ebx;
	params->apm_bios_info.cseg_16 = regs.cx;
	params->apm_bios_info.dseg = regs.dx;
	params->apm_bios_info.cseg_len = regs.si;
	params->apm_bios_info.cseg_16_len = regs.esi >> 16;
	params->apm_bios_info.dseg_len = regs.di;

	regs.eax = 0x5300;
	bios_interrupt(0x15, &regs);
	if(regs.eflags & X86_FLAGS_CF || regs.bx != 0x504D) {
		/* Failed to connect 32-bit interface, disconnect. */
		regs.eax = 0x5304;
		bios_interrupt(0x15, &regs);
		return;
	}

	params->apm_bios_info.version = regs.ax;
	params->apm_bios_info.flags = regs.cx;
}

/** Get Intel SpeedStep BIOS information.
 * @param params	Kernel boot paramters page. */
static void get_ist_info(linux_params_t *params) {
	bios_regs_t regs;

	bios_regs_init(&regs);
	regs.eax = 0xE980;
	regs.edx = 0x47534943;
	bios_interrupt(0x15, &regs);

	params->ist_info.signature = regs.eax;
	params->ist_info.command = regs.ebx;
	params->ist_info.event = regs.ecx;
	params->ist_info.perf_level = regs.edx;
}

/** Parse video mode parameters.
 * @param modep		Where to store VBE mode pointer.
 * @return		Video mode type. */
static uint8_t get_video_mode(vbe_mode_t **modep) {
	uint16_t width = 0, height = 0;
	char *dup, *orig, *tok;
	uint8_t depth = 0;
	value_t *entry;

	// TODO: We should recognize vga= on the command line and convert it
	// to our representation.

	/* Check if we have a valid video mode set in the environment. */
	entry = environ_lookup(current_environ, "video_mode");
	if(!entry || entry->type != VALUE_TYPE_STRING)
		return LINUX_VIDEO_TYPE_VGA;

	if(strcmp(entry->string, "vga") == 0)
		return LINUX_VIDEO_TYPE_VGA;

	dup = orig = kstrdup(entry->string);

	if((tok = strsep(&dup, "x")))
		width = strtol(tok, NULL, 0);
	if((tok = strsep(&dup, "x")))
		height = strtol(tok, NULL, 0);
	if((tok = strsep(&dup, "x")))
		depth = strtol(tok, NULL, 0);

	kfree(orig);

	if(width && height) {
		*modep = vbe_mode_find(width, height, depth);
		if(*modep)
			return LINUX_VIDEO_TYPE_VESA;
	}

	return LINUX_VIDEO_TYPE_VGA;
}

/** Set up the video mode for the kernel.
 * @param params	Kernel boot paramters page. */
static void set_video_mode(linux_params_t *params) {
	vbe_mode_t *mode = NULL;

	params->screen_info.orig_video_isVGA = get_video_mode(&mode);
	switch(params->screen_info.orig_video_isVGA) {
	case LINUX_VIDEO_TYPE_VGA:
		params->screen_info.orig_video_mode = 0x3;
		params->screen_info.orig_video_cols = 80;
		params->screen_info.orig_video_lines = 25;
		vga_cursor_position(&params->screen_info.orig_x, &params->screen_info.orig_y);
		break;
	case LINUX_VIDEO_TYPE_VESA:
		if(mode->info.memory_model == 4)
			boot_error("TODO: Indexed video modes");

		params->screen_info.lfb_width = mode->info.x_resolution;
		params->screen_info.lfb_height = mode->info.y_resolution;
		params->screen_info.lfb_depth = mode->info.bits_per_pixel;
		params->screen_info.lfb_linelength = (vbe_info.vbe_version_major >= 3)
			? mode->info.lin_bytes_per_scan_line
			: mode->info.bytes_per_scan_line;
		params->screen_info.lfb_base = mode->info.phys_base_ptr;
		params->screen_info.lfb_size = ROUND_UP(params->screen_info.lfb_linelength
			* params->screen_info.lfb_height, 65536) >> 16;
		params->screen_info.red_size = mode->info.red_mask_size;
		params->screen_info.red_pos = mode->info.red_field_position;
		params->screen_info.green_size = mode->info.green_mask_size;
		params->screen_info.green_pos = mode->info.green_field_position;
		params->screen_info.blue_size = mode->info.blue_mask_size;
		params->screen_info.blue_pos = mode->info.blue_field_position;

		/* Set the mode. */
		vbe_mode_set(mode);
		break;
	}
}

/**
 * PC-specific Linux kernel environment setup.
 *
 * Since we use the 32-bit boot protocol, it is the job of this function to
 * replicate the work done by the real-mode bootstrap code. This means getting
 * all the information from the BIOS required by the kernel.
 *
 * @param params	Kernel boot parameters page.
 */
void linux_platform_load(linux_params_t *params) {
	if(!get_memory_info(params))
		boot_error("Failed to get Linux memory information");

	get_apm_info(params);
	get_ist_info(params);

	/* Don't bother with EDD and MCA, AFAIK they're not used. */

	set_video_mode(params);
}

#if CONFIG_LAOS_UI

/** Add PC-specific Linux configuration options.
 * @param window	Configuration window. */
void linux_platform_configure(ui_window_t *window) {
	value_t *entry, value;
	ui_entry_t *chooser;
	vbe_mode_t *mode;
	uint8_t type;
	char buf[16];

	/* Don't need a mode chooser if there is no VBE support. */
	if(list_empty(&vbe_modes))
		return;

	/* Check for any video mode set in the environment. */
	type = get_video_mode(&mode);

	/* Save the mode in a properly formatted string. */
	switch(type) {
	case LINUX_VIDEO_TYPE_VESA:
		sprintf(buf, "%ux%ux%u", mode->info.x_resolution,
			mode->info.y_resolution,
			mode->info.bits_per_pixel);
		break;
	case LINUX_VIDEO_TYPE_VGA:
		sprintf(buf, "vga");
		break;
	}

	value.type = VALUE_TYPE_STRING;
	value.string = buf;
	entry = environ_insert(current_environ, "video_mode", &value);

	/* Create a mode chooser. */
	chooser = ui_chooser_create("Video Mode", entry);
	sprintf(buf, "vga");
	ui_chooser_insert(chooser, "VGA", &value);

	LIST_FOREACH(&vbe_modes, iter) {
		mode = list_entry(iter, vbe_mode_t, header);

		sprintf(buf, "%ux%ux%u", mode->info.x_resolution,
			mode->info.y_resolution,
			mode->info.bits_per_pixel);
		ui_chooser_insert(chooser, NULL, &value);
	}

	ui_list_insert(window, chooser, false);
}

#endif
