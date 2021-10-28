#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <unistd.h>

#include <sys/neutrino.h>
#include "../../des_controller/src/des.h"

// Server to Controller
int main(void) {
	// channel id for Controller (client) connection
	int chid;
	// channel id for receive message from Controller
	int rcvid;
	// Display object declare
	Display display;
	//Phase I: Open connection to client
	// Channel to client establishment
	chid = ChannelCreate(0);
	if (chid == -1) {
		fprintf(stderr, "ERROR: Failed to create channel in des_display\n");
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	printf("The display is running as PID %d\n", getpid());

	// Phase II: Receive message from Controller
	while (1) {
		rcvid = MsgReceive(chid, &display, sizeof(display), NULL);
		// while true
		if (rcvid == -1) {
			printf("\nERROR: Failed to receive message from Controller \n");
			exit(EXIT_FAILURE);
		}
		// exit message to terminate
		if (strcmp(display.msg, "exit") == 0) {
			exit(EXIT_FAILURE);
		}
		printf("%s\n", display.msg); //display message
		MsgReply(rcvid, EOK, NULL, 0);
	}
	// Phase III: Close connection to client controller
	ChannelDestroy(chid);
	return EXIT_SUCCESS;
}
