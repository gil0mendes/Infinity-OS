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
 * @brief		x86 CPU initialisation functions.
 */

#include <arch/io.h>

#include <x86/descriptor.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <loader.h>
#include <memory.h>
#include <time.h>

/** Structure defining an interrupt stack frame. */
typedef struct interrupt_frame {
	unsigned long gs;		/**< GS. */
	unsigned long fs;		/**< FS. */
	unsigned long es;		/**< ES. */
	unsigned long ds;		/**< DS. */
	unsigned long di;		/**< EDI. */
	unsigned long si;		/**< ESI. */
	unsigned long bp;		/**< EBP. */
	unsigned long ksp;		/**< ESP (kernel). */
	unsigned long bx;		/**< EBX. */
	unsigned long dx;		/**< EDX. */
	unsigned long cx;		/**< ECX. */
	unsigned long ax;		/**< EAX. */
	unsigned long int_no;		/**< Interrupt number. */
	unsigned long err_code;		/**< Error code (if applicable). */
	unsigned long ip;		/**< IP. */
	unsigned long cs;		/**< CS. */
	unsigned long flags;		/**< FLAGS. */
	unsigned long sp;		/**< SP. */
	unsigned long ss;		/**< SS. */
} __packed interrupt_frame_t;

/** Frequency of the PIT. */
#define PIT_FREQUENCY		1193182L

/** Number of IDT entries. */
#define IDT_ENTRY_COUNT		32

extern void interrupt_handler(interrupt_frame_t *frame);
extern uint8_t isr_array[IDT_ENTRY_COUNT][16];

/** Frequency of the booting CPU, used by spin(). */
static uint64_t cpu_frequency = 0;

/** Interrupt descriptor table. */
static idt_entry_t loader_idt[IDT_ENTRY_COUNT];

/** IDT pointer pointing to the loader IDT. */
idt_pointer_t loader_idtp = {
	.limit = sizeof(loader_idt) - 1,
	.base = (ptr_t)&loader_idt,
};

/** Read the Time Stamp Counter.
 * @return		Value of the TSC. */
static inline uint64_t rdtsc(void) {
	uint32_t high, low;
	__asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
	return ((uint64_t)high << 32) | low;
}

/** Function to calculate the CPU frequency.
 * @return		Calculated frequency. */
static uint64_t calculate_cpu_frequency(void) {
	uint16_t shi, slo, ehi, elo, ticks;
	uint64_t start, end, cycles;

	/* First set the PIT to rate generator mode. */
	out8(0x43, 0x34);
	out8(0x40, 0xFF);
	out8(0x40, 0xFF);

	/* Wait for the cycle to begin. */
	do {
		out8(0x43, 0x00);
		slo = in8(0x40);
		shi = in8(0x40);
	} while(shi != 0xFF);

	/* Get the start TSC value. */
	start = rdtsc();

	/* Wait for the high byte to drop to 128. */
	do {
		out8(0x43, 0x00);
		elo = in8(0x40);
		ehi = in8(0x40);
	} while(ehi > 0x80);

	/* Get the end TSC value. */
	end = rdtsc();

	/* Calculate the differences between the values. */
	cycles = end - start;
	ticks = ((ehi << 8) | elo) - ((shi << 8) | slo);

	/* Calculate frequency. */
	return (cycles * PIT_FREQUENCY) / ticks;
}

/** Spin for a certain amount of time.
 * @note		There is a BIOS interrupt to wait for a certain time
 *			period (INT15/AH=86h), however we don't use this so
 *			that this function is usable where it is unsuitable to
 *			perform a BIOS call.
 * @param us		Microseconds to delay for. */
void spin(timeout_t us) {
	/* Work out when we will finish */
	uint64_t target = rdtsc() + ((cpu_frequency / 1000000) * us);

	/* Spin until the target is reached. */
	while(rdtsc() < target)
		__asm__ volatile("pause");
}

/** Handle an exception.
 * @param frame		Interrupt frame. */
void interrupt_handler(interrupt_frame_t *frame) {
	internal_error("Exception %u (error code %u)\n"
		"cs: 0x%04lx  ds: 0x%04lx  es: 0x%04lx  fs: 0x%04lx  gs: 0x%04lx\n"
		"eflags: 0x%08lx  esp: 0x%08lx\n"
		"eax: 0x%08lx  ebx: 0x%08lx  ecx: 0x%08lx  edx: 0x%08lx\n"
		"edi: 0x%08lx  esi: 0x%08lx  ebp: 0x%08lx  eip: 0x%08lx",
		frame->int_no, frame->err_code, frame->cs, frame->ds, frame->es,
		frame->fs, frame->gs, frame->flags, frame->ksp, frame->ax,
		frame->bx, frame->cx, frame->dx, frame->di, frame->si, frame->bp,
		frame->ip);
	while(1);
}

/** Perform initialisation of the CPU. */
void cpu_init(void) {
	ptr_t addr;
	size_t i;

	/* Fill out the handlers in the IDT. */
	for(i = 0; i < IDT_ENTRY_COUNT; i++) {
		addr = (ptr_t)&isr_array[i];
		loader_idt[i].base0 = (addr & 0xFFFF);
		loader_idt[i].base1 = ((addr >> 16) & 0xFFFF);
		loader_idt[i].sel = SEGMENT_CS;
		loader_idt[i].unused = 0;
		loader_idt[i].flags = 0x8E;
	}

	/* Load the new IDT. */
	__asm__ volatile("lidt %0" :: "m"(loader_idtp));

	/* Calculate the CPU frequency for use by spin(). */
	cpu_frequency = calculate_cpu_frequency();
}
