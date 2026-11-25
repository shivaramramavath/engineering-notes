---
title: "The PID 1 Problem: Signal Handling, Zombie Reaping, and Container Init Systems"
description: "Mastering the Linux PID 1 problem in containers: signal masking, zombie process accumulation, exec form vs shell form, Tini, dumb-init, and docker run --init."
category: "DevOps / Containers"
tags: ["docker", "pid-1", "signals", "zombies", "tini", "dumb-init", "init", "troubleshooting"]
difficulty: "Advanced"
prerequisites: ["01-fundamentals/03-docker-architecture.md", "01-fundamentals/08-container-lifecycle-and-states.md", "Linux process trees and signals"]
estimated_read_time: "25 min"
---

# The PID 1 Problem: Signal Handling, Zombie Reaping, and Container Init Systems

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. The Two Faces of the PID 1 Problem](#2-the-two-faces-of-the-pid-1-problem)
- [3. Deep Dive: Signal Masking and Propagation](#3-deep-dive-signal-masking-and-propagation)
- [4. Deep Dive: Orphaned Children and Zombie Processes](#4-deep-dive-orphaned-children-and-zombie-processes)
- [5. Exec Form vs Shell Form in Dockerfiles](#5-exec-form-vs-shell-form-in-dockerfiles)
- [6. Container Init Solutions: Tini, dumb-init, and `--init`](#6-container-init-solutions-tini-dumb-init-and---init)
- [7. Practical Diagnostic Demonstrations](#7-practical-diagnostic-demonstrations)
- [8. Best Practices & Production Guidelines](#8-best-practices--production-guidelines)
- [9. Common Pitfalls & Anti-Patterns](#9-common-pitfalls--anti-patterns)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

In a traditional Linux operating system, the first process spawned by the kernel is **PID 1** (typically `systemd`, `init`, or `Upstart`). As the ancestor of all processes, PID 1 has two critical system responsibilities:
1. **Adopting and Reaping Zombie Processes**: When a parent process terminates before its child process exits, the orphaned child is reparented to PID 1. PID 1 must continuously call `wait()` or `waitpid()` to clean up terminated children and remove their entries from the kernel process table.
2. **Handling and Propagating Signals**: PID 1 must catch system signals (such as `SIGTERM`, `SIGINT`, `SIGHUP`) and propagate them downward to child process trees to facilitate graceful shutdowns.

In a Docker container, **your application binary is typically executed directly as PID 1** inside the container's PID namespace.

However, standard application runtimes—including **Node.js, Python, Java, Ruby, and Go**—were never designed to act as system init processes. They do not register default signal handlers for signals like `SIGTERM`, and they do not reap orphaned zombie processes. 

This leads to two catastrophic production failure modes:
- **10-Second Shutdown Hangs**: Running `docker stop` or scaling down a Kubernetes pod hangs for 10 seconds before being brutally murdered with `SIGKILL`, severing in-flight HTTP requests and corrupting database transactions.
- **Host Process Table Saturation**: High-volume applications that fork sub-processes (e.g., resizing images with ImageMagick or running shell utilities) accumulate thousands of defunct zombie processes, eventually exhausting the host's PID limit and crashing the entire server.

```
+---------------------------------------------------------------------------------+
|                         THE PID 1 PROBLEM VISUALIZED                            |
|                                                                                 |
|  SCENARIO A: Broken (Shell / No Init)        SCENARIO B: Fixed (With Init)      |
|                                                                                 |
|  [dockerd: docker stop]                      [dockerd: docker stop]             |
|         |                                           |                           |
|         | SIGTERM                                   | SIGTERM                   |
|         v                                           v                           |
|  +-----------------------------+             +-----------------------------+    |
|  | PID 1: /bin/sh -c "node..." |             | PID 1: tini / dumb-init     |    |
|  | - Ignores SIGTERM!          |             | - Catches SIGTERM!          |    |
|  | - Does NOT forward to Node! |             | - Forwards SIGTERM to Node! |    |
|  +--------------+--------------+             | - Reaps all zombie childs!  |    |
|                 |                            +--------------+--------------+    |
|                 v (Never receives signal!)                  |                   |
|  +-----------------------------+                            v SIGTERM           |
|  | PID 2: Node.js Web Server   |             +-----------------------------+    |
|  | - Keeps serving requests!   |             | PID 2: Node.js Web Server   |    |
|  +-----------------------------+             | - Closes DB pool cleanly    |    |
|                 |                            | - Drains HTTP connections   |    |
|                 v 10 seconds timeout...      | - Exits code 0 in 150ms!    |    |
|  [dockerd sends SIGKILL!]                    +-----------------------------+    |
|  (Brutal termination / Data loss!)           (Clean, Graceful Teardown!)        |
+---------------------------------------------------------------------------------+
```

---

## 2. The Two Faces of the PID 1 Problem

### 2.1. The Signal Handling Anomaly
The Linux kernel treats PID 1 fundamentally differently from every other process:
> *"For a process with PID 1, the kernel will only deliver signals for which the process has set an explicit signal handler. Signals for which the process has not set a handler are silently discarded."*

For any normal process (PID > 1), if it does not handle `SIGTERM`, the Linux kernel applies the **default action**, which is immediate process termination. But for PID 1, if it has not registered an explicit handler for `SIGTERM`, the kernel **discards the signal entirely**. The process acts as if nothing happened!

### 2.2. The Zombie Reaping Anomaly
When a process calls `exit()`, it does not immediately disappear from the system. The kernel frees its memory and open files, but retains an entry in the process table storing its PID, termination timestamp, and exit status code. The process becomes a **Zombie (`<defunct>`)**.

The zombie entry remains until the parent process calls `wait()` or `waitpid()` to read the status code. If the parent exits before the child, the orphaned child is reparented to **PID 1**. If PID 1 does not continuously execute `wait()`, zombie entries accumulate indefinitely until the kernel runs out of available process IDs (`kernel.pid_max`).

---

## 3. Deep Dive: Signal Masking and Propagation

```
+---------------------------------------------------------------------------------+
|                       COMMON LINUX SIGNALS IN DOCKER                            |
+---------+-----------+-----------------------+-----------------------------------+
| SIGNAL  | VALUE     | EMITTED BY            | DEFAULT APPLICATION ACTION        |
+---------+-----------+-----------------------+-----------------------------------+
| SIGTERM | Signal 15 | `docker stop`         | Graceful termination request      |
| SIGINT  | Signal 2  | Terminal (Ctrl+C)     | Terminal interrupt request        |
| SIGKILL | Signal 9  | Kernel / `docker kill`| Uncatchable, instantaneous murder |
| SIGHUP  | Signal 1  | Terminal disconnect   | Reload configuration file         |
| SIGCHLD | Signal 17 | Terminating child     | Informs parent to reap child      |
| SIGQUIT | Signal 3  | Terminal (Ctrl+\)     | Graceful exit with core dump      |
+---------+-----------+-----------------------+-----------------------------------+
```

### Why Shells Refuse to Forward Signals:
If your Dockerfile contains:
```dockerfile
CMD npm start
```
Docker executes this using the **shell form**, expanding it to:
```bash
/bin/sh -c "npm start"
```
Here, `/bin/sh` is PID 1, and `npm` is PID 2. When `docker stop` sends `SIGTERM` to PID 1:
1. Standard shells (`/bin/sh`, `/bin/bash`, `/bin/ash`) **do not forward signals to child processes by default**.
2. `/bin/sh` ignores the signal.
3. Your application running as PID 2 never even knows a shutdown was requested!
4. Docker waits for 10 seconds, gives up, and sends `SIGKILL` to PID 1, killing the shell and leaving in-flight requests abruptly severed.

---

## 4. Deep Dive: Orphaned Children and Zombie Processes

```
[Host Process Table Capacity: 32,768 PIDs]
      |
      v
Container PID 1 (Node.js App)
      |
      +---> Forks Child Process (ImageMagick: convert input.jpg output.png)
      |        |
      |        v
      |     Child Finishes Execution!
      |        |
      |        v
      |     State transitions to ZOMBIE (<defunct>)
      |     Memory is freed, but PID slot in kernel remains locked!
      |
      +---> Node.js never calls waitpid()!
      +---> Zombie remains locked in process table!
      |
      v
(After 30,000 requests, host kernel PID table is 100% full -> HOST FREEZES!)
```

---

## 5. Exec Form vs Shell Form in Dockerfiles

To solve signal forwarding issues, developers must master the difference between Dockerfile instruction forms:

### Shell Form (ANTI-PATTERN for Entrypoints):
```dockerfile
CMD node server.js
```
- Spawns `/bin/sh -c "node server.js"`.
- `/bin/sh` becomes PID 1.
- Signals are swallowed; zombies are ignored.

### Exec Form (RECOMMENDED):
```dockerfile
CMD ["node", "server.js"]
```
- Spawns `node server.js` directly via `execve()`.
- `node` becomes PID 1.
- Signals are delivered directly to the Node process (though Node must still register a `process.on('SIGTERM', ...)` handler).

### The `exec` Shell Builtin Trick:
If you must use an entrypoint shell script to evaluate dynamic environment variables, use the shell's `exec` builtin to replace the shell process:
```bash
#!/bin/sh
# Staging and setup commands...
export CONFIG_PATH="/etc/app/config.json"

# Replace the shell process with the target application so it inherits PID 1:
exec node server.js
```

---

## 6. Container Init Solutions: Tini, dumb-init, and `--init`

To provide true PID 1 init capabilities (signal forwarding + zombie reaping) without modifying application code, lightweight container init systems were created.

### 6.1. Docker Built-in Init (`docker run --init`)
Docker natively bundles **Tini** directly into the Docker Engine:
```bash
docker run --init -d -p 8080:80 my-app:latest
```
When `--init` is passed, Docker inserts a tiny, statically linked C init binary (`/sbin/docker-init`) as PID 1 inside the container. It forwards all signals transparently to child processes and reaps zombies continuously.

### 6.2. Embedding Tini in Dockerfile (`krallin/tini`)
If running in Kubernetes or environments where `--init` cannot be passed at runtime:
```dockerfile
# Add Tini to Alpine image
RUN apk add --no-cache tini

# Set Tini as entrypoint
ENTRYPOINT ["/sbin/tini", "--"]
CMD ["node", "server.js"]
```

### 6.3. Yelp's `dumb-init`
An alternative lightweight init system written in C by Yelp, popular in Debian/Ubuntu images:
```dockerfile
RUN apt-get update && apt-get install -y dumb-init
ENTRYPOINT ["/usr/bin/dumb-init", "--"]
CMD ["python3", "app.py"]
```

---

## 7. Practical Diagnostic Demonstrations

### Example 1: Demonstrating the 10-Second Shutdown Hang
Observe how running an application in shell form swallows signals and triggers a 10-second timeout:

```bash
# Launch a container using shell form that traps nothing
docker run -d --name slow-stop alpine sh -c "sleep 1000"

# Time how long it takes docker stop to complete
time docker stop slow-stop
```
*Output:*
```text
slow-stop
real    0m10.284s
```
Notice it took over 10 seconds! Docker was forced to send `SIGKILL` because `/bin/sh` ignored `SIGTERM`.

```bash
# Cleanup
docker rm slow-stop
```

### Example 2: Instant Graceful Shutdown with `exec` Form
Observe the instant shutdown when the application directly receives `SIGTERM`:

```bash
# Launch container using exec form
docker run -d --name fast-stop alpine sleep 1000

# Time how long it takes docker stop to complete
time docker stop fast-stop
```
*Output:*
```text
fast-stop
real    0m0.312s
```
Shutdown occurs in 300 milliseconds!

```bash
# Cleanup
docker rm fast-stop
```

### Example 3: Verifying Docker's Built-in `--init` Flag
Observe Tini operating as PID 1 when launching with `--init`:

```bash
# Launch container with --init
docker run -d --init --name init-demo alpine sleep 3600

# Inspect the process tree inside the container
docker exec init-demo ps -o pid,comm
```
*Output:*
```text
PID   COMMAND
    1 docker-init
    7 sleep
```
Notice `docker-init` is PID 1, and `sleep` is PID 7!

```bash
# Cleanup
docker rm -f init-demo
```

### Example 4: Creating and Observing Zombie Processes
Simulate an application creating defunct zombie processes inside a container without an init system:

```bash
# Run a script that forks children that exit immediately while parent sleeps
docker run --name zombie-demo -d alpine sh -c '
  for i in $(seq 1 5); do
    (exit 0) &
  done
  sleep 60
'

# Inspect processes inside container
docker exec zombie-demo ps aux
```
*Output:*
```text
PID   USER     TIME  COMMAND
    1 root      0:00 sh -c for i in $(seq 1 5); do (exit 0) & done; sleep 60
    7 root      0:00 [sh] <defunct>
    8 root      0:00 [sh] <defunct>
    9 root      0:00 [sh] <defunct>
   10 root      0:00 [sh] <defunct>
   11 root      0:00 [sh] <defunct>
   12 root      0:00 sleep 60
```
Notice PIDs 7 through 11 are stuck as `<defunct>` zombies because `/bin/sh` does not reap them!

```bash
# Cleanup
docker rm -f zombie-demo
```

### Example 5: Proper Signal Handling in Node.js
Demonstrate how a Node.js application should catch `SIGTERM` to drain requests:

```javascript
// server.js
const http = require('http');

const server = http.createServer((req, res) => {
  res.writeHead(200);
  res.end('OK\n');
});

server.listen(8080);

// Explicitly register SIGTERM listener
process.on('SIGTERM', () => {
  console.log('Received SIGTERM! Draining connections...');
  server.close(() => {
    console.log('HTTP server closed. Exiting cleanly.');
    process.exit(0);
  });
});
```

---

## 8. Best Practices & Production Guidelines

1. **Always Use JSON Exec Form in Dockerfiles**: Always write `ENTRYPOINT ["executable", "param1"]` and `CMD ["param2"]`. Never use the raw string shell form (`CMD executable param1`).
2. **Always Use `exec` in Entrypoint Wrappers**: If your container uses a bash script as an `ENTRYPOINT`, ensure the final command replaces the shell:
   ```bash
   exec "$@"
   ```
3. **Enable `--init` in Docker Compose or CLI**: In `compose.yaml`, configure `init: true` for any service that spawns child processes (Node.js, Python, Ruby, CI workers).
4. **Register `SIGTERM` Handlers in Application Code**: Ensure web frameworks (Express, FastAPI, Spring Boot, Gin) intercept `SIGTERM`, stop accepting new ingress requests, complete active requests, and close database connection pools cleanly.
5. **Adjust `STOPSIGNAL` When Necessary**: If your application expects a non-standard shutdown signal (e.g., Nginx expects `SIGQUIT` for graceful stop), specify `STOPSIGNAL SIGQUIT` in the Dockerfile.

---

## 9. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Consequence | Proper Fix |
|:---|:---|:---|:---|
| **Using `CMD node index.js` (String form)** | Docker wraps execution in `/bin/sh -c`. | Shell swallows `SIGTERM`; shutdown hangs for 10s; forced `SIGKILL`. | Use exec form: `CMD ["node", "index.js"]`. |
| **Omitting `exec` in shell wrapper scripts** | Script runs `python app.py` as a child process. | The shell stays as PID 1; Python never receives signals. | Add `exec` prefix: `exec python app.py`. |
| **Running headless Chrome or ImageMagick without an init system** | Tools fork multiple sub-processes that exit quickly. | Hundreds of zombie `<defunct>` processes saturate host PID capacity. | Add `init: true` in Compose or run via Tini. |
| **Assuming Node.js handles `SIGTERM` by default** | Node's default behavior ignores `SIGTERM` when running as PID 1. | Container fails to shut down gracefully even with exec form. | Add `process.on('SIGTERM', ...)` in JavaScript. |
| **Using heavy init systems like `systemd` inside microservices** | Attempting to run full OS init in a container. | Massive memory bloat, security vulnerabilities, requires privileged mode. | Use minimal purpose-built inits like Tini ($<100\text{ KB}$). |
| **Hardcoding `kill -9` in deployment scripts** | Impatient engineers forcing immediate stops. | In-flight HTTP requests severed, caches corrupted, state unsaved. | Allow `SIGTERM` to complete graceful draining. |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. What is the "PID 1 Problem" in Docker containers?</strong></summary>

The PID 1 Problem refers to two specific kernel behaviors when a standard application binary runs as PID 1 inside a container namespace:
1. **Signal Discarding**: The Linux kernel will not apply default signal actions to PID 1. If the process has not registered an explicit handler for `SIGTERM`, the kernel silently discards the signal, preventing graceful shutdown.
2. **Zombie Process Accumulation**: In Linux, orphaned processes are reparented to PID 1. If PID 1 does not continuously call `wait()` or `waitpid()`, terminated child processes remain in the kernel process table as defunct "zombies", eventually exhausting the system PID table.
</details>

<details>
<summary><strong>2. Why does <code>docker stop</code> take exactly 10 seconds to terminate a misconfigured container?</strong></summary>

When you run `docker stop`, Docker sends `SIGTERM` to PID 1 and starts a 10-second countdown timer. If the application is running via shell form (`/bin/sh -c`), the shell ignores `SIGTERM` and fails to forward it to the child process. After the 10-second grace period expires, Docker sends uncatchable `SIGKILL` (signal 9), which terminates the process abruptly.
</details>

<details>
<summary><strong>3. What is the difference between <code>ENTRYPOINT ["node", "app.js"]</code> and <code>ENTRYPOINT node app.js</code>?</strong></summary>

- `ENTRYPOINT ["node", "app.js"]` uses the **exec form**. The binary is invoked directly via `execve()`. `node` becomes PID 1 inside the container, receiving all incoming signals directly.
- `ENTRYPOINT node app.js` uses the **shell form**. Docker implicitly prepends `/bin/sh -c`. `/bin/sh` becomes PID 1, and `node` becomes PID 2. The shell does not forward signals to Node, causing shutdowns to hang.
</details>

<details>
<summary><strong>4. How does the <code>exec</code> shell command solve signal propagation issues in wrapper scripts?</strong></summary>

In POSIX shells, executing a command normally causes the shell to `fork()` a child process, leaving the shell running as the parent. The `exec` builtin instructs the shell not to fork, but rather to replace the current shell process image in memory with the target command using the `execve()` system call. The target process retains the shell's PID (PID 1), allowing it to receive incoming signals directly.
</details>

<details>
<summary><strong>5. What is the purpose of Tini in Docker?</strong></summary>

**Tini** is an ultra-lightweight, single-binary init system written in C specifically for containers. It acts as PID 1, registers signal handlers for all common signals, forwards them immediately to the spawned application process group, and implements an active `waitpid()` loop to reap all orphaned zombie processes. Docker natively integrates Tini via the `--init` flag.
</details>

<details>
<summary><strong>6. What is a Zombie Process, and why can't it be killed with <code>kill -9</code>?</strong></summary>

A zombie process (marked `<defunct>` in `ps`) is a process that has already terminated and released its memory, code, and file descriptors. It is not an active process executing code; it is merely an entry in the kernel's process table waiting for its parent process to read its exit status code via `waitpid()`. Because it is already dead, it cannot receive signals, meaning `kill -9` has zero effect. It can only be cleared when its parent reads its status or when PID 1 reaps it.
</details>

<details>
<summary><strong>7. How do you configure Docker Compose to use an init system for a service?</strong></summary>

In `compose.yaml`, you add the `init: true` attribute to the service definition:
```yaml
services:
  web:
    image: node:20-alpine
    init: true
    command: ["node", "server.js"]
```
This instructs Docker to inject the built-in Tini init binary as PID 1 for that container.
</details>

<details>
<summary><strong>8. What is the <code>STOPSIGNAL</code> instruction in a Dockerfile?</strong></summary>

The `STOPSIGNAL` instruction configures the specific system call signal that the Docker daemon sends to the container when `docker stop` is executed. The default is `SIGTERM`. Some applications require different shutdown signals (e.g., Nginx uses `SIGQUIT` for graceful stop, and systemd uses `SIGRTMIN+3`). Specifying `STOPSIGNAL SIGQUIT` ensures Docker sends the correct graceful shutdown signal instead of `SIGTERM`.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Compare Stop Latencies Between Exec Form and Shell Form</strong></summary>

**Goal**: Prove the 10-second difference between exec form and shell form.

```bash
# Test 1: Shell form (swallows signals)
docker run -d --name test-shell alpine sh -c "sleep 500"
time docker stop test-shell
# Result: ~10 seconds

# Test 2: Exec form (receives signals directly)
docker run -d --name test-exec alpine sleep 500
time docker stop test-exec
# Result: ~0.3 seconds

# Cleanup
docker rm test-shell test-exec
```
</details>

<details>
<summary><strong>Exercise 2: Inspect Process Tree of a Container Running with `--init`</strong></summary>

**Goal**: Verify that `--init` spawns `docker-init` as PID 1.

```bash
# Run with --init
docker run -d --init --name tini-verify alpine sleep 100

# Inspect processes
docker exec tini-verify ps -o pid,ppid,comm

# Notice docker-init is PID 1, and sleep's PPID is 1!
docker rm -f tini-verify
```
</details>

<details>
<summary><strong>Exercise 3: Simulate Graceful `SIGTERM` Handling in Python</strong></summary>

**Goal**: Run a Python script that intercepts `SIGTERM` and shuts down cleanly.

```bash
docker run --rm -i python:3.12-alpine python3 - <<'EOF'
import signal, time, sys

def handler(signum, frame):
    print("Caught SIGTERM! Performing cleanup...", flush=True)
    sys.exit(0)

signal.signal(signal.SIGTERM, handler)
print("Ready. Send SIGTERM to exit cleanly.", flush=True)
while True:
    time.sleep(1)
EOF
```
</details>

<details>
<summary><strong>Exercise 4: Test Signal Override with `--stop-signal`</strong></summary>

**Goal**: Stop a container using a custom signal like `SIGINT`.

```bash
# Start container
docker run -d --name sig-override alpine sleep 100

# Stop using SIGINT instead of default SIGTERM
docker stop --signal SIGINT sig-override

# Cleanup
docker rm sig-override
```
</details>

<details>
<summary><strong>Exercise 5: Inspect Host Process Table for Zombie Accumulation</strong></summary>

**Goal**: Search for defunct processes originating from Docker containers.

```bash
ps aux | grep -i "defunct" | grep -v grep || echo "No zombie processes found on host."
```
</details>

<details>
<summary><strong>Exercise 6: Test `STOPSIGNAL` in an Nginx Container</strong></summary>

**Goal**: Inspect the default `STOPSIGNAL` configured in the official Nginx image.

```bash
docker inspect --format 'StopSignal: {{.Config.StopSignal}}' nginx:alpine
```
*Output confirms Nginx uses `SIGQUIT`!*
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                         PID 1 & SIGNAL HANDLING CHEAT SHEET                           |
+---------------------------------------------------------------------------------------+
| The PID 1 Problem:     PID 1 discards unhandled signals & must reap zombie processes  |
| Exec Form (CORRECT):   ENTRYPOINT ["node", "app.js"] / CMD ["app.js"]                |
| Shell Form (BROKEN):   ENTRYPOINT node app.js (Spawns /bin/sh which swallows signals)|
| Wrapper Script Rule:   Always end wrapper scripts with: exec "$@"                     |
| Enable Built-in Init:  docker run --init ... / init: true in compose.yaml             |
| Standalone Init:       Embed Tini (krallin/tini) or dumb-init in Dockerfile           |
| Custom Signal:         STOPSIGNAL SIGQUIT in Dockerfile                               |
| Golden Rule:           Never let raw application runtimes run as unmanaged PID 1      |
|                        if they fork sub-processes or lack SIGTERM handlers.           |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Docker Documentation: Dockerfile ENTRYPOINT and CMD Best Practices](https://docs.docker.com/engine/reference/builder/#entrypoint)
- [Tini: A tiny but valid init for containers](https://github.com/krallin/tini)
- [Yelp Engineering: dumb-init - An init system for Docker containers](https://engineeringblog.yelp.com/2016/01/dumb-init-an-init-for-docker.html)
- [Node.js Docker Best Practices: Handling Kernel Signals](https://github.com/nodejs/docker-node/blob/main/docs/BestPractices.md#handling-kernel-signals)
- [Linux Manual Pages: waitpid(2) Process Lifecycle](https://man7.org/linux/man-pages/man2/waitpid.2.html)

---

| [< Previous: 08-container-lifecycle-and-states.md](./08-container-lifecycle-and-states.md) | [Next: Module 02 Essential Commands >](../02-commands/README.md) |
| :--- | :--- |
