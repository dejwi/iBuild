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

typedef struct insert_data_t {
  size_t start_offset;
  size_t end_offset;
  unsigned char *data_insert;
  size_t data_size;
  struct insert_data_t *next;
} insert_data_t;

typedef struct {
  int x;
  int y;
  int z;
} vector3_t;

typedef struct {
  int x_size;
  int y_size;
  int z_size;
  vector3_t chunk_pos;
  char **palette;
  int palette_len;
  int *indices;
} block_build_t;

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

unsigned char *find_data_tag_comp(const char *tag_name,
                                  const unsigned char *data);

size_t resolve_tag_end_offset(const unsigned char *data,
                              const nbt_helper_t *start_from_tag);

unsigned short get_ushort_le(const unsigned char *data);

int get_int_le(const unsigned char *data);

size_t print_comp_fields(const unsigned char *data, int nest_depth);

unsigned char *insert_data(const unsigned char *data, size_t size,
                           insert_data_t *payload, size_t *out_size);

void test_chunk_edit(const char *region_path, int x_chunk, int z_chunk,
                     const block_build_t *build);
