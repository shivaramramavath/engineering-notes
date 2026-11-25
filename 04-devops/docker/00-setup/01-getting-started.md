---
title: "Docker Getting Started: Installation, Verification, and Mental Model"
description: "Comprehensive introduction to Docker containerization: prerequisites, architecture overview, daemon verification, and running your first containerized workload."
category: "DevOps / Containers"
tags: ["docker", "containers", "getting-started", "devops", "setup"]
difficulty: "Beginner"
prerequisites: ["Basic Linux shell commands", "Understanding of client-server architecture"]
estimated_read_time: "25 min"
---

# Docker Getting Started: Installation, Verification, and Mental Model

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. Core Concepts & Terminology](#2-core-concepts--terminology)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table: Containers vs Virtual Machines](#9-comparison-table-containers-vs-virtual-machines)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

Docker is an open-source platform that enables developers to build, package, ship, and run distributed applications in lightweight, isolated runtime environments called **containers**. 

Before Docker gained mainstream adoption in 2013, software delivery suffered from the ubiquitous *"it works on my machine"* syndrome: differences in operating system versions, shared dynamic libraries, kernel configurations, system users, and environment variables made transferring code from developer workstations to staging and production clusters fragile, error-prone, and slow.

Traditional hardware virtualization through Virtual Machines (VMs) resolved consistency by virtualizing entire hardware stacks and booting independent guest operating systems. However, this introduced severe overhead in CPU, memory, disk usage, and startup time. Docker revolutionized deployment by leveraging Linux kernel primitives—specifically **Namespaces** for process isolation and **Control Groups (cgroups)** for resource allocation—allowing hundreds of distinct applications to share a single host OS kernel safely without sacrificing performance.

```
+-------------------------------------------------------------+
|                     TRADITIONAL VIRTUAL MACHINE             |
|  [App A]      [App B]      [App C]                          |
|  [Bins/Libs]  [Bins/Libs]  [Bins/Libs]                      |
|  [Guest OS]   [Guest OS]   [Guest OS]  <-- Heavyweight      |
|  +-----------------------------------+                      |
|  |     Hypervisor (Type 1 or 2)      |                      |
|  +-----------------------------------+                      |
|  |     Host OS & Physical Hardware   |                      |
+-------------------------------------------------------------+

+-------------------------------------------------------------+
|                     DOCKER CONTAINER STACK                  |
|  [App A]         [App B]         [App C]                    |
|  [Bins/Libs]     [Bins/Libs]     [Bins/Libs]                |
|  +-----------------------------------------+                |
|  | Docker Engine (containerd + runc)       |                |
|  +-----------------------------------------+                |
|  | Host OS Kernel (Namespaces + cgroups)   | <-- Shared     |
|  +-----------------------------------------+                |
|  | Physical Hardware / Cloud Infrastructure|                |
+-------------------------------------------------------------+
```

---

## 2. Core Concepts & Terminology

Understanding Docker requires mastering several distinct concepts:

- **Docker Image**: An immutable, read-only template consisting of layered filesystems and metadata (entrypoints, exposed ports, environment variables) that define what a container executes.
- **Docker Container**: A running or stopped stateful instance of an image. It consists of the read-only image layers topped with a thin writable container layer (`UpperDir`), executing as isolated processes directly on the host kernel.
- **Dockerfile**: A plain-text configuration file containing sequential directives (`FROM`, `COPY`, `RUN`, `CMD`) used by the Docker builder (`BuildKit`) to produce an image.
- **Docker Daemon (`dockerd`)**: The persistent background process on the host system responsible for managing Docker images, containers, networks, and storage volumes.
- **Docker CLI (`docker`)**: The command-line client program used by developers and CI/CD pipelines to send commands to the daemon via a local UNIX domain socket or remote REST API.
- **Container Registry**: A centralized repository system (e.g., Docker Hub, AWS ECR, GitHub Packages, self-hosted Harbor) for distributing, versioning, and storing container images.
- **containerd & runc**: Under the hood, Docker delegates container lifecycle management to `containerd` (a CNCF-graduated container runtime) and OCI-compliant low-level runtimes such as `runc`.

---

## 3. How It Works Internally

The Docker Engine is structured as a client-server architecture. When a user runs a CLI command, communication occurs over an IPC socket (`/var/run/docker.sock` on Linux or named pipes on Windows).

```
+------------------+         UNIX Domain Socket          +----------------------+
|    Docker CLI    |  -------------------------------->  |    Docker Daemon     |
|  (User Terminal) |  <--------------------------------  |      (dockerd)       |
+------------------+          REST API Responses         +----------+-----------+
                                                                    |
                                                                    | gRPC API
                                                                    v
                                                         +----------------------+
                                                         |      containerd      |
                                                         +----------+-----------+
                                                                    |
                                             +----------------------+----------------------+
                                             |                                             |
                                             v                                             v
                                   +-------------------+                         +-------------------+
                                   |  containerd-shim  |                         |  containerd-shim  |
                                   +---------+---------+                         +---------+---------+
                                             | fork/exec                                   | fork/exec
                                             v                                             v
                                   +-------------------+                         +-------------------+
                                   |   runc (exits)    |                         |   runc (exits)    |
                                   +---------+---------+                         +---------+---------+
                                             | clone()                                     | clone()
                                             v                                             v
                                   +-------------------+                         +-------------------+
                                   |  Process (App 1)  |                         |  Process (App 2)  |
                                   |  [PID/NET/MNT NS] |                         |  [PID/NET/MNT NS] |
                                   +-------------------+                         +-------------------+
```

### Key Execution Phases:
1. **Invocation**: The user invokes `docker run -d -p 8080:80 nginx:alpine`.
2. **API Dispatch**: The Docker CLI serializes the request into an HTTP POST payload sent over `/var/run/docker.sock`.
3. **Image Resolution**: `dockerd` checks the local content store. If `nginx:alpine` is missing, it contacts the configured registry, pulls the compressed layer blobs, verifies SHA256 checksums, and unpacks them into the backing storage driver (OverlayFS).
4. **OCI Spec Generation**: `dockerd` prepares an Open Container Initiative (OCI) `config.json` detailing namespaces, cgroups, environment variables, mounts, and capabilities.
5. **Runtime Hand-off**: `containerd` launches a lightweight supervisor process (`containerd-shim`) which invokes `runc`.
6. **Kernel Execution**: `runc` executes the Linux `clone()` syscall with flags such as `CLONE_NEWPID`, `CLONE_NEWNET`, `CLONE_NEWNS`, sets up cgroups limits, pivots root (`pivot_root`) into the container's root filesystem, drops privileges, and executes the target entrypoint. `runc` exits immediately, leaving the container process under `containerd-shim`.

---

## 4. Syntax & Usage

The modern Docker CLI organizes commands into top-level management objects:

```bash
docker <management-command> <sub-command> [OPTIONS]
```

### Essential First Commands:

```bash
# Verify client and server engine versions
docker version

# Display system-wide Docker host configuration and resource details
docker info

# Run a canonical test image to verify setup
docker run hello-world

# List active containers
docker ps

# List all containers (running, stopped, exited)
docker ps -a

# List locally cached images
docker image ls
```

---

## 5. Practical Examples

### Example 1: Verifying Daemon Connectivity & Capabilities
Inspect your running engine configuration and ensure BuildKit and experimental features are enabled.

```bash
# Check client and server version details
docker version

# Check storage driver, kernel version, and cgroup version
docker info --format 'Storage Driver: {{.Driver}} | Cgroup: {{.CgroupVersion}} | Containers: {{.Containers}}'
```
*Output (Linux Host):*
```text
Storage Driver: overlay2 | Cgroup: 2 | Containers: 0
```

### Example 2: Pulling, Inspecting, and Running an Ephemeral Web Server
Pull a lightweight static web server, expose port 80 to host port 8080, and ensure automatic cleanup upon termination.

```bash
# Pull the Alpine-based Nginx image
docker pull nginx:alpine

# Run in background (-d), remove on exit (--rm), map port 8080 to container 80
docker run -d --rm --name test-nginx -p 8080:80 nginx:alpine

# Verify HTTP connectivity from host
curl -I http://localhost:8080

# View container logs
docker logs test-nginx

# Stop container (it will auto-remove because of --rm)
docker stop test-nginx
```

### Example 3: Running an Interactive Troubleshooting Shell
Spawn an ephemeral container with an interactive TTY to probe network or filesystem behavior.

```bash
# Launch interactive container with bash/sh
docker run --rm -it alpine:latest /bin/sh

# Inside container:
/ # uname -a
/ # ps aux
/ # exit
```

### Example 4: Environment Variables and Working Directory Injection
Demonstrate passing dynamic configuration into a runtime container process.

```bash
docker run --rm \
  -e APP_ENV=production \
  -e DB_TIMEOUT=30 \
  -w /opt/app \
  alpine:latest \
  sh -c "echo 'Working directory:' \$(pwd) && echo 'Environment:' \$APP_ENV"
```
*Output:*
```text
Working directory: /opt/app
Environment: production
```

### Example 5: Resource-Constrained Container Execution
Execute a memory-limited process to observe control group enforcement.

```bash
# Limit container to 128MB RAM and 0.5 CPU core
docker run --rm -it \
  --memory="128m" \
  --cpus="0.5" \
  alpine:latest \
  sh -c "echo 'Memory limit active' && free -m"
```

---

## 6. Best Practices & Production Guidelines

1. **Always Pin Image Tags with Digests or SemVer**: Never rely on `latest` in production manifests or scripts. Use explicit tags like `python:3.12.2-slim-bookworm` or include the content digest (`@sha256:...`).
2. **Never Run as Root in Containers**: By default, container processes run as UID 0 (root). If an attacker breaks out of the container, they inherit root privileges on the host. Always specify a non-root user (`USER appuser`).
3. **Use `--rm` for Interactive / Ephemeral Jobs**: Prevent dangling stopped containers from consuming disk space on development machines and CI runners.
4. **Leverage Docker Healthchecks**: Define healthchecks inside Dockerfiles or runtime flags so orchestrators know when a service is genuinely ready to handle traffic.
5. **Decouple Configuration from Images**: Pass secrets and environment-specific parameters via environment variables or secret mounts, never baking credentials into the image layers.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
|:---|:---|:---|
| **Treating containers like VMs** | Installing SSH servers, cron, and multiple services in one container increases attack surface and bloats image size. | Run a single primary process per container; use Docker exec or log streaming for debugging. |
| **Using `latest` tag in production** | `latest` is mutable; downstream builds or restarts can pull breaking changes unexpectedly. | Pin exact immutable semantic versions or immutable SHA256 image digests. |
| **Storing state inside the container layer** | Any data written to the container's writable layer is destroyed when the container is deleted. | Mount persistent Docker Volumes or external persistent storage for databases. |
| **Ignoring `.dockerignore`** | Unfiltered build contexts send huge `node_modules`, `.git`, and sensitive secrets to the daemon. | Maintain strict `.dockerignore` excluding `.git`, local builds, `.env`, and dependencies. |
| **Baking secrets into images** | `ENV AWS_SECRET=...` or `COPY .env` persists in image layers and is inspectable via `docker history`. | Inject secrets at runtime using environment variables, secrets managers, or BuildKit secret mounts. |
| **Binding host ports without firewall consideration** | By default, Docker modifies iptables rules directly, exposing published ports (`-p 80:80`) publicly despite UFW rules. | Bind explicitly to localhost (`-p 127.0.0.1:80:80`) or configure Docker daemon iptables integration carefully. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Zero Hypervisor Overhead**: Containers run as native host processes. CPU and memory overhead is negligible ($< 1\%$).
- **Instant Boot**: Starting a container takes tens of milliseconds (time to execute `clone()` and mount filesystems) compared to minutes for VM guest OS boots.
- **I/O Overhead**: Storage using the default OverlayFS driver has a slight copy-on-write (CoW) penalty on initial file modification. Heavy database I/O must always bypass OverlayFS by using Docker Volumes (which mount host directories directly).

### Security
- **Kernel Sharing Risk**: If a vulnerability exists in the shared Linux kernel (e.g., Dirty COW, kernel exploit), an attacker with root privileges inside a container may attempt a kernel exploit to compromise the host.
- **Defense in Depth**: Mitigate kernel exposure using unprivileged user namespaces, Seccomp profiles, AppArmor/SELinux policies, and dropping Linux capabilities (`--cap-drop=ALL`).

### Cost
- **High Density**: A single host can run 5x–10x more containerized microservices than equivalent virtual machines due to eliminated guest OS memory footprint.

---

## 9. Comparison Table: Containers vs Virtual Machines

| Feature | Docker Containers | Virtual Machines (KVM / VMware) |
|:---|:---|:---|
| **Virtualization Level** | Operating System (Kernel sharing) | Hardware (Hypervisor abstraction) |
| **Startup Time** | Milliseconds ($10-500\text{ ms}$) | Seconds to minutes ($30-180\text{ s}$) |
| **Memory Footprint** | Megabytes ($5-100\text{ MB}$) | Gigabytes ($1-8\text{ GB}$ per guest OS) |
| **Disk Footprint** | Megabytes (shared layer storage) | Gigabytes (complete OS virtual disk) |
| **Process Isolation** | Kernel Namespaces & cgroups | Hardware-assisted VT-x / AMD-V |
| **I/O Performance** | Native bare-metal execution speed | Emulated/paravirtualized driver overhead |
| **OS Flexibility** | Must share host kernel (Linux on Linux) | Any supported guest OS (Windows on Linux, etc.) |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. What happens under the hood when you execute <code>docker run</code>?</strong></summary>

When you run `docker run`:
1. The Docker CLI sends a REST request to `dockerd` over `/var/run/docker.sock`.
2. `dockerd` checks local storage for the image; if missing, it pulls layers from the remote registry.
3. `dockerd` creates a container configuration spec and requests `containerd` to create the container task.
4. `containerd` spawns `containerd-shim`, which launches `runc`.
5. `runc` invokes the Linux `clone()` system call with namespace flags (`CLONE_NEWPID`, `CLONE_NEWNET`, `CLONE_NEWNS`, etc.), configures cgroup resource limits, mounts the root filesystem via OverlayFS, pivots root, and executes the target entrypoint command.
6. `runc` exits, leaving `containerd-shim` as the parent of the container process to monitor I/O and exit codes.
</details>

<details>
<summary><strong>2. How do Linux Namespaces differ from Control Groups (cgroups)?</strong></summary>

- **Namespaces** provide **isolation**: they determine what a process can *see*. Six core namespaces isolate PIDs (process tree), Network (IPs, routing, ports), Mount (filesystem mounts), IPC (shared memory, message queues), UTS (hostname), and User (UID/GID mapping).
- **Control Groups (cgroups)** provide **resource metering and limits**: they determine what a process can *use*. Cgroups restrict CPU shares, CPU cores, maximum memory, swap, block I/O bandwidth, and network bandwidth.
</details>

<details>
<summary><strong>3. Why should you avoid running containers with <code>--privileged</code> flag?</strong></summary>

The `--privileged` flag gives the container almost all root capabilities of the host system. It disables seccomp, AppArmor, drops capability restrictions, and exposes all host devices (`/dev/*`) directly into the container. An attacker inside a privileged container can trivially mount the host root partition (`mount /dev/sda1 /mnt`) and gain full root control of the host operating system.
</details>

<details>
<summary><strong>4. What is the role of <code>containerd-shim</code>?</strong></summary>

`containerd-shim` is a lightweight daemon that sits between `containerd` and the running container process. It serves three critical functions:
1. It keeps standard I/O pipes open and reports exit status even if `dockerd` or `containerd` restarts, allowing daemon upgrades without killing running containers.
2. It allows `runc` to terminate after creating the container process, avoiding holding an extra process for every container.
3. It reaps zombie processes if the container process is not a dedicated init system.
</details>

<details>
<summary><strong>5. Explain the difference between <code>docker stop</code> and <code>docker kill</code>.</strong></summary>

- `docker stop` sends `SIGTERM` to the container's PID 1 process, allowing it a grace period (default 10 seconds) to flush buffers, finish active requests, and shut down gracefully. If it does not exit within the timeout, it sends `SIGKILL`.
- `docker kill` immediately sends `SIGKILL` (or a user-specified signal) directly to PID 1 without any grace period, terminating the process instantly.
</details>

<details>
<summary><strong>6. What is a "dangling" image in Docker, and how do you clean it up?</strong></summary>

A dangling image is an image layer that is no longer tagged by any named repository reference (commonly shown with `<none>:<none>` in `docker images`). This typically occurs when an image is rebuilt with an existing tag name; the old image layers lose their tag. They can be removed using `docker image prune` or `docker rmi $(docker images -f "dangling=true" -q)`.
</details>

<details>
<summary><strong>7. How does Docker handle port mapping with host firewalls?</strong></summary>

Docker modifies Linux `iptables` directly by inserting its own `DOCKER` chain in the `nat` and `filter` tables. When you publish a port (`-p 8080:80`), Docker adds a DNAT rule routing incoming traffic on host port 8080 directly to the container's private bridge IP. Because Docker's rules are evaluated before traditional firewalls like UFW, published ports are exposed publicly unless explicitly configured with host IP binding (`-p 127.0.0.1:8080:80`) or daemon-level firewall overrides.
</details>

<details>
<summary><strong>8. Why is running multiple services in a single container generally discouraged?</strong></summary>

Running multiple services violates the single-responsibility principle of containers and introduces operational failure modes:
1. Signal handling: Signals like `SIGTERM` sent to PID 1 might not propagate cleanly to child services.
2. Logging: Docker captures stdout/stderr from PID 1; multiplexed services make log parsing and aggregation difficult.
3. Health monitoring: If one secondary service crashes while the primary continues running, Docker will mark the container as healthy despite partial failure.
4. Resource allocation: Cgroups cannot independently throttle or allocate CPU/RAM between services within the same container.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect System Architecture & Host Primitives</strong></summary>

**Goal**: Inspect your host machine's Linux kernel architecture and verify the isolation primitives available to Docker.

```bash
# Query Docker host information
docker info --format 'Kernel: {{.KernelVersion}} | Arch: {{.Architecture}} | Operating System: {{.OperatingSystem}}'

# Verify current Docker context
docker context show
```
</details>

<details>
<summary><strong>Exercise 2: Create a Custom Nginx Container with Custom Static HTML</strong></summary>

**Goal**: Launch an Nginx web server displaying a customized HTML message without building a custom image.

```bash
# Create a local temporary directory with an index.html file
mkdir -p /tmp/docker-web && echo "<h1>Hello from Docker Engine</h1>" > /tmp/docker-web/index.html

# Run Nginx with a bind mount
docker run -d --rm --name custom-web -p 8081:80 -v /tmp/docker-web:/usr/share/nginx/html:ro nginx:alpine

# Verify response
curl http://localhost:8081

# Cleanup
docker stop custom-web && rm -rf /tmp/docker-web
```
</details>

<details>
<summary><strong>Exercise 3: Inspect Container Metadata via Go Templates</strong></summary>

**Goal**: Extract specific runtime parameters such as IP address and state using `docker inspect` formatting.

```bash
# Launch a background container
docker run -d --name inspect-target alpine sleep 120

# Query the assigned IP address directly
docker inspect --format='{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' inspect-target

# Query the container PID on host
docker inspect --format='{{.State.Pid}}' inspect-target

# Cleanup
docker rm -f inspect-target
```
</details>

<details>
<summary><strong>Exercise 4: Test Automatic Restart Policies</strong></summary>

**Goal**: Observe how Docker handles process failures when configured with restart policies.

```bash
# Run a container that exits with an error code after 2 seconds, configured with on-failure
docker run -d --name fail-test --restart on-failure:3 alpine sh -c "sleep 2; exit 1"

# Monitor restarts
sleep 6
docker ps -a --filter "name=fail-test" --format "table {{.Names}}\t{{.Status}}"

# Clean up
docker rm -f fail-test
```
</details>

<details>
<summary><strong>Exercise 5: Real-time Resource Monitoring</strong></summary>

**Goal**: Stream live CPU and memory metrics for active containers.

```bash
# Start two lightweight containers
docker run -d --name worker-1 alpine sleep 60
docker run -d --name worker-2 alpine sleep 60

# View real-time resource utilization stream (non-streaming with --no-stream)
docker stats --no-stream worker-1 worker-2

# Clean up
docker rm -f worker-1 worker-2
```
</details>

<details>
<summary><strong>Exercise 6: Diagnose Network Connectivity Between Containers</strong></summary>

**Goal**: Create a custom user-defined bridge network and verify automatic DNS resolution between two containers.

```bash
# Create custom bridge network
docker network create lab-net

# Run service 1 (Nginx)
docker run -d --name web-svc --network lab-net nginx:alpine

# Run service 2 (Alpine) and ping web-svc by container name
docker run --rm --network lab-net alpine ping -c 3 web-svc

# Clean up
docker stop web-svc && docker rm web-svc
docker network rm lab-net
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                               DOCKER CLI CHEAT SHEET                            |
+---------------------------------------------------------------------------------+
| docker run -d -p 80:80 --name <name> <img:tag>   Run container in background    |
| docker ps / docker ps -a                         List running / all containers  |
| docker stop <name> / docker start <name>         Stop / Start container         |
| docker rm -f <name>                              Force remove container         |
| docker logs -f <name>                            Follow live stdout/stderr logs |
| docker exec -it <name> /bin/sh                   Attach interactive shell       |
| docker images / docker rmi <img:tag>             List / Delete local images     |
| docker system prune -af                          Reclaim all unused containers, |
|                                                  images, and networks           |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Docker Documentation - Engine Overview](https://docs.docker.com/engine/)
- [Open Container Initiative (OCI) Runtime & Image Specifications](https://opencontainers.org/)
- [containerd Architecture and Core Design](https://containerd.io/)
- [Linux Kernel Namespaces Manual Page (man 7 namespaces)](https://man7.org/linux/man-pages/man7/namespaces.7.html)
- [Linux Control Groups v2 Documentation](https://docs.kernel.org/admin-guide/cgroup-v2.html)

---

| [Next: 02-install-on-linux-macos-windows.md](./02-install-on-linux-macos-windows.md) > |
| :--- |
