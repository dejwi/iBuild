#include "mc.h"
#include "zlib-utils.h"
#include <stdio.h>
#include <stdlib.h>

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

  int compressed_data_len;
  int compression_type;
  if (fread(&compressed_data_len, sizeof(compressed_data_len), 1, fptr) != 1) {
    printf("Error reading payload len");
    exit(1);
  }
  if (fread(&compression_type, 1, 1, fptr) != 1) {
    printf("Error reading payload len");
    exit(1);
  }
  compressed_data_len = ntohl(compressed_data_len);

  printf("\n Compression type: %d, len: %d", compression_type,
         compressed_data_len);
  unsigned char *comp_data = malloc(compressed_data_len - 1);

  if (fread(comp_data, compressed_data_len - 1, 1, fptr) != 1) {
    printf("Error reading payload content");
    exit(1);
  }

  size_t uncomp_len = 0;
  unsigned char *uncomp_data =
      decompress_zlib(comp_data, compressed_data_len - 1, &uncomp_len);

  if (uncomp_data) {
    printf("\n Success uncompressing data len: %zu", uncomp_len);
  }
  printf("\nSuccess reading j");
  /* printf("\n Total sectors: %d", total_sector_count); */
  return 0;
}
