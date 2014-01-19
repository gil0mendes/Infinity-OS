/*
 * Copyright (C) 2007-2013 Alex Smith
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
 * @brief		String comparison functions.
 */

#include <string.h>

/** Compare two strings.
 * @param s1		Pointer to the first string.
 * @param s2		Pointer to the second string.
 * @return		An integer less than, equal to or greater than 0 if
 *			s1 is found, respectively, to be less than, to match,
 *			or to be greater than s2. */
int strcmp(const char *s1, const char *s2) {
	for(; *s1 && *s2 && *s1 == *s2; s1++, s2++);
	return *s1 - *s2;
}

/** Compare two strings with a length limit.
 * @param s1		Pointer to the first string.
 * @param s2		Pointer to the second string.
 * @param count		Maximum number of bytes to compare.
 * @return		An integer less than, equal to or greater than 0 if
 *			s1 is found, respectively, to be less than, to match,
 *			or to be greater than s2. */
int strncmp(const char *s1, const char *s2, size_t count) {
	const char *a = s1, *b = s2, *fini = a + count;
	int res;

	while(a < fini) {
		res = *a - *b;
		if(res)
			return res;
		if(!*a)
			return 0;

		a++;
		b++;
	}
	return 0;
}

int strcoll(const char *s1, const char *s2) {
	return strcmp(s1, s2);
}
