/*
 * Copyright (C) 2008-2010 Gil Mendes
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
 * @brief		Directory handling functions.
 */

#ifndef __SYSTEM_DIRENT_H
#define __SYSTEM_DIRENT_H

#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <dirent.h>

#include "libsystem.h"

/** Size of the internal directory entry buffer. */
#define DIRSTREAM_BUF_SIZE	0x1000

struct __dstream_internal {
	handle_t handle;		/**< Handle to the directory. */
	char buf[DIRSTREAM_BUF_SIZE];	/**< Buffer for entry structures. */
};

#endif /* __SYSTEM_DIRENT_H */