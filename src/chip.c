#include "chip.h"
#include "config.h"
#include "log.h"
#include "mem.h"

typedef struct {
  int family;
  uint16_t variant;
  uint16_t rom_size_kb;
  uint16_t rom_pages;
  uint16_t cs_level_0;
  uint16_t blank_checksum;
} chip_profile_t;

static const chip_profile_t profiles[] = {
    {CHIP_F240, 1, ROM_SIZE_F220, ROM_PAGES_F220, CS_LEVEL_0_VAL2,
     BLANK_CHECKSUM_F220},
    {CHIP_F240, 2, ROM_SIZE_F230, ROM_PAGES_F230, CS_LEVEL_0_VAL2,
     BLANK_CHECKSUM_F230},
    {CHIP_F240, 3, ROM_SIZE_F240, ROM_PAGES_F240, CS_LEVEL_0_VAL2,
     BLANK_CHECKSUM_F240},
    {CHIP_F260, 0, ROM_SIZE_F260, ROM_PAGES_F260, CS_LEVEL_0_VAL1,
     BLANK_CHECKSUM_F260},
    {CHIP_F240B, 0, ROM_SIZE_F240B, ROM_PAGES_F240B, CS_LEVEL_0_VAL1,
     BLANK_CHECKSUM_F240B},
    {CHIP_F280, 0, ROM_SIZE_F280, ROM_PAGES_F280, CS_LEVEL_0_VAL2,
     BLANK_CHECKSUM_F280},
    {CHIP_F290, 0, ROM_SIZE_F290, ROM_PAGES_F290, CS_LEVEL_0_VAL2,
     BLANK_CHECKSUM_F290},
    {CHIP_F240C, 0, ROM_SIZE_F240C, ROM_PAGES_F240C, CS_LEVEL_0_VAL2,
     BLANK_CHECKSUM_F240C},
};

static const size_t num_profiles = sizeof(profiles) / sizeof(profiles[0]);

static bool apply_profile(device_t *dev, const chip_profile_t *profile) {
  if (!dev || !profile)
    return false;

  dev->rom_size_kb = profile->rom_size_kb;
  dev->rom_pages = profile->rom_pages;
  dev->cs_level_0 = profile->cs_level_0;
  dev->blank_checksum = profile->blank_checksum;
  dev->max_firmware_size = (uint32_t)profile->rom_size_kb * 1024;
  dev->chip_family = profile->family;

  return true;
}

bool chip_identify(device_t *dev, const uint8_t *response) {
  if (!dev || !response)
    return false;

  uint8_t family_version = response[8];
  uint8_t chip_version = response[9];
  uint8_t chip_revision = response[11];

  if (family_version != 32) {
    log_error("Unsupported family version: %d", family_version);
    return false;
  }

  log_info("SN32 Detected");

  /* Find matching profile */
  for (size_t i = 0; i < num_profiles; i++) {
    const chip_profile_t *profile = &profiles[i];

    if (profile->family == chip_version) {
      /* Check variant if applicable */
      if (chip_version == CHIP_F240 && profile->variant != chip_revision) {
        continue;
      }

      log_info("Chip %s identified", chip_name(profile->family));
      return apply_profile(dev, profile);
    }
  }

  log_error("Unsupported chip version: %d.%d.%d", chip_version, 0,
            chip_revision);
  return false;
}

const char *chip_name(int family) {
  switch (family) {
  case CHIP_F240:
    return "SN32F24X";
  case CHIP_F260:
    return "SN32F26X";
  case CHIP_F240B:
    return "SN32F24XB";
  case CHIP_F280:
    return "SN32F28X";
  case CHIP_F290:
    return "SN32F29X";
  case CHIP_F240C:
    return "SN32F24XC";
  default:
    return "Unknown";
  }
}

int cs_value_to_level(uint16_t cs_value) {
  switch (cs_value) {
  case CS_LEVEL_0_VAL1:
  case CS_LEVEL_0_VAL2:
    return 0;
  case CS_LEVEL_1:
    return 1;
  case CS_LEVEL_2:
    return 2;
  case CS_LEVEL_3:
    return 3;
  default:
    return -1;
  }
}