/*
 * Copyright (C) 2012-2013 Gil Mendes
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file
 * @brief		TAR filesystem handler.
 */

#ifndef __TAR_H
#define __TAR_H

/** Header for a TAR file. */
typedef struct tar_header {
	char name[100];		/**< Name of entry. */
	char mode[8];		/**< Mode of entry. */
	char uid[8];		/**< User ID. */
	char gid[8];		/**< Group ID. */
	char size[12];		/**< Size of entry. */
	char mtime[12];		/**< Modification time. */
	char chksum[8];		/**< Checksum. */
	char typeflag;		/**< Type flag. */
	char linkname[100];	/**< Symbolic link name. */
	char magic[6];		/**< Magic string. */
	char version[2];	/**< TAR version. */
	char uname[32];		/**< User name. */
	char gname[32];		/**< Group name. */
	char devmajor[8];	/**< Device major. */
	char devminor[8];	/**< Device minor. */
	char prefix[155];	/**< Prefix. */
} tar_header_t;

/** TAR entry types. */
#define REGTYPE		'0'	/**< Regular file (preferred code). */
#define AREGTYPE	'\0'	/**< Regular file (alternate code). */
#define LNKTYPE		'1'	/**< Hard link. */
#define SYMTYPE		'2'	/**< Symbolic link (hard if not supported). */
#define CHRTYPE		'3'	/**< Character special. */
#define BLKTYPE		'4'	/**< Block special. */
#define DIRTYPE		'5'	/**< Directory.  */
#define FIFOTYPE	'6'	/**< Named pipe.  */
#define CONTTYPE	'7'	/**< Contiguous file. */

extern void tar_mount(void *addr, size_t size);

#endif /* __TAR_H */
