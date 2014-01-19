/*
 * Copyright (C) 2010-2013 Alex Smith
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
 * @brief		POSIX process creation function.
 */

#include <kernel/object.h>
#include <kernel/process.h>
#include <kernel/status.h>

#include <stdlib.h>

#include "posix_priv.h"

/** Structure containing a fork handler. */
typedef struct fork_handler {
	list_t header;			/**< List link. */
	void (*func)(void);		/**< Function to call. */
} fork_handler_t;

/** List of fork handlers. */
static LIST_DECLARE(fork_handlers);

/** List of child processes created via fork(). */
LIST_DECLARE(child_processes);

/** Lock for child process list. */
int32_t child_processes_lock = MUTEX_INITIALIZER;

/**
 * Create a clone of the calling process.
 *
 * Creates a clone of the calling process. The new process will have a clone of
 * the original process' address space. Data in private mappings will be copied
 * when either the parent or the child writes to them. Non-private mappings
 * will be shared between the processes: any modifications made be either
 * process will be visible to the other. The new process will inherit all
 * file descriptors from the parent, including ones marked as FD_CLOEXEC. Only
 * the calling thread will be duplicated, however. Other threads will not be
 * duplicated into the new process.
 *
 * @return		0 in the child process, process ID of the child in the
 *			parent, or -1 on failure, with errno set appropriately.
 */
pid_t fork(void) {
	posix_process_t *process;
	fork_handler_t *handler;
	status_t ret;

	/* Allocate a child process structure. Do this before creating the
	 * process so we don't fail after creating the child. */
	process = malloc(sizeof(*process));
	if(!process)
		return -1;

	ret = kern_process_clone(&process->handle);
	if(ret != STATUS_SUCCESS) {
		libsystem_status_to_errno(ret);
		free(process);
		return -1;
	}

	if(process->handle == INVALID_HANDLE) {
		/* This is the child. Free the unneeded process structure. */
		free(process);

		/* Empty the child processes list: anything in there is not our
		 * child, but a child of our parent. */
		LIST_FOREACH_SAFE(&child_processes, iter) {
			process = list_entry(iter, posix_process_t, header);

			/* Handles are all invalid as they should not be marked
			 * as inheritable, but try to close them anyway just in
			 * case the user is doing something daft. */
			kern_handle_close(process->handle);
			list_remove(&process->header);
			free(process);
		}

		/* Run post-fork handlers. */
		LIST_FOREACH(&fork_handlers, iter) {
			handler = list_entry(iter, fork_handler_t, header);
			handler->func();
		}

		return 0;
	} else {
		list_init(&process->header);
		process->pid = kern_process_id(process->handle);
		if(process->pid < 1)
			libsystem_fatal("could not get ID of child");

		/* Add it to the child list. */
		kern_mutex_lock(&child_processes_lock, -1);
		list_append(&child_processes, &process->header);
		kern_mutex_unlock(&child_processes_lock);

		return process->pid;
	}
}

/** Register a function to be called after a fork in the child.
 * @param func		Function to call. */
void register_fork_handler(void (*func)(void)) {
	fork_handler_t *handler;

	handler = malloc(sizeof(*handler));
	if(!handler)
		libsystem_fatal("failed to register fork handler");

	handler->func = func;
	list_init(&handler->header);
	list_append(&fork_handlers, &handler->header);
}
