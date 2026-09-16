#ifndef SONIXFLASHER_DEVICE_H
#define SONIXFLASHER_DEVICE_H

#include "types.h"

/* Device VID/PID */
#define VID_SONIX 0x0c45
#define VID_EVISION 0x320F
#define VID_APPLE 0x05ac

#define PID_SN22X 0x7900
#define PID_SN23X 0x7900
#define PID_SN24X 0x7900
#define PID_SN24XB 0x7040
#define PID_SN24XC 0x7160
#define PID_SN26X 0x7010
#define PID_SN28X 0x7120
#define PID_SN29X 0x7140

bool device_open(device_t *dev, uint16_t vid, uint16_t pid);
void device_close(device_t *dev);
bool device_init_protocol(device_t *dev, const reboot_config_t *reboot);
bool device_reboot_to_user_mode(device_t *dev);
bool device_check_code_option(device_t *dev);
bool device_set_code_security(device_t *dev, uint16_t cs_level);
bool device_get_checksum(device_t *dev, uint16_t *checksum);

#endif /* SONIXFLASHER_DEVICE_H */
