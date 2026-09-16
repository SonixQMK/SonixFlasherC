#include "checksum.h"

uint16_t checksum_calculate(const uint8_t *data, size_t len) {
  if (!data || len == 0)
    return 0;

  uint16_t sum = 0;
  size_t i;

  /* Process pairs */
  for (i = 0; i + 1 < len; i += 2) {
    uint16_t value = (uint16_t)(data[i] | (data[i + 1] << 8));
    sum += value;
  }

  /* Process remaining byte */
  if (i < len) {
    sum += data[i];
  }

  return sum;
}
