#pragma once

#include <stdlib.h>

unsigned char *decompress_zlib(const unsigned char *compressed_data,
                               size_t compressed_size,
                               size_t *decompressed_size);

unsigned char *compress_zlib(const unsigned char *data, size_t size,
                             size_t *out_size);
