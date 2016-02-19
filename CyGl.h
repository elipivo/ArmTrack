/*
 * Name: CyGl.h
 * Author: Elijah Pivo
 *
 * Wireless CyberGlove II interface
 */

#ifndef CYGL_H
#define CYGL_H

#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/select.h>


#define WIRED_CYGL_READ_SZ 24
#define WIRELESS_CYGL_READ_SZ 20
#define CYGL_BAUD B115200

typedef struct {
	int id;

	int bufferToUse;
	int hasNewRead1;
	uint8_t readBuffer1[WIRED_CYGL_READ_SZ];
	double readBuffer1Time;
	int hasNewRead2;
	uint8_t readBuffer2[WIRED_CYGL_READ_SZ];
	double readBuffer2Time;

	uint8_t read[WIRED_CYGL_READ_SZ];
	double readTime;

	int WiredCyGl; //1 if wired, 0 if wireless

	int reads;
	int errors;
	int consecutiveErrors;
} CyGl;

/*
 * Sets up the connection to any CyberGlove II.
 * Ensures its ready to collect data from.
 * Returns device id if succeeded, -1 if failed.
 */
int initializeCyGl(CyGl* CyGl);

/*
 * Sets up the connection to a Wireless CyberGlove II.
 * Ensures its ready to collect data from.
 * Returns device id if succeeded, -1 if failed.
 */
int initializeWirelessCyGl(CyGl* CyGl);

/*
 * Sets up the connection to a Wired CyberGlove II.
 * Ensures its ready to collect data from.
 * Returns device id if succeeded, -1 if failed.
 */
int initializeWiredCyGl(CyGl* CyGl);

/*
 * Reconnects to any CyberGlove II. Ensures its ready to read from.
 * Won't reset the number of errors or reads done with the device.
 * Returns device id if succeeded, -1 if failed.
 */
int reconnectCyGl(CyGl* CyGl);

/*
 * Reconnects to a Wireless CyberGlove II. Ensures its ready to read from.
 * Won't reset the number of errors or reads done with the device.
 * Returns device id if succeeded, -1 if failed.
 */
int reconnectWirelessCyGl(CyGl* CyGl);

/*
 * Reconnects to a Wired CyberGlove II. Ensures its ready to read from.
 * Won't reset the number of errors or reads done with the device.
 * Returns device id if succeeded, -1 if failed.
 */
int reconnectWiredCyGl(CyGl* CyGl);

/*
 * Will attempt to initialize a Wired or Wireless CyberGlove II 4 times with a 4 second pause
 * between attempts. Returns 1 if connection succeeds, -1 if not.
 */
int startCyGl(CyGl* CyGl);

/*
 * Will attempt to initialize a Wireless CyberGlove II 4 times with a 4 second pause
 * between attempts. Returns 1 if connection succeeds, -1 if not.
 */
int startWirelessCyGl(CyGl* CyGl);

/*
 * Will attempt to initialize a Wired CyberGlove II 4 times with a 4 second pause
 * between attempts. Returns 1 if connection succeeds, -1 if not.
 */
int startWiredCyGl(CyGl* CyGl);

/*
 * Will attempt to reconnect to a Wired or Wireless CyberGlove II
 * 4 times with a 4 second pause between attempts. Returns 1
 * if connection succeeds, -1 if not.
 */
int restartCyGl(CyGl* CyGl);

/*
 * Will attempt to reconnect to a Wireless CyberGlove II 4 times with
 * a 4 second pause between attempts. Returns 1 if connection
 * succeeds, -1 if not.
 */
int restartWirelessCyGl(CyGl* CyGl);

/*
 * Will attempt to reconnect to a Wired CyberGlove II 4 times with
 * a 4 second pause between attempts. Returns 1 if connection
 * succeeds, -1 if not.
 */
int restartWiredCyGl(CyGl* CyGl);

/*
 * Reads data from a CyberGlove II into the readBuffer.
 * Returns 1 if the read succeeded, -1 if it failed.
 */
int getCyGlData(CyGl* CyGl, double time);

/*
 * Updates the most recent CyGl read. Alternates between
 * updating read from readBuffer1 and readBuffer2.
 * Needs to be called before accessing a CyGl's read information.
 * Also updates the error and consecutiveError fields.
 * Returns 1 if update occurred, -1 otherwise.
 */
int updateCyGlRead(CyGl* CyGl);

/*
 * Ends a session with a CyberGlove device.
 */
void closeCyGl(CyGl* CyGl);

/*
 * Returns the number of bytes of data available to be read
 * in the serial port.
 */
int CyGlDataAvail (const int fd);

/*
 * Get a single character from the serial device.
 */
int serialGetchar (const int fd);


#endif
