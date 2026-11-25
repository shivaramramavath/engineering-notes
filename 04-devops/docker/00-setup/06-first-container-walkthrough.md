---
title: "First Container Walkthrough: Pull, Run, Port-Map, Exec, and Inspect"
description: "End-to-end hands-on walkthrough running your first containerized workload: pulling images, daemon flags, port forwarding, interactive execution, log inspection, and cleanup."
category: "DevOps / Containers"
tags: ["docker", "walkthrough", "tutorial", "hands-on", "nginx", "beginner"]
difficulty: "Beginner"
prerequisites: ["01-getting-started.md", "Docker installed and daemon running"]
estimated_read_time: "25 min"
---

# First Container Walkthrough: Pull, Run, Port-Map, Exec, and Inspect

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. Core Concepts & Terminology](#2-core-concepts--terminology)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Command Structure](#4-syntax--command-structure)
- [5. Step-by-Step Practical Walkthrough](#5-step-by-step-practical-walkthrough)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table: Interactive vs Detached Execution](#9-comparison-table-interactive-vs-detached-execution)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

Running your first container is the gateway to understanding the Docker operational model. While running a local binary requires installing dependencies, configuring operating system services, opening system ports, and handling background process supervision, a container encapsulates the application binary, runtime libraries, and default configurations into a self-contained, sandboxed execution unit.

This walkthrough demonstrates the complete lifecycle of a container:
1. **Pulling** the immutable image from a remote registry.
2. **Running** the container in detached mode with port mapping.
3. **Inspecting** the live process and network configuration.
4. **Interacting** with the running process via `docker exec`.
5. **Streaming** application logs in real time.
6. **Modifying** runtime data and copying files.
7. **Stopping and purging** the container cleanly.

```
+---------------------------------------------------------------------------------+
|                       CONTAINER LIFECYCLE WALKTHROUGH                           |
|                                                                                 |
|  [Remote Registry]                                                              |
|        |                                                                        |
|        | docker pull nginx:alpine                                               |
|        v                                                                        |
|  [Local Image Cache]                                                            |
|        |                                                                        |
|        | docker run -d -p 8080:80 --name my-web                                 |
|        v                                                                        |
|  +-- [Running Container: my-web] ---------------------------------------------+ |
|  |  Process: nginx: master process / worker                                   | |
|  |  Virtual Network: 172.17.0.2:80  <==== Port Forwarded ====> Host :8080    | |
|  |  Writable Layer: UpperDir (Temporary changes)                              | |
|  +----------------------------------------------------------------------------+ |
|        |                               |                         |              |
|        | docker exec -it               | docker logs -f          | docker stop  |
|        v                               v                         v              |
|  [Interactive Shell]           [Log Stream (stdout)]      [Exited State]        |
|                                                                  |              |
|                                                           docker rm             |
|                                                                  v              |
|                                                           [Container Purged]    |
+---------------------------------------------------------------------------------+
```

---

## 2. Core Concepts & Terminology

- **Image vs Container**: The image is the immutable blueprint on disk; the container is the living execution instance in memory with an isolated PID tree and writable storage layer.
- **Detached Mode (`-d`)**: Runs the container as a background daemon process, freeing the invoking terminal session and outputting the container ID.
- **Interactive TTY (`-it`)**: `-i` (`--interactive`) keeps `stdin` open even if unattached; `-t` (`--tty`) allocates a pseudo-terminal screen, allowing interactive bash/sh sessions.
- **Port Publishing (`-p <host_port>:<container_port>`)**: Binds an external TCP/UDP port on the host network interface to a private listening port inside the container's network namespace.
- **Container Name (`--name`)**: Assigns a human-readable identifier to the container instead of relying on auto-generated random names (e.g., `zen_curie`).

---

## 3. How It Works Internally

When you execute `docker run -d -p 8080:80 --name my-web nginx:alpine`:

```
+-------------------------------------------------------------------------------+
|                      PORT BINDING & PACKET FLOW                               |
|                                                                               |
|  [External Client: curl http://localhost:8080]                                |
|        |                                                                      |
|        v Host Physical Interface (eth0 / lo)                                  |
|  [Host Linux Kernel: Port 8080]                                               |
|        |                                                                      |
|        v iptables (PREROUTING DNAT Rule / docker-proxy)                       |
|  [docker0 Bridge Gateway: 172.17.0.1]                                         |
|        |                                                                      |
|        v Virtual Ethernet Pair (vethxxxx -> eth0 inside container)            |
|  [Container Network Namespace: 172.17.0.2]                                    |
|        |                                                                      |
|        v                                                                      |
|  [Nginx Process listening on Port 80]                                         |
+-------------------------------------------------------------------------------+
```

1. **Storage Assembly**: The daemon creates a read-write `UpperDir` on top of the Nginx image's read-only layers using the OverlayFS storage driver.
2. **Namespace Creation**: A new network namespace is created with a virtual ethernet (`veth`) interface paired to the host's default `docker0` bridge.
3. **iptables Programming**: Docker injects a `DNAT` rule into the Linux host `iptables` table, instructing the kernel to translate all incoming packets on host port `8080` to destination IP `172.17.0.2:80`.
4. **Execution**: `containerd-shim` spawns the container's entrypoint (`nginx -g 'daemon off;'`). Standard output streams are redirected to a JSON log file on the host.

---

## 4. Syntax & Command Structure

```bash
# Basic run syntax
docker run [OPTIONS] IMAGE [COMMAND] [ARG...]

# Common Options:
#   -d, --detach               Run container in background
#   -p, --publish [ip:][host]:container  Publish a container's port(s) to the host
#   -P, --publish-all          Publish all exposed ports to random high ports
#   --name string              Assign a name to the container
#   --rm                       Automatically remove the container when it exits
#   -e, --env list             Set environment variables
#   -v, --volume list          Bind mount a volume
#   -it                        Interactive terminal allocation
```

---

## 5. Step-by-Step Practical Walkthrough

### Step 1: Pulling the Image Explicitly
Pull the official lightweight Nginx Alpine image from Docker Hub and inspect its digest:

```bash
docker pull nginx:alpine
```
*Output:*
```text
alpine: Pulling from library/nginx
c926b61bad3c: Pull complete
Digest: sha256:659610f32bb...
Status: Downloaded newer image for nginx:alpine
docker.io/library/nginx:alpine
```

### Step 2: Running the Container in Background Mode
Launch the container, naming it `web-demo`, mapping host port `8080` to container port `80`:

```bash
docker run -d --name web-demo -p 8080:80 nginx:alpine
```
*Output:*
```text
3e7389ab294ec1d2... (Returns the full 64-character container ID)
```

### Step 3: Verifying Runtime State
List running containers to confirm state, uptime, and published ports:

```bash
docker ps --filter "name=web-demo"
```
*Output:*
```text
CONTAINER ID   IMAGE          COMMAND                  CREATED         STATUS         PORTS                  NAMES
3e7389ab294e   nginx:alpine   "/docker-entrypoint.…"   5 seconds ago   Up 4 seconds   0.0.0.0:8080->80/tcp   web-demo
```

Test HTTP connectivity from your host terminal:
```bash
curl -I http://localhost:8080
```
*Output:*
```text
HTTP/1.1 200 OK
Server: nginx/1.27.0
Content-Type: text/html
```

### Step 4: Streaming Container Logs
Monitor incoming web traffic logs in real time:

```bash
# Follow logs in background or separate shell
docker logs -f --tail 10 web-demo &

# Generate a request to observe log output
curl -s http://localhost:8080 > /dev/null
```
*Output:*
```text
172.17.0.1 - - [25/Nov/2026:08:40:10 +0000] "GET / HTTP/1.1" 200 615 "-" "curl/8.4.0" "-"
```

### Step 5: Executing Commands Inside the Container
Enter the running container to inspect the live filesystem:

```bash
# Open interactive shell inside the running container
docker exec -it web-demo /bin/sh

# Inside container shell:
/ # hostname
3e7389ab294e
/ # ps aux
PID   USER     TIME  COMMAND
    1 root      0:00 nginx: master process nginx -g daemon off;
   23 nginx     0:00 nginx: worker process
/ # exit
```

### Step 6: Modifying Files via `docker cp`
Replace Nginx's default welcome page from the host without rebuilding the image:

```bash
# Create custom HTML file on host
echo "<h1>Deployed via Docker Engine!</h1>" > custom_index.html

# Copy file into container's document root
docker cp custom_index.html web-demo:/usr/share/nginx/html/index.html

# Verify updated web page
curl http://localhost:8080
```
*Output:*
```html
<h1>Deployed via Docker Engine!</h1>
```

### Step 7: Clean Shutdown and Removal
Stop the container gracefully and purge its resources:

```bash
# Stop container (sends SIGTERM, waits 10s, then SIGKILL)
docker stop web-demo

# Verify it is in Exited state
docker ps -a --filter "name=web-demo"

# Remove the container
docker rm web-demo

# Clean up local temporary file
rm -f custom_index.html
```

---

## 6. Best Practices & Production Guidelines

1. **Always Name Containers**: Avoid relying on Docker's auto-generated names (`zen_mendeleev`, `epic_bohr`). Explicit names (`--name api-server`) make scripts, logs, and monitoring predictable.
2. **Explicit Host IP Port Binding**: In production, never publish ports as `-p 8080:80` if the port should only be reachable internally. Bind explicitly to localhost: `-p 127.0.0.1:8080:80` to prevent unintended public exposure.
3. **Use `--rm` for Temporary Utility Tasks**: When running one-off tasks (e.g., database migrations, testing DNS, compiling assets), always supply `--rm` so the container is deleted immediately upon exit.
4. **Inspect with Go Templates**: Instead of piping `docker inspect` to `grep` or `jq`, use native Go templates (`--format`) for deterministic script extraction.
5. **Decouple Data with Volumes**: Do not use `docker cp` to update production workloads. Modifying container writable layers creates configuration drift; use persistent Docker Volumes or build new immutable images.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Symptom | Correct Resolution |
|:---|:---|:---|
| **Port Collision (`address already in use`)** | `Error response from daemon: driver failed programming external connectivity ... bind: address already in use` | Another process is using host port 8080. Identify with `netstat -tlpn` or use a different host port: `-p 8081:80`. |
| **Omitting `-d` on long-running services** | The terminal freezes and attaches directly to Nginx stdout; closing the terminal terminates the container. | Always include the `-d` (detach) flag for background web servers and daemons. |
| **Running interactive shell without `-it`** | Container immediately exits with code 0 or fails to process keyboard input. | Always combine `-i` (keep stdin open) and `-t` (allocate pseudo-TTY): `docker run -it alpine sh`. |
| **Confusing Host Port with Container Port** | Port mapping syntax is `-p <host>:<container>`. Writing `-p 80:8080` maps host port 80 to container port 8080 (which is empty). | Ensure the container port matches what the application inside is actually listening on (`-p 8080:80`). |
| **Container exits immediately with code 0** | You run `docker run -d ubuntu` and it stops instantly. | The container has no foreground process running. Ubuntu's default command is `/bin/bash`, which exits when `stdin` closes without a TTY. |
| **Leaking stopped containers (`docker ps -a`)** | Disk space is consumed by hundreds of dead containers over months of development. | Periodically run `docker container prune` or launch one-off tasks with `--rm`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Port Forwarding Overhead**: On Linux, Docker uses native kernel `iptables` DNAT rules with near-zero latency. On macOS and Windows, traffic passes through the userspace hypervisor proxy (`docker-proxy` / VPNKit), adding sub-millisecond latency.
- **Copy-On-Write (CoW)**: In Step 6, when `index.html` was overwritten, OverlayFS copied the file from the read-only layer into the `UpperDir`. While imperceptible for small HTML files, modifying multi-gigabyte files inside container writable layers causes high I/O pauses.

### Security
- **Process Isolation**: The Nginx master process running inside `web-demo` cannot see processes on your host system (different PID namespace).
- **Network Isolation**: The container cannot bind to other network adapters unless configured with `--net=host`.

### Cost
- Memory consumption for `nginx:alpine` in detached mode is only $\sim 6\text{ MB}$ of RAM, allowing dozens of isolated test sites to run concurrently on a basic developer laptop.

---

## 9. Comparison Table: Interactive vs Detached Execution

| Attribute | Interactive Mode (`-it`) | Detached Mode (`-d`) |
|:---|:---|:---|
| **Terminal Control** | Locks invoking terminal; passes stdin/stdout | Frees invoking terminal immediately |
| **Target Workload** | Shells, debugging, REPLs (python, node) | Daemons, web servers, queues, databases |
| **Lifecycle** | Exits when you type `exit` or press `Ctrl+D` | Runs continuously until stopped (`docker stop`) |
| **Process Supervision** | Tied to active terminal session | Managed by `containerd-shim` in background |
| **Log Collection** | Output prints directly to screen | Output captured in background JSON log file |
| **Typical Flags** | `docker run -it --rm alpine sh` | `docker run -d --name web -p 80:80 nginx` |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. What is the difference between <code>docker run</code> and <code>docker start</code>?</strong></summary>

- `docker run` creates an entirely **new** container from an image and starts it. It performs image resolution, layer allocation, namespace creation, network configuration, and process execution.
- `docker start` resumes an **existing**, previously stopped container from its saved state. It re-attaches the existing container layer and restarts the entrypoint process without creating a new container instance.
</details>

<details>
<summary><strong>2. Why does <code>docker run -d ubuntu</code> exit immediately, while <code>docker run -d nginx</code> stays running?</strong></summary>

A container only stays alive as long as its primary foreground process (PID 1) continues running. 
- The default entrypoint for the `ubuntu` image is `/bin/bash`. In detached mode without interactive input, bash encounters EOF and terminates immediately with exit code 0.
- The `nginx` image has an entrypoint configured to run Nginx in the foreground (`nginx -g 'daemon off;'`), which persistently waits for incoming HTTP connections and keeps PID 1 active.
</details>

<details>
<summary><strong>3. How does <code>docker exec</code> interact with an already running container?</strong></summary>

`docker exec` does not start a new container; it spawns an additional process inside the namespaces of an existing, running container. The Docker daemon invokes `runc exec`, which uses the `setns()` Linux system call to attach the new process (e.g., `/bin/sh`) to the existing PID, Network, Mount, and IPC namespaces of the target container.
</details>

<details>
<summary><strong>4. What is the syntax difference between <code>-p 8080:80</code> and <code>-p 127.0.0.1:8080:80</code>?</strong></summary>

- `-p 8080:80` publishes container port 80 to port 8080 on **all network interfaces** (`0.0.0.0`), making it publicly reachable from external networks and internet traffic.
- `-p 127.0.0.1:8080:80` binds container port 80 strictly to the **loopback interface** of the host, meaning only processes running locally on the same host can connect to it.
</details>

<details>
<summary><strong>5. What happens to changes made inside a container when you run <code>docker rm</code>?</strong></summary>

When a container is deleted using `docker rm`, its thin writable layer (`UpperDir` in OverlayFS) is permanently destroyed and erased from disk. Any files created, modified, or deleted within the container that were not saved to a mounted persistent volume or bind mount are permanently lost.
</details>

<details>
<summary><strong>6. Explain the function of the <code>--rm</code> flag in <code>docker run</code>.</strong></summary>

The `--rm` flag automatically deletes the container and its anonymous writable layer as soon as the container process exits or stops. It is essential for ephemeral, one-time jobs (CLI utilities, batch processing, test scripts) to prevent disk space exhaustion from accumulated stopped containers.
</details>

<details>
<summary><strong>7. How can you view the resource utilization (CPU/RAM) of a single running container?</strong></summary>

You can execute:
```bash
docker stats <container_name_or_id>
```
To return a single snapshot without continuous terminal streaming, supply the `--no-stream` flag:
```bash
docker stats --no-stream <container_name_or_id>
```
</details>

<details>
<summary><strong>8. What is the difference between <code>docker logs</code> and attaching to a container?</strong></summary>

- `docker logs` reads from the daemon's stored stdout/stderr log driver (by default, JSON files located at `/var/lib/docker/containers/<id>/<id>-json.log`). It allows viewing historical logs and following new ones.
- Attaching (`docker attach`) directly connects your terminal's standard I/O streams to the container's running PID 1 process. If you send `Ctrl+C` while attached, you send `SIGINT` to the container process, which typically terminates the container.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Launch an Ephemeral Python REPL</strong></summary>

**Goal**: Run an interactive Python session in an isolated container that automatically removes itself upon exit.

```bash
# Run interactive python shell with --rm
docker run --rm -it python:3.12-alpine python3

# Inside Python prompt:
# >>> import sys
# >>> sys.version
# >>> exit()
```
</details>

<details>
<summary><strong>Exercise 2: Inspect Container IP Address Using Go Templates</strong></summary>

**Goal**: Extract the internal container IP assigned by the Docker bridge network.

```bash
# Run background container
docker run -d --name ip-target alpine sleep 30

# Extract IP address
docker inspect --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' ip-target

# Clean up
docker rm -f ip-target
```
</details>

<details>
<summary><strong>Exercise 3: Simulate and Diagnose a Port Conflict</strong></summary>

**Goal**: Observe how Docker behaves when two containers attempt to publish to the same host port.

```bash
# Start container 1 on port 8999
docker run -d --name svc-one -p 8999:80 nginx:alpine

# Attempt to start container 2 on the same host port 8999
docker run -d --name svc-two -p 8999:80 nginx:alpine 2>&1 || echo "Expected port conflict caught!"

# Clean up
docker rm -f svc-one
```
</details>

<details>
<summary><strong>Exercise 4: Stream Real-Time Events During Container Operations</strong></summary>

**Goal**: Observe the daemon's internal state machine events in real time.

```bash
# In terminal 1, listen to Docker events:
docker events --filter "type=container" &
EVENTS_PID=$!

# Trigger container lifecycle events
docker run --name event-test -d alpine sleep 2
docker stop event-test
docker rm event-test

# Kill event listener
kill $EVENTS_PID
```
</details>

<details>
<summary><strong>Exercise 5: Copy Files Back and Forth (`docker cp`)</strong></summary>

**Goal**: Extract a file generated inside a container to the host filesystem.

```bash
# Run container generating a report
docker run -d --name report-gen alpine sh -c "echo 'Performance: Nominal' > /tmp/report.txt; sleep 30"

# Copy file from container to host
docker cp report-gen:/tmp/report.txt ./downloaded_report.txt

# Verify contents
cat ./downloaded_report.txt

# Cleanup
docker rm -f report-gen && rm -f ./downloaded_report.txt
```
</details>

<details>
<summary><strong>Exercise 6: Query Container Exit Codes</strong></summary>

**Goal**: Determine whether a container terminated successfully (0) or crashed with an error code.

```bash
# Run failing container
docker run --name failure-demo alpine sh -c "exit 42"

# Inspect exit code
docker inspect --format '{{.State.ExitCode}}' failure-demo

# Cleanup
docker rm failure-demo
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                         FIRST CONTAINER LIFECYCLE CHEAT SHEET                         |
+---------------------------------------------------------------------------------------+
| Pull Image:                    docker pull <image>:<tag>                              |
| Run Detached:                  docker run -d --name <name> -p <host>:<cont> <image>   |
| Run Interactive (Ephemeral):   docker run --rm -it <image> <shell>                    |
| List Active:                   docker ps                                              |
| List All:                      docker ps -a                                           |
| Follow Logs:                   docker logs -f --tail 50 <name>                        |
| Shell into Running Container:  docker exec -it <name> sh                              |
| Copy In / Out:                 docker cp <src> <dest>                                 |
| Stop Gracefully:               docker stop <name>                                     |
| Delete Container:              docker rm <name>                                       |
| Force Kill & Purge:            docker rm -f <name>                                    |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Docker Documentation: Get Started Guide](https://docs.docker.com/get-started/)
- [Docker CLI Command Reference: docker run](https://docs.docker.com/reference/cli/docker/container/run/)
- [Docker CLI Command Reference: docker exec](https://docs.docker.com/reference/cli/docker/container/exec/)
- [Nginx Official Docker Hub Image Repository](https://hub.docker.com/_/nginx)
- [Linux iptables and Container Bridge Forwarding Architecture](https://docs.docker.com/engine/network/packet-filtering-firewalls/)

---

| [< Previous: 05-docker-context-and-remote-hosts.md](./05-docker-context-and-remote-hosts.md) | [Next: Module 01 Core Fundamentals >](../01-fundamentals/README.md) |
| :--- | :--- |
