#include "mc.h"
#include "zlib-utils.h"
#include <assert.h>
#include <math.h>
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

int get_chunk_offset_in_header(int x, int z) {
  return 4 * ((x & 31) + (z & 31) * 32);
}

unsigned short get_ushort_le(const unsigned char *data) {
  return data[0] << 8 | data[1];
}

unsigned short ushort_to_be(unsigned short value) {
  return (value << 8) | (value >> 8);
}

void *reverse_endian(void *p, size_t size) {
  char *head = (char *)p;
  char *tail = head + size - 1;

  for (; tail > head; --tail, ++head) {
    char temp = *head;
    *head = *tail;
    *tail = temp;
  }
  return p;
}

int get_int_le(const unsigned char *data) {
  char *data_s = (char *)data;
  return data_s[0] << 24 | data_s[1] << 16 | data_s[2] << 8 | data_s[3];
}

int count_min_bits(int n) {
  int count = 0, i;
  if (n == 0)
    return 0;
  for (i = 0; i < 32; i++) {
    if ((1 << i) & n)
      count = i;
  }

  return ++count;
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
    printf("insert data\n");
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

  /* if (edit_list == NULL) */
  /*   return; */

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

  printf("newsize: %zu\n", new_comp_size);
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

void test_chunk_edit(const char *region_path, int x_chunk, int z_chunk,
                     const block_build_t *build) {
  size_t chunk_header_offset = 4 * ((x_chunk & 31) + (z_chunk & 31) * 32);
  chunk_location_t chunk_loc;
  insert_data_t *edit_head = NULL;
  insert_data_t *edit_tail = NULL;

  int *mapped_palette_idxs = malloc(build->palette_len);
  // Fill with empty
  for (int i = 0; i < build->palette_len; i++) {
    mapped_palette_idxs[i] = -1;
  }

  unsigned char *uncomp_data;
  size_t uncomp_size = 0;
  int min_y_sc = build->chunk_pos.y / 16;
  int max_y_sc = (build->y_size + build->chunk_pos.y) / 16;
  // temporary as different sections are not impl
  assert(min_y_sc == max_y_sc);

  int y_section = max_y_sc;
  printf("ysec: %d\n", y_section);

  load_chunk_data(region_path, chunk_header_offset, &chunk_loc, &uncomp_size,
                  &uncomp_data);

  unsigned char *sections = find_data_tag_comp("sections", uncomp_data + 3);
  assert(sections != NULL);
  printf("tes");

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
        printf("palette len: %d\n", palette_len);

        size_t palette_item_offset = 5;
        for (size_t j = 0; j < palette_len; j++) {
          printf("pitem %zu\n", j);
          print_comp_fields(palette + palette_item_offset, 1);

          unsigned char *name_el =
              find_data_tag_comp("Name", palette + palette_item_offset);
          assert(name_el != NULL);

          unsigned short name_len = get_ushort_le(name_el);
          char *name = malloc(name_len + 1);
          memcpy(name, name_el + 2, name_len);
          name[name_len] = '\0';

          for (int k = 0; k < build->palette_len; k++) {
            if (strcmp(name, build->palette[k]) == 0) {
              mapped_palette_idxs[k] = j;
              break;
            }
          }
          free(name);

          palette_item_offset += resolve_tag_end_offset(
              palette + palette_item_offset, &TAG_COMPOUND);
        }

        char **items_to_add = malloc(build->palette_len * sizeof(char *));
        int items_to_add_size = 0;

        for (int j = 0; j < build->palette_len; j++) {
          if (mapped_palette_idxs[j] == -1) {
            items_to_add[items_to_add_size] = build->palette[j];
            items_to_add_size++;
          }
        }

        if (items_to_add_size > 0) {
          printf("Palette needs updating\n");
          int new_palette_len = palette_len + items_to_add_size;

          // resizing not impl
          int bits_old = count_min_bits(palette_len - 1);
          int bits_new = count_min_bits(new_palette_len - 1);
          if (bits_old < 4)
            bits_old = 4;
          if (bits_new < 4)
            bits_new = 4;
          printf("%d . %d\n", bits_old, bits_new);
          printf("%d . %d\n", palette_len - 1, new_palette_len - 1);
          assert(bits_new == bits_old);

          int new_palette_len_be = htonl(new_palette_len);
          printf("newlen: %d, test: %d\n", new_palette_len,
                 get_int_le(&new_palette_len_be));

          insert_data_t *edit_len = malloc(sizeof(insert_data_t));

          edit_len->next = NULL;
          edit_len->data_size = TAG_INT.payload_size;
          edit_len->data_insert = malloc(TAG_INT.payload_size);
          edit_len->start_offset = (palette + 1) - uncomp_data;
          edit_len->end_offset =
              edit_len->start_offset + TAG_INT.payload_size - 1;

          memcpy(edit_len->data_insert, &new_palette_len_be,
                 TAG_INT.payload_size);
          if (edit_tail == NULL) {
            edit_head = edit_len;
            edit_tail = edit_len;
          } else {
            edit_tail->next = edit_len;
            edit_tail = edit_len;
          }

          int total_str_len = 0;
          for (int j = 0; j < items_to_add_size; j++) {
            total_str_len += strlen(items_to_add[j]);
          }
          printf("taotal str len: %d\n", total_str_len);

          char *tag_name = "Name";
          unsigned short tag_name_len = strlen(tag_name);
          unsigned short tag_name_len_be = htons(tag_name_len);

          insert_data_t *edit_items = malloc(sizeof(insert_data_t));
          edit_items->next = NULL;
          edit_items->data_size =
              1 + items_to_add_size * (1 + 2 + 2 + tag_name_len + 1) +
              total_str_len;
          edit_items->data_insert = malloc(edit_items->data_size);
          edit_items->start_offset =
              (palette + palette_item_offset - 1) - uncomp_data;
          edit_items->end_offset = edit_items->start_offset;

          // copy last byte to keep it - for now edit will always overwrite min
          // 1 byte
          memcpy(edit_items->data_insert, palette + palette_item_offset - 1, 1);

          int edit_items_offset = 1;
          for (int j = 0; j < items_to_add_size; j++) {
            memcpy(edit_items->data_insert + edit_items_offset, &TAG_STRING.id,
                   1);
            edit_items_offset++;

            // Add "Name" tag
            memcpy(edit_items->data_insert + edit_items_offset,
                   &tag_name_len_be, 2);
            edit_items_offset += 2;
            memcpy(edit_items->data_insert + edit_items_offset, tag_name,
                   tag_name_len);
            edit_items_offset += tag_name_len;

            unsigned short name_len = strlen(items_to_add[j]);
            unsigned short name_len_be = htons(name_len);

            // Add new block
            memcpy(edit_items->data_insert + edit_items_offset, &name_len_be,
                   2);
            edit_items_offset += 2;
            memcpy(edit_items->data_insert + edit_items_offset, items_to_add[j],
                   name_len);
            edit_items_offset += name_len;

            int empty = 0;
            // Tag end byte
            memcpy(edit_items->data_insert + edit_items_offset, &empty, 1);
            edit_items_offset++;

            for (int k = 0; k < build->palette_len; k++) {
              if (strcmp(items_to_add[j], build->palette[k]) == 0)
                mapped_palette_idxs[k] = palette_len + j;
            }
          }
          printf("addite %zu == %d\n", edit_items->data_size,
                 edit_items_offset);

          edit_tail->next = edit_items;
          edit_tail = edit_items;
          /* edit_head = edit_items; */
        }

        // Free items to add
        /* for (int i = 0; i < items_to_add_size; i++) */
        /*   free(items_to_add[i]); */
        free(items_to_add);

        // Should be all filled by now
        for (int i = 0; i < build->palette_len; i++)
          assert(mapped_palette_idxs[i] != -1);

        // TODO: when theres only one block in a chunk theres no indices field
        unsigned char *indicies_data = find_data_tag_comp("data", block_states);
        assert(indicies_data != NULL);

        int indicies_len = get_int_le(indicies_data);
        printf("indicies len: %d\n", indicies_len);

        int bits = count_min_bits(palette_len - 1);
        if (bits < 4)
          bits = 4;

        for (int y = 0; y < build->y_size; y++) {
          for (int z = 0; z < build->z_size; z++) {
            for (int x = 0; x < build->x_size; x++) {
              int build_pos =
                  y * build->x_size * build->z_size + z * build->x_size + x;
              int indc = build->indices[build_pos];
              // Marked as empty
              if (indc == -1)
                continue;
              int64_t build_block = mapped_palette_idxs[indc];

              int ch_y = y + build->chunk_pos.y % 16;
              int ch_z = z + build->chunk_pos.z;
              int ch_x = x + build->chunk_pos.x;

              int ch_pos = ch_y * 16 * 16 + ch_z * 16 + ch_x;

              printf("Build block: %lld, at: %d\n", build_block, ch_pos);

              int idx = ch_pos / (64 / bits);
              int64_t block = 0;

              memcpy(&block, indicies_data + 4 + idx * 8, 8);
              block = *(int64_t *)reverse_endian(&block, 8);

              int bit_offset = (ch_pos % (64 / bits) * bits);
              int64_t powof = pow(2, bits) - 1;

              int64_t bit_mask = powof << bit_offset;
              /* int64_t new_val = 2; */
              block = (block & (~bit_mask)) | (build_block << bit_offset);

              block = *(int64_t *)reverse_endian(&block, 8);
              memcpy(indicies_data + 4 + idx * 8, &block, 8);
            }
          }
        }

        /* printf("index size: %d\n", bits); */
        /* for (int i = 0; i < 1; i++) { */
        /*   int idx = i / (64 / bits); */
        /*   int64_t block = 0; */
        /*   memcpy(&block, indicies_data + 4 + idx * 8, 8); */
        /*   block = *(int64_t *)reverse_endian(&block, 8); */
        /**/
        /*   int bit_offset = (i % (64 / bits) * bits); */
        /*   int64_t powof = pow(2, bits) - 1; */
        /**/
        /*   int64_t bit_mask = powof << bit_offset; */
        /*   int64_t new_val = 2; */
        /*   block = (block & (~bit_mask)) | (new_val << bit_offset); */
        /**/
        /*   block = *(int64_t *)reverse_endian(&block, 8); */
        /*   memcpy(indicies_data + 4 + idx * 8, &block, 8); */
        /* } */
      }
    }

    s_offset += el_end_offset;
  }

  write_chunk_data(region_path, &chunk_loc, edit_head, uncomp_data,
                   uncomp_size);

  free(uncomp_data);
  free(mapped_palette_idxs);

  // add free edit list
}
