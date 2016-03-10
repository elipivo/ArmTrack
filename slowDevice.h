/*
 * Name: slowDevice.c
 * Author: Elijah Pivo
 *
 * Slow Device Test interface, mimics behavior of EMG data collection.
 *
 */

#ifndef SLOWDEVICE_H
#define SLOWDEVICE_H

#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#define SLOWDEVICE_READ_SZ 1
#define SLOWDEVICE_READS_PER_CYCLE 1

typedef struct {
	int id;

	int bufferToUse;
	int hasNewRead1;
	int readBuffer1[SLOWDEVICE_READ_SZ * SLOWDEVICE_READS_PER_CYCLE];
	double readBuffer1Time;
	int hasNewRead2;
	int readBuffer2[SLOWDEVICE_READ_SZ * SLOWDEVICE_READS_PER_CYCLE];
	double readBuffer2Time;

	int read[SLOWDEVICE_READ_SZ * SLOWDEVICE_READS_PER_CYCLE];
	double readTime;

	int reads;
	int errors;
	int consecutiveErrors;
} SlowDevice;

/*
 * Sets up a slow device.
 * Ensures its ready to collect data from.
 * Returns 1 if initialization succeeded, -1 if it failed
 */
int initializeSlowDevice(SlowDevice* slowDevice);

/*
 * Reconnects to a slow device. Ensures its ready to read from.
 * Won't reset number of errors or reads done with the device.
 * Returns device id if succeeded, -1 if failed.
 */
int reconnectSlowDevice(SlowDevice* slowDevice);

/*
 * Will attempt to initialize a device 4 times with a 4 second pause
 * between attempts. Returns 1 if connection succeeds, -1 if not.
 */
int startSlowDevice(SlowDevice* slowDevice);

/*
 * Will attempt to reconnect a device 4 times with a 4 second pause
 * between attempts. Returns 1 if connection succeeds, -1 if not.
 */
int restartSlowDevice(SlowDevice* slowDevice);

/*
 * Reads data from a slow device. Alternates between storing
 * data in readBuffer1 and readBuffer2.
 * Returns 1 if the read succeeded, -1 if it failed.
 */
int getSlowDeviceData(SlowDevice* slowDevice, double time);

/*
 * Updates read. Needs to be called
 * before accessing a Test's read information. Alternates
 * between updating from readBuffer1 and readBuffer2.
 * Also updates the error and consecutiveError fields.
 * Returns 1 if update occurred, -1 otherwise.
 */
int updateSlowDeviceRead(SlowDevice* slowDevice);

/*
 * Ends a session with a test device.
 */
void closeSlowDevice(SlowDevice* slowDevice);

#endif
