---
title: "Docker Container Lifecycle Commands: run, ps, stop, start, and kill"
description: "Mastering fundamental container lifecycle commands: syntax flags, detached execution, advanced filtering, graceful termination, signal override, and process inspection."
category: "DevOps / Containers"
tags: ["docker", "commands", "run", "ps", "stop", "kill", "lifecycle", "cli"]
difficulty: "Beginner"
prerequisites: ["01-fundamentals/08-container-lifecycle-and-states.md", "Basic shell familiarity"]
estimated_read_time: "25 min"
---

# Docker Container Lifecycle Commands: run, ps, stop, start, and kill

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. Core Concepts & Command Overview](#2-core-concepts--command-overview)
- [3. Deep Dive: `docker run` Flag Architecture](#3-deep-dive-docker-run-flag-architecture)
- [4. Deep Dive: `docker ps` Filtering and Sizing](#4-deep-dive-docker-ps-filtering-and-sizing)
- [5. Deep Dive: `docker stop`, `restart`, and `kill`](#5-deep-dive-docker-stop-restart-and-kill)
- [6. Practical Diagnostic Demonstrations](#6-practical-diagnostic-demonstrations)
- [7. Best Practices & Production Guidelines](#7-best-practices--production-guidelines)
- [8. Common Pitfalls & Anti-Patterns](#8-common-pitfalls--anti-patterns)
- [9. Comparison Table: Lifecycle Management Commands](#9-comparison-table-lifecycle-management-commands)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

The commands `docker run`, `docker ps`, and `docker stop` constitute the bedrock of container operations. Every engineer interacting with Docker, whether debugging a local development environment, authoring a CI/CD build matrix, or inspecting a staging host, executes these commands dozens of times daily.

However, behind their simple appearance lies significant operational nuance:
- `docker run` is actually a composite command that bundles image pulling, volume initialization, network endpoint configuration, cgroup assignment, and process execution into a single operation.
- `docker ps` supports powerful filtering predicates (`status`, `name`, `label`, `publish`) and Go templating that turn raw process listings into automated scripting pipelines.
- `docker stop` coordinates graceful signal handling (`SIGTERM` followed by a configurable timeout and `SIGKILL`), determining whether applications flush databases cleanly or suffer data corruption.

Mastering these commands and their essential flags guarantees predictable, automated, and secure container management.

```
+---------------------------------------------------------------------------------+
|                       CONTAINER MANAGEMENT COMMAND SUITE                        |
|                                                                                 |
|  [Image on Disk]                                                                |
|        |                                                                        |
|        | docker run [OPTIONS] IMAGE [CMD]                                       |
|        v                                                                        |
|  +---------------------------------------------------------------------------+  |
|  | State: RUNNING (PID 1 active)                                             |  |
|  | - Queried via: docker ps [--filter] [--format]                            |  |
|  +---------------------------------------------------------------------------+  |
|        |                                                |                       |
|        | docker stop -t 10 (SIGTERM)                    | docker kill (SIGKILL) |
|        v                                                v                       |
|  +---------------------------------------------------------------------------+  |
|  | State: EXITED (UpperDir preserved, exit code recorded)                     |  |
|  | - Queried via: docker ps -a                                               |  |
|  | - Resumed via: docker start                                               |  |
|  | - Purged via:  docker rm                                                  |  |
|  +---------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------+
```

---

## 2. Core Concepts & Command Overview

- **`docker run`**: Creates a writable container layer over the specified image and starts it.
- **`docker create`**: Pre-allocates and configures a container without starting its process.
- **`docker start`**: Starts one or more stopped or newly created containers.
- **`docker stop`**: Stops a running container gracefully by sending `SIGTERM`, waiting a grace period (default 10s), then sending `SIGKILL`.
- **`docker kill`**: Immediately terminates a running container by sending `SIGKILL` (or a user-specified signal).
- **`docker restart`**: Stops and subsequently restarts a container.
- **`docker ps`**: Lists containers, their uptime, ports, names, and statuses.

---

## 3. Deep Dive: `docker run` Flag Architecture

```bash
docker run [OPTIONS] IMAGE [COMMAND] [ARG...]
```

### Essential Execution Flags:
- `-d`, `--detach`: Runs container in background and prints container ID.
- `-i`, `--interactive`: Keeps `stdin` open even if unattached.
- `-t`, `--tty`: Allocates a pseudo-TTY screen driver. Combined as `-it` for interactive shells.
- `--name <string>`: Assigns an explicit human-readable name to the container.
- `--rm`: Automatically deletes the container and its anonymous volumes when it exits.
- `-e <KEY=VAL>`, `--env-file <file>`: Injects runtime environment variables.
- `-p <host_port>:<container_port>`: Publishes container port to the host.
- `-v <host_path>:<container_path>`: Mounts a host directory or named volume.
- `-w <dir>`, `--workdir <dir>`: Overrides working directory inside container.
- `-u <uid:gid>`, `--user <uid:gid>`: Overrides username or UID/GID executing the process.
- `--entrypoint <binary>`: Overrides the image's default entrypoint binary.
- `--network <net>`: Connects container to a specific Docker network.
- `--restart <policy>`: Sets restart behavior (`no`, `on-failure`, `always`, `unless-stopped`).

---

## 4. Deep Dive: `docker ps` Filtering and Sizing

By default, `docker ps` lists only actively running containers.

### Advanced Flags:
- `-a`, `--all`: Shows all containers (running, exited, paused, created).
- `-q`, `--quiet`: Displays only numeric container IDs (indispensable for automation scripts).
- `-s`, `--size`: Displays physical disk storage used by each container's writable layer (`UpperDir`) alongside the virtual image size.
- `-n <int>`, `--last <int>`: Shows the $N$ most recently created containers.

### Filtering with `--filter`:
You can filter output using powerful key-value predicates:
```bash
# Filter by status (created, restarting, running, removing, paused, exited, dead)
docker ps --filter "status=running"
docker ps --filter "status=exited"

# Filter by exit code
docker ps -a --filter "exited=1"
docker ps -a --filter "exited=137"

# Filter by name (supports regex/substring matching)
docker ps --filter "name=api"

# Filter by published port
docker ps --filter "publish=8080"

# Filter by custom metadata label
docker ps --filter "label=environment=production"
```

---

## 5. Deep Dive: `docker stop`, `restart`, and `kill`

### 5.1. Graceful Shutdown Flow (`docker stop`)
When you invoke `docker stop -t <seconds> <container>`:
1. Docker sends the `STOPSIGNAL` (default `SIGTERM`) to PID 1 inside the container.
2. The application's signal handler catches `SIGTERM` and initiates teardown:
   - Closes listening HTTP sockets to stop accepting new requests.
   - Allows in-flight HTTP requests to complete.
   - Commits active database transactions and flushes cache buffers.
   - Exits cleanly with code 0.
3. If the process has not exited after the timeout period (default 10 seconds), Docker sends `SIGKILL`, terminating the process instantly.

### 5.2. Custom Signal Injection (`docker kill`)
You can send arbitrary Linux signals directly to a container's PID 1:
```bash
# Send SIGHUP to instruct Nginx or Prometheus to reload configuration
docker kill --signal=HUP my-nginx

# Send SIGUSR1 to trigger log rotation
docker kill --signal=USR1 my-app
```

---

## 6. Practical Diagnostic Demonstrations

### Example 1: Full-Featured Detached Execution
Launch an Nginx container with explicit naming, port publishing, custom environment variable, restart policy, and healthcheck:

```bash
docker run -d \
  --name production-web \
  -p 8080:80 \
  -e APP_ENV=production \
  --restart unless-stopped \
  nginx:alpine
```
Verify runtime parameters:
```bash
docker ps --filter "name=production-web" --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}"
```

### Example 2: Interactive Ephemeral Debugging
Launch a throwaway Alpine container to test DNS resolution, ensuring zero disk clutter upon exit:

```bash
docker run --rm -it alpine nslookup google.com
```
*Notice*: Upon printing the DNS response, the container terminates and is immediately destroyed by `--rm`.

### Example 3: Pipeline Automation with Quiet IDs (`-q`)
Stop and purge all exited containers on a host in a single pipeline command:

```bash
# Purge all containers currently in the exited state
docker rm $(docker ps -a -q --filter "status=exited") 2>/dev/null || echo "No exited containers to remove."
```

### Example 4: Measuring Container UpperDir Disk Usage (`-s`)
Inspect the physical disk space consumed by container writable layers:

```bash
# Start container and write a 20MB file
docker run -d --name size-demo alpine sh -c "dd if=/dev/zero of=/scratch.dat bs=1M count=20; sleep 60"

# Inspect size
docker ps -s --filter "name=size-demo" --format "table {{.Names}}\t{{.Size}}"
```
*Output:*
```text
NAMES        SIZE
size-demo    21MB (virtual 28.8MB)
```
Notice that `size-demo` has written $21\text{ MB}$ to its `UpperDir`, on top of Alpine's base image size!

```bash
# Cleanup
docker rm -f size-demo
```

### Example 5: Testing Stop Grace Periods and Signal Forwarding
Test an application requiring a custom 15-second grace period:

```bash
# Launch container that simulates a 5-second graceful shutdown upon SIGTERM
docker run -d --name grace-demo alpine sh -c '
  trap "echo Draining connections...; sleep 5; echo Done!; exit 0" TERM
  while true; do sleep 1; done
'

# Stop with a 10-second timeout; observe graceful termination in ~5 seconds!
time docker stop -t 10 grace-demo

# Verify exit code is 0 (Clean exit!)
docker inspect --format 'ExitCode: {{.State.ExitCode}}' grace-demo

# Cleanup
docker rm grace-demo
```

---

## 7. Best Practices & Production Guidelines

1. **Always Supply `--name`**: Never rely on Docker's auto-generated random names. Explicit naming is mandatory for monitoring, logging, and operational scripts.
2. **Always Use `--rm` for Batch Jobs**: Always pass `--rm` when running migrations, unit test suites, or one-off CLI tools to prevent disk exhaustion from thousands of dead containers.
3. **Configure `--stop-timeout` for Stateful Services**: Databases (PostgreSQL, MySQL, Redis) and search engines (Elasticsearch) require time to flush memory buffers to disk. Set `--stop-timeout 30` or `--stop-timeout 60` during `docker run`.
4. **Bind Ports to Localhost by Default in Dev**: Avoid `-p 8080:80` on laptops connected to public Wi-Fi networks. Use `-p 127.0.0.1:8080:80` to prevent unauthorized local network access.
5. **Enforce Restart Policies Thoughtfully**: Avoid using `--restart always` for applications that crash upon startup due to configuration errors; this creates an aggressive crash loop that floods logs. Use `--restart on-failure:5`.

---

## 8. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Proper Solution |
|:---|:---|:---|:---|
| **Omitting `-d` on daemons** | Running `docker run nginx` without `-d`. | Locks terminal; closing the terminal sends `SIGHUP` and terminates container. | Always pass `-d` for background services. |
| **Omitting `-it` on interactive shells** | Running `docker run ubuntu bash` without `-it`. | Container immediately exits with code 0 because stdin closed. | Always combine `-it` when launching interactive shells. |
| **Using `docker kill` instead of `docker stop`** | Impatient developers killing containers instantly. | Database tables corrupted, in-flight transactions severed, locks orphaned. | Use `docker stop` to permit graceful draining. |
| **Port mapping confusion (`-p 80:8080`)** | Swapping host port and container port. | Traffic sent to host port 80 fails to reach app listening on 80. | Format is strictly `-p <host_port>:<container_port>`. |
| **Forgetting `docker ps -a` when containers fail** | Running `docker ps` and seeing nothing. | Developer assumes container never ran, missing the recorded error state. | Run `docker ps -a` to locate exited containers and inspect logs. |
| **Leaking stopped containers** | Never removing non-rm containers. | Host runs out of disk space due to thousands of preserved writable layers. | Regularly execute `docker container prune -f`. |

---

## 9. Comparison Table: Lifecycle Management Commands

| Command | Target State | Signal Sent | Disk Layer Preserved? | Resumable? | Typical Use Case |
|:---|:---|:---|:---|:---|:---|
| **`docker run`** | Running | None (Creates) | Creates new `UpperDir` | N/A | Starting new workload |
| **`docker start`** | Running | None (Resumes) | Reuses existing `UpperDir`| N/A | Resuming stopped container |
| **`docker stop`** | Exited | `SIGTERM` $\to$ `SIGKILL` | Yes | Yes (`docker start`) | Graceful system shutdown |
| **`docker kill`** | Exited | `SIGKILL` (Default) | Yes | Yes (`docker start`) | Force terminating hung app |
| **`docker pause`** | Paused | None (cgroup freeze)| Yes | Yes (`unpause`) | Freezing CPU for backup |
| **`docker rm`** | Non-Existent | None (Deletes) | **Permanently Destroyed**| **No** | Purging container resources |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. What is the difference between <code>docker create</code> and <code>docker run</code>?</strong></summary>

- `docker create` initializes the container filesystem (allocating the `UpperDir` writable layer), configures network endpoints, sets up cgroup paths, and writes the OCI bundle to disk, placing the container in the `created` state. It does not spawn the container process.
- `docker run` is a composite command: it internally performs `docker create` immediately followed by `docker start`, initiating process execution and placing the container in the `running` state.
</details>

<details>
<summary><strong>2. How does <code>docker stop</code> handle process termination, and what role does the timeout play?</strong></summary>

`docker stop` attempts a graceful shutdown:
1. It sends `SIGTERM` (or the configured `STOPSIGNAL`) to PID 1 inside the container.
2. It starts a countdown timer (default 10 seconds, configurable via `-t` or `--time`).
3. If the container process catches the signal, finishes cleanup, and terminates before the timer expires, the container exits cleanly with code 0.
4. If the timer reaches 0 and the process is still running, Docker sends `SIGKILL` (signal 9), which the Linux kernel handles directly by instantly killing the process.
</details>

<details>
<summary><strong>3. What is the operational danger of running <code>docker run</code> without the <code>--rm</code> flag for test suites?</strong></summary>

Every time `docker run` executes without `--rm`, a new container directory containing metadata, logs, and a writable `UpperDir` storage layer is allocated in `/var/lib/docker/containers/`. In automated CI/CD runners executing thousands of test runs per day, omitting `--rm` rapidly leads to host disk exhaustion, consuming tens of gigabytes of disk space and exhausting available host inodes.
</details>

<details>
<summary><strong>4. How does the <code>-s</code> (or <code>--size</code>) flag in <code>docker ps</code> calculate container disk usage?</strong></summary>

`docker ps -s` displays two values for each container:
1. **Size**: The physical disk space used by the container's writable layer (`UpperDir`). This represents data newly created, copied-up, or modified by the running container.
2. **Virtual Size**: The cumulative sum of the container's writable layer plus the uncompressed read-only image layers inherited from its base image.
</details>

<details>
<summary><strong>5. Explain the difference between <code>docker restart</code> and <code>docker stop && docker start</code>.</strong></summary>

Functionally, `docker restart` is equivalent to stopping a container and immediately starting it again. However, `docker restart` optimizes the transition inside the daemon: it preserves network IP allocations, avoids tearing down virtual network bridge endpoints if configured with live-restore, and accepts a `-t / --time` flag to control graceful shutdown duration before restarting.
</details>

<details>
<summary><strong>6. How can an administrator send a custom reload signal (such as <code>SIGHUP</code>) to a containerized service?</strong></summary>

An administrator can use `docker kill` with the `--signal` flag:
```bash
docker kill --signal=HUP <container_name_or_id>
```
This instructs `dockerd` to send `SIGHUP` to PID 1 inside the container, allowing web servers like Nginx or Envoy to reload configuration files without dropping active TCP connections.
</details>

<details>
<summary><strong>7. How does Docker handle port publishing when multiple containers attempt to bind to the same host port?</strong></summary>

Docker will reject the second container launch with an error:
`Error response from daemon: driver failed programming external connectivity ... bind: address already in use`.
Because the host network namespace can only have one process listening on a given TCP/UDP port at an IP address, port collisions are rejected by the host kernel socket layer.
</details>

<details>
<summary><strong>8. What happens when you execute <code>docker run -d ubuntu</code>, and why?</strong></summary>

The container will launch, allocate layers, and exit immediately with status `Exited (0)`. 
This happens because the official `ubuntu` image defines `/bin/bash` as its default command. In detached mode (`-d`) without interactive input (`-i`), bash reaches end-of-file (EOF) on standard input immediately and terminates cleanly with exit code 0. Containers require a persistent foreground process to stay in the `running` state.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Filter Containers by Exact Label</strong></summary>

**Goal**: Launch containers with custom labels and query them using `--filter`.

```bash
# Launch labeled containers
docker run -d --name lab-web --label tier=frontend nginx:alpine
docker run -d --name lab-db --label tier=backend alpine sleep 100

# Query only frontend tier
docker ps --filter "label=tier=frontend" --format "table {{.Names}}\t{{.Labels}}"

# Cleanup
docker rm -f lab-web lab-db
```
</details>

<details>
<summary><strong>Exercise 2: Test Interactive Entrypoint Overrides</strong></summary>

**Goal**: Override an image's default entrypoint to run an alternative shell.

```bash
# Override Nginx's entrypoint to inspect Alpine environment
docker run --rm -it --entrypoint /bin/sh nginx:alpine -c "uname -a && cat /etc/os-release"
```
</details>

<details>
<summary><strong>Exercise 3: Measure Graceful Stop Timing</strong></summary>

**Goal**: Observe the difference between stopping with a 2-second timeout versus standard 10 seconds.

```bash
# Launch container ignoring SIGTERM
docker run -d --name ignore-term alpine sh -c "trap '' TERM; while true; do sleep 1; done"

# Measure time to stop with 2-second timeout
time docker stop -t 2 ignore-term

# Cleanup
docker rm ignore-term
```
</details>

<details>
<summary><strong>Exercise 4: Query Container Process Count with Go Formatting</strong></summary>

**Goal**: Output custom process and network tables using `docker ps --format`.

```bash
docker ps --format "Container: {{.Names}} is running image: {{.Image}} on ports: {{.Ports}}"
```
</details>

<details>
<summary><strong>Exercise 5: Simulate Configuration Reload via SIGHUP</strong></summary>

**Goal**: Send `SIGHUP` to Nginx and inspect container logs to verify reload.

```bash
# Start Nginx
docker run -d --name reload-lab -p 8094:80 nginx:alpine

# Send SIGHUP
docker kill --signal=HUP reload-lab

# Check logs to observe graceful configuration reload
docker logs reload-lab | tail -n 5

# Cleanup
docker rm -f reload-lab
```
</details>

<details>
<summary><strong>Exercise 6: Bulk Removal of Stopped Containers</strong></summary>

**Goal**: Use shell command substitution with `docker ps -aq` to purge dead containers.

```bash
# Create two exited containers
docker run --name dead-1 alpine echo "done"
docker run --name dead-2 alpine echo "done"

# Bulk remove exited containers
docker rm $(docker ps -a -q --filter "status=exited")

# Verify all are removed
docker ps -a --filter "name=dead"
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                         RUN, PS, STOP COMMAND CHEAT SHEET                             |
+---------------------------------------------------------------------------------------+
| Run in Background:     docker run -d --name <name> -p <h>:<c> <image>                 |
| Run Interactively:     docker run --rm -it <image> /bin/sh                            |
| Restart Policy:        docker run --restart unless-stopped <image>                    |
| List Active:           docker ps                                                      |
| List All + Sizes:      docker ps -as                                                  |
| Filter by Status:      docker ps -a --filter "status=exited"                          |
| Filter by Exit Code:   docker ps -a --filter "exited=137"                             |
| Graceful Stop:         docker stop -t <sec> <name> (SIGTERM -> SIGKILL)               |
| Force Kill:            docker kill <name> (Immediate SIGKILL)                         |
| Send Custom Signal:    docker kill --signal=HUP <name>                                |
| Purge Exited:          docker rm $(docker ps -aq -f status=exited)                    |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Docker CLI Command Reference: docker run](https://docs.docker.com/reference/cli/docker/container/run/)
- [Docker CLI Command Reference: docker ps](https://docs.docker.com/reference/cli/docker/container/ps/)
- [Docker CLI Command Reference: docker stop](https://docs.docker.com/reference/cli/docker/container/stop/)
- [Docker CLI Command Reference: docker kill](https://docs.docker.com/reference/cli/docker/container/kill/)
- [Linux Signal Handling and Process Supervision Concepts](https://man7.org/linux/man-pages/man7/signal.7.html)

---

| [< Previous: Module 01 Fundamentals](../01-fundamentals/README.md) | [Next: 02-logs-exec-inspect.md](./02-logs-exec-inspect.md) > |
| :--- | :--- |
