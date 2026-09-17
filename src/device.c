#include "device.h"
#include "chip.h"
#include "config.h"
#include "hid_io.h"
#include "log.h"
#include "mem.h"
#include <string.h>
#include <unistd.h>

static bool device_send_reboot_cmd(usb_device_t *dev, const char *type) {
  if (!dev || !type)
    return false;

  uint32_t cmd[2] = {0};

  if (strcmp(type, "sonix") == 0 || strcmp(type, "evision") == 0) {
    cmd[0] = 0x5AA555AA;
    cmd[1] = 0xCC3300FF;
  } else if (strcmp(type, "hfd") == 0) {
    cmd[0] = 0x5A8942AA;
    cmd[1] = 0xCC6271FF;
  } else {
    log_error("Unknown reboot type: %s", type);
    return false;
  }

  uint8_t buf[REPORT_SIZE];
  mem_zero(buf, sizeof(buf));
  mem_write_u32_le(buf, cmd[0]);
  mem_write_u32_le(buf + 4, cmd[1]);

  uint8_t attempts = 0;
  while (attempts < MAX_ATTEMPTS) {
    if (hid_send_report(dev, buf, REPORT_SIZE)) {
      log_info("Reboot command sent");
      return true;
    }
    attempts++;
    sleep(HID_WAIT_SEC);
  }

  log_error("Failed to send reboot command");
  return false;
}

bool device_open(device_t *dev, uint16_t vid, uint16_t pid) {
  if (!dev)
    return false;

  if (!usb_device_init()) {
    log_error("USB initialization failed");
    return false;
  }

  dev->handle = NULL;
  uint8_t attempts = 0;

  while (attempts < MAX_ATTEMPTS) {
    if (!usb_device_is_present(vid, pid)) {
      log_raw("Device not found (VID:[0x%04x] PID:[0x%04x]), retrying... "
              "(attempt %d/%d)\r",
              vid, pid, attempts + 1, MAX_ATTEMPTS);
      attempts++;
      sleep(HID_WAIT_SEC);
      continue;
    }

    dev->handle = usb_device_open(vid, pid);
    if (dev->handle) {
      log_info("Device opened successfully");
      dev->vid = vid;
      dev->pid = pid;
      return true;
    }

    log_raw("Device present but failed to open (permissions?), retrying... "
            "(attempt %d/%d)\r",
            attempts + 1, MAX_ATTEMPTS);
    attempts++;
    sleep(HID_WAIT_SEC);
  }

  usb_device_exit();
  log_error("Failed to open device after %d attempts", MAX_ATTEMPTS);
  return false;
}
void device_close(device_t *dev) {
  if (!dev)
    return;

  if (dev->handle) {
    usb_device_close(dev->handle);
    dev->handle = NULL;
  }

  usb_device_exit();
}

static bool get_firmware_version(device_t *dev, uint8_t *buf) {
  if (!dev || !dev->handle || !buf)
    return false;

  mem_zero(buf, REPORT_SIZE);
  buf[0] = CMD_GET_FW_VERSION;
  mem_write_u16_le(buf + 1, CMD_BASE);
  mem_write_u16_le(buf + 4, dev->code_option);

  uint8_t attempts = 0;
  while (attempts < MAX_ATTEMPTS) {
    if (hid_send_report(dev->handle, buf, REPORT_SIZE)) {
      break;
    }
    log_raw("Failed to fetch version, retrying... (attempt %d/%d)\r",
            attempts + 1, MAX_ATTEMPTS);
    attempts++;
    sleep(HID_WAIT_SEC);
  }

  if (attempts >= MAX_ATTEMPTS) {
    log_error("Failed to send version request");
    return false;
  }

  return hid_recv_report(dev->handle, buf, REPORT_SIZE, CMD_GET_FW_VERSION);
}

bool device_init_protocol(device_t *dev, const reboot_config_t *reboot) {
  if (!dev || !dev->handle)
    return false;

  /* Request reboot if configured */
  if (reboot && reboot->requested) {
    log_info("Requesting bootloader reboot");
    if (!device_send_reboot_cmd(dev->handle, reboot->type)) {
      log_error("Reboot request failed");
      return false;
    }
    log_info("");
    sleep(IO_DELAY_SEC);
  }

  /* Fetch and validate firmware version */
  uint8_t buf[REPORT_SIZE];
  if (!get_firmware_version(dev, buf)) {
    log_error("Failed to get firmware version");
    return false;
  }

  /* Identify chip */
  if (!chip_identify(dev, buf)) {
    log_error("Chip identification failed");
    return false;
  }

  /* Extract security level */
  uint16_t cs_value = mem_read_u16_le(buf, 14);
  int level = cs_value_to_level(cs_value);
  if (level < 0) {
    log_error("Unsupported security level: 0x%04x", cs_value);
    return false;
  }

  dev->security_level = level;
  log_info("Security level: CS%d", dev->security_level);
  return true;
}

bool device_set_code_security(device_t *dev, uint16_t cs_level) {
  if (!dev || !dev->handle)
    return false;

  /* Determine target security level from cs_level value */
  int target_level = cs_value_to_level(cs_level);
  if (target_level < 0) {
    log_error("Invalid code security level: 0x%04x", cs_level);
    return false;
  }

  /* Skip if already at target level */
  if (dev->security_level == target_level) {
    log_info("Code security already at CS%d, skipping", target_level);
    return true;
  }

  log_info("Setting code security from CS%d to CS%d", dev->security_level,
           target_level);

  uint8_t buf[REPORT_SIZE];
  mem_zero(buf, sizeof(buf));

  buf[0] = CMD_SET_ENCRYPTION_ALGO;
  mem_write_u16_le(buf + 1, CMD_BASE);
  mem_write_u16_le(buf + 4, dev->code_option);
  mem_write_u16_le(buf + 6, cs_level);

  if (!hid_send_report(dev->handle, buf, REPORT_SIZE)) {
    log_error("Failed to send code security command");
    return false;
  }

  if (!hid_recv_report(dev->handle, buf, REPORT_SIZE,
                       CMD_SET_ENCRYPTION_ALGO)) {
    log_error("Code security verification failed");
    return false;
  }

  mem_zero(buf, sizeof(buf));
  return true;
}

bool device_reboot_to_user_mode(device_t *dev) {
  if (!dev || !dev->handle)
    return false;

  log_info("Rebooting to user mode");

  uint8_t buf[REPORT_SIZE];
  mem_zero(buf, sizeof(buf));

  buf[0] = CMD_RETURN_USER_MODE;
  mem_write_u16_le(buf + 1, CMD_BASE);

  if (!hid_send_report(dev->handle, buf, REPORT_SIZE)) {
    log_error("Failed to send reboot to user mode command");
    return false;
  }

  mem_zero(buf, sizeof(buf));
  return true;
}

bool device_get_checksum(device_t *dev, uint16_t *checksum) {
  if (!dev || !dev->handle || !checksum)
    return false;

  log_info("Getting device checksum");

  uint8_t buf[REPORT_SIZE];
  mem_zero(buf, sizeof(buf));

  buf[0] = CMD_GET_CHECKSUM;
  mem_write_u16_le(buf + 1, CMD_BASE);

  if (!hid_send_report(dev->handle, buf, REPORT_SIZE)) {
    log_error("Failed to send checksum command");
    return false;
  }

  /* Receive and verify response */
  mem_zero(buf, sizeof(buf));
  if (!hid_recv_report(dev->handle, buf, REPORT_SIZE, CMD_GET_CHECKSUM)) {
    log_error("Checksum retrieval failed");
    return false;
  }

  *checksum = mem_read_u16_le(buf, 8);

  log_info("Device checksum: 0x%04x", *checksum);
  return true;
}
