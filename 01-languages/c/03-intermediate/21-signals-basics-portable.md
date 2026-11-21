# 21 Portable Signals Basics

> Master asynchronous signal handling in C, understand the critical concept of Async-Signal Safety, and coordinate graceful process shutdown using `volatile sig_atomic_t`.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [01-fundamentals/04-variables-constants.md](../01-fundamentals/04-variables-constants.md), [03-intermediate/16-inline-restrict-volatile.md](16-inline-restrict-volatile.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard** | C11 / C17 / C23 |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally](#3-how-it-works-internally)
4. [4. Syntax & Usage](#4-syntax--usage)
5. [5. Practical Examples](#5-practical-examples)
6. [6. Best Practices](#6-best-practices)
7. [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [8. Performance & Security Notes](#8-performance--security-notes)
9. [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [10. Interview Questions](#10-interview-questions)
11. [11. Practice Exercises](#11-practice-exercises)
12. [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [13. Further Reading](#13-further-reading)

---

## 1. What & Why

In operating systems engineering, a **signal** is an asynchronous software interrupt delivered by the OS kernel to a running process. Signals notify the process of critical hardware faults or external control events:
- **Hardware Traps**: Dereferencing an unmapped address (`SIGSEGV`), dividing an integer by zero (`SIGFPE`), executing an illegal opcode (`SIGILL`).
- **External Notifications**: User pressing Ctrl+C in terminal (`SIGINT`), process termination requested by system supervisor (`SIGTERM`), broken network pipe (`SIGPIPE`), child process termination (`SIGCHLD`).

Writing signal handlers in C is one of the most perilous tasks in systems programming:
- A signal handler can interrupt normal application code at **any arbitrary assembly instruction**.
- If a signal handler calls non-reentrant standard library functions (such as `printf`, `malloc`, or `free`), it can cause deadlocks and catastrophic heap corruption.

Understanding portable signal registration, `sig_atomic_t` flag synchronization, and async-signal safety is essential for building production-ready daemons and CLI tools.

---

## 2. Core Concepts

### 2.1 Standard Signals in ISO C (`<signal.h>`)
The ISO C standard mandates six standard signals across all platforms (both Windows and Linux):
1. `SIGINT`: Interactive attention (Ctrl+C).
2. `SIGTERM`: Termination request sent to the process.
3. `SIGABRT`: Abnormal termination (triggered by `abort()` or failed `assert()`).
4. `SIGFPE`: Erroneous arithmetic operation (integer division by zero, float overflow).
5. `SIGILL`: Illegal processor instruction or corrupt binary execution.
6. `SIGSEGV`: Invalid storage access (segmentation fault, null-pointer dereference).

### 2.2 The Async-Signal-Safe Principle
Because a signal handler interrupts the main thread mid-execution, **it must never invoke non-reentrant functions**:
- If the main thread was in the middle of executing `malloc()`, it holds an internal heap lock.
- If a signal arrives and the signal handler also calls `malloc()`, it attempts to acquire the same lock, resulting in an immediate **deadlock**!
- If the handler calls `printf()`, it can corrupt internal stdio stream buffers.

The **golden rule of signal handlers**:
> Inside a signal handler, do **nothing** except set a flag of type `volatile sig_atomic_t` or write to a self-pipe. All processing logic must occur in the main loop!

### 2.3 The `sig_atomic_t` Type
The ISO C standard defines `sig_atomic_t` as an integer type that can be read and written as an atomic entity, even in the presence of asynchronous signals:
- Always qualify it as `volatile sig_atomic_t` so that the compiler does not cache its value in a CPU register.
- The standard guarantees that operations on `sig_atomic_t` are atomic with respect to signals on the same thread.

---

## 3. How It Works Internally

### Kernel Interrupt and Stack Frame Redirection
When the operating system kernel delivers a signal to a process:
1. The kernel pauses user-space execution and saves current registers into the process kernel thread structure.
2. The kernel constructs a special "signal frame" on the user-space stack.
3. The kernel redirects the CPU's Instruction Pointer (`RIP`) to the entry point of the registered signal handler.
4. When the signal handler returns, it calls a kernel `sigreturn` system call that restores the original registers and stack, resuming the main program flow seamlessly.

```
Kernel Signal Interruption Timeline:
Normal Thread Execution:
... -> Instruction K -> Instruction K+1 -> Instruction K+2 -> ...
                            ▲
                            │ [OS Kernel Delivers Signal: SIGINT]
                            ▼
        ┌───────────────────────────────────────┐
        │ Signal Frame Allocated on Stack       │
        ├───────────────────────────────────────┤
        │ Handler Executes:                     │
        │   g_shutdown_requested = 1;           │
        ├───────────────────────────────────────┤
        │ sigreturn System Call (Restores CPU)  │
        └───────────────────┬───────────────────┘
                            │
                            ▼
... -> Resumes Instruction K+1 seamlessly!
```

---

## 4. Syntax & Usage

### 4.1 Registering Signals with `signal()`
Standard ISO C provides `signal()`:
```c
#include <signal.h>

void (*signal(int sig, void (*handler)(int)))(int);
```
Special handler macros include:
- `SIG_DFL`: Restore the default operating system action (usually process termination).
- `SIG_IGN`: Ignore the signal completely.
- `SIG_ERR`: Return value indicating registration failure.

### 4.2 Graceful Shutdown Flag Pattern
```c
#include <signal.h>
#include <stdbool.h>

static volatile sig_atomic_t g_keep_running = 1;

static void on_sigint(int sig) {
    (void)sig;
    g_keep_running = 0; /* Strictly safe */
}

void run_server(void) {
    signal(SIGINT, on_sigint);
    while (g_keep_running) {
        /* Process server events */
    }
    /* Orderly shutdown, flush logs, free resources */
}
```

---

## 5. Practical Examples

### Comprehensive Example: Graceful Signal Handling with `sig_atomic_t`
The following complete program demonstrates registering a signal handler for `SIGINT`, raising a simulated interrupt, safely setting an atomic flag, and restoring default handlers.

```c
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>

/* Global atomic shutdown flag: safe type for signal handlers */
static volatile sig_atomic_t g_signal_received = 0;

static void handle_user_signal(int sig) {
    /* STRICT RULE: Only set sig_atomic_t flags inside signal handlers! */
    g_signal_received = sig;
}

int main(void) {
    printf("=== Portable Signals Demonstration ===\n");

    /* Register SIGINT handler */
    void (*prev_handler)(int) = signal(SIGINT, handle_user_signal);
    if (prev_handler == SIG_ERR) {
        fprintf(stderr, "Failed to register SIGINT handler\n");
        return 1;
    }
    printf("Signal handler registered. Triggering mock signal with raise(SIGINT)...\n");

    raise(SIGINT);

    if (g_signal_received == SIGINT) {
        printf("Signal SIGINT (%d) successfully handled via sig_atomic_t flag.\n", (int)g_signal_received);
    } else {
        printf("Signal not received.\n");
    }

    /* Restore default signal handler */
    signal(SIGINT, SIG_DFL);
    printf("Signal handler restored to default.\n");
    return 0;
}
```

#### Compilation & Verified Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 21-signals-basics-portable.c -o 21-signals-basics-portable.exe
./21-signals-basics-portable.exe
```

```text
=== Portable Signals Demonstration ===
Signal handler registered. Triggering mock signal with raise(SIGINT)...
Signal SIGINT (2) successfully handled via sig_atomic_t flag.
Signal handler restored to default.
```

---

## 6. Best Practices

1. **Keep Signal Handlers Under 3 Lines of Code**: Set a `volatile sig_atomic_t` flag and return. Let the main thread perform all I/O, logging, cleanup, and memory deallocation.
2. **Never Call Non-Async-Signal-Safe Functions**: Ban `printf()`, `malloc()`, `free()`, `exit()`, `fopen()` inside signal handlers.
3. **Always Check for `SIG_ERR`**: When calling `signal()`, verify that the return value is not `SIG_ERR`.
4. **Use POSIX `sigaction` for Production Linux Systems**: Standard `signal()` exhibits implementation divergences (in older UNIX, handlers automatically reset to `SIG_DFL` after firing). POSIX `sigaction()` provides reliable semantics and signal masks.
5. **Ignore `SIGPIPE` in Network Daemons**: In network servers, writing to a closed socket triggers `SIGPIPE` which terminates the process by default. Always execute `signal(SIGPIPE, SIG_IGN);`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| Calling `printf()` in signal handler | `printf` uses shared stdio buffers and mutex locks. Causes deadlocks and buffer corruption. | Set an atomic flag or write using raw POSIX `write()`. |
| Calling `malloc()` / `free()` in handler | If interrupted during memory allocation, re-entering `malloc` deadlocks heap mutex. | Pre-allocate buffers or set atomic flag. |
| Non-volatile flag: `int shutdown = 0;` | Compiler caches `shutdown` in a register; loop never observes the update made by signal handler! | Use `volatile sig_atomic_t shutdown = 0;`. |
| Assuming signals queue up | Standard signals are bitmask flags in the kernel; multiple rapid signals collapse into one. | Do not count signals; treat them as state level alerts. |
| Catching `SIGKILL` or `SIGSTOP` | Cannot be caught, blocked, or ignored by user processes. Kernel handles them unconditionally. | Only handle catchable signals (`SIGINT`, `SIGTERM`). |
| Calling `longjmp()` from signal handler | Signal mask remains blocked on POSIX systems, permanently preventing future signal delivery. | Use POSIX `siglongjmp()` with `sigsetjmp()`. |

---

## 8. Performance & Security Notes

- **Time-of-Check to Time-of-Use (TOCTOU) Races**: Asynchronous signals can trigger between any two statements. If an application checks permissions and then executes an action, an asynchronous signal modifying state can subvert security validation.
- **Signal Reentrancy Attacks**: If a signal handler modifies global state or tries to free resources, a second signal arriving mid-handler corrupts the heap (CWE-479).
- **Self-Pipe Trick**: In asynchronous event loops (e.g. `epoll`, `select`), the cleanest way to integrate signal notifications is the self-pipe trick: the signal handler writes a single byte to a non-blocking UNIX pipe, waking up the `select()` or `epoll_wait()` loop safely.

---

## 9. Comparison / When to Use What

| API / Mechanism | Portability | Re-entrancy Safety | Masking Support | Recommended Use |
| :--- | :--- | :--- | :--- | :--- |
| **ISO C `signal()`** | Universal (Windows/Linux) | Low (Implementation defined) | None | Basic Ctrl+C handling in simple tools |
| **POSIX `sigaction()`** | POSIX only (Linux/macOS) | High (Reliable semantics) | Full explicit masks | Production UNIX services and daemons |
| **`sig_atomic_t` Flag** | Universal ISO C | Maximum | N/A | Inter-process shutdown signaling |
| **POSIX `signalfd()`** | Linux only | Optimal (Read signals as file descriptors) | Kernel queue | Modern high-throughput epoll loops |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is `printf()` strictly forbidden inside a signal handler?</strong></summary>

`printf()` is not async-signal-safe. Standard I/O streams maintain internal memory buffers and concurrency locks to serialize access across threads. If the main thread is interrupted by a signal while holding the internal lock for `stdout`, and the signal handler attempts to execute `printf()`, it attempts to acquire the exact same lock held by the interrupted thread. Because the interrupted thread cannot resume until the signal handler exits, the process enters an unrecoverable **deadlock**.
</details>

<details>
<summary><strong>2. What guarantees does the `sig_atomic_t` type provide?</strong></summary>

ISO C (§7.14) specifies `sig_atomic_t` as an integer type that can be accessed as an atomic entity even in the presence of asynchronous signals. This guarantees that a read or write operation on this variable will never be interrupted mid-operation (no torn reads or partial word writes). When qualified with `volatile`, the compiler is also forced to read and write directly to physical RAM rather than caching the value in a CPU register.
</details>

<details>
<summary><strong>3. Why should network servers explicitly ignore `SIGPIPE`?</strong></summary>

When a network client abruptly closes its TCP connection, and the server subsequently writes data to the corresponding socket descriptor, the operating system kernel emits a `SIGPIPE` signal to the server process. By default, `SIGPIPE` immediately terminates the process without a core dump. To prevent a single disconnected client from crashing the entire server, network servers must call `signal(SIGPIPE, SIG_IGN)`, allowing `write()` or `send()` to return `-1` with `errno == EPIPE` instead.
</details>

<details>
<summary><strong>4. What is the difference between `SIGINT`, `SIGTERM`, and `SIGKILL`?</strong></summary>

- `SIGINT`: Interactive attention signal sent by the terminal when the user presses Ctrl+C. Can be caught, ignored, or blocked.
- `SIGTERM`: Polite termination request sent by process supervisors (like systemd or `kill <pid>`), giving the process an opportunity to perform graceful cleanup. Can be caught, ignored, or blocked.
- `SIGKILL`: Unconditional immediate termination issued by the kernel. It **cannot be caught, blocked, or ignored**; the OS kernel destroys the process immediately.
</details>

<details>
<summary><strong>5. How does POSIX `sigaction()` resolve the unreliability of standard C `signal()`?</strong></summary>

Under early UNIX implementations of `signal()`, when a signal handler fired, the OS automatically reset the handler back to `SIG_DFL`. If another signal arrived before the handler re-registered itself, the process crashed. Furthermore, `signal()` does not block other signals during handler execution. POSIX `sigaction()` provides a persistent registration, automatically blocks the arriving signal during its own handler execution, and allows developers to specify a mask of additional signals to block.
</details>

<details>
<summary><strong>6. What is the "Self-Pipe Trick"?</strong></summary>

The self-pipe trick is a design pattern used in single-threaded event-driven servers (e.g. using `select()` or `poll()`). Signal handlers cannot safely update application structures. Instead, the process creates a non-blocking UNIX pipe (`pipe()`). When a signal arrives, the signal handler executes the async-signal-safe system call `write(pipe_fd[1], "x", 1)`. The main event loop monitors `pipe_fd[0]` alongside network sockets; when readable, the main loop handles the signal cleanly in user context.
</details>

<details>
<summary><strong>7. Do standard signals queue up in the operating system?</strong></summary>

No. Standard UNIX signals are represented internally within the process control block in the kernel as a bitmask of pending signals. If five `SIGINT` signals are generated in rapid succession before the process has an opportunity to handle them, the corresponding pending bit is set once, and the handler executes only once. Real-time POSIX signals (`SIGRTMIN` to `SIGRTMAX`) do queue, but standard signals do not.
</details>

<details>
<summary><strong>8. What is the effect of calling `raise(sig)`?</strong></summary>

`raise(sig)` sends the specified signal `sig` directly to the calling thread/process itself. It is functionally equivalent to `pthread_kill(pthread_self(), sig)` on POSIX systems. If a handler is registered for `sig`, it executes synchronously before `raise()` returns.
</details>

---

## 11. Practice Exercises

### Easy: Ignore Ctrl+C
Write a program that ignores `SIGINT` (Ctrl+C) for 5 iterations of a work loop, and then restores default behavior.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

int main(void) {
    printf("Ignoring SIGINT for 5 steps. Try pressing Ctrl+C...\n");
    signal(SIGINT, SIG_IGN);

    for (int i = 1; i <= 5; i++) {
        printf("Working step %d...\n", i);
        /* Simulate work */
    }

    printf("Restoring default SIGINT behavior.\n");
    signal(SIGINT, SIG_DFL);
    return 0;
}
```
</details>

### Medium: Clean Shutdown Flag Pattern
Implement a main loop that runs until either 100 tasks are processed or the user sends `SIGINT`, printing the count of processed tasks upon exit.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>

static volatile sig_atomic_t g_running = 1;

static void handle_interrupt(int sig) {
    (void)sig;
    g_running = 0;
}

int main(void) {
    signal(SIGINT, handle_interrupt);
    int tasks_done = 0;

    while (g_running && tasks_done < 100) {
        tasks_done++;
    }

    printf("Shutdown complete. Total tasks completed: %d\n", tasks_done);
    return 0;
}
```
</details>

### Hard: Signal Interception and Statistics Tracker
Write a signal monitoring system that registers handlers for `SIGINT` and `SIGTERM`, counts occurrences via `sig_atomic_t` counters, and performs safe atomic reporting in the main loop.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>

static volatile sig_atomic_t g_sigint_count = 0;
static volatile sig_atomic_t g_sigterm_count = 0;

static void handler(int sig) {
    if (sig == SIGINT) g_sigint_count++;
    else if (sig == SIGTERM) g_sigterm_count++;
}

int main(void) {
    signal(SIGINT, handler);
    signal(SIGTERM, handler);

    printf("Mocking signals with raise()...\n");
    raise(SIGINT);
    raise(SIGINT);
    raise(SIGTERM);

    printf("Received: SIGINT=%d times, SIGTERM=%d times\n",
           (int)g_sigint_count, (int)g_sigterm_count);

    return 0;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Signal | Standard Meaning | Default Action |
| :--- | :--- | :--- |
| `SIGINT` | Interactive interrupt (Ctrl+C) | Terminate process |
| `SIGTERM` | Termination request (`kill`) | Terminate process |
| `SIGABRT` | Abort signal (`abort()`, failed assert) | Terminate with core dump |
| `SIGSEGV` | Invalid memory dereference | Terminate with core dump |
| `SIGFPE` | Arithmetic error (divide by zero) | Terminate with core dump |
| `SIGILL` | Illegal instruction | Terminate with core dump |
| `volatile sig_atomic_t` | Safe flag type for handlers | Guarantees atomic read/write |

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17) §7.14 (Signal handling `<signal.h>`).
- IEEE Std 1003.1 (POSIX) §2.4 (Signal Concepts & Async-Signal-Safe Functions).
- SEI CERT C Coding Standard: Rule 09. Signal Handling (SIG30-C, SIG31-C).
- W. Richard Stevens, *Advanced Programming in the UNIX Environment* (Addison-Wesley) — Chapter 10: Signals.

---

[← Previous](20-floating-point-pitfalls.md) | [Section Index](README.md) | [Next →](22-stdint-inttypes-and-limits.md)
