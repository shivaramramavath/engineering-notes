---
title: "Container Diagnostics and Observability: logs, exec, and inspect"
description: "Mastering Docker diagnostics: live log streaming, time-window log filtering, interactive process injection with exec, and Go template metadata extraction with inspect."
category: "DevOps / Containers"
tags: ["docker", "commands", "logs", "exec", "inspect", "diagnostics", "troubleshooting", "cli"]
difficulty: "Intermediate"
prerequisites: ["02-commands/01-run-ps-stop.md", "Basic Linux debugging familiarity"]
estimated_read_time: "25 min"
---

# Container Diagnostics and Observability: logs, exec, and inspect

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. Deep Dive: `docker logs` and Logging Drivers](#2-deep-dive-docker-logs-and-logging-drivers)
- [3. Deep Dive: `docker exec` and Process Injection](#3-deep-dive-docker-exec-and-process-injection)
- [4. Deep Dive: `docker inspect` and Go Template Querying](#4-deep-dive-docker-inspect-and-go-template-querying)
- [5. Practical Diagnostic Demonstrations](#5-practical-diagnostic-demonstrations)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table: Diagnostic Commands](#9-comparison-table-diagnostic-commands)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

When a containerized microservice misbehaves in staging or production—returning 502 Bad Gateway, leaking memory, or failing to connect to an external cache—platform engineers cannot simply log in via SSH. Standard container images omit SSH daemons entirely to minimize attack surface and image size.

Instead, Docker provides three foundational commands that form the diagnostic toolkit for container observability:
1. **`docker logs`**: Streams, filters, and searches standard output (`stdout`) and standard error (`stderr`) streams captured by the daemon.
2. **`docker exec`**: Spawns an ephemeral process directly inside an existing container's live namespaces, providing shell access, live network tracing, and filesystem inspection without restarting the container.
3. **`docker inspect`**: Queries the engine's internal state machine, returning comprehensive JSON documents containing IP addresses, volume bindings, environment variables, health status, and cgroup resource limits.

Mastering these three diagnostic commands allows engineers to isolate production incidents in seconds without interrupting running services.

```
+---------------------------------------------------------------------------------+
|                       CONTAINER DIAGNOSTIC INTERACTION                          |
|                                                                                 |
|                        +-----------------------+                                |
|                        | Engineer Workstation  |                                |
|                        +-----------+-----------+                                |
|                                    |                                            |
|          +-------------------------+-------------------------+                  |
|          |                         |                         |                  |
|          | docker logs -f          | docker exec -it /bin/sh | docker inspect   |
|          v                         v                         v                  |
|  +---------------+         +---------------+         +---------------+          |
|  | Reads JSON-Log|         | Calls setns() |         | Queries Engine|          |
|  | on Host Disk  |         | enters live NS|         | In-Memory State|         |
|  +-------+-------+         +-------+-------+         +-------+-------+          |
|          |                         |                         |                  |
|          +--------------------+    |    +--------------------+                  |
|                               |    |    |                                       |
|                               v    v    v                                       |
|                   +-------------------------------+                             |
|                   | Running Container: api-server |                             |
|                   | - PID 1: Node.js Web Server   |                             |
|                   | - Private IP: 172.17.0.4      |                             |
|                   | - Mounts: /var/log, /data     |                             |
|                   +-------------------------------+                             |
+---------------------------------------------------------------------------------+
```

---

## 2. Deep Dive: `docker logs` and Logging Drivers

### 2.1. How Docker Captures Logs
In standard Twelve-Factor Application design, applications do not write to local log files; they write unbuffered text streams directly to `stdout` and `stderr`.

When a container starts, `dockerd` redirects the container's standard I/O file descriptors to the configured **Logging Driver**:
- **`json-file` (Default)**: Writes line-delimited JSON objects to `/var/lib/docker/containers/<id>/<id>-json.log`.
- **`local`**: A binary, ring-buffered storage driver designed to minimize disk fragmentation and prevent out-of-disk crashes.
- **`journald` / `syslog` / `fluentd`**: Routes log lines directly into operating system log managers or enterprise log aggregators (Datadog, Splunk, Grafana Loki).

### 2.2. Command Syntax & Essential Flags:
```bash
docker logs [OPTIONS] CONTAINER
```
- `-f`, `--follow`: Streams logs continuously in real time (equivalent to `tail -f`).
- `-n <int>`, `--tail <int>`: Outputs only the specified number of lines from the end of the log (e.g., `--tail 100`).
- `-t`, `--timestamps`: Prepends RFC3339 formatted timestamps (e.g., `2026-11-25T08:52:10.123456789Z`) to every log entry.
- `--since <timestamp>`: Shows logs generated after a specific time (e.g., `--since 10m` or `--since 2026-11-25T08:00:00Z`).
- `--until <timestamp>`: Shows logs generated before a specific time window.
- `--details`: Shows extra attributes passed to the logging driver (such as environment variables or labels).

---

## 3. Deep Dive: `docker exec` and Process Injection

### 3.1. How `docker exec` Works Under the Hood
`docker exec` does **not** create a new container; it spawns a new process inside an existing container's environment:
1. The Docker CLI sends a REST request to `dockerd` specifying the container ID and execution parameters (`/bin/sh`).
2. `dockerd` invokes `runc exec`.
3. `runc` calls the Linux kernel system call **`setns()`**, attaching the new process to the target container's existing **PID**, **Network**, **Mount**, **IPC**, and **UTS** namespaces.
4. The new process shares the container's virtual ethernet adapter, filesystem view, and environment variables.

### 3.2. Command Syntax & Key Flags:
```bash
docker exec [OPTIONS] CONTAINER COMMAND [ARG...]
```
- `-i`, `--interactive`: Keeps standard input open.
- `-t`, `--tty`: Allocates a pseudo-terminal screen driver.
- `-u <user>`, `--user <user>`: Overrides user identity (e.g., `-u root` to perform administrative troubleshooting in a container running as an unprivileged user).
- `-w <dir>`, `--workdir <dir>`: Sets working directory for the injected command.
- `-e <KEY=VAL>`, `--env <KEY=VAL>`: Injects temporary environment variables for the execution session.
- `-d`, `--detach`: Runs the injected command in the background.

---

## 4. Deep Dive: `docker inspect` and Go Template Querying

### 4.1. Structure of Container Metadata
`docker inspect <target>` returns a detailed, multi-level JSON array containing the entire runtime configuration of a container or image.

Key JSON sub-trees include:
- `.State`: Status, Running, Paused, OOMKilled, ExitCode, StartedAt, FinishedAt.
- `.NetworkSettings`: IPAddress, Gateway, MacAddress, Ports, Networks.
- `.Mounts`: Volume bindings, Source (host path), Destination (container path), Mode, RW/RO.
- `.Config`: Environment variables (`.Config.Env`), WorkingDir, Entrypoint, Cmd, Labels.
- `.HostConfig`: Resource limits (`Memory`, `CpuQuota`), RestartPolicy, PortBindings.

### 4.2. Go Template Query Syntax (`--format`)
Piping `docker inspect` to `grep` or `jq` is fragile in shell scripts. Docker provides native Go template evaluation via `--format`:

```bash
# Query container IP address directly
docker inspect --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' <container>

# Query whether container was OOM killed
docker inspect --format '{{.State.OOMKilled}}' <container>

# Query health status
docker inspect --format '{{.State.Health.Status}}' <container>

# Iterate over all environment variables
docker inspect --format '{{range .Config.Env}}{{println .}}{{end}}' <container>

# Pretty-print complete JSON sub-tree
docker inspect --format '{{json .State}}' <container> | python3 -m json.tool
```

---

## 5. Practical Diagnostic Demonstrations

### Example 1: Time-Windowed Live Log Streaming
Filter application logs to pinpoint errors occurring in the last 15 minutes with microsecond timestamps:

```bash
# Start background web server
docker run -d --name logs-demo -p 8085:80 nginx:alpine

# Generate test traffic
curl -s http://localhost:8085 > /dev/null

# Stream logs with timestamps from the last 10 minutes
docker logs --since 10m -t --tail 50 logs-demo
```
*Output:*
```text
2026-11-25T08:52:45.892019283Z 172.17.0.1 - - [25/Nov/2026:08:52:45 +0000] "GET / HTTP/1.1" 200 615 "-" "curl/8.4.0" "-"
```

### Example 2: Interactive Shell Injection as Root User
Troubleshoot a non-root production container by injecting a shell as root to install diagnostic tools:

```bash
# Run container as unprivileged user
docker run -d --name non-root-app --user 1000:1000 alpine sleep 3600

# Verify standard exec runs as UID 1000
docker exec non-root-app id
# Output: uid=1000 gid=1000

# Inject administrative debugging shell as ROOT (UID 0)!
docker exec -u 0 -it non-root-app whoami
# Output: root

# Cleanup
docker rm -f non-root-app
```

### Example 3: Extracting Container Host PID and Mounting Information
Extract the host OS PID to analyze thread performance via host `top` or `perf`:

```bash
# Run background container
docker run -d --name pid-target alpine sleep 3600

# Extract Host PID
HOST_PID=$(docker inspect --format '{{.State.Pid}}' pid-target)
echo "Container executes as Host PID: $HOST_PID"

# Inspect host process details
ps -o pid,ppid,user,stat,comm -p $HOST_PID

# Cleanup
docker rm -f pid-target
```

### Example 4: Querying Host Port Bindings via Go Templates
Extract published host ports dynamically in automated deployment verification scripts:

```bash
# Start Nginx publishing port 80 to random host port
docker run -d --name port-query-demo -p 80 nginx:alpine

# Extract dynamically allocated host port
HOST_PORT=$(docker inspect --format '{{(index (index .NetworkSettings.Ports "80/tcp") 0).HostPort}}' port-query-demo)
echo "Nginx published to Host Port: $HOST_PORT"

# Verify HTTP response
curl -I http://localhost:$HOST_PORT

# Cleanup
docker rm -f port-query-demo
```

### Example 5: One-Off Network Diagnostic Injection
Inject `ping` or `traceroute` into a container's network namespace without installing networking utilities permanently inside the application image:

```bash
# Run isolated web server
docker run -d --name target-web nginx:alpine

# Inject a one-off diagnostic curl check inside the running container
docker exec target-web wget -qO- http://localhost:80 | grep "Welcome to nginx"

# Cleanup
docker rm -f target-web logs-demo
```

---

## 6. Best Practices & Production Guidelines

1. **Always Use `--tail` When Querying Production Logs**: Never run `docker logs <container>` without `--tail` on long-running production services. A container running for months can output gigabytes of text, saturating the CLI buffer and exhausting terminal memory.
2. **Never SSH into Containers**: Do not install `openssh-server` inside container images. Use `docker exec -it <container> /bin/sh` for interactive access. This minimizes image size and closes remote attack vectors.
3. **Configure Daemon Log Rotation**: Always configure `max-size` (e.g., `50m`) and `max-file` (e.g., `3`) in `/etc/docker/daemon.json` to prevent local JSON log files from consuming 100% of the host root filesystem.
4. **Use Go Templates in Automation Scripts**: Avoid parsing raw `docker inspect` output with shell `awk` or `sed`. Go template formatting (`--format`) is cross-platform, deterministic, and resistant to CLI formatting changes.
5. **Log to Stdout/Stderr Exclusively**: Ensure all microservices log directly to standard console streams. Do not write application logs to internal files inside the container rootfs.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Proper Fix |
|:---|:---|:---|:---|
| **Running `docker logs` without limits on old containers** | Container has accumulated millions of log lines. | Terminal hangs; Docker daemon CPU spikes transferring gigabytes of JSON. | Always supply `--tail 100` or `--since 1h`. |
| **Installing SSH server in container for debugging** | Developer treating container like a VM. | Bloats image size by 50MB+; requires credential management; opens port 22 security vulnerability. | Use `docker exec -it` for on-demand debugging. |
| **Parsing `docker inspect` with `grep`** | Shell scripts grepping IP addresses. | Fragile; breaks when multiple networks or IPv6 addresses exist. | Use Go template: `{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}`. |
| **Writing logs to `/var/log/app.log` inside container** | Application logging to filesystem instead of stdout. | `docker logs` returns nothing; container writable layer expands indefinitely. | Configure application logging framework to output to console (`stdout`). |
| **Running resource-heavy profiling via `exec` in prod** | Spawning heavy debugging tools inside live production containers. | Shares cgroup resource limits; starves production app of CPU/RAM, causing latency spikes. | Mirror traffic to staging or use read-only non-intrusive eBPF tools from the host. |
| **Assuming `exec` environment matches `run` environment** | Passing custom `-e` to `exec` that conflicts with container env. | Diagnostic tool operates under false assumptions. | Inspect container's native environment via `docker exec env`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **`docker logs` Disk I/O Overhead**: The `json-file` logging driver formats every line into a JSON string and writes it synchronously to disk. High-throughput applications logging $>10,000$ lines/sec can experience I/O bottlenecks. Use the `local` driver or `syslog` with async logging (`"mode": "non-blocking"`).
- **`docker exec` Footprint**: Spawning an exec process shares the container's existing cgroup CPU and memory quotas. If a container is near its `--memory` limit, launching a heavy shell or profiler can trigger the Linux OOM killer!

### Security
- **Audit Trails**: Access to `docker exec` bypasses standard application authentication and grants root filesystem access. Audit and restrict Docker socket access via RBAC in production environments.
- **Log Sanitation**: Sensitive data (passwords, JWT tokens, credit card numbers) printed to `stdout` are stored unencrypted in `/var/lib/docker/containers/`. Sanitize application logs before emission.

### Cost
- In cloud logging environments (AWS CloudWatch, Datadog), uncontrolled stdout log ingestion represents up to 20-30% of total cloud infrastructure bills. Use log rotation and sampling.

---

## 9. Comparison Table: Diagnostic Commands

| Feature | `docker logs` | `docker exec` | `docker inspect` |
|:---|:---|:---|:---|
| **Diagnostic Focus** | Application output history | Live runtime interaction & debugging | Internal engine state & metadata |
| **Execution Impact**| Passive read from host disk | Spawns active process inside namespaces | Passive query against daemon RAM |
| **Interactive?** | Read-only stream (`-f`) | Fully interactive TTY (`-it`) | Non-interactive JSON output |
| **Works on Stopped Containers?**| **Yes** | **No** (Must be `running`) | **Yes** |
| **Resource Usage** | Disk read I/O | Shares container cgroup RAM/CPU | Negligible daemon API query |
| **Primary Use Case**| Diagnosing crashes, errors | Shell access, ping, testing DB access | Querying IPs, ports, health state |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. How does <code>docker exec</code> inject a new process into a running container at the Linux kernel level?</strong></summary>

`docker exec` leverages the Linux kernel **`setns()`** system call. 
1. The Docker daemon retrieves the container's target namespace file descriptors from `/proc/<container_pid>/ns/` (including `ns/pid`, `ns/net`, `ns/mnt`, `ns/ipc`, and `ns/uts`).
2. The daemon executes `runc exec`, which opens these file descriptors and calls `setns()` to attach the new process to the existing namespaces.
3. The process is placed into the container's existing cgroup hierarchy and executes the requested binary inside the container's virtual rootfs.
</details>

<details>
<summary><strong>2. What happens if an application writes its logs to a local file inside the container instead of stdout/stderr?</strong></summary>

If an application writes logs to `/var/log/app.log`:
1. `docker logs` will return empty output because the Docker logging driver only captures standard output (file descriptor 1) and standard error (file descriptor 2).
2. The log file is written to the container's read-write `UpperDir` storage layer, consuming host disk space and triggering Copy-on-Write (CoW) overhead.
3. When the container is deleted (`docker rm`), all historical log files are permanently destroyed unless stored on a mounted volume.
</details>

<details>
<summary><strong>3. How can you extract the IP address of a container across multiple user-defined networks using <code>docker inspect</code>?</strong></summary>

Using Go template range syntax:
```bash
docker inspect --format '{{range $net, $conf := .NetworkSettings.Networks}}{{$net}}: {{$conf.IPAddress}} {{end}}' <container>
```
This iterates over all network attachments (e.g., `bridge`, `frontend`, `backend`) and prints the assigned IP address for each.
</details>

<details>
<summary><strong>4. Why can running <code>docker exec -it <container> /bin/sh</code> cause an Out-of-Memory (OOM) crash in production?</strong></summary>

The injected `exec` process executes within the **same cgroup memory controller** as the containerized application. If a container is configured with `--memory="256m"` and the application is actively consuming $245\text{ MB}$, launching an interactive shell and executing diagnostic tools (like `python`, `top`, or `vim`) allocates memory that pushes the cgroup over the $256\text{ MB}$ limit. The kernel OOM killer activates and terminates the highest memory-consuming process (typically the primary production application!).
</details>

<details>
<summary><strong>5. What is the difference between the <code>json-file</code> and <code>local</code> logging drivers in Docker?</strong></summary>

- **`json-file`**: The historical default. Formats log lines as JSON and writes to a text file. It supports no compression and can cause disk exhaustion if rotation is not configured.
- **`local`**: A modern high-performance binary logging driver. It implements automatic log rotation, gzip compression on rotated files, and strict disk space caps by default, protecting the host system from out-of-disk crashes.
</details>

<details>
<summary><strong>6. How can an administrator enter a container running without an interactive shell (e.g., Distroless or scratch)?</strong></summary>

Distroless and scratch images contain only the compiled binary and zero shells (`/bin/sh` or `/bin/bash` do not exist). To debug them:
1. Use **`nsenter`** from the host operating system to enter the container's network or mount namespace using host binaries.
2. In Kubernetes, attach an **Ephemeral Debug Container** sharing the target pod's namespaces.
3. In Docker, run a secondary container sharing the target's network namespace:
   ```bash
   docker run --rm -it --net=container:<target_name> nicolaka/netshoot
   ```
</details>

<details>
<summary><strong>7. How does the <code>--since</code> flag in <code>docker logs</code> parse relative time?</strong></summary>

The `--since` flag accepts both RFC3339 absolute timestamps (`2026-11-25T08:00:00Z`) and relative duration strings parsed via Go's `time.ParseDuration` (e.g., `10m` for 10 minutes, `3h` for 3 hours, `45s` for 45 seconds). Docker reads timestamps embedded in the log entries and streams only lines whose timestamp is $\ge$ the calculated cutoff time.
</details>

<details>
<summary><strong>8. Can <code>docker inspect</code> be executed against images, networks, and volumes in addition to containers?</strong></summary>

Yes. `docker inspect` is a universal inspection command. If given an image tag or ID, it returns image layers and environment variables. If given a network name, it returns subnet ranges and connected containers. If given a volume name, it returns the physical mount path on host storage (`/var/lib/docker/volumes/...`).
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Query Container Health Status via Go Templates</strong></summary>

**Goal**: Inspect the healthcheck status of an Nginx container.

```bash
# Launch container with a healthcheck
docker run -d --name health-lab --health-cmd="wget -qO- http://localhost || exit 1" --health-interval=2s nginx:alpine

# Wait 3 seconds for probe to fire
sleep 3

# Inspect health state
docker inspect --format 'Status: {{.State.Status}} | Health: {{.State.Health.Status}}' health-lab

# Cleanup
docker rm -f health-lab
```
</details>

<details>
<summary><strong>Exercise 2: Time-Filtered Log Inspection with Timestamps</strong></summary>

**Goal**: Filter log output using relative `--since` intervals.

```bash
# Start container generating logs
docker run -d --name log-timer alpine sh -c "while true; do echo 'Tick: '\$(date); sleep 1; done"

sleep 5

# View logs from the last 2 seconds with timestamps
docker logs --since 2s -t log-timer

# Cleanup
docker rm -f log-timer
```
</details>

<details>
<summary><strong>Exercise 3: Extract All Exposed Ports with JSON Formatting</strong></summary>

**Goal**: Pretty-print the port binding map from `docker inspect`.

```bash
docker run -d --name port-map-lab -p 8080:80 -p 8443:443 nginx:alpine
docker inspect --format '{{json .NetworkSettings.Ports}}' port-map-lab | python3 -m json.tool
docker rm -f port-map-lab
```
</details>

<details>
<summary><strong>Exercise 4: Execute Injected Command as Specific User and Working Directory</strong></summary>

**Goal**: Prove execution identity customization via `docker exec`.

```bash
docker run -d --name user-exec-demo alpine sleep 100

# Execute command inside /var/log as nobody user (UID 65534)
docker exec -u nobody -w /var/log user-exec-demo sh -c "whoami && pwd"

docker rm -f user-exec-demo
```
</details>

<details>
<summary><strong>Exercise 5: Inspect Volume Bindings</strong></summary>

**Goal**: Inspect volume source and destination mapping.

```bash
docker run -d --name vol-inspect -v /tmp:/data:ro alpine sleep 100
docker inspect --format '{{range .Mounts}}Source: {{.Source}} -> Destination: {{.Destination}} (RO: {{not .RW}}){{end}}' vol-inspect
docker rm -f vol-inspect
```
</details>

<details>
<summary><strong>Exercise 6: Stream Logs in Detached Follow Mode with Grep Filtering</strong></summary>

**Goal**: Pipeline live logs into standard Linux utilities.

```bash
docker run -d --name stream-lab alpine sh -c "for i in \$(seq 1 10); do echo \"event-type-\$i\"; sleep 0.5; done"
docker logs -f stream-lab | grep "event-type-5"
docker rm -f stream-lab
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                         LOGS, EXEC, INSPECT CHEAT SHEET                               |
+---------------------------------------------------------------------------------------+
| Follow Live Logs:      docker logs -f --tail 50 <name>                                |
| Time-Window Logs:      docker logs --since 15m -t <name>                              |
| Interactive Shell:     docker exec -it <name> /bin/sh                                 |
| Exec as Root:          docker exec -u 0 -it <name> whoami                             |
| Query IP Address:      docker inspect --format '{{range .NetworkSettings.Networks}}   |
|                        {{.IPAddress}}{{end}}' <name>                                  |
| Query Host PID:        docker inspect --format '{{.State.Pid}}' <name>                |
| Query Exit Code:       docker inspect --format '{{.State.ExitCode}}' <name>           |
| Check OOM Killer:      docker inspect --format '{{.State.OOMKilled}}' <name>          |
| Golden Rule:           Never run docker logs without --tail on production containers. |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Docker Documentation: View Container Logs](https://docs.docker.com/engine/reference/commandline/logs/)
- [Docker Documentation: docker exec Reference](https://docs.docker.com/engine/reference/commandline/exec/)
- [Docker Documentation: Format Command and Log Output (Go Templates)](https://docs.docker.com/config/formatting/)
- [Docker Documentation: Configure Logging Drivers](https://docs.docker.com/config/containers/logging/configure/)
- [Linux Kernel Manual: setns(2) System Call](https://man7.org/linux/man-pages/man2/setns.2.html)

---

| [< Previous: 01-run-ps-stop.md](./01-run-ps-stop.md) | [Next: 03-images.md](./03-images.md) > |
| :--- | :--- |
