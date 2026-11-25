---
title: "Real-Time Container Observability: docker events, stats, and top"
description: "Mastering real-time monitoring and event streaming: listening to daemon event buses with docker events, live metric analysis with docker stats, and process inspection with docker top."
category: "DevOps / Containers"
tags: ["docker", "events", "stats", "top", "monitoring", "observability", "metrics", "commands"]
difficulty: "Intermediate"
prerequisites: ["02-commands/01-run-ps-stop.md", "02-commands/06-resource-limits-cpu-memory.md"]
estimated_read_time: "25 min"
---

# Real-Time Container Observability: docker events, stats, and top

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. Deep Dive: `docker events` and Daemon Event Streaming](#2-deep-dive-docker-events-and-daemon-event-streaming)
- [3. Deep Dive: `docker stats` and Live Performance Metrics](#3-deep-dive-docker-stats-and-live-performance-metrics)
- [4. Deep Dive: `docker top` and Process Tree Inspection](#4-deep-dive-docker-top-and-process-tree-inspection)
- [5. Practical Diagnostic Demonstrations](#5-practical-diagnostic-demonstrations)
- [6. Building Event-Driven Auto-Healing Daemons](#6-building-event-driven-auto-healing-daemons)
- [7. Best Practices & Production Guidelines](#7-best-practices--production-guidelines)
- [8. Common Pitfalls & Anti-Patterns](#8-common-pitfalls--anti-patterns)
- [9. Comparison Table: Observability Tools](#9-comparison-table-observability-tools)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

Observing containerized infrastructure requires more than reading static configuration files or periodic log polling. Production platforms require **continuous, real-time awareness**:
- When did a specific microservice crash or get terminated by the Out-of-Memory (OOM) killer?
- Which container is saturating host network bandwidth or thrashing block storage with uncontrolled disk writes?
- What processes and threads are actively executing inside a container from the host kernel's perspective?

Docker addresses these operational requirements through three complementary real-time observability tools:
1. **`docker events`**: A real-time, push-based event bus that streams daemon lifecycle events (container starts, kills, OOM crashes, volume attachments, image pulls).
2. **`docker stats`**: A live, streaming performance dashboard displaying CPU utilization, memory thresholds, network I/O, block I/O, and active PID counts.
3. **`docker top`**: Inspects the active process table inside a container's PID namespace directly from the host.

```
+---------------------------------------------------------------------------------+
|                       REAL-TIME OBSERVABILITY PIPELINE                          |
|                                                                                 |
|                        +-----------------------+                                |
|                        | Docker Daemon Engine  |                                |
|                        +-----------+-----------+                                |
|                                    |                                            |
|          +-------------------------+-------------------------+                  |
|          | (Live Event Stream)     | (Cgroup Metrics)        | (Kernel /proc)   |
|          v                         v                         v                  |
|  [docker events]           [docker stats]            [docker top]               |
|  - container: create,      - CPU %                   - Host PID                 |
|    start, die, oom, kill   - Memory Usage / Limit    - User                     |
|  - volume: mount, create   - Net I/O (bytes in/out)  - CPU %                    |
|  - image: pull, push       - Block I/O (read/write)  - Command arguments        |
|  - network: connect        - PIDs Count              - Thread states            |
+---------------------------------------------------------------------------------+
```

---

## 2. Deep Dive: `docker events` and Daemon Event Streaming

### 2.1. The Daemon Event Bus
The Docker daemon maintains an internal event bus. Every state transition, API call, and cgroup signal emits a structured event payload:
- **Scope**: Events are categorized by type: `container`, `image`, `volume`, `network`, `daemon`, `plugin`, `service`, `node`, `secret`, `config`.
- **Actions**: Actions describe what happened: `attach`, `commit`, `create`, `destroy`, `die`, `exec_create`, `exec_start`, `kill`, `oom`, `pause`, `restart`, `start`, `stop`, `unpause`.

### 2.2. Command Syntax & Filtering
```bash
docker events [OPTIONS]
```
- `-f`, `--filter <key=value>`: Filters incoming event streams by type, container name, image, or event action.
- `--since <timestamp>`: Replays historical events starting from a specific time (e.g., `--since 1h` or RFC3339 timestamp).
- `--until <timestamp>`: Stops streaming after reaching the specified time.
- `--format <template>`: Formats output using Go templates or raw JSON (`--format '{{json .}}'`).

```bash
# Listen strictly to Out-of-Memory (OOM) events across all containers
docker events --filter "event=oom"

# Listen strictly to container termination events (die)
docker events --filter "type=container" --filter "event=die"

# Listen to events affecting a specific container
docker events --filter "container=production-api"
```

---

## 3. Deep Dive: `docker stats` and Live Performance Metrics

### 3.1. How `docker stats` Gathers Metrics
`docker stats` does not inject agents into containers. It reads hardware metrics directly from the host Linux kernel's cgroup files under `/sys/fs/cgroup/` and the `/proc/net/dev` virtual filesystem:
- **CPU %**: Calculated from CFS runtime counters (`cpu.stat` or `cpuacct.usage`) divided by the elapsed host CPU time across available cores.
- **MEM USAGE / LIMIT**: Physical RAM consumption (`memory.current` in cgroups v2) versus the configured limit (`memory.max`).
- **NET I/O**: Network interface counters (bytes received and transmitted over container `eth0`).
- **BLOCK I/O**: Cumulative disk reads and writes tracked via the block I/O cgroup controller (`io.stat`).
- **PIDS**: The current count of running processes and lightweight threads (`pids.current`).

### 3.2. Command Syntax & Scripting Flags:
```bash
docker stats [OPTIONS] [CONTAINER...]
```
- `--no-stream`: Disables continuous terminal updates and outputs a single snapshot of metrics, indispensable for automated shell scripting and cron metrics collection.
- `-a`, `--all`: Shows stats for all containers (defaults to running only).
- `--format <template>`: Formats output as custom tables or JSON.

```bash
# Query single-shot metrics table
docker stats --no-stream

# Format as machine-readable JSON
docker stats --no-stream --format '{{json .}}'
```

---

## 4. Deep Dive: `docker top` and Process Tree Inspection

### 4.1. How `docker top` Works
`docker top <container> [ps_args]` queries the host's `/proc` filesystem to list the host PIDs associated with the container's PID namespace:
- Unlike running `ps` inside the container via `docker exec` (which requires a shell and a `ps` binary installed in the container), `docker top` runs on the **host engine**.
- It works seamlessly on **Distroless** and **scratch** containers that contain zero shell utilities!
- It accepts standard Linux `ps` argument flags (e.g., `-ef`, `aux`, `-o pid,comm,user,%cpu`).

```bash
docker top my-container
docker top my-container -ef
```

---

## 5. Practical Diagnostic Demonstrations

### Example 1: Capturing OOM Events in Real Time
Run an event listener in the background and trigger an Out-of-Memory kill:

```bash
# 1. Start event listener capturing only OOM events
docker events --filter "event=oom" --format 'Container {{.Actor.Attributes.name}} exceeded memory limit at {{.Time}}' &
EVENT_PID=$!

# 2. Launch container exceeding memory limit
docker run --rm --name oom-trigger --memory="16m" alpine python3 -c 'x = "a" * 100000000' 2>/dev/null || true

# Kill event listener
kill $EVENT_PID 2>/dev/null || true
```
*Output:*
```text
Container oom-trigger exceeded memory limit at 1732524589
```

### Example 2: Continuous Real-Time Metrics Dashboard
Stream live metrics for a specific subset of microservice containers:

```bash
# Launch test web service
docker run -d --name metrics-demo -p 8088:80 nginx:alpine

# Stream live metrics for this container
docker stats --no-stream metrics-demo --format "table {{.Name}}\t{{.CPUPerc}}\t{{.MemUsage}}\t{{.NetIO}}\t{{.BlockIO}}\t{{.PIDs}}"

# Cleanup
docker rm -f metrics-demo
```

### Example 3: Inspecting Processes in a Distroless Container with `docker top`
Inspect processes inside a container that has no shell or `ps` installed:

```bash
# Run a container with sleep as PID 1
docker run -d --name top-demo alpine sleep 3600

# Inspect active processes from the host perspective
docker top top-demo -o pid,ppid,user,%cpu,comm

# Notice that the host PID, user, and command arguments are clearly displayed!
docker rm -f top-demo
```

### Example 4: Replaying Historical Events
Query all container termination events that occurred on the host within the last 30 minutes:

```bash
docker events --since 30m --until 0s --filter "type=container" --filter "event=die" --format 'Time: {{.Time}} | Container: {{.Actor.Attributes.name}} | ExitCode: {{.Actor.Attributes.exitCode}}'
```

### Example 5: High-Precision CPU Throttling Detection
Combine `docker stats` with Go formatting to trigger alerts when CPU utilization exceeds 80%:

```bash
docker stats --no-stream --format '{{.Name}}: {{.CPUPerc}}' | awk -F': ' '{gsub("%","",$2); if ($2 > 80.0) print "ALERT: High CPU on "$1}'
```

---

## 6. Building Event-Driven Auto-Healing Daemons

Because `docker events` emits standard line-delimited text streams, engineers can build powerful, lightweight auto-discovery, auto-reloading, or auto-healing daemons using standard bash scripts:

```bash
#!/bin/bash
# Nginx Dynamic Upstream Reloader Daemon
# Listens to container start and die events to reload reverse proxy configs

echo "Starting Nginx Auto-Reloader daemon..."
docker events --filter "type=container" --filter "event=start" --filter "event=die" --format '{{.Status}} {{.Actor.Attributes.name}}' | while read -r status container; do
  echo "Detected event [$status] on container [$container] - Regenerating Nginx upstream..."
  # Re-generate configuration and reload Nginx
  # docker exec nginx-proxy nginx -s reload
done
```

---

## 7. Best Practices & Production Guidelines

1. **Use `--no-stream` for Automation**: Never pipe standard `docker stats` into scripts or cron jobs without `--no-stream`. Without `--no-stream`, Docker continuously emits ANSI terminal clear-screen codes that corrupt log files and buffer endlessly in non-interactive pipes.
2. **Filter Events at the Daemon Level**: When listening to `docker events`, always specify `--filter "type=..."` and `--filter "event=..."`. Unfiltered event streams force the CLI to process high-frequency internal events (`exec_create`, `mount`), consuming unnecessary CPU.
3. **Use `docker top` to Debug Minimal Containers**: For Distroless, scratch, or hardened non-root containers where `docker exec` cannot spawn a shell, use `docker top` to verify process status, UID ownership, and thread counts from the host.
4. **Monitor `Block I/O` for Disk Thrashing**: Applications performing high-frequency unbuffered logging or database updates inside container writable layers exhibit massive Block I/O numbers in `docker stats`. Identify these containers and migrate their paths to Docker Volumes.
5. **Parse JSON for Automated Metrics Exporters**: When building custom Prometheus or Datadog metric collectors, query `docker stats --no-stream --format '{{json .}}'` to parse structured JSON fields reliably.

---

## 8. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Proper Solution |
|:---|:---|:---|:---|
| **Piping streaming `docker stats` into cron/scripts** | Omitting `--no-stream`. | Emits thousands of terminal escape codes; process hangs in buffer; disk fills with garbage. | Always pass `--no-stream` when piping or redirecting stats. |
| **Running unbuffered `docker events` without reconnection logic** | Network glitch or daemon restart terminates stream. | Script stops listening silently; auto-healing or discovery fails permanently. | Wrap `docker events` inside an infinite reconnection loop (`while true; do docker events ...; sleep 1; done`). |
| **Assuming `docker top` requires `ps` in the container** | Thinking `top` fails on minimal images. | Engineers avoid `top` on minimal images, missing critical process visibility. | `docker top` uses the host's `/proc`; it works on all containers. |
| **Misinterpreting `docker stats` CPU % on multi-core hosts** | Container reports `200%` CPU. | Engineer panics thinking it is a bug. | In Linux, 100% = 1 core. On a 4-core host, a multi-threaded app can reach 400% CPU. |
| **Ignoring historical event timestamps** | Running `docker events` expecting past events. | `docker events` defaults to listening to future events only. | Supply `--since <time>` to replay historical events. |
| **Polling `docker inspect` in tight loops** | Polling daemon API every 100ms. | Floods daemon with REST requests, causing daemon CPU spikes. | Use `docker events` for push-based reactive notifications. |

---

## 9. Comparison Table: Observability Tools

| Feature | `docker events` | `docker stats` | `docker top` |
|:---|:---|:---|:---|
| **Data Nature** | Discrete lifecycle state changes | Continuous hardware performance counters | Process table snapshot |
| **Stream Mechanism** | Push-based event stream | Polled continuous dashboard (or snapshot) | Single-shot process listing |
| **Target Scope** | Entire daemon (all objects) | Containers (single or all) | Single container instance |
| **Works on Stopped Containers?**| Historical replay via `--since` | No (Running only) | No (Running only) |
| **Host Resource Impact**| Minimal (Event notifications) | Reads host cgroup files every 1s | Reads host `/proc` file table |
| **Primary Use Case**| Auto-healing, audits, alerting | CPU/RAM leak detection, sizing | Process tree inspection, zombies |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. How does <code>docker stats</code> calculate CPU percentage on a multi-core Linux host?</strong></summary>

`docker stats` calculates CPU percentage by taking two consecutive snapshots of the host and container cgroup CPU counters:
$$\text{CPU \%} = \left( \frac{\Delta \text{Container CPU Usage}}{\Delta \text{System CPU Time}} \right) \times \text{Number of Online CPUs} \times 100$$
Because the multiplier includes the number of online CPU cores, a container running four fully saturated threads on an 8-core machine will report approximately **`400%` CPU utilization**.
</details>

<details>
<summary><strong>2. How does <code>docker top</code> retrieve process information from a container that contains no binaries other than a static Go executable?</strong></summary>

Containers do not virtualize the kernel; processes inside a container are standard Linux processes executing on the host kernel. When you run `docker top <container>`, Docker retrieves the container's primary host PID from `dockerd`'s state machine, locates the corresponding process tree in the host's **`/proc`** filesystem, and executes the host's own `ps` command (or internal kernel traversal) scoped to that process and its children. The container does not need `ps`, `/bin/sh`, or any utilities installed.
</details>

<details>
<summary><strong>3. How can you use <code>docker events</code> to capture when a container is killed by the Out-of-Memory (OOM) killer?</strong></summary>

By applying event action filters:
```bash
docker events --filter "type=container" --filter "event=oom"
```
The Docker daemon listens to the kernel cgroup memory controller's OOM notification event file descriptor. When an OOM occurs, the daemon emits an `oom` event containing the container ID, container name, and exact timestamp.
</details>

<details>
<summary><strong>4. What is the difference between streaming mode and non-streaming mode in <code>docker stats</code>?</strong></summary>

- **Streaming Mode (Default)**: Docker continuously polls cgroup files every 1 second, printing ANSI escape sequences (`\033[2J\033[H`) to clear the terminal screen and render an interactive live updating dashboard.
- **Non-Streaming Mode (`--no-stream`)**: Docker collects a single metric sample, formats the table or JSON output, prints it to standard output, and exits immediately. This is required when piping output into shell utilities (`grep`, `awk`, `jq`) or automated cron metrics collectors.
</details>

<details>
<summary><strong>5. How can you replay all container creation and deletion events that occurred over the last 24 hours?</strong></summary>

Using the `--since` flag:
```bash
docker events --since 24h --until 0s --filter "type=container"
```
Docker queries its in-memory circular event buffer (or systemd journal depending on logging driver configuration) and streams all matching historical events from the past 24 hours.
</details>

<details>
<summary><strong>6. What do the <code>NET I/O</code> and <code>BLOCK I/O</code> columns in <code>docker stats</code> represent?</strong></summary>

- **`NET I/O`**: Displays cumulative network traffic transferred through the container's virtual ethernet interface (`eth0`) since startup, formatted as `<Bytes Received> / <Bytes Transmitted>` (e.g., `12.4MB / 8.2MB`).
- **`BLOCK I/O`**: Displays cumulative disk data read from and written to physical block devices by the container's processes via the cgroup I/O controller, formatted as `<Bytes Read> / <Bytes Written>` (e.g., `450KB / 120MB`).
</details>

<details>
<summary><strong>7. How can you format <code>docker stats</code> to output strictly valid JSON records?</strong></summary>

By specifying the JSON template in `--format`:
```bash
docker stats --no-stream --format '{{json .}}'
```
This produces one JSON document per container containing raw integer byte values and floating-point percentages, suitable for direct ingestion into Elasticsearch or Prometheus.
</details>

<details>
<summary><strong>8. Why should an engineer avoid polling <code>docker inspect</code> in a high-frequency loop, and what should be used instead?</strong></summary>

Executing `docker inspect` repeatedly in a tight loop (e.g., every 50ms) forces the Docker daemon to continuously serialize large JSON structures over the IPC socket, causing high daemon CPU consumption and potential lock contention. Instead, engineers should use **`docker events`**, which establishes a single persistent connection and pushes notifications reactively only when state transitions occur.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Container Threads Using `docker top`</strong></summary>

**Goal**: View individual threads of a multi-threaded application from the host.

```bash
# Start container
docker run -d --name thread-lab nginx:alpine

# Query processes and thread IDs (-eLf)
docker top thread-lab -eLf

# Cleanup
docker rm -f thread-lab
```
</details>

<details>
<summary><strong>Exercise 2: Capture Container Die Events with Exit Codes</strong></summary>

**Goal**: Stream termination events and print the resulting exit codes.

```bash
# In background terminal:
docker events --filter "type=container" --filter "event=die" --format 'Container {{.Actor.Attributes.name}} died with code {{.Actor.Attributes.exitCode}}' &
LISTENER_PID=$!

# Trigger container exit with code 42
docker run --rm --name die-lab alpine sh -c "exit 42" 2>/dev/null || true

# Kill listener
kill $LISTENER_PID 2>/dev/null || true
```
</details>

<details>
<summary><strong>Exercise 3: Format Custom Performance Table with `docker stats`</strong></summary>

**Goal**: Extract only Container Name, CPU %, and Memory Usage.

```bash
docker stats --no-stream --format "table {{.Name}}\t{{.CPUPerc}}\t{{.MemUsage}}"
```
</details>

<details>
<summary><strong>Exercise 4: Stream Real-Time Events Filtered by Image</strong></summary>

**Goal**: Listen only to events triggered by containers running Alpine.

```bash
docker events --filter "image=alpine"
```
</details>

<details>
<summary><strong>Exercise 5: Benchmark Disk I/O Using `docker stats`</strong></summary>

**Goal**: Observe `BLOCK I/O` counters increase during disk write operations.

```bash
# Start container
docker run -d --name io-lab alpine sh -c "dd if=/dev/zero of=/test.dat bs=1M count=50; sleep 60"

# Check Block I/O counter
docker stats --no-stream io-lab --format "table {{.Name}}\t{{.BlockIO}}"

# Cleanup
docker rm -f io-lab
```
</details>

<details>
<summary><strong>Exercise 6: Query Container Process Arguments Using Custom `ps` Flags</strong></summary>

**Goal**: Pass specific formatting arguments to `docker top`.

```bash
docker run -d --name args-lab alpine sleep 1234
docker top args-lab -o pid,user,args
docker rm -f args-lab
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                         EVENTS, STATS, TOP CHEAT SHEET                                |
+---------------------------------------------------------------------------------------+
| Listen to All Events:  docker events                                                  |
| Listen to OOM Events:  docker events --filter "event=oom"                             |
| Replay Past Events:    docker events --since 1h --until 0s                            |
| Event JSON Output:     docker events --format '{{json .}}'                            |
| Live Metrics Dashboard:docker stats                                                   |
| Single Metric Snapshot:docker stats --no-stream (Mandatory for scripting!)            |
| Format Stats Output:   docker stats --no-stream --format                              |
|                        "table {{.Name}}\t{{.CPUPerc}}\t{{.MemUsage}}"                 |
| Inspect Host Processes:docker top <container>                                         |
| Custom ps Arguments:   docker top <container> -o pid,user,%cpu,comm                   |
| Golden Rule:           Never poll docker inspect in loops; use docker events instead. |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Docker CLI Command Reference: docker events](https://docs.docker.com/reference/cli/docker/system/events/)
- [Docker CLI Command Reference: docker stats](https://docs.docker.com/reference/cli/docker/container/stats/)
- [Docker CLI Command Reference: docker top](https://docs.docker.com/reference/cli/docker/container/top/)
- [Linux Kernel Documentation: Control Group v2 Process and Metrics Accounting](https://docs.kernel.org/admin-guide/cgroup-v2.html)
- [Prometheus: cAdvisor (Container Advisor) Metrics Architecture](https://github.com/google/cadvisor)

---

| [< Previous: 08-copying-files-and-exec-patterns.md](./08-copying-files-and-exec-patterns.md) | [Next: 10-formatting-output-with-go-templates.md](./10-formatting-output-with-go-templates.md) > |
| :--- | :--- |
