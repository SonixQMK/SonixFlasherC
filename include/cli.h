#ifndef SONIXFLASHER_CLI_H
#define SONIXFLASHER_CLI_H

#include "types.h"

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

#endif /* SONIXFLASHER_CLI_H */
