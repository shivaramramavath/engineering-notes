# 09 Time & Random

> Master calendar time, monotonic clocks, high-resolution benchmarking, deterministic pseudo-random number generators (PRNGs), and unbiased distributions in standard C.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | [01-fundamentals/04-variables-constants.md](../01-fundamentals/04-variables-constants.md), [03-intermediate/05-bit-manipulation.md](05-bit-manipulation.md) |
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

Timekeeping and randomness are twin pillars of systems programming:
- **Timekeeping**: Microsecond-accurate performance profiling, timeout enforcement in network sockets, event scheduling, expiration timers, and structured logging.
- **Randomness**: Shuffling datasets, generating statistical simulations (Monte Carlo), load balancing with jitter, game mechanics, and cryptographic key generation.

However, historical C APIs in `<time.h>` and `<stdlib.h>` are fraught with design pitfalls:
- **Wall Clocks vs Monotonic Clocks**: Wall clocks can jump backward or freeze due to NTP synchronization, making them hazardous for measuring elapsed time.
- **Year 2038 Vulnerability**: 32-bit signed `time_t` counters will overflow on January 19, 2038, crashing legacy embedded systems.
- **Flawed Standard `rand()`**: The standard library `rand()` uses a weak Linear Congruential Generator (LCG) with low entropy, is thread-unsafe, and has a tiny range on Windows (`RAND_MAX` = 32,767).
- **Modulo Bias**: Naively writing `rand() % N` skews probability distributions, causing subtle algorithmic defects.

This guide details modern timekeeping patterns, thread-safe conversions, high-precision clocks, and high-performance pseudo-random algorithms.

---

## 2. Core Concepts

### 2.1 Calendar Time vs Monotonic Time
1. **Calendar Time (Wall Clock)**: Measures seconds elapsed since the Unix Epoch (00:00:00 UTC, January 1, 1970). Subject to timezone adjustments, daylight saving time (DST), leap seconds, and NTP adjustments. Used strictly for human timestamps and logging.
2. **Monotonic Time**: Measures continuous, strictly increasing ticks from an arbitrary fixed point (such as system boot). Monotonic clocks never jump backward, regardless of NTP adjustments. Essential for elapsed duration benchmarking and timeouts.

### 2.2 Time Types in Standard C
- `time_t`: Real arithmetic type holding calendar time (seconds since Epoch). On modern 64-bit systems, this is a 64-bit integer.
- `struct tm`: Broken-down calendar representation holding fields:
  - `tm_sec` (0–60, allowing leap seconds)
  - `tm_min` (0–59)
  - `tm_hour` (0–23)
  - `tm_mday` (1–31)
  - `tm_mon` (0–11, where 0 is January!)
  - `tm_year` (Years since 1900!)
  - `tm_wday` (0–6, days since Sunday)
  - `tm_isdst` (Daylight saving flag)
- `clock_t`: Type returned by `clock()`, representing CPU processor time consumed by the calling process in units of `CLOCKS_PER_SEC`.
- `struct timespec` (C11): High-resolution time structure containing:
  - `time_t tv_sec;` (Seconds)
  - `long tv_nsec;` (Nanoseconds)

### 2.3 The Anatomy of PRNGs and Modulo Bias
The standard function `rand()` generates a pseudo-random integer between `0` and `RAND_MAX`.
If you map this to a range $[0, N-1]$ using modulo:
```c
int val = rand() % N; /* BIASED IF RAND_MAX + 1 IS NOT DIVISIBLE BY N */
```
If `RAND_MAX` is 32767 and $N = 10$, numbers 0 through 7 occur 3277 times, while numbers 8 and 9 occur only 3276 times. For larger ranges, this bias causes significant statistical distortion.
The unbiased solution is **rejection sampling**: discarding numbers that fall above the largest multiple of $N$ representable in the range.

---

## 3. How It Works Internally

### Hardware Time Sources & Operating System Clocks
Modern CPUs provide high-frequency hardware counters:
- **TSC (Time Stamp Counter)**: An x86 64-bit register incremented on every CPU clock cycle (read via `RDTSC`). In modern processors, invariant TSC increments at a constant frequency regardless of CPU frequency scaling.
- **RTC (Real-Time Clock)**: Battery-backed hardware clock on the motherboard tracking calendar time across system reboots.
- **HPET / ACPI PM Timer**: High precision event timers used by the OS kernel to schedule interrupts and calibrate time sources.

```
Hardware to User-Space Time Architecture:
┌──────────────────────────────────────────┐
│ User Application: time(), timespec_get() │
├──────────────────────────────────────────┤
│ C Runtime (vDSO / Fast System Call)      │
├──────────────────────────────────────────┤
│ Kernel Timer Subsystem                   │
├──────────────────────────────────────────┤
│ Hardware Clocks: TSC / HPET / RTC        │
└──────────────────────────────────────────┘
```

On Linux, reading the clock often avoids a costly kernel context switch via the **vDSO** (virtual Dynamic Shared Object), reading hardware timers directly from mapped memory in user space.

---

## 4. Syntax & Usage

### 4.1 C11 High-Resolution Time (`timespec_get`)
```c
#include <time.h>
#include <stdio.h>

void measure_elapsed(void) {
    struct timespec start, end;
    timespec_get(&start, TIME_UTC);

    /* Perform computational work */

    timespec_get(&end, TIME_UTC);
    double elapsed_sec = (double)(end.tv_sec - start.tv_sec) +
                         (double)(end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Elapsed: %.6f seconds\n", elapsed_sec);
}
```

### 4.2 Modern Fast PRNG: Xoroshiro64*
Instead of `rand()`, systems developers use fast, compact algorithms like Xoroshiro or PCG.
```c
#include <stdint.h>

static uint64_t s_state = 0x853c49e6748fea9bULL;

static inline uint32_t xoroshiro_next(void) {
    uint64_t x = s_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    s_state = x;
    return (uint32_t)((x * 0x2545F4914F6CDD1DULL) >> 32);
}
```

---

## 5. Practical Examples

### Comprehensive Example: Time Formatting, CPU Benchmarking, and Unbiased PRNG
The following complete program demonstrates formatting local timestamps, measuring CPU clock consumption, and generating unbiased random integers using rejection sampling.

```c
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

/* Robust Xoroshiro64* pseudo-random generator (avoids rand() limitations) */
static uint64_t s_rng_state = 0x853c49e6748fea9bULL;

static inline uint32_t xoroshiro_next(void) {
    uint64_t x = s_rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    s_rng_state = x;
    return (uint32_t)((x * 0x2545F4914F6CDD1DULL) >> 32);
}

/* Uniform integer distribution in range [min, max] without modulo bias */
static uint32_t uniform_random_range(uint32_t min, uint32_t max) {
    if (min >= max) return min;
    uint32_t range = max - min + 1u;
    uint32_t threshold = (uint32_t)(0xFFFFFFFFu - (0xFFFFFFFFu % range));

    uint32_t r;
    do {
        r = xoroshiro_next();
    } while (r >= threshold);

    return min + (r % range);
}

int main(void) {
    printf("=== Time and Deterministic Random Generation ===\n");

    /* Time formatting */
    time_t now = time(NULL);
    struct tm *local_info = localtime(&now);
    char buf[128];
    if (local_info && strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC/Local", local_info)) {
        printf("Current formatted timestamp: %s\n", buf);
    }

    /* Benchmarking execution clock ticks */
    clock_t start_clock = clock();
    volatile uint64_t sum = 0;
    for (int i = 0; i < 1000000; i++) {
        sum += (uint64_t)i;
    }
    clock_t end_clock = clock();
    double cpu_time_ms = ((double)(end_clock - start_clock) / CLOCKS_PER_SEC) * 1000.0;
    printf("Benchmarked 1M loop iterations: %.4f ms CPU time (accumulated: %llu)\n", cpu_time_ms, (unsigned long long)sum);

    /* Uniform unbiased random number generation */
    printf("Unbiased random rolls (1 to 6):\n  ");
    for (int i = 0; i < 10; i++) {
        printf("%u ", uniform_random_range(1, 6));
    }
    printf("\n");

    return 0;
}
```

#### Compilation & Verified Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror 09-time-and-random.c -o 09-time-and-random.exe
./09-time-and-random.exe
```

```text
=== Time and Deterministic Random Generation ===
Current formatted timestamp: 2026-11-21 11:36:06 UTC/Local
Benchmarked 1M loop iterations: 0.0000 ms CPU time (accumulated: 499999500000)
Unbiased random rolls (1 to 6):
  6 4 6 1 2 1 1 2 6 6
```

---

## 6. Best Practices

1. **Never Use `time()` or Wall Clocks for Benchmarking**: NTP updates or user clock adjustments can cause wall clock readings to jump backward or jump forward by minutes. Always use monotonic clocks (`timespec_get` with monotonic mode where available, or POSIX `clock_gettime(CLOCK_MONOTONIC)`).
2. **Use Re-entrant Functions**: Standard `localtime()` and `gmtime()` return a pointer to a single shared static buffer inside the C runtime. In multi-threaded applications, calls from different threads overwrite each other. Use `localtime_r` / `gmtime_r` (POSIX) or `localtime_s` / `gmtime_s` (C11 Annex K).
3. **Never Use `rand()` for Security or Cryptography**: `rand()` is predictable and vulnerable to seed recovery. For cryptographic keys, nonces, or tokens, use operating system entropy: `/dev/urandom` or `getrandom()` on Linux, `BCryptGenRandom` on Windows.
4. **Seed PRNGs Once at Initialization**: Calling `srand(time(NULL))` inside a loop or function re-initializes the generator with the identical seed every time the loop executes within the same second, producing repeated sequences.
5. **Eliminate Modulo Bias**: Always employ rejection sampling when mapping PRNG integers to bounded ranges.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | The Fix |
| :--- | :--- | :--- |
| `rand() % N` | Generates biased distributions when `RAND_MAX + 1` is not evenly divisible by `N`. | Use rejection sampling (discard values $\ge \text{threshold}$). |
| `srand(time(NULL))` inside a loop | Re-initializes PRNG state every iteration; within the same second, produces identical numbers. | Seed the PRNG exactly once at program startup. |
| Using `clock()` to measure wall time | `clock()` measures CPU core time consumed by process, not elapsed wall time (sleeps and I/O register as 0). | Use `timespec_get()` or POSIX `clock_gettime()`. |
| Multi-threaded `localtime(&t)` | Returns pointer to a static internal buffer; threads overwrite each other's timestamps. | Use `localtime_r(&t, &res)` or `localtime_s(&t, &res)`. |
| Assuming `tm_mon` is 1..12 | `struct tm` defines `tm_mon` as `0..11` (0 is January). | Add 1 when displaying to users: `tm->tm_mon + 1`. |
| Assuming `tm_year` is full year | `struct tm` defines `tm_year` as years since 1900 (e.g. 124 for year 2024). | Add 1900: `tm->tm_year + 1900`. |

---

## 8. Performance & Security Notes

- **The Year 2038 Problem (Y2038)**: 32-bit signed integers overflow on 2038-01-19 at 03:14:07 UTC. Systems compiling with 32-bit `time_t` will wrap to 1901. Ensure compiler flags enforce 64-bit `time_t` (`-D_TIME_BITS=64 -D_FILE_OFFSET_BITS=64` on 32-bit Linux).
- **Timing Side Channels**: In authentication systems (e.g. comparing password hashes or HMACs), measuring execution time with high-resolution clocks allows attackers to deduce byte-by-byte matches. Always compare security secrets using constant-time algorithms (`CRYPTO_memcmp` or custom non-short-circuiting loops).
- **PRNG State Leakage**: Standard LCGs (`rand()`) leak their complete internal state after observing as few as 2 or 3 output values. Never use PRNGs for session IDs or authentication tokens.

---

## 9. Comparison / When to Use What

| API / Tool | Resolution | Monotonic? | Re-entrant? | Recommended Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **`time()`** | 1 second | No (Wall) | Yes | Rough calendar timestamps |
| **`clock()`** | Microsecond | CPU Time | Yes | Profiling computational CPU cycles |
| **`timespec_get(TIME_UTC)`** | Nanosecond | No (Wall) | Yes | High-precision ISO C timestamps |
| **`clock_gettime(CLOCK_MONOTONIC)`** | Nanosecond | Yes | Yes | Production benchmarking & timeouts (POSIX) |
| **`rand()`** | 15–31 bits | N/A | No | Trivial non-critical scripts; avoid in prod |
| **Xoroshiro / PCG** | 32/64 bits | N/A | Yes | High-performance simulations, games, shuffles |
| **`getrandom()` / OS CSPRNG** | Cryptographic | N/A | Yes | Session tokens, salts, cryptographic keys |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is `clock_gettime(CLOCK_MONOTONIC)` preferred over `gettimeofday()` for benchmarking?</strong></summary>

`gettimeofday()` returns the system calendar (wall clock) time. If an administrator adjusts the system time, if daylight saving time kicks in, or if the Network Time Protocol (NTP) daemon slews or steps the clock, `gettimeofday()` can produce non-monotonic readings, including negative elapsed durations. In contrast, `CLOCK_MONOTONIC` represents time strictly increasing from an arbitrary baseline (such as system boot) and is unaffected by system clock steps, guaranteeing accurate interval measurements.
</details>

<details>
<summary><strong>2. Explain modulo bias in random number generation and how to eliminate it.</strong></summary>

When mapping numbers from a generator range $[0, M-1]$ to a target range $[0, N-1]$ using $R \% N$, if $M$ is not an exact integer multiple of $N$, the remainder values $0$ through $(M \% N) - 1$ have one additional outcome mapped to them compared to the rest of the range. To eliminate this bias, calculate the largest multiple of $N$ that fits within $M$: $threshold = M - (M % N)$. If the random generator produces a number $R >= threshold$, discard it and sample again (rejection sampling).
</details>

<details>
<summary><strong>3. What is the Year 2038 problem and how is it mitigated in modern C?</strong></summary>

The Unix Epoch began on January 1, 1970. On systems where `time_t` is defined as a 32-bit signed integer, the maximum value it can store is $2^{31}-1 = 2,147,483,647$ seconds, which corresponds to 03:14:07 UTC on Tuesday, January 19, 2038. After this point, the integer overflows to $-2,147,483,648$, wrapping to December 13, 1901. Modern 64-bit systems define `time_t` as a 64-bit signed integer, extending the overflow horizon by approximately 292 billion years.
</details>

<details>
<summary><strong>4. Why is `localtime()` unsafe in multi-threaded programs?</strong></summary>

The ISO C specification dictates that `localtime()` returns a pointer to a `struct tm`. To avoid heap allocation on every call, the standard library maintains a single static `struct tm` instance. When multiple threads call `localtime()` simultaneously, they read and write to this shared static structure without synchronization, resulting in data races and corrupted timestamp fields. The thread-safe alternative is `localtime_r()` (POSIX) or `localtime_s()` (C11), where the caller supplies the destination structure memory.
</details>

<details>
<summary><strong>5. What does the `clock()` function actually measure?</strong></summary>

`clock()` does not measure elapsed real-world time. It measures the amount of CPU processor time consumed by the calling process (and all its threads combined). If a program executes `sleep(5)`, `clock()` will show virtually zero elapsed clock ticks because the CPU core was idle during the sleep. Conversely, if 8 threads execute intensive math in parallel for 1 second, `clock()` will report approximately 8 seconds of consumed CPU time.
</details>

<details>
<summary><strong>6. How does an invariant Time Stamp Counter (TSC) work?</strong></summary>

On modern x86 processors, the `RDTSC` assembly instruction reads the 64-bit Time Stamp Counter. In older CPUs, the TSC ticked at the current processor clock frequency, meaning power saving (frequency scaling) or thermal throttling warped the timing. Modern CPUs feature an "invariant TSC" that ticks at a fixed, constant nominal frequency across all power states and sleep modes, providing an ultra-low-overhead nanosecond-accurate timer.
</details>

<details>
<summary><strong>7. Why should you never use `rand()` to generate session keys or password reset tokens?</strong></summary>

`rand()` is an algorithmic pseudo-random number generator, typically an LCG of the form $X_{n+1} = (a X_n + c) \pmod m$. The sequence is completely deterministic. Given only a small sample of outputs, an attacker can solve for the internal state and predict every future and past value generated by the process. Security tokens require true non-deterministic entropy from OS cryptographic sources (`/dev/urandom` or `getrandom()`).
</details>

<details>
<summary><strong>8. What is the difference between `gmtime()` and `localtime()`?</strong></summary>

`gmtime()` converts a `time_t` timestamp into broken-down time represented in Coordinated Universal Time (UTC). `localtime()` takes the system's active timezone rules and daylight saving time (DST) adjustments into account, converting the timestamp into local calendar time. For network protocols and database storage, UTC (`gmtime`) should always be preferred to prevent timezone conversion ambiguities.
</details>

---

## 11. Practice Exercises

### Easy: Check if a Year is a Leap Year
Write an inline function `bool is_leap_year(int year)` implementing Gregorian calendar rules (divisible by 4, not by 100 unless divisible by 400).

<details>
<summary>Solution</summary>

```c
#include <stdbool.h>

static inline bool is_leap_year(int year) {
    return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}
```
</details>

### Medium: Stopwatch Interval Timer
Implement a clean stopwatch data structure using C11 `timespec_get` providing `stopwatch_start`, `stopwatch_stop`, and `stopwatch_elapsed_ms`.

<details>
<summary>Solution</summary>

```c
#include <time.h>
#include <stdbool.h>

typedef struct {
    struct timespec start_time;
    struct timespec stop_time;
    bool is_running;
} Stopwatch;

void stopwatch_start(Stopwatch *sw) {
    timespec_get(&sw->start_time, TIME_UTC);
    sw->is_running = true;
}

void stopwatch_stop(Stopwatch *sw) {
    timespec_get(&sw->stop_time, TIME_UTC);
    sw->is_running = false;
}

double stopwatch_elapsed_ms(const Stopwatch *sw) {
    struct timespec current = sw->stop_time;
    if (sw->is_running) {
        timespec_get(&current, TIME_UTC);
    }
    double sec = (double)(current.tv_sec - sw->start_time.tv_sec);
    double nsec = (double)(current.tv_nsec - sw->start_time.tv_nsec);
    return (sec * 1000.0) + (nsec / 1e6);
}
```
</details>

### Hard: Fisher-Yates Array Shuffle with Unbiased PRNG
Implement a generic array shuffler `void shuffle(void *base, size_t n, size_t size, uint32_t (*rng_func)(void))` using the Fisher-Yates algorithm.

<details>
<summary>Solution</summary>

```c
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static uint32_t unbiased_range(uint32_t max_inclusive, uint32_t (*rng_func)(void)) {
    if (max_inclusive == 0) return 0;
    uint32_t range = max_inclusive + 1u;
    uint32_t threshold = (uint32_t)(0xFFFFFFFFu - (0xFFFFFFFFu % range));
    uint32_t r;
    do {
        r = rng_func();
    } while (r >= threshold);
    return r % range;
}

void shuffle(void *base, size_t n, size_t size, uint32_t (*rng_func)(void)) {
    if (!base || n <= 1 || size == 0) return;

    char *arr = (char *)base;
    char temp[256]; /* Buffer for small elements */

    for (size_t i = n - 1; i > 0; i--) {
        size_t j = (size_t)unbiased_range((uint32_t)i, rng_func);
        if (i == j) continue;

        /* Swap elements at index i and j */
        char *ptr_i = arr + (i * size);
        char *ptr_j = arr + (j * size);

        if (size <= sizeof(temp)) {
            memcpy(temp, ptr_i, size);
            memcpy(ptr_i, ptr_j, size);
            memcpy(ptr_j, temp, size);
        } else {
            for (size_t b = 0; b < size; b++) {
                char t = ptr_i[b];
                ptr_i[b] = ptr_j[b];
                ptr_j[b] = t;
            }
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Operation | C Idiom | Warning |
| :--- | :--- | :--- |
| Calendar Epoch Time | `time(NULL)` | 1-second resolution only |
| High-Resolution Time | `timespec_get(&ts, TIME_UTC)` | C11 standard |
| Format Datetime | `strftime(buf, sz, "%F %T", tm)` | Always check buffer return value |
| Thread-Safe UTC Time | `gmtime_r(&t, &tm)` | Never use bare `gmtime` across threads |
| CPU Execution Ticks | `clock() / (double)CLOCKS_PER_SEC` | Measures CPU usage, not sleep time |
| Fast Deterministic RNG | Xoroshiro64* / PCG32 | Far superior to `rand()` in speed and period |

---

## 13. Further Reading

- ISO/IEC 9899:2018 (C17) §7.27 (Date and time `<time.h>`).
- SEI CERT C Coding Standard: MSC50-C (Do not use `rand()` for generating pseudorandom numbers in security-sensitive contexts).
- SEI CERT C Coding Standard: MSC51-C (Ensure that random number generator seeds are properly initialized).
- David Blackman and Sebastiano Vigna, *Scrambled Linear Pseudorandom Number Generators* (ACM TOMS, 2021) — Foundations of Xoroshiro / Xoshiro algorithms.

---

[← Previous](08-unicode-locale.md) | [Section Index](README.md) | [Next →](10-undefined-behavior-and-sanitizers.md)
