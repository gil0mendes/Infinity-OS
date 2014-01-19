/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		POSIX file open function.
 */

#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>

#include "posix_priv.h"

/** Convert POSIX open() flags to kernel flags.
 * @param oflag		POSIX open flags.
 * @param krightsp	Where to store kernel rights.
 * @param kflagsp	Where to store kernel flags.
 * @param kcreatep	Where to store kernel creation flags. */
static inline void convert_open_flags(int oflag, uint32_t *krightsp,
	uint32_t *kflagsp, unsigned *kcreatep)
{
	uint32_t krights = 0;
	uint32_t kflags = 0;

	if(oflag & O_RDONLY)
		krights |= FILE_RIGHT_READ;
	if(oflag & O_WRONLY)
		krights |= FILE_RIGHT_WRITE;

	*krightsp = krights;

	if(oflag & O_NONBLOCK)
		kflags |= FILE_NONBLOCK;
	if(oflag & O_APPEND)
		kflags |= FILE_APPEND;

	*kflagsp = kflags;

	if(oflag & O_CREAT) {
		*kcreatep = (oflag & O_EXCL) ? FS_MUST_CREATE : FS_CREATE;
	} else {
		*kcreatep = 0;
	}
}

/** Open a file or directory.
 * @param path		Path to file to open.
 * @param oflag		Flags controlling how to open the file.
 * @param ...		Mode to create the file with if O_CREAT is specified.
 * @return		File descriptor referring to file (positive value) on
 *			success, -1 on failure (errno will be set to the error
 *			reason). */
int open(const char *path, int oflag, ...) {
	file_type_t type;
	file_info_t info;
	uint32_t rights;
	uint32_t kflags;
	unsigned kcreate;
	handle_t handle;
	status_t ret;

	/* Check whether the arguments are valid. I'm not sure if the second
	 * check is correct, POSIX doesn't say anything about O_CREAT with
	 * O_DIRECTORY. */
	if(!(oflag & O_RDWR) || (oflag & O_EXCL && !(oflag & O_CREAT))) {
		errno = EINVAL;
		return -1;
	} else if(oflag & O_CREAT && oflag & O_DIRECTORY) {
		errno = EINVAL;
		return -1;
	} else if(!(oflag & O_WRONLY) && oflag & O_TRUNC) {
		errno = EACCES;
		return -1;
	}

	/* If O_CREAT is specified, we assume that we're going to be opening
	 * a file. Although POSIX doesn't specify anything about O_CREAT with
	 * a directory, Linux fails with EISDIR if O_CREAT is used with a
	 * directory that already exists. */
	if(oflag & O_CREAT) {
		type = FILE_TYPE_REGULAR;
	} else {
		ret = kern_fs_info(path, true, &info);
		if(ret != STATUS_SUCCESS) {
			libsystem_status_to_errno(ret);
			return -1;
		}

		type = info.type;

		if(oflag & O_DIRECTORY && type != FILE_TYPE_DIR) {
			errno = ENOTDIR;
			return -1;
		}
	}

	/* Convert the flags to kernel flags. */
	convert_open_flags(oflag, &rights, &kflags, &kcreate);

	/* Open according to the entry type. */
	switch(type) {
	case FILE_TYPE_DIR:
		if(oflag & O_WRONLY || oflag & O_TRUNC) {
			errno = EISDIR;
			return -1;
		}
	case FILE_TYPE_REGULAR:
		//if(oflag & O_CREAT) {
			///* Obtain the creation mask. */
			//va_start(args, oflag);
			//mode = va_arg(args, mode_t);
			//va_end(args);

			/* Apply the creation mode mask. */
			//mode &= ~current_umask;

			/* Convert the mode to a kernel ACL. */
			//security.acl = posix_mode_to_acl(NULL, mode);
			//if(!security.acl)
			//	return -1;
		//}

		/* Open the file, creating it if necessary. */
		ret = kern_fs_open(path, rights, kflags, kcreate, &handle);
		if(ret != STATUS_SUCCESS) {
			libsystem_status_to_errno(ret);
			return -1;
		}

		/* Truncate the file if requested. */
		if(oflag & O_TRUNC) {
			ret = kern_file_resize(handle, 0);
			if(ret != STATUS_SUCCESS) {
				kern_handle_close(handle);
				libsystem_status_to_errno(ret);
				return -1;
			}
		}
		break;
	default:
		errno = ENOTSUP;
		return -1;
	}

	/* Mark the handle as inheritable if not opening with O_CLOEXEC. */
	if(!(oflag & O_CLOEXEC))
		kern_handle_set_flags(handle, HANDLE_INHERITABLE);

	return (int)handle;
}

/**
 * Open and possibly create a file.
 *
 * Opens a file, creating it if it does not exist. If it does exist, it will be
 * truncated to zero length.
 *
 * @param path		Path to file.
 * @param mode		Mode to create file with if it doesn't exist.
 *
 * @return		File descriptor referring to file (positive value) on
 *			success, -1 on failure (errno will be set to the error
 *			reason).
 */
int creat(const char *path, mode_t mode) {
	return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}
