/*
 * Name: IMU.h
 * Author: Elijah Pivo
 *
 * IMU interface
 *
 */

#ifndef IMU_H
#define IMU_H

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

typedef struct {
	int id;

	int bufferToUse;
	int hasNewRead1;
	float readBuffer1[IMU_READ_SZ];
	double readBuffer1Time;
	int hasNewRead2;
	float readBuffer2[IMU_READ_SZ];
	double readBuffer2Time;

	float read[IMU_READ_SZ];
	double readTime;

	int reads;
	int errors;
	int consecutiveErrors;
} IMU;

/*
 * Sets up an IMU chain.
 * Ensures its ready to collect data from.
 * Returns 1 if initialization succeeded, -1 if it failed
 */
int initializeIMU(IMU* IMU);

/*
 * Reconnects to an IMU chain. Ensures its ready to read from.
 * Won't reset number of errors or reads done with the device.
 * Returns device id if succeeded, -1 if failed.
 */
int reconnectIMU(IMU* IMU);

/*
 * Will attempt to initialize an IMU 4 times with a 4 second pause
 * between attempts. Returns 1 if connection succeeds, -1 if not.
 */
int startIMU(IMU* IMU);

/*
 * Will attempt to reconnect an IMU 4 times with a 4 second pause
 * between attempts. Returns 1 if connection succeeds, -1 if not.
 */
int restartIMU(IMU* IMU);

/*
 * Reads data from an IMU chain.
 * Returns 1 if the read succeeded, -1 if it failed.
 */
int getIMUData(IMU* IMU, double time);

/*
 * Updates the most recent IMU read. Alternates between
 * updating read from readBuffer1 and readBuffer2.
 * Needs to be called before accessing an IMU's read
 * information. Also updates the error
 * and consecutiveError fields. Returns 1 if update
 * occurred, -1 otherwise.
 */
int updateIMURead(IMU* IMU);

/*
 * Ends a session with an IMU chain.
 */
void closeIMU(IMU* IMU);

/*
 * Returns the number of bytes of data available to be read in
 * the serial port.
 */
int IMUDataAvail (const int fd);

#endif
