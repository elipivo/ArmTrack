
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/select.h>

int CyGlDataAvail(const int fd);

int main(void) {
	printf("Initializing Wired CyberGlove\n");

	const char device[] = "/dev/rfcomm0";
	int tempID;

	if ((tempID = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK)) == -1) {
		printf("Failed to open device.\n");
		return -1;
	}
	printf("Opened device.\n");

	// Set BAUD rate
	struct termios options;
	tcgetattr(tempID, &options);
	cfmakeraw(&options);
	cfsetispeed(&options, B115200);
	cfsetospeed(&options, B115200);

	options.c_cflag |= (CLOCAL | CREAD) ;
	options.c_cflag &= ~PARENB ;
	options.c_cflag &= ~CSTOPB ;


	options.c_cflag &= ~CSIZE ;
	options.c_cflag |= CS8 ;
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG) ;
	options.c_oflag &= ~OPOST ;

	tcsetattr(tempID, TCSANOW, &options);

	usleep(10000) ;	// 10mS

	if (write(tempID, "T", 1) != 1) {
		printf("Failed to write to device.\n");
	}
	printf("Sent T\n");

	uint16_t w1 = 1152;
	uint16_t w2 = 1;
	if (write(tempID, &w1, 2) != 2) {
		printf("Failed to write to device.\n");
	}
	printf("Sent 1152\n");
	if (write(tempID, &w2, 2) != 2) {
		printf("Failed to write to device.\n");
	}
	printf("Sent 1\n");

	//wait at most 7 seconds for a response
	for (int i = 0; CyGlDataAvail(tempID) <= 0; i++) {
		sleep(1);
		if (i == 7) {
			fprintf(stderr, "CyberGlove Error: No response received.");
			return -1;
		}
	}
	uint8_t response;
	printf("Response received:\n");
	while (read(tempID, &response, 1) != -1) {
		printf("|%c - %i|\t", (char) response, (int) response);
	}
	printf("\n");

	usleep(10000);

	if (write(tempID, "?T", 2) != 2) {
		printf("Failed to write to device.\n");
	}
	printf("Sent ?T\n");

	//wait at most 7 seconds for a response
	for (int i = 0; CyGlDataAvail(tempID) <= 0; i++) {
		sleep(1);
		if (i == 7) {
			fprintf(stderr, "CyberGlove Error: No response received.");
			return -1;
		}
	}
	printf("Response received:\n");
	while (read(tempID, &response, 1) != -1) {
		printf("|%c - %i|\t", (char) response, (int) response);
	}
	printf("\n");

	if (write(tempID, "S", 1) != 1) {
		printf("Failed to write to device.\n");
	}
	printf("Sent G\n");

	for (int i = 0; i < 100; i++) {

		//wait at most 7 seconds for a response
		for (int i = 0; CyGlDataAvail(tempID) <= 0; i++) {
			sleep(1);
			if (i == 7) {
				fprintf(stderr, "CyberGlove Error: No response received.");
				return -1;
			}
		}

		printf("Response received:\n");
		while (read(tempID, &response, 1) != -1) {
			printf("|%c - %i|\t", (char) response, (int) response);
		}
		printf("\n");
	}

	return 1;
}

int CyGlDataAvail(const int fd) {
	int result;

	if (ioctl(fd, FIONREAD, &result) == -1) {
		return -1;
	}

	return result;
}
