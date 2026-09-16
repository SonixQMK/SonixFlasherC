#ifndef SONIXFLASHER_CONFIG_H
#define SONIXFLASHER_CONFIG_H

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <limits.h>
#endif

#include <hidapi.h>

/* Protocol Constants */
#define REPORT_SIZE 64
#define MAX_ATTEMPTS 5
#define RETRY_DELAY_MS 100
#define IO_DELAY_SEC 1
#define HID_WAIT_SEC 3
#define FLASH_CONFIRM_DELAY_SEC 5

/* Firmware Constraints */
#define MIN_FIRMWARE_SIZE 0x100
#define DEFAULT_OFFSET 0x200

/* Application Info */
#define APP_NAME "sonixflasher"
#define APP_VERSION "2.0.8"

#endif /* SONIXFLASHER_CONFIG_H */
