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
 * @brief		I/O context functions.
 */

#include <io/fs.h>

#include <proc/process.h>

#include <assert.h>
#include <status.h>

/**
 * Initialize an I/O context.
 *
 * Initializes an I/O context structure. If a parent context is provided, then
 * the new context will inherit parts of the parent context such as current
 * working directory. If no parent is specified, the working directory will be
 * set to the root of the filesystem.
 *
 * @param context	Context to initialize.
 * @param parent	Parent context (can be NULL).
 */
void io_context_init(io_context_t *context, io_context_t *parent) {
	rwlock_init(&context->lock, "io_context_lock");

	/* Inherit parent's current/root directories if possible. */
	if(parent) {
		rwlock_read_lock(&parent->lock);

		assert(parent->root_dir);
		assert(parent->curr_dir);

		fs_dentry_retain(parent->root_dir);
		context->root_dir = parent->root_dir;
		fs_dentry_retain(parent->curr_dir);
		context->curr_dir = parent->curr_dir;

		rwlock_unlock(&parent->lock);
	} else if(root_mount) {
		fs_dentry_retain(root_mount->root);
		context->root_dir = root_mount->root;
		fs_dentry_retain(root_mount->root);
		context->curr_dir = root_mount->root;
	} else {
		/* This should only be the case when the kernel process is
		 * being created. They will be set when the FS is
		 * initialized. */
		assert(!kernel_proc);

		context->curr_dir = NULL;
		context->root_dir = NULL;
	}
}

/** Destroy an I/O context.
 * @param context	Context to destroy. */
void io_context_destroy(io_context_t *context) {
	fs_dentry_release(context->curr_dir);
	fs_dentry_release(context->root_dir);
}

/**
 * Set the working directory of an I/O context.
 *
 * Sets the working directory of an I/O context to the specified directory
 * entry. The previous working directory will be released, and the supplied
 * entry will be referenced.
 *
 * @param context	Context to set directory of.
 * @param entry		Entry to set to.
 */
void io_context_set_curr_dir(io_context_t *context, fs_dentry_t *entry) {
	fs_dentry_t *prev;

	fs_dentry_retain(entry);

	rwlock_write_lock(&context->lock);
	prev = context->curr_dir;
	context->curr_dir = entry;
	rwlock_unlock(&context->lock);

	fs_dentry_release(prev);
}

/**
 * Set the root directory of an I/O context.
 *
 * Sets both the root directory and working directory of an I/O context to
 * the specified directory entry.
 *
 * @param context	Context to set in.
 * @param entry		Entry to set to.
 */
void io_context_set_root_dir(io_context_t *context, fs_dentry_t *entry) {
	fs_dentry_t *prev_curr, *prev_root;

	/* Reference twice: one for root, one for working. */
	fs_dentry_retain(entry);
	fs_dentry_retain(entry);

	rwlock_write_lock(&context->lock);
	prev_curr = context->curr_dir;
	context->curr_dir = entry;
	prev_root = context->root_dir;
	context->root_dir = entry;
	rwlock_unlock(&context->lock);

	fs_dentry_release(prev_curr);
	fs_dentry_release(prev_root);
}
