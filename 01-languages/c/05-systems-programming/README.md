# Module 05: Systems Programming

> Master Unix/Linux systems programming in C: POSIX file descriptors, process lifecycle (`fork`/`exec`/`waitpid`), signal architecture, IPC pipelines, multi-threading with POSIX pthreads and C11 threads, synchronization primitives, Berkeley network sockets, high-performance I/O multiplexing (`select`/`poll`/`epoll`), virtual memory mappings (`mmap`), and daemon engineering.

| Metadata | Details |
| :--- | :--- |
| **Module Code** | `05-systems-programming` |
| **Prerequisites** | [02-core-concepts/11-file-io.md](../02-core-concepts/11-file-io.md), [03-intermediate/03-error-handling.md](../03-intermediate/03-error-handling.md), [04-advanced/01-custom-allocators-and-arenas.md](../04-advanced/01-custom-allocators-and-arenas.md) |
| **Duration** | ~35 hours |
| **Status** | In Progress (Batches 19–20) |
| **Standard Baseline**| POSIX.1-2008 / ISO C11 / C17 / C23 (Linux kernel 5.x+, glibc 2.28+, Win32/WSL2 cross-notes) |

---

## Overview & Learning Outcomes

Systems programming in C bridges the gap between software algorithms and operating system kernel internals. Rather than relying on high-level language runtimes or runtime garbage collectors, systems programmers interface directly with the kernel via system calls (`syscall`), managing hardware resources, kernel file tables, process trees, and network interfaces.

In this module, you will master:
1. **Low-Level I/O & File Descriptors**: Kernel file descriptor tables, open file descriptions, v-nodes/inodes, non-blocking I/O (`O_NONBLOCK`), atomic appending, and descriptor duplication (`dup`, `dup2`).
2. **Process Engineering**: The Unix process model, parent-child relationships, copy-on-write (COW) memory semantics during `fork()`, program image replacement with `execve()`, and avoiding zombie and orphan processes with `waitpid()`.
3. **Signal Architecture & Asynchronous Safety**: Kernel signal dispatching, signal masks (`sigprocmask`), reliable signal handling (`sigaction`), `volatile sig_atomic_t`, and avoiding deadlocks in non-reentrant handlers.
4. **Inter-Process Communication (IPC)**: Kernel byte pipes (`pipe()`), named FIFOs (`mkfifo`), POSIX message queues, shared memory segments (`shm_open`, `mmap`), and POSIX semaphores.
5. **Concurrency & Thread Synchronization**: Multi-threading with POSIX pthreads (`pthread_create`, `pthread_join`) and C11 `<threads.h>`, mutual exclusion (`pthread_mutex_t`), condition variables (`pthread_cond_t`), atomic memory orderings (`<stdatomic.h>`), and avoiding deadlocks.
6. **Network Programming & Socket Architecture**: IPv4/IPv6 Berkeley stream and datagram sockets (`AF_INET`, `AF_INET6`), connection handshakes, TCP/UDP client-server design, and handling partial reads and writes.
7. **High-Performance Event Multiplexing**: Moving beyond blocking I/O threads using readiness notification engines: `select()`, `poll()`, and scalable Linux `epoll` with Edge-Triggered (`EPOLLET`) vs Level-Triggered notification.
8. **Virtual Memory & System Architecture**: Memory-mapped files (`mmap`, `MAP_SHARED` vs `MAP_PRIVATE`), Unix daemon detachment (`setsid`), system resource limits (`getrlimit`), and high-resolution POSIX timers.

---

## Module Table of Contents

| Note | Title | Level | Status | Focus Areas |
| :--- | :--- | :--- | :--- | :--- |
| **01** | [File Descriptors and POSIX I/O](./01-file-descriptors-and-posix-io.md) | Systems | Complete | Kernel fd tables, `open`, `read`, `write`, `close`, `lseek`, `dup2`, `O_NONBLOCK` |
| **02** | [Processes: fork, exec, wait](./02-processes-fork-exec-wait.md) | Systems | Complete | Copy-on-write `fork`, `execve`, `waitpid`, zombie/orphan prevention, process states |
| **03** | [Signals and Asynchronous Handlers](./03-signals.md) | Systems | Complete | `sigaction`, signal masks, `sigprocmask`, reentrancy, `volatile sig_atomic_t` |
| **04** | [Pipes and IPC](./04-pipes-and-ipc.md) | Systems | Complete | Anonymous `pipe()`, named FIFOs, `SIGPIPE` defense, multi-stage pipelines |
| **05** | [POSIX Threads (pthreads)](./05-pthreads.md) | Systems | Complete | `pthread_create`, `pthread_join`, thread attributes, stack sizing, cleanup handlers |
| **06** | [Mutexes, Condition Variables & Semaphores](./06-mutex-condvar-semaphores.md) | Systems | Complete | `pthread_mutex`, spurious wakeups, `pthread_cond_wait`, POSIX named/unnamed semaphores |
| **07** | [C11 Atomics and Memory Model](./07-c11-atomics-and-memory-order.md) | Systems | Complete | `<stdatomic.h>`, CAS loops, relaxed vs acquire-release vs seq_cst memory orders |
| **08** | [C11 Standard Threads](./08-c11-threads-h.md) | Systems | Complete | ISO C11 `<threads.h>`, `thrd_create`, `mtx_t`, `cnd_t`, `tss_t` thread-local storage |
| **09** | [Network Sockets: TCP and UDP](./09-sockets-tcp-udp.md) | Systems | Complete | Berkeley sockets, `sockaddr_in`, `bind`, `listen`, `accept`, non-blocking client/server |
| **10** | [I/O Multiplexing: select, poll, epoll](./10-io-multiplexing-select-poll-epoll.md) | Systems | Complete | Event notification, `FD_SETSIZE` limits, `pollfd`, Linux `epoll` Edge vs Level triggered |
| **11** | [Memory Mapping (mmap) & Shared Memory](./11-mmap-and-shared-memory.md) | Systems | Complete | Virtual memory, `mmap`, `munmap`, `MAP_SHARED`, `shm_open`, zero-copy IPC |
| **12** | [Daemons and Direct System Calls](./12-daemons-and-system-calls.md) | Systems | Complete | Unix daemonization, double-fork, `setsid`, `/dev/null` redirection, `syslog`, `syscall()` |
| **13** | [Terminals and termios](./13-terminals-and-termios.md) | Systems | Planned | Canonical vs raw mode, `tcgetattr`/`tcsetattr`, ANSI escape codes, terminal TUIs |
| **14** | [Filesystem APIs: stat & dirent](./14-filesystem-apis-stat-dirent.md) | Systems | Planned | `stat`, `lstat`, inode metadata, recursive directory traversal (`opendir`/`readdir`) |
| **15** | [System Error Handling & errno](./15-errno-and-system-error-handling.md) | Systems | Planned | Thread-local `errno`, `strerror_r`, `EINTR` restart loops, error propagation idioms |
| **16** | [POSIX Time and Timers](./16-time-and-timers-posix.md) | Systems | Planned | `clock_gettime`, `CLOCK_MONOTONIC`, `timerfd_create`, `nanosleep` vs busy wait |
| **17** | [Resource Limits and Process Control](./17-resource-limits-and-process-control.md) | Systems | Planned | `getrlimit`, `setrlimit`, `RLIMIT_NOFILE`, core dump limits, nice values, cgroups |

---

## Suggested Study Order
1. **Kernel I/O & Process Core**: Start with [File Descriptors & POSIX I/O](./01-file-descriptors-and-posix-io.md), [Processes](./02-processes-fork-exec-wait.md), and [Signals](./03-signals.md).
2. **Inter-Process Coordination**: Master [Pipes & IPC](./04-pipes-and-ipc.md) and [mmap & Shared Memory](./11-mmap-and-shared-memory.md).
3. **Concurrency & Synchronization**: Dive into [POSIX Threads](./05-pthreads.md), [Mutexes & Condvars](./06-mutex-condvar-semaphores.md), and [C11 Atomics](./07-c11-atomics-and-memory-order.md).
4. **Networking & Scale**: Advance through [Sockets TCP/UDP](./09-sockets-tcp-udp.md) and [I/O Multiplexing (epoll)](./10-io-multiplexing-select-poll-epoll.md).
5. **System Management & Daemons**: Conclude with [Filesystem APIs](./14-filesystem-apis-stat-dirent.md), [POSIX Timers](./16-time-and-timers-posix.md), and [Daemons & Syscalls](./12-daemons-and-system-calls.md).

---

[← Previous Module: Advanced](../04-advanced/README.md) | [C Language Track Index](../README.md) | [Next Module: Build & Tooling](../06-build-and-tooling/README.md)
