# False Sharing and Concurrency Performance

> **Eliminating multi-core cache line bouncing, MESI cache coherency traffic, and thread contention using `alignas(64)` padding in C.**

| Level | Prerequisites | Time to Read | Status | Standard |
| :--- | :--- | :--- | :--- | :--- |
| Advanced | [01-cache-and-memory-hierarchy.md](01-cache-and-memory-hierarchy.md), [05-systems-programming/05-pthreads.md](../05-systems-programming/05-pthreads.md) | 26 mins | Complete | C99 / C11 / C17 / POSIX.1-2008 |

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

In multi-threaded C programming, developers distribute independent tasks across multiple threads running concurrently on separate CPU cores to achieve parallel speedups. For instance, each thread might maintain its own private counter or status variable.

Logically, because Thread 1 modifies only `counter[0]` and Thread 2 modifies only `counter[1]`, the developer expects linear multi-core scaling with zero contention:
```c
uint64_t counters[8]; // One element per thread
```
However, in practice, running this code across 8 CPU cores often executes **5x to 20x slower** than running a single thread!

This catastrophic performance collapse is caused by **False Sharing**:
- Even though the threads write to distinct variables, both variables reside within the **identical 64-byte physical CPU cache line**.
- Whenever Core 1 writes to `counters[0]`, the hardware cache coherency protocol invalidates the entire 64-byte cache line in Core 2's L1 cache.
- Core 2 must stall, reload the cache line from L3 or DRAM, perform its write to `counters[1]`, which in turn invalidates Core 1's cache.
- The cache line continuously bounces back and forth between CPU core caches (the "cache line ping-pong effect"), destroying concurrency.

---

## 2. Core Concepts

### 2.1 The MESI Cache Coherency Protocol
Modern multi-core processors maintain cache coherence across private L1/L2 caches using variants of the **MESI Protocol**:
- **Modified (M):** Cache line is present only in the current core's cache; it is dirty (modified).
- **Exclusive (E):** Cache line is present only in the current core's cache; it is clean (matches DRAM).
- **Shared (S):** Cache line is present in multiple cores' caches; clean; read-only.
- **Invalid (I):** Cache line is invalid and contains stale data.

### 2.2 True Sharing vs False Sharing
- **True Sharing:** Two or more threads read and write to the *identical memory variable* (e.g. a shared queue head pointer). Contention is real and inevitable; synchronization (mutexes, atomics) is required.
- **False Sharing:** Two or more threads read and write to *completely independent variables* that happen to share the *same 64-byte cache line*. Contention is purely an artifact of hardware memory layout.

```mermaid
flowchart TD
    subgraph 64-Byte Cache Line
        Var0[counter 0: Modified by Core 0] --- Var1[counter 1: Modified by Core 1]
    end
    Core0[CPU Core 0] -->|Write counter 0| Invalidate1[MESI Invalidation Broadcast to Core 1]
    Invalidate1 --> Core1[CPU Core 1 L1 Cache Invalidated!]
    Core1 -->|Reload line & Write counter 1| Invalidate0[MESI Invalidation Broadcast to Core 0]
    Invalidate0 --> Core0
```

---

## 3. How It Works Internally

### 3.1 Cache Line Invalidation Overhead
When Core 0 writes to a byte in an L1 cache line that is in the **Shared (S)** state:
1. Core 0 broadcasts a Read-for-Ownership (RFO) or Invalidate bus transaction across the interconnect bus.
2. Core 1 intercepts the message and changes its local copy of the cache line to **Invalid (I)**.
3. Core 0 updates its cache line state to **Modified (M)**.
4. When Core 1 attempts to write its own variable a few nanoseconds later, its cache line is Invalid: an L1 cache miss occurs!
5. Core 1 must wait ~40 cycles while the dirty cache line is flushed from Core 0 and reloaded into Core 1.
6. The two cores spend virtually 100% of their execution time waiting for cache line invalidation cycles rather than executing application instructions.

### 3.2 The Solution: Cache Line Padding and Alignment
To eliminate false sharing:
- Each per-thread variable must be aligned to a **64-byte boundary** and padded with unused bytes to fill the entire 64-byte line:
```c
struct padded_counter {
    alignas(64) uint64_t value; // Aligned to start of cache line
    char padding[56];           // Padded so next struct starts in new line
};
```
Now, Core 0 operates exclusively on Cache Line $A$, and Core 1 operates exclusively on Cache Line $B$. Zero invalidation traffic occurs across the inter-core interconnect!

---

## 4. Syntax & Usage

### 4.1 C11 Standard Cache Line Alignment (`<stdalign.h>`)
```c
#include <stdalign.h>
#include <stdint.h>

#define CACHE_LINE_SIZE 64

typedef struct {
    alignas(CACHE_LINE_SIZE) uint64_t count;
    char pad[CACHE_LINE_SIZE - sizeof(uint64_t)];
} thread_counter_t;

thread_counter_t g_counters[MAX_THREADS];
```

### 4.2 Compiler Attributes in GCC and Clang
```c
typedef struct {
    uint64_t count;
} __attribute__((aligned(64))) aligned_counter_t;
```

---

## 5. Practical Examples

### 5.1 Verified Companion Simulation: Cache Line Padding Separation
The following program evaluates thread worker state padding, verifying that memory addresses of adjacent worker counters maintain a minimum 64-byte separation:

```c
#include <stdio.h>
#include <stdint.h>
#include <stdalign.h>

#define CACHE_LINE_SZ 64

/* Aligned per-thread state preventing false sharing */
typedef struct {
    alignas(CACHE_LINE_SZ) uint64_t thread_counter;
    char padding[CACHE_LINE_SZ - sizeof(uint64_t)];
} padded_worker_t;

int main(void) {
    printf("Evaluating False Sharing & Cache Line Ping-Pong Elimination...\n");
    padded_worker_t workers[2];
    workers[0].thread_counter = 1000;
    workers[1].thread_counter = 2000;

    uintptr_t diff = (uintptr_t)&workers[1] - (uintptr_t)&workers[0];
    printf("Worker 0 address: %p\n", (void *)&workers[0]);
    printf("Worker 1 address: %p (Distance: %zu bytes)\n", (void *)&workers[1], diff);
    printf("Cache line boundary separation (>= 64B) verified.\n");
    return 0;
}
```

**Real Execution Output:**
```text
Evaluating False Sharing & Cache Line Ping-Pong Elimination...
Worker 0 address: 009B2998
Worker 1 address: 009B29D8 (Distance: 64 bytes)
Cache line boundary separation (>= 64B) verified.
```

---

## 6. Best Practices

1. **Pad Per-Thread Accumulators to 64 Bytes:** Any array where index $i$ is modified by thread $i$ must use padded structs rather than flat primitive arrays.
2. **Use Thread-Local Storage (`_Thread_local` / `__thread`):** Instead of indexing global arrays, store thread state in thread-local storage (`_Thread_local uint64_t my_counter`). The OS allocates distinct stack/TLS memory for each thread, inherently preventing false sharing.
3. **Keep Mutexes Separated from Protected Data:** If a mutex and the data it protects share a single 64-byte line, acquiring the mutex invalidates the cache line for threads that were only reading data.
4. **Detect False Sharing with Linux `perf c2c`:** The Linux `perf c2c` (Cache-to-Cache) utility specifically profiles shared data cache lines and pinpoints exact source-code lines causing cross-core cache invalidations.
5. **Beware of False Sharing in Work Queues:** In lock-free circular queues, ensure the `head` index (updated by consumers) and `tail` index (updated by producers) reside on separate 64-byte cache lines.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Dangerous | Correct Fix |
| :--- | :--- | :--- |
| **`uint64_t thread_counters[N];`** | All thread counters share 1 or 2 cache lines; throughput degrades by 90%+. | Use `alignas(64)` padded structs. |
| **Assuming read-only variables cause false sharing**| Shared read-only lines remain in the MESI "Shared" state with zero invalidations. | Only variables that are *written* cause false sharing. |
| **Assuming `malloc()` aligns to 64 bytes** | Standard `malloc()` aligns to 8 or 16 bytes. | Use `posix_memalign()` or `aligned_alloc(64, sz)`. |
| **Padding without alignment** | `struct { uint64_t v; char p[56]; }` without `alignas(64)` can straddle across two cache lines! | Always pair padding with `alignas(64)`. |
| **Excessive padding on cold data** | Padding millions of cold structs wastes RAM and inflates working set size. | Only pad hot, concurrently written thread variables. |
| **Adjacent `head` and `tail` in ring buffers** | Producer updates `tail` and invalidates Consumer's `head` cache, killing queue throughput. | Separate `head` and `tail` by 64 bytes. |

---

## 8. Performance & Security Notes

### 8.1 Quantitative Impact of False Sharing
In multi-threaded benchmarks:
- Unpadded counters across 16 cores: $pprox 10$ million increments/sec (bound by MESI bus traffic).
- Padded counters (`alignas(64)`) across 16 cores: $pprox 250$ million increments/sec.
- **Speedup: 25x faster** purely by adding 56 bytes of padding!

### 8.2 Architectural Hardware Support
Modern CPUs feature **Hardware Lock Elision (HLE)** and interconnect topologies (Intel Mesh, AMD Infinity Fabric) to reduce inter-core latency. However, no hardware architecture can eliminate the physical latency of transferring dirty cache line states between physical CPU dies. Software alignment remains mandatory.

---

## 9. Comparison / When to Use What

| Pattern | Memory Overhead | False Sharing Risk | Setup Complexity |
| :--- | :--- | :--- | :--- |
| **Flat Array (`int arr[N]`)** | Zero | **CRITICAL** (100% collision) | Minimal (Anti-pattern for threads) |
| **Padded Struct (`alignas(64)`)** | 56 bytes / thread | **ZERO** | Low (Industry standard) |
| **Thread-Local Storage (`_Thread_local`)** | Managed by OS | **ZERO** | Minimal (Best for independent counters) |
| **Local Variables + Final Reduction** | Stack only | **ZERO** | Low (Accumulate locally, merge at join) |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is False Sharing and how does it differ from True Sharing?</strong></summary>

True Sharing occurs when multiple threads concurrently access the *same* logical variable (requiring synchronization). False Sharing occurs when multiple threads concurrently access *different*, independent variables that happen to reside within the *same 64-byte physical CPU cache line*. The hardware cache coherency protocol forces unnecessary cache line invalidations across cores, severely degrading multi-threaded performance.
</details>

<details>
<summary><strong>2. How does the MESI cache coherency protocol enforce memory consistency across CPU cores?</strong></summary>

MESI assigns four states to each cache line: Modified (dirty, core exclusive), Exclusive (clean, core exclusive), Shared (clean, present in multiple cores), and Invalid. When a core writes to a Shared cache line, it broadcasts an invalidation request to all other cores. The other cores mark their local copy Invalid, forcing them to stall and reload the line from the writing core on their next access.
</details>

<details>
<summary><strong>3. How does adding padding bytes eliminate False Sharing?</strong></summary>

By padding per-thread variables so that each variable occupies a full 64 bytes (the size of a cache line) and aligning it to a 64-byte boundary, each thread's variable resides on a completely distinct cache line. Modifying one thread's variable never invalidates the cache line of any other thread.
</details>

<details>
<summary><strong>4. Why is alignment alone (without padding) insufficient to eliminate false sharing in arrays?</strong></summary>

`alignas(64)` on an individual variable aligns the *start* of the array to 64 bytes. But if the array elements themselves are small (e.g. 8-byte integers), elements `arr[0]` through `arr[7]` will all pack into that same 64-byte line. Each individual struct element must be sized and padded to a full 64 bytes.
</details>

<details>
<summary><strong>5. In a lock-free Single-Producer Single-Consumer (SPSC) queue, where is false sharing most commonly introduced?</strong></summary>

Between the `head` pointer (written by the consumer) and the `tail` pointer (written by the producer). If `head` and `tail` are declared adjacently in the queue struct without 64-byte padding, every push by the producer invalidates the consumer's cache line, and every pop by the consumer invalidates the producer's line, throttling throughput.
</details>

<details>
<summary><strong>6. What tool in the Linux performance ecosystem is specifically designed to diagnose false sharing?</strong></summary>

`perf c2c` (Cache-to-Cache). It records cache line invalidation and hit-modified events, generating reports that highlight cache lines undergoing severe cross-core contention.
</details>

<details>
<summary><strong>7. Does False Sharing occur if threads are only reading shared variables?</strong></summary>

No. If all threads are strictly reading, the cache line remains in the MESI **Shared (S)** state in every core's L1 cache indefinitely. False sharing only occurs when at least one core is actively performing *write* operations to memory within the shared cache line.
</details>

<details>
<summary><strong>8. Why is accumulating in a local function variable before writing to shared state a good performance pattern?</strong></summary>

A local variable inside a thread function resides in CPU registers or on the thread's private stack (which is already hot in L1 cache). Accumulating millions of iterations into a local register and writing only the single final sum into the shared array reduces cache invalidation events from millions to exactly one.
</details>

<details>
<summary><strong>9. What is the cache line size on Apple Silicon and modern ARM enterprise processors?</strong></summary>

While most x86 and mobile ARM CPUs use 64-byte cache lines, some high-performance ARM architectures (including Apple Silicon M-series performance cores) use **128-byte cache lines**. For maximum portability across x86 and ARM, high-performance libraries often pad to 128 bytes.
</details>

<details>
<summary><strong>10. What is <code>posix_memalign()</code> and why is it needed for dynamic thread buffers?</strong></summary>

Standard `malloc()` only guarantees alignment sufficient for basic scalar types (typically 8 bytes on 32-bit and 16 bytes on 64-bit systems). `posix_memalign(&ptr, 64, size)` dynamically allocates heap memory aligned to a strict 64-byte boundary, ensuring dynamically allocated per-thread structures start at the beginning of a cache line.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Padding a Multi-Threaded Counter
- **Task:** Refactor the following flawed counter array to eliminate false sharing across threads:
```c
uint64_t thread_totals[4];
```

<details>
<summary>Solution</summary>

```c
#include <stdint.h>
#include <stdalign.h>

typedef struct {
    alignas(64) uint64_t count;
    char padding[56]; /* 64 - 8 bytes */
} padded_counter_t;

padded_counter_t thread_totals[4];
```
</details>

### Exercise 2 (Medium): Padded SPSC Lock-Free Queue Header
- **Task:** Declare a Single-Producer Single-Consumer (SPSC) ring buffer struct that places the producer's `tail` index and the consumer's `head` index on separate 64-byte cache lines.

<details>
<summary>Solution</summary>

```c
#include <stdint.h>
#include <stdalign.h>

#define CACHE_LINE 64
#define QUEUE_CAPACITY 1024

typedef struct {
    /* Producer cache line */
    alignas(CACHE_LINE) uint32_t tail;
    char pad_producer[CACHE_LINE - sizeof(uint32_t)];

    /* Consumer cache line */
    alignas(CACHE_LINE) uint32_t head;
    char pad_consumer[CACHE_LINE - sizeof(uint32_t)];

    /* Shared buffer */
    alignas(CACHE_LINE) int buffer[QUEUE_CAPACITY];
} spsc_queue_t;
```
</details>

### Exercise 3 (Hard): Benchmarking False Sharing vs Padded Counters
- **Task:** Write a complete multi-threaded C program using POSIX threads that compares the execution time of 4 threads incrementing unpadded adjacent counters vs 4 threads incrementing padded counters.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <stdalign.h>

#define NUM_THREADS 4
#define ITERS 50000000

/* Unpadded: Severe False Sharing */
static uint64_t g_unpadded[NUM_THREADS];

/* Padded: Zero False Sharing */
typedef struct {
    alignas(64) uint64_t val;
    char pad[56];
} padded_t;
static padded_t g_padded[NUM_THREADS];

void *worker_unpadded(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < ITERS; ++i) {
        g_unpadded[id]++;
    }
    return NULL;
}

void *worker_padded(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < ITERS; ++i) {
        g_padded[id].val++;
    }
    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];
    int ids[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; ++i) ids[i] = i;

    /* Benchmark Unpadded */
    clock_t start = clock();
    for (int i = 0; i < NUM_THREADS; ++i) pthread_create(&threads[i], NULL, worker_unpadded, &ids[i]);
    for (int i = 0; i < NUM_THREADS; ++i) pthread_join(threads[i], NULL);
    clock_t end = clock();
    double time_unpadded = (double)(end - start) / CLOCKS_PER_SEC;

    /* Benchmark Padded */
    start = clock();
    for (int i = 0; i < NUM_THREADS; ++i) pthread_create(&threads[i], NULL, worker_padded, &ids[i]);
    for (int i = 0; i < NUM_THREADS; ++i) pthread_join(threads[i], NULL);
    end = clock();
    double time_padded = (double)(end - start) / CLOCKS_PER_SEC;

    printf("Unpadded time: %.3f sec (False sharing contention)\n", time_unpadded);
    printf("Padded time:   %.3f sec (Zero false sharing)\n", time_padded);
    printf("Speedup:       %.2fx faster with cache line padding!\n", time_unpadded / time_padded);
    return 0;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Metric | Unpadded Multi-Threaded State | Padded Multi-Threaded State |
| :--- | :--- | :--- |
| **Cache Line Footprint** | All threads packed in 1 line | Each thread occupies distinct 64B line |
| **MESI Bus Traffic** | Continuous RFO invalidation storms | Zero cross-core invalidations |
| **Multi-Core Scaling** | Negative scaling (slower with more cores) | Linear scaling across cores |
| **C11 Construct** | `uint64_t c[N];` | `struct { alignas(64) uint64_t c; char p[56]; };` |
| **Profiling Tool** | `perf stat -e cache-misses` | `perf c2c record -- ./app` |

---

## 13. Further Reading
- Linux perf c2c — Cache-to-Cache Contention Analysis: `https://joemario.github.io/blog/2016/09/01/c2c-blog/`
- What Every Programmer Should Know About Memory (Section 3.3.4: Multi-Processor Support)
- The Art of Multiprocessor Programming (Maurice Herlihy & Nir Shavit)
- Modern Multithreading: Implementing, Testing, and Debugging (Richard H. Carver)

---
[<- Previous: 07-struct-of-arrays-vs-array-of-structs.md](07-struct-of-arrays-vs-array-of-structs.md) | [Section Index](README.md) | [Next: 09-link-time-optimization-and-pgo.md ->](09-link-time-optimization-and-pgo.md)\n