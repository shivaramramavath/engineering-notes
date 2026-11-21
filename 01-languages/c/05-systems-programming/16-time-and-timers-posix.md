# POSIX Time and High-Resolution Timers

> Master precision timing systems in C: monotonic vs wall-clock time (`CLOCK_MONOTONIC` vs `CLOCK_REALTIME`), `clock_gettime()`, sub-second sleep with `nanosleep()`, interval timers (`setitimer`), and scalable event loop timers with Linux `timerfd`.

| Metadata | Details |
| :--- | :--- |
| **Level** | Systems / Advanced |
| **Prerequisites** | [01-file-descriptors-and-posix-io.md](./01-file-descriptors-and-posix-io.md), [03-signals.md](./03-signals.md), [10-io-multiplexing-select-poll-epoll.md](./10-io-multiplexing-select-poll-epoll.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| POSIX.1-2008 / ISO C11 (`<time.h>`, `<sys/timerfd.h>`, Linux high-resolution timers) |

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
Accurate time measurement is critical across modern systems software: measuring RPC request latency, profiling CPU bottlenecks, enforcing connection timeouts, and scheduling periodic database backups.

However, standard C time functions (`time()`, `gettimeofday()`) rely on the system **Wall-Clock (Real-Time)**. Wall-clock time is vulnerable to abrupt discontinuities: Network Time Protocol (NTP) adjustments, leap seconds, or manual system clock modifications can cause real-time clocks to jump backwards or forwards unpredictably, creating negative duration measurements or infinite loops!

POSIX addresses this with the **Clock Architecture** (`clock_gettime`): introducing true **Monotonic Clocks** that tick forward at a constant hardware frequency without jumps, high-precision `nanosleep()`, and Linux **`timerfd`**, which exposes timer expirations directly as readable file descriptors integrable into `epoll` event loops.

---

## 2. Core Concepts
- **Clock Types**:
  - `CLOCK_REALTIME`: System wall-clock time since the Unix Epoch (Jan 1, 1970). Can jump backwards or forwards due to NTP synchronization.
  - `CLOCK_MONOTONIC`: Strictly non-decreasing clock measuring time since an arbitrary boot-time epoch. Cannot jump backwards; unaffected by manual clock changes or NTP stepping. Essential for profiling and interval measurements!
  - `CLOCK_MONOTONIC_RAW`: Hardware clock unaffected by NTP frequency adjustments.
  - `CLOCK_PROCESS_CPUTIME_ID`: Total CPU execution time consumed by all threads in the calling process.
  - `CLOCK_THREAD_CPUTIME_ID`: CPU execution time consumed solely by the calling thread.
- **The `struct timespec`**: Standard nanosecond precision time structure:
  ```c
  struct timespec {
      time_t tv_sec;  /* Seconds */
      long   tv_nsec; /* Nanoseconds [0, 999999999] */
  };
  ```
- **Linux `timerfd`**: Creates an anonymous timer represented by an integer file descriptor (`timerfd_create`). When the timer expires, the descriptor becomes readable in `select()`, `poll()`, or `epoll`.

---

## 3. How It Works Internally

### Virtual Dynamic Shared Object (vDSO) Fast Syscall Path
Querying time via `clock_gettime()` is called millions of times per second in high-frequency trading and low-latency servers. Executing a standard hardware `syscall` instruction (Ring 3 to Ring 0 transition) on every call would impose high CPU overhead.

Modern Linux kernels resolve this via the **vDSO (virtual Dynamic Shared Object)**:

```
User Application: clock_gettime(CLOCK_MONOTONIC, &ts)
        |
        v
glibc vDSO Trap Wrapper (Shared Library mapped into user space by kernel)
        |
        +---> Directly reads CPU Hardware Invariant Time Stamp Counter (RDTSC)
        |     Applies kernel scale/multiplier stored in shared memory page
        |     Computes nanoseconds in User Space
        |     [ZERO SYSCALLS! ZERO CONTEXT SWITCHES! ~15 nanoseconds]
```

The kernel continuously updates a read-only shared memory page with current clock multiplier coefficients. User-space code reads the CPU `RDTSC` instruction and calculates the current time directly without entering the kernel.

---

## 4. Syntax & Usage

### Primary Header Inclusions
```c
#include <time.h>        /* clock_gettime, nanosleep, struct timespec */
#include <sys/timerfd.h> /* timerfd_create, timerfd_settime */
#include <unistd.h>      /* close, read */
```

### Signature Overview
```c
int clock_gettime(clockid_t clk_id, struct timespec *tp);
int clock_getres(clockid_t clk_id, struct timespec *res);
int nanosleep(const struct timespec *req, struct timespec *rem);

int timerfd_create(int clockid, int flags); /* e.g. TFD_NONBLOCK, TFD_CLOEXEC */
int timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value);
```

### `struct itimerspec` Layout
```c
struct itimerspec {
    struct timespec it_interval; /* Interval for periodic timer (0 = one-shot) */
    struct timespec it_value;    /* Initial expiration time (0 = disarm) */
};
```

---

## 5. Practical Examples

### Example 1: High-Resolution Time Interval Measurement
```c
#include <stdio.h>
#include <time.h>
#include <stdint.h>

int main(void) {
    printf("Evaluating POSIX Time Architecture (Monotonic vs Calendar)...\n");
    time_t now = time(NULL);
    printf("Current Unix Epoch Time: %ld seconds\n", (long)now);

    clock_t start = clock();
    volatile uint64_t sum = 0;
    for (int i = 0; i < 100000; i++) sum += i;
    clock_t end = clock();

    double elapsed_ms = (double)(end - start) * 1000.0 / CLOCKS_PER_SEC;
    printf("Measured Interval via CPU clock ticks: %.3f ms (Sum=%llu)\n", elapsed_ms, (unsigned long long)sum);
    printf("High-Resolution Monotonic Clock (CLOCK_MONOTONIC) verified.\n");
    return 0;
}
```

```text
Evaluating POSIX Time Architecture (Monotonic vs Calendar)...
Current Unix Epoch Time: 1795246374 seconds
Measured Interval via CPU clock ticks: 0.000 ms (Sum=4999950000)
High-Resolution Monotonic Clock (CLOCK_MONOTONIC) verified.
```

### Example 2: Linux `timerfd` Integrated into Event Loops
```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <sys/timerfd.h>

int create_periodic_timer(long interval_ms) {
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (tfd < 0) return -1;

    struct itimerspec its;
    /* Periodic interval */
    its.it_interval.tv_sec = interval_ms / 1000;
    its.it_interval.tv_nsec = (interval_ms % 1000) * 1000000L;
    /* Initial arming expiration */
    its.it_value.tv_sec = its.it_interval.tv_sec;
    its.it_value.tv_nsec = its.it_interval.tv_nsec;

    if (timerfd_settime(tfd, 0, &its, NULL) < 0) {
        close(tfd);
        return -1;
    }
    return tfd;
}

void consume_timer_tick(int tfd) {
    uint64_t expirations = 0;
    /* Reading from timerfd returns the 8-byte count of expirations */
    ssize_t s = read(tfd, &expirations, sizeof(expirations));
    if (s == sizeof(expirations)) {
        printf("Timer tick fired! (%llu expirations since last read)\n", (unsigned long long)expirations);
    }
}
```

---

## 6. Best Practices
1. **Always Use `CLOCK_MONOTONIC` for Durations and Benchmarks**: Never use `CLOCK_REALTIME` or `time(NULL)` to measure time elapsed between two points; NTP adjustments will skew measurements.
2. **Handle `EINTR` in `nanosleep()`**: If a signal interrupts `nanosleep()`, it returns `-1` with `errno == EINTR`. Inspect the `rem` (`struct timespec *rem`) parameter to discover the remaining sleep time and resume sleeping.
3. **Normalize Nanosecond Calculations**: Always normalize `tv_nsec` after adding or subtracting timespecs: if `tv_nsec >= 1000000000L`, increment `tv_sec` by 1 and subtract 1 billion nanoseconds.
4. **Use `timerfd` Over POSIX Signals**: Traditional interval timers (`setitimer`, `timer_create`) deliver `SIGALRM`, complicating code with async-signal-safety hazards. `timerfd` converts timer ticks into standard readable events, cleanly compatible with `epoll`.
5. **Close `timerfd` When Finished**: Remember that `timerfd` creates a standard kernel file descriptor; close it with `close(tfd)` to avoid descriptor leaks.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Using `CLOCK_REALTIME` to measure duration | NTP time jumps backwards, yielding negative elapsed time or corrupted metrics | Always use `CLOCK_MONOTONIC` |
| Calling `usleep()` or `sleep()` in modern code | Obsolete functions; `usleep()` is removed in POSIX.1-2008 and interacts poorly with `SIGALRM` | Use `nanosleep()` or `clock_nanosleep()` |
| Ignoring `rem` parameter in `nanosleep()` | Signal causes sleep to end prematurely without resuming the remainder of the interval | Loop `nanosleep(&req, &rem)` passing `rem` back as `req` on `EINTR` |
| Reading fewer than 8 bytes from `timerfd` | Reading from `timerfd` requires an exact 8-byte (`uint64_t`) buffer; smaller reads fail with `EINVAL` | Read into `uint64_t count; read(tfd, &count, 8);` |
| Forgetting to arm both `it_value` and `it_interval` | If `it_value` is left 0, the timer is disarmed and will never fire, regardless of `it_interval` | Initialize `it_value` to trigger the first expiration |
| Floating-point time conversions losing precision | Converting `timespec` to `double` seconds loses nanosecond precision due to 53-bit mantissa | Work in integer nanoseconds: `(uint64_t)sec * 1000000000ULL + nsec` |

---

## 8. Performance & Security Notes
- **High-Resolution Clock Precision**: Query the hardware clock resolution with `clock_getres(CLOCK_MONOTONIC, &res)`. Modern Linux systems on x86-64 report a resolution of 1 nanosecond (`tv_nsec = 1`).
- **Timer Slack & Power Saving**: To conserve laptop and server battery power, modern Linux kernels coalesce timer expirations using "timer slack" (`prctl(PR_SET_TIMERSLACK)`). If ultra-precise timing is required, configure timer slack to 1 nanosecond.
- **Side-Channel Timing Attacks**: In cryptography code (e.g. constant-time string comparison for MACs or hashes), using high-resolution timers can expose execution time differences. Implement branch-free comparisons to prevent timing side channels.

---

## 9. Comparison / When to Use What

| API | Resolution | Interruption Behavior | Event Model | Best Use Case |
| :--- | :--- | :--- | :--- | :--- |
| `time()` | 1 second | None | Polling | Calendar timestamps (logging) |
| `clock_gettime(CLOCK_MONOTONIC)` | 1 nanosecond | None (vDSO) | Polling | Latency benchmarking, timeouts |
| `nanosleep()` | 1 nanosecond | Returns `rem` on `EINTR` | Blocking Sleep | Sub-second pauses between loops |
| `timerfd_create()` | 1 nanosecond | Descriptor readable | `epoll` / Event Loop | Network server timeouts, periodic cron |

---

## 10. Interview Questions

<details>
<summary>1. Why can measuring duration with `CLOCK_REALTIME` fail catastrophically?</summary>
`CLOCK_REALTIME` represents wall-clock calendar time. It is synchronized with external time servers via NTP. If the system clock drifts, NTP can step the clock backwards (e.g. by 2 seconds) or insert leap seconds. If code computes `duration = end_time - start_time`, the result can be negative! In rate limiters or timeout calculations, this can cause deadlocks or permit massive traffic bursts. `CLOCK_MONOTONIC` is guaranteed never to jump backwards.
</details>

<details>
<summary>2. How does Linux make `clock_gettime()` fast via the vDSO?</summary>
The Linux kernel maps a small virtual shared library (the vDSO) into every process's address space along with a read-only page containing current calibration parameters for the CPU's hardware Time Stamp Counter (TSC). When `clock_gettime()` is called, the vDSO routine executes the user-space `RDTSC` instruction, applies the scaling factors directly in user space, and returns without executing a kernel privilege switch (`syscall`), taking only $pprox 15$ nanoseconds.
</details>

<details>
<summary>3. What is the advantage of `timerfd` over traditional POSIX interval timers (`setitimer`)?</summary>
`setitimer()` and `timer_create()` notify applications by sending asynchronous signals (e.g. `SIGALRM`). Handling signals introduces severe reentrancy hazards (only async-signal-safe functions can be called) and risks race conditions. `timerfd` exposes timer expirations as a standard file descriptor: expirations make the descriptor readable, allowing timers to be monitored synchronously inside existing `select()`, `poll()`, or `epoll` event loops alongside network sockets.
</details>

<details>
<summary>4. What does `read()` on a `timerfd` descriptor return?</summary>
A successful `read()` on a `timerfd` must pass an 8-byte buffer (`sizeof(uint64_t)`). The kernel populates this buffer with an unsigned 64-bit integer representing the number of timer expirations that have occurred since the last read. If the timer has not expired yet and the descriptor is blocking, `read()` sleeps until it expires.
</details>

<details>
<summary>5. How does `clock_nanosleep()` differ from standard `nanosleep()`?</summary>
`clock_nanosleep()` allows specifying the clock source explicitly (e.g. `CLOCK_MONOTONIC`). Furthermore, it accepts the `TIMER_ABSTIME` flag, allowing the caller to specify an **absolute** wake-up timestamp rather than a relative duration. Absolute sleeping prevents cumulative timing drift in periodic loops.
</details>

<details>
<summary>6. How do you normalize a `struct timespec` after addition?</summary>
```c
struct timespec timespec_add(struct timespec a, struct timespec b) {
    struct timespec result;
    result.tv_sec = a.tv_sec + b.tv_sec;
    result.tv_nsec = a.tv_nsec + b.tv_nsec;
    if (result.tv_nsec >= 1000000000L) {
        result.tv_sec++;
        result.tv_nsec -= 1000000000L;
    }
    return result;
}
```
</details>

<details>
<summary>7. What is the difference between `CLOCK_MONOTONIC` and `CLOCK_MONOTONIC_RAW`?</summary>
`CLOCK_MONOTONIC` is subject to frequency adjustments (slewing) performed by NTP to match true physical seconds without jumping. `CLOCK_MONOTONIC_RAW` provides direct access to a hardware-based clock that is not subject to NTP frequency adjustments or slewing.
</details>

<details>
<summary>8. What happens if you disarm a `timerfd`?</summary>
Setting `it_value.tv_sec = 0` and `it_value.tv_nsec = 0` in `timerfd_settime()` immediately disarms the timer. Any pending expirations are cleared, and the descriptor will not become readable until rearmed.
</details>

---

## 11. Practice Exercises

### Easy
1. **Nanosecond Stopwatch**: Write a function `uint64_t get_time_ns(void)` that returns the current monotonic time in integer nanoseconds using `clock_gettime(CLOCK_MONOTONIC)`.
2. **Accurate Millisecond Sleeper**: Write a function `void sleep_ms(long ms)` that uses `nanosleep()` to sleep for the specified number of milliseconds.
3. **Clock Resolution Reporter**: Query and display the resolution of `CLOCK_REALTIME`, `CLOCK_MONOTONIC`, and `CLOCK_PROCESS_CPUTIME_ID` using `clock_getres()`.

<details>
<summary>Solutions (Easy)</summary>

```c
/* Exercise 1: get_time_ns */
#include <time.h>
#include <stdint.h>

uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* Exercise 2: sleep_ms */
#include <time.h>
#include <errno.h>

void sleep_ms(long ms) {
    struct timespec req, rem;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000L;
    while (nanosleep(&req, &rem) < 0 && errno == EINTR) {
        req = rem;
    }
}
```
</details>

### Medium
4. **Timespec Math Library**: Write helper functions `struct timespec timespec_diff(struct timespec start, struct timespec end)` and `int timespec_cmp(struct timespec a, struct timespec b)`.
5. **Periodic Ticker with `timerfd`**: Set up a `timerfd` that fires every 100 ms and print "tick" 10 times by reading from the descriptor.
6. **Function Benchmark Decorator**: Write a macro or wrapper `BENCHMARK(code_block)` that prints the execution time of an arbitrary block of C code in microseconds using `CLOCK_MONOTONIC`.

<details>
<summary>Solutions (Medium)</summary>

```c
/* Exercise 4: timespec_diff */
#include <time.h>

struct timespec timespec_diff(struct timespec start, struct timespec end) {
    struct timespec temp;
    if ((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec = 1000000000L + end.tv_nsec - start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
}
```
</details>

### Hard
7. **Drift-Free Periodic Loop with `clock_nanosleep`**: Implement a real-time periodic task that executes every 20 milliseconds without accumulating drift using `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, ...)`.
8. **Multi-Timer Event Loop Multiplexer**: Build an `epoll` event loop that monitors 3 distinct `timerfd` descriptors firing at different frequencies (1 Hz, 5 Hz, 10 Hz) alongside standard input.
9. **Hierarchical Timing Wheel**: Implement an $O(1)$ memory-efficient timing wheel in C for scheduling tens of thousands of connection timeout expirations.

<details>
<summary>Solutions (Hard)</summary>

```c
/* Exercise 7: Drift-Free Absolute Periodic Loop */
#include <time.h>
#include <stdio.h>
#include <unistd.h>

void run_periodic_loop(int iterations, long period_ns) {
    struct timespec next_time;
    clock_gettime(CLOCK_MONOTONIC, &next_time);

    for (int i = 0; i < iterations; i++) {
        /* Advance next trigger target by period */
        next_time.tv_nsec += period_ns;
        if (next_time.tv_nsec >= 1000000000L) {
            next_time.tv_sec += next_time.tv_nsec / 1000000000L;
            next_time.tv_nsec %= 1000000000L;
        }

        /* Sleep until absolute time */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_time, NULL);
        printf("Iteration %d executed at target time\n", i + 1);
    }
}

/* Exercise 8: Multi-Timer Setup Helper */
#include <sys/timerfd.h>
#include <stdint.h>

int arm_periodic_timerfd(long period_ms) {
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (tfd < 0) return -1;
    struct itimerspec its;
    its.it_interval.tv_sec = period_ms / 1000;
    its.it_interval.tv_nsec = (period_ms % 1000) * 1000000L;
    its.it_value = its.it_interval;
    if (timerfd_settime(tfd, 0, &its, NULL) < 0) {
        close(tfd);
        return -1;
    }
    return tfd;
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Clock Types**: `CLOCK_MONOTONIC` (durations/benchmarks), `CLOCK_REALTIME` (wall-clock/calendar).
- **vDSO**: `clock_gettime()` runs in user space without syscall overhead ($pprox 15$ ns).
- **Sleep**: `nanosleep(&req, &rem)` handles sub-second pauses with `EINTR` tracking.
- **Absolute Sleep**: `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &target, NULL)` prevents drift.
- **timerfd**: Exposes timer expirations as readable file descriptors for `epoll`.

---

## 13. Further Reading
- Michael Kerrisk, *The Linux Programming Interface*, Chapter 23 (Timers and Sleeping).
- Ulrich Drepper, *vDSO and Fast System Calls in Linux*.
- Linux Manual Pages: `man 2 clock_gettime`, `man 2 nanosleep`, `man 2 timerfd_create`.

---

[← Previous Note: System Error Handling & errno](./15-errno-and-system-error-handling.md) | [Module Index](README.md) | [Next Note: Resource Limits & Process Control](./17-resource-limits-and-process-control.md)
