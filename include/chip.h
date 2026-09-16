#ifndef SONIXFLASHER_CHIP_H
#define SONIXFLASHER_CHIP_H

#include "types.h"

/* ROM Sizes (KB) */
#define ROM_SIZE_F220 16
#define ROM_SIZE_F230 32
#define ROM_SIZE_F240 64
#define ROM_SIZE_F260 30
#define ROM_SIZE_F240B 64
#define ROM_SIZE_F240C 128
#define ROM_SIZE_F280 128
#define ROM_SIZE_F290 256

/* ROM Pages */
#define ROM_PAGES_F220 16
#define ROM_PAGES_F230 32
#define ROM_PAGES_F240 64
#define ROM_PAGES_F240B 1024
#define ROM_PAGES_F240C 128
#define ROM_PAGES_F260 480
#define ROM_PAGES_F280 128
#define ROM_PAGES_F290 256

/* Chip Family IDs */
#define CHIP_F240 1
#define CHIP_F260 2
#define CHIP_F240B 3
#define CHIP_F280 4
#define CHIP_F290 5
#define CHIP_F240C 6

/* Code Security Levels */
#define CS_LEVEL_0_VAL1 0x0000
#define CS_LEVEL_0_VAL2 0xFFFF
#define CS_LEVEL_1 0x5A5A
#define CS_LEVEL_2 0xA5A5
#define CS_LEVEL_3 0x55AA

/* Blank ROM Checksums */
#define BLANK_CHECKSUM_F220 0xE000
#define BLANK_CHECKSUM_F230 0xC000
#define BLANK_CHECKSUM_F240 0x8000
#define BLANK_CHECKSUM_F260 0x8000
#define BLANK_CHECKSUM_F240B 0x8000
#define BLANK_CHECKSUM_F280 0x0000
#define BLANK_CHECKSUM_F290 0x0000
#define BLANK_CHECKSUM_F240C 0x0000

bool chip_identify(device_t *dev, const uint8_t *response);
const char *chip_name(int family);
int cs_value_to_level(uint16_t cs_value);
#endif /* SONIXFLASHER_CHIP_H */
