/*
 * Copyright (C) 2009-2013 Gil Mendes
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
 * @brief		Kernel logging functions.
 */

#include <arch/page.h>
#include <arch/cpu.h>

#include <lib/printf.h>

#include <mm/malloc.h>
#include <mm/phys.h>

#include <sync/spinlock.h>

#include <console.h>
#include <laos.h>
#include <kdb.h>
#include <kernel.h>
#include <status.h>

/** Cyclic kernel log buffer. */
static struct {
	unsigned char level;		/**< Log level. */
	unsigned char ch;		/**< Character. */
} klog_buffer[CONFIG_KLOG_SIZE] __aligned(PAGE_SIZE);

/** Start of the log buffer. */
static uint32_t klog_start = 0;

/** Number of characters in the buffer. */
static uint32_t klog_length = 0;

/** Lock protecting the kernel log. */
static SPINLOCK_DECLARE(klog_lock);

/** LAOS log buffer. */
laos_log_t *laos_log = NULL;
size_t laos_log_size = 0;

/** Helper for kvprintf().
 * @param ch		Character to display.
 * @param data		Pointer to log level.
 * @param total		Pointer to total character count. */
static void kvprintf_helper(char ch, void *data, int *total) {
	int level = *(int *)data;

	/* Store in the log buffer. */
	klog_buffer[(klog_start + klog_length) % CONFIG_KLOG_SIZE].level = level;
	klog_buffer[(klog_start + klog_length) % CONFIG_KLOG_SIZE].ch = (unsigned char)ch;
	if(klog_length < CONFIG_KLOG_SIZE) {
		klog_length++;
	} else {
		klog_start = (klog_start + 1) % CONFIG_KLOG_SIZE;
	}

	/* Write to the console. */
	if(debug_console.out)
		debug_console.out->putc(ch);
	if(level >= LOG_NOTICE && main_console.out)
		main_console.out->putc(ch);

	laos_log_write(ch);

	*total = *total + 1;
}

/** Print a formatted message to the kernel log.
 * @param level		Log level.
 * @param fmt		Format string for message.
 * @param args		Arguments to substitute into format string.
 * @return		Number of characters written. */
int kvprintf(int level, const char *fmt, va_list args) {
	int ret;

	#if !CONFIG_DEBUG
	/* When debug output is disabled, do not do anything. */
	if(level == LOG_DEBUG)
		return 0;
	#endif

	spinlock_lock(&klog_lock);
	ret = do_vprintf(kvprintf_helper, &level, fmt, args);
	spinlock_unlock(&klog_lock);

	return ret;
}

/** Print a formatted message to the kernel log.
 * @param level		Log level.
 * @param fmt		Format string for message.
 * @param ...		Arguments to substitute into format string.
 * @return		Number of characters written. */
int kprintf(int level, const char *fmt, ...) {
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = kvprintf(level, fmt, args);
	va_end(args);

	return ret;
}

/** Write to the LAOS log.
 * @param ch		Character to write. */
void laos_log_write(char ch) {
	if(laos_log) {
		laos_log->buffer[(laos_log->start + laos_log->length) % laos_log_size] = ch;
		if(laos_log->length < laos_log_size) {
			laos_log->length++;
		} else {
			laos_log->start = (laos_log->start + 1) % laos_log_size;
		}
	}
}

/** Flush the LAOS log. */
void laos_log_flush(void) {
	arch_cpu_invalidate_caches();
}

/** Print out the kernel log buffer.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_log(int argc, char **argv, kdb_filter_t *filter) {
	int level = -1;
	size_t i, pos;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s [/level]\n\n", argv[0]);

		kdb_printf("Prints out the contents of the kernel log buffer. If no level is specified\n");
		kdb_printf("the entire log will be printed, otherwise only characters with the specified\n");
		kdb_printf("level or higher will be printed.\n");
		kdb_printf("  Log levels:\n");
		kdb_printf("    d    Debug.\n");
		kdb_printf("    n    Normal.\n");
		kdb_printf("    w    Warning.\n");
		return KDB_SUCCESS;
	} else if(!(argc == 1 || (argc == 2 && argv[1][0] == '/'))) {
		kdb_printf("Invalid arguments. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	/* Look for a log level. */
	if(argc == 2) {
		argv[1]++;
		switch(*argv[1]) {
		case 'd': level = LOG_DEBUG; break;
		case 'n': level = LOG_NOTICE; break;
		case 'w': level = LOG_WARN; break;
		default:
			kdb_printf("Unknown level character '%c'\n", *argv[1]);
			return KDB_FAILURE;
		}
	}

	for(i = 0, pos = klog_start; i < klog_length; i++) {
		if(level == -1 || klog_buffer[pos].level >= level)
			kdb_printf("%c", klog_buffer[pos].ch);

		if(++pos >= CONFIG_KLOG_SIZE)
			pos = 0;
	}

	return KDB_SUCCESS;
}

/** Initialize the kernel log. */
__init_text void log_early_init(void) {
	laos_tag_log_t *tag = laos_tag_iterate(LAOS_TAG_LOG, NULL);
	if(tag) {
		laos_log = (laos_log_t *)((ptr_t)tag->log_virt);
		laos_log_size = tag->log_size - sizeof(laos_log_t);
	}

	/* Register the KDB command. */
	kdb_register_command("log", "Display the kernel log buffer.", kdb_cmd_log);
}

/** Create the kernel log device. */
static __init_text void log_init(void) {
	laos_tag_log_t *tag;

	/* The LAOS log mapping will go away so we need to remap it somewhere
	 * else. */
	if(laos_log) {
		tag = laos_tag_iterate(LAOS_TAG_LOG, NULL);
		laos_log = phys_map(tag->log_phys, tag->log_size, MM_BOOT);
	}
}

INITCALL(log_init);
