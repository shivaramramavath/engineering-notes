<!--
================================================================================
Metadata:
  Title: Linux Architecture: Kernel, User Space & System Calls
  Track: Linux & Systems Engineering / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 01-fundamentals/01-getting-started.md
  Key Terminology: Monolithic Kernel, User Space, Ring 0, Ring 3, System Calls, VFS, MMU, task_struct, glibc
================================================================================
-->

# Linux Architecture: Kernel, User Space & System Calls

[<- 01-getting-started.md](./01-getting-started.md) | [Back to Module Overview](./README.md) | [03-filesystem-hierarchy-and-navigation.md ->](./03-filesystem-hierarchy-and-navigation.md)

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Troubleshooting](#7-common-mistakes--troubleshooting)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table](#9-comparison-table)
- [10. Real-World Interview Questions](#10-real-world-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference Cheat Sheet](#12-summary--quick-reference-cheat-sheet)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

At the center of operating systems theory lies the design of the **Kernel**: the foundational software program that arbitrates access between application software and physical hardware (CPU, physical memory, storage drives, and network controllers).

Historically, two rival architectural paradigms emerged:
1. **Microkernels** (e.g., Mach, Minix, QNX): Only minimal primitives (IPC, low-level scheduling, basic paging) reside in kernel mode. Device drivers, filesystems, and networking stacks run as distinct userspace servers communicating via message passing. While highly modular and fault-tolerant, microkernels incur severe context-switching and IPC serialization overhead.
2. **Monolithic Kernels** (e.g., Linux, FreeBSD): The entire operating system logic—the CPU scheduler, Virtual Memory Manager, Virtual File System (VFS), networking stack, device drivers, and cryptographic subsystems—executes within a single unified address space in supervisor mode (**Ring 0**).

Linux chose the monolithic architecture, providing extraordinary raw performance: function calls between subsystems occur directly in memory without IPC marshalling. To prevent monolithic brittleness, modern Linux incorporates **Loadable Kernel Modules (LKMs)** and **eBPF (Extended Berkeley Packet Filter)**, allowing dynamic runtime extension without recompiling the kernel.

For DevOps engineers and SREs, understanding this architecture is essential for:
- Diagnosing high system CPU (`%sys`) and context-switch bottlenecks.
- Understanding how container engines (Docker/containerd) isolate processes without hypervisor overhead.
- Profiling high-throughput I/O using system call tracing (`strace`, `perf`, `bpftrace`).

---

## 2. Core Concepts

### CPU Protection Rings
Modern x86_64 and ARM64 processors implement hardware-enforced privilege levels. On x86 architectures, these are represented as Rings 0 through 3:
- **Ring 0 (Supervisor / Kernel Space)**: Complete hardware access. Can execute privileged instructions (`cli`, `sti`, `invlpg`), manipulate page table root pointers (`CR3`), configure interrupt descriptor tables (IDT), and communicate directly with memory controllers and PCIe devices.
- **Ring 3 (User Space)**: Restricted execution. Applications run in isolated virtual address spaces mapped by the CPU's Memory Management Unit (MMU). Direct access to physical memory or hardware I/O ports is prohibited.

```
+------------------------------------------------------------------------+
|                          RING 3: USER SPACE                            |
|                                                                        |
|   +-------------------+  +-------------------+  +------------------+   |
|   |   Web Server      |  |  Postgres RDBMS   |  |   Bash Shell     |   |
|   |  (Virtual Memory) |  |  (Virtual Memory) |  | (Virtual Memory) |   |
|   +---------+---------+  +---------+---------+  +--------+---------+   |
|             |                      |                     |             |
|             +----------------------+---------------------+             |
|                                    |                                   |
|                        Standard C Library (glibc)                      |
|                                    |                                   |
|                    System Call Wrapper (syscall, trap)                 |
+====================================|===================================+
|                          RING 0: KERNEL SPACE                          |
|                                    v                                   |
|                         [ System Call Table ]                          |
|                                    |                                   |
|       +----------------------------+---------------------------+       |
|       |                            |                           |       |
|       v                            v                           v       |
|  [ Process Manager ]      [ Memory Manager ]          [ VFS / Storage] |
|  - CFS Scheduler          - Page Allocator (Buddy)    - Ext4 / XFS     |
|  - task_struct            - Page Cache / Swap         - Block Layer    |
|  - Namespaces / Cgroups   - MMU / Page Tables         - NVMe / SCSI    |
|                                    |                                   |
|                                    v                                   |
|                       [ Network Stack / Drivers ]                      |
|                       - Netfilter / TCP/IP                             |
|                       - NIC Device Drivers (e1000, ixgbe, virtio)       |
+========================================================================+
|                              HARDWARE                                  |
|             [ CPU / MMU ]   [ Physical RAM ]   [ NVMe / NICs ]         |
+------------------------------------------------------------------------+
```

### The System Call Interface (SCI)
A **system call** is the standardized gateway through which a userspace application requests services from the kernel. Whenever a program reads a file (`read`), allocates anonymous memory (`mmap`), spawns a child process (`clone`/`fork`), or transmits a packet over a socket (`sendto`), it must trigger a system call.

### Process Abstraction (`task_struct`)
In the Linux kernel, every executing entity (whether a standalone single-threaded process or an individual thread in a thread pool) is represented by a `struct task_struct` defined in `<linux/sched.h>`. It maintains:
- Process Identifiers (`PID`, `TGID` - Thread Group ID).
- Process state (Running, Interruptible Sleep, Uninterruptible Sleep `D-state`, Zombie).
- Virtual memory descriptors (`struct mm_struct`).
- File descriptor tables (`struct files_struct`).
- Security credentials (`struct cred`: UID, GID, capabilities).
- Signal handling tables and CPU scheduling metrics.

---

## 3. How It Works Internally

### The Anatomy of an x86_64 System Call
When an application calls `read(fd, buf, count)` in C, the journey from userspace to disk and back follows a precise microarchitectural sequence:

```
[ User Application: read() ]
          |
          v
[ glibc read() Wrapper ]
  1. Load syscall number (__NR_read = 0) into %rax
  2. Load arguments: fd -> %rdi, buf -> %rsi, count -> %rdx
  3. Execute CPU instruction: 'syscall'
          |
          |  (CPU switches privilege: Ring 3 -> Ring 0)
          |  (CPU saves %rip and %rflags into internal registers)
          v
[ Kernel Entry: entry_SYSCALL_64 ]
  4. Kernel switches to per-process Kernel Stack
  5. Pushes user registers to stack (creates struct pt_regs)
  6. Reads %rax and indexes into sys_call_table[0] -> ksys_read()
          |
          v
[ Virtual File System (VFS) Layer ]
  7. Validates fd in current->files->fdt
  8. Obtains struct file pointer
  9. Calls file->f_op->read_iter() (e.g., ext4_file_read_iter)
          |
          v
[ Page Cache / Block Layer ]
  10. Checks if data resides in RAM (Page Cache hit)
      -> If miss: triggers Block I/O request (bio), puts process in 'D' state
  11. Copies bytes from Kernel Page Cache to User buffer (%rsi) via copy_to_user()
          |
          v
[ Kernel Exit: sysretq / iretq ]
  12. Places bytes read into %rax (return value)
  13. Restores saved user registers
  14. Executes 'sysretq' instruction (Ring 0 -> Ring 3)
          |
          v
[ User Application resumes execution ]
```

### The Virtual File System (VFS)
Linux implements the Unix philosophy: *"Everything is a file"*. The kernel achieves this uniform abstraction through the VFS. Whether querying an ext4 hard drive, an in-memory synthetic filesystem (`/proc`, `/sys`), a network share (`NFS`), or a Unix domain socket, userspace interacts with the exact same system calls (`openat`, `read`, `write`, `close`).

The VFS defines four core object models:
1. **Superblock**: Represents an entire mounted filesystem (block size, total inodes, filesystem status).
2. **Inode**: Represents a specific metadata entity on disk (file size, permissions, owner, timestamps, block pointers). *Does not store the file name.*
3. **Dentry (Directory Entry)**: Connects a human-readable file path string to its underlying inode number, cached in the kernel's high-speed **dcache**.
4. **File Object**: Represents an open file instance created when a process calls `open()`. Holds the current seek offset (`f_pos`), access mode flags, and reference counts.

---

## 4. Syntax & Usage

### Inspecting System Calls with `strace`
`strace` is the primary diagnostic utility for intercepting and recording system calls made by a process and the signals received.

```bash
# Basic trace of a single command execution
strace ls -la /etc

# Summary / Profiling mode: aggregate count, time, and errors per system call
strace -c ls -la /etc

# Trace only specific system calls (e.g., file-related or network-related)
strace -e trace=openat,read,write cat /etc/resolv.conf
strace -e trace=network curl -s http://example.com

# Attach to an existing running process by PID
sudo strace -p 1420 -f

# Capture microsecond timestamps and system call duration
strace -tt -T -p 1420 -o /tmp/trace_output.txt
```

### Inspecting Process Kernel State via `/proc`
The `/proc` virtual filesystem exposes direct views into kernel data structures:

```bash
# View process system call state (PID 1)
sudo cat /proc/1/syscall
# Output: 232 0x0 0x0 0x0 0x0 0x0 0x7ffd9864 0x7f...
# (Format: syscall_number arg1 arg2 arg3 arg4 arg5 arg6 sp pc)

# View thread and process status, including Voluntary / Nonvoluntary context switches
grep -E "voluntary_ctxt_switches|Threads|voluntary" /proc/$$/status

# View address space memory maps
head -n 20 /proc/$$/maps
```

---

## 5. Practical Examples

### Example 1: Profiling System Call Overhead of a Command
```bash
# Measure where time is spent during a find operation
strace -c find /usr -name "*.so" >/dev/null

# Sample Output Analysis:
# % time     seconds  usecs/call     calls    errors syscall
# ------ ----------- ----------- --------- --------- ----------------
#  54.12    0.042100           3     12450           newfstatat
#  28.40    0.022100           4      5210           getdents64
#  12.10    0.009400           5      1820       120 openat
#   5.38    0.004180           2      1700           close
# ------ ----------- ----------- --------- --------- ----------------
# 100.00    0.077780                 21180       120 total
```
*Insight*: The command spent 54% of its kernel time inside `newfstatat` verifying file properties and 28% inside `getdents64` reading directory entries.

### Example 2: Attaching `strace` to a Blocked / Frozen Process
```bash
# Identify a stuck process in uninterruptible sleep ('D' state) or waiting on network
ps aux | grep -i "[D]"

# Attach strace to observe the blocking system call
sudo strace -p 3842

# If strace prints:
# restart_syscall(<... resuming interrupted poll ...>)
# -> The process is blocked waiting on an I/O event or network socket.

# If strace prints:
# futex(0x7f4b3c00, FUTEX_WAIT_PRIVATE, 2, NULL ...
# -> The process is deadlocked or waiting on a userspace mutex lock.
```

### Example 3: Diagnosing "File Not Found" Caused by Missing Dynamic Libraries
```bash
# When a binary returns 'No such file or directory' despite existing:
strace -e trace=openat,execve ./custom_binary 2>&1 | grep -E "ENOENT|ld-linux"

# Output reveals the missing dependency:
# openat(AT_FDCWD, "/lib64/ld-linux-x86-64.so.2", O_RDONLY|O_CLOEXEC) = -1 ENOENT (No such file or directory)
# Remediation: Install the required libc package or fix architecture symlinks.
```

### Example 4: Identifying High CPU Overhead from Excessive Context Switches
```bash
# Monitor voluntary vs involuntary context switches across all CPUs
vmstat 1 5

# Sample Output:
# procs -----------memory---------- ---swap-- -----io---- -system-- ------cpu-----
#  r  b   swpd   free   buff  cache   si   so    bi    bo   in    cs us sy id wa st
#  4  0      0 204812  42100 812940    0    0     0    12 1200 45000 12 38 50  0  0

# Note the 'cs' (context switches) column showing 45,000/sec and 'sy' (system CPU) at 38%.
# Investigate offending processes using pidstat:
pidstat -w 1 3
```

### Example 5: Writing a Minimal Assembly Program Calling Raw System Calls (No glibc)
```bash
# Create a pure x86_64 assembly program that writes to stdout using syscall
cat << 'EOF' > /tmp/hello.s
.global _start
.text
_start:
    # sys_write(fd=1, buf=msg, count=14)
    mov $1, %rax        # syscall number for sys_write on x86_64
    mov $1, %rdi        # file descriptor 1 (stdout)
    lea msg(%rip), %rsi # pointer to message buffer
    mov $14, %rdx       # string length
    syscall             # invoke kernel system call

    # sys_exit(status=0)
    mov $60, %rax       # syscall number for sys_exit
    xor %rdi, %rdi      # return status 0
    syscall

.data
msg:
    .ascii "Hello, Kernel\n"
EOF

# Assemble and link without any C standard library
as /tmp/hello.s -o /tmp/hello.o
ld /tmp/hello.o -o /tmp/hello_bare

# Run and inspect binary dependencies
/tmp/hello_bare
# Output: Hello, Kernel

ldd /tmp/hello_bare
# Output: not a dynamic executable (0 shared library dependencies, executes directly via kernel)
```

---

## 6. Best Practices

- **Batch System Calls**: Minimize round-trips to kernel space. Use buffered I/O in applications (`setvbuf`, language buffers like Go's `bufio` or Node's Streams) rather than writing 1 byte at a time.
- **Never Run `strace` on High-Throughput Production Daemons**: `strace` relies on the `ptrace` system call, which pauses the target process twice for every single syscall (once on entry, once on exit). This can degrade database throughput by 80–90%. Use **eBPF tools** (`bpftrace`, `bcc`, `perf`) instead.
- **Utilize Modern Async I/O (`io_uring`)**: For extreme I/O workloads, migrate from synchronous `read`/`write` or readiness polling (`epoll`) to `io_uring`, which uses shared memory ring buffers to achieve zero-syscall I/O submissions.
- **Inspect `/proc/sys/kernel` Parameters**: Familiarize yourself with tunable kernel knobs (`sysctl`) governing scheduler behavior, memory allocation policies, and file descriptor maximums.
- **Enforce Seccomp Filters on Containers**: Restrict the attack surface of containerized workloads by blocking dangerous system calls (such as `reboot`, `mount`, `ptrace`, `kexec_load`) using container security profiles.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| Running `strace -f` on production PostgreSQL | `ptrace` stop hooks pause all threads on entry and exit of every syscall, causing query timeouts and cascade connection failures. | Use eBPF tracepoints: `sudo bpftrace -e 'tracepoint:syscalls:sys_enter_write /pid == 1234/ { ... }'` |
| Assuming `printf()` or `console.log()` immediately reaches disk | Standard C runtime buffers userspace stdout; data stays in userspace memory until buffer flushes or newline is encountered. | Call `fflush(stdout)` or use `fsync(fd)` to guarantee persistence before power-loss events. |
| Process stuck in `D` state (Uninterruptible Sleep) cannot be killed with `kill -9` | The process is blocked inside a kernel system call (typically waiting for unresponsive NFS/storage or hardware response); signals are deferred until return to user space. | Resolve the underlying hardware/NFS issue; `kill -9` cannot terminate a thread while sleeping in kernel space. |
| Confusing User CPU (`%us`) with System CPU (`%sy`) | High `%sy` indicates the bottleneck is not user application algorithms, but kernel operations (syscall overhead, page faults, lock contention, packet processing). | Profile kernel functions with `perf top` or `perf record -g` rather than application debuggers. |
| Ignoring `open()` return values | When `open()` fails, it returns `-1` and sets `errno`. Passing `-1` to subsequent `read()` calls produces confusing `EBADF (Bad file descriptor)` errors. | Always check return values and format error messages using `strerror(errno)` or equivalent runtime handlers. |
| Confusing Process ID (`PID`) with Thread ID (`TID`) | In userspace POSIX terminology, a process has one PID and many TIDs. In the Linux kernel, every thread is a `task_struct` with its own unique PID, while sharing a common Thread Group ID (`TGID`). | When diagnosing multi-threaded apps in `/proc`, look at `/proc/<PID>/task/<TID>/`. |

---

## 8. Performance, Security & Cost Implications

- **Performance**: Every transition between User Space and Kernel Space forces CPU register state preservation, cache line invalidation, and TLB (Translation Lookaside Buffer) strain. Mitigations for CPU hardware speculative execution vulnerabilities (Spectre/Meltdown), such as Kernel Page Table Isolation (KPTI), add a 2–15% CPU tax to syscall-heavy workloads.
- **Security**: The System Call Interface is the primary attack boundary between untrusted userspace code and the operating system. Over 80% of kernel privilege escalation vulnerabilities originate in flawed syscall handlers. Minimizing exposed syscalls via **Seccomp** (Secure Computing Mode) is mandatory in production container environments.
- **Cost**: High system CPU utilization (`%sy`) directly translates to over-provisioning compute nodes in cloud environments (AWS EC2 / GCP Compute Engine). Optimizing syscall patterns often allows downsizing instance classes by 2x to 4x.

---

## 9. Comparison Table

| Attribute | Monolithic Kernel (Linux) | Microkernel (Mach, Minix, QNX) | Hybrid Kernel (Windows NT, macOS XNU) |
| :--- | :--- | :--- | :--- |
| **Execution Space** | Drivers, VFS, Net, Scheduler all in Ring 0 | Only IPC, Scheduler, Basic Memory in Ring 0 | Drivers & Core subsystems in Ring 0; High-level APIs in Ring 3 |
| **Subsystem Communication**| Direct C function calls in memory | Inter-Process Communication (IPC) messages | Direct function calls + LPC message passing |
| **Performance** | Highest throughput, zero IPC tax | Slower due to constant context switches | Balanced |
| **Fault Isolation** | Driver crash can cause Kernel Panic | Driver crash only crashes isolated server daemon | Driver crash can cause Blue Screen of Death (BSOD) |
| **Extensibility** | Loadable Kernel Modules (LKMs), eBPF | Start/Stop userspace daemons dynamically | Dynamically loaded kernel-mode drivers |
| **Real-World Examples** | Linux, FreeBSD | QNX (Automotive), Minix 3, seL4 | Windows 11, macOS Darwin, iOS |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain the step-by-step sequence of events that occurs when an application executes a system call on x86_64.</strong></summary>

**Answer**:
1. **Setup**: The userspace application or `glibc` wrapper places the system call number into register `%rax` (e.g., `0` for `sys_read`) and loads arguments into `%rdi`, `%rsi`, `%rdx`, `%r10`, `%r8`, and `%r9`.
2. **Transition**: The CPU executes the `syscall` instruction. The CPU hardware switches execution privilege from Ring 3 (User) to Ring 0 (Kernel), saves the next instruction pointer (`%rip`) into `%rcx`, and saves the flags register (`%rflags`) into `%r11`.
3. **Dispatch**: The CPU jumps to the address stored in the Model Specific Register `MSR_LSTAR` (`entry_SYSCALL_64`).
4. **Kernel Stack Switch**: The kernel switches the stack pointer `%rsp` from the user stack to the process's kernel-mode stack.
5. **Context Preservation**: The kernel pushes userspace registers onto the kernel stack to construct a `struct pt_regs`.
6. **Execution**: The kernel verifies that `%rax` does not exceed `NR_syscalls`, indexes into `sys_call_table`, and invokes the corresponding C function (e.g., `ksys_read()`).
7. **Return**: The function places its return value into `%rax`. The kernel restores registers from `struct pt_regs` and executes `sysretq`, restoring Ring 3 privilege and returning execution to the userspace caller.
</details>

<details>
<summary><strong>2. What is a "D state" process and why cannot it be terminated with `kill -9`?</strong></summary>

**Answer**:
A process in state `D` is in **TASK_UNINTERRUPTIBLE** (uninterruptible sleep). In this state, a thread has invoked a kernel system call and is sleeping while waiting for an external hardware event (almost always synchronous block storage I/O, paging, or an unresponsive NFS mount).

The kernel deliberately ignores all asynchronous signals (including `SIGKILL` / `kill -9`) while a task is in `D` state to protect kernel data structure integrity: if a process were reaped mid-syscall while holding kernel mutexes or waiting on filesystem blocks, the kernel state would become corrupted, leading to an immediate kernel panic. The process will only handle the pending `SIGKILL` once the underlying I/O completes and execution returns to the syscall exit boundary.
</details>

<details>
<summary><strong>3. What is the Virtual File System (VFS) and what role does it serve?</strong></summary>

**Answer**:
The VFS is an abstraction layer within the Linux kernel that provides a uniform interface to userspace programs regardless of the underlying physical storage mechanism or filesystem format.

It defines standard interfaces (`file_operations`, `inode_operations`, `dentry_operations`) implemented by individual filesystem drivers (ext4, XFS, Btrfs, NFS, procfs, sysfs). Because of VFS, userspace applications use identical system calls (`open`, `read`, `write`, `stat`) to interact with an NVMe drive, an in-memory kernel metric (`/proc/cpuinfo`), a hardware terminal (`/dev/tty`), or a network socket.
</details>

<details>
<summary><strong>4. How does the Linux kernel differentiate between a Process and a Thread?</strong></summary>

**Answer**:
At the kernel scheduler level, Linux does not have fundamentally distinct data structures for processes versus threads. Every execution context is represented by a `task_struct`.

When a program creates a thread via `pthread_create()`, the underlying system call invoked is `clone()` with specific sharing flags:
- `CLONE_VM`: Share the virtual memory address space.
- `CLONE_FS`: Share filesystem information (root and current working directory).
- `CLONE_FILES`: Share open file descriptor tables.
- `CLONE_SIGHAND`: Share signal handlers.

Threads belong to the same **Thread Group** and share a common Thread Group ID (`TGID`, which userspace calls the `PID`). A traditional single-threaded process is simply a thread group containing exactly one task.
</details>

<details>
<summary><strong>5. What is the difference between a Minor Page Fault and a Major Page Fault?</strong></summary>

**Answer**:
When a process accesses a virtual memory address that is not currently mapped to physical RAM in the CPU's MMU page tables, a **Page Fault** hardware exception is generated:
- **Minor Page Fault**: The requested memory page is already present in physical RAM, but the MMU page table entry has not yet been initialized for this process. Common examples: allocating memory via `malloc`/`mmap` (kernel uses lazy allocation and only maps the page on first write) or sharing a read-only copy-on-write (COW) page. The kernel updates the page table mapping immediately without disk I/O; resolution latency is measured in nanoseconds.
- **Major Page Fault**: The requested memory page is not in physical RAM at all; it must be fetched from secondary block storage (such as reading an unmapped section of an executable binary or retrieving a page swapped out to disk). The process is placed into `D` state while block I/O executes; resolution latency is measured in milliseconds.
</details>

<details>
<summary><strong>6. What is Seccomp (Secure Computing Mode) and how is it used in container security?</strong></summary>

**Answer**:
Seccomp is a Linux kernel security feature that restricts the system calls a process is permitted to execute. Using Berkeley Packet Filter (BPF) bytecode rules, seccomp filters incoming system calls based on syscall number and argument values.

If a process invokes an unauthorized system call, seccomp can immediately kill the process (`SECCOMP_RET_KILL`), return an error code like `EPERM` (`SECCOMP_RET_ERRNO`), or trap to a tracing supervisor. Container runtimes (Docker, containerd, Kubernetes) apply a default seccomp profile that blocks approximately 44 out of 300+ syscalls (including `reboot`, `syslog`, `mount`, `kexec_load`, and `ptrace`), preventing container breakout vulnerabilities even if the process runs as container root.
</details>

<details>
<summary><strong>7. Why is `io_uring` considered superior to `epoll` for high-throughput I/O?</strong></summary>

**Answer**:
`epoll` is an asynchronous **readiness notification** mechanism: it informs the application that a file descriptor is ready to be read or written, but the application must subsequently execute synchronous `read()` or `write()` system calls, incurring system call overhead and context switches for every data transfer. Furthermore, `epoll` does not support asynchronous operations on regular disk files (disk reads always block in Linux).

`io_uring` provides true **asynchronous completion-based I/O** via two lock-free ring buffers (a Submission Queue and a Completion Queue) shared between userspace memory and the kernel. Applications write I/O requests directly to the queue without invoking system calls. A single `io_uring_enter()` call can submit hundreds of operations, or the kernel can run a polling thread (`IORING_SETUP_SQPOLL`), completely eliminating system call overhead during continuous I/O streams.
</details>

<details>
<summary><strong>8. What is the performance penalty of `ptrace` when used by tools like `strace`?</strong></summary>

**Answer**:
`ptrace` requires the traced process to halt execution and signal the tracing process twice for every single system call:
1. `PTRACE_SYSCALL` stops the process immediately before the kernel executes the syscall, transferring CPU control to `strace` to inspect arguments.
2. The kernel executes the syscall, halts the process again upon completion, and transfers CPU control to `strace` to inspect return values.

This doubles context switches and CPU cache evictions for every syscall, frequently slowing application throughput by 10x to 50x. In high-traffic database or web environments, running `strace` can trigger connection pool exhaustion and cluster failovers.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Trace System Calls of a Basic Shell Utility</strong></summary>

**Task**: Execute the `uname -a` command using `strace` and identify the exact system call invoked to retrieve operating system release facts.

**Solution**:
```bash
strace -e trace=uname uname -a
# Observed output:
# uname({sysname="Linux", nodename="...", release="6.8.0-45-generic", ...}) = 0
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Inspect Virtual Memory Mappings via `/proc`</strong></summary>

**Task**: Inspect the memory layout of your current running shell process, identifying the text (code), heap, and stack regions.

**Solution**:
```bash
cat /proc/$$/maps | grep -E "\[heap\]|\[stack\]|/bin/bash"
# Shows memory addresses, permissions (r-xp for text, rwxp for heap/stack), and inode references.
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Profile File Open Activity on Web Requests</strong></summary>

**Task**: Use `strace` to track all files opened by `curl` when making an HTTPS request, filtering out successful opens versus failed lookups.

**Solution**:
```bash
strace -e trace=openat curl -s https://example.com > /dev/null 2> /tmp/curl_trace.txt
# Inspect failed opens (e.g., searching for SSL certificates across multiple paths)
grep "ENOENT" /tmp/curl_trace.txt | head -n 10
# Inspect successful certificate open
grep "/etc/ssl" /tmp/curl_trace.txt
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Trace Context Switching Dynamics</strong></summary>

**Task**: Write a bash loop that creates process churn and observe the impact on voluntary versus involuntary context switches using `pidstat`.

**Solution**:
```bash
# Terminal 1: Run CPU and thread churn workload
python3 -c "import time, threading; [threading.Thread(target=lambda: [time.sleep(0.001) for _ in range(1000)]).start() for _ in range(20)]" &
WORKLOAD_PID=$!

# Terminal 2: Monitor context switches for this PID
pidstat -w -p $WORKLOAD_PID 1 5
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Inspect eBPF Tracepoints for Syscall Monitoring</strong></summary>

**Task**: Use `bpftrace` to count system calls globally across all processes on the system over a 5-second interval without using `strace`.

**Solution**:
```bash
# Verify bpftrace installation (Ubuntu: sudo apt install -y bpftrace)
sudo bpftrace -e 'tracepoint:raw_syscalls:sys_enter { @[comm] = count(); } interval:s:5 { exit(); }'

# Output displays count of system calls per process executable without pausing threads.
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Apply a Restrictive Seccomp Filter via Docker</strong></summary>

**Task**: Run a container with a custom seccomp profile or Docker flag that blocks the `mkdir` system call, verifying that directory creation fails with `Operation not permitted`.

**Solution**:
```bash
# Create custom seccomp profile blocking mkdir
cat << 'EOF' > /tmp/deny-mkdir.json
{
  "defaultAction": "SCMP_ACT_ALLOW",
  "syscalls": [
    {
      "names": ["mkdir", "mkdirat"],
      "action": "SCMP_ACT_ERRNO"
    }
  ]
}
EOF

# Run container with custom seccomp profile
docker run --rm --security-opt seccomp=/tmp/deny-mkdir.json alpine:3.20 sh -c "mkdir /test"
# Output: mkdir: can't create directory '/test': Operation not permitted
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Subsystem / Metric | Inspection Tool | Key Insight |
| :--- | :--- | :--- |
| **System Call Tracing** | `strace -c <command>` | Aggregate count, execution duration, and error codes |
| **Process Syscall State** | `cat /proc/<PID>/syscall` | Currently executing syscall number and registers |
| **Context Switching** | `vmstat 1` / `pidstat -w` | Evaluates `%sy` tax and CPU scheduling frequency |
| **Kernel Parameters** | `sysctl -a` / `/proc/sys/` | Runtime configuration of kernel behavior |
| **Kernel Profiling** | `perf top` / `bpftrace` | Zero-overhead sampling of active kernel functions |
| **Process State** | `ps -eo pid,state,comm` | Identifies `R` (Running), `S` (Sleeping), `D` (Disk/I/O wait) |

---

## 13. Further Reading & Official Documentation

- [The Linux Kernel Documentation: System Calls](https://docs.kernel.org/process/adding-syscalls.html)
- [man 2 syscalls: Overview of Linux System Calls](https://man7.org/linux/man-pages/man2/syscalls.2.html)
- [Brendan Gregg: Linux Systems Performance & eBPF](https://www.brendangregg.com/overview.html)
- [LWN.net: Anatomy of a System Call](https://lwn.net/Articles/604287/)
- [Docker Documentation: Seccomp Security Profiles](https://docs.docker.com/engine/security/seccomp/)

---

[<- 01-getting-started.md](./01-getting-started.md) | [Back to Module Overview](./README.md) | [03-filesystem-hierarchy-and-navigation.md ->](./03-filesystem-hierarchy-and-navigation.md)
