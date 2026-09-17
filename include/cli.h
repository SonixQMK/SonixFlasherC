#ifndef SONIXFLASHER_CLI_H
#define SONIXFLASHER_CLI_H

#include "types.h"

typedef struct {
  const char *name;
  uint16_t vid;
  uint16_t pid;
} known_device_t;

extern const known_device_t KNOWN_DEVICES[];
extern const size_t KNOWN_DEVICES_COUNT;

typedef struct {
  flash_config_t flash;
  reboot_config_t reboot;
  uint16_t vid;
  uint16_t pid;
  bool verbose;
} cli_args_t;

bool cli_parse(int argc, char *argv[], cli_args_t *args);
void cli_print_version(void);
void cli_print_usage(const char *prog_name);
void cli_print_devices(void);
void cli_print_connected_devices(void);

#endif /* SONIXFLASHER_CLI_H */
