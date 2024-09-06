#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <hidapi.h>

#define REPORT_SIZE 64
#define REPORT_LENGTH (REPORT_SIZE + 1)
#define USER_ROM_SIZE_SN32F260 30   // in KB
#define USER_ROM_SIZE_SN32F240 64   // in KB
#define USER_ROM_SIZE_SN32F240B 64  // in KB
#define USER_ROM_SIZE_SN32F240C 128 // in KB
#define USER_ROM_SIZE_SN32F280 128  // in KB
#define USER_ROM_SIZE_SN32F290 256  // in KB
#define USER_ROM_SIZE_KB(x) ((x) * 1024)

#define USER_ROM_PAGES_SN32F260 480
#define USER_ROM_PAGES_SN32F240 64
#define USER_ROM_PAGES_SN32F240B 1024
#define USER_ROM_PAGES_SN32F240C 128
#define USER_ROM_PAGES_SN32F280 128
#define USER_ROM_PAGES_SN32F290 256

#define QMK_OFFSET_DEFAULT 0x200

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

#define CMD_OK 0xFAFAFAFA
#define VALID_FW 0xAAAA5555

#define SN240 1
#define SN260 2
#define SN240B 3
#define SN280 4
#define SN290 5
#define SN240C 6

#define CS1 0x5A5A
#define CS2 0xA5A5
#define CS3 0x55AA

#define SONIX_VID 0x0c45
#define SN268_PID 0x7010
#define SN248B_PID 0x7040
#define SN248C_PID 0x7145
#define SN248_PID 0x7900
#define SN289_PID 0x7120
#define SN299_PID 0x7140

#define EVISION_VID 0x320F
#define APPLE_VID 0x05ac

#define MAX_ATTEMPTS 5

#define PROJECT_NAME "sonixflasher"
#define PROJECT_VER "2.0.1"

uint16_t        CS0              = 0;
uint16_t        USER_ROM_SIZE    = USER_ROM_SIZE_SN32F260;
uint16_t        USER_ROM_PAGES   = USER_ROM_PAGES_SN32F260;
long            MAX_FIRMWARE     = USER_ROM_SIZE_KB(USER_ROM_SIZE_SN32F260);
bool            flash_jumploader = false;
bool            debug            = false;
static uint16_t code_option      = 0x0000; // Initial Code Option Table
int             chip;
uint16_t        cs_level;

static void print_vidpid_table() {
    printf("Supported VID/PID pairs:\n");
    printf("+-----------------+------------+------------+\n");
    printf("|      Device     |    VID     |    PID     |\n");
    printf("+-----------------+------------+------------+\n");
    printf("| SONIX SN26x     | 0x%04X     | 0x%04X     |\n", SONIX_VID, SN268_PID);
    printf("| SONIX SN24xB    | 0x%04X     | 0x%04X     |\n", SONIX_VID, SN248B_PID);
    printf("| SONIX SN24xC    | 0x%04X     | 0x%04X     |\n", SONIX_VID, SN248C_PID);
    printf("| SONIX SN24x     | 0x%04X     | 0x%04X     |\n", SONIX_VID, SN248_PID);
    printf("| SONIX SN28x     | 0x%04X     | 0x%04X     |\n", SONIX_VID, SN289_PID);
    printf("| SONIX SN29x     | 0x%04X     | 0x%04X     |\n", SONIX_VID, SN299_PID);
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
            "  --reboot -r      Request bootloader reboot in OEM firmware (options: evision, hfd) \n"
            "  --debug -d       Enable debug mode \n"
            "  --list-vidpid -l Display supported VID/PID pairs\n"
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
    printf("\n");
}

bool hid_set_feature(hid_device *dev, unsigned char *data, size_t length) {
    if (length > REPORT_SIZE) {
        fprintf(stderr, "ERROR: Report can't be more than %d bytes!! (Attempted: %zu bytes)\n", REPORT_SIZE, length);
        return false;
    }

    if (debug) {
        printf("Sending payload...\n");
        print_data(data, length);
    }
    // hidapi will hijack a 0x00 for Report ID and strip it from the buffer.
    // Check if the first byte of data is 0x00
    if (data[0] == 0x00) {
        // Allocate a temporary buffer with an extra byte
        unsigned char temp_buf[REPORT_SIZE + 1];

        // Set the Report ID byte (0x00) at the start of the buffer
        temp_buf[0] = 0x00;

        // Copy the data into the buffer, starting from the second byte
        // This allows the actual 0x00 to be sent
        memcpy(temp_buf + 1, data, length);

        // Send the feature report using the temporary buffer
        if (hid_send_feature_report(dev, temp_buf, length + 1) < 0) {
            fprintf(stderr, "ERROR: Error while writing command %0x2X! Reason: %ls\n", data[0], hid_error(dev));
            return false;
        }
    } else {
        // Send the report as is
        if (hid_send_feature_report(dev, data, length) < 0) {
            fprintf(stderr, "ERROR: Error while writing command %0x2X! Reason: %ls\n", data[0], hid_error(dev));
            return false;
        }
    }

    return true;
}
int sn32_decode_chip(unsigned char *data) {
    if (data[8] == 32) {
        printf("Sonix SN32 Detected.\n");
        printf("Checking variant...\n");
        sleep(2);
        int sn32_variant;
        switch (data[9]) {
            case SN240:
                printf("240 Detected.\n");
                USER_ROM_SIZE  = USER_ROM_SIZE_SN32F240;
                USER_ROM_PAGES = USER_ROM_PAGES_SN32F240;
                MAX_FIRMWARE   = USER_ROM_SIZE_KB(USER_ROM_SIZE);
                CS0            = 0xFFFF;
                sn32_variant   = SN240;
                break;
            case SN260:
                printf("260 Detected.\n");
                USER_ROM_SIZE  = USER_ROM_SIZE_SN32F260;
                USER_ROM_PAGES = USER_ROM_PAGES_SN32F260;
                MAX_FIRMWARE   = USER_ROM_SIZE_KB(USER_ROM_SIZE);
                CS0            = 0;
                sn32_variant   = SN260;
                break;
            case SN240B:
                printf("240B Detected.\n");
                USER_ROM_SIZE  = USER_ROM_SIZE_SN32F240B;
                USER_ROM_PAGES = USER_ROM_PAGES_SN32F240B;
                MAX_FIRMWARE   = USER_ROM_SIZE_KB(USER_ROM_SIZE);
                CS0            = 0;
                sn32_variant   = SN240B;
                break;
            case SN280:
                printf("280 Detected.\n");
                USER_ROM_SIZE  = USER_ROM_SIZE_SN32F280;
                USER_ROM_PAGES = USER_ROM_PAGES_SN32F280;
                MAX_FIRMWARE   = USER_ROM_SIZE_KB(USER_ROM_SIZE);
                CS0            = 0xFFFF;
                sn32_variant   = SN280;
                break;
            case SN290:
                printf("290 Detected.\n");
                USER_ROM_SIZE  = USER_ROM_SIZE_SN32F290;
                USER_ROM_PAGES = USER_ROM_PAGES_SN32F290;
                MAX_FIRMWARE   = USER_ROM_SIZE_KB(USER_ROM_SIZE);
                CS0            = 0xFFFF;
                sn32_variant   = SN290;
                break;
            case SN240C:
                printf("240C Detected. \n");
                USER_ROM_SIZE  = USER_ROM_SIZE_SN32F240C;
                USER_ROM_PAGES = USER_ROM_PAGES_SN32F240C;
                MAX_FIRMWARE   = USER_ROM_SIZE_KB(USER_ROM_SIZE);
                CS0            = 0xFFFF;
                sn32_variant   = SN240C;
                break;
            default:
                fprintf(stderr, "ERROR: Unsupported bootloader version: %d, we don't support this chip.\n", data[9]);
                sn32_variant = 0;
                break;
        }

        return sn32_variant;
    } else {
        fprintf(stderr, "ERROR: Unsupported family version: %d, we don't support this chip.\n", data[8]);
        return 0;
    }
}

bool sn32_check_isp_code_option(unsigned char *data) {
    uint16_t received_code_option = (data[12] << 8) | data[13];
    printf("Expected Code Option Table: 0x%04X\n", code_option);
    printf("Received Code Option Table: 0x%04X\n", received_code_option);
    if (received_code_option != code_option) {
        printf("Updating Code Option Table from 0x%04X to 0x%04X\n", code_option, received_code_option);
        code_option = received_code_option;
        return false;
    }
    return true;
}

uint16_t sn32_get_cs_level(unsigned char *data) {
    cs_level             = 0;
    uint16_t combined_cs = (data[14] << 8) | data[15];

    if (combined_cs == CS0) {
        printf("Current Security level: CS0. \n");
    } else {
        switch (combined_cs) {
            case CS1:
                printf("Current Security level: CS1. \n");
                break;
            case CS2:
                printf("Current Security level: CS2. \n");
                break;
            case CS3:
                printf("Current Security level: CS3. \n");
                break;
            default:
                fprintf(stderr, "ERROR: Unsupported Security level: %04x, we don't support this chip.\n", combined_cs);
                break;
        }
    }
    return combined_cs;
}

bool hid_get_feature(hid_device *dev, unsigned char *data, uint32_t command) {
    clear_buffer(data, sizeof(data));
    int res = hid_get_feature_report(dev, data, REPORT_LENGTH);

    if (res == REPORT_LENGTH) {
        // Shift the data buffer to remove the first byte
        memmove(data, data + 1, res - 1);

        if (debug) {
            printf("Received payload...\n");
            print_data(data, res - 1);
        }

        // Check the status directly in the data buffer
        unsigned int status = *((unsigned int *)(data + 4));
        if (status != CMD_OK) {
            fprintf(stderr, "ERROR: Invalid response status: 0x%08x, expected 0x%08x for command 0x%02X.\n", status, CMD_OK, command & 0xFF);
            return false;
        }
        // Success
        return true;

    } else {
        fprintf(stderr, "ERROR: Failed to get feature report for command 0x%02X: got response of length %d, expected %d.\n", command & 0xFF, res, REPORT_LENGTH);
        return false;
    }
}

bool send_magic_command(hid_device *dev, const uint32_t *command) {
    unsigned char buf[REPORT_SIZE];

    clear_buffer(buf, sizeof(buf));
    write_buffer_32(buf, command[0]);
    write_buffer_32(buf + sizeof(uint32_t), command[1]);
    if (!hid_set_feature(dev, buf, sizeof(buf))) return false;
    clear_buffer(buf, sizeof(buf));
    return true;
}

bool reboot_to_bootloader(hid_device *dev, char *oem_option) {
    uint32_t evision_reboot[2] = {0x5AA555AA, 0xCC3300FF};
    uint32_t hfd_reboot[2]     = {0x5A8942AA, 0xCC6271FF};

    if (oem_option == NULL) {
        printf("ERROR: reboot option cannot be null.\n");
        return false;
    }
    if (strcmp(oem_option, "evision") == 0) {
        return send_magic_command(dev, evision_reboot);
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
            printf("Bootloader reboot succesfull.\n");
        else {
            printf("ERROR: Bootloader reboot failed.\n");
            return false;
        }
    }

    // 01) Initialize
    printf("Fetching flash version...\n");
    sleep(2);
    clear_buffer(buf, REPORT_SIZE);
    buf[0] = CMD_GET_FW_VERSION;
    write_buffer_16(buf + 1, CMD_BASE);
    write_buffer_16(buf + 3, code_option);
    uint8_t attempt_no = 1;
    while (!hid_set_feature(dev, buf, REPORT_SIZE) && attempt_no <= MAX_ATTEMPTS) // Try {MAX ATTEMPTS} to init flash.
    {
        printf("Flash failed to fetch flash version, re-trying in 3 seconds. Attempt %d of %d...\n", attempt_no, MAX_ATTEMPTS);
        sleep(3);
        attempt_no++;
    }
    if (attempt_no > MAX_ATTEMPTS) return false;

    if (!hid_get_feature(dev, buf, CMD_GET_FW_VERSION)) return false;
    chip = sn32_decode_chip(buf);
    if (chip == 0) return false;
    cs_level = sn32_get_cs_level(buf);
    if (!sn32_check_isp_code_option(buf)) return false;

    bool reboot_fail = !read_response_32(buf, 0, 0, &resp);
    bool init_fail   = !read_response_32(buf, 0, CMD_VERIFY(CMD_GET_FW_VERSION), &resp);
    if (init_fail) {
        if (oem_reboot && reboot_fail) {
            fprintf(stderr, "ERROR: Failed to initialize: response cmd is 0x%08x, expected 0x%08x.\n", resp, 0);
        } else
            fprintf(stderr, "ERROR: Failed to initialize: response cmd is 0x%08x, expected 0x%08x.\n", resp, CMD_GET_FW_VERSION);
        return false;
    }
    return true;
}

bool protocol_code_option_check(hid_device *dev) {
    unsigned char buf[REPORT_SIZE];
    // 02) Prepare for Code Option Table check
    printf("Checking Code Option Table...\n");
    clear_buffer(buf, REPORT_SIZE);
    buf[0] = CMD_COMPARE_CODE_OPTION;
    write_buffer_16(buf + 1, CMD_BASE);
    write_buffer_16(buf + 3, code_option);
    if (!hid_set_feature(dev, buf, REPORT_SIZE)) return false;
    clear_buffer(buf, REPORT_SIZE);
    return true;
}

bool protocol_reset_cs(hid_device *dev) {
    unsigned char buf[REPORT_SIZE];
    // 03) Reset Code Option Table
    printf("Resetting Code Option Table...\n");
    clear_buffer(buf, REPORT_SIZE);
    buf[0] = CMD_SET_ENCRYPTION_ALGO;
    write_buffer_16(buf + 1, CMD_BASE);
    write_buffer_16(buf + 3, code_option);
    write_buffer_16(buf + 5, CS0); // WARNING THIS SETS CS0
    if (!hid_set_feature(dev, buf, REPORT_SIZE)) return false;
    if (!hid_get_feature(dev, buf, CMD_SET_ENCRYPTION_ALGO)) return false;
    clear_buffer(buf, REPORT_SIZE);
    return true;
}

bool erase_flash(hid_device *dev) {
    unsigned char buf[REPORT_SIZE];
    // 04) Erase flash
    printf("Erasing flash...\n");
    clear_buffer(buf, REPORT_SIZE);
    buf[0] = CMD_ENABLE_ERASE;
    write_buffer_16(buf + 1, CMD_BASE);
    write_buffer_16(buf + 8, USER_ROM_PAGES);
    if (!hid_set_feature(dev, buf, REPORT_SIZE)) return false;
    if (!hid_get_feature(dev, buf, CMD_ENABLE_ERASE)) return false;
    clear_buffer(buf, REPORT_SIZE);
    return true;
}

bool protocol_reboot_user(hid_device *dev) {
    unsigned char buf[REPORT_SIZE];
    // 08) Reboot to User Mode
    printf("Flashing done. Rebooting.\n");
    clear_buffer(buf, REPORT_SIZE);
    buf[0] = CMD_RETURN_USER_MODE;
    write_buffer_16(buf + 1, CMD_BASE);
    if (!hid_set_feature(dev, buf, REPORT_SIZE)) return false;
    clear_buffer(buf, REPORT_SIZE);
    return true;
}

bool flash(hid_device *dev, long offset, FILE *firmware, long fw_size, bool skip_offset_check) {
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
    sleep(2);
    clear_buffer(buf, REPORT_SIZE);
    buf[0] = CMD_ENABLE_PROGRAM;
    write_buffer_16(buf + 1, CMD_BASE);
    write_buffer_32(buf + 4, (uint32_t)offset);
    write_buffer_32(buf + 8, (uint32_t)(fw_size / REPORT_SIZE));
    if (!hid_set_feature(dev, buf, REPORT_SIZE)) return false;

    if (!hid_get_feature(dev, buf, CMD_ENABLE_PROGRAM)) return false;
    clear_buffer(buf, REPORT_SIZE);

    // 06) Flash
    printf("Flashing device, please wait...\n");

    size_t bytes_read = 0;
    clear_buffer(buf, REPORT_SIZE);
    while ((bytes_read = fread(buf, 1, REPORT_SIZE, firmware)) > 0) {
        if (bytes_read < REPORT_SIZE) {
            fprintf(stderr, "WARNING: Read %zu bytes, expected %d bytes.\n", bytes_read, REPORT_SIZE);
        }
        if (!hid_set_feature(dev, buf, bytes_read)) return false;

        clear_buffer(buf, REPORT_SIZE);
    }
    clear_buffer(buf, REPORT_SIZE);

    // 07) Verify flash complete
    printf("\n");
    printf("Verifying flash completion...\n");
    if (!hid_get_feature(dev, buf, CMD_ENABLE_PROGRAM)) return false;
    if (read_response_32(buf, (sizeof(buf) - sizeof(VALID_FW)), VALID_FW, &resp)) {
        printf("Flash completion verified. \n");
        return true;
    } else {
        fprintf(stderr, "ERROR: Failed to verify flash completion: response is 0x%08x, expected 0x%08x.\n", resp, VALID_FW);
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
    if (fw_size < 0x100) {
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

int main(int argc, char *argv[]) {
    int         opt, opt_index, res;
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
                file_name = optarg;
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

    FILE *fp = fopen(file_name, "rb");

    if (fp == NULL) {
        fprintf(stderr, "ERROR: Could not open file (Does the file exist?).\n");
        fclose(fp);
        exit(1);
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // if jumploader is not 0x200 in length, add padded zeroes to file
    if (flash_jumploader && file_size < QMK_OFFSET_DEFAULT) {
        printf("Warning: jumploader binary doesnt have a size of: 0x%04x bytes.\n", QMK_OFFSET_DEFAULT);
        printf("Truncating jumploader binary to: 0x%04x.\n", QMK_OFFSET_DEFAULT);

        // Close device before truncating it
        fclose(fp);
        if (truncate(file_name, QMK_OFFSET_DEFAULT) != 0) {
            fprintf(stderr, "ERROR: Could not truncate file.\n");
            exit(1);
        }

        // Try open the file again.
        fp = fopen(file_name, "rb");
        if (fp == NULL) {
            fprintf(stderr, "ERROR: Could not open file.\n");
            fclose(fp);
            exit(1);
        }
    }

    // Try to open the device
    res = hid_init();
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
        if (vid != SONIX_VID || (pid != SN248_PID && pid != SN248B_PID && pid != SN248C_PID && pid != SN268_PID && pid != SN289_PID && pid != SN299_PID)) {
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
        if (!ok) exit(1);
        sleep(3);
        if (chip == SN240 || chip == SN290) ok = protocol_code_option_check(handle); // 240 and 290
        if (!ok) exit(1);
        sleep(1);
        if (cs_level != CS0) ok = protocol_reset_cs(handle);
        if (!ok) exit(1);
        sleep(1);
        if (chip == SN240 || chip == SN290) ok = erase_flash(handle);
        if (!ok) exit(1);
        sleep(1);

        while (file_size % REPORT_SIZE != 0)
            file_size++; // Add padded zereos (if any) to file_size, we need to take in consideration when the file doesnt fill the buffer.

        if (((flash_jumploader && sanity_check_jumploader_firmware(file_size)) || (!flash_jumploader && sanity_check_firmware(file_size, offset))) && (flash(handle, offset, fp, file_size, no_offset_check))) {
            printf("Device succesfully flashed!\n");
            sleep(3);
            protocol_reboot_user(handle);
        } else {
            fprintf(stderr, "ERROR: Could not flash the device. Try again.\n");
            exit(1);
        }
    } else {
        fprintf(stderr, "ERROR: Could not open the device (Is the device connected?).\n");
        exit(1);
    }

    fclose(fp);
    hid_close(handle);
    res = hid_exit();

    if (res < 0) {
        fprintf(stderr, "ERROR: Could not close the device.\n");
    }

    exit(0);
}
