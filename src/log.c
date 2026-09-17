#include "log.h"
#include <stdarg.h>
#include <stdio.h>

static log_level_t current_level = LOG_INFO;
static bool raw_line_pending = false;

#define CLEAR_EOL "\033[K"

void log_init(log_level_t level) { current_level = level; }

static void log_end_raw_line(void) {
  if (raw_line_pending) {
    fprintf(stderr, "\n");
    raw_line_pending = false;
  }
}

static void log_print(log_level_t level, const char *fmt, va_list args)
    PRINTF_LIKE(2, 0);

static void log_print_raw(const char *fmt, va_list args) PRINTF_LIKE(1, 0);

static void log_print(log_level_t level, const char *fmt, va_list args) {
  if (level > current_level)
    return;

  log_end_raw_line();

  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
}

static void log_print_raw(const char *fmt, va_list args) {
  fprintf(stderr, "\r" CLEAR_EOL);
  vfprintf(stderr, fmt, args);
  fflush(stderr);
  raw_line_pending = true;
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

  log_end_raw_line();

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