/*
 * Name: quickDevice.c
 * Author: Elijah Pivo
 *
 * quickDevice interface
 *
 */

#include "quickDevice.h"

int initializeQuickDevice(QuickDevice* quickDevice) {

	quickDevice->id = 1;
	quickDevice->hasNewRead1 = 0;
	quickDevice->hasNewRead2 = 0;
	quickDevice->bufferToUse = 2;
	for (int i = 0; i < QUICKDEVICE_READ_SZ; i++) {
		quickDevice->readBuffer1[i] = 0;
		quickDevice->readBuffer2[i] = 0;
		quickDevice->read[i] = 0;
	}
	quickDevice->reads = 0;
	quickDevice->errors = 0;
	quickDevice->consecutiveErrors = 0;

	return quickDevice->id;

}

int reconnectQuickDevice(QuickDevice* quickDevice) {

	//first close the device
	quickDevice->id = -1;

	//then initialize the device
	quickDevice->id = 1;
	quickDevice->hasNewRead1 = 0;
	quickDevice->hasNewRead2 = 0;
	quickDevice->bufferToUse = 2;
	for (int i = 0; i < QUICKDEVICE_READ_SZ; i++) {
		quickDevice->readBuffer1[i] = 0;
		quickDevice->readBuffer2[i] = 0;
		quickDevice->read[i] = 0;

	}
	quickDevice->consecutiveErrors = 0;

	return quickDevice->id;

}

int startQuickDevice(QuickDevice* quickDevice) {

	//try to reconnect 4 times, waiting 4 sec between attempt

	for (int attempt = 0; initializeQuickDevice(quickDevice) == -1; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
		sleep(4); //try again after 4 seconds
	}

	return 1;
}

int restartQuickDevice(QuickDevice* quickDevice) {

	//try to reconnect 4 times, waiting 4 sec between attempt

	for (int attempt = 0; reconnectQuickDevice(quickDevice) == -1; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
		sleep(4); //try again after 4 seconds
	}

	return 1;
}

int getQuickDeviceData(QuickDevice* quickDevice, double time) {

//	struct timeval start, end;
//	gettimeofday(&start, NULL);

	quickDevice->reads++;

//	fprintf(stderr, "Here 5\n");

	switch (quickDevice->reads % 2) {
	case 0:
		//read into readBuffer1 on even reads
		quickDevice->readBuffer1Time = time;
		for (int i = 0; i < QUICKDEVICE_READ_SZ; i++) {
			quickDevice->readBuffer1[i] = quickDevice->reads;
		}

//		printf("Here 6\n");
		usleep(30000);
		quickDevice->hasNewRead1 = 1;
//		return -1;
		break;
	case 1:
		//read into readBuffer2 on odd reads
		quickDevice->readBuffer2Time = time;

		for (int i = 0; i < QUICKDEVICE_READ_SZ; i++) {
			quickDevice->readBuffer2[i] = quickDevice->reads;
		}
//		printf("Here 7\n");
		usleep(25000);
		quickDevice->hasNewRead2 = 1;
//		return -1;
		break;
	}
//	//allows manipulation of time process takes
//	gettimeofday(&end, NULL);
//	while ((end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) * .000001 <= .03) {
//		gettimeofday(&end, NULL);
//	}

//	fprintf(stderr, "Here 8\n");

	return 1;
}

int updateQuickDeviceRead(QuickDevice* quickDevice) {

//	fprintf(stderr, "Here 1\n");

	switch (quickDevice->bufferToUse) {
	case 1:
		quickDevice->bufferToUse = 2;
		quickDevice->readTime = quickDevice->readBuffer1Time;
		if (quickDevice->hasNewRead1 == 1) {
			//New data available
			memcpy(&quickDevice->read, &quickDevice->readBuffer1, QUICKDEVICE_READ_SZ * sizeof(int)); //update the read field
			quickDevice->consecutiveErrors = 0; //data collection was successful
			quickDevice->hasNewRead1 = 0;

//			fprintf(stderr, "Here 2\n");

			return 1;
		}
		break;
	case 2:
		quickDevice->bufferToUse = 1;
		quickDevice->readTime = quickDevice->readBuffer2Time;
		//update from readBuffer2 on even reads
		if (quickDevice->hasNewRead2 == 1) {
			//New data available in read buffer 2
			memcpy(&quickDevice->read, &quickDevice->readBuffer2, QUICKDEVICE_READ_SZ * sizeof(int)); //update the read field
			quickDevice->consecutiveErrors = 0; //data collection was successful
			quickDevice->hasNewRead2 = 0;

//			fprintf(stderr, "Here 3\n");

			return 1;
		}
		break;
	}

//	fprintf(stderr, "Here 4\n");

	//data collection must have been unsuccessful
	quickDevice->errors++;
	quickDevice->consecutiveErrors++;
	return -1;
}

void closeQuickDevice(QuickDevice* quickDevice) {

	quickDevice->id = -1;
	quickDevice->hasNewRead1 = 0;
	quickDevice->hasNewRead2 = 0;
	for (int i = 0; i < QUICKDEVICE_READ_SZ; i++) {
		quickDevice->readBuffer1[i] = 0;
		quickDevice->readBuffer2[i] = 0;
		quickDevice->read[i] = 0;
	}
	quickDevice->reads = 0;
	quickDevice->errors = 0;
	quickDevice->consecutiveErrors = 0;

}
