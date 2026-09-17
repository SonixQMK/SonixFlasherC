#ifndef SONIXFLASHER_LOG_H
#define SONIXFLASHER_LOG_H

#if defined(__GNUC__) || defined(__clang__)
#define PRINTF_LIKE(fmt_idx, args_idx)                                         \
  __attribute__((format(printf, fmt_idx, args_idx)))
#else
#define PRINTF_LIKE(fmt_idx, args_idx)
#endif

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
void log_error(const char *fmt, ...) PRINTF_LIKE(1, 2);
void log_warn(const char *fmt, ...) PRINTF_LIKE(1, 2);
void log_info(const char *fmt, ...) PRINTF_LIKE(1, 2);
void log_debug(const char *fmt, ...) PRINTF_LIKE(1, 2);
void log_raw(const char *fmt, ...) PRINTF_LIKE(1, 2);
void log_hex(const char *label, const uint8_t *data, size_t len);

#endif /* SONIXFLASHER_LOG_H */
