/* Jesse Gempel
 * 3/18/2025
 * Professor Mark Hauschild
 * CMP SCI 4760-001
*/


// The worker.c file works with CHILD processes.
// It prints out child and parent process IDs, as well as child process and termination times, to the user for each iteration.


#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>


// Starting a memory segment for system clock seconds.
#define SHMKEY1 42069
#define INT_BUFFER_SIZE sizeof(int)


// Starting a memory segment for system clock nanoseconds.
#define SHMKEY2 42070
#define LONG_BUFFER_SIZE sizeof(long int)


// Starting a memory segment for a log file to validate ftok() function.
#define SHMKEY3 42071
#define LOGFILE_BUFFER_SIZE 105


// Permissions for memory queue.
#define PERMISSIONS 0644


// Holds message queue information.
typedef struct messageBuffer {
   long int messageType;
   char stringData[100];
   int integerData;
} messageBuffer;


// Initializes information for message buffer.
messageBuffer receiveBuffer;
messageBuffer sendBuffer;
int messageQueueID;
key_t key;


// Gathers logfile name from oss.c to validate ftok() function in worker.c (this file).
int childProcessTimeSeconds;
int childTerminationTimeSeconds;
long int childProcessTimeNano;
long int childTerminationTimeNano;


int getTerminationTimeSeconds (int processSeconds, int systemClockSeconds) {
   return processSeconds + systemClockSeconds;
}


// The point in simulated system time when child terminates.
long int getTerminationTimeNano (long int processNanoseconds, long int systemClockNano, int *termSeconds) {
   long int terminationNano = processNanoseconds + systemClockNano;
   long int oneBillionSeconds = 1000000000;
   
   while (terminationNano >= oneBillionSeconds) {
      (*termSeconds)++;
      terminationNano -= oneBillionSeconds;
   }

   return terminationNano;
}

// Amount of time that child process stays in system.
int getElapsedTimeSeconds (int startingTimeSeconds, int currentTimeSeconds) {
   return currentTimeSeconds - startingTimeSeconds;
}


int main(int argc, char** argv) {
  
   // Creates two shared memory identifiers (and one for a log file).
   int secondsShmid = shmget(SHMKEY1, INT_BUFFER_SIZE, 0777);
   long int nanoShmid = shmget(SHMKEY2, LONG_BUFFER_SIZE, 0777);

   int logfileShmid = shmget(SHMKEY3, LOGFILE_BUFFER_SIZE, 0777);


   // Attaches the system time into shared memory.
   int *sharedSeconds = (int *)shmat(secondsShmid, 0, 0);
   long int *sharedNanoseconds = (long int *)shmat(nanoShmid, 0, 0);


   // Attaches log file into shared memory.
   char *logfileFP = (char *)shmat(logfileShmid, 0, 0);


   // Used for constant system time updated from shared memory.
   int systemClockSeconds = *sharedSeconds;
   long int systemClockNano = *sharedNanoseconds;


   // Used for termination time calculations. Will not be updated.
   int initialSystemClockSecs = *sharedSeconds;
   long int initialSystemClockNano = *sharedNanoseconds;        
 

   // Attempts to set up a message queue.
   if ((key = ftok(logfileFP, 1)) == -1) {
      printf("ERROR in worker.c: problem with ftok() function.\n");
      printf("Cannot access a key for message queue initialization.\n\n");

      exit(-1);
   }

   if ((messageQueueID = msgget(key, PERMISSIONS)) == -1) {
      printf("ERROR in worker.c: problem with msgget() function.\n");
      printf("Cannot acquire a message queue ID for initialization.\n\n");

      exit(-1);
   }


   // If executing ./worker [timeLimitForChildren] [timeLimitNanoseconds], user must enter two integers after ./worker.
   // Otherwise, present error and terminate program.
   if (argc != 3) {
      printf("ERROR in 'worker.c': You must enter ./worker followed by two integers.\n\n");

      printf("Integer 1: the maximum time limit for a child process (in seconds).\n");
      printf("Integer 2: the # of nanoseconds AFTER seconds time limit has been reached.\n\n");

      printf("Example: ./worker 5 200000\n\n");

      exit(-1);
   }


   // Store user input that specifies the amount of time that the loop below will iterate.
   if (argc == 3) { 
      childProcessTimeSeconds = atoi(argv[1]);
      childProcessTimeNano = atoi(argv[2]);
   }


   // Determines the system time that the child process should terminate.
   childTerminationTimeSeconds = getTerminationTimeSeconds(childProcessTimeSeconds, initialSystemClockSecs);
   childTerminationTimeNano = getTerminationTimeNano(childProcessTimeNano, initialSystemClockNano, &childTerminationTimeSeconds);


   // Print starting message.
   printf("WORKER PID: %d   PPID: %d  SysClockS: %d  SysClockNano: %ld  TermTimeS: %d  TermTimeNano: %ld ---Just starting\n", getpid(), getppid(), systemClockSeconds, systemClockNano, childTerminationTimeSeconds, childTerminationTimeNano);


   // Keep track of how many times the do-while loop iterates.
   long int iterations = 0;


   do {
      // Receives a message from the parent.
      if (msgrcv(messageQueueID, &receiveBuffer, sizeof(messageBuffer), getpid(), 0) == -1) {
         printf("ERROR in worker.c: Problem with msgrcv() function.\n");
         printf("Cannot receive message from worker.c.\n\n");

         exit(-1);
      }
      
   
      // Compare # of seconds before and after shared memory is re-read.
      int secondsBeforeMemRead = systemClockSeconds;                                              // Before read.
      systemClockSeconds = *sharedSeconds;                                                        // During read.
      int secondsAfterMemRead = systemClockSeconds;                                               // After read.

      systemClockNano = *sharedNanoseconds;

      int secondsRan = getElapsedTimeSeconds(initialSystemClockSecs, systemClockSeconds);
 

      // Slow down program to prevent race conditions between Process Table and printf() message times (for oss.c and worker.c, respectively).
      int i;
      for (i = 0; i < 10000000; i++) {
         //  Do nothing.
      }


      // If a child is about to terminate, print an exit message.
      if (systemClockSeconds >= childTerminationTimeSeconds && 
            (systemClockSeconds > childTerminationTimeSeconds || systemClockNano >= childTerminationTimeNano)) {

	 sendBuffer.messageType = getpid();
         sendBuffer.integerData = 0;

         snprintf(sendBuffer.stringData, sizeof(sendBuffer.stringData), "Message received to child. Now sending to parent. Child is now terminating.");

         if (msgsnd(messageQueueID, &sendBuffer, sizeof(messageBuffer) - sizeof(long int), 0) == -1) {
            printf("ERROR in worker.c: Problem with msgsnd() function.\n");
            printf("Cannot send message to oss.c.\n\n");

            exit(-1);
         }

         printf("WORKER PID: %d   PPID: %d  SysClockS: %d  SysClockNano: %ld  TermTimeS: %d  TermTimeNano: %ld ---Terminating\n", getpid(), getppid(), systemClockSeconds, systemClockNano, childTerminationTimeSeconds, childTerminationTimeNano);
 
	 break;
      }


      // If a child is still running, print a progress message with # of iterations.
      else {
         sendBuffer.messageType = getpid();
         sendBuffer.integerData = 1;


         if (msgsnd(messageQueueID, &sendBuffer, sizeof(messageBuffer) - sizeof(long int), 0) == -1) {
            printf("ERROR in worker.c: Problem with msgsnd() function.\n");
            printf("Cannot send message to oss.c.\n\n");

            exit(-1);
         }

	 iterations++;
         printf("WORKER PID: %d   PPID: %d  SysClockS: %d  SysClockNano: %ld  TermTimeS: %d  TermTimeNano: %ld ---%ld iteration(s) have passed since starting\n", getpid(), getppid(), systemClockSeconds, systemClockNano, childTerminationTimeSeconds, childTerminationTimeNano, iterations);
         
      }
   }
   while (1);
  

   // Detach shared memory.
   shmdt(sharedSeconds);
   shmdt(sharedNanoseconds);
   shmdt(logfileFP);


   return EXIT_SUCCESS;

}
