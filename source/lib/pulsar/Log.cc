/*
 * Copyright (C) 2014 Gil Mendes
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
 * @brief 	Internal libpulsar logging functions
 */

#include <kernel/process.h>
#include <kernel/thread.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "Internal.h"

/**
 * Print out a message
 *
 * @param stream 	Stream to print to
 * @param prefix 	What to print before the actual message
 * @param fmt 		Format string
 * @param args 		Argument list
 * @param terminate Whether to terminate the program after printing the
 * 					message.
 */
static void do_log_message(FILE *stream, const char *prefix, const char *fmt, 
		va_list args, bool terminate)
{
	fprintf(stream, "*** %s (%d/%d): ", prefix, kern_process_id(-1), kern_thread_id(-1));
	vfprintf(stream, fmt, args);
	fprintf(stream, "\n");
	if (terminate)
	{
		abort();
	}
}

#if CONFIG_DEBUG
/**
 * Print a debug message
 */
void libpulsar_debug(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	do_log_message(stdout, "DEBUG", fmt, args, false);
	va_end(args);
}
#endif

/**
 * Print a warning message
 */
void libpulsar_warn(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	do_log_message(stderr, "WARNING", fmt, args, false);
	va_end(args);
}

/**
 * Print a fatal error message and exit
 */
void libpulsar_fatal(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	do_log_message(stderr, "FATAL", fmt, args, true);
	va_end(args);
}
