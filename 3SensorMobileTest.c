/* Name: 3SensorMobileTest.c
 * Author: Elijah Pivo
 *
 * Description:
 * 	Reads data from connected sensors and prints and stores it after a switch
 * 	is flipped. Employs mobile program procedure described below with computer
 * 	displays for testing. Only deals with IMU, Force, and CyGl Sensors.
 *
 * Usage:
 * 	Compile with: gcc -o mobileTest mobileTest.c IMU.c CyGl.c Force.c -lwiringPi -pthread -std=gnu99 -Wall -Wextra
 *
 * 	Starts and stops recording data when a switch is flipped.
 *
 * Procedure:
 *   1.	First, the program will repeatedly attempt to initialize the sensors.
 * 		To denote the beginning of an initialization cycle, both LED’s will blink
 * 		at the same time. The following blinks represent whether a sensor has
 * 		been initialized or not. The Green LED will blink if it has been initialized,
 * 	    the Red LED will blink if not. The first flash represents whether or not the
 * 	    IMU chain is initialized, second whether or not the Wireless CyberGlove 2
 * 	    is initialized, and third whether or not the Force sensor is initialized.
 * 	 2.	Once all desired sensors are initialized, flip the switch to start
 * 	 	recording data.
 * 	 3.	During successful data recording, the Green LED will remain on and data
 * 	 	will be stored to the file “ArmTrackData.txt”. A missed read means one
 * 	 	of the sensors didn’t return data within the 25ms read cycle so the most
 * 	 	recent data available was used instead. A missed read on a single sensor
 * 	 	will cause the Red LED to flash and the most recent successful data will
 * 	 	be stored instead with an asterisk preceding the line. A sustained program
 * 	 	failure is considered more than 20 consecutive misreads. Under a sustained
 * 	 	program failure the Red LED will remain on while the Pi will attempt to
 * 	 	reconnect to all the sensors that were being used. If the program can’t
 * 	 	reconnect to a sensor it will ignore that sensor and record with the
 * 	 	functional sensors.
 * 	 4.	At the end of a recording session flip the switch off.
 * 	 5.	The Green and Red LED will blink once simultaneously followed by a number
 * 	  	of blinks of just the Red LED. After these, the Green and Red LED will flash
 * 	   	once again. Each of the Red LED blinks represents a percent of data that has
 * 	   	been misread. For example, 2 blinks means 2% of the data collected contained
 * 	   	a missed read.
 * 	 6.	The Pi will save the data file and, if attached to the Eduroam Wifi in the lab,
 * 	 	upload it to DropBox.
 * 	 7.	Finally, the Pi will shut off.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <setjmp.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include "wiringPi.h"
#include "wiringSerial.h"
#include "IMU.h"
#include "CyGl.h"
#include "Force.h"

typedef struct {
	IMU IMU;
	CyGl CyGl;
	Force Force;

	double time;
	int errors;
	int reads;

	//controls IMU, CyGl, and Force collection threads
	int IMUControl;
	int CyGlControl;
	int ForceControl;
	//controls print thread
	int print;

	FILE* outFile;
} Data;

#define NUM_SENSORS 3

#define GREEN_LED 29
#define RED_LED 28
#define SWITCH 27

void startSensors();
void startThreads();
void* printSaveData();
void getData();
void* collectIMU();
void* collectCyGl();
void* collectForce();
void endSession();

Data data;

pthread_t printThread;
pthread_mutex_t printLock;
pthread_cond_t printSignal;

pthread_t IMUThread;
pthread_mutex_t IMULock;
pthread_cond_t IMUSignal;

pthread_t CyGlThread;
pthread_mutex_t CyGlLock;
pthread_cond_t CyGlSignal;

pthread_t ForceThread;
pthread_mutex_t ForceLock;
pthread_cond_t ForceSignal;

int main(void) {

	fprintf(stderr, "Beginning Mobile Test\n");

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

	//initialize wiringPi and then setup pins
	if (wiringPiSetup() != 0) {
		exit(0);
	}

	pinMode(GREEN_LED, OUTPUT);
	pinMode(RED_LED, OUTPUT);

	pinMode(SWITCH, INPUT);
	pullUpDnControl(SWITCH, PUD_UP);

	data.print = 0;
	data.IMUControl = 0;
	data.CyGlControl = 0;
	data.ForceControl = 0;

	fprintf(stderr, "Connecting to sensors.\n");

	//repeatedly initialize until switch is flipped on
	while (digitalRead(SWITCH) == 0) {

		//flash both LED's at start of each initialization cycle
		digitalWrite(GREEN_LED, 1);
		digitalWrite(RED_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(GREEN_LED, 0);
		digitalWrite(RED_LED, 0);
		usleep(500000); //.5 sec

		startSensors();

		sleep(2); //wait two seconds between start cycles
	}

	//start threads and initializes mutex's for connected sensors
	startThreads();

	fprintf(stderr, "Collecting data.\n");
	digitalWrite(GREEN_LED, 1); //turn on green LED while recording data

	data.outFile = fopen("/home/pi/Desktop/ArmTrack/ArmTrackData.bin", "wb");
	data.errors = 0;
	data.reads = 0;

	struct timeval last;
	struct timeval curr;
	struct timeval temp;

	gettimeofday(&curr, NULL); //update current time

	while(digitalRead(SWITCH) == 1) {

		last.tv_sec = curr.tv_sec; last.tv_usec = curr.tv_usec; //update last time
		gettimeofday(&curr, NULL); //update current time
		data.time += (curr.tv_sec - last.tv_sec) + (curr.tv_usec - last.tv_usec) * .000001; //increment by difference between last and current time

		getData();

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
		if (data.CyGl.consecutiveErrors > 20) {
			//.5 sec of missed data
			//big error happening, try to reconnect to the CyGl
			fprintf(stderr, "ERROR: Too many consecutive missed reads.\n");
			fprintf(stderr, "ERROR: Trying to reconnect to CyberGlove.\n");
			if (restartCyGl(&data.CyGl) != 1) {
				//couldn't reconnect, just end the program here
				fprintf(stderr, "ERROR: Couldn't reconnect to CyberGlove.\n");
				fprintf(stderr, "ERROR: Ending recording session.\n");
				endSession();
				return 0;
			}
			fprintf(stderr, "ERROR: Successfully reconnected to CyberGlove.\n");
			fprintf(stderr, "ERROR: Continuing data recording.\n");
		}
		if (data.Force.consecutiveErrors > 20) {
			//.5 sec of missed data
			//big error happening, try to reconnect to the Force sensors
			fprintf(stderr, "ERROR: Too many consecutive missed reads.\n");
			fprintf(stderr, "ERROR: Trying to reconnect to Force sensors.\n");
			if (restartQuickDevice(&data.Force) != 1) {
				//couldn't reconnect, just end the program here
				fprintf(stderr, "ERROR: Couldn't reconnect to Force sensors.\n");
				fprintf(stderr, "ERROR: Ending recording session.\n");
				endSession();
				return 0;
			}
			fprintf(stderr, "ERROR: Successfully reconnected to Force Sensors.\n");
			fprintf(stderr, "ERROR: Continuing data recording.\n");
		}

		if ((int) data.time % 60 == 0) {
			//save file every 60 seconds
			fclose(data.outFile);
			data.outFile = fopen("/home/pi/Desktop/ArmTrack/ArmTrackData.bin", "ab");
		}

		do {
			gettimeofday(&temp, NULL);
		} while ( (temp.tv_sec - curr.tv_sec) + (temp.tv_usec - curr.tv_usec) * .000001 < .024993);
	}
	endSession();
	return 1;
}

void startSensors() {

	closeIMU(&data->IMU);
	closeCyGl(&data->CyGl);
	closeForce(&data->Force);

	if (initializeIMU(&data->IMU) != -1) {
		fprintf(stderr, "IMU initialized.\n");
		//blink GREEN led if IMU did connect
		digitalWrite(GREEN_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(GREEN_LED, 0);
	} else {
		fprintf(stderr, "Couldn't initialize IMU.\n");
		//blink RED led if IMU didn't connect
		digitalWrite(RED_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(RED_LED, 0);
	}

	if (initializeCyGl(&data->CyGl) != -1) {
		fprintf(stderr, "CyberGlove II initialized.\n");
		//blink GREEN led if CyberGlove did connect
		digitalWrite(GREEN_LED, 1);
		usleep(500000); //.5
		digitalWrite(GREEN_LED, 0);
		usleep(500000); //.5 sec
	} else {
		fprintf(stderr, "Couldn't initialize CyberGlove II.\n");
		//blink RED led if CyberGlove didn't connect
		digitalWrite(RED_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(RED_LED, 0);
		usleep(500000); //.5 sec
	}

	if (initializeForce(&data->Force) != -1) {
		fprintf(stderr, "Force sensors initialized.\n");
		//blink GREEN led if force sensors did connect
		digitalWrite(GREEN_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(GREEN_LED, 0);
		usleep(500000); //.5 sec
	} else {
		fprintf(stderr, "Couldn't initialize force sensors.\n");
		//blink RED led if force sensors didn't connect
		digitalWrite(RED_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(RED_LED, 0);
		usleep(500000); //.5 sec
	}
}

void startThreads() {

	if (data.IMU.id > 0) {

		pthread_mutex_init(&IMULock, NULL);
		pthread_cond_init(&IMUSignal, NULL);

		if (pthread_create(&IMUThread, NULL, collectIMU, NULL) != 0) {
			fprintf(stderr, "Couldn't start IMU data collection thread...\n");
			exit(1);
		}
	}
	if (data.CyGl.id > 0) {

		pthread_mutex_init(&CyGlLock, NULL);
		pthread_cond_init(&CyGlSignal, NULL);

		if (pthread_create(&CyGlThread, NULL, collectCyGl, NULL) != 0) {
			fprintf(stderr, "Couldn't start CyGl data collection thread...\n");
			exit(1);
		}
	}
	if (data.Force.id > 0) {
		pthread_mutex_init(&ForceLock, NULL);
		pthread_cond_init(&ForceSignal, NULL);

		if (pthread_create(&ForceThread, NULL, collectForce, NULL) != 0) {
			fprintf(stderr, "Couldn't start Force data collection thread...\n");
			exit(1);
		}
	}

	pthread_mutex_init(&printLock, NULL);
	pthread_cond_init(&printSignal, NULL);
	if (pthread_create(&printThread, NULL, printData, NULL) != 0) {
		fprintf(stderr, "Couldn't create print thread...\n");
		exit(1);
	}

	//ensure threads are ready
	usleep(30000);
}

void getData() {

	data->reads++;

	if (data.IMU.id > 0) {
		pthread_mutex_lock(&IMULock);
		data.IMUControl = 1;
		pthread_cond_signal(&IMUSignal);
		pthread_mutex_unlock(&IMULock);
	}
	if (data.CyGl.id > 0) {
		pthread_mutex_lock(&CyGlLock);
		data.CyGlControl = 1;
		pthread_cond_signal(&CyGlSignal);
		pthread_mutex_unlock(&CyGlLock);
	}
	if (data.Force.id > 0) {
		pthread_mutex_lock(&ForceLock);
		data.Force = 1;
		pthread_cond_signal(&ForceSignal);
		pthread_mutex_unlock(&ForceLock);
	}

	//wait for data collection to be ready for another cycle
	if (data.IMU.id > 0) {
		while (data.IMUControl != 2) {}
	}
	if (data.CyGl.id > 0) {
		while (data.CyGlControl != 2) {}
	}
	if (data.Force.id > 0) {
		while (data.ForceControl != 2) {}
	}
}

void* collectIMU() {

	while (1 == 1) {
		pthread_mutex_lock(&IMULock);
		data.IMUControl = 2; //signals ready to accept a collection request
		pthread_cond_wait(&IMUSignal, &IMULock);
		data.IMU = 0;
		getIMUData(&data.IMU, data.time);
		pthread_mutex_unlock(&IMULock);
	}
}

void* collectCyGl() {
	while (1 == 1) {
		pthread_mutex_lock(&CyGlLock);
		data.CyGlControl = 2; //signals ready to accept a collection request
		pthread_cond_wait(&CyGlSignal, &CyGlLock);
		getCyGlData(&data.CyGl, data.time);
		data.CyGlControl = 0;
		pthread_mutex_unlock(&CyGlLock);
	}
}

void* collectForce() {
	while (1 == 1) {
		pthread_mutex_lock(&ForceLock);
		data.ForceControl = 2; //signals ready to accept a collection request
		pthread_cond_wait(&ForceSignal, &ForceLock);
		getForceData(&data.Force, data.time);
		data.ForceControl= 0;
		pthread_mutex_unlock(&ForceLock);
	}
}

void* printSaveData() {

	while (1 == 1) {

		pthread_mutex_lock(&printLock);
		data.print = 2; //signals ready to accept a print request
		pthread_cond_wait(&printSignal, &printLock);
		data.print = 0;

		/* Prints:
		 *
		 * TIME
		 * IMU READ
		 * CyGl READ
		 * Force READ
		 * EMG READ
		 *
		 * TIME
		 * IMU READ
		 * CYGL READ
		 * Force READ
		 * EMG READ
		 *
		 * ...
		 */

		data.reads++;

		int IMUError = updateIMURead(&data.IMU);
		int CyGlError = updateCyGlRead(&data.CyGl);
		int ForceError = updateForceRead(&data.Force);

		//unused lines are labeled "SENSOR UNUSED"

		if (IMUError != -1 && CyGlError != -1 && ForceError != -1) {
			//no missed read
			fwrite('=', sizeof(char), 1, data->outFile);
		} else {
			//report missed read
			fprintf(stderr, "*");
			fwrite('*', sizeof(char), 1, data->outFile);
		}

		//write time
		fprintf(stderr, "%05.3f\n", data->time);
		fwrite(&time, sizeof(double), 1, data->outFile);
		fwrite(' ', sizeof(char), 1, data->outFile);

		//IMU
		if (data->IMU.id != -1) {
			//IMU missed read flag
			if (IMUError == -1) {
				//this sensor had a missed read, mark it with an asterisk
				fprintf(stderr, "*");
				fwrite('*', sizeof(char), 1, data->outFile);
			} else {
				//no missed read
				fwrite('=', sizeof(char), 1, data->outFile);
			}
			//save IMU data
			for (int i = 0; i < IMU_READ_SZ; i++) {
				fprintf(stderr, "%03.2f\t", data->IMU.read[i]);
			}
			fwrite(data->IMU.read, sizeof(float), sizeof(data->IMU.read)/sizeof(float), data->outFile);
		} else {
			fprintf(stderr, "IMU UNUSED");
		}
		fprintf(stderr, "\n");
		fwrite(' ', sizeof(char), 1, data->outFile);

		//CyberGlove
		if (data->CyGl.id != -1) {
			//CyGl missed read flag
			if (CyGlError == -1) {
				//this sensor had a missed read, mark it with an asterisk
				fprintf(stderr, "*");
				fwrite('*', sizeof(char), 1, data->outFile);
			} else {
				//no missed read
				fwrite('=', sizeof(char), 1, data->outFile);
			}
			//save CyGl data
			for (int i = 0; i < CYGL_READ_SZ; i++) {
				fprintf(stderr, "%i\t", data->CyGl.read[i]);
			}
			fwrite(data->CyGl.read, sizeof(int), sizeof(data->CyGl.read)/sizeof(int), data->outFile);
		} else {
			fprintf(stderr, "CyGl UNUSED");
		}
		fprintf(stderr, "\n");
		fwrite(' ', sizeof(char), 1, data->outFile);
		//Force Sensors
		if (data->Force.id != -1) {
			//Force missed read flag
			if (ForceError == -1) {
				//this sensor had a missed read, mark it with an asterisk
				fprintf(stderr, "*");
				fwrite('*', sizeof(char), 1, data->outFile);
			} else {
				//no missed read
				fwrite('=', sizeof(char), 1, data->outFile);
			}
			//save force data
			for (int i = 0; i < FORCE_READ_SZ; i++) {
				fprintf(stderr, "%06.6f\t", data->Force.read[i]);
			}
			fwrite(data->Force.read, sizeof(int), sizeof(data->Force.read)/sizeof(int), data->outFile);
		} else {
			fprintf(stderr, "Force UNUSED");
		}
		fprintf(stderr, "\n");
		fwrite(' ', sizeof(char), 1, data->outFile);


		//terminate data collection period
		fprintf(stderr, "\n");
		fwrite('\n', sizeof(char), 1, data->outFile);

		pthread_mutex_unlock(&printLock);
	}
	pthread_exit(NULL);
}

void endSession(Data* data) {

	pthread_cancel(printThread);
	pthread_cancel(IMUThread);
	pthread_cancel(CyGlThread);
	pthread_cancel(ForceThread);

	pthread_mutex_destroy(&printLock);
	pthread_mutex_destroy(&IMULock);
	pthread_mutex_destroy(&CyGlLock);
	pthread_mutex_destroy(&ForceLock);
	pthread_cond_destroy(&printSignal);
	pthread_cond_destroy(&IMUSignal);
	pthread_cond_destroy(&CyGlSignal);
	pthread_cond_destroy(&ForceSignal);

	//close and save file
	fclose(data->outFile);

	//upload file to DropBox (hold green and red LED on during upload)
	digitalWrite(GREEN_LED, 1); digitalWrite(RED_LED, 1);
	system("/home/pi/Dropbox-Uploader/dropbox_uploader.sh upload /home/pi/Desktop/ArmTrack/ArmTrackData.txt");
	digitalWrite(GREEN_LED, 0); digitalWrite(RED_LED, 0);
	sleep(1);

	//report Error percentage
	double percentMissed = (data->errors /(double) data->reads) * 100;

	fprintf(stderr, "Elapsed Time (sec): %05.3f\tPercent Missed: %5.3f%%\n",
			data->time, percentMissed);

	//blink green and red LED once
	//then blink red once for each percent missed
	//then blink both green and red once again and end the program

	digitalWrite(GREEN_LED, 1); digitalWrite(RED_LED, 1);
	sleep(1);
	digitalWrite(GREEN_LED, 0); digitalWrite(RED_LED, 0);
	sleep(1);

	for (int i = 0; i < percentMissed; i++) {
		digitalWrite(RED_LED, 1);
		usleep(250000); //.25 sec
		digitalWrite(RED_LED, 0);
		usleep(250000); //.25 sec
	}

	digitalWrite(GREEN_LED, 1);
	digitalWrite(RED_LED, 1);
	sleep(1);
	digitalWrite(GREEN_LED, 0);
	digitalWrite(RED_LED, 0);

	//close all sensors
	closeIMU(&data->IMU);
	closeCyGl(&data->CyGl);
	closeForce(&data->Force);

	fprintf(stderr, "Session Ended\n\n");

	//then turn off the raspberry pi on actual mobile program but not here so I can do repeated runs
	//system("sudo shutdown -h now");
}
