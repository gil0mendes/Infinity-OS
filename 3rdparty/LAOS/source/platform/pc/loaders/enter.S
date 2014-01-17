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
 * @brief		PC kernel entry functions.
 */

#include <platform/loader.h>

#include <x86/asm.h>

#define CHAINLOAD_STACK		0x7C00
#define CHAINLOAD_ADDR		0x7C00

.section .text, "ax", @progbits

/** Drop to real mode and jump to a new boot sector.
 * @param id		ID of boot disk.
 * @param part		Address of partition table entry. */
FUNCTION_START(chain_loader_enter)
	/* Save arguments. */
	movl	4(%esp), %edx
	movl	8(%esp), %esi

	cli

	/* Switch to the real-mode stack. */
	movl	$CHAINLOAD_STACK, %esp

	/* Switch to the BIOS IVT. */
	lidt	(bios_idtp)

	/* Set data segments. */
	movw	$SEGMENT_DS16, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss

	/* Jump into the real mode code segment. */
	ljmp	$SEGMENT_CS16, $1f - LOADER_LOAD_ADDR

	/* Clear the PE (Protection Enable) bit of CR0. */
1:	movl	%cr0, %eax
	andl	$~(1<<0), %eax
	movl	%eax, %cr0

	/* Set correct segments. */
	.byte 0x66
	ljmp	$(LOADER_LOAD_ADDR >> 4), $2f - LOADER_LOAD_ADDR
.code16
2:	xorw	%ax, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movw	%ax, %ss

	sti

	push	%edx
	push	%esi

	/* Restore the console to a decent state. Set display page to the first
	 * and move the cursor to (0, 0). */
	movw	$0x0500, %ax
	int	$0x10
	movw	$0x0200, %ax
	xorw	%bx, %bx
	xorw	%dx, %dx
	int	$0x10

	/* Try to disable the A20 line. */
	call	disable_a20

	/* Jump to the loader. */
	xorl	%eax, %eax
	xorl	%ebx, %ebx
	xorl	%ecx, %ecx
	xorl	%edi, %edi
	xorl	%ebp, %ebp
	pop	%esi
	pop	%edx
	ljmp	$0, $CHAINLOAD_ADDR
FUNCTION_END(chain_loader_enter)

/** Test whether the A20 line is enabled.
 * @note		Partially based on code found at
 *			http://wiki.osdev.org/A20_Line
 * @return		AX = 1 if enabled, AX = 0 if not. */
PRIVATE_FUNCTION_START(test_a20)
.code16
	/* Iterate checking to give time for disabling attempts we make to take
	 * effect. */
	push	%fs
	push	%gs
	push	%cx
	mov	$0x1000, %cx

	/* Set segment registers and addresses to compare. */
1:	xor	%ax, %ax
	mov	%ax, %fs
	not	%ax
	mov	%ax, %gs
	mov	$0x1000, %di
	mov	$0x1010, %si

	/* Write different values to the two addresses, and compare. If they
	 * both equal the second value written, the A20 line is not enabled. */
	movb	$0x00, %fs:(%di)
	outb	%al, $0x80
	movb	$0xFF, %gs:(%si)
	outb	%al, $0x80
	cmpb	$0xFF, %fs:(%di)
	xorw	%ax, %ax
	je	2f
	loop	1b
	inc	%ax
2:	pop	%cx
	pop	%gs
	pop	%fs
	ret
FUNCTION_END(test_a20)

/** Disable the A20 line. */
PRIVATE_FUNCTION_START(disable_a20)
	mov	$0x100, %cx
.Lbegin:
	/* First try using a BIOS interrupt. */
	mov	$0x2400, %ax
	int	$0x15
	call	test_a20
	test	%ax, %ax
	jnz	.Ldisable_fast
	ret
.Ldisable_fast:
	/* Try the fast A20 mechanism. */
	inb	$0x92, %al
	andb	$~((1<<0) | (1<<1)), %al
	outb	%al, $0x92
	call	test_a20
	test	%ax, %ax
	jnz	.Lfail
	ret
.Lfail:
	loop	.Lbegin

	/* Don't error or anything on failure, on some machines A20 is always
	 * enabled. */
	ret
FUNCTION_END(disable_a20)
