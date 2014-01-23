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
 * @brief 	Internal libpulsar definitions.
 */

#ifndef __INTERNAL_H
#define __INTERNAL_H

#include <CoreDefs.h>

// Compiler attribute/builtin macros
#define likely(x)		__builtin_expect(!!(x), 1)
#define unlikely(x)		__builtin_expect(!!(x), 0)

#if CONFIG_DEBUG
extern void libpulsar_debug(const char *fmt, ...) PULSAR_PUBLIC __attribute__((format(printf, 1, 2)));
#else
extern inline void libpulsar_debug(const char *fmt, ...) {};
#endif
extern inline void libpulsar_warn(const char *fmt, ...) PULSAR_PUBLIC __attribute__((format(printf, 1, 2)));
extern void libpulsar_fatal(const char *fmt, ...) PULSAR_PUBLIC __attribute__((format(printf, 1, 2)));

namespace pulsar
{
	class EventLoop;
}

extern __thread pulsar::EventLoop *g_event_loop;

#endif // __INTERNAL_H
