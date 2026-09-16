#ifndef SONIXFLASHER_HID_IO_H
#define SONIXFLASHER_HID_IO_H

#include <hidapi.h>
#include <stdbool.h>
#include <stdint.h>

/* HID Commands - ISP Protocol v9 */
#define CMD_BASE 0x55AA
#define CMD_GET_FW_VERSION 0x1
#define CMD_SET_ENCRYPTION_ALGO 0x3
#define CMD_ENABLE_ERASE 0x4
#define CMD_ENABLE_PROGRAM 0x5
#define CMD_GET_CHECKSUM 0x6
#define CMD_RETURN_USER_MODE 0x7
#define CMD_VERIFY(x) ((CMD_BASE << 8) | (x))
#define CMD_ACK 0xFAFAFAFA

bool hid_send_report(hid_device *dev, const uint8_t *data, size_t len);
bool hid_recv_report(hid_device *dev, uint8_t *data, size_t len,
                     uint32_t expected_cmd);
bool hid_send_payload(hid_device *dev, const uint8_t *data, size_t len,
                      size_t current, size_t total);

#endif /* SONIXFLASHER_HID_IO_H */
