# Mutexes, Condition Variables, and Semaphores

> Master concurrent thread synchronization: mutual exclusion (`pthread_mutex_t`), avoiding deadlocks, condition variables (`pthread_cond_t`), spurious wakeups, and POSIX semaphores (`sem_t`).

| Metadata | Details |
| :--- | :--- |
| **Level** | Systems / Advanced |
| **Prerequisites** | [05-pthreads.md](./05-pthreads.md), [01-fundamentals/14-sequence-points-and-evaluation-order.md](../01-fundamentals/14-sequence-points-and-evaluation-order.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| POSIX.1-2008 / ISO C11 (glibc NPTL, Linux futex synchronization) |

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
In multi-threaded programming, shared memory enables ultra-fast communication between threads. However, without synchronization, concurrent reads and writes to shared variables cause **data races**—situations where the final outcome depends on non-deterministic thread interleaving, resulting in memory corruption and undefined behavior.

To coordinate access to shared state, POSIX provides three fundamental synchronization primitives:
1. **Mutexes (`pthread_mutex_t`)**: Enforce mutual exclusion so that only one thread can execute a critical section at any given instant.
2. **Condition Variables (`pthread_cond_t`)**: Enable threads to suspend execution and sleep efficiently until another thread notifies them that a specific application condition (e.g. "queue is not empty") has become true.
3. **Semaphores (`sem_t`)**: Maintain an atomic integer counter to regulate access to a finite pool of shared resources (e.g. limiting database connections to 10).

---

## 2. Core Concepts
- **Critical Section**: A block of code that accesses shared mutable state and must be executed atomically by at most one thread at a time.
- **Mutex Types**:
  - `PTHREAD_MUTEX_NORMAL`: Fast default; attempting to re-lock from the same thread causes deadlock.
  - `PTHREAD_MUTEX_RECURSIVE`: Allows the holding thread to lock multiple times; requires matching number of unlocks.
  - `PTHREAD_MUTEX_ERRORCHECK`: Returns `EDEADLK` if a thread attempts to re-lock a mutex it already owns.
- **Condition Variables & Predicates**: A condition variable is **not** a boolean flag. It is a signaling queue that must always be paired with an external boolean predicate and an associated mutex.
- **Spurious Wakeup**: POSIX explicitly allows `pthread_cond_wait()` to return even if no thread signaled the condition variable. Therefore, condition waits must **always** occur within a `while` loop that re-checks the predicate: `while (!condition) pthread_cond_wait(&cond, &mutex);`.
- **Counting Semaphores**:
  - `sem_wait(&sem)`: Decrements counter. If counter is 0, blocks until counter > 0.
  - `sem_post(&sem)`: Increments counter atomically, waking any blocked waiting thread.

---

## 3. How It Works Internally

### Linux Fast Userspace Mutex (Futex)
Modern Linux systems do not execute a costly kernel transition for uncontended mutex acquisitions. Instead, pthreads mutexes are implemented using the **Futex** (`sys_futex`) subsystem:

```
Thread A (Uncontended Fast Path)              Thread B (Contended Slow Path)
+-----------------------------------+        +-----------------------------------+
| atomic_cmpxchg(mutex_word, 0, 1)  |        | atomic_cmpxchg(mutex_word, 0, 1)  |
| Returns SUCCESS                   |        | Fails (Value is already 1)        |
| [Critical Section Executes]       |        |                                   |
| atomic_store(mutex_word, 0)       |        | Transition to Kernel:             |
| [Done - 0 Syscalls Invoked!]      |        | sys_futex(FUTEX_WAIT, &word, 1)   |
+-----------------------------------+        | Descheduled to sleep queue        |
                                             +-----------------------------------+
                                                              |
                                             Woken by Thread A unlock via:
                                             sys_futex(FUTEX_WAKE, &word, 1)
```

1. **Uncontended Case**: An atomic Compare-And-Swap (CAS) instruction updates a 32-bit integer in user-space RAM from 0 to 1. The lock is acquired in $pprox 10$ nanoseconds without entering the kernel.
2. **Contended Case**: If another thread already holds the lock, the contending thread issues the `futex` system call. The Linux kernel suspends the thread on an internal wait queue, freeing CPU cores until the lock owner unlocks and issues `FUTEX_WAKE`.

---

## 4. Syntax & Usage

### Primary Header Inclusions
```c
#include <pthread.h>
#include <semaphore.h>
```

### Signature Overview
```c
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_trylock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
int pthread_mutex_destroy(pthread_mutex_t *mutex);

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);
int pthread_cond_destroy(pthread_cond_t *cond);

int sem_init(sem_t *sem, int pshared, unsigned int value);
int sem_wait(sem_t *sem);
int sem_post(sem_t *sem);
int sem_destroy(sem_t *sem);
```

---

## 5. Practical Examples

### Example 1: Synchronized Bounded Buffer (Producer-Consumer Logic)
```c
#include <stdio.h>
#include <stdbool.h>

#define QUEUE_CAP 4

typedef struct {
    int items[QUEUE_CAP];
    int head;
    int tail;
    int count;
    bool locked;
} BoundedBuffer;

void buf_init(BoundedBuffer *q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->locked = false;
}

bool buf_push(BoundedBuffer *q, int item) {
    if (q->count == QUEUE_CAP) return false; /* Buffer full; wait on cond_not_full */
    q->items[q->tail] = item;
    q->tail = (q->tail + 1) % QUEUE_CAP;
    q->count++;
    return true;
}

bool buf_pop(BoundedBuffer *q, int *out) {
    if (q->count == 0) return false; /* Buffer empty; wait on cond_not_empty */
    *out = q->items[q->head];
    q->head = (q->head + 1) % QUEUE_CAP;
    q->count--;
    return true;
}

int main(void) {
    BoundedBuffer queue;
    buf_init(&queue);
    printf("Initializing Synchronized Bounded Buffer (Capacity: %d)\n", QUEUE_CAP);

    for (int i = 10; i <= 40; i += 10) {
        buf_push(&queue, i);
        printf("Producer pushed %d (Queue count: %d/%d)\n", i, queue.count, QUEUE_CAP);
    }

    bool overflow = buf_push(&queue, 50);
    printf("Producer push on FULL buffer result: %s (Must wait on cond_not_full)\n", overflow ? "SUCCESS" : "BLOCKED");

    int val = 0;
    while (buf_pop(&queue, &val)) {
        printf("Consumer popped %d (Queue count: %d/%d, signaled cond_not_full)\n", val, queue.count, QUEUE_CAP);
    }

    bool underflow = buf_pop(&queue, &val);
    printf("Consumer pop on EMPTY buffer result: %s (Must wait on cond_not_empty)\n", underflow ? "SUCCESS" : "BLOCKED");
    return 0;
}
```

```text
Initializing Synchronized Bounded Buffer (Capacity: 4)
Producer pushed 10 (Queue count: 1/4)
Producer pushed 20 (Queue count: 2/4)
Producer pushed 30 (Queue count: 3/4)
Producer pushed 40 (Queue count: 4/4)
Producer push on FULL buffer result: BLOCKED (Must wait on cond_not_full)
Consumer popped 10 (Queue count: 3/4, signaled cond_not_full)
Consumer popped 20 (Queue count: 2/4, signaled cond_not_full)
Consumer popped 30 (Queue count: 1/4, signaled cond_not_full)
Consumer popped 40 (Queue count: 0/4, signaled cond_not_full)
Consumer pop on EMPTY buffer result: BLOCKED (Must wait on cond_not_empty)
```

### Example 2: Full Multi-Threaded Producer-Consumer with Condvars
```c
#include <pthread.h>
#include <stdbool.h>

#define CAPACITY 8

typedef struct {
    int buffer[CAPACITY];
    int head, tail, count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} PCQueue;

void pc_init(PCQueue *q) {
    q->head = q->tail = q->count = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void pc_push(PCQueue *q, int val) {
    pthread_mutex_lock(&q->lock);
    while (q->count == CAPACITY) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    q->buffer[q->tail] = val;
    q->tail = (q->tail + 1) % CAPACITY;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

int pc_pop(PCQueue *q) {
    pthread_mutex_lock(&q->lock);
    while (q->count == 0) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    int val = q->buffer[q->head];
    q->head = (q->head + 1) % CAPACITY;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return val;
}
```

---

## 6. Best Practices
1. **Always Use a `while` Loop with Condition Variables**: Never use `if (!ready) pthread_cond_wait(...)`. Spurious wakeups and race conditions among signaled threads require re-evaluating the predicate in a loop.
2. **Establish Strict Lock Ordering**: If multiple locks must be acquired simultaneously (e.g. transferring money from Account A to Account B), always acquire them in a globally consistent order (e.g. sorted by memory address or account ID) to prevent deadlocks.
3. **Keep Critical Sections Minimal**: Hold locks only while accessing or modifying shared memory. Never perform slow disk I/O, network requests, or heavy computations inside a locked section.
4. **Always Unlock Mutexes on All Return Paths**: Ensure every error path unlocks the acquired mutex, or use cleanup handlers to prevent orphaned locks.
5. **Destroy Synchronization Objects**: When finished with dynamically allocated mutexes or semaphores, call `pthread_mutex_destroy()` and `sem_destroy()` to release kernel resources.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Using `if` instead of `while` for `pthread_cond_wait` | Spurious wakeups cause threads to proceed when the queue is still empty, causing buffer underflows | Always write `while (!condition) pthread_cond_wait(&cond, &mutex);` |
| Signaling condition variable without updating state | Waiting thread wakes up, finds predicate still false, and immediately returns to sleep | Update shared predicate variables before signaling |
| Inconsistent lock acquisition order | Thread 1 acquires Lock A then B; Thread 2 acquires Lock B then A $	o$ Circular Deadlock | Enforce strict global hierarchy: always acquire Lock A before Lock B |
| Calling `pthread_mutex_unlock` on an unowned mutex | Undefined behavior; on error-checking mutexes returns `EPERM` | Only the thread that successfully locked the mutex may unlock it |
| Destroying a locked mutex | Undefined behavior; subsequent threads attempting to lock corrupt memory | Ensure all threads have finished and mutex is unlocked before calling destroy |
| Forgetting to initialize mutex or condvar | Accessing uninitialized synchronization structures causes instant memory crashes | Initialize statically (`PTHREAD_MUTEX_INITIALIZER`) or dynamically via `init()` |

---

## 8. Performance & Security Notes
- **Lock Contention**: When many threads compete for the same mutex, threads spend more time in kernel context switches than doing useful work. Mitigate contention by partitioning data (e.g. sharded hash tables with separate bucket locks) or switching to lock-free read-mostly primitives (`pthread_rwlock_t`).
- **Priority Inversion**: If a low-priority thread holds a mutex, and a high-priority thread blocks waiting for it, an intermediate-priority thread can preempt the low-priority thread, starving the high-priority thread indefinitely. POSIX provides Priority Inheritance mutexes (`pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT)`) to elevate the low-priority thread temporarily.
- **Deadlock Detection via Lockdep / Helgrind**: Detect synchronization flaws before production by profiling multi-threaded test suites with Valgrind's Helgrind tool (`valgrind --tool=helgrind ./app`) or ThreadSanitizer (`-fsanitize=thread`).

---

## 9. Comparison / When to Use What

| Primitive | Mechanism | Ownership | Best Use Case |
| :--- | :--- | :--- | :--- |
| **Mutex (`pthread_mutex_t`)** | Binary lock (0 or 1) | Strict (Only locker can unlock) | Protecting shared structs and data structures |
| **Condition Variable (`pthread_cond_t`)** | Signaling queue | None (Paired with a mutex) | State transitions (buffer full/empty, task ready) |
| **Semaphore (`sem_t`)** | Atomic counter | None (Any thread can post) | Resource pools, signaling between ISRs/threads |
| **Read-Write Lock (`pthread_rwlock_t`)** | Multiple readers / single writer | Readers or Writer | Read-heavy caches and routing tables |
| **Spinlock (`pthread_spinlock_t`)** | Busy-wait CPU loop | Strict | Ultra-short critical sections (< 50 instructions) |

---

## 10. Interview Questions

<details>
<summary>1. Why does `pthread_cond_wait()` require a mutex parameter?</summary>
To eliminate the race condition between checking the condition predicate and putting the thread to sleep. `pthread_cond_wait()` atomically releases the mutex and suspends the calling thread in the kernel sleep queue. Without this atomicity, another thread could signal the condition *after* the first thread unlocked the mutex but *before* it fell asleep, causing the signal to be missed permanently (lost wakeup).
</details>

<details>
<summary>2. What is a "spurious wakeup" and how must code handle it?</summary>
A spurious wakeup occurs when `pthread_cond_wait()` returns even though no thread called `pthread_cond_signal()` or `pthread_cond_broadcast()`. This is permitted by the POSIX specification to allow high-performance kernel thread scheduling on multiprocessor systems. To prevent race conditions, the condition wait must always be enclosed in a `while` loop that re-checks the application predicate: `while (!work_ready) pthread_cond_wait(&cond, &lock);`.
</details>

<details>
<summary>3. What is the difference between `pthread_cond_signal()` and `pthread_cond_broadcast()`?</summary>
`pthread_cond_signal()` wakes up at least one thread waiting on the condition variable. `pthread_cond_broadcast()` wakes up all threads currently waiting on the condition variable. Use broadcast when the state change satisfies multiple threads (e.g. worker pool shutdown or barrier completion).
</details>

<details>
<summary>4. What are the four Coffman conditions required for a deadlock to occur?</summary>
1. **Mutual Exclusion**: Resources cannot be shared simultaneously.
2. **Hold and Wait**: A thread holds at least one resource while waiting to acquire another.
3. **No Preemption**: Resources cannot be forcibly confiscated from a holding thread.
4. **Circular Wait**: A closed chain of threads exists such that each thread holds a resource requested by the next thread.
Eliminating any one condition prevents deadlocks.
</details>

<details>
<summary>5. What is the difference between a binary semaphore and a mutex?</summary>
A mutex has **ownership**: only the thread that locked the mutex is permitted to unlock it. A semaphore has no concept of ownership: any thread (or interrupt service routine) can call `sem_post()` to increment the semaphore, allowing one thread to wait and another thread to post for synchronization.
</details>

<details>
<summary>6. How does a recursive mutex work, and when should it be avoided?</summary>
A recursive mutex allows the thread currently holding the lock to acquire it again without deadlocking, maintaining an internal recursion depth count. It must be unlocked an equal number of times before other threads can acquire it. It should generally be avoided because it often masks poor architectural separation of concerns and cannot be used with condition variables.
</details>

<details>
<summary>7. What happens if a thread terminates while holding a mutex?</summary>
On standard mutexes, the mutex remains locked permanently, causing any other thread that attempts to acquire it to deadlock indefinitely. POSIX provides robust mutexes (`PTHREAD_MUTEX_ROBUST`), which cause subsequent `pthread_mutex_lock()` calls to return `EOWNERDEAD`, allowing the acquiring thread to repair state and mark the mutex consistent.
</details>

<details>
<summary>8. How do Read-Write locks (`pthread_rwlock_t`) improve performance?</summary>
They allow concurrent access for multiple read-only operations while maintaining exclusive access for write operations. In read-heavy scenarios (e.g. 95% reads, 5% writes), read-write locks significantly reduce thread contention compared to standard mutual exclusion.
</details>

---

## 11. Practice Exercises

### Easy
1. **Thread Counter Protected by Mutex**: Write a program where 4 threads each increment a shared global counter 10,000 times using a mutex. Verify the final value is exactly 40,000.
2. **Simple Binary Semaphore**: Implement a synchronization point where thread B waits on `sem_wait()` until thread A finishes its initialization and calls `sem_post()`.
3. **Trylock Demo**: Write a function that attempts to lock a mutex using `pthread_mutex_trylock()`, prints whether it acquired the lock or found it busy, and unlocks if acquired.

<details>
<summary>Solutions (Easy)</summary>

```c
/* Exercise 1 */
#include <pthread.h>
#include <stdio.h>

static int counter = 0;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *worker_inc(void *arg) {
    (void)arg;
    for (int i = 0; i < 10000; i++) {
        pthread_mutex_lock(&lock);
        counter++;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}
```
</details>

### Medium
4. **Thread Barrier Implementation**: Implement a reusable thread barrier `void barrier_wait(Barrier *b)` for $N$ threads using a mutex and a condition variable.
5. **Connection Pool Semaphore**: Use a POSIX counting semaphore initialized to 5 to simulate a pool of 5 database connections shared across 20 worker threads.
6. **Dining Philosophers Deadlock Solution**: Implement the dining philosophers problem for 5 philosophers, using global resource ordering (lock lowest-numbered fork first) to prove deadlock impossibility.

<details>
<summary>Solutions (Medium)</summary>

```c
/* Exercise 4: Reusable Barrier */
#include <pthread.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int threshold;
    int count;
    int cycle;
} Barrier;

void barrier_init(Barrier *b, int count) {
    pthread_mutex_init(&b->mutex, NULL);
    pthread_cond_init(&b->cond, NULL);
    b->threshold = count;
    b->count = 0;
    b->cycle = 0;
}

void barrier_wait(Barrier *b) {
    pthread_mutex_lock(&b->mutex);
    int current_cycle = b->cycle;
    b->count++;
    if (b->count == b->threshold) {
        b->cycle++;
        b->count = 0;
        pthread_cond_broadcast(&b->cond);
    } else {
        while (current_cycle == b->cycle) {
            pthread_cond_wait(&b->cond, &b->mutex);
        }
    }
    pthread_mutex_unlock(&b->mutex);
}
```
</details>

### Hard
7. **Thread-Safe Multi-Producer Multi-Consumer Bounded Queue**: Implement a complete generic bounded ring buffer supporting arbitrary pointer items, graceful cancellation shutdown, and timeout waits.
8. **Fair Ticket Lock Implementation**: Build a starvation-free ticket lock using standard C atomics and condition variables where threads receive a ticket number and wait for their turn.
9. **Reader-Writer Lock with Writer Preference**: Implement a custom reader-writer lock from scratch using a mutex and condition variables that prioritizes waiting writers to prevent writer starvation.

<details>
<summary>Solutions (Hard)</summary>

```c
/* Exercise 9: Writer-Preferred Read-Write Lock */
#include <pthread.h>

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t readers_ok;
    pthread_cond_t writers_ok;
    int active_readers;
    int waiting_writers;
    int writer_active;
} WriterPrefRWLock;

void rwlock_init(WriterPrefRWLock *rw) {
    pthread_mutex_init(&rw->lock, NULL);
    pthread_cond_init(&rw->readers_ok, NULL);
    pthread_cond_init(&rw->writers_ok, NULL);
    rw->active_readers = 0;
    rw->waiting_writers = 0;
    rw->writer_active = 0;
}

void rwlock_rdlock(WriterPrefRWLock *rw) {
    pthread_mutex_lock(&rw->lock);
    /* Readers wait if a writer is active OR writers are queued */
    while (rw->writer_active || rw->waiting_writers > 0) {
        pthread_cond_wait(&rw->readers_ok, &rw->lock);
    }
    rw->active_readers++;
    pthread_mutex_unlock(&rw->lock);
}

void rwlock_wrlock(WriterPrefRWLock *rw) {
    pthread_mutex_lock(&rw->lock);
    rw->waiting_writers++;
    while (rw->writer_active || rw->active_readers > 0) {
        pthread_cond_wait(&rw->writers_ok, &rw->lock);
    }
    rw->waiting_writers--;
    rw->writer_active = 1;
    pthread_mutex_unlock(&rw->lock);
}

void rwlock_unlock(WriterPrefRWLock *rw) {
    pthread_mutex_lock(&rw->lock);
    if (rw->writer_active) {
        rw->writer_active = 0;
    } else {
        rw->active_readers--;
    }

    if (rw->waiting_writers > 0) {
        if (rw->active_readers == 0) {
            pthread_cond_signal(&rw->writers_ok);
        }
    } else {
        pthread_cond_broadcast(&rw->readers_ok);
    }
    pthread_mutex_unlock(&rw->lock);
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Mutex Init**: `pthread_mutex_init(&m, NULL)` or `= PTHREAD_MUTEX_INITIALIZER`.
- **Lock/Unlock**: Always pair `pthread_mutex_lock(&m)` with `pthread_mutex_unlock(&m)`.
- **Condvar Pattern**:
  ```c
  pthread_mutex_lock(&m);
  while (!predicate) pthread_cond_wait(&cv, &m);
  /* Mutex is held and predicate is true */
  pthread_mutex_unlock(&m);
  ```
- **Signal**: `pthread_cond_signal(&cv)` (wakes 1), `pthread_cond_broadcast(&cv)` (wakes all).
- **Semaphores**: `sem_init(&s, 0, val)`, `sem_wait(&s)` (dec), `sem_post(&s)` (inc).
- **Deadlock Avoidance**: Enforce globally identical lock acquisition order.

---

## 13. Further Reading
- W. Richard Stevens, *UNIX Network Programming, Volume 2: Interprocess Communications*, Part 2 (Semaphores & Mutexes).
- Michael Kerrisk, *The Linux Programming Interface*, Chapters 30 & 31.
- Silberschatz, Galvin, Gagne, *Operating System Concepts*, Chapter 6 (Synchronization Tools).

---

[← Previous Note: POSIX Threads (pthreads)](./05-pthreads.md) | [Module Index](README.md) | [Next Note: C11 Atomics and Memory Model](./07-c11-atomics-and-memory-order.md)
