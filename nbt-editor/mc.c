#include "mc.h"
#include "mc-io.h"
#include "nbt.h"
#include "nums.h"
#include "zlib-utils.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

insert_data_t *
handle_palette(const unsigned char *block_states, const block_build_t *build,
               const unsigned char *uncomp_data, int *out_mapped_palette_idxs,
               int *out_palette_len, size_t *out_palette_end_offset) {
  insert_data_t *head = NULL;
  insert_data_t *tail = NULL;
  // Fill with empty
  for (int i = 0; i < build->palette_len; i++) {
    out_mapped_palette_idxs[i] = -1;
  }
  unsigned char *palette = find_data_tag_comp("palette", block_states);
  assert(palette != NULL);

  printf("test\n");
  int palette_len = get_int_le(palette + 1);
  *out_palette_len = palette_len;
  printf("palette len: %d\n", palette_len);

  // +1 for LIST element type and +4 for LIST length
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

    // Fill in already existing blocks
    for (int k = 0; k < build->palette_len; k++) {
      if (strcmp(name, build->palette[k]) == 0) {
        out_mapped_palette_idxs[k] = j;
        break;
      }
    }
    free(name);

    palette_item_offset +=
        resolve_tag_end_offset(palette + palette_item_offset, &TAG_COMPOUND);
  }
  *out_palette_end_offset = (palette + palette_item_offset) - uncomp_data;

  char **items_to_add = malloc(build->palette_len * sizeof(char *));
  int items_to_add_size = 0;

  // Check if there are blocks not already present in the palette
  for (int j = 0; j < build->palette_len; j++) {
    if (out_mapped_palette_idxs[j] == -1) {
      items_to_add[items_to_add_size] = build->palette[j];
      items_to_add_size++;
    }
  }

  if (items_to_add_size == 0) {
    free(items_to_add);
    return NULL;
  }

  printf("Palette needs updating\n");
  int new_palette_len = palette_len + items_to_add_size;
  int new_palette_len_be = htonl(new_palette_len);

  // resizing not impl
  int bits_old = count_min_bits(palette_len - 1);
  int bits_new = count_min_bits(new_palette_len - 1);
  if (bits_old < 4)
    bits_old = 4;
  if (bits_new < 4)
    bits_new = 4;
  assert(bits_new == bits_old);

  insert_data_t *edit_len = malloc(sizeof(insert_data_t));

  edit_len->next = NULL;
  edit_len->data_size = TAG_INT.payload_size;
  edit_len->data_insert = malloc(TAG_INT.payload_size);
  edit_len->start_offset = (palette + 1) - uncomp_data;
  edit_len->end_offset = edit_len->start_offset + TAG_INT.payload_size - 1;

  memcpy(edit_len->data_insert, &new_palette_len_be, TAG_INT.payload_size);
  append_insert_list(&head, &tail, edit_len);

  int total_str_len = 0;
  for (int j = 0; j < items_to_add_size; j++) {
    total_str_len += strlen(items_to_add[j]);
  }

  char *tag_name = "Name";
  unsigned short tag_name_len = strlen(tag_name);
  unsigned short tag_name_len_be = htons(tag_name_len);

  insert_data_t *edit_items = malloc(sizeof(insert_data_t));
  edit_items->next = NULL;
  // 1 - copied first - items * (tag_id, tag_name_len, tag_name, name_len,
  // tag_end)
  edit_items->data_size =
      1 + items_to_add_size * (1 + 2 + tag_name_len + 2 + 1) + total_str_len;
  edit_items->data_insert = malloc(edit_items->data_size);
  edit_items->start_offset = (palette + palette_item_offset - 1) - uncomp_data;
  edit_items->end_offset = edit_items->start_offset;

  // copy last byte to keep it - for now edit will always overwrite min
  // 1 byte
  memcpy(edit_items->data_insert, uncomp_data + edit_items->start_offset, 1);

  int edit_items_offset = 1;
  for (int j = 0; j < items_to_add_size; j++) {
    // TAG_ID
    memcpy(edit_items->data_insert + edit_items_offset, &TAG_STRING.id, 1);
    edit_items_offset++;

    // Add "Name" tag
    memcpy(edit_items->data_insert + edit_items_offset, &tag_name_len_be, 2);
    edit_items_offset += 2;
    memcpy(edit_items->data_insert + edit_items_offset, tag_name, tag_name_len);
    edit_items_offset += tag_name_len;

    unsigned short name_len = strlen(items_to_add[j]);
    unsigned short name_len_be = htons(name_len);

    // Add our new block
    memcpy(edit_items->data_insert + edit_items_offset, &name_len_be, 2);
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
        out_mapped_palette_idxs[k] = palette_len + j;
    }
  }

  append_insert_list(&head, &tail, edit_items);

  free(items_to_add);
  return head;
}

void test_chunk_edit(const char *region_path, int x_chunk, int z_chunk,
                     const block_build_t *build) {
  size_t chunk_header_offset = 4 * ((x_chunk & 31) + (z_chunk & 31) * 32);
  chunk_location_t chunk_loc;
  insert_data_t *edit_head = NULL;
  insert_data_t *edit_tail = NULL;

  unsigned char *uncomp_data;
  size_t uncomp_size = 0;
  int min_y_sc = build->pos.y / 16;
  int max_y_sc = (build->y_size + build->pos.y - 1) / 16;

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
    unsigned char *s_item = sections + s_offset;
    size_t el_end_offset = resolve_tag_end_offset(s_item, &TAG_COMPOUND);

    s_offset += el_end_offset;

    unsigned char *y_pos_el = find_data_tag_comp("Y", s_item);
    assert(y_pos_el != NULL);
    int y_pos = ((char *)y_pos_el)[0];

    if (y_pos < min_y_sc || y_pos > max_y_sc)
      continue;

    printf("Editing y_section:%d\n", y_pos);
    unsigned char *block_states = find_data_tag_comp("block_states", s_item);
    assert(block_states != NULL);

    // Handle palette changes
    int *mapped_palette_idxs = malloc(build->palette_len * sizeof(int));
    int palette_len = 0;
    size_t palette_end_offset = 0;

    insert_data_t *edit_palette =
        handle_palette(block_states, build, uncomp_data, mapped_palette_idxs,
                       &palette_len, &palette_end_offset);
    if (edit_palette != NULL)
      append_insert_list(&edit_head, &edit_tail, edit_palette);

    // Should be all filled by now
    for (int i = 0; i < build->palette_len; i++)
      assert(mapped_palette_idxs[i] != -1);

    int bits = count_min_bits(palette_len - 1);
    if (bits < 4)
      bits = 4;

    unsigned char *indices_el = find_data_tag_comp("data", block_states);
    if (indices_el == NULL) {
      printf("Creating indices\n");
      size_t block_states_end_offset =
          resolve_tag_end_offset(block_states, &TAG_COMPOUND);
      block_states_end_offset =
          (block_states + block_states_end_offset) - uncomp_data;

      char *tag_name = "data";
      unsigned short tag_name_len = strlen(tag_name);
      unsigned short tag_name_len_be = htons(tag_name_len);

      int len = 4096 / (64 / bits);
      int len_be = htonl(len);
      int total_indices_size =
          1 + 2 + tag_name_len + 4 + len * TAG_LONG.payload_size + 1;
      unsigned char *new_indices = calloc(1, total_indices_size);

      int indices_offset = 0;
      memcpy(new_indices, &TAG_LONG_ARRAY.id, 1);
      indices_offset++;

      memcpy(new_indices + indices_offset, &tag_name_len_be, 2);
      indices_offset += 2;
      memcpy(new_indices + indices_offset, tag_name, tag_name_len);
      indices_offset += tag_name_len;

      memcpy(new_indices + indices_offset, &len_be, 4);
      indices_offset += 4;
      indices_offset += len * TAG_LONG.payload_size;

      // copy over last byte from block_states at the end
      memcpy(new_indices + indices_offset,
             uncomp_data + block_states_end_offset - 1, 1);
      printf("last byte: %d\n", (uncomp_data + block_states_end_offset - 1)[0]);
      printf("last byte2: %d\n", (uncomp_data + block_states_end_offset)[0]);
      printf("%d == %d\n", total_indices_size, indices_offset + 1);

      insert_data_t *edit_indices = malloc(sizeof(insert_data_t));
      edit_indices->next = NULL;
      edit_indices->data_size = total_indices_size;
      edit_indices->data_insert = new_indices;
      edit_indices->start_offset = block_states_end_offset - 1;
      edit_indices->end_offset = edit_indices->start_offset;

      append_insert_list(&edit_head, &edit_tail, edit_indices);

      indices_el = new_indices + 1 + 2 + tag_name_len;
    }
    assert(indices_el != NULL);

    int indicies_len = get_int_le(indices_el);
    printf("indicies len: %d\n", indicies_len);

    int min_y = y_pos * 16;
    int max_y = ((y_pos + 1) * 16) - 1;

    int min_z = z_chunk * 16;
    int max_z = ((z_chunk + 1) * 16) - 1;

    int min_x = x_chunk * 16;
    int max_x = ((x_chunk + 1) * 16) - 1;

    for (int y = 0; y < build->y_size; y++) {
      for (int z = 0; z < build->z_size; z++) {
        for (int x = 0; x < build->x_size; x++) {
          int actual_y = y + build->pos.y;
          int actual_z = z + build->pos.z;
          int actual_x = x + build->pos.x;
          // Check if belongs to this section
          if (actual_y < min_y || actual_y > max_y)
            continue;
          if (actual_z < min_z || actual_z > max_z)
            continue;
          if (actual_x < min_x || actual_x > max_x)
            continue;

          int build_pos =
              y * build->x_size * build->z_size + z * build->x_size + x;
          int indc = build->indices[build_pos];
          // Marked as empty
          if (indc == -1)
            continue;
          int64_t build_block = mapped_palette_idxs[indc];

          int ch_y = actual_y % 16;
          int ch_z = actual_z % 16;
          int ch_x = actual_x % 16;

          int ch_pos = ch_y * 16 * 16 + ch_z * 16 + ch_x;

          printf("Build block: %lld, at: %d\n", build_block, ch_pos);

          int idx = ch_pos / (64 / bits);
          int64_t block = 0;

          memcpy(&block, indices_el + 4 + idx * 8, 8);
          block = *(int64_t *)reverse_endian(&block, 8);

          int bit_offset = (ch_pos % (64 / bits) * bits);
          int64_t powof = pow(2, bits) - 1;

          int64_t bit_mask = powof << bit_offset;
          /* int64_t new_val = 2; */
          block = (block & (~bit_mask)) | (build_block << bit_offset);

          block = *(int64_t *)reverse_endian(&block, 8);
          memcpy(indices_el + 4 + idx * 8, &block, 8);
        }
      }
    }
    free(mapped_palette_idxs);
  }

  write_chunk_data(region_path, &chunk_loc, edit_head, uncomp_data,
                   uncomp_size);

  free(uncomp_data);
  free_insert_list(edit_head);
}
