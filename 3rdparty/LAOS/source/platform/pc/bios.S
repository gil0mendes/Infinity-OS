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
 * @brief		BIOS interrupt functions.
 */

#include <platform/loader.h>

#include <x86/asm.h>

/** Size of the registers structure. */
#define REGS_SIZE	0x24

/** Stack to use in real mode. */
#define REAL_MODE_STACK	0xFFFC
#define REAL_MODE_SP	(REAL_MODE_STACK - REGS_SIZE)

.section .text, "ax", @progbits

/** Macro to switch to real mode.
 * @note		Trashes EAX. */
#define PROT_TO_REAL	\
	/* Save the stack pointer at the top of the real mode stack and switch \
	 * to it. */ \
	pushfl; \
	cli; \
	movl	%esp, (REAL_MODE_STACK); \
	movl	$REAL_MODE_SP, %esp; \
	\
	/* Switch to the BIOS IVT. */ \
	lidt	(bios_idtp); \
	\
	/* Set data segments. */ \
	movw	$SEGMENT_DS16, %ax; \
	movw	%ax, %ds; \
	movw	%ax, %es; \
	movw	%ax, %fs; \
	movw	%ax, %gs; \
	movw	%ax, %ss; \
	\
	/* Jump into the real mode code segment. */ \
	ljmp	$SEGMENT_CS16, $1f - LOADER_LOAD_ADDR; \
	\
	/* Clear the PE (Protection Enable) bit of CR0. */ \
1:	movl	%cr0, %eax; \
	andl	$~(1<<0), %eax; \
	movl	%eax, %cr0; \
	\
	/* Set correct segments. */ \
	.byte 0x66; \
	ljmp	$(LOADER_LOAD_ADDR >> 4), $2f - LOADER_LOAD_ADDR; \
.code16 \
2:	xorw	%ax, %ax; \
	mov	%ax, %ds; \
	mov	%ax, %es; \
	mov	%ax, %fs; \
	mov	%ax, %gs; \
	mov	%ax, %ss

/** Macro to switch to protected mode.
 * @note		Trashes EAX. */
#define REAL_TO_PROT	\
	/* Ensure interrupts are disabled. */ \
	cli; \
	\
	/* Return to protected mode. */ \
	movl	%cr0, %eax; \
	orl	$(1<<0), %eax; \
	movl	%eax, %cr0; \
	data32 ljmp $SEGMENT_CS, $3f; \
.code32 \
3: \
	mov	$SEGMENT_DS, %ax; \
	mov	%ax, %ds; \
	mov	%ax, %es; \
	mov	%ax, %fs; \
	mov	%ax, %gs; \
	mov	%ax, %ss; \
	\
	/* Switch back to the loader IDT. */ \
	lidt	(loader_idtp); \
	\
	/* Restore stack pointer. */ \
	mov	(REAL_MODE_STACK), %esp; \
	popfl

/** Call a BIOS interrupt.
 * @param num		Interrupt number.
 * @param regs		Registers to use for the interrupt. */
FUNCTION_START(bios_interrupt)
	/* Save callee-save registers. */
	push	%ebp
	push	%ebx
	push	%edi
	push	%esi

	/* Write the interrupt number. */
	movl	20(%esp), %eax
	movb	%al, .Lint

	/* Copy the registers structure to the real mode stack. */
	movl	24(%esp), %esi
	movl	$REAL_MODE_SP, %edi
	movl	$(REGS_SIZE >> 2), %ecx
	rep movsl

	/* Switch to real mode. */
	PROT_TO_REAL

	/* Get the registers to use. */
	pop	%eax
	movl	28(%esp), %eax
	mov	%ax, %es
	movl	0(%esp), %eax
	movl	4(%esp), %ebx
	movl	8(%esp), %ecx
	movl	12(%esp), %edx
	movl	16(%esp), %edi
	movl	20(%esp), %esi
	movl	24(%esp), %ebp

	/* Enable interrupts across the interrupt. */
	sti

	/* Hard-code in an INT instruction that gets modified above to contain
	 * the correct interrupt number. */
	.byte 0xCD
.Lint:	.byte 0x0

	/* Save the new register/flags state. */
	mov	%eax, 0(%esp)
	mov	%ebx, 4(%esp)
	mov	%ecx, 8(%esp)
	mov	%edx, 12(%esp)
	mov	%edi, 16(%esp)
	mov	%esi, 20(%esp)
	mov	%ebp, 24(%esp)
	mov	%es, %ax
	movl	%eax, 28(%esp)
	pushfl

	/* Switch back to protected mode. */
	REAL_TO_PROT

	/* Copy the registers structure back from the real mode stack. */
	movl	$REAL_MODE_SP, %esi
	movl	24(%esp), %edi
	movl	$(REGS_SIZE >> 2), %ecx
	rep movsl

	/* Pop callee-save registers and return. */
	pop	%esi
	pop	%edi
	pop	%ebx
	pop	%ebp
	ret
FUNCTION_END(bios_interrupt)

/** Call a PXE function.
 * @param func		Function to call.
 * @param segoff	Linear address of data argument.
 * @return		Return code from call. */
FUNCTION_START(pxe_call_real)
	movl	4(%esp), %ecx
	movl	8(%esp), %edx
	pushl	%ebx
	movl	pxe_entry_point, %ebx

	/* Switch to real mode. */
	PROT_TO_REAL

	/* Enable interrupts across the call. */
	sti

	/* Call the entry point. */
	pushl	%ebx
	pushl	%edx
	pushw	%cx
	movw	%sp, %bx
	lcall	*%ss:6(%bx)
	addw	$10, %sp
	movzwl	%ax, %ecx

	/* Switch back to protected mode. */
	REAL_TO_PROT

	/* Save return code and return. */
	movl	%ecx, %eax
	popl	%ebx
	ret
FUNCTION_END(pxe_call_real)

/** IDT pointer for the BIOS IVT. */
SYMBOL(bios_idtp)
        .word 0x7FF
        .long 0
