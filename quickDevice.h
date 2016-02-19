/*
 * Name: quickDevice.h
 * Author: Elijah Pivo
 *
 * Test interface for quick devices. (IMU, Force, CyberGlove)
 */

#ifndef QUICKDEVICE_H
#define QUICKDEVICE_H

#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#define QUICKDEVICE_READ_SZ 1

typedef struct {
	int id;

	int bufferToUse;
	int hasNewRead1;
	int readBuffer1[QUICKDEVICE_READ_SZ];
	double readBuffer1Time;
	int hasNewRead2;
	int readBuffer2[QUICKDEVICE_READ_SZ];
	double readBuffer2Time;

	int read[QUICKDEVICE_READ_SZ];
	double readTime;

	int reads;
	int errors;
	int consecutiveErrors;
} QuickDevice;

/*
 * Sets up a quick device.
 * Ensures its ready to collect data from.
 * Returns 1 if initialization succeeded, -1 if it failed
 */
int initializeQuickDevice(QuickDevice* quickDevice);

/*
 * Reconnects to a quick device. Ensures its ready to read from.
 * Won't reset number of errors or reads done with the device.
 * Returns device id if succeeded, -1 if failed.
 */
int reconnectQuickDevice(QuickDevice* quickDevice);

/*
 * Will attempt to initialize a device 4 times with a 4 second pause
 * between attempts. Returns 1 if connection succeeds, -1 if not.
 */
int startQuickDevice(QuickDevice* quickDevice);

/*
 * Will attempt to reconnect a device 4 times with a 4 second pause
 * between attempts. Returns 1 if connection succeeds, -1 if not.
 */
int restartQuickDevice(QuickDevice* quickDevice);

/*
 * Reads data from a quick device. Alternates between
 * putting data in readBuffer1 and readBuffer2.
 * Returns 1 if the read succeeded, -1 if it failed.
 */
int getQuickDeviceData(QuickDevice* quickDevice, double time);

/*
 * Updates the most recent read. Alternates between
 * updating read from readBuffer1 and readBuffer2.
 * Needs to be called before accessing a quick
 * device's read information. Also updates the error
 * and consecutiveError fields. Returns 1 if update
 * occurred, -1 otherwise.
 */
int updateQuickDeviceRead(QuickDevice* quickDevice);

/*
 * Ends a session with a test device.
 */
void closeQuickDevice(QuickDevice* quickDevice);

#endif
