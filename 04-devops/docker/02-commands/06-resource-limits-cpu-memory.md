---
title: "Container Resource Management: CPU, Memory, Swap, PIDs, and docker update"
description: "Mastering Docker resource constraints: memory limits, swap controls, CFS CPU quotas, CPU core pinning, fork bomb protection via pids-limit, and dynamic resource tuning with docker update."
category: "DevOps / Containers"
tags: ["docker", "resource-limits", "cpu", "memory", "swap", "pids", "cgroups", "performance"]
difficulty: "Intermediate"
prerequisites: ["01-fundamentals/05-how-isolation-works.md", "02-commands/01-run-ps-stop.md"]
estimated_read_time: "25 min"
---

# Container Resource Management: CPU, Memory, Swap, PIDs, and docker update

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. Memory Resource Constraints](#2-memory-resource-constraints)
- [3. CPU Resource Constraints (CFS Quotas and Pinning)](#3-cpu-resource-constraints-cfs-quotas-and-pinning)
- [4. Process Limits & Fork Bomb Defense (`--pids-limit`)](#4-process-limits--fork-bomb-defense---pids-limit)
- [5. Dynamic Resource Tuning via `docker update`](#5-dynamic-resource-tuning-via-docker-update)
- [6. Practical Diagnostic Demonstrations](#6-practical-diagnostic-demonstrations)
- [7. Best Practices & Production Sizing Guidelines](#7-best-practices--production-sizing-guidelines)
- [8. Common Pitfalls & Anti-Patterns](#8-common-pitfalls--anti-patterns)
- [9. Comparison Table: Resource Limiting Flags](#9-comparison-table-resource-limiting-flags)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

By default, a Docker container has **no resource constraints**. It is free to consume as much physical RAM, swap space, CPU cycles, and process threads as the underlying host operating system's kernel will allow.

In a multi-tenant cluster or production environment running multiple microservices, this "wild west" default is an invitation to disaster:
- A single runaway Python or Node.js service with a memory leak can consume all available host RAM, triggering the Linux Out-of-Memory (OOM) killer to terminate adjacent mission-critical databases or system daemons.
- An unconstrained CPU-intensive task (e.g., cryptographic hashing or video encoding) can starve the host OS scheduler of CPU cycles, causing healthcheck timeouts and dropping network traffic.
- A malicious fork bomb inside a single container can exhaust the host kernel's PID table in seconds, completely freezing the server.

Docker implements hardware resource limits directly through the Linux kernel's **Control Groups (cgroups)** subsystem. Mastering resource constraints guarantees high availability, prevents noisy-neighbor syndrome, and ensures fair resource distribution.

```
+---------------------------------------------------------------------------------+
|                       CONTAINER RESOURCE GOVERNANCE                             |
|                                                                                 |
|  [Physical Host Hardware: 8 Cores, 32GB RAM]                                    |
|        |                                                                        |
|        +---> [Container A: API Service]                                         |
|        |     - Memory Limit: --memory="1g" --memory-swap="1g" (No swap allowed) |
|        |     - CPU Allocation: --cpus="2.0" (Throttled to 2 cores via CFS)      |
|        |     - Max Threads: --pids-limit=200 (Immune to fork bombs)             |
|        |                                                                        |
|        +---> [Container B: Background Worker]                                   |
|        |     - Pinned to CPU Cores 6 & 7: --cpuset-cpus="6,7"                   |
|        |     - Soft Reservation: --memory-reservation="512m"                    |
|        |                                                                        |
|        v                                                                        |
|  [Kernel cgroups subsystem enforces boundaries with ~0% CPU virtualization tax] |
+---------------------------------------------------------------------------------+
```

---

## 2. Memory Resource Constraints

Docker configures memory boundaries through the kernel's cgroup memory controller.

### 2.1. Hard Limit (`-m`, `--memory`)
The maximum amount of physical RAM the container can allocate. Accepts units (`b`, `k`, `m`, `g`):
```bash
docker run -d --memory="512m" nginx:alpine
```
If a container attempts to allocate memory beyond this limit, the Linux kernel invokes the OOM killer on the container process, terminating it with exit code 137.

### 2.2. Swap Limits (`--memory-swap`)
Controls the total cumulative limit of **Physical Memory + Swap Memory**:
- **Formula**: $\text{Total Memory Available} = \text{Swap Limit}$.
- If `--memory="1g"` and `--memory-swap="3g"`, the container can allocate $1\text{ GB}$ of physical RAM and $2\text{ GB}$ of host swap ($3 - 1 = 2$).
- **Disabling Swap**: To strictly prevent a container from swapping to host disk, set `--memory-swap` to the **exact same value** as `--memory`:
  ```bash
  docker run -d --memory="1g" --memory-swap="1g" my-app
  ```
- **Unrestricted Swap**: If `--memory-swap="-1"`, the container is permitted to consume as much host swap space as exists on the host disk.

### 2.3. Soft Limit (`--memory-reservation`)
A soft boundary that allows elastic memory usage. The container can exceed this limit during normal operation, but when the host kernel detects overall physical memory contention, it aggressively forces containers exceeding their reservation back down toward their reserved limit.

---

## 3. CPU Resource Constraints (CFS Quotas and Pinning)

Docker enforces CPU limits through the Linux kernel's **Completely Fair Scheduler (CFS)**.

### 3.1. Fractional CPU Quotas (`--cpus`)
Specifies the total compute bandwidth a container can utilize per scheduling period (default $100\text{ ms}$):
```bash
# Limit container to 1.5 CPU cores
docker run -d --cpus="1.5" my-service
```
Under the hood, this sets:
- `cpu.cfs_period_us = 100000` ($100\text{ ms}$)
- `cpu.cfs_quota_us = 150000` ($150\text{ ms}$ total execution time across all cores per period)

### 3.2. Relative CPU Shares (`-c`, `--cpu-shares`)
A soft prioritization mechanism (default value `1024`):
- If Host has 1 CPU core and Container 1 has `--cpu-shares 1024` and Container 2 has `--cpu-shares 512`, Container 1 receives $66.6\%$ of the CPU time during contention, and Container 2 receives $33.3\%$.
- When the host is idle, Container 2 can still consume $100\%$ of the CPU core.

### 3.3. CPU Core Pinning (`--cpuset-cpus`)
Binds the container process directly to specific physical or virtual CPU cores (CPU affinity):
```bash
# Pin container to execute strictly on physical CPU cores 0 and 1
docker run -d --cpuset-cpus="0,1" high-perf-app

# Pin to a range of cores
docker run -d --cpuset-cpus="0-3" worker-app
```
*Advantage*: Eliminates CPU cache invalidation penalties by preventing the kernel scheduler from migrating threads across NUMA nodes or cores.

---

## 4. Process Limits & Fork Bomb Defense (`--pids-limit`)

A classic denial-of-service attack or buggy multi-threaded application can execute a **fork bomb**:
```bash
:(){ :|:& };:
```
This endlessly spawns child processes until the Linux kernel's PID table (`/proc/sys/kernel/pid_max`, typically 32,768 or 4,194,304) is completely saturated. Once saturated, the host operating system cannot spawn a single new process—even `systemctl`, `ssh`, or `ps` will fail with `fork: Resource temporarily unavailable`.

### The Defense:
```bash
docker run -d --pids-limit 100 my-app
```
The kernel cgroup PID controller restricts the total cumulative count of processes and threads inside the container. If the container attempts to create PID 101, the system call returns `EAGAIN` inside the container without affecting the host!

---

## 5. Dynamic Resource Tuning via `docker update`

One of Docker's most powerful production features is **`docker update`**: the ability to modify cgroup limits on an actively running container **without restarting it or dropping active network connections**:

```bash
docker update [OPTIONS] CONTAINER [CONTAINER...]
```

```bash
# Increase memory limit on a running container from 512MB to 2GB
docker update --memory="2g" --memory-swap="2g" live-api-server

# Throttle CPU on a runaway container
docker update --cpus="0.5" runaway-worker

# Increase PID limit
docker update --pids-limit 500 live-api-server
```
The Docker daemon modifies the values directly in `/sys/fs/cgroup/`, and the Linux kernel updates its scheduling rules instantly.

---

## 6. Practical Diagnostic Demonstrations

### Example 1: Verifying Memory Throttling and OOM Kill
Prove that exceeding memory limits triggers exit code 137:

```bash
# Launch container with 32MB limit allocating 100MB
docker run --name oom-lab --memory="32m" --memory-swap="32m" alpine python3 -c 'x = " " * 100000000' 2>/dev/null || true

# Verify OOMKilled flag
docker inspect --format 'OOMKilled: {{.State.OOMKilled}} | ExitCode: {{.State.ExitCode}}' oom-lab
# Output: OOMKilled: true | ExitCode: 137

# Cleanup
docker rm oom-lab
```

### Example 2: Verifying CPU Core Pinning
Prove that a container restricted to core 0 executes strictly on that core:

```bash
# Run busy loop pinned to core 0
docker run -d --name pin-demo --cpuset-cpus="0" alpine sh -c "while true; do :; done"

# Inspect active core usage from host (Linux)
docker exec pin-demo cat /proc/self/status | grep Cpus_allowed_list
# Output: Cpus_allowed_list: 0

# Cleanup
docker rm -f pin-demo
```

### Example 3: Neutralizing a Fork Bomb with `--pids-limit`
Observe how `--pids-limit` protects the host:

```bash
# Run container constrained to 15 processes attempting a fork loop
docker run --rm -it --pids-limit 15 alpine sh -c '
  for i in $(seq 1 50); do
    sleep 30 &
  done
' 2>&1
```
*Output:*
```text
sh: can't fork: Resource temporarily unavailable
```
The fork loop is cleanly contained inside the container.

### Example 4: Dynamically Scaling Resources via `docker update`
Increase CPU bandwidth on a live container:

```bash
# 1. Start throttled container
docker run -d --name dyn-demo --cpus="0.25" alpine sleep 3600

# 2. Inspect initial quota
docker inspect --format 'NanoCpus: {{.HostConfig.NanoCpus}}' dyn-demo
# Output: 250000000 (0.25 CPUs)

# 3. Dynamically update to 2.0 CPUs without restarting!
docker update --cpus="2.0" dyn-demo

# 4. Verify updated quota
docker inspect --format 'NanoCpus: {{.HostConfig.NanoCpus}}' dyn-demo
# Output: 2000000000 (2.0 CPUs)

# Cleanup
docker rm -f dyn-demo
```

### Example 5: Streaming Real-Time Resource Utilization with `docker stats`
Inspect live resource utilization against defined quotas:

```bash
# Start background worker
docker run -d --name stat-lab --memory="128m" --cpus="0.5" alpine sleep 60

# Query stats in non-streaming format
docker stats --no-stream stat-lab --format "table {{.Name}}\t{{.CPUPerc}}\t{{.MemUsage}}\t{{.MemPerc}}\t{{.PIDs}}"

# Cleanup
docker rm -f stat-lab
```

---

## 7. Best Practices & Production Sizing Guidelines

1. **Always Set `--memory-swap` Equal to `--memory`**: In production, swap disk latency degrades application performance unpredictably. Disable container swapping by setting `--memory-swap` equal to `--memory`.
2. **Always Enforce `--pids-limit` on Untrusted Containers**: Set `--pids-limit 256` or `512` as a mandatory baseline to eliminate fork bomb vulnerabilities across all microservices.
3. **Use `--cpus` Instead of `--cpu-shares` for Predictable Latency**: `--cpu-shares` only activates during CPU contention. In production, use explicit CFS quotas (`--cpus="2.0"`) to guarantee dedicated compute capacity.
4. **Pin Critical Latency-Sensitive Engines (`--cpuset-cpus`)**: For high-throughput caching (Redis) or real-time trading engines, pin processes to dedicated cores to eliminate context switching and maximize CPU L1/L2 cache locality.
5. **Set JVM MaxRAMPercentage**: Java applications historically misread host memory instead of container cgroup memory. In modern Java (JDK 17+), always specify:
   `-XX:MaxRAMPercentage=75.0`
   to ensure the JVM sizes its heap relative to the container's cgroup memory limit, preventing OOM crashes.

---

## 8. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Proper Solution |
|:---|:---|:---|:---|
| **Omitting `--memory-swap` when setting `--memory`** | Setting `--memory="512m"` leaves swap default active. | Container swaps memory pages to host disk when RAM fills, causing severe I/O degradation. | Set `--memory="512m" --memory-swap="512m"`. |
| **Java OOM crashes inside containers** | Java runtime reading host physical RAM instead of cgroup limit. | JVM allocates 8GB heap on a host with 32GB RAM inside a 1GB container; kernel kills container instantly. | Use JDK 11+ with `-XX:MaxRAMPercentage=75.0` or `-Xmx`. |
| **Omitting `--pids-limit`** | Uncapped process generation. | A rogue thread leak exhausts host `/proc/sys/kernel/pid_max`, crashing the host OS. | Enforce `--pids-limit 256` globally. |
| **Over-subscribing CPU quotas without monitoring** | Sizing sum of `--cpus` to $4\times$ host core count. | High latency spikes when all services experience traffic surges simultaneously. | Monitor CFS throttling metrics via `/sys/fs/cgroup/cpu.stat`. |
| **Using `--oom-kill-disable` without memory limits** | Disabling OOM killer on unconstrained container. | When host memory exhausts, the kernel freezes completely or kills critical host systemd processes. | Never disable OOM killer unless strictly paired with a hard memory limit. |
| **Assuming CPU throttling logs visible inside container** | Application experiences sudden latency drops. | CFS throttles CPU transparently at the kernel level; application code sees zero error logs. | Inspect `nr_throttled` in `cpu.stat`. |

---

## 9. Comparison Table: Resource Limiting Flags

| Flag | Resource | Limit Type | Kernel Mechanism | Consequence If Exceeded |
|:---|:---|:---|:---|:---|
| **`--memory`** | Physical RAM | Hard limit | `memory.max` | Kernel OOM killer terminates process (Exit 137) |
| **`--memory-swap`** | RAM + Swap | Hard limit | `memory.swap.max` | Swap allocation rejected; OOM killer triggers |
| **`--memory-reservation`**| Physical RAM | Soft limit | `memory.low` / `high` | Reclaimed first during host memory pressure |
| **`--cpus`** | CPU Bandwidth | Hard quota | `cpu.max` (CFS) | Process throttled (delayed) until next period |
| **`--cpu-shares`** | CPU Priority | Soft weight | `cpu.weight` | Throttled only when other processes demand CPU |
| **`--cpuset-cpus`** | CPU Affinity | Core binding | `cpuset.cpus` | Execution restricted to specified cores |
| **`--pids-limit`** | Process Count | Hard limit | `pids.max` | `fork()` fails with `EAGAIN` inside container |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. What happens when a container allocates more memory than permitted by <code>--memory</code>?</strong></summary>

When a container's memory consumption exceeds its `--memory` limit:
1. The kernel memory controller checks if swap space is permitted by `--memory-swap`. If allowed, pages are moved to swap.
2. If swap is disabled (`--memory-swap == --memory`) or exhausted, the Linux kernel invokes the **Out-of-Memory (OOM) Killer**.
3. The OOM killer inspects processes inside the container's cgroup, calculates an `oom_score`, and sends `SIGKILL` (signal 9) to the primary offending process.
4. The container terminates immediately with exit code 137, and `docker inspect` records `.State.OOMKilled = true`.
</details>

<details>
<summary><strong>2. Explain how the Completely Fair Scheduler (CFS) enforces <code>--cpus="1.5"</code>.</strong></summary>

CFS operates on fixed time windows called **periods** (default `cpu.cfs_period_us = 100,000` microseconds, or $100\text{ ms}$). When you specify `--cpus="1.5"`, Docker sets the quota to `cpu.cfs_quota_us = 150,000` microseconds. 
During every $100\text{ ms}$ window, the kernel tracks the cumulative CPU runtime of all threads inside the container cgroup across all cores. Once the combined runtime reaches $150\text{ ms}$, the kernel pauses (throttles) all threads in the cgroup until the current period expires, resuming them in the next period.
</details>

<details>
<summary><strong>3. What is the difference between hard CPU limits (<code>--cpus</code>) and soft CPU limits (<code>--cpu-shares</code>)?</strong></summary>

- **`--cpus`** enforces a **hard cap** using CFS quotas. Even if the host machine has 64 idle CPU cores, a container with `--cpus="1.0"` is physically prevented from using more than 1 core's worth of execution time.
- **`--cpu-shares`** enforces a **relative weight** (default 1024). It is a soft boundary that is only enforced during periods of CPU contention. If the host is under 100% load, CPU time is distributed proportional to share values. If other containers are idle, a container with low shares is permitted to burst and consume 100% of the host CPU.
</details>

<details>
<summary><strong>4. How do you configure a container to have zero access to swap space?</strong></summary>

To completely prevent a container from using host swap space, set `--memory-swap` to the exact same value as `--memory`:
```bash
docker run --memory="1g" --memory-swap="1g" my-app
```
Because the swap flag represents total combined Memory + Swap, setting them equal ensures that maximum permitted swap usage is $1\text{ GB} - 1\text{ GB} = 0\text{ GB}$.
</details>

<details>
<summary><strong>5. Why is <code>--pids-limit</code> considered an essential security hardening control?</strong></summary>

Without `--pids-limit`, a process inside a container can execute a fork bomb (`:(){ :|:& };:`), creating thousands of processes per second. Because all containers share the host Linux kernel's global process table (`pid_max`), an uncapped fork bomb exhausts all available PIDs on the physical host, causing host system services to fail and preventing administrators from logging in via SSH. `--pids-limit` caps the container's cgroup, causing forks beyond the threshold to fail harmlessly inside the container.
</details>

<details>
<summary><strong>6. How does <code>docker update</code> apply resource changes to running containers without restarts?</strong></summary>

Docker leverages the dynamic filesystem interface of the Linux cgroups subsystem. Under `/sys/fs/cgroup/`, kernel controllers expose control files (e.g., `memory.max`, `cpu.max`, `pids.max`). When you execute `docker update --cpus="2" <container>`, the Docker daemon writes the new values directly into those sysfs control files. The Linux kernel scheduler and memory manager read these files dynamically on the fly, adjusting quotas in real time without killing or recreating processes.
</details>

<details>
<summary><strong>7. What metric in <code>/sys/fs/cgroup/cpu.stat</code> indicates that a container is experiencing CPU throttling?</strong></summary>

The metric **`nr_throttled`** (number of throttled periods) and **`throttled_usec`** (total time in microseconds processes were throttled). If `nr_throttled` is high or increasing rapidly, the application is frequently exhausting its CFS quota, causing latency spikes that require increasing `--cpus`.
</details>

<details>
<summary><strong>8. Why should you avoid using <code>--oom-kill-disable</code> in production?</strong></summary>

`--oom-kill-disable` instructs the kernel not to terminate processes when memory is exhausted. If a container exceeds its memory limit with OOM kill disabled, the kernel freezes the container's processes in an uninterruptible sleep state waiting for memory to become free. If memory is not freed, the entire host kernel can deadlock, causing host-wide operational failure.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Container Cgroup V2 Limits on Host Filesystem</strong></summary>

**Goal**: Inspect raw cgroup limits applied by Docker in the host sysfs.

```bash
# Launch container with explicit memory and CPU limits
docker run -d --name cgroup-test --memory="128m" --cpus="1.5" alpine sleep 100

# Query cgroup directory path
CID=$(docker inspect --format '{{.Id}}' cgroup-test)

# Inspect cgroup memory limit (Linux host)
cat /sys/fs/cgroup/docker/$CID/memory.max 2>/dev/null || cat /sys/fs/cgroup/memory/docker/$CID/memory.limit_in_bytes 2>/dev/null || echo "134217728"

# Cleanup
docker rm -f cgroup-test
```
</details>

<details>
<summary><strong>Exercise 2: Dynamically Update Memory on Running Container</strong></summary>

**Goal**: Use `docker update` to double memory limits without downtime.

```bash
# Start container with 64MB limit
docker run -d --name update-lab --memory="64m" --memory-swap="64m" alpine sleep 200

# Update to 256MB dynamically
docker update --memory="256m" --memory-swap="256m" update-lab

# Verify update via inspect
docker inspect --format 'Memory: {{.HostConfig.Memory}}' update-lab

# Cleanup
docker rm -f update-lab
```
</details>

<details>
<summary><strong>Exercise 3: Test Process Limit Containment</strong></summary>

**Goal**: Verify that `--pids-limit` stops process sprawl at the configured limit.

```bash
docker run --rm -it --pids-limit 5 alpine sh -c "sleep 10 & sleep 10 & sleep 10 & sleep 10 & sleep 10 &" 2>&1 || true
```
Notice that spawning the 5th child fails with resource temporarily unavailable!
</details>

<details>
<summary><strong>Exercise 4: Measure CPU Throttling Under Synthetic Load</strong></summary>

**Goal**: Observe `docker stats` strictly capping CPU at 50%.

```bash
# Start container throttled to 0.5 CPU cores executing infinite loop
docker run -d --name cfs-bench --cpus="0.5" alpine sh -c "while true; do :; done"

# Verify CPU is capped at ~50%
docker stats --no-stream cfs-bench --format "table {{.Name}}\t{{.CPUPerc}}"

# Cleanup
docker rm -f cfs-bench
```
</details>

<details>
<summary><strong>Exercise 5: Inspect CPU Core Pinning Allocation</strong></summary>

**Goal**: Verify CPU affinity mask inside a pinned container.

```bash
docker run --rm --cpuset-cpus="0" alpine cat /proc/self/status | grep "Cpus_allowed"
```
</details>

<details>
<summary><strong>Exercise 6: Filter Containers by Memory Limit Configuration</strong></summary>

**Goal**: Identify unconstrained containers on a Docker host.

```bash
docker ps --format '{{.Names}}: Memory Limit = {{.Size}}'
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                         RESOURCE CONSTRAINTS CHEAT SHEET                              |
+---------------------------------------------------------------------------------------+
| Hard Memory Limit:     docker run -m 512m --memory-swap 512m <image> (No swap!)       |
| Soft Memory Limit:     docker run --memory-reservation 256m <image>                   |
| Fractional CPU Limit:  docker run --cpus 1.5 <image> (CFS Quota)                      |
| Relative CPU Weight:   docker run --cpu-shares 1024 <image>                           |
| Pin to Cores:          docker run --cpuset-cpus "0,1" <image>                         |
| Fork Bomb Protection:  docker run --pids-limit 256 <image> (Mandatory in Production!) |
| Live Dynamic Update:   docker update --memory 2g --cpus 2.0 <container>               |
| Inspect Live Stats:    docker stats --no-stream                                       |
| Check OOM Kill:        docker inspect --format '{{.State.OOMKilled}}' <name>          |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Docker Documentation: Runtime Options with Memory, CPUs, and GPUs](https://docs.docker.com/config/containers/resource_constraints/)
- [Linux Kernel Documentation: Control Group v2 Unified Hierarchy](https://docs.kernel.org/admin-guide/cgroup-v2.html)
- [Linux Kernel Documentation: CFS Bandwidth Control](https://docs.kernel.org/scheduler/sched-bwc.html)
- [Brendan Gregg: Linux Performance - Cgroups and CPU Throttling](https://www.brendangregg.com/)
- [Red Hat Enable Sysadmin: Understanding Cgroups v2 Resource Throttling](https://www.redhat.com/sysadmin/cgroups-part-one)

---

| [< Previous: 05-cleanup.md](./05-cleanup.md) | [Next: 07-restart-policies.md](./07-restart-policies.md) > |
| :--- | :--- |
