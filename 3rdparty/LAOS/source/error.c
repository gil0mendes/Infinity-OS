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
 * @brief		Boot error handling functions.
 */

#include <lib/printf.h>

#include <loader.h>
#include <memory.h>
#include <ui.h>

/** Boot error message. */
static const char *boot_error_format;
static va_list boot_error_args;

#if CONFIG_LAOS_UI

/** Boot error window state. */
static ui_window_t *boot_error_window;
static ui_window_t *debug_log_window;

#endif

/** Helper for internal_error_printf().
 * @param ch		Character to display.
 * @param data		Ignored.
 * @param total		Pointer to total character count. */
static void internal_error_printf_helper(char ch, void *data, int *total) {
	if(debug_console)
		debug_console->putch(ch);
	if(main_console)
		main_console->putch(ch);

	*total = *total + 1;
}

/** Formatted print function for internal_error(). */
static int internal_error_printf(const char *fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = do_printf(internal_error_printf_helper, NULL, fmt, args);
	va_end(args);

	return ret;
}

/** Raise an internal error.
 * @param fmt		Error format string.
 * @param ...		Values to substitute into format. */
void __noreturn internal_error(const char *fmt, ...) {
	va_list args;

	if(main_console)
		main_console->reset();

	internal_error_printf("\nAn internal error has occurred:\n\n");

	va_start(args, fmt);
	do_printf(internal_error_printf_helper, NULL, fmt, args);
	va_end(args);

	internal_error_printf("\n\n");
	internal_error_printf("Please report this error to https://groups.google.com/d/forum/infinity-os\n");
	internal_error_printf("Backtrace:\n");
	backtrace(internal_error_printf);
	while(1);
}

/** Helper for boot_error_printf().
 * @param ch		Character to display.
 * @param data		Console to print to.
 * @param total		Pointer to total character count. */
static void boot_error_printf_helper(char ch, void *data, int *total) {
	console_t *console = data;
	if(console)
		console->putch(ch);

	*total = *total + 1;
}

/** Formatted print function for boot_error(). */
static int boot_error_printf(console_t *console, const char *fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = do_printf(boot_error_printf_helper, console, fmt, args);
	va_end(args);

	return ret;
}

/** Print the boot error message. */
static void boot_error_display(console_t *console) {
	boot_error_printf(console, "An error has occurred during boot:\n\n");

	do_printf(boot_error_printf_helper, console, boot_error_format, boot_error_args);

	boot_error_printf(console, "\n\n");
	boot_error_printf(console, "Ensure that you have enough memory available, that you do not have any\n");
	boot_error_printf(console, "malfunctioning hardware and that your computer meets the minimum system\n");
	boot_error_printf(console, "requirements for the operating system.\n");
}

#if CONFIG_LAOS_UI

/** Render the boot error window.
 * @param window	Window to render. */
static void boot_error_window_render(ui_window_t *window) {
	boot_error_display(main_console);
}

/** Write the help text for the boot error window.
 * @param window	Window to write for. */
static void boot_error_window_help(ui_window_t *window) {
	kprintf("F1 = Debug Log  Esc = Reboot");
}

/** Handle input on the boot error window.
 * @param window	Window input was performed on.
 * @param key		Key that was pressed.
 * @return		Input handling result. */
static input_result_t boot_error_window_input(ui_window_t *window, uint16_t key) {
	switch(key) {
	case CONSOLE_KEY_F1:
		ui_window_display(debug_log_window, 0);
		return INPUT_RENDER;
	case '\e':
		platform_reboot();
	default:
		return INPUT_HANDLED;
	}
}

/** Boot error window type. */
static ui_window_type_t boot_error_window_type = {
	.render = boot_error_window_render,
	.help = boot_error_window_help,
	.input = boot_error_window_input,
};

#endif

/** Display details of a boot error.
 * @param fmt		Error format string.
 * @param ...		Values to substitute into format. */
void __noreturn boot_error(const char *fmt, ...) {
	va_list args;

	if(main_console)
		main_console->reset();
	if(debug_console)
		debug_console->putch('\n');

	boot_error_format = fmt;
	va_start(boot_error_args, fmt);
	boot_error_display(debug_console);
	va_end(args);

	#if CONFIG_LAOS_UI
	/* Create the debug log window. */
	debug_log_window = ui_textview_create("Debug Log", debug_log, DEBUG_LOG_SIZE,
		debug_log_start, debug_log_length);

	/* Create the error window and display it. */
	boot_error_window = kmalloc(sizeof(ui_window_t));
	ui_window_init(boot_error_window, &boot_error_window_type, "Boot Error");
	ui_window_display(boot_error_window, 0);
	#else
	boot_error_display(main_console);
	#endif

	while(true);
}
