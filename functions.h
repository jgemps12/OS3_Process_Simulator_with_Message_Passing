#ifndef FUNCTIONS_H_
#define FUNCTIONS_H_
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>


// For process table.
#define PROCESS_COUNT 20

// For memory queue.
#define PERMISSIONS 0644

/********************************STRUCTS**************************************/
// For process table operations.
struct PCB {
   int occupied;                        // Is the entry in the process table empty (0) or full (1)?
   pid_t processID;                     // Child's process ID.
   int startSeconds;                    // Time when a process forked (in seconds).
   long int startNanoseconds;           // Time when a process forked (in nanoseconds).
   int messagesSent;                    // # of messages sent by parent process for a child.
};
extern struct PCB processTable[PROCESS_COUNT];

// Holds message queue information.
typedef struct messageBuffer {
   long int messageType;
   char stringData[100];
   int integerData;
} messageBuffer;

/***************************GLOBAL VARIABLES**********************************/
// For log file operations.
extern char logfile[105];
extern char suffix[];
extern FILE *logOutputFP;
extern char *logfileFP;

// For shared memory operations.
extern int secondsShmid;
extern long int nanoShmid;
extern int logfileShmid;

// For message buffer operations. 
extern messageBuffer sendBuffer;
extern messageBuffer receiveBuffer;
extern int messageQueueID;
extern key_t key;

// For system clock operations.
extern int systemClockSeconds;
extern long long int systemClockNano;
extern long long int systemNanoOnly;
extern long int systemClockIncrement;

// For system clock sharing between processes.
extern int *secondsShared;
extern long int *nanosecondsShared;

// For time conversions.
extern long int halfBillionNanoseconds;
extern long int oneBillionNanoseconds;
extern long int oneQuarterSecond;

/*************************FUNCTiION PROTOTYPES********************************/
// For initialization.
void initializeLogfile();
void initializeMessageQueue();

// For user input validation.
void checkForOptargEntryError(int, char []);
void checkForSimulExceedsProcError(int, int);

// For system clock/time operations.
long long int incrementClock(int *, long long int *, int);
int randomizeChildSecondsLimit(int);
long int randomizeChildNanoseconds(int, int);
long long int convertSystemTimeToNanosecondsOnly(int *, long long int *);
long long int determineNextLaunchNanoseconds (int, long long int);

// For process table operations.
int addToProcessTable(pid_t);
void removeFromProcessTable(pid_t);
void printProcessTable();

// For message passing operations.
void sendMessageToUSER();
void receiveMessageFromUSER(int);

// For guiding the user.
void printHelpMessage();

// For terminating the program.
void detachAndClearSharedMemory();
void removeMessageQueue();
void printProgramSummary(int, int);
void periodicallyTerminateProgram(int);


#endif
