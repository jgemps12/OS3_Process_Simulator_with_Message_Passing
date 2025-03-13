/* Jesse Gempel
 * 2/27/2025
 * Professor Mark Hauschild
 * CMP SCI 4760-001
*/


// The oss.c file works with PARENT processes.
// It launches a specific number of user processes with user input gathered from the 'getopt()' switch statement. 
// This time, process launches will depend on a simulated system clock.


#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
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


// Creates and attaches two shared memory identifiers.
int secondsShmid = shmget(SHMKEY1, INT_BUFFER_SIZE, 0777 | IPC_CREAT);
long int nanoShmid = shmget(SHMKEY2, LONG_BUFFER_SIZE, 0777 | IPC_CREAT);

int *secondsShared = (int *)shmat(secondsShmid, 0, 0);
long int *nanosecondsShared = (long int *)shmat(nanoShmid, 0, 0);


// A process table holds information about each child process.
struct PCB {
   int occupied;                        // Is the entry in the process table empty (0) or full (1)?
   pid_t processID;                     // Child's process ID.
   int startSeconds;                    // Time when a process forked (in seconds).
   long int startNanoseconds;           // Time when a process forked (in nanoseconds).
};
struct PCB processTable[20];


// Places nanosecond values into variables for easier code readability.
long int halfBillionNanoseconds = 500000000;
long int oneBillionNanoseconds = 1000000000;
long int oneQuarterSecond = 250000000;

// Initialization of simulated system time. Increments in a way that makes the simulated system time appear similar to real time.
int systemClockSeconds = 0;
long long int systemClockNano = 0;

long int systemClockIncrement = oneQuarterSecond;
//int systemClockIncrement = 1000;


// Uses system nanoseconds to determine when next process should launch.
long long int currentLaunchTimeNano = systemClockIncrement;
long long int nextLaunchTimeNano = systemClockIncrement;
int currentLaunchTimeSeconds = 0;
int nextLaunchTimeSeconds = 0;


// Function prototypes.
void checkForOptargEntryError(int, char []);
void checkForSimulExceedsProcError(int, int);
void incrementClock(int *, long long int *, int);
int randomizeChildSecondsLimit(int);
long int randomizeChildNanoseconds(int, int);
long long int determineNextLaunchNanoseconds(int, long long int);
int addToProcessTable(pid_t);
void removeFromProcessTable(pid_t);
void printProcessTable();
void printHelpMessage();
void periodicallyTerminateProgram(int);



int main(int argc, char** argv) {  
   int opt;

   // If user does not input the arguments corresponding to variables below, assign default values.
   int proc = 1;
   int simul = 1;
   int timeLimitForChildren = 1;
   int intervalInMSToLaunchChildren = 500;
   
   char logfile[105] = "logfile.txt";
   char suffix[] = ".txt";

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

	    strncpy(basename, optarg, sizeof(basename) - 1);
	    basename[sizeof(basename) - 1] = '\0';

	    strcat(basename, suffix);
	    strcpy(logfile, basename);

            printf("Logfile name: %s\n", logfile);

	    break;

         default:
	    printf("ERROR in oss.c: Arguments are invalid or you forgot to input a value for them.\n");
	    printf("Please type './oss -h' for help.\n\n");

            exit(-1);

 	    break;
      }
   }


    printf("Logfile name: %s\n", logfile);





   bool processesFinished = false;
   int childrenActive = 0;                                          // # of children running simultaneously (not to be confused with 'proc').
   int totalChildrenLaunched = 0;                                   // # of children launched so far (not to be confused with 'simul').
   
   // Initializes shared memory segments.
   *secondsShared = 0;
   *nanosecondsShared = 0;

   // Signal handler for terminating program after 60 real-life seconds.
   signal(SIGALRM, periodicallyTerminateProgram);
   alarm(60);


   while (processesFinished == false) {
      incrementClock(&systemClockSeconds, &systemClockNano, childrenActive);
     // sleep(0.5); 
      //
      //
      //printf("childrenActive: %d \t systemClockSeconds: %d\t systemClockNano: %Lf\n", childrenActive, systemClockSeconds, systemClockNano);
           //printf("\tnextLaunchTimeNano: %Lf  systemClockIncrement: %Lf \t oneQuarterSecond: %Lf\n\n", nextLaunchTimeNano, systemClockIncrement, (long double) oneQuarterSecond);


      // System time in shared memory constantly updates in loop.
      *secondsShared = systemClockSeconds;
      *nanosecondsShared = systemClockNano;


      // Process table prints every half second.
      if (systemClockNano == halfBillionNanoseconds || systemClockNano == 0) {
         printProcessTable();
      }
     

      // If children are still available to launch simultaneously.
      if (childrenActive < simul && totalChildrenLaunched < proc) {
         pid_t processID;


         // Spinlocks ('while' and 'for' loops) prevent multiple Process Tables from printing out in short time bursts.
         while (systemClockNano - nextLaunchTimeNano != (long double) oneQuarterSecond) {
	 //while (systemClockNano <= nextLaunchTimeNano) {  
	//do {
	   int i;
	   for (i = 0; i < 500; i++) {
 	      // Do nothing.
	   }   


	   if (systemClockNano == halfBillionNanoseconds || systemClockNano == 0) {
              printProcessTable();
           }
	   incrementClock(&systemClockSeconds, &systemClockNano, childrenActive);
           printf("childrenActive: %d \t systemClockSeconds: %d\t systemClockNano: %lld\n", childrenActive, systemClockSeconds, systemClockNano);
           printf("\tnextLaunchTimeNano: %lld systemClockIncrement: %ld \t oneQuarterSecond: %ld\n\n", nextLaunchTimeNano,  systemClockIncrement, oneQuarterSecond);

           long int difference = systemClockNano - nextLaunchTimeNano;
           printf("Difference: %ld, oneQuarterSecond: %ld\n", difference, oneQuarterSecond);

           // Launches a child based on [intervalInMSToLaunchChildren].
	   if (systemClockNano - nextLaunchTimeNano == (long double) oneQuarterSecond) {
		  printf("LAUNCHEDDDDDDDDD\n\n\n");
           //if (systemClockNano >= nextLaunchTimeNano) {
//      	      if (systemClockNano != 0) {
	          processID = fork();
              
	          nextLaunchTimeNano = determineNextLaunchNanoseconds(intervalInMSToLaunchChildren, currentLaunchTimeNano);
                  currentLaunchTimeNano = nextLaunchTimeNano;

		  break;
	       }
  //          }  
         }
//	 while (systemClockNano < nextLaunchTimeNano);
         //while (systemClockNano - nextLaunchTimeNano < systemClockIncrement);        

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
         if (processID > 0) {
            childrenActive++;
            totalChildrenLaunched++;
 
	    //if (childrenActive >= 1) {
	  //     systemClockIncrement = (double) oneQuarterSecond / childrenActive;
        //    }

            if (addToProcessTable(processID) == -1) {
               printf("ERROR in oss.c: Process Control Block (PCB) table is full.\n");
	       printf("Cannot add PID %d\n", processID);
	    }
	 }
      }
    
    
      // If no more children are running and the maximum # of total children have been launched, end loop/program.
      if (childrenActive == 0 && totalChildrenLaunched == proc) {
	 processesFinished = true;
      }


      // If the limit of simultaneous children has been reached, but more still need to be launched, wait for them to terminate.
      if (childrenActive == simul && totalChildrenLaunched < proc) {
         int status;
	 pid_t pid;


	 while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
	    removeFromProcessTable(pid);
            childrenActive--;

	    //if (childrenActive >= 1) {
              // systemClockIncrement = (double) oneQuarterSecond / childrenActive;
            //}
	 }
      }


      // If all available children have launched, but not all of them finished, wait for them to terminate.
      if (childrenActive > 0 && totalChildrenLaunched == proc) {
         int status;
	 pid_t pid;

	 while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
	    removeFromProcessTable(pid);
            childrenActive--;

	    //if (childrenActive >= 1) {
             //  systemClockIncrement = (double) oneQuarterSecond / childrenActive;
           // }
	 }
      }
   }
   printProcessTable();

  
   shmdt(secondsShared);
   shmdt(nanosecondsShared);
   shmctl(secondsShmid, IPC_RMID, NULL);
   shmctl(nanoShmid, IPC_RMID, NULL);


   return EXIT_SUCCESS;
}




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


// Adjust system time's seconds and nanoseconds.
void incrementClock(int *seconds, long long int *nanoseconds, int activeChildrenCount) {
   //if (activeChildrenCount <= 0) {
     // return;
  // }

   long long int increment;
   long long int remainder;

   static long long int remainderValue = 0;
  
   if (activeChildrenCount > 0) {
      increment = oneQuarterSecond / activeChildrenCount;
      remainder = oneQuarterSecond % activeChildrenCount;
      printf("increment = %lld, remainder = %lld, remainderValue = %lld\n\n", increment, remainder, remainderValue);
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


   if (*nanoseconds > oneBillionNanoseconds) {
       *nanoseconds = 0;
       (*seconds)++;
   }
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


// Only deals with system nanoseconds to determine next launch time. 
// System seconds is implicitly dealt with in main().
long long int determineNextLaunchNanoseconds (int intervalMS, long long int oldLaunchTime) {
    long long int nanoConversion = (long long int)intervalMS * 1000000;
    long double newLaunchTime = (long double) oldLaunchTime + nanoConversion;


    while (newLaunchTime > oneBillionNanoseconds) {
       newLaunchTime -= oneBillionNanoseconds;
    }

    return newLaunchTime;
}


// These three function manage and print entries in the PCB table.
int addToProcessTable(pid_t pid) {
   int i;
   for (i = 0; i < 20; i++) {
      if (processTable[i].occupied == 0) {
         processTable[i].occupied = 1;
         processTable[i].processID = pid;
         processTable[i].startSeconds = systemClockSeconds;
         processTable[i].startNanoseconds = systemClockNano;

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

         break;
      }
   }
}
void printProcessTable() {
   printf("\nOSS PID: %d  SysClockS: %d  SysClockNano: %lld\n", getpid(), systemClockSeconds, systemClockNano);
   printf("Process Table:\n");
   printf("Entry\t Occupied\t PID\t\t StartS\t StartN\n");

   int i;

   for (i = 0; i < 20; i++) {
      if (processTable[i].occupied == 1) {
         printf("%d\t %d\t\t %d\t %d\t %ld\n", i, processTable[i].occupied, processTable[i].processID, processTable[i].startSeconds, processTable[i].startNanoseconds);
      }
      else {
         printf("%d\t %d\t\t %d\t\t %d\t %ld\n", i, processTable[i].occupied, processTable[i].processID, processTable[i].startSeconds, processTable[i].startNanoseconds);
      }

   }

   printf("\n");
}


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
   printf("Now freeing shared memory...\n");

   shmdt(secondsShared);
   shmdt(nanosecondsShared);
   shmctl(secondsShmid, IPC_RMID, NULL);
   shmctl(nanoShmid, IPC_RMID, NULL);

   printf("\nShared memory detachment and deletion complete.\n");
   printf("Now exiting program...\n\n");

   exit(-1);
}


