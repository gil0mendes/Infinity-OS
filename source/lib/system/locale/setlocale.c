/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Set locale function.
 */

#include <locale.h>
#include <string.h>

/**
 * Set the current locale.
 *
 * Sets the current locale for the given category to the locale corresponding
 * to the given string.
 *
 * @param category	Category to set locale for.
 * @param name		Name of locale to set.
 *
 * @return		Name of new locale.
 */
char *setlocale(int category, const char *name) {
	if(name != NULL) {
		if(strcmp(name, "C") && strcmp(name, "POSIX") && strcmp(name, ""))
			return NULL;
	}

	return (char *)"C";
}