#ifndef SONIXFLASHER_FILE_UTIL_H
#define SONIXFLASHER_FILE_UTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

char *file_get_absolute_path(const char *path);
long file_get_size(FILE *fp);
bool file_validate(const char *path);
bool file_prepare_image(const char *path, long *out_size,
                        uint16_t *out_checksum, bool is_jumploader);

#endif /* SONIXFLASHER_FILE_UTIL_H */
