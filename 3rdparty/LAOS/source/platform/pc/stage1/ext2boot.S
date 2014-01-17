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
 * @brief		Ext2/3/4 boot sector.
 *
 * This code will load the loader binary from an Ext2/3/4 filesystem. It is
 * intended to be written to the first 1024 bytes of a partition and used as
 * the partition's boot sector.
 *
 * Since the MBR only loads 512 bytes for us, we are split into 2 sections, and
 * we load the second half early on in the main function (along with the FS
 * superblock).
 *
 * The start LBA (32-bit) of the boot partition should be written to offset 502.
 * The inode number of the loader itself should be written to offset 506.
 *
 * Current limitations:
 *  - Only uses INT13 Extensions. This is also the case with the loader itself,
 *    if anyone feels like adding support for the legacy interface, go ahead.
 *  - Uses 32-bit LBAs, meaning we only support disks up to 2TB.
 *  - Assumes EXT2_DYNAMIC_REV. The loader itself requires this at the moment,
 *    if that changes we should add support for it here too.
 *  - Only supports up to the indirect block. I doubt any greater will be
 *    needed here, unless we have to deal with very large directories or
 *    something.
 */

#include <platform/loader.h>

#include <x86/asm.h>

#include "../../../fs/ext2.h"

/** Address of the stack. */
#define STACK_OFFSET		0x6000

/** Segment that inode data is loaded to. */
#define LOAD_SEGMENT		(LOADER_LOAD_ADDR >> 4)

/** Print a message. */
#define PRINT(s)		\
	movw	$s, %si; \
	call	puts

/** Print an error message and halt. */
#define ERROR(s)		\
	movw	$s, %si; \
	jmp 	error

#define pushad			data32 pusha
#define popad			data32 popa

.section .text
.code16

/** Main function. */
FUNCTION_START(_start)
	/* Set up the stack and other segment registers. */
	xorw	%ax, %ax
	movw	%ax, %ss
	movw	$STACK_OFFSET, %sp
	movw	%ax, %ds
	movw	%ax, %es

	PRINT(msg_loading)

	/* Save drive ID. */
	mov	%dl, bios_drive_id

	/* Read in our second half and the superblock (3 sectors total). */
	movb	$3, %cl
	xorl	%eax, %eax
	inc	%eax
	movw	$second_half, %di
	call	read_disk_sectors

	/* Check whether this is a valid Ext2 filesystem. */
	cmpw	$EXT2_MAGIC, (superblock + EXT2_SUPERBLOCK_OFF_MAGIC)
	jne	.Lbad_fs

	/* Compute and save the block size. Although s_log_block_size is a
	 * 32-bit field in the superblock, in reality it'll never even be
	 * greater than 10, so just treat it as a byte. */
	movl	$1024, %eax
	movb	(superblock + EXT2_SUPERBLOCK_OFF_LOG_BLOCK_SIZE), %cl
	shl	%cl, %eax
	xorl	%edx, %edx
	movl	%eax, block_size
	movl	$512, %ecx
	div	%ecx
	movl	%eax, sectors_per_block

	/* Compute and save the group table start LBA. */
	movl	(superblock + EXT2_SUPERBLOCK_OFF_FIRST_DATA_BLOCK), %ecx
	inc	%ecx
	xchg	%eax, %ecx
	mull	%ecx
	movl	%eax, group_table_offset

	/* Read in our inode. */
	movl	inode_number, %eax
	call	read_inode

	/* Read in the inode data. */
	call	read_inode_data

	PRINT(msg_ok)

	/* Execute the bootloader. */
	mov	bios_drive_id, %dl
	movl	partition_lba, %ebx
	xorl	%ecx, %ecx
	ljmp	$(LOADER_LOAD_ADDR >> 4), $0
.Lbad_fs:
	ERROR(msg_not_ext2)
FUNCTION_END(_start)

/** Read a single sector from the boot partition.
 * @param %eax		Start offset.
 * @param %es:%di	Destination buffer. */
PRIVATE_FUNCTION_START(read_disk_sector)
	/* Set count and fall through. */
	movb	$1, %cl
FUNCTION_END(read_disk_sector)

/** Read sectors from the boot partition.
 * @param %cl		Sector count.
 * @param %eax		Start offset.
 * @param %es:%di	Destination buffer. */
PRIVATE_FUNCTION_START(read_disk_sectors)
	pushad

	/* Get the real offset. */
	add	partition_lba, %eax

	/* Initialise DAP. */
	movb	$0x10, dap_size
	movb	$0, dap_reserved1
	xorb	%ch, %ch
	movw	%cx, dap_count
	movw	%di, dap_buffer_offset
	movw	%es, dap_buffer_segment
	movl	%eax, dap_lba_low
	movl	$0, dap_lba_high

	/* Perform the read. */
	movb	bios_drive_id, %dl
	movw	$dap_size, %si
	movb	$0x42, %ah
	int	$0x13
	jc	1f
	popad
	ret
1:	ERROR(msg_read_failed)
FUNCTION_END(read_disk_sectors)

/** Read an inode into the inode buffer.
 * @param %eax		Inode to read. */
PRIVATE_FUNCTION_START(read_inode)
	pushad

	/* Compute the group number in EAX, and inode table offset in EDX. */
	dec	%eax
	xorl	%edx, %edx
	divl	(superblock + EXT2_SUPERBLOCK_OFF_INODES_PER_GROUP)
	push	%eax
	movl	%edx, %eax
	movzxw	(superblock + EXT2_SUPERBLOCK_OFF_INODE_SIZE), %ecx
	mul	%ecx
	pop	%edx
	xchg	%eax, %edx
	push	%edx

	/* Need to read in the group table entry. Work out which sector of the
	 * table it is in (EAX), and the offset into that entry (EDX), and read
	 * it in. */
	movl	$EXT2_GROUP_DESC_SIZE, %ecx
	mul	%ecx
	xorl	%edx, %edx
	movl	$512, %ecx
	div	%ecx
	addl	group_table_offset, %eax
	movw	$temp_buffer, %di
	call	read_disk_sector

	/* Retrieve the inode table block, then work out the sector that the
	 * inode is in (EAX) and the offset into that sector (EDX), and read
	 * it in. */
	movl	(temp_buffer + EXT2_GROUP_DESC_OFF_INODE_TABLE)(%edx), %eax
	mull	sectors_per_block
	pop	%edx
	xchg	%eax, %edx
	push	%edx
	xorl	%edx, %edx
	movl	$512, %ecx
	div	%ecx
	pop	%ecx
	add	%ecx, %eax
	movw	$temp_buffer, %di
	call	read_disk_sector

	/* Copy the inode to the inode buffer. */
	cld
	movw	$EXT2_INODE_SIZE, %cx
	movw	$inode, %di
	movw	$temp_buffer, %si
	addw	%dx, %si
	rep movsb

	popad
	ret
FUNCTION_END(read_inode)

/** Read a filesystem block.
 * @param %eax		Raw block number to read.
 * @param %es:%di	Location to read to. */
PRIVATE_FUNCTION_START(read_block)
	push	%eax
	push	%ecx

	/* Check for sparse blocks (raw block number 0). */
	test	%eax, %eax
	jz	.Lsparse_block

	/* Read sectors in from the disk. */
	movl	sectors_per_block, %ecx
	mul	%ecx
	call	read_disk_sectors
	jmp	.Lret
.Lsparse_block:
	push	%es
	push	%di

	/* Fill block with zeros. */
	movl	block_size, %ecx
	xorb	%al, %al
	rep stosb

	pop	%di
	pop	%es
.Lret:
	pop	%ecx
	pop	%eax
	ret
FUNCTION_END(read_block)

/** Read all data for the inode to LOADER_LOAD_ADDR. */
PRIVATE_FUNCTION_START(read_inode_data)
	pushad

	/* Set the new segment and start data address. */
	push	%es
	push	$(LOADER_LOAD_ADDR >> 4)
	pop	%es
	xorw	%di, %di

	/* Get total inode data size, divide it by block size and increment if
	 * there is a remainder, to get the total number of blocks we need to
	 * read. */
	movl	(inode + EXT2_INODE_OFF_SIZE), %eax
	xorl	%edx, %edx
	divl	block_size
	test	%edx, %edx
	jz	1f
	inc	%eax
1:
	/* Read each block to the destination.
	 * EAX = current block number, ECX = total blocks to read. */
	xorl	%ecx, %ecx
	xchg	%eax, %ecx
2:	push	%eax
	call	get_raw_block
	call	read_block

	/* Increment data buffer, handle overflowing into the next segment. */
	addw	block_size, %di
	jnc	3f
	movw	%es, %dx
	addw	$0x1000, %dx
	movw	%dx, %es
3:	pop	%eax
	inc	%eax
	dec	%ecx
	jnz	2b

	pop	%es
	popad
	ret
FUNCTION_END(read_inode_data)

/** Display a string on the screen.
 * @param %ds:%si	String to display. */
PRIVATE_FUNCTION_START(puts)
	pushw	%ax
1:	lodsb
	test	%al, %al
	jz	2f
	pushw	%bx
	movb	$0x0E, %ah
	movw	$1, %bx
	int	$0x10
	popw	%bx
	jmp	1b
2:	popw	%ax
	ret
FUNCTION_END(puts)

/* Print an error and stop.
 * @param %ds:%si	String to display. */
PRIVATE_FUNCTION_START(error)
	call	puts
1:	hlt
	jmp	1b
FUNCTION_END(error)

/** Error strings. */
msg_loading:		.asciz "Loading... "
msg_read_failed:	.asciz "Read failed!\r\n"
msg_not_ext2:		.asciz "Bad FS!\r\n"
msg_ok:			.asciz "OK\r\n"

/* Pad up to the end of the first 512 bytes, with space for the partition LBA
 * and the boot signature at the end. */
. = _start + 0x1F6
partition_lba:		.long 0
inode_number:		.long 0
boot_signature:		.word 0xAA55

second_half:

/** Look up a raw block number in the current inode.
 * @param %eax		Block number.
 * @return		Raw block number in %eax. Trashes %edx and %ebx. */
PRIVATE_FUNCTION_START(get_raw_block)
	/* Test for extents. */
	testl	$EXT4_EXTENTS_FL, (inode + EXT2_INODE_OFF_FLAGS)
	jnz	.Lextents

	cmp	$EXT2_NDIR_BLOCKS, %eax
	jae	.Lindirect

	/* Raw block number is stored in the inode. */
	shl	$2, %eax
	movl	(inode + EXT2_INODE_OFF_BLOCK)(%eax), %eax
	ret
.Lindirect:
	sub	$EXT2_NDIR_BLOCKS, %eax
	shl	$2, %eax
	cmp	block_size, %eax
	jae	.Lbi_indirect

	/* Read in the indirect block. */
	push	%eax
	movl	(inode + EXT2_INODE_OFF_BLOCK + (EXT2_IND_BLOCK * 4)), %eax
	call	read_block
	pop	%eax
	push	%di
	addw	%ax, %di
	movl	%es:(%di), %eax
	pop	%di
	ret
.Lbi_indirect:
	/* Don't bother supporting this. We only have to load the loader here,
	 * which shouldn't be too big. */
	ERROR(msg_too_large)
.Lextents:
	/* Find the leaf. First iteration we want to operate on i_block. All
	 * other iterations we want to operate on the current buffer location.
	 * Safe for us to trash %dx here to use as a loop counter. */
	push	%es
	push	%di
	xorw	%dx, %dx
	movw	%dx, %es
	movw	$inode + EXT2_INODE_OFF_BLOCK, %di

1:	/* Check whether the extent header is valid. */
	cmpw	$EXT4_EXT_MAGIC, %es:EXT4_EXTENT_HEADER_OFF_MAGIC(%di)
	jne	.Lbad_extent

	/* If the depth is 0, this is the leaf. */
	cmpw	$0, %es:EXT4_EXTENT_HEADER_OFF_DEPTH(%di)
	je	.Lfound_extent

	/* Loop through to find the index covering the block (%dx == 0). */
	movw	%es:EXT4_EXTENT_HEADER_OFF_ENTRIES(%di), %bx
	addw	$EXT4_EXTENT_HEADER_SIZE, %di
2:	cmpw	%bx, %dx
	jae	3f

	cmpl	%es:EXT4_EXTENT_IDX_OFF_BLOCK(%di), %eax
	jb	3f

	addw	$EXT4_EXTENT_IDX_SIZE, %di
	inc	%dx
	jmp	2b

3:	/* Count of 0 means the block was not covered by any index. */
	test	%dx, %dx
	jz	.Lbad_extent

	/* Get the block number to read. index[i - 1]. */
	movl	%es:(EXT4_EXTENT_IDX_OFF_LEAF - EXT4_EXTENT_IDX_SIZE)(%di), %ebx

	/* Restore the base address and read the next block (would be nice if
	 * %sp could be used as a base register...). File block number is
	 * preserved in %ebx. */
	pop	%di
	pop	%es
	xchgl	%eax, %ebx
	call	read_block
	movl	%ebx, %eax
	push	%es
	push	%di
	xorw	%dx, %dx
	jmp	1b
.Lfound_extent:
	/* %es:%di points at the header of the leaf. Loop through to find the
	 * extent covering the block (%dx == 0). TODO: Could possibly merge
	 * this with the similar loop above, but I'm lazy. Should be done if we
	 * need to free up some space in the future. */
	movw	%es:EXT4_EXTENT_HEADER_OFF_ENTRIES(%di), %bx
	addw	$EXT4_EXTENT_HEADER_SIZE, %di
1:	cmpw	%bx, %dx
	jae	2f

	cmpl	%es:EXT4_EXTENT_OFF_BLOCK(%di), %eax
	jb	2f

	addw	$EXT4_EXTENT_SIZE, %di
	inc	%dx
	jmp	1b

2:	/* Count of 0 means the block was not covered by any extent. */
	test	%dx, %dx
	jz	.Lbad_extent

	/* Subtract start block from block number. extent[i - 1]. */
	sub	%es:(EXT4_EXTENT_OFF_BLOCK - EXT4_EXTENT_SIZE)(%di), %eax

	/* If the offset is greater than the length of the extent, return 0,
	 * else add on the physical start block. */
	movzxw	%es:(EXT4_EXTENT_OFF_LEN - EXT4_EXTENT_SIZE)(%di), %edx
	movl	%eax, %ebx
	xorl	%eax, %eax
	cmp	%edx, %ebx
	jae	3f
	movl	%es:(EXT4_EXTENT_OFF_START - EXT4_EXTENT_SIZE)(%di), %eax
	add	%ebx, %eax

	/* Restore old base address and return. */
3:	pop	%di
	pop	%es
	ret
.Lbad_extent:
	ERROR(msg_bad_extent)
FUNCTION_END(get_raw_block)

/** More error strings. */
msg_too_large:		.asciz "Too large!\r\n"
msg_bad_extent:		.asciz "Bad extent!\r\n"

/** Pad up to 1KB. */
. = _start + 0x3FF
.byte 0

.section .bss

/** Superblock. This is read into memory along with our second half. */
superblock:		.fill EXT2_SUPERBLOCK_SIZE

/** Current inode buffer. */
inode:			.fill EXT2_INODE_SIZE

/** Temporary buffer. */
temp_buffer:		.fill 512

/** Disk address packet. */
dap_size:		.byte 0
dap_reserved1:		.byte 0
dap_count:		.byte 0
dap_reserved2:		.byte 0
dap_buffer_offset:	.word 0
dap_buffer_segment:	.word 0
dap_lba_low:		.long 0
dap_lba_high:		.long 0

/** Saved BIOS drive ID. */
bios_drive_id:		.byte 0

/** Pre-computed FS information. */
block_size:		.long 0
sectors_per_block:	.long 0
group_table_offset:	.long 0
