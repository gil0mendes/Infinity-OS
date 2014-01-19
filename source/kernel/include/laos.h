/*
 * Copyright (C) 2011-2013 Gil Mendes
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
 * @brief		LAOS utility functions.
 */

#ifndef __KERNEL_LAOS_H
#define __KERNEL_LAOS_H

#include <lib/utility.h>

#include "../../boot/include/laos.h"

extern laos_log_t *laos_log;
extern size_t laos_log_size;

extern void *laos_tag_iterate(uint32_t type, void *current);

/** Iterate over the LAOS tag list. */
#define LAOS_ITERATE(_type, _vtype, _vname) \
	for(_vtype *_vname = laos_tag_iterate((_type), NULL); \
		_vname; \
		_vname = laos_tag_iterate((_type), _vname))

/** Get additional data following a LAOS tag.
 * @param tag		Tag to get data from.
 * @param offset	Offset of the data to get.
 * @return		Pointer to data. */
#define laos_tag_data(tag, offset)	\
	((void *)(ROUND_UP((ptr_t)tag + sizeof(*tag), 8) + offset))

extern bool laos_boolean_option(const char *name);
extern uint64_t laos_integer_option(const char *name);
extern const char *laos_string_option(const char *name);

extern void laos_log_write(char ch);
extern void laos_log_flush(void);

#endif /* __KERNEL_LAOS_H */
