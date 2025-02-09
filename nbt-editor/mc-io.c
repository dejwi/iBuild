#include "mc-io.h"
#include "zf_log.h"
#include "zlib-utils.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#endif

void create_chunk_location(const chunk_location_raw_t *from,
                           chunk_location_t *to) {
  to->offset = 0;
  to->offset = from->offset[0] << 16 | from->offset[1] << 8 | from->offset[2];
  to->sector_count = from->sector_count;
}

void append_insert_list(insert_data_t **head, insert_data_t **tail,
                        insert_data_t *add) {
  if (*head == NULL) {
    *head = add;
    *tail = add;
  } else {
    (*tail)->next = add;
    *tail = add;
  }
  if (add->next != NULL) {
    insert_data_t *temp = add->next;
    while (temp->next != NULL) {
      temp = temp->next;
    }
    *tail = temp;
  }
}

void free_insert_list(insert_data_t *head) {
  while (head != NULL) {
    free(head->data_insert);
    insert_data_t *temp = head;
    head = head->next;
    free(temp);
  }
}

unsigned char *insert_data(const unsigned char *data, size_t size,
                           const insert_data_t *payload, size_t *out_size) {
  int total_size_diff = 0;
  size_t data_offset = 0;
  size_t buffer_offset = 0;

  const insert_data_t *temp = payload;
  while (temp) {
    ZF_LOGV("insert off: %zu", temp->start_offset);
    total_size_diff +=
        temp->start_offset - temp->end_offset + temp->data_size - 1;

    temp = temp->next;
  }
  ZF_LOGV("size_diff_insert: %d", total_size_diff);

  unsigned char *buffer = malloc(size + total_size_diff);
  if (buffer == NULL) {
    ZF_LOGE("Failed to allocate buffer for insert data");
    exit(1);
  }

  temp = payload;
  const insert_data_t *prev = NULL;

  while (temp) {
    ZF_LOGV("insert data");
    if (temp->start_offset != 0 && prev == NULL) {
      memcpy(buffer + buffer_offset, data + data_offset,
             temp->start_offset - data_offset);

      buffer_offset += temp->start_offset - data_offset;
    }
    if (prev != NULL) {
      memcpy(buffer + buffer_offset, data + data_offset,
             temp->start_offset - prev->end_offset - 1);

      buffer_offset += temp->start_offset - prev->end_offset - 1;
    }
    memcpy(buffer + buffer_offset, temp->data_insert, temp->data_size);

    buffer_offset += temp->data_size;
    data_offset = temp->end_offset + 1;

    prev = temp;
    temp = temp->next;
  }

  if (data_offset < size) {
    memcpy(buffer + buffer_offset, data + data_offset, size - data_offset);
  }

  *out_size = size + total_size_diff;
  return buffer;
}

int load_chunk_data(const char *region_path, size_t header_offset,
                    chunk_location_t *out_loc, size_t *out_size,
                    unsigned char **out_data) {
  FILE *fRegion;
  // Open a file in read mode
  fRegion = fopen(region_path, "rb");
  ZF_LOGV("REGION_PATH = %s", region_path);
  ZF_LOGV("HEADER_OFFSET = %zu", header_offset);

  chunk_location_raw_t chunk_loc_raw;
  insert_data_t *edit_list = NULL;

  fseek(fRegion, header_offset, SEEK_SET);
  if (fread(&chunk_loc_raw, sizeof(chunk_loc_raw), 1, fRegion) != 1) {
    ZF_LOGE("Error reading");
    exit(1);
  }
  create_chunk_location(&chunk_loc_raw, out_loc);
  // Chunk is probably not generated
  if (out_loc->offset == 0)
    return 1;

  fseek(fRegion, out_loc->offset * SECTOR_SIZE, SEEK_SET);

  int comp_data_size = 0;
  int comp_type = 0;
  if (fread(&comp_data_size, sizeof(comp_data_size), 1, fRegion) != 1) {
    ZF_LOGE("Error reading payload len");
    exit(1);
  }
  // -1 accounting for compression type byte
  comp_data_size = ntohl(comp_data_size) - 1;
  ZF_LOGV("read size: %d", comp_data_size + 1);

  if (fread(&comp_type, 1, 1, fRegion) != 1) {
    ZF_LOGE("Error reading compression type");
    exit(1);
  }
  if (comp_type != 2) {
    ZF_LOGE("Unhandled compression type = %d", comp_type);
    exit(1);
  }

  unsigned char *comp_data = malloc(comp_data_size);
  if (fread(comp_data, comp_data_size, 1, fRegion) != 1) {
    ZF_LOGE("Error reading payload content");
    exit(1);
  }
  fclose(fRegion);

  size_t uncomp_size = 0;
  unsigned char *uncomp_data =
      decompress_zlib(comp_data, comp_data_size, &uncomp_size);

  if (!uncomp_data) {
    ZF_LOGE("Error uncompressing data\n");
    exit(1);
  }

  *out_data = uncomp_data;
  *out_size = uncomp_size;

  free(comp_data);
  return 0;
}

void write_chunk_data(const char *region_path,
                      const chunk_location_t *chunk_loc,
                      const insert_data_t *edit_list, const unsigned char *data,
                      size_t data_size) {
  size_t new_uncomp_size = 0;
  unsigned char *new_uncomp_data =
      insert_data(data, data_size, edit_list, &new_uncomp_size);

  ZF_LOGV("Uncompressed data size: %lu", new_uncomp_size);

  size_t new_comp_size = 0;
  unsigned char *new_comp_data =
      compress_zlib(new_uncomp_data, new_uncomp_size, &new_comp_size);

  ZF_LOGV("Compressed data size: %lu", new_comp_size);

  // Calculate the new size padded to the nearest multiple of 4KiB
  size_t padded_size = ((new_comp_size + 5 + 4095) / 4096) * 4096;

  // Resize the file to accommodate the new chunk size
  struct stat st;
  if (stat(region_path, &st) != 0) {
    ZF_LOGE("Failed to stat region file");
    exit(1);
  }

  FILE *fRegion = fopen(region_path, "r+b");
  if (fRegion == NULL) {
    ZF_LOGE("Failed to open region file");
    exit(1);
  }

  fseek(fRegion, 0, SEEK_END);
  size_t file_size = ftell(fRegion);
  size_t old_chunk_end =
      chunk_loc->offset * SECTOR_SIZE + chunk_loc->sector_count * SECTOR_SIZE;
  size_t new_chunk_end = chunk_loc->offset * SECTOR_SIZE + padded_size;

  ZF_LOGV("chunk offset = %d", chunk_loc->offset);

  ZF_LOGV("Old chunk end: %lu, New chunk end: %lu", old_chunk_end,
          new_chunk_end);

  if (new_chunk_end > file_size) {
    // Extend the file size if the new chunk end exceeds the current file size
    ZF_LOGV("Extending file size to %lu", new_chunk_end);
    if (ftruncate(fileno(fRegion), new_chunk_end) != 0) {
      ZF_LOGE("Failed to resize region file");
      fclose(fRegion);
      exit(1);
    }
  } else if (new_chunk_end > old_chunk_end) {
    // Move data after the chunk to make space for the new chunk size
    ZF_LOGV("Moving data to make space for new chunk size");
    size_t move_size = file_size - old_chunk_end;
    unsigned char *buffer = malloc(move_size);
    fseek(fRegion, old_chunk_end, SEEK_SET);
    fread(buffer, 1, move_size, fRegion);
    fseek(fRegion, new_chunk_end, SEEK_SET);
    fwrite(buffer, 1, move_size, fRegion);
    free(buffer);
  } else if (new_chunk_end < old_chunk_end) {
    // Move data after the chunk to shrink the space for the new chunk size
    ZF_LOGV("Shrinking space for new chunk size");
    size_t move_size = file_size - old_chunk_end;
    unsigned char *buffer = malloc(move_size);
    fseek(fRegion, old_chunk_end, SEEK_SET);
    fread(buffer, 1, move_size, fRegion);
    fseek(fRegion, new_chunk_end, SEEK_SET);
    fwrite(buffer, 1, move_size, fRegion);
    free(buffer);
    if (ftruncate(fileno(fRegion),
                  file_size - (old_chunk_end - new_chunk_end)) != 0) {
      ZF_LOGE("Failed to resize region file");
      fclose(fRegion);
      exit(1);
    }
  }

  // Write the new chunk data
  fseek(fRegion, chunk_loc->offset * SECTOR_SIZE, SEEK_SET);
  int new_size_be = htonl(new_comp_size + 1);
  fwrite(&new_size_be, 4, 1, fRegion);

  // Move over compression byte
  fseek(fRegion, 1, SEEK_CUR);

  fwrite(new_comp_data, new_comp_size, 1, fRegion);

  // Not sure if it causes to break saves
  // Pad the remaining space with zeros
  /* size_t padding_size = padded_size - (new_comp_size + 5); */
  /* unsigned char *padding = calloc(1, padding_size); */
  /* fwrite(padding, padding_size, 1, fRegion); */
  /* free(padding); */

  // Update the sector count in the chunk location
  int new_sector_count = padded_size / SECTOR_SIZE;
  ZF_LOGV("Sector count %d -> %d", chunk_loc->sector_count, new_sector_count);

  if (new_sector_count != chunk_loc->sector_count) {
    // Update the region file header
    fseek(fRegion, 0, SEEK_SET);
    for (int i = 0; i < 1024; i++) {
      // 1024 chunks in header - each 4 bytes
      size_t header_pos = i * sizeof(chunk_location_raw_t);
      fseek(fRegion, header_pos, SEEK_SET);

      chunk_location_raw_t chunk_loc_raw;
      if (fread(&chunk_loc_raw, sizeof(chunk_loc_raw), 1, fRegion) != 1) {
        ZF_LOGE("Error reading header at index %d", i);
        break;
      }

      chunk_location_t temp_chunk_loc;
      create_chunk_location(&chunk_loc_raw, &temp_chunk_loc);

      // Skip empty headers
      if (temp_chunk_loc.offset == 0) {
        continue;
      }

      if (temp_chunk_loc.offset == chunk_loc->offset) {
        // Update the sector count for the matching header
        chunk_loc_raw.sector_count = new_sector_count;
        fseek(fRegion, header_pos, SEEK_SET);
        fwrite(&chunk_loc_raw, sizeof(chunk_loc_raw), 1, fRegion);
      } else if (temp_chunk_loc.offset > chunk_loc->offset) {
        // Update offsets for chunks after the modified one
        temp_chunk_loc.offset += new_sector_count - chunk_loc->sector_count;
        chunk_loc_raw.offset[0] = (temp_chunk_loc.offset >> 16) & 0xFF;
        chunk_loc_raw.offset[1] = (temp_chunk_loc.offset >> 8) & 0xFF;
        chunk_loc_raw.offset[2] = temp_chunk_loc.offset & 0xFF;
        fseek(fRegion, header_pos, SEEK_SET);
        fwrite(&chunk_loc_raw, sizeof(chunk_loc_raw), 1, fRegion);
      }
    }
  }

  fclose(fRegion);
  free(new_uncomp_data);
  free(new_comp_data);
}
