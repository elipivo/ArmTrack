/**
 * Figuring out how to use the EMG driver library...
 *
 * Compile with:
 *
 * gcc -g -std=gnu99 -Wall -I. -o ExpEMG ExpEMG.c -L. -lmccusb  -lm -L/usr/local/lib -lhidapi-libusb -lusb-1.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "/home/pi/mcc-libusb/pmd.h"
#include "/home/pi/mcc-libusb/usb-1408FS.h"

int main(void) {

	int ret = libusb_init(NULL);
	if (ret < 0) {
		fprintf(stderr, "ERROR: Failed to initialize libusb.\n");
		return 1;
	}

	libusb_device_handle *udev = NULL;
	if ((udev = usb_device_find_USB_MCC(USB1408FS_PID, NULL))) {
		fprintf(stderr, "USB-1408FS Device is found!\n");
	} else {
		fprintf(stderr, "No device found.\n");
		return 1;;
	}

	// claim all the needed interfaces for AInScan
	for (int i = 1; i <= 3; i++) {
		ret = libusb_detach_kernel_driver(udev, i);
		if (ret < 0) {
			fprintf(stderr, "Can't detach kernel from interface");
			usbReset_USB1408FS(udev);
			exit(-1);
		}
		ret = libusb_claim_interface(udev, i);
		if (ret < 0) {
			fprintf(stderr, "Can't claim interface.");
			exit(-1);
		}
	}

	usbDConfigPort_USB1408FS(udev, DIO_PORTA, DIO_DIR_OUT);
	usbDConfigPort_USB1408FS(udev, DIO_PORTB, DIO_DIR_IN);
	usbDOut_USB1408FS(udev, DIO_PORTA, 0);
	usbDOut_USB1408FS(udev, DIO_PORTA, 0);

	fprintf(stderr, "Scanning data!\n");

	float freq = 8000;
	int count = 200;
	uint8_t options = AIN_EXECUTION | AIN_GAIN_QUEUE;
	signed short in_data[200];
	struct timeval start, end;

	for (int reads = 0; reads < 2500; reads++) {
		usbAInStop_USB1408FS(udev);

		for (int i = 0; i < 200; i++) {
			in_data[i] = 9;
		}

		gettimeofday(&start, NULL);
		usbAInScan_USB1408FS_SE(udev, 0, 0, count, &freq, options, in_data);
		gettimeofday(&end, NULL);

		printf("%f\n", (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) * .000001);
		for (int i = 0; i < 25; i++) {
			printf("Read %i:   ", i + 1);
			for (int j = 0; j < 8; j++) {
				printf("%.2fV   ", volts_1408FS_SE(in_data[i * 8 + j]));
			}
			printf("\n");
		}
		printf("\n");
	}


	fprintf(stderr, "Data scan done!\n");

	libusb_clear_halt(udev, LIBUSB_ENDPOINT_IN | 1);
	libusb_clear_halt(udev, LIBUSB_ENDPOINT_OUT| 2);
	libusb_clear_halt(udev, LIBUSB_ENDPOINT_IN | 3);
	libusb_clear_halt(udev, LIBUSB_ENDPOINT_IN | 4);
	for (int i = 0; i <= 4; i++) {
		libusb_release_interface(udev, i);
	}
	libusb_close(udev);

	return 1;
}
