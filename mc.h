#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

#define SECTOR_SIZE 4096

typedef struct {
  unsigned char offset[3];
  unsigned char sector_count;
} chunk_location_raw_t;

typedef struct {
  uint32_t offset;
  uint8_t sector_count;
} chunk_location_t;

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

void create_chunk_location(chunk_location_raw_t *from, chunk_location_t *to);

int get_chunk_offset_in_header(int x, int z);

unsigned char *find_data_by_tag_name(char *tag_name, unsigned char *data,
                                     size_t data_len);

size_t resolve_tag_end_offset(unsigned char *data,
                              const nbt_helper_t *start_from_tag);
