#pragma once

#include <stdint.h>
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

/**
 * Converts raw chunk location data to a structured format.
 *
 * @param from Pointer to the raw chunk location data.
 * @param to Pointer to the structured chunk location data.
 */
void create_chunk_location(const chunk_location_raw_t *from,
                           chunk_location_t *to);

/**
 * Appends an insert data node to the insert list.
 *
 * @param head Pointer to the head of the insert list.
 * @param tail Pointer to the tail of the insert list.
 * @param add Pointer to the insert data node to be added.
 */
void append_insert_list(insert_data_t **head, insert_data_t **tail,
                        insert_data_t *add);

/**
 * Frees the memory allocated for the insert list.
 *
 * @param head Pointer to the head of the insert list.
 */
void free_insert_list(insert_data_t *head);

/**
 * Inserts data into the given data buffer at specified offsets.
 *
 * @param data Pointer to the original data buffer.
 * @param size Size of the original data buffer.
 * @param payload Pointer to the insert data list.
 * @param out_size Pointer to the size of the new data buffer.
 * @return Pointer to the new data buffer with inserted data.
 */
unsigned char *insert_data(const unsigned char *data, size_t size,
                           const insert_data_t *payload, size_t *out_size);

/**
 * Loads chunk data from a region file.
 *
 * @param region_path Path to the region file.
 * @param header_offset Offset of the chunk header in the region file.
 * @param out_loc Pointer to the chunk location structure to be filled.
 * @param out_size Pointer to the size of the loaded chunk data to be filled.
 * @param out_data Pointer to the buffer to store the loaded chunk data.
 * @return 0 on success, 1 if the chunk is not generated.
 */
int load_chunk_data(const char *region_path, size_t header_offset,
                    chunk_location_t *out_loc, size_t *out_size,
                    unsigned char **out_data);

/**
 * Writes chunk data to a region file.
 *
 * @param region_path Path to the region file.
 * @param chunk_loc Pointer to the chunk location structure.
 * @param edit_list Pointer to the insert data list with desired modifications.
 * @param data Pointer to the original data buffer.
 * @param data_size Size of the original data buffer.
 */
void write_chunk_data(const char *region_path,
                      const chunk_location_t *chunk_loc,
                      const insert_data_t *edit_list, const unsigned char *data,
                      size_t data_size);

