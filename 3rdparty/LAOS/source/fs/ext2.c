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
 * @brief		Ext2 filesystem support.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <endian.h>
#include <fs.h>
#include <loader.h>
#include <memory.h>

#include "ext2.h"

/** Data for an Ext2 mount. */
typedef struct ext2_mount {
	ext2_superblock_t sb;		/**< Superblock of the filesystem. */
	ext2_group_desc_t *group_tbl;	/**< Pointer to block group descriptor table. */
	uint32_t inodes_per_group;	/**< Inodes per group. */
	uint32_t inodes_count;		/**< Inodes count. */
	size_t block_size;		/**< Size of a block on the filesystem. */
	size_t block_groups;		/**< Number of block groups. */
	size_t inode_size;		/**< Size of an inode. */
} ext2_mount_t;

static bool ext2_read(file_handle_t *handle, void *buf, size_t count, offset_t offset);

/** Read a block from an Ext2 filesystem.
 * @param mount		Mount to read from.
 * @param buf		Buffer to read into.
 * @param num		Block number.
 * @return		Whether successful. */
static bool ext2_block_read(mount_t *mount, void *buf, uint32_t num) {
	ext2_mount_t *data = mount->data;
	return disk_read(mount->disk, buf, data->block_size, (uint64_t)num * data->block_size);
}

/** Recurse through the extent index tree to find a leaf.
 * @param mount		Mount being read from.
 * @param header	Extent header to start at.
 * @param block		Block number to get.
 * @param buf		Temporary buffer to use.
 * @return		Pointer to header for leaf, NULL on failure. */
static ext4_extent_header_t *ext4_find_leaf(mount_t *mount, ext4_extent_header_t *header,
	uint32_t block, void *buf)
{
	ext4_extent_idx_t *index;
	uint16_t i;

	while(true) {
		index = (ext4_extent_idx_t *)&header[1];

		if(le16_to_cpu(header->eh_magic) != EXT4_EXT_MAGIC) {
			return NULL;
		} else if(!le16_to_cpu(header->eh_depth)) {
			return header;
		}

		for(i = 0; i < le16_to_cpu(header->eh_entries); i++) {
			if(block < le32_to_cpu(index[i].ei_block))
				break;
		}

		if(!i) {
			return NULL;
		} else if(!ext2_block_read(mount, buf, le32_to_cpu(index[i - 1].ei_leaf))) {
			return NULL;
		}
		header = (ext4_extent_header_t *)buf;
	}
}

/** Get the raw block number from an inode block number.
 * @todo		Triple indirect blocks. Not really that big a deal,
 *			it is unlikely that a file that big will need to be
 *			read during boot.
 * @param handle	Handle to inode to get block number from.
 * @param block		Block number within the inode to get.
 * @param nump		Where to store raw block number.
 * @return		Whether successful. */
static bool ext2_inode_block_get(file_handle_t *handle, uint32_t block, uint32_t *nump) {
	uint32_t *i_block = NULL, *bi_block = NULL, num;
	ext2_mount_t *mount = handle->mount->data;
	ext2_inode_t *inode = handle->data;
	ext4_extent_header_t *header;
	ext4_extent_t *extent;
	void *buf = NULL;
	bool ret = false;
	uint16_t i;

	if(le32_to_cpu(inode->i_flags) & EXT4_EXTENTS_FL) {
		buf = kmalloc(mount->block_size);
		header = ext4_find_leaf(handle->mount, (ext4_extent_header_t *)inode->i_block, block, buf);
		if(!header)
			goto out;

		extent = (ext4_extent_t *)&header[1];
		for(i = 0; i < le16_to_cpu(header->eh_entries); i++) {
			if(block < le32_to_cpu(extent[i].ee_block))
				break;
		}

		if(!i)
			goto out;

		block -= le32_to_cpu(extent[i - 1].ee_block);
		if(block >= le16_to_cpu(extent[i - 1].ee_len)) {
			*nump = 0;
		} else {
			*nump = block + le32_to_cpu(extent[i - 1].ee_start);
		}

		ret = true;
		goto out;
	} else {
		/* First check if it's a direct block. This is easy to handle,
		 * just need to get it straight out of the inode structure. */
		if(block < EXT2_NDIR_BLOCKS) {
			*nump = le32_to_cpu(inode->i_block[block]);
			ret = true;
			goto out;
		}

		block -= EXT2_NDIR_BLOCKS;
		i_block = kmalloc(mount->block_size);

		/* Check whether the indirect block contains the block number
		 * we need. The indirect block contains as many 32-bit entries
		 * as will fit in one block of the filesystem. */
		if(block < (mount->block_size / sizeof(uint32_t))) {
			num = le32_to_cpu(inode->i_block[EXT2_IND_BLOCK]);
			if(num == 0) {
				*nump = 0;
				ret = true;
				goto out;
			} else if(!ext2_block_read(handle->mount, i_block, num)) {
				goto out;
			}

			*nump = le32_to_cpu(i_block[block]);
			ret = true;
			goto out;
		}

		block -= mount->block_size / sizeof(uint32_t);
		bi_block = kmalloc(mount->block_size);

		/* Not in the indirect block, check the bi-indirect blocks. The
		 * bi-indirect block contains as many 32-bit entries as will
		 * fit in one block of the filesystem, with each entry pointing
		 * to an indirect block. */
		if(block < ((mount->block_size / sizeof(uint32_t)) * (mount->block_size / sizeof(uint32_t)))) {
			num = le32_to_cpu(inode->i_block[EXT2_DIND_BLOCK]);
			if(num == 0) {
				*nump = 0;
				ret = true;
				goto out;
			} else if(!ext2_block_read(handle->mount, bi_block, num)) {
				goto out;
			}

			/* Get indirect block inside bi-indirect block. */
			num = le32_to_cpu(bi_block[block / (mount->block_size / sizeof(uint32_t))]);
			if(num == 0) {
				*nump = 0;
				ret = true;
				goto out;
			} else if(!ext2_block_read(handle->mount, i_block, num)) {
				goto out;
			}

			*nump = le32_to_cpu(i_block[block % (mount->block_size / sizeof(uint32_t))]);
			ret = true;
			goto out;
		}

		/* Triple indirect block. I somewhat doubt this will be needed,
		 * aren't likely to need to read files that big. */
		dprintf("ext2: tri-indirect blocks not yet supported!\n");
		goto out;
	}
out:
	kfree(bi_block);
	kfree(i_block);
	kfree(buf);
	return ret;
}

/** Read blocks from an Ext2 inode.
 * @param handle	Handle to inode to read from.
 * @param buf		Buffer to read into.
 * @param block		Starting block number.
 * @return		Whether read successfully. */
static bool ext2_inode_block_read(file_handle_t *handle, void *buf, uint32_t block) {
	ext2_mount_t *mount = handle->mount->data;
	ext2_inode_t *inode = handle->data;
	uint32_t raw = 0;

	if(block >= ROUND_UP(le32_to_cpu(inode->i_size), mount->block_size) / mount->block_size) {
		return false;
	} else if(!ext2_inode_block_get(handle, block, &raw)) {
		return false;
	}

	/* If the block number is 0, then it's a sparse block. */
	if(raw == 0) {
		memset(buf, 0, mount->block_size);
		return true;
	} else {
		return ext2_block_read(handle->mount, buf, raw);
	}
}

/** Read an inode from the filesystem.
 * @param mount		Mount to read from.
 * @param id		ID of node. If the node is a symbolic link, the link
 *			destination will be returned.
 * @param from		Directory that the inode was found in.
 * @return		Pointer to handle to inode on success, NULL on failure. */
static file_handle_t *ext2_inode_get(mount_t *mount, uint32_t id, file_handle_t *from) {
	ext2_mount_t *data = mount->data;
	file_handle_t *handle;
	ext2_inode_t *inode;
	size_t group, size;
	offset_t offset;
	uint16_t type;
	char *dest;

	/* Get the group descriptor table containing the inode. */
	if((group = (id - 1) / data->inodes_per_group) >= data->block_groups) {
		dprintf("ext2: bad inode number %llu\n", id);
		return NULL;
	}

	/* Get the offset of the inode in the group's inode table. */
	offset = ((id - 1) % data->inodes_per_group) * data->inode_size;

	/* Read the inode into memory. */
	inode = kmalloc(sizeof(ext2_inode_t));
	size = (data->inode_size <= sizeof(ext2_inode_t)) ? data->inode_size : sizeof(ext2_inode_t);
	offset = ((offset_t)le32_to_cpu(data->group_tbl[group].bg_inode_table) * data->block_size) + offset;
	if(!disk_read(mount->disk, inode, size, offset)) {
		dprintf("ext2: failed to read inode %" PRIu64 "\n", id);
		kfree(inode);
		return false;
	}

	type = le16_to_cpu(inode->i_mode) & EXT2_S_IFMT;
	switch(type) {
	case EXT2_S_IFLNK:
		assert(from);

		/* Symbolic link. Read in the link and try to open that path. */
		size = le32_to_cpu(inode->i_size);
		dest = kmalloc(size + 1);
		if(le32_to_cpu(inode->i_blocks) == 0) {
			memcpy(dest, inode->i_block, size);
			kfree(inode);
		} else {
			handle = file_handle_create(mount, false, inode);
			if(!ext2_read(handle, dest, size, 0)) {
				file_close(handle);
				return NULL;
			}

			file_close(handle);
		}

		dest[size] = 0;

		/* Welp, recursion. TODO: Should limit this. */
		handle = file_open(dest, from);
		break;
	case EXT2_S_IFDIR:
		handle = file_handle_create(mount, true, inode);
		break;
	case EXT2_S_IFREG:
		handle = file_handle_create(mount, false, inode);
		break;
	default:
		/* Don't support reading other types here. */
		handle = NULL;
		break;
	}

	return handle;
}

/** Create an instance of an Ext2 filesystem.
 * @param mount		Mount structure to fill in.
 * @return		Whether succeeded in mounting. */
static bool ext2_mount(mount_t *mount) {
	ext2_mount_t *data;
	offset_t offset;
	size_t size;

	/* Create a mount structure to track information about the mount. */
	data = mount->data = kmalloc(sizeof(ext2_mount_t));

	/* Read in the superblock. Must recheck whether we support it as
	 * something could change between probe and this function. */
	if(!disk_read(mount->disk, &data->sb, sizeof(ext2_superblock_t), 1024)) {
		goto fail;
	} else if(le16_to_cpu(data->sb.s_magic) != EXT2_MAGIC) {
		goto fail;
	} else if(le32_to_cpu(data->sb.s_rev_level) != EXT2_DYNAMIC_REV) {
		/* Have to reject this because GOOD_OLD_REV does not have
		 * a UUID or label. */
		dprintf("ext2: not EXT2_DYNAMIC_REV!\n");
		goto fail;
	}

	/* Get useful information out of the superblock. */
	data->inodes_per_group = le32_to_cpu(data->sb.s_inodes_per_group);
	data->inodes_count = le32_to_cpu(data->sb.s_inodes_count);
	data->block_size = 1024 << le32_to_cpu(data->sb.s_log_block_size);
	data->block_groups = data->inodes_count / data->inodes_per_group;
	data->inode_size = le16_to_cpu(data->sb.s_inode_size);

	/* Read in the group descriptor table. */
	offset = data->block_size * (le32_to_cpu(data->sb.s_first_data_block) + 1);
	size = ROUND_UP(data->block_groups * sizeof(ext2_group_desc_t), data->block_size);
	data->group_tbl = kmalloc(size);
	if(!disk_read(mount->disk, data->group_tbl, size, offset))
		goto fail;

	/* Now get the root inode (second inode in first group descriptor) */
	if(!(mount->root = ext2_inode_get(mount, EXT2_ROOT_INO, NULL)))
		goto fail;

	/* Store label and UUID. */
	mount->label = kstrdup(data->sb.s_volume_name);
	mount->uuid = kmalloc(37);
	sprintf(mount->uuid, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		data->sb.s_uuid[0], data->sb.s_uuid[1], data->sb.s_uuid[2],
		data->sb.s_uuid[3], data->sb.s_uuid[4], data->sb.s_uuid[5],
		data->sb.s_uuid[6], data->sb.s_uuid[7], data->sb.s_uuid[8],
		data->sb.s_uuid[9], data->sb.s_uuid[10], data->sb.s_uuid[11],
		data->sb.s_uuid[12], data->sb.s_uuid[13], data->sb.s_uuid[14],
		data->sb.s_uuid[15]);

	dprintf("ext2: mounted%s%s (%s) (uuid: %s)\n", (mount->label[0]) ? " " : "", mount->label,
		mount->disk->device.name, mount->uuid);
	return true;
fail:
	kfree(data);
	return false;
}

/** Close a handle.
 * @param handle	Handle to close. */
static void ext2_close(file_handle_t *handle) {
	kfree(handle->data);
}

/** Read from an Ext2 inode.
 * @param handle	Handle to the inode.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset into the file.
 * @return		Whether read successfully. */
static bool ext2_read(file_handle_t *handle, void *buf, size_t count, offset_t offset) {
	ext2_mount_t *mount = handle->mount->data;
	size_t blksize = mount->block_size;
	uint32_t start, end, i, size;
	void *block = NULL;

	/* Allocate a temporary buffer for partial transfers if required. */
	if(offset % blksize || count % blksize)
		block = kmalloc(blksize);

	/* Now work out the start block and the end block. Subtract one from
	 * count to prevent end from going onto the next block when the offset
	 * plus the count is an exact multiple of the block size. */
	start = offset / blksize;
	end = (offset + (count - 1)) / blksize;

	/* If we're not starting on a block boundary, we need to do a partial
	 * transfer on the initial block to get up to a block boundary. 
	 * If the transfer only goes across one block, this will handle it. */
	if(offset % blksize) {
		/* Read the block into the temporary buffer. */
		if(!ext2_inode_block_read(handle, block, start)) {
			kfree(block);
			return false;
		}

		size = (start == end) ? count : blksize - (size_t)(offset % blksize);
		memcpy(buf, block + (offset % blksize), size);
		buf += size; count -= size; start++;
	}

	/* Handle any full blocks. */
	size = count / blksize;
	for(i = 0; i < size; i++, buf += blksize, count -= blksize, start++) {
		/* Read directly into the destination buffer. */
		if(!ext2_inode_block_read(handle, buf, start)) {
			kfree(block);
			return false;
		}
	}

	/* Handle anything that's left. */
	if(count > 0) {
		if(!ext2_inode_block_read(handle, block, start)) {
			kfree(block);
			return false;
		}

		memcpy(buf, block, count);
	}

	kfree(block);
	return true;
}

/** Get the size of a file.
 * @param handle	Handle to the file.
 * @return		Size of the file. */
static offset_t ext2_size(file_handle_t *handle) {
	ext2_inode_t *inode = handle->data;
	return le32_to_cpu(inode->i_size);
}

/** Iterate over directory entries.
 * @param handle	Handle to directory.
 * @param cb		Callback to call on each entry.
 * @param arg		Data to pass to callback.
 * @return		Whether read successfully. */
static bool ext2_iterate(file_handle_t *handle, dir_iterate_cb_t cb, void *arg) {
	ext2_inode_t *inode = handle->data;
	char *buf = NULL, *name = NULL;
	ext2_dirent_t *dirent;
	uint32_t current = 0;
	file_handle_t *child;
	bool ret = false;

	/* Allocate buffers to read the data into. */
	buf = kmalloc(le32_to_cpu(inode->i_size));
	name = kmalloc(EXT2_NAME_MAX + 1);

	/* Read in all the directory entries required. */
	if(!ext2_read(handle, buf, le32_to_cpu(inode->i_size), 0))
		goto out;

	while(current < le32_to_cpu(inode->i_size)) {
		dirent = (ext2_dirent_t *)(buf + current);
		current += le16_to_cpu(dirent->rec_len);

		if(dirent->file_type != EXT2_FT_UNKNOWN && dirent->name_len != 0) {
			strncpy(name, dirent->name, dirent->name_len);
			name[dirent->name_len] = 0;

			/* Create a handle to the child. */
			child = ext2_inode_get(handle->mount, le32_to_cpu(dirent->inode), handle);
			if(!child)
				goto out;

			if(!cb(name, child, arg)) {
				file_close(child);
				break;
			}

			file_close(child);
		} else if(!le16_to_cpu(dirent->rec_len)) {
			break;
		}
	}

	ret = true;
out:
	kfree(buf);
	kfree(name);
	return ret;
}

/** Ext2 filesystem operations structure. */
BUILTIN_FS_TYPE(ext2_fs_type) = {
	.mount = ext2_mount,
	.close = ext2_close,
	.read = ext2_read,
	.size = ext2_size,
	.iterate = ext2_iterate,
};
