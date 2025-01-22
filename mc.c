#include "mc.h"
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

unsigned short get_ushort_le(unsigned char *data) {
  return data[0] << 8 | data[1];
}
int get_int_le(unsigned char *data) {
  return data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
}

size_t resolve_tag_end_offset(unsigned char *data,
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
    printf("List of elements: %s, of len: %d\n", list_el_tag->name,
           list_el_count);
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
unsigned char *find_data_by_tag_name(char *search_name, unsigned char *data,
                                     size_t data_len) {
  size_t offset = 0;

  while (offset < data_len) {
    const nbt_helper_t *tag = get_helper_tag(data[offset]);
    if (tag == NULL) {
      fprintf(stderr, "Tag not identified find-data id: %d", data[offset]);
      exit(1);
    }
    offset++;

    unsigned short name_len = get_ushort_le(data + offset);
    offset += 2;

    char *name = malloc(name_len + 1);
    memcpy(name, data + offset, name_len);
    name[name_len] = '\0';
    offset += name_len;

    // debug
    printf("searching at %s;nlen: %d;\n%s==%s\n", tag->name, name_len,
           search_name, name);

    int cmp_result = strcmp(name, search_name);
    free(name);
    if (cmp_result == 0) {
      return data + offset;
    }

    offset += resolve_tag_end_offset(data + offset, tag);
  }

  return NULL;
}
