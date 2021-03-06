/*
 * Copyright (C) 2009-2013 Gil Mendes
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		AMD64 MMU context definitions.
 */

#ifndef __ARCH_MMU_H
#define __ARCH_MMU_H

#include <types.h>

/** Size of TLB flush array. */
#define INVALIDATE_ARRAY_SIZE	128

/** AMD64 MMU context structure. */
typedef struct arch_mmu_context {
	phys_ptr_t pml4;		/**< Physical address of the PML4. */

	/** Array of TLB entries to flush when unlocking context.
	 * @note		If the count becomes greater than the array
	 *			size, then the entire TLB will be flushed. */
	ptr_t pages_to_invalidate[INVALIDATE_ARRAY_SIZE];
	size_t invalidate_count;
} arch_mmu_context_t;

#endif /* __ARCH_MMU_H */
