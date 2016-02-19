/* Myo interface
 *
 * Author: Elijah Pivo
 * Date: 11/7/2015
 */

#ifndef MYO_H
#define MYO_H

#include <termios.h> //set up serial connection
#include <fcntl.h> //open,
#include <sys/ioctl.h> //ioctl
#include <stdio.h> //input and output
#include <pthread.h> //for pthread_exit()
#include <unistd.h> //for sleep, usleep, write, read
#include <stdlib.h> //for exit
#include <stdint.h> //for uint8_t

#define MYO_READ_SZ 8

struct Myo {
	int id;
	int hasNewRead;
	int readBuffer[MYO_READ_SZ];
	int read[MYO_READ_SZ];
};

/*
 * Sets up the connection to a Myo.
 * Ensures its ready to read from.
 * @param a pointer to a Myo struct so it can update the id
 * @return whether or not the initialization succeeded (-1 if it failed)
 */
int initializeMyo(struct Myo* Myo);

/*
 * Reads data from a Myo.
 * Error messages will be reported to stderr.
 * @param takes a pointer to a Myo struct so it can update the read array
 * @return whether or not the read succeeded (-1 if it failed)
 */
int getMyoData(struct Myo* Myo);

/*
 * For use in a pthread. Reads data from a Myo.
 * Error messages will be reported to stderr.
 * @param takes a pointer to a Myo struct so it can update the read array
 * @return status
 */
void* pGetMyoData(void *threadarg);

/*
 * Updates the most recent read.
 */
void updateMyoRead(struct Myo* Myo);

/*
 * Will end a session with a Myo device.
 * @param takes a pointer to a Myo struct
 */
void closeMyo(struct Myo* Myo);

#endif
