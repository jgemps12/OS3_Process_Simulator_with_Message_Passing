/* Jesse Gempel
 * 2/27/2025
 * Professor Mark Hauschild
 * CMP SCI 4760-001
*/


// The user.c file works with CHILD processes.
// It prints out child and parent process IDs, as well as child process and termination times, to the user for a specific period of time.


#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
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
  
   // Creates two shared memory identifiers.
   int secondsShmid = shmget(SHMKEY1, INT_BUFFER_SIZE, 0777);
   long int nanoShmid = shmget(SHMKEY2, LONG_BUFFER_SIZE, 0777);

   // Attaches the system time into shared memory.
   int *sharedSeconds = (int *)shmat(secondsShmid, 0, 0);
   long int *sharedNanoseconds = (long int *)shmat(nanoShmid, 0, 0);

   // Used for constant system time updated from shared memory.
   int systemClockSeconds = *sharedSeconds;
   long int systemClockNano = *sharedNanoseconds;

   // Used for termination time calculations. Will not be updated.
   int initialSystemClockSecs = *sharedSeconds;
   long int initialSystemClockNano = *sharedNanoseconds;        
  

   // If executing ./worker [timeLimitForChildren] [timeLimitNanoseconds], user must enter two integers after ./worker.
   // Otherwise, present error and terminate program.
   if (argc != 3) {
      printf("ERROR in 'worker.c': You must enter ./user followed by two integers.\n\n");

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


   // Output the PID, PPID, and time information every system time second.
   printf("WORKER PID: %d   PPID: %d  SysClockS: %d  SysClockNano: %ld  TermTimeS: %d  TermTimeNano: %ld ---Just starting\n", getpid(), getppid(), systemClockSeconds, systemClockNano, childTerminationTimeSeconds, childTerminationTimeNano);


   // Do not iteratively print statement in loop until a whole second passes.
   int i;
   for (i = 0; i < childProcessTimeSeconds; i++) { 
      while(1) {

	 // Compare # of seconds before and after shared memory is re-read.
         int secondsBeforeMemRead = systemClockSeconds;                                              // Before read.
         systemClockSeconds = *sharedSeconds;                                                        // During read.
         int secondsAfterMemRead = systemClockSeconds;                                               // After read.

         int secondsRan = getElapsedTimeSeconds(initialSystemClockSecs, systemClockSeconds);
 

         // Ensures that # of nanoseconds is similar to the last second as to the current second.
	 // That way, the statement below will wait one COMPLETE second before printing again.
         if (secondsBeforeMemRead != secondsAfterMemRead) {

            long int nanosecondsBeforeMemRead = systemClockNano;
            systemClockNano = *sharedNanoseconds;
            long int nanosecondsAfterMemRead = systemClockNano;


	    while (nanosecondsBeforeMemRead > nanosecondsAfterMemRead) {
               systemClockNano = *sharedNanoseconds;
               nanosecondsAfterMemRead = systemClockNano;
            }

            printf("WORKER PID: %d   PPID: %d  SysClockS: %d  SysClockNano: %ld  TermTimeS: %d  TermTimeNano: %ld ---%d seconds have passed since starting\n", getpid(), getppid(), systemClockSeconds, systemClockNano, childTerminationTimeSeconds, childTerminationTimeNano, secondsRan);


            break;
	 }
      }
   }
   

   // Now the system clock time equals the termination time (in seconds).
   // Process printf() statement once system time equals termination time (in nanoseconds).
   while (1) {                                                       
      systemClockNano = *sharedNanoseconds;                                                          // Memory update.

      if (systemClockNano >= childTerminationTimeNano) {
         printf("WORKER PID: %d   PPID: %d  SysClockS: %d  SysClockNano: %ld  TermTimeS: %d  TermTimeNano: %ld ---Terminating\n", getpid(), getppid(), systemClockSeconds, systemClockNano, childTerminationTimeSeconds, childTerminationTimeNano);
       
	 
	 break;
      }
   }


   // Detach shared memory.
   shmdt(sharedSeconds);
   shmdt(sharedNanoseconds);


   return EXIT_SUCCESS;

}
