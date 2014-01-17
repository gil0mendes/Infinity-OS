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
 * @brief		Console functions.
 */

#include <lib/printf.h>

#include <console.h>
#include <loader.h>

/** Debug output log. */
char debug_log[DEBUG_LOG_SIZE];
size_t debug_log_start = 0;
size_t debug_log_length = 0;

/** Main console. */
console_t *main_console = NULL;

/** Debug console. */
console_t *debug_console = NULL;

/** Helper for kvprintf().
 * @param ch		Character to display.
 * @param data		Console to use.
 * @param total		Pointer to total character count. */
static void kvprintf_helper(char ch, void *data, int *total) {
	if(main_console)
		main_console->putch(ch);

	*total = *total + 1;
}

/** Output a formatted message to the console.
 * @param fmt		Format string used to create the message.
 * @param args		Arguments to substitute into format.
 * @return		Number of characters printed. */
int kvprintf(const char *fmt, va_list args) {
	return do_printf(kvprintf_helper, NULL, fmt, args);
}

/** Output a formatted message to the console.
 * @param fmt		Format string used to create the message.
 * @param ...		Arguments to substitute into format.
 * @return		Number of characters printed. */
int kprintf(const char *fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = kvprintf(fmt, args);
	va_end(args);

	return ret;
}

/** Helper for dvprintf().
 * @param ch		Character to display.
 * @param data		Console to use.
 * @param total		Pointer to total character count. */
static void dvprintf_helper(char ch, void *data, int *total) {
	if(debug_console)
		debug_console->putch(ch);

	/* Store in the log buffer. */
	debug_log[(debug_log_start + debug_log_length) % DEBUG_LOG_SIZE] = ch;
	if(debug_log_length < DEBUG_LOG_SIZE) {
		debug_log_length++;
	} else {
		debug_log_start = (debug_log_start + 1) % DEBUG_LOG_SIZE;
	}

	*total = *total + 1;
}

/** Output a formatted message to the debug console.
 * @param fmt		Format string used to create the message.
 * @param args		Arguments to substitute into format.
 * @return		Number of characters printed. */
int dvprintf(const char *fmt, va_list args) {
	return do_printf(dvprintf_helper, NULL, fmt, args);
}

/** Output a formatted message to the debug console.
 * @param fmt		Format string used to create the message.
 * @param ...		Arguments to substitute into format.
 * @return		Number of characters printed. */
int dprintf(const char *fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = dvprintf(fmt, args);
	va_end(args);

	return ret;
}
