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
 * @brief		Circular doubly-linked list implementation.
 */

#ifndef __LIB_LIST_H
#define __LIB_LIST_H

#include <types.h>

/** Doubly linked list node structure. */
typedef struct list {
	struct list *prev;		/**< Pointer to previous entry. */
	struct list *next;		/**< Pointer to next entry. */
} list_t;

/** Iterate over a list.
 * @param list		Head of list to iterate.
 * @param iter		Variable name to set to node pointer on each iteration. */
#define LIST_FOREACH(list, iter)		\
	for(list_t *iter = (list)->next; iter != (list); iter = iter->next)

/** Iterate over a list in reverse.
 * @param list		Head of list to iterate.
 * @param iter		Variable name to set to node pointer on each iteration. */
#define LIST_FOREACH_REVERSE(list, iter)	\
	for(list_t *iter = (list)->prev; iter != (list); iter = iter->prev)

/** Iterate over a list safely.
 * @note		Safe to use when the loop may modify the list - caches
 *			the next pointer from the entry before the loop body.
 * @param list		Head of list to iterate.
 * @param iter		Variable name to set to node pointer on each iteration. */
#define LIST_FOREACH_SAFE(list, iter)		\
	for(list_t *iter = (list)->next, *_##iter = iter->next; \
	    iter != (list); iter = _##iter, _##iter = _##iter->next)

/** Iterate over a list in reverse.
 * @note		Safe to use when the loop may modify the list.
 * @param list		Head of list to iterate.
 * @param iter		Variable name to set to node pointer on each iteration. */
#define LIST_FOREACH_REVERSE_SAFE(list, iter)	\
	for(list_t *iter = (list)->prev, *_##iter = iter->prev; \
	    iter != (list); iter = _##iter, _##iter = _##iter->prev)

/** Initializes a statically declared linked list. */
#define LIST_INITIALIZER(_var)			\
	{ \
		.prev = &_var, \
		.next = &_var, \
	}

/** Statically declares a new linked list. */
#define LIST_DECLARE(_var)			\
	list_t _var = LIST_INITIALIZER(_var)

/** Get a pointer to the structure containing a list node.
 * @param entry		List node pointer.
 * @param type		Type of the structure.
 * @param member	Name of the list node member in the structure.
 * @return		Pointer to the structure. */
#define list_entry(entry, type, member)		\
	((type *)((char *)entry - offsetof(type, member)))

/** Get a pointer to the next structure in a list.
 * @note		Does not check if the next entry is the head.
 * @param entry		Current entry.
 * @param type		Type of the structure.
 * @param member	Name of the list node member in the structure.
 * @return		Pointer to the next structure. */
#define list_next(entry, type, member)		\
	((type *)((char *)((entry)->next) - offsetof(type, member)))

/** Get a pointer to the previous structure in a list.
 * @note		Does not check if the previous entry is the head.
 * @param entry		Current entry.
 * @param type		Type of the structure.
 * @param member	Name of the list node member in the structure.
 * @return		Pointer to the previous structure. */
#define list_prev(entry, type, member)		\
	((type *)((char *)((entry)->prev) - offsetof(type, member)))

/** Get a pointer to the first structure in a list.
 * @note		Does not check if the list is empty.
 * @param list		Head of the list.
 * @param type		Type of the structure.
 * @param member	Name of the list node member in the structure.
 * @return		Pointer to the first structure. */
#define list_first(list, type, member)		list_next(list, type, member)

/** Get a pointer to the last structure in a list.
 * @note		Does not check if the list is empty.
 * @param list		Head of the list.
 * @param type		Type of the structure.
 * @param member	Name of the list node member in the structure.
 * @return		Pointer to the last structure. */
#define list_last(list, type, member)		list_prev(list, type, member)

/** Checks whether the given list is empty.
 * @param list		List to check. */
#define list_empty(list)			\
	(((list)->prev == (list)) && ((list)->next) == (list))

/** Internal part of list_remove(). */
static inline void list_real_remove(list_t *entry) {
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
}

/** Initializes a linked list.
 * @param list		List to initialize. */
static inline void list_init(list_t *list) {
	list->prev = list->next = list;
}

/** Add an entry to a list before the given entry.
 * @param exist		Existing entry to add before.
 * @param entry		Entry to append. */
static inline void list_add_before(list_t *exist, list_t *entry) {
	list_real_remove(entry);

	exist->prev->next = entry;
	entry->next = exist;
	entry->prev = exist->prev;
	exist->prev = entry;
}

/** Add an entry to a list after the given entry.
 * @param exist		Existing entry to add after.
 * @param entry		Entry to append. */
static inline void list_add_after(list_t *exist, list_t *entry) {
	list_real_remove(entry);

	exist->next->prev = entry;
	entry->next = exist->next;
	entry->prev = exist;
	exist->next = entry;
}

/** Append an entry to a list.
 * @param list		List to append to.
 * @param entry		Entry to append. */
static inline void list_append(list_t *list, list_t *entry) {
	list_add_before(list, entry);
}

/** Prepend an entry to a list.
 * @param list		List to prepend to.
 * @param entry		Entry to prepend. */
static inline void list_prepend(list_t *list, list_t *entry) {
	list_add_after(list, entry);
}

/** Remove a list entry from its containing list.
 * @param entry		Entry to remove. */
static inline void list_remove(list_t *entry) {
	list_real_remove(entry);
	list_init(entry);
}

#endif /* __LIB_LIST_H */
