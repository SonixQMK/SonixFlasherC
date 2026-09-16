#ifndef SONIXFLASHER_CHECKSUM_H
#define SONIXFLASHER_CHECKSUM_H

#include <stddef.h>
#include <stdint.h>

uint16_t checksum_calculate(const uint8_t *data, size_t len);

#endif /* SONIXFLASHER_CHECKSUM_H */
