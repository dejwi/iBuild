#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

typedef struct {
  int x;
  int y;
  int z;
} vector3_t;

typedef struct {
  int x_size;
  int y_size;
  int z_size;
  vector3_t chunk_pos;
  char **palette;
  int palette_len;
  int *indices;
} block_build_t;

void test_chunk_edit(const char *region_path, int x_chunk, int z_chunk,
                     const block_build_t *build);
