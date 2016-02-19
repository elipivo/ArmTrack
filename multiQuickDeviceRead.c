/*
 * Name: multiQuickDeviceRead.c
 * Author: Elijah Pivo
 *
 * Description:
 * 	Tests timing architecture for multiple devices whose data collection takes less than 25ms. (IMU, Force, CyGl)
 * 	This test uses a parallel background thread to print data.
 *
 * Usage:
 * 	Compile with: gcc -o multiQuickDeviceRead multiQuickDeviceRead.c quickDevice.c -std=gnu99 -Wall -Wextra -pthread
 * 	Start with ./quickDeviceRead, end program with ctrl-d
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

#include "quickDevice.h"

//global data structure
typedef struct {
	QuickDevice quickDevice1;
	QuickDevice quickDevice2;
	QuickDevice quickDevice3;

	double time;
	int errors;
	int reads;

	int device1; //controls device threads
	int device2;
	int device3;
	int print; //controls print thread
} Data;

void* printData();
void getData();
void* getData1();
void* getData2();
void* getData3();
void endSession();

Data data;

pthread_t printThread;
pthread_mutex_t printLock;
pthread_cond_t printSignal;

pthread_t device1Thread;
pthread_mutex_t device1Lock;
pthread_cond_t device1Signal;

pthread_t device2Thread;
pthread_mutex_t device2Lock;
pthread_cond_t device2Signal;

pthread_t device3Thread;
pthread_mutex_t device3Lock;
pthread_cond_t device3Signal;

int main(void) {

	fprintf(stderr, "Reading Test Devices\n");

	//make stdin non blocking
	int flags = fcntl(fileno(stdin), F_GETFL, 0);
	flags |= O_NONBLOCK;
	flags = fcntl(fileno(stdin), F_SETFL, flags);
	char userInput;

//// should only use this with programs controlled by a switch I believe
//	//make data collection thread a time critical thread
//	struct sched_param param;
//	param.sched_priority = sched_get_priority_max(SCHED_RR);
//	if (sched_setscheduler(0, SCHED_RR, &param) != 0) {
//		fprintf(stderr, "ERROR: Data Collection Thread Priority not set.\n");
//		fprintf(stderr, "*Remember to run as root.*\n");
//		exit(1);
//	}
//
//	//lock process in memory
//	if (mlockall(MCL_FUTURE) != 0) {
//		fprintf(stderr, "ERROR: Couldn't lock process in memory.\n");
//		exit(1);
//	}

	data.print = 0;
	data.device1 = 0;
	data.device2 = 0;
	data.device3 = 0;

	if (startQuickDevice(&data.quickDevice1) != 1) {
		printf("Test Error: Couldn't start quickDevice1.\n");
		exit(1);
	} else {

		pthread_mutex_init(&device1Lock, NULL);
		pthread_cond_init(&device1Signal, NULL);

		if (pthread_create(&device1Thread, NULL, getData1, NULL) != 0) {
			fprintf(stderr, "Couldn't start device1 data collection thread...\n");
			exit(1);
		}
	}
	if (startQuickDevice(&data.quickDevice2) != 1) {
		printf("Test Error: Couldn't start quickDevice2.\n");
		exit(1);
	} else {

		pthread_mutex_init(&device2Lock, NULL);
		pthread_cond_init(&device2Signal, NULL);

		if (pthread_create(&device2Thread, NULL, getData2, NULL) != 0) {
			fprintf(stderr, "Couldn't start device2 data collection thread...\n");
			exit(1);
		}
	}
	if (startQuickDevice(&data.quickDevice3) != 1) {
		printf("Test Error: Couldn't start quickDevice3.\n");
		exit(1);
	} else {

		pthread_mutex_init(&device3Lock, NULL);
		pthread_cond_init(&device3Signal, NULL);

		if (pthread_create(&device3Thread, NULL, getData3, NULL) != 0) {
			fprintf(stderr, "Couldn't start device3 data collection thread...\n");
			exit(1);
		}
	}

	pthread_mutex_init(&printLock, NULL);
	pthread_cond_init(&printSignal, NULL);
	if (pthread_create(&printThread, NULL, printData, NULL) != 0) {
		fprintf(stderr, "Couldn't create print thread...\n");
		exit(1);
	}

	usleep(30000);

	struct timeval last;
	struct timeval curr;
	struct timeval temp;

	gettimeofday(&curr, NULL); //update current time

	while(read(fileno(stdin), &userInput, 1) < 0) {

		last.tv_sec = curr.tv_sec; last.tv_usec = curr.tv_usec; //update last time
		gettimeofday(&curr, NULL); //update current time
		data.time += (curr.tv_sec - last.tv_sec) + (curr.tv_usec - last.tv_usec) * .000001; //increment by difference between last and current time

		getData();

		/*
		 * getData() never takes longer than 25ms.
		 * Can adjust this max time value to be faster after measuring
		 * average sensor response times.
		 */

		while (data.print != 2) {}; //wait for print thread to be ready
		pthread_mutex_lock(&printLock);
		data.print = 1;
		pthread_cond_signal(&printSignal);
		pthread_mutex_unlock(&printLock);

		if (data.quickDevice1.consecutiveErrors > 20) {
			//.5 sec of missed data
			//big error happening, try to reconnect to the IMU
			fprintf(stderr, "ERROR: Too many consecutive missed reads.\n");
			fprintf(stderr, "ERROR: Trying to reconnect to quick device.\n");
			if (restartQuickDevice(&data.quickDevice1) != 1) {
				//couldn't reconnect, just end the program here

				fprintf(stderr, "ERROR: Couldn't reconnect to quick device.\n");
				fprintf(stderr, "ERROR: Ending recording session.\n");

				endSession();
				return 0;
			}
			fprintf(stderr, "ERROR: Successfully reconnected to quick device.\n");
			fprintf(stderr, "ERROR: Continuing data recording.\n");
		}
		if (data.quickDevice2.consecutiveErrors > 20) {
			//.5 sec of missed data
			//big error happening, try to reconnect to the IMU
			fprintf(stderr, "ERROR: Too many consecutive missed reads.\n");
			fprintf(stderr, "ERROR: Trying to reconnect to quick device.\n");
			if (restartQuickDevice(&data.quickDevice2) != 1) {
				//couldn't reconnect, just end the program here
				fprintf(stderr, "ERROR: Couldn't reconnect to quick device.\n");
				fprintf(stderr, "ERROR: Ending recording session.\n");
				endSession();
				return 0;
			}
			fprintf(stderr, "ERROR: Successfully reconnected to quick device.\n");
			fprintf(stderr, "ERROR: Continuing data recording.\n");
		}
		if (data.quickDevice3.consecutiveErrors > 20) {
			//.5 sec of missed data
			//big error happening, try to reconnect to the IMU
			fprintf(stderr, "ERROR: Too many consecutive missed reads.\n");
			fprintf(stderr, "ERROR: Trying to reconnect to quick device.\n");
			if (restartQuickDevice(&data.quickDevice3) != 1) {
				//couldn't reconnect, just end the program here
				fprintf(stderr, "ERROR: Couldn't reconnect to quick device.\n");
				fprintf(stderr, "ERROR: Ending recording session.\n");
				endSession();
				return 0;
			}
			fprintf(stderr, "ERROR: Successfully reconnected to quick device.\n");
			fprintf(stderr, "ERROR: Continuing data recording.\n");
		}

		//simple timing method, good enough for now, could improve later
		do {
			gettimeofday(&temp, NULL);
		} while ( (temp.tv_sec - curr.tv_sec) + (temp.tv_usec - curr.tv_usec) * .000001 < .024993);
	}
	endSession();
	return 1;
}

void getData() {

	if (data.quickDevice1.id > 0) {
		pthread_mutex_lock(&device1Lock);
		data.device1 = 1;
		pthread_cond_signal(&device1Signal);
		pthread_mutex_unlock(&device1Lock);
	}
	if (data.quickDevice2.id > 0) {
		pthread_mutex_lock(&device2Lock);
		data.device2 = 1;
		pthread_cond_signal(&device2Signal);
		pthread_mutex_unlock(&device2Lock);
	}
	if (data.quickDevice3.id > 0) {
		pthread_mutex_lock(&device3Lock);
		data.device3 = 1;
		pthread_cond_signal(&device3Signal);
		pthread_mutex_unlock(&device3Lock);
	}

	//wait for data collection to be ready for another cycle
	if (data.quickDevice1.id > 0) {
		while (data.device1 != 2) {}
	}
	if (data.quickDevice2.id > 0) {
		while (data.device2 != 2) {}
	}
	if (data.quickDevice3.id > 0) {
		while (data.device3 != 2) {}
	}

}

void* getData1() {

	while (1 == 1) {

		pthread_mutex_lock(&device1Lock);
		data.device1 = 2; //signals ready to accept a collection request
		pthread_cond_wait(&device1Signal, &device1Lock);
		data.device1 = 0;
		getQuickDeviceData(&data.quickDevice1, data.time);
		pthread_mutex_unlock(&device1Lock);
	}
}

void* getData2() {
	while (1 == 1) {
		pthread_mutex_lock(&device2Lock);
		data.device2 = 2; //signals ready to accept a collection request
		pthread_cond_wait(&device2Signal, &device2Lock);

		getQuickDeviceData(&data.quickDevice2, data.time);
		data.device2 = 0;
		pthread_mutex_unlock(&device2Lock);
	}
}

void* getData3() {
	while (1 == 1) {
		pthread_mutex_lock(&device3Lock);
		data.device3 = 2; //signals ready to accept a collection request
		pthread_cond_wait(&device3Signal, &device3Lock);

		getQuickDeviceData(&data.quickDevice3, data.time);
		data.device3 = 0;
		pthread_mutex_unlock(&device3Lock);
	}
}

void* printData() {

	while (1 == 1) {

		pthread_mutex_lock(&printLock);
		data.print = 2; //signals ready to accept a print request
		pthread_cond_wait(&printSignal, &printLock);
		data.print = 0;

		/*
		 * Format:
		 * 		Time1 -Channels of TestDevice Data-
		 * 		Time2 -Channels of TestDevice Data-
		 * 		...
		 */

		data.reads++;
		int error = 0;

		if (updateQuickDeviceRead(&data.quickDevice1) == -1) {
			//		printf("*");
			error = 1;
		}
		printf("%f\t", data.quickDevice1.readTime);
		for (int i = 0; i < QUICKDEVICE_READ_SZ; i++) {
			printf("%i\t", data.quickDevice1.read[i]);
		}

		if (updateQuickDeviceRead(&data.quickDevice2) == -1) {
			//		printf("*");
			error = 1;
		}
		for (int i = 0; i < QUICKDEVICE_READ_SZ; i++) {
			printf("%i\t", data.quickDevice2.read[i]);
		}

		if (updateQuickDeviceRead(&data.quickDevice3) == -1) {
			//		printf("*");
			error = 1;
		}
		for (int i = 0; i < QUICKDEVICE_READ_SZ; i++) {
			printf("%i\t", data.quickDevice3.read[i]);
		}
		printf("\n");

		data.errors += error;

		pthread_mutex_unlock(&printLock);
	}
	pthread_exit(NULL);
}

void endSession() {

	pthread_cancel(printThread);
	pthread_cancel(device1Thread);
	pthread_cancel(device2Thread);
	pthread_cancel(device3Thread);

	pthread_mutex_destroy(&printLock);
	pthread_mutex_destroy(&IMULock);
	pthread_mutex_destroy(&CyGlLock);
	pthread_mutex_destroy(&ForceLock);
	pthread_cond_destroy(&printSignal);
	pthread_cond_destroy(&IMUSignal);
	pthread_cond_destroy(&CyGlSignal);
	pthread_cond_destroy(&ForceSignal);

	double percentMissed = (data.errors / (double) data.reads) * 100;

	fprintf(stderr, "Elapsed Time (sec): %05.3f\tPercent Missed: %5.3f%%\n",
			data.time, percentMissed);

	closeQuickDevice(&data.quickDevice1);
	closeQuickDevice(&data.quickDevice2);
	closeQuickDevice(&data.quickDevice3);

	fprintf(stderr, "Session Ended\n\n");
}
