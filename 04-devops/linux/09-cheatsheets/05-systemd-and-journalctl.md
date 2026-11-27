# Systemd & Journalctl: High-Density Operations Cheatsheet

| Metadata | Details |
| :--- | :--- |
| **Topic** | Systemd Service Management, Unit Directives & Journalctl Log Analysis |
| **Module** | `09-cheatsheets` — High-Density Linux Production Cheatsheets |
| **Track** | `04-devops/linux` |
| **Target OS** | Systemd v250+ (Ubuntu 24.04, RHEL 9.4) |
| **Last Updated**| 2026 |

---

## 1. Systemctl Service Lifecycle Reference

```bash
# === Service Operations ===
sudo systemctl start nginx                  # Start service immediately
sudo systemctl stop nginx                   # Stop service gracefully
sudo systemctl restart nginx                # Full stop and start
sudo systemctl reload nginx                 # Hot reload configuration (SIGHUP)
sudo systemctl reload-or-restart nginx      # Reload if supported, else restart
systemctl status nginx                      # View status, PID, memory, recent logs

# === Boot Enablement ===
sudo systemctl enable nginx                 # Enable service on boot (creates symlink)
sudo systemctl disable nginx                # Disable service on boot (removes symlink)
sudo systemctl enable --now nginx           # Enable AND start immediately!
systemctl is-active nginx                   # Returns 'active' or 'inactive' (exit code 0/3)
systemctl is-enabled nginx                  # Returns 'enabled' or 'disabled'
systemctl is-failed nginx                   # Returns 'failed' if in error state

# === Service Masking (Preventing Accidental Launches) ===
# Symlinks unit to /dev/null; cannot be started even as a dependency!
sudo systemctl mask nginx                   # Mask service
sudo systemctl unmask nginx                 # Unmask service

# === Daemon Reload (MANDATORY after modifying unit files) ===
sudo systemctl daemon-reload                # Reload systemd manager configuration
```

---

## 2. Unit Inspection & Modification

```bash
# === Inspect Units ===
systemctl cat nginx.service                 # Print active unit file + all drop-in overrides
systemctl show nginx.service                # Dump all low-level internal properties
systemctl show -p MainPID,ActiveState nginx # Query specific properties
systemctl list-dependencies nginx           # View unit dependency tree
systemctl list-units --type=service         # List all currently active services
systemctl list-units --failed               # List all failed units on the host
systemctl list-unit-files                   # List all installed unit files and enablement

# === Modifying & Overriding Units ===
# ALWAYS use 'edit' to create safe drop-in files in /etc/systemd/system/*.d/
sudo systemctl edit nginx                   # Opens editor for drop-in override (override.conf)
sudo systemctl edit --full nginx            # Copy vendor unit to /etc and edit completely
sudo systemctl revert nginx                 # Delete all custom overrides and restore vendor unit
```

---

## 3. Systemd Timers Quick Reference

```bash
# === Timer Operations ===
systemctl list-timers                       # List all active timers, next run, and elapsed
systemctl list-timers --all                 # Include inactive/disabled timers
sudo systemctl enable --now my-task.timer   # Enable and start a timer unit
systemctl status my-task.timer              # View timer state

# === OnCalendar Syntax Examples ===
# Format: DayOfWeek Year-Month-Day Hour:Minute:Second
OnCalendar=*-*-* 02:00:00                   # Daily at 02:00:00
OnCalendar=Mon,Fri *-*-* 04:30:00           # Mondays and Fridays at 04:30
OnCalendar=*-*-01 00:00:00                  # Monthly on the 1st
OnCalendar=hourly                           # Once every hour (00 minutes)
OnCalendar=weekly                           # Weekly on Monday 00:00:00

# === Monotonic Timers (Relative Intervals) ===
OnBootSec=15min                             # 15 minutes after kernel boot
OnUnitActiveSec=1h                          # 1 hour after the service last executed
RandomizedDelaySec=300                      # Add up to 5 minutes of random jitter
Persistent=true                             # Run missed jobs immediately after reboot
```

---

## 4. Systemd Boot & Security Analysis

```bash
# === Boot Performance Profiling ===
systemd-analyze                             # Total boot time (kernel + initrd + userspace)
systemd-analyze blame                       # List services sorted by startup duration
systemd-analyze critical-chain              # Print critical chain tree slowing down boot
systemd-analyze plot > /tmp/boot.svg        # Generate graphic SVG waterfall timeline of boot

# === Security Sandboxing Audit ===
systemd-analyze security                    # Audit security exposure scores across all units
systemd-analyze security nginx.service      # Deep dive into sandboxing flags for a unit
```

---

## 5. Journalctl: High-Performance Log Queries

```bash
# === Basic Navigation ===
journalctl                                  # Paged display of all system logs
journalctl -f                               # Follow live log stream (like tail -f)
journalctl -n 50                            # Show last 50 lines
journalctl -e                               # Jump directly to the end of the pager
journalctl -q                               # Quiet mode (suppresses informational headers)

# === Service & Process Filtering ===
journalctl -u nginx.service                 # Logs for specific systemd unit
journalctl -u nginx -u php-fpm              # Combine logs from multiple units
journalctl _PID=45120                       # Logs generated by specific process ID
journalctl /usr/sbin/nginx                  # Logs generated by specific binary path
journalctl _COMM=sshd                       # Logs generated by executable name

# === Boot Filtering ===
journalctl -b                               # Logs from current boot only
journalctl -b -1                            # Logs from PREVIOUS boot
journalctl --list-boots                     # Enumerate all recorded boot cycles

# === Time Filtering (RFC3339 or Human Expressions) ===
journalctl --since "1 hour ago"
journalctl --since "yesterday" --until "2 hours ago"
journalctl --since "2026-11-26 10:00:00" --until "2026-11-26 10:30:00"

# === Severity / Priority Filtering ===
# Priorities: 0:emerg, 1:alert, 2:crit, 3:err, 4:warning, 5:notice, 6:info, 7:debug
journalctl -p err..emerg                    # Errors, critical, alerts, and emergencies only
journalctl -u nginx -p err -b               # Current boot errors for nginx

# === Output Formatting ===
journalctl -u nginx -o json-pretty          # Output structured JSON format
journalctl -u nginx -o cat                  # Clean raw output (no timestamps or host prefixes)
journalctl -k                               # Kernel ring buffer logs only (like dmesg)
```

---

## 6. Journal Storage Maintenance & Vacuuming

```bash
# === Check Storage Usage ===
journalctl --disk-usage                     # Displays total disk space consumed by logs

# === Verify Integrity ===
sudo journalctl --verify                    # Cryptographically verify journal files

# === Purge / Vacuum Expired Logs ===
sudo journalctl --vacuum-size=1G            # Prune logs until size is under 1 Gigabyte
sudo journalctl --vacuum-time=14d           # Prune logs older than 14 days
sudo journalctl --vacuum-files=5            # Retain only 5 most recent journal archives

# === Persistent Journal Configuration (/etc/systemd/journald.conf) ===
[Journal]
Storage=persistent                          # Store in /var/log/journal/ (persists reboot)
Compress=yes                                # Compress rotated log chunks
SystemMaxUse=2G                             # Maximum disk space allocated to journal
SystemKeepFree=5G                           # Ensure 5GB disk space remains free on root
MaxRetentionSec=1month                      # Maximum age of log entries
```

---

## 7. Essential Service Unit File Template

```ini
# /etc/systemd/system/myapp.service
[Unit]
Description=Production Application Service
Documentation=https://docs.example.com
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=apprunner
Group=apprunner
WorkingDirectory=/opt/myapp
ExecStart=/usr/bin/node /opt/myapp/server.js
ExecReload=/bin/kill -HUP $MAINPID
Restart=always
RestartSec=5s
TimeoutStopSec=30s

# Environment Variables
Environment=NODE_ENV=production
EnvironmentFile=/etc/default/myapp

# Security Sandboxing
ProtectSystem=strict
ProtectHome=true
PrivateTmp=true
PrivateDevices=true
ProtectKernelTunables=true
ProtectControlGroups=true
NoNewPrivileges=true
CapabilityBoundingSet=CAP_NET_BIND_SERVICE
AmbientCapabilities=CAP_NET_BIND_SERVICE
ReadWritePaths=/var/log/myapp /var/lib/myapp

# Resource Limits (Cgroups v2)
MemoryMax=1G
CPUQuota=150%
TasksMax=200

[Install]
WantedBy=multi-user.target
```

---
*Next Cheatsheet: [06-networking-commands.md](file:///04-devops/linux/09-cheatsheets/06-networking-commands.md)*
