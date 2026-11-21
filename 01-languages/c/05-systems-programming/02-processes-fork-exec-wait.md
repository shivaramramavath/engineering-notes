# Processes: fork, exec, and wait

> Master the Unix process model, process creation with `fork()`, memory virtual address spaces and Copy-on-Write (COW), binary execution with `execve()`, child process reaping via `waitpid()`, and zombie/orphan lifecycle management.

| Metadata | Details |
| :--- | :--- |
| **Level** | Systems / Advanced |
| **Prerequisites** | [01-file-descriptors-and-posix-io.md](./01-file-descriptors-and-posix-io.md), [04-advanced/05-abi-and-calling-conventions.md](../04-advanced/05-abi-and-calling-conventions.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| POSIX.1-2008 / ISO C11 (Linux kernel 2.6+, glibc, MinGW / WSL cross-platform notes) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why
In Unix-like operating systems, processes are the fundamental unit of isolation, resource accounting, and execution. Unlike threads, which share a single virtual address space, each process possesses a private address space, file descriptor table, signal handling table, and credentials (UID/GID).

Rather than providing a single complex API to launch a new program with specific privileges, environment, and file descriptors, Unix split program creation into two orthogonal primitives:
1. `fork()` clones the calling process, duplicating its virtual address space, file descriptor table, and state.
2. `execve()` replaces the calling process's current program image, heap, stack, and text with a new executable loaded from disk.

Between `fork()` and `execve()`, the child process executes within its own private address space, allowing the developer to alter standard file descriptors (I/O redirection), drop privileges, set process groups, or configure signal masks safely before starting the new program.

---

## 2. Core Concepts
- **PID and PPID**: Every process is uniquely identified by a Process ID (`pid_t`, returned by `getpid()`). The creating process is the Parent Process ID (`getppid()`). The root ancestor is PID 1 (`init` / `systemd`).
- **Copy-on-Write (COW)**: Modern kernels do not duplicate physical RAM pages when `fork()` is called. Instead, the page table entries are copied and marked read-only. When either parent or child writes to a page, a hardware page fault triggers the kernel to allocate and copy that single 4 KB page.
- **Zombie Process (`<defunct>`)**: When a child process terminates (`exit()`), the kernel frees its address space and open files, but retains its entry in the process table (containing its PID, termination status, and resource usage metrics) until the parent reaps it via `wait()` or `waitpid()`. An unreaped terminated child is a zombie.
- **Orphan Process**: If a parent process terminates before its child, the child becomes an orphan. The kernel automatically re-parents orphaned children to PID 1 (`systemd`/`init`), which periodically calls `waitpid()` to reap them.
- **Exit Status Macros**:
  - `WIFEXITED(status)`: Evaluates to true if child exited normally via `exit()` or `return` from `main`.
  - `WEXITSTATUS(status)`: Retrieves the low-order 8 bits of the exit status code.
  - `WIFSIGNALED(status)`: True if child was terminated by an unhandled signal (e.g. `SIGSEGV`, `SIGKILL`).
  - `WTERMSIG(status)`: Returns the signal number that caused the termination.

---

## 3. How It Works Internally

### Process Creation and Address Space Cloning (Copy-on-Write)
```
Parent Virtual Memory                          Physical RAM Pages                          Child Virtual Memory
+-----------------------+                    +---------------------+                    +-----------------------+
| Text (Code)           |------------------->| Page 0x1000 (R-X)   |<-------------------| Text (Code)           |
+-----------------------+                    +---------------------+                    +-----------------------+
| Data / Globals        |----+               | Page 0x2000 (R-O)   |<----+              | Data / Globals        |
+-----------------------+    +-------------->| [Marked Read-Only]  |     +------------->+-----------------------+
| Heap                  |----+               +---------------------+     |              | Heap                  |
+-----------------------+                    | Page 0x3000 (R-O)   |<----+              +-----------------------+
| Stack                 |                    +---------------------+                    | Stack                 |
+-----------------------+                                                               +-----------------------+
                                                          |
                                           [Child writes to Stack page]
                                                          |
                                                          v (Hardware MMU Page Fault)
                                            Kernel allocates Page 0x7000 (RW)
                                            Copies original contents from 0x3000
                                            Child page table updated to Page 0x7000
```

When `fork()` executes:
1. The kernel allocates a new `task_struct` (in Linux) and assigns a unique PID.
2. It duplicates the parent's file descriptor table, incrementing reference counts on open file descriptions.
3. It copies parent page tables and marks all writable pages with the `COW` bit and read-only permissions in the MMU.
4. `fork()` returns twice:
   - Returns **0** in the context of the newly created child process.
   - Returns the child's **PID** (> 0) in the context of the parent process.
   - Returns **-1** on failure (e.g. system PID limit reached).

---

## 4. Syntax & Usage

### Primary Header Inclusions
```c
#include <unistd.h>   /* fork, execve, getpid, getppid */
#include <sys/types.h>/* pid_t */
#include <sys/wait.h> /* wait, waitpid, WIFEXITED, etc. */
```

### Signature Overview
```c
pid_t fork(void);
pid_t wait(int *wstatus);
pid_t waitpid(pid_t pid, int *wstatus, int options);
int execve(const char *pathname, char *const argv[], char *const envp[]);
int execlp(const char *file, const char *arg, ... /* (char *) NULL */);
```

---

## 5. Practical Examples

### Example 1: Process State Lifecycle and Reaping Simulation
```c
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    PROC_EMBRYO,
    PROC_RUNNING,
    PROC_ZOMBIE,
    PROC_TERMINATED
} ProcState;

typedef struct {
    int pid;
    int ppid;
    ProcState state;
    int exit_code;
} ProcessControlBlock;

const char *state_str(ProcState s) {
    switch (s) {
        case PROC_EMBRYO: return "EMBRYO";
        case PROC_RUNNING: return "RUNNING";
        case PROC_ZOMBIE: return "ZOMBIE (Defunct)";
        case PROC_TERMINATED: return "REAPED/TERMINATED";
        default: return "UNKNOWN";
    }
}

int main(void) {
    ProcessControlBlock parent = { .pid = 1001, .ppid = 1, .state = PROC_RUNNING, .exit_code = 0 };
    ProcessControlBlock child  = { .pid = 1002, .ppid = 1001, .state = PROC_EMBRYO, .exit_code = 0 };

    printf("Parent PID: %d, Initial State: %s\n", parent.pid, state_str(parent.state));
    child.state = PROC_RUNNING;
    printf("Child PID: %d created (fork). State: %s, PPID: %d\n", child.pid, state_str(child.state), child.ppid);

    // Child terminates with status code 42
    child.exit_code = 42;
    child.state = PROC_ZOMBIE;
    printf("Child exited with code %d. State: %s\n", child.exit_code, state_str(child.state));

    // Parent reaps the child
    printf("Parent waiting for child PID %d...\n", child.pid);
    int status = child.exit_code;
    child.state = PROC_TERMINATED;
    printf("Reaped child PID %d, WIFEXITED=1, WEXITSTATUS=%d. State: %s\n", child.pid, status, state_str(child.state));
    return 0;
}
```

```text
Parent PID: 1001, Initial State: RUNNING
Child PID: 1002 created (fork). State: RUNNING, PPID: 1001
Child exited with code 42. State: ZOMBIE (Defunct)
Parent waiting for child PID 1002...
Reaped child PID 1002, WIFEXITED=1, WEXITSTATUS=42. State: REAPED/TERMINATED
```

### Example 2: Classic Fork-Exec Pipeline with Waitpid
```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

void spawn_child_command(void) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        /* Child Process Context */
        char *args[] = {"/bin/echo", "Hello from spawned child process", NULL};
        execv(args[0], args);
        
        /* If execv returns, an error occurred */
        perror("execv failed");
        _exit(127); /* Use _exit in child after failed exec */
    } else {
        /* Parent Process Context */
        int status;
        pid_t reaped = waitpid(pid, &status, 0);
        if (reaped > 0) {
            if (WIFEXITED(status)) {
                printf("Child PID %d exited cleanly with code: %d\n", reaped, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("Child PID %d killed by signal: %d\n", reaped, WTERMSIG(status));
            }
        }
    }
}
```

---

## 6. Best Practices
1. **Always Use `_exit()` or `_Exit()` in Child on Exec Failure**: Never call standard `exit()` in the child if `exec()` fails; `exit()` flushes and closes user-space stdio buffers that are shared with the parent, causing duplicated output or closed file handles in the parent.
2. **Prevent Zombie Accumulation**: Every `fork()` call must be matched by a corresponding `waitpid()`. For asynchronous child processing, establish a `SIGCHLD` handler that calls `waitpid(-1, &status, WNOHANG)` in a loop.
3. **Check for `fork()` Failure**: `fork()` can fail if the system reaches `RLIMIT_NPROC` or PID exhaustions. Always handle `pid < 0`.
4. **Pass NULL Terminated Arrays to Exec**: `argv` and `envp` arrays passed to `execve` must terminate with a `NULL` pointer. Omitting the sentinel causes memory access violations.
5. **Close Unneeded Descriptors in Child**: Before invoking `execve()`, close all file descriptors not explicitly intended for the child, or mark them with `O_CLOEXEC`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Forgetting to reap children | Terminated children remain in the kernel process table as zombies (`defunct`), consuming PIDs until the system exhausts PIDs | Call `waitpid()` or handle `SIGCHLD` with a non-blocking loop |
| Calling `wait(&status)` when expecting a specific child | `wait()` reaps *any* child that terminates first, causing race conditions in multi-child architectures | Use `waitpid(target_pid, &status, 0)` |
| Flawed `SIGCHLD` handler calling `waitpid` once | Standard Unix signals do not queue; multiple children terminating simultaneously generate only a single `SIGCHLD` signal | Call `while (waitpid(-1, &status, WNOHANG) > 0)` inside the signal handler |
| Calling `exit()` instead of `_exit()` after failed `exec` | `exit()` runs `atexit` handlers and flushes stdio streams shared with parent, causing duplicate output or side effects | Call `_exit(127)` or `_Exit(127)` immediately |
| Assuming `fork()` duplicates open file offsets | Open file descriptions are shared between parent and child; child writes advance the file offset seen by the parent | Duplicate descriptors with `open()` or use `pread()`/`pwrite()` for independent positions |
| Executing shell commands with `system()` with user inputs | Vulnerable to command injection vulnerabilities (e.g. `; rm -rf /`) | Use `fork()` + `execve()` passing arguments explicitly as a tokenized array |

---

## 8. Performance & Security Notes
- **Virtual Memory Fork Overhead**: Although Copy-on-Write eliminates copying physical page contents, `fork()` must duplicate the page table entries themselves. For processes with tens of gigabytes of allocated virtual memory (e.g. large Redis databases), duplicating page tables can take tens of milliseconds, inducing latency spikes. On modern Linux, `posix_spawn()` or `vfork()` can avoid page table duplication if `exec()` immediately follows.
- **Privilege Separation**: A secure architectural pattern involves a privileged parent process (running as `root` to bind to port 80/443), which calls `fork()`, drops all Linux capabilities and changes UID/GID in the child via `setuid()` / `setgid()`, and only then executes worker logic.
- **FD Leakage Attack**: If a parent process opens `/etc/shadow` or a database authentication socket and forks without `FD_CLOEXEC`, a hijacked child executing untrusted code can read or write to those descriptors directly.

---

## 9. Comparison / When to Use What

| Primitive | Mechanism | Memory Isolation | Use Case |
| :--- | :--- | :--- | :--- |
| `fork()` | Clones process with COW page tables | Fully isolated address space | Daemonization, worker process pools |
| `execve()` | Replaces current image with new executable | Discards old space, loads ELF binary | Running external binaries and CLI utilities |
| `posix_spawn()`| High-level combination of fork + exec | Avoids page table copying on some OSs | Standard POSIX alternative to fork-exec |
| `pthread_create()`| Creates thread within same process | Shared virtual address space | High-concurrency compute and shared memory |
| `system()` | Spawns `/bin/sh -c command` | Fork-exec running a shell interpreter | Quick scripts only; unsafe for untrusted input |

---

## 10. Interview Questions

<details>
<summary>1. How does Copy-on-Write (COW) work during `fork()`?</summary>
The kernel duplicates the parent's page table entries into the child rather than copying physical memory frames. All writable pages are marked read-only in both parent and child page tables. When either process attempts to write to a page, the CPU hardware MMU triggers a Page Fault exception. The OS kernel's page fault handler intercepts the trap, allocates a new physical frame, copies the 4 KB page data into it, updates the faulting process's page table to point to the new frame with read-write permissions, and resumes execution.
</details>

<details>
<summary>2. What is a Zombie process, and how does it differ from an Orphan process?</summary>
A zombie is a process that has completed execution via `exit()`, but still occupies an entry in the operating system process table because its parent has not yet read its exit status with `wait()` or `waitpid()`. It holds no physical memory, code, or open files—only kernel process accounting metadata. An orphan process is a running child whose parent process terminated before it; orphans are automatically adopted by PID 1 (`init`/`systemd`), which reaps them when they eventually exit.
</details>

<details>
<summary>3. Why must `waitpid(-1, &status, WNOHANG)` be called in a loop inside a `SIGCHLD` handler?</summary>
POSIX signals are standard bitmaps without queues: if three child processes exit simultaneously, the kernel sends only a single `SIGCHLD` signal to the parent. If the signal handler calls `waitpid()` once, only one child is reaped, leaving the remaining two as permanent zombies. A `while (waitpid(-1, &status, WNOHANG) > 0)` loop reaps all currently exited children until none remain.
</details>

<details>
<summary>4. What does `execve()` return on success?</summary>
`execve()` **never returns** on success because the calling process's text, heap, and stack are completely overwritten and replaced by the new binary. Control transfers directly to the entry point (`_start`) of the new program. `execve()` returns only on failure, returning `-1` and setting `errno`.
</details>

<details>
<summary>5. What is the difference between `execv`, `execl`, `execvp`, and `execlp`?</summary>
The letters indicate argument conventions:
- `l` (list): Arguments passed as a comma-separated variable list ending with `(char *)NULL`.
- `v` (vector): Arguments passed as a `NULL`-terminated array of pointers (`char *argv[]`).
- `p` (path): Searches the directories listed in the `PATH` environment variable for the executable.
- `e` (environment): Allows passing an explicit custom environment array (`char *envp[]`).
</details>

<details>
<summary>6. Why is `_exit()` preferred over `exit()` in the child branch after a failed `exec`?</summary>
Standard `exit()` calls registered `atexit()` cleanup handlers and flushes user-space stdio output buffers. Because the child inherited the parent's address space and stdio buffers, calling `exit()` would execute application cleanup handlers twice and could flush duplicate buffered data to disk or sockets. `_exit()` is a direct system call that terminates the process immediately without running user-space cleanups.
</details>

<details>
<summary>7. What happens to open file descriptors across a `fork()`?</summary>
All open file descriptors are duplicated into the child. The child receives descriptor numbers identical to the parent's, and both point to the *same underlying Open File Description* in the kernel. Consequently, they share the file offset: if the child reads 50 bytes, the parent's next read will begin at byte 51.
</details>

<details>
<summary>8. How can a parent detect whether a child was killed by a signal rather than exiting normally?</summary>
By examining the integer status populated by `waitpid()` using the macro `WIFSIGNALED(status)`. If true, the child died due to an unhandled signal, and `WTERMSIG(status)` returns the specific signal number (e.g. `SIGSEGV` for segmentation fault, `SIGKILL` for killed).
</details>

---

## 11. Practice Exercises

### Easy
1. **PID Inspector**: Write a program that prints its own PID, PPID, andUID.
2. **Simple Fork**: Write a program that forks a child; the child prints "I am child with PID X" and exits with 0; the parent waits for the child and prints "Child finished".
3. **Exec Whoami**: Write a small program that forks and executes `/usr/bin/whoami` (or `/bin/hostname`) using `execlp()`.

<details>
<summary>Solutions (Easy)</summary>

```c
/* Exercise 1 */
#include <stdio.h>
#include <unistd.h>
void print_ids(void) {
    printf("PID: %d, PPID: %d, UID: %d\n", getpid(), getppid(), getuid());
}

/* Exercise 2 */
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
void simple_fork(void) {
    pid_t pid = fork();
    if (pid == 0) {
        printf("I am child with PID %d\n", getpid());
        _exit(0);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
        printf("Child finished\n");
    }
}

/* Exercise 3 */
#include <unistd.h>
#include <sys/wait.h>
void exec_hostname(void) {
    if (fork() == 0) {
        execlp("hostname", "hostname", (char *)NULL);
        _exit(127);
    }
    wait(NULL);
}
```
</details>

### Medium
4. **Child Exit Code Checker**: Write a program that forks a child that exits with code 77. The parent must verify that `WIFEXITED` is true and `WEXITSTATUS` matches 77.
5. **Parallel Task Dispatcher**: Write a function `void run_parallel_jobs(int n)` that spawns `n` worker processes, each performing a calculation and exiting. The parent must reap all `n` children without leaving any zombies.
6. **Non-blocking Reaper**: Implement a loop in the parent that does background work and uses `waitpid(-1, &status, WNOHANG)` to reap children as they complete.

<details>
<summary>Solutions (Medium)</summary>

```c
/* Exercise 4: Child Exit Code Checker */
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

void check_exit_code(void) {
    pid_t pid = fork();
    if (pid == 0) {
        _exit(77);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 77) {
        printf("Success: Child exited with expected code 77\n");
    }
}

/* Exercise 5: Parallel Worker Dispatcher */
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

void run_parallel_jobs(int n) {
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            /* Child worker task */
            _exit(i * 2);
        }
    }
    /* Reap all children */
    int reaped = 0;
    int status;
    while (reaped < n) {
        pid_t p = waitpid(-1, &status, 0);
        if (p > 0) {
            printf("Reaped worker PID %d with exit code %d\n", p, WEXITSTATUS(status));
            reaped++;
        }
    }
}
```
</details>

### Hard
7. **Mini Process Supervisor (Watchdog)**: Implement a supervisor process that launches a worker binary. If the worker crashes (terminated by a signal or non-zero exit), the supervisor restarts it up to 5 times with exponential backoff.
8. **Double-Fork Daemon Spawner**: Implement a function `pid_t spawn_detached_worker(void (*worker_fn)(void))` that uses the double-fork technique to spawn a worker process whose parent immediately terminates, leaving the worker adopted by PID 1.
9. **Secure Shell Command Wrapper**: Implement a safe `int safe_system(const char *cmd, char *const argv[])` that avoids invoking `/bin/sh`, drops privileges if running setuid, and executes `execve` directly with environment cleansing.

<details>
<summary>Solutions (Hard)</summary>

```c
/* Exercise 8: Double-Fork Detached Worker */
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

void spawn_detached_worker(void (*worker_fn)(void)) {
    pid_t pid = fork();
    if (pid < 0) return;

    if (pid > 0) {
        /* First parent: wait for immediate child to terminate */
        waitpid(pid, NULL, 0);
        return;
    }

    /* First child: fork a second time */
    pid_t grandchild = fork();
    if (grandchild < 0) _exit(1);

    if (grandchild > 0) {
        /* First child exits immediately so grandchild is orphaned and adopted by PID 1 */
        _exit(0);
    }

    /* Grandchild context: fully detached */
    worker_fn();
    _exit(0);
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Forking**: `pid_t pid = fork()` clones process. Returns 0 in child, child PID in parent, -1 on error.
- **Copy-on-Write**: Pages are shared read-only until written to, avoiding physical RAM copying.
- **Executing**: `execve(path, argv, envp)` overwrites current process. Never returns on success.
- **Waiting**:
  - `waitpid(pid, &status, 0)`: Blocks until specific child terminates.
  - `waitpid(-1, &status, WNOHANG)`: Non-blocking check for any exited child.
- **Status Decoding**:
  - `WIFEXITED(s)`: True if normal termination.
  - `WEXITSTATUS(s)`: Returns 8-bit exit code.
  - `WIFSIGNALED(s)`: True if killed by signal (`WTERMSIG(s)`).
- **Zombies**: Terminated processes waiting for parent `waitpid()`.

---

## 13. Further Reading
- W. Richard Stevens, *Advanced Programming in the UNIX Environment*, Chapter 8 (Process Control).
- Michael Kerrisk, *The Linux Programming Interface*, Chapters 24, 25, 26, & 27.
- Linux Manual Pages: `man 2 fork`, `man 2 execve`, `man 2 waitpid`.

---

[← Previous Note: File Descriptors & POSIX I/O](./01-file-descriptors-and-posix-io.md) | [Module Index](README.md) | [Next Note: Signals & Asynchronous Handlers](./03-signals.md)
