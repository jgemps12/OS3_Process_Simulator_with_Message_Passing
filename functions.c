#include "functions.h"
#include <sys/wait.h>
#include <stdio.h>


/************************************************PROGRAM INITIALIZATION************************************************/
void initializeLogfile() {
	logOutputFP = fopen(logfile, "w");
	
	if (logOutputFP == NULL) {
		printf("ERROR in oss.c: cannot create a log file named '%s'", logfile);
		
		exit(-1);
	}
}

void initializeMessageQueue() {
	if ((key = ftok(logfile, 1)) == -1) {
		printf("ERROR in oss.c: problem with ftok() function.\n");
		printf("Cannot access a key for message queue initialization.\n\n");
		
		exit(-1);
	}
	
	if ((messageQueueID = msgget(key, PERMISSIONS | IPC_CREAT)) == -1) {
		printf("ERROR in oss.c: problem with msgget() function.\n");
		printf("Cannot acquire a message queue ID for initialization.\n\n");
		
		exit(-1);
	}
	printf("Message queue is now set up.\n\n");
}

/************************************************USER INPUT VALIDATION*************************************************/
// Displays error messages based on 'optarg' arguments.
void checkForOptargEntryError(int value, char getoptArgument[]) {
	if ((value <= 0 || value > 10)  && (strcmp(getoptArgument, "-n [proc]") == 0)) {
		printf("ERROR in oss.c: User must enter an integer between 1 and 10 for argument %s.\n\n", getoptArgument);
		
		exit(-1);
	}
	if ((value <= 0 || value > 1000) && (strcmp(getoptArgument, "-i [intervalInMSToLaunchChildren]") == 0)) {
		printf("ERROR in oss.c: User must enter an integer between 1 and 1000 for argument %s.\n\n", getoptArgument);
		
		exit(-1);
	}
	if ((strcmp(getoptArgument, "-n [proc]") != 0) || (strcmp(getoptArgument, "-i [intervalInMSToLaunchChildren]") != 0)) {
		if (value <= 0) {
			printf("ERROR in oss.c: User must enter a positive integer for argument %s.\n\n", getoptArgument);
			
			exit(-1);
		}
	}
}

// Displays error message if # of simlataneous processes exceeds the total process count.
void checkForSimulExceedsProcError(int simulProcesses, int totalProcesses) {
	if (simulProcesses > totalProcesses) {
		printf("ERROR in oss.c: The -s [simul] value '%d' cannot be greater than the -n [proc] value '%d'.\n\n", simulProcesses, totalProcesses);
		
		exit(-1);
	}
}

/***********************************************SYSTEM CLOCK OPERATIONS************************************************/
// Adjust system time's seconds and nanoseconds. System time is 250 ms DIVIDED BY number of running processes.
long long int incrementClock(int *seconds, long long int *nanoseconds, int activeChildrenCount) {
	long long int increment;
	long long int remainder;
	
	static long long int remainderValue = 0;
	
	if (activeChildrenCount > 0) {
		increment = oneQuarterSecond / activeChildrenCount;
		remainder = oneQuarterSecond % activeChildrenCount;
	}
	else {
		increment = oneQuarterSecond;
		remainder = 0;
	}
	
	(*nanoseconds) += increment;
	remainderValue += remainder;
	
	if (activeChildrenCount > 0 && remainderValue >= activeChildrenCount) {
		*nanoseconds += 1;
		remainderValue -= activeChildrenCount;
	}
	if (*nanoseconds >= oneBillionNanoseconds) {
		*nanoseconds = 0;
		(*seconds)++;
	}
	systemNanoOnly = convertSystemTimeToNanosecondsOnly(seconds, nanoseconds);
	
	return increment;
}

// Generates a value between 1 and [timeLimitForChildren].
int randomizeChildSecondsLimit(int childTimeLimit) {
	srand(time(NULL) ^ getpid());
	
	return (rand() % childTimeLimit) + 1;
}

// Generates a random number of nanoseconds.
// Prevents nanoseconds from being nonzero when random # of seconds equals user-inputted maximum.
long int randomizeChildNanoseconds(int childTimeLimitSecs, int randomSeconds) {
	if (childTimeLimitSecs == randomSeconds) {
		return 0;
	}
	else {
		srand(time(NULL) ^ getpid());
		return (rand() % (oneBillionNanoseconds + 1));
	}
}

// TOTAL nanoseconds used to determine when to launch the next process.
long long int convertSystemTimeToNanosecondsOnly (int *seconds, long long int *nanoseconds) {
	long long int nanosecondsWithoutSecs = *nanoseconds;
	int i;
	for (i = 1; i <= (*seconds); i++) {
		nanosecondsWithoutSecs += oneBillionNanoseconds;
	}
	
	return nanosecondsWithoutSecs;
}

// Only deals with system nanoseconds to determine next launch time. 
long long int determineNextLaunchNanoseconds (int intervalMS, long long int currentNanoTime) {
	long long int nanoConversion = (long long int)intervalMS * 1000000;
	long double newLaunchTime = (long double) currentNanoTime + nanoConversion;
	
	return newLaunchTime;
}

/**********************************************PROCESS TABLE OPERATIONS************************************************/      
// These three function manage and print entries in the PCB table.
int addToProcessTable(pid_t pid) {
	int i;
	for (i = 0; i < 20; i++) {
		if (processTable[i].occupied == 0) {
			processTable[i].occupied = 1;
			processTable[i].processID = pid;
			processTable[i].startSeconds = systemClockSeconds;
			processTable[i].startNanoseconds = systemClockNano;
			processTable[i].messagesSent = 0;
			
			if (systemClockNano == oneBillionNanoseconds) {
				processTable[i].startSeconds++;
				processTable[i].startNanoseconds = 0;
			}
			return i;
		}
	}
	// If table is full, return this value to print an error in main() function.
	return -1;
}

void removeFromProcessTable(pid_t pid) {
	int i;
	for (i = 0; i < 20; i++) {
		if (processTable[i].processID == pid) {
			processTable[i].occupied = 0;
			processTable[i].processID = 0;
			processTable[i].startSeconds = 0;
			processTable[i].startNanoseconds = 0;
			processTable[i].messagesSent = 0;
			
			break;
		}
	}
}

void printProcessTable() {
	printf("\nOSS PID: %d  SysClockS: %d  SysClockNano: %lld\n", getpid(), systemClockSeconds, systemClockNano);
	printf("Process Table:\n");
	printf("Entry\t Occupied\t PID\t\t StartS\t StartN\t\t MessagesSent\n");
	
	int i;
	for (i = 0; i < 20; i++) {
		// If row in table is occupied, reduce tabbing after Row #3 (processID).
		if (processTable[i].occupied == 1) {
			// Reduce tabbing after Row #5 (startNanoseconds) if startNanoseconds has a large number.
			if (processTable[i].startNanoseconds >= 1000000) {
				printf("%d\t %d\t\t %d\t %d\t %ld\t %d\n", i, processTable[i].occupied, processTable[i].processID, processTable[i].startSeconds, processTable[i].startNanoseconds, processTable[i].messagesSent);
			}
			else {
				printf("%d\t %d\t\t %d\t %d\t %ld\t\t %d\n", i, processTable[i].occupied, processTable[i].processID, processTable[i].startSeconds, processTable[i].startNanoseconds, processTable[i].messagesSent);
			}
		}
		else {
			printf("%d\t %d\t\t %d\t\t %d\t %ld\t\t %d\n", i, processTable[i].occupied, processTable[i].processID, processTable[i].startSeconds, processTable[i].startNanoseconds, processTable[i].messagesSent);
		}
	}
	printf("\n");	
}

/********************************************MESSAGE PASSING OPERATIONS************************************************/
void sendMessageToUSER() {
	if (msgsnd(messageQueueID, &sendBuffer, sizeof(messageBuffer) - sizeof(long int), 0) == -1) {
		printf("ERROR in oss.c: Problem with msgsnd() function.\n");
		printf("Cannot send message to worker.c.\n\n");
		
		periodicallyTerminateProgram(-1);
	}
}

void receiveMessageFromUSER(int child) {
	if (msgrcv(messageQueueID, &receiveBuffer, sizeof(messageBuffer), processTable[child].processID, 0) == -1) {
		printf("ERROR in oss.c: Problem with msgrcv() function.\n");
		printf("Cannot receive message from worker.c.\n\n");
		
		periodicallyTerminateProgram(-1);
	}
}

/***************************************************USER GUIDANCE******************************************************/
// Displays a help message if user enters './oss -h'.
void printHelpMessage() {
	printf("\n\n\nThis program displays information about child and parent processes, including:\n");
	printf("\t1.) Process IDs (or PIDs).\n");
	printf("\t2.) Parent process IDs (or PPIDs).\n");
	printf("\t3.) A Process Control Block (PCB) table with child process entry information.\n");
	printf("\t4.) System clock and termination times for child processes.\n\n");
	
	
	printf("\n\nTo execute this program, type './oss', then type in any combination of options:\n\n\n");
	printf("Option:                       What to enter after option:               Default values (if argu-      Description:\n");
	printf("                                                                         ment is not entered):\n\n");
	printf("  -h                           > nothing.                                 > (not applicable)           > Displays this help menu.\n"); 
	printf("  -n [proc]                    > an integer between 1 and 10.             > defaults to 1.             > Runs a total # of processes.\n");
	printf("  -s [simul]                   > an integer smaller than '-n [proc]'.     > defaults to 1.             > Runs a max # of processes simultaneously.\n");
	printf("  -t [timeLimitForChildren]    > an integer of at least 1.                > defaults to 1.             > Runs each process between 1 and\n");
	printf("                                                                                                          [timeLimitForChildren] seconds.\n");
	printf("  -i [intervalInMS             > an integer between 1 and 1000.           > defaults to 500.           > Runs a new process every [interval\n");
	printf("       ToLaunchChildren]                                                                                  inMSToLaunchChildren] milliseconds.\n");
	printf("  -f [logfile]                 > a file's basename.                       > defaults to 'logfile'      > Stores output relating to parent and\n");
	printf("                                                                                                          child processes.\n\n\n"); 
	
	printf("For example, typing './oss -n 6 -s 4 -t 5 -i 600 -f storage' will run:\n");
	printf("\t1.) a total of 6 processes.\n");
	printf("\t2.) a maximum of 4 processes simultaneously.\n");
	printf("\t3.) each process for a duration between 1 and 5 seconds. Duration is randomly selected.\n");
	printf("\t4.) a new process every 600 milliseconds.\n");
	printf("\t5.) while storing message statuses inside a file called 'storage.txt'.\n\n\n");
	
	exit(0);
}

/*************************************************PROGRAM TERMINATION**************************************************/
void detachAndClearSharedMemory() {
	shmdt(secondsShared);
	shmdt(nanosecondsShared);
	shmdt(logfileFP);
	shmctl(secondsShmid, IPC_RMID, NULL);
	shmctl(nanoShmid, IPC_RMID, NULL);
	shmctl(logfileShmid, IPC_RMID, NULL);
}

void removeMessageQueue() {
	if (msgctl(messageQueueID, IPC_RMID, NULL) == -1) {
		printf("ERROR in oss.c: problem with msgctl() function.\n");
		printf("Cannot delete or remove message queue.\n\n");
		
		exit(-1);
	}
}

void printProgramSummary(int processes, int messages) {
	printf("**********************PROGRAM SUMMARY**********************\n\n");
	printf("Total processes launched:            %d\n", processes);
	printf("Total messages sent from OSS:        %d\n", messages);
	printf("\n\n");
	printf("***********************************************************\n\n");
}

// Gracefully terminates program after 60 real life seconds.
void periodicallyTerminateProgram(int signal) {
	printf("SIGALRM in oss.c; This program has ran for 60 seconds.\n");
	printf("Now terminating all child processes...\n\n");
	
	int i;
	for (i = 0; i < 20; i++) {
		if (processTable[i].occupied == 1) {
			pid_t processID = processTable[i].processID;
			
			if (processID > 0) {
				kill(processTable[i].processID, SIGTERM);
				printf("Signal SIGTERM was sent to PID %d\n", processID);
			}
		}
	}
	printf("\nChild process termination complete.\n");
	
	// Shared memory operations.
	printf("Now freeing shared memory...\n");
	detachAndClearSharedMemory();
	printf("\nShared memory detachment and deletion complete.\n");
	
	// Message queue operations.
	printf("Now deleting the message queue...\n");
	
	// Remove the message queue.
	if (msgctl(messageQueueID, IPC_RMID, NULL) == -1) {
		printf("ERROR in oss.c: problem with msgctl() function.\n");
		printf("Cannot delete or remove message queue.\n\n");
		
		exit(-1);
	}
	
	printf("\nMessage queue removal and deletion complete.\n");
	printf("Now exiting program...\n\n");
	
	exit(0);
}
