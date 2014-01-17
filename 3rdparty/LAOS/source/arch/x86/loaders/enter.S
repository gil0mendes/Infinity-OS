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
 * @brief		x86 kernel entry functions.
 */

#include <arch/page.h>

#include <x86/asm.h>
#include <x86/cpu.h>

#include <platform/loader.h>

#include <laos.h>

.section .text, "ax", @progbits

#if CONFIG_LAOS_LOADER_LAOS

#define ENTRY_ARGS_TRANSITION_CR3	0
#define ENTRY_ARGS_VIRT			8
#define ENTRY_ARGS_KERNEL_CR3		16
#define ENTRY_ARGS_SP			24
#define ENTRY_ARGS_ENTRY		32
#define ENTRY_ARGS_TAGS			40
#define ENTRY_ARGS_TRAMPOLINE		48

/** Enter a 64-bit LAOS kernel.
 * @param args		Entry arguments structure. */
FUNCTION_START(laos_arch_enter64)
.code32
	/* Store arguments address in EDI. */
	movl	4(%esp), %edi

	/* Enable PAE. */
	movl	%cr4, %eax
	orl	$X86_CR4_PAE, %eax
	movl	%eax, %cr4

	/* Point CR3 to the transition PML4. */
	movl	ENTRY_ARGS_TRANSITION_CR3(%edi), %eax
	movl	%eax, %cr3

	/* Enable long mode by setting EFER.LME. */
	movl	$X86_MSR_EFER, %ecx
	rdmsr
	orl	$X86_EFER_LME, %eax
	wrmsr

	/* Set PG (Paging Enable) to put us in compatibility mode. */
	movl	%cr0, %ecx
	orl	$X86_CR0_PG, %ecx
	movl	%ecx, %cr0

	/* Jump into the 64-bit code segment. */
	ljmp	$SEGMENT_CS64, $.Llmode
.align 8
.code64
.Llmode:
	/* Set data segments. */
	xorl	%eax, %eax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss

	/* Clear high 32 bits of arguments address. */
	mov	%edi, %edi

	/* Get the trampoline location and jump to it. */
	movq	ENTRY_ARGS_VIRT(%rdi), %rdi
	leaq	ENTRY_ARGS_TRAMPOLINE(%rdi), %rax
	jmp	*%rax

SYMBOL(laos_trampoline64)
	/* Switch to the real kernel page directory. */
	movq	ENTRY_ARGS_KERNEL_CR3(%rdi), %rax
	movq	%rax, %cr3

	/* Set the stack pointer. */
	movq	ENTRY_ARGS_SP(%rdi), %rsp

	/* Clear the stack frame/RFLAGS. */
	xorq	%rbp, %rbp
	push	$0
	popf

	/* Retrieve tag list address and entry point. */
	movq	ENTRY_ARGS_TAGS(%rdi), %rsi
	movq	ENTRY_ARGS_ENTRY(%rdi), %rax

	/* Call the kernel. */
	movq	$LAOS_MAGIC, %rdi
	call	*%rax
1:	jmp	1b

SYMBOL(laos_trampoline64_size)
	.long	. - laos_trampoline64
FUNCTION_END(laos_arch_enter64)

/** Enter a 32-bit LAOS kernel.
 * @param args		Entry arguments structure. */
FUNCTION_START(laos_arch_enter32)
.code32
	/* Store arguments address in EDI. */
	movl	4(%esp), %edi

	/* Enable PSE, the MMU code may have created mappings with large pages. */
	movl	%cr4, %eax
	orl	$X86_CR4_PSE, %eax
	movl	%eax, %cr4

	/* Point CR3 to the transition page directory. */
	movl	ENTRY_ARGS_TRANSITION_CR3(%edi), %eax
	movl	%eax, %cr3

	/* Set PG (Paging Enable). */
	movl	%cr0, %ecx
	orl	$X86_CR0_PG, %ecx
	movl	%ecx, %cr0

	/* Get the trampoline location and jump to it. */
	movl	ENTRY_ARGS_VIRT(%edi), %edi
	leal	ENTRY_ARGS_TRAMPOLINE(%edi), %eax
	jmp	*%eax

SYMBOL(laos_trampoline32)
	/* Switch to the real kernel page directory. */
	movl	ENTRY_ARGS_KERNEL_CR3(%edi), %eax
	movl	%eax, %cr3

	/* Set the stack pointer. */
	movl	ENTRY_ARGS_SP(%edi), %esp

	/* Clear the stack frame/EFLAGS. */
	xorl	%ebp, %ebp
	push	$0
	popf

	/* Retrieve tag list address and entry point. */
	movl	ENTRY_ARGS_TAGS(%edi), %edx
	movl	ENTRY_ARGS_ENTRY(%edi), %eax

	/* Call the kernel. */
	push	%edx
	push	$LAOS_MAGIC
	call	*%eax
1:	jmp	1b

SYMBOL(laos_trampoline32_size)
	.long	. - laos_trampoline32
FUNCTION_END(laos_arch_enter32)

#endif /* CONFIG_LAOS_LOADER_LAOS */

#if CONFIG_LAOS_LOADER_LINUX

/** Enter a Linux kernel.
 * @param entry		Address of kernel entry point.
 * @param params	Address of kernel parameters page.
 * @param sp		Stack pointer to use. */
FUNCTION_START(linux_arch_enter)
.code32
	cli

	/* Load the GDT and new segments. */
	lgdt	(linux_gdtp)
	ljmp	$0x10, $2f
2:
	mov	$0x18, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss

	/* Get the arguments. */
	movl	4(%esp), %eax
	movl	8(%esp), %esi
	movl	12(%esp), %esp

	/* Clear out EFLAGS and other registers required to be 0. */
	xorl	%ebp, %ebp
	xorl	%edi, %edi
	xorl	%ebx, %ebx
	push	$0
	popf

	/* Jump to the kernel entry point. */
	call	*%eax
1:	jmp	1b
FUNCTION_END(linux_arch_enter)

#endif /* CONFIG_LAOS_LOADER_LINUX */

.section .data, "aw", @progbits

#if CONFIG_LAOS_LOADER_LINUX

/** GDT pointer. */
linux_gdtp:
        .word .L__gdt_end-__linux_gdt-1
        .long __linux_gdt

/** Global descriptor table. */
__linux_gdt:
	.quad 0x0000000000000000                /**< NULL descriptor (0x00). */
	.quad 0x0000000000000000                /**< NULL descriptor (0x08). */
        .quad 0x00CF9A000000FFFF                /**< 32-bit code     (0x10). */
        .quad 0x00CF92000000FFFF                /**< 32-bit data     (0x18). */
.L__gdt_end:

#endif /* CONFIG_LAOS_LOADER_LINUX */
