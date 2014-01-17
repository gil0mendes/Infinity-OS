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
 * @brief		Memory management functions.
 */

#ifndef __MEMORY_H
#define __MEMORY_H

#include <arch/page.h>

#include <lib/list.h>

/** Structure used to represent a physical memory range. */
typedef struct memory_range {
	list_t header;			/**< Link to range list. */

	phys_ptr_t start;		/**< Start of the range. */
	phys_size_t size;		/**< Size of the range. */
	unsigned type;			/**< Type of the range. */
} memory_range_t;

extern list_t memory_ranges;

/** Physical memory range types.
 * @note		These should be the same as the LAOS definitions. */
#define PHYS_MEMORY_FREE	0
#define PHYS_MEMORY_ALLOCATED	1
#define PHYS_MEMORY_RECLAIMABLE 2
#define PHYS_MEMORY_PAGETABLES	3
#define PHYS_MEMORY_STACK	4
#define PHYS_MEMORY_MODULES	5
#define PHYS_MEMORY_INTERNAL	6

/** Flags for phys_memory_alloc(). */
#define PHYS_ALLOC_CANFAIL	(1<<0)	/**< The allocation is allowed to fail. */
#define PHYS_ALLOC_HIGH		(1<<1)	/**< Allocate the highest possible address. */

extern void *kmalloc(size_t size);
extern void *krealloc(void *addr, size_t size);
extern void kfree(void *addr);

extern void phys_memory_add(phys_ptr_t start, phys_size_t size, unsigned type);
extern void phys_memory_protect(phys_ptr_t start, phys_size_t size);
extern bool phys_memory_alloc(phys_size_t size, phys_size_t align, phys_ptr_t min_addr,
	phys_ptr_t max_addr, unsigned type, unsigned flags, phys_ptr_t *physp);

extern void platform_memory_detect(void);

extern void memory_init(void);
extern void memory_finalize(void);

#endif /* __MEMORY_H */
