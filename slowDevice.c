/*
 * Name: slowDevice.c
 * Author: Elijah Pivo
 *
 * Slow Device Test interface, mimics behavior of EMG data collection.
 *
 */



#include "slowDevice.h"

#include <stdio.h>

int initializeSlowDevice(SlowDevice* slowDevice) {

	slowDevice->id = 1;
	slowDevice->hasNewRead1 = 0;
	slowDevice->hasNewRead2 = 0;
	slowDevice->bufferToUse = 1;
	for (int i = 0; i < SLOWDEVICE_READ_SZ * SLOWDEVICE_READS_PER_CYCLE; i++) {
		slowDevice->readBuffer1[i] = 0;
		slowDevice->readBuffer2[i] = 0;
		slowDevice->read[i] = 0;
	}
	slowDevice->reads = 0;
	slowDevice->errors = 0;
	slowDevice->consecutiveErrors = 0;

	return slowDevice->id;

}

int reconnectSlowDevice(SlowDevice* slowDevice) {

	//first close the device
	slowDevice->id = -1;

	//then initialize the device
	slowDevice->id = 1;
	slowDevice->hasNewRead1 = 0;
	slowDevice->hasNewRead2 = 0;
	slowDevice->bufferToUse = 1;
	for (int i = 0; i < SLOWDEVICE_READ_SZ * SLOWDEVICE_READS_PER_CYCLE; i++) {
		slowDevice->readBuffer1[i] = 0;
		slowDevice->readBuffer2[i] = 0;
		slowDevice->read[i] = 0;
	}
	slowDevice->consecutiveErrors = 0;

	return slowDevice->id;
}

int startSlowDevice(SlowDevice* slowDevice) {

	//try to reconnect 4 times, waiting 4 sec between attempt

	for (int attempt = 0; initializeSlowDevice(slowDevice) == -1; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
		sleep(4); //try again after 4 seconds
	}

	return 1;
}

int restartSlowDevice(SlowDevice* slowDevice) {

	//try to reconnect 4 times, waiting 4 sec between attempt

	for (int attempt = 0; reconnectSlowDevice(slowDevice) == -1; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
		sleep(4); //try again after 4 seconds
	}

	return 1;
}

int getSlowDeviceData(SlowDevice* slowDevice, double time) {
		struct timeval start, end;
		gettimeofday(&start, NULL);

		slowDevice->reads++;

		switch (slowDevice->reads % 2) {
		case 0:
			//read into readBuffer1 on even reads
			slowDevice->readBuffer1Time = time;
			for (int i = 0; i < SLOWDEVICE_READ_SZ * SLOWDEVICE_READS_PER_CYCLE; i++) {
				slowDevice->readBuffer1[i] = slowDevice->reads;
			}

			slowDevice->hasNewRead1 = 1;
			break;
		case 1:
			//read into readBuffer2 on odd reads
			slowDevice->readBuffer2Time = time;

			for (int i = 0; i < SLOWDEVICE_READ_SZ * SLOWDEVICE_READS_PER_CYCLE; i++) {
				slowDevice->readBuffer2[i] = slowDevice->reads;
			}

			slowDevice->hasNewRead2 = 1;
			break;
		}

		//this part is proving to be inconsistently timed...
		//allows manipulation of time process takes
		do {
			gettimeofday(&end, NULL);
		} while ((end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) * .000001 < .202);


		return 1;
}

int updateSlowDeviceRead(SlowDevice* slowDevice) {

	switch (slowDevice->bufferToUse) {
	case 1:
		slowDevice->bufferToUse = 2;
		slowDevice->readTime = slowDevice->readBuffer1Time;
		if (slowDevice->hasNewRead2 == 1) {
			//New data available in read buffer 2
			memcpy(&slowDevice->read, &slowDevice->readBuffer2, SLOWDEVICE_READ_SZ * SLOWDEVICE_READS_PER_CYCLE * sizeof(int)); //update the read field
			slowDevice->consecutiveErrors = 0; //data collection was successful
			slowDevice->hasNewRead2 = 0;
			return 1;
		}
		break;
	case 2:
		slowDevice->bufferToUse = 1;
		slowDevice->readTime = slowDevice->readBuffer2Time;
		if (slowDevice->hasNewRead1 == 1) {
			//New data available
			memcpy(&slowDevice->read, &slowDevice->readBuffer1, SLOWDEVICE_READ_SZ * SLOWDEVICE_READS_PER_CYCLE * sizeof(int)); //update the read field
			slowDevice->consecutiveErrors = 0; //data collection was successful
			slowDevice->hasNewRead1 = 0;
			return 1;
		}

		break;
	}

	//data collection must have been unsuccessful
	slowDevice->errors++;
	slowDevice->consecutiveErrors++;
	return -1;
}

void closeSlowDevice(SlowDevice* slowDevice) {

	slowDevice->id = -1;
	slowDevice->hasNewRead1 = 0;
	slowDevice->hasNewRead2 = 0;
	for (int i = 0; i < SLOWDEVICE_READ_SZ * SLOWDEVICE_READS_PER_CYCLE; i++) {
		slowDevice->readBuffer1[i] = 0;
		slowDevice->readBuffer2[i] = 0;
		slowDevice->read[i] = 0;
	}
	slowDevice->reads = 0;
	slowDevice->errors = 0;
	slowDevice->consecutiveErrors = 0;

}
