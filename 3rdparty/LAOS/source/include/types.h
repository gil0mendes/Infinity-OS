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
 * @brief		Type definitions.
 */

#ifndef __TYPES_H
#define __TYPES_H

#include <arch/types.h>

#include <compiler.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

/** Extra integer types. */
typedef uint64_t offset_t;		/**< Type used to store an offset into a object. */
typedef int64_t timeout_t;		/**< Type used to store a time value in microseconds. */

/** Type limit macros. */
#define INT8_MIN	(-128)
#define INT8_MAX	127
#define UINT8_MAX	255u
#define INT16_MIN	(-32767-1)
#define INT16_MAX	32767
#define UINT16_MAX	65535u
#define INT32_MIN	(-2147483647-1)
#define INT32_MAX	2147483647
#define UINT32_MAX	4294967295u
#define INT64_MIN	(-9223372036854775807ll-1)
#define INT64_MAX	9223372036854775807ll
#define UINT64_MAX	18446744073709551615ull

#endif /* __TYPES_H */
