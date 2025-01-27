#include "mc.h"
#include "zlib-utils.h"
#include <stdio.h>

int main() {
  char TEST_PATH[] =
      "/Users/dawid/Library/Application "
      "Support/PrismLauncher/instances/Light-Craft---1.19.3---4.6.3/minecraft/"
      "saves/testc/region/r.0.0.mca";
  char *wool = "minecraft:yellow_wool";
  int ind[] = {
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  -1, -1, 0,  0,  -1, -1,
      -1, -1, 0,  0,  -1, -1, -1, -1, 0,  0,  -1, -1, -1, -1, 0,
      0,  -1, -1, -1, -1, 0,  0,  -1, -1, -1, -1, 0,  0,  -1, -1,
  };
  block_build_t build = {.palette = &wool,
                         .palette_len = 1,
                         .y_size = 5,
                         .x_size = 6,
                         .z_size = 2,
                         .chunk_pos = {.x = 5, .y = 89, .z = 6},
                         .indices = ind

  };
  test_chunk_edit(TEST_PATH, 0, 0, &build);
  /* printf("%d %d %d\n", 96 / 16, 79 / 16, 85 / 16); */

  printf("Success reading end\n");
  return 0;
}
