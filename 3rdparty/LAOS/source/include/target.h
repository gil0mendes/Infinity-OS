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
 * @brief		Target system definitions.
 *
 * On some platforms a single build of the loader may support loading kernels
 * for multiple different configurations (targets), for example the x86 loader
 * can load both 32- and 64-bit kernels. This file provides definitions to help
 * deal with multiple targets.
 *
 * @todo		Multi-endian systems?
 */

#ifndef __TARGET_H
#define __TARGET_H

#include <types.h>

/** Integer types that can represent pointers/sizes for all targets. */
typedef uint64_t target_ptr_t;
typedef uint64_t target_size_t;

/** Possible target operating mode types. */
typedef enum target_type {
	TARGET_TYPE_32BIT,		/**< 32-bit. */
	TARGET_TYPE_64BIT,		/**< 64-bit. */
} target_type_t;

#endif /* __TARGET_H */
