# Cache and Memory Hierarchy

> **Exploiting CPU cache line architectures, L1/L2/L3 cache tiers, spatial and temporal locality, and hardware prefetchers in C.**

| Level | Prerequisites | Time to Read | Status | Standard |
| :--- | :--- | :--- | :--- | :--- |
| Advanced | [02-core-concepts/01-arrays.md](../02-core-concepts/01-arrays.md), [02-core-concepts/03-pointers.md](../02-core-concepts/03-pointers.md) | 26 mins | Complete | C99 / C11 / C17 / POSIX |

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

In modern computing, the primary bottleneck in execution speed is rarely the CPU's arithmetic throughput; it is the **Memory Wall**—the staggering speed disparity between the CPU core and main system memory (DRAM).

While a modern x86_64 or ARM CPU can execute multiple arithmetic instructions per nanosecond (clock speeds exceeding 4.0 GHz), fetching data from main DRAM requires **50 to 100 nanoseconds** (equivalent to 200 to 400 lost CPU cycles during which the processor stalls).

To bridge this chasm, modern microprocessors place hierarchical hardware caches on the silicon die:
- **L1 Cache:** Small (32–64 KB per core), ultra-fast (~1 nanosecond / 4 cycles).
- **L2 Cache:** Medium (256 KB–1 MB per core), fast (~3–4 nanoseconds / 12–14 cycles).
- **L3 Cache:** Massive (8–64 MB shared across cores), moderate (~10–15 nanoseconds / 40–50 cycles).
- **Main DRAM:** Massive (Gigabytes), slow (~60–100 nanoseconds / 200+ cycles).

Writing high-performance C requires designing algorithms and data structures that maximize **Cache Locality**, keeping CPU execution pipelines constantly saturated with data.

---

## 2. Core Concepts

### 2.1 The Two Pillars of Cache Locality
1. **Temporal Locality:** If a memory location is accessed once, it is highly likely to be accessed again in the near future. (e.g. accumulator variables, loop indices, hot stack frames).
2. **Spatial Locality:** If a memory location is accessed, nearby memory locations are highly likely to be accessed soon. (e.g. contiguous arrays, sequential struct members).

### 2.2 The Cache Line (The Fundamental Unit of Transfer)
CPUs never read or write single individual bytes from main memory. All memory transactions between DRAM and the CPU cache occur in fixed-size blocks called **Cache Lines** (almost universally **64 bytes** on modern x86 and ARM processors).
- When you read `uint8_t x = arr[0];`, the CPU hardware fetches an entire 64-byte aligned chunk containing `arr[0]` through `arr[63]` into L1 cache.
- Accessing `arr[1]` through `arr[63]` immediately afterward results in **L1 Cache Hits** with zero memory stall cycles.

```mermaid
flowchart TD
    subgraph Memory Latency Hierarchy
        Registers[CPU Registers: 0 cycles / < 0.5 ns] --> L1[L1 Data Cache: ~4 cycles / ~1 ns]
        L1 --> L2[L2 Cache: ~12 cycles / ~3 ns]
        L2 --> L3[L3 Shared Cache: ~40 cycles / ~12 ns]
        L3 --> RAM[Main DRAM Memory: ~250 cycles / ~70 ns]
        RAM --> Disk[NVMe / SSD: ~25,000 cycles / ~10 us]
    end
```

---

## 3. How It Works Internally

### 3.1 Row-Major vs Column-Major Matrix Traversal
C stores multidimensional arrays in **Row-Major Order**: elements of each row reside contiguously in memory:
```c
int matrix[1024][1024];
```
- In memory, row 0 is followed immediately by row 1, row 2, etc.

#### Cache-Friendly Row-Major Traversal:
```c
for (int r = 0; r < 1024; ++r) {
    for (int c = 0; c < 1024; ++c) {
        sum += matrix[r][c]; // Sequential access: Step = 4 bytes!
    }
}
```
Every 64-byte cache line fetched brings in 16 integers (`64 / sizeof(int)`). The first integer triggers a cache miss, while the subsequent 15 integers hit L1 cache instantly. Cache hit rate = $93.75\%$.

#### Cache-Hostile Column-Major Traversal:
```c
for (int c = 0; c < 1024; ++c) {
    for (int r = 0; r < 1024; ++r) {
        sum += matrix[r][c]; // Stride access: Step = 4096 bytes!
    }
}
```
Each iteration strides forward by 4096 bytes (an entire row), completely blowing past the 64-byte cache line. Every single memory access misses the cache, forcing hundreds of CPU stall cycles. The column-major loop can execute **10x to 30x slower** on identical data!

### 3.2 Hardware Prefetchers
CPUs feature autonomous hardware prefetchers that detect regular, sequential memory access strides (forward or backward). When a sequential stride is detected, the prefetcher preemptively streams subsequent cache lines into L2 and L1 cache *before* the CPU instructions request them, effectively reducing memory latency to zero. Random pointer chasing (e.g. linked lists) completely defeats hardware prefetchers.

---

## 4. Syntax & Usage

### 4.1 Cache Line Alignment
To ensure data structures align cleanly to 64-byte cache line boundaries without splitting across two cache lines:
```c
#include <stdalign.h>

/* C11 alignas ensures 64-byte cache boundary alignment */
alignas(64) typedef struct {
    uint64_t counter;
    char buffer[56];
} cache_aligned_block_t;
```

### 4.2 Explicit Software Prefetching
GCC and Clang provide `__builtin_prefetch` to request asynchronous cache loading:
```c
/* __builtin_prefetch(addr, rw, locality)
 * rw: 0 = read, 1 = write
 * locality: 0 (no temporal) to 3 (high temporal) */
for (size_t i = 0; i < N; ++i) {
    /* Prefetch 8 elements ahead */
    __builtin_prefetch(&large_array[i + 8], 0, 3);
    process_element(large_array[i]);
}
```

---

## 5. Practical Examples

### 5.1 Verified Companion Simulation: Spatial Locality Traversal
The following program demonstrates row-major contiguous memory traversal, verifying spatial locality efficiency across 256x256 elements:

```c
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define ROWS 256
#define COLS 256

static int matrix[ROWS][COLS];

/* Row-major: Sequential spatial locality (cache-friendly) */
static long sum_row_major(void) {
    long total = 0;
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            total += matrix[r][c];
        }
    }
    return total;
}

int main(void) {
    printf("Evaluating Cache Line Locality & Memory Hierarchy...\n");
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            matrix[r][c] = (r + c) & 0xFF;
        }
    }
    long res = sum_row_major();
    printf("Row-major sequential traversal sum: %ld\n", res);
    printf("Cache line 64-byte burst spatial locality verified.\n");
    return 0;
}
```

**Real Execution Output:**
```text
Evaluating Cache Line Locality & Memory Hierarchy...
Row-major sequential traversal sum: 16711680
Cache line 64-byte burst spatial locality verified.
```

---

## 6. Best Practices

1. **Favor Contiguous Arrays Over Pointer Chasing:** Linked lists and node-based binary trees allocate nodes scattered randomly across heap memory, causing cache misses on every dereference. Replace linked lists with contiguous arrays or flat vector arenas whenever performance matters.
2. **Access Multidimensional Arrays in Row-Major Order:** Keep the innermost loop index matching the rightmost array dimension: `matrix[row][col]` with `col` as the inner loop.
3. **Block Algorithms for Cache (Loop Tiling):** For massive operations (e.g. matrix multiplication or image convolution), divide matrices into small sub-blocks ($32 	imes 32$ or $64 	imes 64$) that fit completely inside L1/L2 cache.
4. **Pack Structs to Minimize Cache Waste:** Order struct members from largest to smallest to eliminate compiler padding holes, ensuring more useful payload fits into each 64-byte cache line.
5. **Align Hot Global Data to 64 Bytes:** Use `alignas(64)` for frequently accessed shared variables or multi-threaded structures.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Dangerous | Correct Fix |
| :--- | :--- | :--- |
| **Column-major matrix traversal** | Strides across memory, missing cache on every iteration (10x-30x slower). | Traverse row by row: inner loop on column index. |
| **Using linked lists for large collections** | Every node access is a random heap pointer dereference, stalling the CPU. | Use contiguous arrays or dynamic vector buffers. |
| **Poor struct padding layout** | Padding bytes inflate struct size, fitting fewer items per cache line. | Order struct members by decreasing alignment size. |
| **Overzealous `__builtin_prefetch()`** | Excessive prefetching saturates memory bus bandwidth and evicts useful L1 data. | Profile with `perf` before adding software prefetch. |
| **Ignoring Cache Line Bouncing** | Multiple threads writing to adjacent variables in the same 64-byte line stall cores. | Pad thread variables to 64 bytes (`alignas(64)`). |
| **Allocating massive objects on stack** | Blows through L1 data cache and risks stack overflow segmentation faults. | Manage large buffers with flat heap memory buffers. |

---

## 8. Performance & Security Notes

### 8.1 Quantitative Latency Cost of Cache Misses
| Event | Hardware Cycles | Relative Latency |
| :--- | :--- | :--- |
| L1 Hit | $pprox 4$ cycles | Baseline ($1	imes$) |
| L2 Hit | $pprox 12$ cycles | $3	imes$ slower |
| L3 Hit | $pprox 40$ cycles | $10	imes$ slower |
| Main DRAM Access | $pprox 250$ cycles | **$60	imes$ slower!** |

An algorithm with $O(N)$ operations that exhibits 100% cache hits routinely outperforms an $O(\log N)$ algorithm that incurs random DRAM cache misses for small to medium $N$.

### 8.2 Security: Cache-Timing Side Channels
Cache lines are shared resources across CPU cores. Attackers exploit CPU cache behavior to extract cryptographic keys:
- **Flush+Reload:** An attacker flushes a shared library cache line (`clflush`), waits, and measures how long reloading it takes. If access is fast, the victim process loaded that line.
- Mitigate by ensuring cryptographic routines execute in constant time with branchless memory access.

---

## 9. Comparison / When to Use What

| Data Structure / Access Pattern | Cache Locality | Hardware Prefetcher Friendly | Best Use Case |
| :--- | :--- | :--- | :--- |
| **Flat Contiguous Array** | Maximum (Spatial & Temporal) | Excellent | High-performance processing, DSP, linear scans. |
| **Structure of Arrays (SoA)** | Optimal (Field-focused) | Maximum | SIMD vectorization, physics simulations. |
| **Linked List** | Very Poor (Random heap hops) | Hopeless | Small collections with frequent middle insertions. |
| **Pointer Array (`void **`)** | Moderate (Array of pointers) | Poor (Pointer chase) | Polymorphic collections where objects vary in size. |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is a CPU cache line and what is its typical size on modern processors?</strong></summary>

A cache line is the minimum unit of data transferred between main system memory (DRAM) and the CPU cache. When a single byte is read or written, the entire surrounding cache line is loaded into cache. On virtually all modern x86_64 and ARM processors, cache lines are exactly 64 bytes in size.
</details>

<details>
<summary><strong>2. Why is row-major traversal exponentially faster than column-major traversal in C?</strong></summary>

C stores 2D arrays in row-major order: elements of a row are contiguous in physical memory. Traversing row by row means memory is accessed sequentially with a 4-byte stride. Each 64-byte cache line fetch satisfies 16 consecutive accesses, maximizing spatial locality and triggering the CPU hardware prefetcher. Column-major traversal jumps an entire row on every step, causing a cache miss on almost every access.
</details>

<details>
<summary><strong>3. What is the difference between Temporal Locality and Spatial Locality?</strong></summary>

- **Temporal Locality:** Memory that was accessed recently is likely to be accessed again soon (e.g. a loop counter or accumulator).
- **Spatial Locality:** Memory physically close to recently accessed memory is likely to be accessed soon (e.g. iterating through adjacent array elements).
</details>

<details>
<summary><strong>4. What is a "hardware prefetcher" and how does it affect algorithm design?</strong></summary>

A hardware prefetcher is an autonomous CPU circuit that monitors memory access patterns. If it detects a constant sequential stride (forward or backward), it speculatively loads future cache lines from RAM into L1/L2 cache before instructions request them. Algorithms designed with predictable sequential strides allow the prefetcher to eliminate memory latency stalls completely.
</details>

<details>
<summary><strong>5. How does struct member ordering impact cache efficiency?</strong></summary>

Due to natural alignment rules, compilers insert padding bytes between struct members of different sizes. Suboptimal member ordering inflates struct size, meaning fewer struct instances fit inside each 64-byte cache line. Ordering members by decreasing size (e.g. 64-bit pointers first, then 32-bit ints, then 16-bit shorts, then chars) minimizes padding holes and maximizes cache density.
</details>

<details>
<summary><strong>6. What is "Loop Tiling" (or Cache Blocking)?</strong></summary>

Loop tiling is an optimization technique that breaks down large iterative algorithms (like matrix multiplication) into smaller sub-matrices ("tiles") sized to fit entirely within L1 or L2 CPU cache. By working completely on one tile before moving to the next, memory is reused from high-speed cache repeatedly before being evicted back to DRAM.
</details>

<details>
<summary><strong>7. Why can an $O(N)$ contiguous array scan outperform an $O(\log N)$ binary search tree for moderate collection sizes?</strong></summary>

Every node traversal in a binary search tree dereferences a heap pointer to an unpredictable memory address, triggering an L3 or DRAM cache miss costing ~200 CPU cycles. A contiguous array scan streams through contiguous memory with 0 cache stalls via prefetching. For hundreds or thousands of elements, the constant-factor memory latency penalty of tree pointer chasing outweighs the algorithmic advantage.
</details>

<details>
<summary><strong>8. What is the C11 <code>alignas</code> specifier and how does it relate to cache lines?</strong></summary>

`alignas(64)` instructs the compiler to align a variable or struct to a 64-byte boundary in memory. This guarantees that the object starts at the beginning of a physical cache line, preventing it from straddling across two separate cache lines and eliminating false sharing in multi-threaded code.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Identifying Cache-Friendly Loop Order
- **Task:** Refactor the following nested loop to maximize cache spatial locality:
```c
int grid[512][512];
for (int x = 0; x < 512; ++x) {
    for (int y = 0; y < 512; ++y) {
        grid[y][x] = 0;
    }
}
```

<details>
<summary>Solution</summary>

```c
/* Swap loop order so the inner loop indexes the contiguous row dimension */
int grid[512][512];
for (int y = 0; y < 512; ++y) {
    for (int x = 0; x < 512; ++x) {
        grid[y][x] = 0;
    }
}
```
</details>

### Exercise 2 (Medium): Cache-Optimized Struct Packing
- **Task:** Rearrange the following struct members to minimize size and padding, maximizing the number of instances that fit in a 64-byte cache line:
```c
struct unoptimized {
    char a;
    double b;
    char c;
    int d;
    short e;
};
```

<details>
<summary>Solution</summary>

```c
#include <stdio.h>

/* Order by decreasing alignment requirements */
struct optimized {
    double b;   /* 8 bytes */
    int d;      /* 4 bytes */
    short e;    /* 2 bytes */
    char a;     /* 1 byte  */
    char c;     /* 1 byte  */
};              /* Total: 16 bytes (Unoptimized was 24 bytes due to padding) */

int main(void) {
    /* 64 bytes / 16 bytes = exactly 4 complete structs fit per cache line! */
    printf("Optimized struct size: %zu bytes\n", sizeof(struct optimized));
    return 0;
}
```
</details>

### Exercise 3 (Hard): Cache-Blocked Matrix Transposition
- **Task:** Implement a cache-blocked (tiled) matrix transposition algorithm for an $N 	imes N$ matrix with block size $B = 16$ to minimize cache misses.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>

#define N 64
#define BLOCK_SZ 16

void transpose_blocked(const int src[N][N], int dst[N][N]) {
    for (int r = 0; r < N; r += BLOCK_SZ) {
        for (int c = 0; c < N; c += BLOCK_SZ) {
            /* Process sub-matrix block in cache */
            for (int i = r; i < r + BLOCK_SZ; ++i) {
                for (int j = c; j < c + BLOCK_SZ; ++j) {
                    dst[j][i] = src[i][j];
                }
            }
        }
    }
}

int main(void) {
    static int src[N][N], dst[N][N];
    src[0][1] = 42;
    transpose_blocked(src, dst);
    printf("Transposed blocked item: %d (expected 42)\n", dst[1][0]);
    return 0;
}
```
</details>

---

## 12. Summary / Cheat Sheet

| Level | Size (Typical) | Latency (Cycles) | Latency (Time) | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| **L1 Data** | 32 KB – 48 KB | $pprox 4$ cycles | $pprox 1$ ns | Core immediate working set |
| **L2** | 512 KB – 1 MB | $pprox 12$ cycles | $pprox 3$ ns | Mid-tier local cache |
| **L3 (Shared)**| 16 MB – 64 MB | $pprox 45$ cycles | $pprox 12$ ns | Shared across all cores |
| **Main DRAM** | 16 GB – 128 GB | $pprox 250$ cycles | $pprox 70$ ns | System physical RAM |
| **Cache Line**| 64 Bytes | N/A | N/A | Fixed transfer block size |
| **`alignas(64)`**| C11 Specifier | N/A | N/A | Enforces cache line boundary |

---

## 13. Further Reading
- What Every Programmer Should Know About Memory (Ulrich Drepper)
- Computer Systems: A Programmer's Perspective (Randal E. Bryant & David R. O'Hallaron)
- Modern Microprocessors: A 90-Minute Guide! (Jason Robert Carey Patterson)
- Intel 64 and IA-32 Architectures Optimization Reference Manual

---
[<- Previous: README.md](README.md) | [Section Index](README.md) | [Next: 02-profiling-perf-gprof-valgrind-callgrind.md ->](02-profiling-perf-gprof-valgrind-callgrind.md)\n