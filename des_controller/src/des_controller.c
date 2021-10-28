#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <string.h>

#include <sys/neutrino.h>
#include <sys/netmgr.h>
#include "des.h"

void *Start();
void *leftLock();
void *rightLock();
void *leftUnlock();
void *rightUnlock();
void *doorOpened();
void *weighScale();
void *leftcloseState();
void *rightCloseState();

// State function pointer
typedef void *(*StateFunc)();
// Display object declare
Display display;
// check weigh variable
int isWeighed;

int main(int argc, char* argv[]) {
	// inputs channel ID
	int inputChid;
	// display channel ID
	int displayChid;
	// channel id for receive message
	int rcvid;
	pid_t spid;
	spid = atoi(argv[1]);
	// Person object declare
	Person person;
	//Function pointer for Start state
	StateFunc stateptr = Start;
	//Get display's PID from command-line arguments
	if (argc != 2) {
		fprintf(stderr, "Missing PID to display\n");
		exit(EXIT_FAILURE);
	}
	// Phase I: Create channel for inputs process to attach and attach to display's channel
	inputChid = ChannelCreate(0);
	//Call ChannelCreate() to create a channel for the inputs process to attach
	if (inputChid == -1) {
		fprintf(stderr, "ERROR: Failed to create channel in des_controller\n");
		exit(EXIT_FAILURE);
	}
	//connect to input server
	displayChid = ConnectAttach(ND_LOCAL_NODE, spid, 1, _NTO_SIDE_CHANNEL, 0);
	if (displayChid == -1) {
		fprintf(stderr, "ERROR: Failed to connect to display\n");
		exit(EXIT_FAILURE);
	}
	printf("The controller is running as PID: %d\n", getpid());
	printf("Waiting for Person...\n");
	// Phase II: Receive person object from inputs, send EOK back to inputs

	while (1) {
		rcvid = MsgReceive(inputChid, &person, sizeof(person), NULL);
		if (strcmp(person.msg, inMessage[EXIT]) == 0) {
			printf("Exiting controller.\n");
			strcpy(display.msg, "exit");
			MsgSend(displayChid, &display, sizeof(display), NULL, 0);
			break;
		}
		// Get input event from Person object and advance state machine to next accepting state (or error state)
		stateptr = (StateFunc) (*stateptr)(&person);
		MsgSend(displayChid, &display, sizeof(display), NULL, 0);
		MsgReply(rcvid, EOK, NULL, 0);
	}
	// Phase III: Detach display's channel and close connection to inputs
	ConnectDetach(displayChid);
	ChannelDestroy(inputChid);

	return EXIT_SUCCESS;
}

// start state
void *Start(Person *per) {
	if (strcmp(per->msg, inMessage[LS]) == 0) {
		strcpy(display.msg, outMessage[LEFT_SCAN]);
		strcat(display.msg, per->id);
		return leftLock; // next state
	}
	if (strcmp(per->msg, inMessage[RS]) == 0) {
		strcpy(display.msg, outMessage[RIGHT_SCAN]);
		strcat(display.msg, per->id);
		return rightLock; // next state
	} else {
		strcpy(display.msg, errorMessage[SCAN]); // error state
		return Start;
	}
}

// left lock state
void *leftLock(Person *per) {
	if (per->direction == OUTBOUND && (strcmp(per->msg, inMessage[GLU]) == 0)
			&& (isWeighed)) {
		strcpy(display.msg, outMessage[GUARD_LEFT_UNLOCK]);
		return leftUnlock; // next state
	}
	if (per->direction == INBOUND && (strcmp(per->msg, inMessage[GLU]) == 0)
			&& (!isWeighed)) {
		strcpy(display.msg, outMessage[GUARD_LEFT_UNLOCK]);
		return leftUnlock; // next state
	} else {
		strcpy(display.msg, errorMessage[UNLOCK]); // error state
		return leftLock;
	}
}
// right lock state
void *rightLock(Person *per) {
	if (per->direction == INBOUND && (strcmp(per->msg, inMessage[GRU]) == 0)
			&& (isWeighed)) {
		strcpy(display.msg, outMessage[GUARD_RIGHT_UNLOCK]);
		return rightUnlock; // next state
	} else if (per->direction == OUTBOUND
			&& (strcmp(per->msg, inMessage[GRU]) == 0) && (!isWeighed)) {
		strcpy(display.msg, outMessage[GUARD_RIGHT_UNLOCK]);
		return rightUnlock; // next state
	} else {
		//"ERROR: Must right unlock next "
		strcpy(display.msg, errorMessage[UNLOCK]); // error state
		return rightLock;
	}
}

// left unlock state
void *leftUnlock(Person *per) {
	if (per->direction == OUTBOUND && (strcmp(per->msg, inMessage[LO]) == 0)
			&& (isWeighed)) {
		strcpy(display.msg, outMessage[LEFT_OPEN]);
		return doorOpened; // next state
	}
	if (per->direction == INBOUND && (strcmp(per->msg, inMessage[LO]) == 0)
			&& (!isWeighed)) {
		strcpy(display.msg, outMessage[LEFT_OPEN]);
		return doorOpened; // next state
	} else {
		strcpy(display.msg, errorMessage[OPEN]); // error state
		return leftUnlock;
	}
}
// right unlock state
void *rightUnlock(Person* per) {
	if (per->direction == INBOUND && (strcmp(per->msg, inMessage[RO]) == 0)
			&& (isWeighed)) {
		strcpy(display.msg, outMessage[RIGHT_OPEN]);
		return doorOpened; // next state
	} else if (per->direction == OUTBOUND
			&& (strcmp(per->msg, inMessage[RO]) == 0) && (!isWeighed)) {
		strcpy(display.msg, outMessage[RIGHT_OPEN]);
		return doorOpened; // next state
	} else {
		strcpy(display.msg, errorMessage[OPEN]); // error state
		return rightUnlock;
	}
}

// Door Opened State
void *doorOpened(Person *per) {
	if ((strcmp(per->msg, inMessage[WS]) == 0) && (!isWeighed)) {
		isWeighed = 1;
		strcpy(display.msg, outMessage[WEIGHT_SCALE]);
		char weight[100];
		itoa(per->weight, weight, 10);
		strcat(display.msg, weight);
		return weighScale; // next state
	} else if (per->direction == INBOUND
			&& (strcmp(per->msg, inMessage[RC]) == 0) && (isWeighed)) {
		strcpy(display.msg, outMessage[RIGHT_CLOSED]);
		return rightCloseState; // next state
	} else if (per->direction == OUTBOUND
			&& (strcmp(per->msg, inMessage[LC]) == 0) && (isWeighed)) {
		strcpy(display.msg, outMessage[LEFT_CLOSED]);
		return leftcloseState; // next state
		//
	} else {
		strcpy(display.msg,
				"ERROR: Must do weight scaling OR closing door next"); // error state
		return doorOpened;
	}
}

// Weigh state
void *weighScale(Person *per) {
	if (per->direction == INBOUND && (strcmp(per->msg, inMessage[LC]) == 0)) {
		strcpy(display.msg, outMessage[LEFT_CLOSED]);
		return leftcloseState; // next state
	} else if (per->direction == OUTBOUND
			&& (strcmp(per->msg, inMessage[RC]) == 0)) {
		strcpy(display.msg, outMessage[RIGHT_CLOSED]);
		return rightCloseState; // next state
	} else {
		strcpy(display.msg, errorMessage[CLOSE]); // error state
		return weighScale;
	}
}

// Left Close State
void *leftcloseState(Person *per) {
	if (per->direction == INBOUND && (strcmp(per->msg, inMessage[GLL]) == 0)) {
		strcpy(display.msg, outMessage[GUARD_LEFT_LOCK]);
		return rightLock; // next state
	} else if (per->direction == OUTBOUND
			&& (strcmp(per->msg, inMessage[GLL]) == 0)) {
		strcpy(display.msg, outMessage[GUARD_LEFT_LOCK]);
		strcat(display.msg, "\nWaiting for Person...");
		isWeighed = 0;
		return Start; // next state
	} else {
		strcpy(display.msg, errorMessage[LOCK]); //error state
		return leftcloseState;
	}
}

// Right Close State
void *rightCloseState(Person *per) {
	if (per->direction == OUTBOUND && (strcmp(per->msg, inMessage[GRL]) == 0)) {
		strcpy(display.msg, outMessage[GUARD_RIGHT_LOCK]);
		return leftLock; // next state
	} else if (per->direction == INBOUND
			&& (strcmp(per->msg, inMessage[GRL]) == 0)) {
		strcpy(display.msg, outMessage[GUARD_RIGHT_LOCK]);
		strcat(display.msg, "\nWaiting for Person...");
		isWeighed = 0;
		return Start; // next state
	} else {
		strcpy(display.msg, errorMessage[LOCK]); // error state
		return rightCloseState;
	}
}
