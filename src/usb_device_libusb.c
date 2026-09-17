#include "config.h"
#include "mem.h"
#include "usb_device.h"

#include <libusb-1.0/libusb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define HID_REPORT_TYPE_FEATURE 0x03

#define HID_REQUEST_GET_REPORT 0x01
#define HID_REQUEST_SET_REPORT 0x09

/*
 * HID class requests addressed to a specific USB interface.
 *
 * GET_REPORT: device -> host
 * SET_REPORT: host -> device
 */
#define HID_GET_REPORT_REQUEST_TYPE                                            \
  ((uint8_t)((uint8_t)LIBUSB_ENDPOINT_IN |                                     \
             (uint8_t)LIBUSB_REQUEST_TYPE_CLASS |                              \
             (uint8_t)LIBUSB_RECIPIENT_INTERFACE))

#define HID_SET_REPORT_REQUEST_TYPE                                            \
  ((uint8_t)((uint8_t)LIBUSB_ENDPOINT_OUT |                                    \
             (uint8_t)LIBUSB_REQUEST_TYPE_CLASS |                              \
             (uint8_t)LIBUSB_RECIPIENT_INTERFACE))

/*
 * One libusb context for the program lifecycle:
 *
 *   usb_device_init()
 *   usb_device_open()
 *   usb_device_close()
 *   usb_device_exit()
 */
static libusb_context *usb_context;

struct usb_device {
  libusb_device_handle *handle;
  bool interface_claimed;
  bool kernel_driver_detached;
  char error[256];
};

static void set_error(usb_device_t *dev, const char *message, int result) {
  if (!dev)
    return;

  (void)snprintf(dev->error, sizeof(dev->error), "%s: %s", message,
                 libusb_error_name(result));
}

bool usb_device_is_present(uint16_t vid, uint16_t pid) {
  if (!usb_context)
    return false;

  libusb_device **list = NULL;
  ssize_t count = libusb_get_device_list(usb_context, &list);
  if (count < 0)
    return false;

  bool found = false;

  for (ssize_t i = 0; i < count; i++) {
    struct libusb_device_descriptor desc;

    if (libusb_get_device_descriptor(list[i], &desc) != 0)
      continue;

    if (desc.idVendor == vid && desc.idProduct == pid) {
      found = true;
      break;
    }
  }

  libusb_free_device_list(list, 1);
  return found;
}

/*
 * Match hidapi feature-report behavior:
 *
 * - data[0] is the HID report ID.
 * - Sonix uses report ID 0x00.
 * - For report ID 0x00, HID places the ID in wValue, not the control-transfer
 *   data stage. Skip data[0] for libusb, then restore it in the return count.
 */
static int feature_report_transfer(usb_device_t *dev, uint8_t request_type,
                                   uint8_t request, uint8_t *data, size_t len) {
  if (!dev || !dev->handle || !data || len == 0) {
    if (dev)
      (void)snprintf(dev->error, sizeof(dev->error),
                     "invalid USB device or data");
    return -1;
  }

  if (len > REPORT_SIZE + 1) {
    (void)snprintf(dev->error, sizeof(dev->error),
                   "feature report too large: %zu (max %d)", len,
                   REPORT_SIZE + 1);
    return -1;
  }

  uint8_t report_id = data[0];
  uint8_t *transfer_data = data;
  size_t transfer_len = len;
  bool skipped_report_id = false;

  /*
   * Report type is stored in the high byte and report ID in the low byte.
   * Sonix uses a feature report with ID 0x00.
   */
  uint16_t value = (HID_REPORT_TYPE_FEATURE << 8) | report_id;

  if (report_id == 0x00) {
    transfer_data++;
    transfer_len--;
    skipped_report_id = true;
  }

  int result = libusb_control_transfer(
      dev->handle, request_type, request, value, HID_INTERFACE_NUMBER,
      transfer_data, (uint16_t)transfer_len, HID_CONTROL_TIMEOUT_MS);

  if (result < 0) {
    set_error(dev, "HID feature report transfer failed", result);
    return -1;
  }

  /*
   * Keep the public wrapper API compatible with hidapi:
   * report ID + report payload.
   */
  if (skipped_report_id)
    result++;

  return result;
}

bool usb_device_init(void) {
  if (usb_context)
    return true;

  int result = libusb_init(&usb_context);
  if (result < 0) {
    usb_context = NULL;
    return false;
  }

  return true;
}

void usb_device_exit(void) {
  if (!usb_context)
    return;

  libusb_exit(usb_context);
  usb_context = NULL;
}

usb_device_t *usb_device_open(uint16_t vid, uint16_t pid) {
  if (!usb_context)
    return NULL;

  usb_device_t *dev = calloc(1, sizeof(*dev));
  if (!dev)
    return NULL;

  dev->handle = libusb_open_device_with_vid_pid(usb_context, vid, pid);

  if (!dev->handle) {
    free(dev);
    return NULL;
  }

  int result = libusb_kernel_driver_active(dev->handle, HID_INTERFACE_NUMBER);

  if (result == 1) {
    result = libusb_detach_kernel_driver(dev->handle, HID_INTERFACE_NUMBER);

    if (result < 0) {
      set_error(dev, "failed to detach kernel driver", result);
      usb_device_close(dev);
      return NULL;
    }

    dev->kernel_driver_detached = true;
  } else if (result < 0 && result != LIBUSB_ERROR_NOT_SUPPORTED) {
    set_error(dev, "failed to check kernel driver", result);
    usb_device_close(dev);
    return NULL;
  }

  result = libusb_claim_interface(dev->handle, HID_INTERFACE_NUMBER);
  if (result < 0) {
    set_error(dev, "failed to claim HID interface", result);
    usb_device_close(dev);
    return NULL;
  }

  dev->interface_claimed = true;
  return dev;
}

void usb_device_close(usb_device_t *dev) {
  if (!dev)
    return;

  if (dev->handle) {
    if (dev->interface_claimed) {
      libusb_release_interface(dev->handle, HID_INTERFACE_NUMBER);
      dev->interface_claimed = false;
    }

    if (dev->kernel_driver_detached) {
      libusb_attach_kernel_driver(dev->handle, HID_INTERFACE_NUMBER);
      dev->kernel_driver_detached = false;
    }

    libusb_close(dev->handle);
    dev->handle = NULL;
  }

  free(dev);
}

int usb_device_write(usb_device_t *dev, const uint8_t *data, size_t len) {
  if (!dev) {
    return -1;
  }

  if (!data || len == 0 || len > REPORT_SIZE + 1) {
    (void)snprintf(dev->error, sizeof(dev->error),
                   "invalid write length: %zu (max %d)", len, REPORT_SIZE + 1);
    return -1;
  }

  uint8_t buf[REPORT_SIZE + 1];
  mem_zero(buf, sizeof(buf));
  mem_copy(buf, data, len);

  return feature_report_transfer(dev, HID_SET_REPORT_REQUEST_TYPE,
                                 HID_REQUEST_SET_REPORT, buf, len);
}

int usb_device_read(usb_device_t *dev, uint8_t *data, size_t len) {
  if (!dev) {
    return -1;
  }

  if (!data || len == 0 || len > REPORT_SIZE + 1) {
    (void)snprintf(dev->error, sizeof(dev->error),
                   "invalid read length: %zu (max %d)", len, REPORT_SIZE + 1);
    return -1;
  }

  /* libusb GET_REPORT path expects the report ID to be present in the
   * first byte, but the transfer helper may strip it out when report_id ==
   * 0x00. We keep the public API consistent with hidapi's REPORT_SIZE+1
   * semantics. */
  uint8_t buf[REPORT_SIZE + 1];
  mem_zero(buf, sizeof(buf));

  int result = feature_report_transfer(dev, HID_GET_REPORT_REQUEST_TYPE,
                                       HID_REQUEST_GET_REPORT, buf, len);

  if (result < 0) {
    return result;
  }

  /* feature_report_transfer() may have incremented result to account for the
   * stripped report ID; undo that so the caller sees the original payload
   * length while preserving the same semantics as hidapi. */
  if (result > 0 && result <= (int)len) {
    mem_copy(data, buf, (size_t)result);
    return result;
  }

  if (result > (int)len) {
    /* report ID was reinserted, move data back after the 0x00 byte */
    mem_copy(data, buf + 1, len);
    return result;
  }

  return result;
}

const char *usb_device_error(usb_device_t *dev) {
  if (!dev || dev->error[0] == '\0')
    return "unknown USB error";

  return dev->error;
}