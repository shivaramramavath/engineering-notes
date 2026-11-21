# ISO C11 Standard Threads (<threads.h>)

> Master the standard C threading library: ISO C11 `<threads.h>`, thread creation (`thrd_create`), joining and detaching (`thrd_join`), mutexes (`mtx_t`), condition variables (`cnd_t`), and thread-specific storage (`tss_t`).

| Metadata | Details |
| :--- | :--- |
| **Level** | Systems / Advanced |
| **Prerequisites** | [05-pthreads.md](./05-pthreads.md), [06-mutex-condvar-semaphores.md](./06-mutex-condvar-semaphores.md), [07-c11-atomics-and-memory-order.md](./07-c11-atomics-and-memory-order.md) |
| **Time to read** | 30 minutes |
| **Status** | Complete |
| **Standard Baseline**| ISO C11 / C17 / C23 (`<threads.h>`, glibc 2.28+, MSVC 2019+, Clang/GCC) |

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
Prior to the C11 standard, multi-threaded C programming relied entirely on platform-specific third-party libraries: POSIX threads (`pthreads`) on Unix/Linux/macOS, and the Win32 Threads API (`CreateThread`) on Windows. This created a fractured ecosystem where writing cross-platform concurrent libraries required verbose `#ifdef _WIN32` preprocessor abstractions.

ISO C11 introduced native language-level threading support via the optional `<threads.h>` header. It provides standardized thread lifecycles, mutual exclusion, condition variables, and thread-specific storage directly in the standard library. If a compiler implementation does not support threading, it defines the macro `__STDC_NO_THREADS__`.

---

## 2. Core Concepts
- **Thread Management**:
  - `thrd_t`: Thread handle type.
  - `thrd_create(&t, func, arg)`: Spawns a thread executing `int (*thrd_start_t)(void *)`. Notice that unlike pthreads (which returns `void *`), C11 thread routines return an `int` status code!
  - `thrd_join(t, &res)`: Waits for thread termination and retrieves the integer exit status.
  - `thrd_detach(t)`: Relinquishes joinability; reclaims resources automatically on exit.
- **Mutex Management (`mtx_t`)**:
  - `mtx_plain`: Standard simple mutual exclusion lock.
  - `mtx_timed`: Mutex supporting timeout acquisitions (`mtx_timedlock`).
  - `mtx_recursive`: Recursive lock allowing re-acquisition by the same owning thread.
- **Condition Variables (`cnd_t`)**:
  - `cnd_init`, `cnd_wait`, `cnd_timedwait`, `cnd_signal`, `cnd_broadcast`, `cnd_destroy`.
- **Thread-Specific Storage (`tss_t`)**:
  - Standardized thread-local key allocation (`tss_create`) with registered destructor callbacks.

---

## 3. How It Works Internally

### Standard Abstraction Layer Mapping
Under the hood, C standard library implementations map `<threads.h>` primitives directly onto the host operating system's native threading APIs:

```
C11 Standard Code:        #include <threads.h> -> thrd_create(), mtx_lock()
                                        |
                 +----------------------+----------------------+
                 | (Linux / glibc / musl)                      | (Windows / MSVC / UCRT)
                 v                                             v
POSIX pthreads: pthread_create(), pthread_mutex_lock()       Win32: CreateThread(), SRWLock
```

On Linux (glibc 2.28+), `thrd_t` is typedef'd directly to `pthread_t`, and `thrd_create` is an inlined wrapper around `pthread_create`. On modern Windows (MSVC 2019+ and UCRT), `mtx_t` maps to high-speed Slim Reader/Writer (SRW) locks or Critical Sections.

---

## 4. Syntax & Usage

### Primary Header Inclusions
```c
#ifndef __STDC_NO_THREADS__
#include <threads.h>
#endif
```

### Signature Overview
```c
int thrd_create(thrd_t *thr, thrd_start_t func, void *arg);
int thrd_join(thrd_t thr, int *res);
int thrd_detach(thrd_t thr);
void thrd_exit(int res);
thrd_t thrd_current(void);
void thrd_yield(void);
int thrd_sleep(const struct timespec *duration, struct timespec *remaining);

int mtx_init(mtx_t *mutex, int type);
int mtx_lock(mtx_t *mutex);
int mtx_unlock(mtx_t *mutex);
void mtx_destroy(mtx_t *mutex);

int cnd_init(cnd_t *cond);
int cnd_wait(cnd_t *cond, mtx_t *mutex);
int cnd_signal(cnd_t *cond);
int cnd_broadcast(cnd_t *cond);
void cnd_destroy(cnd_t *cond);
```

---

## 5. Practical Examples

### Example 1: ISO C11 Threads Execution Cycle
```c
#include <stdio.h>
#include <stdint.h>

typedef enum {
    THRD_SUCCESS = 0,
    THRD_ERROR = 1
} thrd_status_t;

typedef struct {
    int id;
    int input;
    int output;
} ThreadData;

int simulated_thrd_routine(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    data->output = data->input * data->input;
    return THRD_SUCCESS;
}

int main(void) {
    printf("Initializing ISO C11 <threads.h> Model Simulation...\n");
    ThreadData threads[2] = {
        { .id = 1, .input = 12, .output = 0 },
        { .id = 2, .input = 25, .output = 0 }
    };

    for (int i = 0; i < 2; i++) {
        int res = simulated_thrd_routine(&threads[i]);
        printf("Thread %d (thrd_create) executed with status %d\n", threads[i].id, res);
    }

    for (int i = 0; i < 2; i++) {
        printf("Thread %d (thrd_join) computed result: %d\n", threads[i].id, threads[i].output);
    }
    printf("C11 threads execution cycle finished cleanly.\n");
    return 0;
}
```

```text
Initializing ISO C11 <threads.h> Model Simulation...
Thread 1 (thrd_create) executed with status 0
Thread 2 (thrd_create) executed with status 0
Thread 1 (thrd_join) computed result: 144
Thread 2 (thrd_join) computed result: 625
C11 threads execution cycle finished cleanly.
```

### Example 2: Synchronized Task Worker with `mtx_t` and `cnd_t`
```c
#ifndef __STDC_NO_THREADS__
#include <threads.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    mtx_t lock;
    cnd_t cv;
    bool ready;
    int payload;
} SharedChannel;

int worker_func(void *arg) {
    SharedChannel *chan = (SharedChannel *)arg;
    
    mtx_lock(&chan->lock);
    while (!chan->ready) {
        cnd_wait(&chan->cv, &chan->lock);
    }
    printf("Worker received payload: %d\n", chan->payload);
    mtx_unlock(&chan->lock);
    return 0;
}
#endif
```

---

## 6. Best Practices
1. **Always Check `__STDC_NO_THREADS__`**: Because `<threads.h>` is an optional ISO C11 feature, library headers should check `#ifdef __STDC_NO_THREADS__` and provide a fallback wrapper around pthreads or Win32 threads.
2. **Return `int` from Thread Routines**: Remember that C11 threads return an `int` exit code rather than `void *`. Returning pointer addresses directly can cause truncation on 64-bit platforms.
3. **Use `mtx_destroy()` on Allocated Mutexes**: While some OS implementations use lightweight structs, others allocate kernel synchronization resources. Always clean up with `mtx_destroy()`.
4. **Use `_Thread_local` (or `<threads.h>` `thread_local`)**: For simple per-thread state, the language keyword `_Thread_local` is faster and cleaner than manual `tss_t` key management.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Returning pointer from `thrd_start_t` | C11 thread function signature is `int (*)(void *)`; casting a 64-bit pointer to 32-bit `int` truncates addresses | Store results in heap struct passed via argument |
| Assuming `<threads.h>` is available everywhere | Optional in C11; older compilers (e.g. GCC 4.x or older macOS SDKs) omit `<threads.h>` | Guard with `#ifndef __STDC_NO_THREADS__` |
| Attempting thread cancellation | C11 threads have no equivalent to `pthread_cancel()` | Use cooperative cancellation with atomic boolean flags |
| Mixing `mtx_t` with `pthread_cond_wait()` | Type mismatch; C11 condition variables work strictly with `mtx_t`, not `pthread_mutex_t` | Use matching primitives from the same API family |
| Forgetting loop around `cnd_wait` | Like POSIX, C11 condition variables permit spurious wakeups | Always write `while (!predicate) cnd_wait(&cond, &mutex);` |

---

## 8. Performance & Security Notes
- **Thin Abstraction Overhead**: Modern implementations of `<threads.h>` inline almost all calls directly into host system primitives. There is zero runtime performance penalty compared to raw `pthreads` on Linux or SRW locks on Windows.
- **Missing Features Compared to Pthreads**: C11 threads omit stack size configuration, thread affinity (pinning to specific CPU cores), priority scheduling, and asynchronous cancellation. High-performance systems programming requiring fine-grained control over core binding or RTOS priorities should retain native pthreads.

---

## 9. Comparison / When to Use What

| Feature | ISO C11 Threads (`<threads.h>`) | POSIX Threads (`pthreads`) |
| :--- | :--- | :--- |
| **Portability** | Standard C (Windows & POSIX) | POSIX only (Linux, macOS, BSD, WSL) |
| **Return Type** | `int` | `void *` |
| **Cancellation** | Not supported (Cooperative only) | Supported (`pthread_cancel`) |
| **Stack Sizing** | Not supported (OS default only) | Supported (`pthread_attr_setstacksize`) |
| **CPU Affinity** | Not supported | Supported (`pthread_setaffinity_np`) |

---

## 10. Interview Questions

<details>
<summary>1. Is `<threads.h>` guaranteed to be present in every C11 conforming compiler?</summary>
No. In ISO C11, multi-threading support is explicitly optional. A conforming implementation can define `__STDC_NO_THREADS__ = 1` and omit `<threads.h>` entirely (as Apple did in Xcode macOS SDKs for many years, preferring pthreads).
</details>

<details>
<summary>2. How does the return type of a C11 thread routine differ from pthreads?</summary>
In pthreads, the routine signature is `void *(*start_routine)(void *)`, allowing arbitrary pointers to be returned to `pthread_join()`. In C11 `<threads.h>`, the signature is `int (*thrd_start_t)(void *)`, returning a simple integer status code (analogous to process exit status).
</details>

<details>
<summary>3. What mutex types does C11 `mtx_init()` support?</summary>
The second argument is a bitwise mask composed of:
- `mtx_plain`: Simple non-recursive mutex.
- `mtx_recursive`: Recursive mutex allowing repeated locking by the same thread.
- `mtx_timed`: Mutex supporting timeout operations via `mtx_timedlock()`.
These flags can be combined (e.g. `mtx_recursive | mtx_timed`).
</details>

<details>
<summary>4. What is the difference between `tss_t` and `_Thread_local`?</summary>
`_Thread_local` is a compiler keyword (macro `thread_local` in `<threads.h>`) for static and global variables, giving each thread a private copy allocated automatically. `tss_t` is a dynamic, runtime key-based Thread-Specific Storage facility that allows registering custom destructor functions executed when a thread terminates.
</details>

<details>
<summary>5. How does `thrd_sleep()` differ from POSIX `nanosleep()`?</summary>
`thrd_sleep()` takes two pointers to `struct timespec` (requested duration and remaining duration if interrupted). It returns 0 on success, -1 if interrupted by a signal, and a negative value on failure, providing a standardized sub-second sleep primitive across Windows and POSIX.
</details>

<details>
<summary>6. How can you terminate a C11 thread cooperatively from another thread?</summary>
Because C11 lacks `pthread_cancel()`, the application must share an atomic boolean flag (`atomic_bool stop_requested;`). The worker thread periodically inspects the flag, and if true, cleans up resources and returns from its start routine or calls `thrd_exit(0)`.
</details>

<details>
<summary>7. What is `thrd_yield()` and how does it affect scheduling?</summary>
`thrd_yield()` provides a hint to the operating system scheduler that the calling thread is willing to relinquish the remainder of its current CPU time slice. The scheduler may deschedule the calling thread and run another ready thread of equal or higher priority. Unlike `thrd_sleep()`, `thrd_yield()` does not guarantee any minimum sleep duration.
</details>

<details>
<summary>8. How do `cnd_timedwait()` and `mtx_timedlock()` handle clock timeouts in C11?</summary>
Both functions require an absolute calendar timeout specified via `struct timespec` (using the system's `TIME_UTC` base obtained via `timespec_get(&ts, TIME_UTC)`). If the condition variable is not signaled or the mutex is not acquired before the specified absolute time arrives, the functions return `thrd_timedout`.
</details>

---

## 11. Practice Exercises

### Easy
1. **C11 Hello Thread**: Write a program using `thrd_create` and `thrd_join` that prints "C11 Worker Running" and returns 42.
2. **C11 Mutex Counter**: Increment a shared variable 10,000 times across 2 threads using `mtx_t`.
3. **High-Resolution Sleep**: Use `thrd_sleep()` to sleep for 250 milliseconds (250,000,000 nanoseconds).

<details>
<summary>Solutions (Easy)</summary>

```c
/* Exercise 1 */
#include <stdio.h>
#ifndef __STDC_NO_THREADS__
#include <threads.h>

int worker(void *arg) {
    (void)arg;
    printf("C11 Worker Running\n");
    return 42;
}

void run(void) {
    thrd_t t;
    thrd_create(&t, worker, NULL);
    int res = 0;
    thrd_join(t, &res);
    printf("Joined with exit code: %d\n", res);
}
#endif
```
</details>

### Medium
4. **C11 Thread-Safe Logger**: Implement a thread-safe file logger using `mtx_t` guarding `fprintf()`.
5. **C11 Producer-Consumer**: Implement a 1-item handoff channel between 2 threads using `mtx_t` and `cnd_t`.
6. **Thread-Local ID**: Use `_Thread_local` to assign a unique sequential worker index to each spawned thread.

<details>
<summary>Solutions (Medium)</summary>

```c
/* Exercise 5: 1-Item Handoff Channel */
#include <stdbool.h>
#ifndef __STDC_NO_THREADS__
#include <threads.h>

typedef struct {
    int val;
    bool full;
    mtx_t lock;
    cnd_t cv;
} Handoff;

void handoff_put(Handoff *h, int v) {
    mtx_lock(&h->lock);
    while (h->full) cnd_wait(&h->cv, &h->lock);
    h->val = v;
    h->full = true;
    cnd_signal(&h->cv);
    mtx_unlock(&h->lock);
}

int handoff_get(Handoff *h) {
    mtx_lock(&h->lock);
    while (!h->full) cnd_wait(&h->cv, &h->lock);
    int v = h->val;
    h->full = false;
    cnd_signal(&h->cv);
    mtx_unlock(&h->lock);
    return v;
}
#endif
```
</details>

### Hard
7. **Cross-Platform Thread Wrapper**: Implement a header `compat_threads.h` that detects `<threads.h>` and provides a clean fall-back to POSIX pthreads on systems where `__STDC_NO_THREADS__` is defined.
8. **Timed Mutex Trylock Loop**: Implement a worker that attempts to acquire an `mtx_timed` lock with a 50 ms timeout, logging timeout events if another thread holds the lock too long.
9. **C11 Dynamic Thread Pool**: Implement a 4-thread worker pool using only C11 `<threads.h>` primitives.

<details>
<summary>Solutions (Hard)</summary>

```c
/* Exercise 7: Compat Wrapper Architecture */
#ifndef COMPAT_THREADS_H
#define COMPAT_THREADS_H

#if !defined(__STDC_NO_THREADS__) && (__STDC_VERSION__ >= 201112L)
  #include <threads.h>
#else
  #include <pthread.h>
  typedef pthread_t thrd_t;
  typedef pthread_mutex_t mtx_t;
  typedef pthread_cond_t cnd_t;
  enum { thrd_success = 0, thrd_error = 1 };
  enum { mtx_plain = 0 };

  static inline int mtx_init_compat(mtx_t *m, int type) {
      (void)type; return pthread_mutex_init(m, NULL);
  }
  static inline int mtx_lock_compat(mtx_t *m) { return pthread_mutex_lock(m); }
  static inline int mtx_unlock_compat(mtx_t *m) { return pthread_mutex_unlock(m); }
#endif

#endif
```
</details>

---

## 12. Summary / Cheat Sheet
- **Availability**: Optional in C11 (`#ifndef __STDC_NO_THREADS__`).
- **Threads**: `thrd_create(&t, fn, arg)`, `thrd_join(t, &res)` (returns `int`).
- **Mutex**: `mtx_init(&m, mtx_plain)`, `mtx_lock(&m)`, `mtx_unlock(&m)`.
- **Condvar**: `cnd_wait(&cv, &m)`, `cnd_signal(&cv)`.
- **Thread Local**: `_Thread_local int val;` or `tss_create(&key, destructor)`.

---

## 13. Further Reading
- ISO/IEC 9899:2011 (C11 Standard), Section 7.26 (Threads `<threads.h>`).
- Jens Gustedt, *Modern C*, Section on Imperative Concurrency.
- cppreference.com: C Thread support library documentation.

---

[← Previous Note: C11 Atomics & Memory Model](./07-c11-atomics-and-memory-order.md) | [Module Index](README.md) | [Next Note: Network Sockets: TCP & UDP](./09-sockets-tcp-udp.md)
