#include <stdlib.h>

unsigned char *decompress_zlib(const unsigned char *compressed_data,
                               size_t compressed_size,
                               size_t *decompressed_size);
