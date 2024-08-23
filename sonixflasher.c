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
#define MAX_FIRMWARE_SN32F260 (30 * 1024) //30k
#define MAX_FIRMWARE_SN32F240 (64 * 1024) //64k
#define MAX_FIRMWARE_SN32F240C (128 * 1024) //128k
#define MAX_FIRMWARE_SN32F290 (256 * 1024) //256k
#define QMK_OFFSET_DEFAULT 0x200

#define CMD_BASE 0x55AA00
#define CMD_INIT (CMD_BASE + 0x1)
#define CMD_PREPARE (CMD_BASE + 0x5)
#define CMD_REBOOT (CMD_BASE + 0x7)

#define SONIX_VID  0x0c45
#define SN268_PID  0x7010
#define SN248B_PID 0x7040
#define SN248C_PID 0x7145
#define SN248_PID  0x7900
#define SN299_PID  0x7140

#define EVISION_VID 0x320F
#define APPLE_VID 0x05ac

#define EXPECTED_STATUS 0xFAFAFAFA

#define MAX_ATTEMPTS 5

#define PROJECT_NAME "sonixflasher"
#define PROJECT_VER "1.1.0"

long MAX_FIRMWARE = MAX_FIRMWARE_SN32F260;

static void print_usage(char *m_name)
{
    fprintf(stderr,
        "Usage: \n"
        "  %s <cmd> [options]\n"
        "where <cmd> is one of:\n"
        "  --vidpid -v      Set VID for device to flash\n"
        "  --offset -o      Set flashing offset (default: 0)\n"
        "  --file -f        Binary of the firmware to flash (*.bin extension) \n"
        "  --jumploader -j  Define if we are flashing a jumploader \n"
        "  --reboot -r      Request bootloader reboot in OEM firmware (options: evision, hfd) \n"
        "  --version -V     Print version information\n"
        "\n"
        "Examples: \n"
        ". Flash jumploader to device w/ vid/pid 0x0c45/0x7040 \n"
        "   sonixflasher --vidpid 0c45/7040 --file fw.bin -j\n"
        ". Flash fw to device w/ vid/pid 0x0c45/0x7040 and offset 0x200\n"
        "   sonixflasher --vidpid 0c45/7040 --file fw.bin -o 0x200\n"
        "\n"
        ""
        "", m_name);
}

//Display program version
static void display_version(char *m_name)
{
    fprintf(stderr,"%s " PROJECT_VER "\n",m_name);
}

void clear_buffer(unsigned char *data, size_t lenght)
{
    for(int i = 0; i < lenght; i++) data[i] = 0;
}

void print_buffer(unsigned char *data, size_t lenght)
{   
    printf("Sending Report...\n");
    for(int i = 0; i < lenght; i++) printf("%02x", data[i]);
    printf("\n");
}

bool read_response_32(unsigned char *data, uint32_t expected_result, uint32_t *resp)
{
    uint32_t r = *resp;

    memcpy(&r, data, 4);

    *resp = r;
    return r == expected_result;
}

void write_buffer_32(unsigned char *data, uint32_t cmd)
{   
    memcpy(data, &cmd, 4);
}

bool hid_set_feature(hid_device *dev, unsigned char *data, size_t length)
{
    if(length > REPORT_SIZE)
    {
        fprintf(stderr, "ERROR: Report can't be more than %d bytes!! (Attempted: %zu bytes)\n", REPORT_SIZE, length);
        return false;
    }
    unsigned char buf[REPORT_LENGTH];

    // Set the Report ID byte (first byte of data)
    buf[0] = 0x0;
    memcpy(buf + 1, data, length);

    int res = hid_send_feature_report(dev, data, length + 1);

    if(res < 0)
    {
        fprintf(stderr, "ERROR: Error while writing!\n");
        return false;
    }

    return true;
}

int hid_get_feature(hid_device *dev, unsigned char *data)
{
    int res = hid_get_feature_report(dev, data, REPORT_LENGTH);
    // If the read was successful and data length is greater than 0
    if (res > 0) {
        // Shift the data buffer to remove the first byte
        memmove(data, data + 1, res - 1);
        // Return the length of the data excluding the removed first byte
        return res - 1;
    }

    // Return 0 if the read was not successful or the buffer length is not sufficient
    return 0;
}

bool send_magic_command(hid_device *dev, const uint32_t *command)
{
    unsigned char buf[REPORT_SIZE];

    clear_buffer(buf, sizeof(buf));
    write_buffer_32(buf,command[0]);
    write_buffer_32(buf + sizeof(uint32_t),command[1]);
    if(!hid_set_feature(dev,buf, sizeof(buf))) return false;
    clear_buffer(buf, sizeof(buf));
    return true;
}

bool reboot_to_bootloader(hid_device *dev, char *oem_option)
{
    uint32_t evision_reboot[2] = { 0x5AA555AA, 0xCC3300FF };
    uint32_t hfd_reboot[2] = { 0x5A8942AA, 0xCC6271FF };

    if (oem_option == NULL)
    {
        printf("ERROR: reboot option cannot be null.\n");
        return false;
    }
    if(strcmp(oem_option, "evision") == 0)
    {
        return send_magic_command(dev,evision_reboot);
    }
    else if(strcmp(oem_option, "hfd") == 0)
    {
        return send_magic_command(dev,hfd_reboot);
    }
    printf("ERROR: unsupported reboot option selected.\n");
    return false;
}

bool flash(hid_device *dev, long offset, FILE *firmware, long fw_size, bool skip_size_check, bool oem_reboot, char *oem_option)
{
    unsigned char buf[REPORT_SIZE];
    int read_bytes;
    uint32_t resp = 0;
    uint32_t status = 0;

    if(skip_size_check == false)
    {
        if(fw_size + offset > MAX_FIRMWARE)
        {
            printf("ERROR: Firmware is too large too flash.\n");
            return false;
        }
    }

    // 0) Request bootloader reboot

    if(oem_reboot)
    {
        printf("Requesting bootloader reboot...\n");
        if(reboot_to_bootloader(dev, oem_option)) printf("Bootloader reboot succesfull.\n");
        else
        {
            printf("ERROR: Bootloader reboot failed.\n");
            return false;
        }
    }
    
    // 1) Initialize

    printf("Initializing flash...\n");

    clear_buffer(buf, REPORT_SIZE);
    write_buffer_32(buf, CMD_INIT);
    uint8_t attempt_no = 1;
    while(!hid_set_feature(dev, buf, REPORT_SIZE) && attempt_no <= MAX_ATTEMPTS) // Try {MAX ATTEMPTS} to init flash.
    {
        printf("Flash failed to init, re-trying in 3 seconds. Attempt %d of %d...\n", attempt_no, MAX_ATTEMPTS);
        sleep(3);
        attempt_no++;
    }
    if(attempt_no > MAX_ATTEMPTS) return false;

    clear_buffer(buf, REPORT_SIZE);
    read_bytes = hid_get_feature(dev, buf);
    if(read_bytes != REPORT_SIZE)
    {
        fprintf(stderr, "ERROR: Failed to initialize: got response of length %d, expected %d.\n", read_bytes, REPORT_SIZE);
        return false;
    }
    bool reboot_fail = !read_response_32(buf, 0, &resp);
    bool init_fail = !read_response_32(buf, CMD_INIT, &resp);
    if(init_fail)
    {
        if(oem_reboot && reboot_fail)
        {
            fprintf(stderr, "ERROR: Failed to initialize: response cmd is 0x%08x, expected 0x%08x.\n", resp, 0);
        }
        else fprintf(stderr, "ERROR: Failed to initialize: response cmd is 0x%08x, expected 0x%08x.\n", resp, CMD_INIT);
        return false;
    }
    // // 2) Prepare for flash

    printf("Preparing for flash...\n");

    clear_buffer(buf, REPORT_SIZE);
    write_buffer_32(buf, CMD_PREPARE);
    write_buffer_32(buf+4, (uint32_t)offset);
    write_buffer_32(buf+8, (uint32_t)(fw_size/REPORT_SIZE));
    if(!hid_set_feature(dev, buf, REPORT_SIZE)) return false;

    clear_buffer(buf, REPORT_SIZE);
    read_bytes = hid_get_feature(dev, buf);
    if(!read_response_32(buf, CMD_PREPARE, &resp)) // Read cmd
    {
        fprintf(stderr, "ERROR: Failed to initialize: response cmd is 0x%08x, expected 0x%08x.\n", resp, CMD_PREPARE);
        return false;
    }
    if(!read_response_32(buf + 4, EXPECTED_STATUS, &status))// Read status
    {
        fprintf(stderr, "ERROR: Failed to initialize: response status is 0x%08x, expected 0x%08x.\n", status, EXPECTED_STATUS);
        return false;
    }

    // // 3) Flash

    printf("Flashing device, please wait...\n");

    size_t bytes_read = 0;
    clear_buffer(buf, REPORT_SIZE);
    while ((bytes_read = fread(buf+1, 1, REPORT_SIZE, firmware)) > 0)
    {
        if(!hid_set_feature(dev, buf, REPORT_SIZE)) return false;
        clear_buffer(buf, REPORT_SIZE);
    }

    printf("Flashing done. Rebooting.\n");

    // // 4) reboot

    clear_buffer(buf, REPORT_SIZE);
    write_buffer_32(buf, CMD_REBOOT);
    if(!hid_set_feature(dev, buf, REPORT_SIZE)) return false;

    return true;

}

int str2buf(void* buffer, char* delim_str, char* string, int buflen, int bufelem_size)
{
    char    *s;
    int     pos = 0;
    if( string==NULL ) return -1;
    memset(buffer,0,buflen);  // bzero() not defined on Win32?
    while((s = strtok(string, delim_str)) != NULL && pos < buflen){
        string = NULL;
        switch(bufelem_size) {
        case 1:
            ((uint8_t*)buffer)[pos++] = (uint8_t)strtol(s, NULL, 0); break;
        case 2:
            ((int*)buffer)[pos++] = (int)strtol(s, NULL, 0); break;
        }
    }
    return pos;
}

bool sanity_check_firmware(long fw_size, long offset)
{
    if(fw_size + offset > MAX_FIRMWARE)
    {
        fprintf(stderr, "ERROR: Firmware is too large too flash: 0x%08lx max allowed is 0x%08lx.\n", fw_size, MAX_FIRMWARE - offset);
        return false;
    }
    if(fw_size < 0x100)
    {
        fprintf(stderr, "ERROR: Firmware is too small.");
        return false;
    }

    return true;

    //TODO check pointer validity
}

bool sanity_check_jumploader_firmware(long fw_size)
{
    if(fw_size > QMK_OFFSET_DEFAULT)
    {
        fprintf(stderr, "ERROR: Jumper loader is too large: 0x%08lx max allowed is 0x%08lx.\n", fw_size, MAX_FIRMWARE - QMK_OFFSET_DEFAULT);
        return false;
    }

    return true;
}

int main(int argc, char* argv[])
{
    int opt, opt_index, res;
    hid_device *handle;

    uint16_t vid = 0;
    uint16_t pid = 0;
    long offset = 0;
    char* file_name = NULL;
    char* endptr = NULL;
    char* reboot_opt = NULL;

    bool reboot_requested = false;
    bool flash_jumploader = false;

    if(argc < 2)
    {   
        print_usage(PROJECT_NAME);
        exit(1);
    }

    struct option longoptions[] =
    {
        {"help",       no_argument, 0, 'h'},
        {"version",    no_argument, 0, 'V'},
        {"vidpid",     required_argument, NULL, 'v'},
        {"offset",     optional_argument, NULL, 'o'},
        {"file",       required_argument, NULL, 'f'},
        {"jumploader", required_argument, NULL, 'j'},
        {"reboot",     required_argument, NULL, 'r'},
        {NULL,0,0,0}
    };

    while ((opt = getopt_long(argc, argv, "hVv:o:r:f:j", longoptions, &opt_index)) != -1)
    {
        switch (opt)
        {
            case 'h': // Show help
                print_usage(PROJECT_NAME);
                break;
            case 'V': // version
                display_version(PROJECT_NAME);
                break;
            case 'v': // Set vid/pid
                if( sscanf(optarg, "%4hx/%4hx", &vid,&pid) !=2 ) {  // match "23FE/AB12"
                    if( !sscanf(optarg, "%4hx:%4hx", &vid,&pid) ) { // match "23FE:AB12"
                        // else try parsing standard dec/hex values
                        int wordbuf[4]; // a little extra space
                        str2buf(wordbuf, ":/, ", optarg, sizeof(wordbuf), 2);
                        vid = wordbuf[0]; pid = wordbuf[1];
                    }
                }
                // make sure we have the correct vidpid
                if(vid == 0 || pid == 0) {
                    fprintf(stderr, "ERROR: invalid vidpid -'%s'.\n",optarg);
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
                reboot_opt = optarg;
                reboot_requested = true;
                break;
            case 'j': // Jumploader
                flash_jumploader = true;
                break;
            case '?':
            default:
                switch (optopt)
                {
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
        if(opt == 'h' || opt == 'V') exit(1);
    }

    if (file_name == NULL)
    {
        fprintf(stderr, "ERROR: filename cannot be null.\n");
        exit(1);
    }

    printf("Firmware to flash: %s with offset 0x%04lx, device: 0x%04x/0x%04x.\n", file_name, offset, vid, pid);

    FILE* fp = fopen(file_name, "rb");

    if(fp == NULL)
    {
        fprintf(stderr, "ERROR: Could not open file (Does the file exist?).\n");
        fclose(fp);
        exit(1);
    }

    // Get file size
    fseek(fp, 0 , SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0 , SEEK_SET);

    // if jumploader is not 0x200 in length, add padded zeroes to file
    if(flash_jumploader && file_size < QMK_OFFSET_DEFAULT)
    {   
        printf("Warning: jumploader binary doesnt have a size of: 0x%04x bytes.\n", QMK_OFFSET_DEFAULT);
        printf("Truncating jumploader binary to: 0x%04x.\n", QMK_OFFSET_DEFAULT);

        // Close device before truncating it
        fclose(fp);
        if (truncate(file_name, QMK_OFFSET_DEFAULT) != 0)
        {
            fprintf(stderr, "ERROR: Could not truncate file.\n");
            exit(1);
        }
        
        // Try open the file again.
        fp = fopen(file_name, "rb");
        if(fp == NULL)
        {
            fprintf(stderr, "ERROR: Could not open file.\n");
            fclose(fp);
            exit(1);
        }
    }

    // Try to open the device
    res = hid_init();

    printf("Opening device...\n");
    handle = hid_open(vid, pid, NULL);

    uint8_t attempt_no = 1;
    while(handle == NULL && attempt_no <= MAX_ATTEMPTS) // Try {MAX ATTEMPTS} to connect to device.
    {   
        printf("Device failed to open, re-trying in 3 seconds. Attempt %d of %d...\n", attempt_no, MAX_ATTEMPTS);
        sleep(3);
        handle = hid_open(vid, pid, NULL);
        attempt_no++;
    }

    if(handle)
    {
        printf("Device opened successfully...\n");

        // Check VID/PID
        if(vid != SONIX_VID  || (pid != SN248_PID && pid != SN248B_PID && pid!= SN248C_PID && pid != SN268_PID && pid !=SN299_PID))
        {
            if(vid == EVISION_VID && !reboot_requested) printf("Warning: eVision VID detected! You probably need to use the reboot option.\n");
            if(vid == APPLE_VID && !reboot_requested) printf("Warning: Apple VID detected! You probably need to use the reboot option.\n");
            printf("Warning: Flashing a non-sonix bootloader device, you are now on your own.\n");
            sleep(3);

            // Set max firmware to 64k, useful when flashing a Sonix Board that isnt in BL mode (Redragons, Keychrons)
            MAX_FIRMWARE = MAX_FIRMWARE_SN32F240; // Maybe add a param to override this (?)
            printf("Warning: We assume a ROM size of 64k.\n");
            sleep(3);
        }

        // Set max fw size depending on VID/PID
        if(vid == SONIX_VID)
        {
            switch (pid)
            {
                case SN248_PID:
                case SN248B_PID:
                    MAX_FIRMWARE = MAX_FIRMWARE_SN32F240;
                    break;
                case SN248C_PID:
                    MAX_FIRMWARE = MAX_FIRMWARE_SN32F240C;
                case SN299_PID:
                    MAX_FIRMWARE = MAX_FIRMWARE_SN32F290;
                    break;
                case SN268_PID:
                    MAX_FIRMWARE = MAX_FIRMWARE_SN32F260;

                    if(!flash_jumploader && offset == 0) // Failsafe when flashing a 268 w/o jumploader and offset
                    {
                        printf("Warning: Flashing 26X without offset.\n");
                        printf("Fail safing to offset 0x%04x\n", QMK_OFFSET_DEFAULT);

                        offset = QMK_OFFSET_DEFAULT;
                    }
                    break;

                default:
                    fprintf(stderr, "ERROR: Unsupported sonix bootloader device. Quitting.\n");
                    exit(1);
            }
        }

        while (file_size % REPORT_SIZE != 0) file_size++; // Add padded zereos (if any) to file_size, since we are using a fixed 64 + 1 buffer, we need to take in consideration when the file doesnt fill the buffer.

        if( ((flash_jumploader  && sanity_check_jumploader_firmware(file_size)) || 
             (!flash_jumploader && sanity_check_firmware(file_size, offset))) &&
             (flash(handle, offset, fp, file_size, false, reboot_requested, reboot_opt)))
        {
            printf("Device succesfully flashed!\n");
        }
        else
        {
            fprintf(stderr, "ERROR: Could not flash the device. Try again.\n");
        }
    }
    else
    {
        fprintf(stderr, "ERROR: Could not open the device (Is the device connected?).\n");
    }

    fclose(fp);
    hid_close(handle);
    res = hid_exit();

    if(res < 0)
    {
        fprintf(stderr, "ERROR: Could not close the device.\n");
    }

	exit(0);
}
