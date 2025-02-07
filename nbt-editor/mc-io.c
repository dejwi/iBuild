#include "mc-io.h"
#include "zf_log.h"
#include "zlib-utils.h"
#include <stdio.h>
#include <string.h>

void create_chunk_location(const chunk_location_raw_t *from,
                           chunk_location_t *to) {
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

  ZF_LOGV("%lu == %lu", data_size, new_uncomp_size);

  size_t new_comp_size = 0;
  unsigned char *new_comp_data =
      compress_zlib(new_uncomp_data, new_uncomp_size, &new_comp_size);

  /* printf("after comp %d == %lu\n", comp_data_size, new_comp_size); */
  FILE *fRegion = fopen(region_path, "r+b");
  fseek(fRegion, chunk_loc->offset * SECTOR_SIZE, SEEK_SET);

  ZF_LOGV("newsize: %zu", new_comp_size);
  // account for compression byte
  int new_size = htonl(new_comp_size + 1);
  fwrite(&new_size, 4, 1, fRegion);

  // move over compression type byte
  fseek(fRegion, 1, SEEK_CUR);

  fwrite(new_comp_data, new_comp_size, 1, fRegion);
  fclose(fRegion);

  free(new_uncomp_data);
  free(new_comp_data);
}
