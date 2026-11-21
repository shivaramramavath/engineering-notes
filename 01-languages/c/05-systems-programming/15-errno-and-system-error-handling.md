# System Error Handling and errno

> Master robust system-level error handling in C: thread-local `errno`, `strerror` vs thread-safe `strerror_r`, `perror`, categorizing system call failures, handling interrupted system calls (`EINTR`), and structured error propagation patterns.

| Metadata | Details |
| :--- | :--- |
| **Level** | Systems / Advanced |
| **Prerequisites** | [01-file-descriptors-and-posix-io.md](./01-file-descriptors-and-posix-io.md), [03-intermediate/03-error-handling.md](../03-intermediate/03-error-handling.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| POSIX.1-2008 / ISO C11 (`<errno.h>`, `<string.h>`, thread-safe reentrancy) |

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
Unlike higher-level programming languages that use exception hierarchies (`try / catch`) to bubble errors up the call stack, the C language has no native exception mechanism. Operating system kernels and standard C runtime libraries communicate errors through integer return codes and a global thread-local status variable: **`errno`**.

System calls can fail for dozens of valid operational reasons: network connections drop (`ECONNRESET`), file descriptors run out (`EMFILE`), files don't exist (`ENOENT`), or a signal interrupts a blocking system call (`EINTR`). Developing production systems in C requires mastering the lifecycle of `errno`: knowing when it is set, preserving it across secondary cleanup calls, handling transient errors with retries, and formatting error messages safely in multi-threaded environments.

---

## 2. Core Concepts
- **`errno`**: A thread-local integer macro defined in `<errno.h>`. When a system call fails (usually returning `-1`), it sets `errno` to a positive integer indicating the reason for failure.
- **Rule of `errno` Cleansing**: Successful system calls are **never** required to reset `errno` to 0. Therefore, code must **never** inspect `errno` unless the preceding system call returned a failure code!
- **Transient vs Fatal Errors**:
  - *Transient / Recoverable*: `EINTR` (interrupted by signal; retry immediately), `EAGAIN` / `EWOULDBLOCK` (non-blocking descriptor not ready; retry when event loop wakes), `EBUSY` (resource temporarily held).
  - *Fatal / Programmer Error*: `EBADF` (invalid file descriptor), `EFAULT` (bad memory pointer address), `EINVAL` (invalid parameter).
- **String Conversion**:
  - `perror(prefix)`: Writes `prefix: <description>` directly to `stderr`.
  - `strerror(errnum)`: Returns a pointer to a static string describing the error (Thread-Unsafe!).
  - `strerror_r(errnum, buf, buflen)`: POSIX thread-safe reentrant error formatter writing to a user-provided buffer.

---

## 3. How It Works Internally

### Thread-Local `errno` Implementation
Historically, in single-threaded Unix, `errno` was a global variable: `extern int errno;`. In modern multi-threaded operating systems, this would create devastating race conditions where Thread A's error code would be overwritten by Thread B.

In ISO C11 and POSIX.1-2008, `errno` is redefined as a macro that expands to a function call returning a pointer to thread-local storage:

```c
/* Internal glibc definition */
extern int *__errno_location(void);
#define errno (*__errno_location())
```

```
Thread 1 Execution Context                    Thread 2 Execution Context
+-----------------------------+               +-----------------------------+
| open("/nonexistent", ...)   |               | read(fd, buf, 1024)         |
| Fails: returns -1           |               | Interrupted: returns -1     |
| __errno_location() called   |               | __errno_location() called   |
|   v                         |               |   v                         |
| Thread 1 TLS: errno = ENOENT|               | Thread 2 TLS: errno = EINTR |
+-----------------------------+               +-----------------------------+
```

Each thread accesses its own private memory slot inside Thread-Local Storage (TLS). Neither thread interferes with the other.

---

## 4. Syntax & Usage

### Primary Header Inclusions
```c
#include <errno.h>
#include <string.h>
#include <stdio.h>
```

### Signature Overview
```c
char *strerror(int errnum);
int strerror_r(int errnum, char *buf, size_t buflen); /* XSI-compliant POSIX */
void perror(const char *s);
```

### Common Standard Error Constants
- `ENOENT` (2): No such file or directory.
- `EACCES` (13): Permission denied.
- `EEXIST` (17): File exists.
- `EINVAL` (22): Invalid argument.
- `EMFILE` (24): Too many open files (Process limit reached).
- `ENFILE` (23): Too many open files in system (System-wide limit).
- `EINTR` (4): Interrupted system call.
- `EAGAIN` / `EWOULDBLOCK` (11): Resource temporarily unavailable.

---

## 5. Practical Examples

### Example 1: Evaluating Thread-Local errno & Retry Loop
```c
#include <stdio.h>
#include <errno.h>
#include <string.h>

int simulate_system_call(int arg) {
    if (arg < 0) {
        errno = EINVAL; /* Invalid argument */
        return -1;
    }
    if (arg == 42) {
        errno = EINTR;  /* Interrupted system call */
        return -1;
    }
    return arg * 2;
}

int main(void) {
    printf("Evaluating Thread-Local errno & System Error Handling...\n");
    errno = 0;

    int res = simulate_system_call(-5);
    if (res < 0) {
        printf("Syscall returned -1 with errno=%d: %s\n", errno, strerror(errno));
    }

    res = simulate_system_call(42);
    if (res < 0 && errno == EINTR) {
        printf("Caught EINTR (Interrupted system call), retrying operation...\n");
        res = simulate_system_call(10);
        printf("Retry successful! Output = %d\n", res);
    }
    return 0;
}
```

```text
Evaluating Thread-Local errno & System Error Handling...
Syscall returned -1 with errno=22: Invalid argument
Caught EINTR (Interrupted system call), retrying operation...
Retry successful! Output = 20
```

### Example 2: Robust Syscall Wrapper Preserving errno Across Cleanup
```c
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

int safe_open_and_setup(const char *path) {
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        /* Preserve original errno before doing cleanup */
        int saved_errno = errno;
        
        /* Cleanup operations (which themselves might modify errno) */
        unlink("temp_lock.pid");
        
        /* Restore original errno so caller receives accurate failure diagnosis */
        errno = saved_errno;
        return -1;
    }
    return fd;
}

void print_thread_safe_error(const char *prefix, int err) {
    char err_buf[256];
#if defined(_GNU_SOURCE) && !defined(__STDC_LIB_EXT1__)
    char *msg = strerror_r(err, err_buf, sizeof(err_buf));
    fprintf(stderr, "%s: %s\n", prefix, msg);
#else
    strerror_r(err, err_buf, sizeof(err_buf));
    fprintf(stderr, "%s: %s\n", prefix, err_buf);
#endif
}
```

---

## 6. Best Practices
1. **Never Check `errno` Without Checking Return Value First**: System calls only set `errno` on failure. A successful call leaves whatever value was previously stored in `errno` unchanged.
2. **Save `errno` Immediately**: If your error-handling code performs secondary cleanup calls (like `close()`, `free()`, or logging), those calls can overwrite `errno`. Save it first: `int err = errno; ...; errno = err;`.
3. **Always Handle `EINTR` in Blocking Loops**: Signals (such as `SIGCHLD` or user signals) interrupt blocking calls like `read()`, `write()`, `waitpid()`, and `epoll_wait()`. Wrap them in `while (res < 0 && errno == EINTR)` retry loops.
4. **Use Thread-Safe `strerror_r()` in Multi-Threaded Code**: Standard `strerror()` returns a pointer to internal static memory that is shared across threads, causing race conditions.
5. **Zero Out `errno` Before Library Calls That Return Valid Negative Numbers**: Functions like `strtol()` return values where 0 or negative numbers can be valid results. Reset `errno = 0` before calling, then verify `errno != 0`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Checking `if (errno != 0)` after system call | Successful system calls do not reset `errno` to 0; detects leftover errors from previous operations | Check `if (res == -1)` first, then read `errno` |
| Calling `printf("%s", strerror(errno))` in multithreaded code | `strerror()` uses internal static buffer overwritten by concurrent calls in other threads | Use thread-safe `strerror_r()` |
| Allowing cleanup functions to overwrite `errno` | Calling `close(other_fd)` inside error handler changes `errno`, destroying the real root cause | Save `int saved_errno = errno;` before cleanups |
| Aborting on `EINTR` | Signals interrupt normal blocking calls; aborting terminates valid I/O unexpectedly | Retry the system call immediately in a loop |
| Confusing GNU `strerror_r` with POSIX `strerror_r` | GNU version returns `char *`, while POSIX version returns `int`; mismatch causes compiler errors | Check `#if (_POSIX_C_SOURCE >= 200112L) && ! _GNU_SOURCE` |
| Using `errno` in signal handlers without saving | Handler executes asynchronously on arbitrary threads; modifying `errno` corrupts interrupted code | Save `int saved = errno;` on entry, restore before returning |

---

## 8. Performance & Security Notes
- **Signal Handlers and `errno`**: Modifying `errno` inside a signal handler is a subtle bug: if a signal handler calls an async-signal-safe function that fails (like `write()`), it alters `errno` on the thread it interrupted. Always save and restore `errno` at the start and end of every signal handler.
- **Error Information Leakage**: Do not expose raw internal `strerror()` text (such as file paths or database descriptors) to unauthenticated network clients, as this reveals server internal directory structures and filesystem architecture to attackers.

---

## 9. Comparison / When to Use What

| API | Thread Safety | Output Destination | Best Use Case |
| :--- | :--- | :--- | :--- |
| `perror()` | Thread-safe (POSIX) | Writes directly to `stderr` | Quick CLI tool debugging |
| `strerror()` | **Thread-Unsafe** | Returns static `char *` | Single-threaded test scripts |
| `strerror_r()` | **Thread-Safe** | Writes to user-supplied buffer | Multi-threaded servers, libraries |

---

## 10. Interview Questions

<details>
<summary>1. Is `errno` a global variable in modern multi-threaded C programs?</summary>
No. In modern POSIX and ISO C11 systems, `errno` is defined as a macro expanding to a function call (e.g. `*__errno_location()`) that returns a pointer to an integer allocated in Thread-Local Storage (TLS). Each thread has its own private `errno`, ensuring that system call failures on one thread do not overwrite or interfere with error codes on sibling threads.
</details>

<details>
<summary>2. Why must a program never check `errno` unless a system call returned an explicit failure code?</summary>
The POSIX standard specifies that system calls and standard library functions are permitted to set `errno` to a non-zero value *only when an error occurs*. When a system call succeeds, it is explicitly **not required** to reset `errno` to 0. Therefore, if a previous operation set `errno` to `ENOENT`, and a subsequent system call succeeds, `errno` may still contain `ENOENT`. Checking `errno` without verifying that the system call returned `-1` produces false-positive error detections.
</details>

<details>
<summary>3. Why must signal handlers always save and restore `errno`?</summary>
Signals can interrupt thread execution at any arbitrary instruction. If a thread makes a system call that fails and is about to inspect `errno`, but is interrupted by a signal handler that makes another system call (such as `write()`), the signal handler will overwrite `errno`. When the handler returns, the main thread will read the wrong error code. To prevent this, every signal handler that invokes system calls must save `int saved = errno;` on entry and restore `errno = saved;` before returning.
</details>

<details>
<summary>4. What is `EINTR` and how should systems code respond to it?</summary>
`EINTR` ("Interrupted system call") is set when a thread is blocked in a slow system call (like `read()`, `write()`, `waitpid()`, or `epoll_wait()`) and a signal is delivered to the process. Rather than continuing to block, the kernel wakes the system call and returns `-1` with `errno == EINTR`. Robust systems code must treat `EINTR` as a temporary interruption and retry the system call in a loop, unless the application has registered a shutdown request.
</details>

<details>
<summary>5. What are the two conflicting versions of `strerror_r()` in C?</summary>
1. **XSI-compliant POSIX version**: `int strerror_r(int errnum, char *buf, size_t buflen);`. Always populates `buf` and returns 0 on success.
2. **GNU-specific version**: `char *strerror_r(int errnum, char *buf, size_t buflen);`. May return a pointer to an internal static immutable string *or* populate `buf` and return `buf`.
</details>

<details>
<summary>6. How do you distinguish between an error and a valid negative return in `strtol()`?</summary>
Because `strtol()` can return 0 or negative numbers as valid parsed integers, the return value alone cannot indicate failure. The caller must set `errno = 0` immediately before calling `strtol()`. After calling, check if `errno != 0` (e.g. `ERANGE` for overflow) or if the `endptr` did not advance.
</details>

<details>
<summary>7. What is the difference between `EMFILE` and `ENFILE`?</summary>
`EMFILE` ("Too many open files") means the calling *process* has exhausted its per-process file descriptor limit (`RLIMIT_NOFILE`). `ENFILE` ("Too many open files in system") means the entire *operating system* has exhausted its global kernel table capacity for open file descriptions (`/proc/sys/fs/file-max`).
</details>

<details>
<summary>8. How should library functions report errors in C?</summary>
Production C libraries should return explicit integer status codes (e.g. `0` for success, negative error enum values for failures) and avoid setting global `errno` directly, or define custom error enum codes passed back via an output parameter pointer (`Error **err`), preventing namespace pollution and collision with OS system errors.
</details>

---

## 11. Practice Exercises

### Easy
1. **Error Name Printer**: Write a function that takes an integer error code and prints its standard string description using `perror()`.
2. **File Opener with Diagnostics**: Attempt to open a non-existent file and print the resulting `errno` number and description.
3. **Numeric Strtol Validator**: Write a function using `strtol()` with `errno = 0` to safely convert a string to a long integer, detecting overflow/underflow (`ERANGE`).

<details>
<summary>Solutions (Easy)</summary>

```c
/* Exercise 2 */
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

void test_open_fail(void) {
    int fd = open("/invalid/path/to/file", O_RDONLY);
    if (fd < 0) {
        printf("Open failed with errno %d: %s\n", errno, strerror(errno));
    }
}
```
</details>

### Medium
4. **Resilient EINTR Read Loop**: Implement `ssize_t robust_read(int fd, void *buf, size_t count)` that automatically retries if interrupted by `EINTR`.
5. **Thread-Safe Error Logger**: Implement `void log_error(const char *msg)` that uses POSIX `strerror_r()` to format `errno` into a 256-byte buffer and prints to `stderr`.
6. **Signal Handler Errno Guard**: Write a `SIGUSR1` signal handler that makes a `write()` system call while carefully preserving and restoring the caller's `errno`.

<details>
<summary>Solutions (Medium)</summary>

```c
/* Exercise 4: robust_read with EINTR */
#include <unistd.h>
#include <errno.h>

ssize_t robust_read(int fd, void *buf, size_t count) {
    ssize_t n;
    while ((n = read(fd, buf, count)) < 0) {
        if (errno == EINTR) continue; /* Interrupted by signal; retry */
        return -1;                    /* Fatal error */
    }
    return n;
}

/* Exercise 6: Signal Handler Errno Guard */
#include <signal.h>
#include <unistd.h>
#include <errno.h>

void safe_signal_handler(int sig) {
    (void)sig;
    int saved_errno = errno; /* Save interrupted thread's errno */

    const char msg[] = "SIGUSR1 handled\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);

    errno = saved_errno;     /* Restore original errno */
}
```
</details>

### Hard
7. **Structured Error Monad in C**: Implement an error-handling pattern using a `Result` struct: `typedef struct { bool ok; union { int value; const char *err_msg; }; } Result;` with helper constructors and unwrapping.
8. **Fault Injection Wrapper**: Write a preloadable wrapper around `open()`, `read()`, and `write()` that randomly injects simulated failures (`EAGAIN`, `EINTR`, `EIO`) 10% of the time to test system resilience.
9. **Hierarchical System Error Translator**: Build an error mapper that translates POSIX `errno` values into standardized application-level error codes with HTTP status code equivalents (e.g. `ENOENT` $	o$ 404, `EACCES` $	o$ 403, `ETIMEDOUT` $	o$ 504).

<details>
<summary>Solutions (Hard)</summary>

```c
/* Exercise 7: Structured Result Monad */
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    bool ok;
    union {
        int value;
        const char *err;
    } data;
} IntResult;

IntResult ok_result(int val) {
    IntResult r;
    r.ok = true;
    r.data.value = val;
    return r;
}

IntResult err_result(const char *msg) {
    IntResult r;
    r.ok = false;
    r.data.err = msg;
    return r;
}

IntResult safe_divide(int a, int b) {
    if (b == 0) return err_result("Division by zero error");
    return ok_result(a / b);
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Check Rule**: Only inspect `errno` if the system call returned failure (`-1`).
- **Thread Safety**: `errno` is thread-local. Use `strerror_r()` for thread-safe formatting.
- **Save Rule**: Always save `int saved = errno;` before executing secondary cleanup logic.
- **EINTR**: Always retry system calls when `errno == EINTR`.
- **Signal Handlers**: Always save and restore `errno` inside signal handlers.

---

## 13. Further Reading
- W. Richard Stevens, *Advanced Programming in the UNIX Environment*, Chapter 1 (Error Handling).
- Michael Kerrisk, *The Linux Programming Interface*, Chapter 3 (System Calls and Error Handling).
- CERT C Coding Standard: ERR30-C (Take care when reading errno).

---

[← Previous Note: Filesystem APIs: stat & dirent](./14-filesystem-apis-stat-dirent.md) | [Module Index](README.md) | [Next Note: POSIX Time and Timers](./16-time-and-timers-posix.md)
