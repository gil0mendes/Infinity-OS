/*
 * Copyright (C) 2009-2014 Gil Mendes
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
 * @brief		Unidirectional data pipe implementation.
 */

#include <io/request.h>

#include <ipc/pipe.h>

#include <mm/kmem.h>
#include <mm/malloc.h>
#include <mm/slab.h>

#include <proc/thread.h>

#include <assert.h>
#include <kernel.h>
#include <object.h>
#include <status.h>

/** Cache for pipe structures. */
static slab_cache_t *pipe_cache;

/** Constructor for pipe structures.
 * @param obj		Object to construct.
 * @param data		Unused. */
static void pipe_ctor(void *obj, void *data) {
	pipe_t *pipe = obj;

	mutex_init(&pipe->lock, "pipe_lock", 0);
	notifier_init(&pipe->space_notifier, pipe);
	notifier_init(&pipe->data_notifier, pipe);
}

/** Read a byte from a pipe.
 * @param pipe		Pipe structure. Should be locked, and have data.
 * @return		Character read. */
static inline char pipe_get(pipe_t *pipe) {
	char ch;

	ch = pipe->buf[pipe->start];
	if(++pipe->start >= PIPE_SIZE)
		pipe->start = 0;

	semaphore_up(&pipe->space_sem, 1);
	return ch;
}

/** Write a byte to a pipe.
 * @param pipe		Pipe structure. Should be locked, and have space.
 * @param ch		Character to write. */
static inline void pipe_insert(pipe_t *pipe, char ch) {
	pipe->buf[pipe->end] = ch;
	if(++pipe->end >= PIPE_SIZE)
		pipe->end = 0;

	semaphore_up(&pipe->data_sem, 1);
}

/**
 * Read data from a pipe.
 *
 * Reads data from a pipe into a buffer. Reads of less than or equal to
 * PIPE_SIZE will either read all the requested data, or none at all. Reads of
 * greater than PIPE_SIZE may only return part of the data. A read may not be
 * able to read all data if the calling thread is interrupted, or non-blocking
 * mode is requested.
 *
 * @param pipe		Pipe to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param nonblock	Whether to allow blocking.
 * @param bytesp	Where to store number of bytes read.
 *
 * @return		Status code describing result of the operation.
 */
status_t pipe_read(pipe_t *pipe, char *buf, size_t count, bool nonblock, size_t *bytesp) {
	status_t ret = STATUS_SUCCESS;
	size_t i = 0;

	if(count && count <= PIPE_SIZE) {
		/* Try to get all required data before reading. */
		for(i = 0; i < count; i++) {
			ret = semaphore_down_etc(&pipe->data_sem, (nonblock) ? 0 : -1,
				SLEEP_INTERRUPTIBLE);
			if(ret != STATUS_SUCCESS) {
				semaphore_up(&pipe->data_sem, i);
				i = 0;
				goto out;
			}
		}

		mutex_lock(&pipe->lock);

		for(i = 0; i < count; i++)
			buf[i] = pipe_get(pipe);

		notifier_run(&pipe->space_notifier, NULL, false);

		mutex_unlock(&pipe->lock);
	} else if(count) {
		for(i = 0; i < count; i++) {
			ret = semaphore_down_etc(&pipe->data_sem, (nonblock) ? 0 : -1,
				SLEEP_INTERRUPTIBLE);
			if(ret != STATUS_SUCCESS)
				goto out;

			mutex_lock(&pipe->lock);
			buf[i] = pipe_get(pipe);
			notifier_run(&pipe->space_notifier, NULL, false);
			mutex_unlock(&pipe->lock);
		}
	}
out:
	if(bytesp)
		*bytesp = i;

	return ret;
}

/**
 * Write data to a pipe.
 *
 * Writes data from a buffer to a pipe. Writes of less than or equal to
 * PIPE_SIZE will either write all the requested data, or none at all. Writes
 * of greater than PIPE_SIZE may only write part of the data. A write may not
 * be able to write all data if the calling thread is interrupted, or
 * non-blocking mode is requested.
 *
 * @param pipe		Pipe to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param nonblock	Whether to allow blocking.
 * @param bytesp	Where to store number of bytes written.
 *
 * @return		Status code describing result of the operation.
 */
status_t pipe_write(pipe_t *pipe, const char *buf, size_t count, bool nonblock, size_t *bytesp) {
	status_t ret = STATUS_SUCCESS;
	size_t i = 0;

	if(count && count <= PIPE_SIZE) {
		/* Try to get all required space before writing. */
		for(i = 0; i < count; i++) {
			ret = semaphore_down_etc(&pipe->space_sem, (nonblock) ? 0 : -1,
				SLEEP_INTERRUPTIBLE);
			if(ret != STATUS_SUCCESS) {
				semaphore_up(&pipe->space_sem, i);
				i = 0;
				goto out;
			}
		}

		mutex_lock(&pipe->lock);

		for(i = 0; i < count; i++)
			pipe_insert(pipe, buf[i]);

		/* For atomic writes, we only run the data notifier after
		 * writing everything, so we don't do too many calls. */
		notifier_run(&pipe->data_notifier, NULL, false);

		mutex_unlock(&pipe->lock);
	} else if(count) {
		for(i = 0; i < count; i++) {
			ret = semaphore_down_etc(&pipe->space_sem, (nonblock) ? 0 : -1,
				SLEEP_INTERRUPTIBLE);
			if(ret != STATUS_SUCCESS)
				goto out;

			mutex_lock(&pipe->lock);
			pipe_insert(pipe, buf[i]);
			notifier_run(&pipe->data_notifier, NULL, false);
			mutex_unlock(&pipe->lock);
		}
	}
out:
	if(bytesp)
		*bytesp = i;

	return ret;
}

/** Perform I/O on a pipe.
 * @param pipe		Pipe to perform I/O on.
 * @param request	I/O request.
 * @param nonblock	Whether to allow blocking.
 * @return		Status code describing result of the operation. */
status_t pipe_io(pipe_t *pipe, io_request_t *request, bool nonblock) {
	char *buf;
	size_t count;
	status_t ret, err;

	buf = kmalloc(request->total, MM_USER);
	if(!buf)
		return STATUS_NO_MEMORY;

	if(request->op == IO_OP_READ) {
		ret = pipe_read(pipe, buf, request->total, nonblock, &count);

		if(count) {
			err = io_request_copy(request, buf, count);
			if(err != STATUS_SUCCESS)
				ret = err;
		}
	} else {
		ret = io_request_copy(request, buf, request->total);
		if(ret != STATUS_SUCCESS) {
			kfree(buf);
			return ret;
		}

		ret = pipe_write(pipe, buf, request->total, nonblock, &count);
		request->transferred -= request->total - count;
	}

	kfree(buf);
	return ret;
}

/**
 * Wait for a pipe to be readable or writable.
 *
 * Waits for a pipe to become readable or writable, and notifies the specified
 * object wait pointer when it is. This is a convenience function, for example
 * for devices that use pipes internally.
 *
 * @param pipe		Pipe to wait for.
 * @param write		Whether to wait to be writable (pipe is classed as
 *			        writable when there is space in the buffer).
 * @param event     Object event structure.
 */
void
pipe_wait(pipe_t *pipe, bool write, object_event_t *event)
{
	if(write) {
		if(semaphore_count(&pipe->space_sem)) {
			object_event_signal(event, 0);
		} else {
			notifier_register(&pipe->space_notifier, object_event_notifier, event);
		}
	} else {
		if(semaphore_count(&pipe->data_sem)) {
			object_event_signal(event, 0);
		} else {
			notifier_register(&pipe->data_notifier, object_event_notifier, event);
		}
	}
}

/**
* Stop waiting for a pipe event.
*
* @param pipe		Pipe to stop waiting for.
* @param write		Whether waiting to be writable.
* @param event      Object event structure.
*/
void
pipe_unwait(pipe_t *pipe, bool write, object_event_t *event)
{
	notifier_t *notifier = (write) ? &pipe->space_notifier : &pipe->data_notifier;
	notifier_unregister(notifier, object_event_notifier, event);
}

/** Create a new pipe.
 * @return		Pointer to pipe structure. */
pipe_t *pipe_create(void) {
	pipe_t *pipe;

	pipe = slab_cache_alloc(pipe_cache, MM_KERNEL);
	semaphore_init(&pipe->space_sem, "pipe_space_sem", PIPE_SIZE);
	semaphore_init(&pipe->data_sem, "pipe_data_sem", 0);
	pipe->buf = kmem_alloc(PIPE_SIZE, MM_KERNEL);
	pipe->start = 0;
	pipe->end = 0;
	return pipe;
}

/** Destroy a pipe.
 * @note		Caller must ensure that nothing is using the pipe.
 * @param pipe		Pipe to destroy. */
void pipe_destroy(pipe_t *pipe) {
	assert(!mutex_held(&pipe->lock));
	assert(notifier_empty(&pipe->space_notifier));
	assert(notifier_empty(&pipe->data_notifier));
	kmem_free(pipe->buf, PIPE_SIZE);
	slab_cache_free(pipe_cache, pipe);
}

/** Initialize the pipe slab cache. */
static __init_text void pipe_cache_init(void) {
	pipe_cache = object_cache_create("pipe_cache", pipe_t, pipe_ctor, NULL,
		NULL, 0, MM_BOOT);
}

INITCALL(pipe_cache_init);
