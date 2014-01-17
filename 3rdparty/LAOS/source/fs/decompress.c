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
 * @brief		File decompression support.
 */

#include <lib/utility.h>

#include <endian.h>
#include <memory.h>
#include <fs.h>
#include <loader.h>

#include "../lib/zlib/zlib.h"

#include "decompress.h"

/** Magic numbers for a gzip file. */
#define GZIP_MAGIC1		0x1F
#define GZIP_MAGIC2		0x8B
#define GZIP_DEFLATE		0x08

/** Size of the input buffer. */
#define INPUT_BUFFER_SIZE	4096

/** Decompression state structure. */
typedef struct decompress_state {
	z_stream stream;		/**< Zlib stream. */
	offset_t output_size;		/**< Total size of the output file. */
	offset_t output_offset;		/**< Current offset in the output file. */
	offset_t input_size;		/**< Total size of the input file. */
	offset_t input_offset;		/**< Current offset in the input file. */

	/** Buffer for data read from the input file. */
	uint8_t buffer[INPUT_BUFFER_SIZE];
} decompress_state_t;

/** Currently active decompression state. */
static decompress_state_t *active_decompress_state = NULL;

/** Allocation function for zlib.
 * @param data		Ignored.
 * @param items		Number of items.
 * @param size		Size of each item.
 * @return		Pointer to allocated memory. */
static void *zlib_alloc(void *data, unsigned int items, unsigned int size) {
	/* Should be OK to use kmalloc() here, the zlib docs say that inflate's
	 * memory usage should not exceed ~44KB. */
	return kmalloc(items * size);
}

/** Free function for zlib.
 * @param data		Ignored.
 * @param addr		Address of buffer to free. */
static void zlib_free(void *data, void *addr) {
	kfree(addr);
}

/** Initialize decompression state for a file.
 * @param handle	Handle to file to open. */
void decompress_open(file_handle_t *handle) {
	decompress_state_t *state;
	uint8_t id[4];
	uint32_t size;

	if(!handle->mount->type->read(handle, id, sizeof(id), 0))
		return;

	/* Check for gzip identification information. */
	if(id[0] != GZIP_MAGIC1 || id[1] != GZIP_MAGIC2 || id[2] != GZIP_DEFLATE)
		return;

	/* Allocate a state structure. */
	state = kmalloc(sizeof(*state));
	state->input_size = handle->mount->type->size(handle);
	state->input_offset = 0;

	/* Get the original file size. This is stored in the last 4 bytes of
	 * the file (ISIZE field). */
	if(!handle->mount->type->read(handle, &size, sizeof(size), state->input_size - 4)) {
		kfree(state);
		return;
	}

	state->output_size = le32_to_cpu(size);
	state->output_offset = 0;

	/* Store the state so that the FS code will direct reads through us. */
	handle->compressed = state;
}

/** Free decompression state for a file.
 * @param handle	Handle to file to close. */
void decompress_close(file_handle_t *handle) {
	decompress_state_t *state = handle->compressed;

	if(state == active_decompress_state)
		inflateEnd(&state->stream);

	kfree(state);
}

/** Fetch input from the source file.
 * @param handle	Handle to input file.
 * @param state		Decompression state.
 * @return		Whether successful. */
static bool stream_input(file_handle_t *handle, decompress_state_t *state) {
	offset_t count;

	/* Don't read past the end of the file. */
	count = MIN(state->input_size - state->input_offset, INPUT_BUFFER_SIZE);

	if(!handle->mount->type->read(handle, state->buffer, count, state->input_offset))
		return false;

	state->input_offset += count;
	state->stream.next_in = state->buffer;
	state->stream.avail_in = count;
	return true;
}

/** Decompress data from the file and write it to a buffer.
 * @param handle	Handle to input file.
 * @param state		Decompression state.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * return		Whether successful. */
static bool stream_output(file_handle_t *handle, decompress_state_t *state, void *buf, size_t count) {
	int ret;

	state->stream.next_out = buf;
	state->stream.avail_out = count;

	do {
		/* Make sure we have input available. */
		if(!state->stream.avail_in && !stream_input(handle, state))
			return false;

		ret = inflate(&state->stream, Z_NO_FLUSH);
		if(ret == Z_DATA_ERROR)
			return false;
	} while(state->stream.avail_out && ret != Z_STREAM_END);

	if(state->stream.avail_out)
		return false;

	state->output_offset += count;
	return true;
}

/** Read from a compressed file.
 * @param handle	Handle to file to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset in the file to read from.
 * @return		Whether the read was successful. */
bool decompress_read(file_handle_t *handle, void *buf, size_t count, offset_t offset) {
	decompress_state_t *state = handle->compressed;
	uint8_t ch;
	int ret;

	if(offset >= state->output_size) {
		return false;
	} else if((offset + count) > state->output_size) {
		return false;
	}

	/* Make us the active state. We only keep one state active at a time
	 * to avoid using too much heap space. */
	if(state != active_decompress_state) {
		if(active_decompress_state)
			inflateEnd(&active_decompress_state->stream);

		state->output_offset = 0;
		state->input_offset = 0;

		/* Initialize zlib state. */
		state->stream.zalloc = zlib_alloc;
		state->stream.zfree = zlib_free;
		state->stream.opaque = NULL;
		state->stream.next_in = NULL;
		state->stream.avail_in = 0;
		ret = inflateInit2(&state->stream, 15 + 16);
		if(ret != Z_OK) {
			dprintf("fs: failed to initialize zlib: %d\n", ret);
			return false;
		}

		active_decompress_state = state;
	}

	if(state->output_offset > offset) {
		/* Return to the beginning of the stream. */
		inflateReset(&state->stream);
		state->output_offset = 0;
		state->input_offset = 0;
		state->stream.avail_in = 0;
	}

	/* Read in bytes until we reach the desired position. */
	while(state->output_offset < offset) {
		if(!stream_output(handle, state, &ch, 1))
			return false;
	}

	return stream_output(handle, state, buf, count);
}

/** Get the size of a compressed file.
 * @param handle	Handle to the file.
 * @return		Size of the file. */
offset_t decompress_size(file_handle_t *handle) {
	decompress_state_t *state = handle->compressed;
	return state->output_size;
}
