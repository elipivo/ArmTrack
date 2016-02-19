/* Name: mobileArmTrack.c
 * Author: Elijah Pivo
 * Date: 10/30/15
 *
 * Description:
 * 	Stores data from connected sensors to
 * 	a textfile after a switch is flipped on.
 *
 * Usage:
 * 	Compile with: gcc -o mobileArmTrack mobileArmTrack.c IMU.c CyGl.c Force.c EMG.c -lwiringPi -pthread -std=gnu99 -Wall -Wextra
 *	Run with: sudo ./mobileArmTrack
 *
 * 	Starts and stops recording data when a switch is flipped.
 *
 *  Successful Run:
 * 	1) The program will repeatedly attempt to initialize the sensors.
 * 	   The LEDs will flash 5 times per initialization cycle.
 * 	   At the beginning of the cycle, both LED's will blink at the same time.
 * 	   The second flash represents whether or not the IMU is connected.
 * 	   The third flash represents whether or not the Cyber Glove 2 is connected.
 * 	   The fourth flash represents whether or not the Force sensor is connected.
 * 	   The fifth flash represents whether or not the EMG sensor is connected.
 * 	2) Once the LED's show all desired sensors are connected,
 * 	   flip the switch to start recording data.
 * 	3) During successful data recording, GREEN LED will remain on and
 * 	   data will be printed to the screen.
 * 	4) At the end of recording the GREEN LED will flash once followed
 * 	   by the GREEN and RED LEDs flashing once for every percentage
 * 	   of reads that were missed. Then the GREEN LED will flash once.
 * 	5) Then the pi will then turn off.
 *
 *	Errors:
 * 	1) A missed read will cause the RED LED to flash once.
 * 	2) A fatal program failure will cause the RED LED to flash quickly
 * 	   for one minute and then shut the Pi down.
 *
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

#include "Force.h"
#include "CyGl.h"
#include "IMU.h"
#include "EMG.h"

#include "wiringPi.h"
#include "wiringSerial.h"

#define NUM_SENSORS 4

#define GREEN_LED 29
#define RED_LED 28
#define SWITCH 27

void getData(struct IMU* IMU, struct CyGl* CyGl, struct Force* Force, struct EMG* EMG);
void updateReads(struct IMU* IMU, struct CyGl* CyGl, struct Force* Force, struct EMG* EMG);
void saveData(FILE* outFile, double time, struct IMU IMU, struct CyGl CyGl, struct Force Force, struct EMG EMG);
void setMode(int signum);
void startTimer();
void startSensors(struct IMU* IMU, struct CyGl* CyGl, struct Force* Force, struct EMG* EMG);
void fatalError();

pthread_t threads[NUM_SENSORS];

volatile int mode = 0;

jmp_buf return_to_loop;

int main(void) {

	fprintf(stderr, "Beginning Mobile Test\n");

	fprintf(stderr, "Initializing.\n");
	//initialize wiringPi and then setup pins
	if (wiringPiSetup() != 0) {
		exit(0);
	}

	pinMode(GREEN_LED, OUTPUT);
	pinMode(RED_LED, OUTPUT);

	pinMode(SWITCH, INPUT);
	pullUpDnControl(SWITCH, PUD_UP);

	struct IMU IMU;
	struct CyGl CyGl;
	struct Force Force;
	struct EMG EMG;

	//repeatedly initialize until switch is flipped on
	while (digitalRead(SWITCH) == 0) {

		//flash both LED's at start of each initialization cycle
		digitalWrite(GREEN_LED, 1);
		digitalWrite(RED_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(GREEN_LED, 0);
		digitalWrite(RED_LED, 0);
		usleep(500000); //.5 sec

		startSensors(&IMU, &CyGl, &Force, &EMG);

		sleep(2);
	}

	FILE *outFile = fopen("/home/pi/Desktop/ArmTrack/ArmTrackData.txt", "w");

	fprintf(stderr, "Collecting data... \n");
	//turn on green LED while recording data
	digitalWrite(GREEN_LED, 1);

	startTimer();

	double time = -.025;
	int errors = 0;

	sigset_t z;
	sigemptyset(&z);
	sigaddset(&z, SIGALRM);

	if (setjmp(return_to_loop)!= 0) {
		sigprocmask(SIG_UNBLOCK, &z, NULL);
	}

	while(digitalRead(SWITCH) == 1) {
		/* Modes:
       mode 0) Waiting to read data
       mode 1) Collecting data
       mode 2) ERROR: read wasn't completed
		 */

		if (mode == 1) {

			time += .025; //update time first so that this is never missed

			//collect data
			getData(&IMU, &CyGl, &Force, &EMG);

			//block alarm now
			sigprocmask(SIG_BLOCK, &z, NULL);

			updateReads(&IMU, &CyGl, &Force, &EMG);

			mode = 0;

			//save data to a file
			saveData(outFile, time, IMU, CyGl, Force, EMG);

			digitalWrite(RED_LED, 0);
			digitalWrite(GREEN_LED, 1);

			//unblock alarm signal
			sigprocmask(SIG_UNBLOCK, &z, NULL);

		} else if (mode == 2) {
			//turn RED led on and GREEN led off while handling error

			digitalWrite(RED_LED, 1);
			digitalWrite(GREEN_LED, 0);

			//data collection wasn't completed for at least one sensor
			fprintf(outFile, "*");
			saveData(outFile, time, IMU, CyGl, Force, EMG);

			errors++;
			mode = 1;

		}
	}

	//first block alarm signal
	sigprocmask(SIG_BLOCK, &z, NULL);

	closeIMU(&IMU);
	closeCyGl(&CyGl);
	closeForce(&Force);
	closeEMG(&EMG);
	fclose(outFile);

	double percentMissed = (errors /(time / .025)) * 100;

	fprintf(stderr, "Elapsed Time (sec): %05.3f\tPercent Missed: %5.3f%%\n",
			time, percentMissed);

	fprintf(stderr, "Session Ended\n\n");

	//blink green once
	//then blink both green and red once for each percent missed
	//then blink just green once again and end the program

	digitalWrite(RED_LED, 0);
	digitalWrite(GREEN_LED, 0);
	sleep(1);
	digitalWrite(GREEN_LED, 1);
	sleep(1);
	digitalWrite(GREEN_LED, 0);
	sleep(1);

	for (int i = 0; i < percentMissed; i++) {
		digitalWrite(GREEN_LED, 1);
		digitalWrite(RED_LED, 1);
		usleep(250000); //.25 sec
		digitalWrite(GREEN_LED, 0);
		digitalWrite(RED_LED, 0);
		usleep(250000); //.25 sec
	}

	digitalWrite(GREEN_LED, 1);
	sleep(1);
	digitalWrite(GREEN_LED, 0);
	sleep(1);
	digitalWrite(RED_LED, 0);
	digitalWrite(GREEN_LED, 0);

	//then turn off the raspberry pi on actual mobile program but not here so i can do repeated runs
	//system("sudo shutdown -h now");

	return 0;
}

void getData(struct IMU* IMU, struct CyGl* CyGl, struct Force* Force, struct EMG* EMG) {

	void *status;

	//for each of the connected sensors, create a pthread and wait for them to complete
	//when they do return

	if (IMU->id != -1) {
		//create a pthread for IMU
		pthread_create(&threads[0], NULL, pGetIMUData, (void *) IMU);
	}

	if (CyGl->id != -1) {
		//create a pthread for CyberGlove
		pthread_create(&threads[1], NULL, pGetCyGlData, (void *) CyGl);
	}

	if (Force->id != -1) {
		//create a pthread for Force sensors

		pthread_create(&threads[2], NULL, pGetForceData, (void *) Force);
	}

	if (EMG->id != -1) {
		//create a pthread for EMG
		pthread_create(&threads[3], NULL, pGetEMGData, (void *) EMG);
	}

	if (IMU->id != -1) {
		//wait for IMU to finish
		pthread_join(threads[0], &status);
	}

	if (CyGl->id != -1) {
		//wait for CyberGlove to finish
		pthread_join(threads[1], &status);
	}

	if (Force->id != -1) {
		//wait for Force sensors to finish
		pthread_join(threads[2], &status);
	}

	if (EMG->id != -1) {
		//wait for EMG to finish
		pthread_join(threads[3], &status);
	}

}

void saveData(FILE* outFile, double time, struct IMU IMU, struct CyGl CyGl, struct Force Force, struct EMG EMG) {

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

	fprintf(outFile, "%05.3f\n", time);

	if (IMU.id != -1) {
		for (int i = 0; i < IMU_READ_SZ; i++) {
			fprintf(outFile, "%03.2f\t", IMU.read[i]);
		}
	}
	fprintf(outFile, "\n");

	if (CyGl.id != -1) {
		for (int i = 0; i < CYGL_READ_SZ; i++) {
			fprintf(outFile, "%i\t", CyGl.read[i]);
		}
	}
	fprintf(outFile, "\n");

	if (Force.id != -1) {
		for (int i = 0; i < FORCE_READ_SZ; i++) {
				fprintf(outFile, "%06.6f\t", Force.read[i]);
			}
	}
	fprintf(outFile, "\n");

	if (EMG.id != -1) {
		//need to implement
	}
	fprintf(outFile, "\n");

	fprintf(outFile, "\n");

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

void startSensors(struct IMU* IMU, struct CyGl* CyGl, struct Force* Force, struct EMG* EMG) {

	closeIMU(IMU);
	closeCyGl(CyGl);
	closeForce(Force);
	closeEMG(EMG);

	if (initializeIMU(IMU) != -1) {
		fprintf(stderr, "IMU initialized.\n");
		//blink GREEN led if IMU did connect
		digitalWrite(GREEN_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(GREEN_LED, 0);
	} else {
		fprintf(stderr, "Couldn't initalize IMU.\n");
		//blink RED led if IMU didn't connect
		digitalWrite(RED_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(RED_LED, 0);
	}

	if (initializeCyGl(CyGl) != -1) {
		fprintf(stderr, "CyberGlove II initialized.\n");
		//blink GREEN led if CyberGlove did connect
		digitalWrite(GREEN_LED, 1);
		usleep(500000); //.5
		digitalWrite(GREEN_LED, 0);
		usleep(500000); //.5 sec
	} else {
		fprintf(stderr, "Couldn't initalize CyberGlove II.\n");
		//blink RED led if CyberGlove didn't connect
		digitalWrite(RED_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(RED_LED, 0);
		usleep(500000); //.5 sec
	}

	if (initializeForce(Force) != -1) {
		fprintf(stderr, "Force sensors initialized.\n");
		//blink GREEN led if force sensors did connect
		digitalWrite(GREEN_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(GREEN_LED, 0);
		usleep(500000); //.5 sec
	} else {
		fprintf(stderr, "Couldn't initalize force sensors.\n");
		//blink RED led if force sensors didn't connect
		digitalWrite(RED_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(RED_LED, 0);
		usleep(500000); //.5 sec
	}

	if (initializeEMG(EMG) != -1) {
		fprintf(stderr, "EMG initialized.\n");
		//blink GREEN led if EMG did connect
		digitalWrite(GREEN_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(GREEN_LED, 0);
		usleep(500000); //.5 sec
	} else {
		fprintf(stderr, "Couldn't initalize EMG.\n");
		//blink RED led if EMG didn't connect
		digitalWrite(RED_LED, 1);
		usleep(500000); //.5 sec
		digitalWrite(RED_LED, 0);
		usleep(500000); //.5 sec
	}
}

/*
 * After a successful read, update last read.
 */
void updateReads(struct IMU* IMU, struct CyGl* CyGl, struct Force* Force, struct EMG* EMG) {

	if (IMU->id != -1) {
		updateIMURead(IMU);
	}
	if (CyGl->id != -1) {
		updateCyGlRead(CyGl);
	}
	if (Force->id != -1) {
		updateForceRead(Force);
	}
	if (EMG->id != -1) {
		updateEMGRead(EMG);
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


void fatalError() {

	//block the timer signal
	sigset_t z;
	sigemptyset(&z);
	sigaddset(&z, SIGALRM);
	sigprocmask(SIG_BLOCK, &z, NULL);

	fprintf(stderr, "FATAL ERROR\n");

	//flash red led quickly (4 times a second) for 60 seconds, then turn off the raspberry pi
	digitalWrite(GREEN_LED, 0);

	for (int i = 0; i < 240; i++) {
		digitalWrite(RED_LED, 1);
		usleep(125000);
		digitalWrite(RED_LED, 0);
		usleep(125000);
	}

	//then turn off the raspberry pi in actual mobile program, but not here so I can do repeated runs
	system("sudo shutdown -h now");
}
