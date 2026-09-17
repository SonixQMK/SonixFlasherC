#include "mem.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void mem_zero(void *dst, size_t len) {
  if (!dst)
    return;
  memset(dst, 0, len);
}

void mem_copy(void *dst, const void *src, size_t len) {
  if (!dst || !src)
    return;
  memcpy(dst, src, len);
}

uint16_t mem_read_u16_le(const uint8_t *data, size_t offset) {
  if (!data)
    return 0;
  return (uint16_t)(data[offset] | (data[offset + 1] << 8));
}

uint32_t mem_read_u32_le(const uint8_t *data, size_t offset) {
  if (!data)
    return 0;
  return (uint32_t)(data[offset] | (data[offset + 1] << 8) |
                    (data[offset + 2] << 16) | (data[offset + 3] << 24));
}

void mem_write_u16_le(uint8_t *data, uint16_t value) {
  if (!data)
    return;
  data[0] = (uint8_t)(value & 0xFF);
  data[1] = (uint8_t)((value >> 8) & 0xFF);
}

void mem_write_u32_le(uint8_t *data, uint32_t value) {
  if (!data)
    return;
  data[0] = (uint8_t)(value & 0xFF);
  data[1] = (uint8_t)((value >> 8) & 0xFF);
  data[2] = (uint8_t)((value >> 16) & 0xFF);
  data[3] = (uint8_t)((value >> 24) & 0xFF);
}
