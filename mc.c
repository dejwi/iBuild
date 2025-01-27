#include "mc.h"
#include "zlib-utils.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// Definitions of nbt tag constants
#define NEW_NBT(name, type, size) const nbt_helper_t name = {type, size, #name};

NEW_NBT(TAG_END, 0, -1)
NEW_NBT(TAG_BYTE, 1, 1)
NEW_NBT(TAG_SHORT, 2, 2)
NEW_NBT(TAG_INT, 3, 4)
NEW_NBT(TAG_LONG, 4, 8)
NEW_NBT(TAG_FLOAT, 5, 4)
NEW_NBT(TAG_DOUBLE, 6, 8)
NEW_NBT(TAG_BYTE_ARRAY, 7, -1)
NEW_NBT(TAG_STRING, 8, -1)
NEW_NBT(TAG_LIST, 9, -1)
NEW_NBT(TAG_COMPOUND, 10, -1)
NEW_NBT(TAG_INT_ARRAY, 11, -1)
NEW_NBT(TAG_LONG_ARRAY, 12, -1)

#undef NEW_NBT

static const nbt_helper_t *all_tags[] = {
    &TAG_END,      &TAG_BYTE,      &TAG_SHORT,      &TAG_INT,    &TAG_LONG,
    &TAG_FLOAT,    &TAG_DOUBLE,    &TAG_BYTE_ARRAY, &TAG_STRING, &TAG_LIST,
    &TAG_COMPOUND, &TAG_INT_ARRAY, &TAG_LONG_ARRAY};

const nbt_helper_t *get_helper_tag(uint8_t id) {
  for (int i = 0; i < sizeof(all_tags) / sizeof(all_tags[0]); i++) {
    if (all_tags[i]->id == id) {
      return all_tags[i];
    }
  }
  return NULL;
}

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

unsigned short get_ushort_le(const unsigned char *data) {
  return data[0] << 8 | data[1];
}

unsigned short ushort_to_be(unsigned short value) {
  return (value << 8) | (value >> 8);
}

int get_int_le(const unsigned char *data) {
  char *data_s = (char *)data;
  return data_s[0] << 24 | data_s[1] << 16 | data_s[2] << 8 | data_s[3];
}

size_t resolve_tag_end_offset(const unsigned char *data,
                              const nbt_helper_t *start_from_tag) {
  size_t offset = 0;
  const nbt_helper_t *tag = start_from_tag;

  if (start_from_tag == NULL) {
    tag = get_helper_tag(data[offset]);
    if (tag == NULL) {
      fprintf(stderr, "Tag not identified resolve-tag id: %d\n", data[offset]);
      exit(1);
    }
    offset++;

    unsigned short name_len = get_ushort_le(data + offset);
    offset += 2;

    /* char *name = malloc(name_len + 1); */
    /* memcpy(name, data + offset, name_len); */
    /* name[name_len] = '\0'; */
    /* printf("Resolve offset: %s\n", name); */
    /* free(name); */
    offset += name_len;
  }

  if (tag->payload_size != -1) {
    offset += tag->payload_size;
    return offset;
  }

  if (tag->id == TAG_BYTE_ARRAY.id) {
    int array_len = get_int_le(data + offset);
    offset += 4 + array_len;
  } else if (tag->id == TAG_STRING.id) {
    int len = get_ushort_le(data + offset);
    offset += 2 + len;
  } else if (tag->id == TAG_LIST.id) {
    const nbt_helper_t *list_el_tag = get_helper_tag(data[offset]);
    if (list_el_tag == NULL) {
      fprintf(stderr, "Tag not identified list-el id: %d", data[offset]);
      exit(1);
    }
    offset += 1;

    int list_el_count = get_int_le(data + offset);
    offset += 4;
    /* printf("List of elements: %s, of len: %d\n", list_el_tag->name, */
    /*        list_el_count); */
    if (list_el_tag->payload_size != -1) {
      offset += list_el_count * list_el_tag->payload_size;
    } else {
      for (int i = 0; i < list_el_count; i++) {
        offset += resolve_tag_end_offset(data + offset, list_el_tag);
      }
    }
  } else if (tag->id == TAG_COMPOUND.id) {
    while (data[offset] != TAG_END.id) {
      offset += resolve_tag_end_offset(data + offset, NULL);
    }
    offset++;
  } else if (tag->id == TAG_INT_ARRAY.id) {
    int el_count = get_int_le(data + offset);
    offset += 4 + el_count * TAG_INT.payload_size;
  } else if (tag->id == TAG_LONG_ARRAY.id) {
    int el_count = get_int_le(data + offset);
    offset += 4 + el_count * TAG_LONG.payload_size;
  } else if (tag->id == TAG_END.id) {
    offset++;
  } else {
    fprintf(stderr,
            "Somehow resolve end tag offset didn't match element, offset: %zu",
            offset);
    exit(1);
  }

  return offset;
}
unsigned char *find_data_tag_comp(const char *search_name,
                                  const unsigned char *data) {
  size_t offset = 0;

  for (;;) {
    const nbt_helper_t *tag = get_helper_tag(data[offset]);
    if (tag == NULL) {
      fprintf(stderr, "Tag not identified find-data id: %d", data[offset]);
      exit(1);
    }
    if (tag->id == TAG_END.id)
      return NULL;

    offset++;

    unsigned short name_len = get_ushort_le(data + offset);
    offset += 2;

    char *name = malloc(name_len + 1);
    memcpy(name, data + offset, name_len);
    name[name_len] = '\0';
    offset += name_len;

    // debug
    /* printf("searching at %s;nlen: %d;\n%s==%s\n", tag->name, name_len, */
    /*        search_name, name); */

    int cmp_result = strcmp(name, search_name);
    free(name);
    if (cmp_result == 0) {
      return (unsigned char *)(data + offset);
    }

    offset += resolve_tag_end_offset(data + offset, tag);
  }
  return NULL;
}

size_t print_comp_fields(const unsigned char *data, int nest_depth) {
  size_t offset = 0;

  while (data[offset] != TAG_END.id) {
    const nbt_helper_t *tag = get_helper_tag(data[offset]);
    if (tag == NULL) {
      fprintf(stderr, "Tag not identified print-comp id: %d", data[offset]);
      exit(1);
    }
    offset++;

    unsigned short name_len = get_ushort_le(data + offset);
    offset += 2;

    char *name = malloc(name_len + 1);
    memcpy(name, data + offset, name_len);
    name[name_len] = '\0';
    offset += name_len;

    if (tag->id == TAG_COMPOUND.id && nest_depth > 0) {
      printf("%s = COMPOUND START\n", name);
      offset += print_comp_fields(data + offset, nest_depth - 1);
      printf("%s = COMPOUND END\n", name);
    } else if (tag->id == TAG_STRING.id) {
      unsigned short tag_val_len = get_ushort_le(data + offset);
      offset += 2;

      char *tag_val = malloc(tag_val_len + 1);
      memcpy(tag_val, data + offset, tag_val_len);
      tag_val[tag_val_len] = '\0';
      printf("%s = '%s'\n", name, tag_val);

      free(tag_val);

      offset += tag_val_len;
    } else {
      printf("%s = %s\n", name, tag->name);
      offset += resolve_tag_end_offset(data + offset, tag);
    }

    free(name);
  }

  return offset + 1;
}

unsigned char *insert_data(const unsigned char *data, size_t size,
                           insert_data_t *payload, size_t *out_size) {
  int total_size_diff = 0;
  size_t data_offset = 0;
  size_t buffer_offset = 0;

  insert_data_t *temp = payload;
  while (temp) {
    total_size_diff +=
        temp->start_offset - temp->end_offset + temp->data_size - 1;

    temp = temp->next;
  }
  printf("size_diff_insert: %d\n", total_size_diff);

  unsigned char *buffer = malloc(size + total_size_diff);
  if (buffer == NULL) {
    fprintf(stderr, "Failed to allocate buffer for insert data\n");
    exit(1);
  }

  temp = payload;
  insert_data_t *prev = NULL;

  while (temp) {
    if (temp->start_offset != 0) {
      memcpy(buffer + buffer_offset, data + data_offset,
             temp->start_offset - data_offset);

      buffer_offset += temp->start_offset - data_offset;
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

void load_chunk_data(const char *region_path, size_t header_offset,
                     chunk_location_t *out_loc, size_t *out_size,
                     unsigned char **out_data) {
  FILE *fRegion;
  // Open a file in read mode
  fRegion = fopen(region_path, "rb");

  chunk_location_raw_t chunk_loc_raw;
  insert_data_t *edit_list = NULL;

  fseek(fRegion, header_offset, SEEK_SET);
  if (fread(&chunk_loc_raw, sizeof(chunk_loc_raw), 1, fRegion) != 1) {
    fprintf(stderr, "Error reading\n");
    exit(1);
  }
  create_chunk_location(&chunk_loc_raw, out_loc);

  fseek(fRegion, out_loc->offset * SECTOR_SIZE, SEEK_SET);

  int comp_data_size = 0;
  int comp_type = 0;
  if (fread(&comp_data_size, sizeof(comp_data_size), 1, fRegion) != 1) {
    fprintf(stderr, "Error reading payload len");
    exit(1);
  }
  // -1 accounting for compression type byte
  comp_data_size = ntohl(comp_data_size) - 1;
  printf("read size: %d\n", comp_data_size + 1);

  if (fread(&comp_type, 1, 1, fRegion) != 1) {
    fprintf(stderr, "Error reading compression type");
    exit(1);
  }
  if (comp_type != 2) {
    fprintf(stderr, "Unhandled compression type = %d", comp_type);
    exit(1);
  }

  unsigned char *comp_data = malloc(comp_data_size);
  if (fread(comp_data, comp_data_size, 1, fRegion) != 1) {
    fprintf(stderr, "Error reading payload content");
    exit(1);
  }
  fclose(fRegion);

  size_t uncomp_size = 0;
  unsigned char *uncomp_data =
      decompress_zlib(comp_data, comp_data_size, &uncomp_size);

  if (!uncomp_data) {
    fprintf(stderr, "Error uncompressing data\n");
    exit(1);
  }

  *out_data = uncomp_data;
  *out_size = uncomp_size;

  free(comp_data);
}

void write_chunk_data(const char *region_path, chunk_location_t *chunk_loc,
                      insert_data_t *edit_list, const unsigned char *data,
                      size_t data_size) {

  if (edit_list == NULL)
    return;

  size_t new_uncomp_size = 0;
  unsigned char *new_uncomp_data =
      insert_data(data, data_size, edit_list, &new_uncomp_size);

  printf("%lu == %lu\n", data_size, new_uncomp_size);

  size_t new_comp_size = 0;
  unsigned char *new_comp_data =
      compress_zlib(new_uncomp_data, new_uncomp_size, &new_comp_size);

  /* printf("after comp %d == %lu\n", comp_data_size, new_comp_size); */
  FILE *fRegion = fopen(region_path, "r+b");
  fseek(fRegion, chunk_loc->offset * SECTOR_SIZE, SEEK_SET);

  // account for compression byte
  int new_size = htonl(new_comp_size + 1);
  printf("newsize: %d\n", new_size);
  fwrite(&new_size, 4, 1, fRegion);

  // move over compression type byte
  fseek(fRegion, 1, SEEK_CUR);

  fwrite(new_comp_data, new_comp_size, 1, fRegion);
  fclose(fRegion);

  free(new_uncomp_data);
  free(new_comp_data);
}

void test_chunk_edit(const char *region_path, int x_chunk, int z_chunk,
                     int y_section, char *block_from, char *block_to) {
  size_t chunk_header_offset = 4 * ((x_chunk & 31) + (z_chunk & 31) * 32);
  chunk_location_t chunk_loc;
  insert_data_t *edit_list = NULL;

  unsigned char *uncomp_data;
  size_t uncomp_size = 0;

  load_chunk_data(region_path, chunk_header_offset, &chunk_loc, &uncomp_size,
                  &uncomp_data);

  unsigned char *sections = find_data_tag_comp("sections", uncomp_data + 3);
  assert(sections != NULL);

  // First byte is list element type but we know that it's compound
  int s_offset = 1;
  int s_len = get_int_le(sections + s_offset);
  s_offset += 4;

  // iterate over section items (Y sections)
  for (size_t i = 0; i < s_len; i++) {
    size_t el_end_offset =
        resolve_tag_end_offset(sections + s_offset, &TAG_COMPOUND);

    unsigned char *y_pos = find_data_tag_comp("Y", sections + s_offset);
    assert(y_pos != NULL);

    if (((char *)y_pos)[0] == y_section) {
      unsigned char *block_states =
          find_data_tag_comp("block_states", sections + s_offset);
      if (block_states != NULL) {
        unsigned char *palette = find_data_tag_comp("palette", block_states);
        assert(palette != NULL);

        int palette_len = get_int_le(palette + 1);

        size_t palette_item_offset = 5;
        for (size_t j = 0; j < palette_len; j++) {
          /* palette_item_offset += */
          /*     print_comp_fields(palette + palette_item_offset, 1); */
          unsigned char *name_el =
              find_data_tag_comp("Name", palette + palette_item_offset);
          assert(name_el != NULL);

          unsigned short name_len = get_ushort_le(name_el);
          char *name = malloc(name_len + 1);
          memcpy(name, name_el + 2, name_len);
          name[name_len] = '\0';
          if (strcmp(name, block_from) == 0) {
            // Name offset - includes first 2 bytes for length
            size_t name_offset = name_el - uncomp_data;
            printf("Found block at %lu\n", name_offset);

            if (edit_list != NULL) {
              // temp
              free(edit_list->data_insert);
              free(edit_list);
            }
            edit_list = malloc(sizeof(insert_data_t));
            edit_list->next = NULL;
            edit_list->start_offset = name_offset;
            edit_list->end_offset = name_offset + name_len - 1 + 2;

            unsigned short block_to_len = strlen(block_to);
            edit_list->data_size = block_to_len + 2;
            edit_list->data_insert = malloc(edit_list->data_size);

            unsigned short block_to_len_be = htons(block_to_len);
            memcpy(edit_list->data_insert, &block_to_len_be, 2);
            memcpy(edit_list->data_insert + 2, block_to, block_to_len);
          }
          free(name);

          palette_item_offset += resolve_tag_end_offset(
              palette + palette_item_offset, &TAG_COMPOUND);
        }
      }
    }

    s_offset += el_end_offset;
  }

  write_chunk_data(region_path, &chunk_loc, edit_list, uncomp_data,
                   uncomp_size);

  free(uncomp_data);

  // add free edit list
}
