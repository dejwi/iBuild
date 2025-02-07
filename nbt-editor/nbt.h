#pragma once

#include <stdint.h>
#include <stdlib.h>

typedef struct {
  uint8_t id;
  int payload_size;
  char name[40];
} nbt_helper_t;

extern const nbt_helper_t TAG_END;
extern const nbt_helper_t TAG_BYTE;
extern const nbt_helper_t TAG_SHORT;
extern const nbt_helper_t TAG_INT;
extern const nbt_helper_t TAG_LONG;
extern const nbt_helper_t TAG_FLOAT;
extern const nbt_helper_t TAG_DOUBLE;
extern const nbt_helper_t TAG_BYTE_ARRAY;
extern const nbt_helper_t TAG_STRING;
extern const nbt_helper_t TAG_LIST;
extern const nbt_helper_t TAG_COMPOUND;
extern const nbt_helper_t TAG_INT_ARRAY;
extern const nbt_helper_t TAG_LONG_ARRAY;

/**
 * Retrieves the NBT helper tag based on the given ID.
 *
 * @param id The ID of the NBT tag.
 * @return A pointer to the corresponding nbt_helper_t structure.
 */
const nbt_helper_t *get_helper_tag(uint8_t id);

/**
 * Finds the data tag within a compound tag by its name.
 *
 * @param tag_name The name of the tag to search for.
 * @param data The data buffer containing the NBT data.
 * @return A pointer to the data of the found tag, or NULL if not found.
 */
unsigned char *find_data_tag_comp(const char *tag_name,
                                  const unsigned char *data);

/**
 * Resolves the end offset of a tag starting from a given tag.
 *
 * @param data The data buffer containing the NBT data.
 * @param start_from_tag The tag to start resolving from (optional - skips ID,
 * NAME_LEN, NAME bytes).

 * @return The offset to the end of the tag.
 */
size_t resolve_tag_end_offset(const unsigned char *data,
                              const nbt_helper_t *start_from_tag);

/**
 * Prints the fields of a compound tag (DEBUG log level).
 *
 * @param data The data buffer containing the NBT data.
 * @param nest_depth The depth of nesting to print.
 * @return The offset to the end of the printed fields.
 */
size_t print_comp_fields(const unsigned char *data, int nest_depth);
