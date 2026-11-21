# Resource Limits and Process Control

> Master process containment and system limits in C: POSIX resource limits (`getrlimit`, `setrlimit`), descriptor caps (`RLIMIT_NOFILE`), memory limits (`RLIMIT_AS`), core dump configuration (`RLIMIT_CORE`), process nice priorities (`nice`, `getpriority`/`setpriority`), CPU affinity, and modern Linux isolation foundations (cgroups & namespaces).

| Metadata | Details |
| :--- | :--- |
| **Level** | Systems / Advanced |
| **Prerequisites** | [01-file-descriptors-and-posix-io.md](./01-file-descriptors-and-posix-io.md), [02-processes-fork-exec-wait.md](./02-processes-fork-exec-wait.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| POSIX.1-2008 / Linux kernel (`<sys/resource.h>`, `<sched.h>`) |

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
In multi-tenant servers, cloud environments, and daemon infrastructure, unconstrained processes pose significant stability hazards: a memory leak can trigger the kernel Out-Of-Memory (OOM) killer, terminating critical system services; an infinite loop can starve other CPU cores; or a process exhausting file descriptors can prevent network servers from accepting new connections.

Operating systems provide resource containment mechanisms allowing processes to query and enforce their own operating limits. Using the POSIX **Resource Limits API (`getrlimit` / `setrlimit`)**, applications can tune maximum open file descriptors, restrict virtual memory consumption, configure core dump file generation, adjust CPU scheduling priorities (`nice`), and pin critical threads to specific physical CPU cores.

---

## 2. Core Concepts
- **Soft Limit vs Hard Limit (`struct rlimit`)**:
  - `rlim_cur` (Soft Limit): The effective limit currently enforced by the kernel. An unprivileged process may increase or decrease its soft limit up to the hard limit.
  - `rlim_max` (Hard Limit): The ceiling for the soft limit. Only privileged processes (with `CAP_SYS_RESOURCE` or `root`) can raise the hard limit. Unprivileged processes can only lower it.
  - `RLIM_INFINITY`: Constant indicating an unconstrained resource limit.
- **Key Resource Limits**:
  - `RLIMIT_NOFILE`: Maximum number of open file descriptors plus one. Essential for tuning high-performance servers from the default 1024 up to 65,536+.
  - `RLIMIT_AS`: Maximum size of the process's total virtual address space. Exceeding this limit causes `malloc()` and `mmap()` to fail with `ENOMEM`.
  - `RLIMIT_CORE`: Maximum size of a core dump file in bytes. Setting to 0 disables core dumps; setting to `RLIM_INFINITY` enables full crash dumps for post-mortem GDB debugging.
  - `RLIMIT_CPU`: Maximum CPU time in seconds. Exceeding sends `SIGXCPU`.
  - `RLIMIT_STACK`: Maximum stack segment size.
- **Scheduling Priority (Nice Value)**: Ranges from `-20` (highest scheduling priority, consuming maximum CPU slices) to `+19` (lowest priority, "nicest" to other processes). Unprivileged users can only lower their priority (increase nice value).

---

## 3. How It Works Internally

### Kernel Resource Accounting in `task_struct`
In the Linux kernel, every process possesses a `struct signal_struct` containing an array of limits: `struct rlimit rlim[RLIM_NLIMITS]`.

```
Process Hierarchy                                 Kernel Limit Check on Syscall
+-------------------------------+                 +---------------------------------------------+
| Parent Process (e.g. bash)    |                 | sys_open() / sys_socket()                   |
|   RLIMIT_NOFILE: (1024, 4096) |                 | Checks: current_fds < task->rlim[NOFILE].cur|
+-------------------------------+                 | If false: returns -1 with errno = EMFILE    |
                |                                 +---------------------------------------------+
                v fork() inherits limits
+-------------------------------+                 +---------------------------------------------+
| Child Daemon                  |                 | sys_mmap() / sys_brk()                      |
| setrlimit(RLIMIT_NOFILE,      |                 | Checks: total_vm + pages <= rlim[AS].cur    |
|           {65536, 65536})     |                 | If false: returns -1 with errno = ENOMEM    |
+-------------------------------+                 +---------------------------------------------+
```

When a process calls `fork()`, the child inherits an exact copy of the parent's resource limits. When limits are altered via `setrlimit()`, the kernel updates the `task_struct` fields immediately. Subsequent system calls perform constant-time integer checks against these limits before allocating kernel objects.

---

## 4. Syntax & Usage

### Primary Header Inclusions
```c
#include <sys/resource.h> /* getrlimit, setrlimit, getpriority, setpriority */
#include <sys/time.h>     /* struct rlimit */
#include <unistd.h>       /* nice */
#include <sched.h>        /* sched_setaffinity, CPU_SET */
```

### Signature Overview
```c
int getrlimit(int resource, struct rlimit *rlim);
int setrlimit(int resource, const struct rlimit *rlim);

int getpriority(int which, id_t who);
int setpriority(int which, id_t who, int prio);
int nice(int inc);

int sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask);
int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask);
```

---

## 5. Practical Examples

### Example 1: Resource Limits Query and Simulation
```c
#include <stdio.h>
#include <stdint.h>

typedef struct {
    uint64_t rlim_cur; // Soft limit
    uint64_t rlim_max; // Hard limit
} RLimitSim;

int main(void) {
    printf("Simulating POSIX Resource Limits (getrlimit / setrlimit)...\n");
    RLimitSim nofile = { .rlim_cur = 1024, .rlim_max = 65536 };
    RLimitSim as     = { .rlim_cur = 1024ULL * 1024 * 1024 * 2, .rlim_max = 1024ULL * 1024 * 1024 * 4 };

    printf("RLIMIT_NOFILE (Max Open File Descriptors):\n");
    printf("  Soft Limit: %llu\n", (unsigned long long)nofile.rlim_cur);
    printf("  Hard Limit: %llu\n", (unsigned long long)nofile.rlim_max);

    printf("RLIMIT_AS (Max Virtual Address Space Memory):\n");
    printf("  Soft Limit: %llu GB\n", (unsigned long long)(as.rlim_cur / (1024 * 1024 * 1024)));
    printf("  Hard Limit: %llu GB\n", (unsigned long long)(as.rlim_max / (1024 * 1024 * 1024)));
    printf("Process Nice Priority range: [-20 (highest) to +19 (lowest)] verified.\n");
    return 0;
}
```

```text
Simulating POSIX Resource Limits (getrlimit / setrlimit)...
RLIMIT_NOFILE (Max Open File Descriptors):
  Soft Limit: 1024
  Hard Limit: 65536
RLIMIT_AS (Max Virtual Address Space Memory):
  Soft Limit: 2 GB
  Hard Limit: 4 GB
Process Nice Priority range: [-20 (highest) to +19 (lowest)] verified.
```

### Example 2: Programmatic File Descriptor Limit Escalation
```c
#include <stdio.h>
#include <sys/resource.h>

int maximize_fd_limit(rlim_t desired_limit) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        return -1;
    }

    /* If soft limit is already higher, do nothing */
    if (rl.rlim_cur >= desired_limit) {
        return 0;
    }

    /* Raise soft limit up to hard limit or desired value */
    if (desired_limit <= rl.rlim_max) {
        rl.rlim_cur = desired_limit;
    } else {
        rl.rlim_cur = rl.rlim_max;
    }

    if (setrlimit(RLIMIT_NOFILE, &rl) < 0) {
        return -1;
    }

    printf("Successfully raised RLIMIT_NOFILE soft limit to: %ld\n", (long)rl.rlim_cur);
    return 0;
}
```

---

## 6. Best Practices
1. **Always Raise `RLIMIT_NOFILE` in Network Servers**: The default Linux soft limit of 1024 descriptors is insufficient for web servers and proxies. Raise it to the hard limit during startup.
2. **Enable Core Dumps in Debug Builds**: In staging environments, configure `RLIMIT_CORE` to `RLIM_INFINITY` so that unexpected segmentation faults generate a core file for post-mortem GDB debugging. Disable in production (`rlim_cur = 0`) to prevent leaking memory secrets to disk.
3. **Never Lower Hard Limits Unintentionally**: Once an unprivileged process lowers its hard limit, it **cannot** be raised again. Only adjust `rlim_cur` unless you deliberately want to permanently restrict the process.
4. **Use CPU Pinning (`sched_setaffinity`) for Latency-Critical Threads**: Pin worker threads to dedicated CPU cores to eliminate OS thread migration overhead and maximize L1/L2 cache locality.
5. **Set Background Tasks to High Nice Values**: Background indexing or garbage collection worker threads should run with `nice(10)` or `nice(19)` to avoid starving latency-sensitive request threads.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Unprivileged process attempting to raise `rlim_max` | Fails immediately with `EPERM` (Operation not permitted) | Only raise `rlim_cur` up to current `rlim_max`, or require `CAP_SYS_RESOURCE` |
| Setting `RLIMIT_CORE = 0` during development | Prevents kernel from creating core dump files when application crashes, hindering GDB debugging | Set `rlim_cur = RLIM_INFINITY;` |
| Confusing `nice` priority direction | Positive numbers mean lower priority ("nicer" to others); negative numbers mean higher priority | Use `+10` to deprioritize; use `-10` to prioritize |
| Assuming `RLIMIT_AS` limits physical RAM | `RLIMIT_AS` limits total *virtual* address space; large sparse `mmap` or allocator arenas fail even if physical RAM is free | Use Linux cgroups (`memory.max`) to limit physical RAM consumption |
| Exceeding `RLIMIT_CPU` unexpectedly | Kernel sends `SIGXCPU` when soft CPU limit is exceeded, terminating process abruptly | Catch `SIGXCPU` and initiate graceful shutdown |
| Overwriting hard limits when setting soft limits | Setting `rl.rlim_max = desired` permanently drops the ceiling for the lifetime of the process | Keep `rl.rlim_max` unchanged: `rl.rlim_cur = desired;` |

---

## 8. Performance & Security Notes
- **Sandboxing & Attack Containment**: Restricting `RLIMIT_NPROC` (maximum child processes) prevents fork bomb denial-of-service attacks. Restricting `RLIMIT_FSIZE` prevents rogue processes from filling up root filesystems with huge files.
- **Linux Control Groups (cgroups v2)**: POSIX resource limits apply only per-process. Modern cloud containers (Docker, Kubernetes) manage resource isolation across entire hierarchies of processes using kernel **cgroups v2** (`/sys/fs/cgroup/`):
  - `memory.max`: Hard ceiling on physical memory; triggers OOM killer if exceeded.
  - `cpu.max`: Enforces hard CPU bandwidth quotas (e.g. 50,000 microseconds per 100,000 microsecond period = 0.5 CPU cores).
  - `io.max`: Limits read/write IOPS and bytes-per-second on storage devices.
- **Linux Namespaces**: While cgroups isolate resource usage, namespaces isolate visibility: PID namespaces (isolated process trees), Network namespaces (isolated IP addresses and routing tables), and Mount namespaces (isolated filesystem views).

---

## 9. Comparison / When to Use What

| Mechanism | Scope | Enforcement | Primary Use Case |
| :--- | :--- | :--- | :--- |
| **POSIX `setrlimit()`** | Single process + children | Kernel system call check | FD tuning, core dumps, basic stack size |
| **Process Nice (`nice()`)** | Single process / thread | CPU CFS Scheduler weighting | Background workers, CPU batch jobs |
| **CPU Affinity (`sched.h`)** | Single thread | Pinned CPU core mask | Low-latency audio, network packet processing |
| **Linux cgroups v2** | Entire process tree | Kernel hierarchical container | Docker, Kubernetes, systemd services |

---

## 10. Interview Questions

<details>
<summary>1. What is the difference between a soft limit and a hard limit in `setrlimit()`?</summary>
The soft limit (`rlim_cur`) is the effective boundary enforced by the operating system kernel for the resource. An unprivileged process can increase or decrease its soft limit anywhere between 0 and the current hard limit. The hard limit (`rlim_max`) serves as an absolute ceiling for the soft limit. Only privileged processes (running as `root` or with the `CAP_SYS_RESOURCE` capability) can increase the hard limit.
</details>

<details>
<summary>2. Why does a high-concurrency web server need to alter `RLIMIT_NOFILE`?</summary>
By default, most Linux distributions set the per-process soft file descriptor limit to 1024. In a web server handling concurrent connections, every connected socket, listening socket, log file, and database connection requires a file descriptor. With a 1024 limit, the server will fail to accept new connections beyond ~1000, returning `EMFILE`. Raising `RLIMIT_NOFILE` to 65,536 or higher allows the server to scale to tens of thousands of simultaneous clients.
</details>

<details>
<summary>3. What happens when a process exceeds its `RLIMIT_AS` (Address Space) limit?</summary>
When a process attempts to allocate additional virtual memory (e.g. via `malloc()`, `calloc()`, `sbrk()`, or `mmap()`) that would cause its total virtual address space to exceed `rlim_cur`, the kernel system call fails and returns an out-of-memory error code: `mmap()` returns `MAP_FAILED` with `errno == ENOMEM`, and `malloc()` returns `NULL`.
</details>

<details>
<summary>4. What signal does the kernel send when a process exceeds its `RLIMIT_CPU`?</summary>
When a process consumes more CPU seconds than allowed by its soft `RLIMIT_CPU`, the kernel delivers the `SIGXCPU` signal. If caught or ignored, the process continues running, but the kernel sends `SIGXCPU` once every second until the hard limit is reached, at which point the kernel forcibly terminates the process with `SIGKILL`.
</details>

<details>
<summary>5. How does the Unix "nice" value influence CPU scheduling?</summary>
The nice value is an integer ranging from -20 (highest priority) to +19 (lowest priority). In the Linux Completely Fair Scheduler (CFS), the nice value determines the relative proportional weight assigned to a task. A task with a lower nice value receives a larger share of CPU execution time compared to a task with a higher nice value when competing for CPU cores.
</details>

<details>
<summary>6. How do you configure a process to enable core dumps programmatically?</summary>
```c
struct rlimit rl;
rl.rlim_cur = RLIM_INFINITY;
rl.rlim_max = RLIM_INFINITY;
setrlimit(RLIMIT_CORE, &rl);
```
Setting `rlim_cur` to `RLIM_INFINITY` ensures that crash dumps of any size will be written to disk when an unhandled terminating signal occurs (e.g. `SIGSEGV`).
</details>

<details>
<summary>7. What is CPU pinning (thread affinity) and why is it used?</summary>
CPU pinning uses `sched_setaffinity()` to bind a thread to a specific set of CPU cores. By preventing the operating system scheduler from migrating the thread between different CPU cores, it eliminates thread context switching latency, preserves CPU L1/L2 cache contents, and eliminates inter-core cache invalidation.
</details>

<details>
<summary>8. What is the fundamental difference between `RLIMIT_AS` and Linux cgroups memory limit?</summary>
`RLIMIT_AS` restricts **virtual** address space allocations; a process mapping a 10 GB file via `mmap` consumes 10 GB of address space even if only 4 KB of physical RAM is touched. In contrast, cgroups (`memory.max`) restricts the actual **physical Resident Set Size (RSS)** and page cache used by the process group, providing true physical RAM containment.
</details>

---

## 11. Practice Exercises

### Easy
1. **File Descriptor Limit Inspector**: Write a program that queries and prints the current soft and hard limits for `RLIMIT_NOFILE`.
2. **Core Dump Disabler**: Write a function that programmatically sets `RLIMIT_CORE` to 0 to prevent core dumps.
3. **Nice Priority Reader**: Write a function that queries and prints the current process's nice priority using `getpriority(PRIO_PROCESS, 0)`.

<details>
<summary>Solutions (Easy)</summary>

```c
/* Exercise 1 */
#include <stdio.h>
#include <sys/resource.h>

void print_nofile_limits(void) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
        printf("Soft: %ld, Hard: %ld\n", (long)rl.rlim_cur, (long)rl.rlim_max);
    }
}

/* Exercise 2 */
#include <sys/resource.h>

void disable_core_dumps(void) {
    struct rlimit rl = { .rlim_cur = 0, .rlim_max = 0 };
    setrlimit(RLIMIT_CORE, &rl);
}
```
</details>

### Medium
4. **Memory Guard Utility**: Implement a function `int set_max_memory_mb(long mb)` that uses `RLIMIT_AS` to restrict virtual memory, and test that a large `malloc(2000 * 1024 * 1024)` fails with `NULL`.
5. **CPU Affinity Pinning**: Write a function that pins the current thread to CPU Core 0 using `sched_setaffinity()`.
6. **Graceful CPU Time Limit**: Set `RLIMIT_CPU` soft limit to 2 seconds and register a `SIGXCPU` handler that logs a warning message when triggered.

<details>
<summary>Solutions (Medium)</summary>

```c
/* Exercise 4: set_max_memory_mb */
#include <sys/resource.h>
#include <stdlib.h>
#include <stdio.h>

int set_max_memory_mb(long mb) {
    struct rlimit rl;
    rl.rlim_cur = (rlim_t)mb * 1024 * 1024;
    rl.rlim_max = rl.rlim_cur;
    return setrlimit(RLIMIT_AS, &rl);
}

void test_mem_limit(void) {
    set_max_memory_mb(50); /* Cap at 50 MB */
    void *ptr = malloc(100 * 1024 * 1024); /* Attempt 100 MB */
    if (!ptr) {
        printf("Success: Memory allocation was blocked by RLIMIT_AS!\n");
    } else {
        free(ptr);
    }
}
```
</details>

### Hard
7. **Sandbox Execution Wrapper**: Build a sandboxing wrapper `int run_sandboxed(const char *binary, rlim_t max_as, rlim_t max_cpu, rlim_t max_files)` that forks a child, sets strict resource limits, drops privileges, and executes untrusted code.
8. **Thread Worker Affinity Dispatcher**: Implement a thread pool manager that pins Worker 0 to Core 0, Worker 1 to Core 1, and so on, utilizing `pthread_setaffinity_np()`.
9. **Cgroup V2 Memory Controller Reader**: Write a C utility that reads `/sys/fs/cgroup/memory.current` and `/sys/fs/cgroup/memory.max` to report live container memory utilization metrics.

<details>
<summary>Solutions (Hard)</summary>

```c
/* Exercise 7: Sandboxed Runner */
#include <unistd.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <stdlib.h>

int run_sandboxed(const char *cmd, rlim_t max_as, rlim_t max_cpu, rlim_t max_files) {
    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        /* Set Memory Limit */
        struct rlimit r_as = { .rlim_cur = max_as, .rlim_max = max_as };
        setrlimit(RLIMIT_AS, &r_as);

        /* Set CPU Time Limit */
        struct rlimit r_cpu = { .rlim_cur = max_cpu, .rlim_max = max_cpu };
        setrlimit(RLIMIT_CPU, &r_cpu);

        /* Set File Descriptors Limit */
        struct rlimit r_fd = { .rlim_cur = max_files, .rlim_max = max_files };
        setrlimit(RLIMIT_NOFILE, &r_fd);

        execl(cmd, cmd, (char *)NULL);
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    return status;
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Limits**: `getrlimit(res, &rl)` / `setrlimit(res, &rl)`.
- **Soft vs Hard**: Soft (`rlim_cur`) is enforced; Hard (`rlim_max`) is ceiling.
- **Key Resources**:
  - `RLIMIT_NOFILE`: Max open file descriptors.
  - `RLIMIT_AS`: Max virtual address space.
  - `RLIMIT_CORE`: Max core dump size (0 = disable).
  - `RLIMIT_CPU`: Max CPU time (sends `SIGXCPU`).
- **Nice Priority**: `nice(inc)` adjusts scheduling weight [-20 to +19].
- **CPU Affinity**: `sched_setaffinity()` pins execution to specific CPU cores.

---

## 13. Further Reading
- W. Richard Stevens, *Advanced Programming in the UNIX Environment*, Chapter 7 (Process Environment - Resource Limits).
- Michael Kerrisk, *The Linux Programming Interface*, Chapter 36 (Process Resources).
- Linux Kernel Documentation: Control Groups v2 (`Documentation/admin-guide/cgroup-v2.rst`).

---

[← Previous Note: POSIX Time & Timers](./16-time-and-timers-posix.md) | [Module Index](README.md) | [Next Module: Build & Tooling](../06-build-and-tooling/README.md)
