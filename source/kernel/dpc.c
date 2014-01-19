/*
 * Copyright (C) 2010 Gil Mendes
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
 * @brief		Deferred procedure call functions.
 *
 * @todo		Per-CPU DPC thread.
 */

#include <lib/list.h>

#include <mm/kmem.h>
#include <mm/page.h>

#include <proc/thread.h>

#include <sync/semaphore.h>
#include <sync/spinlock.h>

#include <assert.h>
#include <dpc.h>
#include <status.h>

/** Structure describing a DPC request. */
typedef struct dpc_request {
	list_t header;			/**< Link to requests/free list. */
	dpc_function_t function;	/**< Function to call. */
	void *arg;			/**< Argument to pass to handler. */
} dpc_request_t;

/** Lists of free and pending DPC requests. */
static LIST_DECLARE(dpc_free);
static LIST_DECLARE(dpc_requests);
static SPINLOCK_DECLARE(dpc_lock);

/** Semaphore that the DPC thread waits on. */
static SEMAPHORE_DECLARE(dpc_request_sem, 0);

/** DPC thread. */
static thread_t *dpc_thread = NULL;

/** DPC thread main function.
 * @param arg1		Unused.
 * @param arg2		Unused. */
static void dpc_thread_func(void *arg1, void *arg2) {
	dpc_request_t *request;

	while(true) {
		semaphore_down(&dpc_request_sem);

		/* Get the next request in the list. */
		spinlock_lock(&dpc_lock);
		assert(!list_empty(&dpc_requests));
		request = list_first(&dpc_requests, dpc_request_t, header);
		list_remove(&request->header);
		spinlock_unlock(&dpc_lock);

		/* Call the function. */
		request->function(request->arg);

		/* Return the structure to the free list. */
		spinlock_lock(&dpc_lock);
		list_prepend(&dpc_free, &request->header);
		spinlock_unlock(&dpc_lock);
	}
}

/** DPC structure allocator.
 * @return		Pointer to allocated structure. */
static dpc_request_t *dpc_request_alloc(void) {
	dpc_request_t *request;

	if(list_empty(&dpc_free)) {
		/* TODO: Allocate more before we run out. */
		fatal("Out of DPC request structures");
	}

	request = list_first(&dpc_free, dpc_request_t, header);
	list_remove(&request->header);
	return request;
}

/**
 * Make a DPC request.
 *
 * Adds a function to the DPC queue to be called by the DPC thread. This
 * function is safe to use from interrupt context.
 *
 * @param function	Function to call.
 * @param arg		Argument to pass to the function.
 */
void dpc_request(dpc_function_t function, void *arg) {
	dpc_request_t *request;

	spinlock_lock(&dpc_lock);

	request = dpc_request_alloc();
	request->function = function;
	request->arg = arg;

	/* Add it to the queue and wake up the DPC thread. */
	list_append(&dpc_requests, &request->header);
	semaphore_up(&dpc_request_sem, 1);

	spinlock_unlock(&dpc_lock);
}

/** Check whether the DPC system has been initialized.
 * @return		Whether initialized. */
bool dpc_inited(void) {
	return dpc_thread;
}

/** Initialize the DPC thread. */
__init_text void dpc_init(void) {
	dpc_request_t *alloc;
	status_t ret;
	size_t i;

	/* Allocate a chunk of DPC structures. We do not allocate a new
	 * structure upon every dpc_request() call to make it usable from
	 * interrupt context. */
	alloc = kmem_alloc(PAGE_SIZE, MM_BOOT);
	for(i = 0; i < (PAGE_SIZE / sizeof(dpc_request_t)); i++) {
		list_init(&alloc[i].header);
		list_append(&dpc_free, &alloc[i].header);
	}

	/* Create the DPC thread */
	ret = thread_create("dpc", NULL, 0, dpc_thread_func, NULL, NULL, &dpc_thread);
	if(ret != STATUS_SUCCESS)
		fatal("Failed to create DPC thread: %d\n", ret);

	thread_run(dpc_thread);
}
