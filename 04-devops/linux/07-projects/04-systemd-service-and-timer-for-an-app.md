# Project 04: Production Systemd Service & Timer Architecture

| Metadata | Details |
| :--- | :--- |
| **Project** | Production Systemd Service & Timer Architecture with Security Sandboxing |
| **Module** | `07-projects` — Production Reference Projects & Labs |
| **Track** | `04-devops/linux` |
| **Target OS** | Ubuntu 24.04 LTS (systemd v255+), RHEL 9.4+ (systemd v252+) |
| **Complexity** | Intermediate |
| **Estimated Duration**| 45–60 minutes |

---

## 1. Architecture & Design

### Scenario Context
Running application processes directly via shell scripts, `screen`/`tmux` sessions, or rudimentary init scripts leaves production services vulnerable to silent crashes, unbounded resource leaks, lack of process supervision, and full root compromise upon exploit.

Systemd provides unified service lifecycle management, fine-grained Linux cgroup resource limits, automated restarts, and robust kernel-level security sandboxing. This project constructs an enterprise-grade production service deployment for a backend microservice, complete with:
1. **Hardened systemd Service Unit**: Least-privilege execution (`DynamicUser=yes` or isolated user), kernel namespace isolation (`ProtectSystem=strict`, `PrivateTmp=yes`, `NoNewPrivileges=yes`), and memory/CPU limits.
2. **Watchdog Heartbeat Integration**: Automated recovery if the application event loop deadlocks (`WatchdogSec=30s`).
3. **Paired Monotonic Cleanup Timer Unit**: Periodic maintenance job (`OnCalendar`) with randomized jitter (`RandomizedDelaySec=300`) and persistent missed-event catching (`Persistent=true`).

### Systemd Process Isolation Architecture
```
+-------------------------------------------------------------------------+
|                  Hardened Systemd Service Sandbox                       |
|                                                                         |
|  +-------------------------------------------------------------------+  |
|  | systemd Service Supervisor (PID 1)                                |  |
|  | - Restarts on failure (Restart=always, RestartSec=5s)             |  |
|  | - Hardware Watchdog Ping monitor (WatchdogSec=30s)                |  |
|  +-------------------------------------------------------------------+  |
|                                  | Spawns Sandboxed Child Process       |
|                                  v                                      |
|  +-------------------------------------------------------------------+  |
|  | APPLICATION SANDBOX (Node.js / Python / Go Daemon)                |  |
|  |                                                                   |  |
|  | 1. Filesystem Containment:                                        |  |
|  |    - ProtectSystem=strict (Entire /usr, /boot, /etc read-only)    |  |
|  |    - ProtectHome=true (Completely hides /home, /root, /run/user)  |  |
|  |    - PrivateTmp=true (Isolated private /tmp and /var/tmp)         |  |
|  |    - ReadWritePaths=/var/lib/api-app /var/log/api-app             |  |
|  |                                                                   |  |
|  | 2. Kernel & Privilege Lockdown:                                  |  |
|  |    - DynamicUser=yes (Transient ephemeral UID allocation)         |  |
|  |    - NoNewPrivileges=true (Blocks SUID escalation)                |  |
|  |    - ProtectKernelTunables=true / ProtectControlGroups=true       |  |
|  |    - RestrictAddressFamilies=AF_INET AF_INET6 AF_UNIX             |  |
|  |                                                                   |  |
|  | 3. Cgroups v2 Resource Throttling:                                |  |
|  |    - MemoryMax=512M | CPUQuota=80% | TasksMax=100                 |  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

---

## 2. Implementation Artifacts

### Artifact 1: Production Application Mock Binary (`/opt/api-app/server.py`)

A mock Python microservice supporting systemd watchdog notification pings via `systemd-notify`.

```bash
#!/usr/bin/env bash
# Setup application working environment
sudo mkdir -p /opt/api-app /var/lib/api-app /var/log/api-app
sudo chmod 755 /opt/api-app

cat << 'EOF' | sudo tee /opt/api-app/server.py > /dev/null
#!/usr/bin/env python3
import time
import os
import sys
import subprocess

print("Initializing Production Backend API Server...")

# Verify filesystem isolation
writable_dir = "/var/lib/api-app"
test_file = os.path.join(writable_dir, "heartbeat.txt")

try:
    with open(test_file, "a") as f:
        f.write(f"Booted at {time.time()}\n")
    print(f"Verified write access to {writable_dir}")
except Exception as e:
    print(f"Failed writing to {writable_dir}: {e}", file=sys.stderr)

# Inform systemd that initialization is complete
def notify(state):
    try:
        subprocess.run(["systemd-notify", state], check=False)
    except FileNotFoundError:
        pass

notify("--ready")
print("Notified systemd: READY=1. Entering heartbeat loop...")

counter = 0
while True:
    time.sleep(5)
    counter += 1
    # Ping systemd watchdog
    notify("WATCHDOG=1")
    if counter % 6 == 0:
        print(f"Application active: {counter * 5}s uptime. Watchdog ping sent.")
EOF

sudo chmod 755 /opt/api-app/server.py
```

### Artifact 2: Hardened Systemd Service Unit (`api-app.service`)

`/etc/systemd/system/api-app.service`:
```ini
[Unit]
Description=Hardened Production Backend API Service
Documentation=file:///opt/api-app/server.py
After=network-online.target
Wants=network-online.target

[Service]
Type=notify
ExecStart=/usr/bin/python3 /opt/api-app/server.py
Restart=always
RestartSec=5s
TimeoutStartSec=30s
WatchdogSec=20s

# --- Identity & Execution Environment ---
DynamicUser=yes
StateDirectory=api-app
LogsDirectory=api-app
WorkingDirectory=/opt/api-app

# --- Filesystem Sandboxing ---
ProtectSystem=strict
ProtectHome=true
PrivateTmp=true
PrivateDevices=true
ProtectKernelTunables=true
ProtectKernelModules=true
ProtectControlGroups=true
ProtectKernelLogs=true
ProtectProc=invisible
ProcSubset=pid

# --- Network & Syscall Sandboxing ---
RestrictAddressFamilies=AF_INET AF_INET6 AF_UNIX
RestrictNamespaces=true
LockPersonality=true
MemoryDenyWriteExecute=true
NoNewPrivileges=true
CapabilityBoundingSet=

# --- Cgroups v2 Resource Governance ---
CPUWeight=100
CPUQuota=80%
MemoryMax=512M
MemoryHigh=400M
TasksMax=100

[Install]
WantedBy=multi-user.target
```

### Artifact 3: Maintenance Cleanup Service & Timer Unit

`/etc/systemd/system/api-app-cleanup.service`:
```ini
[Unit]
Description=Clean Expired API Heartbeat Data
After=local-fs.target

[Service]
Type=oneshot
User=root
ExecStart=/usr/bin/find /var/lib/private/api-app -type f -name "heartbeat.txt" -mtime +7 -delete
StandardOutput=journal
StandardError=journal
```

`/etc/systemd/system/api-app-cleanup.timer`:
```ini
[Unit]
Description=Trigger API Data Cleanup Weekly on Sunday at 03:00 UTC

[Timer]
OnCalendar=Sun *-*-* 03:00:00 UTC
Persistent=true
RandomizedDelaySec=600

[Install]
WantedBy=timers.target
```

---

## 3. Execution & Verification

### Step-by-Step Test Procedure

1. **Deploy and Reload Systemd**:
   ```bash
   sudo systemctl daemon-reload
   sudo systemctl enable --now api-app.service
   sudo systemctl enable --now api-app-cleanup.timer
   ```

2. **Verify Active Service Status**:
   ```bash
   systemctl status api-app.service
   ```

3. **Simulated Verification Output**:
   ```text
   ● api-app.service - Hardened Production Backend API Service
        Loaded: loaded (/etc/systemd/system/api-app.service; enabled; preset: enabled)
        Active: active (running) since Wed 2026-11-26 10:15:02 UTC; 25s ago
      Main PID: 45120 (python3)
        Status: "READY=1"
         Tasks: 1 (limit: 100)
        Memory: 18.2M (peak: 19.5M, max: 512.0M)
           CPU: 120ms
        CGroup: /system.slice/api-app.service
                └─45120 /usr/bin/python3 /opt/api-app/server.py

   Nov 26 10:15:02 node-01 python3[45120]: Initializing Production Backend API Server...
   Nov 26 10:15:02 node-01 python3[45120]: Verified write access to /var/lib/api-app
   Nov 26 10:15:02 node-01 systemd[1]: Started api-app.service - Hardened Production Backend API Service.
   Nov 26 10:15:02 node-01 python3[45120]: Notified systemd: READY=1. Entering heartbeat loop...
   Nov 26 10:15:32 node-01 python3[45120]: Application active: 30s uptime. Watchdog ping sent.
   ```

4. **Verify Systemd Security Assessment**:
   Run systemd's built-in security scoring analyzer:
   ```bash
   systemd-analyze security api-app.service
   # Expected Output:
   # -> Overall exposure level 1.1 OK :-) (Extremely Secure)
   ```

5. **Verify Timer List**:
   ```bash
   systemctl list-timers api-app-cleanup.timer
   # NEXT                        LEFT          LAST PASSED UNIT                   ACTIVATES
   # Sun 2026-11-30 03:08:14 UTC 3 days left   n/a  n/a    api-app-cleanup.timer  api-app-cleanup.service
   ```

---

## 4. Failure Modes & Edge Cases

| Failure Mode / Incident | Root Cause | Remediation Runbook |
| :--- | :--- | :--- |
| **Service Terminated by Watchdog (`Watchdog timeout (20s)`)** | Application thread hung on locked database query or CPU freeze; failed to send `WATCHDOG=1`. | Inspect journal logs (`journalctl -u api-app -e`). Systemd automatically terminates and restarts the hung process. Fix application deadlock. |
| **`Read-only file system` Error on Startup** | Service attempted to write to `/etc`, `/usr`, or unapproved directory while `ProtectSystem=strict` is active. | Explicitly grant write access using `ReadWritePaths=/path/to/dir` or leverage systemd-managed `StateDirectory=`. |
| **`Permission denied` with DynamicUser** | DynamicUser allocates a transient UID (e.g., 62341) that does not own existing static files. | Never use static `chown` for DynamicUser. Always use `StateDirectory=appname` which systemd automatically mounts and chowns. |
| **Service Enters Restart Crash Loop (`start-limit-hit`)** | Service crashes continuously faster than `RestartSec`. | Inspect `/etc/systemd/system/api-app.service` and add `StartLimitIntervalSec=60s` and `StartLimitBurst=5` to prevent journal flooding. |

---

## 5. Production Considerations

- **Journald Log Rate Limiting**: Ensure high-traffic services do not exhaust system logging. Configure `LogRateLimitIntervalSec=30s` and `LogRateLimitBurst=1000` inside the service unit.
- **Graceful Shutdown Signals**: Configure `TimeoutStopSec=30s` and `KillSignal=SIGTERM` to allow background workers to finish in-flight requests before systemd issues `SIGKILL`.
- **Systemd Drop-Ins for Overrides**: Never edit `/usr/lib/systemd/system/` upstream vendor files directly. Always use drop-in overrides located in `/etc/systemd/system/<service>.service.d/*.conf` via `systemctl edit <service>`.

---
*Next Project: [05-build-a-container-with-unshare.md](file:///04-devops/linux/07-projects/05-build-a-container-with-unshare.md)*
