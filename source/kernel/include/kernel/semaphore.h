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
 * @brief		Semaphore functions.
 */

#ifndef __KERNEL_SEMAPHORE_H
#define __KERNEL_SEMAPHORE_H

#include <kernel/object.h>

#ifdef __cplusplus
extern "C" {
#endif

extern status_t kern_semaphore_create(const char *name, size_t count, handle_t *handlep);
extern status_t kern_semaphore_open(semaphore_id_t id, handle_t *handlep);
extern semaphore_id_t kern_semaphore_id(handle_t handle);
extern status_t kern_semaphore_down(handle_t handle, nstime_t timeout);
extern status_t kern_semaphore_up(handle_t handle, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_SEMAPHORE_H */
