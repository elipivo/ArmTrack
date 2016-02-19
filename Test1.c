/*
 * Name: Test1.c
 * Author: Elijah Pivo
 *
 * Description:
 * 	Tests timing architectures for fast reaction devices.
 * 	(IMU chain, Force Sensors, and CyberGlove 2)
 *
 * Usage:
 * 	Compile with: gcc -o Test1 Test1.c TestDevice.c -std=gnu99 -Wall -Wextra
 * 	Start with ./Test1, end program with ctrl-d
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
#include <sys/types.h>

#include "sTestDevice.h"

typedef struct {
	TestDevice TestDevice;
	double time;
} Data;

void printData(Data* data);
void setMode(int signum);
int startTestDevice(TestDevice* TestDevice);
int restartTestDevice(TestDevice* TestDevice);
void startTimer();
void endSession(Data* data);

volatile int mode = 0;

jmp_buf return_to_loop;

int main(void) {

	fprintf(stderr, "Reading Test Device\n");

	//make stdin non blocking
	int flags = fcntl(fileno(stdin), F_GETFL, 0);
	flags |= O_NONBLOCK;
	flags = fcntl(fileno(stdin), F_SETFL, flags);
	char userInput;

	Data data;

	if (startTestDevice(&data.TestDevice) != 1) {
		fprintf(stderr, "Test Error: Couldn't start Test Device.\n");
		exit(1);
	}

	/**
	 * to  record print times
	 */

	FILE* outFile = fopen("300print.txt", "w");

	sigset_t z;
	sigemptyset(&z);
	sigaddset(&z, SIGALRM);

	struct timeval last;
	struct timeval curr;
	gettimeofday(&curr, NULL); //update current time
	data.time = -.025;

	startTimer();

	//location we jump to after mode has been set
	if (setjmp(return_to_loop)!= 0) {
		sigprocmask(SIG_UNBLOCK, &z, NULL); //need to unblock alarm upon return
	}

	while(read(fileno(stdin), &userInput, 1) < 0) {
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
			if (getTestDeviceData(&data.TestDevice) == -1) {

				mode = 2; //deal with error

			} else {

				struct timeval start;
				struct timeval end;
				gettimeofday(&start, NULL); //update current time

				sigprocmask(SIG_BLOCK, &z, NULL); //block alarm now
				printData(&data);
				mode = 0;
				sigprocmask(SIG_UNBLOCK, &z, NULL); //unblock alarm signal

				gettimeofday(&end, NULL);

				fprintf(outFile, "%li\n", end.tv_usec - start.tv_usec);

			}

		} else if (mode == 2) {

			//data collection wasn't completed
			sigprocmask(SIG_BLOCK, &z, NULL); //block alarm signal
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

				mode = 0;

			} else {

				mode = 1;

			}

			sigprocmask(SIG_UNBLOCK, &z, NULL); //unblock alarm signal
		}
	}

	endSession(&data);

	return 0;
}

void endSession(Data* data) {
	//first block alarm signal
	sigset_t z;
	sigemptyset(&z);
	sigaddset(&z, SIGALRM);
	sigprocmask(SIG_BLOCK, &z, NULL);

	double percentMissed = (data->TestDevice.errors / (double) data->TestDevice.reads) * 100;

	fprintf(stderr, "Elapsed Time (sec): %05.3f\tPercent Missed: %5.3f%%\n",
			data->time, percentMissed);

	closeTestDevice(&data->TestDevice);

	fprintf(stderr, "Session Ended\n\n");
}

void printData(Data* data) {

	/*
	 * Format:
	 * 		Time1 -Channels of Test Data-
	 * 		Time2 -Channels of Test Data-
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

void setMode(int signum) {

	/* Modes:
       mode 0) Waiting to read data
       mode 1) Collecting data
       mode 2) ERROR: read wasn't completed
	 */

	if (mode == 0) { //was waiting to record data
		mode = 1; //record data
	} else if (mode == 1) { //was recording data
		mode = 2; //report a corrupted data set and then record next data
	}

	longjmp(return_to_loop, 1);

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
	timer.it_value.tv_usec = 40000; //expires in 40 milliseconds, 40000

	timer.it_interval.tv_sec = 0; //not using seconds
	timer.it_interval.tv_usec = 40000; //and repeats forever, 40000

	setitimer(ITIMER_REAL, &timer,  NULL);

}

int startTestDevice(TestDevice* TestDevice) {

	//try to initialize 4 times, waiting 4 sec between attempt

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

