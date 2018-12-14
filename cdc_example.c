/*
 * This is a simple example to communicate with a CDC-ACM USB device
 * using libusb.
 *
 * Author: Christophe Augier <christophe.augier@gmail.com>
 */
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <libusb-1.0/libusb.h>

/* You may want to change the VENDOR_ID and PRODUCT_ID
 * depending on your device.
 */
#define VENDOR_ID      0x03eb //Atmel Corp. Controller
#define PRODUCT_ID     0x2404 //Canbus

#define ACM_CTRL_DTR   0x01
#define ACM_CTRL_RTS   0x02

/* We use a global variable to keep the device handle
 */
static struct libusb_device_handle *devh = NULL;

/* The Endpoint address are hard coded. You should use lsusb -v to find
 * the values corresponding to your device.
 */
static int ep_in_addr  = 0x81;
static int ep_out_addr = 0x02;

void write_char(unsigned char *c, int length)
{
    /* To send a char to the device simply initiate a bulk_transfer to the
     * Endpoint with address ep_out_addr.
     */
    int actual_length;
    if (libusb_bulk_transfer(devh, ep_out_addr, c, length,
                             &actual_length, 0) < 0) {
        fprintf(stderr, "Error while sending char\n");
    }
}

int read_chars(unsigned char * data, int size)
{
    /* To receive characters from the device initiate a bulk_transfer to the
     * Endpoint with address ep_in_addr.
     */
    int actual_length;
    int rc = libusb_bulk_transfer(devh, ep_in_addr, data, size, &actual_length,
                                  1000);
    if (rc == LIBUSB_ERROR_TIMEOUT) {
        printf("timeout (%d)\n", actual_length);
        return -1;
    } else if (rc < 0) {
        fprintf(stderr, "Error while waiting for char\n");
        return -1;
    }

    return actual_length;
}

static void usage(int argc, char **argv)
{
	printf("Usage:\t\n");
	printf("%s prog command1 command2\n", argv[0]);
	printf("%s read_sw_version\n", argv[0]);
	printf("%s request_jump_to_bootloader_app\n", argv[0]);
	printf("%s request_to_reset_device\n", argv[0]);
	printf("%s query_device_mode\n", argv[0]);
	printf("%s enable_CANbus\n", argv[0]);
	printf("%s disable_CANbus\n", argv[0]);
	printf("%s enable_loopback_mode\n", argv[0]);
	printf("%s disable_loopback_mode\n", argv[0]);
	printf("%s CANbus_status\n", argv[0]);
	printf("%s CANbus_err_count\n", argv[0]);
}

int main(int argc, char **argv)
{
	const char *cmd, *cmd1, *cmd2 = NULL;
    int rc, cmd_size;
    unsigned char data[64];

	memset(data, 0, sizeof(data));
	
	if (argc == 1)
	{
		usage(argc, argv);
		return 0;
	}
	else
		cmd = argv[1];

	if (argc > 0 && strcmp(cmd, "prog") == 0)
	{
		cmd1 = argv[2];
		cmd2 = argv[3];
		data[0] = (unsigned char)strtoul(cmd1, NULL, 16);
		data[1] = (unsigned char)strtoul(cmd2, NULL, 16);
		cmd_size = 2;
	}
	else if (argc > 0 && strcmp(cmd, "read_sw_version") == 0)
	{
		data[0] = 0xFF;
		data[1] = 0xC0;
		cmd_size = 2;
	}
	else if (argc > 0 && strcmp(cmd, "request_jump_to_bootloader_app") == 0)
	{
		data[0] = 0xFF;
		data[1] = 0xB0;
		cmd_size = 2;
	}
	else if (argc > 0 && strcmp(cmd, "request_to_reset_device") == 0)
	{
		data[0] = 0xFF;
		data[1] = 0xB1;
		cmd_size = 2;
	}
	else if (argc > 0 && strcmp(cmd, "query_device_mode") == 0)
	{
		data[0] = 0xFF;
		data[1] = 0xB2;
		cmd_size = 2;
	}
	else if (argc > 0 && strcmp(cmd, "enable_CANbus") == 0)
	{
		data[0] = 0xFF;
		data[1] = 0x00;
		data[2] = 0x01;
		cmd_size = 3;
	}
	else if (argc > 0 && strcmp(cmd, "disable_CANbus") == 0)
	{
		data[0] = 0xFF;
		data[1] = 0x00;
		data[2] = 0x00;
		cmd_size = 3;
	}
	else if (argc > 0 && strcmp(cmd, "enable_loopback_mode") == 0)
	{
		data[0] = 0xFF;
		data[1] = 0x02;
		data[2] = 0x01;
		cmd_size = 3;
	}
	else if (argc > 0 && strcmp(cmd, "disable_loopback_mode") == 0)
	{
		data[0] = 0xFF;
		data[1] = 0x02;
		data[2] = 0x00;
		cmd_size = 3;
	}
	else if (argc > 0 && strcmp(cmd, "CANbus_status") == 0)
	{
		data[0] = 0xFF;
		data[1] = 0x06;
		cmd_size = 2;
	}
	else if (argc > 0 && strcmp(cmd, "CANbus_err_count") == 0)
	{
		data[0] = 0xFF;
		data[1] = 0x07;
		cmd_size = 2;
	}
	else
	{
		usage(argc, argv);
		return 0;
	}

    /* Initialize libusb
     */
    rc = libusb_init(NULL);
    if (rc < 0) {
        fprintf(stderr, "Error initializing libusb: %s\n", libusb_error_name(rc));
        exit(1);
    }

    /* Set debugging output to max level.
     */
    libusb_set_debug(NULL, 3);

    /* Look for a specific device and open it.
     */
    devh = libusb_open_device_with_vid_pid(NULL, VENDOR_ID, PRODUCT_ID);
    if (!devh) {
        fprintf(stderr, "Error finding USB device\n");
        goto out;
    }

    /* As we are dealing with a CDC-ACM device, it's highly probable that
     * Linux already attached the cdc-acm driver to this device.
     * We need to detach the drivers from all the USB interfaces. The CDC-ACM
     * Class defines two interfaces: the Control interface and the
     * Data interface.
     */
    for (int if_num = 0; if_num < 2; if_num++) {
        if (libusb_kernel_driver_active(devh, if_num)) {
            libusb_detach_kernel_driver(devh, if_num);
        }
        rc = libusb_claim_interface(devh, if_num);
        if (rc < 0) {
            fprintf(stderr, "Error claiming interface: %s\n",
                    libusb_error_name(rc));
            goto out;
        }
    }

    /* Start configuring the device:
     * - set line state
     */
    rc = libusb_control_transfer(devh, 0x21, 0x22, ACM_CTRL_DTR | ACM_CTRL_RTS,
                                0, NULL, 0, 0);
    if (rc < 0) {
        fprintf(stderr, "Error during control transfer: %s\n",
                libusb_error_name(rc));
    }

    /* - set line encoding: here 9600 8N1
     * 9600 = 0x2580 ~> 0x80, 0x25 in little endian
     */
    unsigned char encoding[] = { 0x80, 0x25, 0x00, 0x00, 0x00, 0x00, 0x08 };
    rc = libusb_control_transfer(devh, 0x21, 0x20, 0, 0, encoding,
                                sizeof(encoding), 0);
    if (rc < 0) {
        fprintf(stderr, "Error during control transfer: %s\n",
                libusb_error_name(rc));
    }

    /* We can now start sending or receiving data to the device
     */
#if 1
    unsigned char buf[65];
    int len;

   	fprintf(stdout, "Sent:");
	for (int i=0; i < cmd_size; i++)
    	fprintf(stdout, "0x%02X ", data[i]);
	fprintf(stdout, "\n");

    while (1) {
        write_char(data, sizeof(data));
        len = read_chars(buf, 64);
		if (cmd_size == 3) break;
		if ((buf[0] != data[0]) || (buf[1] != data[1]))
		{
            fprintf(stdout, "Polling\n");
			continue;
		}
        fprintf(stdout, "Received (%d):",len);
        for (int i = 0 ;i < len ; i++)
            fprintf(stdout, "0x%02X ",buf[i]);
        if (len != 0) {
            fprintf(stdout, "\n");
            break;
        }
    }
#endif
    libusb_release_interface(devh, 0);

out:
    if (devh)
            libusb_close(devh);
    libusb_exit(NULL);
    return rc;
}
