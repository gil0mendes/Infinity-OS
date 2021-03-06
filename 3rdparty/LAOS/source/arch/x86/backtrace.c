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
 * @brief		x86 backtrace function.
 */

#include <loader.h>

/** Structure containing a stack frame. */
typedef struct stack_frame {
	struct stack_frame *next;	/**< Pointer to next stack frame. */
	ptr_t addr;			/**< Function return address. */
} stack_frame_t;

/** Print out a backtrace.
 * @param printfn	Print function to use. */
void backtrace(int (*printfn)(const char *fmt, ...)) {
	stack_frame_t *frame;
	ptr_t addr = 0;

	__asm__ volatile("mov %%ebp, %0" : "=r"(addr));
	frame = (stack_frame_t *)addr;

	while(frame) {
		printfn(" %p\n", frame->addr);
		frame = frame->next;
	}
}
