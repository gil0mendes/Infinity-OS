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
 * @brief		x86 MMU definitions.
 */

#ifndef __X86_MMU_H
#define __X86_MMU_H

#include <mmu.h>

/** Definitions of paging structure bits. */
#define X86_PTE_PRESENT		(1<<0)	/**< Page is present. */
#define X86_PTE_WRITE		(1<<1)	/**< Page is writable. */
#define X86_PTE_USER		(1<<2)	/**< Page is accessible in CPL3. */
#define X86_PTE_PWT		(1<<3)	/**< Page has write-through caching. */
#define X86_PTE_PCD		(1<<4)	/**< Page has caching disabled. */
#define X86_PTE_ACCESSED	(1<<5)	/**< Page has been accessed. */
#define X86_PTE_DIRTY		(1<<6)	/**< Page has been written to. */
#define X86_PTE_LARGE		(1<<7)	/**< Page is a large page. */
#define X86_PTE_GLOBAL		(1<<8)	/**< Page won't be cleared in TLB. */

/** x86 MMU context structure. */
struct mmu_context {
	uint32_t cr3;			/**< Value loaded into CR3. */
	bool is64;			/**< Whether this is a 64-bit context. */
	unsigned phys_type;		/**< Physical memory type for page tables. */
};

#endif /* __X86_MMU_H */
