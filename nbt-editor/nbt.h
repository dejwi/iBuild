#include <stdlib.h>
#include <stdint.h>

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

const nbt_helper_t *get_helper_tag(uint8_t id);

unsigned char *find_data_tag_comp(const char *tag_name,
                                  const unsigned char *data);

size_t resolve_tag_end_offset(const unsigned char *data,
                              const nbt_helper_t *start_from_tag);

size_t print_comp_fields(const unsigned char *data, int nest_depth);
