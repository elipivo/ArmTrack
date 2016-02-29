/*
 * Name: readEMG.c
 * Author: Elijah Pivo
 *
 * Description:
 * 	Reads data from the EMG and
 * 	prints it to the screen.
 *
 * Usage:
 * 	Compile with:
 *		gcc -std=gnu99 -pthread -g -Wall -I. -o readEMG readEMG.c EMG.c -L. -lmccusb  -lm -L/usr/local/lib -lhidapi-libusb -lusb-1.0
 *
 *
 * 	Start with ./readEMG, end program with ctrl-d
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



#include "EMG.h"

//global data structure
typedef struct {
	EMG EMG;
	double time;

	/* Controls print thread:
	 * 1) print read
	 * 0) don't print
	 * -1) end print thread
	 */
	int print;
} Data;

void* printThread();
void endSession();

Data data;

pthread_t printThreadID;
pthread_mutex_t printLock;
pthread_cond_t printSignal;

int main(void) {

	fprintf(stderr, "Reading EMG\n");

	fprintf(stderr, "Here 1\n");

	//make stdin non blocking
	int flags = fcntl(fileno(stdin), F_GETFL, 0);
	flags |= O_NONBLOCK;
	flags = fcntl(fileno(stdin), F_SETFL, flags);
	char userInput;

	fprintf(stderr, "Here 2\n");

	if (startEMG(&data.EMG) != 1) {
		printf("readEMG Error: Couldn't connect to EMG.\n");
		exit(1);
	}

	fprintf(stderr, "Here 3\n");

	pthread_mutex_init(&printLock, NULL);
	pthread_cond_init(&printSignal, NULL);
	data.print = 0;
	if (pthread_create(&printThreadID, NULL, printThread, NULL) != 0) {
		fprintf(stderr, "Couldn't create print thread...\n");
		exit(1);
	}

	fprintf(stderr, "Here 4\n");

	struct timeval last;
	struct timeval curr;
	struct timeval temp;

	fprintf(stderr, "Here 5\n");

	gettimeofday(&curr, NULL); //update current time

	while(read(fileno(stdin), &userInput, 1) < 0) {

		last.tv_sec = curr.tv_sec; last.tv_usec = curr.tv_usec; //update last time
		gettimeofday(&curr, NULL); //update current time
		data.time += (curr.tv_sec - last.tv_sec) + (curr.tv_usec - last.tv_usec) * .000001; //increment by difference between last and current time

		fprintf(stderr, "Here 6\n");

		getEMGData(&data.EMG, data.time);

		fprintf(stderr, "Here 7\n");

		while (data.print != 2) {}; //wait for print thread to be ready
		pthread_mutex_lock(&printLock);
		data.print = 1;
		pthread_cond_signal(&printSignal);
		pthread_mutex_unlock(&printLock);

		if (data.EMG.consecutiveErrors > 20) {
			//.5 sec of missed data
			//big error happening, try to reconnect to the EMG

			fprintf(stderr, "EMG ERROR: Too many consecutive missed reads.\n");
			fprintf(stderr, "EMG ERROR: Trying to reconnect to EMG.\n");

			if (restartEMG(&data.EMG) != 1) {
				//couldn't reconnect, just end the program here

				fprintf(stderr, "EMG ERROR: Couldn't reconnect to EMG.\n");
				fprintf(stderr, "EMG ERROR: Ending recording session.\n");

				endSession();
				return 0;
			}

			fprintf(stderr, "EMG ERROR: Successfully reconnected to EMG.\n");
			fprintf(stderr, "EMG ERROR: Continuing data recording.\n");
		}

		do {
			gettimeofday(&temp, NULL);
		} while ( (temp.tv_sec - curr.tv_sec) + (temp.tv_usec - curr.tv_usec) * .000001 < .024993);

	}

	endSession();
	return 1;
}

void endSession() {

	pthread_cancel(printThreadID);
	pthread_mutex_destroy(&printLock);
	pthread_cond_destroy(&printSignal);

	usleep(25000); //ensure prints complete

	double percentMissed = (data.EMG.errors/ (double) data.EMG.reads) * 100;

	fprintf(stderr, "Elapsed Time (sec): %05.3f\tPercent Missed: %5.3f%%\n",
			data.EMG.readTime, percentMissed);

	closeEMG(&data.EMG);

	fprintf(stderr, "Session Ended\n\n");
}

void* printThread() {

	/*
	 * Format:
	 * 		Time1 <Channels of EMG Data>
	 * 		Time2 <Channels of EMG Data>
	 * 		...
	 */

	while (1 == 1) {

		pthread_mutex_lock(&printLock);
		data.print = 2;
		pthread_cond_wait(&printSignal, &printLock);
		data.print = 0;

		if (updateEMGRead(&data.EMG) == -1) {
			printf("*");
		}

		for (int i = 0; i < EMG_READS_PER_CYCLE; i++) {
			printf("Read %i:   ", i + 1);
			for (int j = 0; j < EMG_READ_SZ; j++) {
				printf("%fV\t", data.EMG.read[i * EMG_READ_SZ + j]);
			}
			printf("\n");
		}
		printf("\n");

		pthread_mutex_unlock(&printLock);
	}
	pthread_exit(NULL);
}
