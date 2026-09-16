#include "log.h"
#include <stdarg.h>

static log_level_t current_level = LOG_INFO;

void log_init(log_level_t level) { current_level = level; }

static void log_print(log_level_t level, const char *fmt, va_list args) {
  if (level > current_level)
    return;
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
}

static void log_print_raw(const char *fmt, va_list args) {
  vfprintf(stderr, fmt, args);
  fflush(stderr);
}

void log_error(const char *fmt, ...) {
  if (!fmt)
    return;
  va_list args;
  va_start(args, fmt);
  log_print(LOG_ERROR, fmt, args);
  va_end(args);
}

void log_warn(const char *fmt, ...) {
  if (!fmt)
    return;
  va_list args;
  va_start(args, fmt);
  log_print(LOG_WARN, fmt, args);
  va_end(args);
}

void log_info(const char *fmt, ...) {
  if (!fmt)
    return;
  va_list args;
  va_start(args, fmt);
  log_print(LOG_INFO, fmt, args);
  va_end(args);
}

void log_debug(const char *fmt, ...) {
  if (!fmt)
    return;
  va_list args;
  va_start(args, fmt);
  log_print(LOG_DEBUG, fmt, args);
  va_end(args);
}

void log_raw(const char *fmt, ...) {
  if (!fmt)
    return;
  va_list args;
  va_start(args, fmt);
  log_print_raw(fmt, args);
  va_end(args);
}

void log_hex(const char *label, const uint8_t *data, size_t len) {
  if (!data || len == 0)
    return;
  if (current_level < LOG_DEBUG)
    return;

  const size_t bytes_per_line = 32;
  size_t offset = 0;

  while (offset < len) {
    size_t chunk =
        (len - offset < bytes_per_line) ? (len - offset) : bytes_per_line;

    fprintf(stderr, "%s: ", label ? label : "Data");
    if (len > bytes_per_line) {
      fprintf(stderr, "[0x%04zx] ", offset);
    }

    for (size_t i = 0; i < chunk; i++) {
      fprintf(stderr, "%02x", data[offset + i]);
    }
    fprintf(stderr, "\n");

    offset += chunk;
  }
}
