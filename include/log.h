#ifndef SONIXFLASHER_LOG_H
#define SONIXFLASHER_LOG_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
  LOG_SILENT,
  LOG_ERROR,
  LOG_WARN,
  LOG_INFO,
  LOG_DEBUG
} log_level_t;

void log_init(log_level_t level);
void log_error(const char *fmt, ...);
void log_warn(const char *fmt, ...);
void log_info(const char *fmt, ...);
void log_debug(const char *fmt, ...);
void log_hex(const char *label, const uint8_t *data, size_t len);
void log_raw(const char *fmt, ...);
void log_hex(const char *label, const uint8_t *data, size_t len);

#endif /* SONIXFLASHER_LOG_H */
