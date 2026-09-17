#include "hid_io.h"
#include "config.h"
#include "log.h"
#include "mem.h"
#include "usb_device.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

bool hid_send_report(usb_device_t *dev, const uint8_t *data, size_t len) {
  if (!dev || !data) {
    log_error("Invalid HID device or data");
    return false;
  }

  if (len > REPORT_SIZE) {
    log_error("Report too large: %zu > %d", len, REPORT_SIZE);
    return false;
  }

  /* Allocate buffer with report ID prefix */
  uint8_t buf[REPORT_SIZE + 1];
  mem_zero(buf, sizeof(buf));
  buf[0] = 0x00; /* Report ID */
  mem_copy(buf + 1, data, len);

  log_hex("Sending", buf, len + 1);

  if (usb_device_write(dev, buf, len + 1) < 0) {
    log_error("Failed to send HID report: %s", usb_device_error(dev));
    return false;
  }

  return true;
}

bool hid_send_payload(usb_device_t *dev, const uint8_t *data, size_t len,
                      size_t current, size_t total) {
  if (!dev || !data) {
    log_error("Invalid HID device or data");
    return false;
  }

  if (len > REPORT_SIZE) {
    log_error("Payload too large: %zu > %d", len, REPORT_SIZE);
    return false;
  }

  /* Allocate buffer with report ID prefix */
  uint8_t buf[REPORT_SIZE + 1];
  mem_zero(buf, sizeof(buf));
  buf[0] = 0x00; /* Report ID */
  mem_copy(buf + 1, data, len);

  /* Calculate progress */
  double percent =
      (total > 0) ? (100.0 * (double)current / (double)total) : 0.0;

  log_debug("Sending payload: chunk %zu/%zu (%.1f%%) - %zu bytes", current,
            total, percent, len);

  /* Log data with proper hex formatting */
  log_hex("  Data", data, len);

  if (usb_device_write(dev, buf, len + 1) < 0) {
    log_error("Failed to send HID payload: %s", usb_device_error(dev));
    return false;
  }

  return true;
}

bool hid_recv_report(usb_device_t *dev, uint8_t *data, size_t len,
                     uint32_t expected_cmd) {
  if (!dev || !data) {
    log_error("Invalid HID device or buffer");
    return false;
  }

  if (len < 8) {
    log_error("Buffer too small for command response");
    return false;
  }

  /* Allocate buffer with report ID prefix */
  uint8_t *buf = malloc(len + 1);
  if (!buf) {
    log_error("Failed to allocate %zu bytes", len + 1);
    return false;
  }

  uint8_t attempts = 0;
  while (attempts < MAX_ATTEMPTS) {
    mem_zero(buf, len + 1);
    buf[0] = 0x00; /* Report ID */

    int res = usb_device_read(dev, buf, len + 1);

    if (res == (int)(len + 1)) {
      /* Strip report ID and copy payload */
      mem_copy(data, buf + 1, len);
      log_hex("Received", data, len);

      /* Verify response */
      uint32_t cmd_reply = mem_read_u32_le(data, 0);
      uint32_t status = mem_read_u32_le(data, 4);

      if (cmd_reply == CMD_VERIFY(expected_cmd)) {
        if (status == CMD_ACK) {
          free(buf);
          return true;
        }
        log_error("Invalid status: 0x%08x, expected 0x%08x", status, CMD_ACK);
        free(buf);
        return false;
      }

      log_error("Invalid command response: 0x%08x, expected 0x%08x", cmd_reply,
                expected_cmd);
      free(buf);
      return false;
    }

    if (res < 0) {
      log_raw("Device busy, retrying... (attempt %d/%d)", attempts + 1,
              MAX_ATTEMPTS);
      attempts++;
      usleep(RETRY_DELAY_MS * 1000);
    } else {
      log_error("Invalid response length: %d, expected %zu", res, len + 1);
      free(buf);
      return false;
    }
  }

  log_error("Failed to receive report after %d attempts", MAX_ATTEMPTS);
  free(buf);
  return false;
}
