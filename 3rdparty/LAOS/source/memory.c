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

#include <lib/list.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <loader.h>
#include <memory.h>

/** Structure representing an area on the heap. */
typedef struct heap_chunk {
	list_t header;			/**< Link to chunk list. */
	size_t size;			/**< Size of chunk including struct. */
	bool allocated;			/**< Whether the chunk is allocated. */
} heap_chunk_t;

/** Size of the heap (128KB). */
#define HEAP_SIZE		131072

/** Statically allocated heap. */
static uint8_t heap[HEAP_SIZE] __aligned(PAGE_SIZE);
static LIST_DECLARE(heap_chunks);

/** List of physical memory ranges. */
LIST_DECLARE(memory_ranges);

/** Allocate memory from the heap.
 * @note		An internal error will be raised if heap is full.
 * @param size		Size of allocation to make.
 * @return		Address of allocation. */
void *kmalloc(size_t size) {
	heap_chunk_t *chunk = NULL, *new;
	size_t total;

	if(size == 0)
		internal_error("Zero-sized allocation!");

	/* Align all allocations to 8 bytes. */
	size = ROUND_UP(size, 8);
	total = size + sizeof(heap_chunk_t);

	/* Create the initial free segment if necessary. */
	if(list_empty(&heap_chunks)) {
		chunk = (heap_chunk_t *)heap;
		chunk->size = HEAP_SIZE;
		chunk->allocated = false;
		list_init(&chunk->header);
		list_append(&heap_chunks, &chunk->header);
	} else {
		/* Search for a free chunk. */
		LIST_FOREACH(&heap_chunks, iter) {
			chunk = list_entry(iter, heap_chunk_t, header);
			if(!chunk->allocated && chunk->size >= total) {
				break;
			} else {
				chunk = NULL;
			}
		}

		if(!chunk)
			internal_error("Exhausted heap space (want %zu bytes)", size);
	}

	/* Resize the segment if it is too big. There must be space for a
	 * second chunk header afterwards. */
	if(chunk->size >= (total + sizeof(heap_chunk_t))) {
		new = (heap_chunk_t *)((char *)chunk + total);
		new->size = chunk->size - total;
		new->allocated = false;
		list_init(&new->header);
		list_add_after(&chunk->header, &new->header);
		chunk->size = total;
	}

	chunk->allocated = true;
	return ((char *)chunk + sizeof(heap_chunk_t));
}

/** Resize a memory allocation.
 * @param addr		Address of old allocation.
 * @param size		New size of allocation.
 * @return		Address of new allocation, or NULL if size is 0. */
void *krealloc(void *addr, size_t size) {
	heap_chunk_t *chunk;
	void *new;

	if(size == 0) {
		kfree(addr);
		return NULL;
	} else {
		new = kmalloc(size);
		if(addr) {
			chunk = (heap_chunk_t *)((char *)addr - sizeof(heap_chunk_t));
			memcpy(new, addr, MIN(chunk->size - sizeof(heap_chunk_t), size));
			kfree(addr);
		}
		return new;
	}
}

/** Free memory allocated with kfree().
 * @param addr		Address of allocation. */
void kfree(void *addr) {
	heap_chunk_t *chunk, *adj;

	if(!addr)
		return;

	/* Get the chunk and free it. */
	chunk = (heap_chunk_t *)((char *)addr - sizeof(heap_chunk_t));
	if(!chunk->allocated)
		internal_error("Double free on address %p", addr);
	chunk->allocated = false;

	/* Coalesce adjacent free segments. */
	if(chunk->header.next != &heap_chunks) {
		adj = list_entry(chunk->header.next, heap_chunk_t, header);
		if(!adj->allocated) {
			assert(adj == (heap_chunk_t *)((char *)chunk + chunk->size));
			chunk->size += adj->size;
			list_remove(&adj->header);
		}
	}
	if(chunk->header.prev != &heap_chunks) {
		adj = list_entry(chunk->header.prev, heap_chunk_t, header);
		if(!adj->allocated) {
			assert(chunk == (heap_chunk_t *)((char *)adj + adj->size));
			adj->size += chunk->size;
			list_remove(&chunk->header);
		}
	}
}

/** Create a memory range structure.
 * @param start		Start address.
 * @param size		Size of the range.
 * @param type		Type of range.
 * @return		Pointer to range structure. */
static memory_range_t *memory_range_create(phys_ptr_t start, phys_size_t size, unsigned type) {
	memory_range_t *range = kmalloc(sizeof(memory_range_t));
	list_init(&range->header);
	range->start = start;
	range->size = size;
	range->type = type;
	return range;
}

/** Merge adjacent ranges.
 * @param range		Range to merge. */
static inline void memory_range_merge(memory_range_t *range) {
	memory_range_t *other;

	if(memory_ranges.next != &range->header) {
		other = list_entry(range->header.prev, memory_range_t, header);
		if(other->start + other->size == range->start && other->type == range->type) {
			range->start = other->start;
			range->size += other->size;
			list_remove(&other->header);
			kfree(other);
		}
	}
	if(memory_ranges.prev != &range->header) {
		other = list_entry(range->header.next, memory_range_t, header);
		if(other->start == range->start + range->size && other->type == range->type) {
			range->size += other->size;
			list_remove(&other->header);
			kfree(other);
		}
	}
}

/** Add a range of physical memory.
 * @param start		Start of the range (must be page-aligned).
 * @param size		Size of the range (must be page-aligned).
 * @param type		Type of the range. */
static void memory_range_insert(phys_ptr_t start, phys_size_t size, unsigned type) {
	memory_range_t *range, *other, *split;
	phys_ptr_t range_end, other_end;

	assert(!(start % PAGE_SIZE));
	assert(!(size % PAGE_SIZE));
	assert(size);

	range_end = start + size - 1;

	range = memory_range_create(start, size, type);

	/* Try to find where to insert the region in the list. */
	LIST_FOREACH(&memory_ranges, iter) {
		other = list_entry(iter, memory_range_t, header);
		if(start <= other->start) {
			list_add_before(&other->header, &range->header);
			break;
		}
	}

	/* If the range has not been added, add it now. */
	if(list_empty(&range->header))
		list_append(&memory_ranges, &range->header);

	/* Check if the new range has overlapped part of the previous range. */
	if(memory_ranges.next != &range->header) {
		other = list_entry(range->header.prev, memory_range_t, header);
		other_end = other->start + other->size - 1;

		if(range->start <= other_end) {
			if(other_end > range_end) {
				/* Must split the range. */
				split = memory_range_create(range_end + 1,
					other_end - range_end,
					other->type);
				list_add_after(&range->header, &split->header);
			}

			other->size = range->start - other->start;
		}
	}

	/* Swallow up any following ranges that the new range overlaps. */
	LIST_FOREACH_SAFE(&range->header, iter) {
		if(iter == &memory_ranges)
			break;

		other = list_entry(iter, memory_range_t, header);
		other_end = other->start + other->size - 1;

		if(other->start > range_end) {
			break;
		} else if(other_end > range_end) {
			/* Resize the range and finish. */
			other->start = range_end + 1;
			other->size = other_end - range_end;
			break;
		} else {
			/* Completely remove the range. */
			list_remove(&other->header);
			kfree(other);
		}
	}

	/* Finally, merge the region with adjacent ranges of the same type. */
	memory_range_merge(range);
}

/** Add a range of physical memory.
 * @param start		Start of the range (must be page-aligned).
 * @param size		Size of the range (must be page-aligned).
 * @param type		Type of the range. */
void phys_memory_add(phys_ptr_t start, phys_size_t size, unsigned type) {
	memory_range_insert(start, size, type);

	dprintf("memory: added range 0x%" PRIxPHYS "-0x%" PRIxPHYS " (type: %u)\n",
		start, start + size, type);
}

/**
 * Mark all free areas in a range as internal.
 *
 * Searches through the given range and marks all currently free areas as
 * internal, so that they will not be allocated from by the boot loader. They
 * will be returned to free when memory_finalize() is called.
 *
 * @param start		Start of the range.
 * @param size		Size of the range.
 */
void phys_memory_protect(phys_ptr_t start, phys_size_t size) {
	phys_ptr_t match_start, match_end, end;
	memory_range_t *range;

	start = ROUND_DOWN(start, PAGE_SIZE);
	end = ROUND_UP(start + size, PAGE_SIZE) - 1;

	LIST_FOREACH_SAFE(&memory_ranges, iter) {
		range = list_entry(iter, memory_range_t, header);
		if(range->type != PHYS_MEMORY_FREE)
			continue;

		match_start = MAX(start, range->start);
		match_end = MIN(end, range->start + range->size - 1);
		if(match_end <= match_start)
			continue;

		memory_range_insert(match_start, match_end - match_start + 1,
			PHYS_MEMORY_INTERNAL);
	}
}

/** Check whether a range can satisfy an allocation
 * @param range		Range to check.
 * @param size		Size of the allocation.
 * @param align		Alignment of the allocation.
 * @param min_addr	Minimum address for the start of the allocated range.
 * @param max_addr	Maximum address of the end of the allocated range.
 * @param flags		Behaviour flags.
 * @param physp		Where to store address for allocation.
 * @return		Whether the range can satisfy the allocation. */
static bool is_suitable_range(memory_range_t *range, phys_size_t size,
	phys_size_t align, phys_ptr_t min_addr, phys_ptr_t max_addr,
	unsigned flags, phys_ptr_t *physp)
{
	phys_ptr_t start, match_start, match_end;

	if(range->type != PHYS_MEMORY_FREE)
		return false;

	/* Check if this range contains addresses in the requested range. */
	match_start = MAX(min_addr, range->start);
	match_end = MIN(max_addr - 1, range->start + range->size - 1);
	if(match_end <= match_start)
		return false;

	/* Align the base address and check that the range fits. */
	if(flags & PHYS_ALLOC_HIGH) {
		start = ROUND_DOWN((match_end - size) + 1, align);
		if(start < match_start)
			return false;
	} else {
		start = ROUND_UP(match_start, align);
		if((start + size - 1) > match_end)
			return false;
	}

	*physp = start;
	return true;
}

/**
 * Allocate a range of physical memory.
 *
 * Allocates a range of physical memory satisfying the specified constraints.
 * Unless PHYS_ALLOC_CANFAIL is specified, a boot error will be raised if the
 * allocation fails. This function will always allocate memory below 4GB.
 *
 * @param size		Size of the range (multiple of PAGE_SIZE).
 * @param align		Alignment of the range (power of 2, at least PAGE_SIZE).
 * @param min_addr	Minimum address for the start of the allocated range.
 * @param max_addr	Maximum address of the end of the allocated range.
 * @param type		Type to give the allocated range (must not be
 *			PHYS_MEMORY_FREE).
 * @param flags		Behaviour flags.
 * @param physp		Where to store address of allocation.
 *
 * @return		Whether successfully allocated (always true unless
 *			PHYS_ALLOC_CANFAIL specified).
 */
bool phys_memory_alloc(phys_size_t size, phys_size_t align, phys_ptr_t min_addr,
	phys_ptr_t max_addr, unsigned type, unsigned flags,
	phys_ptr_t *physp)
{
	memory_range_t *range;
	phys_ptr_t start;

	if(!align)
		align = PAGE_SIZE;

	#if ARCH_PHYSICAL_64BIT
	/* Typically the boot loader runs in 32-bit mode, need to ensure that
	 * all addresses allocated are accessible. */
	if(!max_addr || max_addr > 0x100000000LL)
		max_addr = 0x100000000LL;
	#endif

	assert(!(size % PAGE_SIZE));
	assert(((max_addr - 1) - min_addr) >= (size - 1));
	assert(type >= PHYS_MEMORY_ALLOCATED);

	/* Find a free range that is large enough to hold the new range. */
	if(flags & PHYS_ALLOC_HIGH) {
		LIST_FOREACH_REVERSE(&memory_ranges, iter) {
			range = list_entry(iter, memory_range_t, header);
			if(is_suitable_range(range, size, align, min_addr, max_addr, flags, &start))
				break;

			range = NULL;
		}
	} else {
		LIST_FOREACH(&memory_ranges, iter) {
			range = list_entry(iter, memory_range_t, header);
			if(is_suitable_range(range, size, align, min_addr, max_addr, flags, &start))
				break;

			range = NULL;
		}
	}

	if(!range) {
		if(!(flags & PHYS_ALLOC_CANFAIL))
			boot_error("You do not have enough memory available");

		return false;
	}

	/* Insert a new range over the top of the allocation. */
	memory_range_insert(start, size, type);

	dprintf("memory: allocated 0x%" PRIxPHYS "-0x%" PRIxPHYS " (align: 0x%" PRIxPHYS ", "
		"type: %u, flags: 0x%x)\n", start, start + size, align, type, flags);
	*physp = start;
	return true;
}

/** Dump a list of physical memory ranges. */
static void phys_memory_dump(void) {
	memory_range_t *range;

	LIST_FOREACH(&memory_ranges, iter) {
		range = list_entry(iter, memory_range_t, header);

		dprintf(" 0x%016" PRIxPHYS "-0x%016" PRIxPHYS ": ", range->start,
			range->start + range->size);
		switch(range->type) {
		case PHYS_MEMORY_FREE:
			dprintf("Free\n");
			break;
		case PHYS_MEMORY_ALLOCATED:
			dprintf("Allocated\n");
			break;
		case PHYS_MEMORY_RECLAIMABLE:
			dprintf("Reclaimable\n");
			break;
		case PHYS_MEMORY_PAGETABLES:
			dprintf("Pagetables\n");
			break;
		case PHYS_MEMORY_STACK:
			dprintf("Stack\n");
			break;
		case PHYS_MEMORY_MODULES:
			dprintf("Modules\n");
			break;
		case PHYS_MEMORY_INTERNAL:
			dprintf("Internal\n");
			break;
		default:
			dprintf("???\n");
			break;
		}
	}
}

/** Initialise the memory manager. */
void memory_init(void) {
	phys_ptr_t start, end;

	/* Detect memory ranges. */
	platform_memory_detect();

	/* Mark the boot loader itself as internal so that it gets reclaimed
	 * before entering the kernel. */
	start = ROUND_DOWN((phys_ptr_t)((ptr_t)__start), PAGE_SIZE);
	end = ROUND_UP((phys_ptr_t)((ptr_t)__end), PAGE_SIZE);
	phys_memory_protect(start, end - start);

	dprintf("memory: initial memory map:\n");
	phys_memory_dump();
}

/** Finalize the memory map. */
void memory_finalize(void) {
	memory_range_t *range;

	/* Reclaim all internal memory ranges. */
	LIST_FOREACH(&memory_ranges, iter) {
		range = list_entry(iter, memory_range_t, header);
		if(range->type == PHYS_MEMORY_INTERNAL) {
			range->type = PHYS_MEMORY_FREE;
			memory_range_merge(range);
		}
	}

	/* Dump the memory map to the debug console. */
	dprintf("memory: final memory map:\n");
	phys_memory_dump();
}
