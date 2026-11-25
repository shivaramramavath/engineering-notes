<!--
================================================================================
Metadata:
  Title: Container Resource Management and OOM Handling
  Topic: Docker Production Engineering
  Difficulty: Advanced
  Estimated Time: 50 minutes
  Prerequisites: 01-fundamentals/06-linux-kernel-primitives.md, 02-commands/02-logs-exec-inspect.md
  Key Terminology: cgroups v1/v2, CFS (Completely Fair Scheduler), CPU Quota, Memory Limit, Swap, OOM Killer, oom_score_adj, Exit Code 137
================================================================================
-->

# Container Resource Management and OOM Handling

[<- 09-logging-drivers-and-monitoring.md](./09-logging-drivers-and-monitoring.md) | [Back to Module Overview](./README.md) | [11-docker-daemon-configuration-and-troubleshooting.md ->](./11-docker-daemon-configuration-and-troubleshooting.md)

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

By default, a Docker container has no resource constraints and can consume as much memory, CPU cycles, disk I/O, and file descriptors as the host operating system kernel allows. In multi-tenant host environments, uncontrolled containers create the classic "noisy neighbor" hazard:
1. A memory leak in one rogue application exhausts host RAM and triggers the Linux Out-Of-Memory (OOM) Killer, terminating arbitrary critical host daemons (e.g., `dockerd`, `sshd`, or shared databases).
2. An infinite loop consumes 100% of host CPU cores, starving sibling containers and causing health check timeouts across the entire platform.
3. Uncontrolled fork bombs spawn thousands of processes, exhausting kernel PID tables.

Configuring explicit resource limits ensures isolation, deterministic performance, predictable autoscaling, and graceful failure modes without jeopardizing host stability.

---

## 2. Core Concepts

### Linux Control Groups (cgroups)
Resource limits are enforced directly by the Linux kernel's **cgroups** subsystem:
- **cgroups v1**: Hierarchical control groups where each resource controller (memory, cpu, blkio, pids) resides in an independent directory tree (`/sys/fs/cgroup/memory`, `/sys/fs/cgroup/cpu`). Managing interdependent constraints across multiple trees caused synchronization deadlocks and accounting discrepancies.
- **cgroups v2**: A single unified hierarchy (`/sys/fs/cgroup`) introduced in Linux kernel 4.5+ and standard in modern distributions (Ubuntu 22.04+, RHEL 9+, Debian 11+). In v2, processes belong to exactly one unified node, providing precise, integrated tracking of memory, I/O writebacks, CPU throttling, and PSI (Pressure Stall Information).

### CPU Allocation Models
- **CFS (Completely Fair Scheduler) Bandwidth Control**: Hard upper ceiling enforced across microsecond periods (default `period = 100000us` or 100ms). Setting `--cpus="1.5"` allocates a quota of `150000us` per 100ms window.
- **CPU Shares (Relative Weighting)**: Soft priority weighting (default `1024`). When CPU contention occurs, cycles are apportioned in proportion to configured share ratios. If CPU is idle, a container with low shares can burst to 100%.
- **CPU Pinning (`cpuset`)**: Pinning container processes to physical or virtual cores (e.g., `--cpuset-cpus="0,1"`), bypassing scheduler context switching overhead.

### Memory Allocation Models
- **Hard Memory Limit (`--memory` or `-m`)**: Strict physical RAM ceiling. If container memory exceeds this limit, the Linux kernel invokes the OOM killer.
- **Memory + Swap (`--memory-swap`)**: Combined total of RAM plus swap space allowed.
- **Soft Memory Limit (`--memory-reservation`)**: Soft threshold. Docker attempts to keep container memory below this mark when host memory is constrained.
- **Kernel Memory & Memory Swappiness**: Controls aggressiveness of anonymous memory paging (`--memory-swappiness=0..100`).

### Linux Out-Of-Memory (OOM) Killer & Exit Code 137
When physical RAM and swap are exhausted, the Linux kernel calls `out_of_memory()`. The kernel scores every process (`oom_score`) based on memory consumption and administrative bias (`oom_score_adj`). The process with the highest score is sent `SIGKILL` (signal 9). When Docker detects a container killed by `SIGKILL` due to OOM, it sets container exit code to `137` ($128 + 9$).

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------+
|                               LINUX HOST KERNEL                                 |
|                                                                                 |
|  +---------------------------------------------------------------------------+  |
|  |                        UNIFIED CGROUPS V2 HIERARCHY                       |  |
|  |                           /sys/fs/cgroup/system.slice/                     |  |
|  |                                                                           |  |
|  |  +-----------------------------+       +-------------------------------+  |  |
|  |  |  Container A (Memory Limit) |       |  Container B (CPU Throttling) |  |  |
|  |  |  memory.max = 512MB         |       |  cpu.max = 100000 100000 (1C) |  |  |
|  |  |  memory.current = 510MB     |       |  cpu.stat: throttled_usec     |  |  |
|  |  +--------------+--------------+       +---------------+---------------+  |  |
|  +-----------------|--------------------------------------|------------------+  |
|                    |                                      |                     |
|         Allocation Request > 512MB            100ms Window Exhausted            |
|                    v                                      v                     |
|  +---------------------------------+      +-------------------------------+     |
|  |      Kernel Page Allocator      |      |   CFS Bandwidth Controller    |     |
|  |  Reclaim cached file pages      |      |  Throttle threads until next  |     |
|  |  If dirty/anon pages cannot free|      |  100ms CFS allocation period  |     |
|  +-----------------+---------------+      +-------------------------------+     |
|                    |                                                            |
|          Allocation Fails                                                       |
|                    v                                                            |
|  +---------------------------------+                                            |
|  |      KERNEL OOM KILLER          |                                            |
|  |  Reads /proc/[pid]/oom_score    |                                            |
|  |  Container PID has highest score|                                            |
|  |  Sends SIGKILL (kill -9)        |                                            |
|  +-----------------+---------------+                                            |
+--------------------|------------------------------------------------------------+
                     v
+---------------------------------------------------------------------------------+
|                             DOCKER ENGINE / CONTAINERD                          |
|                                                                                 |
|  1. containerd receives SIGCHLD from exited PID                                 |
|  2. containerd inspects exit event: ExitCode = 137                              |
|  3. Inspects cgroup event counter: oom_kill > 0                                 |
|  4. docker inspect reports:                                                     |
|       "State": { "OOMKilled": true, "ExitCode": 137, "Status": "exited" }       |
+---------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### CPU Constraints CLI Flags

```bash
# Hard CPU limit (CFS quota: 2.5 cores)
docker run -d --cpus="2.5" --name api-service my-api:latest

# Soft CPU shares (default 1024; 512 gives half priority under contention)
docker run -d --cpu-shares=512 --name background-worker worker:latest

# Pin to specific physical CPU cores (core 0 and core 2)
docker run -d --cpuset-cpus="0,2" --name latency-sensitive db:latest

# Pin to specific NUMA memory nodes (NUMA node 0)
docker run -d --cpuset-mems="0" --name numa-app compute:latest

# Explicit CFS quota and period (50ms period, 25ms quota = 50% of 1 core)
docker run -d --cpu-period=50000 --cpu-quota=25000 test-app:latest
```

### Memory Constraints CLI Flags

```bash
# Hard limit of 512MB RAM (swap defaults to 2x memory: 512MB RAM + 512MB swap)
docker run -d -m 512m --name web web:latest

# Disable swap entirely (set memory-swap equal to memory)
docker run -d -m 512m --memory-swap=512m --name noswap-web web:latest

# Set soft reservation (guaranteed floor / eviction trigger threshold)
docker run -d -m 1g --memory-reservation=512m db:latest

# Restrict container process table (PID limit to block fork bombs)
docker run -d --pids-limit=200 secure-worker:latest

# Adjust host OOM score preference (-1000 protects from kill, 1000 marks first target)
docker run -d --oom-score-adj=-500 critical-proxy:latest
```

---

## 5. Practical Examples

### Example 1: Docker Compose Production Resource Constraints (Deploy Block)

```yaml
# compose.prod.yml
services:
  web-api:
    image: company/web-api:v2.4.0
    restart: unless-stopped
    deploy:
      resources:
        limits:
          cpus: '2.0'
          memory: 1024M
          pids: 150
        reservations:
          cpus: '0.5'
          memory: 512M
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
      interval: 15s
      timeout: 5s
      retries: 3
    environment:
      - NODE_OPTIONS=--max-old-space-size=800

  cache:
    image: redis:7.4-alpine
    restart: unless-stopped
    command: ["redis-server", "--maxmemory", "200mb", "--maxmemory-policy", "allkeys-lru"]
    deploy:
      resources:
        limits:
          cpus: '1.0'
          memory: 256M
        reservations:
          cpus: '0.2'
          memory: 128M
```

### Example 2: Simulating OOMKill and Inspecting Diagnostic Markers

```bash
# 1. Run a container with 64MB hard memory limit and no swap
docker run -d --name oom-test -m 64m --memory-swap=64m python:3.12-alpine \
  python3 -c "b = bytearray(128 * 1024 * 1024)"

# 2. Wait 2 seconds and inspect exit status
docker ps -a --filter "name=oom-test"
# Output shows: STATUS Exited (137) 2 seconds ago

# 3. Inspect container metadata for OOM confirmation
docker inspect oom-test --format 'ExitCode: {{.State.ExitCode}} | OOMKilled: {{.State.OOMKilled}} | Error: {{.State.Error}}'
# Output: ExitCode: 137 | OOMKilled: true | Error:

# 4. Check kernel dmesg for OOM killer trace
dmesg -T | grep -E -i "oom[-_]killer|killed process"
```

### Example 3: Diagnosing Real-Time CPU Throttling

```bash
# 1. Start a CPU-constrained workload (1 core ceiling)
docker run -d --name throttle-demo --cpus="1.0" bash -c "while true; do :; done"

# 2. Inspect real-time container metrics
docker stats throttle-demo --no-stream
# Output shows CPU ~100.00%

# 3. Read Linux cgroup v2 cpu.stat directly from the host filesystem
CONTAINER_ID=$(docker inspect --format '{{.Id}}' throttle-demo)
cat /sys/fs/cgroup/system.slice/docker-${CONTAINER_ID}.scope/cpu.stat

# Sample output:
# usage_usec 14205812
# user_usec 14192000
# system_usec 13812
# nr_periods 302
# nr_throttled 215          <-- Number of periods where CPU was clamped
# throttled_usec 8920102    <-- Total time threads were suspended in microseconds
```

### Example 4: Configuring Java / JVM Heap with Container Limits

Modern Java runtimes (Java 11+, 17+, 21+) inspect cgroup limits automatically via `UseContainerSupport` (default on):

```dockerfile
# syntax=docker/dockerfile:1.7
FROM eclipse-temurin:21-jre-alpine

WORKDIR /app
COPY app.jar /app/app.jar

# -XX:MaxRAMPercentage dynamically sizes heap relative to cgroup memory limit
# 75% heap leaves 25% for off-heap, Metaspace, GC threads, and OS buffers
ENV JAVA_TOOL_OPTIONS="-XX:+UseContainerSupport \
                       -XX:MaxRAMPercentage=75.0 \
                       -XX:InitialRAMPercentage=50.0 \
                       -XX:+ExitOnOutOfMemoryError"

USER 10001:10001
ENTRYPOINT ["java", "-jar", "/app/app.jar"]
```

```bash
# Container limit = 1024MB -> JVM Max Heap automatically calculates to ~768MB
docker run -d -m 1024m --memory-swap=1024m my-java-app:latest
```

### Example 5: Defending Against Fork Bombs with `--pids-limit`

```bash
# Run malicious or unverified code with process limits
docker run --rm -it --pids-limit=50 alpine:latest sh

# Inside the container, test fork bomb:
# :(){ :|:& };:
# Output: sh: can't fork: Resource temporarily unavailable
# Host machine remains completely responsive and stable!
```

---

## 6. Best Practices

1. **Always Set Memory Limits and Disable Swap**: For production web applications, set `--memory-swap` equal to `--memory`. Swapping degraded memory to disk introduces unpredictable 500ms+ latency spikes into HTTP request handling.
2. **Leave Headroom for Runtimes**: Node.js (`--max-old-space-size`), Python, Go, and Java JVM have native memory overhead (native bindings, thread stacks, garbage collector allocations). Always set internal heap limits to 70–80% of the container hard memory limit.
3. **Always Set `--pids-limit`**: Prevent thread leaks and fork bombs from exhausting host process slots (`kernel.pid_max`). A limit of 100–300 pids is sufficient for most application containers.
4. **Monitor `nr_throttled` Rather Than Simple CPU%**: An application running at 80% CPU usage can experience 40% request throttling if burst spikes saturate the 100ms CFS window. Track throttling counters in Prometheus.
5. **Use Docker Compose Resource Reservations for Scheduling**: Use `limits` to prevent runaway consumption and `reservations` to guarantee baseline density.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Symptom | Root Cause | Diagnosis Command | Remediation |
|:------------------|:-----------|:------------------|:------------|
| **Container exits immediately with code 137** | Linux Kernel OOM Killer terminated PID 1 | `docker inspect <id> --format '{{.State.OOMKilled}}'` | Increase `-m` limit or optimize memory leaks in application code. |
| **High response latency despite low CPU%** | CFS CPU Throttling due to burst activity | Check `cpu.stat` inside cgroup for `nr_throttled` | Increase `--cpus` or lengthen `--cpu-period` to accommodate burst traffic. |
| **Node.js process crashes without OOMKilled=true** | V8 internal heap allocation limit hit before container limit | Inspect container logs: `FATAL ERROR: Ineffective mark-compacts near heap limit` | Configure `NODE_OPTIONS="--max-old-space-size=..."` matching container limit. |
| **Java ignores container limits and host dies** | Ancient JVM version (pre-Java 8u191) reading host physical RAM | `docker exec <id> java -XX:+PrintFlagsFinal -version \| grep MaxHeapSize` | Upgrade base image to Java 17/21 or set `-XX:+UseContainerSupport`. |
| **Host SSD thrashing and high IO wait** | Container paging memory to host swap disk | `vmstat 1` shows high `si`/`so` | Set `--memory-swap` equal to `--memory` to forbid swapping. |
| **Fork failure: "Cannot allocate memory"** | Process limit reached (`--pids-limit`) or host PID ceiling hit | Check `dmesg \| grep cgroup` for pid exhaustion | Increase `--pids-limit` or debug thread/subprocess leaks in container. |

---

## 8. Performance, Security & Cost Implications

### Performance
Enforcing hard CPU limits (`--cpus`) without profiling can inadvertently degrade throughput. When a container exceeds its quota within a 100ms window, the kernel deschedules threads for the remainder of that period, causing tail latency ($P99$) spikes. In latency-critical microservices, prefer soft priority weighting (`--cpu-shares`) combined with host-level horizontal scaling.

### Security
Containers without resource restrictions represent a major Denial of Service (DoS) attack surface. A malicious payload executing an in-memory decompression bomb (Zip Bomb) or thread multiplier can bring down an entire cloud compute node, affecting all colocated services.

### Cost
Properly tuned resource requests allow aggressive bin-packing on AWS EC2, Google Compute Engine, or bare-metal servers. Without resource reservations, operations teams over-provision host clusters by 400% to guard against unpredictable noisy neighbors.

---

## 9. Comparison Table

| Feature | `--cpus` (CFS Quota) | `--cpu-shares` (Weight) | `--cpuset-cpus` (Pinning) |
|:--------|:---------------------|:------------------------|:--------------------------|
| **Enforcement** | Hard ceiling per 100ms | Soft relative priority | Physical core lock |
| **Bursting Above Quota**| No | Yes (if host idle) | No (locked to core capacity)|
| **Context Switch Overhead**| Low | Low | Lowest (zero cache eviction) |
| **Ideal Workload** | Predictable HTTP APIs | Asynchronous background workers | Real-time gaming, high-throughput DBs |
| **Configuration Example**| `--cpus="1.5"` | `--cpu-shares=512` | `--cpuset-cpus="0,1"` |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>Q1: What does Docker exit code 137 signify, and how do you verify if the kernel OOM killer was responsible?</strong></summary>

Exit code 137 signifies termination by `SIGKILL` (signal 9, because $128 + 9 = 137$). While any external `docker kill` or operator `kill -9` emits code 137, an OOM kill specifically triggers the kernel OOM killer subsystem.
To verify:
1. Inspect container state: `docker inspect <container_id> --format '{{.State.OOMKilled}}'`. If true, the OOM killer was invoked.
2. Check host kernel message logs: `dmesg -T | grep -E -i "oom[-_]killer|killed process"`, which prints the specific process name, PID, requested pages, and total memory score at the exact moment of termination.
</details>

<details>
<summary><strong>Q2: Explain the architectural difference between Linux cgroups v1 and cgroups v2 regarding memory and I/O management.</strong></summary>

In cgroups v1, every resource controller (memory, blkio, cpu) was an independent hierarchy. A process could be in group `/memory/groupA` and `/blkio/groupB`. Because the controllers did not communicate, buffered I/O writebacks could not be attributed to the originating process in memory, causing disk throttling to fail for cached memory. In cgroups v2, a unified hierarchy is enforced: every process belongs to a single leaf node where memory, CPU, and blkio controllers work synchronously. Furthermore, cgroups v2 introduces PSI (Pressure Stall Information), measuring the exact percentage of time CPU, memory, and I/O are stalled waiting on kernel locks or page recovery.
</details>

<details>
<summary><strong>Q3: How do `--memory` and `--memory-swap` interact when configured together?</strong></summary>

The `--memory-swap` flag represents total allowed memory **plus** swap:
- `-m 500m --memory-swap 1g`: Container can use 500MB physical RAM and 500MB swap (total 1GB).
- `-m 500m --memory-swap 500m`: Swap is completely disabled for this container.
- `-m 500m` (without swap flag): Swap defaults to 2x memory (500MB RAM + 500MB swap = 1GB total).
- `-m 500m --memory-swap -1`: Container has 500MB RAM limit and unlimited access to host swap space.
</details>

<details>
<summary><strong>Q4: Why does a Node.js container exit with code 134 or 1 instead of 137 when experiencing a memory leak?</strong></summary>

By default on 64-bit systems, V8 sets its internal old generation heap limit to approximately 1.4GB to 2GB. If you run a container with `-m 4g`, Node.js will hit V8's internal allocation limit long before the container reaches 4GB. When V8 fails an internal allocation, it throws `FATAL ERROR: Ineffective mark-compacts near heap limit Allocation failed - JavaScript heap out of memory` and exits with SIGABRT (exit code 134) or exit code 1. To fix this, you must set `NODE_OPTIONS="--max-old-space-size=3200"` so V8 can utilize the available memory.
</details>

<details>
<summary><strong>Q5: What is CFS Bandwidth Throttling, and how can an application experience high latency when CPU utilization is only 50%?</strong></summary>

The Completely Fair Scheduler enforces CPU limits using a time period (typically 100ms). If a container has `--cpus="1.0"`, it receives a quota of 100ms CPU runtime per 100ms period. If an application runs 4 concurrent threads that burst simultaneously for 25ms, it consumes $4 \times 25\text{ms} = 100\text{ms}$ of CPU time in the first 25ms of the period. For the remaining 75ms of that window, the kernel completely suspends (throttles) all threads in the container. To an external metric scraper polling every 15 seconds, CPU utilization appears low, but individual requests experience 75ms latency pauses.
</details>

<details>
<summary><strong>Q6: How does the kernel `oom_score_adj` work and how can Docker configure it?</strong></summary>

The Linux kernel calculates `oom_score` (0 to 1000) for every process when RAM is exhausted, indicating how attractive the process is to kill. The administrator can adjust this via `/proc/[pid]/oom_score_adj` (-1000 to +1000). A value of -1000 disables OOM killing for that process completely, while +1000 makes it the first target. Docker exposes `--oom-score-adj`: setting `--oom-score-adj=-500` ensures a critical proxy container will be spared by the kernel in favor of other unconstrained host processes.
</details>

<details>
<summary><strong>Q7: How do you identify whether a Linux system is running cgroups v1 or cgroups v2?</strong></summary>

Execute `stat -fc %T /sys/fs/cgroup/`.
- If the output is `cgroup2fs`, the host is running unified cgroups v2.
- If the output is `tmpfs`, the host is running legacy cgroups v1.
Alternatively, `docker info --format '{{.CgroupVersion}}'` outputs either `1` or `2`.
</details>

<details>
<summary><strong>Q8: Can `--oom-kill-disable` be used without setting `--memory`? What are the consequences?</strong></summary>

No. Enabling `--oom-kill-disable` without setting `-m / --memory` can crash the entire host machine. If memory is exhausted and the kernel is forbidden from killing the runaway process, the kernel cannot reclaim memory pages to service kernel allocations, causing a kernel panic or complete system lockup. Docker requires `-m` to be set if `--oom-kill-disable` is specified.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect host cgroup version and container limits via sysfs</strong></summary>

**Task**: Write commands to verify host cgroup version and directly read a running container's memory ceiling from `/sys/fs/cgroup`.

**Solution**:
```bash
# 1. Check Docker cgroup version
docker info --format 'Cgroup Version: {{.CgroupVersion}}'

# 2. Start a container with 256MB memory limit
docker run -d --name cgroup-check -m 256m alpine sleep 3600

# 3. Retrieve container Full ID
CID=$(docker inspect --format '{{.Id}}' cgroup-check)

# 4. Read memory limit directly from kernel cgroup filesystem:
# For cgroups v2:
cat /sys/fs/cgroup/system.slice/docker-${CID}.scope/memory.max
# Expected output: 268435456 (256 * 1024 * 1024 bytes)

# For cgroups v1:
# cat /sys/fs/cgroup/memory/docker/${CID}/memory.limit_in_bytes

docker rm -f cgroup-check
```
</details>

<details>
<summary><strong>Exercise 2: Induce and analyze an OOMKilled container</strong></summary>

**Task**: Launch a container that allocates 150MB of RAM with a 100MB limit. Extract the exact timestamp and exit condition using `docker inspect`.

**Solution**:
```bash
# 1. Run Python script allocating 150MB bytearray inside a 100MB container
docker run --name mem-crash -m 100m --memory-swap=100m python:3.12-alpine \
  python3 -c "import time; time.sleep(1); x = bytearray(150 * 1024 * 1024)"

# 2. Extract exit details
docker inspect mem-crash --format '
Exit Code: {{.State.ExitCode}}
OOM Killed: {{.State.OOMKilled}}
Finished At: {{.State.FinishedAt}}
'
# Cleanup
docker rm mem-crash
```
</details>

<details>
<summary><strong>Exercise 3: Pinning a container to core 0 with verification</strong></summary>

**Task**: Launch an alpine container pinned strictly to CPU core 0. Verify using `/proc/self/status` inside the container.

**Solution**:
```bash
# 1. Run container pinned to core 0
docker run --rm --cpuset-cpus="0" alpine grep Cpus_allowed_list /proc/self/status

# Output:
# Cpus_allowed_list:	0
```
</details>

<details>
<summary><strong>Exercise 4: Configuring Docker Compose for memory safety</strong></summary>

**Task**: Write a `compose.yaml` snippet running a Redis server with a 256MB hard limit, 128MB reservation, and no swap.

**Solution**:
```yaml
services:
  redis:
    image: redis:7.4-alpine
    deploy:
      resources:
        limits:
          memory: 256M
        reservations:
          memory: 128M
    # In Compose v2, to forbid swap use host config or memswap_limit
    memswap_limit: 256M
```
</details>

<details>
<summary><strong>Exercise 5: Real-time container metrics scripting</strong></summary>

**Task**: Write a bash one-liner that continuously prints the container name, CPU percentage, and memory usage formatted as human-readable columns.

**Solution**:
```bash
docker stats --format "table {{.Name}}\t{{.CPUPerc}}\t{{.MemUsage}}\t{{.NetIO}}\t{{.BlockIO}}"
```
</details>

<details>
<summary><strong>Exercise 6: Preventing container fork bombs</strong></summary>

**Task**: Demonstrate that `--pids-limit` protects the host against a rogue process spawning 500 background sleep loops.

**Solution**:
```bash
# Run with limit of 30 PIDs
docker run --rm --pids-limit=30 alpine sh -c '
for i in $(seq 1 100); do
  sleep 100 &
done
wait
'
# Result: Process will fail to fork around PID 30 with "sh: can't fork: Resource temporarily unavailable"
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

### Common Resource Flags

```bash
# CPU Controls
--cpus="1.5"             # Hard quota: 1.5 CPU cores per CFS period
--cpu-shares=512         # Relative priority weight (default 1024)
--cpuset-cpus="0,2,3"    # Lock execution to physical cores 0, 2, and 3
--cpu-period=100000      # CFS scheduler period in microseconds (default 100ms)
--cpu-quota=50000        # Max microseconds allowed per period

# Memory Controls
-m, --memory=512m        # Hard RAM ceiling (exceeding triggers OOM)
--memory-swap=512m       # Total RAM + Swap (setting equal to RAM disables swap)
--memory-reservation=256m# Soft limit / reservation for host reclaim trigger
--oom-score-adj=-500     # Host OOM killer priority bias (-1000 to 1000)
--oom-kill-disable       # Disable OOM killer (requires -m, risky!)

# Process Controls
--pids-limit=200         # Maximum concurrent processes/threads inside container
```

### Exit Codes Reference
- **137**: Process terminated by `SIGKILL` ($128 + 9$). Caused by `docker stop` (after grace period timeout), `docker kill`, or **Kernel OOM Killer**.
- **134**: Process terminated by `SIGABRT` ($128 + 6$). Common when JVM or Node.js internal memory allocators abort prior to kernel OOM.
- **143**: Process terminated by `SIGTERM` ($128 + 15$). Graceful shutdown signal.

---

## 13. Further Reading & Official Documentation

- [Docker Documentation: Runtime Options with Memory, CPUs, and GPUs](https://docs.docker.com/engine/containers/resource_constraints/)
- [Linux Kernel Documentation: Control Group v2](https://docs.kernel.org/admin-guide/cgroup-v2.html)
- [Kernel Documentation: Completely Fair Scheduler (CFS)](https://docs.kernel.org/scheduler/sched-bwc.html)
- [Brendan Gregg: Linux Systems Performance Analysis](https://www.brendangregg.com/)
- [Docker Documentation: Specify resources in Compose](https://docs.docker.com/compose/compose-file/deploy/#resources)

---

[<- 09-logging-drivers-and-monitoring.md](./09-logging-drivers-and-monitoring.md) | [Back to Module Overview](./README.md) | [11-docker-daemon-configuration-and-troubleshooting.md ->](./11-docker-daemon-configuration-and-troubleshooting.md)
