# Explanation of each stage of the program

## Stage 1a
**Goal(s) achieved:**
- Automated P3 & P4.

It doesn't rely on the yield() function in order to switch programs. It automatically switches programs by use of an IRQ every tick (~1 second).

**Useful info:**
* *scheduler()* chooses which process will own the CPU for execution, while other processes are on hold. When the CPU is idle, it will choose at least one of the processes available in the ready queue for execution.
* *dispatch()* provides control of the CPU to the process. Functions performed:
    * Context Switching
    * Switching to user mode
    * Moving to the correct location in the newly loaded program.
* TIMER chooses how long the current process can run. Prevents the process from making the system run forever.
* Lab 3 uses cooperative scheduling, where the invocation of the scheduler is **volunteered** by the currently running process (e.g. yield()).
* Lab 4 uses preemptive scheduling, where the invocation of the scheduler is **forced** onto the currently running process (e.g. Use of an IRQ).

## Stage 1b
**Goal(s) achieved:**
* Max amount of programs that can be run != Amount of programs to run.
* Implemented a scheduling algorithm (round-robin with priority + age).

Processes now rely upon a general section of memory rather than being assigned their own.

**Useful info:**
* Here are the reasons for using a scheduling algorithm:
     * The CPU uses scheduling to improve its efficiency.
     * It helps you to allocate resources among competing processes.
     * The maximum utilization of CPU can be obtained with multi-programming.
     * The processes which are to be executed are in ready queue.
* STATE ADVANTAGES OF ROUND ROBIN WITH PRIORITY AND AGE

## Stage 2a
**Goal(s) achieved:**
* Dynamically creates ( fork() ), executes ( exec() ) and terminates ( exit() ) processes.

     
