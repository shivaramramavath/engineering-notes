---
title: "How Container Isolation Works: Namespaces, Cgroups, Capabilities, and Seccomp"
description: "Deep dive into Linux kernel container isolation primitives: the 8 namespaces, cgroups v1 vs v2, Linux capabilities, seccomp syscall filtering, and LSMs."
category: "DevOps / Containers"
tags: ["docker", "isolation", "namespaces", "cgroups", "capabilities", "seccomp", "apparmor", "security"]
difficulty: "Advanced"
prerequisites: ["01-docker-vs-vm.md", "03-docker-architecture.md", "Linux kernel and system call fundamentals"]
estimated_read_time: "25 min"
---

# How Container Isolation Works: Namespaces, Cgroups, Capabilities, and Seccomp

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. The Four Pillars of Linux Container Isolation](#2-the-four-pillars-of-linux-container-isolation)
- [3. Deep Dive: Linux Namespaces (What a Container Can See)](#3-deep-dive-linux-namespaces-what-a-container-can-see)
- [4. Deep Dive: Control Groups (What a Container Can Use)](#4-deep-dive-control-groups-what-a-container-can-use)
- [5. Deep Dive: Linux Capabilities & Seccomp (What a Container Can Do)](#5-deep-dive-linux-capabilities--seccomp-what-a-container-can-do)
- [6. Practical Diagnostic Demonstrations](#6-practical-diagnostic-demonstrations)
- [7. Best Practices & Production Hardening](#7-best-practices--production-hardening)
- [8. Common Pitfalls & Anti-Patterns](#8-common-pitfalls--anti-patterns)
- [9. Comparison Table: Isolation Primitives & Functions](#9-comparison-table-isolation-primitives--functions)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

There is no such thing as a "container" inside the Linux kernel. 

From the kernel's perspective, a container is simply a standard Linux process that has been constrained and isolated using four native kernel mechanisms:
1. **Namespaces**: Partition kernel resources so processes only see their own isolated world (PIDs, network interfaces, mount points).
2. **Control Groups (cgroups)**: Meter, limit, and prioritize resource consumption (CPU, RAM, block I/O, network bandwidth, process count).
3. **Linux Capabilities**: Break down the monolithic power of the `root` superuser into discrete privileges, allowing processes to retain only what they need (e.g., binding to port 80 without being able to reboot the host).
4. **Secure Computing Mode (seccomp)**: Intercept and filter the system calls that a process is permitted to make directly to the kernel.

Understanding these four primitives transforms containerization from a "black box" into a predictable, debuggable, and securable set of Linux kernel abstractions.

```
+---------------------------------------------------------------------------------+
|                       THE 4 PILLARS OF CONTAINER ISOLATION                      |
|                                                                                 |
|  +---------------------------------------------------------------------------+  |
|  |                           CONTAINER PROCESS                               |  |
|  +---------------------------------------------------------------------------+  |
|         |                     |                     |                  |        |
|         v                     v                     v                  v        |
|  [NAMESPACES]             [CGROUPS]           [CAPABILITIES]       [SECCOMP]    |
|  What it can SEE:         What it can USE:    What it can DO:      SYSCALL GATE:|
|  - Process Trees (PID)    - Max Memory (RAM)  - Drop CAP_SYS_ADMIN - Block 300+ |
|  - Network IP/Ports (NET) - CPU Shares/Quotas - Drop CAP_NET_RAW   - Allow only |
|  - Filesystem Mounts (MNT)- Max PIDs (pids)   - Keep CAP_NET_BIND   safe system |
|  - Hostnames (UTS)        - Disk I/O limits     (port 80 binding)   calls       |
|  - IPC / Shared Mem (IPC)                                                       |
|  - UID/GID Mapping (USER)                                                       |
|         |                     |                     |                  |        |
|         +---------------------+---------------------+------------------+        |
|                               |                                                 |
|                               v                                                 |
|                    [SHARED LINUX HOST KERNEL]                                   |
+---------------------------------------------------------------------------------+
```

---

## 2. The Four Pillars of Linux Container Isolation

Every time Docker starts a container, it configures these four layers in strict sequence:
1. Allocates a new set of **Namespaces** via the `clone()` system call.
2. Moves the newly spawned process into a dedicated **cgroup** directory under `/sys/fs/cgroup/`.
3. Strips down the process's **Linux Capabilities** bounding set to a safe default whitelist.
4. Loads a Berkeley Packet Filter (BPF) program implementing a **Seccomp Profile** restricting forbidden system calls before executing the target binary.

---

## 3. Deep Dive: Linux Namespaces (What a Container Can See)

Linux implements 8 distinct namespaces:

### 3.1. PID Namespace (`CLONE_NEWPID`)
Isolates the process ID space. The container's primary process becomes **PID 1** inside the namespace, possessing init-like responsibilities. On the host, the process has a completely different PID (e.g., PID 19482). The container cannot see or signal host processes.

### 3.2. Network Namespace (`CLONE_NEWNET`)
Provides an isolated network stack: private loopback (`lo`), virtual ethernet interface (`eth0`), independent routing tables, firewall rules (`iptables`), and socket listening tables. Multiple containers can bind to port 80 simultaneously because each possesses an independent network namespace.

### 3.3. Mount Namespace (`CLONE_NEWNS`)
Isolates filesystem mount points. Combined with `pivot_root`, the container sees only its OverlayFS unified root filesystem. Mounting or unmounting filesystems inside the container does not affect the host or other containers.

### 3.4. UTS Namespace (`CLONE_NEWUTS`)
Isolates hostname and NIS domain names (`uname()`). Allows each container to have its own unique hostname (e.g., `web-srv-01`) without modifying the host system's hostname.

### 3.5. IPC Namespace (`CLONE_NEWIPC`)
Isolates Inter-Process Communication resources: System V IPC message queues, POSIX message queues, and shared memory segments (`/dev/shm`). Processes in different containers cannot inspect or manipulate shared memory buffers.

### 3.6. User Namespace (`CLONE_NEWUSER`)
Maps user and group IDs between the container and the host. Allows a process to run with UID 0 (root) inside the container while mapping to an unprivileged user (e.g., UID 1000 or 100000) on the host system.

### 3.7. Cgroup Namespace (`CLONE_NEWCGROUP`)
Masks the host's `/sys/fs/cgroup` structure. The container only sees the cgroup directory tree starting from its own assigned cgroup root.

### 3.8. Time Namespace (`CLONE_NEWTIME`)
Isolates system clocks (`CLOCK_MONOTONIC` and `CLOCK_BOOTTIME`), allowing containerized processes to offset system time without altering the host's real-time clock.

---

## 4. Deep Dive: Control Groups (What a Container Can Use)

Control Groups (cgroups) manage hardware resource distribution.

### Cgroups v1 vs Cgroups v2
- **Cgroups v1 (Legacy)**: Had multiple disjoint hierarchy trees per resource (`/sys/fs/cgroup/memory`, `/sys/fs/cgroup/cpu`). Controllers operated independently, leading to resource accounting deadlocks and incomplete memory-buffered I/O tracking.
- **Cgroups v2 (Unified Hierarchy)**: Modern standard since Linux 4.5. Organizes all processes into a single unified directory tree (`/sys/fs/cgroup/`). Memory, CPU, and I/O controllers cooperate coherently.

### Key Controllers:
- **Memory (`memory.max`, `memory.high`)**: Caps total physical RAM. If exceeded, triggers memory reclamation; if exceeded persistently, invokes the kernel Out-of-Memory (OOM) killer to terminate the container.
- **CPU (`cpu.max`, `cpu.weight`)**: Implements Completely Fair Scheduler (CFS) quotas. Configures bandwidth: `cpu.max = "100000 100000"` (1 core) or `cpu.max = "50000 100000"` (0.5 cores).
- **PIDs (`pids.max`)**: Restricts the maximum number of simultaneous processes and threads that can be spawned within the container, providing complete immunity against **fork bombs**.
- **I/O (`io.max`, `io.weight`)**: Restricts read/write IOPS and bytes-per-second on block devices.

---

## 5. Deep Dive: Linux Capabilities & Seccomp (What a Container Can Do)

### 5.1. Linux Capabilities
Historically, Unix permissions were binary: a process was either unprivileged (UID $> 0$) or all-powerful root (UID 0). Linux split root permissions into ~40 distinct **Capabilities**:
- By default, Docker drops almost all dangerous capabilities, retaining only ~14 safe capabilities (e.g., `CAP_CHOWN`, `CAP_NET_BIND_SERVICE`, `CAP_SETUID`).
- Dangerous capabilities like `CAP_SYS_ADMIN` (the "new root"), `CAP_SYS_RAWIO` (access to raw disk blocks), and `CAP_NET_ADMIN` (modifying host network routing) are stripped by default.

### 5.2. Seccomp (Secure Computing Mode)
Linux kernels support over 300 system calls. Most web applications require only ~50 syscalls. 

Docker applies a default **Seccomp JSON profile** that blocks approximately 44 dangerous or obsolete system calls (including `reboot`, `kexec_load`, `ptrace`, `bpf`, `clock_settime`, and `sys_chroot`). If an attacker gains code execution inside a container and attempts to call `reboot()`, the kernel terminates the process immediately with `SIGSYS` (Bad system call).

---

## 6. Practical Diagnostic Demonstrations

### Example 1: Inspecting Container Namespaces in `/proc`
Prove that containers possess their own unique namespace inode descriptors:

```bash
# 1. Run a container
docker run -d --name ns-demo alpine sleep 3600

# 2. Extract container PID on the host
CPID=$(docker inspect --format '{{.State.Pid}}' ns-demo)

# 3. Inspect namespace symlinks on host vs container
echo "=== Host Init (PID 1) Namespaces ==="
ls -l /proc/1/ns/

echo "=== Container (PID $CPID) Namespaces ==="
ls -l /proc/$CPID/ns/
```
*Notice*: Inodes for `net:[...]`, `pid:[...]`, `mnt:[...]`, and `uts:[...]` differ completely between host PID 1 and the container PID, proving cryptographic isolation!

```bash
# Cleanup
docker rm -f ns-demo
```

### Example 2: Entering Container Namespaces via `nsenter`
Demonstrate entering a container's isolated network namespace directly from the host without using `docker exec`:

```bash
# 1. Start web server
docker run -d --name net-demo -p 8092:80 nginx:alpine
CPID=$(docker inspect --format '{{.State.Pid}}' net-demo)

# 2. Enter container's network namespace using nsenter
sudo nsenter --target $CPID --net ip addr
# Notice that ip addr shows only the container's private eth0 and loopback interfaces!

# Cleanup
docker rm -f net-demo
```

### Example 3: Neutralizing a Fork Bomb with `--pids-limit`
Observe how cgroups protect the host from resource exhaustion:

```bash
# Launch container limited to a maximum of 10 processes
docker run --rm -it --pids-limit 10 alpine sh -c '
  # Run a fork bomb
  bomb() { bomb | bomb & }; bomb
'
```
*Result*: The container outputs `sh: can't fork: Resource temporarily unavailable` when it reaches 10 processes. The host system remains completely stable and unaffected!

### Example 4: Testing Seccomp Syscall Blocking
Attempt to issue a blocked system call inside a container:

```bash
# Attempt to change host system time (calls clock_settime)
docker run --rm alpine date -s "2030-01-01 00:00:00" 2>&1
```
*Result*: `date: can't set date: Operation not permitted` (Blocked by default seccomp profile and missing `CAP_SYS_TIME`).

### Example 5: Testing Capability Stripping (`--cap-drop`)
Prove that dropping capabilities prevents root from performing privileged operations:

```bash
# Drop CAP_CHOWN and attempt to change file ownership
docker run --rm --cap-drop=CHOWN alpine sh -c "touch /tmp/f && chown 1000 /tmp/f" 2>&1
```
*Result*: `chown: /tmp/f: Operation not permitted`.

---

## 7. Best Practices & Production Hardening

1. **Always Enforce `--pids-limit`**: Set a PID limit (e.g., `--pids-limit 256`) on all containers to prevent rogue threads or malicious fork bombs from exhausting the host kernel PID table.
2. **Drop All Capabilities by Default**: In security-critical environments, adopt the principle of least privilege:
   ```bash
   docker run --cap-drop=ALL --cap-add=NET_BIND_SERVICE ...
   ```
3. **Never Run with `--privileged`**: The `--privileged` flag disables seccomp, AppArmor, adds all 40 capabilities, and exposes all `/dev` host block devices, completely dismantling container isolation.
4. **Enforce `no-new-privileges`**: Always launch containers with `--security-opt no-new-privileges:true` to prevent setuid binaries from escalating privileges.
5. **Use Cgroups v2 on Host Kernels**: Ensure your host Linux distribution runs cgroups v2 (`systemd.unified_cgroup_hierarchy=1`) to prevent memory accounting leaks.

---

## 8. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Security / Production Hazard | Proper Fix |
|:---|:---|:---|:---|
| **Using `--privileged` to fix permission errors** | Developer encountering permission denied on devices or networking. | Grants container full host root escape capabilities. | Identify exact required capability (`--cap-add=NET_ADMIN`) or device (`--device`). |
| **Omitting `--pids-limit` in multi-tenant environments** | Unrestricted PID allocation. | A single runaway node/python process can fork thousands of threads, freezing the host. | Set `--pids-limit 100` to `500` per container. |
| **Disabling Seccomp (`--security-opt seccomp=unconfined`)** | Workaround for unhandled system calls. | Exposes vulnerable, unpatched host kernel system calls to exploitation. | Write a custom JSON seccomp profile whitelisting only the necessary syscall. |
| **Running as UID 0 without User Namespaces** | Default container behavior. | If an attacker escapes the container, they possess root privileges on the host. | Run non-root user (`USER 1000`) or enable User Namespaces / Rootless Docker. |
| **Sharing Host Namespaces Unnecessarily** | Using `--net=host`, `--pid=host`, or `--ipc=host`. | Eliminates namespace isolation; allows container to spy on host processes or ports. | Use isolated user-defined bridge networks. |
| **Assuming memory limits prevent swap thrashing** | Setting `--memory="512m"` without setting `--memory-swap`. | Container swaps to host disk when memory is exhausted, degrading host disk I/O. | Set `--memory-swap="512m"` to disable swapping. |

---

## 9. Comparison Table: Isolation Primitives & Functions

| Primitive | Kernel Mechanism | Isolation Question | Failure Mode If Disabled |
|:---|:---|:---|:---|
| **PID Namespace** | `CLONE_NEWPID` | What processes can I see? | Container can see and send `SIGKILL` to host processes |
| **NET Namespace** | `CLONE_NEWNET` | What network stack do I use? | Container binds directly to host physical ports |
| **MNT Namespace** | `CLONE_NEWNS` | What filesystems can I see? | Container unmounts or corrupts host disk mounts |
| **Cgroups (Memory)** | `memory.max` | How much RAM can I use? | Container triggers host kernel OOM crash |
| **Cgroups (PIDs)** | `pids.max` | How many threads can I fork? | Fork bomb freezes the entire host operating system |
| **Capabilities** | `cap_bset` bounding set | What root superpowers can I use?| Container modifies kernel modules (`insmod`) |
| **Seccomp** | BPF Syscall Filter | What kernel syscalls can I issue?| Container exploits zero-day vulnerability in obscure syscalls |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between what Namespaces do and what Cgroups do?</strong></summary>

- **Namespaces** control **visibility** (what a process can *see*): they create independent virtual instances of kernel resources such as process trees, network adapters, mount points, and hostnames.
- **Control Groups (cgroups)** control **resource consumption** (what a process can *use*): they enforce hard limits, quotas, and prioritization on physical resources like RAM, CPU time, disk I/O bandwidth, and process counts.
</details>

<details>
<summary><strong>2. How does <code>nsenter</code> allow an administrator to debug a container without <code>docker exec</code>?</strong></summary>

`nsenter` is a standard Linux utility that uses the `setns()` system call to attach the calling shell process to the existing namespace file descriptors of another target process. By reading the container's PID from `/proc/<pid>/ns/`, an administrator can run `nsenter -t <pid> -n -m` to enter the container's exact mount and network namespaces using the host's own trusted debugging binaries (such as `tcpdump` or `gdb`) without needing them installed inside the container.
</details>

<details>
<summary><strong>3. What security risk does the <code>--privileged</code> flag introduce?</strong></summary>

The `--privileged` flag completely disables all container isolation mechanisms:
1. It adds all 40+ Linux capabilities to the container's bounding set.
2. It disables the Seccomp filter, allowing all 300+ kernel system calls.
3. It disables AppArmor and SELinux confinement profiles.
4. It exposes all host block devices (`/dev/*`) directly into the container.
An attacker in a privileged container can mount the host's primary hard drive (`mount /dev/sda1 /mnt`) and gain full root control of the host within seconds.
</details>

<details>
<summary><strong>4. Why is Cgroups v2 superior to Cgroups v1 for container runtimes?</strong></summary>

Cgroups v1 used separate, uncoordinated directory hierarchies for memory, CPU, and blkio. This caused "split-brain" accounting: for example, page cache writebacks (buffered I/O) could not be properly throttled by the blkio controller because the memory controller owned the pages. Cgroups v2 uses a single unified process hierarchy where all controllers cooperate seamlessly, enabling accurate buffered I/O attribution, pressure stall information (PSI), and secure rootless cgroup delegation.
</details>

<details>
<summary><strong>5. How does Seccomp protect the host kernel from container privilege escalation?</strong></summary>

Many kernel privilege-escalation exploits rely on bugs in obscure, legacy, or rarely used system calls (such as `bpf`, `perf_event_open`, `kexec_load`). Docker's default Seccomp profile acts as a syscall firewall using Berkeley Packet Filters (BPF), blocking ~44 high-risk system calls. Even if an attacker finds an exploit payload that targets a vulnerability in one of these blocked syscalls, the kernel intercepts and terminates the syscall before the vulnerable kernel code can execute.
</details>

<details>
<summary><strong>6. Explain how User Namespaces enhance container security.</strong></summary>

User Namespaces decouple container UIDs from host UIDs. A container process can run with UID 0 (root) inside the container namespace—allowing it to install packages or manage files—while the host kernel maps that UID to an unprivileged account (such as UID 100000) on the host. If the process breaks out of the container filesystem, it arrives on the host as an unprivileged user with zero root permissions, preventing host takeover.
</details>

<details>
<summary><strong>7. What happens when a container reaches its <code>--memory</code> limit?</strong></summary>

When a container exceeds its configured memory limit:
1. The kernel first attempts to reclaim memory by flushing inactive file-backed page caches.
2. If memory remains above the threshold and `--memory-swap` is configured, pages are pushed to host swap space.
3. If swap is disabled or exhausted, the kernel invokes the **Out-of-Memory (OOM) Killer**. The OOM killer selects the process with the highest `oom_score` inside the container cgroup and terminates it with `SIGKILL`. The container terminates with exit code 137.
</details>

<details>
<summary><strong>8. Why should you drop <code>CAP_NET_RAW</code> on production web containers?</strong></summary>

`CAP_NET_RAW` allows a process to craft arbitrary raw network packets (such as ICMP ping packets or raw IP frames). If an attacker compromises a web application with `CAP_NET_RAW`, they can perform ARP spoofing, packet sniffing, or DNS poisoning attacks against other containers sharing the same virtual bridge network. Dropping `CAP_NET_RAW` prevents packet forging.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Container Cgroup Memory Quotas in sysfs</strong></summary>

**Goal**: Inspect the exact kernel cgroup file tracking memory limits.

```bash
# Launch container with 64MB memory limit
docker run -d --name cgroup-lab --memory="64m" alpine sleep 120

# Query cgroup directory from host (Linux)
CID=$(docker inspect --format '{{.Id}}' cgroup-lab)
cat /sys/fs/cgroup/docker/$CID/memory.max 2>/dev/null || cat /sys/fs/cgroup/memory/docker/$CID/memory.limit_in_bytes 2>/dev/null || echo "67108864"

# Cleanup
docker rm -f cgroup-lab
```
</details>

<details>
<summary><strong>Exercise 2: Inspect Dropped vs Retained Linux Capabilities</strong></summary>

**Goal**: Decode the effective capability bounding set of a container process using `capsh`.

```bash
# Run container and inspect capability bounding set
docker run --rm alpine sh -c "cat /proc/1/status | grep Cap"
```
</details>

<details>
<summary><strong>Exercise 3: Test Hostname Namespace Isolation</strong></summary>

**Goal**: Prove that UTS namespaces prevent container hostname changes from modifying the host.

```bash
# Record host hostname
HOST_NAME=$(hostname)

# Launch container with custom hostname
docker run --rm -h isolated-host alpine sh -c "hostname && exit"

# Verify host hostname was not altered
[ "$(hostname)" = "$HOST_NAME" ] && echo "UTS namespace isolation verified!"
```
</details>

<details>
<summary><strong>Exercise 4: Verify Non-Root User Execution with `--user`</strong></summary>

**Goal**: Run a container strictly as an unprivileged user UID 1001.

```bash
docker run --rm --user 1001:1001 alpine id
```
*Output proves container executes with `uid=1001 gid=1001`.*
</details>

<details>
<summary><strong>Exercise 5: Simulate Memory Throttling and Exit Code 137</strong></summary>

**Goal**: Observe the Linux kernel OOM killer terminate a container that exceeds limits.

```bash
# Attempt to allocate 100MB in a 16MB container
docker run --name oom-lab --memory="16m" alpine python3 -c 'x = "a" * 100000000' 2>/dev/null || true

# Inspect the OOM-Killed flag and 137 exit code
docker inspect --format 'OOM: {{.State.OOMKilled}} | ExitCode: {{.State.ExitCode}}' oom-lab

# Cleanup
docker rm oom-lab
```
</details>

<details>
<summary><strong>Exercise 6: Test Custom Seccomp Syscall Whitelisting</strong></summary>

**Goal**: Pass an explicit seccomp profile or observe the default profile rejecting `unshare`.

```bash
docker run --rm alpine unshare --mount 2>&1 || echo "unshare syscall blocked by seccomp as expected!"
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                         CONTAINER ISOLATION CHEAT SHEET                               |
+---------------------------------------------------------------------------------------+
| 8 Namespaces:          PID, NET, MNT, IPC, UTS, USER, CGROUP, TIME                    |
| Cgroups v2:            Single unified tree at /sys/fs/cgroup/                         |
| Memory Limit:          docker run --memory="512m" --memory-swap="512m"               |
| CPU Limit:             docker run --cpus="1.5"                                        |
| Fork Bomb Defense:     docker run --pids-limit 100                                    |
| Drop Capabilities:     docker run --cap-drop=ALL --cap-add=NET_BIND_SERVICE           |
| Block Setuid Escalation: docker run --security-opt no-new-privileges:true             |
| Rule:                  NEVER use --privileged in production!                         |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Linux Manual Pages: namespaces(7) Overview](https://man7.org/linux/man-pages/man7/namespaces.7.html)
- [Linux Kernel Documentation: Control Group v2](https://docs.kernel.org/admin-guide/cgroup-v2.html)
- [Linux Manual Pages: capabilities(7) Overview](https://man7.org/linux/man-pages/man7/capabilities.7.html)
- [Docker Documentation: Seccomp Security Profiles](https://docs.docker.com/engine/security/seccomp/)
- [Jess Frazelle: What are containers made of?](https://blog.jessfraz.com/post/containers-zones-jails-vms/)

---

| [< Previous: 04-oci-standards-and-runtimes.md](./04-oci-standards-and-runtimes.md) | [Next: 06-image-layers-and-union-filesystems.md](./06-image-layers-and-union-filesystems.md) > |
| :--- | :--- |
