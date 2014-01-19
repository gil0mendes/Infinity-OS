/*
 * Copyright (C) 2013 Alex Smith
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
 * @brief		Internal libsystem definitions.
 */

#ifndef __SYSTEM__INTERNAL_H
#define __SYSTEM__INTERNAL_H

/** Compiler attribute definitions. */
#define __libsystem_unused		__attribute__((unused))
#define __libsystem_used		__attribute__((used))
#define __libsystem_packed		__attribute__((packed))
#define __libsystem_aligned(a)		__attribute__((aligned(a)))
#define __libsystem_noreturn		__attribute__((noreturn))
#define __libsystem_malloc		__attribute__((malloc))
#define __libsystem_printf(a, b)	__attribute__((format(printf, a, b)))
#define __libsystem_deprecated		__attribute__((deprecated))

#endif /* __SYSTEM__INTERNAL_H */
