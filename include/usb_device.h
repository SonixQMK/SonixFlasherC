#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Generic USB HID device backend abstraction.
 *
 * NOTE: write()/read() map to HID *Feature Reports* (SET_REPORT/GET_REPORT),
 * not interrupt transfers. That's all this device class needs, so we don't
 * expose separate feature/output/input report calls - keep it simple.
 *
 * data[0] is always the Report ID (0x00 for this device) on both write and
 * read (read: caller pre-fills data[0], backend uses it as GET_REPORT id).
 */

#define HID_CONTROL_TIMEOUT_MS 1000
#define HID_INTERFACE_NUMBER 0

typedef struct usb_device usb_device_t;

bool usb_device_init(void);
void usb_device_exit(void);

bool usb_device_is_present(uint16_t vid, uint16_t pid);

usb_device_t *usb_device_open(uint16_t vid, uint16_t pid);
void usb_device_close(usb_device_t *dev);

int usb_device_write(usb_device_t *dev, const uint8_t *data, size_t len);
int usb_device_read(usb_device_t *dev, uint8_t *data, size_t len);

const char *usb_device_error(usb_device_t *dev);

#endif /* USB_DEVICE_H */