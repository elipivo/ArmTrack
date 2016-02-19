/*
 * Name: Test2.c
 * Author: Elijah Pivo
 *
 * Description:
 * 	Tests timing architectures whose data collection takes precisely 25ms. (EMG Band)
 *
 * Usage:
 * 	Compile with: gcc -o Test2 Test2.c TestDevice.c -std=gnu99 -Wall -Wextra -pthread
 * 	Start with ./Test2, end program with ctrl-d
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

#include "sTestDevice.h"

//create a global data structure
typedef struct {
	TestDevice TestDevice;
	double time;
} Data;

void printData(Data* data);
int startTestDevice(TestDevice* TestDevice);
int restartTestDevice(TestDevice* TestDevice);
void endSession(Data* data);

int main(void) {

	fprintf(stderr, "Reading Test Device\n");

	//make stdin non blocking
	int flags = fcntl(fileno(stdin), F_GETFL, 0);
	flags |= O_NONBLOCK;
	flags = fcntl(fileno(stdin), F_SETFL, flags);
	char userInput;

	Data data;

	if (startTestDevice(&data.TestDevice) != 1) {
		printf("Test Error: Couldn't start Test Device.\n");
		exit(1);
	}

	struct timeval last;
	struct timeval curr;
	gettimeofday(&curr, NULL); //update current time

	while(read(fileno(stdin), &userInput, 1) < 0) {

		last.tv_sec = curr.tv_sec; last.tv_usec = curr.tv_usec; //update last time
		gettimeofday(&curr, NULL); //update current time
		data.time += (curr.tv_sec - last.tv_sec) + (curr.tv_usec - last.tv_usec) * .000001; //increment by difference between last and current time

		//collect data (should take precisely 25ms)
		getTestDeviceData(&data.TestDevice);

		//print data (try to make this as fast as possible) (normal print -> new thread each cycle -> one smart thread)
		printData(&data);

		if (data.TestDevice.consecutiveErrors > 20) {
			//.5 sec of missed data
			//big error happening, try to reconnect to the IMU

			fprintf(stderr, "Test ERROR: Too many consecutive missed reads.\n");
			fprintf(stderr, "Test ERROR: Trying to reconnect to Test device.\n");

			sleep(2); //pretending restart takes time

			if (restartTestDevice(&data.TestDevice) != 1) {
				//couldn't reconnect, just end the program here

				fprintf(stderr, "Test ERROR: Couldn't reconnect to Test device.\n");
				fprintf(stderr, "Test ERROR: Ending recording session.\n");

				endSession(&data);
				return 0;
			}

			fprintf(stderr, "Test ERROR: Successfully reconnected to Test device.\n");
			fprintf(stderr, "Test ERROR: Continuing data recording.\n");
		}
	}

	endSession(&data);

	return 0;
}

void endSession(Data* data) {

	double percentMissed = (data->TestDevice.errors / (double) data->TestDevice.reads) * 100;

	fprintf(stderr, "Elapsed Time (sec): %05.3f\tPercent Missed: %5.3f%%\n",
			data->time, percentMissed);

	closeTestDevice(&data->TestDevice);

	fprintf(stderr, "Session Ended\n\n");
}

void printData(Data* data) {

	/*
	 * Format:
	 * 		Time1 -Channels of IMU Data-
	 * 		Time2 -Channels of IMU Data-
	 * 		...
	 */

	if (updateTestDeviceRead(&data->TestDevice) == -1) {
		printf("*");
	}

	printf("%f\t", data->time);
	for (int i = 0; i < TESTDEVICE_READ_SZ; i++) {
		printf("%i\t", data->TestDevice.read[i]);
	}
	printf("\n");

}

int startTestDevice(TestDevice* TestDevice) {

	//try to reconnect 4 times, waiting 4 sec between attempt

	for (int attempt = 0; initializeTestDevice(TestDevice) == -1 && attempt < 4; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
		sleep(4); //try again after 4 seconds
	}

	return 1;
}

int restartTestDevice(TestDevice* TestDevice) {

	//try to reconnect 4 times, waiting 4 sec between attempt

	for (int attempt = 0; reconnectTestDevice(TestDevice) == -1 && attempt < 4; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
		sleep(4); //try again after 4 seconds
	}

	return 1;
}

