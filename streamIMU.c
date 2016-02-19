#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#define BAUD 115200

int main() {

	//make stdin non blocking
	int flags = fcntl(fileno(stdin), F_GETFL, 0);
	flags |= O_NONBLOCK;
	flags = fcntl(fileno(stdin), F_SETFL, flags);
	char userInput;

	const char device[] = "/dev/ttyACM0";
	int ID = 0;

	if ((ID = open(device, O_RDWR | O_NOCTTY | O_DSYNC)) == -1) {
		return -1;
	}

	// Set BAUD rate
	struct termios options;
	tcgetattr(ID, &options);
	cfmakeraw(&options);
	cfsetispeed(&options, BAUD);
	cfsetospeed(&options, BAUD);
	tcsetattr(ID, TCSANOW, &options);

	while (read(ID, NULL, 1) == 1) {}
	if (write(ID, "s", 1) != 1) {
		return -1;
	}

	float buffer[sizeof(float) * 12];

	while (read(fileno(stdin), &userInput, 1) < 0) {

		//read stop byte
		read(ID, &buffer, 12 * sizeof(float));
		if (read(ID, NULL, sizeof(unsigned char)) != sizeof(unsigned char)) {
			printf("ERROR\n");
			return -1;
		}

		for (int i = 0; i < 12; i++) {
			printf("%f\t", buffer[i]);
		}
		printf("\n");

//		exit(1);
	}

	//request reading
	write(ID, "h", 1);

	return 0;
}
