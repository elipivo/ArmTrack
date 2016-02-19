#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#define IMU_READ_SZ 12
#define IMU_BAUD B115200

int IMUDataAvail(const int fd);

int main(void) {

	const char device[] = "/dev/ttyACM0";
	int tempID = 0;

	if ((tempID = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK)) == -1) {
		fprintf(stderr, "IMU Error: Failed to connect.\n");
		return -1;
	}

	// Set BAUD rate
	struct termios options;
	tcgetattr(tempID, &options);
	cfmakeraw(&options);
	cfsetispeed(&options, IMU_BAUD);
	cfsetospeed(&options, IMU_BAUD);

	tcsetattr(tempID, TCSANOW, &options);

	usleep(10000);      // 10mS

	char send = 'i';
	if (write(tempID, &send, 1) != 1) {
		fprintf(stderr, "IMU Error: Failed to request state.\n");
		return -1;
	}

	unsigned char temp = 0x02;

	for (int i = 0; IMUDataAvail(tempID) <= 0; i++) {
		//wait at most 2 seconds
		if (i == 2) {
			fprintf(stderr, "IMU Error: No response received.\n");
			return -1;
		}
		sleep(1);
	}

	int avail = IMUDataAvail(tempID);
	int correctResponse = 0;
	if (avail == 1) {

		read(tempID, &temp, 1);

		if (temp == 'y') {
			correctResponse = 1;
		} else {
			fprintf(stderr, "IMU Error: Wrong response received: *%c*.\n", temp);

			return -1;
		}

	} else {
		//got too much data, print it out and quit
		fprintf(stderr, "IMU Error: Wrong Response received:\n");

		for (int i = 0; i < avail; i++) {
			read(tempID, &temp, 1);
			fprintf(stderr, "|%c|", temp);
		}
		fprintf(stderr, "\n");
		return -1; //try to get response again
	}

		if (write(tempID, "s", 1) != 1) {
			printf("Failed to write to device.\n");
		}
		printf("Sent s\n");

	/*********************/
	while (1 == 1) {



		fd_set set;
		struct timeval timeout;
		timeout.tv_sec = 8;
		timeout.tv_usec = 60000; //wait 30 ms
		int retval = 1;
		FD_ZERO(&set);
		FD_SET(tempID, &set);
		float readBuffer[IMU_READ_SZ];
		char stop;
		retval = select(tempID + 1, &set, NULL, NULL, &timeout);
		if (retval <= 0) {
			//either error or timeout was reached
			printf("Here 1\n");
			return -1;
		} else {
			do {
				//get the reading
				read(tempID, &readBuffer, IMU_READ_SZ * sizeof(float));

				//check for the stop byte
				if (read(tempID, &stop, sizeof(unsigned char))
						!= sizeof(unsigned char)) {
					//no stop byte
					printf("Here 2\n");
					return -1;
				}
				if (stop != 0xFF) {
					//not the stop byte
					printf("Here 3\n");
					return -1;
				}
			} while (IMUDataAvail(tempID) != 0);
		}

		for (int i = 0; i < IMU_READ_SZ; i++) {
			printf("%f\t", readBuffer[i]);
		}
		printf("\n");

//		printf("Here 4\n");


	}
}

int IMUDataAvail(const int fd) {
	int result;

	if (ioctl(fd, FIONREAD, &result) == -1) {
		return -1;
	}

	return result;
}

