#include "zlib-utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define BLOCK_SIZE 16384

typedef struct block_t {
  unsigned char *data;
  struct block_t *next;
} block_t;

void free_blocks(block_t *head) {
  while (head) {
    block_t *temp = head;
    free(head->data);
    head = head->next;
    free(temp);
  }
}

unsigned char *decompress_zlib(const unsigned char *compressed_data,
                               size_t compressed_size,
                               size_t *decompressed_size) {
  z_stream stream;
  int ret;
  block_t *head = malloc(sizeof(block_t));
  block_t *tail = head;

  head->data = malloc(BLOCK_SIZE);
  head->next = NULL;

  *decompressed_size = 0;

  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;
  stream.opaque = Z_NULL;
  stream.next_in = (Bytef *)compressed_data;
  stream.avail_in = compressed_size;

  if (inflateInit(&stream) != Z_OK) {
    fprintf(stderr, "Failed to initialize zlib.\n");
    return NULL;
  }

  stream.next_out = head->data;
  stream.avail_out = BLOCK_SIZE;

  // Decompression loop
  do {
    ret = inflate(&stream, Z_NO_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
      fprintf(stderr, "Decompression failed with error code: %d\n", ret);
      inflateEnd(&stream);
      free_blocks(head);
      return NULL;
    }

    if (ret != Z_STREAM_END && stream.avail_in > 0 && stream.avail_out == 0) {
      // Create a new block
      block_t *new_block = malloc(sizeof(block_t));
      if (!new_block) {
        fprintf(stderr, "Memory allocation failed.\n");
        inflateEnd(&stream);
        free_blocks(head);
        return NULL;
      }
      new_block->data = malloc(BLOCK_SIZE);
      if (!new_block->data) {
        fprintf(stderr, "Memory allocation failed.\n");
        free(new_block);
        inflateEnd(&stream);
        free_blocks(head);
        return NULL;
      }
      new_block->next = NULL;

      stream.avail_out = BLOCK_SIZE;
      stream.next_out = new_block->data;

      tail->next = new_block;
      tail = new_block;
    }
  } while (ret != Z_STREAM_END);

  inflateEnd(&stream);

  *decompressed_size = stream.total_out;

  // Combine all blocks into a single buffer
  unsigned char *combined_data = malloc(*decompressed_size);
  if (!combined_data) {
    fprintf(stderr, "Memory allocation failed.\n");
    free_blocks(head);
    return NULL;
  }

  size_t offset = 0;
  block_t *current = head;
  while (current) {
    size_t bytes_left = *decompressed_size - offset;
    memcpy(combined_data + offset, current->data,
           bytes_left > BLOCK_SIZE ? BLOCK_SIZE : bytes_left);
    offset += BLOCK_SIZE;
    current = current->next;
  }

  free_blocks(head);
  return combined_data;
}

unsigned char *compress_zlib(const unsigned char *data, size_t size,
                             size_t *out_size) {
  z_stream stream;

  unsigned char *buffer = malloc(size);
  if (buffer == NULL) {
    fprintf(stderr, "Failed to allocate buffer for data compression");
    exit(1);
  }

  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;
  stream.opaque = Z_NULL;
  stream.avail_in = size;
  stream.next_in = (Bytef *)data;
  stream.avail_out = size;
  stream.next_out = buffer;

  if (deflateInit(&stream, Z_DEFAULT_COMPRESSION) != Z_OK) {
    fprintf(stderr, "DeflateInit failed\n");
    exit(1);
  }
  if (deflate(&stream, Z_FINISH) != Z_STREAM_END) {
    fprintf(stderr, "Deflate failed\n");
    exit(1);
  }
  if (deflateEnd(&stream) != Z_OK) {
    fprintf(stderr, "DeflateEnd failed\n");
    exit(1);
  }

  *out_size = stream.total_out;
  return buffer;
}
