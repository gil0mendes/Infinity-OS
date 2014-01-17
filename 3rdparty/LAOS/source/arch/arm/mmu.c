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
 * @brief		ARM MMU functions.
 */

#include <arm/mmu.h>

#include <lib/string.h>

#include <assert.h>
#include <loader.h>
#include <memory.h>

/** Allocate a paging structure. */
static phys_ptr_t allocate_structure(mmu_context_t *ctx, size_t size) {
	phys_ptr_t addr;

	phys_memory_alloc(size, size, 0, 0, ctx->phys_type, 0, &addr);
	memset((void *)addr, 0, size);
	return addr;
}

/** Map a section in a context.
 * @param ctx		Context to map in.
 * @param virt		Virtual address to map.
 * @param phys		Physical address to map to. */
static void map_section(mmu_context_t *ctx, ptr_t virt, phys_ptr_t phys) {
	uint32_t *l1;
	int l1e;

	assert(!(virt % LARGE_PAGE_SIZE));
	assert(!(phys % LARGE_PAGE_SIZE));

	l1 = (uint32_t *)ctx->l1;
	l1e = virt / LARGE_PAGE_SIZE;
	l1[l1e] = phys | (1<<1) | (1<<10);
}

/** Map a small page in a context.
 * @param ctx		Context to map in.
 * @param virt		Virtual address to map.
 * @param phys		Physical address to map to. */
static void map_small(mmu_context_t *ctx, ptr_t virt, phys_ptr_t phys) {
	uint32_t *l1, *l2;
	phys_ptr_t addr;
	int l1e, l2e;

	l1 = (uint32_t *)ctx->l1;
	l1e = virt / LARGE_PAGE_SIZE;
	if(!(l1[l1e] & (1<<0))) {
		/* FIXME: Second level tables are actually 1KB. Should probably
		 * split up these pages and use them fully. */
		addr = allocate_structure(ctx, PAGE_SIZE);
		l1[l1e] = addr | (1<<0);
	}

	l2 = (uint32_t *)(l1[l1e] & 0xFFFFFC00);
	l2e = (virt % LARGE_PAGE_SIZE) / PAGE_SIZE;
	l2[l2e] = phys | (1<<1) | (1<<4);
}

/** Create a mapping in an MMU context.
 * @param ctx		Context to map in.
 * @param virt		Virtual address to map.
 * @param phys		Physical address to map to.
 * @param size		Size of the mapping to create.
 * @return		Whether created successfully. */
bool mmu_map(mmu_context_t *ctx, target_ptr_t virt, phys_ptr_t phys, target_size_t size) {
	uint32_t i;

	if(virt % PAGE_SIZE || phys % PAGE_SIZE || size % PAGE_SIZE)
		return false;

	/* Map using sections where possible. To do this, align up to a 1MB
	 * boundary using small pages, map anything possible with sections,
	 * then do the rest using small pages. If virtual and physical addresses
	 * are at different offsets from a section boundary, we cannot map
	 * using sections. */
	if((virt % LARGE_PAGE_SIZE) == (phys % LARGE_PAGE_SIZE)) {
		while(virt % LARGE_PAGE_SIZE && size) {
			map_small(ctx, virt, phys);
			virt += PAGE_SIZE;
			phys += PAGE_SIZE;
			size -= PAGE_SIZE;
		}
		while(size / LARGE_PAGE_SIZE) {
			map_section(ctx, virt, phys);
			virt += LARGE_PAGE_SIZE;
			phys += LARGE_PAGE_SIZE;
			size -= LARGE_PAGE_SIZE;
		}
	}

	/* Map whatever remains. */
	for(i = 0; i < size; i += PAGE_SIZE)
		map_small(ctx, virt + i, phys + i);

	return true;
}

/** Create a new MMU context.
 * @param target	Target operation mode definition.
 * @param phys_type	Physical memory type to use when allocating tables.
 * @return		Pointer to context. */
mmu_context_t *mmu_context_create(target_type_t target, unsigned phys_type) {
	mmu_context_t *ctx;

	assert(target == TARGET_TYPE_32BIT);

	ctx = kmalloc(sizeof(*ctx));
	ctx->phys_type = phys_type;
	ctx->l1 = allocate_structure(ctx, 0x4000);
	return ctx;
}
