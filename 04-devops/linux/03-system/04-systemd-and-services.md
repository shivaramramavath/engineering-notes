<!--
================================================================================
Metadata:
  Title: Systemd & Service Management
  Track: Linux & Systems Engineering / DevOps
  Difficulty: Intermediate
  Estimated Time: 60 minutes
  Prerequisites: 03-system/01-processes-and-signals.md
  Key Terminology: systemd, PID 1, systemctl, Unit Types (.service, .target, .socket, .timer, .mount), Unit Sections ([Unit], [Service], [Install]), Restart Policies, Cgroup Slices, Security Sandboxing (ProtectSystem, NoNewPrivileges, PrivateTmp)
================================================================================
-->

# Systemd & Service Management

[<- 03-package-management.md](./03-package-management.md) | [Back to Module Overview](./README.md) | [05-journald-and-logging.md ->](./05-journald-and-logging.md)

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

In modern Linux distributions (Ubuntu, Debian, RHEL, Rocky Linux, Fedora, Arch, SUSE), **`systemd`** is the system and service manager that executes as **PID 1**. It is the very first userspace process spawned by the Linux kernel during boot, and it serves as the ultimate parent and supervisor for every daemon, container runtime, and background worker running on the machine.

Prior to systemd, Linux relied on legacy System V (SysV) init scripts (`/etc/init.d/`). SysV was sequential, slow, and fundamentally fragile:
- Daemons were managed by hundreds of lines of brittle shell scripts.
- Process tracking was done using text PID files (`/var/run/app.pid`), which easily became stale on crashes.
- Child processes that double-forked (`daemonize()`) could escape supervision completely.

Systemd modernized Linux systems engineering by introducing declarative unit files, parallelized dependency-driven service startup, strict process tracking using kernel **cgroups**, automatic failure restarts, socket activation, and native kernel security sandboxing.

---

## 2. Core Concepts

### Systemd Unit Types
Everything managed by systemd is encapsulated as an abstracted **Unit**. Unit files have specific file extensions denoting their purpose:

| Unit Type | Suffix | Operational Purpose |
| :--- | :---: | :--- |
| **Service** | `.service` | Manages application daemons and background processes (start, stop, restart, supervision). |
| **Target** | `.target` | Synchronization points and groupings of units (replaces legacy SysV runlevels, e.g. `multi-user.target`). |
| **Socket** | `.socket` | Encapsulates an IPC or network socket for on-demand **Socket Activation**. |
| **Timer** | `.timer` | Schedulers and monotonic timers (modern replacement for cron). |
| **Mount / AutoMount** | `.mount` / `.automount` | Filesystem mount point supervision (derived from or overrides `/etc/fstab`). |
| **Path** | `.path` | Activates services based on filesystem inotify events (e.g., file creation). |
| **Slice** | `.slice` | Manages hierarchical cgroup resource pools (e.g., `system.slice`, `user.slice`). |

### Unit File Directory Precedence
Systemd searches for unit files across three primary directory tiers, evaluated in strict order of overriding precedence:

```
+-------------------------------------------------------------------------+
| SYSTEMD UNIT FILE OVERRIDE PRECEDENCE                                   |
|                                                                         |
|  1. Runtime Ephemeral: /run/systemd/system/ (Highest Precedence)        |
|     (Created dynamically by processes, wiped on reboot)                 |
|                            │                                            |
|                            v                                            |
|  2. Administrator Custom: /etc/systemd/system/                          |
|     (User modifications, custom services, drop-ins. Overrides vendor!)  |
|                            │                                            |
|                            v                                            |
|  3. Distribution Vendor:  /usr/lib/systemd/system/ (Lowest Precedence)  |
|     (Installed by package managers like apt or dnf. Never edit directly!)|
+-------------------------------------------------------------------------+
```

### Anatomy of a `.service` Unit File
A systemd service unit is structured into three standard INI-style sections:
1. **`[Unit]`**: Metadata and dependency declarations (`Description`, `Documentation`, `After`, `Requires`, `Wants`).
2. **`[Service]`**: Execution specifications, binary paths (`ExecStart`, `ExecReload`), lifecycle policies (`Type`, `Restart`), credentials (`User`, `Group`), and sandboxing directives (`ProtectSystem`).
3. **`[Install]`**: Installation instructions evaluated by `systemctl enable` (`WantedBy`, `RequiredBy`).

---

## 3. How It Works Internally

Systemd guarantees foolproof process tracking by binding every unit to a dedicated **Control Group (Cgroup)**:

```mermaid
flowchart TD
    A[PID 1 systemd] -->|Creates cgroup slice| B[/sys/fs/cgroup/system.slice/nginx.service]
    B --> C[Executes ExecStart: /usr/sbin/nginx]
    C --> D[Master Process PID 1045 placed in cgroup]
    D -->|Worker forks: fork| E[Worker 1 PID 1046 in cgroup]
    D -->|Worker forks: fork| F[Worker 2 PID 1047 in cgroup]
    
    G[Administrator runs: systemctl stop nginx] --> H[systemd reads cgroup membership]
    H --> I[Signals ALL PIDs in /sys/fs/cgroup/.../cgroup.procs simultaneously]
    I --> J[Zero orphaned processes escape shutdown!]
```

### Why Double-Forking Cannot Escape Systemd
In legacy SysV init, if a rogue or compromised process executed `fork()` twice, it severed its relationship with its parent and re-parented to PID 1. If the init script called `kill $(cat /var/run/app.pid)`, only the original parent was terminated; the grandchildren survived as unkillable orphan daemons.
In systemd, the Linux kernel automatically assigns all child processes to the **exact same cgroup** as their parent. When systemd stops a service (`KillMode=control-group`), it signals every single process listed in that service's cgroup, making process leaks architecturally impossible.

---

## 4. Syntax & Usage

### 1. Service Lifecycle Management with `systemctl`
```bash
# Start a service immediately
sudo systemctl start nginx

# Stop a running service
sudo systemctl stop nginx

# Restart service (stop + start)
sudo systemctl restart nginx

# Reload configuration without dropping client connections
sudo systemctl reload nginx

# Check detailed status, execution state, active cgroup, and recent log tail
systemctl status nginx

# Verify if service is currently running
systemctl is-active nginx

# Verify if service is enabled at boot
systemctl is-enabled nginx

# Enable service to start automatically on system boot
sudo systemctl enable nginx

# Disable service from starting on boot
sudo systemctl disable nginx

# Enable and start in a single command
sudo systemctl enable --now nginx

# Mask a service (links unit to /dev/null, rendering it completely unstartable)
sudo systemctl mask apache2
sudo systemctl unmask apache2
```

### 2. Managing System Targets (Runlevels)
```bash
# View the current default boot target
systemctl get-default
# Output: multi-user.target (Headless Server) or graphical.target (Desktop GUI)

# Change default boot target
sudo systemctl set-default multi-user.target

# Switch to a target immediately (e.g. Rescue / Single-User mode)
sudo systemctl isolate rescue.target
```

### 3. Reloading Systemd After Unit File Edits
Whenever you author, edit, or delete any file in `/etc/systemd/system/`, systemd must reload its internal dependency graph:
```bash
sudo systemctl daemon-reload
```

---

## 5. Practical Examples

### 1. Basic: Authoring a Clean Production Node.js Service Unit
Deploy a backend Node.js microservice running under an unprivileged user:
```ini
# /etc/systemd/system/orders-api.service
[Unit]
Description=Orders REST API Microservice
Documentation=https://docs.internal.company.com/orders
After=network.target postgresql.service
Wants=postgresql.service

[Service]
Type=simple
User=orders-app
Group=orders-app
WorkingDirectory=/opt/orders-api
EnvironmentFile=/etc/default/orders-api
ExecStart=/usr/bin/node /opt/orders-api/server.js
ExecReload=/bin/kill -HUP $MAINPID

# Restart policy: Restart if crashed or killed, but not on clean exit 0
Restart=on-failure
RestartSec=5s

# Resource constraints
LimitNOFILE=65536
MemoryMax=1G

[Install]
WantedBy=multi-user.target
```
*Activation*:
```bash
sudo systemctl daemon-reload
sudo systemctl enable --now orders-api.service
```

### 2. Real-World: Non-Destructive Unit Customization via Drop-in Overrides
Never edit `/usr/lib/systemd/system/nginx.service` directly, as distro package updates will overwrite your changes. Instead, use systemd **Drop-ins**:
```bash
# Opens an interactive editor creating /etc/systemd/system/nginx.service.d/override.conf
sudo systemctl edit nginx
```
Add only the specific parameters you wish to override:
```ini
[Service]
# Increase file descriptor limit for high-concurrency proxying
LimitNOFILE=100000
# Add custom security sandbox
ProtectSystem=strict
ReadWritePaths=/var/log/nginx /var/cache/nginx
```
Verify the composite configuration:
```bash
systemctl cat nginx
```

### 3. Edge Case: Understanding Service `Type=` Directives
Choosing the wrong `Type=` in `[Service]` causes services to hang during startup or crash unexpectedly:
- **`Type=simple`** (Default): Systemd assumes the process started by `ExecStart` is the main service. It considers the unit active immediately.
- **`Type=forking`**: Required for traditional daemons (like classic Apache or Nginx with daemonization enabled) that call `fork()`. Systemd waits for the parent to exit and tracks the child via `PIDFile=`.
- **`Type=oneshot`**: For scripts that run a task and exit (e.g., database schema migrations). Systemd waits for process completion before proceeding. Often paired with `RemainAfterExit=yes`.
- **`Type=notify`**: The application sends an explicit D-Bus notification (`sd_notify("READY=1")`) when fully initialized. Systemd blocks dependent services until notification is received.

### 4. Failure Case: The Rapid Crash Restart Loop (Rate-Limiting)
A service crashes immediately on boot due to a syntax error in its config file. Naive `Restart=always` could burn 100% CPU restarting thousands of times a second:
*Systemd's Built-in Protection*:
```text
systemd[1]: orders-api.service: Start request repeated too quickly.
systemd[1]: orders-api.service: Failed with result 'start-limit-hit'.
```
*Configuring Crash Limits*:
In the `[Unit]` section:
```ini
StartLimitIntervalSec=60s
StartLimitBurst=5
```
If the service crashes more than 5 times within 60 seconds, systemd places it in a failed state and ceases restart attempts, preserving system stability.

### 5. Production-Grade: Hardened Zero-Trust Microservice Sandbox
A production unit file showcasing systemd's extensive Linux security sandboxing capabilities (Namespaces, Seccomp, and Capabilities):
```ini
# /etc/systemd/system/payment-gateway.service
[Unit]
Description=Payment Gateway Service
After=network.target

[Service]
Type=simple
User=payment-svc
Group=payment-svc
WorkingDirectory=/opt/payment
ExecStart=/opt/payment/bin/gateway

Restart=always
RestartSec=3s

# ==============================================================================
# Linux Kernel Security Sandboxing (Zero-Trust Directives)
# ==============================================================================
# Mounts /usr, /boot, and /etc read-only for this process
ProtectSystem=strict

# Prevents access to /home, /root, and /run/user
ProtectHome=true

# Provides an isolated, private /tmp directory invisible to other processes
PrivateTmp=true

# Creates a private network namespace (only loopback available; disable if network required!)
# PrivateNetwork=true

# Blocks process and any children from ever gaining new privileges (drops SUID)
NoNewPrivileges=true

# Restricts process to binding network ports, dropping CAP_SYS_ADMIN, CAP_NET_ADMIN
CapabilityBoundingSet=CAP_NET_BIND_SERVICE
AmbientCapabilities=CAP_NET_BIND_SERVICE

# Hides all hardware device nodes except /dev/null, /dev/zero, /dev/random
PrivateDevices=true

# Restricts system call filtering via Seccomp
SystemCallFilter=@system-service
SystemCallErrorNumber=EPERM

# Explicitly allowed writable directories
ReadWritePaths=/var/log/payment /var/lib/payment

[Install]
WantedBy=multi-user.target
```

---

## 6. Best Practices

1. **Never Edit `/usr/lib/systemd/system/`**: Always place custom units in `/etc/systemd/system/`, and customize distro units using `sudo systemctl edit <unit>`.
2. **Always Run Daemons Under Dedicated Service Accounts**: Never specify `User=root` unless raw hardware access or initial capability binding is strictly required.
3. **Always Set `Restart=on-failure`**: Avoid `Restart=always` unless intentional; `on-failure` allows clean administrative stops (`exit 0`) without unwanted restarts.
4. **Always Define `LimitNOFILE` Explicitly**: By default, systemd services do not inherit the limits from `/etc/security/limits.conf`. Set `LimitNOFILE=65536` in the service unit.
5. **Always Call `systemctl daemon-reload`**: Changes to `.service` files on disk are ignored by systemd until `daemon-reload` is executed.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Antipattern | Underlying Cause | Consequence | Production Solution |
| :--- | :--- | :--- | :--- |
| Forgetting `systemctl daemon-reload` | Modifying unit files without notifying PID 1. | Systemd executes the old in-memory unit configuration. | Run `sudo systemctl daemon-reload`. |
| Missing absolute binary paths | Systemd does not search standard `$PATH` for `ExecStart`. | `Executable path is not absolute: node`. | Use full path: `ExecStart=/usr/bin/node /path/app.js`. |
| `Type=simple` with daemonizing app | The app double-forks and parent exits; systemd thinks service stopped. | Service is marked failed or restarts continuously. | Use `Type=forking` and define `PIDFile=`, or disable app daemonization. |
| Masking a service unintentionally | Creating broken symlinks or running `systemctl mask`. | `Unit is masked. Failed to start`. | Run `sudo systemctl unmask <unit>`. |
| Misunderstanding `Requires` vs `Wants` | `Requires` creates a hard failure dependency. | If dependent service fails to start, target service is aborted. | Use `Wants=` for soft dependencies; `Requires=` for strict needs. |
| Forgetting `After=` with dependencies | `Requires` controls *what* starts, but **not the order**. | Services start concurrently; client crashes before database is ready. | Always combine `Requires=db` with `After=db`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Parallel Bootstrapping**: Systemd analyzes the unit dependency graph (DAG) and launches independent services in parallel. This slashed Linux server boot times from 45+ seconds in SysV init down to **< 2 seconds** on modern cloud instances.
- **Socket Activation Overhead**: Socket activation allows daemons to remain completely dormant until the first TCP packet arrives on their port, consuming 0 MB RAM until called.

### Security
- **Built-in Seccomp & Namespaces**: Prior to systemd, isolating a daemon required Docker containers or complex chroots. Directives like `ProtectSystem=strict` and `PrivateTmp=true` isolate compromised daemons natively with negligible performance overhead.

### Cost
- In cloud autoscaling groups, rapid systemd startup times allow new VM instances to achieve healthcheck readiness faster, reducing autoscaling lag during traffic spikes.

---

## 9. Comparison Table

| Attribute | Systemd (`systemctl`) | Legacy SysV Init (`/etc/init.d/`) | Upstart (Deprecated Ubuntu) |
| :--- | :--- | :--- | :--- |
| **Configuration Format**| Declarative INI Unit Files | Procedural Bash Scripts | Declarative Stanza Files |
| **Process Tracking** | **Kernel Cgroups (100% reliable)**| Fragile text PID files | Ptrace tracking (fragile) |
| **Startup Model** | Concurrent / Dependency DAG | Strictly Sequential | Event-driven |
| **Process Sandboxing** | Built-in (Namespaces/Seccomp) | None (Requires manual chroot) | Limited |
| **Logging** | Unified binary `journald` | Plain text files via syslog | Log files in `/var/log/upstart/` |
| **Resource Limits** | Built-in (`MemoryMax`, `CPUQuota`)| `/etc/security/limits.conf` | `limit` stanzas |

---

## 10. Real-World Interview Questions

### Q1: What is the exact difference between `Requires=` and `Wants=` in systemd?
<details>
<summary>Answer</summary>
Both directives declare dependencies between units, but handle failure differently:
- **`Requires=` (Hard Dependency)**: If Unit A has `Requires=Unit B`, starting Unit A will also attempt to start Unit B. If Unit B fails to start, Unit A **will not be started**. Furthermore, if Unit B is stopped while Unit A is running, Unit A will be stopped as well.
- **`Wants=` (Soft Dependency)**: Starting Unit A will attempt to start Unit B, but if Unit B fails to start or crashes, Unit A **continues running unaffected**.
*Crucial Note*: Neither `Requires` nor `Wants` guarantees startup ordering! You must pair them with `After=` to ensure Unit B finishes initializing before Unit A starts.
</details>

### Q2: How does `systemctl mask <service>` work under the hood?
<details>
<summary>Answer</summary>
`systemctl mask <service>` creates a symbolic link in `/etc/systemd/system/` pointing the service unit file directly to `/dev/null`:
```bash
/etc/systemd/system/nginx.service -> /dev/null
```
Because `/etc/systemd/system/` has higher precedence than `/usr/lib/systemd/system/`, systemd reads the unit as `/dev/null`. This makes it completely impossible for any user, script, or dependent service to start the masked service, either manually or automatically, until it is explicitly unmasked.
</details>

### Q3: What is the difference between `systemctl reload` and `systemctl restart`?
<details>
<summary>Answer</summary>
- **`systemctl restart <service>`**: Fully terminates the running process (sending `SIGTERM`, waiting for exit) and then starts a brand-new process instance. This drops existing TCP connections and causes a momentary service interruption.
- **`systemctl reload <service>`**: Sends a signal (typically `SIGHUP` or invokes `ExecReload=`) to the running daemon instructing it to re-read its configuration files from disk **without stopping the process or dropping active client connections**. If the daemon does not support configuration reloading, `reload` will fail.
</details>

### Q4: How does systemd track child processes to ensure complete cleanup on service shutdown?
<details>
<summary>Answer</summary>
Systemd places every service into its own dedicated Linux **Control Group (Cgroup)** in the unified cgroup hierarchy (`/sys/fs/cgroup/system.slice/<service_name>`).
When a process forks child processes, the Linux kernel automatically assigns those children to the same cgroup. When `systemctl stop` is issued, systemd inspects the `cgroup.procs` file for that unit and issues signals (`SIGTERM`, followed by `SIGKILL` if necessary) to **every PID in the cgroup**. This guarantees that no rogue or double-forked child processes can escape termination.
</details>

### Q5: What is a Systemd "Target", and what is its SysV init equivalent?
<details>
<summary>Answer</summary>
A **Target** (`.target`) is a special systemd unit that groups other units together through dependencies, establishing synchronization milestones during system state transitions.
It directly replaces legacy System V **Runlevels**:
- `poweroff.target` -> Runlevel 0 (Halt)
- `rescue.target` -> Runlevel 1 (Single-User recovery)
- `multi-user.target` -> Runlevels 2, 3, 4 (Standard multi-user non-graphical server)
- `graphical.target` -> Runlevel 5 (Multi-user with graphical desktop)
- `reboot.target` -> Runlevel 6 (Reboot)
</details>

### Q6: What does `systemctl edit <service>` do, and why is it preferred over editing unit files directly?
<details>
<summary>Answer</summary>
`systemctl edit <service>` creates and opens a drop-in override file at:
`/etc/systemd/system/<service>.service.d/override.conf`
**Why it is preferred**:
1. It does not touch the vendor-supplied unit file in `/usr/lib/systemd/system/`, meaning package updates via `apt` or `dnf` will never overwrite your configurations.
2. It automatically invokes `systemctl daemon-reload` when you save and exit the editor.
3. You only define the specific keys you want to override rather than duplicating the entire 50-line unit file.
</details>

### Q7: What is Socket Activation, and what are its advantages?
<details>
<summary>Answer</summary>
Socket Activation is a mechanism where systemd creates and listens on a network or IPC socket (`.socket` unit) on behalf of a service before the service is even running.
**Advantages**:
1. **On-Demand Resource Conservation**: Daemons that are rarely used (e.g., CUPS printing or debugging daemons) consume 0 MB of RAM until a request hits the socket, at which point systemd spawns the daemon and passes the open file descriptor.
2. **Zero-Downtime Restarts**: If the backend service restarts or crashes, the listening socket remains open in systemd. Incoming requests queue up in the kernel socket buffer rather than failing with "Connection refused".
</details>

### Q8: Explain what `KillMode=process` vs `KillMode=control-group` means.
<details>
<summary>Answer</summary>
- **`KillMode=control-group`** (Default): On service stop, systemd kills the main process and **all child processes** remaining in the service's cgroup.
- **`KillMode=process`**: On service stop, systemd kills **only the main process**, leaving any spawned child processes running.
*Warning*: `KillMode=process` is considered dangerous and is deprecated in modern systemd because it leads to orphaned processes and resource leaks.
</details>

---

## 11. Practice Exercises

### Exercise 1: Mask and Unmask a Service (Easy)
**Task**: Mask the `cron` (or `systemd-timesyncd`) service so it cannot be started, verify that attempting to start it fails, and then cleanly unmask it.

<details>
<summary>Solution</summary>

```bash
# Mask service
sudo systemctl mask systemd-timesyncd

# Verify failure on start attempt
sudo systemctl start systemd-timesyncd || true

# Unmask service
sudo systemctl unmask systemd-timesyncd
sudo systemctl status systemd-timesyncd
```
</details>

### Exercise 2: Create a Drop-in Override for Service Restarts (Easy)
**Task**: Use `systemctl edit` to add a drop-in override to `cron` setting `Restart=always` and `RestartSec=10s`. Verify using `systemctl cat cron`.

<details>
<summary>Solution</summary>

```bash
sudo systemctl edit cron
```
*In the editor, insert*:
```ini
[Service]
Restart=always
RestartSec=10s
```
*Verification*:
```bash
systemctl cat cron
```
</details>

### Exercise 3: Inspecting Service Cgroup Membership (Medium)
**Task**: Start `nginx` or another daemon. Find its active cgroup slice path in `/sys/fs/cgroup/`, list all running PIDs inside that cgroup via `cgroup.procs`, and compare with `systemctl status`.

<details>
<summary>Solution</summary>

```bash
# Verify active PIDs via systemctl
systemctl status nginx | grep -A 5 "CGroup:"

# Read directly from Linux kernel cgroups v2 filesystem
cat /sys/fs/cgroup/system.slice/nginx.service/cgroup.procs
```
</details>

### Exercise 4: Building a One-Shot Database Seed Unit (Medium)
**Task**: Author a `Type=oneshot` service named `db-seed.service` that creates a file `/tmp/db_initialized.flag`. Ensure that systemd marks the service active and does not attempt to restart it (`RemainAfterExit=yes`).

<details>
<summary>Solution</summary>

Create `/etc/systemd/system/db-seed.service`:
```ini
[Unit]
Description=Database Seeding Job

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/touch /tmp/db_initialized.flag

[Install]
WantedBy=multi-user.target
```
*Execution*:
```bash
sudo systemctl daemon-reload
sudo systemctl start db-seed.service
systemctl is-active db-seed.service # Outputs: active
```
</details>

### Exercise 5: Investigating Failed Service State & Resetting Failed Counters (Hard)
**Task**: Intentionally create a failing service `broken.service` with an invalid `ExecStart=/bin/false`. Start it until it trips the start rate limit. Inspect the failure reasons, fix the unit, reset the failure counter with `systemctl reset-failed`, and start it successfully.

<details>
<summary>Solution</summary>

```bash
# 1. Create broken unit
cat <<EOF | sudo tee /etc/systemd/system/broken.service
[Unit]
Description=Broken Test Service

[Service]
Type=simple
ExecStart=/bin/false
Restart=always
StartLimitBurst=2
StartLimitIntervalSec=10s
EOF

# 2. Trigger failure
sudo systemctl daemon-reload
sudo systemctl start broken.service || true
sleep 2

# 3. Inspect failed state
systemctl status broken.service

# 4. Repair service
sudo sed -i 's|/bin/false|/bin/sleep 60|' /etc/systemd/system/broken.service

# 5. Clear failed state and reload
sudo systemctl daemon-reload
sudo systemctl reset-failed broken.service
sudo systemctl start broken.service
systemctl is-active broken.service

# Clean up
sudo systemctl stop broken.service
sudo rm -f /etc/systemd/system/broken.service
sudo systemctl daemon-reload
```
</details>

### Exercise 6: Socket Activation with Netcat Echo Server (Hard)
**Task**: Build a socket-activated TCP service: create `echo-server.socket` listening on port 9999 and `echo-server.service` that handles the connection using `/bin/cat`. Verify that connecting via `nc localhost 9999` starts the service on demand.

<details>
<summary>Solution</summary>

1. Create socket unit `/etc/systemd/system/echo-server.socket`:
```ini
[Unit]
Description=Echo Server Socket

[Socket]
ListenStream=9999
Accept=yes

[Install]
WantedBy=sockets.target
```

2. Create template service unit `/etc/systemd/system/echo-server@.service`:
```ini
[Unit]
Description=Echo Server Instance
StandardInput=socket
StandardOutput=socket

[Service]
ExecStart=/bin/cat
```

3. Enable and start the socket:
```bash
sudo systemctl daemon-reload
sudo systemctl enable --now echo-server.socket
```

4. Test on-demand activation:
```bash
echo "Hello Socket Activation" | nc -N localhost 9999
# Output: Hello Socket Activation
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Command | Description |
| :--- | :--- | :--- |
| **Start Service** | `sudo systemctl start <unit>` | Initiates unit execution |
| **Stop Service** | `sudo systemctl stop <unit>` | Terminates running unit |
| **Restart Service** | `sudo systemctl restart <unit>` | Full stop and restart |
| **Reload Config** | `sudo systemctl reload <unit>` | Re-reads configs without dropping connections |
| **Check Status** | `systemctl status <unit>` | Displays state, PID, logs, and cgroup |
| **Enable on Boot** | `sudo systemctl enable <unit>` | Creates symlinks in `/etc/systemd/system/` |
| **Disable Boot** | `sudo systemctl disable <unit>`| Removes symlinks |
| **Mask Service** | `sudo systemctl mask <unit>` | Links unit to `/dev/null` |
| **Edit Drop-in** | `sudo systemctl edit <unit>` | Creates override in `<unit>.d/override.conf` |
| **Reload Daemon** | `sudo systemctl daemon-reload` | Reloads unit files from disk into PID 1 |
| **Reset Failed** | `sudo systemctl reset-failed` | Clears failed state and burst counters |
| **List Unit Files**| `systemctl list-unit-files` | Reports enabled/disabled/masked states |

---

## 13. Further Reading & Official Documentation

- [systemd System and Service Manager Documentation](https://systemd.io/)
- [Linux Man Pages: systemd(1)](https://man7.org/linux/man-pages/man1/systemd.1.html)
- [Linux Man Pages: systemctl(1)](https://man7.org/linux/man-pages/man1/systemctl.1.html)
- [Linux Man Pages: systemd.service(5)](https://man7.org/linux/man-pages/man5/systemd.service.5.html)
- [Lennart Poettering: systemd for Administrators Blog Series](http://0pointer.de/blog/projects/systemd-for-admins-1.html)

---

[<- 03-package-management.md](./03-package-management.md) | [Back to Module Overview](./README.md) | [05-journald-and-logging.md ->](./05-journald-and-logging.md)
