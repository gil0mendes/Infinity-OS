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
 * @brief		x86 descriptor table definitions.
 */

#ifndef __X86_DESCRIPTOR_H
#define __X86_DESCRIPTOR_H

#include <types.h>

/** GDT pointer loaded into the GDTR register. */
typedef struct gdt_pointer {
	uint16_t limit;			/**< Total size of GDT. */
	ptr_t base;			/**< Virtual address of GDT. */
} __packed gdt_pointer_t;

/** IDT pointer loaded into the IDTR register. */
typedef struct idt_pointer {
	uint16_t limit;			/**< Total size of IDT. */
	ptr_t base;			/**< Virtual address of IDT. */
} __packed idt_pointer_t;

/** Structure of a GDT descriptor. */
typedef struct gdt_entry {
	unsigned limit0 : 16;		/**< Low part of limit. */
	unsigned base0 : 16;		/**< Low part of base. */
	unsigned base1 : 8;		/**< Middle part of base. */
	unsigned access : 8;		/**< Access flags. */
	unsigned limit1 : 4;		/**< High part of limit. */
	unsigned available : 1;		/**< Spare bit. */
	unsigned unused : 1;		/**< Unused. */
	unsigned special : 1;		/**< Special. */
	unsigned granularity : 1;	/**< Granularity. */
	unsigned base2 : 8;		/**< High part of base. */
} __packed gdt_entry_t;

/** Structure of an IDT entry. */
typedef struct idt_entry {
	unsigned base0 : 16;		/**< Low part of handler address. */
	unsigned sel : 16;		/**< Code segment selector. */
	unsigned unused : 8;		/**< Unused - always zero. */
	unsigned flags : 8;		/**< Flags. */
	unsigned base1 : 16;		/**< High part of handler address. */
} __packed idt_entry_t;

/** Set the GDTR register.
 * @param base		Virtual address of GDT.
 * @param limit		Size of GDT. */
static inline void lgdt(ptr_t base, uint16_t limit) {
	gdt_pointer_t gdtp = { limit, base };
	__asm__ volatile("lgdt %0" :: "m"(gdtp));
}

/** Set the IDTR register.
 * @param base		Base address of IDT.
 * @param limit		Size of IDT. */
static inline void lidt(ptr_t base, uint16_t limit) {
	idt_pointer_t idtp = { limit, base };
	__asm__ volatile("lidt %0" :: "m"(idtp));
}

#endif /* __X86_DESCRIPTOR_H */
