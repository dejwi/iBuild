#include "mc.h"
#include "zlib-utils.h"
#include <stdio.h>

int main() {
  char TEST_PATH[] =
      "/Users/dawid/Library/Application "
      "Support/PrismLauncher/instances/Light-Craft---1.19.3---4.6.3/minecraft/"
      "saves/testc/region/r.0.0.mca";

  test_chunk_edit(TEST_PATH, 0, 0, 5, "minecraft:sand", "minecraft:cake");

  printf("Success reading end\n");
  return 0;
}
