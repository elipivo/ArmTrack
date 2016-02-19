/*
 * Name: slowDeviceRead.c
 * Author: Elijah Pivo
 *
 * Description:
 * 	Tests timing architectures whose data collection takes precisely 25ms. (EMG Band)
 * 	This test uses a parallel background thread to print data.
 *
 * Usage:
 * 	Compile with: gcc -o slowDeviceRead slowDeviceRead.c slowDevice.c -std=gnu99 -Wall -Wextra -pthread
 * 	Start with ./slowDeviceRead, end program with ctrl-d
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
#include <sched.h>
#include <sys/mman.h>

#include "slowDevice.h"

//global data structure
typedef struct {
	SlowDevice slowDevice;
	double time;

	int print; //ends the printRead cycle when 0
} Data;

void* printData();
int startSlowDevice();
int restartSlowDevice();
void endSession();

Data data;

pthread_t printThreadID;

int main(void) {

	fprintf(stderr, "Reading Test Device\n");

	//make stdin non blocking
	int flags = fcntl(fileno(stdin), F_GETFL, 0);
	flags |= O_NONBLOCK;
	flags = fcntl(fileno(stdin), F_SETFL, flags);
	char userInput;

// should only use this with programs controlled by a switch I believe
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

	if (startSlowDevice(&data.slowDevice) != 1) {
		printf("Test Error: Couldn't start slowDevice.\n");
		exit(1);
	}

	//thread to print data in background
	pthread_create(&printThreadID, NULL, printData, NULL);

	struct timeval last;
	struct timeval curr;
	gettimeofday(&curr, NULL); //update current time

	while(read(fileno(stdin), &userInput, 1) < 0) {

		last.tv_sec = curr.tv_sec; last.tv_usec = curr.tv_usec; //update last time
		gettimeofday(&curr, NULL); //update current time
		data.time += (curr.tv_sec - last.tv_sec) + (curr.tv_usec - last.tv_usec) * .000001; //increment by difference between last and current time

		//collect data (should take precisely 25ms)
		getSlowDeviceData(&data.slowDevice);
		data.slowDevice.readTime = data.time;
		data.print = 1; //signal print thread to print results

		if (data.slowDevice.consecutiveErrors > 20) {
			//.5 sec of missed data
			//big error happening, try to reconnect to the Test device

			fprintf(stderr, "ERROR: Too many consecutive missed reads.\n");
			fprintf(stderr, "ERROR: Trying to reconnect to slow device.\n");

			sleep(2); //pretending restart takes time

			if (restartSlowDevice(&data.slowDevice) != 1) {
				//couldn't reconnect, just end the program here

				fprintf(stderr, "ERROR: Couldn't reconnect to slow device.\n");
				fprintf(stderr, "ERROR: Ending recording session.\n");

				endSession();
				return 0;
			}

			fprintf(stderr, "ERROR: Successfully reconnected to slow device.\n");
			fprintf(stderr, "ERROR: Continuing data recording.\n");
		}

//		if (data.time > 10) {
//			//here so I can try process scheduling stuff and it won't lock me out
//			endSession();
//			return 1;
//		}
	}

	endSession();
	return 1;
}

void endSession() {

	usleep(25000);
	data.print = -1;

	double percentMissed = (data.slowDevice.errors / (double) data.slowDevice.reads) * 100;

	fprintf(stderr, "Elapsed Time (sec): %05.3f\tPercent Missed: %5.3f%%\n",
			data.time, percentMissed);

	closeSlowDevice(&data.slowDevice);

	fprintf(stderr, "Session Ended\n\n");
}

void* printData() {

//	//make data collection thread a time critical thread
//	struct sched_param param;
//	param.sched_priority = sched_get_priority_max(SCHED_RR);
//	if (sched_setscheduler(0, SCHED_RR, &param) != 0) {
//		fprintf(stderr, "ERROR: Data Collection Thread Priority not set.\n");
//		fprintf(stderr, "*Remember to run as root.*\n");
//	}
//
//	//lock process in memory
//	if (mlockall(MCL_FUTURE) != 0) {
//		fprintf(stderr, "ERROR: Couldn't lock process in memory.\n");
//		exit(1);
//	}

	/*
	 * Format:
	 * 		Time1 -Channels of pTestDevice Data-
	 * 		Time2 -Channels of pTestDevice Data-
	 * 		...
	 */

	while (data.print != -1) {

		usleep(1);

		if (data.print == 1) {
			data.print = 0;

			usleep(1);

			if (updateSlowDeviceRead(&data.slowDevice) == -1) {
				printf("*");

//				fprintf(stderr, "Failed to update readBuffer1\n");
//				exit(1);
			}
			printf("%f\t", data.slowDevice.readTime);
			for (int i = 0; i < SLOWDEVICE_READ_SZ; i++) {
				printf("%i\t", data.slowDevice.read[i]);
			}
			printf("\n");
		}
	}
	pthread_exit(NULL);
}

int startSlowDevice() {

	//try to reconnect 4 times, waiting 4 sec between attempt

	for (int attempt = 0; initializeSlowDevice(&data.slowDevice) == -1 && attempt < 4; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
		sleep(4); //try again after 4 seconds
	}

	return 1;
}

int restartSlowDevice() {

	//try to reconnect 4 times, waiting 4 sec between attempt

	for (int attempt = 0; reconnectSlowDevice(&data.slowDevice) == -1 && attempt < 4; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
		sleep(4); //try again after 4 seconds
	}

	return 1;
}

