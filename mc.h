#include <stdint.h>

#define SECTOR_SIZE 4096

typedef struct {
  unsigned char offset[3];
  unsigned char sector_count;
} chunk_location_raw_t;

typedef struct {
  uint32_t offset;
  uint8_t sector_count;
} chunk_location_t;

void create_chunk_location(chunk_location_raw_t *from, chunk_location_t *to);

int get_chunk_offset_in_header(int x, int z);
