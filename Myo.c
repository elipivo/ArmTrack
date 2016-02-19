/*
 * Myo interface
 *
 * Author: Elijah Pivo
 * Date: 11/7/2015
 */


#include "Myo.h"
#include "myohw.h"

/*
 * Sets up the connection to a Myo.
 * Ensures its ready to read from.
 * @param a pointer to a Myo struct so it can update the id
 * @return -1 if failed, otherwise new value for id
 */
int initializeMyo(struct Myo* Myo) {
	int status;
	const char device[] = "\dev\ttyACM0";

	for (int i = 0; i < MYO_READ_SZ; i++) {
		Myo->read[i] = 0;
	}
	Myo->id = -1;

	if ((Myo->id = open(device, O_RDWR)) == -1) {
		fprintf(stderr, "Myo Error: Failed to connect.\n");
		Myo->id = -1;
		return -1;
	}

	//try to connect
	fprintf(stderr, "Get list of devices.");
	uint8_t writeBuf[5];
	writeBuf[0] = 0x00;
	writeBuf[1] = 0x01;
	writeBuf[2] = 6;
	writeBuf[3] = 2;
	writeBuf[4] = 0x01;
	write(Myo->id, &writeBuf, 5);

	uint8_t readBuf[15];
	for (int i = 0; i < 15; i++) {
		readBuf[i] = 0x00;
	}

	read(Myo->id, &readBuf, 15);
	for (int i = 0; i < 15; i++) {
		fprintf(stderr, "*%d*\n", (int) readBuf[i]);
	}

	/*

	//test I can cause a vibration for now

	fprintf(stderr, "Sending vibrate.\n");
	//make a vibration command and then write it directly, see if that works
	myohw_command_vibrate_t vibrateCommand;
	vibrateCommand.header.command = myohw_command_set_mode;
	vibrateCommand.header.payload_size = 1;
	vibrateCommand.type = myohw_vibration_long;
	write(Myo->id, vibrateCommand, sizeof(vibrateCommand)); //not sure if sizeof() works here

	/*
	//other method writing Bytes directly starting with the characteristic we
	//we want to right to, then the command, then the type of vibration.
	uint8_t writeBuf[5];
	writeBuf[0] = CommandCharacteristic >> 8; //not sure this is right
	writeBuf[1] = CommandCharacteristic; //not sure this is right
	writeBuf[2] = myohw_command_vibrate;
	writeBuf[3] = myohw_vibration_long;
	write(Myo->id, writeBuf, 4);
	 */
	//test I get a response


}

/*
 * Reads data from a Myo.
 * Error messages will be reported to stderr.
 * @param takes a pointer to a Myo struct so it can update the read array
 * @return whether or not the read succeeded (-1 if it failed)
 */
int getMyoData(struct Myo* Myo) {
	//request reading

	//get the reading

	return 1;
}

/*
 * For use in a pthread. Reads data from a Myo.
 * Error messages will be reported to stderr.
 * @param takes a pointer to a Myo struct so it can update the read array
 * @return status
 */
void* pGetMyoData(void *threadarg) {

	struct Myo* Myo = (struct Myo*) threadarg;

	if (getMyoData(Myo) == -1) {
		//error occured so don't let this thread complete

		while (1 == 1) {}
	}

	pthread_exit(NULL);
}

/*
 * Updates the most recent read.
 */
void updateMyoRead(struct Myo* Myo) {

	if (Myo->hasNewRead == 1) {
		memcpy(&Myo->read, &Myo->readBuffer, MYO_READ_SZ * sizeof(int));
		Myo->hasNewRead = 0;
	}

}

/*
 * Will end a session with a Myo device.
 * @param takes a pointer to a Myo struct
 */
void closeMyo(struct Myo* Myo) {

	tcflush(Myo->id, TCIOFLUSH);
	close(Myo->id);
	Myo->id = -1;

}
