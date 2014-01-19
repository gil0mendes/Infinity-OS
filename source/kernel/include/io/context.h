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

#ifndef __IO_CONTEXT_H
#define __IO_CONTEXT_H

#include <sync/rwlock.h>

struct fs_dentry;

/** Structure containing an I/O context. */
typedef struct io_context {
	rwlock_t lock;			/**< Lock to protect context. */
	struct fs_dentry *root_dir;	/**< Root directory. */
	struct fs_dentry *curr_dir;	/**< Current working directory. */
} io_context_t;

extern void io_context_init(io_context_t *context, io_context_t *parent);
extern void io_context_destroy(io_context_t *context);
extern void io_context_set_curr_dir(io_context_t *context,
	struct fs_dentry *entry);
extern void io_context_set_root_dir(io_context_t *context,
	struct fs_dentry *entry);

#endif /* __IO_CONTEXT_H */
