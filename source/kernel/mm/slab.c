/*
 * Copyright (C) 2009-2011 Gil Mendes
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
 * @brief		Slab allocator.
 *
 * Reference:
 * - The slab allocator: An object-caching kernel memory allocator
 *   http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.29.4759
 * - Magazines and Vmem: Extending the Slab Allocator to Many CPUs and
 *   Arbitrary Resources
 *   http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.6.8388
 *
 * This implementation uses the magazine layer described in the second of the
 * above papers for good multiprocessor scalability. The only difference is
 * that we do not leave empty slabs lying around - when a slab becomes empty,
 * it is freed immediately.
 *
 * @todo		Dynamic magazine resizing.
 * @todo		Allocation hash table resizing.
 */

#include <lib/fnv.h>
#include <lib/string.h>
#include <lib/utility.h>

#include <mm/kmem.h>
#include <mm/malloc.h>
#include <mm/slab.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <assert.h>
#include <cpu.h>
#include <kdb.h>
#include <kernel.h>
#include <module.h>
#include <status.h>

struct slab;

/** Slab magazine structure. */
typedef struct slab_magazine {
	/** Array of objects in the magazine. */
	void *objects[SLAB_MAGAZINE_SIZE];
	size_t rounds;				/**< Number of rounds currently in the magazine. */

	list_t header;				/**< Link to depot lists. */
} slab_magazine_t;

/** Slab per-CPU cache structure. */
typedef struct __cacheline_aligned slab_percpu {
	slab_magazine_t *loaded;		/**< Current (loaded) magazine. */
	slab_magazine_t *previous;		/**< Previous magazine. */
} slab_percpu_t;

/** Slab buffer control structure.
 * @note		The order of this structure is important: the pointer
 *			MUST be first, as it is the only member that exists for
 *			small-object caches. */
typedef struct slab_bufctl {
	struct slab_bufctl *next;		/**< Address of next buffer. */

	struct slab *parent;			/**< Parent slab structure. */
	void *object;				/**< Pointer to actual object. */
} slab_bufctl_t;

/** Slab structure. */
typedef struct slab {
	list_t header;				/**< Link to appropriate slab list in cache. */

	void *base;				/**< Base address of allocation. */
	size_t refcount;			/**< Reference count. */
	slab_bufctl_t *free;			/**< List of free buffers. */
	size_t colour;				/**< Colour of the slab. */
	slab_cache_t *parent;			/**< Cache containing the slab. */
} slab_t;

/** Reclaim priorities to use for caches. */
#define SLAB_DEFAULT_PRIORITY		0
#define SLAB_METADATA_PRIORITY		1
#define SLAB_MAG_PRIORITY		2

/** Internally-used caches. */
static slab_cache_t slab_cache_cache;		/**< Cache for allocation of new slab caches. */
static slab_cache_t slab_mag_cache;		/**< Cache for magazine structures. */
static slab_cache_t slab_bufctl_cache;		/**< Cache for buffer control structures. */
static slab_cache_t slab_slab_cache;		/**< Cache for slab structures. */
static slab_cache_t *slab_percpu_cache = NULL;	/**< Cache for per-CPU structures. */

/** List of all slab caches. */
static LIST_DEFINE(slab_caches);
static MUTEX_DEFINE(slab_caches_lock, 0);

/** Destroy a slab.
 * @param cache		Cache to destroy in.
 * @param slab		Slab to destroy. */
static void slab_destroy(slab_cache_t *cache, slab_t *slab) {
	void *addr = slab->base;
	slab_bufctl_t *bufctl;

	assert(!slab->refcount);

	list_remove(&slab->header);

	/* Destroy all buffer control structures and the slab structure if
	 * stored externally. */
	if(cache->flags & SLAB_CACHE_LARGE) {
		while(slab->free) {
			bufctl = slab->free;
			slab->free = bufctl->next;

			slab_cache_free(&slab_bufctl_cache, bufctl);
		}

		slab_cache_free(&slab_slab_cache, slab);
	}

	cache->slab_count--;
	kmem_free(addr, cache->slab_size);
}

/** Allocate a new slab and divide it up into objects.
 * @param cache		Cache to allocate from.
 * @param mmflag	Allocation behaviour flags.
 * @return		Pointer to slab structure. */
static inline slab_t *slab_create(slab_cache_t *cache, unsigned mmflag) {
	slab_bufctl_t *bufctl, *prev = NULL;
	slab_t *slab;
	void *addr;
	size_t i;

	/* Drop slab lock while creating as a reclaim may occur that wants to
	 * free to this cache. */
	mutex_unlock(&cache->slab_lock);

	/* Allocate a new slab. */
	addr = kmem_alloc(cache->slab_size, mmflag & MM_FLAG_MASK);
	if(unlikely(!addr)) {
		mutex_lock(&cache->slab_lock);
		return NULL;
	}

	mutex_lock(&cache->slab_lock);

	/* Create the slab structure for the slab. */
	if(cache->flags & SLAB_CACHE_LARGE) {
		slab = slab_cache_alloc(&slab_slab_cache, mmflag & MM_FLAG_MASK);
		if(unlikely(!slab)) {
			kmem_free(addr, cache->slab_size);
			return NULL;
		}
	} else {
		slab = (slab_t *)(((ptr_t)addr + cache->slab_size) - sizeof(slab_t));
	}

	cache->slab_count++;

	list_init(&slab->header);
	slab->base = addr;
	slab->refcount = 0;
	slab->free = NULL;
	slab->colour = cache->colour_next;
	slab->parent = cache;

	/* Divide the buffer up into unconstructed, free objects. */
	for(i = 0; i < cache->obj_count; i++) {
		if(cache->flags & SLAB_CACHE_LARGE) {
			bufctl = slab_cache_alloc(&slab_bufctl_cache, mmflag & MM_FLAG_MASK);
			if(unlikely(!bufctl)) {
				slab_destroy(cache, slab);
				return NULL;
			}

			bufctl->parent = slab;
			bufctl->object = (void *)((ptr_t)addr + slab->colour + (i * cache->obj_size));
		} else {
			bufctl = (slab_bufctl_t *)((ptr_t)addr + slab->colour + (i * cache->obj_size));
		}

		/* Add to the free list. */
		bufctl->next = NULL;
		if(!prev) {
			slab->free = bufctl;
		} else {
			prev->next = bufctl;
		}
		prev = bufctl;
	}

	/* Success - update the cache colour and return. Do not add to any
	 * slab lists - the caller will do so. */
	cache->colour_next += cache->align;
	if(cache->colour_next > cache->colour_max)
		cache->colour_next = 0;

	return slab;
}

/** Destruct an object and free it to the slab layer.
 * @param cache		Cache to free to.
 * @param obj		Object to free. */
static inline void slab_obj_free(slab_cache_t *cache, void *obj) {
	slab_bufctl_t *bufctl, *prev = NULL;
	uint32_t hash;
	slab_t *slab;

	mutex_lock(&cache->slab_lock);

	/* Find the buffer control structure. For large object caches, look it
	 * up on the allocation hash table. Otherwise, we use the start of the
	 * buffer as the structure. */
	if(cache->flags & SLAB_CACHE_LARGE) {
		hash = fnv_hash_integer((ptr_t)obj) % SLAB_HASH_SIZE;
		for(bufctl = cache->bufctl_hash[hash]; bufctl; bufctl = bufctl->next) {
			if(bufctl->object == obj)
				break;

			prev = bufctl;
		}

		if(unlikely(!bufctl)) {
			fatal("Free (%s): object %p not found in hash table", cache->name, obj);
		}

		slab = bufctl->parent;
		assert(slab->parent == cache);

		/* Take the object off the hash chain. */
		if(prev) {
			prev->next = bufctl->next;
		} else {
			cache->bufctl_hash[hash] = bufctl->next;
		}
	} else {
		bufctl = (slab_bufctl_t *)obj;

		/* Find the slab corresponding to the object. The structure
		 * will be at the end of the slab. */
		slab = (slab_t *)(round_down((ptr_t)obj, cache->slab_size) + (cache->slab_size - sizeof(slab_t)));
		if(unlikely(slab->parent != cache))
			fatal("Free (%s): slab structure for %p invalid (%p)", cache->name, obj, slab->parent);
	}

	/* Call the object destructor. */
	if(cache->dtor)
		cache->dtor(obj, cache->data);

	assert(slab->refcount);

	/* Return the object to the slab's free list. */
	bufctl->next = slab->free;
	slab->free = bufctl;

	if(--slab->refcount == 0) {
		/* Slab empty, destroy it. */
		slab_destroy(cache, slab);
	} else if((slab->refcount + 1) == cache->obj_count) {
		/* Take from the full list and move to the partial list. */
		list_append(&cache->slab_partial, &slab->header);
	}

	mutex_unlock(&cache->slab_lock);
}

/** Allocate an object from the slab layer and construct it.
 * @param cache		Cache to allocate from.
 * @param mmflag	Allocation behaviour flags.
 * @return		Pointer to allocated object, or NULL on failure. */
static inline void *slab_obj_alloc(slab_cache_t *cache, unsigned mmflag) {
	slab_bufctl_t *bufctl;
	uint32_t hash;
	slab_t *slab;
	void *obj;

	mutex_lock(&cache->slab_lock);

	/* If there is a slab in the partial list, take it. */
	if(!list_empty(&cache->slab_partial)) {
		slab = list_first(&cache->slab_partial, slab_t, header);
	} else {
		/* No slabs with free objects available - allocate a new
		 * slab. */
		slab = slab_create(cache, mmflag);
		if(unlikely(!slab)) {
			mutex_unlock(&cache->slab_lock);
			return NULL;
		}
	}

	assert(slab->free);

	/* Take an object from the slab. If the metadata is stored externally,
	 * then the object address is contained in the object field of the
	 * bufctl structure. Otherwise, the object address is the same as the
	 * structure address. */
	bufctl = slab->free;
	slab->free = bufctl->next;
	slab->refcount++;

	obj = (cache->flags & SLAB_CACHE_LARGE) ? bufctl->object : (void *)bufctl;

	/* Place the allocation on the allocation hash table if required. */
	if(cache->flags & SLAB_CACHE_LARGE) {
		hash = fnv_hash_integer((ptr_t)obj) % SLAB_HASH_SIZE;
		bufctl->next = cache->bufctl_hash[hash];
		cache->bufctl_hash[hash] = bufctl;
	}

	/* Check if a list move is required. */
	if(slab->refcount == cache->obj_count) {
		list_append(&cache->slab_full, &slab->header);
	} else {
		list_append(&cache->slab_partial, &slab->header);
	}

	/* Construct the object and return it. Unlock the cache before calling
	 * the constructor as it may cause a reclaim. */
	mutex_unlock(&cache->slab_lock);
	if(cache->ctor)
		cache->ctor(obj, cache->data);

	return obj;
}

/** Get a full magazine from a cache's depot.
 * @param cache		Cache to get from.
 * @return		Pointer to magazine on success, NULL on failure. */
static inline slab_magazine_t *slab_magazine_get_full(slab_cache_t *cache) {
	slab_magazine_t *mag = NULL;

	mutex_lock(&cache->depot_lock);

	if(!list_empty(&cache->magazine_full)) {
		mag = list_first(&cache->magazine_full, slab_magazine_t, header);
		list_remove(&mag->header);
		assert(mag->rounds == SLAB_MAGAZINE_SIZE);
	}

	mutex_unlock(&cache->depot_lock);
	return mag;
}

/** Return a full magazine to the depot.
 * @param cache		Cache to return to.
 * @param mag		Magazine to return. */
static inline void slab_magazine_put_full(slab_cache_t *cache, slab_magazine_t *mag) {
	assert(mag->rounds == SLAB_MAGAZINE_SIZE);

	mutex_lock(&cache->depot_lock);
	list_prepend(&cache->magazine_full, &mag->header);
	mutex_unlock(&cache->depot_lock);
}

/** Get an empty magazine from a cache's depot.
 * @param cache		Cache to get from.
 * @return		Pointer to magazine on success, NULL on failure. */
static inline slab_magazine_t *slab_magazine_get_empty(slab_cache_t *cache) {
	slab_magazine_t *mag = NULL;

	mutex_lock(&cache->depot_lock);

	if(!list_empty(&cache->magazine_empty)) {
		mag = list_first(&cache->magazine_empty, slab_magazine_t, header);
		list_remove(&mag->header);
		assert(!mag->rounds);
	} else {
		/* None available, try to allocate a new structure. We do not
		 * wait for memory to be available here as if a new magazine
		 * cannot be allocated on the first try it means that the system
		 * is low on memory. In this case, the object should be freed
		 * back to the source. TODO: If low on memory, should not
		 * attempt to allocate at all. */
		mag = slab_cache_alloc(&slab_mag_cache, MM_ATOMIC);
		if(mag) {
			list_init(&mag->header);
			mag->rounds = 0;
		}
	}

	mutex_unlock(&cache->depot_lock);
	return mag;
}

/** Return an empty magazine to the depot.
 * @param cache		Cache to return to.
 * @param mag		Magazine to return. */
static inline void slab_magazine_put_empty(slab_cache_t *cache, slab_magazine_t *mag) {
	assert(!mag->rounds);

	mutex_lock(&cache->depot_lock);
	list_prepend(&cache->magazine_empty, &mag->header);
	mutex_unlock(&cache->depot_lock);
}

/** Destroy a magazine.
 * @param cache		Cache the magazine belongs to.
 * @param mag		Magazine to destroy. */
static inline void slab_magazine_destroy(slab_cache_t *cache, slab_magazine_t *mag) {
	size_t i;

	/* Free all rounds within the magazine, if any. */
	for(i = 0; i < mag->rounds; i++)
		slab_obj_free(cache, mag->objects[i]);

	list_remove(&mag->header);
	slab_cache_free(&slab_mag_cache, mag);
}

/** Allocate an object from the magazine layer.
 * @param cache		Cache to allocate from.
 * @return		Pointer to object on success, NULL on failure. */
static inline void *slab_cpu_obj_alloc(slab_cache_t *cache) {
	slab_percpu_t *cc;
	slab_magazine_t *mag;
	bool state;
	void *ret;

	/* We do not need locking on the per-CPU cache as it will not be used
	 * by any other CPUs. We do however need to disable interrupts to
	 * prevent a thread switch from occurring mid-operation. */
	state = local_irq_disable();

	cc = &cache->cpu_caches[curr_cpu->id];

	/* Check if we have a magazine to allocate from. */
	if(likely(cc->loaded)) {
		if(cc->loaded->rounds) {
			ret = cc->loaded->objects[--cc->loaded->rounds];
			local_irq_restore(state);
			return ret;
		} else if(cc->previous && cc->previous->rounds) {
			/* Previous has rounds, exchange loaded with previous
			 * and allocate from it. */
			swap(cc->loaded, cc->previous);
			ret = cc->loaded->objects[--cc->loaded->rounds];
			local_irq_restore(state);
			return ret;
		}
	}

	/* Try to get a full magazine from the depot. */
	mag = slab_magazine_get_full(cache);
	assert(!local_irq_state());
	if(unlikely(!mag)) {
		local_irq_restore(state);
		return NULL;
	}

	/* Return previous to the depot. */
	if(cc->previous) {
		slab_magazine_put_empty(cache, cc->previous);
		assert(!local_irq_state());
	}

	cc->previous = cc->loaded;
	cc->loaded = mag;
	ret = cc->loaded->objects[--cc->loaded->rounds];
	local_irq_restore(state);
	return ret;
}

/** Free an object to the magazine layer.
 * @param cache		Cache to free to.
 * @param obj		Object to free.
 * @return		True if succeeded, false if not. */
static inline bool slab_cpu_obj_free(slab_cache_t *cache, void *obj) {
	slab_percpu_t *cc;
	slab_magazine_t *mag;
	bool state;

	state = local_irq_disable();

	cc = &cache->cpu_caches[curr_cpu->id];

	/* If the loaded magazine has spare slots, just put the object there
	 * and return. */
	if(likely(cc->loaded)) {
		if(cc->loaded->rounds < SLAB_MAGAZINE_SIZE) {
			cc->loaded->objects[cc->loaded->rounds++] = obj;
			local_irq_restore(state);
			return true;
		} else if(cc->previous && cc->previous->rounds < SLAB_MAGAZINE_SIZE) {
			/* Previous has spare slots, exchange them and insert
			 * the object. */
			swap(cc->loaded, cc->previous);
			cc->loaded->objects[cc->loaded->rounds++] = obj;
			local_irq_restore(state);
			return true;
		}
	}

	/* Get a new empty magazine. */
	mag = slab_magazine_get_empty(cache);
	assert(!local_irq_state());
	if(unlikely(!mag)) {
		local_irq_restore(state);
		return false;
	}

	/* Load the new magazine, and free the previous. */
	if(likely(cc->previous)) {
		slab_magazine_put_full(cache, cc->previous);
		assert(!local_irq_state());
	}
	cc->previous = cc->loaded;
	cc->loaded = mag;

	cc->loaded->objects[cc->loaded->rounds++] = obj;
	local_irq_restore(state);
	return true;
}

#if CONFIG_SLAB_TRACING

/** Function names to skip over in trace_return_address(). */
static const char *trace_skip_names[] = {
	"kmalloc", "krealloc", "kcalloc", "kfree", "kstrdup", "kstrndup",
	"kmemdup", "kbasename", "kdirname",
};

/** Get the address for allocation tracing output.
 * @param cache		Cache being allocated from. */
static __always_inline void *trace_return_address(slab_cache_t *cache) {
	symbol_t sym;
	void *addr;
	size_t i, j;

	addr = __builtin_return_address(0);
	if(!symbol_from_addr((ptr_t)addr - 1, &sym, NULL))
		return addr;

	/* If we're called through another allocation function, we want the
	 * address printed to be the caller of that. This can be multiple
	 * levels deep, e.g. kstrdup -> kmalloc -> slab_cache_alloc. We have to
	 * pass a constant integer to __builtin_return_address, so hardcode
	 * this for 2 levels deep. Yeah, this is terribly inefficient, but this
	 * is only enabled for debugging. */
	for(i = 0; i < ARRAY_SIZE(trace_skip_names); i++) {
		if(strcmp(trace_skip_names[i], sym.name) == 0) {
			addr = __builtin_return_address(1);
			if(!symbol_from_addr((ptr_t)addr - 1, &sym, NULL))
				return addr;

			for(j = 0; j < ARRAY_SIZE(trace_skip_names); j++) {
				if(strcmp(trace_skip_names[j], sym.name) == 0)
					return __builtin_return_address(2);
			}

			return addr;
		}
	}

	return addr;
}

#endif /* CONFIG_SLAB_TRACING */

/** Allocate a constructed object from a slab cache.
 * @param cache		Cache to allocate from.
 * @param mmflag	Allocation behaviour flags.
 * @return		Pointer to allocated object or NULL if unable to
 *			allocate.  */
void *slab_cache_alloc(slab_cache_t *cache, unsigned mmflag) {
	void *ret;

	assert(cache);

	if(!(cache->flags & SLAB_CACHE_NOMAG)) {
		ret = slab_cpu_obj_alloc(cache);
		if(likely(ret)) {
			#if CONFIG_SLAB_STATS
			atomic_inc(&cache->alloc_total);
			atomic_inc(&cache->alloc_current);
			#endif
			#if CONFIG_SLAB_TRACING
			kprintf(LOG_DEBUG, "slab: allocated %p from %s at %pB\n",
				ret, cache->name, trace_return_address(cache));
			#endif
			return ret;
		}
	}

	/* Cannot allocate from magazine layer, allocate from slab layer. */
	ret = slab_obj_alloc(cache, mmflag);
	if(likely(ret)) {
		#if CONFIG_SLAB_STATS
		atomic_inc(&cache->alloc_total);
		atomic_inc(&cache->alloc_current);
		#endif
		#if CONFIG_SLAB_TRACING
		kprintf(LOG_DEBUG, "slab: allocated %p from %s at %pB\n",
			ret, cache->name, trace_return_address(cache));
		#endif
	}

	return ret;
}

/** Free an object to a slab cache.
 * @param cache		Cache to free to.
 * @param obj		Object to free. */
void slab_cache_free(slab_cache_t *cache, void *obj) {
	assert(cache);

	if(!(cache->flags & SLAB_CACHE_NOMAG)) {
		if(likely(slab_cpu_obj_free(cache, obj))) {
			#if CONFIG_SLAB_STATS
			atomic_dec(&cache->alloc_current);
			#endif
			#if CONFIG_SLAB_TRACING
			kprintf(LOG_DEBUG, "slab: freed %p to %s at %pB\n",
				obj, cache->name, trace_return_address(cache));
			#endif
			return;
		}
	}

	/* Cannot free to magazine layer, free to slab layer. */
	slab_obj_free(cache, obj);
	#if CONFIG_SLAB_STATS
	atomic_dec(&cache->alloc_current);
	#endif
	#if CONFIG_SLAB_TRACING
	kprintf(LOG_DEBUG, "slab: freed %p to %s at %pB\n", obj, cache->name,
		trace_return_address(cache));
	#endif
}

/** Create the per-CPU data for a slab cache.
 * @param cache         Cache to create for.
 * @return		Status code describing result of the operation. */
static status_t slab_percpu_init(slab_cache_t *cache, unsigned mmflag) {
	assert(cpu_count != 0);
	assert(slab_percpu_cache);

	cache->cpu_caches = slab_cache_alloc(slab_percpu_cache, mmflag);
	if(!cache->cpu_caches)
		return STATUS_NO_MEMORY;

	memset(cache->cpu_caches, 0, sizeof(slab_percpu_t) * (highest_cpu_id + 1));
	return STATUS_SUCCESS;
}

/** Initialize a slab cache.
 * @param cache		Cache to initialize.
 * @param name		Name of cache (for debugging purposes).
 * @param size		Size of each object.
 * @param align		Alignment of each object. Must be a power of two.
 * @param ctor		Constructor callback - performs one-time initialization
 *			of an object (optional).
 * @param dtor		Destructor callback - undoes anything done by the
 *			constructor, if applicable (optional).
 * @param data		Data to pass as second parameter to callback functions.
 * @param priority	Reclaim priority (lower values will be reclaimed before
 *			higher values).
 * @param flags		Flags to modify the behaviour of the cache.
 * @param mmflag	Allocation behaviour flags.
 * @return		Status code describing result of the operation. */
static status_t slab_cache_init(slab_cache_t *cache, const char *name,
	size_t size, size_t align, slab_ctor_t ctor, slab_dtor_t dtor,
	void *data, int priority, unsigned flags, unsigned mmflag)
{
	slab_cache_t *exist;
	status_t ret;

	assert(size);
	assert(!align || is_pow2(align));
	assert(!(flags & SLAB_CACHE_LATEMAG));

	mutex_init(&cache->depot_lock, "slab_depot_lock", 0);
	mutex_init(&cache->slab_lock, "slab_slab_lock", 0);
	list_init(&cache->magazine_full);
	list_init(&cache->magazine_empty);
	list_init(&cache->slab_partial);
	list_init(&cache->slab_full);
	list_init(&cache->header);
	#if CONFIG_SLAB_STATS
	atomic_set(&cache->alloc_current, 0);
	atomic_set(&cache->alloc_total, 0);
	#endif
	cache->slab_count = 0;

	memset(cache->bufctl_hash, 0, sizeof(cache->bufctl_hash));

	strncpy(cache->name, name, SLAB_NAME_MAX);
	cache->name[SLAB_NAME_MAX - 1] = 0;

	cache->flags = flags;
	cache->ctor = ctor;
	cache->dtor = dtor;
	cache->data = data;
	cache->priority = priority;
	cache->colour_next = 0;

	/* Alignment must be at lest SLAB_ALIGN_MIN. */
	cache->align = max(SLAB_ALIGN_MIN, align);

	/* Make sure the object size is aligned. */
	size = round_up(size, cache->align);
	cache->obj_size = size;

	/* If the cache contains large objects, set the large flag which causes
	 * to not store metadata within allocated space. */
	if(size >= (PAGE_SIZE / SLAB_LARGE_FRACTION)) {
		cache->flags |= SLAB_CACHE_LARGE;

		/* Compute the appropriate slab size. */
		cache->slab_size = round_up(size, PAGE_SIZE);
		while((cache->slab_size % size) > (cache->slab_size / SLAB_WASTE_FRACTION))
			cache->slab_size += PAGE_SIZE;

		cache->obj_count = cache->slab_size / size;
		cache->colour_max = cache->slab_size - (cache->obj_count * size);
	} else {
		cache->slab_size = PAGE_SIZE;
		cache->obj_count = (cache->slab_size - sizeof(slab_t))
			/ size;
		cache->colour_max = (cache->slab_size -
			(cache->obj_count * size)) - sizeof(slab_t);
	}

	/* If we want the magazine layer to be enabled but the CPU count is
	 * not known, disable it until it is known. */
	if(!(cache->flags & SLAB_CACHE_NOMAG) && !slab_percpu_cache)
		cache->flags |= (SLAB_CACHE_NOMAG | SLAB_CACHE_LATEMAG);

	/* Initialize the CPU caches if required. */
	if(!(cache->flags & SLAB_CACHE_NOMAG)) {
		ret = slab_percpu_init(cache, mmflag);
		if(ret != STATUS_SUCCESS)
			return ret;
	}

	mutex_lock(&slab_caches_lock);

	/* Add the cache to the global cache list, keeping it ordered by
	 * priority. */
	if(list_empty(&slab_caches)) {
		list_append(&slab_caches, &cache->header);
	} else {
		LIST_FOREACH(&slab_caches, iter) {
			exist = list_entry(iter, slab_cache_t, header);

			if(exist->priority > priority) {
				list_add_before(&exist->header, &cache->header);
				break;
			} else if(exist->header.next == &slab_caches) {
				list_append(&slab_caches, &cache->header);
				break;
			}
		}
	}

	mutex_unlock(&slab_caches_lock);

	kprintf(LOG_DEBUG, "slab: created cache %s (obj_size: %u, slab_size: %u"
		", align: %u)\n", cache->name, cache->obj_size, cache->slab_size,
		cache->align);
	return STATUS_SUCCESS;
}

/** Create a slab cache.
 * @param name		Name of cache (for debugging purposes).
 * @param size		Size of each object.
 * @param align		Alignment of each object. Must be a power of two.
 * @param ctor		Constructor callback (optional).
 * @param dtor		Destructor callback (optional).
 * @param data		Data to pass as second parameter to callback functions.
 * @param flags		Flags to modify the behaviour of the cache.
 * @param mmflag	Allocation behaviour flags.
 * @return		Pointer to cache on success, NULL on failure. */
slab_cache_t *slab_cache_create(const char *name, size_t size, size_t align,
	slab_ctor_t ctor, slab_dtor_t dtor, void *data, unsigned flags,
	unsigned mmflag)
{
	slab_cache_t *cache;
	status_t ret;

	cache = slab_cache_alloc(&slab_cache_cache, mmflag);
	if(!cache)
		return NULL;

	ret = slab_cache_init(cache, name, size, align, ctor, dtor, data,
		SLAB_DEFAULT_PRIORITY, flags, mmflag);
	if(ret != STATUS_SUCCESS) {
		slab_cache_free(&slab_cache_cache, cache);
		return NULL;
	}

	return cache;
}

/** Destroy a slab cache.
 * @param cache		Cache to destroy. */
void slab_cache_destroy(slab_cache_t *cache) {
	assert(cache);

	mutex_lock(&cache->depot_lock);

	/* Destroy empty magazines. */
	LIST_FOREACH_SAFE(&cache->magazine_empty, iter)
		slab_magazine_destroy(cache, list_entry(iter, slab_magazine_t, header));

	/* Destroy full magazines. */
	LIST_FOREACH_SAFE(&cache->magazine_full, iter)
		slab_magazine_destroy(cache, list_entry(iter, slab_magazine_t, header));

	mutex_unlock(&cache->depot_lock);

	mutex_lock(&cache->slab_lock);

	if(!list_empty(&cache->slab_partial) || !list_empty(&cache->slab_full))
		fatal("Cache %s still has allocations during destruction", cache->name);

	mutex_unlock(&cache->slab_lock);

	mutex_lock(&slab_caches_lock);
	list_remove(&cache->header);
	mutex_unlock(&slab_caches_lock);

	slab_cache_free(&slab_cache_cache, cache);
}

/** Prints a list of all slab caches.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_slab(int argc, char **argv, kdb_filter_t *filter) {
	slab_cache_t *cache;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s\n\n", argv[0]);

		kdb_printf("Prints a list of all active slab caches and some statistics about them.\n");
		return KDB_SUCCESS;
	}

	#if CONFIG_SLAB_STATS
	kdb_printf("Name                      Align  Obj Size Slab Size Flags Slab Count Current Total\n");
	kdb_printf("====                      =====  ======== ========= ===== ========== ======= =====\n");
	#else
	kdb_printf("Name                      Align  Obj Size Slab Size Flags Slab Count\n");
	kdb_printf("====                      =====  ======== ========= ===== ==========\n");
	#endif

	LIST_FOREACH(&slab_caches, iter) {
		cache = list_entry(iter, slab_cache_t, header);

		#if CONFIG_SLAB_STATS
		kdb_printf("%-*s %-6zu %-8zu %-9zu %-5d %-10zu %-7d %d\n",
			SLAB_NAME_MAX, cache->name, cache->align, cache->obj_size,
			cache->slab_size, cache->flags, cache->slab_count,
			atomic_get(&cache->alloc_current),
			atomic_get(&cache->alloc_total));
		#else
		kdb_printf("%-*s %-6zu %-8zu %-9zu %-5d %zu\n",
			SLAB_NAME_MAX, cache->name, cache->align, cache->obj_size,
			cache->slab_size, cache->flags, cache->slab_count);
		#endif
	}

	return KDB_SUCCESS;
}

/** Initialize the slab allocator. */
__init_text void slab_init(void) {
	/* Intialise the cache for cache structures. */
	slab_cache_init(&slab_cache_cache, "slab_cache_cache", sizeof(slab_cache_t),
		alignof(slab_cache_t), NULL, NULL, NULL, SLAB_METADATA_PRIORITY,
		0, MM_BOOT);

	/* Initialize the magazine cache. This cannot have the magazine layer
	 * enabled, for pretty obvious reasons. */
	slab_cache_init(&slab_mag_cache, "slab_mag_cache", sizeof(slab_magazine_t),
		alignof(slab_magazine_t), NULL, NULL, NULL, SLAB_MAG_PRIORITY,
		SLAB_CACHE_NOMAG, MM_BOOT);

	/* Create other internal caches. */
	slab_cache_init(&slab_bufctl_cache, "slab_bufctl_cache", sizeof(slab_bufctl_t),
		alignof(slab_bufctl_t), NULL, NULL, NULL, SLAB_METADATA_PRIORITY,
		0, MM_BOOT);
	slab_cache_init(&slab_slab_cache, "slab_slab_cache", sizeof(slab_t),
		alignof(slab_t), NULL, NULL, NULL, SLAB_METADATA_PRIORITY, 0,
		MM_BOOT);

	/* Register the KDB command. */
	kdb_register_command("slab", "Display slab cache statistics.", kdb_cmd_slab);
}

/** Enable the magazine layer. */
__init_text void slab_late_init(void) {
	slab_cache_t *cache;
	size_t size;

	/* Create the cache for per-CPU structures. */
	size = sizeof(slab_percpu_t) * (highest_cpu_id + 1);
	slab_percpu_cache = slab_cache_alloc(&slab_cache_cache, MM_BOOT);
	slab_cache_init(slab_percpu_cache, "slab_percpu_cache", size,
		alignof(slab_percpu_t), NULL, NULL, NULL, SLAB_METADATA_PRIORITY,
		SLAB_CACHE_NOMAG, MM_BOOT);

	mutex_lock(&slab_caches_lock);

	/* Create per-CPU structures for all caches that want the magazine layer. */
	LIST_FOREACH(&slab_caches, iter) {
		cache = list_entry(iter, slab_cache_t, header);

		if(cache->flags & SLAB_CACHE_LATEMAG) {
			assert(cache->flags & SLAB_CACHE_NOMAG);
			slab_percpu_init(cache, MM_BOOT);
			cache->flags &= ~(SLAB_CACHE_LATEMAG | SLAB_CACHE_NOMAG);
		}
	}

	mutex_unlock(&slab_caches_lock);
}
