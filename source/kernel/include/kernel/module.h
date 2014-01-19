/*
 * Copyright (C) 2010-2013 Gil Mendes
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
 * @brief		Kernel module functions.
 */

#ifndef __KERNEL_MODULE_H
#define __KERNEL_MODULE_H

#include <kernel/limits.h>
#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Module information structure. */
typedef struct module_info {
	char name[MODULE_NAME_MAX];	/**< Name of the module. */
	char desc[MODULE_DESC_MAX];	/**< Description of the module. */
	size_t count;			/**< Reference count of the module. */
	size_t load_size;		/**< Size of the module in memory. */
} module_info_t;

extern status_t kern_module_load(const char *path, char *depbuf);
extern status_t kern_module_info(module_info_t *infop, size_t *countp);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_MODULE_H */
