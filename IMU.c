/*
 * Name: IMU.c
 * Author: Elijah Pivo
 *
 * IMU interface
 *
 */

#include "IMU.h"

int initializeIMU(IMU* IMU) {

	IMU->id = -1;
	IMU->hasNewRead1 = 0;
	IMU->hasNewRead2 = 0;
	IMU->bufferToUse = 2;
	for (int i = 0; i < IMU_READ_SZ; i++) {
		IMU->readBuffer1[i] = 0;
		IMU->readBuffer2[i] = 0;
		IMU->read[i] = 0;

	}
	IMU->reads = 0;
	IMU->errors = 0;
	IMU->consecutiveErrors = 0;

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

	if (correctResponse == 1) {
		IMU->id = tempID;
		return IMU->id;
	} else {
		return -1;
	}
}

int reconnectIMU(IMU* IMU) {

	//save info we want to save
	int e = IMU->errors;
	int r = IMU->reads;

	//close the device
	closeIMU(IMU);

	//restart the device
	initializeIMU(IMU);

	IMU->errors = e;
	IMU->reads = r;
	return IMU->id;
}

int startIMU(IMU* IMU) {

	//try to initialize 4 times

	for (int attempt = 0; initializeIMU(IMU) == -1; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
		sleep(1);
	}

	return 1;
}

int restartIMU(IMU* IMU) {

	//try to reconnect 4 times
	for (int attempt = 0; reconnectIMU(IMU) == -1; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
		sleep(1);
	}

	return 1;
}

int getIMUData(IMU* IMU, double time) {

	IMU->reads++;

	fd_set set;
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 24000; //wait 24 ms
	int retval = 1;
	FD_ZERO(&set);
	FD_SET(IMU->id, &set);

	unsigned char stop;

	switch (IMU->reads % 2) {
	case 0:
		//read into readBuffer1 on even reads
		IMU->readBuffer1Time = time;

		//request reading
		if (write(IMU->id, "w", 1) != 1) {
			return -1;
		}

		retval = select(IMU->id + 1, &set, NULL, NULL, &timeout);
		if (retval <= 0) {
			//either error or timeout was reached
			return -1;
		} else {
			do {
				//get the reading
				read(IMU->id, &IMU->readBuffer1, IMU_READ_SZ * sizeof(float));

				//check for the stop byte
				if (read(IMU->id, &stop, sizeof(unsigned char))
						!= sizeof(unsigned char)) {
					//no stop byte
					return -1;
				}
				if (stop != 0xFF) {
					//not the stop byte
					return -1;
				}
			} while (IMUDataAvail(IMU->id) != 0);
		}


		IMU->hasNewRead1 = 1;
		break;
	case 1:
		IMU->readBuffer2Time = time;

		//request reading
		if (write(IMU->id, "w", 1) != 1) {
			return -1;
		}

		retval = select(IMU->id + 1, &set, NULL, NULL, &timeout);
		if (retval <= 0) {
			//either error or timeout was reached
			return -1;
		} else {
			do {
				//get the reading
				read(IMU->id, &IMU->readBuffer2, IMU_READ_SZ * sizeof(float));

				//check for the stop byte
				if (read(IMU->id, &stop, sizeof(unsigned char))
						!= sizeof(unsigned char)) {
					//no stop byte
					return -1;
				}
				if (stop != 0xFF) {
					//not the stop byte
					return -1;
				}
			} while (IMUDataAvail(IMU->id) != 0);
		}

		IMU->hasNewRead2 = 1;
		break;
	}

	return 1;
}

int updateIMURead(IMU* IMU) {

	switch (IMU->bufferToUse) {
	case 1:
		IMU->bufferToUse = 2;
		IMU->readTime = IMU->readBuffer1Time;
		if (IMU->hasNewRead1 == 1) {
			//New data available
			memcpy(&IMU->read, &IMU->readBuffer1, IMU_READ_SZ * sizeof(float)); //update the read field
			IMU->consecutiveErrors = 0; //data collection was successful
			IMU->hasNewRead1 = 0;
			return 1;
		}
		break;
	case 2:
		IMU->bufferToUse = 1;
		IMU->readTime = IMU->readBuffer2Time;
		if (IMU->hasNewRead2 == 1) {
			//New data available
			memcpy(&IMU->read, &IMU->readBuffer2, IMU_READ_SZ * sizeof(float)); //update the read field
			IMU->consecutiveErrors = 0; //data collection was successful
			IMU->hasNewRead2 = 0;
			return 1;
		}
		break;
	}

	//data collection must have been unsuccessful
	IMU->errors++;
	IMU->consecutiveErrors++;
	return -1;
}

void closeIMU(IMU* IMU) {

	tcflush(IMU->id, TCIOFLUSH);
	close(IMU->id);

	IMU->id = -1;
	IMU->hasNewRead1 = 0;
	IMU->hasNewRead2 = 0;
	for (int i = 0; i < IMU_READ_SZ; i++) {
		IMU->readBuffer1[i] = 0;
		IMU->readBuffer2[i] = 0;
		IMU->read[i] = 0;
	}
	IMU->reads = 0;
	IMU->errors = 0;
	IMU->consecutiveErrors = 0;
}

int IMUDataAvail(const int fd) {
	int result;

	if (ioctl(fd, FIONREAD, &result) == -1) {
		return -1;
	}

	return result;
}
