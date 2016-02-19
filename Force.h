/*
 * Name: Force.h
 * Author: Elijah Pivo
 *
 * Force sensors interface
 */

#ifndef FORCE_H
#define FORCE_H

#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <pthread.h>
#include <linux/i2c-dev.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>

#define FORCE_READ_SZ 4

typedef struct {
	int id;

	int bufferToUse;
	int hasNewRead1;
	float readBuffer1[FORCE_READ_SZ];
	double readBuffer1Time;
	int hasNewRead2;
	float readBuffer2[FORCE_READ_SZ];
	double readBuffer2Time;

	float read[FORCE_READ_SZ];
	double readTime;

	int reads;
	int errors;
	int consecutiveErrors;
} Force;

/*
 * Sets up Force sensors.
 * Ensures its ready to collect data from.
 * Returns 1 if initialization succeeded, -1 if it failed
 */
int initializeForce(Force* Force);

/*
 * Reconnects to Force sensors. Ensures they're ready to read from.
 * Won't reset number of errors or reads done with the device.
 * Returns device id if succeeded, -1 if failed.
 */
int reconnectForce(Force* Force);

/*
 * Will attempt to initialize Force sensors 4 times with a 4 second pause
 * between attempts. Returns 1 if connection succeeds, -1 if not.
 */
int startForce(Force* Force);

/*
 * Will attempt to reconnect to Force sensors 4 times with a 4 second pause
 * between attempts. Returns 1 if connection succeeds, -1 if not.
 */
int restartForce(Force* Force);

/*
 * Reads data from Force sensors.
 * Returns 1 if the read succeeded, -1 if it failed.
 */
int getForceData(Force* Force, double time);

/*
 * Updates the most recent Force read. Alternates between
 * updating read from readBuffer1 and readBuffer2.
 * Needs to be called before accessing a Force Sensor's read
 * information. Also updates the error and consecutiveError
 * fields. Returns 1 if update occurred, -1 otherwise.
 */
int updateForceRead(Force* Force);

/*
 * Ends a session with force sensors.
 */
void closeForce(Force* Force);

#endif

