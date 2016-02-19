/*
 * Implements Myo-specific communication protocol
 */

#include <stdio.h> //stderr,
#include <fcntl.h> //O_RDWR

void connect() {

	//mimic python constructor
	char device[] = "tty???"; //name of Myo bluetooth dongle

	int Myo; //Myo device handler id

	if ((Myo = open(device, O_RDWR)) == -1) {
		fprintf(stderr, "Error: Failed to connect to Myo Bluetooth Dongle.\n");
		exit(0);
	}

	/*
	 * Then the python thing instantiates the BT class,
	 * EMG handlers, imu_handelrs, arm_handlers, and pose handlers
	 * I think...
	 */

	/* looking at the method that connects the bluetooth dongle
	to the Myo */

	/*
	 * 1. Ends previous scans and connections
	 * 2. Calls BT.discover method
	 * 3. Then repeatedly reads in bluetooth response packets using
	 * 		the recv_packet() method
	 * 		if it ends with something it does something complicated
	 * 		on line 212 to get the address.
	 * 4. Then it calls bluetooth connect() method on the address
	 * 5. Sets self.conn to the muiltiord(conn_pakt.payload)
	 * 6. Then it calls bluetooth method wait_event()
	 * 7. Then it figures out the firmware version by calling
	 * 		read_attr method in Myo_raw
	 * 8. Then if the firmware is the same as 0? we:
	 * 		call write_attr method with a lot of info, the first of which isn't necessary
	 *
	 * 		then we enable EMG data with the write_attr
	 * 		and enable IMU data with write_attr
	 *
	 * 		then we set the underlying EMG sensors sampling rate and
	 * 		i think how fast it actually tells us stuff and the low-pass
	 * 		filtering rate of the EMG data
	 *
	 * 		also we set the rate at which IMU sends back data I think
	 *
	 * 		Make sure to actual send this then via write_attr
	 *
	 * 	  Otherwise we:
	 * 	  	only enable the IMU data
	 * 	  	maybe this is related to how they started allowing access
	 * 	  	to raw EMG data after a firmware update?
	 *
	 */
}
