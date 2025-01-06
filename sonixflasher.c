#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

#include <hidapi.h>

#define REPORT_SIZE 64
#define USER_ROM_SIZE_SN32F260 30   // in KB
#define USER_ROM_SIZE_SN32F220 16   // in KB
#define USER_ROM_SIZE_SN32F230 32   // in KB
#define USER_ROM_SIZE_SN32F240 64   // in KB
#define USER_ROM_SIZE_SN32F240B 64  // in KB
#define USER_ROM_SIZE_SN32F240C 128 // in KB
#define USER_ROM_SIZE_SN32F280 128  // in KB
#define USER_ROM_SIZE_SN32F290 256  // in KB
#define USER_ROM_SIZE_KB(x) ((x) * 1024)

#define USER_ROM_PAGES_SN32F260 480
#define USER_ROM_PAGES_SN32F220 16
#define USER_ROM_PAGES_SN32F230 32
#define USER_ROM_PAGES_SN32F240 64
#define USER_ROM_PAGES_SN32F240B 1024
#define USER_ROM_PAGES_SN32F240C 128
#define USER_ROM_PAGES_SN32F280 128
#define USER_ROM_PAGES_SN32F290 256

#define QMK_OFFSET_DEFAULT 0x200
#define MIN_FIRMWARE 0x100

#define CMD_BASE 0x55AA
#define CMD_GET_FW_VERSION 0x1
#define CMD_COMPARE_CODE_OPTION 0x2
#define CMD_SET_ENCRYPTION_ALGO 0x3
#define CMD_ENABLE_ERASE 0x4
#define CMD_ENABLE_PROGRAM 0x5
#define CMD_GET_CHECKSUM 0x6
#define CMD_RETURN_USER_MODE 0x7
#define CMD_SET_CS 0x8
#define CMD_GET_CS 0x9
#define CMD_VERIFY(x) ((CMD_BASE << 8) | (x))

#define CMD_ACK 0xFAFAFAFA
#define LAST_CHUNK_OFFSET (REPORT_SIZE - sizeof(uint32_t))

#define SN240 1
#define SN260 2
#define SN240B 3
#define SN280 4
#define SN290 5
#define SN240C 6

#define CS0_0 0x0000
#define CS0_1 0xFFFF

#define CS1 0x5A5A
#define CS2 0xA5A5
#define CS3 0x55AA

#define SONIX_VID 0x0c45
#define SN229_PID 0x7900
#define SN239_PID SN229_PID
#define SN249_PID SN229_PID
#define SN248B_PID 0x7040
#define SN248C_PID 0x7160
#define SN268_PID 0x7010
#define SN289_PID 0x7120
#define SN299_PID 0x7140

#define EVISION_VID 0x320F
#define APPLE_VID 0x05ac

#define MAX_ATTEMPTS 5
#define RETRY_DELAY_MS 100

#define PROJECT_NAME "sonixflasher"
#define PROJECT_VER "2.0.8"

uint16_t           BLANK_CHECKSUM   = 0x0000;
uint16_t           CS0              = CS0_0;
uint16_t           USER_ROM_SIZE    = USER_ROM_SIZE_SN32F260;
uint16_t           USER_ROM_PAGES   = USER_ROM_PAGES_SN32F260;
long               MAX_FIRMWARE     = USER_ROM_SIZE_KB(USER_ROM_SIZE_SN32F260);
bool               flash_jumploader = false;
bool               debug            = false;
static uint16_t    code_option      = 0x0000; // Initial Code Option Table
int                chip;
int                cs_level;
const unsigned int known_isp_pids[] = {SN229_PID, SN239_PID, SN249_PID, SN248B_PID, SN248C_PID, SN268_PID, SN289_PID, SN299_PID};

static void print_vidpid_table() {
    printf("Supported VID/PID pairs:\n");
    printf("+-----------------+------------+------------+\n");
    printf("|      Device     |    VID     |    PID     |\n");
    printf("+-----------------+------------+------------+\n");
    printf("| SONIX SN32F22x  | 0x%04X     | 0x%04X     |\n", SONIX_VID, SN229_PID);
    printf("| SONIX SN32F23x  | 0x%04X     | 0x%04X     |\n", SONIX_VID, SN239_PID);
    printf("| SONIX SN32F24x  | 0x%04X     | 0x%04X     |\n", SONIX_VID, SN249_PID);
    printf("| SONIX SN32F24xB | 0x%04X     | 0x%04X     |\n", SONIX_VID, SN248B_PID);
    printf("| SONIX SN32F24xC | 0x%04X     | 0x%04X     |\n", SONIX_VID, SN248C_PID);
    printf("| SONIX SN32F26x  | 0x%04X     | 0x%04X     |\n", SONIX_VID, SN268_PID);
    printf("| SONIX SN32F28x  | 0x%04X     | 0x%04X     |\n", SONIX_VID, SN289_PID);
    printf("| SONIX SN32F29x  | 0x%04X     | 0x%04X     |\n", SONIX_VID, SN299_PID);
    printf("+-----------------+------------+------------+\n");
}

static void print_usage(char *m_name) {
    fprintf(stderr,
            "Usage: \n"
            "  %s <cmd> [options]\n"
            "where <cmd> is one of:\n"
            "  --vidpid -v      Set VID for device to flash \n"
            "  --offset -o      Set flashing offset (default: 0)\n"
            "  --file -f        Binary of the firmware to flash (*.bin extension) \n"
            "  --jumploader -j  Define if we are flashing a jumploader \n"
            "  --reboot -r      Request bootloader reboot in OEM firmware (options: sonix, evision, hfd) \n"
            "  --debug -d       Enable debug mode \n"
            "  --nooffset -k    Disable offset checks \n"
            "  --list-vidpid -l Display supported VID/PID pairs \n"
            "  --version -V     Print version information \n"
            "\n"
            "Examples: \n"
            ". Flash jumploader to device w/ vid/pid 0x0c45/0x7040 \n"
            "   sonixflasher --vidpid 0c45/7040 --file fw.bin -j\n"
            ". Flash fw to device w/ vid/pid 0x0c45/0x7040 and offset 0x200\n"
            "   sonixflasher --vidpid 0c45/7040 --file fw.bin -o 0x200\n"
            "\n"
            ""
            "",
            m_name);
}

// Display program version
static void display_version(char *m_name) {
    fprintf(stderr, "%s " PROJECT_VER "\n", m_name);
}

void cleanup(hid_device *handle) {
    if (handle) hid_close(handle);
    if (hid_exit() != 0) {
        fprintf(stderr, "ERROR: Could not close the device.\n");
    }
}

void error(hid_device *handle) {
    cleanup(handle);
    exit(1);
}

void clear_buffer(unsigned char *data, size_t length) {
    for (int i = 0; i < length; i++)
        data[i] = 0;
}

void print_buffer(unsigned char *data, size_t length) {
    printf("Sending Report...\n");
    for (int i = 0; i < length; i++)
        printf("%02x", data[i]);
    printf("\n");
}

bool read_response_16(unsigned char *data, uint32_t offset, uint16_t expected_result, uint16_t *resp) {
    uint16_t r = *resp;

    memcpy(&r, data + offset, sizeof(uint16_t));

    *resp = r;
    return r == expected_result;
}

bool read_response_32(unsigned char *data, uint32_t offset, uint32_t expected_result, uint32_t *resp) {
    uint32_t r = *resp;

    memcpy(&r, data + offset, sizeof(uint32_t));

    *resp = r;
    return r == expected_result;
}

void write_buffer_32(unsigned char *data, uint32_t cmd) {
    memcpy(data, &cmd, 4);
}

void write_buffer_16(unsigned char *data, uint16_t cmd) {
    memcpy(data, &cmd, 2);
}

uint16_t checksum16(const unsigned char *data, size_t size) {
    uint16_t sum = 0;
    size_t   i;

    for (i = 0; i + 1 < size; i += 2) {
        uint16_t value = data[i] | (data[i + 1] << 8);
        sum += value;
    }

    if (i < size) {
        sum += data[i];
    }

    return sum;
}

void print_data(const unsigned char *data, int length) {
    for (int i = 0; i < length; i++) {
        if (i % 16 == 0) {
            if (i > 0) {
                printf("\n");
            }
            printf("%04x: ", i); // Print address offset
        }
        printf("%02x ", data[i]);
    }
    printf("\n");
}

bool is_known_isp_pid(unsigned int pid) {
    size_t num_known_pids = sizeof(known_isp_pids) / sizeof(known_isp_pids[0]);
    for (size_t i = 0; i < num_known_pids; ++i) {
        if (pid == known_isp_pids[i]) {
            return true;
        }
    }
    return false;
}

bool hid_set_feature(hid_device *dev, unsigned char *data, size_t length) {
    if (length > REPORT_SIZE) {
        fprintf(stderr, "ERROR: Report can't be more than %d bytes!! (Attempted: %zu bytes)\n", REPORT_SIZE, length);
        return false;
    }

    if (debug) {
        printf("\n");
        printf("Sending payload...\n");
        print_data(data, length);
    }

    // Set Report ID to 0 before passing to hidapi.
    // Allocate a send buffer with an extra byte
    unsigned char send_buf[REPORT_SIZE + 1];

    // Set the Report ID byte (0x00) at the start of the buffer
    send_buf[0] = 0x00;

    // Copy the data into the buffer, starting from the second byte
    memcpy(send_buf + 1, data, length);

    // Send the feature report using the send buffer
    if (hid_send_feature_report(dev, send_buf, length + 1) < 0) {
        fprintf(stderr, "ERROR: Error while writing command 0x%02x! Reason: %ls\n", data[0], hid_error(dev));
        return false;
    }

    return true;
}
int sn32_decode_chip(unsigned char *data) {
    // data[8-11] holds the bootloader version
    if (data[8] == 32) {
        printf("Sonix SN32 Detected.\n");
        printf("\n");
        printf("Checking variant... ");

        int sn32_family;
        switch (data[9]) {
            case SN240:
                switch (data[11]) {
                    case 1:
                        printf("220 Detected!\n");
                        USER_ROM_SIZE  = USER_ROM_SIZE_SN32F220;
                        USER_ROM_PAGES = USER_ROM_PAGES_SN32F220;
                        MAX_FIRMWARE   = USER_ROM_SIZE_KB(USER_ROM_SIZE);
                        CS0            = CS0_1;
                        BLANK_CHECKSUM = 0xe000;
                        sn32_family    = SN240;
                        break;
                    case 2:
                        printf("230 Detected!\n");
                        USER_ROM_SIZE  = USER_ROM_SIZE_SN32F230;
                        USER_ROM_PAGES = USER_ROM_PAGES_SN32F230;
                        MAX_FIRMWARE   = USER_ROM_SIZE_KB(USER_ROM_SIZE);
                        CS0            = CS0_1;
                        BLANK_CHECKSUM = 0xc000;
                        sn32_family    = SN240;
                        break;
                    case 3:
                        printf("240 Detected!\n");
                        USER_ROM_SIZE  = USER_ROM_SIZE_SN32F240;
                        USER_ROM_PAGES = USER_ROM_PAGES_SN32F240;
                        MAX_FIRMWARE   = USER_ROM_SIZE_KB(USER_ROM_SIZE);
                        CS0            = CS0_1;
                        BLANK_CHECKSUM = 0x8000;
                        sn32_family    = SN240;
                        break;
                    default:
                        printf("\n");
                        fprintf(stderr, "ERROR: Unsupported 2xx variant: %d.%d.%d, we don't support this chip.\n", data[9], data[10], data[11]);
                        sn32_family = 0;
                        break;
                }
                break;
            case SN260:
                printf("260 Detected!\n");
                USER_ROM_SIZE  = USER_ROM_SIZE_SN32F260;
                USER_ROM_PAGES = USER_ROM_PAGES_SN32F260;
                MAX_FIRMWARE   = USER_ROM_SIZE_KB(USER_ROM_SIZE);
                CS0            = CS0_0;
                BLANK_CHECKSUM = 0x8000;
                sn32_family    = SN260;
                break;
            case SN240B:
                printf("240B Detected!\n");
                USER_ROM_SIZE  = USER_ROM_SIZE_SN32F240B;
                USER_ROM_PAGES = USER_ROM_PAGES_SN32F240B;
                MAX_FIRMWARE   = USER_ROM_SIZE_KB(USER_ROM_SIZE);
                CS0            = CS0_0;
                BLANK_CHECKSUM = 0x8000;
                sn32_family    = SN240B;
                break;
            case SN280:
                printf("280 Detected!\n");
                USER_ROM_SIZE  = USER_ROM_SIZE_SN32F280;
                USER_ROM_PAGES = USER_ROM_PAGES_SN32F280;
                MAX_FIRMWARE   = USER_ROM_SIZE_KB(USER_ROM_SIZE);
                CS0            = CS0_1;
                BLANK_CHECKSUM = 0x0000;
                sn32_family    = SN280;
                break;
            case SN290:
                printf("290 Detected!\n");
                USER_ROM_SIZE  = USER_ROM_SIZE_SN32F290;
                USER_ROM_PAGES = USER_ROM_PAGES_SN32F290;
                MAX_FIRMWARE   = USER_ROM_SIZE_KB(USER_ROM_SIZE);
                CS0            = CS0_1;
                BLANK_CHECKSUM = 0x0000;
                sn32_family    = SN290;
                break;
            case SN240C:
                printf("240C Detected!\n");
                USER_ROM_SIZE  = USER_ROM_SIZE_SN32F240C;
                USER_ROM_PAGES = USER_ROM_PAGES_SN32F240C;
                MAX_FIRMWARE   = USER_ROM_SIZE_KB(USER_ROM_SIZE);
                CS0            = CS0_1;
                BLANK_CHECKSUM = 0x0000;
                sn32_family    = SN240C;
                break;
            default:
                printf("\n");
                fprintf(stderr, "ERROR: Unsupported bootloader version: %d.%d.%d, we don't support this chip.\n", data[9], data[10], data[11]);
                sn32_family = 0;
                break;
        }

        return sn32_family;
    } else {
        fprintf(stderr, "ERROR: Unsupported family version: %d, we don't support this chip.\n", data[8]);
        return 0;
    }
}

bool sn32_check_isp_code_option(unsigned char *data) {
    uint16_t received_code_option = (data[12] << 8) | data[13];
    printf("Checking Code Option Table... Expected: 0x%04X Received: 0x%04X.\n", code_option, received_code_option);
    if (received_code_option != code_option) {
        printf("Updating Code Option Table from 0x%04X to 0x%04X\n", code_option, received_code_option);
        code_option = received_code_option;
        return false;
    }
    return true;
}

int sn32_get_code_security(unsigned char *data) {
    cs_level          = -1;
    uint16_t cs_value = (data[14] << 8) | data[15];

    switch (cs_value) {
        case CS0_0:
        case CS0_1:
            cs_level = 0;
            break;
        case CS1:
            cs_level = 1;
            break;
        case CS2:
            cs_level = 2;
            break;
        case CS3:
            cs_level = 3;
            break;
        default:
            fprintf(stderr, "ERROR: Unsupported Code Security value: 0x%04X, we don't support this chip.\n", cs_value);
            return cs_level;
    }

    printf("Current Security level: CS%d. Code Security value: 0x%04X.\n", cs_level, cs_value);
    return cs_level;
}

bool hid_get_feature(hid_device *dev, unsigned char *data, size_t data_size, uint32_t command) {
    clear_buffer(data, data_size);

    uint8_t attempt_no = 1;
    while (attempt_no <= MAX_ATTEMPTS) {
        clear_buffer(data, data_size);

        // Attempt to get the feature report
        int res = hid_get_feature_report(dev, data, data_size + 1);

        if (res == (data_size + 1)) {
            // Shift the data buffer to remove the Report ID
            memmove(data, data + 1, res - 1);

            if (debug) {
                printf("\n");
                printf("Received payload...\n");
                print_data(data, res - 1);
            }

            // Check the status directly in the data buffer
            unsigned int cmdreply = *((unsigned int *)(data));
            unsigned int status   = *((unsigned int *)(data + 4));
            if (cmdreply == CMD_VERIFY(command)) {
                if (status != CMD_ACK) {
                    fprintf(stderr, "ERROR: Invalid response status: 0x%08x, expected 0x%08x for command 0x%02x.\n", status, CMD_ACK, command & 0xFF);
                    return false;
                }

                // Success
                return true;
            } else {
                fprintf(stderr, "ERROR: Invalid response command: 0x%08x, expected command 0x%02x.\n", cmdreply, command & 0xFF);
                if ((cmdreply == CMD_VERIFY(CMD_ENABLE_PROGRAM)) && (status == CMD_ACK)) {
                    printf("Device progam pending. Please power cycle the device.\n");
                }
                return false;
            }
        } else if (res < 0) {
            // Error condition, such as abort pipe
            fprintf(stderr, "ERROR: Device busy or failed to get feature report, retrying...\n");
            attempt_no++;
            usleep(RETRY_DELAY_MS * 1000); // Delay before retrying
        } else {
            // Incorrect response length
            fprintf(stderr, "ERROR: Invalid response length for command 0x%02x: got %d, expected %zu.\n", command & 0xFF, res, data_size + 1);
            return false;
        }
    }

    // After retries failed
    fprintf(stderr, "ERROR: Failed to get feature report for command 0x%02x after %d retries.\n", command & 0xFF, attempt_no);
    return false;
}

bool send_magic_command(hid_device *dev, const uint32_t *command) {
    unsigned char buf[REPORT_SIZE];

    clear_buffer(buf, sizeof(buf));
    write_buffer_32(buf, command[0]);
    write_buffer_32(buf + sizeof(uint32_t), command[1]);
    uint8_t attempt_no = 1;
    while (!hid_set_feature(dev, buf, REPORT_SIZE) && attempt_no <= MAX_ATTEMPTS) // Try {MAX ATTEMPTS} to init flash.
    {
        printf("Failed to greet device, re-trying in 1 second. Attempt %d of %d...\n", attempt_no, MAX_ATTEMPTS);
        sleep(1);
        attempt_no++;
    }
    if (attempt_no > MAX_ATTEMPTS) return false;
    clear_buffer(buf, sizeof(buf));
    return true;
}

bool reboot_to_bootloader(hid_device *dev, char *oem_option) {
    uint32_t sonix_reboot[2] = {0x5AA555AA, 0xCC3300FF};
    uint32_t hfd_reboot[2]   = {0x5A8942AA, 0xCC6271FF};

    if (oem_option == NULL) {
        printf("ERROR: reboot option cannot be null.\n");
        return false;
    }
    if (strcmp(oem_option, "sonix") == 0 || strcmp(oem_option, "evision") == 0) {
        return send_magic_command(dev, sonix_reboot);
    } else if (strcmp(oem_option, "hfd") == 0) {
        return send_magic_command(dev, hfd_reboot);
    }
    printf("ERROR: unsupported reboot option selected.\n");
    return false;
}

bool protocol_init(hid_device *dev, bool oem_reboot, char *oem_option) {
    unsigned char buf[REPORT_SIZE];
    uint32_t      resp = 0;
    chip               = 0;
    // 0) Request bootloader reboot
    if (oem_reboot) {
        printf("Requesting bootloader reboot...\n");
        if (reboot_to_bootloader(dev, oem_option))
            printf("Bootloader reboot request success.\n");
        else {
            printf("ERROR: Bootloader reboot request failed.\n");
            return false;
        }
    }

    // 01) Initialize
    printf("\n");
    printf("Fetching flash version...\n");

    clear_buffer(buf, REPORT_SIZE);
    buf[0] = CMD_GET_FW_VERSION;
    write_buffer_16(buf + 1, CMD_BASE);
    write_buffer_16(buf + 4, code_option);
    uint8_t attempt_no = 1;
    while (!hid_set_feature(dev, buf, REPORT_SIZE) && attempt_no <= MAX_ATTEMPTS) // Try {MAX ATTEMPTS} to init flash.
    {
        printf("Flash failed to fetch flash version, re-trying in 3 seconds. Attempt %d of %d...\n", attempt_no, MAX_ATTEMPTS);
        sleep(3);
        attempt_no++;
    }
    if (attempt_no > MAX_ATTEMPTS) return false;

    if (!hid_get_feature(dev, buf, REPORT_SIZE, CMD_GET_FW_VERSION)) return false;
    chip = sn32_decode_chip(buf);
    if (chip == 0) return false;
    cs_level = sn32_get_code_security(buf);
    if (cs_level < 0) return false;
    if (!sn32_check_isp_code_option(buf)) return false;

    bool reboot_fail = !read_response_32(buf, 0, 0, &resp);
    bool init_fail   = !read_response_32(buf, 0, CMD_VERIFY(CMD_GET_FW_VERSION), &resp);
    if (init_fail) {
        if (oem_reboot && reboot_fail) {
            fprintf(stderr, "ERROR: Failed to initialize: response cmd is 0x%08x, expected 0x%08x.\n", resp, 0);
        } else
            fprintf(stderr, "ERROR: Failed to initialize: response cmd is 0x%08x, expected 0x%08x.\n", resp, CMD_VERIFY(CMD_GET_FW_VERSION));
        return false;
    }
    return true;
}

bool protocol_code_option_check(hid_device *dev) {
    unsigned char buf[REPORT_SIZE];
    // 02) Prepare for Code Option Table check
    printf("\n");
    printf("Checking Code Option Table...\n");
    clear_buffer(buf, REPORT_SIZE);
    buf[0] = CMD_COMPARE_CODE_OPTION;
    write_buffer_16(buf + 1, CMD_BASE);
    write_buffer_16(buf + 4, code_option);
    if (!hid_set_feature(dev, buf, REPORT_SIZE)) return false;
    clear_buffer(buf, REPORT_SIZE);
    return true;
}

bool protocol_code_option_set(hid_device *dev, uint16_t code_option, uint16_t cs_value) {
    unsigned char buf[REPORT_SIZE];
    // 03) Set Code Option Table
    printf("\n");
    printf("Setting Code Option Table 0x%04x with Code Security value 0x%04X...\n", code_option, cs_value);
    clear_buffer(buf, REPORT_SIZE);
    buf[0] = CMD_SET_ENCRYPTION_ALGO;
    write_buffer_16(buf + 1, CMD_BASE);
    write_buffer_16(buf + 4, code_option);
    write_buffer_16(buf + 6, cs_value);
    if (!hid_set_feature(dev, buf, REPORT_SIZE)) return false;
    if (!hid_get_feature(dev, buf, REPORT_SIZE, CMD_SET_ENCRYPTION_ALGO)) return false;
    clear_buffer(buf, REPORT_SIZE);
    return true;
}

bool erase_flash(hid_device *dev, uint16_t page_start, uint16_t page_end, uint16_t blank_checksum) {
    unsigned char buf[REPORT_SIZE];
    uint16_t      resp = 0;
    // 04) Erase flash
    printf("\n");
    printf("Erasing flash from page %u to page %u...\n", page_start, page_end);
    clear_buffer(buf, REPORT_SIZE);
    buf[0] = CMD_ENABLE_ERASE;
    write_buffer_16(buf + 1, CMD_BASE);
    write_buffer_16(buf + 4, page_start);
    write_buffer_16(buf + 8, page_end);
    if (!hid_set_feature(dev, buf, REPORT_SIZE)) return false;
    if (!hid_get_feature(dev, buf, REPORT_SIZE, CMD_ENABLE_ERASE)) return false;
    if (read_response_16(buf, 8, blank_checksum, &resp)) {
        printf("Flash erase verified. \n");
        return true;
    } else {
        fprintf(stderr, "ERROR: Failed to verify flash erase: response is 0x%04x, expected 0x%04x.\n", resp, blank_checksum);
        return false;
    }
    clear_buffer(buf, REPORT_SIZE);
    return false;
}

bool protocol_reboot_user(hid_device *dev) {
    unsigned char buf[REPORT_SIZE];
    // 08) Reboot to User Mode
    printf("\n");
    printf("Flashing done. Rebooting.\n");
    clear_buffer(buf, REPORT_SIZE);
    buf[0] = CMD_RETURN_USER_MODE;
    write_buffer_16(buf + 1, CMD_BASE);
    if (!hid_set_feature(dev, buf, REPORT_SIZE)) return false;
    clear_buffer(buf, REPORT_SIZE);
    return true;
}

bool flash(hid_device *dev, long offset, const char *file_name, long fw_size, bool skip_offset_check) {
    FILE *firmware = fopen(file_name, "rb");
    if (firmware == NULL) {
        fprintf(stderr, "ERROR: Could not open firmware file (Does the file exist?).\n");
        return false;
    }

    unsigned char buf[REPORT_SIZE];
    uint32_t      resp = 0;

    if (chip == SN260 && !flash_jumploader && offset == 0) // Failsafe when flashing a 268 w/o jumploader and offset
    {
        printf("Warning: 26X flashing without offset.\n");
        printf("Warning: POTENTIALLY DANGEROUS OPERATION.\n");
        sleep(3);
        if (skip_offset_check) {
            printf("Warning: Flashing 26X without offset. Operation will continue after 10s...\n");
            sleep(10);
        } else {
            printf("Fail safing to offset 0x%04x\n", QMK_OFFSET_DEFAULT);
            offset = QMK_OFFSET_DEFAULT;
        }
    }

    // 05) Enable program
    printf("\n");
    printf("Enabling Program mode...\n");

    clear_buffer(buf, REPORT_SIZE);
    buf[0] = CMD_ENABLE_PROGRAM;
    write_buffer_16(buf + 1, CMD_BASE);
    write_buffer_32(buf + 4, (uint32_t)offset);
    write_buffer_32(buf + 8, (uint32_t)(fw_size / REPORT_SIZE));
    if (!hid_set_feature(dev, buf, REPORT_SIZE)) return false;

    if (!hid_get_feature(dev, buf, REPORT_SIZE, CMD_ENABLE_PROGRAM)) return false;
    clear_buffer(buf, REPORT_SIZE);

    // 06) Flash
    printf("Flashing device, please wait...\n");

    size_t   bytes_read = 0;
    uint16_t checksum   = 0;
    uint32_t last_chunk = 0;
    clear_buffer(buf, REPORT_SIZE);
    while ((bytes_read = fread(buf, 1, REPORT_SIZE, firmware)) > 0) {
        if (bytes_read < REPORT_SIZE) {
            fprintf(stderr, "WARNING: Read %zu bytes, expected %d bytes.\n", bytes_read, REPORT_SIZE);
        }
        checksum += checksum16(buf, bytes_read);

        // Capture the last 4 bytes of this buffer for last_chunk
        if (bytes_read >= sizeof(uint32_t)) {
            memcpy(&last_chunk, buf + (bytes_read - sizeof(uint32_t)), sizeof(uint32_t));
        } else {
            memcpy(&last_chunk, buf, bytes_read);
        }

        if (!hid_set_feature(dev, buf, bytes_read)) return false;

        clear_buffer(buf, REPORT_SIZE);
    }
    printf("Flashed File Checksum: 0x%04x\n", checksum);
    clear_buffer(buf, REPORT_SIZE);
    fclose(firmware);

    // 07) Verify flash complete
    printf("\n");
    printf("Verifying flash completion...\n");
    if (!hid_get_feature(dev, buf, REPORT_SIZE, CMD_ENABLE_PROGRAM)) return false;
    if (read_response_32(buf, LAST_CHUNK_OFFSET, last_chunk, &resp)) {
        printf("Flash completion verified. \n");
        uint16_t resp_16 = (uint16_t)resp;
        if (read_response_16(buf, 8, checksum, &resp_16)) {
            printf("Flash Verification Checksum: OK!\n");
            return true;
        } else {
            if (offset != 0) {
                printf("Warning: offset 0x%04lx requested. Flash Verification Checksum disabled.\n", offset);
                return true;
            }
            fprintf(stderr, "ERROR:Flash Verification Checksum: FAILED! response is 0x%04x, expected 0x%04x.\n", resp_16, checksum);
            return false;
        }
        return false;
    } else {
        fprintf(stderr, "ERROR: Failed to verify flash completion: response is 0x%08x, expected 0x%08x.\n", resp, last_chunk);
        return false;
    }
    return false;
}

int str2buf(void *buffer, char *delim_str, char *string, int buflen, int bufelem_size) {
    char *s;
    int   pos = 0;
    if (string == NULL) return -1;
    memset(buffer, 0, buflen); // bzero() not defined on Win32?
    while ((s = strtok(string, delim_str)) != NULL && pos < buflen) {
        string = NULL;
        switch (bufelem_size) {
            case 1:
                ((uint8_t *)buffer)[pos++] = (uint8_t)strtol(s, NULL, 0);
                break;
            case 2:
                ((int *)buffer)[pos++] = (int)strtol(s, NULL, 0);
                break;
        }
    }
    return pos;
}

bool sanity_check_firmware(long fw_size, long offset) {
    if (fw_size + offset > MAX_FIRMWARE) {
        fprintf(stderr, "ERROR: Firmware is too large too flash: 0x%08lx max allowed is 0x%08lx.\n", fw_size, MAX_FIRMWARE - offset);
        return false;
    }
    if (fw_size < MIN_FIRMWARE) {
        fprintf(stderr, "ERROR: Firmware is too small.");
        return false;
    }

    return true;

    // TODO check pointer validity
}

bool sanity_check_jumploader_firmware(long fw_size) {
    if (fw_size > QMK_OFFSET_DEFAULT) {
        fprintf(stderr, "ERROR: Jumper loader is too large: 0x%08lx max allowed is 0x%08lx.\n", fw_size, MAX_FIRMWARE - QMK_OFFSET_DEFAULT);
        return false;
    }

    return true;
}

long get_file_size(FILE *fp) {
    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "ERROR: Could not read EOF.\n");
        return -1;
    }

    long file_size = ftell(fp);
    if (file_size == -1L) {
        fprintf(stderr, "ERROR: File size calculation failed.\n");
        return -1;
    }

    // Reset file position to the beginning
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "ERROR: File size cleanup failed.\n");
        return -1;
    }

    return file_size;
}

bool truncate_and_reopen(const char *file_name, FILE **fp, long new_size) {
    fclose(*fp); // Close the file before truncating
    if (truncate(file_name, new_size) != 0) {
        fprintf(stderr, "ERROR: Could not truncate file to size %ld.\n", new_size);
        return false;
    }

    *fp = fopen(file_name, "rb"); // Reopen the file in read mode
    if (*fp == NULL) {
        fprintf(stderr, "ERROR: Could not reopen file after truncation.\n");
        return false;
    }

    return true;
}

long prepare_file_to_flash(const char *file_name, bool flash_jumploader) {
    FILE *fp = fopen(file_name, "rb");
    if (fp == NULL) {
        fprintf(stderr, "ERROR: Could not open file (Does the file exist?).\n");
        return -1;
    }

    long file_size = get_file_size(fp);
    if (file_size == -1L) {
        fclose(fp);
        return -1;
    }

    if (file_size == 0) {
        fprintf(stderr, "ERROR: File is empty.\n");
        fclose(fp);
        return -1;
    }
    printf("\n");
    printf("File size: %ld bytes\n", file_size);

    // If jumploader is not 0x200 in length, add padded zeroes to file
    if (flash_jumploader && file_size < QMK_OFFSET_DEFAULT) {
        printf("Warning: jumploader binary doesn't have a size of: 0x%04x bytes.\n", QMK_OFFSET_DEFAULT);
        printf("Truncating jumploader binary to: 0x%04x.\n", QMK_OFFSET_DEFAULT);

        if (!truncate_and_reopen(file_name, &fp, QMK_OFFSET_DEFAULT)) {
            return -1;
        }

        // Recalculate file size
        file_size = get_file_size(fp);
        if (file_size == -1L) {
            fprintf(stderr, "ERROR: Truncated file size calculation failed.\n");
            fclose(fp);
            return -1;
        }
    }

    // Adjust file size to fit in the HID report
    if (file_size % REPORT_SIZE != 0) {
        printf("File size must be adjusted to fit in the HID report.\n");
        long padded_file_size = file_size;
        printf("File size before padding: %ld bytes\n", padded_file_size);
        while (padded_file_size % REPORT_SIZE != 0) {
            padded_file_size++;
        }
        printf("File size after padding: %ld bytes\n", padded_file_size);

        if (!truncate_and_reopen(file_name, &fp, padded_file_size)) {
            return -1;
        }

        // Recalculate file size
        file_size = get_file_size(fp);
        if (file_size == -1L) {
            fprintf(stderr, "ERROR: Truncated file size calculation failed.\n");
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return file_size;
}

char *get_full_path(const char *file_name) {
    char *full_path = NULL;

#ifdef _WIN32
    char buffer[MAX_PATH];
    if (GetFullPathName(file_name, MAX_PATH, buffer, NULL) != 0) {
        full_path = strdup(buffer);
    } else {
        fprintf(stderr, "ERROR: Could not resolve full path for file: '%s'\n", file_name);
    }
#else
    char buffer[PATH_MAX];
    if (realpath(file_name, buffer) != NULL) {
        full_path = strdup(buffer);
    } else {
        fprintf(stderr, "ERROR: Could not resolve full path for file: '%s'\n", file_name);
    }
#endif

    return full_path;
}

int main(int argc, char *argv[]) {
    int         opt, opt_index;
    hid_device *handle;

    uint16_t vid              = 0;
    uint16_t pid              = 0;
    long     offset           = 0;
    char    *file_name        = NULL;
    char    *endptr           = NULL;
    char    *reboot_opt       = NULL;
    bool     reboot_requested = false;
    debug                     = false;
    bool no_offset_check      = false;

    if (argc < 2) {
        print_usage(PROJECT_NAME);
        exit(1);
    }
    // clang-format off
    struct option longoptions[] = {{"help", no_argument, 0, 'h'},
                                 {"version", no_argument, 0, 'V'},
                                 {"vidpid", required_argument, NULL, 'v'},
                                 {"offset", required_argument, NULL, 'o'},
                                 {"file", required_argument, NULL, 'f'},
                                 {"jumploader", no_argument, NULL, 'j'},
                                 {"reboot", required_argument, NULL, 'r'},
                                 {"debug", no_argument, NULL, 'd'},
                                 {"nooffset", no_argument, NULL, 'k'},
                                 {"list-vidpid", no_argument, NULL, 'l'},
                                 {NULL, 0, 0, 0}};
    // clang-format on

    while ((opt = getopt_long(argc, argv, "hlVv:o:r:f:jdk", longoptions, &opt_index)) != -1) {
        switch (opt) {
            case 'h': // Show help
                print_usage(PROJECT_NAME);
                break;
            case 'l': // list-vidpid
                print_vidpid_table();
                break;
            case 'V': // version
                display_version(PROJECT_NAME);
                break;
            case 'v':                                               // Set vid/pid
                if (sscanf(optarg, "%4hx/%4hx", &vid, &pid) != 2) { // match "23FE/AB12"
                    if (!sscanf(optarg, "%4hx:%4hx", &vid, &pid)) { // match "23FE:AB12"
                        // else try parsing standard dec/hex values
                        int wordbuf[4]; // a little extra space
                        str2buf(wordbuf, ":/, ", optarg, sizeof(wordbuf), 2);
                        vid = wordbuf[0];
                        pid = wordbuf[1];
                    }
                }
                // make sure we have the correct vidpid
                if (vid == 0 || pid == 0) {
                    fprintf(stderr, "ERROR: invalid vidpid -'%s'.\n", optarg);
                    print_vidpid_table();
                    exit(1);
                }
                break;
            case 'f': // file name
                file_name = get_full_path(optarg);
                break;
            case 'o': // offset
                offset = strtol(optarg, &endptr, 0);
                if (errno == ERANGE || *endptr != '\0') {
                    fprintf(stderr, "ERROR: invalid offset value -'%s'.\n", optarg);
                    exit(1);
                }
                break;
            case 'r': // reboot
                reboot_opt       = optarg;
                reboot_requested = true;
                break;
            case 'j': // Jumploader
                flash_jumploader = true;
                break;
            case 'd': // debug
                debug = true;
                break;
            case 'k': // skip offset check
                no_offset_check = true;
                break;
            case '?':
            default:
                switch (optopt) {
                    case 'f':
                    case 'v':
                    case 'o':
                    case 'r':
                        fprintf(stderr, "ERROR: option '-%c' requires a parameter.\n", optopt);
                        break;
                    case 0:
                        fprintf(stderr, "ERROR: invalid option.\n");
                        break;
                    default:
                        fprintf(stderr, "ERROR: invalid option '-%c'.\n", optopt);
                        break;
                }
                exit(1);
        }
        // exit clean after printing
        if (opt == 'h' || opt == 'V') exit(1);
    }

    if (file_name == NULL) {
        fprintf(stderr, "ERROR: filename cannot be null.\n");
        exit(1);
    }

    printf("Firmware to flash: %s with offset 0x%04lx, device: 0x%04x/0x%04x.\n", file_name, offset, vid, pid);

    // Try to open the device
    if (hid_init() < 0) {
        fprintf(stderr, "ERROR: Could not initialize HID.\n");
        exit(1);
    }
    printf("\n");
    printf("\n");
    printf("Opening device...\n");
    handle = hid_open(vid, pid, NULL);

    uint8_t attempt_no = 1;
    while (handle == NULL && attempt_no <= MAX_ATTEMPTS) // Try {MAX ATTEMPTS} to connect to device.
    {
        printf("Device failed to open, re-trying in 3 seconds. Attempt %d of %d...\n", attempt_no, MAX_ATTEMPTS);
        sleep(3);
        handle = hid_open(vid, pid, NULL);
        attempt_no++;
    }

    if (handle) {
        printf("\n");
        printf("Device opened successfully...\n");

        // Check VID/PID
        if (vid != SONIX_VID || !is_known_isp_pid(pid)) {
            if (vid == EVISION_VID && !reboot_requested) printf("Warning: eVision VID detected! You probably need to use the reboot option.\n");
            if (vid == APPLE_VID && !reboot_requested) printf("Warning: Apple VID detected! You probably need to use the reboot option.\n");
            printf("Warning: Flashing a non-sonix bootloader device, you are now on your own.\n");
            sleep(3);
        }
        attempt_no = 1;
        bool ok    = protocol_init(handle, reboot_requested, reboot_opt);
        while (!ok && attempt_no <= MAX_ATTEMPTS) {
            printf("Device failed to init, re-trying in 3 seconds. Attempt %d of %d...\n", attempt_no, MAX_ATTEMPTS);
            sleep(3);
            ok = protocol_init(handle, reboot_requested, reboot_opt);
            attempt_no++;
        }
        if (!ok) error(handle);
        sleep(1);
        if (chip != SN240B && chip != SN260) ok = protocol_code_option_check(handle);
        if (!ok) error(handle);
        sleep(1);
        if (cs_level != 0) {
            printf("Resetting Code Security from CS%d to CS%d...\n", cs_level, 0);
            ok = protocol_code_option_set(handle, code_option, CS0);
        }
        if (!ok) error(handle);
        sleep(1);
        if (chip != SN240B && chip != SN260) ok = erase_flash(handle, 0, USER_ROM_PAGES, BLANK_CHECKSUM);
        if (!ok) error(handle);
        sleep(1);

        long prepared_file_size = prepare_file_to_flash(file_name, flash_jumploader);
        if (prepared_file_size < 0) {
            fprintf(stderr, "ERROR: File preparation failed.\n");
            free(file_name);
            error(handle);
        }
        if (((flash_jumploader && sanity_check_jumploader_firmware(prepared_file_size)) || (!flash_jumploader && sanity_check_firmware(prepared_file_size, offset))) && (flash(handle, offset, file_name, prepared_file_size, no_offset_check))) {
            printf("Device succesfully flashed!\n");
            sleep(2);
            protocol_reboot_user(handle);
        } else {
            fprintf(stderr, "ERROR: Could not flash the device. Try again.\n");
            free(file_name);
            error(handle);
        }
    } else {
        fprintf(stderr, "ERROR: Could not open the device (Is the device connected?).\n");
        free(file_name);
        error(handle);
    }
    free(file_name);
    cleanup(handle);
    exit(0);
}
