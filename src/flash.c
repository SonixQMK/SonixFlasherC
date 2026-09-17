#include "flash.h"
#include "checksum.h"
#include "chip.h"
#include "config.h"
#include "hid_io.h"
#include "log.h"
#include "mem.h"
#include <stdio.h>
#include <unistd.h>

bool flash_erase(device_t *dev) {
  if (!dev || !dev->handle)
    return false;

  log_info("Erasing flash (pages 0-%d)", dev->rom_pages);

  uint8_t buf[REPORT_SIZE];
  mem_zero(buf, sizeof(buf));

  buf[0] = CMD_ENABLE_ERASE;
  mem_write_u16_le(buf + 1, CMD_BASE);
  mem_write_u16_le(buf + 4, 0);              /* Start page */
  mem_write_u16_le(buf + 8, dev->rom_pages); /* End page */

  if (!hid_send_report(dev->handle, buf, REPORT_SIZE)) {
    log_error("Failed to send erase command");
    return false;
  }

  if (!hid_recv_report(dev->handle, buf, REPORT_SIZE, CMD_ENABLE_ERASE)) {
    log_error("Erase verification failed");
    return false;
  }

  uint16_t checksum = mem_read_u16_le(buf, 8);
  if (checksum != dev->blank_checksum) {
    log_error("Erase checksum mismatch: 0x%04x != 0x%04x", checksum,
              dev->blank_checksum);
    return false;
  }

  log_info("Flash erased successfully");
  return true;
}

bool flash_program(device_t *dev, const flash_config_t *config) {
  if (!dev || !dev->handle || !config || !config->file_path)
    return false;

  FILE *fp = fopen(config->file_path, "rb");
  if (!fp) {
    log_error("Cannot open firmware file: %s", config->file_path);
    return false;
  }

  /* Special case: F26X without offset is dangerous */
  uint32_t offset = config->offset;
  if (dev->chip_family == CHIP_F260 && !config->is_jumploader && offset == 0) {
    log_warn("F26X flashing without offset - potentially dangerous");
    if (!config->skip_offset_check) {
      log_info("Using safe default offset: 0x%04x", DEFAULT_OFFSET);
      offset = DEFAULT_OFFSET;
    } else {
      log_warn("Proceeding without offset (at user risk)");
      sleep(FLASH_CONFIRM_DELAY_SEC);
    }
  }

  /* Enable program mode */
  log_info("Enabling program mode (offset: 0x%04x)", offset);

  uint8_t buf[REPORT_SIZE];
  mem_zero(buf, sizeof(buf));

  buf[0] = CMD_ENABLE_PROGRAM;
  mem_write_u16_le(buf + 1, CMD_BASE);
  mem_write_u32_le(buf + 4, offset);

  /* Calculate number of reports */
  if (fseek(fp, 0, SEEK_END) != 0) {
    log_error("Failed to seek firmware file");
    fclose(fp);
    return false;
  }

  long fw_size = ftell(fp);
  if (fw_size <= 0) {
    log_error("Invalid firmware size");
    fclose(fp);
    return false;
  }

  if (fseek(fp, 0, SEEK_SET) != 0) {
    log_error("Failed to rewind firmware file");
    fclose(fp);
    return false;
  }

  size_t fw_size_bytes = (size_t)fw_size;
  size_t total_chunks = (fw_size_bytes + REPORT_SIZE - 1) / REPORT_SIZE;
  if (total_chunks > UINT32_MAX) {
    log_error("Firmware has too many chunks");
    fclose(fp);
    return false;
  }
  mem_write_u32_le(buf + 8, (uint32_t)total_chunks);

  if (!hid_send_report(dev->handle, buf, REPORT_SIZE)) {
    log_error("Failed to enable program mode");
    fclose(fp);
    return false;
  }

  if (!hid_recv_report(dev->handle, buf, REPORT_SIZE, CMD_ENABLE_PROGRAM)) {
    log_error("Program mode verification failed");
    fclose(fp);
    return false;
  }

  /* Flash data */
  log_info("Programming device (%zu chunks)...", total_chunks);

  uint16_t checksum = 0;
  uint32_t last_chunk = 0;
  size_t bytes_read = 0;
  size_t chunk_num = 0;

  mem_zero(buf, sizeof(buf));
  while ((bytes_read = fread(buf, 1, REPORT_SIZE, fp)) > 0) {
    chunk_num++;
    checksum += checksum_calculate(buf, bytes_read);

    if (bytes_read >= sizeof(uint32_t)) {
      last_chunk = mem_read_u32_le(buf, bytes_read - sizeof(uint32_t));
    }

    if (!hid_send_payload(dev->handle, buf, bytes_read, chunk_num,
                          total_chunks)) {
      log_error("Failed to program data");
      fclose(fp);
      return false;
    }

    mem_zero(buf, sizeof(buf));
  }

  fclose(fp);
  log_info("File checksum: 0x%04x", checksum);

  /* Verify programming complete */
  log_info("Verifying programming...");

  if (!hid_recv_report(dev->handle, buf, REPORT_SIZE, CMD_ENABLE_PROGRAM)) {
    log_error("Programming verification failed");
    return false;
  }

  uint32_t recv_chunk = mem_read_u32_le(buf, LAST_CHUNK_OFFSET);
  if (recv_chunk != last_chunk) {
    log_error("Last chunk mismatch: 0x%08x != 0x%08x", recv_chunk, last_chunk);
    return false;
  }

  /* Verify checksum unless offset is used */
  if (offset == 0) {
    uint16_t recv_checksum = mem_read_u16_le(buf, 8);
    if (recv_checksum != checksum) {
      log_error("Checksum mismatch: 0x%04x != 0x%04x", recv_checksum, checksum);
      return false;
    }
  } else {
    log_warn("Checksum verification skipped (offset used)");
  }

  log_info("Programming verified successfully");
  return true;
}

bool flash_verify_checksum(device_t *dev, uint16_t expected_checksum) {
  if (!dev || !dev->handle)
    return false;

  log_info("Verifying flash checksum (expected: 0x%04x)", expected_checksum);

  uint16_t device_checksum = 0;
  if (!device_get_checksum(dev, &device_checksum)) {
    log_error("Failed to get device checksum");
    return false;
  }

  if (device_checksum != expected_checksum) {
    log_error("Checksum mismatch: device=0x%04x, expected=0x%04x",
              device_checksum, expected_checksum);
    return false;
  }

  log_info("Checksum verified successfully");
  return true;
}
