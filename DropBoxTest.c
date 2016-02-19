/* Name: mobileTest.c
 * Author: Elijah Pivo
 *
 * Usage:
 * 	Compile with: gcc -o DropBoxTest DropBoxTest.c -std=gnu99 -Wall -Wextra
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {

	//create File
	FILE* outFile = fopen("/home/pi/Desktop/ArmTrack/ArmTrackData.txt", "w");
	fprintf(outFile, "FLIM FLAM!");
	fclose(outFile);

	//ensure we are connected to the Internet
	sleep(40);

	//ping google.com
	system("ping www.google.com -c 1 > /home/pi/Desktop/ArmTrack/Internet.txt");

	//upload file to DropBox
	system("/home/pi/Dropbox-Uploader/dropbox_uploader.sh upload /home/pi/Desktop/ArmTrack/ArmTrackData.txt / > report.txt");

	return 0;
}
