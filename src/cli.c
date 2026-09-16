#include "cli.h"
#include "config.h"
#include "log.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_vid_pid(const char *str, uint16_t *vid, uint16_t *pid) {
  if (!str || !vid || !pid)
    return -1;

  unsigned int parsed_vid;
  unsigned int parsed_pid;
  char extra;

  if (sscanf(str, "%x/%x%c", &parsed_vid, &parsed_pid, &extra) == 2 ||
      sscanf(str, "%x:%x%c", &parsed_vid, &parsed_pid, &extra) == 2) {
    if (parsed_vid <= UINT16_MAX && parsed_pid <= UINT16_MAX) {
      *vid = (uint16_t)parsed_vid;
      *pid = (uint16_t)parsed_pid;
      return 0;
    }
  }

  log_error("Invalid VID/PID format: %s (expected XXXX/XXXX)", str);
  return -1;
}

void cli_print_devices(void) {
  printf("Supported devices:\n");
  printf("+-----------------+-------+-------+\n");
  printf("| Device          | VID   | PID   |\n");
  printf("+-----------------+-------+-------+\n");
  printf("| SONIX SN32F22X  | 0c45  | 7900  |\n");
  printf("| SONIX SN32F23X  | 0c45  | 7900  |\n");
  printf("| SONIX SN32F24X  | 0c45  | 7900  |\n");
  printf("| SONIX SN32F24XB | 0c45  | 7040  |\n");
  printf("| SONIX SN32F24XC | 0c45  | 7160  |\n");
  printf("| SONIX SN32F26X  | 0c45  | 7010  |\n");
  printf("| SONIX SN32F28X  | 0c45  | 7120  |\n");
  printf("| SONIX SN32F29X  | 0c45  | 7140  |\n");
  printf("+-----------------+-------+-------+\n");
}

void cli_print_version(void) { printf("%s %s\n", APP_NAME, APP_VERSION); }

void cli_print_usage(const char *prog_name) {
  if (!prog_name)
    prog_name = APP_NAME;

  printf("Usage: %s [OPTIONS]\n", prog_name);
  printf("\n");
  printf("Options:\n");
  printf("  -v, --vid-pid VID/PID    Device VID/PID (required)\n");
  printf("  -f, --file PATH          Firmware file path (required)\n");
  printf("  -o, --offset ADDR        Flash offset (default: 0)\n");
  printf("  -j, --jumploader         Flash jumploader instead of firmware\n");
  printf("  -r, --reboot TYPE        Request reboot before flashing "
         "(sonix/evision/hfd)\n");
  printf("  -k, --no-offset-check    Skip offset validation for F26X\n");
  printf("  -d, --debug              Enable debug output\n");
  printf("  -l, --list-devices       List supported devices\n");
  printf("  -V, --version            Print version\n");
  printf("  -h, --help               Print this help\n");
  printf("\n");
  printf("Examples:\n");
  printf("  %s -v 0c45/7040 -f firmware.bin\n", prog_name);
  printf("  %s -v 0c45/7040 -f bootloader.bin -j -o 0x200\n", prog_name);
  printf("  %s -v 0c45/7010 -f firmware.bin -r sonix\n", prog_name);
}

bool cli_parse(int argc, char *argv[], cli_args_t *args) {
  if (!args)
    return false;

  /* Initialize defaults */
  memset(args, 0, sizeof(cli_args_t));
  args->flash.offset = 0;
  args->reboot.type = "sonix";

  static struct option opts[] = {{"vid-pid", required_argument, NULL, 'v'},
                                 {"file", required_argument, NULL, 'f'},
                                 {"offset", required_argument, NULL, 'o'},
                                 {"jumploader", no_argument, NULL, 'j'},
                                 {"reboot", required_argument, NULL, 'r'},
                                 {"no-offset-check", no_argument, NULL, 'k'},
                                 {"debug", no_argument, NULL, 'd'},
                                 {"list-devices", no_argument, NULL, 'l'},
                                 {"version", no_argument, NULL, 'V'},
                                 {"help", no_argument, NULL, 'h'},
                                 {NULL, 0, NULL, 0}};

  int c, idx = 0;
  while ((c = getopt_long(argc, argv, "v:f:o:jr:kd?lVh", opts, &idx)) != -1) {
    switch (c) {
    case 'v':
      if (parse_vid_pid(optarg, &args->vid, &args->pid) != 0)
        return false;
      break;

    case 'f':
      args->flash.file_path = optarg;
      break;

    case 'o':
      args->flash.offset = strtoul(optarg, NULL, 0);
      break;

    case 'j':
      args->flash.is_jumploader = true;
      break;

    case 'r':
      args->reboot.type = optarg;
      args->reboot.requested = true;
      break;

    case 'k':
      args->flash.skip_offset_check = true;
      break;

    case 'd':
      args->verbose = true;
      break;

    case 'l':
      cli_print_devices();
      exit(0);
      break;

    case 'V':
      cli_print_version();
      exit(0);
      break;

    case 'h':
    case '?':
    default:
      cli_print_usage(argv[0]);
      exit(c == 'h' ? 0 : 1);
    }
  }

  /* Validate required arguments */
  if (args->vid == 0 || args->pid == 0) {
    log_error("Missing or invalid VID/PID");
    return false;
  }

  if (!args->flash.file_path) {
    log_error("Missing firmware file path");
    return false;
  }

  return true;
}
