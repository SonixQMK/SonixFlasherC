#ifndef SONIXFLASHER_TYPES_H
#define SONIXFLASHER_TYPES_H

#include <hidapi.h>
#include <stdbool.h>
#include <stdint.h>

/* Device State */
typedef struct {
  hid_device *handle;
  uint16_t vid;
  uint16_t pid;
  int chip_family;
  int security_level;
  uint16_t rom_size_kb;
  uint16_t rom_pages;
  uint32_t max_firmware_size;
  uint16_t blank_checksum;
  uint16_t cs_level_0;
  uint16_t code_option;
} device_t;

/* Flash Configuration */
typedef struct {
  const char *file_path;
  uint32_t offset;
  bool is_jumploader;
  bool skip_offset_check;
  bool verbose;
} flash_config_t;

/* Reboot Configuration */
typedef struct {
  bool requested;
  const char *type; /* "sonix", "evision", "hfd" */
} reboot_config_t;

#endif /* SONIXFLASHER_TYPES_H */
