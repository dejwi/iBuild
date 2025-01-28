#include "mc.h"
#include "zlib-utils.h"
#include <stdio.h>

int main() {
  char TEST_PATH[] =
      "/Users/dawid/Library/Application "
      "Support/PrismLauncher/instances/Light-Craft---1.19.3---4.6.3/minecraft/"
      "saves/testc/region/r.0.0.mca";

  char **pal = malloc(sizeof(char *) * 2);
  pal[0] = "minecraft:pink_wool";
  pal[1] = "minecraft:red_wool";

  int ind[] = {
      0,  0,  0, 0, 0,  0,  /**/ 0,  0,  0, 0, 0,  0,
      0,  0,  0, 0, 0,  0,  /**/ 0,  0,  0, 0, 0,  0,
      -1, -1, 0, 0, -1, -1, /**/ -1, -1, 0, 0, -1, -1,
      -1, -1, 0, 0, -1, -1, /**/ -1, -1, 0, 0, -1, -1,
      -1, -1, 0, 0, -1, -1, /**/ -1, -1, 0, 0, -1, -1,
      -1, -1, 0, 0, -1, -1, /**/ -1, -1, 0, 0, -1, -1,
      -1, -1, 0, 0, -1, -1, /**/ -1, -1, 0, 0, -1, -1,
      -1, -1, 1, 1, -1, -1, /**/ -1, -1, 1, 1, -1, -1,
      -1, -1, 1, 1, -1, -1, /**/ -1, -1, 1, 1, -1, -1,
  };
  block_build_t build = {.palette = pal,
                         .palette_len = 2,
                         .y_size = 9,
                         .x_size = 6,
                         .z_size = 2,
                         .chunk_pos = {.x = 5, .y = 117, .z = 6},
                         .indices = ind

  };
  test_chunk_edit(TEST_PATH, 0, 0, &build);
  /* printf("%d %d %d\n", 96 / 16, 79 / 16, 85 / 16); */

  printf("Success reading end\n");
  return 0;
}
