# Processes, Signals & Systemd: Technical Interview Q&A

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Process Lifecycle, Signals, Zombie Reaping & Systemd Architecture |
| **Module** | `08-interview` — Technical Interview Preparation & Scenario Q&A |
| **Track** | `04-devops/linux` |
| **Target Level**| Mid to Senior DevOps / SRE / Systems Engineers |
| **Questions** | 20 Deep Scenario & Architectural Questions |
| **Last Updated**| 2026 |

---

## Overview

Process management and supervision questions are standard in SRE and DevOps technical interviews. Candidates must articulate the kernel process state machine, signal delivery mechanics, process hierarchy propagation, and modern systemd service orchestration under fault conditions.

---

### Q1: Detail all Linux Process States (`R`, `S`, `D`, `Z`, `T`) and their operational significance.
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

| State Code | Name | Description & Scheduling Behavior |
| :--- | :--- | :--- |
| **`R`** | Running / Runnable | Task is actively executing on a CPU core or queued in the CFS scheduler runqueue ready to execute. |
| **`S`** | Interruptible Sleep | Task is waiting for an event (network socket, timer, terminal I/O). Awakens immediately upon receiving a signal (e.g., `SIGTERM`). |
| **`D`** | **Uninterruptible Sleep** | Task is waiting for a synchronous hardware/driver event (typically disk block I/O or NFS storage lock). **Ignores ALL signals**, including `SIGKILL`. Counts toward system Load Average! |
| **`Z`** | **Zombie** | Task has terminated (`exit()`) and released its memory/file handles, but its exit status has not yet been reaped by its parent via `waitpid()`. Consumes an entry in the process table. |
| **`T`** | Stopped / Traced | Task stopped by a signal (`SIGSTOP`, `Ctrl+Z` `SIGTSTP`) or being inspected by a debugger (`ptrace`). |
| **`I`** | Idle | Kernel thread sleeping without contributing to system load average. |
</details>

---

### Q2: Why cannot a process in state `D` (Uninterruptible Sleep) be killed by `kill -9`?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Mechanism:**
The state `D` (`TASK_UNINTERRUPTIBLE`) exists to protect operating system and filesystem consistency. When a thread initiates a synchronous hardware transaction (e.g., waiting for an NVMe controller DMA completion, or waiting for an RPC reply on an unresponsive NFS mount), the kernel puts the thread to sleep with signal delivery disabled. 

If the kernel permitted `SIGKILL` to terminate the process mid-operation, the driver's internal data structures, hardware DMA memory mappings, and filesystem state would be left in an undefined, corrupted state.

**Remediation:**
You cannot kill a `D` state process from user space. You must resolve the underlying blocked hardware/network storage condition (e.g., unmount hung NFS share via `umount -f -l`). If the hardware controller is permanently wedged, a host reboot is required.
</details>

---

### Q3: What is the exact difference between a Zombie Process and an Orphan Process?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Zombie Process:**
- **Status**: The process has finished executing (`exit_group()`). Its virtual memory, heap, and open file descriptors are completely freed by the kernel.
- **Problem**: Its parent process failed to call `wait()` or `waitpid()`. The process entry remains in the kernel process table (`task_struct`) to preserve its termination exit code.
- **Fix**: You cannot kill a zombie with `kill -9` (it is already dead!). You must send `SIGCHLD` to the parent, restart the parent process, or terminate the parent.

**Orphan Process:**
- **Status**: An active, running process whose parent process terminated before it did.
- **Reaping**: The Linux kernel automatically re-parents orphaned processes to **PID 1** (`systemd` or an init subreaper). PID 1 periodically executes `wait()` to reap them when they eventually exit, ensuring orphans do not become permanent zombies.
</details>

---

### Q4: Explain the Unix `fork()` and `execve()` execution model and Copy-on-Write (CoW).
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**The Two-Stage Process Spawning Model:**
1. **`fork()`**: Creates an exact duplicate child process. The child inherits identical copies of the parent's file descriptor table, environment variables, credentials, and virtual memory address layout.
2. **`execve()`**: Completely replaces the calling process's memory space, code segment (`.text`), stack, and heap with a brand-new binary executable loaded from disk, while preserving open file descriptors (unless marked `O_CLOEXEC`).

**Copy-on-Write (CoW) Optimization:**
If `fork()` duplicated physical memory (e.g., copying 16GB of RAM from a Java or Redis parent), process creation would be catastrophically slow and double memory usage. Instead, `fork()` copies only the **page table pointers**, marking physical pages as **read-only**.
- If neither process modifies memory, they share the physical RAM pages indefinitely.
- As soon as either parent or child attempts to write to a page, the CPU triggers a Minor Page Fault. The kernel allocates a new physical RAM page, copies the 4KB data, updates the page table, and marks it writable.
</details>

---

### Q5: What is the difference between `SIGTERM` (15) and `SIGKILL` (9)? Which signals cannot be caught or ignored?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Comparison:**
- **`SIGTERM` (Signal 15)**: The standard termination signal. It is delivered to the process's signal queue and can be intercepted by a signal handler. It gives the application opportunity to close database connections, flush memory buffers to disk, finish in-flight HTTP requests, and remove temporary PID files (graceful shutdown).
- **`SIGKILL` (Signal 9)**: The immediate kill signal. It is handled directly by the kernel's scheduler and process management subsystems. The process is never informed; execution halts instantly.

**Non-Catchable Signals:**
**`SIGKILL` (9)** and **`SIGSTOP` (19)** cannot be caught, blocked, or ignored by any user-space application under any circumstance.
</details>

---

### Q6: What does the `SIGHUP` (Signal 1) signal do?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Historical vs Modern Role:**
- **Historical (Hangup)**: Originated in serial terminal / modem connections. When the physical serial line disconnected, the terminal driver sent `SIGHUP` to the controlling process group, terminating all foreground shell jobs.
- **Modern Daemon Standard**: In modern background daemons (Nginx, Apache, Prometheus, PostgreSQL), `SIGHUP` is repurposed as the **Configuration Reload Signal**. Upon receiving `SIGHUP`, the master process re-reads its configuration files from disk, re-opens log files, and gracefully reloads worker processes without dropping active client connections.
</details>

---

### Q7: What are Linux Process Priorities: Nice values vs. Real-Time Scheduling Priorities?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Two Scheduling Domains:**
1. **Standard CFS (Completely Fair Scheduler)**:
   - Uses **Nice values** ranging from **`-20`** (highest priority, most CPU time) to **`+19`** (lowest priority, "nicest" to other processes). Default is `0`.
   - Only `root` can assign negative nice values.
   - Nice values determine the proportional CPU time weight assigned to a task.
2. **Real-Time Schedulers (`SCHED_FIFO` / `SCHED_RR`)**:
   - Priorities range from **`1` to `99`**.
   - A real-time task *always* preempts any standard CFS process. Used for audio synthesis, telecommunications, robotics, and high-frequency trading. Misconfigured real-time processes with infinite loops can completely freeze a multi-core machine.
</details>

---

### Q8: Explain Systemd Unit Types: Services, Targets, Slices, and Timers.
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

| Unit Type | Extension | Core Purpose & Functionality |
| :--- | :--- | :--- |
| **Service** | `.service` | Manages process execution, supervision, restart policies, and resource sandboxing. |
| **Target** | `.target` | Synchronization point and grouping mechanism representing system states (e.g., `multi-user.target`, `network-online.target`). Replaces legacy SysVinit runlevels. |
| **Slice** | `.slice` | Hierarchical Cgroup nodes managing resource limits (`system.slice`, `user.slice`) across multiple services. |
| **Timer** | `.timer` | Cron replacement providing calendar and monotonic event scheduling with systemd integration. |
| **Socket** | `.socket` | Socket activation unit. Systemd opens the listening socket and launches the `.service` on first connection. |
| **Mount** | `.mount` | Controls filesystem mount points; dynamically auto-generated from `/etc/fstab`. |
</details>

---

### Q9: What is the exact difference between `Requires=`, `Wants=`, `After=`, and `Before=` in systemd?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Dependency vs. Ordering:**
A common candidate mistake is confusing **dependency** (what starts) with **ordering** (when it starts).

- **Dependency Directives (What to start):**
  - **`Wants=foo.service`**: Soft dependency. When this unit starts, `foo.service` is also scheduled to start. If `foo.service` fails, this unit continues running normally.
  - **`Requires=foo.service`**: Strict dependency. If `foo.service` fails to start, this unit is aborted immediately. If `foo.service` stops, this unit is also stopped.
- **Ordering Directives (When to start):**
  - **`After=foo.service`**: If both units are scheduled to run, this unit delays execution until `foo.service` has fully started. **Does NOT imply dependency!**
  - **`Before=foo.service`**: This unit must finish starting before `foo.service` begins.

*Crucial rule: `Requires=` without `After=` causes both units to launch concurrently in parallel!*
</details>

---

### Q10: How does Systemd track child processes using Cgroups instead of legacy PID files?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**The Legacy Forking Flaw:**
In SysVinit, a daemon detached by calling `fork()` twice and writing its PID to `/var/run/app.pid`. If the daemon spawned worker subprocesses, or if an attacker manipulated the PID file, the init system lost track of the real processes, leaving orphaned zombie processes running after `service stop`.

**Systemd Cgroups Solution:**
Systemd places every service into its own unique Control Group path (e.g., `/sys/fs/cgroup/system.slice/nginx.service`).
- In Linux, child processes **inevitably inherit the cgroup membership of their parent across all forks and execs**.
- It is impossible for a process to escape its cgroup without root privileges.
- When `systemctl stop` is invoked, systemd reads the exact list of PIDs currently residing in that cgroup and issues `SIGTERM` (followed by `SIGKILL` after `TimeoutStopSec`) to *every single child process*, guaranteeing 100% clean teardowns.
</details>

---

### Q11: Explain the systemd service types: `simple`, `forking`, `oneshot`, and `notify`.
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

| Service Type | Mechanism & Readiness Criteria | Typical Usage |
| :--- | :--- | :--- |
| **`simple`** (Default) | Systemd considers the service started immediately after the `execve()` system call succeeds. Child process must NOT background itself. | Modern foreground daemons (Node.js, Go, Docker). |
| **`forking`** | Process calls `fork()` to background itself. Systemd considers it ready once the parent process exits cleanly. Requires `PIDFile=`. | Legacy traditional Unix daemons (older Nginx, Postfix). |
| **`oneshot`** | Process runs a task and terminates. Systemd blocks dependent services until the process exits with status 0. | Setup scripts, DB migrations, backup scripts. |
| **`notify`** | Identical to `simple`, but the process must explicitly send an IPC notification (`sd_notify("READY=1")`) over a UNIX socket before systemd marks it ready. | Robust production services; prevents race conditions where dependents connect before service is listening. |
</details>

---

### Q12: How does the Systemd Watchdog (`WatchdogSec=`) detect application deadlocks?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Mechanism:**
1. In the service unit, configure `WatchdogSec=30s`.
2. Systemd opens a UNIX domain datagram socket and passes its path in the environment variable `$NOTIFY_SOCKET`.
3. The application must periodically ping the socket at least once every 15 seconds by sending `WATCHDOG=1` (via `sd_notify` or writing to `$NOTIFY_SOCKET`).
4. If an internal thread lock, database deadlock, or infinite loop freezes the application event loop, the application fails to send the ping within the 30-second window.
5. Systemd detects the timeout, sends `SIGABRT` (to generate a diagnostic core dump), forces `SIGKILL`, and restarts the service automatically.
</details>

---

### Q13: What is `DynamicUser=yes` in systemd and how does it enhance security?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Mechanism:**
Historically, deploying a microservice securely required manually creating a static unprivileged user (`useradd -r ...`).

With `DynamicUser=yes`:
- Systemd dynamically allocates an ephemeral User ID and Group ID from the range `61184–65519` exclusively for the lifetime of the service.
- When the service stops, the UID is completely recycled.
- The service is executed with a read-only rootfs (`ProtectSystem=strict`), private `/tmp`, and no access to `/home`.
- Persistent storage is managed seamlessly via `StateDirectory=my-app`, which systemd automatically mounts and chowns to the dynamic UID.
</details>

---

### Q14: Explain the systemd security directives: `ProtectSystem=strict`, `PrivateTmp=yes`, and `NoNewPrivileges=yes`.
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

- **`ProtectSystem=strict`**: Mounts the entire filesystem hierarchy (`/usr`, `/boot`, `/etc`, `/lib`) as strictly read-only for the service. Only directories explicitly declared in `ReadWritePaths=` or `StateDirectory=` can be written to.
- **`PrivateTmp=yes`**: Creates an isolated private mount namespace for `/tmp` and `/var/tmp`. The service cannot see or tamper with temporary files created by host processes or other services.
- **`NoNewPrivileges=yes`**: Sets `PR_SET_NO_NEW_PRIVS`. Ensures that child processes spawned by the service cannot gain elevated privileges through `setuid` binaries (e.g., `sudo`, `passwd`) or file capabilities.
</details>

---

### Q15: What is D-Bus and what is its role in modern Linux systems?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Mechanism:**
**D-Bus (Desktop/Data Bus)** is an IPC (Inter-Process Communication) message bus system allowing applications to talk to one another and communicate with system daemons using structured RPC methods and signals.

**Architecture:**
- **System Bus (`systemd-logind`, `NetworkManager`, `systemd`)**: Runs as a single system-wide daemon (`dbus-daemon` or `dbus-broker`). Mediates privileged operations like changing IP addresses, shutting down the host, or querying systemd units via `systemctl`.
- **Session Bus**: Runs per logged-in user desktop session for GUI IPC.

When you execute `systemctl restart nginx`, `systemctl` connects to the System D-Bus socket (`/run/dbus/system_bus_socket`), invoking the `org.freedesktop.systemd1.Manager.RestartUnit` method on PID 1.
</details>

---

### Q16: How do you inspect and analyze `/proc/<pid>/status` during an incident?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Key Metrics in `/proc/<pid>/status`:**
- `State:` Process run state (`R`, `S`, `D`, `Z`).
- `VmPeak / VmSize:` Virtual memory footprint.
- `VmRSS:` Resident Set Size (physical RAM currently occupied by the process).
- `VmSwap:` Amount of process memory currently paged out to swap space.
- `Threads:` Number of active kernel threads running in this thread group.
- `nonvoluntary_ctxt_switches:` Number of times the CPU forced the task to yield because its time slice expired (high value indicates CPU starvation/contention).
- `voluntary_ctxt_switches:` Number of times the task voluntarily went to sleep waiting for I/O or a lock.
</details>

---

### Q17: What is the difference between a Process and a Thread in the Linux Kernel?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**The Kernel Reality:**
In the Linux kernel, there is strictly **no separate scheduler entity for a thread versus a process**. Both are represented by the identical C data structure: **`struct task_struct`**.

Both are created via the `clone()` system call:
- When creating a **Process**: `clone()` allocates a new virtual memory address space, new file descriptor table, and new signal tables.
- When creating a **Thread** (via `pthread_create()`): `clone()` is called with flags `CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND`. The new `task_struct` shares the exact same virtual memory page tables and file descriptor tables as the caller.

In Linux, threads are simply processes that share address space, grouped under a common Thread Group ID (`TGID`), which user space displays as the Process ID (PID).
</details>

---

### Q18: What is `systemd-journald` and how does it differ from traditional `rsyslog`?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Key Distinctions:**
- **`rsyslog`**: Traditional plaintext logging daemon. Reads RFC 3164/5424 text lines from `/dev/log` and writes unindexed text files to `/var/log/syslog` or `/var/log/messages`. Lacks metadata; parsing requires fragile regex matching.
- **`systemd-journald`**: Structured, binary logging service. Captures stdout/stderr directly from services, early boot messages from the kernel ring buffer, and audit logs.
  - Automatically attaches rich metadata to every record: `_PID`, `_UID`, `_SYSTEMD_UNIT`, `_COMM`, `_EXE`, `_HOSTNAME`.
  - Immune to arbitrary formatting injection.
  - Queries are indexed and instantaneous: `journalctl -u nginx --since "10 minutes ago" -p err`.
</details>

---

### Q19: How do you configure a persistent Systemd Monotonic Timer with Randomized Jitter?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**The Unit Configuration (`app-sync.timer`):**
```ini
[Unit]
Description=Trigger Application Data Sync

[Timer]
OnCalendar=*-*-* 04:00:00 UTC
Persistent=true
RandomizedDelaySec=600

[Install]
WantedBy=timers.target
```
**Why These Flags Matter in Production:**
- **`Persistent=true`**: If the server was powered off or rebooting at 04:00 UTC, systemd catches up and executes the job immediately upon boot, rather than skipping the run until the next day.
- **`RandomizedDelaySec=600` (Jitter)**: Adds up to 10 minutes of pseudo-random delay to the start time. Essential in cloud fleets to prevent the "Thundering Herd" problem where 5,000 servers all hammer a central database at exactly 04:00:00 UTC.
</details>

---

### Q20: What are Systemd Overrides and Drop-in Files, and why should you never edit `/lib/systemd/system/` files?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Unit File Precedence Hierarchy:**
1. `/etc/systemd/system/` (System Administrator local configurations - **Highest Precedence**).
2. `/run/systemd/system/` (Runtime ephemeral units).
3. `/usr/lib/systemd/system/` (Upstream distribution vendor files - **Lowest Precedence**).

**The Hazard:**
Editing files in `/usr/lib/systemd/system/` directly means your modifications will be **completely overwritten without warning during the next package upgrade** (`apt upgrade nginx`).

**The Solution (Drop-in Overrides):**
Create a drop-in override directory: `/etc/systemd/system/<service>.service.d/*.conf`.
```bash
sudo systemctl edit nginx # Automatically creates override.conf
```
Systemd parses the vendor unit first, then merges your drop-in overrides on top, preserving custom environment variables, restart limits, and sandboxing across all future package upgrades.
</details>

---
*Next Note: [05-networking-qa.md](file:///04-devops/linux/08-interview/05-networking-qa.md)*
