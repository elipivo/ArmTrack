/*
 * Name: EMG.h
 * Author: Elijah Pivo
 *
 * EMG MCC-DAQ USB1408FS interface
 */

#ifndef EMG_H
#define EMG_H

#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>

//mcc-daq driver includes
#include "/home/pi/mcc-libusb/pmd.h"
#include "/home/pi/mcc-libusb/usb-1408FS.h"

#define EMG_READS_PER_CYCLE 25 //number of reads we need in 25ms
#define EMG_READ_SZ 8    //number of channels we read from
#define CYCLE_TIME .025 //in seconds

typedef struct  {
	int id;
	libusb_device_handle *udev;

	int bufferToUse;
	int hasNewRead1;
	signed short readBuffer1[EMG_READ_SZ * EMG_READS_PER_CYCLE];
	double readBuffer1Time;
	int hasNewRead2;
	signed short readBuffer2[EMG_READ_SZ * EMG_READS_PER_CYCLE];
	double readBuffer2Time;

	float read[EMG_READ_SZ * EMG_READS_PER_CYCLE];
	double readTime;

	int reads;
	int errors;
	int consecutiveErrors;
} EMG;

/*
 * Sets up an EMG.
 * Ensures its ready to collect data from.
 * Returns 1 if initialization succeeded, -1 if it failed
 */
int initializeEMG(EMG* EMG);

/*
 * Reconnects to an EMG. Ensures its ready to read from.
 * Won't reset number of errors or reads done with the device.
 * Returns device id if succeeded, -1 if failed.
 */
int reconnectEMG(EMG* EMG);

/*
 * Will attempt to initialize an EMG 4 times with a 4 second pause
 * between attempts. Returns 1 if connection succeeds, -1 if not.
 */
int startEMG(EMG* EMG);

/*
 * Will attempt to reconnect a device 4 times with a 4 second pause
 * between attempts. Returns 1 if connection succeeds, -1 if not.
 */
int restartEMG(EMG* EMG);

/*
 * Reads data from an EMG. Alternates between
 * putting data in readBuffer1 and readBuffer2.
 * Returns 1 if the read succeeded, -1 if it failed.
 */
int getEMGData(EMG* EMG, double time);

/*
 * Updates the most recent read. Alternates between
 * updating read from readBuffer1 and readBuffer2.
 * Needs to be called before accessing an EMG read
 * information. Also updates the error
 * and consecutiveError fields. Returns 1 if update
 * occurred, -1 otherwise.
 */
int updateEMGRead(EMG* EMG);

/*
 * Ends a session with an EMG.
 */
void closeEMG(EMG* EMG);

#endif
