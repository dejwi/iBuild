#include "mc.h"
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

int main() {
  char TEST_PATH[] =
      "/Users/dawid/Library/Application "
      "Support/PrismLauncher/instances/Light-Craft---1.19.3---4.6.3/minecraft/"
      "saves/testc/region/r.0.0.mca";

  FILE *fptr;
  // Open a file in read mode
  fptr = fopen(TEST_PATH, "rb");
  int total_sector_count = 0;
  /* int modify_count = SECTOR_SIZE / sizeof(chunk_location); */
  int modify_count = 1;
  chunk_location_raw_t chunk_raw;
  for (int i = 0; i < modify_count; i++) {

    // Store the content of the file

    // Read the content and store it inside myString
    if (fread(&chunk_raw, sizeof(chunk_raw), 1, fptr) != 1) {
      printf("Error reading");
      exit(1);
    }

    /* total_sector_count += s_count; */
    /* printf("%d+%d ", offset, s_count); */
  }

  chunk_location_t chunk;
  create_chunk_location(&chunk_raw, &chunk);

  fseek(fptr, chunk.offset * SECTOR_SIZE, SEEK_SET);

  unsigned char payload_head[5];
  if (fread(&payload_head, sizeof(payload_head), 1, fptr) != 1) {
    printf("Error reading payload");
    exit(1);
  }

  int comp_type = payload_head[4];
  printf("\n Compression type: %d", comp_type);

  printf("\nSuccess reading j");
  /* printf("\n Total sectors: %d", total_sector_count); */
  return 0;
}
