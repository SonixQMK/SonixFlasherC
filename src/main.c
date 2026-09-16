#include "checksum.h"
#include "chip.h"
#include "cli.h"
#include "config.h"
#include "device.h"
#include "file_util.h"
#include "flash.h"
#include "log.h"
#include "types.h"
#include <stdlib.h>
#include <unistd.h>

static void cleanup_and_exit(device_t *dev, char *file_path, int code) {
  if (dev)
    device_close(dev);
  if (file_path)
    free(file_path);
  exit(code);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    cli_print_usage(argv[0]);
    return 1;
  }

  cli_args_t args = {0};
  if (!cli_parse(argc, argv, &args)) {
    return 1;
  }

  log_init(args.verbose ? LOG_DEBUG : LOG_INFO);

  /* Resolve absolute path */
  char *abs_path = file_get_absolute_path(args.flash.file_path);
  if (!abs_path) {
    return 1;
  }
  args.flash.file_path = abs_path;

  /* Validate file */
  if (!file_validate(args.flash.file_path)) {
    free(abs_path);
    return 1;
  }

  /* Prepare firmware image (pad/align/checksum) */
  long prepared_size = 0;
  uint16_t firmware_checksum = 0;
  if (!file_prepare_image(args.flash.file_path, &prepared_size,
                          &firmware_checksum, args.flash.is_jumploader)) {
    free(abs_path);
    return 1;
  }

  log_info("Firmware: %s (size: %ld bytes, checksum: 0x%04x)",
           args.flash.file_path, prepared_size, firmware_checksum);
  log_info("Target device: VID:[0x%04x] PID:[0x%04x]", args.vid, args.pid);
  log_info("");

  /* Open device */
  device_t dev = {0};
  if (!device_open(&dev, args.vid, args.pid)) {
    cleanup_and_exit(&dev, abs_path, 1);
  }

  /* Initialize protocol - Get FW version */
  if (!device_init_protocol(&dev, &args.reboot)) {
    cleanup_and_exit(&dev, abs_path, 1);
  }

  log_info("Device initialized: %s", chip_name(dev.chip_family));

  for (int i = FLASH_CONFIRM_DELAY_SEC; i > 0; i--) {
    log_raw("Starting flash in %d seconds...\r", i);
    sleep(IO_DELAY_SEC);
  }
  log_info("");
  log_info("Flashing now!");
  log_info("");

  /* Reset code security to CS0 if needed */
  if (!device_set_code_security(&dev, dev.cs_level_0)) {
    cleanup_and_exit(&dev, abs_path, 1);
  }
  log_info("");
  sleep(IO_DELAY_SEC);

  /* Erase flash */
  if (dev.chip_family != CHIP_F240B && dev.chip_family != CHIP_F260) {
    if (!flash_erase(&dev)) {
      cleanup_and_exit(&dev, abs_path, 1);
    }
    sleep(IO_DELAY_SEC);

    /* Verify erase via checksum */
    if (!flash_verify_checksum(&dev, dev.blank_checksum)) {
      log_error("Erase verification failed");
      cleanup_and_exit(&dev, abs_path, 1);
    }
    log_info("");
    sleep(IO_DELAY_SEC);
  }

  /* Program flash (send data + enable program) */
  if (!flash_program(&dev, &args.flash)) {
    cleanup_and_exit(&dev, abs_path, 1);
  }
  log_info("");
  sleep(IO_DELAY_SEC);

  if (args.flash.offset == 0) {
    /* Verify programmed checksum matches firmware */
    if (!flash_verify_checksum(&dev, firmware_checksum)) {
      log_error("Program verification failed");
      cleanup_and_exit(&dev, abs_path, 1);
    }
    log_info("");
    sleep(IO_DELAY_SEC);
  } else {
    /* Partial flash: print device checksum informational only */
    uint16_t device_checksum = 0;
    if (device_get_checksum(&dev, &device_checksum)) {
      log_info("Partial flash (offset 0x%04x) - device checksum: 0x%04x",
               args.flash.offset, device_checksum);
      log_info("");
    }
  }

  /* Reboot to user mode */
  if (!device_reboot_to_user_mode(&dev)) {
    log_warn("Warning: Device reboot command may have failed");
  }
  log_info("");
  log_info("=== FLASHING COMPLETED SUCCESSFULLY ===");
  sleep(IO_DELAY_SEC);

  cleanup_and_exit(&dev, abs_path, 0);
}
