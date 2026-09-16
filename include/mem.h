#ifndef SONIXFLASHER_MEM_H
#define SONIXFLASHER_MEM_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Safe buffer operations */
void mem_zero(void *dst, size_t len);
void mem_copy(void *dst, const void *src, size_t len);
uint16_t mem_read_u16_le(const uint8_t *data, size_t offset);
uint32_t mem_read_u32_le(const uint8_t *data, size_t offset);
void mem_write_u16_le(uint8_t *data, uint16_t value);
void mem_write_u32_le(uint8_t *data, uint32_t value);

#endif /* SONIXFLASHER_MEM_H */
