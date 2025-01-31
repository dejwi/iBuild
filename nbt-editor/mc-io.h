#include <stdlib.h>

#define SECTOR_SIZE 4096

typedef struct {
  unsigned char offset[3];
  unsigned char sector_count;
} chunk_location_raw_t;

typedef struct {
  uint32_t offset;
  uint8_t sector_count;
} chunk_location_t;

typedef struct insert_data_t {
  size_t start_offset;
  size_t end_offset;
  unsigned char *data_insert;
  size_t data_size;
  struct insert_data_t *next;
} insert_data_t;

void create_chunk_location(const chunk_location_raw_t *from,
                           chunk_location_t *to);

void append_insert_list(insert_data_t **head, insert_data_t **tail,
                        insert_data_t *add);

void free_insert_list(insert_data_t *head);

unsigned char *insert_data(const unsigned char *data, size_t size,
                           const insert_data_t *payload, size_t *out_size);

void load_chunk_data(const char *region_path, size_t header_offset,
                     chunk_location_t *out_loc, size_t *out_size,
                     unsigned char **out_data);

void write_chunk_data(const char *region_path,
                      const chunk_location_t *chunk_loc,
                      const insert_data_t *edit_list, const unsigned char *data,
                      size_t data_size);
