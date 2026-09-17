#include "chip.h"
#include "cli.h"
#include "config.h"
#include "device.h"
#include "file_util.h"
#include "flash.h"
#include "log.h"
#include "types.h"
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

static void cleanup_and_exit(device_t *dev, char *file_path, int code) {
  if (dev)
    device_close(dev);
  if (file_path)
    free(file_path);
  exit(code);
}

static int run_user_mode(cli_args_t *args) {
  device_t dev = {0};

  if (!device_open(&dev, args->vid, args->pid)) {
    cleanup_and_exit(&dev, NULL, 1);
  }

  /* Best-effort protocol init; a device already stuck in bootloader may
   * not answer every command the same way, but we still want the FW
   * version query so device_reboot_to_user_mode() has a valid handle. */
  if (!device_init_protocol(&dev, &args->reboot)) {
    log_warn("Protocol init failed, attempting reboot anyway");
  }

  if (!device_reboot_to_user_mode(&dev)) {
    log_error("Failed to reboot device to user mode");
    cleanup_and_exit(&dev, NULL, 1);
  }

  log_info("Device rebooted to user mode");
  cleanup_and_exit(&dev, NULL, 0);
  return 0;
}

static int run_info(cli_args_t *args) {
  device_t dev = {0};

  if (!device_open(&dev, args->vid, args->pid)) {
    cleanup_and_exit(&dev, NULL, 1);
  }

  if (!device_init_protocol(&dev, &args->reboot)) {
    log_error("Failed to query device info");
    cleanup_and_exit(&dev, NULL, 1);
  }

  uint16_t device_checksum = 0;
  bool checksum_valid = device_get_checksum(&dev, &device_checksum);

  log_info("Device:          %s", chip_name(dev.chip_family));
  log_info("VID:PID:         0x%04x:0x%04x", dev.vid, dev.pid);
  log_info("ROM size:        %u KB", dev.rom_size_kb);
  log_info("ROM pages:       %u", dev.rom_pages);
  log_info("Max firmware:    %u bytes", dev.max_firmware_size);
  log_info("Blank checksum:  0x%04x", dev.blank_checksum);
  log_info("Security level:  CS%d", dev.security_level);
  log_info("Code option:     0x%04x", dev.code_option);

  if (checksum_valid) {
    log_info("Flash checksum:  0x%04x", device_checksum);

    if (device_checksum == dev.blank_checksum) {
      log_info("Flash state:     blank");
    } else {
      log_info("Flash state:     programmed or unknown");
    }
  } else {
    log_info("Flash checksum:  unavailable");
    log_warn("Could not retrieve device checksum");
  }

  cleanup_and_exit(&dev, NULL, 0);
  return 0;
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

  if (args.mode == CLI_MODE_USER_MODE) {
    return run_user_mode(&args);
  }

  if (args.mode == CLI_MODE_INFO) {
    return run_info(&args);
  }

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
  log_info("\n");

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
    log_raw("Starting flash in %d seconds...", i);
    sleep(IO_DELAY_SEC);
  }
  log_info("\n");
  log_info("Flashing now!");
  log_info("\n");

  /* Reset code security to CS0 if needed */
  if (!device_set_code_security(&dev, dev.cs_level_0)) {
    cleanup_and_exit(&dev, abs_path, 1);
  }
  log_info("\n");
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
    log_info("\n");
    sleep(IO_DELAY_SEC);
  }

  /* Program flash (send data + enable program) */
  if (!flash_program(&dev, &args.flash)) {
    cleanup_and_exit(&dev, abs_path, 1);
  }
  log_info("\n");
  sleep(IO_DELAY_SEC);

  if (args.flash.offset == 0) {
    /* Verify programmed checksum matches firmware */
    if (!flash_verify_checksum(&dev, firmware_checksum)) {
      log_error("Program verification failed");
      cleanup_and_exit(&dev, abs_path, 1);
    }
    log_info("\n");
    sleep(IO_DELAY_SEC);
  } else {
    /* Partial flash: print device checksum informational only */
    uint16_t device_checksum = 0;
    if (device_get_checksum(&dev, &device_checksum)) {
      log_info("Partial flash (offset 0x%04x) - device checksum: 0x%04x",
               args.flash.offset, device_checksum);
      log_info("\n");
    }
  }

  /* Reboot to user mode */
  if (!device_reboot_to_user_mode(&dev)) {
    log_warn("Warning: Device reboot command may have failed");
  }
  log_info("\n");
  log_info("=== FLASHING COMPLETED SUCCESSFULLY ===");
  sleep(IO_DELAY_SEC);

  cleanup_and_exit(&dev, abs_path, 0);
}