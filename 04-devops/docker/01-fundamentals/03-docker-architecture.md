---
title: "Docker Engine Architecture: CLI, dockerd, containerd, containerd-shim, and runc"
description: "Comprehensive architectural breakdown of Docker Engine: client-server REST API, daemon internals, containerd gRPC architecture, containerd-shim lifecycle, and runc OCI execution."
category: "DevOps / Containers"
tags: ["docker", "architecture", "dockerd", "containerd", "shim", "runc", "oci", "engine"]
difficulty: "Intermediate"
prerequisites: ["01-docker-vs-vm.md", "02-images-and-containers.md", "Understanding of Linux processes and IPC"]
estimated_read_time: "25 min"
---

# Docker Engine Architecture: CLI, dockerd, containerd, containerd-shim, and runc

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. Core Architectural Components](#2-core-architectural-components)
- [3. How It Works Internally: The End-to-End Execution Flow](#3-how-it-works-internally-the-end-to-end-execution-flow)
- [4. Deep Dive into the Subsystems](#4-deep-dive-into-the-subsystems)
- [5. Practical Diagnostic Demonstrations](#5-practical-diagnostic-demonstrations)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table: Engine Components & Responsibilities](#9-comparison-table-engine-components--responsibilities)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

In its initial releases (2013), Docker was a single monolithic binary written in Go that managed everything: command-line parsing, REST APIs, image pulling, network bridge creation, volume mounts, process execution, and logging.

Over time, this monolithic architecture proved unmaintainable and dangerous for enterprise production: restarting the Docker daemon to apply a minor bug fix or update would forcefully kill all running customer containers on the host. Furthermore, the broader cloud-native ecosystem required standardized, interoperable building blocks that could be integrated directly into Kubernetes without carrying Docker's user-facing CLI abstractions.

Between 2015 and 2018, Docker led a massive architectural refactoring:
1. Low-level container execution was spun out into the **Open Container Initiative (OCI)** reference implementation: **`runc`**.
2. Mid-level container supervision and image distribution were refactored into **`containerd`** (donated to the CNCF, where it graduated).
3. The process isolation shim layer was decoupled into **`containerd-shim`**.
4. The user-facing daemon **`dockerd`** became a high-level orchestration coordinator handling developer UX, build systems (BuildKit), volume drivers, and `libnetwork`.

Mastering this layered architecture is essential for diagnosing container crashes, understanding memory footprints, and debugging complex production orchestration issues.

```
+---------------------------------------------------------------------------------+
|                       MODERN DOCKER ENGINE ARCHITECTURE                         |
|                                                                                 |
|  [User Terminal / CI Pipeline]                                                  |
|        |                                                                        |
|        | docker run -d -p 8080:80 nginx                                         |
|        v                                                                        |
|  +--------------------+                                                         |
|  |     Docker CLI     | (Client: parses flags, formats JSON request)            |
|  +---------+----------+                                                         |
|            |                                                                    |
|            | REST API over /var/run/docker.sock                                 |
|            v                                                                    |
|  +--------------------+                                                         |
|  |   dockerd Daemon   | (Coordinates high-level services: volumes, networking)  |
|  |                    | (BuildKit, local image cache, authentication, API)     |
|  +---------+----------+                                                         |
|            |                                                                    |
|            | gRPC over /run/containerd/containerd.sock                          |
|            v                                                                    |
|  +--------------------+                                                         |
|  |     containerd     | (CNCF Core Runtime: image pulling, unpacks layers)      |
|  |                    | (Content store, manages execution task lifecycles)      |
|  +---------+----------+                                                         |
|            |                                                                    |
|            | Spawns per-container supervisor via fork/exec                      |
|            v                                                                    |
|  +--------------------+       +--------------------+                            |
|  |  containerd-shim   |       |  containerd-shim   | (Keeps pipes/TTY open)     |
|  +---------+----------+       +---------+----------+ (Reaps zombies, handles    |
|            |                            |             daemon restarts)          |
|            | Invokes runc               |                                       |
|            v                            v                                       |
|  +--------------------+       +--------------------+                            |
|  |    runc (exits)    |       |    runc (exits)    | (OCI CLI: configures Linux |
|  +---------+----------+       +---------+----------+  namespaces/cgroups,       |
|            |                            |             pivots root, execs app)   |
|            v clone()                    v clone()                               |
|  +--------------------+       +--------------------+                            |
|  |   Nginx (PID 1)    |       |   Redis (PID 1)    | (Application processes     |
|  |   Container App    |       |   Container App    |  running in isolated NS)   |
|  +--------------------+       +--------------------+                            |
+---------------------------------------------------------------------------------+
```

---

## 2. Core Architectural Components

- **Docker CLI (`docker`)**: The command-line interface binary executed by developers. It translates CLI flags into JSON REST payloads and transmits them to the Docker daemon over a UNIX domain socket or TCP connection.
- **Docker Daemon (`dockerd`)**: The persistent background service. It implements the Docker Engine REST API, coordinates storage drivers (OverlayFS), manages virtual bridge networks (`libnetwork`), stores image metadata, and communicates with `containerd`.
- **`containerd`**: A high-performance, CNCF-graduated container runtime. It manages the complete container lifecycle of its host system: image transfer and storage, container execution and supervision, low-level storage attachments, and network attachments.
- **`containerd-shim`**: A dedicated, lightweight supervisor process spawned for every running container. It isolates the running container process from `containerd` and `dockerd`, keeping standard I/O streams and file descriptors open even if the main Docker daemon is killed or restarted.
- **`runc`**: The universal low-level OCI reference runtime. Written in Go, it accepts an OCI `config.json` bundle, interacts directly with the Linux kernel to configure namespaces and cgroups, pivots root, executes the target binary, and exits immediately.

---

## 3. How It Works Internally: The End-to-End Execution Flow

When a user executes `docker run -d --name web -p 8080:80 nginx:alpine`, the components coordinate through a strict sequence:

```
[CLI] ---> (REST / POST) ---> [dockerd]
                                 |
                                 | 1. Resolves image; checks local content store
                                 | 2. Allocates network endpoints from libnetwork
                                 |
                              [gRPC API]
                                 |
                                 v
                            [containerd]
                                 |
                                 | 3. Unpacks image layers into snapshot
                                 | 4. Generates OCI config.json bundle
                                 | 5. Spawns containerd-shim
                                 |
                                 v
                        [containerd-shim]
                                 |
                                 | 6. Spawns runc create <container-id>
                                 |
                                 v
                              [runc]
                                 |
                                 | 7. Calls clone(CLONE_NEWPID | CLONE_NEWNET | ...)
                                 | 8. Applies cgroups resource limits
                                 | 9. Calls pivot_root() into merged OverlayFS
                                 | 10. Calls execve("/docker-entrypoint.sh", ...)
                                 | 11. runc EXITS immediately!
                                 |
                                 v
                        [Nginx Master Process]
                                 ^
                                 | (Monitored directly by containerd-shim)
                        [containerd-shim]
```

---

## 4. Deep Dive into the Subsystems

### 4.1. The Role of `containerd-shim`
Why have an extra process (`containerd-shim`) between `containerd` and the container?
1. **Daemonless Restarts (Live-Restore)**: If `dockerd` or `containerd` crashes, is restarted, or is upgraded, the containers keep running uninterrupted because their standard I/O pipes (stdin/stdout/stderr) are held by the shim process, not by `dockerd`.
2. **Reaping Zombie Processes**: If the application running inside the container spawns child processes that exit without being reaped by their parent, `containerd-shim` serves as the sub-reaper, preventing zombie processes (`<defunct>`) from saturating the host PID table.
3. **Exit Code Persistence**: The shim holds the exit status of the container process after it terminates so that `containerd` can query it asynchronously even if `dockerd` was offline at the moment of exit.

### 4.2. Storage Drivers (`libcontainer` & OverlayFS)
`dockerd` manages image layers and storage drivers. When a container starts, `containerd` creates an active snapshot via the configured storage driver (modern default is `overlay2`), composing the immutable image layers (`LowerDir`) and the scratch layer (`UpperDir`) into a unified virtual root (`MergedDir`).

### 4.3. Networking (`libnetwork`)
`dockerd` uses `libnetwork` to implement the Container Network Model (CNM):
- Creates Linux bridge devices (e.g., `docker0`).
- Generates `veth` (virtual ethernet) interface pairs, moving one end into the container's network namespace and attaching the other end to the bridge.
- Programs Linux kernel `iptables` and `nftables` NAT rules to handle incoming traffic routing and port forwarding.

---

## 5. Practical Diagnostic Demonstrations

### Example 1: Tracing the Process Hierarchy from the Host
Inspect the live Linux process tree to visualize the exact relationship between `dockerd`, `containerd`, `containerd-shim`, and the containerized application.

```bash
# 1. Run a container
docker run -d --name arch-demo alpine sleep 3600

# 2. Trace the process tree from the host OS
pstree -p $(pgrep dockerd)
```
*Output (Linux Host):*
```text
dockerd(1204)───containerd(1312)───containerd-shim(18920)───sleep(18945)
```
Notice:
- `dockerd` is the parent of `containerd`.
- `containerd` spawned `containerd-shim`.
- `containerd-shim` is the direct parent of `sleep` (the container process).
- `runc` is nowhere to be seen—because it finished its setup work and exited immediately!

```bash
# Cleanup
docker rm -f arch-demo
```

### Example 2: Inspecting the Raw UNIX Domain Socket
Demonstrate that the Docker CLI is merely an HTTP client communicating with `dockerd` over `/var/run/docker.sock` using `curl`:

```bash
# Query the Docker daemon REST API directly using curl over the UNIX socket!
sudo curl --unix-socket /var/run/docker.sock http://localhost/version
```
*Output (Raw JSON response from dockerd):*
```json
{
  "Platform": {"Name": "Docker Engine - Community"},
  "Version": "29.8.0",
  "ApiVersion": "1.56",
  "MinAPIVersion": "1.24",
  "GitCommit": "af61852",
  "GoVersion": "go1.22.12",
  "Os": "linux",
  "Arch": "amd64",
  "KernelVersion": "6.8.0-45-generic"
}
```

### Example 3: Inspecting `containerd` Directly with `ctr`
Query `containerd`'s internal storage and tasks directly using its bundled debugging tool (`ctr`), completely bypassing `dockerd`:

```bash
# List containerd namespaces managed by Docker
sudo ctr --address /run/containerd/containerd.sock namespaces ls

# List images cached inside containerd's "moby" namespace
sudo ctr --address /run/containerd/containerd.sock --namespace moby images ls -q
```

### Example 4: Verifying Live-Restore Behavior
Prove that restarting the Docker daemon does not terminate running containers when `live-restore` is enabled:

```bash
# 1. Ensure live-restore is enabled in /etc/docker/daemon.json
sudo python3 -c '
import json, os
path = "/etc/docker/daemon.json"
cfg = json.load(open(path)) if os.path.exists(path) else {}
cfg["live-restore"] = True
with open(path, "w") as f:
    json.dump(cfg, f, indent=2)
'
sudo systemctl reload docker

# 2. Start a container that tracks uptime
docker run -d --name uptime-tracker alpine sh -c "while true; do date; sleep 1; done"

# 3. Restart the system Docker service
sudo systemctl restart docker

# 4. Verify that the container remained running throughout the daemon restart!
docker ps --filter "name=uptime-tracker"
# Notice the container has NOT restarted; Status shows "Up X seconds" continuing uninterrupted!

# Cleanup
docker rm -f uptime-tracker
```

### Example 5: Inspecting the Generated OCI Specification Bundle
Observe the OCI `config.json` that `containerd` passes to `runc` to execute a container:

```bash
# 1. Run a container
docker run -d --name oci-spec-lab alpine sleep 100

# 2. Extract container ID
CID=$(docker inspect --format '{{.Id}}' oci-spec-lab)

# 3. Inspect the containerd state bundle for this container (requires sudo)
sudo cat /run/containerd/io.containerd.runtime.v2.task/moby/$CID/config.json | python3 -m json.tool | head -n 35
```
*Output reveals raw OCI configuration*: namespaces (`pid`, `network`, `mount`), resource limits, capabilities (`CAP_CHOWN`, `CAP_NET_BIND_SERVICE`), and process arguments.

```bash
# Cleanup
docker rm -f oci-spec-lab
```

---

## 6. Best Practices & Production Guidelines

1. **Always Enable `"live-restore": true`**: In production Linux hosts, always configure `live-restore` in `/etc/docker/daemon.json`. This guarantees that routine Docker daemon upgrades or daemon crashes do not drop live network connections or kill mission-critical containers.
2. **Never Expose `/var/run/docker.sock` to Containers**: Mount-binding the host Docker socket into an application container (`-v /var/run/docker.sock:/var/run/docker.sock`) grants that container full root control over the host.
3. **Monitor `containerd` and `dockerd` Separately**: In enterprise observability stacks, collect metrics from both `dockerd` (API latency, volume attachments) and `containerd` (task creation latency, snapshot unpack time).
4. **Use `systemd` cgroup Driver**: Align Docker with the host systemd cgroup hierarchy by setting `"exec-opts": ["native.cgroupdriver=systemd"]` to avoid split-brain memory accounting between kernel services and container runtimes.
5. **Tune Daemon Ulimits**: Configure system-wide default open file limits (`nofile`) in `daemon.json` so microservices handling heavy concurrent connections do not hit `Too many open files` errors.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Consequence | Proper Fix |
|:---|:---|:---|:---|
| **Mounting `/var/run/docker.sock` inside app containers** | Allowing an app (like Jenkins or Portainer) to spawn sibling containers. | An attacker who compromises the app container gains instant host root access. | Use Docker-in-Docker (DinD), Kaniko for builds, or remote SSH contexts. |
| **Omitting `live-restore` in production** | Default Docker configuration lacks live-restore. | Upgrading the `docker-ce` package kills all running production containers. | Add `"live-restore": true` to `/etc/docker/daemon.json`. |
| **Using `cgroupfs` driver on systemd hosts** | Defaulting to cgroupfs driver on modern Linux distributions. | Two distinct cgroup managers fight over resources, causing erratic OOM behavior. | Configure `"exec-opts": ["native.cgroupdriver=systemd"]`. |
| **Assuming `runc` stays running** | Looking for `runc` in process tables to measure container overhead. | `runc` exits immediately after container execution; monitoring it yields nothing. | Monitor `containerd-shim` and the container application process itself. |
| **Killing `containerd-shim` directly** | Terminating shim processes with `kill -9`. | Container process becomes orphaned or terminates without proper cleanup in `containerd`. | Use `docker stop` or `docker rm -f` to trigger orderly teardowns. |
| **Unbounded JSON log accumulation** | Default logging configuration stores logs infinitely. | Host `/var/lib/docker/` fills 100% disk capacity, freezing the daemon. | Set `max-size: 50m` and `max-file: 3` in `daemon.json`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Zero Runtime Virtualization Tax**: Because `runc` exits after initialization and `containerd-shim` consumes less than $2\text{ MB}$ of resident memory, Docker Engine introduces virtually zero CPU or memory penalty during steady-state container execution.
- **REST vs gRPC Overhead**: Internal communication between `dockerd` and `containerd` uses gRPC with Protobuf serialization over UNIX sockets, executing in microseconds.

### Security
- **Daemon Socket Attack Surface**: Write access to `/var/run/docker.sock` is equivalent to passwordless root access on the host. Secure it with file permissions (`root:docker`, `0660`) or mutual TLS.
- **Rootless Engine**: In Rootless Mode, `dockerd`, `containerd`, and `containerd-shim` execute inside an unprivileged user namespace, completely neutralizing host root compromise risks.

### Cost
- **High Resource Efficiency**: A single high-capacity Linux host can run thousands of `containerd-shim` instances simultaneously without degrading kernel scheduling performance.

---

## 9. Comparison Table: Engine Components & Responsibilities

| Component | Layer / Scope | Binary Name | Protocol / Interface | Primary Responsibility |
|:---|:---|:---|:---|:---|
| **Docker CLI** | User Space Client | `docker` | HTTP / REST API | Parses commands, UX, renders output |
| **Docker Daemon** | High-Level Management | `dockerd` | REST API / gRPC | Builds, networks, volumes, auth, plugins |
| **containerd** | Core Runtime | `containerd` | gRPC over UNIX socket| Image pulling, snapshotting, task supervisor |
| **containerd-shim**| Process Supervisor | `containerd-shim-runc-v2` | IPC / Pipes | Reaps zombies, holds I/O, daemonless survival|
| **runc** | Low-Level OCI Runtime | `runc` | OCI Spec CLI | Invokes `clone()`, cgroups, `pivot_root` |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. Explain the architectural chain of events from running <code>docker run</code> to your code executing.</strong></summary>

1. **Docker CLI** translates CLI arguments into a REST HTTP POST payload sent over `/var/run/docker.sock`.
2. **dockerd** receives the request, validates authentication, coordinates volume attachments and network endpoint creation via `libnetwork`.
3. **dockerd** issues a gRPC request to **containerd** to prepare the execution task.
4. **containerd** retrieves the image layers, unpacks them via the storage snapshotter (OverlayFS), and creates an OCI `config.json` bundle.
5. **containerd** launches a dedicated **containerd-shim** process.
6. **containerd-shim** calls **runc** with the bundle path (`runc create`).
7. **runc** invokes the Linux `clone()` system call with namespace flags, sets up cgroups resource limits, mounts the root filesystem, pivots root (`pivot_root`), and calls `execve()` to replace itself with the container's entrypoint binary.
8. **runc** exits, leaving the container process supervised directly by **containerd-shim**.
</details>

<details>
<summary><strong>2. Why was <code>runc</code> designed to exit immediately after launching a container?</strong></summary>

`runc` is designed according to the Unix philosophy of doing one thing well: it is a transient tool to configure kernel namespaces, cgroups, and execute the container binary. If `runc` remained running for the entire lifetime of every container, the host would waste memory holding hundreds of idle Go runtime instances. By exiting immediately, memory overhead is eliminated, leaving only the lightweight C-based or minimal Go `containerd-shim` to supervise I/O.
</details>

<details>
<summary><strong>3. What is the "Live-Restore" feature, and what architectural component makes it possible?</strong></summary>

"Live-Restore" allows containers to remain running without interruption when the `dockerd` daemon crashes, shuts down, or restarts during system updates. This is made possible by **`containerd-shim`**. Because the shim process is the direct parent of the container process and holds open its standard I/O file descriptors, the termination of `dockerd` or `containerd` does not close standard I/O pipes or trigger `SIGHUP`/`SIGTERM` to the container process.
</details>

<details>
<summary><strong>4. How does <code>dockerd</code> communicate with <code>containerd</code>?</strong></summary>

`dockerd` communicates with `containerd` via **gRPC** over a local UNIX domain socket (typically located at `/run/containerd/containerd.sock`). It uses Protocol Buffers (protobuf) to serialize API calls for image transfer, snapshot management, and task lifecycles.
</details>

<details>
<summary><strong>5. What is the difference between an OCI Runtime Specification and an OCI Image Specification?</strong></summary>

- The **OCI Image Specification** defines how container images are packaged, archived as tarballs, cryptographically hashed via content digests, and described via JSON manifests and configuration descriptors.
- The **OCI Runtime Specification** defines how an unpacked image bundle on disk (consisting of a `rootfs` directory and a `config.json` file) must be executed by an OCI runtime (`runc`, `crun`), defining namespace setup, cgroups constraints, mount options, and lifecycle hooks.
</details>

<details>
<summary><strong>6. Why is mounting <code>/var/run/docker.sock</code> into a container considered a critical security vulnerability?</strong></summary>

The Docker daemon socket provides unrestricted access to the Docker REST API. A container that mounts `/var/run/docker.sock` can issue requests to the daemon to create a new privileged container mounting the host's root filesystem:
```bash
docker run -v /:/host -it alpine chroot /host
```
This allows an attacker to bypass all container isolation, read host `/etc/shadow`, install backdoors, and gain full root control over the underlying physical host.
</details>

<details>
<summary><strong>7. What role does <code>containerd-shim</code> play in handling zombie processes?</strong></summary>

In Unix systems, when a child process terminates, it remains in the process table as a "zombie" until its parent calls `wait()` or `waitpid()` to read its exit code. If an application inside a container spawns child processes and fails to reap them, and PID 1 terminates, the orphaned processes are reparented. `containerd-shim` registers itself as a sub-reaper via `prctl(PR_SET_CHILD_SUBREAPER, 1)`, ensuring that orphaned zombie processes are reparented to the shim and reaped immediately, preventing host PID table exhaustion.
</details>

<details>
<summary><strong>8. What is the difference between <code>runc</code> and <code>crun</code>?</strong></summary>

- **`runc`** is the original OCI reference runtime developed in Go by Docker and the OCI community.
- **`crun`** is an alternative OCI runtime written in pure C by Red Hat. Because it avoids the Go runtime and garbage collector overhead, `crun` is significantly smaller, starts containers up to $2\times$ faster, uses less memory, and natively supports modern cgroups v2 features.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Query Docker Daemon API Directly via cURL</strong></summary>

**Goal**: Inspect active containers by issuing raw HTTP requests to `/var/run/docker.sock`.

```bash
# Query active containers using the REST API over UNIX socket
sudo curl -s --unix-socket /var/run/docker.sock http://localhost/containers/json | python3 -m json.tool
```
</details>

<details>
<summary><strong>Exercise 2: Trace Process Tree Hierarchy with `ps`</strong></summary>

**Goal**: Identify the exact PID of `dockerd`, `containerd`, `containerd-shim`, and your application.

```bash
# Start background container
docker run -d --name shim-lab alpine sleep 300

# Inspect PID from container metadata
CPID=$(docker inspect --format '{{.State.Pid}}' shim-lab)

# Inspect parent PID hierarchy on host
ps -f -p $CPID
# Notice that the PPID (parent PID) points to containerd-shim!

# Cleanup
docker rm -f shim-lab
```
</details>

<details>
<summary><strong>Exercise 3: Inspect containerd Tasks via `ctr`</strong></summary>

**Goal**: Use `ctr` to observe how containerd tracks the execution task independently of Docker.

```bash
# Run container in Docker
docker run -d --name ctr-demo alpine sleep 120

# Query containerd task directly
sudo ctr --address /run/containerd/containerd.sock --namespace moby tasks ls

# Cleanup
docker rm -f ctr-demo
```
</details>

<details>
<summary><strong>Exercise 4: Verify Sub-Reaper Zombie Management</strong></summary>

**Goal**: Confirm that `containerd-shim` reaps orphaned zombie processes.

```bash
# Run container that spawns a background child and exits the immediate parent
docker run --rm alpine sh -c "(sleep 1 &); sleep 2"
```
Notice that the system cleanly terminates without leaving orphan processes hanging on the host.
</details>

<details>
<summary><strong>Exercise 5: Inspect Host iptables Rules Programmed by `libnetwork`</strong></summary>

**Goal**: View the NAT rules injected by `dockerd` when publishing container ports.

```bash
# Start web server publishing port 8765
docker run -d --name net-trace -p 8765:80 nginx:alpine

# Inspect iptables NAT table for DOCKER chain rules (requires sudo)
sudo iptables -t nat -L DOCKER -n -v | grep 8765

# Cleanup
docker rm -f net-trace
```
</details>

<details>
<summary><strong>Exercise 6: Test Daemon Reload without Container Disruption</strong></summary>

**Goal**: Reload `/etc/docker/daemon.json` configuration without restarting the daemon process.

```bash
# Send SIGHUP to dockerd to reload configuration safely
sudo systemctl reload docker
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                         DOCKER ENGINE ARCHITECTURE CHEAT SHEET                        |
+---------------------------------------------------------------------------------------+
| Docker CLI:            Sends HTTP REST JSON payloads to /var/run/docker.sock          |
| dockerd:               Coordinates builds, volumes, networks, high-level API          |
| containerd:            Pulls images, unpacks rootfs, manages tasks over gRPC          |
| containerd-shim:       Holds open stdio/pipes, reaps zombies, enables live-restore    |
| runc:                  OCI CLI tool; sets up namespaces/cgroups, execs, and exits     |
| Direct API Query:      curl --unix-socket /var/run/docker.sock http://localhost/info  |
| Debug containerd:      ctr --namespace moby tasks ls                                  |
| Live-Restore:          "live-restore": true in /etc/docker/daemon.json (Mandatory!)   |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Docker Documentation: Docker Architecture Overview](https://docs.docker.com/get-started/overview/#docker-architecture)
- [containerd Official Architecture Design and Specifications](https://github.com/containerd/containerd/blob/main/docs/design/architecture.md)
- [Open Container Initiative (OCI) Runtime Specification](https://github.com/opencontainers/runtime-spec)
- [runc Official GitHub Repository](https://github.com/opencontainers/runc)
- [Docker Engine Source Code Repository (Moby Project)](https://github.com/moby/moby)

---

| [< Previous: 02-images-and-containers.md](./02-images-and-containers.md) | [Next: 04-oci-standards-and-runtimes.md](./04-oci-standards-and-runtimes.md) > |
| :--- | :--- |
