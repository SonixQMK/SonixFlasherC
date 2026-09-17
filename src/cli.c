#include "cli.h"
#include "config.h"
#include "device.h"
#include "log.h"
#include "usb_device.h"
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_vid_pid(const char *str, uint16_t *vid, uint16_t *pid) {
  if (!str || !vid || !pid)
    return -1;

  char *end;
  unsigned long parsed_vid = strtoul(str, &end, 16);

  if (end == str || (*end != '/' && *end != ':')) {
    log_error("Invalid VID/PID format: %s (expected XXXX/XXXX)", str);
    return -1;
  }

  char *pid_start = end + 1;
  unsigned long parsed_pid = strtoul(pid_start, &end, 16);

  if (end == pid_start || *end != '\0') {
    log_error("Invalid VID/PID format: %s (expected XXXX/XXXX)", str);
    return -1;
  }

  if (parsed_vid > UINT16_MAX || parsed_pid > UINT16_MAX) {
    log_error("Invalid VID/PID value: %s", str);
    return -1;
  }

  *vid = (uint16_t)parsed_vid;
  *pid = (uint16_t)parsed_pid;

  return 0;
}

static bool parse_reboot_type(const char *str, const char **type) {
  if (!str || !type)
    return false;

  if (strcmp(str, "sonix") == 0 || strcmp(str, "evision") == 0 ||
      strcmp(str, "hfd") == 0) {
    *type = str;
    return true;
  }

  log_error("Invalid reboot type: %s (expected sonix, evision, or hfd)", str);
  return false;
}

const known_device_t KNOWN_DEVICES[] = {
    {"SONIX SN32F22X", VID_SONIX, PID_SN22X},
    {"SONIX SN32F23X", VID_SONIX, PID_SN23X},
    {"SONIX SN32F24X", VID_SONIX, PID_SN24X},
    {"SONIX SN32F24XB", VID_SONIX, PID_SN24XB},
    {"SONIX SN32F24XC", VID_SONIX, PID_SN24XC},
    {"SONIX SN32F26X", VID_SONIX, PID_SN26X},
    {"SONIX SN32F28X", VID_SONIX, PID_SN28X},
    {"SONIX SN32F29X", VID_SONIX, PID_SN29X},
};

const size_t KNOWN_DEVICES_COUNT =
    sizeof(KNOWN_DEVICES) / sizeof(KNOWN_DEVICES[0]);

void cli_print_devices(void) {
  printf("Supported devices:\n");
  printf("+-----------------+-------+-------+\n");
  printf("| Device          | VID   | PID   |\n");
  printf("+-----------------+-------+-------+\n");

  for (size_t i = 0; i < KNOWN_DEVICES_COUNT; i++) {
    printf("| %-15s | %04x  | %04x  |\n", KNOWN_DEVICES[i].name,
           KNOWN_DEVICES[i].vid, KNOWN_DEVICES[i].pid);
  }

  printf("+-----------------+-------+-------+\n");
}

void cli_print_connected_devices(void) {
  printf("Scanning for connected Sonix devices...\n\n");

  if (!usb_device_init()) {
    log_error("USB initialization failed");
    return;
  }

  bool any_found = false;

  printf("+-----------------+-------+-------+\n");
  printf("| Device          | VID   | PID   |\n");
  printf("+-----------------+-------+-------+\n");

  for (size_t i = 0; i < KNOWN_DEVICES_COUNT; i++) {
    if (usb_device_is_present(KNOWN_DEVICES[i].vid, KNOWN_DEVICES[i].pid)) {
      printf("| %-15s | %04x  | %04x  |\n", KNOWN_DEVICES[i].name,
             KNOWN_DEVICES[i].vid, KNOWN_DEVICES[i].pid);
      any_found = true;
    }
  }

  printf("+-----------------+-------+-------+\n");

  if (!any_found) {
    printf("\nNo supported devices found.\n");
    printf("If your device is in normal mode (not bootloader), it may not "
           "be detectable.\n");
    printf("Try requesting a reboot with -r <type> once connected.\n");
  }

  usb_device_exit();
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
         "(sonix/evision/hfd default: sonix)\n");
  printf("  -k, --no-offset-check    Skip offset validation for F26X\n");
  printf("  -d, --debug              Enable debug output\n");
  printf("  -l, --list-devices       List supported devices\n");
  printf("  -c, --list-connected     Scan and list connected Sonix devices\n");
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

  static struct option opts[] = {{"vid-pid", required_argument, NULL, 'v'},
                                 {"file", required_argument, NULL, 'f'},
                                 {"offset", required_argument, NULL, 'o'},
                                 {"jumploader", no_argument, NULL, 'j'},
                                 {"reboot", no_argument, NULL, 'r'},
                                 {"no-offset-check", no_argument, NULL, 'k'},
                                 {"debug", no_argument, NULL, 'd'},
                                 {"list-devices", no_argument, NULL, 'l'},
                                 {"list-connected", no_argument, NULL, 'c'},
                                 {"version", no_argument, NULL, 'V'},
                                 {"help", no_argument, NULL, 'h'},
                                 {NULL, 0, NULL, 0}};

  int c, idx = 0;
  while ((c = getopt_long(argc, argv, "v:f:o:jrkd?lcVh", opts, &idx)) != -1) {
    switch (c) {
    case 'v':
      if (parse_vid_pid(optarg, &args->vid, &args->pid) != 0)
        return false;
      break;

    case 'f':
      args->flash.file_path = optarg;
      break;

    case 'o':
      args->flash.offset = (uint32_t)strtoul(optarg, NULL, 0);
      break;

    case 'j':
      args->flash.is_jumploader = true;
      break;

    case 'r':
      args->reboot.requested = true;
      args->reboot.type = "sonix";

      if (optind < argc && argv[optind][0] != '-') {
        if (!parse_reboot_type(argv[optind], &args->reboot.type))
          return false;

        optind++;
      }
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

    case 'c':
      cli_print_connected_devices();
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
