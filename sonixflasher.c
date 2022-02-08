#include <stdio.h>
#include <wchar.h>
#include <stdint.h>
#include <stdbool.h>

#include <hidapi.h>

#define RESPONSE_LEN 64
#define MAX_FIRMWARE_SN32F260 (30 * 1024) //30k
#define MAX_FIRMWARE_SN32F240 (30 * 1024) //64k

#define CMD_INIT 0x01AA5500
#define CMD_PREPARE 0x05AA5500
#define CMD_REBOOT 0x07AA5500

#define EXPECTED_STATUS 0xFAFAFAFA

long MAX_FIRMWARE = MAX_FIRMWARE_SN32F260;

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

    for(int i = 0; i < 4; i++)
    {
        r += data[i] << (8 * (3 - i));
        printf("%02x\n", data[i]);
    }

    printf("%08x\n", r);

    return r == expected_result;
}

void write_buffer_32(unsigned char *data, uint32_t cmd)
{
    for(int i = 0; i < 4; i++)
    {
        data[i] = (unsigned char)(cmd >> (8 * (3 - i)));
    }
}

void write_buffer_32_se(unsigned char *data, uint32_t cmd)
{
    uint32_t cmd_swapped = ((cmd>>24)&0xff) | // move byte 3 to byte 0
                           ((cmd<<8)&0xff0000) | // move byte 1 to byte 2
                           ((cmd>>8)&0xff00) | // move byte 2 to byte 1
                           ((cmd<<24)&0xff000000); // byte 0 to byte 3

    write_buffer_32(data, cmd_swapped);
}

bool hid_set_feature(hid_device *dev, unsigned char *data, size_t length)
{
    if(length > 65)
    {
        printf("Report cant be more than 65 bytes!!\n");
        return false;
    }

    int res = hid_send_feature_report(dev, data, length);

    if(res < 0)
    {
        printf("Error while writing!\n");
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
            printf("Firmware is too large too flash\n");
            return false;
        }
    }
    
    // 1) Initialize
    clear_buffer(buf, 65);
    write_buffer_32(buf+1, CMD_INIT);
    hid_set_feature(dev, buf, 65);

    clear_buffer(buf, 65);
    read_bytes = hid_get_feature(dev, buf);
    print_buffer(buf, 65);
    if(read_bytes != RESPONSE_LEN + 1)
    {
        printf("Failed to initialize: got response of length %d, expected %d\n", read_bytes, RESPONSE_LEN);
        return false;
    }
    if(!read_response_32(buf+1, CMD_INIT, &resp)) // Read cmd
    {
        printf("Failed to initialize: response cmd 1 is 0x%08x, expected 0x%08x\n", resp, CMD_INIT);
        return false;
    }

    // 2) Prepare for flash
    clear_buffer(buf, 65);
    write_buffer_32(buf+1, CMD_PREPARE);
    write_buffer_32_se(buf+5, (uint32_t)offset);
    write_buffer_32_se(buf+9, (uint32_t)(fw_size)); // This is borked, needs to be fixed.

    hid_set_feature(dev, buf, 65);

    clear_buffer(buf, 65);
    read_bytes = hid_get_feature(dev, buf);
    if(!read_response_32(buf + 1, CMD_PREPARE, &resp))// Read cmd
    {
        printf("Failed to initialize: response cmd is 0x%08x, expected 0x%08x\n", resp, CMD_PREPARE);
        return false;
    }
    if(!read_response_32(buf + 5, EXPECTED_STATUS, &status))// Read status
    {
        printf("Failed to initialize: response status is 0x%08x, expected 0x%08x\n", status, EXPECTED_STATUS);
        return false;
    }

    // 3) Flash
    size_t bytes_read = 0;
    clear_buffer(buf, 65);
    while ((bytes_read = fread(buf+1, 1, 64, firmware)) > 0)
    {
        hid_set_feature(dev, buf, 65);
        clear_buffer(buf, 65);
    }
    
    // 4) reboot
    clear_buffer(buf, 65);
    write_buffer_32(buf + 1, CMD_REBOOT);
    hid_set_feature(dev, buf, 65);

    return true;
    
}

int main(int argc, char* argv[])
{
	int res;
    hid_device *handle;
    FILE* fp = fopen("test.bin", "rb");
    if(fp) {
        fseek(fp, 0 , SEEK_END);
        long file_size = ftell(fp);
        fseek(fp, 0 , SEEK_SET);


        hid_device *handle;
        res = hid_init();
        handle = hid_open(0x0c45, 0x7040, NULL);

        flash(handle, 0x0, fp, file_size, true);

        fclose(fp);
        hid_close(handle);
        res = hid_exit();
        
    }

	return 0;
}