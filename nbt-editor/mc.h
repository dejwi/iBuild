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
  vector3_t pos;
  char **palette;
  int palette_len;
  int *indices;
} block_build_t;

/**
 * Edits a chunk in a Minecraft region file.
 *
 * @param region_path Path to the region file.
 * @param x_chunk X (chunk) coordinate of the chunk.
 * @param z_chunk Z (chunk) coordinate of the chunk.
 * @param build Pointer to the block build structure containing the changes.
 * @return 0 on success, 1 if the chunk is not generated.
 */
int chunk_edit(const char *region_path, int x_chunk, int z_chunk,
               const block_build_t *build);
