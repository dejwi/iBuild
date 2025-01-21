#include "mc.h"

void create_chunk_location(chunk_location_raw_t *from, chunk_location_t *to) {
  to->offset = from->offset[0] << 16 | from->offset[1] << 8 | from->offset[2];
  to->sector_count = from->sector_count;
}

/* int get_chunk_offest(chunk_location_raw_t *chunk) { */
/*   return chunk->offset[0] << 16 | chunk->offset[1] << 8 | chunk->offset[2];
 */
/* } */

int get_chunk_offset_in_header(int x, int z) {
  return 4 * ((x & 31) + (z & 31) * 32);
}
