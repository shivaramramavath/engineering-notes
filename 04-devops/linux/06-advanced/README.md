# Module 06: Advanced Systems, Containers & Observability

[<- Back to Linux Track Root](../README.md) | [<- Previous Module: 05-production](../05-production/README.md) | [Next Module: 07-projects ->](../07-projects/README.md)

---

## 1. Overview & Scope

To truly master Linux engineering, an engineer must move beyond configuring userspace utilities and understand the deep primitives of the Linux kernel itself. Container engines such as Docker, containerd, and Kubernetes do not employ virtualization; they are orchestration abstractions layered over kernel namespaces, control groups (cgroups), seccomp profiles, and Linux capabilities.

Module 06 explores these low-level kernel primitives. You will dissect container isolation mechanisms from scratch using `unshare`, analyze system calls and signals using `strace` and `gdb`, profile CPU execution and kernel context switches using `perf` and flamegraphs, author dynamic kernel tracing scripts using extended Berkeley Packet Filters (eBPF) and `bpftrace`, and tune high-concurrency network stacks.

---

## 2. Core Architecture: Container Primitives & eBPF Observability

```
+---------------------------------------------------------------------------------------+
| LINUX KERNEL PRIMITIVES & DYNAMIC OBSERVABILITY ARCHITECTURE                          |
|                                                                                       |
|   [ Containerized User Process ]                                                      |
|   ├── Bound by 8 Linux Namespaces: PID, NET, MNT, IPC, UTS, USER, CGROUP, TIME        |
|   ├── Throttled by Unified Cgroups v2: memory.max, cpu.max, io.max                    |
|   ├── Restricted by Capabilities: Drops CAP_SYS_ADMIN, CAP_NET_ADMIN                  |
|   └── Filtered by Seccomp-BPF: Restricts disallowed system calls                      |
|         │                                                                             |
|         v (System Calls: clone, read, write, epoll_wait)                              |
|   +=============================================================================+     |
|   | System Call Dispatcher & Tracepoints (sys_enter / sys_exit)                 |     |
|   +=============================================================================+     |
|         │                                                                             |
|         v                                                                             |
|   [ Linux Kernel Space & eBPF In-Kernel Virtual Machine ]                             |
|   ├── Kprobes / Kretprobes: Dynamic kernel function tracing                           |
|   ├── Tracepoints: Static instrumentation points                                      |
|   ├── Uprobes: Dynamic userspace function tracing (e.g. libc malloc)                  |
|   └── eBPF Bytecode Programs (JIT Compiled, Verified Safety)                          |
|         │                                                                             |
|         v (eBPF Ring Buffers & Maps)                                                  |
|   [ Userspace Observability Tools: bpftrace, BCC, perf, Flamegraphs ]                 |
+---------------------------------------------------------------------------------------+
```

---

## 3. Module Notes Catalog

| # | Note | Level | Time | Key Concepts & Topics |
| :---: | :--- | :---: | :---: | :--- |
| **01** | [Namespaces & Control Groups (Cgroups v2)](./01-namespaces-and-cgroups.md) | Advanced | 65m | The 8 namespaces in depth, cgroups v1 vs unified cgroups v2 (`/sys/fs/cgroup`), memory controllers, CPU shares/quotas, blkio limits |
| **02** | [Containers from Scratch: Unshare, Chroot & Pivot Root](./02-containers-from-scratch.md) | Advanced | 70m | Building an isolated container without Docker, `unshare -mpf`, `pivot_root` vs `chroot`, mounting `/proc` inside namespaces, veth pairing |
| **03** | [Advanced Debugging: Strace, Ltrace & GDB](./03-advanced-debugging.md) | Advanced | 65m | Tracing system calls (`strace -tt -T -e`), library calls (`ltrace`), attaching GDB to running processes, inspecting core dumps, debugging deadlocks |
| **04** | [CPU Profiling & Flamegraphs with Linux Perf](./04-perf-and-flamegraphs.md) | Advanced | 60m | Hardware performance counters (PMU), `perf record`, `perf report`, symbol resolution, sampling frequency, generating Brendan Gregg flamegraphs |
| **05** | [eBPF & Dynamic Tracing with Bpftrace](./05-ebpf-and-bpftrace-intro.md) | Advanced | 70m | Extended BPF architecture, kernel verifier, kprobes/kretprobes, tracepoints, uprobes, one-liner `bpftrace` programs, measuring latency distributions |
| **06** | [Kernel Tuning & High-Throughput Network Stack Optimization](./06-kernel-tuning-and-network-stack-tuning.md) | Advanced | 65m | TCP window scale, ephemeral port exhaustion (`ip_local_port_range`), `somaxconn`, SYN cookies, congestion control (BBR vs Cubic), hugepages |
| **07** | [Linux Security Modules & POSIX Capabilities](./07-linux-security-modules-and-capabilities.md) | Advanced | 60m | Breaking root into granular capabilities (`CAP_NET_BIND_SERVICE`, `CAP_SYS_PTRACE`), `getcap`/`setcap`, ambient capabilities, LSM hooks |
| **08** | [Kernel Tracing: Ftrace & SystemTap Overview](./08-systemtap-ftrace-overview.md) | Advanced | 55m | Built-in kernel `ftrace` (`/sys/kernel/debug/tracing`), function graph tracer, trace-cmd, SystemTap scripting and kernel module compilation |

---

## 4. Key Milestones & Learning Objectives

By completing Module 06, you will be able to:
1. **Demystify Container Internals**: Assemble a fully isolated container using low-level kernel primitives (`unshare`, `pivot_root`, cgroups v2, and network namespaces) with zero third-party software.
2. **Debug Complex Distributed Bugs**: Intercept, profile, and isolate hanging system calls, file descriptor leaks, and memory corruptions using `strace` and GDB core analysis.
3. **Generate Production Flamegraphs**: Identify CPU hot paths, inefficient kernel locks, and lock contention using `perf` and SVG flamegraphs.
4. **Implement Zero-Overhead eBPF Probes**: Write dynamic `bpftrace` instrumentation scripts to monitor disk I/O latency histograms and socket lifecycles live in production without crashing the host.
5. **Optimize High-Traffic Network Stacks**: Tune sysctl TCP buffer sizes, backlog queues, and connection tracking parameters to handle 100,000+ concurrent connections per node.
