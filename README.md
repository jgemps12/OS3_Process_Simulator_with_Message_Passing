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

## Program Code Operations:
### Simulated System Clock:
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

### Message Queue Operations: 
The two files, *oss.c* and *worker.c*, both utilize two different message queues, `sendBuffer` and `receiveBuffer`. They both operate under a struct that contains two members:
  - `messageType`- stores the child process's **Process ID (PID)**.
    - Both *oss.c* and *worker.c* files use this information to determine which child process must receive a message and send it back.
  - `integerData`- stores one of two numbers: `0` or `1`.
    - **oss.c** (i.e., the parent) stores a number to determine whether a process occupies a specific row in the Process Control Block (PCB) table.
    - **worker.c** (i.e., the child) uses this member variable to determine whether a specific process should terminate. `1` is sent to OSS if a process should stay in the        system, whereas `0` is returned if the process should terminate.
      
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
- Each new process launch **incremented** to a minimum of every 200 ms.
- A file called **INFO.txt** stores information about each child's message sending, retrieval, and termination.

#### i.) Program Initialization and *msgsnd()*:
The program begins by using **fork()** to launch a child process with PID **2003552**, which is launched after `500,000,000` nanoseconds of system runtime. The "parent" (i.e., OSS) makes a call to **msgsnd()** to send a message to the child process, which contains information about PIDs, system clock times, and termination times. Meanwhile, the "worker" must receive this information from the parent and provide confirmation through console output. The launch time values for this process are determined by `SysClockS` and `SysClockNano` as shown below:

```bash
OSS: Sending message to Worker #0 PID 2003552 at time 0:500000000
WORKER PID: 2003552   PPID: 2003551  SysClockS: 0  SysClockNano: 500000000  TermTimeS: 5  TermTimeNano: 500000000 ---Just starting
```
**NOTE:** Actual process start times may not necessarily reflect the user input for `-i` due to large increments occurring within the system clock. The example above shows the process starting at 500 ms, even though the user inputted `-i` = 200 ms.

#### ii.) Process Runtime and *msgrcv()*:
Information about the child process (PID **2003552**) is then received by the worker by utilizing a call to **msgrcv()**. The child process will undergo an **iteration**, meaning that the parent and child must both send and receive messages to and from each other. For these iterations to continue, the worker must *send* a message to OSS using **msgsnd()**, then OSS must *receive* the message through **msgrcv()**. 

The output below demonstrates two events: 

> 1.) The worker displaying the number of iterations for a child process (first two lines).

> 2.) OSS confirming the sending and receiving of messages to a child (last two lines). 
```bash
WORKER PID: 2003552   PPID: 2003551  SysClockS: 0  SysClockNano: 500000000  TermTimeS: 5  TermTimeNano: 500000000 ---1 iteration(s) have passed since starting
WORKER PID: 2003552   PPID: 2003551  SysClockS: 0  SysClockNano: 500000000  TermTimeS: 5  TermTimeNano: 500000000 ---2 iteration(s) have passed since starting
OSS: Receiving message from Worker #0 PID 2003552 at time 0:500000000
OSS: Sending message to Worker #0 PID 2003552 at time 1:0
```

The simulated system clock will continue to iterate until a process can finally terminate. Termination times for Process **2003552** are illustrated by `TermTimeS` and `TermTimeNano` as shown above.

#### iii.) Concurrency:
Even though multiple child processes (i.e., **2003552** and **2003553**) exist concurrently in the system, only one child is allowed to send and receive messages to and from the parent process at a time. A modified **Round-Robin scheduling algorithm** decides which child can pass messages at any given time. 

**NOTE:** This algorithm is *modified* since Round-Robin scheduling typically runs processes for a fixed amount of time. For this project, a child does not run for predetermined time periods. It must instead pass messages to and from OSS (i.e., the parent) before the next child can run, which can result in variable runtimes.

```bash
OSS: Sending message to Worker #0 PID 2003552 at time 1:125000000
WORKER PID: 2003552   PPID: 2003551  SysClockS: 1  SysClockNano: 125000000  TermTimeS: 5  TermTimeNano: 500000000 ---5 iteration(s) have passed since starting
WORKER PID: 2003552   PPID: 2003551  SysClockS: 1  SysClockNano: 125000000  TermTimeS: 5  TermTimeNano: 500000000 ---6 iteration(s) have passed since starting
OSS: Receiving message from Worker #0 PID 2003552 at time 1:125000000

OSS: Sending message to Worker #1 PID 2003553 at time 1:125000000
WORKER PID: 2003553   PPID: 2003551  SysClockS: 1  SysClockNano: 125000000  TermTimeS: 6  TermTimeNano: 0 ---3 iteration(s) have passed since starting
WORKER PID: 2003553   PPID: 2003551  SysClockS: 1  SysClockNano: 125000000  TermTimeS: 6  TermTimeNano: 0 ---4 iteration(s) have passed since starting
OSS: Receiving message from Worker #1 PID 2003553 at time 1:125000000
```
**Round-Robin scheduling** allows the execution of each process based on their ordering in the Process Control Block (PCB) table, from first entry to last entry. As shown above, the PCB table's first entry is Process P0 (**2003552**), whereas its last entry is Process P1 (**2003553**). Process P0 must always run before Process P1, then these two children run repeatedly in this particular order until one of them must be terminated. 

#### iv.) Process Termination:
Since the system clock time is greater than (or in this case, equal to) the termination time for Process **2003552**, the child will now terminate.

```bash
WORKER PID: 2003552   PPID: 2003551  SysClockS: 5  SysClockNano: 500000000  TermTimeS: 5  TermTimeNano: 500000000 ---Terminating
```
#### v.) Program Termination:
Two more process need to run in the system until they reach their termination times and terminate. Child termination is conducted by using a call to **waitpid()**.

```bash
...
...
WORKER PID: 2003554   PPID: 2003551  SysClockS: 11  SysClockNano: 0  TermTimeS: 11  TermTimeNano: 0 ---Terminating
OSS: Receiving message from Worker #0 PID 2003554 at time 11:0
**OSS: Worker #0 PID 2003554 is planning to terminate.**

All 3 children have finished running. Now terminating program.
Program successfully terminated.
```

### Example 2: Log File Output
The log file output serves as a consolidation of the output that prints onto the console. Whereas the console outputs *all* detailed information about parent and child processes, the log file only deals with output relating to OSS operations (i.e., those relating to the parent).

```bash
OSS: Sending message to Worker #0 PID 1532282 at time 1:479166666
OSS: Receiving message from Worker #0 PID 1532282 at time 1:479166666
OSS: Sending message to Worker #1 PID 1532283 at time 1:479166666
OSS: Receiving message from Worker #1 PID 1532283 at time 1:479166666
OSS: Sending message to Worker #2 PID 1532284 at time 1:479166666
```

A full, real-life example of this output can be viewed by clicking on the **logfile.txt** file in this repository.

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
