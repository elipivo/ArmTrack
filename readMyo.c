/*
  Reads data from EMG through Myo.

  Compile with: gcc -o readMyo readMyo.c -std=gnu99 -Wall -Wextra

*/

#include <stdio.h>
#include <stdlib.h>
#include "Myo.h"

#define MYO_READ_SIZE 8

int MyoDataAvail (const int fd);

int main(void) {
	const char device[] = "/dev/ttyACM0";

	struct Myo Myo;

	for (int i = 0; i < MYO_READ_SZ; i++) {
		Myo.read[i] = 0;
	}
	Myo.id = -1;

	if ((Myo.id = open(device, O_RDWR)) == -1) {
		fprintf(stderr, "Myo Error: Failed to connect.\n");
		Myo.id = -1;
		return -1;
	}

	//try to connect
	fprintf(stderr, "Get list of devices.");
	uint8_t writeBuf[5];
	writeBuf[0] = 0x00;
	writeBuf[1] = 0x01;
	writeBuf[2] = 6;
	writeBuf[3] = 2;
	writeBuf[4] = 0x01;
	write(Myo.id, &writeBuf, 5);

	usleep(1000);

	uint8_t readBuf;
	while (MyoDataAvail(Myo.id) > 0) {
		read(Myo.id, &readBuf, 1);
		fprintf(stderr, "*%d*\n", (int) readBuf);
		usleep(100);
	}
}

int MyoDataAvail (const int fd) {
	int result ;

	if (ioctl (fd, FIONREAD, &result) == -1)
		return -1 ;

	return result ;
}
