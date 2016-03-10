/* Name: structureTest.c
 * Author: Elijah Pivo
 *
 * Description:
 * 	Employs most of the mobile program procedure, but without any hardware
 * 	requirements.
 *
 * Usage:
 * 	Compile with: gcc -o structureTest structureTest.c quickDevice.c slowDevice.c -pthread -std=gnu99 -Wall -Wextra
 *
 *  Start recording with sudo ./structureTest, stop with ctrl-d.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include "quickDevice.h"
#include "slowDevice.h"

typedef struct {
	QuickDevice IMU;
	QuickDevice CyGl;
	QuickDevice Force;
	SlowDevice EMG;

	int readsSinceEMG;
	double time;
	int errors;
	int reads;

	/*
	 * Array Location Significance:
	 * 0: IMU Control
	 * 1: CyGl Control
	 * 2: Force Control
	 * 3: EMG Control
	 * 4: Print Control
	 *
	 * Value Meanings:
	 * 0: Handling a read or print request.
	 * 1: Get a read or trigger print.
	 * 2: Ready to accept a read or print request.
	 */
	int controlValues[5];

	FILE* EMGFile; //holds just EMG data
	FILE* outFile; //holds time stamped IMU, CyGl, Force sensor info
} Data;

void setPriority(int priority);
void startSensors();
void startThreads();
void getData();
void* IMUThread();
void* CyGlThread();
void* ForceThread();
void* EMGThread();
void checkSensors();
void* printSaveDataThread();
void endSession();

Data data;

/*
 * Array location significance is the same as controlValues.
 */
pthread_t threads[5];
pthread_mutex_t threadLocks[5];
pthread_cond_t threadSignals[5];

int main(void) {

	fprintf(stderr, "Beginning Structure Test\n");

	//make stdin non blocking
	int flags = fcntl(fileno(stdin), F_GETFL, 0);
	flags |= O_NONBLOCK;
	flags = fcntl(fileno(stdin), F_SETFL, flags);
	char userInput;

	data.readsSinceEMG = 0;

//	setPriority();

	fprintf(stderr, "Connecting to sensors.\n");

	startSensors();
	printf("Here 3");

	//start data collection and print threads, initialize necessary mutex's
	startThreads();

	fprintf(stderr, "Collecting data.\n");

	data.outFile = fopen("/home/pi/Desktop/ArmTrack/ArmTrackData.txt", "w");
	data.EMGFile = fopen("/home/pi/Desktop/ArmTrack/ArmTrackEMGData.txt", "w");
	data.errors = 0;
	data.reads = 0;

	struct timeval last;
	struct timeval curr;
	struct timeval temp;

	gettimeofday(&curr, NULL); //update current time

	while(read(fileno(stdin), &userInput, 1) < 0) {

		last.tv_sec = curr.tv_sec; last.tv_usec = curr.tv_usec; //update last time
		gettimeofday(&curr, NULL); //update current time
		data.time += (curr.tv_sec - last.tv_sec) + (curr.tv_usec - last.tv_usec) * .000001; //increment by difference between last and current time

		getData();

		//signal print thread
		while (data.controlValues[4] != 2) {};
		pthread_mutex_lock(&threadLocks[4]);
		data.controlValues[4] = 1;
		pthread_cond_signal(&threadSignals[4]);
		pthread_mutex_unlock(&threadLocks[4]);

		checkSensors();

		//wait for 25ms cycle length
		do {
			gettimeofday(&temp, NULL);
		} while ( (temp.tv_sec - curr.tv_sec) + (temp.tv_usec - curr.tv_usec) * .000001 < .024993);


		if (data.time > 13) {
			endSession();
			return 1;
		}

	}

	endSession();
	return 1;
}

void setPriority(int priority) {
	//should be careful with this!
	//make data collection thread a time critical thread
	struct sched_param param;
	param.sched_priority = priority;
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

void startSensors() {

	closeQuickDevice(&data.IMU);
	closeQuickDevice(&data.CyGl);
	closeQuickDevice(&data.Force);
	closeSlowDevice(&data.EMG);

	if (initializeQuickDevice(&data.IMU) != -1) {
		fprintf(stderr, "IMU initialized.\n");
	} else {
		fprintf(stderr, "Couldn't initialize IMU.\n");
	}

	if (initializeQuickDevice(&data.CyGl) != -1) {
		fprintf(stderr, "CyberGlove II initialized.\n");
	} else {
		fprintf(stderr, "Couldn't initialize CyberGlove II.\n");
	}

	if (initializeQuickDevice(&data.Force) != -1) {
		fprintf(stderr, "Force sensors initialized.\n");
	} else {
		fprintf(stderr, "Couldn't initialize force sensors.\n");
	}

	if (initializeSlowDevice(&data.EMG) != -1) {
		fprintf(stderr, "EMG initialized.\n");
	} else {
		fprintf(stderr, "Couldn't initialize EMG.\n");
	}

}

void startThreads() {

	for (int i = 0; i < 5; i++) {
		data.controlValues[i] = 0;
	}

	if (data.IMU.id != -1) {

		pthread_mutex_init(&threadLocks[0], NULL);
		pthread_cond_init(&threadSignals[0], NULL);

		if (pthread_create(&threads[0], NULL, IMUThread, NULL) != 0) {
			fprintf(stderr, "ERROR: Couldn't start IMU data collection thread.\n");
			exit(1);
		}
	}
	if (data.CyGl.id != -1) {

		pthread_mutex_init(&threadLocks[1], NULL);
		pthread_cond_init(&threadSignals[1], NULL);

		if (pthread_create(&threads[1], NULL, CyGlThread, NULL) != 0) {
			fprintf(stderr, "ERROR: Couldn't start CyGl data collection thread.\n");
			exit(1);
		}
	}
	if (data.Force.id != -1) {

		pthread_mutex_init(&threadLocks[2], NULL);
		pthread_cond_init(&threadSignals[2], NULL);

		if (pthread_create(&threads[2], NULL, ForceThread, NULL) != 0) {
			fprintf(stderr, "ERROR: Couldn't start Force data collection thread.\n");
			exit(1);
		}
	}
	if (data.EMG.id != -1) {

		pthread_mutex_init(&threadLocks[3], NULL);
		pthread_cond_init(&threadSignals[3], NULL);

		data.controlValues[3] = 0; //stop EMG

		if (pthread_create(&threads[3], NULL, EMGThread, NULL) != 0) {
			fprintf(stderr, "ERROR: Couldn't start EMG data collection thread.\n");
			exit(1);
		}
	}

	pthread_mutex_init(&threadLocks[4], NULL);
	pthread_cond_init(&threadSignals[4], NULL);
	if (pthread_create(&threads[4], NULL, printSaveDataThread, NULL) != 0) {
		fprintf(stderr, "ERROR: Couldn't create print and save thread.\n");
		exit(1);
	}

	//ensure threads are ready
	usleep(30000);

	data.controlValues[3] = 1; //start EMG
}

void getData() {

	if (data.IMU.id != -1) {
		pthread_mutex_lock(&threadLocks[0]);
		data.controlValues[0] = 1;
		pthread_cond_signal(&threadSignals[0]);
		pthread_mutex_unlock(&threadLocks[0]);
	}
	if (data.CyGl.id != -1) {
		pthread_mutex_lock(&threadLocks[1]);
		data.controlValues[1] = 1;
		pthread_cond_signal(&threadSignals[1]);
		pthread_mutex_unlock(&threadLocks[1]);
	}
	if (data.Force.id != -1) {
		pthread_mutex_lock(&threadLocks[2]);
		data.controlValues[2] = 1;
		pthread_cond_signal(&threadSignals[2]);
		pthread_mutex_unlock(&threadLocks[2]);
	}

	//wait for data collection to be ready for another cycle
	if (data.IMU.id != -1) {
		while (data.controlValues[0] != 2) {}
	}
	if (data.CyGl.id != -1) {
		while (data.controlValues[1] != 2) {}
	}
	if (data.Force.id != -1) {
		while (data.controlValues[2] != 2) {}
	}

	data.readsSinceEMG++;
	if (data.EMG.id != -1 && data.readsSinceEMG == 8) {
		//once every 8 reads, it will wait for new EMG data
		while (data.controlValues[3] != 2) {}
		data.controlValues[3] = 1;
		data.readsSinceEMG = 0;
	}

}

void* IMUThread() {

	//make data collection thread a time critical thread
	setPriority(95);

	while (1 == 1) {
		pthread_mutex_lock(&threadLocks[0]);
		data.controlValues[0] = 2; //signals ready to accept a collection request
		pthread_cond_wait(&threadSignals[0], &threadLocks[0]);
		data.controlValues[0] = 0;
		getQuickDeviceData(&data.IMU, data.time);
		pthread_mutex_unlock(&threadLocks[0]);
	}
}

void* CyGlThread() {

	//make data collection thread a time critical thread
	setPriority(95);

	while (1 == 1) {
		pthread_mutex_lock(&threadLocks[1]);
		data.controlValues[1] = 2; //signals ready to accept a collection request
		pthread_cond_wait(&threadSignals[1], &threadLocks[1]);
		data.controlValues[1] = 0;
		getQuickDeviceData(&data.CyGl, data.time);
		pthread_mutex_unlock(&threadLocks[1]);
	}
}

void* ForceThread() {

	//make data collection thread a time critical thread
	setPriority(95);

	while (1 == 1) {
		pthread_mutex_lock(&threadLocks[2]);
		data.controlValues[2] = 2; //signals ready to accept a collection request
		pthread_cond_wait(&threadSignals[2], &threadLocks[2]);
		data.controlValues[2] = 0;
		getQuickDeviceData(&data.Force, data.time);
		pthread_mutex_unlock(&threadLocks[2]);
	}
}

void* EMGThread() {

	//make data collection thread a time critical thread
	setPriority(95);

	while (1 == 1) {
		if (data.controlValues[3] != 0) {
			//collect data
			getSlowDeviceData(&data.EMG, data.time);
			data.controlValues[3] = 2;
		}
	}
}

void checkSensors() {

	if (data.IMU.id != -1 &&  data.IMU.consecutiveErrors > 20) {
		//.5 sec of missed data
		//big error happening, try to reconnect to the IMU
		fprintf(stderr, "ERROR: Too many consecutive missed reads.\n");
		fprintf(stderr, "ERROR: Trying to reconnect to IMU.\n");
		if (restartQuickDevice(&data.IMU) != 1) {
			//couldn't reconnect, just end the program here
			fprintf(stderr, "ERROR: Couldn't reconnect to IMU.\n");
			closeQuickDevice(&data.IMU);
		} else {
			fprintf(stderr, "ERROR: Successfully reconnected to IMU.\n");
		}
		fprintf(stderr, "ERROR: Continuing data recording.\n");
	}
	if (data.CyGl.id != -1 && data.CyGl.consecutiveErrors > 20) {
		//.5 sec of missed data
		//big error happening, try to reconnect to the CyberGlove
		fprintf(stderr, "ERROR: Too many consecutive missed reads.\n");
		fprintf(stderr, "ERROR: Trying to reconnect to CyberGlove.\n");
		if (restartQuickDevice(&data.CyGl) != 1) {
			//couldn't reconnect, just end the program here
			fprintf(stderr, "ERROR: Couldn't reconnect to CyberGlove.\n");
			closeQuickDevice(&data.CyGl);
		} else {
			fprintf(stderr, "ERROR: Successfully reconnected to CyberGlove.\n");
		}
		fprintf(stderr, "ERROR: Continuing data recording.\n");
	}
	if (data.Force.id != -1 && data.Force.consecutiveErrors > 20) {
		//.5 sec of missed data
		//big error happening, try to reconnect to the Force sensors
		fprintf(stderr, "ERROR: Too many consecutive missed reads.\n");
		fprintf(stderr, "ERROR: Trying to reconnect to Force sensors.\n");
		if (restartQuickDevice(&data.Force) != 1) {
			//couldn't reconnect, just end the program here
			fprintf(stderr, "ERROR: Couldn't reconnect to Force sensors.\n");
			closeQuickDevice(&data.Force);
		} else {
			fprintf(stderr, "ERROR: Successfully reconnected to Force sensors.\n");
		}
		fprintf(stderr, "ERROR: Continuing data recording.\n");
	}
	if (data.EMG.id != -1 && data.EMG.consecutiveErrors > 20) {
		//.5 sec of missed data
		//big error happening, try to reconnect to the EMG

		//stop EMG data collection
		data.controlValues[3] = 0;

		fprintf(stderr, "ERROR: Too many consecutive missed reads.\n");
		fprintf(stderr, "ERROR: Trying to reconnect to EMG.\n");
		if (restartSlowDevice(&data.EMG) != 1) {
			//couldn't reconnect, just end the program here
			fprintf(stderr, "ERROR: Couldn't reconnect to EMG.\n");
			closeSlowDevice(&data.EMG);
		} else {
			//restart EMG data collection
			data.controlValues[3] = 1;
			fprintf(stderr, "ERROR: Successfully reconnected to EMG.\n");
		}
		fprintf(stderr, "ERROR: Continuing data recording.\n");
	}
}

void* printSaveDataThread() {

	int IMUError, CyGlError, ForceError, EMGError;

	while (1 == 1) {

		pthread_mutex_lock(&threadLocks[4]);
		data.controlValues[4] = 2; //signals ready to accept a print request
		pthread_cond_wait(&threadSignals[4], &threadLocks[4]);
		data.controlValues[4] = 0;

		/* Prints:
		 *
		 * TIME
		 * IMU READ
		 * CyGl READ
		 * Force READ
		 * EMG READ
		 *
		 * ...
		 */

		data.reads++;

		if (data.IMU.id != -1) {
			IMUError = updateQuickDeviceRead(&data.IMU);
		} else {
			IMUError = 1; // no error
		}
		if (data.CyGl.id != -1) {
			CyGlError = updateQuickDeviceRead(&data.CyGl);
		} else {
			CyGlError = 1; //no error
		}
		if (data.Force.id != -1) {
			ForceError = updateQuickDeviceRead(&data.Force);
		} else {
			ForceError = 1; //no error
		}
		if (data.EMG.id != -1 && data.readsSinceEMG == 0) {
			EMGError = updateSlowDeviceRead(&data.EMG);
		} else {
			EMGError = 1;
		}

		if (IMUError == -1 || CyGlError == -1 || ForceError == -1 || EMGError == -1) {
			data.errors++;
			printf("*");
		}

		//write time from one of connected sensors if possible
		if (data.IMU.id != -1) {
			printf("%5f\n", data.IMU.readTime);
			fprintf(data.outFile, "%5f\t", data.IMU.readTime);
		} else if (data.CyGl.id != -1) {
			printf("%5f\n", data.CyGl.readTime);
			fprintf(data.outFile, "%5f\t", data.CyGl.readTime);
		} else if (data.Force.id != -1) {
			printf("%5f\n", data.Force.readTime);
			fprintf(data.outFile, "%5f\t", data.Force.readTime);
		} else if (data.EMG.id != -1) {
			printf("%5f\n", data.EMG.readTime);
			fprintf(data.outFile, "%5f\t", data.EMG.readTime);
		} else {
			printf("%5f\n", data.time);
			fprintf(data.outFile, "%5f\t", data.time);
		}

		//IMU
		//IMU missed read flag
		if (IMUError == -1) {
			//this sensor had a missed read, mark it with an asterisk
			printf("*");
		}
		//save IMU data
		for (int i = 0; i < QUICKDEVICE_READ_SZ; i++) {
			printf("%i\t", data.IMU.read[i]);
			fprintf(data.outFile, "%i\t", data.IMU.read[i]);
		}
		printf("\n");

		//CyberGlove
		//CyGl missed read flag
		if (CyGlError == -1) {
			//this sensor had a missed read, mark it with an asterisk
			printf("*");
		}
		for (int i = 0; i < QUICKDEVICE_READ_SZ; i++) {
			printf("%i\t", (int) data.CyGl.read[i]);
			fprintf(data.outFile, "%i\t", (int) data.CyGl.read[i]);
		}
		printf("\n");

		//Force Sensors
		//Force missed read flag
		if (ForceError == -1) {
			//this sensor had a missed read, mark it with an asterisk
			printf("*");
		}
		//save force data
		for (int i = 0; i < QUICKDEVICE_READ_SZ; i++) {
			printf("%i\t", data.Force.read[i]);
			fprintf(data.outFile, "%i\t", data.Force.read[i]);
		}
		printf("\n");

		if (data.EMG.id != -1 && data.readsSinceEMG == 0) {
			//EMG missed read flag
			if (EMGError == -1) {
				//this sensor had a missed read, mark it with an asterisk
				printf("*");
			}
			for (int i = 0; i < SLOWDEVICE_READS_PER_CYCLE; i++) {
				for (int j = 0; j < SLOWDEVICE_READ_SZ; j++) {
					printf("%i\t", data.EMG.read[i * EMG_READ_SZ + j]);
					fprintf(data.outFile, "%i\t", data.EMG.read[i * EMG_READ_SZ + j]);
				}
				printf("\n");
				fprintf(data.outFile, "\n");
			}
			printf("\n");
		} else if (data.EMG.id == -1) {
			printf("EMG UNUSED");
		}
		//terminate data collection period
		printf("\n");

		pthread_mutex_unlock(&threadLocks[4]);

		//save file every 60 seconds
		if ((int) data.time % 60 == 0) {
			fclose(data.outFile);
			data.outFile = fopen("/home/pi/Desktop/ArmTrack/ArmTrackData.txt", "a");
			data.EMGFile = fopen("/home/pi/Desktop/ArmTrack/ArmTrackEMGData.txt", "a");
		}
	}

	pthread_exit(NULL);
}

void endSession() {

	//ensure the print thread is allowed to complete
	usleep(30000);
	for (int i = 0; i < 5; i++) {
		pthread_cancel(threads[i]);
		pthread_mutex_destroy(&threadLocks[i]);
		pthread_cond_destroy(&threadSignals[i]);
	}

	//close and save file
	fclose(data.outFile);

	//upload file to DropBox (hold green and red LED on during upload)
	fprintf(stderr, "Uploading to dropbox...\t");
	system("/home/pi/Dropbox-Uploader/dropbox_uploader.sh upload /home/pi/Desktop/ArmTrack/ArmTrackData.bin /");
	fprintf(stderr, "done.\n");

	//report Error percentage
	double percentMissed = (data.errors /(double) data.reads) * 100;

	fprintf(stderr, "Elapsed Time (sec): %05.3f\tPercent Missed: %5.3f%%\n",
			data.time, percentMissed);

	//close all sensors
	closeQuickDevice(&data.IMU);
	closeQuickDevice(&data.CyGl);
	closeQuickDevice(&data.Force);
	closeSlowDevice(&data.EMG);

	fprintf(stderr, "Session Ended\n\n");

	//then turn off the raspberry pi on actual mobile program but not here so I can do repeated runs
	//system("sudo shutdown -h now");
}
