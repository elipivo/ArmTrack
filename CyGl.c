/*
 * Name: CyGl.c
 * Author: Elijah Pivo
 *
 * Wireless CyberGlove II interface
 */


#include "CyGl.h"

int initializeCyGl(CyGl* CyGl) {

	if (initializeWiredCyGl(CyGl) == -1) {
		if (initializeWirelessCyGl(CyGl) == -1) {
			return -1;
		}
	}

	return CyGl->id;

}

int initializeWirelessCyGl(CyGl* CyGl) {

	CyGl->id = -1;
	CyGl->hasNewRead1 = 0;
	CyGl->hasNewRead2 = 0;
	CyGl->bufferToUse = 2;
	for (int i = 0; i < WIRED_CYGL_READ_SZ; i++) {
		CyGl->readBuffer1[i] = 0;
		CyGl->readBuffer2[i] = 0;
		CyGl->read[i] = 0;
	}
	CyGl->reads = 0;
	CyGl->errors = 0;
	CyGl->consecutiveErrors = 0;

	const char device[] = "/dev/rfcomm0";
	int tempID;

	if ((tempID = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK)) == -1) {
		fprintf(stderr, "Wireless CyGl ERROR: Couldn't open device.\n");
		return -1;
	}

	// Set BAUD rate
	struct termios options;
	tcgetattr(tempID, &options);
	cfmakeraw(&options);
	cfsetispeed(&options, CYGL_BAUD);
	cfsetospeed(&options, CYGL_BAUD);

	options.c_cflag |= (CLOCAL | CREAD) ;
	options.c_cflag &= ~PARENB ;
	options.c_cflag &= ~CSTOPB ;
	options.c_cflag &= ~CSIZE ;
	options.c_cflag |= CS8 ;
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG) ;
	options.c_oflag &= ~OPOST ;

	tcsetattr(tempID, TCSANOW, &options);

	usleep(10000) ;	// 10mS

	if (write(tempID, "?g", 2) != 2) {
		fprintf(stderr, "Wireless CyGl ERROR: Could request state.\n");
	}

	//wait at most 7 seconds for a response
	for (int i = 0; CyGlDataAvail(tempID) <= 0; i++) {
		sleep(1);
		if (i == 7) {
			fprintf(stderr, "Wireless CyGl ERROR: No response received.\n");
			return -1;
		}
	}

	//ensure its the correct response
	char response[] = "?g e?";
	static int RESPONSE_LENGTH = 8;
	int correctResponse = 1;
	char letter;

	for (int i = 0; read(tempID, &letter, 1) != -1; i++) {

		if (i < 5 && letter != response[i]) {
			//letter mismatch
			correctResponse = 0;
		} else if (i > RESPONSE_LENGTH) {
			//length mismatch
			correctResponse = 0;
		}

		if (correctResponse == 0) {
			printf("%c", letter);
		}
	}

	if (correctResponse == 1) {
//		set it to blocking
		fcntl (tempID, F_SETFL, O_RDWR | O_NOCTTY);
		CyGl->id = tempID;
		CyGl->WiredCyGl = 0;

		//start stream
		if (write(CyGl->id, "S", 1) != 1) {
			fprintf(stderr, "CyberGlove Error: Couldn't request reading.\n");
			return -1;
		}

		return CyGl->id;
	} else {
		return -1;
	}

}

int initializeWiredCyGl(CyGl* CyGl) {

	CyGl->id = -1;
	CyGl->hasNewRead1 = 0;
	CyGl->hasNewRead2 = 0;
	CyGl->bufferToUse = 2;
	for (int i = 0; i < WIRED_CYGL_READ_SZ; i++) {
		CyGl->readBuffer1[i] = 0;
		CyGl->readBuffer2[i] = 0;
		CyGl->read[i] = 0;
	}
	CyGl->reads = 0;
	CyGl->errors = 0;
	CyGl->consecutiveErrors = 0;

	const char device[] = "/dev/ttyUSB0";
	int tempID;

	if ((tempID = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK)) == -1) {
		fprintf(stderr, "Wired CyGl ERROR: Couldn't open device.\n");
		return -1;
	}

	// Set BAUD rate
	struct termios options;
	tcgetattr(tempID, &options);
	cfmakeraw(&options);
	cfsetispeed(&options, CYGL_BAUD);
	cfsetospeed(&options, CYGL_BAUD);
	tcsetattr(tempID, TCSANOW, &options);

	usleep(10000) ;	// 10mS

	//request reading
	if (write(tempID, "G", 1) != 1) {
		fprintf(stderr, "Wired CyGl ERROR: Couldn't request state.\n");
		return -1;
	}

	//wait at most 2 seconds for a response
	for (int i = 0; CyGlDataAvail(tempID) <= 0; i++) {
		sleep(1);
		if (i == 2) {
			fprintf(stderr, "Wired CyGl ERROR: No response received.\n");
			return -1;
		}
	}

	//get the reading
	read(CyGl->id, NULL, WIRED_CYGL_READ_SZ * sizeof(uint8_t));

	//set it to blocking
	fcntl (tempID, F_SETFL, O_RDWR | O_NOCTTY);

	CyGl->id = tempID;
	CyGl->WiredCyGl = 1;

	return CyGl->id;

}

int reconnectCyGl(CyGl* CyGl) {

	if (reconnectWiredCyGl(CyGl) == -1) {
		if (reconnectWirelessCyGl(CyGl) == -1) {
			return -1;
		}
	}

	return CyGl->id;

}

int reconnectWirelessCyGl(CyGl* CyGl) {

	//save info we want to save
	int e = CyGl->errors;
	int r = CyGl->reads;

	//close the device
	closeCyGl(CyGl);

	//restart the device
	initializeWirelessCyGl(CyGl);

	CyGl->errors = e;
	CyGl->reads = r;
	return CyGl->id;

}

int reconnectWiredCyGl(CyGl* CyGl) {

	//save info we want to save
	int e = CyGl->errors;
	int r = CyGl->reads;

	//close the device
	closeCyGl(CyGl);

	//restart the device
	initializeWiredCyGl(CyGl);

	CyGl->errors = e;
	CyGl->reads = r;
	return CyGl->id;

}

int startCyGl(CyGl* CyGl) {

	//try to reconnect 4 times, waiting 4 sec between attempt
	for (int attempt = 0; initializeCyGl(CyGl) == -1 && attempt < 4; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
		sleep(4); //try again after 4 seconds
	}

	return 1;
}

int startWirelessCyGl(CyGl* CyGl) {

	//try to reconnect 4 times, waiting 4 sec between attempt

	for (int attempt = 0; initializeWirelessCyGl(CyGl) == -1 && attempt < 4; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
	}

	return 1;
}

int startWiredCyGl(CyGl* CyGl) {

	//try to reconnect 4 times, waiting 4 sec between attempt

	for (int attempt = 0; initializeWiredCyGl(CyGl) == -1 && attempt < 4; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
	}

	return 1;
}

int restartCyGl(CyGl* CyGl) {

	//try to reconnect 4 times, waiting 4 sec between attempt

	for (int attempt = 0; reconnectCyGl(CyGl) == -1 && attempt < 4; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
	}

	return 1;
}

int restartWirelessCyGl(CyGl* CyGl) {

	//try to reconnect 4 times, waiting 4 sec between attempt

	for (int attempt = 0; reconnectWirelessCyGl(CyGl) == -1 && attempt < 4; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
	}

	return 1;
}

int restartWiredCyGl(CyGl* CyGl) {

	//try to reconnect 4 times, waiting 4 sec between attempt

	for (int attempt = 0; reconnectWiredCyGl(CyGl) == -1 && attempt < 4; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
	}

	return 1;
}

int getCyGlData(CyGl* CyGl, double time) {

	CyGl->reads++;

	fd_set set;
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 24000; //wait 24 ms
	int retval = 1;
	FD_ZERO(&set);
	FD_SET(CyGl->id, &set);

	switch (CyGl->reads % 2) {
	case 0:
		//read into readBuffer1 on even reads
		CyGl->readBuffer1Time = time;

//		//request reading
		if (write(CyGl->id, "G", 1) != 1) {
			fprintf(stderr, "CyberGlove Error: Couldn't request reading.\n");
			return -1;
		}

		//get the reading, ensures its the most up to date
		if (CyGl->WiredCyGl == 1) {

			retval = select(CyGl->id + 1, &set, NULL, NULL, &timeout);
			if (retval <= 0) {
				//either error or timeout was reached
				return -1;
			} else {
				do {
					//read ready
					read(CyGl->id, CyGl->readBuffer1, WIRED_CYGL_READ_SZ * sizeof(uint8_t));
				} while (CyGlDataAvail(CyGl->id) != 0);
			}

		} else {

			retval = select(CyGl->id + 1, &set, NULL, NULL, &timeout);
			if (retval <= 0) {
				//either error or timeout was reached
				return -1;
			} else {
				do {
					//read ready
					read(CyGl->id, CyGl->readBuffer1, WIRELESS_CYGL_READ_SZ * sizeof(uint8_t));
				} while (CyGlDataAvail(CyGl->id) != 0);
			}
		}

		CyGl->hasNewRead1 = 1;
		break;
	case 1:
		//read into readBuffer2 on odd reads
		CyGl->readBuffer2Time = time;

//		//request reading
		if (write(CyGl->id, "G", 1) != 1) {
			fprintf(stderr, "CyberGlove Error: Couldn't request reading.\n");
			return -1;
		}

		if (CyGl->WiredCyGl == 1) {
			//wait 30000 for a first read
			retval = select(CyGl->id + 1, &set, NULL, NULL, &timeout);
			if (retval <= 0) {
				//either error or timeout was reached
				return -1;
			} else {
				do {
					//read ready
					read(CyGl->id, CyGl->readBuffer2, WIRED_CYGL_READ_SZ * sizeof(uint8_t));
				} while (CyGlDataAvail(CyGl->id) != 0);
			}
		} else {
			//wait 30000 for a first read
			retval = select(CyGl->id + 1, &set, NULL, NULL, &timeout);
			if (retval <= 0) {
				//either error or timeout was reached
				return -1;
			} else {
				do {
					//read ready
					read(CyGl->id, CyGl->readBuffer2, WIRELESS_CYGL_READ_SZ * sizeof(uint8_t));
				} while (CyGlDataAvail(CyGl->id) != 0);
			}
		}
		CyGl->hasNewRead2 = 1;
		break;
	}

	return 1;
}

int updateCyGlRead(CyGl* CyGl) {

	switch(CyGl->bufferToUse) {
	case 1:
		CyGl->bufferToUse = 2;
		CyGl->readTime = CyGl->readBuffer1Time;
		if (CyGl->hasNewRead1 == 1) {
			//New data available
			memcpy(&(CyGl->read), &(CyGl->readBuffer1), WIRED_CYGL_READ_SZ * sizeof(uint8_t)); //update the read field
			CyGl->consecutiveErrors = 0;
			CyGl->hasNewRead1 = 0;
			return 1;
		}
		break;
	case 2:
		CyGl->bufferToUse = 1;
		CyGl->readTime = CyGl->readBuffer2Time;
		if (CyGl->hasNewRead2 == 1) {
			//New data available
			memcpy(&(CyGl->read), &(CyGl->readBuffer2), WIRED_CYGL_READ_SZ * sizeof(uint8_t)); //update the read field
			CyGl->consecutiveErrors = 0;
			CyGl->hasNewRead2 = 0;
			return 1;
		}
		break;
	}

	//no new data available, data collection must not have been completed
	CyGl->errors++;
	CyGl->consecutiveErrors++;
	return -1;

}

void closeCyGl(CyGl* CyGl) {

	tcflush(CyGl->id, TCIOFLUSH);
	close(CyGl->id);

	CyGl->id = -1;
	CyGl->hasNewRead1 = 0;
	CyGl->hasNewRead2 = 0;
	CyGl->bufferToUse = 2;
	for (int i = 0; i < WIRED_CYGL_READ_SZ; i++) {
		CyGl->readBuffer1[i] = 0;
		CyGl->readBuffer2[i] = 0;
		CyGl->read[i] = 0;
	}
	CyGl->reads = 0;
	CyGl->errors = 0;
	CyGl->consecutiveErrors = 0;

}

int CyGlDataAvail(const int fd) {
	int result;

	if (ioctl(fd, FIONREAD, &result) == -1) {
		return -1;
	}

	return result;
}
