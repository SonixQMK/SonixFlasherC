#ifndef SONIXFLASHER_FLASH_H
#define SONIXFLASHER_FLASH_H

#include "device.h"
#include "types.h"

/* Last chunk offset for verification */
#define LAST_CHUNK_OFFSET (REPORT_SIZE - sizeof(uint32_t))

bool flash_program(device_t *dev, const flash_config_t *config);
bool flash_erase(device_t *dev);
bool flash_verify_checksum(device_t *dev, uint16_t expected_checksum);

#endif /* SONIXFLASHER_FLASH_H */
