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
 * @brief		Virtual memory region allocator.
 */

#include <lib/allocator.h>
#include <lib/utility.h>

#include <assert.h>
#include <loader.h>
#include <memory.h>

/** Structure of a region. */
typedef struct allocator_region {
	list_t header;			/**< List header. */

	target_ptr_t start;		/**< Start of the region. */
	target_size_t size;		/**< Size of the region. */
	bool allocated;			/**< Whether the region is allocated. */
} allocator_region_t;

/** Create a region structure.
 * @param start		Start of the region.
 * @param size		Size of the region.
 * @param allocated	Whether the region is allocated.
 * @return		Pointer to created region. */
static allocator_region_t *allocator_region_create(target_ptr_t start,
	target_size_t size, bool allocated)
{
	allocator_region_t *region = kmalloc(sizeof(*region));
	list_init(&region->header);
	region->start = start;
	region->size = size;
	region->allocated = allocated;
	return region;
}

/** Insert a region into the allocator.
 * @param alloc		Allocator to insert into.
 * @param region	Region to insert. */
static void insert_region(allocator_t *alloc, allocator_region_t *region) {
	allocator_region_t *other, *split;
	phys_ptr_t region_end, other_end;

	/* We need to deal with the case where start + size wraps to 0, i.e. if
	 * we are allocating from the whole address space. */
	region_end = region->start + region->size - 1;

	/* Try to find where to insert the region in the list. */
	LIST_FOREACH(&alloc->regions, iter) {
		other = list_entry(iter, allocator_region_t, header);
		if(region->start <= other->start) {
			list_add_before(&other->header, &region->header);
			break;
		}
	}

	/* If the region has not been added, add it now. */
	if(list_empty(&region->header))
		list_append(&alloc->regions, &region->header);

	/* Check if the new region has overlapped part of the previous. */
	if(list_first(&alloc->regions, allocator_region_t, header) != region) {
		other = list_prev(&region->header, allocator_region_t, header);
		other_end = other->start + other->size - 1;

		if(region->start <= other_end) {
			if(other_end > region_end) {
				/* Must split the region. */
				split = allocator_region_create(region_end + 1,
					other_end - region_end,
					other->allocated);
				list_add_after(&region->header, &split->header);
			}

			other->size = region->start - other->start;
		}
	}

	/* Swallow up any following ranges that the new range overlaps. */
	LIST_FOREACH_SAFE(&region->header, iter) {
		if(iter == &alloc->regions)
			break;

		other = list_entry(iter, allocator_region_t, header);
		other_end = other->start + other->size - 1;

		if(other->start > region_end) {
			break;
		} else if(other_end > region_end) {
			/* Resize the range and finish. */
			other->start = region_end + 1;
			other->size = other_end - region_end;
			break;
		} else {
			/* Completely remove the range. */
			list_remove(&other->header);
			kfree(other);
		}
	}
}

/** Allocate a region from an allocator.
 * @param alloc		Allocator to allocate from.
 * @param size		Size of the region to allocate.
 * @param align		Alignment of the region.
 * @param addrp		Where to store address of allocated region.
 * @return		Whether successful in allocating region. */
bool allocator_alloc(allocator_t *alloc, target_size_t size, target_size_t align,
	target_ptr_t *addrp)
{
	allocator_region_t *region;
	target_ptr_t start;

	assert(!(size % PAGE_SIZE));
	assert(!(align % PAGE_SIZE));
	assert(size);

	if(!align)
		align = PAGE_SIZE;

	LIST_FOREACH(&alloc->regions, iter) {
		region = list_entry(iter, allocator_region_t, header);
		if(region->allocated)
			continue;

		start = ROUND_UP(region->start, align);
		if(start + size - 1 > region->start + region->size - 1)
			continue;

		*addrp = start;

		/* Create a new allocated region and insert over this space. */
		region = allocator_region_create(start, size, true);
		insert_region(alloc, region);
		return true;
	}

	return false;
}

/**
 * Mark a region as allocated.
 *
 * Tries to mark a region of the address space as allocated, ensuring that no
 * other regions are already allocated within it.
 *
 * @param alloc		Allocator to insert into.
 * @param addr		Start of region to insert.
 * @param size		Size of region to insert.
 *
 * @return		Whether successfully inserted.
 */
bool allocator_insert(allocator_t *alloc, target_ptr_t addr, target_size_t size) {
	target_ptr_t region_end, other_end;
	allocator_region_t *other;

	assert(!(addr % PAGE_SIZE));
	assert(!(size % PAGE_SIZE));
	assert(size);

	region_end = addr + size - 1;

	/* Check for conflicts with other allocated regions. */
	LIST_FOREACH(&alloc->regions, iter) {
		other = list_entry(iter, allocator_region_t, header);
		if(!other->allocated)
			continue;

		other_end = other->start + other->size - 1;
		if(MAX(addr, other->start) <= MIN(region_end, other_end))
			return false;
	}

	allocator_reserve(alloc, addr, size);
	return true;
}

/**
 * Block a region from being allocated.
 *
 * Prevents any future allocations from returning any address within the given
 * region.
 *
 * @param alloc		Allocator to reserve in.
 * @param addr		Address of region to reserve.
 * @param size		Size of region to reserve.
 */
void allocator_reserve(allocator_t *alloc, target_ptr_t addr, target_size_t size) {
	target_ptr_t region_end, alloc_end, end;
	allocator_region_t *region;

	assert(!(addr % PAGE_SIZE));
	assert(!(size % PAGE_SIZE));
	assert(size);

	/* Trim given range to be within the allocator. */
	region_end = addr + size - 1;
	alloc_end = alloc->start + alloc->size - 1;
	addr = MAX(addr, alloc->start);
	end = MIN(region_end, alloc_end);
	if(end < addr)
		return;
	size = MIN(region_end, alloc_end) - addr + 1;

	region = allocator_region_create(addr, size, true);
	insert_region(alloc, region);
}

/** Initialize an allocator.
 * @param alloc		Allocator to initialize.
 * @param start		Start of range that the allocator manages.
 * @param size		Size of range that the allocator manages. A size of 0
 *			can be used in conjunction with 0 start to mean the
 *			entire address space. */
void allocator_init(allocator_t *alloc, target_ptr_t start, target_size_t size) {
	allocator_region_t *region;

	assert(!(start % PAGE_SIZE));
	assert(!(size % PAGE_SIZE));
	assert((start + size) > start || (start + size) == 0);

	list_init(&alloc->regions);
	alloc->start = start;
	alloc->size = size;

	/* Add a free region covering the entire space. */
	region = allocator_region_create(start, size, false);
	list_append(&alloc->regions, &region->header);
}
