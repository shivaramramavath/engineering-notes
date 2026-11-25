---
title: "Container Lifecycle and State Machine: States, Transitions, and Exit Codes"
description: "Mastering the Docker container lifecycle: the 7 container states (created, running, paused, restarting, removing, exited, dead), state transitions, and exit code forensics."
category: "DevOps / Containers"
tags: ["docker", "lifecycle", "state-machine", "exit-codes", "signals", "oom", "troubleshooting"]
difficulty: "Intermediate"
prerequisites: ["01-docker-vs-vm.md", "02-images-and-containers.md", "Basic Linux signal handling"]
estimated_read_time: "25 min"
---

# Container Lifecycle and State Machine: States, Transitions, and Exit Codes

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. The 7 Container States](#2-the-7-container-states)
- [3. The Container State Machine & Transitions](#3-the-container-state-machine--transitions)
- [4. Container Exit Codes Forensics](#4-container-exit-codes-forensics)
- [5. Practical Diagnostic Demonstrations](#5-practical-diagnostic-demonstrations)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table: Pause vs Stop vs Kill](#9-comparison-table-pause-vs-stop-vs-kill)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

A Docker container is not a static object; it is a dynamic process managed by a formal finite state machine inside `containerd` and `dockerd`. At any given microsecond, a container occupies exactly one of seven distinct lifecycle states.

When production microservices fail, platform engineers must immediately determine:
- Did the application terminate cleanly with an intentional shutdown (Exit code 0)?
- Did the application throw an unhandled exception or crash (Exit code 1)?
- Did the missing binary or typo in the entrypoint prevent initialization (Exit code 127)?
- Did the Linux kernel Out-of-Memory (OOM) killer forcefully murder the process (Exit code 137)?
- Did the orchestrator gracefully drain the pod (Exit code 143)?

Mastering the container state machine and deciphering Linux exit codes is the baseline skill required for automated self-healing, healthcheck probes, and zero-downtime deployment pipelines.

```
+---------------------------------------------------------------------------------+
|                       CONTAINER FINITE STATE MACHINE                            |
|                                                                                 |
|                        +---------------+                                        |
|                        | [Non-Existent]|                                        |
|                        +-------+-------+                                        |
|                                |                                                |
|             docker create      |      docker run                                |
|             +------------------+---------------------+                          |
|             |                                        |                          |
|             v                                        v                          |
|      +-------------+        docker start       +-------------+                  |
|      |   CREATED   | ------------------------> |   RUNNING   | <-------+        |
|      +-------------+                           +------+------+         |        |
|             |                                    |    |                |        |
|             | docker rm            docker pause  |    | docker restart |        |
|             v                                    v    |                |        |
|      +-------------+                      +--------+  |   docker       |        |
|      | [Destroyed] |                      | PAUSED |  |   unpause      |        |
|      +-------------+                      +----+---+  |                |        |
|             ^                                  |      +----------------+        |
|             |                             kill |                                |
|             | docker rm                        v                                |
|             |                            +-------------+                        |
|             +--------------------------- |   EXITED    | <----------------------+
|             |                            +------+------+   (Process terminates, |
|             |                                   |          stop, kill, OOM)     |
|             |                                   |                               |
|             | Unrecoverable error               | Crash loop restart            |
|             v                                   v                               |
|      +-------------+                     +-------------+                        |
|      |    DEAD     |                     | RESTARTING  |                        |
|      +-------------+                     +-------------+                        |
+---------------------------------------------------------------------------------+
```

---

## 2. The 7 Container States

The Docker Engine API defines seven mutually exclusive lifecycle states:

### 1. `created`
The container has been initialized: storage layers (`UpperDir`) are allocated, network endpoints are reserved, and the OCI `config.json` bundle is written to disk. However, no process has been spawned. The primary application binary has not executed.

### 2. `running`
The container's primary process (PID 1) is actively executing on the host CPU. Namespaces and cgroups are actively enforced. The container is responding to network packets, timers, or I/O.

### 3. `paused`
The container's processes are frozen in memory using the Linux **cgroup freezer subsystem**. The processes consume zero CPU cycles, but all allocated memory pages remain locked in host RAM. Network sockets remain open, but TCP ACKs and incoming data buffers are held until resumed.

### 4. `restarting`
The container has exited (typically due to a crash or error), and Docker is actively applying a restart policy (`--restart on-failure` or `--restart always`) to reboot the process.

### 5. `removing`
The container is in the middle of being torn down. `dockerd` is disconnecting virtual network interfaces, unmounting OverlayFS layers, and cleaning up state files.

### 6. `exited`
The container process has terminated. The host PID no longer exists. However, the container's writable filesystem layer (`UpperDir`), configuration, metadata, and final exit code are preserved on disk.

### 7. `dead`
A rare error state where the container process has terminated, but the daemon failed to cleanly unmount the filesystem or release kernel resources (e.g., due to an unkillable D-state I/O lock or busy mount). The container cannot be restarted and must be force-removed.

---

## 3. The Container State Machine & Transitions

### Command-Driven Transitions:
- `docker create <image>`: $\text{Non-Existent} \longrightarrow \text{created}$
- `docker start <container>`: $\text{created} \mid \text{exited} \longrightarrow \text{running}$
- `docker run <image>`: $\text{Non-Existent} \longrightarrow \text{created} \longrightarrow \text{running}$ (Atomic invocation)
- `docker pause <container>`: $\text{running} \longrightarrow \text{paused}$
- `docker unpause <container>`: $\text{paused} \longrightarrow \text{running}$
- `docker stop <container>`: $\text{running} \longrightarrow \text{exited}$ (Sends `SIGTERM`, waits 10s grace period, sends `SIGKILL`)
- `docker kill <container>`: $\text{running} \longrightarrow \text{exited}$ (Sends immediate `SIGKILL`)
- `docker restart <container>`: $\text{running} \longrightarrow \text{exited} \longrightarrow \text{running}$
- `docker rm <container>`: $\text{created} \mid \text{exited} \mid \text{dead} \longrightarrow \text{Non-Existent}$

---

## 4. Container Exit Codes Forensics

When a container enters the `exited` state, its process returns an integer exit code. Exit codes follow standard POSIX and Linux conventions:

```
+---------------------------------------------------------------------------------+
|                         EXIT CODE REFERENCE TABLE                               |
+-----------+-------------------------+-------------------------------------------+
| EXIT CODE | COMMON MEANING          | DIAGNOSTIC ROOT CAUSE                     |
+-----------+-------------------------+-------------------------------------------+
| 0         | Success / Clean Exit    | Process completed its work (e.g. batch job|
|           |                         | finished successfully).                   |
+-----------+-------------------------+-------------------------------------------+
| 1         | General Application     | Uncaught exception, Python stack trace,   |
|           | Error                   | Node.js crash, missing configuration.     |
+-----------+-------------------------+-------------------------------------------+
| 2         | Misuse of Shell Builtin | Incorrect syntax in entrypoint script or  |
|           |                         | invalid command-line flags.               |
+-----------+-------------------------+-------------------------------------------+
| 126       | Command Cannot Execute  | Entrypoint binary lacks executable (+x)   |
|           |                         | permissions or architecture mismatch.     |
+-----------+-------------------------+-------------------------------------------+
| 127       | File / Command Not Found| Typo in entrypoint or missing dynamic     |
|           |                         | shared library (e.g., glibc on Alpine).   |
+-----------+-------------------------+-------------------------------------------+
| 128 + N   | Fatal Signal N          | Process terminated by Linux kernel signal |
+-----------+-------------------------+-------------------------------------------+
| 137       | Fatal Signal 9 (SIGKILL)| Forcefully murdered: either manually      |
| (128 + 9) | or OOM-Killed           | via `docker kill` or by Host Kernel OOM!  |
+-----------+-------------------------+-------------------------------------------+
| 139       | Fatal Signal 11(SIGSEGV)| Segmentation Fault: invalid memory read/  |
| (128 + 11)|                         | write in C/C++/Go native extension.       |
+-----------+-------------------------+-------------------------------------------+
| 143       | Fatal Signal 15(SIGTERM)| Clean termination signal sent by Docker   |
| (128 + 15)|                         | or Kubernetes orchestrator.               |
+-----------+-------------------------+-------------------------------------------+
```

### The 137 Detective: Distinguishing Manual Kill from Out-of-Memory (OOM)
When a container exits with code 137:
- Did an engineer run `docker kill`?
- OR did the container hit its memory quota and get murdered by the Linux OOM killer?
**Always check the metadata**:
```bash
docker inspect --format 'OOMKilled: {{.State.OOMKilled}} | ExitCode: {{.State.ExitCode}}' <container>
```
If `OOMKilled: true`, the host kernel terminated the process because it exceeded its `--memory` limit.

---

## 5. Practical Diagnostic Demonstrations

### Example 1: Stepping Through `created` to `running`
Observe the distinct transition from `created` to `running`:

```bash
# 1. Create container without starting it
docker create --name step-demo -p 8089:80 nginx:alpine

# 2. Inspect state
docker inspect --format 'State: {{.State.Status}} | PID: {{.State.Pid}}' step-demo
# Output: State: created | PID: 0 (No host PID allocated yet!)

# 3. Start container
docker start step-demo

# 4. Inspect state again
docker inspect --format 'State: {{.State.Status}} | PID: {{.State.Pid}}' step-demo
# Output: State: running | PID: 19482 (Host PID is now live!)

# Cleanup
docker rm -f step-demo
```

### Example 2: Freezing and Resuming with `docker pause`
Demonstrate process suspension using the cgroup freezer:

```bash
# 1. Run counter in background
docker run -d --name pause-demo alpine sh -c 'i=0; while true; do echo $i; i=$((i+1)); sleep 1; done'

# 2. Monitor logs for 3 seconds
sleep 3 && docker logs --tail 3 pause-demo

# 3. Freeze container processes
docker pause pause-demo
docker inspect --format '{{.State.Status}}' pause-demo
# Output: paused

# 4. Wait 5 seconds; notice that log generation is completely frozen!
sleep 5
docker logs --tail 3 pause-demo

# 5. Unpause container; counter immediately resumes!
docker unpause pause-demo
sleep 2 && docker logs --tail 3 pause-demo

# Cleanup
docker rm -f pause-demo
```

### Example 3: Simulating and Diagnosing Exit Code 127 (Command Not Found)
Diagnose an entrypoint error where a script is missing:

```bash
# Attempt to run a non-existent binary
docker run --name exit-127-demo alpine /bin/non_existent_binary 2>&1 || true

# Inspect the recorded exit code
docker inspect --format 'ExitCode: {{.State.ExitCode}} | Error: {{.State.Error}}' exit-127-demo
# Output: ExitCode: 127

# Cleanup
docker rm exit-127-demo
```

### Example 4: Diagnosing Out-of-Memory (OOM) Exit Code 137
Observe the kernel OOM killer in action:

```bash
# Run container constrained to 20MB RAM allocating 100MB
docker run --name oom-lab-demo --memory="20m" alpine sh -c "python3 -c 'a = \"x\" * 100000000'" 2>/dev/null || true

# Inspect OOM forensic flags
docker inspect --format 'Status: {{.State.Status}} | ExitCode: {{.State.ExitCode}} | OOMKilled: {{.State.OOMKilled}}' oom-lab-demo
# Output: Status: exited | ExitCode: 137 | OOMKilled: true

# Cleanup
docker rm oom-lab-demo
```

### Example 5: Testing Automatic Restart Policies
Observe the `restarting` state during backoff retry loops:

```bash
# Launch a failing container configured to restart on failure up to 3 times
docker run -d --name restart-demo --restart on-failure:3 alpine sh -c "sleep 1; exit 1"

# Query state repeatedly over 6 seconds
for i in 1 2 3 4; do
  docker inspect --format 'Status: {{.State.Status}} | RestartCount: {{.RestartCount}}' restart-demo
  sleep 2
done

# Cleanup
docker rm -f restart-demo
```

---

## 6. Best Practices & Production Guidelines

1. **Always Handle `SIGTERM` Gracefully in Application Code**: When `docker stop` is called, Docker sends `SIGTERM`. Ensure your application catches `SIGTERM`, stops accepting new HTTP connections, drains in-flight database transactions, and exits with code 0 within 10 seconds.
2. **Configure Realistic Stop Timeouts**: If your application requires more than 10 seconds to flush dirty buffers to disk (e.g., Elasticsearch, Postgres, Kafka), override the default timeout: `docker stop --time 30 <container>` or specify `STOPSIGNAL` and `stop_grace_period` in Compose.
3. **Never Rely on `--restart always` for Crashing Containers**: An infinite crash loop consumes CPU cycles and fills log disks. Use `--restart on-failure:5` with exponential backoff in orchestrators.
4. **Automate Dead Container Cleanup**: Dead containers retain open volume locks and unmounted filesystem handles. Ensure monitoring alerts fire if any container enters the `dead` state.
5. **Always Inspect `.State.OOMKilled` on Exit Code 137**: Never assume code 137 is a manual stop. In automated CI/CD and production monitoring, always check the `OOMKilled` boolean attribute.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Symptom | Root Cause | Proper Fix |
|:---|:---|:---|:---|
| **Ignoring `SIGTERM` in PID 1 process** | Container always takes exactly 10 seconds to stop, then exits with code 137. | App process does not register a `SIGTERM` handler; Docker times out and issues `SIGKILL`. | Catch `SIGTERM` in app code or use an init system like `tini` or `dumb-init`. |
| **Confusing Exit Code 137 with Exit Code 143** | Assuming all terminations are crashes. | 143 is a clean graceful exit via `SIGTERM` ($128+15$); 137 is a forced kill ($128+9$). | Treat 143 as expected orchestrator scale-down; treat 137 as an OOM or forced kill. |
| **Using `docker pause` on database containers** | Database locks up or cluster nodes consider the paused node dead. | TCP connections remain open but heartbeats stop; cluster failover triggers split-brain. | Stop or drain database nodes gracefully; do not freeze databases with `pause`. |
| **Container stuck in `removing` state** | `docker rm -f` hangs indefinitely. | Kernel driver is waiting on hung NFS/CIFS network storage or unkillable D-state I/O process. | Unmount stuck NFS mounts on host or reboot host kernel if D-state persists. |
| **Hardcoding `exit 1` in entrypoint scripts on success** | Successful batch scripts marked as failures by CI/CD. | Developer used non-zero exit code to terminate script. | Always terminate successful scripts with `exit 0`. |
| **Accumulating thousands of `exited` containers** | Host disk gradually fills up over weeks of execution. | Developers not passing `--rm` flag to one-off utility containers. | Run `docker container prune -f` in daily cron jobs. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Zero Resource Consumption in `exited` State**: Exited containers consume zero CPU and zero RAM. However, they retain their `UpperDir` storage layer on disk.
- **`pause` vs `stop` Latency**: `docker pause` freezes processes in less than $1\text{ ms}$ (kernel cgroup freeze). Resuming takes $<1\text{ ms}$. `docker stop` requires process teardown and restarts require process bootup ($100-2000\text{ ms}$).

### Security
- **Zombies and Leaked State**: Abandoned `exited` containers may contain sensitive in-memory data that was dumped to disk during crashes. Purge exited containers containing sensitive data immediately.

### Cost
- In cloud container services (AWS ECS Fargate, Google Cloud Run), billing ceases the instant a container enters the `exited` state. Failing to terminate hung or idling containers incurs unnecessary cloud costs.

---

## 9. Comparison Table: Pause vs Stop vs Kill

| Action | Command | Kernel Mechanism | CPU Usage | RAM Usage | Network Sockets | Recovery Speed |
|:---|:---|:---|:---|:---|:---|:---|
| **Pause** | `docker pause` | cgroup freezer | 0% | Retained in RAM | Kept Open | Instant ($< 1\text{ ms}$) |
| **Stop** | `docker stop` | `SIGTERM` $\to$ wait $\to$ `SIGKILL` | 0% | Released to Host | Closed | Moderate (Re-exec app) |
| **Kill** | `docker kill` | Immediate `SIGKILL` (Signal 9) | 0% | Released to Host | Abruptly Severed | Moderate (Re-exec app) |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. What is the difference between <code>docker stop</code> and <code>docker kill</code>?</strong></summary>

- `docker stop` initiates a **graceful shutdown**. It sends `SIGTERM` (signal 15) to PID 1 inside the container, granting it a configurable timeout (default 10 seconds) to finish active requests, flush database buffers, and exit cleanly. If the container does not terminate within the grace period, it sends `SIGKILL`.
- `docker kill` initiates an **immediate termination**. It sends `SIGKILL` (signal 9) directly to PID 1 with zero grace period. The kernel immediately destroys the process table entry without allowing the application to execute cleanup logic.
</details>

<details>
<summary><strong>2. How do you mathematically calculate the signal number from an exit code greater than 128?</strong></summary>

In POSIX operating systems, when a process is terminated by a fatal signal, the exit code is calculated as:
$$\text{Exit Code} = 128 + N$$
Where $N$ is the Linux signal number.
For example:
- Exit code 137: $137 - 128 = 9 \implies \text{Signal 9 (SIGKILL)}$.
- Exit code 143: $143 - 128 = 15 \implies \text{Signal 15 (SIGTERM)}$.
- Exit code 139: $139 - 128 = 11 \implies \text{Signal 11 (SIGSEGV - Segmentation Fault)}$.
</details>

<details>
<summary><strong>3. What occurs under the hood when a container enters the <code>paused</code> state?</strong></summary>

Docker uses the Linux **cgroup freezer subsystem** (`cgroup.freeze` in cgroups v2 or `freezer.state` in cgroups v1). The kernel halts the scheduling of all tasks inside that cgroup. The processes are frozen in their current execution state: no CPU cycles are consumed, and no instructions are executed. However, process memory pages remain locked in RAM, and open file descriptors and TCP sockets remain open in the kernel network stack.
</details>

<details>
<summary><strong>4. Why would a container enter the <code>dead</code> state, and how do you resolve it?</strong></summary>

A container enters the `dead` state when its process has terminated, but the Docker daemon cannot finish removing or unmounting its resources. This typically occurs when a backing storage driver has busy mount points, an external volume driver (like NFS) hangs, or a process is stuck in an uninterruptible sleep state (Linux "D state") waiting on failed disk I/O. To resolve it, administrators must unmount stuck mounts on the host, resolve I/O locks, and execute `docker rm -f <container>`.
</details>

<details>
<summary><strong>5. What does Exit Code 126 signify in Docker?</strong></summary>

Exit Code 126 signifies **"Command Invoked Cannot Execute"**. This means the file specified in `ENTRYPOINT` or `CMD` was found by the kernel, but cannot be executed. Common causes include:
1. The file lacks execute permissions (`chmod +x`).
2. The binary was compiled for a different CPU architecture (e.g., executing an x86_64 binary on an ARM64 host without emulation).
3. The binary requires dynamic linker libraries that are incompatible with the host architecture.
</details>

<details>
<summary><strong>6. What does Exit Code 127 signify in Docker?</strong></summary>

Exit Code 127 signifies **"File or Command Not Found"**. This means the path specified in `ENTRYPOINT` or `CMD` does not exist inside the container's rootfs. In Alpine-based containers, this also occurs frequently when attempting to execute a binary compiled for `glibc` (like standard Node or Go binaries) inside an Alpine container that only provides `musl libc`.
</details>

<details>
<summary><strong>7. How can you query only containers that have exited with an error code using the Docker CLI?</strong></summary>

You can filter containers using the `--filter` flag and inspect exit codes:
```bash
docker ps -a --filter "status=exited" --filter "exited=1"
```
Or format output using Go templates:
```bash
docker ps -a --format '{{if ne .State.ExitCode 0}}{{.Names}}: {{.State.ExitCode}}{{end}}'
```
</details>

<details>
<summary><strong>8. What is the difference between restart policy <code>always</code> and <code>unless-stopped</code>?</strong></summary>

- `--restart always`: The daemon will automatically restart the container whenever it exits (regardless of exit code). If the Docker daemon itself restarts or the physical host reboots, the container will automatically be started on boot, even if it was manually stopped by an administrator prior to the reboot.
- `--restart unless-stopped`: Behaves identically to `always`, with one critical distinction: if the container was in an explicitly **stopped** state at the moment the Docker daemon or physical host reboots, Docker will **not** automatically restart it upon system boot.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Detailed State Object Using Go Templates</strong></summary>

**Goal**: Extract the complete runtime state JSON object of a running container.

```bash
# Start container
docker run -d --name state-lab alpine sleep 100

# Inspect full State sub-tree
docker inspect --format '{{json .State}}' state-lab | python3 -m json.tool

# Cleanup
docker rm -f state-lab
```
</details>

<details>
<summary><strong>Exercise 2: Simulate and Catch Exit Code 126 (Permission Denied)</strong></summary>

**Goal**: Observe exit code 126 when executing a script lacking execute permissions.

```bash
# Attempt to run a shell script without +x
docker run --name exit-126-demo alpine sh -c "echo 'echo hi' > /run.sh && /run.sh" 2>&1 || true

# Inspect the resulting exit code
docker inspect --format 'ExitCode: {{.State.ExitCode}}' exit-126-demo
# Output shows ExitCode: 126

# Cleanup
docker rm exit-126-demo
```
</details>

<details>
<summary><strong>Exercise 3: Test Stop Timeout Override</strong></summary>

**Goal**: Observe Docker sending `SIGTERM`, waiting for an explicit timeout, and then sending `SIGKILL`.

```bash
# Run container that traps and ignores SIGTERM
docker run -d --name stubborn-app alpine sh -c "trap '' TERM; while true; do sleep 1; done"

# Time how long it takes to stop with a 3-second timeout
time docker stop --time 3 stubborn-app

# Inspect exit code (will be 137 because SIGKILL was forced)
docker inspect --format 'ExitCode: {{.State.ExitCode}}' stubborn-app

# Cleanup
docker rm stubborn-app
```
</details>

<details>
<summary><strong>Exercise 4: Verify Restart Count Tracking</strong></summary>

**Goal**: Inspect how the Docker daemon increments the restart counter.

```bash
# Start container with on-failure restart policy
docker run -d --name count-demo --restart on-failure:2 alpine sh -c "sleep 1; exit 1"

sleep 4
docker inspect --format 'RestartCount: {{.RestartCount}} | Status: {{.State.Status}}' count-demo

# Cleanup
docker rm -f count-demo
```
</details>

<details>
<summary><strong>Exercise 5: Inspect StartedAt and FinishedAt Timestamps</strong></summary>

**Goal**: Calculate execution duration using container lifecycle timestamps.

```bash
# Run batch job
docker run --name time-demo alpine sleep 2

# Query timestamps
docker inspect --format 'Started: {{.State.StartedAt}} | Finished: {{.State.FinishedAt}}' time-demo

# Cleanup
docker rm time-demo
```
</details>

<details>
<summary><strong>Exercise 6: Filter Containers by Lifecycle State</strong></summary>

**Goal**: Use CLI filters to isolate containers in paused or exited states.

```bash
docker ps -a --filter "status=paused"
docker ps -a --filter "status=exited"
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                         CONTAINER LIFECYCLE CHEAT SHEET                               |
+---------------------------------------------------------------------------------------+
| 7 States:              created, running, paused, restarting, removing, exited, dead   |
| Freeze / Unfreeze:     docker pause <name> / docker unpause <name>                    |
| Graceful Shutdown:     docker stop --time <sec> <name> (SIGTERM -> SIGKILL)           |
| Immediate Murder:      docker kill <name> (SIGKILL)                                   |
| Exit Code 0:           Clean, successful completion                                   |
| Exit Code 1:           Application crash / uncaught exception                         |
| Exit Code 126:         Command cannot execute (permission / arch mismatch)            |
| Exit Code 127:         Command or dynamic library not found                           |
| Exit Code 137:         Killed by Signal 9 (SIGKILL) or Host Kernel OOM Killer!        |
| Exit Code 143:         Gracefully stopped by Signal 15 (SIGTERM)                      |
| Check OOM:             docker inspect --format '{{.State.OOMKilled}}' <name>          |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Docker Documentation: Container Lifecycle & State Reference](https://docs.docker.com/engine/reference/commandline/ps/)
- [Linux Signal Architecture Manual Page (man 7 signal)](https://man7.org/linux/man-pages/man7/signal.7.html)
- [Linux Out-of-Memory (OOM) Killer Kernel Documentation](https://docs.kernel.org/admin-guide/mm/concepts.html)
- [Linux Control Groups v2: The Freezer Subsystem](https://docs.kernel.org/admin-guide/cgroup-v2.html)
- [POSIX Standard Exit Codes and Shell Return Values](https://pubs.opengroup.org/onlinepubs/9699919799/)

---

| [< Previous: 07-registries-and-image-naming.md](./07-registries-and-image-naming.md) | [Next: 09-pid-1-signals-and-init.md](./09-pid-1-signals-and-init.md) > |
| :--- | :--- |
