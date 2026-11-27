# Linux Basics & Architecture: Technical Interview Q&A

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Architecture, Kernel Mechanics & System Primitives |
| **Module** | `08-interview` — Technical Interview Preparation & Scenario Q&A |
| **Track** | `04-devops/linux` |
| **Target Level**| Junior to Senior DevOps / SRE / Systems Engineers |
| **Questions** | 20 Deep Technical & Architectural Questions |
| **Last Updated**| 2026 |

---

## Overview

Modern technical interviews evaluate an engineer's mental model of operating system behavior: the boundary between user space and kernel space, how system calls execute, process address spaces, and distribution package architectures.

---

### Q1: What is the exact distinction between the Linux Kernel and a Linux Distribution?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Mechanism:**
The **Linux Kernel** is the core supervisor program (`vmlinuz`) that executes in privileged hardware mode (Ring 0 on x86_64). It manages CPU scheduling, virtual memory paging, block storage drivers, the network protocol stack, and hardware interrupts. The kernel exposes system calls (`syscalls`) to user space.

A **Linux Distribution** (e.g., Ubuntu, Debian, RHEL, Alpine) is an integrated operating system distribution combining:
1. The Linux kernel.
2. The GNU or Musl C runtime library (`glibc` or `musl-libc`).
3. Core userland utilities (GNU Coreutils, systemd, bash).
4. A package management ecosystem (`apt/dpkg`, `dnf/rpm`, `apk`).
5. Default security policies (SELinux, AppArmor, default firewall rules).

**Interview Follow-Up Tip:**
Mention that container base images (e.g., `alpine:latest` vs `ubuntu:24.04`) do *not* contain a kernel; they provide only the userland rootfs. They share the host kernel running underneath.
</details>

---

### Q2: What happens under the hood when a CPU transitions from Ring 3 (User Space) to Ring 0 (Kernel Space)?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Mechanism:**
Modern x86_64 processors implement hardware privilege rings. Ring 3 is unprivileged user space; Ring 0 is privileged supervisor mode.

When a user program issues a system call (e.g., `write()`):
1. **Instruction Trigger**: The CPU executes the `SYSCALL` (or `SYSENTER`) machine instruction.
2. **Context Save**: The CPU hardware saves the current user-space instruction pointer (`RIP`) and flags (`RFLAGS`) into special model-specific registers (`RCX` and `R11`), switches the stack pointer (`RSP`) from user stack to the kernel thread stack, and flips the processor privilege level from Ring 3 to Ring 0.
3. **Dispatcher Execution**: The CPU jumps to the kernel's central system call entry handler (`entry_SYSCALL_64`).
4. **Table Lookup**: The kernel reads the system call number from the `RAX` register and looks up the corresponding function pointer in the `sys_call_table`.
5. **Execution & Return**: Once executed, the kernel executes `SYSRET`, which restores the user stack and switches back to Ring 3.
</details>

---

### Q3: Explain the fundamental architectural difference between a Monolithic Kernel (Linux) and a Microkernel (Mach, MINIX, seL4).
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Mechanism:**
- **Monolithic Kernel (Linux)**: All major operating system subsystems—VFS filesystem, device drivers, network protocol stack, memory management, and process scheduler—run together inside a single, unified address space in Ring 0. Subsystems communicate via fast direct C function calls. The primary drawback is that a single bug in a device driver can trigger a fatal kernel panic.
- **Microkernel (seL4, QNX)**: Only minimal core primitives reside in Ring 0 (basic thread scheduling, IPC, address space management). Device drivers, filesystems, and network stacks run as separate, isolated user-space processes (Ring 3). Subsystems communicate via Inter-Process Communication (IPC) messages. A crash in a filesystem driver does not crash the OS, but constant context switching and IPC overhead can degrade throughput.

**Modern Hybrid Reality:**
Linux mitigates monolithic fragility using dynamic Loadable Kernel Modules (`.ko`), eBPF sandboxed verification, and user-space filesystems (FUSE).
</details>

---

### Q4: Trace the complete Linux Boot Sequence from Power Button to Login Prompt.
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Step-by-Step Sequence:**
1. **Power-On & Firmware**: Motherboard executes UEFI (Unified Extensible Firmware Interface) or legacy BIOS, runs POST (Power-On Self Test), and reads EFI System Partition (`/boot/efi`).
2. **Bootloader (GRUB2)**: Loads kernel image (`vmlinuz`) and initial RAM disk (`initramfs`) into memory and passes kernel boot parameters (e.g., `root=UUID=... quiet splash`).
3. **Initramfs & Early User Space**: The kernel initializes CPU, memory controllers, and mounts the temporary root filesystem from RAM (`initramfs`). Initramfs loads necessary storage drivers (NVMe, RAID, LVM, multipath, LUKS encryption), mounts the real root filesystem (`/`), and pivots root via `pivot_root`.
4. **Init Execution (Systemd)**: The kernel executes `/sbin/init` (symlinked to `systemd`), which becomes PID 1.
5. **Systemd Target Ordering**: Systemd evaluates unit dependencies, bringing up `sysinit.target`, `basic.target`, local filesystems, `network.target`, and reaches `multi-user.target` or `graphical.target`, launching `sshd` and `getty` login shells.
</details>

---

### Q5: What is the Virtual Filesystem (VFS) in Linux and why is it essential?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Mechanism:**
The **Virtual Filesystem (VFS)** is an abstraction layer inside the Linux kernel that provides a uniform, standardized object-oriented C API (`open`, `read`, `write`, `close`, `stat`) to user-space applications, regardless of the underlying filesystem format.

**Key VFS Objects:**
- **`superblock`**: Represents an entire mounted filesystem instance (tracks block size, total inodes, filesystem type).
- **`inode`**: Represents a specific file metadata object (file size, permissions, owner, timestamps, block pointers).
- **`dentry` (Directory Entry)**: Connects human-readable path strings (`/var/log/syslog`) to inode numbers. Cached in the high-speed `dcache`.
- **`file`**: Represents an active open file instance held by a process (tracks current read/write byte offset and access flags).

Whether an application reads from an NVMe drive running `ext4`, an AWS EFS share over `NFS`, a container `overlayfs`, or a virtual filesystem (`/proc`), the code invokes the identical `read(fd, buf, count)` system call.
</details>

---

### Q6: What does the aphorism "Everything is a file" mean in Linux, and what are its practical limitations?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Meaning:**
Linux exposes regular disk files, hardware block devices (`/dev/nvme0n1`), character devices (`/dev/urandom`), IPC pipelines (anonymous pipes, named FIFOs), Unix domain sockets, and running kernel state (`/proc`, `/sys`) through unified filesystem paths. All of these can be inspected, opened, read, and written using standard system calls (`open()`, `read()`, `write()`, `close()`) and common CLI utilities (`cat`, `head`, `tee`, `lsof`).

**Practical Limitations & Exceptions:**
1. **Network Interfaces**: Physical network cards (`eth0`, `wlan0`) are *not* represented as files in `/dev/`. Packets cannot be read using `cat /dev/eth0`. Network configuration is managed via Netlink socket system calls (`iproute2`).
2. **Asynchronous I/O Engines**: Modern high-performance APIs like `io_uring` bypass traditional synchronous file descriptor semantics.
3. **Seekability**: Many pseudo-files (pipes, FIFO queues, `/dev/urandom`, socket streams) are non-seekable; executing `lseek()` on them returns `-1 ESPIPE`.
</details>

---

### Q7: Explain the difference between `/proc`, `/sys`, and `/dev`.
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

| Virtual Filesystem | Kernel Subsystem | Primary Purpose & Usage |
| :--- | :--- | :--- |
| **`/proc` (procfs)** | Process & Kernel State | Process information (`/proc/<pid>/status`, `fd/`, `maps`) and legacy sysctl tuning interfaces (`/proc/sys/net/...`). |
| **`/sys` (sysfs)** | Unified Device & Driver Model | Structured tree of hardware devices, bus controllers, block devices (`/sys/block/`), cgroups (`/sys/fs/cgroup/`), and kernel module tunables. |
| **`/dev` (devtmpfs)** | Device Special Files | Character and block device nodes (`/dev/sda`, `/dev/null`, `/dev/zero`, `/dev/pts/`) managed by `udevd` allowing user-space to interface with kernel drivers. |

**Key Takeaway:**
`/proc` is process- and software-centric; `/sys` is hardware-, bus-, and subsystem-centric; `/dev` holds the actual communication endpoints for device drivers.
</details>

---

### Q8: What is a System Call vs. a C Standard Library (glibc) Function?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Distinction:**
- **System Call (`syscall`)**: A direct request from user space to the kernel (Ring 0) requiring a hardware CPU context switch. Examples: `openat()`, `read()`, `write()`, `clone()`, `epoll_create()`. There are approximately ~350-450 system calls in Linux.
- **Library Function**: A standard C function executing entirely within user space (Ring 3). Examples: `strlen()`, `strcpy()`, `printf()`, `malloc()`.

**The Bridge (Wrappers & Buffering):**
Many library functions are user-space abstractions around system calls. For example, `printf()` formats strings in an in-memory user buffer before calling the `write()` system call. `malloc()` manages a heap memory pool in user space; it only invokes the `brk()` or `mmap()` system calls when the internal memory pool needs expansion.
</details>

---

### Q9: What is POSIX and why does it matter in modern DevOps engineering?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Definition:**
**POSIX (Portable Operating System Interface)** is a family of IEEE standards defining the API, shell environment, and utility interface for Unix-like operating systems.

**Operational Relevance:**
1. **Script Portability**: Writing bash scripts adhering to POSIX shell standards (`#!/bin/sh` without "bashisms" like `[[`, arrays, or non-standard flags) guarantees that provisioning scripts run seamlessly across Ubuntu, RHEL, Alpine (BusyBox/Ash), macOS, and FreeBSD.
2. **API Consistency**: Code written in C, Go, Python, or Rust calling POSIX primitives (`fork`, `execve`, `pthreads`, `mmap`) compiles and executes across all major Unix variants with minimal platform-specific patching.
</details>

---

### Q10: How does Linux manage Virtual Memory and 64-bit Process Address Spaces?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Mechanism:**
On 64-bit architectures (x86_64), CPUs typically use a 48-bit (or 57-bit 5-level) virtual address space:
- Lower 128 TB (`0x0000000000000000` to `0x00007FFFFFFFFFFF`) is private **User Space**.
- Upper 128 TB (`0xFFFF800000000000` to `0xFFFFFFFFFFFFFFFF`) is **Kernel Space**.

```
  0x00007FFFFFFFFFFF +-----------------------+
                     | Execution Stack (v)   | Grows downward
                     | Dynamic Libraries     | (mmap)
                     | Heap (^)              | Grows upward (brk)
                     | BSS / Data Segments   | Global uninitialized/init
  0x0000000000400000 | Code / Text (.text)   | Read-only executable binary
  0x0000000000000000 +-----------------------+ Null pointer trap zone
```

The CPU Hardware Memory Management Unit (MMU) translates virtual addresses to physical RAM addresses using hierarchical 4-level page tables, cached in the high-speed Translation Lookaside Buffer (TLB).
</details>

---

### Q11: What is a Page Fault, and what is the difference between Minor and Major Page Faults?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Mechanism:**
A **Page Fault** is an CPU hardware interrupt generated when a program attempts to access a virtual memory address that is not currently mapped to physical RAM.

- **Minor Page Fault (`soft fault`)**: The memory page is already loaded in physical RAM, but the MMU page table entry has not yet been established. Common examples include allocating memory via `malloc()` (demand paging allocates physical RAM only upon first write) or sharing read-only shared library pages (`libc.so`). Requires no disk I/O; resolves in nanoseconds.
- **Major Page Fault (`hard fault`)**: The requested memory page is *not* in physical RAM; it resides on physical block storage (either paged out to swap space, or memory-mapped from a disk file via `mmap`). The kernel must block the thread, initiate disk read I/O, wait for storage completion, and update the MMU. Can introduce multi-millisecond latency spikes.
</details>

---

### Q12: Explain the role and architecture of Systemd vs. Legacy SysVinit.
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Differences:**
- **SysVinit (Legacy)**: Serial initialization using shell scripts (`/etc/init.d/*`). Services booted one by one, resulting in slow boot times. Process supervision was primitive: daemons detached via double-forking, leaving PID tracking to error-prone `/var/run/*.pid` text files.
- **Systemd (Modern Standard)**: Declarative, parallel initialization driven by dependency trees (`Wants`, `Requires`, `After`).
  - **Cgroups Process Tracking**: Every service is encapsulated in its own cgroup slice (`system.slice/nginx.service`). Even if a daemon forks 100 child processes, systemd tracks every process ID and guarantees clean teardown on `systemctl stop`.
  - **Socket Activation**: Systemd opens listening sockets (e.g., port 80) *before* launching the service binary, enabling concurrent service boot and zero-downtime reloads.
  - **Unified Logging (`journald`)**: Binary, structured event logs capturing early-boot events, standard output, and kernel messages.
</details>

---

### Q13: What is the Linux OOM (Out-Of-Memory) Killer and how does it determine which process to terminate?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Mechanism:**
Linux practices **memory overcommit** (`vm.overcommit_memory = 0` default), granting `malloc()` requests that exceed physical RAM on the assumption that processes rarely utilize 100% of their allocated virtual memory simultaneously. When physical RAM and swap are completely exhausted, the kernel invokes the OOM Killer to prevent a complete system lockup.

**Scoring Algorithm (`oom_badness`):**
The kernel calculates a score (`/proc/<pid>/oom_score`, range 0 to 1000) for every process based on:
1. Percentage of total system RAM and swap consumed by the process.
2. Root-owned system processes receive minor score discounts.
3. The value configured in `/proc/<pid>/oom_score_adj` (-1000 to +1000). Setting `-1000` completely immunizes a process (e.g., PostgreSQL, SSHD) from being terminated.
</details>

---

### Q14: What is the difference between Buffered I/O, Direct I/O (`O_DIRECT`), and Synchronous I/O (`O_SYNC`)?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**I/O Modes:**
1. **Buffered I/O (Default)**: Application `write()` writes data to the kernel's in-memory Page Cache. The system call returns immediately. The kernel flushes dirty pages to disk asynchronously via `kswapd`/`flush` threads. Reads hit the page cache if cached.
2. **Direct I/O (`O_DIRECT`)**: Bypasses the Linux page cache entirely. Data is transferred via Direct Memory Access (DMA) directly between user-space memory buffers and the storage controller. Used by enterprise databases (PostgreSQL, Oracle, MySQL InnoDB) which manage their own buffer cache and want to prevent duplicate caching.
3. **Synchronous I/O (`O_SYNC` / `fsync`)**: Writes data through to physical non-volatile storage media before the system call returns, guaranteeing crash consistency at the expense of write latency.
</details>

---

### Q15: What is the Linux Page Cache and how does `sync` vs `fsync` work?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Mechanism:**
The **Page Cache** uses available free RAM to cache disk data blocks, converting expensive storage reads into sub-microsecond memory lookups. In Linux, "free memory is wasted memory": the OS automatically fills idle RAM with page cache and reclaims it instantly when applications demand heap memory.

**Sync System Calls:**
- `sync()`: Schedules a flush of *all* dirty buffers across the entire operating system to underlying storage. Executes asynchronously/globally.
- `fsync(fd)`: Blocks the calling thread until all modified data and metadata for that *specific file descriptor* are physically committed to permanent disk media.
- `fdatasync(fd)`: Similar to `fsync`, but only flushes data blocks and critical file size metadata, avoiding unnecessary inode timestamp updates.
</details>

---

### Q16: How do CPU Interrupts and SoftIRQs work during high-throughput packet processing?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Two-Stage Interrupt Architecture:**
1. **Top-Half (Hard IRQ)**: The physical NIC issues a hardware interrupt to the CPU. The CPU suspends active work, saves registers, disables further interrupts, acknowledges the hardware interrupt, schedules deferred work in the kernel ring buffer, and returns immediately (< 1 microsecond).
2. **Bottom-Half (SoftIRQ / NAPI)**: Deferred processing is handled asynchronously by kernel threads (`ksoftirqd/X`). In high-speed networking, Linux switches to **NAPI (New API)** polling: the CPU polls the NIC ring buffer directly, batching thousands of packets per cycle without suffering interrupt storms.

If `top` displays high `%si` (Software Interrupt) utilization, the CPU core is saturated processing network packet receive queues.
</details>

---

### Q17: What are the primary differences between Musl libc (Alpine) and GNU glibc (Ubuntu/RHEL)?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

| Metric / Feature | GNU `glibc` | `musl-libc` |
| :--- | :--- | :--- |
| **Primary Distros** | Ubuntu, Debian, RHEL, Fedora | Alpine Linux |
| **Binary Size** | Large (~2 MB+) | Extremely Small (~600 KB) |
| **Memory Allocator**| Ptmalloc (highly tuned for multi-threaded performance) | Musl allocator (minimal footprint, slower under heavy thread contention) |
| **DNS Resolution** | Supports NSS (`/etc/nsswitch.conf`), concurrent queries | Strictly parallel, ignores non-standard NSS modules |
| **C Standard Compliance**| Permissive with GNU extensions | Strict POSIX standard compliance |
| **Binary Compatibility**| Standard pre-compiled wheels (Python, Go) | Requires dedicated `musl` build or `gcompat` |

**DevOps Gotcha:**
Running standard Python C-extension wheels (`manylinux`) on Alpine often fails because they link against `glibc`.
</details>

---

### Q18: What is `glibc` NSS (Name Service Switch) and `/etc/nsswitch.conf`?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Mechanism:**
The **Name Service Switch (NSS)** is a modular mechanism used by the C library to configure how system databases (such as hostnames, user passwords, and group memberships) are resolved.

Configured in `/etc/nsswitch.conf`:
```text
passwd:         files systemd
hosts:          files mdns4_minimal [NOTFOUND=return] dns myhostname
```
When an application calls `gethostbyname()` or `getpwnam()`:
1. `files`: Checks `/etc/hosts` or `/etc/passwd`.
2. `dns`: Queries configured nameservers in `/etc/resolv.conf`.
3. If an NSS plugin fails or hangs (e.g., an uncontactable LDAP/Active Directory NSS module), even simple commands like `ls -l` can stall while attempting to resolve UIDs to usernames!
</details>

---

### Q19: Explain the difference between Asynchronous, Synchronous, Blocking, and Non-Blocking I/O.
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**I/O Model Matrix:**
- **Synchronous Blocking**: Application calls `read()`. Thread is put to sleep until data is fully copied into user-space buffer.
- **Synchronous Non-Blocking (`O_NONBLOCK`)**: Application calls `read()`. If no data is available, kernel immediately returns `-1 EAGAIN` / `EWOULDBLOCK`. Application must poll repeatedly.
- **I/O Multiplexing (`epoll` / `select`)**: Single thread monitors thousands of file descriptors. The kernel notifies the thread when specific descriptors become ready for non-blocking read/write.
- **True Asynchronous I/O (`io_uring`, POSIX AIO)**: Application submits an I/O request to a kernel submission queue and continues executing immediately. The kernel performs the I/O in the background and posts completion events to a completion queue.
</details>

---

### Q20: What are the fundamental trade-offs between `epoll` and `io_uring`?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Comparison:**
- **`epoll` (Event-driven notification)**: Informs the application *when* a socket is ready to be read or written. The application must still perform the synchronous `read()` or `write()` system call, incurring CPU context switches for every I/O operation. Furthermore, `epoll` cannot handle asynchronous disk file I/O (files always report ready).
- **`io_uring` (Asynchronous submission/completion queues)**: Uses two lockless ring buffers shared directly between user space and kernel space memory. The application places I/O requests into the Submission Queue (SQ) without making a system call. The kernel processes them and writes results to the Completion Queue (CQ). Supports true zero-syscall asynchronous operations across *both* network sockets and disk files, achieving millions of IOPS.
</details>

---
*Next Note: [02-filesystem-and-permissions-qa.md](file:///04-devops/linux/08-interview/02-filesystem-and-permissions-qa.md)*
