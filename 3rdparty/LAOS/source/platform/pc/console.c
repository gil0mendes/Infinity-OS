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
 * @brief		PC console code.
 */

#include <arch/io.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <pc/bios.h>
#include <pc/console.h>

#include <console.h>
#include <loader.h>

/** Define the serial port to use. */
#define SERIAL_PORT		0x3F8		/**< COM1. */
//#define SERIAL_PORT		0x2F8		/**< COM2. */
//#define SERIAL_PORT		0x3E8		/**< COM3. */
//#define SERIAL_PORT		0x2E8		/**< COM4. */

/** VGA attributes. */
#define VGA_ATTRIB		0x0700
#define VGA_COLS		80
#define VGA_ROWS		25

static void pc_console_clear(int x, int y, int width, int height);
static void pc_console_scroll_down(void);

/** VGA memory pointer. */
static uint16_t *vga_mapping = (uint16_t *)VGA_MEM_BASE;

/** VGA cursor position. */
static int vga_cursor_x = 0;
static int vga_cursor_y = 0;
static bool vga_cursor_visible = true;

/** VGA draw region. */
static draw_region_t vga_region;

/** Get the VGA console cursor position.
 * @param _x		Where to store cursor X position.
 * @param _y		Where to store cursor Y position. */
void vga_cursor_position(uint8_t *_x, uint8_t *_y) {
	*_x = vga_cursor_x;
	*_y = vga_cursor_y;
}

/** Update the hardware cursor. */
static void update_hw_cursor(void) {
	int x = (vga_cursor_visible) ? vga_cursor_x : 0;
	int y = (vga_cursor_visible) ? vga_cursor_y : (VGA_ROWS + 1);
	uint16_t pos = (y * VGA_COLS) + x;

	out8(VGA_CRTC_INDEX, 14);
	out8(VGA_CRTC_DATA, pos >> 8);
	out8(VGA_CRTC_INDEX, 15);
	out8(VGA_CRTC_DATA, pos & 0xFF);
}

/** Reset the VGA console. */
static void pc_console_reset(void) {
	vga_cursor_x = vga_cursor_y = 0;
	vga_region.x = vga_region.y = 0;
	vga_region.width = VGA_COLS;
	vga_region.height = VGA_ROWS;

	vga_cursor_visible = true;
	update_hw_cursor();

	pc_console_clear(0, 0, VGA_COLS, VGA_ROWS);
}

/** Set the VGA console draw region.
 * @param region	Region to set. */
static void pc_console_set_region(draw_region_t *region) {
	vga_region = *region;
	vga_cursor_x = vga_region.x;
	vga_cursor_y = vga_region.y;
	update_hw_cursor();
}

/** Get the VGA console draw region.
 * @param region	Region structure to fill in. */
static void pc_console_get_region(draw_region_t *region) {
	*region = vga_region;
}

/** Write a character to the VGA console.
 * @param ch		Character to write. */
static void pc_console_putch(char ch) {
	switch(ch) {
	case '\b':
		/* Backspace, move back one character if we can. */
		if(vga_cursor_x > vga_region.x) {
			vga_cursor_x--;
		} else if(vga_cursor_y > vga_region.y) {
			vga_cursor_x = vga_region.x + vga_region.width - 1;
			vga_cursor_y--;
		}
		break;
	case '\r':
		/* Carriage return, move to the start of the line. */
		vga_cursor_x = vga_region.x;
		break;
	case '\n':
		/* Newline, treat it as if a carriage return was also there. */
		vga_cursor_x = vga_region.x;
		vga_cursor_y++;
		break;
	case '\t':
		vga_cursor_x += 8 - (vga_cursor_x % 8);
		break;
	default:
		/* If it is a non-printing character, ignore it. */
		if(ch < ' ')
			break;

		vga_mapping[(vga_cursor_y * VGA_COLS) + vga_cursor_x] &= 0xFF00;
		vga_mapping[(vga_cursor_y * VGA_COLS) + vga_cursor_x] |= ch;
		vga_cursor_x++;
		break;
	}

	/* If we have reached the edge of the region insert a new line. */
	if(vga_cursor_x >= (vga_region.x + vga_region.width)) {
		vga_cursor_x = vga_region.x;
		vga_cursor_y++;
	}

	/* Scroll if we've reached the end of the draw region. */
	if(vga_cursor_y >= (vga_region.y + vga_region.height)) {
		if(vga_region.scrollable)
			pc_console_scroll_down();

		vga_cursor_y = vga_region.y + vga_region.height - 1;
	}

	update_hw_cursor();
}

/** Change the highlight on a portion of the console.
 * @note		Position is relative to the draw region.
 * @param x		Start X position.
 * @param y		Start Y position.
 * @param width		Width of the highlight.
 * @param height	Height of the highlight. */
static void pc_console_highlight(int x, int y, int width, int height) {
	uint16_t word, fg, bg;
	int i, j;

	for(i = vga_region.y + y; i < (vga_region.y + y + height); i++) {
		for(j = vga_region.x + x; j < (vga_region.x + x + width); j++) {
			/* Swap the foreground/background colour. */
			word = vga_mapping[(i * VGA_COLS) + j];
			fg = (word << 4) & 0xF000;
			bg = (word >> 4) & 0x0F00;
			vga_mapping[(i * VGA_COLS) + j] = (word & 0xFF) | fg | bg;
		}
	}
}

/** Clear a portion of the console.
 * @note		Position is relative to the draw region.
 * @param x		Start X position.
 * @param y		Start Y position.
 * @param width		Width of the highlight.
 * @param height	Height of the highlight. */
static void pc_console_clear(int x, int y, int width, int height) {
	int i, j;

	for(i = vga_region.y + y; i < (vga_region.y + y + height); i++) {
		for(j = vga_region.x + x; j < (vga_region.x + x + width); j++)
			vga_mapping[(i * VGA_COLS) + j] = ' ' | VGA_ATTRIB;
	}
}

/** Move the cursor.
 * @note		Position is relative to the draw region.
 * @param x		New X position.
 * @param y		New Y position. */
static void pc_console_move_cursor(int x, int y) {
	if(x < 0) {
		vga_cursor_x = vga_region.x + vga_region.width + x;
	} else {
		vga_cursor_x = vga_region.x + x;
	}
	if(y < 0) {
		vga_cursor_y = vga_region.y + vga_region.height + y;
	} else {
		vga_cursor_y = vga_region.y + y;
	}

	update_hw_cursor();
}

/** Set whether the cursor is visible.
 * @param visible	Whether the cursor is visible. */
static void pc_console_show_cursor(bool visible) {
	vga_cursor_visible = visible;
	update_hw_cursor();
}

/** Scroll the console up by one row. */
static void pc_console_scroll_up(void) {
	int i;

	/* Shift down the content of the VGA memory. */
	for(i = 0; i < (vga_region.height - 1); i++) {
		memcpy(vga_mapping + vga_region.x + (VGA_COLS * (vga_region.y + vga_region.height - i - 1)),
			vga_mapping + vga_region.x + (VGA_COLS * (vga_region.y + vga_region.height - i - 2)),
			vga_region.width * 2);
	}

	/* Fill the first row with blanks. */
	for(i = 0; i < vga_region.width; i++) {
		vga_mapping[(vga_region.y * VGA_COLS) + vga_region.x + i] &= 0xFF00;
		vga_mapping[(vga_region.y * VGA_COLS) + vga_region.x + i] |= ' ';
	}
}

/** Scroll the console down by one row. */
static void pc_console_scroll_down(void) {
	int i;

	/* Shift up the content of the VGA memory. */
	for(i = 0; i < (vga_region.height - 1); i++) {
		memcpy(vga_mapping + vga_region.x + (VGA_COLS * (vga_region.y + i)),
			vga_mapping + vga_region.x + (VGA_COLS * (vga_region.y + i + 1)),
			vga_region.width * 2);
	}

	/* Fill the last row with blanks. */
	for(i = 0; i < vga_region.width; i++) {
		vga_mapping[((vga_region.y + vga_region.height - 1) * VGA_COLS) + vga_region.x + i] &= 0xFF00;
		vga_mapping[((vga_region.y + vga_region.height - 1) * VGA_COLS) + vga_region.x + i] |= ' ';
	}
}

/** Read a key from the console.
 * @return		Key read from the console. */
static uint16_t pc_console_get_key(void) {
	uint8_t ascii, scan;
	bios_regs_t regs;

	bios_regs_init(&regs);

	/* INT16 AH=00h on Apple's BIOS emulation will hang forever if there
	 * is no key available, so loop trying to poll for one first. */
	do {
		regs.eax = 0x0100;
		bios_interrupt(0x16, &regs);
	} while(regs.eflags & X86_FLAGS_ZF);

	/* Get the key code. */
	regs.eax = 0x0000;
	bios_interrupt(0x16, &regs);

	/* Convert certain scan codes to special keys. */
	ascii = regs.eax & 0xFF;
	scan = (regs.eax >> 8) & 0xFF;
	switch(scan) {
	case 0x48:
		return CONSOLE_KEY_UP;
	case 0x50:
		return CONSOLE_KEY_DOWN;
	case 0x4B:
		return CONSOLE_KEY_LEFT;
	case 0x4D:
		return CONSOLE_KEY_RIGHT;
	case 0x47:
		return CONSOLE_KEY_HOME;
	case 0x4F:
		return CONSOLE_KEY_END;
	case 0x53:
		return CONSOLE_KEY_DELETE;
	case 0x3B ... 0x44:
		return CONSOLE_KEY_F1 + (scan - 0x3B);
	default:
		/* Convert CR to LF. */
		return (ascii == '\r') ? '\n' : ascii;
	}
}

/** Check if a key is waiting to be read.
 * @return		Whether a key is waiting to be read. */
static bool pc_console_check_key(void) {
	bios_regs_t regs;

	bios_regs_init(&regs);
	regs.eax = 0x0100;
	bios_interrupt(0x16, &regs);
	return !(regs.eflags & X86_FLAGS_ZF);
}

/** Main console. */
static console_t pc_console = {
	.width = VGA_COLS,
	.height = VGA_ROWS,

	.reset = pc_console_reset,
	.set_region = pc_console_set_region,
	.get_region = pc_console_get_region,
	.putch = pc_console_putch,
	.highlight = pc_console_highlight,
	.clear = pc_console_clear,
	.move_cursor = pc_console_move_cursor,
	.show_cursor = pc_console_show_cursor,
	.scroll_up = pc_console_scroll_up,
	.scroll_down = pc_console_scroll_down,
	.get_key = pc_console_get_key,
	.check_key = pc_console_check_key,
};

#ifdef SERIAL_PORT
/** Write a character to the serial console.
 * @param ch		Character to write. */
static void serial_console_putch(char ch) {
	if(ch == '\n')
		serial_console_putch('\r');

	out8(SERIAL_PORT, ch);
	while(!(in8(SERIAL_PORT + 5) & 0x20));
}

/** Debug console. */
static console_t serial_console = {
	.putch = serial_console_putch,
};
#endif

/** Initialise the console. */
void console_init(void) {
#ifdef SERIAL_PORT
	uint8_t status;

	/* Only enable the serial port when it is present. */
	status = in8(SERIAL_PORT + 6);
	if((status & ((1<<4) | (1<<5))) && status != 0xFF) {
		out8(SERIAL_PORT + 1, 0x00);  /* Disable all interrupts */
		out8(SERIAL_PORT + 3, 0x80);  /* Enable DLAB (set baud rate divisor) */
		out8(SERIAL_PORT + 0, 0x03);  /* Set divisor to 3 (lo byte) 38400 baud */
		out8(SERIAL_PORT + 1, 0x00);  /*                  (hi byte) */
		out8(SERIAL_PORT + 3, 0x03);  /* 8 bits, no parity, one stop bit */
		out8(SERIAL_PORT + 2, 0xC7);  /* Enable FIFO, clear them, with 14-byte threshold */
		out8(SERIAL_PORT + 4, 0x0B);  /* IRQs enabled, RTS/DSR set */

		/* Wait for transmit to be empty. */
		while(!(in8(SERIAL_PORT + 5) & 0x20));

		debug_console = &serial_console;
	}
#endif
	pc_console_reset();
	main_console = &pc_console;
}
