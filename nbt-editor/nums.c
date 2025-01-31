#include <stdlib.h>

unsigned short get_ushort_le(const unsigned char *data) {
  return data[0] << 8 | data[1];
}

unsigned short ushort_to_be(unsigned short value) {
  return (value << 8) | (value >> 8);
}

void *reverse_endian(void *p, size_t size) {
  char *head = (char *)p;
  char *tail = head + size - 1;

  for (; tail > head; --tail, ++head) {
    char temp = *head;
    *head = *tail;
    *tail = temp;
  }
  return p;
}

int get_int_le(const unsigned char *data) {
  char *data_s = (char *)data;
  return data_s[0] << 24 | data_s[1] << 16 | data_s[2] << 8 | data_s[3];
}

int count_min_bits(int n) {
  int count = 0, i;
  if (n == 0)
    return 0;
  for (i = 0; i < 32; i++) {
    if ((1 << i) & n)
      count = i;
  }

  return ++count;
}
