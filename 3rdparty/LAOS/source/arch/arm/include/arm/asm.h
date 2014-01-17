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
 * @brief		ARM assembly code definitions.
 */

#ifndef __ARM_ASM_H
#define __ARM_ASM_H

#ifndef __ASM__
# error "What are you doing?"
#endif

/** Macro to define the beginning of a global function. */
#define FUNCTION_START(name)		\
	.global name; \
	.type name, %function; \
	name:

/** Macro to define the beginning of a private function. */
#define PRIVATE_FUNCTION_START(name)	\
	.type name, %function; \
	name:

/** Macro to define the end of a function. */
#define FUNCTION_END(name)		\
	.size name, . - name

/** Macro to define a global symbol. */
#define SYMBOL(name)			\
	.global name; \
	name:

#endif /* __ARM_ASM_H */
