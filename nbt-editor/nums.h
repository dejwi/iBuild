#pragma once

#include <stdlib.h>

unsigned short get_ushort_le(const unsigned char *data);

unsigned short ushort_to_be(unsigned short value);

void *reverse_endian(void *p, size_t size);

int get_int_le(const unsigned char *data);

int count_min_bits(int n);
