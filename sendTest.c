/**
 * A program that allows me to send characters to a particular device
 * and then prints any response.
 */

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>

int main() {

	char device[100];
	printf("Device Name: ");
	scanf("%s", device);
	printf("\nCommunicating with: %s\n", device);

	int deviceID;
	if ((deviceID = open(device, O_RDWR | O_NOCTTY)) == -1) {
		printf("Couldn't open that device.\n");
		exit(1);
	}

	struct termios options;
	tcgetattr(deviceID, &options);
	cfmakeraw(&options);
	cfsetispeed(&options, 115200);
	cfsetospeed(&options, 115200);
	tcsetattr(deviceID, TCSANOW, &options);

	fd_set set;
	struct timeval timeout;
	timeout.tv_sec = 5; //wait 1 second for any response
	timeout.tv_usec = 0;
	int retval  = 1;
	FD_ZERO(&set);
	FD_SET(deviceID, &set);

	char cont = 'y';
	do {
		char writeBuf;
		printf("\nEnter character to send: ");
		scanf("%c", &writeBuf);
		printf("\nSending: %c\n", writeBuf);

		if (write(deviceID, &writeBuf, 1) != 1) {
			printf("Couldn't send character.\n");
		} else {
			printf("Awaiting response.\n");

			char response;
			while (retval != -1 && retval != 0) {

				retval = select(deviceID + 1, &set, NULL, NULL, &timeout);
				if (retval == -1) {
					perror("select\n");
				} else if (retval == 0) {
					printf("timeout\n");
				} else {
					read(deviceID, &response, 1);
					printf("|%c -", response);
					printf(" %d|", response);
				}

			}
			retval = 1;
//
//			read(deviceID, &response, 1);
//			printf("|%c|", response);
//			printf("\n");
		}

		printf("Would you like to send another character? (y/n): ");
		scanf("%c", &cont);
	} while (cont == 'y');

	return 0;
}
