<!--
================================================================================
Metadata:
  Title: NGINX Command Line & Unix Signals: Lifecycle, Hot Reloads & Upgrades
  Track: NGINX & Traffic Engineering / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 01-fundamentals/01-installation.md
  Key Terminology: Master Process, Worker Process, SIGHUP, SIGQUIT, SIGUSR1, SIGUSR2, SIGWINCH, Zero-Downtime Upgrade, daemon off
================================================================================
-->

# NGINX Command Line & Unix Signals: Lifecycle, Hot Reloads & Upgrades

[<- 05-directives-reference-by-context.md](./05-directives-reference-by-context.md) | [Back to Module Overview](./README.md) | [07-default-server-and-catch-all-hosts.md ->](./07-default-server-and-catch-all-hosts.md)

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

In production environments, managing NGINX extends far beyond running `systemctl restart nginx`. In automated CI/CD deployment pipelines, containerized Kubernetes pods, and high-availability edge nodes, engineers frequently interact directly with the NGINX executable using command-line arguments and low-level **Unix Signals**.

Understanding the signal-driven lifecycle of NGINX enables:
- **Zero-Downtime Hot Reloads (`SIGHUP`)**: Loading new routing rules, upstream configurations, or TLS certificates without dropping active TCP connections or interrupting in-flight file downloads.
- **On-the-Fly Binary Upgrades (`SIGUSR2`)**: Replacing the compiled `/usr/sbin/nginx` executable binary in memory during a major version upgrade with zero dropped requests.
- **Container Lifecycle Optimization (`daemon off;`)**: Running NGINX as the primary foreground process (PID 1) in Docker containers while ensuring SIGTERM signals cleanly propagate to child workers.

---

## 2. Core Concepts

### Master vs. Worker Process Roles
NGINX uses a multi-process architecture consisting of two distinct tiers:
1. **The Master Process (Runs as root)**:
   - Does not handle client network requests.
   - Binds to privileged ports (e.g., 80, 443).
   - Reads and validates configuration files.
   - Manages the lifecycle of worker processes (spawning, monitoring, terminating).
   - Receives Unix signals from the operating system and delegates actions to workers.
2. **The Worker Processes (Run as unprivileged `nginx`)**:
   - Execute non-blocking event loops (`epoll`/`kqueue`).
   - Accept incoming client connections on inherited listening sockets.
   - Read requests, proxy data to upstreams, serve static files, and handle TLS cryptography.

```
+-------------------------------------------------------------+
| NGINX PROCESS TOPOLOGY                                      |
|                                                             |
| [ OS Kernel / System Administrator ]                        |
|                     |                                       |
|                     v (Signals: HUP, USR1, USR2, QUIT)      |
| [ Master Process: PID 1024 (root) ]                         |
|   ├── Manages sockets, config AST, and worker pool          |
|   ├── Inter-Process Communication (Unix Domain Sockets)     |
|   │                                                         |
|   ├── [ Worker Process: PID 1025 (nginx) ] -> epoll loop    |
|   ├── [ Worker Process: PID 1026 (nginx) ] -> epoll loop    |
|   └── [ Cache Manager / Loader: PID 1027 ] -> shm pruning   |
+-------------------------------------------------------------+
```

---

## 3. How It Works Internally

### On-The-Fly Binary Upgrade Architecture (`SIGUSR2`)
NGINX has the extraordinary ability to upgrade its compiled binary in memory without dropping a single active network connection. This is achieved through coordinated Unix signals:

```
+--------------------------------------------------------------------------+
| ZERO-DOWNTIME BINARY UPGRADE SEQUENCE                                    |
|                                                                          |
| 1. Administrator replaces /usr/sbin/nginx binary on disk with new build. |
| 2. Administrator executes: kill -USR2 <master_pid>                       |
| 3. Old Master renames /run/nginx.pid to /run/nginx.pid.oldbin.           |
| 4. Old Master executes execve() to spawn NEW Master (which writes new    |
|    /run/nginx.pid) and spawns NEW Worker processes.                      |
|                                                                          |
|         +-----------------------------------------------------+          |
|         | Both Old and New Masters are now active in memory!  |          |
|         | Both sets of workers share the listening sockets!   |          |
|         +-----------------------------------------------------+          |
|                                                                          |
| 5. Administrator executes: kill -WINCH <old_master_pid>                  |
|    -> Old Master sends SIGQUIT to OLD workers.                           |
|    -> Old workers stop accepting new connections and gracefully exit.    |
|    -> All new traffic is now seamlessly handled by NEW workers!          |
|                                                                          |
| 6. Verification: Check error logs and curl requests to new workers.     |
|    - If healthy: kill -QUIT <old_master_pid> (Old master terminates).    |
|    - If defective: kill -HUP <old_master_pid> (Restores old workers)     |
|                    kill -QUIT <new_master_pid> (Rolls back safely).      |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### NGINX Command Line Options
```bash
# Print version string only
nginx -v

# Print version, compiler details, and ./configure flags
nginx -V

# Test configuration syntax and verify includes
sudo nginx -t

# Test configuration and print entire compiled include tree to stdout
sudo nginx -T

# Test custom configuration file without affecting running service
sudo nginx -t -c /path/to/test-nginx.conf

# Set prefix path (defaults to /etc/nginx or compile-time prefix)
sudo nginx -p /opt/my-nginx/

# Pass global directives directly via CLI (essential for Docker)
nginx -g "daemon off;"

# Send control signals to running master process
sudo nginx -s reload   # Graceful reload
sudo nginx -s quit     # Graceful stop
sudo nginx -s stop     # Fast immediate stop
sudo nginx -s reopen   # Reopen log files (logrotate)
```

### Standard Unix Signal Mappings
Instead of using `nginx -s <signal>`, administrators can send signals directly to the master process using `kill`:

| Unix Signal | Command Equivalent | Target Process | Action Executed |
| :--- | :--- | :--- | :--- |
| **`SIGHUP`** | `nginx -s reload` | Master | Re-reads config, starts new workers, gracefully terminates old workers |
| **`SIGQUIT`** | `nginx -s quit` | Master / Worker | Graceful shutdown; waits for active requests to finish |
| **`SIGTERM` / `SIGINT`** | `nginx -s stop` | Master / Worker | Immediate termination; aborts active connections |
| **`SIGUSR1`** | `nginx -s reopen` | Master | Reopens access and error log file descriptors |
| **`SIGUSR2`** | *(none)* | Master | Spawns a new master and new workers with updated binary |
| **`SIGWINCH`** | *(none)* | Master | Gracefully shuts down workers while keeping master alive |

---

## 5. Practical Examples

### Example 1: Reloading NGINX via Systemd vs. Direct Unix Signals
```bash
# Method 1: Modern Systemd Command (Wrapper around SIGHUP)
sudo systemctl reload nginx

# Method 2: Native CLI wrapper (Reads /run/nginx.pid and sends SIGHUP)
sudo nginx -s reload

# Method 3: Direct POSIX kill command (Sends SIGHUP directly to master PID)
sudo kill -HUP $(cat /run/nginx.pid)

# Verify reload event in system logs
journalctl -u nginx --since "1 minute ago" --no-pager
# Output: [notice] 1024#1024: signal 1 (SIGHUP) received, reconfiguring
# Output: [notice] 1024#1024: reconfigured
```

### Example 2: Scripting Automated Log Rotation with `kill -USR1`
```bash
#!/usr/bin/env bash
# Production log rotation pattern: /etc/logrotate.d/nginx
set -euo pipefail

LOG_DIR="/var/log/nginx"
PID_FILE="/run/nginx.pid"

# 1. Rename existing active log files
mv "$LOG_DIR/access.log" "$LOG_DIR/access.log.$(date +%F)"
mv "$LOG_DIR/error.log" "$LOG_DIR/error.log.$(date +%F)"

# 2. Instruct NGINX master to reopen log descriptors
# Workers immediately start writing to brand new files with the original names!
if [ -f "$PID_FILE" ]; then
    kill -USR1 $(cat "$PID_FILE")
fi

# 3. Compress rotated files
gzip -9 "$LOG_DIR/access.log.$(date +%F)"
gzip -9 "$LOG_DIR/error.log.$(date +%F)"
```

### Example 3: Executing a Zero-Downtime Binary Upgrade
```bash
# Scenario: Upgrading NGINX from 1.30.7 to 1.30.8 without dropping connections
# 1. Check existing running master PID
OLD_PID=$(cat /run/nginx.pid)
echo "Active Master PID: $OLD_PID"

# 2. Replace /usr/sbin/nginx binary with updated package or compiled build
sudo cp /tmp/new_nginx /usr/sbin/nginx

# 3. Signal master to execute the new binary
sudo kill -USR2 $OLD_PID

# 4. Verify both masters are running
ps aux | grep nginx
# Output shows:
# root 1024 ... nginx: master process /usr/sbin/nginx (OLD)
# root 2048 ... nginx: master process /usr/sbin/nginx (NEW)

# 5. Tell the old master to gracefully stop its workers
sudo kill -WINCH $OLD_PID

# 6. Verify traffic is cleanly serving through new workers
curl -I http://localhost/

# 7. Finalize upgrade: Terminate the old master
sudo kill -QUIT $OLD_PID
echo "Upgrade complete. New Master PID is $(cat /run/nginx.pid)"
```

### Example 4: Running NGINX in Foreground for Docker (`daemon off;`)
```dockerfile
# Dockerfile
FROM alpine:3.20

RUN apk add --no-cache nginx

# Copy custom configuration
COPY nginx.conf /etc/nginx/nginx.conf

# Expose HTTP
EXPOSE 80

# CRITICAL: NGINX defaults to running as a background daemon.
# In Docker, if PID 1 forks and exits, the container halts immediately!
# 'daemon off;' forces NGINX to stay in the foreground as PID 1.
CMD ["nginx", "-g", "daemon off;"]
```

### Example 5: Diagnosing and Terminating Hung Worker Processes
```bash
# Identify worker processes consuming 100% CPU in an infinite loop
top -b -n 1 | grep nginx

# Inspect what system call a specific worker PID is blocked in
sudo strace -p 1420 -c

# If a specific worker is deadlocked or unresponsive, kill that specific worker:
sudo kill -TERM 1420

# The master process detects child termination immediately and spawns a fresh worker:
# [alert] 1024#1024: worker process 1420 exited on signal 15
# [notice] 1024#1024: start worker process 1455
```

---

## 6. Best Practices

- **Always Run `nginx -t` Before Any Signal**: Never execute `nginx -s reload` or `kill -HUP` without verifying syntax first. If syntax is broken, reload is rejected, but catching errors before signaling prevents accidental deployment of broken states.
- **Use `daemon off;` in Container Images**: Always run NGINX in the foreground inside Docker containers. If NGINX forks to the background, container engines assume the process terminated and kill the container.
- **Use `nginx -s quit` for Graceful Termination**: Avoid `systemctl stop nginx` if it sends `SIGTERM` immediately during automated CI/CD deployments. Send `SIGQUIT` so active file uploads and API calls finish gracefully.
- **Automate Binary Upgrades with Verification Guards**: When scripting zero-downtime upgrades, always query health check endpoints after `SIGWINCH` before executing `SIGQUIT` on the old master. If health checks fail, issue `kill -HUP $OLD_PID` to roll back instantly.
- **Check PID File Location**: When sending direct `kill` signals, verify the PID file path declared in `/etc/nginx/nginx.conf` (`pid /run/nginx.pid;`). Sending signals to an outdated PID kills the wrong process!

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| Docker container exits immediately after `docker run` | NGINX defaulted to background daemon mode; PID 1 exited after spawning workers. | Start container with `nginx -g 'daemon off;'`. |
| `nginx: [error] open() "/run/nginx.pid" failed (2: No such file or directory)` | Attempted `nginx -s reload`, but NGINX is not running or PID file was removed by a reboot. | Start NGINX via `sudo systemctl start nginx` or launch the master binary. |
| Sending `SIGTERM` causes 502 Bad Gateway errors on clients | `SIGTERM` stops workers immediately without draining active connections. | Send `SIGQUIT` (graceful shutdown) to let workers drain in-flight requests. |
| Log files keep growing after manual rotation with `mv` | Linux holds file descriptors to deleted/moved files; NGINX continues writing to the old inode. | Send `kill -USR1 <master_pid>` to reopen fresh log files. |
| Running `nginx` as unprivileged user fails with `bind() to 0.0.0.0:80 failed (13: Permission denied)` | Ports below 1024 are privileged; only root can bind them. | Run master as root (workers run as `nginx`) or grant `CAP_NET_BIND_SERVICE`. |
| Multiple master processes running simultaneously without upgrade in progress | An engineer launched `nginx` while another instance was already active. | Inspect processes via `ps aux \| grep nginx` and kill orphan processes. |

---

## 8. Performance, Security & Cost Implications

- **Performance**: Graceful reloads (`SIGHUP`) allow zero-downtime configuration updates. However, issuing `SIGHUP` 1,000 times a minute (e.g., from an un-throttled discovery agent) causes worker process churn and memory bloat, as old workers wait for long-lived keepalive connections to drain.
- **Security**: The master process must run as `root` to bind low ports and manage file permissions, but it drops privileges immediately for worker processes. Never configure `user root;` inside `nginx.conf`, as a worker compromise would grant total root access to the underlying operating system.
- **Cost**: Flawless live binary upgrades eliminate maintenance windows and redundant standby cloud infrastructure, enabling continuous deployment on single-node or low-redundancy systems.

---

## 9. Comparison Table

| Signal | Unix Name | Trigger Method | Master Action | Worker Action |
| :--- | :--- | :--- | :--- | :--- |
| **Reload** | `SIGHUP` | `nginx -s reload` | Re-reads config; starts new workers | Old workers finish requests and exit |
| **Graceful Stop**| `SIGQUIT` | `nginx -s quit` | Waits for workers to terminate, then exits | Closes listeners, finishes active requests |
| **Fast Stop** | `SIGTERM` / `SIGINT` | `nginx -s stop` | Immediately kills workers, then exits | Drops all active connections and exits |
| **Reopen Logs** | `SIGUSR1` | `nginx -s reopen` | Reopens log file descriptors | Reopens log file descriptors |
| **Upgrade Binary**| `SIGUSR2` | `kill -USR2 <pid>`| Spawns new master with new executable | Continues serving until WINCH/QUIT |
| **Stop Workers** | `SIGWINCH` | `kill -WINCH <pid>`| Keeps master alive, drops worker pool | Gracefully terminates |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain the step-by-step internal process of what happens when you run `nginx -s reload`.</strong></summary>

**Answer**:
1. The CLI command `nginx -s reload` starts a temporary NGINX helper process.
2. The helper process reads the PID file (e.g., `/run/nginx.pid`) to find the PID of the active master process.
3. The helper sends the `SIGHUP` signal to the master process and exits.
4. The master process validates the syntax of `nginx.conf` and all included files. If invalid, it logs an error and aborts with zero changes.
5. If valid, the master opens any new listening sockets or shared memory zones required by the updated configuration.
6. The master forks a new set of worker processes running the new configuration.
7. The master sends `SIGQUIT` to the old worker processes.
8. The old worker processes close their listening sockets (so new connections only go to the new workers), finish processing all currently active client requests, and terminate cleanly.
</details>

<details>
<summary><strong>2. How does an engineer perform an on-the-fly binary upgrade of NGINX without dropping connections?</strong></summary>

**Answer**:
1. Replace the `/usr/sbin/nginx` executable binary on disk with the new version.
2. Send `kill -USR2 <old_master_pid>`. The old master renames its PID file to `nginx.pid.oldbin`, executes the new binary, and starts a new master process with its own workers.
3. Both old and new workers now share the same socket listeners and process traffic concurrently.
4. Send `kill -WINCH <old_master_pid>`. The old master gracefully shuts down its workers, shifting 100% of incoming traffic to the new workers.
5. Verify application health and logs on the new workers.
6. Send `kill -QUIT <old_master_pid>`. The old master terminates, completing the upgrade.
If issues arise in step 5, send `kill -HUP <old_master_pid>` to restart old workers, and `kill -QUIT <new_master_pid>` to rollback cleanly.
</details>

<details>
<summary><strong>3. Why is `nginx -g "daemon off;"` standard practice in Dockerfiles? What happens if it is omitted?</strong></summary>

**Answer**:
By default, NGINX is designed to run as a traditional Unix background daemon: the initial process forks child workers and the parent process terminates, releasing the terminal.

In Docker and container runtimes, a container's lifecycle is tied directly to the execution state of **PID 1**. If `daemon off;` is omitted, the initial `nginx` process spawns the background daemon and exits immediately. The container runtime detects that PID 1 has terminated and stops the container instantly with exit code 0. Setting `daemon off;` forces the NGINX master process to remain in the foreground as PID 1, keeping the container alive.
</details>

<details>
<summary><strong>4. What is the difference between `SIGTERM` and `SIGQUIT` in NGINX?</strong></summary>

**Answer**:
- **`SIGTERM` (or `SIGINT`)**: Fast / Immediate Shutdown. The master process signals all workers to abort immediately. All active client TCP connections are abruptly severed, ongoing file transfers are interrupted, and any in-flight proxy requests receive connection resets.
- **`SIGQUIT`**: Graceful Shutdown. The workers close their listening sockets to stop accepting new requests, but keep active client connections open until they complete. Once all active requests finish and response bytes are delivered, the workers and master terminate cleanly.
</details>

<details>
<summary><strong>5. Why does log rotation require sending the `SIGUSR1` signal to NGINX?</strong></summary>

**Answer**:
In Linux, moving or renaming a file (`mv access.log access.log.1`) does not alter the underlying inode or close open file descriptors. Because NGINX worker processes hold open file descriptors to the log files, they will continue writing log bytes to `access.log.1` even after it has been renamed.

Sending `SIGUSR1` to the master process instructs NGINX to re-open all log files using their configured pathnames. NGINX opens the new `access.log`, switches its file descriptors, and releases the handle on the renamed file so backup tools can compress or archive it.
</details>

<details>
<summary><strong>6. What happens if a worker process crashes unexpectedly (e.g., due to a segmentation fault)?</strong></summary>

**Answer**:
The master process acts as a supervisor: it registers a signal handler for `SIGCHLD`. When a worker process crashes, the Linux kernel reaps the process and sends `SIGCHLD` to the master process.

The master logs an alert:
`[alert] 1024#1024: worker process 1025 exited on signal 11 (core dumped)`
The master process immediately forks a new worker process to replace the failed child, restoring the configured worker pool capacity within milliseconds.
</details>

<details>
<summary><strong>7. How can you test an alternate configuration file without overwriting the production `/etc/nginx/nginx.conf`?</strong></summary>

**Answer**:
Use the `-c` (configuration file) and `-t` (test) flags:
```bash
sudo nginx -t -c /path/to/staging_nginx.conf
```
NGINX will parse and validate the specified file and all of its relative includes without affecting the active, running production NGINX instance.
</details>

<details>
<summary><strong>8. What is the difference between `nginx -s stop` and `nginx -s quit`?</strong></summary>

**Answer**:
- `nginx -s stop`: Triggers a `SIGTERM` signal. It terminates NGINX immediately without waiting for client requests to finish. Active connections are dropped.
- `nginx -s quit`: Triggers a `SIGQUIT` signal. It initiates a graceful shutdown, allowing workers to finish servicing current HTTP requests and drain connections before exiting.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Query Running NGINX Processes</strong></summary>

**Task**: Write a shell command to list the NGINX master process and all child worker processes, showing their PIDs, user ownership, and command lines.

**Solution**:
```bash
ps -ef | grep "[n]ginx"
# Output shows master running as root and workers running as nginx/www-data.
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Extract Master Process PID from File</strong></summary>

**Task**: Write the bash command to query the active master PID from the standard system runtime path and send a graceful reload signal using `kill`.

**Solution**:
```bash
sudo kill -HUP $(cat /run/nginx.pid)
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Validate Configuration and Perform Hot Reload</strong></summary>

**Task**: Author a hardened deployment one-liner that tests NGINX configuration syntax, and if and only if syntax is valid, executes a hot reload.

**Solution**:
```bash
sudo nginx -t && sudo nginx -s reload
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Simulate Log Rotation with `USR1`</strong></summary>

**Task**: Write a script that renames `/tmp/test_access.log` to `/tmp/test_access.log.bak`, signals NGINX using `SIGUSR1`, and verifies that NGINX creates a new log file.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

# Rotate
mv /var/log/nginx/access.log /var/log/nginx/access.log.bak

# Signal master
sudo kill -USR1 $(cat /run/nginx.pid)

# Verify new access.log is created
ls -la /var/log/nginx/access.log
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Script an Automated Zero-Downtime Binary Upgrade</strong></summary>

**Task**: Author a production shell script that executes an automated zero-downtime binary upgrade, checking health before terminating the old master.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

PID_FILE="/run/nginx.pid"
OLD_PID=$(cat "$PID_FILE")

echo "Step 1: Spawning new master with USR2..."
kill -USR2 "$OLD_PID"
sleep 2

echo "Step 2: Gracefully stopping old workers with WINCH..."
kill -WINCH "$OLD_PID"
sleep 2

echo "Step 3: Verifying health check on new workers..."
if curl -sf http://127.0.0.1/healthz >/dev/null; then
    echo "Health check passed. Terminating old master..."
    kill -QUIT "$OLD_PID"
    echo "Upgrade successful!"
else
    echo "HEALTH CHECK FAILED! Rolling back to old master..."
    kill -HUP "$OLD_PID"
    NEW_PID=$(cat "$PID_FILE")
    kill -QUIT "$NEW_PID"
    echo "Rollback complete."
    exit 1
fi
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Audit Worker Process CPU Affinity and Limits</strong></summary>

**Task**: Write a bash script that inspects each running NGINX worker process, querying its open file descriptor count and CPU affinity mask.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

echo "Auditing NGINX Worker Processes..."
for pid in $(pgrep -f "nginx: worker process"); do
    FD_COUNT=$(ls -1 /proc/$pid/fd 2>/dev/null | wc -l)
    AFFINITY=$(taskset -cp $pid 2>/dev/null | awk -F: '{print $2}')
    echo "Worker PID $pid: Open FDs = $FD_COUNT | CPU Affinity =$AFFINITY"
done
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Command | Description |
| :--- | :--- | :--- |
| **Test Config** | `sudo nginx -t` | Syntax test only |
| **Dump Config** | `sudo nginx -T` | Dumps complete compiled config |
| **Hot Reload** | `sudo nginx -s reload` | Reloads config with 0 downtime |
| **Graceful Stop** | `sudo nginx -s quit` | Waits for connections to drain |
| **Fast Stop** | `sudo nginx -s stop` | Immediate connection abort |
| **Reopen Logs** | `sudo nginx -s reopen` | Reopens log file descriptors |
| **Binary Upgrade** | `kill -USR2 <master_pid>`| Spawns new master for live upgrade |
| **Drain Workers** | `kill -WINCH <master_pid>`| Drains old workers during upgrade |
| **Docker Foreground**| `nginx -g 'daemon off;'` | Keeps NGINX running as PID 1 |

---

## 13. Further Reading & Official Documentation

- [Official NGINX Documentation: Controlling NGINX Processes](https://nginx.org/en/docs/control.html)
- [Official NGINX Documentation: Command-Line Parameters](https://nginx.org/en/docs/switches.html)
- [Official NGINX Documentation: Upgrading Executable on the Fly](https://nginx.org/en/docs/control.html#upgrade)
- [Docker Official Image: NGINX Best Practices](https://github.com/nginxinc/docker-nginx)
- [Linux man 2 kill: Send Signal to a Process](https://man7.org/linux/man-pages/man2/kill.2.html)

---

[<- 05-directives-reference-by-context.md](./05-directives-reference-by-context.md) | [Back to Module Overview](./README.md) | [07-default-server-and-catch-all-hosts.md ->](./07-default-server-and-catch-all-hosts.md)
