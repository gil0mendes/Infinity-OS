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
 * @brief		Core definitions.
 */

#ifndef __LOADER_H
#define __LOADER_H

#include <arch/loader.h>

#include <platform/loader.h>

#include <types.h>

struct ui_window;

/** Structure defining an OS loader type. */
typedef struct loader_type {
	/** Load the operating system.
	 * @note		Should not return. */
	void (*load)(void) __noreturn;

	#if CONFIG_LAOS_UI
	/** Return a window for configuring the OS.
	 * @return		Pointer to configuration window. */
	struct ui_window *(*configure)(void);
	#endif
} loader_type_t;

/** Builtin object definition structure. */
typedef struct builtin {
	/** Type of the builtin. */
	enum {
		BUILTIN_TYPE_COMMAND,
		BUILTIN_TYPE_FS,
		BUILTIN_TYPE_PARTITION_MAP,
	} type;

	/** Pointer to object. */
	void *object;
} builtin_t;

/** Type of a hook function to call before booting an OS. */
typedef void (*preboot_hook_t)(void);

extern char __start[], __end[];
extern builtin_t __builtins_start[], __builtins_end[];

/** Define a builtin object. */
#define DEFINE_BUILTIN(type, object) \
	static builtin_t __builtin_##name __section(".builtins") __used = { \
		type, \
		&object \
	}

/** Iterate over builtin objects. */
#define BUILTIN_ITERATE(btype, otype, vname) \
	int __iter_##vname = 0; \
	for(otype *vname = (otype *)__builtins_start[0].object; \
			__iter_##vname < (__builtins_end - __builtins_start); \
			vname = (otype *)__builtins_start[++__iter_##vname].object) \
		if(__builtins_start[__iter_##vname].type == btype)

extern int kvprintf(const char *fmt, va_list args);
extern int kprintf(const char *fmt, ...) __printf(1, 2);
extern int dvprintf(const char *fmt, va_list args);
extern int dprintf(const char *fmt, ...) __printf(1, 2);

extern void backtrace(int (*printfn)(const char *fmt, ...));

extern void internal_error(const char *fmt, ...) __printf(1, 2) __noreturn;
extern void boot_error(const char *fmt, ...) __printf(1, 2) __noreturn;

extern void loader_register_preboot_hook(preboot_hook_t hook);
extern void loader_preboot(void);

extern void loader_main(void) __noreturn;

#endif /* __LOADER_H */
