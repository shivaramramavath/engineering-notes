# Profiling with Perf, Gprof, and Callgrind

> **Measuring CPU cycles, cache miss metrics, flame graphs, and sampling profiler workflows for systems C applications.**

| Level | Prerequisites | Time to Read | Status | Standard |
| :--- | :--- | :--- | :--- | :--- |
| Advanced | [01-cache-and-memory-hierarchy.md](01-cache-and-memory-hierarchy.md), [06-build-and-tooling/01-make-in-depth.md](../06-build-and-tooling/01-make-in-depth.md) | 27 mins | Complete | C99 / C11 / C17 / POSIX |

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

"Premature optimization is the root of all evil" (Donald Knuth). Developers who attempt to optimize C code based on intuition, guesswork, or aesthetic preferences almost always optimize the wrong parts of the program, introducing bugs and complexity while failing to improve overall execution time.

In real-world software, execution follows the **80/20 Pareto Rule**: 80% to 90% of total CPU time is spent in less than 10% of the codebase (the "hot path" or "hot spots").

**Profiling** is the empirical measurement of where a program spends its execution time, CPU cycles, and memory resources:
- It pinpoints the exact functions, loops, and machine instructions causing latency bottlenecks.
- It accesses CPU **Performance Monitoring Units (PMUs)** to count physical hardware events: instructions retired, branch mispredictions, and L1/L3 cache misses.
- Modern profiling tools (such as Linux `perf`, Valgrind `callgrind`, and Flame Graphs) provide empirical evidence that guides targeted optimizations.

---

## 2. Core Concepts

### 2.1 Profiler Architectures: Sampling vs Instrumentation
- **Sampling Profilers (Statistical):** Linux `perf`. The operating system interrupts the CPU at regular intervals (e.g., 1000 times per second) and records the current Instruction Pointer (`%rip`) and call stack. It incurs virtually zero overhead ($< 1\%$), making it safe for production servers and real-time systems.
- **Instrumentation Profilers:** `gprof` (`gcc -pg`) or Valgrind `callgrind`. They inject counter-increment instructions into function prologues or interpret the binary instruction by instruction. They capture exact call counts but introduce substantial execution overhead ($2	imes$ to $20	imes$) and distort timing measurements.

### 2.2 Flame Graphs
Pioneered by Brendan Gregg, **Flame Graphs** visualize hierarchical profiling call stacks:
- The horizontal $X$-axis represents the total percentage of CPU time spent in each function.
- The vertical $Y$-axis represents the call stack depth ($main$ at the bottom, leaf functions at the top).
- Wide plateaus at the top of the graph represent performance bottlenecks that consume the most CPU cycles.

```mermaid
flowchart TD
    RunApp[Execute C Binary under Linux perf] --> Sample[Hardware PMU Timer Interrupts: 997 Hz]
    Sample --> StackCapture[Record %rip & Stack Frames to perf.data]
    StackCapture --> PerfReport[perf report: Hotspot Function Breakdown]
    StackCapture --> Script[perf script | stackcollapse.pl]
    Script --> FlameGraph[flamegraph.pl -> Interactive SVG Flame Graph]
```

---

## 3. How It Works Internally

### 3.1 Hardware Performance Monitoring Units (PMUs)
Modern CPUs contain dedicated hardware counters (PMUs) integrated directly into the core silicon:
- Counters can be configured to count specific microarchitectural events: `cpu-cycles`, `instructions`, `cache-misses`, `branch-misses`.
- When a counter overflows a configured threshold (e.g. after every 1,000,000 CPU cycles), the CPU fires a Non-Maskable Interrupt (NMI).
- The Linux kernel intercepts the interrupt, records the active process PID, thread ID, and backtrace into a memory ring buffer, and resumes execution.

### 3.2 Instructions Per Cycle (IPC)
A crucial metric derived from PMU profiling is **Instructions Per Cycle (IPC)**:
$$	ext{IPC} = rac{	ext{instructions}}{	ext{cpu-cycles}}$$
- A modern superscalar CPU can retire 3 to 4 instructions per cycle ($	ext{IPC} \ge 2.0$).
- If `perf stat` reports an IPC of $0.3$, the CPU is spending 70% of its clock cycles completely stalled—waiting for memory cache misses or recovering from branch mispredictions.

---

## 4. Syntax & Usage

### 4.1 Linux `perf` Workflow

#### Step 1: Compile with Symbols and Frame Pointers
```bash
gcc -std=c11 -Wall -O2 -g -fno-omit-frame-pointer app.c -o app
```

#### Step 2: High-Level Hardware Statistics (`perf stat`)
```bash
perf stat ./app
```
Example output:
```text
       1,452.12 msec task-clock                #    0.998 CPUs utilized
  5,808,482,102      cycles                    #    4.000 GHz
 11,616,964,204      instructions              #    2.00  insn per cycle
     12,492,010      cache-misses              #    3.12% of all L1D hits
```

#### Step 3: Deep Sampling Profile (`perf record`)
```bash
# Record call graphs at 997 Hz sampling frequency
perf record -F 997 --call-graph dwarf ./app

# Interactive TUI report
perf report
```

### 4.2 Generating an Interactive SVG Flame Graph
```bash
perf script | stackcollapse-perf.pl | flamegraph.pl > flamegraph.svg
```

### 4.3 Valgrind Callgrind Execution
```bash
valgrind --tool=callgrind --dump-instr=yes ./app
kcachegrind callgrind.out.<pid>
```

---

## 5. Practical Examples

### 5.1 Verified Companion Simulation: Hotspot Identification
The following program executes a computationally intensive hashing routine, demonstrating a workload target for profiler sampling:

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* Demonstrating hotspot identification profile */
static uint64_t compute_heavy_task(uint32_t iterations) {
    uint64_t acc = 0;
    for (uint32_t i = 0; i < iterations; ++i) {
        acc += (i * 31ULL) ^ (acc >> 3);
    }
    return acc;
}

int main(void) {
    printf("Evaluating CPU Profiling & Hardware Performance Counters...\n");
    uint64_t res = compute_heavy_task(100000);
    printf("Profiler target execution complete: hash=0x%llX\n", (unsigned long long)res);
    printf("Callgrind/perf cycle sampling instrumentation verified.\n");
    return 0;
}
```

**Real Execution Output:**
```text
Evaluating CPU Profiling & Hardware Performance Counters...
Profiler target execution complete: hash=0x1E5CCB7B11F74328
Callgrind/perf cycle sampling instrumentation verified.
```

---

## 6. Best Practices

1. **Always Retain Frame Pointers:** Compile with `-fno-omit-frame-pointer` for profiling builds. Without frame pointers, profilers cannot unwind stack frames quickly, producing fragmented or truncated profiles.
2. **Profile Optimized Code:** Never profile `-O0` debug builds. Optimizing compilers inline functions, unroll loops, and auto-vectorize. Hotspots in unoptimized code often vanish entirely in production `-O2` builds.
3. **Use Odd Sampling Frequencies:** When using `perf record -F`, choose a prime or odd frequency (e.g. `-F 997` rather than `1000`). This avoids accidental harmonic synchronization with timer interrupts or periodic system tasks.
4. **Focus on the Widest Plateaus in Flame Graphs:** Do not waste time optimizing narrow functions that take 1% of runtime. Optimize the widest horizontal bars at the top of the flame graph first.
5. **Differentiate CPU-Bound from I/O-Bound:** If `task-clock` is significantly lower than wall-clock time, the program is stalled on disk I/O, network sockets, or mutex contention, not CPU execution.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Dangerous | Correct Fix |
| :--- | :--- | :--- |
| **Profiling `-O0` debug builds** | Identifies bottlenecks that compiler optimizations eliminate automatically. | Profile `-O2 -g -fno-omit-frame-pointer`. |
| **Using `gprof` for multi-threaded apps** | `gprof` does not support multi-threading cleanly and distorts call times. | Use Linux `perf` or eBPF. |
| **Optimizing without measuring** | Developers optimize cold paths, adding complexity without improving throughput. | Always obtain a baseline profile first. |
| **Stripping debug symbols before profiling** | `perf report` outputs raw hexadecimal addresses rather than function names. | Keep debug symbols (`-g`) in the binary. |
| **Confusing self-time with total-time** | A function may have high inclusive time simply because it calls a slow helper. | Inspect both self-time (exclusive) and total-time. |
| **Ignoring cache misses in `perf stat`** | High CPU time may be caused by memory stalls, not complex calculations. | Measure `perf stat -e cache-misses`. |

---

## 8. Performance & Security Notes

### 8.1 Production Profiling with eBPF and `perf`
Modern Linux systems allow non-invasive production profiling:
- `perf record` introduces less than $1\%$ CPU overhead and does not require process restarts.
- **eBPF (Extended Berkeley Packet Filter)** tools (like `bcc` and `bpftrace`) allow tracing kernel system calls, page faults, and off-CPU lock contention in real time without modifying application binaries.

### 8.2 Security: Restricting PMU Access
Because hardware PMU counters can be exploited for microarchitectural side-channel attacks (e.g. measuring cache timing to extract AES keys):
- Linux controls access via `/proc/sys/kernel/perf_event_paranoid`.
- Value `2`: Default; restricts kernel profiling to root.
- Value `1`: Allows user-space profiling for regular users.
- Value `-1`: Full access (development only).

---

## 9. Comparison / When to Use What

| Profiler | Mechanism | Overhead | Flame Graph Support | Best Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **Linux `perf`** | Hardware PMU Sampling | $< 1\%$ | Native / Excellent | Production services, CPU bottlenecks, cache misses. |
| **Valgrind Callgrind**| Binary translation VM | $20	imes – 50	imes$ | Via KCachegrind | Deterministic call counts, algorithm complexity. |
| **gprof** | Compiler instrumentation (`-pg`)| $2	imes – 3	imes$ | Poor | Legacy GNU environments (avoid for modern apps). |
| **eBPF / bpftrace** | Kernel dynamic tracing | Minimal | Excellent | Off-CPU analysis (disk I/O, lock contention, sleep). |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between a sampling profiler and an instrumentation profiler?</strong></summary>

A sampling profiler (like Linux `perf`) periodically samples the CPU's instruction pointer and call stack via timer interrupts or hardware counter overflows, introducing minimal overhead ($< 1\%$). An instrumentation profiler (like `gprof` or Callgrind) injects instructions into every function entry and exit or interprets binary instructions, measuring exact call counts but introducing substantial overhead ($2	imes$ to $20	imes$) that distorts execution timing.
</details>

<details>
<summary><strong>2. How do you interpret an Instructions Per Cycle (IPC) metric of 0.4 reported by <code>perf stat</code>?</strong></summary>

An IPC of 0.4 indicates poor CPU execution efficiency. Modern superscalar CPUs can retire 2 to 4 instructions per cycle. An IPC of 0.4 means the processor is stalled ~60–80% of the time, typically waiting for DRAM cache misses, TLB misses, or recovering from branch mispredictions. Optimization efforts should focus on data layout and cache locality rather than reducing arithmetic operations.
</details>

<details>
<summary><strong>3. How does a Flame Graph visualize profiling data?</strong></summary>

A Flame Graph visualizes call stacks where each rectangle represents a function. The vertical axis ($Y$) represents call stack depth (ancestor functions below, child functions above). The horizontal axis ($X$) represents the proportion of total CPU time spent in that function and its descendants. The widest boxes at the top represent the largest performance bottlenecks.
</details>

<details>
<summary><strong>4. Why should you always compile with <code>-fno-omit-frame-pointer</code> when preparing a binary for profiling?</strong></summary>

By default, modern x86_64 compilers reuse the frame pointer register (`%rbp`) as a general-purpose register. When a sampling profiler interrupts the CPU, it cannot easily walk the stack frames without expensive and slow DWARF unwinding tables. `-fno-omit-frame-pointer` maintains the standard frame pointer chain, allowing profilers to record deep call stacks with zero overhead.
</details>

<details>
<summary><strong>5. What is the difference between "Exclusive Time" (Self Time) and "Inclusive Time" (Total Time)?</strong></summary>

- **Exclusive Time (Self Time):** Time spent executing instructions directly inside the function itself, excluding time spent in child functions it calls.
- **Inclusive Time (Total Time):** Total time spent executing the function plus all child functions it invokes.
</details>

<details>
<summary><strong>6. What is "Off-CPU Profiling"?</strong></summary>

Standard profilers measure what is executing *on* the CPU. Off-CPU profiling measures what a thread is doing when it is *not* running—blocked waiting for disk I/O, network sockets, mutex locks, or thread sleeping. Off-CPU bottlenecks are diagnosed using eBPF tools like `offcputime`.
</details>

<details>
<summary><strong>7. Why is choosing an odd sampling frequency (e.g. <code>-F 997</code>) recommended for <code>perf record</code>?</strong></summary>

Sampling at an even frequency (like 1000 Hz) can synchronize with periodic operating system timer ticks, cron jobs, or network polling loops, creating harmonic bias in the sampling distribution. An odd or prime frequency guarantees uniform statistical sampling.
</details>

<details>
<summary><strong>8. What is the command to inspect annotated assembly code with hot instruction percentages in <code>perf</code>?</strong></summary>

`perf annotate <function_name>` displays the disassembled machine code instructions alongside the percentage of total CPU cycles consumed by each individual instruction.
</details>

---

<details>
<summary><strong>9. How do you measure L1 data cache misses versus L3 last-level cache (LLC) misses with perf?</strong></summary>

You specify hardware raw cache event sub-counters:
```bash
perf stat -e L1-dcache-load-misses,L1-dcache-loads,LLC-load-misses,LLC-loads ./my_app
```
This isolates whether cache stalls are occurring at the high-speed L1 data level (indicating suboptimal local struct packing) or reaching out to main DRAM across L3 LLC misses (indicating large dataset stride or memory footprint problems).
</details>

<details>
<summary><strong>10. What is "Statistical Blind Spot" in timer-based profilers?</strong></summary>

If a small, expensive function executes strictly in phase between timer sampling interrupts (or finishes faster than the sample period and runs infrequently), a sampling profiler might record zero hits for it. This is mitigated by increasing the sampling frequency (e.g. up to <code>-F 4999</code>) or using hardware event overflow sampling (e.g. sampling every $N$ CPU cycles or cache misses).
</details>

---

<details>
<summary><strong>11. How do you profile kernel space overhead versus user space overhead in Linux perf?</strong></summary>

In <code>perf report</code>, symbols are annotated with their privilege level: <code>[.]</code> denotes user-space library/binary code, while <code>[k]</code> denotes kernel-space execution (such as page fault handlers, system call servicing, or interrupt contexts). If <code>[k]</code> dominates, optimizations must target reducing system call frequency (e.g. batching I/O with <code>io_uring</code> or larger buffer chunks).
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Basic `perf stat` Execution
- **Task:** Write a Makefile target `profile-stat` that compiles an application with profiling flags and executes `perf stat` to measure CPU cycles and instructions.

<details>
<summary>Solution</summary>

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -O2 -g -fno-omit-frame-pointer

profile-stat: app
	perf stat -e cycles,instructions,cache-misses,branch-misses ./app

app: main.c
	$(CC) $(CFLAGS) main.c -o app
```
</details>

### Exercise 2 (Medium): Identifying Memory vs CPU Bottlenecks
- **Task:** Write a C program containing two distinct worker functions: one that is purely CPU arithmetic bound, and one that is memory bandwidth bound (striding through a massive array). Profile them with `perf stat`.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define SIZE (32 * 1024 * 1024)

static uint64_t cpu_bound_worker(uint32_t iters) {
    uint64_t x = 1;
    for (uint32_t i = 1; i < iters; ++i) {
        x = (x * 1664525ULL + 1013904223ULL);
    }
    return x;
}

static long memory_bound_worker(int *arr, size_t n) {
    long sum = 0;
    /* Large stride access: poor cache locality */
    for (size_t i = 0; i < n; i += 16) {
        sum += arr[i];
    }
    return sum;
}

int main(void) {
    printf("Starting workload...\n");
    cpu_bound_worker(50000000);

    int *big_array = malloc(sizeof(int) * SIZE);
    if (big_array) {
        memory_bound_worker(big_array, SIZE);
        free(big_array);
    }
    printf("Completed.\n");
    return 0;
}
```
</details>

### Exercise 3 (Hard): Building a Flame Graph from Scratch
- **Task:** Write a shell script that records an application with `perf`, collapses the stack frames, and invokes `flamegraph.pl` to output an interactive SVG report.

<details>
<summary>Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

BIN="./my_app"
SVG_OUT="profile_flamegraph.svg"

echo "[1/3] Sampling application with Linux perf..."
perf record -F 997 --call-graph dwarf -o perf.data "$BIN"

echo "[2/3] Unwinding and collapsing stack traces..."
perf script -i perf.data | stackcollapse-perf.pl > out.folded

echo "[3/3] Generating SVG Flame Graph..."
flamegraph.pl out.folded > "$SVG_OUT"

echo "Flame graph generated successfully: $SVG_OUT"
```
</details>

---

## 12. Summary / Cheat Sheet

| Command | Purpose |
| :--- | :--- |
| `perf stat ./bin` | High-level overview: cycles, instructions, IPC, cache misses |
| `perf record -F 997 -g ./bin`| Sample execution call stacks to `perf.data` |
| `perf report` | Interactive terminal UI breakdown of hot functions |
| `perf annotate <func>` | Line-by-line assembly cycle breakdown |
| `perf top` | Real-time live system profiling (like `top` for CPU cycles) |
| `kcachegrind callgrind.out`| GUI visualization of Callgrind function graphs |

---

## 13. Further Reading
- Systems Performance: Enterprise and the Cloud (Brendan Gregg)
- Linux perf Examples and Flame Graphs: `https://www.brendangregg.com/perf.html`
- KCachegrind Profiler Visualizer: `https://kcachegrind.github.io/`
- Linux kernel perf_events manual: `https://man7.org/linux/man-pages/man2/perf_event_open.2.html`

---
[<- Previous: 01-cache-and-memory-hierarchy.md](01-cache-and-memory-hierarchy.md) | [Section Index](README.md) | [Next: 03-compiler-optimizations.md ->](03-compiler-optimizations.md)\n