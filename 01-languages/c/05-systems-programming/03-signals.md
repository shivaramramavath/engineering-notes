# Signals and Asynchronous Handlers

> Master POSIX signal architecture, signal delivery and pending masks, reliable handling with `sigaction`, signal blocking with `sigprocmask`, asynchronous signal safety, and clean shutdowns with `volatile sig_atomic_t`.

| Metadata | Details |
| :--- | :--- |
| **Level** | Systems / Advanced |
| **Prerequisites** | [01-file-descriptors-and-posix-io.md](./01-file-descriptors-and-posix-io.md), [02-processes-fork-exec-wait.md](./02-processes-fork-exec-wait.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| POSIX.1-2008 / ISO C11 (Linux kernel, glibc, POSIX.1b real-time signals) |

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
Signals are software interrupts sent by the operating system kernel or other processes to notify a program of asynchronous events. Common triggers include hardware exceptions (e.g., dividing by zero triggering `SIGFPE`, memory access violations triggering `SIGSEGV`), user terminal requests (`Ctrl+C` sending `SIGINT`), timer expirations (`SIGALRM`), or child termination (`SIGCHLD`).

Signals interrupt normal CPU execution flow at arbitrary instruction boundaries. Writing signal handlers in C requires extreme discipline: calling non-reentrant functions (such as `malloc()` or `printf()`) from within a signal handler can corrupt heap metadata or deadlock the process. POSIX `sigaction` provides reliable, well-defined signal delivery semantics to replace legacy, unsafe `signal()` behavior.

---

## 2. Core Concepts
- **Signal Disposition**: The action taken upon receipt of a signal:
  - Default action (`SIG_DFL`): Terminate, dump core, ignore, or stop process.
  - Ignore (`SIG_IGN`): Discard the signal silently. Note: `SIGKILL` and `SIGSTOP` cannot be ignored or caught.
  - User-defined handler: Execute a designated function pointer.
- **Pending and Blocked Signals**: A signal is *generated* when the event occurs, *pending* while queued by the kernel, and *delivered* when the handler executes. If a signal is *blocked* via the process signal mask, it remains pending until explicitly unmasked.
- **Async-Signal-Safe Functions**: Functions guaranteed by POSIX to be safely callable from within a signal handler without inducing deadlocks or data corruption. Safe functions include `write()`, `_exit()`, `read()`, and `close()`. Unsafe functions include `malloc()`, `free()`, `printf()`, `fopen()`, and any function acquiring a mutex.
- **`volatile sig_atomic_t`**: An integer type defined in `<signal.h>` guaranteed to be read and written atomically in a single CPU instruction without tearing. The `volatile` qualifier prevents the compiler from caching its value in a CPU register.

---

## 3. How It Works Internally

### Kernel Signal Delivery Mechanism
When a signal is generated for a target process:
1. The kernel updates the process's `task_struct` signal bitmap, setting the corresponding pending bit.
2. When the target process is scheduled and prepares to return from kernel space to user space (e.g. following a system call or hardware timer interrupt), the kernel inspects the pending signal mask against the blocked signal mask.
3. If an unblocked signal is pending:
   - The kernel constructs a signal stack frame on the user stack, saving CPU registers (instruction pointer `RIP`, stack pointer `RSP`, flags).
   - It overwrites `RIP` with the address of the user's signal handler and sets the return address to a kernel trampoline stub (`__restore_rt`).
   - The CPU transitions to user mode and begins executing the signal handler.
4. When the handler returns, it calls the `sigreturn()` system call, which restores the original user CPU registers from the stack frame and resumes the interrupted program.

```
User Execution Flow                     Kernel Space                         Signal Handler Flow
+-------------------+                                                        +-------------------+
| Instruction A     |                                                        | Handler begins    |
| Instruction B     |                                                        | Flag = 1          |
| [Interrupt/Syscall|-------------> Check Pending Signals                    | Return            |
+-------------------+               Pending bit is SET                       +-------------------+
                                    Construct User Stack Frame                         |
                                    Set RIP = &handler                                 |
                                    Set Ret = sigreturn()                              |
                                           |                                           |
                                           +-------------------------------------------+
                                                                                       |
                                    sigreturn() Syscall Invoked <----------------------+
                                    Restore CPU registers
                                    Clear pending mask
                                           |
+-------------------+                      |
| Instruction C     |<---------------------+
| (Program Resumes) |
+-------------------+
```

---

## 4. Syntax & Usage

### Primary Header Inclusions
```c
#include <signal.h>
#include <unistd.h>
#include <string.h>
```

### Signature Overview
```c
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, int signum);
int sigdelset(sigset_t *set, int signum);
int sigismember(const sigset_t *set, int signum);
```

### `struct sigaction` Layout
```c
struct sigaction {
    void     (*sa_handler)(int);                        /* Pointer to signal handler */
    void     (*sa_sigaction)(int, siginfo_t *, void *); /* 3-arg handler if SA_SIGINFO */
    sigset_t   sa_mask;                                 /* Mask of signals to block during handler */
    int        sa_flags;                                /* Modifiers: SA_RESTART, SA_SIGINFO, etc. */
    void     (*sa_restorer)(void);                      /* Obsolete / internal */
};
```

---

## 5. Practical Examples

### Example 1: Clean Asynchronous Signal Shutdown
```c
#include <stdio.h>
#include <signal.h>

static volatile sig_atomic_t sig_received = 0;

void handle_sigint(int signum) {
    sig_received = signum;
}

int main(void) {
    signal(SIGINT, handle_sigint);
    printf("Signal handler registered for SIGINT (signum=%d)\n", SIGINT);
    printf("Current sig_received flag: %d\n", (int)sig_received);

    printf("Simulating signal arrival via raise(SIGINT)...\n");
    raise(SIGINT);

    printf("Signal delivered! sig_received flag: %d\n", (int)sig_received);
    if (sig_received == SIGINT) {
        printf("Clean asynchronous signal shutdown executed successfully.\n");
    }
    return 0;
}
```

```text
Signal handler registered for SIGINT (signum=2)
Current sig_received flag: 0
Simulating signal arrival via raise(SIGINT)...
Signal delivered! sig_received flag: 2
Clean asynchronous signal shutdown executed successfully.
```

### Example 2: Robust Signal Registration with `sigaction` and `SA_RESTART`
```c
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

static volatile sig_atomic_t shutdown_requested = 0;

static void sigint_handler(int signum) {
    (void)signum;
    shutdown_requested = 1;
}

int setup_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    /* Automatically restart interrupted system calls */
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) < 0) {
        return -1;
    }
    if (sigaction(SIGTERM, &sa, NULL) < 0) {
        return -1;
    }
    return 0;
}
```

---

## 6. Best Practices
1. **Never Call Non-Async-Signal-Safe Functions**: Do not call `printf`, `malloc`, `free`, `exit`, `syslog`, or any function that locks mutexes inside a signal handler. Use the self-pipe trick or write to a `volatile sig_atomic_t` flag.
2. **Always Use `sigaction` Over `signal`**: The ISO C `signal()` function has implementation-defined semantics across platforms: on some OSs it resets the disposition to `SIG_DFL` after one trigger; on others it does not block the signal during handler execution.
3. **Use `SA_RESTART` Consciously**: `SA_RESTART` causes interrupted system calls (`read`, `write`, `accept`) to resume automatically rather than failing with `EINTR`. If you want a signal to interrupt blocking I/O immediately to abort a loop, omit `SA_RESTART`.
4. **Block Signals During Critical Sections**: If a data structure is updated by main code and inspected by a signal handler, mask the signal using `sigprocmask()` around the update to prevent partial updates.
5. **Handle `EINTR`**: If not using `SA_RESTART`, check `if (errno == EINTR)` in all blocking system calls and retry.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Calling `printf()` inside a signal handler | `printf()` acquires an internal buffer lock; if the program was interrupted inside `printf`, the handler deadlocks | Use low-level `write(STDERR_FILENO, msg, len)` or set an atomic flag |
| Calling `malloc()` or `free()` in a handler | Heap metadata corruption or deadlock if interrupted while `malloc` owned the heap bucket lock | Pre-allocate all memory before registering handlers |
| Using plain `int flag` instead of `volatile sig_atomic_t` | Compiler optimizes loop by caching the variable in a CPU register, never observing the update made by the handler | Declare as `static volatile sig_atomic_t flag = 0;` |
| Assuming signals queue up | Standard POSIX signals do not queue; 10 identical signals sent in quick succession result in a single handler invocation | Never use signals as counters; treat them as state transition triggers |
| Forgetting to initialize `sa_mask` | Uninitialized stack garbage causes random signals to be blocked during handler execution | Always call `sigemptyset(&sa.sa_mask)` before `sigaction()` |
| Ignoring `SIGPIPE` in network daemons | Writing to a closed socket terminates the entire server process with `SIGPIPE` by default | Set `SIGPIPE` to `SIG_IGN` during daemon initialization: `signal(SIGPIPE, SIG_IGN)` |

---

## 8. Performance & Security Notes
- **The Self-Pipe Trick**: In high-performance event loops (`select`, `poll`, `epoll`), signal handlers should not perform heavy logic. Instead, create an anonymous pipe via `pipe()`, set the write end to non-blocking, and have the signal handler execute a single 1-byte `write(self_pipe[1], "x", 1)`. The main event loop monitors the read end of the pipe, turning asynchronous signals into synchronous I/O events.
- **Signal Stack Exhaustion (`sigaltstack`)**: If a process crashes due to stack overflow (`SIGSEGV`), the kernel cannot construct the signal stack frame on the already exhausted stack, causing instant termination. Register an alternate signal stack with `sigaltstack()` and pass `SA_ONSTACK` to reliably handle stack overflows.
- **Signal-Based Race Conditions (TOCTOU)**: Unmasking a signal right before a blocking call (like `pause()`) contains a race condition where the signal arrives between unmasking and `pause()`, deadlocking the process. Use `sigsuspend()` to atomically unmask and wait.

---

## 9. Comparison / When to Use What

| Feature | Legacy `signal()` | Modern `sigaction()` | Real-Time Signals (`SIGRTMIN`..`SIGRTMAX`) |
| :--- | :--- | :--- | :--- |
| **Portability** | Universal ANSI C | POSIX.1-2001+ | POSIX.1b Real-Time Extensions |
| **Queueing** | No (lost duplicates) | No (lost duplicates) | **Yes** (strict FIFO queueing) |
| **Data Payload** | Signal number only | Can pass `siginfo_t` | Can pass `sigval` union (`sigqueue`) |
| **Restart Behavior**| Implementation-defined | Configurable via `SA_RESTART` | Configurable |
| **Handler Blocking**| Platform-dependent | Explicit `sa_mask` | Explicit `sa_mask` |

---

## 10. Interview Questions

<details>
<summary>1. What makes a function "async-signal-safe"?</summary>
An async-signal-safe function is either reentrant or non-interruptible by signals. It must not modify static or global non-atomic state without protection, must not acquire mutexes or spinlocks that could already be held by the interrupted thread, and must not invoke memory allocators (`malloc`/`free`). POSIX explicitly enumerates all async-signal-safe functions in the `signal.h` specification.
</details>

<details>
<summary>2. Why does calling `malloc()` in a signal handler risk deadlock?</summary>
General-purpose allocators protect heap metadata (bins, chunks, free lists) with internal synchronization locks. If the application is interrupted by a signal while executing `malloc()` (holding the heap lock), and the signal handler attempts to allocate memory with `malloc()`, it attempts to acquire the exact lock already held by the interrupted context on the same thread, causing an inescapable deadlock.
</details>

<details>
<summary>3. What is the purpose of the `volatile` keyword in `volatile sig_atomic_t`?</summary>
`sig_atomic_t` guarantees that reading and writing the integer occurs in a single atomic memory instruction without tearing. The `volatile` qualifier instructs the C compiler that the variable's value can change asynchronously outside the compiler's static analysis, disabling register caching optimizations and forcing a physical memory read/write on every access.
</details>

<details>
<summary>4. How does `sigsuspend()` eliminate race conditions?</summary>
If code unblocks a signal with `sigprocmask()` and then calls `pause()`, a context switch or signal delivery between those two instructions can trigger the handler before `pause()` is entered, causing `pause()` to block indefinitely waiting for a signal that has already been delivered. `sigsuspend(&mask)` atomically sets the signal mask and puts the thread to sleep in a single atomic kernel transition.
</details>

<details>
<summary>5. What are the differences between standard signals and real-time signals?</summary>
1. **Queueing**: Real-time signals (`SIGRTMIN` to `SIGRTMAX`) are queued by the kernel; multiple occurrences are delivered in order, whereas standard signals are lost if duplicate signals arrive before delivery.
2. **Order**: Lower-numbered real-time signals are delivered with higher priority.
3. **Payload**: Real-time signals can carry an integer or pointer payload using `sigqueue()`.
</details>

<details>
<summary>6. Can `SIGKILL` or `SIGSTOP` be caught or ignored? Why?</summary>
No. The POSIX specification strictly forbids catching, blocking, or ignoring `SIGKILL` and `SIGSTOP`. This guarantees that the operating system administrator and kernel always retain an infallible mechanism to terminate runaway or unresponsive processes and suspend tasks.
</details>

<details>
<summary>7. What happens if a signal handler is executing and the same signal arrives again?</summary>
By default with `sigaction`, the signal currently being handled is automatically added to the thread's signal mask (`sa_mask`) during handler execution, preventing nested recursive invocations of the same handler. Once the handler returns, the signal is unmasked.
</details>

<details>
<summary>8. What is the "self-pipe trick" and why is it used?</summary>
The self-pipe trick integrates asynchronous signal handling into synchronous I/O multiplexers (`select`, `poll`, `epoll`). The handler writes a single byte to an anonymous pipe. The main loop, which is blocked waiting for socket events, wakes up because the pipe descriptor is readable, allowing signal handling logic to be executed cleanly within the main event loop thread without async-safety restrictions.
</details>

---

## 11. Practice Exercises

### Easy
1. **Signal Name Printer**: Write a function that converts standard signal integers (`SIGINT`, `SIGTERM`, `SIGSEGV`) to their standard string names.
2. **SIGINT Counter**: Register a `SIGINT` handler that increments a `volatile sig_atomic_t` counter and exits after 3 presses.
3. **Alarm Timer**: Write a program that sets an alarm for 2 seconds using `alarm(2)` and waits with `pause()`.

<details>
<summary>Solutions (Easy)</summary>

```c
/* Exercise 1 */
#include <signal.h>
const char *get_sig_name(int sig) {
    switch (sig) {
        case SIGINT: return "SIGINT";
        case SIGTERM: return "SIGTERM";
        case SIGSEGV: return "SIGSEGV";
        case SIGALRM: return "SIGALRM";
        default: return "UNKNOWN";
    }
}

/* Exercise 2 */
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
static volatile sig_atomic_t sigint_count = 0;
void count_sigint(int s) {
    (void)s;
    sigint_count++;
}
void run_counter(void) {
    struct sigaction sa = {0};
    sa.sa_handler = count_sigint;
    sigaction(SIGINT, &sa, NULL);
    while (sigint_count < 3) {
        pause();
    }
    printf("Received 3 SIGINTs. Exiting.\n");
}
```
</details>

### Medium
4. **Reliable Graceful Shutdown**: Implement a server main loop that sleeps for 1 second in a loop and terminates cleanly when either `SIGINT` or `SIGTERM` is received.
5. **Signal Masking Critical Section**: Write a function that safely modifies a shared data structure by masking all signals during modification and restoring the previous mask.
6. **Async-Signal-Safe String Logger**: Implement a logging function `void safe_log(const char *msg)` that uses only `write(STDERR_FILENO, ...)` and manual length calculation.

<details>
<summary>Solutions (Medium)</summary>

```c
/* Exercise 5: Critical Section with Signal Masking */
#include <signal.h>

void critical_section(void (*task)(void)) {
    sigset_t block_all, old_mask;
    sigfillset(&block_all);
    
    /* Block all signals */
    sigprocmask(SIG_BLOCK, &block_all, &old_mask);
    
    /* Execute critical work without interruption */
    task();
    
    /* Restore original signal mask */
    sigprocmask(SIG_SETMASK, &old_mask, NULL);
}

/* Exercise 6: Async-Signal-Safe Logger */
#include <unistd.h>

void safe_log(const char *msg) {
    if (!msg) return;
    size_t len = 0;
    while (msg[len] != '\0') len++;
    write(STDERR_FILENO, msg, len);
}
```
</details>

### Hard
7. **Complete Self-Pipe Implementation**: Build a minimal event loop with `pipe()`, `O_NONBLOCK`, and `sigaction` that forwards `SIGUSR1` and `SIGINT` through the pipe to process them in the main thread.
8. **Crash Stack Tracer (`SIGSEGV` Handler with `sigaltstack`)**: Set up a dedicated 64 KB alternate signal stack using `sigaltstack()`, register a `SIGSEGV` handler with `SA_ONSTACK`, and write a crash notification message to `STDERR_FILENO` before calling `_exit(1)`.
9. **Real-Time Signal Queue Processor**: Write a producer-consumer model where process A sends 5 queued messages with payloads to process B using `sigqueue()`, and process B retrieves each payload via `SA_SIGINFO`.

<details>
<summary>Solutions (Hard)</summary>

```c
/* Exercise 8: Alternate Signal Stack for Stack Overflow Catching */
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

static void crash_handler(int sig) {
    const char msg[] = "CRASH: Caught SIGSEGV on alternate stack! Terminating.\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
    _exit(EXIT_FAILURE);
}

void install_crash_protection(void) {
    stack_t alt_stack;
    alt_stack.ss_sp = malloc(SIGSTKSZ);
    if (!alt_stack.ss_sp) return;
    alt_stack.ss_size = SIGSTKSZ;
    alt_stack.ss_flags = 0;

    sigaltstack(&alt_stack, NULL);

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = crash_handler;
    sa.sa_flags = SA_ONSTACK;

    sigaction(SIGSEGV, &sa, NULL);
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Dispositions**: `SIG_DFL` (default), `SIG_IGN` (ignore), or function pointer.
- **Uncatchable Signals**: `SIGKILL` (force kill), `SIGSTOP` (force pause).
- **Registration**: Always use `sigaction(signum, &sa, NULL)`.
- **Async-Signal-Safety**: Only call reentrant functions (`write`, `_exit`). Never call `malloc` or `printf`.
- **Atomic Flag**: Use `static volatile sig_atomic_t flag = 0;`.
- **Signal Masking**: `sigprocmask(SIG_BLOCK, &set, &oldset)` protects critical sections.
- **Self-Pipe Trick**: Write 1 byte to non-blocking pipe inside handler to notify main event loop safely.

---

## 13. Further Reading
- W. Richard Stevens, *Advanced Programming in the UNIX Environment*, Chapter 10 (Signals).
- Michael Kerrisk, *The Linux Programming Interface*, Chapters 20, 21, & 22 (Signals).
- CERT C Coding Standard: SIG30-C (Call only asynchronous-safe functions within signal handlers).

---

[← Previous Note: Processes (fork, exec, wait)](./02-processes-fork-exec-wait.md) | [Module Index](README.md) | [Next Note: Pipes and IPC](./04-pipes-and-ipc.md)
