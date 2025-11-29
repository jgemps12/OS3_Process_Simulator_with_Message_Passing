# OS3: Process Simulator with Message Passing Operations

This project simulates the behavior of a basic operating system. It launches a specified number of child processes—a limited number of which can run simultaneously—over a series of loop iterations. 
It also retains core features from OS2: Simulated System Clock and Shared Memory, such as the Process Control Block (PCB) table, simulated system clock, and shared memory operations.


To build on OS2, this project introduces a **message queue**. The queue's goal is to facilitate communication between parent (OSS) and child (worker) processes. The operating system keeps track of how many messages are
sent by OSS (i.e. the parent) while **Round-Robin Scheduling** determines the order in which children receive those messages. 

## Key Features:
- Simulates a system clock that tracks time in seconds and nanoseconds using shared memory.
- Uses a **message queue** to communicate between OSS and its children.
- Uses **Round-Robin scheduling** to determine which child recieves the next message from OSS.
- Implements:
  - `fork()` for process creation.
  - `msgget()` and `msgctl()` for memory queue creation and deletion.
  - `msgsnd()` and `msgrcv()` for sending and receiving messsages within the message queue.
  - `waitpid()` for communication between parent and child.
- Uses a Process Control Block (PCB) table to record metadata, such as:
  - Process ID (PID).
  - Table slot occupancy.
  - Process start time.
  - Number of **messages sent**.
- Ensures clean termination of all processes to avoid zombie processes.

## System Clock Operations:
Throughout the duration of the program, the **simulated system clock** increments at a rate of 

$$
\text{\\# milliseconds} 
= \frac{\text{250 ms}}{\\# \text{active processes}}
$$

For instance, if **4 active processes** exist in the system, then the system clock will increment in this manner:

$$
\text{\\# milliseconds} 
= \frac{\text{250 ms}}{\text{4 processes}} 
= \textbf{62.5 ms}
$$

During this instance as shown above, the clock increments every **62.5 milliseconds**. The number of active processes ranges between 1 and `-s` processes, where `-s` is the maximum number of processes running concurrently. Since child processes constantly enter and exit the system, the system clock will increment at various rates due to `-s` constantly changing. This setup aims to simulate "slowing down" of the clock when the system is busy.

## How to Compile and Run:

1.) To *compile* the program, type:
```bash
make
```
2.) To *run* the program, type:
```bash
./oss -n <process_count> -s <max_simultaneous_process_count> -t <child_time_limit_seconds> -i <child_launch_interval_in_MS> -f <logfile_name>
```
### Command-Line Options:

| Option | Argument Format              | Required (yes/no)     | Description                                                                                         |
|--------|------------------------------|-----------------------|-----------------------------------------------------------------------------------------------------|
|  `-h`  | *(none)*                     | No                    | Displays help menu, then terminates program.                                                        |
|  `-n`  | an integer between 1 and 10. | No (default=1)        | Sets a total number of processes to run.                                                            |
|  `-s`  | an integer <= value of `-n`. | No (default=1)        | Sets a maximum number of processes to run simultaneously.                                           |
|  `-t`  | an integer >= 1.             | No (default=1)        | Sets a maximum number of seconds each process runs. Actual value is random between 1 and `-t` value.|
|  `-i`  | an integer >= 1.             | No (default=500)      | Sets an interval between child process launches (in *milliseconds*).                                |
|  `-f`  | a string.                    | No (default="logfile")| Sets a basename for a .txt file, which stores logging information about child processes.            |

## Example Output:

### Example 1: Console Log Output

Upon typing this command into the terminal:
```bash
./oss -n 3 -s 2 -t 5 -i 200 -f INFO
```
This program runs:
- 3 processes **total**.
- 2 processes **simultaneously**.
- Each process for a random time between 1 and 5 **seconds**, with 5 as the **maximum** time.
- A new process **incremented** to every 200 ms.
- A file called **INFO.txt** stores information about each child's message sending, retrieval, and termination.


```bash
OSS: Sending message to Worker #0 PID 1532282 at time 0:500000000
WORKER PID: 1532282   PPID: 1532281  SysClockS: 0  SysClockNano: 500000000  TermTimeS: 6  TermTimeNano: 755279253 ---Just starting
WORKER PID: 1532282   PPID: 1532281  SysClockS: 0  SysClockNano: 500000000  TermTimeS: 6  TermTimeNano: 755279253 ---1 iteration(s) have passed since starting
WORKER PID: 1532282   PPID: 1532281  SysClockS: 0  SysClockNano: 500000000  TermTimeS: 6  TermTimeNano: 755279253 ---2 iteration(s) have passed since starting
OSS: Receiving message from Worker #0 PID 1532282 at time 0:500000000
...
...
WORKER PID: 1532301   PPID: 1532281  SysClockS: 14  SysClockNano: 500000000  TermTimeS: 14  TermTimeNano: 384559769 ---Terminating
OSS: Receiving message from Worker #0 PID 1532301 at time 14:500000000
**OSS: Worker #0 PID 1532301 is planning to terminate.**
All 6 children have finished running. Now terminating program.

**********************PROGRAM SUMMARY**********************
Total processes launched:            6
Total messages sent from OSS:        475
***********************************************************

```
### Example 2: Log File Output
```bash
OSS: Sending message to Worker #0 PID 1532282 at time 1:479166666
OSS: Receiving message from Worker #0 PID 1532282 at time 1:479166666
OSS: Sending message to Worker #1 PID 1532283 at time 1:479166666
OSS: Receiving message from Worker #1 PID 1532283 at time 1:479166666
OSS: Sending message to Worker #2 PID 1532284 at time 1:479166666
```
### Example 3: Process Control Block (PCB) Table Output
Every half second of simulated system time, a Process Table prints to the console as shown below:

```
OSS PID: 1532281  SysClockS: 2  SysClockNano: 0
Process Table:
Entry    Occupied        PID             StartS  StartN          MessagesSent
0        1               1532282         0       500000000       13
1        1               1532283         1       0               12
2        1               1532284         1       250000000       11
3        1               1532285         1       416666666       10
4        0               0               0       0               0
...
```
A full Process Table contains 20 rows, one for each child process slot. 

#### Column Definitions:
- **Entry** - the index of the process that resides in the table.
- **Occupied** - is the process running? (**1** if *yes*; **0** if *no*)
- **PID** - the Process ID of a child inside a table row.
- **StartS** - start time of the process in *seconds*.
- **StartN** - start time of the process in *nanoseconds*.
- **MessagesSent** - total number of messages sent to a child process.

## Skills Learned:
- Applied message queues to facilitate communication between parent and child.
- Implemented Round-Robin scheduling to control the ordering of child process selection.
- Simulated system time tracking, process launching, and message passing by using a simulated clock.
- Maintained a PCB table for visualizing child process information, including messages sent from OSS to a child.
- Enforced limits on the runtime, quantity, and concurrency of processes running in the system.

## Tested On:
- Ubuntu 20.04.6 (LTS)
- GCC 10.5.0
- Make 4.2.1

## License:
This project is licensed under the [MIT License](LICENSE).
