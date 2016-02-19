/*
 * Name: Force.c
 * Author: Elijah Pivo
 *
 * Force sensors interface
 */

#include "Force.h"

int initializeForce(Force* Force) {

	Force->id = -1;
	Force->hasNewRead1 = 0;
	Force->hasNewRead2 = 0;
	Force->bufferToUse = 2;
	for (int i = 0; i < FORCE_READ_SZ; i++) {
		Force->readBuffer1[i] = 0;
		Force->readBuffer2[i] = 0;
		Force->read[i] = 0;
	}
	Force->reads = 0;
	Force->errors = 0;
	Force->consecutiveErrors = 0;

	const char device[] = "/dev/i2c-1";
	const int FORCE_SENSOR_ID = 0x48;

	int tempID = -1;

	//open I2C devices
	if ((tempID = open(device, O_RDWR)) == -1) {
		fprintf(stderr, "Force Error: Couldn't open I2C.\n");
		return -1;
	}

	// connect to ads1115 (force sensor ADC)
	if (ioctl(tempID, I2C_SLAVE, FORCE_SENSOR_ID) < 0) {
		fprintf(stderr, "Force Error: Couldn't find ADS1115 device address.\n");
		return -1;
	}

	Force->id = tempID;

	return Force->id;
}

int reconnectForce(Force* Force) {

	//close
	tcflush(Force->id, TCIOFLUSH);
	close(Force->id);

	//reconnect
	Force->id = -1;
	Force->hasNewRead1 = 0;
	Force->hasNewRead2 = 0;
	Force->bufferToUse = 2;
	for (int i = 0; i < FORCE_READ_SZ; i++) {
		Force->readBuffer1[i] = 0;
		Force->readBuffer2[i] = 0;
		Force->read[i] = 0;
	}
	Force->consecutiveErrors = 0;

	const char device[] = "/dev/i2c-1";
	const int FORCE_SENSOR_ID = 0x48;

	int tempID = -1;

	//open I2C devices
	if ((tempID = open(device, O_RDWR)) == -1) {
		fprintf(stderr, "Force Error: Couldn't open I2C.\n");
		return -1;
	}

	// connect to ads1115 (force sensor ADC)
	if (ioctl(tempID, I2C_SLAVE, FORCE_SENSOR_ID) < 0) {
		fprintf(stderr, "Force Error: Couldn't find ADS1115 device address.\n");
		return -1;
	}

	Force->id = tempID;

	return Force->id;
}

int startForce(Force* Force) {

	//try to reconnect 4 times, waiting 4 sec between attempt

	for (int attempt = 0; initializeForce(Force) == -1; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
		sleep(4); //try again after 4 seconds
	}

	return 1;
}

int restartForce(Force* Force) {

	//try to reconnect 4 times, waiting 4 sec between attempt

	for (int attempt = 0; reconnectForce(Force) == -1; attempt++) {
		if (attempt == 3) {
			return -1; //4th attempt failed, give up
		}
		sleep(4); //try again after 4 seconds
	}

	return 1;
}

int getForceData(Force* Force, double time) {

	Force->reads++;

	uint8_t writeBuf[3]; //Buffer to store the 3 bytes we write to the I2C device
	uint8_t readBuf[2]; //Buffer to store the data bytes read from the force sensor
	int16_t val; //store composite data from force sensors

	switch (Force->reads % 2) {
	case 0:
		//read into readBuffer1 on even reads
		Force->readBuffer1Time = time;

		for (int i = 0; i < FORCE_READ_SZ; i++) {

			writeBuf[0] = 1; //set pointer register so that the following two bytes write to the config register

			/*
			 * 1|AAA|000|1
			 * 1: Starts a conversion
			 * AAA: Selects the force sensor
			 * 000: Sets gain to cover 6.144V (all of 5V source)
			 * 1: Sets single-shot mode
			 */

			switch (i) {
			case 0: writeBuf[1] = 0xC1; //Sets 8 MSBs of config register to 1|100|000|1
			break;
			case 1: writeBuf[1] = 0xD1; //Sets 8 MSBs of config register to 1|101|000|1
			break;
			case 2: writeBuf[1] = 0xE1; //Sets 8 MSBs of config register to 1|110|000|1
			break;
			case 3: writeBuf[1] = 0xF1; //Sets 8 MSBs of config register to 1|111|000|1
			break;
			default:
				exit(0);
				break;
			}

			/*
			 * 101|0|0|0|11
			 * 101: Set sampling speed to 1/250 SPS or ~4ms per conversion
			 * 0: doesn't matter for us since comparator is disabled
			 * 0: doesn't matter for us since comparator is disabled
			 * 0: doesn't matter for us since comparator is disabled
			 * 11: disables the comparator
			 */

			writeBuf[2] = 0xA3; //Sets 8 LSBs of config register to 101|0|0|0|11

			//initialize the buffer used to read data from ADS1115 to 0
			readBuf[0] = 0;
			readBuf[1] = 0;

			//write buffer to the ADS1115 to begin single conversion
			if (write(Force->id, writeBuf, 3) != 3) {
				return -1;
			}

			//Wait for the conversion to complete
			//we repeatedly read the config buffer and wait for bit 15 to change from 0->1

			while ((readBuf[0] & 0x80) == 0) { //readBuf[0] contains 8 MSBs of config register, AND with 1000|0000 to select bit 15

				if (read(Force->id, readBuf, 2) != 2) {
					return -1;
				}
			}

			//conversion completed!

			writeBuf[0] = 0; //set pointer register to 0 to read from the conversion register
			if (write(Force->id, writeBuf, 1) != 1) {
				return -1;
			}

			if (read(Force->id, readBuf, 2) != 2) { //read the contents of the conversion register into readBuf
				return -1;
			}

			val = readBuf[0] << 8 | readBuf[1]; //combine the two bytes of readBuf into a single 16 bit result

			Force->readBuffer1[i] = (float) val * 6.144 / 32768.0; //convert the result to V and store it in our read array
		}

		Force->hasNewRead1 = 1;
		return 1;

		break;
	case 1:
		//read into readBuffer2 on odd reads
		Force->readBuffer2Time = time;

		for (int i = 0; i < FORCE_READ_SZ; i++) {

			writeBuf[0] = 1; //set pointer register so that the following two bytes write to the config register

			/*
			 * 1|AAA|000|1
			 * 1: Starts a conversion
			 * AAA: Selects the force sensor
			 * 000: Sets gain to cover 6.144V (all of 5V source)
			 * 1: Sets single-shot mode
			 */

			switch (i) {
			case 0: writeBuf[1] = 0xC1; //Sets 8 MSBs of config register to 1|100|000|1
			break;
			case 1: writeBuf[1] = 0xD1; //Sets 8 MSBs of config register to 1|101|000|1
			break;
			case 2: writeBuf[1] = 0xE1; //Sets 8 MSBs of config register to 1|110|000|1
			break;
			case 3: writeBuf[1] = 0xF1; //Sets 8 MSBs of config register to 1|111|000|1
			break;
			default:
				exit(0);
				break;
			}

			/*
			 * 101|0|0|0|11
			 * 101: Set sampling speed to 1/250 SPS or ~4ms per conversion
			 * 0: doesn't matter for us since comparator is disabled
			 * 0: doesn't matter for us since comparator is disabled
			 * 0: doesn't matter for us since comparator is disabled
			 * 11: disables the comparator
			 */

			writeBuf[2] = 0xA3; //Sets 8 LSBs of config register to 101|0|0|0|11

			//initialize the buffer used to read data from ADS1115 to 0
			readBuf[0] = 0;
			readBuf[1] = 0;

			//write buffer to the ADS1115 to begin single conversion
			if (write(Force->id, writeBuf, 3) != 3) {
				return -1;
			}

			//Wait for the conversion to complete
			//we repeatedly read the config buffer and wait for bit 15 to change from 0->1

			while ((readBuf[0] & 0x80) == 0) { //readBuf[0] contains 8 MSBs of config register, AND with 1000|0000 to select bit 15

				if (read(Force->id, readBuf, 2) != 2) {
					return -1;
				}
			}

			//conversion completed!

			writeBuf[0] = 0; //set pointer register to 0 to read from the conversion register
			if (write(Force->id, writeBuf, 1) != 1) {
				return -1;
			}

			if (read(Force->id, readBuf, 2) != 2) { //read the contents of the conversion register into readBuf
				return -1;
			}

			val = readBuf[0] << 8 | readBuf[1]; //combine the two bytes of readBuf into a single 16 bit result

			Force->readBuffer2[i] = (float) val * 6.144 / 32768.0; //convert the result to V and store it in our read array
		}

		Force->hasNewRead2 = 1;
		return 1;
		break;
	}

	return -1;
}

int updateForceRead(Force* Force) {

	switch (Force->bufferToUse) {
	case 1:
		Force->bufferToUse = 2;
		Force->readTime = Force->readBuffer1Time;
		if (Force->hasNewRead1 == 1) {
			//new data available
			memcpy(&Force->read, &Force->readBuffer1, FORCE_READ_SZ * sizeof(float)); //update read
			Force->consecutiveErrors = 0; //data collection must have been successful
			Force->hasNewRead1 = 0;
			return 1;
		}
		break;
	case 2:
		Force->bufferToUse = 1;
		Force->readTime = Force->readBuffer2Time;
		if (Force->hasNewRead2 == 1) {
			//new data available
			memcpy(&Force->read, &Force->readBuffer2, FORCE_READ_SZ * sizeof(float)); //update read
			Force->consecutiveErrors = 0; //data collection must have been successful
			Force->hasNewRead2 = 0;
			return 1;
		}
		break;
	}

	//no new data available, data collection must have been unsuccessful
	Force->errors++;
	Force->consecutiveErrors++;
	return -1;
}

void closeForce(Force* Force) {

	tcflush(Force->id, TCIOFLUSH);
	close(Force->id);

	Force->id = -1;
	Force->hasNewRead1 = 0;
	Force->hasNewRead2 = 0;
	for (int i = 0; i < FORCE_READ_SZ; i++) {
		Force->readBuffer1[i] = 0;
		Force->readBuffer2[i] = 0;
		Force->read[i] = 0;
	}
	Force->reads = 0;
	Force->errors = 0;
	Force->consecutiveErrors = 0;
}
