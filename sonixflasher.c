#include <stdio.h>
#include <wchar.h>
#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include <hidapi.h>

#define RESPONSE_LEN 64
#define MAX_FIRMWARE_SN32F260 (30 * 1024) //30k
#define MAX_FIRMWARE_SN32F240 (64 * 1024) //64k
#define QMK_OFFSET_DEFAULT 0x200

#define CMD_BASE 0x55AA0000
#define CMD_INIT (CMD_BASE + 0x100)
#define CMD_PREPARE (CMD_BASE + 0x500)
#define CMD_REBOOT (CMD_BASE + 0x700)

#define SONIX_VID  0x0c45
#define SN268_PID  0x7010
#define SN248B_PID 0x7040
#define SN248_PID  0x7900

#define EXPECTED_STATUS 0xFAFAFAFA

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
    if(length > 65)
    {
        fprintf(stderr, "ERROR: Report cant be more than 65 bytes!!\n");
        return false;
    }

    int res = hid_send_feature_report(dev, data, length);

    if(res < 0)
    {
        fprintf(stderr, "ERROR: Error while writing!\n");
        return false;
    }

    return true;
}

int hid_get_feature(hid_device *dev, unsigned char *data)
{
    return hid_get_feature_report(dev, data, RESPONSE_LEN + 1);
}

bool flash(hid_device *dev, long offset, FILE *firmware, long fw_size, bool skip_size_check)
{
    unsigned char buf[65];
    int read_bytes;
    uint32_t resp = 0;
    uint32_t status = 0;

    if(skip_size_check == false)
    {
        if(fw_size + offset > MAX_FIRMWARE)
        {
            printf("ERROR: Firmware is too large too flash\n");
            return false;
        }
    }
    
    // 1) Initialize

    printf("Initializing flash...\n");

    clear_buffer(buf, 65);
    write_buffer_32(buf, CMD_INIT);
    hid_set_feature(dev, buf, 65);

    clear_buffer(buf, 65);
    read_bytes = hid_get_feature(dev, buf);
    if(read_bytes != RESPONSE_LEN + 1)
    {
        fprintf(stderr, "ERROR: Failed to initialize: got response of length %d, expected %d\n", read_bytes, RESPONSE_LEN);
        return false;
    }
    if(!read_response_32(buf, CMD_INIT, &resp)) // Read cmd
    {   
        fprintf(stderr, "ERROR: Failed to initialize: response cmd is 0x%08x, expected 0x%08x\n", resp, CMD_INIT);
        return false;
    }

    // // 2) Prepare for flash

    printf("Preparing for flash...\n");

    clear_buffer(buf, 65);
    write_buffer_32(buf, CMD_PREPARE);
    write_buffer_32(buf+5, (uint32_t)offset);
    write_buffer_32(buf+9, (uint32_t)(fw_size/64)); 
    hid_set_feature(dev, buf, 65);

    clear_buffer(buf, 65);
    read_bytes = hid_get_feature(dev, buf);
    if(!read_response_32(buf, CMD_PREPARE, &resp)) // Read cmd
    {
        fprintf(stderr, "ERROR: Failed to initialize: response cmd is 0x%08x, expected 0x%08x\n", resp, CMD_PREPARE);
        return false;
    }
    if(!read_response_32(buf + 5, EXPECTED_STATUS, &status))// Read status
    {
        fprintf(stderr, "ERROR: Failed to initialize: response status is 0x%08x, expected 0x%08x\n", status, EXPECTED_STATUS);
        return false;
    }

    // // 3) Flash

    printf("Flashing device, please wait...\n");

    size_t bytes_read = 0;
    clear_buffer(buf, 65);
    while ((bytes_read = fread(buf+1, 1, 64, firmware)) > 0)
    {
        hid_set_feature(dev, buf, 65);
        clear_buffer(buf, 65);
    }

    printf("Flashing done. Rebooting\n");

    // // 4) reboot

    clear_buffer(buf, 65);
    write_buffer_32(buf, CMD_REBOOT);
    hid_set_feature(dev, buf, 65);

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
        fprintf(stderr, "ERROR: Firmware is too large too flash: 0x%08x max allowed is 0x%08x\n", fw_size, MAX_FIRMWARE - offset);
        return false;
    }
    if(fw_size < 0x100)
    {
        fprintf(stderr, "ERROR: Firmware is too small");
        return false;
    }

    return true;

    //TODO check pointer validity
}

bool sanity_check_jumploader_firmware(long fw_size)
{
    if(fw_size > QMK_OFFSET_DEFAULT)
    {
        fprintf(stderr, "ERROR: Jumper loader is too large: 0x%08x max allowed is 0x%08x\n", fw_size, MAX_FIRMWARE - QMK_OFFSET_DEFAULT);
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

    bool flash_jumploader = false;

    if(argc < 2)
    {   
        print_usage("sonixflasher");
        exit(1);
    }

    struct option longoptions[] =
    {
        {"help",       no_argument, 0, 'h'},
        {"vidpid",     required_argument, NULL, 'v'},
        {"offset",     optional_argument, NULL, 'o'},
        {"file",       required_argument, NULL, 'f'},
        {"jumploader", required_argument, NULL, 'j'},
        {NULL,0,0,0}
    };

    while ((opt = getopt_long(argc, argv, "hv:o:f:j", longoptions, &opt_index)) != -1)
    {
        switch (opt)
        {
            case 0: // Show help
                print_usage("sonixflasher");
                break;
            case 'v': // Set vid/pid
                if( sscanf(optarg, "%4hx/%4hx", &vid,&pid) !=2 ) {  // match "23FE/AB12"
                    if( !sscanf(optarg, "%4hx:%4hx", &vid,&pid) ) { // match "23FE:AB12"
                        // else try parsing standard dec/hex values
                        int wordbuf[4]; // a little extra space
                        int parsedlen = str2buf(wordbuf, ":/, ", optarg, sizeof(wordbuf), 2);
                        vid = wordbuf[0]; pid = wordbuf[1];
                    }
                }
                break;
            case 'f': // file name
                file_name = optarg;
                break;
            case 'o': // offset 
                offset = strtol(optarg,NULL, 0);
                break;
            case 'j':
                flash_jumploader = true;
                break;
            case '?':
            default:
                switch (optopt)
                {
                    case 'f':
                    case 'v':
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

    }

    if (file_name == NULL)
    {
        fprintf(stderr, "ERROR: filename cannot be null.\n");
        exit(1);
    }

    printf("Firmware to flash: %s with offset 0x%04x, device: 0x%04x/0x%04x\n", file_name, offset, vid, pid);

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

    // Try to open the device
    res = hid_init();
    handle = hid_open(vid, pid, NULL);

    // Check VID/PID
    if(vid != SONIX_VID  || (pid != SN248_PID && pid != SN248B_PID && pid != SN268_PID))
    {
        printf("Warning: Flashing a non-sonix device, you are now on your own\n");
    }

    if(handle)
    {
        // Set max fw size depending on VID/PID
        if(vid == SONIX_VID)
        {
            switch (pid)
            {
                case SN248_PID:
                case SN248B_PID:
                    MAX_FIRMWARE = MAX_FIRMWARE_SN32F240;
                    break;

                case SN268_PID:
                default:
                    MAX_FIRMWARE = MAX_FIRMWARE_SN32F260;

                    if(!flash_jumploader) // Failsafe when flashing a 268 w/o jumploader and offset
                    {
                        printf("Warning! Flashing 26X without offset\n");
                        printf("Fail safing to offset 0x200\n");

                        offset = QMK_OFFSET_DEFAULT;
                    }

                    break;
            }
        }

        if( ((flash_jumploader  && sanity_check_jumploader_firmware(file_size)) || 
             (!flash_jumploader && sanity_check_firmware(file_size, offset))) &&
             (flash(handle, offset, fp, file_size, false)))
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
	exit(0);
}
