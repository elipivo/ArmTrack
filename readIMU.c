/*
 * Name: readIMU.c
 * Author: Elijah Pivo
 *
 * Description:
 * 	Reads data from the IMU and
 * 	prints it to the screen.
 *
 * Usage:
 * 	Compile with: gcc -o readIMU readIMU.c IMU.c -std=gnu99 -Wall -Wextra -pthread
 * 	Start with ./readIMU, end program with ctrl-d
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <sys/mman.h>

#include "IMU.h"

typedef struct {
	IMU IMU;
	double time;

	//controls printThread
	int print;
} Data;

void* printData();
void setPriority();
void endSession();

Data data;

pthread_t printThread;
pthread_mutex_t printLock;
pthread_cond_t printSignal;

int main(void) {

	fprintf(stderr, "Reading IMU\n");

	//make stdin non blocking
	int flags = fcntl(fileno(stdin), F_GETFL, 0);
	flags |= O_NONBLOCK;
	flags = fcntl(fileno(stdin), F_SETFL, flags);
	char userInput;

//	setPriority();

	if (startIMU(&data.IMU) != 1) {
		fprintf(stderr, "readIMU Error: Couldn't start IMU.\n");
		exit(1);
	}

	pthread_mutex_init(&printLock, NULL);
	pthread_cond_init(&printSignal, NULL);
	data.print = 0;
	if (pthread_create(&printThread, NULL, printData, NULL) != 0) {
		fprintf(stderr, "Couldn't create print thread...\n");
		exit(1);
	}

	struct timeval last;
	struct timeval curr;
	struct timeval temp;

	gettimeofday(&curr, NULL); //update current time

	while (read(fileno(stdin), &userInput, 1) < 0) {

		last.tv_sec = curr.tv_sec; last.tv_usec = curr.tv_usec; //update last time
		gettimeofday(&curr, NULL); //update current time
		data.time += (curr.tv_sec - last.tv_sec) + (curr.tv_usec - last.tv_usec) * .000001; //increment by difference between last and current time

		getIMUData(&data.IMU, data.time);

		while (data.print != 2) {}; //wait for print thread to be ready
		pthread_mutex_lock(&printLock);
		data.print = 1;
		pthread_cond_signal(&printSignal);
		pthread_mutex_unlock(&printLock);

		if (data.IMU.consecutiveErrors > 20) {
			//.5 sec of missed data
			//big error happening, try to reconnect to the IMU

			fprintf(stderr, "ERROR: Too many consecutive missed reads.\n");
			fprintf(stderr, "ERROR: Trying to reconnect to IMU.\n");

			if (restartIMU(&data.IMU) != 1) {
				//couldn't reconnect, just end the program here

				fprintf(stderr, "ERROR: Couldn't reconnect to IMU.\n");
				fprintf(stderr, "ERROR: Ending recording session.\n");

				endSession();
				return 0;
			}

			fprintf(stderr, "ERROR: Successfully reconnected to IMU.\n");
			fprintf(stderr, "ERROR: Continuing data recording.\n");

		}

		do {
			gettimeofday(&temp, NULL);
		} while ( (temp.tv_sec - curr.tv_sec) + (temp.tv_usec - curr.tv_usec) * .000001 < .024993);
	}
	endSession();
	return 1;
}

void setPriority() {
	//should be careful with this!
	//make data collection thread a time critical thread
	struct sched_param param;
	param.sched_priority = sched_get_priority_max(SCHED_RR);
	if (sched_setscheduler(0, SCHED_RR, &param) != 0) {
		fprintf(stderr, "ERROR: Data Collection Thread Priority not set.\n");
		fprintf(stderr, "*Remember to run as root.*\n");
		exit(1);
	}

	//lock process in memory
	if (mlockall(MCL_FUTURE) != 0) {
		fprintf(stderr, "ERROR: Couldn't lock process in memory.\n");
		exit(1);
	}
}

void endSession() {

	while (data.print != 2) {}; //wait for print thread to be done

	pthread_cancel(printThread);
	pthread_mutex_destroy(&printLock);
	pthread_cond_destroy(&printSignal);

	double percentMissed = (data.IMU.errors / (double) data.IMU.reads) * 100;

	fprintf(stderr, "Elapsed Time (sec): %05.3f\tPercent Missed: %5.3f%%\n",
			data.time, percentMissed);

	closeIMU(&data.IMU);

	fprintf(stderr, "Session Ended\n\n");
}

void* printData() {

	while (1 == 1) {

		pthread_mutex_lock(&printLock);
		data.print = 2; //signals ready to accept a print request
		pthread_cond_wait(&printSignal, &printLock);
		data.print = 0;

		/*
		 * Format:
		 * 		Time1 -Channels of IMU Data-
		 * 		Time2 -Channels of IMU Data-
		 * 		...
		 */

		if (updateIMURead(&data.IMU) == -1) {
			printf("*");
		}

		printf("%f\t", data.IMU.readTime);
		for (int i = 0; i < IMU_READ_SZ; i++) {
			printf("%f\t", data.IMU.read[i]);
		}
		printf("\n");
		pthread_mutex_unlock(&printLock);
	}
	pthread_exit(NULL);
}
