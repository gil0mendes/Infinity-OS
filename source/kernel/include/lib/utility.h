/*
 * Copyright (C) 2007-2012 Gil Mendes
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
 * @brief		Utility functions/macros.
 */

#ifndef __LIB_UTILITY_H
#define __LIB_UTILITY_H

#include <arch/bitops.h>

#include <types.h>

/** Round a value up.
 * @param val		Value to round.
 * @param nearest	Boundary to round up to.
 * @return		Rounded value. */
#define round_up(val, nearest)		\
	__extension__ \
	({ \
		typeof(val) __n = val; \
		if(__n % (nearest)) { \
			__n -= __n % (nearest); \
			__n += nearest; \
		} \
		__n; \
	})

/** Round a value down.
 * @param val		Value to round.
 * @param nearest	Boundary to round up to.
 * @return		Rounded value. */
#define round_down(val, nearest)	\
	__extension__ \
	({ \
		typeof(val) __n = val; \
		if(__n % (nearest)) { \
			__n -= __n % (nearest); \
		} \
		__n; \
	})

/** Check if a value is a power of 2.
 * @param val		Value to check.
 * @return		Whether value is a power of 2. */
#define is_pow2(val)		((val) && ((val) & ((val) - 1)) == 0)

/** Get the number of bits in a type. */
#define BITS(t)			(sizeof(t) * 8)

/** Get the number of elements in an array. */
#define ARRAY_SIZE(a)		(sizeof((a)) / sizeof((a)[0]))

/** Get the lowest value out of a pair of values. */
#define min(a, b)		((a) < (b) ? (a) : (b))

/** Get the highest value out of a pair of values. */
#define max(a, b)		((a) < (b) ? (b) : (a))

/** Swap two values. */
#define swap(a, b)	\
	{ \
		typeof(a) __tmp = a; \
		a = b; \
		b = __tmp; \
	}

/** Implementation for native-sized values. */
static inline int highbit_native(unsigned long val) {
	if(!val)
		return 0;

	return fls(val) + 1;
}

#if CONFIG_32BIT
/** Implementation for long long values on 32-bit systems. */
static inline int highbit_ll(unsigned long long val) {
	unsigned long high, low;

	if(!val)
		return 0;

	high = (unsigned long)((val >> 32) & 0xffffffff);
	low = (unsigned long)(val & 0xffffffff);
	if(high) {
		return fls(high) + 32 + 1;
	} else {
		return fls(low) + 1;
	}
}
#endif // CONFIG_32BIT

/** Get log base 2 (high bit) of a value.
 * @param val		Value to get high bit from.
 * @return		High bit + 1. */
#if CONFIG_32BIT
# define highbit(val)	_Generic((val), \
	unsigned long long: highbit_ll, \
	long long: highbit_ll, \
	default: highbit_native)(val)
#else
# define highbit(val)	highbit_native(val)
#endif

/** Checksum a memory range.
 * @param start		Start of range to check.
 * @param size		Size of range to check.
 * @return		True if checksum is equal to 0, false if not. */
static inline bool checksum_range(void *start, size_t size) {
	uint8_t *range = (uint8_t *)start;
	uint8_t checksum = 0;
	size_t i;

	for(i = 0; i < size; i++) {
		checksum += range[i];
	}
	return (checksum == 0);
}

extern void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

#endif /* __LIB_UTILITY_H */
