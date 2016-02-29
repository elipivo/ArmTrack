/* Name: mobileArmTrackTest.c
 * Author: Elijah Pivo
 *
 * Description:
 * 	Reads data from connected sensors and prints and stores it after a switch
 * 	is flipped. Employs mobile program procedure described below with computer
 * 	displays for testing.
 *
 * Usage:
 * 	Compile with:
 * 		gcc -std=gnu99 -g -Wall -lwiringPi -pthread -Wextra -L. -lmccusb  -lm -L/usr/local/lib -lhidapi-libusb -lusb-1.0 -I. -o mobileArmTrackTest mobileArmTrackTest.c IMU.c CyGl.c Force.c EMG.c
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
 * 	    is initialized, third whether or not the Force sensor is initialized, and
 * 	    lastly whether or not the EMG sensor band is initialized.
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
#include "EMG.h"

typedef struct {
	IMU IMU;
	CyGl CyGl;
	Force Force;
	EMG EMG;

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
	FILE* outFile;
} Data;

#define GREEN_LED 28
#define RED_LED 29
#define SWITCH 27

void setPriority();
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

	fprintf(stderr, "Beginning Mobile Test\n");

	//initialize wiringPi and setup pins
	if (wiringPiSetup() != 0) {
		exit(0);
	}
	pinMode(GREEN_LED, OUTPUT);
	pinMode(RED_LED, OUTPUT);
	pinMode(SWITCH, INPUT);
	pullUpDnControl(SWITCH, PUD_UP);

	fprintf(stderr, "Connecting to sensors.\n");

	//repeatedly initialize until switch is flipped off
	do  {

		//flash both LED's at start of each initialization cycle
		digitalWrite(GREEN_LED, 1);
		digitalWrite(RED_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(GREEN_LED, 0);
		digitalWrite(RED_LED, 0);
		usleep(500000); //.5 sec

		startSensors();

		sleep(2); //wait two seconds between start cycles
	} while (digitalRead(SWITCH) == 0);

	//start data collection and print threads, initialize necessary mutex's
	startThreads();

	fprintf(stderr, "Collecting data.\n");

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

		digitalWrite(GREEN_LED, 1); //turn on green LED while recording data
		digitalWrite(RED_LED, 0);

		getData();

		//signal print thread
		while (data.controlValues[4] != 2) {};
		pthread_mutex_lock(&threadLocks[4]);
		data.controlValues[4] = 1;
		pthread_cond_signal(&threadSignals[4]);
		pthread_mutex_unlock(&threadLocks[4]);

		checkSensors();

		//wait for 25ms cycle length
		if (data.EMG.id == -1) {
			do {
				gettimeofday(&temp, NULL);
			} while ( (temp.tv_sec - curr.tv_sec) + (temp.tv_usec - curr.tv_usec) * .000001 < .024993);
		}

		//for testing and not locking up pi
		if (data.time > 60) {
			endSession();
			return 1;
		}

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

void startSensors() {

	closeIMU(&data.IMU);
	closeCyGl(&data.CyGl);
	closeForce(&data.Force);
	closeEMG(&data.EMG);

	if (initializeIMU(&data.IMU) != -1) {
		fprintf(stderr, "IMU initialized.\n");
		//blink GREEN led if IMU did connect
		digitalWrite(GREEN_LED, 1);
		usleep(500000); //.5
		digitalWrite(GREEN_LED, 0);
		usleep(100000); //.1 sec
	} else {
		fprintf(stderr, "Couldn't initialize IMU.\n");
		//blink RED led if IMU didn't connect
		digitalWrite(RED_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(RED_LED, 0);
		usleep(100000); //.1 sec
	}

	if (initializeCyGl(&data.CyGl) != -1) {
		fprintf(stderr, "CyberGlove II initialized.\n");
		//blink GREEN led if CyberGlove did connect
		digitalWrite(GREEN_LED, 1);
		usleep(500000); //.5
		digitalWrite(GREEN_LED, 0);
		usleep(100000); //.1 sec
	} else {
		fprintf(stderr, "Couldn't initialize CyberGlove II.\n");
		//blink RED led if CyberGlove didn't connect
		digitalWrite(RED_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(RED_LED, 0);
		usleep(100000); //.1 sec
	}

	if (initializeForce(&data.Force) != -1) {
		fprintf(stderr, "Force sensors initialized.\n");
		//blink GREEN led if force sensors did connect
		digitalWrite(GREEN_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(GREEN_LED, 0);
		usleep(100000); //.1 sec
	} else {
		fprintf(stderr, "Couldn't initialize force sensors.\n");
		//blink RED led if force sensors didn't connect
		digitalWrite(RED_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(RED_LED, 0);
		usleep(100000); //.1 sec
	}

	data.EMG.id = -1;

	if (initializeEMG(&data.EMG) != -1) {
		fprintf(stderr, "EMG initialized.\n");
		//blink GREEN led if EMG did connect
		digitalWrite(GREEN_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(GREEN_LED, 0);
		usleep(500000); //.5 sec
	} else {
		fprintf(stderr, "Couldn't initialize EMG.\n");
		//blink RED led if EMG didn't connect
		digitalWrite(RED_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(RED_LED, 0);
		usleep(500000); //.5 sec
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

		data.controlValues[3] = 1; //start EMG

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

	if (data.EMG.id != -1) {
		while (data.controlValues[3] != 2) {}
		data.controlValues[3] = 1;
	}

}

void* IMUThread() {

	//make data collection thread a time critical thread
	setPriority();

	while (1 == 1) {
		pthread_mutex_lock(&threadLocks[0]);
		data.controlValues[0] = 2; //signals ready to accept a collection request
		pthread_cond_wait(&threadSignals[0], &threadLocks[0]);
		data.controlValues[0] = 0;
		getIMUData(&data.IMU, data.time);
		pthread_mutex_unlock(&threadLocks[0]);
	}
}

void* CyGlThread() {

	//make data collection thread a time critical thread
	setPriority();

	while (1 == 1) {
		pthread_mutex_lock(&threadLocks[1]);
		data.controlValues[1] = 2; //signals ready to accept a collection request
		pthread_cond_wait(&threadSignals[1], &threadLocks[1]);
		data.controlValues[1] = 0;
		getCyGlData(&data.CyGl, data.time);
		pthread_mutex_unlock(&threadLocks[1]);
	}
}

void* ForceThread() {

	//make data collection thread a time critical thread
	setPriority();

	while (1 == 1) {
		pthread_mutex_lock(&threadLocks[2]);
		data.controlValues[2] = 2; //signals ready to accept a collection request
		pthread_cond_wait(&threadSignals[2], &threadLocks[2]);
		data.controlValues[2] = 0;
		getForceData(&data.Force, data.time);
		pthread_mutex_unlock(&threadLocks[2]);
	}
}

void* EMGThread() {

	//make data collection thread a time critical thread
	setPriority();

	while (1 == 1) {
		if (data.controlValues[3] != 0) {
			//collect data
			getEMGData(&data.EMG, data.time);
			data.controlValues[3] = 2;
		}
	}
}

void checkSensors() {

	if (data.IMU.id != -1 && data.IMU.consecutiveErrors > 20) {
		//.5 sec of missed data
		//big error happening, try to reconnect to the IMU

		//turn on red LED
		digitalWrite(GREEN_LED, 0);
		digitalWrite(RED_LED, 1);
		fprintf(stderr, "ERROR: Too many consecutive missed reads.\n");
		fprintf(stderr, "ERROR: Trying to reconnect to IMU.\n");

		if (restartIMU(&data.IMU) != 1) {
			//couldn't reconnect, continue without IMU
			fprintf(stderr, "ERROR: Couldn't reconnect to IMU.\n");
			closeIMU(&data.IMU);
		} else {
			fprintf(stderr, "ERROR: Successfully reconnected to IMU.\n");
		}
		fprintf(stderr, "ERROR: Continuing data recording.\n");
	}

	if (data.CyGl.id != -1 && data.CyGl.consecutiveErrors > 20) {
		//.5 sec of missed data
		//big error happening, try to reconnect to the CyberGlove

		//turn on red LED
		digitalWrite(GREEN_LED, 0);
		digitalWrite(RED_LED, 1);
		fprintf(stderr, "ERROR: Too many consecutive missed reads.\n");
		fprintf(stderr, "ERROR: Trying to reconnect to CyberGlove.\n");

		if (restartCyGl(&data.CyGl) != 1) {
			//couldn't reconnect, continue without CyberGlove
			fprintf(stderr, "ERROR: Couldn't reconnect to CyberGlove.\n");
			closeCyGl(&data.CyGl);
		} else {
			fprintf(stderr, "ERROR: Successfully reconnected to CyberGlove.\n");
		}
		fprintf(stderr, "ERROR: Continuing data recording.\n");
	}

	if (data.Force.id != -1 && data.Force.consecutiveErrors > 20) {
		//.5 sec of missed data
		//big error happening, try to reconnect to the Force sensors

		//turn on red LED
		digitalWrite(GREEN_LED, 0);
		digitalWrite(RED_LED, 1);
		fprintf(stderr, "ERROR: Too many consecutive missed reads.\n");
		fprintf(stderr, "ERROR: Trying to reconnect to Force sensors.\n");

		if (restartForce(&data.Force) != 1) {
			//couldn't reconnect, just end the program here
			fprintf(stderr, "ERROR: Couldn't reconnect to Force sensors.\n");
			closeForce(&data.Force);
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

		//turn on red LED
		digitalWrite(GREEN_LED, 0);
		digitalWrite(RED_LED, 1);
		fprintf(stderr, "ERROR: Too many consecutive missed reads.\n");
		fprintf(stderr, "ERROR: Trying to reconnect to EMG.\n");

		if (restartEMG(&data.EMG) != 1) {
			//couldn't reconnect, just end the program here
			fprintf(stderr, "ERROR: Couldn't reconnect to EMG.\n");
			closeEMG(&data.EMG);
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

		//unused lines are labeled "SENSOR UNUSED"

		data.reads++;

		if (data.IMU.id != -1) {
			IMUError = updateIMURead(&data.IMU);
		} else {
			IMUError = 1;
		}
		if (data.CyGl.id != -1) {
			CyGlError = updateCyGlRead(&data.CyGl);
		} else {
			CyGlError = 1;
		}
		if (data.Force.id != -1) {
			ForceError = updateForceRead(&data.Force);
		} else {
			ForceError = 1;
		}
		if (data.EMG.id != -1) {
			EMGError = updateEMGRead(&data.EMG);
		} else {
			EMGError = 1;
		}

		if (IMUError != -1 && CyGlError != -1 && ForceError != -1 && EMGError != -1) {
			//no missed read
			fwrite("=", sizeof(char), 1, data.outFile);
		} else {
			//report missed read

			digitalWrite(GREEN_LED, 0); //turn on red LED due to a missed read
			digitalWrite(RED_LED, 1);

			data.errors++;
			fprintf(stderr, "*");
			fwrite("*", sizeof(char), 1, data.outFile);
		}

		//write time
		printf("%f\n", data.time);
		fwrite(&time, sizeof(double), 1, data.outFile);
		fwrite(" ", sizeof(char), 1, data.outFile);

		//IMU
		if (data.IMU.id != -1) {
			//IMU missed read flag
			if (IMUError == -1) {
				//this sensor had a missed read, mark it with an asterisk
				printf("*");
				fwrite("*", sizeof(char), 1, data.outFile);
			} else {
				//no missed read
				fwrite("=", sizeof(char), 1, data.outFile);
			}
			//save IMU data
			for (int i = 0; i < IMU_READ_SZ; i++) {
				printf("%f\t", data.IMU.read[i]);
			}
			fwrite(data.IMU.read, sizeof(float), sizeof(data.IMU.read)/sizeof(float), data.outFile);
		} else {
			printf("IMU UNUSED");
		}
		printf("\n");
		fwrite(" ", sizeof(char), 1, data.outFile);

		//CyberGlove
		if (data.CyGl.id != -1) {
			//CyGl missed read flag
			if (CyGlError == -1) {
				//this sensor had a missed read, mark it with an asterisk
				printf("*");
				fwrite("*", sizeof(char), 1, data.outFile);
			} else {
				//no missed read
				fwrite("=", sizeof(char), 1, data.outFile);
			}

			if (data.CyGl.WiredCyGl == 1) {
				for (int i = 0; i < WIRED_CYGL_READ_SZ; i++) {
					printf("%i\t", (int) data.CyGl.read[i]);
				}
				fwrite(data.CyGl.read, sizeof(int), WIRED_CYGL_READ_SZ, data.outFile);
			} else {
				for (int i = 0; i < WIRELESS_CYGL_READ_SZ; i++) {
					printf("%i\t", (int) data.CyGl.read[i]);
				}
				fwrite(data.CyGl.read, sizeof(int), WIRELESS_CYGL_READ_SZ, data.outFile);
			}

		} else {
			printf("CyGl UNUSED");
		}
		printf("\n");
		fwrite(" ", sizeof(char), 1, data.outFile);

		//Force Sensors
		if (data.Force.id != -1) {
			//Force missed read flag
			if (ForceError == -1) {
				//this sensor had a missed read, mark it with an asterisk
				printf("*");
				fwrite("*", sizeof(char), 1, data.outFile);
			} else {
				//no missed read
				fwrite("=", sizeof(char), 1, data.outFile);
			}
			//save force data
			for (int i = 0; i < FORCE_READ_SZ; i++) {
				printf("%f\t", data.Force.read[i]);
			}
			fwrite(data.Force.read, sizeof(int), sizeof(data.Force.read)/sizeof(int), data.outFile);
		} else {
			printf("Force UNUSED");
		}
		printf("\n");
		fwrite(" ", sizeof(char), 1, data.outFile);

		if (data.EMG.id != -1) {
			//EMG missed read flag
			if (EMGError == -1) {
				//this sensor had a missed read, mark it with an asterisk
				printf("*");
				fwrite("*", sizeof(char), 1, data.outFile);
			} else {
				//no missed read
				fwrite("=", sizeof(char), 1, data.outFile);
			}
			for (int i = 0; i < EMG_READS_PER_CYCLE; i++) {
				printf("Read %i:   ", i + 1);
				for (int j = 0; j < EMG_READ_SZ; j++) {
					printf("%f\t", data.EMG.read[i * EMG_READ_SZ + j]);
				}
				printf("\n");
			}
			printf("\n");
			fwrite(data.EMG.read, sizeof(float), sizeof(data.EMG.read)/sizeof(float), data.outFile);
		} else {
			printf("EMG UNUSED");
		}
		printf("\n");
		fwrite(" ", sizeof(char), 1, data.outFile);

		//terminate data collection period
		printf("\n");
		fwrite("\n", sizeof(char), 1, data.outFile);

		pthread_mutex_unlock(&threadLocks[4]);

		//save file every 60 seconds
		if ((int) data.time % 60 == 0) {
			fclose(data.outFile);
			data.outFile = fopen("/home/pi/Desktop/ArmTrack/ArmTrackData.bin", "ab");
		}
	}

	pthread_exit(NULL);
}

void endSession() {

	while (data.controlValues[4] != 2) {}; //wait for print thread to be done

	for (int i = 0; i < 5; i++) {
		pthread_cancel(threads[i]);
		pthread_mutex_destroy(&threadLocks[i]);
		pthread_cond_destroy(&threadSignals[i]);
	}

	//close and save file
	fclose(data.outFile);

	//upload file to DropBox (hold green and red LED on during upload)
	digitalWrite(GREEN_LED, 1); digitalWrite(RED_LED, 1);
	system("/home/pi/Dropbox-Uploader/dropbox_uploader.sh upload /home/pi/Desktop/ArmTrack/ArmTrackData.bin /");
	digitalWrite(GREEN_LED, 0); digitalWrite(RED_LED, 0);
	sleep(1);

	//report Error percentage
	double percentMissed = (data.errors /(double) data.reads) * 100;

	fprintf(stderr, "Elapsed Time (sec): %05.3f\tPercent Missed: %5.3f%%\n",
			data.time, percentMissed);

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
	closeIMU(&data.IMU);
	closeCyGl(&data.CyGl);
	closeForce(&data.Force);
	closeEMG(&data.EMG);

	fprintf(stderr, "Session Ended\n\n");

	//then turn off the raspberry pi on actual mobile program but not here so I can do repeated runs
	//system("sudo shutdown -h now");
}
