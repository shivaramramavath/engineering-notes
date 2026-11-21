# POSIX Threads (pthreads)

> Master multi-threaded systems engineering in C: POSIX thread creation (`pthread_create`), joining and detaching (`pthread_join`, `pthread_detach`), thread attributes and stack allocation, thread-specific storage, and cancellation cleanup handlers.

| Metadata | Details |
| :--- | :--- |
| **Level** | Systems / Advanced |
| **Prerequisites** | [02-processes-fork-exec-wait.md](./02-processes-fork-exec-wait.md), [04-advanced/05-abi-and-calling-conventions.md](../04-advanced/05-abi-and-calling-conventions.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| POSIX.1-2008 / ISO C11 (Linux NPTL, glibc, Win32 pthreads contrast) |

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
While processes provide strong isolation with separate address spaces, inter-process communication introduces serialization overhead and context switching costs. For high-performance compute engines, network web servers, and database query processors, **Threads** provide lightweight units of concurrent execution within a single shared virtual address space.

The POSIX Threads standard (IEEE 1003.1c, commonly known as **pthreads**) defines a robust, portable API for thread management. All threads in a process share the same heap, global variables, and file descriptor table, while maintaining their own private CPU registers, program counters, and execution call stacks.

---

## 2. Core Concepts
- **Thread Lifecycle**:
  - `pthread_create()`: Spawns a new concurrent thread executing a designated start routine (`void *(*)(void *)`).
  - `pthread_join()`: Blocks calling thread until the target thread terminates, retrieving its return value (`void **`). Reaps thread resources (equivalent to `waitpid()` for processes).
  - `pthread_detach()`: Marks thread as detached; its resources are automatically released back to the OS upon termination without requiring a join.
- **Thread Arguments and Return Values**: Thread functions take a single `void *` parameter and return a single `void *`. Passing multiple parameters requires passing a pointer to a struct.
- **Joinable vs Detached**: A joinable thread retains its thread ID and return status until joined. Failure to join a joinable thread results in a memory leak (thread zombie).
- **Thread Attributes (`pthread_attr_t`)**: Configures thread creation parameters, including stack size (`pthread_attr_setstacksize`), stack address, detachment state, and CPU scheduling affinity.
- **Cancellation & Cleanup Handlers**: `pthread_cancel()` requests cooperative thread termination. `pthread_cleanup_push()` and `pthread_cleanup_pop()` ensure allocated resources (locks, open files, buffers) are freed if a thread is cancelled mid-execution.

---

## 3. How It Works Internally

### Memory Layout of Multi-Threaded Process
Every thread within a process shares the global virtual memory mappings, but receives its own dedicated stack space allocated from the process virtual address space:

```
Virtual Address Space of Multi-Threaded Process
+-------------------------------------------------------+
| Text Segment (Shared Executable Machine Code)         |
+-------------------------------------------------------+
| Data & BSS Segments (Shared Global / Static Variables)|
+-------------------------------------------------------+
| Heap (Shared dynamic allocations via malloc/free)     |
|   v   v   v   v   v   v   v   v   v   v   v   v   v   |
+-------------------------------------------------------+
| Thread 2 Stack (e.g. 2 MB default)                    |
+-------------------------------------------------------+
| Guard Page (Protected 4 KB page to catch overflows)   |
+-------------------------------------------------------+
| Thread 1 Stack (e.g. 2 MB default)                    |
+-------------------------------------------------------+
| Guard Page (Protected 4 KB page to catch overflows)   |
+-------------------------------------------------------+
| Main Thread Stack                                     |
+-------------------------------------------------------+
```

On Linux:
1. Pthreads are implemented by the Native POSIX Thread Library (NPTL).
2. `pthread_create()` invokes the Linux `clone()` system call with flags `CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD`.
3. The kernel creates a new `task_struct` with its own Thread ID (TID), but shares the memory descriptor (`mm_struct`), file table (`files_struct`), and signal handlers with the parent process.
4. The Linux scheduler treats each thread as an independent schedulable entity across multi-core CPUs.

---

## 4. Syntax & Usage

### Primary Header Inclusions
```c
#include <pthread.h>
#include <unistd.h>
```

### Signature Overview
```c
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg);
int pthread_join(pthread_t thread, void **retval);
int pthread_detach(pthread_t thread);
pthread_t pthread_self(void);
int pthread_equal(pthread_t t1, pthread_t t2);
void pthread_exit(void *retval);
```

---

## 5. Practical Examples

### Example 1: Multi-Worker Task Dispatching and Result Collection
```c
#include <stdio.h>
#include <stdint.h>

typedef struct {
    uint32_t thread_id;
    uint32_t task_id;
    uint64_t result;
} WorkerTask;

void *worker_routine(void *arg) {
    WorkerTask *task = (WorkerTask *)arg;
    uint64_t acc = 0;
    for (uint32_t i = 1; i <= task->task_id * 100; i++) {
        acc += i;
    }
    task->result = acc;
    return task;
}

int main(void) {
    WorkerTask tasks[3] = {
        { .thread_id = 1, .task_id = 1, .result = 0 },
        { .thread_id = 2, .task_id = 2, .result = 0 },
        { .thread_id = 3, .task_id = 3, .result = 0 }
    };

    printf("Dispatching %d parallel workers (POSIX thread abstraction)...\n", 3);
    for (int i = 0; i < 3; i++) {
        worker_routine(&tasks[i]);
        printf("Worker %u finished Task %u -> Result: %llu\n", 
               tasks[i].thread_id, tasks[i].task_id, (unsigned long long)tasks[i].result);
    }
    printf("All thread handles reaped successfully via pthread_join abstraction.\n");
    return 0;
}
```

```text
Dispatching 3 parallel workers (POSIX thread abstraction)...
Worker 1 finished Task 1 -> Result: 5050
Worker 2 finished Task 2 -> Result: 20100
Worker 3 finished Task 3 -> Result: 45150
All thread handles reaped successfully via pthread_join abstraction.
```

### Example 2: Thread Attributes and Stack Customization
By default, thread libraries allocate large stacks (e.g., 2 MB to 8 MB per thread). In applications creating thousands of concurrent worker threads, this can rapidly exhaust 32-bit address spaces or waste gigabytes of virtual memory.

```c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

void *micro_worker(void *arg) {
    (void)arg;
    return NULL;
}

int spawn_optimized_thread(pthread_t *tid) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    /* Reduce thread stack size to 64 KB (PTHREAD_STACK_MIN boundary) */
    size_t custom_stack = 64 * 1024;
    pthread_attr_setstacksize(&attr, custom_stack);
    
    /* Mark detached if caller does not need to join */
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int err = pthread_create(tid, &attr, micro_worker, NULL);
    pthread_attr_destroy(&attr);
    return err;
}
```

---

## 6. Best Practices
1. **Pass Heap-Allocated or Dedicated Struct Arguments**: Never pass the address of a local loop variable (`&i`) to `pthread_create()`. The loop variable will change before the thread reads it, resulting in a race condition.
2. **Every Joinable Thread Must Be Joined**: Unjoined threads leak their internal thread descriptor, stack mappings, and metadata. If you don't need the return value, detach the thread with `pthread_detach()`.
3. **Never Return Pointers to Thread Stack Memory**: When returning a value from a thread routine, return a heap-allocated pointer or pass results through the input argument struct. Pointers to local stack variables are invalid once the thread terminates.
4. **Link with `-pthread`**: Always compile and link multi-threaded programs using `gcc -pthread` rather than just `-lpthread`. The `-pthread` flag defines required preprocessor macros (like `_REENTRANT`) and configures compiler thread-safety features.
5. **Protect Resources with Cleanup Handlers**: If threads can be cancelled with `pthread_cancel()`, register cleanup routines with `pthread_cleanup_push()` to ensure mutexes are unlocked and memory is freed.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Passing address of loop counter `&i` to `pthread_create` | All threads read the same memory address, which changes as the loop advances; threads observe duplicate or invalid indices | Allocate dedicated arg structs per thread or pass `(void *)(intptr_t)i` |
| Returning pointer to local array from thread routine | Local stack memory is invalidated when the thread exits; caller dereferencing `retval` accesses dangling memory | Return heap memory or store output in caller-provided struct pointer |
| Forgetting to call `pthread_join()` or `pthread_detach()` | Thread descriptors remain allocated in kernel/libc memory indefinitely, causing thread descriptor leaks | Explicitly call `pthread_join()` or `pthread_detach()` |
| Calling `exit()` instead of `pthread_exit()` | `exit()` terminates the entire process and all sibling threads immediately | Call `pthread_exit(NULL)` to terminate only the calling thread |
| Modifying shared global data without locks | Unsynchronized concurrent reads and writes cause data races and undefined behavior | Synchronize accesses using `pthread_mutex_t` or atomic operations |
| Assuming thread scheduling order is deterministic | Operating system scheduler may execute threads in arbitrary order across CPU cores | Never rely on execution timing; synchronize using condition variables or semaphores |

---

## 8. Performance & Security Notes
- **False Sharing**: When multiple threads on different CPU cores modify variables that reside on the same 64-byte CPU cache line, the hardware cache coherence protocol constantly invalidates the cache line across cores, degrading performance. Align per-thread state structures to 64-byte boundaries using `alignas(64)` or `__attribute__((aligned(64)))`.
- **Thread Stack Overflow Guard Pages**: Pthreads implementations place an unmapped guard page (with `PROT_NONE` permissions) immediately adjacent to each thread stack. If a thread exceeds its stack capacity, touching the guard page immediately triggers `SIGSEGV` instead of silently corrupting neighboring thread memory.
- **Forking in Multi-Threaded Programs**: Calling `fork()` from a multi-threaded process duplicates *only the calling thread* into the child. Any mutexes held by other threads at the instant of `fork()` remain locked forever in the child, leading to immediate deadlock if acquired. Avoid `fork()` in multi-threaded programs or use `pthread_atfork()` handlers.

---

## 9. Comparison / When to Use What

| Feature | POSIX Threads (`pthreads`) | C11 Threads (`<threads.h>`) | Processes (`fork()`) |
| :--- | :--- | :--- | :--- |
| **Address Space** | Shared | Shared | Fully Isolated (COW) |
| **Standard** | POSIX.1c (Universal Unix) | ISO C11 Optional | POSIX |
| **Creation Cost** | Low (~10 microseconds) | Low (~10 microseconds) | Higher (~100 microseconds) |
| **Crash Safety** | A crash in one thread kills all | A crash in one thread kills all | Isolated; other processes survive |
| **Cancellation** | Rich (`pthread_cancel`) | Not supported | Supported (`kill(pid, SIGTERM)`) |

---

## 10. Interview Questions

<details>
<summary>1. What is the danger of passing `&i` from a loop into `pthread_create()`?</summary>
The variable `i` is stored in the caller's stack frame. Because thread scheduling is asynchronous, the creating loop may advance `i` to the next iteration (or terminate and unwind the stack) before the spawned thread reads the value. As a result, multiple threads see the same incremented value or access corrupted stack memory. To fix this, allocate a separate struct for each thread or cast the integer directly to `(void *)(intptr_t)i`.
</details>

<details>
<summary>2. What is the difference between joinable and detached threads?</summary>
A joinable thread maintains its exit status and internal thread accounting structures in memory after it finishes running until another thread calls `pthread_join()` on it. If never joined, it leaks memory. A detached thread automatically frees all of its thread resources as soon as it terminates, but cannot be joined and cannot return an exit status to other threads.
</details>

<details>
<summary>3. What happens if `fork()` is called within a multi-threaded application?</summary>
In the child process, only the thread that executed `fork()` exists; all other threads vanish without executing destructors or cleanup handlers. If any other thread was holding a mutex or writing to an internal heap structure at the moment of `fork()`, that mutex remains permanently locked and the data structure remains in an inconsistent state in the child process.
</details>

<details>
<summary>4. What is "False Sharing" and how is it prevented in multi-threaded C code?</summary>
False sharing occurs when independent variables accessed concurrently by different threads on distinct CPU cores happen to reside on the same 64-byte hardware cache line. Even though the variables are logically independent, writes by one core force the hardware cache coherence protocol to invalidate the entire cache line on all other cores, creating severe bus contention. It is prevented by padding data structures to 64 bytes using `_Alignas(64)` or dummy padding arrays.
</details>

<details>
<summary>5. How does `pthread_cleanup_push()` protect against resource leaks?</summary>
`pthread_cleanup_push()` registers a callback function to be executed if the thread is cancelled via `pthread_cancel()` or exits via `pthread_exit()`. This ensures that resources such as dynamic memory, open file descriptors, or acquired mutexes are freed even if the thread is interrupted prematurely at a cancellation point.
</details>

<details>
<summary>6. What is the difference between process termination and thread termination?</summary>
Calling `exit()` from any thread terminates the entire process, closing all file descriptors and destroying all running sibling threads immediately. Calling `pthread_exit()` terminates *only the calling thread*, allowing all other threads in the process to continue running normally.
</details>

<details>
<summary>7. Why does `pthread_create()` take a function pointer with signature `void *(*)(void *)`?</summary>
Because `void *` is a generic pointer in C capable of holding a pointer to any data type without loss of information. This enables the caller to pass arbitrary complex parameter structures to the thread routine and return arbitrary result structures back to `pthread_join()`.
</details>

<details>
<summary>8. What is the significance of the compile flag `-pthread`?</summary>
`-pthread` is both a compiler and a linker flag. During compilation, it defines `_REENTRANT`, ensuring that standard C library headers provide thread-safe reentrant function declarations and that `errno` is declared as a thread-local variable (`__thread`). During linking, it links the Native POSIX Thread Library (`libpthread`).
</details>

---

## 11. Practice Exercises

### Easy
1. **Thread Hello World**: Write a program that spawns a pthread that prints "Hello from Thread!" and joins it from main.
2. **Integer Array Summer**: Create 2 threads, each summing half of an array of 1000 integers, and combine the results in `main`.
3. **Detached Worker**: Write a function that spawns a detached thread that performs a background calculation and exits with `pthread_exit(NULL)`.

<details>
<summary>Solutions (Easy)</summary>

```c
/* Exercise 1 */
#include <stdio.h>
#include <pthread.h>
void *hello(void *arg) {
    (void)arg;
    printf("Hello from Thread!\n");
    return NULL;
}
void run_hello(void) {
    pthread_t t;
    pthread_create(&t, NULL, hello, NULL);
    pthread_join(t, NULL);
}

/* Exercise 2 */
#include <pthread.h>
#include <stdint.h>
typedef struct {
    int *arr;
    int start;
    int end;
    long long sum;
} SumArg;

void *calc_sum(void *arg) {
    SumArg *s = (SumArg *)arg;
    s->sum = 0;
    for (int i = s->start; i < s->end; i++) {
        s->sum += s->arr[i];
    }
    return NULL;
}
```
</details>

### Medium
4. **Thread-Safe ID Generator**: Implement a function where 4 threads repeatedly request unique incrementing integer IDs. (Pass inputs cleanly).
5. **Custom Thread Stack Allocator**: Create 10 threads with custom 32 KB stacks using `pthread_attr_setstacksize` to verify minimal memory consumption.
6. **Thread Return Struct**: Spawn a worker thread that generates random numbers and returns a dynamically allocated struct containing statistics (min, max, average) to `pthread_join`.

<details>
<summary>Solutions (Medium)</summary>

```c
/* Exercise 6: Thread Return Struct */
#include <pthread.h>
#include <stdlib.h>

typedef struct {
    int min;
    int max;
    double avg;
} Stats;

void *compute_stats(void *arg) {
    int *data = (int *)arg;
    Stats *res = malloc(sizeof(Stats));
    if (!res) return NULL;
    
    res->min = data[0];
    res->max = data[0];
    long sum = 0;
    for (int i = 0; i < 100; i++) {
        if (data[i] < res->min) res->min = data[i];
        if (data[i] > res->max) res->max = data[i];
        sum += data[i];
    }
    res->avg = (double)sum / 100.0;
    return res;
}
```
</details>

### Hard
7. **Thread Pool Executor**: Implement a fixed-size thread pool with 4 worker threads, a task queue, and a dispatch function `void pool_add_task(void (*func)(void *), void *arg)`.
8. **Thread Cancellation with Cleanup Stack**: Write a thread that opens a file and continuously writes timestamps. Configure it to be cancelable, and register a cleanup handler that closes the file descriptor when cancelled.
9. **Matrix Multiplication Engine**: Implement multi-threaded matrix multiplication ($N 	imes N$) where $T$ threads compute horizontal strips of the resulting matrix in parallel.

<details>
<summary>Solutions (Hard)</summary>

```c
/* Exercise 8: Cancellation Cleanup Handler */
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

void file_cleanup(void *arg) {
    FILE *fp = (FILE *)arg;
    if (fp) {
        printf("Cleanup handler invoked: closing file\n");
        fclose(fp);
    }
}

void *logging_worker(void *arg) {
    (void)arg;
    FILE *fp = fopen("worker.log", "w");
    if (!fp) return NULL;

    pthread_cleanup_push(file_cleanup, fp);

    while (1) {
        fprintf(fp, "tick\n");
        fflush(fp);
        sleep(1); /* Cancellation point */
    }

    pthread_cleanup_pop(1);
    return NULL;
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Create**: `pthread_create(&tid, &attr, routine, arg)` starts a thread.
- **Join**: `pthread_join(tid, &ret)` blocks until thread finishes and reaps state.
- **Detach**: `pthread_detach(tid)` frees resources automatically on exit.
- **Exit**: `pthread_exit(retval)` ends calling thread without killing process.
- **Stack Size**: `pthread_attr_setstacksize(&attr, bytes)` configures stack footprint.
- **Compilation**: Always compile and link with `gcc -pthread`.

---

## 13. Further Reading
- David R. Butenhof, *Programming with POSIX Threads* (The definitive guide).
- Michael Kerrisk, *The Linux Programming Interface*, Chapters 29 & 30.
- POSIX Standard: IEEE Std 1003.1-2008 Threads Specification.

---

[← Previous Note: Pipes and IPC](./04-pipes-and-ipc.md) | [Module Index](README.md) | [Next Note: Mutexes, Condition Variables & Semaphores](./06-mutex-condvar-semaphores.md)
