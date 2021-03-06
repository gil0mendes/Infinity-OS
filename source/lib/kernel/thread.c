/*
 * Copyright (C) 2010-2014 Gil Mendes
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
 * @brief		Thread functions.
 */

#include <kernel/futex.h>
#include <kernel/object.h>
#include <kernel/private/thread.h>

#include <inttypes.h>

#include "libkernel.h"

/** Information used by thread_create(). */
typedef struct thread_create {
    volatile int32_t futex;		/**< Futex to wait on. */
    tls_tcb_t *tcb;             /**< TLS thread control block */
    thread_entry_t entry;		/**< Real entry point. */
	void *arg;			        /**< Real entry point argument. */
} thread_create_t;

/** Saved ID for the current thread. */
__thread thread_id_t curr_thread_id = -1;

/**
* Thread entry wrapper.
*
* @param _create	Pointer to information structure.
*/
static int
thread_trampoline(void *_create)
{
	thread_create_t *create = _create;
    thread_id_t id;
	thread_entry_t entry;
	void *arg;

    id = _kern_thread_id(THREAD_SELF);

    /* Set our TCB */
    dprintf("tls: TCB for thread %", PRId32 " is %p\n",
        id, create->tcb);
    kern_thread_control(THREAD_SET_TLS_ADDR, create->tcb, NULL);

    /* Save our ID */
    curr_thread_id = id;

	/* After we unblock the creating thread, create is no longer valid. */
	entry = create->entry;
	arg = create->arg;

	/* Unblock our creator. */
	create->futex = 1;
	kern_futex_wake((int32_t *)&create->futex, 1, NULL);

	/* Call the real entry point. */
	kern_thread_exit(entry(arg));
}

/**
 * Create a new thread.
 *
 * Create a new thread within the calling process and start it executing at
 * the given entry function. If a stack is provided for the thread, that will
 * be used, and the stack will not be freed when the thread exits. Otherwise,
 * a stack will be allocated for the thread with a default size, and will be
 * freed when the thread exits.
 *
 * @param name		Name to give the thread.
 * @param entry		Thread entry point.
 * @param arg		Argument to pass to the entry point.
 * @param stack		Details of the stack to use/allocate for the new thread
 *			(optional). See the documentation for thread_stack_t
 *			for details.
 * @param flags		Creation behaviour flags.
 * @param handlep	Where to store handle to the thread (optional).
 *
 * @return		Status code describing result of the operation.
 */
status_t __export
kern_thread_create(const char *name, thread_entry_t entry, void *arg,
	const thread_stack_t *stack, uint32_t flags, handle_t *handlep)
{
	thread_create_t create;
	status_t ret;

	if(!entry)
		return STATUS_INVALID_ARG;

    create.futex = 0;
	create.entry = entry;
	create.arg = arg;

    /* Allocate a TLS clock */
    ret = tls_alloc(&create.tcb);
    if (ret != STATUS_SUCCESS) {
        return ret;
    }

	/* Create the thread. */
	ret = _kern_thread_create(name, thread_trampoline, &create, stack, flags, handlep);
	if(ret != STATUS_SUCCESS) {
        tls_destroy(create.tcb);
        return ret;
    }

	/* Wait for the thread to complete TLS setup. TODO: There is a possible
	 * bug here: if the thread somehow ends up killed before it wakes us
	 * we will get stuck. We should create an event object instead and wait
	 * on both that and the thread so we get woken if the thread dies. */
	kern_futex_wait((int32_t *)&create.futex, 0, -1);

	return STATUS_SUCCESS;
}

/** Get the ID of a thread.
 * @param handle	Handle for thread to get ID of, or THREAD_SELF to get
 *			ID of the calling thread.
 * @return		Thread ID on success, -1 if handle is invalid. */
thread_id_t __export kern_thread_id(handle_t handle) {
	/* We save the current thread ID to avoid having to perform a kernel
	 * call just to get our own ID. */
	if(handle < 0) {
		return curr_thread_id;
	} else {
		return _kern_thread_id(handle);
	}
}

/**
* Terminate the calling thread.
*
* @param status	Exit status code.
*/
void __export
kern_thread_exit(int status)
{
    tls_tcb_t *tcb = arch_tls_tcb();

    dprintf("tls: destroying block %p for thread %" PRId32 "\n", tcb->base,
        curr_thread_id);
    tls_destroy(tcb);

	_kern_thread_exit(status);
}
