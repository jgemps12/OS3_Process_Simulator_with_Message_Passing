// The oss.c file works with PARENT processes.
// It launches a specific number of user processes with user input gathered from the 'getopt()' switch statement. 
// This time, process launches will depend on a simulated system clock as well as a memory queue.

#include "functions.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>

// Starting a memory segment for system clock seconds.
#define SHMKEY1 42069
#define INT_BUFFER_SIZE sizeof(int)

// Starting a memory segment for system clock nanoseconds.
#define SHMKEY2 42070
#define LONG_BUFFER_SIZE sizeof(long int)

// Starting a memory segment for a log file.
#define SHMKEY3 42071
#define LOGFILE_BUFFER_SIZE 105

// Permissions for memory queue.
#define PERMISSIONS 0644

// Initializes a log file to store message queue information.
char logfile[105] = "logfile.txt";
char suffix[] = ".txt";
FILE *logOutputFP = NULL;

// Creates and attaches two shared memory identifiers (plus one for a log file).
int secondsShmid = shmget(SHMKEY1, INT_BUFFER_SIZE, 0777 | IPC_CREAT);
int *secondsShared = (int *)shmat(secondsShmid, 0, 0);

long int nanoShmid = shmget(SHMKEY2, LONG_BUFFER_SIZE, 0777 | IPC_CREAT);
long int *nanosecondsShared = (long int *)shmat(nanoShmid, 0, 0);

int logfileShmid = shmget(SHMKEY3, LOGFILE_BUFFER_SIZE, 0777 | IPC_CREAT);
char *logfileFP = (char *)shmat(logfileShmid, 0, 0);

// A process table holds information about each child process.
struct PCB processTable[PROCESS_COUNT];

// Initializes information for message buffer.
messageBuffer sendBuffer;
messageBuffer receiveBuffer;
int messageQueueID;
key_t key;

// Places nanosecond values into variables for easier code readability.
long int halfBillionNanoseconds = 500000000;
long int oneBillionNanoseconds = 1000000000;
long int oneQuarterSecond = 250000000;

// Initialization of simulated system time. Increments every (250 / number of active processes) seconds.
int systemClockSeconds = 0;
long long int systemClockNano = 0;
long long int systemNanoOnly = 0;                                
long int systemClockIncrement = oneQuarterSecond;

// Uses system nanoseconds to determine when next process should launch.
long long int currentLaunchTimeNano = systemClockIncrement;
long long int nextLaunchTimeNano = systemClockIncrement;
int currentLaunchTimeSeconds = 0;
int nextLaunchTimeSeconds = 0;

// For determining the child process order in which messages are to be sent.
int currentChildIndex = 0;


int main(int argc, char** argv) {  
	int opt;
	strcpy(logfileFP, logfile);
	
	// If user does not input the arguments corresponding to variables below, assign default values.
	int proc = 1;
	int simul = 1;
	int timeLimitForChildren = 1;
	int intervalInMSToLaunchChildren = 500;
	
	// Copies the default log file name into shared memory.
	strcpy(logfileFP, "logfile.txt");
	
	// If user puts invalid values for opt arguments, one of these strings will be passed into 'checkForOptargError()'.
	// This will help state the option in which the error occurred while trying to execute './oss'.
	char procName[] = "-n [proc]";
	char simulName[] = "-s [simul]";
	char timeLimitName[] = "-t [timeLimitForChildren]";
	char intervalName[] = "-i [intervalInMSToLaunchChildren]";
	char logfileName[] = "-f [logfile]";
	
	while ((opt = getopt(argc, argv, "hn:s:t:i:f:")) != -1) {
		switch (opt) {
			case 'h':
				printHelpMessage();
				break;
			
			case 'n':
				proc = atoi(optarg);
				checkForOptargEntryError(proc, procName);
				break;
			
			case 's':
				simul = atoi(optarg);
				checkForOptargEntryError(simul, simulName);
				checkForSimulExceedsProcError(simul, proc);
				break;
			
			case 't':
				timeLimitForChildren = atoi(optarg);
				checkForOptargEntryError(timeLimitForChildren, timeLimitName);
				break;
			
			case 'i':
				intervalInMSToLaunchChildren = atoi(optarg);
				checkForOptargEntryError(intervalInMSToLaunchChildren, intervalName);
				
				nextLaunchTimeNano = determineNextLaunchNanoseconds(intervalInMSToLaunchChildren, currentLaunchTimeNano);
				currentLaunchTimeNano = nextLaunchTimeNano;
				
				break;
			
			case 'f':
				char basename[100];
				
				// Gathers filename input.
				strncpy(basename, optarg, sizeof(basename) - 1);
				basename[sizeof(basename) - 1] = '\0';
				
				// Adds .txt suffix to user-inputted basename.
				strcat(basename, suffix);
				strcpy(logfile, basename);                    
				
				// Copies user-inputted filename into shared memory.
				strcpy(logfileFP, logfile);
				
				break;
			
			default:
				printf("ERROR in oss.c: Arguments are invalid or you forgot to input a value for them.\n");
				printf("Please type './oss -h' for help.\n\n");
				exit(-1);
			
				break;
		}
	}
	
	bool processesFinished = false;
	int childrenActive = 0;                                          // # of children running simultaneously (not to be confused with 'proc').
	int totalChildrenLaunched = 0;                                   // # of children launched so far (not to be confused with 'simul').  
	int totalMessagesSent = 0;
	int nextChild = 0;
	
	// Initializes shared memory segments.
	*secondsShared = 0;
	*nanosecondsShared = 0;
	
	// Creates .txt file and message queue to store message update information from oss.c (this file).
	initializeLogfile();
	initializeMessageQueue();
	
	// Signal handler for terminating program after 60 real-life seconds.
	signal(SIGALRM, periodicallyTerminateProgram);
	alarm(60);
	
	while (processesFinished == false) {
		systemClockIncrement = incrementClock(&systemClockSeconds, &systemClockNano, childrenActive);
		
		// System time in shared memory constantly updates in loop.
		*secondsShared = systemClockSeconds;
		*nanosecondsShared = systemClockNano;
		
		// If children are still available to launch simultaneously.
		if (childrenActive < simul && totalChildrenLaunched < proc) {
			pid_t processID;
			
			// Spinlock ('while' loop) prevents multiple Process Tables from printing out in short time bursts.
			while (systemNanoOnly - nextLaunchTimeNano != (long double) oneQuarterSecond) {
				int i;
				
				if (systemClockNano == halfBillionNanoseconds || systemClockNano == 0) {
					printProcessTable();	
				}
				systemClockIncrement = incrementClock(&systemClockSeconds, &systemClockNano, childrenActive); 
				
				// Launches a child based on [intervalInMSToLaunchChildren].
				if ((systemNanoOnly - nextLaunchTimeNano >= (long double) oneQuarterSecond) || (systemNanoOnly - nextLaunchTimeNano >= 0)) {   
					processID = fork();
					
					nextLaunchTimeNano = determineNextLaunchNanoseconds(intervalInMSToLaunchChildren, systemNanoOnly);
					break;
				}   
			}
			
			// Work with child process. Send [timeLimitForChildren] to worker.c to execute the child process.
			if (processID == 0) {
				*secondsShared = systemClockSeconds;
				*nanosecondsShared = systemClockNano;
				
				// Stores randomized child process run times into strings to facilatate execl()'s operation.
				int randomSeconds = randomizeChildSecondsLimit(timeLimitForChildren);
				char randomizedTimeSeconds[50];
				
				long int randomNanoseconds = randomizeChildNanoseconds(timeLimitForChildren, randomSeconds);
				char randomizedTimeNanoseconds[50];
				
				sprintf(randomizedTimeSeconds, "%d", randomSeconds);
				sprintf(randomizedTimeNanoseconds, "%ld", randomNanoseconds);
				
				// Run child processes.
				execl("./worker", "worker", randomizedTimeSeconds, randomizedTimeNanoseconds, NULL);
				
				printf("ERROR in oss.c: the execl() function has failed. Terminating program.\n\n");
				exit(-1);
			}
			
			// Work with parent process. Increment the current # of total children and those running simultaneously.
			// Meanwhile, send a message to a running child process.
			if (processID > 0) {
				childrenActive++;
				totalChildrenLaunched++;
				
				if (addToProcessTable(processID) == -1) {
					printf("ERROR in oss.c: Process Control Block (PCB) table is full.\n");
					printf("Cannot add PID %d\n", processID);
				}
			}
		}
		
		// For-loop acts as a Round Robin scheduling mechanism to determine which child should receive the next message from the parent.
		for (nextChild = 0; nextChild < totalChildrenLaunched; nextChild++) {	 	      
			// Print process table for every half second of simulated system time.
			if ((systemClockNano == halfBillionNanoseconds || systemClockNano == 0)  && (nextChild == 0)) {
				printProcessTable();
			}
			
			if (processTable[nextChild].occupied == 1) {	    
				// A buffer stores information about what will be sent to a child.
				sendBuffer.messageType = processTable[nextChild].processID;
				sendBuffer.integerData = processTable[nextChild].occupied;
				snprintf(sendBuffer.stringData, sizeof(sendBuffer.stringData), "Message sent to child %d again. Child is still running.", nextChild);
				
				// Parent process sends a message to a child process.
				sendMessageToUSER();
				totalMessagesSent++;
				
				printf("OSS: Sending message to Worker #%d PID %ld at time %d:%lld\n", nextChild, sendBuffer.messageType, systemClockSeconds, systemClockNano);
				fprintf(logOutputFP, "OSS: Sending message to Worker #%d PID %ld at time %d:%lld\n", nextChild, sendBuffer.messageType, systemClockSeconds, systemClockNano);
				fflush(logOutputFP);
				
				// Slow down program to prevent race conditions between times in Process Table and those analyzed in worker.c.
				// Also prevents multiple empty Process Tables from printing towards the program's end.
				int i; 
				for (i = 0; i < 20000000; i++) {
					// Do nothing.
				}
				
				// Another buffer stores info about what the parent receives from a child.
				receiveBuffer.messageType = processTable[nextChild].processID;
				receiveBuffer.integerData = processTable[nextChild].occupied;
				
				// Parent process receives a message from a child process. Output printed to a logfile.
				receiveMessageFromUSER(nextChild); 
				
				printf("OSS: Receiving message from Worker #%d PID %ld at time %d:%lld\n", nextChild, receiveBuffer.messageType, systemClockSeconds, systemClockNano);
				fprintf(logOutputFP, "OSS: Receiving message from Worker #%d PID %ld at time %d:%lld\n", nextChild, receiveBuffer.messageType, systemClockSeconds, systemClockNano);
				fflush(logOutputFP);
				
				processTable[nextChild].messagesSent++;
				
				// If a child process will end, output in console and logfile that it will do so.
				if (receiveBuffer.integerData == 0) {
					removeFromProcessTable(receiveBuffer.messageType);
					
					printf("**OSS: Worker #%d PID %ld is planning to terminate.**\n\n", nextChild, receiveBuffer.messageType);
					fprintf(logOutputFP, "**OSS: Worker #%d PID %ld is planning to terminate.**\n\n", nextChild, receiveBuffer.messageType);
					fflush(logOutputFP);
				} 
			} 
			
			// If no more children are running and the maximum # of total children have been launched, end loop/program.
			if (childrenActive == 0 && totalChildrenLaunched == proc) {
				printf("\n\nAll %d children have finished running. Now terminating program.\n", proc);
				processesFinished = true;
				
				break;
			}
			
			// If the limit of simultaneous children has been reached, but more still need to be launched, wait for them to terminate.
			if (childrenActive == simul && totalChildrenLaunched < proc) {
				int status;
				pid_t pid;
				
				while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
					removeFromProcessTable(pid);
					childrenActive--;
					nextLaunchTimeNano = determineNextLaunchNanoseconds(intervalInMSToLaunchChildren, systemNanoOnly);
				}
			}
			
			// If all available children have launched, but not all of them finished, wait for them to terminate.
			if (childrenActive > 0 && totalChildrenLaunched == proc) {
				int status;
				pid_t pid;
				
				while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
					removeFromProcessTable(pid);
					childrenActive--;
				}
			}
		}
	}
	printProcessTable();
	fclose(logOutputFP);
	detachAndClearSharedMemory();
	removeMessageQueue();
	
	printf("Program successfully terminated.\n\n\n");
	printProgramSummary(proc, totalMessagesSent);
	
	return EXIT_SUCCESS;
}
