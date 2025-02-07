#include "nbt.h"
#include "nums.h"
#include "zf_log.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

size_t resolve_tag_end_offset(const unsigned char *data,
                              const nbt_helper_t *start_from_tag) {
  size_t offset = 0;
  const nbt_helper_t *tag = start_from_tag;

  if (start_from_tag == NULL) {
    tag = get_helper_tag(data[offset]);
    if (tag == NULL) {
      ZF_LOGE("Tag not identified resolve-tag id: %d", data[offset]);
      exit(1);
    }
    offset++;

    unsigned short name_len = get_ushort_le(data + offset);
    offset += 2;

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
      ZF_LOGE("Tag not identified list-el id: %d", data[offset]);
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
    ZF_LOGE("Somehow resolve end tag offset didn't match element, offset: %zu",
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
      ZF_LOGE("Tag not identified find-data id: %d", data[offset]);
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
      ZF_LOGE("Tag not identified print-comp id: %d", data[offset]);
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
      ZF_LOGD("%s = COMPOUND START", name);
      offset += print_comp_fields(data + offset, nest_depth - 1);
      ZF_LOGD("%s = COMPOUND END", name);
    } else if (tag->id == TAG_STRING.id) {
      unsigned short tag_val_len = get_ushort_le(data + offset);
      offset += 2;

      char *tag_val = malloc(tag_val_len + 1);
      memcpy(tag_val, data + offset, tag_val_len);
      tag_val[tag_val_len] = '\0';
      ZF_LOGD("%s = '%s'", name, tag_val);

      free(tag_val);

      offset += tag_val_len;
    } else {
      ZF_LOGD("%s = %s\n", name, tag->name);
      offset += resolve_tag_end_offset(data + offset, tag);
    }

    free(name);
  }

  return offset + 1;
}
