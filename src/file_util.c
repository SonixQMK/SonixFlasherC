#include "file_util.h"
#include "checksum.h"
#include "config.h"
#include "log.h"
#include "mem.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <limits.h>
#endif

char *file_get_absolute_path(const char *path) {
  if (!path)
    return NULL;

  char *result = NULL;

#ifdef _WIN32
  char buffer[MAX_PATH];
  if (GetFullPathName(path, MAX_PATH, buffer, NULL)) {
    size_t len = strlen(buffer);
    result = malloc(len + 1);
    if (result)
      mem_copy(result, buffer, len + 1);
  }
#else
  char buffer[PATH_MAX];
  if (realpath(path, buffer)) {
    size_t len = strlen(buffer);
    result = malloc(len + 1);
    if (result)
      mem_copy(result, buffer, len + 1);
  }
#endif

  if (!result) {
    log_error("Failed to resolve path: %s", path);
  }

  return result;
}

long file_get_size(FILE *fp) {
  if (!fp)
    return -1;

  if (fseek(fp, 0, SEEK_END) != 0) {
    log_error("Failed to seek to EOF");
    return -1;
  }

  long size = ftell(fp);
  if (size == -1L) {
    log_error("Failed to get file size");
    return -1;
  }

  if (fseek(fp, 0, SEEK_SET) != 0) {
    log_error("Failed to reset file position");
    return -1;
  }

  return size;
}

bool file_validate(const char *path) {
  if (!path)
    return false;

  FILE *fp = fopen(path, "rb");
  if (!fp) {
    log_error("Cannot open file: %s", path);
    return false;
  }

  long size = file_get_size(fp);
  (void)fclose(fp);

  if (size <= 0) {
    log_error("Invalid file size: %ld", size);
    return false;
  }

  if (size < MIN_FIRMWARE_SIZE) {
    log_error("File too small: %ld < %d", size, MIN_FIRMWARE_SIZE);
    return false;
  }

  return true;
}

static bool file_calculate_checksum(FILE *fp, uint16_t *out_checksum) {
  uint8_t buf[REPORT_SIZE];
  uint16_t checksum = 0;
  size_t bytes_read;

  while (!feof(fp)) {
    bytes_read = fread(buf, 1, REPORT_SIZE, fp);

    if (bytes_read > 0)
      checksum += checksum_calculate(buf, bytes_read);

    if (ferror(fp))
      return false;
  }

  *out_checksum = checksum;
  return true;
}

bool file_prepare_image(const char *path, long *out_size,
                        uint16_t *out_checksum, bool is_jumploader) {
  if (!path || !out_size || !out_checksum)
    return false;

  FILE *fp = fopen(path, "r+b");
  if (!fp) {
    log_error("Cannot open file: %s", path);
    return false;
  }

  long size = file_get_size(fp);
  if (size < 0) {
    (void)fclose(fp);
    return false;
  }

  /* Pad jumploader to DEFAULT_OFFSET if needed */
  if (is_jumploader && size < DEFAULT_OFFSET) {
    log_info("Padding jumploader to %d bytes", DEFAULT_OFFSET);
    if (truncate(path, DEFAULT_OFFSET) != 0) {
      log_error("Failed to truncate file");
      (void)fclose(fp);
      return false;
    }
    size = DEFAULT_OFFSET;
  }

  /* Align to REPORT_SIZE */
  long aligned_size = size;
  if (size % REPORT_SIZE != 0) {
    aligned_size = ((size / REPORT_SIZE) + 1) * REPORT_SIZE;
    log_info("Aligning file: %ld -> %ld bytes", size, aligned_size);
    if (truncate(path, aligned_size) != 0) {
      log_error("Failed to align file");
      (void)fclose(fp);
      return false;
    }
  }

  /* Calculate checksum in one pass */
  if (fseek(fp, 0, SEEK_SET) != 0) {
    log_error("Failed to reset file position for checksum");
    (void)fclose(fp);
    return false;
  }

  uint16_t checksum = 0;
  if (!file_calculate_checksum(fp, &checksum)) {
    log_error("Failed to read file for checksum");
    (void)fclose(fp);
    return false;
  }

  (void)fclose(fp);
  *out_size = aligned_size;
  *out_checksum = checksum;
  return true;
}
