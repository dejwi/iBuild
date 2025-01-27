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
