/**
 * Program to test the time it takes to get responses from
 * an IMU, Force, EMG, Wireless CyberGlove, Wired CyberGlove.
 *
 * Compile with:
 *		gcc -o responseTest responseTest.c IMU.c Force.c EMG.c CyGl.c -std=gnu99 -Wall -Wextra
 * Run with:
 *		./responseTest
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <sched.h>
#include <sys/mman.h>

//#include "IMU.h"
//#include "Force.h"
#include "CyGl.h"

void setPriority();

int main() {

	printf("Testing Response Time.\n");

//	IMU IMU;
//	if (startIMU(&IMU) == -1) {
//		printf("ERROR: Couldn't connect to IMU.\n");
//		exit(1);
//	} else {
//		printf("Connected to IMU.\n");
//	}
//
//	FILE* outFile = fopen("responseTime.txt", "w");
//	if (outFile == NULL) {
//		printf("ERROR: couldn't open text file.\n");
//		exit(1);
//	}
//
////	setPriority();
//
//	struct timeval start, end;
//
//	printf("Recording response times.\n");
//	for (int i = 0; i < 1000; i++) {
//		gettimeofday(&start, NULL);
//		getIMUData(&IMU, 0);
//		gettimeofday(&end, NULL);
//
//		double time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) * .000001;
//		fprintf(outFile, "%f\n", time);
//	}
//	printf("Done recording response times.\n");
//	closeIMU(&IMU);

//	Force Force;
//	if (initializeForce(&Force) == -1) {
//		printf("ERROR: Couldn't connect to Force sensors.\n");
//		exit(1);
//	}
//
//	FILE* outFile = fopen("responseTime.txt", "w");
//
//	struct timeval start, end;
//
//	for (int i = 0; i < 1000; i++) {
//		gettimeofday(&start, NULL);
//		getForceData(&Force, 0);
//		gettimeofday(&end, NULL);
//
//		fprintf(outFile, "%f\n", (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) * .000001);
//	}
//
//	closeForce(&Force);

	CyGl CyGl;
	if (startCyGl(&CyGl) != 1) {
		fprintf(stderr, "ERROR: Couldn't connect to CyberGlove II.\n");
		exit(1);
	} else {
		printf("Connected to CyGl.\n");
	}

	FILE* outFile = fopen("responseTime.txt", "w");

	struct timeval start, end;

	for (int i = 0; i < 1000; i++) {

		fprintf(stderr, "%i\n", i);

		gettimeofday(&start, NULL);
		getCyGlData(&CyGl, 0);
		gettimeofday(&end, NULL);

		fprintf(outFile, "%f\n", (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) * .000001);
	}

	closeCyGl(&CyGl);

	fclose(outFile);
	return 0;
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
