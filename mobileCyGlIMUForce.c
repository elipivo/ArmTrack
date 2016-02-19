/* Name: mobileCyGlIMUForce.c
 * Author: Elijah Pivo
 *
 * Description:
 * 	Reads data from IMU, CyGl, and Force sensors and prints and stores it after a switch
 * 	is flipped. Employs mobile program procedure described below with computer
 * 	displays for testing.
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
#include <setjmp.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <pthread.h>
#include "wiringPi.h"
#include "wiringSerial.h"
#include "IMU.h"
#include "CyGl.h"
#include "Force.h"

typedef struct {
	IMU IMU;
	CyGl CyGl;
	Force Force;

	int reads;
	int errors;
	double time;
	FILE* outFile;
} Data;

#define NUM_SENSORS 3

#define GREEN_LED 29
#define RED_LED 28
#define SWITCH 27

void printSaveData(Data* data);
int getData(Data* data);
void setMode(int signum);
void startTimer();
void startSensors(Data* data);
int restartIMU(IMU* IMU); int restartCyGl(CyGl* CyGl);
int restartForce(Force* Force);
void endSession(Data* data);

pthread_t threads[NUM_SENSORS];

volatile int mode = 0;

jmp_buf return_to_loop;

int main(void) {

	fprintf(stderr, "Beginning Mobile Test\n");

	//initialize wiringPi and then setup pins
	if (wiringPiSetup() != 0) {
		exit(0);
	}

	pinMode(GREEN_LED, OUTPUT);
	pinMode(RED_LED, OUTPUT);

	pinMode(SWITCH, INPUT);
	pullUpDnControl(SWITCH, PUD_UP);

	fprintf(stderr, "Connecting to sensors.\n");

	Data data;

	//repeatedly initialize until switch is flipped on
	while (digitalRead(SWITCH) == 0) {

		//flash both LED's at start of each initialization cycle
		digitalWrite(GREEN_LED, 1);
		digitalWrite(RED_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(GREEN_LED, 0);
		digitalWrite(RED_LED, 0);
		usleep(500000); //.5 sec

		startSensors(&data);

		sleep(2); //wait two seconds between start cycles
	}

	fprintf(stderr, "Collecting data.\n");
	digitalWrite(GREEN_LED, 1); //turn on green LED while recording data

	sigset_t z; //set of signals containing just the timer alarm
	sigemptyset(&z);
	sigaddset(&z, SIGALRM);

	data.outFile = fopen("/home/pi/Desktop/ArmTrack/ArmTrackData.bin", "wb");
	data.errors = 0;
	data.reads = 0;

	struct timeval last;
	struct timeval curr;
	gettimeofday(&curr, NULL); //update current time
	data.time = -.025;

	startTimer();

	if (setjmp(return_to_loop)!= 0) {
		sigprocmask(SIG_UNBLOCK, &z, NULL); //where we return to after timer goes off
	}

	while(digitalRead(SWITCH) == 1) {
		/* Modes:
		 * mode 0) Waiting to read data
		 * mode 1) Collecting data
		 * mode 2) ERROR: read wasn't completed
		 */

		if (mode == 1) {

			last.tv_sec = curr.tv_sec; last.tv_usec = curr.tv_usec; //update last time
			gettimeofday(&curr, NULL); //update current time
			data.time += (curr.tv_sec - last.tv_sec) + (curr.tv_usec - last.tv_usec) * .000001; //increment by difference between last and current time

			//collect data
			if (getData(&data) == -1) {

				mode = 2; //deal with error

			} else {

				//process data
				sigprocmask(SIG_BLOCK, &z, NULL); //block timer signal
				printSaveData(&data, 0);
				if ((int) data.time % 60 == 0) {
					//save text file every 60 seconds
					fclose(data.outFile);
					data.outFile = fopen("/home/pi/Desktop/ArmTrack/ArmTrackData.bin", "ab");
				}

				mode = 0;
				digitalWrite(RED_LED, 0);
				digitalWrite(GREEN_LED, 1);

				sigprocmask(SIG_UNBLOCK, &z, NULL); //unblock timer signal
			}

		} else if (mode == 2) {

			//error
			sigprocmask(SIG_BLOCK, &z, NULL); //block timer signal

			//turn RED led on and GREEN led off while handling error
			digitalWrite(RED_LED, 1);
			digitalWrite(GREEN_LED, 0);

			//store most recent data
			printSaveData(&data, 1); //store the most recent data
			data.errors++;

			mode = 1;

			//check for sustained errors and try to correct any
			if (data.IMU.id != -1 && data.IMU.consecutiveErrors > 20) {
				fprintf(stderr, "ERROR: Sustained IMU error, reconnecting... ");
				//reconnect to IMU
				if (restartIMU(&data.IMU) == -1) {
					fprintf(stderr, "couldn't reconnect to IMU chain.\n");
				}
				fprintf(stderr, "reconnected to IMU chain.\n");

				mode = 0;
			}
			if (data.CyGl.id != -1 && data.CyGl.consecutiveErrors > 20) {
				fprintf(stderr, "ERROR: Sustained CyberGlove error, reconnecting... ");
				//reconnect to CyGl
				if (restartCyGl(&data.CyGl) == -1) {
					fprintf(stderr, "couldn't reconnect to CyberGlove.\n");
				}
				fprintf(stderr, "reconnected to CyberGlove.\n");

				mode = 0;
			}
			if (data.Force.id != -1 && data.Force.consecutiveErrors > 20) {
				fprintf(stderr, "ERROR: Sustained Force sensors error, reconnecting... ");
				//reconnect to Force
				if (restartForce(&Force) == -1) {
					fprintf(stderr, "couldn't reconnect to Force sensors.\n");
				}
				fprintf(stderr, "reconnected to Force sensors.\n");

				mode = 0;
			}

			sigprocmask(SIG_UNBLOCK, &z, NULL); //unblock signal
		}
	}

	endSession(&data);

	return 0;
}

void endSession(Data* data) {

	//first block alarm signal
	sigset_t z; //set of signals containing just the timer alarm
	sigemptyset(&z);
	sigaddset(&z, SIGALRM);
	sigprocmask(SIG_BLOCK, &z, NULL);

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

int getData(Data* data) {

	data->reads++;

	void *status;

	//for each of the connected sensors, create a pthread and wait for them to complete
	//when they do return

	if (data->IMU.id != -1) {
		//create a pthread for IMU
		pthread_create(&threads[0], NULL, pGetIMUData, (void *) &data->IMU);
	}

	if (data->CyGl.id != -1) {
		//create a pthread for CyberGlove
		pthread_create(&threads[1], NULL, pGetCyGlData, (void *) &data->CyGl);
	}

	if (data->Force.id != -1) {
		//create a pthread for Force sensors
		pthread_create(&threads[2], NULL, pGetForceData, (void *) &data->Force);
	}

	if (data->IMU.id != -1) {
		//wait for IMU to finish
		pthread_join(threads[0], &status);
	}

	if (data->CyGl.id != -1) {
		//wait for CyberGlove to finish
		pthread_join(threads[1], &status);
	}

	if (data->Force.id != -1) {
		//wait for Force sensors to finish
		pthread_join(threads[2], &status);
	}

	return 1;

}

void printSaveData(Data* data, int error) {

	/* Prints:
	 *
	 * TIME
	 * IMU READ
	 * CyGl READ
	 * Force READ
	 *
	 * TIME
	 * IMU READ
	 * CYGL READ
	 * Force READ
	 *
	 * ...
	 */

	//unused lines are labeled "SENSOR UNUSED"

	if (error == 1) {
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
		if (updateIMURead(&data->IMU) == -1) {
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
		if (updateCyGlRead(&data->CyGl) == -1) {
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
		if (updateForceRead(&data->Force) == -1) {
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

	//terminate data collection period
	fprintf(stderr, "\n");
	fwrite('\n', sizeof(char), 1, data->outFile);

}

void setMode(int signum) {

	/* Modes:
       mode 0) Waiting to read data
       mode 1) Collecting data
       mode 2) ERROR: either read or save wasn't completed
	 */

	for (int i = 0; i < NUM_SENSORS; i++) {
		pthread_cancel(threads[i]);
	}

	if (mode == 0) {
		mode = 1;
	} else if (mode == 1) {
		mode = 2;
	}

	longjmp(return_to_loop, 1);

}

void startSensors(Data* data) {

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

void startTimer() {

	//necessary timer variables
	struct itimerval timer;
	struct sigaction sa;

	//Set Up and Start Timer
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &setMode;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGALRM, &sa, NULL);

	timer.it_value.tv_sec = 0; //not using seconds
	timer.it_value.tv_usec = 25000; //expires in 25 milliseconds, 25000

	timer.it_interval.tv_sec = 0; //not using seconds
	timer.it_interval.tv_usec = 25000; //and repeats forever, 25000

	setitimer(ITIMER_REAL, &timer,  NULL);

}

int restartIMU(IMU* IMU) {

	//try to reconnect 4 times, waiting 4 sec between attempt
	for (int attempt = 0; reconnectIMU(IMU) == -1 && attempt < 4; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
		sleep(4); //try again after 4 seconds
	}

	return 1;
}

int restartCyGl(CyGl* CyGl) {

	//try to reconnect 4 times, waiting 4 sec between attempt
	for (int attempt = 0; reconnectCyGl(CyGl) == -1 && attempt < 4; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
		sleep(4); //try again after 4 seconds
	}

	return 1;
}

int restartForce(Force* Force) {
	//try to reconnect 4 times, waiting 4 sec between attempt

	for (int attempt = 0; reconnectForce(Force) == -1 && attempt < 4; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
		sleep(4); //try again after 4 seconds
	}

	return 1;
}
