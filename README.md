Project 1 – System Call Implementation for xv6
Name: Paria Hesami
Student ID: 218994442

---

Overview
--------

In this project I implemented a new system call for xv6-riscv called getprocs() and a user-space program ps that uses it.

- getprocs() collects information about currently running processes in the kernel and copies it to a user-provided array of struct procinfo.
- ps calls getprocs(), converts the numeric process states to human-readable strings, and prints a table of processes with columns:

    PID    PPID   STATE      SIZE       NAME

This project helped me understand the system call path from user space to kernel space, how to safely access shared kernel data structures (the process table), and how to copy data between kernel and user memory.

---

Files I Modified / Added
------------------------

Kernel

- kernel/procinfo.h  
  - New header shared between kernel and user code.  
  - Defines the struct procinfo that describes a process:

    struct procinfo {
      int pid;
      int ppid;
      int state;
      uint sz;
      char name[16];
    };

- kernel/syscall.h  
  - Added a new system call number:

    #define SYS_getprocs 23

- kernel/syscall.c  
  - Added the external prototype for the kernel handler:

    extern uint64 sys_getprocs(void);

  - Registered the syscall in the dispatch table:

    [SYS_getprocs] sys_getprocs,

- kernel/sysproc.c  
  - Added #include "procinfo.h" and extern declarations for the process table and its lock:

    extern struct proc proc[NPROC];
    extern struct spinlock wait_lock;

  - Implemented the kernel side of the system call:

    uint64 sys_getprocs(void);

    This function walks the proc[] table, fills a temporary kernel buffer of struct procinfo, and then uses copyout() to send that data to the user buffer.

User

- user/user.h  
  - Added a forward declaration and prototype so user programs can call the syscall:

    struct procinfo;
    int getprocs(struct procinfo *pinfo, int max_procs);

- user/usys.pl  
  - Added an entry so the build system generates the user-space stub for the syscall:

    entry("getprocs");

- user/ps.c  
  - New user program that calls getprocs(), converts numeric states to strings, and prints the process table.

- Makefile  
  - Added ps to the list of user programs so it gets built and included in the file system:

    $U/_ps\

---

Locking Strategy and Synchronization Approach
---------------------------------------------

The main shared data structure I access in the kernel is the global process table proc[]. Multiple kernel functions can access or modify this table, so walking it without proper locking could lead to races or inconsistent data.

To make getprocs() thread-safe, I reuse the same lock that xv6 already uses to protect the process table in wait(), which is wait_lock.

My locking strategy inside sys_getprocs() is:

1. Read and validate the user-provided max_procs argument (using argint).
   - If max <= 0, return -1 immediately (invalid argument).
   - If max > NPROC, clamp it down to NPROC so I never write past the size of the local buffer.

2. Acquire wait_lock before iterating over the proc[] array:

   acquire(&wait_lock);

3. While holding wait_lock, iterate over proc[] and for each non-UNUSED process:
   - Read its fields (pid, parent, state, sz, name).
   - Store them into the next element of a local kernel array struct procinfo buf[NPROC].

4. Release wait_lock after the loop is done:

   release(&wait_lock);

At this point I have a consistent snapshot of process information stored in buf[]. By holding wait_lock only while I’m reading from proc[] (and not while copying data out to user space), I avoid data races but also avoid holding the lock longer than necessary.

---

Memory Management and copyout Logic
-----------------------------------

The user-visible interface of the syscall is:

    int getprocs(struct procinfo *pinfo, int max_procs);

The kernel implementation is sys_getprocs() in kernel/sysproc.c. It follows the usual xv6 pattern for retrieving syscall arguments:

- argaddr(0, &uaddr); – reads the first argument (the user pointer pinfo) into a uint64 uaddr.
- argint(1, &max);   – reads the second argument (max_procs) into an int max.

Important points about memory and safety:

- The kernel never dereferences the user pointer directly. Instead it allocates a temporary array in kernel memory:

    struct procinfo buf[NPROC];

- After clamping max and acquiring wait_lock, sys_getprocs() fills entries in buf[] with information from the process table:

    buf[count].pid   = p->pid;
    buf[count].ppid  = p->parent ? p->parent->pid : 0;
    buf[count].state = p->state;
    buf[count].sz    = p->sz;
    safestrcpy(buf[count].name, p->name, sizeof(buf[count].name));

- Once the kernel buffer is filled and the lock is released, I copy the data from kernel space to user space using copyout():

    if (copyout(myproc()->pagetable,
                uaddr,
                (char *)buf,
                count * sizeof(struct procinfo)) < 0)
      return -1;

  copyout() uses the calling process’s page table (myproc()->pagetable) to translate the user virtual address. If the user address is invalid or not writable, copyout() returns < 0, and I treat that as an error: sys_getprocs() returns -1.

- On success, sys_getprocs() returns the actual number of entries written (count). This number is at most max and at most NPROC.

This design respects memory isolation: the kernel never directly dereferences user pointers and only copies data into user memory through copyout().

---

Testing Approach and Test Cases
-------------------------------

I tested the implementation interactively from the xv6 shell. My goals were to verify:

- The new syscall was wired correctly.
- ps produced the expected table.
- The output reflected changes in the process table as new processes were created and as they exited.
- There were no crashes or obvious inconsistencies.

Test 1 – Basic ps right after boot
----------------------------------

Commands:

    $ ps

Expected / observed:

- The header row is printed:

    PID    PPID   STATE      SIZE       NAME

- At least three processes appear, similar to:

    1      0      SLEEPING   16384      init
    2      1      SLEEPING   20480      sh
    3      2      RUNNING    16384      ps

- init has PPID = 0, sh has PPID = 1, and ps has PPID = 2.
- The STATE column shows valid strings like SLEEPING and RUNNING.

This confirmed that getprocs() returns reasonable information and that ps correctly decodes and prints it.

Test 2 – Additional processes (logstress and grind)
---------------------------------------------------

Commands:

    $ logstress &
    $ grind &
    $ ps

Observed example output (shortened):

    PID    PPID   STATE      SIZE       NAME
    1      0      SLEEPING   16384      init
    2      1      SLEEPING   20480      sh
    17     1      SLEEPING   20480      grind
    18     17     SLEEPING   20480      grind
    19     18     RUNNABLE   20480      grind
    20     18     SLEEPING   26491      grind
    125    2      RUNNING    16384      ps
    142    20     RUNNABLE   26491      grind

This shows:

- Multiple grind processes appear with various states (SLEEPING, RUNNABLE).
- Their PPIDs form a tree rooted at sh as expected.
- init and sh remain present, and ps appears as a running process.

This test demonstrates that getprocs() correctly returns information about many processes and that the table output stays consistent when new processes are created.

Test 3 – Repeated calls to ps
-----------------------------

Commands:

    $ ps
    $ ps
    $ ps

Observed:

- No crashes or strange values.
- The table was printed correctly each time.
- PIDs and PPIDs were consistent across runs, apart from new processes that were spawned by grind or the new ps invocation.

This shows that getprocs() can be safely called multiple times.

Test 4 – Argument validation and bounds
---------------------------------------

Code-level behavior checked in sys_getprocs():

- If max_procs <= 0, the function immediately returns -1.
- If max_procs > NPROC, it is clamped down to NPROC before filling the kernel buffer.
- The number of entries actually written and returned (count) is at most max_procs.

This ensures that the kernel does not write past either the kernel buffer or the user buffer, and that invalid values for max_procs are treated as errors.

---

Usage Instructions
------------------

Building:

    $ cd xv6-riscv
    $ make qemu

This compiles the kernel and boots xv6 under QEMU.

After boot, you will see:

    xv6 kernel is booting

    init: starting sh
    $

Running ps:

At the xv6 shell prompt, run:

    $ ps

This prints a table of processes:

    PID    PPID   STATE      SIZE       NAME
    1      0      SLEEPING   16384      init
    2      1      SLEEPING   20480      sh
    3      2      RUNNING    16384      ps
    ...

- PID: process ID.
- PPID: parent process ID (0 for init).
- STATE: one of UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, or ZOMBIE.
- SIZE: size of the process’s memory in bytes.
- NAME: process name (up to 16 characters).

To see more processes, you can run other programs and then call ps again:

    $ grind &
    $ ps

The new processes will appear as additional rows in the table, showing how getprocs() exposes the current state of the xv6 process table to user space.
