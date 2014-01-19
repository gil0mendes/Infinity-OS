/*
 * Copyright (C) 2009-2010 Gil Mendes
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
 * @brief		Kernel status code definitions.
 */

#ifndef __KERNEL_STATUS_H
#define __KERNEL_STATUS_H

/** Definitions of status codes returned by kernel functions. */
#define STATUS_SUCCESS			0	/**< Operation completed successfully. */
#define STATUS_NOT_IMPLEMENTED		1	/**< Operation not implemented. */
#define STATUS_NOT_SUPPORTED		2	/**< Operation not supported. */
#define STATUS_WOULD_BLOCK		3	/**< Operation would block. */
#define STATUS_INTERRUPTED		4	/**< Interrupted while blocking. */
#define STATUS_TIMED_OUT		5	/**< Timed out while waiting. */
#define STATUS_INVALID_SYSCALL		6	/**< Invalid system call number. */
#define STATUS_INVALID_ARG		7	/**< Invalid argument specified. */
#define STATUS_INVALID_HANDLE		8	/**< Non-existant handle or handle with incorrect type. */
#define STATUS_INVALID_ADDR		9	/**< Invalid memory location specified. */
#define STATUS_INVALID_REQUEST		10	/**< Invalid device request specified. */
#define STATUS_INVALID_EVENT		11	/**< Invalid object event specified. */
#define STATUS_OVERFLOW			12	/**< Integer overflow. */
#define STATUS_NO_MEMORY		13	/**< Out of memory. */
#define STATUS_NO_HANDLES		14	/**< No handles are available. */
#define STATUS_NO_SEMAPHORES		15	/**< No semaphores are available. */
#define STATUS_NO_AREAS			16	/**< No shared memory areas are available. */
#define STATUS_PROCESS_LIMIT		17	/**< Process limit reached. */
#define STATUS_THREAD_LIMIT		18	/**< Thread limit reached. */
#define STATUS_READ_ONLY		19	/**< Object cannot be modified. */
#define STATUS_PERM_DENIED		20	/**< Operation not permitted. */
#define STATUS_ACCESS_DENIED		21	/**< Requested access rights denied. */
#define STATUS_NOT_DIR			22	/**< Path component is not a directory. */
#define STATUS_NOT_REGULAR		23	/**< Path does not refer to a regular file. */
#define STATUS_NOT_SYMLINK		24	/**< Path does not refer to a symbolic link. */
#define STATUS_NOT_MOUNT		25	/**< Path does not refer to root of a mount. */
#define STATUS_NOT_FOUND		26	/**< Requested object could not be found. */
#define STATUS_NOT_EMPTY		27	/**< Directory is not empty. */
#define STATUS_ALREADY_EXISTS		28	/**< Object already exists. */
#define STATUS_TOO_SMALL		29	/**< Provided buffer is too small. */
#define STATUS_TOO_LARGE		30	/**< Provided buffer is too large. */
#define STATUS_TOO_LONG			31	/**< Provided string is too long. */
#define STATUS_DIR_FULL			32	/**< Directory is full. */
#define STATUS_UNKNOWN_FS		33	/**< Filesystem has an unrecognised format. */
#define STATUS_CORRUPT_FS		34	/**< Corruption detected on the filesystem. */
#define STATUS_FS_FULL			35	/**< No space is available on the filesystem. */
#define STATUS_SYMLINK_LIMIT		36	/**< Exceeded nested symbolic link limit. */
#define STATUS_IN_USE			37	/**< Object is in use. */
#define STATUS_DEVICE_ERROR		38	/**< An error occurred during a hardware operation. */
#define STATUS_STILL_RUNNING		39	/**< Process/thread is still running. */
#define STATUS_UNKNOWN_IMAGE		40	/**< Executable image has an unrecognised format. */
#define STATUS_MALFORMED_IMAGE		41	/**< Executable image format is incorrect. */
#define STATUS_MISSING_LIBRARY		42	/**< Required library not found. */
#define STATUS_MISSING_SYMBOL		43	/**< Referenced symbol not found. */
#define STATUS_TRY_AGAIN		44	/**< Attempt the operation again. */
#define STATUS_DIFFERENT_FS		45	/**< Link source and destination on different FS. */
#define STATUS_IS_DIR			46	/**< Not a directory. */
#define STATUS_CONN_HUNGUP		47	/**< Connection was hung up. */
#define STATUS_CONN_ACTIVE		48	/**< Connection is already active. */
#define STATUS_CONN_INACTIVE		49	/**< Connection is currently inactive. */

#if !defined(KERNEL) && !defined(__ASM__)
#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const char *__kernel_status_strings[];
extern size_t __kernel_status_size;

#ifdef __cplusplus
}
#endif
#endif
#endif /* __KERNEL_STATUS_H */
