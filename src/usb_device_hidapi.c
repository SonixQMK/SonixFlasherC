#include "log.h"
#include "usb_device.h"

#include <hidapi.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

struct usb_device {
  hid_device *handle;
  char error[256];
};

static void set_hid_error(usb_device_t *dev) {
  if (!dev)
    return;

  dev->error[0] = '\0';

  const wchar_t *wide_error = hid_error(dev->handle);
  if (!wide_error) {
    strcpy(dev->error, "unknown HID error");
    return;
  }

  size_t converted = wcstombs(dev->error, wide_error, sizeof(dev->error) - 1);

  if (converted == (size_t)-1) {
    strcpy(dev->error, "failed to convert HID error");
    return;
  }

  dev->error[converted] = '\0';
}

bool usb_device_init(void) { return hid_init() == 0; }

void usb_device_exit(void) {
  if (hid_exit() != 0)
    log_warn("HID exit failed");
}

usb_device_t *usb_device_open(uint16_t vid, uint16_t pid) {
  hid_device *handle = hid_open(vid, pid, NULL);
  if (!handle)
    return NULL;

  usb_device_t *dev = calloc(1, sizeof(*dev));
  if (!dev) {
    hid_close(handle);
    return NULL;
  }

  dev->handle = handle;
  strcpy(dev->error, "no HID error");

  return dev;
}

void usb_device_close(usb_device_t *dev) {
  if (!dev)
    return;

  if (dev->handle) {
    hid_close(dev->handle);
    dev->handle = NULL;
  }

  free(dev);
}

int usb_device_write(usb_device_t *dev, const uint8_t *data, size_t len) {
  if (!dev || !dev->handle || !data) {
    if (dev)
      strcpy(dev->error, "invalid HID device or data");
    return -1;
  }

  int result = hid_send_feature_report(dev->handle, data, len);
  if (result < 0)
    set_hid_error(dev);

  return result;
}

int usb_device_read(usb_device_t *dev, uint8_t *data, size_t len) {
  if (!dev || !dev->handle || !data) {
    if (dev)
      strcpy(dev->error, "invalid HID device or data");
    return -1;
  }

  int result = hid_get_feature_report(dev->handle, data, len);
  if (result < 0)
    set_hid_error(dev);

  return result;
}

const char *usb_device_error(usb_device_t *dev) {
  if (!dev || dev->error[0] == '\0')
    return "unknown HID error";

  return dev->error;
}