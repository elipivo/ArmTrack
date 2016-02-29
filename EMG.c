/*
 * Name: EMG.c
 * Author: Elijah Pivo
 *
 * EMG MCC-DAQ USB1408FS interface
 */

#include "EMG.h"

int initializeEMG(EMG* EMG) {

	EMG->id = -1;
	EMG->udev = NULL;
	EMG->hasNewRead1 = 0;
	EMG->hasNewRead2 = 0;
	EMG->bufferToUse = 2;
	for (int i = 0; i < EMG_READ_SZ; i++) {
		EMG->readBuffer1[i] = 0;
		EMG->readBuffer2[i] = 0;
		EMG->read[i] = 0;
	}
	EMG->reads = 0;
	EMG->errors = 0;
	EMG->consecutiveErrors = 0;

	fprintf(stderr, "Here a\n");

	if (libusb_init(NULL) < 0) {
		fprintf(stderr, "ERROR: Failed to initialize libusb.\n");
		return 1;
	}

	fprintf(stderr, "Here b\n");

	if (!(EMG->udev = usb_device_find_USB_MCC(USB1408FS_PID, NULL))) {
		fprintf(stderr, "No device found.\n");
		return -1;
	}

	fprintf(stderr, "Here c\n");

	// claim all the needed interfaces for AInScan
	for (int i = 1; i <= 3; i++) {
		int ret = libusb_detach_kernel_driver(EMG->udev, i);
		if (ret < 0) {
			fprintf(stderr, "Can't detach kernel from interface");
			usbReset_USB1408FS(EMG->udev);
			return -1;
		}
		ret = libusb_claim_interface(EMG->udev, i);
		if (ret < 0) {
			fprintf(stderr, "Can't claim interface.");
			return -1;
		}
	}

	fprintf(stderr, "Here d\n");

	usbDConfigPort_USB1408FS(EMG->udev, DIO_PORTA, DIO_DIR_OUT);
	usbDConfigPort_USB1408FS(EMG->udev, DIO_PORTB, DIO_DIR_IN);
	usbDOut_USB1408FS(EMG->udev, DIO_PORTA, 0);
	usbDOut_USB1408FS(EMG->udev, DIO_PORTA, 0);

	EMG->id = 1;

	return EMG->id;
}

int reconnectEMG(EMG* EMG) {

	//save info we want to save
	int e = EMG->errors;
	int r = EMG->reads;

	//close the device
	closeEMG(EMG);

	//restart the device
	initializeEMG(EMG);

	EMG->errors = e;
	EMG->reads = r;
	return EMG->id;
}

int startEMG(EMG* EMG) {
	//try to reconnect 4 times, waiting 4 sec between attempt
	for (int attempt = 0; initializeEMG(EMG) == -1; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
		sleep(4); //try again after 4 seconds
	}
	return 1;
}

int restartEMG(EMG* EMG) {
	//try to reconnect 4 times, waiting 4 sec between attempt
	for (int attempt = 0; reconnectEMG(EMG) == -1; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
		sleep(4); //try again after 4 seconds
	}
	return 1;
}

int getEMGData(EMG* EMG, double time) {
	struct timeval start, end;
	gettimeofday(&start, NULL);

	float freq = 8000;
	int count = EMG_READS_PER_CYCLE * EMG_READ_SZ;
	uint8_t options = AIN_EXECUTION | AIN_GAIN_QUEUE;

	EMG->reads++;

	switch (EMG->reads % 2) {
	case 0:
		//read into readBuffer1 on even reads
		EMG->readBuffer1Time = time;

		usbAInStop_USB1408FS(EMG->udev);

		if (usbAInScan_USB1408FS_SE(EMG->udev, 0, 0, count, &freq, options, EMG->readBuffer1)
				!= count) { //need to check error handling here

			//ensure method takes precisely 25ms even if error occurs
			gettimeofday(&end, NULL);
			while ((end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) * .000001 <= .025) {
				gettimeofday(&end, NULL);
			}

			return -1;
		}

		EMG->hasNewRead1 = 1;
		break;
	case 1:
		//read into readBuffer2 on odd reads
		EMG->readBuffer2Time = time;

		usbAInStop_USB1408FS(EMG->udev);

		if (usbAInScan_USB1408FS_SE(EMG->udev, 0, 0, count, &freq, options, EMG->readBuffer2)
				!= count) { //need to check error handling here

			//ensure method takes precisely 25ms even if error occurs
			gettimeofday(&end, NULL);
			while ((end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) * .000001 <= .025) {
				gettimeofday(&end, NULL);
			}

			return -1;
		}

		EMG->hasNewRead2 = 1;
		break;
	}

	return 1;

}

int updateEMGRead(EMG* EMG) {

	switch(EMG->bufferToUse) {
	case 1:
		EMG->bufferToUse = 2;
		EMG->readTime = EMG->readBuffer1Time;
		if (EMG->hasNewRead1 == 1) {
			//New data available
			//convert to voltage (float) for read data
			for (int i = 0; i < EMG_READ_SZ * EMG_READS_PER_CYCLE; i++) {
				EMG->read[i] = volts_1408FS_SE(EMG->readBuffer1[i]);
			}
			EMG->consecutiveErrors = 0;
			EMG->hasNewRead1 = 0;
			return 1;
		}
		break;
	case 2:
		EMG->bufferToUse = 1;
		EMG->readTime = EMG->readBuffer2Time;
		if (EMG->hasNewRead2 == 1) {
			//new data available
			//convert to voltage (float) for read data
			for (int i = 0; i < EMG_READ_SZ * EMG_READS_PER_CYCLE; i++) {
				EMG->read[i] = volts_1408FS_SE(EMG->readBuffer2[i]);
			}
			EMG->consecutiveErrors = 0;
			EMG->hasNewRead2 = 0;
			return 1;
		}
		break;
	}

	//no new data available, data collection must not have been completed
	EMG->errors++;
	EMG->consecutiveErrors++;

	return -1;
}

void closeEMG(EMG* EMG) {

	if (EMG->id != -1) {
		libusb_clear_halt(EMG->udev, LIBUSB_ENDPOINT_IN | 1);
		libusb_clear_halt(EMG->udev, LIBUSB_ENDPOINT_OUT| 2);
		libusb_clear_halt(EMG->udev, LIBUSB_ENDPOINT_IN | 3);
		libusb_clear_halt(EMG->udev, LIBUSB_ENDPOINT_IN | 4);

		for (int i = 0; i <= 4; i++) {
			libusb_release_interface(EMG->udev, i);
		}
		libusb_close(EMG->udev);
	}

	EMG->id = -1;
	EMG->udev = NULL;
	EMG->hasNewRead1 = 0;
	EMG->hasNewRead2 = 0;
	EMG->bufferToUse = 2;
	for (int i = 0; i < EMG_READ_SZ; i++) {
		EMG->readBuffer1[i] = 0;
		EMG->readBuffer2[i] = 0;
		EMG->read[i] = 0;
	}
	EMG->reads = 0;
	EMG->errors = 0;
	EMG->consecutiveErrors = 0;

}
