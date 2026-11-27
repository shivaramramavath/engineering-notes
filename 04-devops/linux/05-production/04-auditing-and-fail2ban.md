# Linux Auditing with Auditd, Fail2ban, and File Integrity Monitoring

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Audit Framework (`auditd`), `ausearch`, `aureport`, File Integrity Monitoring (AIDE), and `fail2ban` Intrusion Prevention |
| **Module** | `05-production` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `04-networking/05-firewall.md`, `05-production/01-server-hardening.md` |
| **Target Audience** | Security Engineers, SOC Analysts, SREs, Systems Administrators |
| **Target Distribution** | Ubuntu 24.04 LTS, Rocky Linux / RHEL 9.4 |
| **Key Tools / Files** | `auditd`, `auditctl`, `ausearch`, `aureport`, `fail2ban-client`, `aide`, `/etc/audit/rules.d/`, `/etc/fail2ban/` |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [Internal Working](#internal-working)
4. [Syntax & CLI Tools](#syntax--cli-tools)
5. [Progressive Practical Examples](#progressive-practical-examples)
6. [Best Practices](#best-practices)
7. [Common Mistakes & Gotchas](#common-mistakes--gotchas)
8. [Performance, Security & Cost Implications](#performance-security--cost-implications)
9. [Comparison Table / Alternatives](#comparison-table--alternatives)
10. [Real-World Troubleshooting & Interview Q&A](#real-world-troubleshooting--interview-qa)
11. [Hands-On Practice Exercises](#hands-on-practice-exercises)
12. [Summary Cheat Sheet & Further Reading](#summary-cheat-sheet--further-reading)

---

## What & Why

In production infrastructure, security is not just about keeping intruders out; it is about **knowing who touched what, when, and how**. When an incident occurs:
- Standard syslog messages can be manipulated or truncated by root attackers.
- High-frequency brute-force botnets can saturate SSH and web login endpoints.
- Critical system binaries (`/bin/login`, `/usr/bin/sudo`) or configuration files (`/etc/pam.d/`, `/etc/shadow`) can be covertly tampered with.

Linux addresses these challenges through a three-pronged defense and telemetry stack:
1. **The Linux Audit Subsystem (`auditd`)**: An in-kernel event logging framework that hooks system calls directly. Even if an attacker gains root and removes application logs, `auditd` preserves an immutable, forensic-grade trail of syscall executions, file modifications, and network connections.
2. **File Integrity Monitoring (AIDE)**: Creates a cryptographic baseline database of file attributes, hashes (SHA-256), and inodes, detecting unauthorized modifications or rootkit installations.
3. **Automated Intrusion Prevention (`fail2ban`)**: A log-parsing daemon that dynamically monitors log streams (SSH, Nginx, Postfix), detects brute-force authentication failures via regex filters, and automatically injects temporary drop rules into the kernel firewall (`nftables` / `iptables`).

---

## Core Concepts

### The Linux Audit Architecture
The audit subsystem operates inside the Linux kernel and communicates with userspace via a dedicated Netlink socket (`NETLINK_AUDIT`):
- **Kernel Audit Engine**: Intercepts system calls based on rules loaded into kernel memory.
- **`auditd` Daemon**: The userspace background service responsible for receiving audit records from the kernel and writing them to `/var/log/audit/audit.log`.
- **`auditctl`**: The command-line utility used to query, add, and delete kernel audit rules dynamically.
- **Rule Files (`/etc/audit/rules.d/*.rules`)**: Persistent declarative audit rule files compiled by `augenrules` into `/etc/audit/audit.rules`.

### Audit Rule Categories
1. **File System Watches (`-w`)**: Tracks access or modifications to specific files or directory trees.
   ```text
   -w /etc/shadow -p wa -k identity_changes
   ```
   `-p wa` monitors write (`w`) and attribute changes (`a`). `-k identity_changes` attaches a searchable keyword tag.
2. **System Call Auditing (`-a`)**: Hooks specific kernel syscalls (`execve`, `settimeofday`, `chmod`, `unlinkat`) filtered by user, architecture, or exit code.
   ```text
   -a always,exit -F arch=b64 -S execve -F auid>=1000 -F auid!=unset -k user_commands
   ```
3. **Control Rules**: Configures global audit behavior (e.g. setting buffer sizes, enabling immutable lock mode `-e 2`).

### What is `auid` (Audit UID)?
The **Audit User ID (`auid`)** is the original login identifier assigned by PAM when a user first establishes a session.
- If user `alice` (UID 1001) logs in via SSH and runs `sudo su -` to become `root` (UID 0), the active UID changes to `0`, **but the `auid` remains permanently set to `1001`**.
- Any command executed as root carries `auid=1001`, making it impossible for administrators to disguise their actions behind shared root accounts.

### Fail2ban Architecture: Jails, Filters, and Actions
`fail2ban` combines three modular primitives:
1. **Filter (`/etc/fail2ban/filter.d/`)**: Regular expressions (`failregex`) that parse log files for failed authentication events.
2. **Action (`/etc/fail2ban/action.d/`)**: Shell commands executed when a threshold is breached (e.g., adding an IP to an `nftables` drop set, sending an email alert).
3. **Jail (`/etc/fail2ban/jail.d/` or `jail.local`)**: Combines a Filter, an Action, target log paths, and thresholds (`maxretry = 5`, `findtime = 600`, `bantime = 1h`).

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                                  Userspace: Process Execution                           |
|                            Developer runs: chmod 777 /etc/shadow                        |
+----------------------------------------------------+------------------------------------+
                                                     |
                                            Syscall: chmod()
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                                    Linux Kernel Audit Hook                              |
|   1. Evaluates syscall rules & file watches in kernel memory                            |
|   2. Matches Rule: -w /etc/shadow -p wa -k identity_changes                             |
|   3. Captures: Syscall, UID=0, AUID=1001 (alice), PID, Executable, Terminal, Return     |
+----------------------------------------------------+------------------------------------+
                                                     |
                                            Netlink Socket (NETLINK_AUDIT)
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                                    auditd (Userspace Daemon)                            |
|   - Reads binary event buffer                                                           |
|   - Formats and writes record to: /var/log/audit/audit.log                              |
+----------------------------------------------------+------------------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                                     Fail2ban Monitoring Loop                            |
|   1. Tail /var/log/audit/audit.log or /var/log/auth.log via inotify                     |
|   2. Evaluates failregex matching failed authentication events                          |
|   3. Increments failure tally per IP within 'findtime' window                           |
|   4. Threshold exceeded (maxretry=5):                                                   |
|      -> Executes action: nft add element inet filter f2b-sshd { 203.0.113.88 }          |
+-----------------------------------------------------------------------------------------+
```

### The Fail2ban Nftables Action Flow
Modern `fail2ban` utilizes kernel `nftables` sets:
1. On startup, `fail2ban` creates an `inet` table with a set (e.g., `f2b-sshd`).
2. When an IP is banned, `fail2ban-client` executes:
   ```bash
   nft add element inet filter f2b-sshd { 203.0.113.88 }
   ```
3. Ingress packets from that IP are dropped in kernel space with $O(1)$ efficiency.
4. When `bantime` expires, `fail2ban` removes the element from the set automatically.

---

## Syntax & CLI Tools

### Managing Audit Rules with `auditctl`
```bash
# List all active kernel audit rules
sudo auditctl -l

# Check audit system status (enabled, backlog limit, lost events)
sudo auditctl -s

# Add a runtime file watch (-w) with permissions (w=write, a=attribute) and key (-k)
sudo auditctl -w /etc/passwd -p wa -k passwd_changes

# Delete a specific rule
sudo auditctl -W /etc/passwd -p wa -k passwd_changes

# Flush all active audit rules
sudo auditctl -D
```

### Forensic Querying with `ausearch` and `aureport`
```bash
# Search for events associated with a specific key tag (-k)
sudo ausearch -k identity_changes

# Search events by original login user ID (-ua / --uid)
sudo ausearch -ua 1001 -ts today

# Search for failed system calls only (unsuccessful access attempts)
sudo ausearch --success no -ts recent

# Generate executive summary report of authentication events
sudo aureport -au

# Generate summary report of failed commands and executables
sudo aureport -x --summary
```

### Managing Fail2ban with `fail2ban-client`
```bash
# Check status of fail2ban daemon and list active jails
sudo fail2ban-client status

# Check status and currently banned IPs for a specific jail
sudo fail2ban-client status sshd

# Manually ban an IP address
sudo fail2ban-client set sshd banip 203.0.113.88

# Manually unban an IP address
sudo fail2ban-client set sshd unbanip 203.0.113.88

# Reload configuration without dropping existing bans
sudo fail2ban-client reload
```

---

## Progressive Practical Examples

### Example 1: Building a Production-Grade `/etc/audit/rules.d/audit.rules`
Comprehensive CIS-compliant audit configuration capturing user identity modifications, privilege escalation, and network configuration changes.

```text
# /etc/audit/rules.d/audit.rules

# 1. Reset rules and set buffer capacity
-D
-b 8192
-f 1

# 2. Critical Identity & Authentication File Watches
-w /etc/passwd -p wa -k identity
-w /etc/shadow -p wa -k identity
-w /etc/group -p wa -k identity
-w /etc/gshadow -p wa -k identity
-w /etc/security/opasswd -p wa -k identity

# 3. Sudoers and Privilege Configuration
-w /etc/sudoers -p wa -k sudoers_changes
-w /etc/sudoers.d/ -p wa -k sudoers_changes

# 4. System Startup and Network Environment
-w /etc/issue -p wa -k system_banner
-w /etc/issue.net -p wa -k system_banner
-w /etc/hosts -p wa -k network_config
-w /etc/network/ -p wa -k network_config
-w /etc/netplan/ -p wa -k network_config

# 5. Track Execution of Privilege Escalation Commands (SUID / SGID)
-a always,exit -F arch=b64 -S execve -F euid=0 -F auid>=1000 -F auid!=unset -k root_escalated_commands

# 6. Track File Deletion Calls by Regular Users
-a always,exit -F arch=b64 -S unlink -S unlinkat -S rename -S renameat -F auid>=1000 -F auid!=unset -k file_deletions

# 7. Make the configuration immutable (must reboot to alter rules!)
-e 2
```

```bash
# Compile and load rules into running kernel:
sudo augenrules --load
sudo auditctl -l
```

### Example 2: Deconstructing an Audit Event Record
When `/etc/shadow` is modified, `auditd` generates a multi-line record:

```text
type=SYSCALL msg=audit(1764151200.123:452): arch=c000003e syscall=257 success=yes exit=3 
  a0=ffffff9c a1=7ffd12 a2=241 a3=1b6 items=1 ppid=1420 pid=2840 auid=1001 uid=0 gid=0 
  euid=0 suid=0 fsuid=0 egid=0 sgid=0 fsgid=0 tty=pts0 ses=3 comm="vim" exe="/usr/bin/vim.basic" 
  key="identity"
type=PATH msg=audit(1764151200.123:452): item=0 name="/etc/shadow" inode=135249 dev=08:01 
  mode=0100640 ouid=0 ogid=42 rdev=00:00 nametype=NORMAL
```
**Forensic Analysis**:
- `time=1764151200.123`: Exact Unix timestamp with millisecond precision.
- `syscall=257`: `openat()` system call.
- `auid=1001`: The action was executed by the human user who originally logged in with UID 1001 (`alice`).
- `uid=0`: The process had escalated to root.
- `exe="/usr/bin/vim.basic"`: The command used was Vim.
- `name="/etc/shadow"`: The file targeted.
- `key="identity"`: The rule tag that matched.

### Example 3: Configuring Fail2ban with Nftables Backend (`jail.local`)
Create a hardened, custom `fail2ban` configuration defending OpenSSH and Nginx.

```ini
# /etc/fail2ban/jail.local
[DEFAULT]
# Ban hosts for 1 hour after 5 failures in 10 minutes
bantime  = 1h
findtime = 10m
maxretry = 5

# Use modern nftables backend for packet drops
banaction = nftables-multiport
banaction_allports = nftables-allports

# Ignore trusted internal IPs and loopback
ignoreip = 127.0.0.1/8 ::1 192.168.1.0/24 10.0.0.0/8

[sshd]
enabled  = true
port     = ssh
logpath  = %(sshd_log)s
backend  = systemd
maxretry = 3
bantime  = 2h

[nginx-http-auth]
enabled  = true
port     = http,https
logpath  = /var/log/nginx/error.log

[nginx-botsearch]
enabled  = true
port     = http,https
logpath  = /var/log/nginx/access.log
maxretry = 2
bantime  = 24h
```

```bash
# Restart fail2ban and verify active jail:
sudo systemctl restart fail2ban
sudo fail2ban-client status sshd
```

### Example 4: Creating a Custom Fail2ban Filter for Application Scans
Block malicious web crawlers hunting for sensitive environment files (`.env`, `wp-login.php`, `config.json`).

```ini
# /etc/fail2ban/filter.d/nginx-malicious-probes.conf
[Definition]
failregex = ^<HOST> -.* "(GET|POST) /+(\.env|\.git|wp-login\.php|phpmyadmin|actuator) HTTP.*" (404|403)
ignoreregex =
```

```ini
# Append to /etc/fail2ban/jail.local:
[nginx-malicious-probes]
enabled  = true
port     = http,https
filter   = nginx-malicious-probes
logpath  = /var/log/nginx/access.log
maxretry = 1
bantime  = 48h
```

```bash
# Test regex against active access log:
sudo fail2ban-regex /var/log/nginx/access.log /etc/fail2ban/filter.d/nginx-malicious-probes.conf
```

### Example 5: Implementing File Integrity Monitoring with AIDE
Advanced Intrusion Detection Environment (AIDE) computes cryptographic checksums across critical binaries.

```bash
# 1. Install AIDE
sudo apt-get install -y aide

# 2. Initialize the baseline database
sudo aideinit

# 3. Move the baseline database to production location
sudo cp /var/lib/aide/aide.db.new /var/lib/aide/aide.db

# 4. Run an integrity check
sudo aide --check
# Returns: "AIDE found NO differences between database and filesystem. OK"

# 5. Simulate unauthorized modification:
echo "# compromised" | sudo tee -a /bin/login

# 6. Re-run check to observe alert:
sudo aide --check
# Output:
# File: /bin/login
#  Size     : 56128    , 56142
#  SHA256   : e3b0c44298fc1c... , 4bf5122f3445a...
#  CHANGED  : /bin/login
```

---

## Best Practices

1. **Make Audit Rules Immutable in Production (`-e 2`)**: Place `-e 2` as the very last line in `/etc/audit/rules.d/audit.rules`. In this mode, no user (not even `root`) can modify, delete, or disable audit rules without rebooting the server.
2. **Never Ignore the Audit Backlog Limit (`-b`)**: If a server experiences heavy disk I/O or a security incident, the kernel's audit event queue can overflow. Set `-b 8192` or higher, and define `-f 1` (rate limit or drop events) or `-f 2` (kernel panic if audit fails, required in high-assurance defense systems).
3. **Always Configure `ignoreip` in Fail2ban**: Always whitelist management subnets, VPN gateways, and localhost in `/etc/fail2ban/jail.local`. Failing to whitelist management IPs risks locking yourself out during automated configuration testing.
4. **Use Persistent `backend = systemd` for Fail2ban on Modern Distros**: Rather than having fail2ban poll text files like `/var/log/auth.log` with file polling, configure `backend = systemd` to stream events directly from the `systemd-journald` binary bus.
5. **Store AIDE Database on Read-Only or Remote Storage**: If an attacker gains root, they can simply run `aideinit` to overwrite the baseline database. Store a read-only copy of `aide.db` on an offline USB drive, write-once S3 bucket, or read-only NFS share.
6. **Forward Audit Logs to a Centralized SIEM in Real-Time**: Deploy `auditd-plugins` (`audispd`) to stream audit records over TLS to a centralized SIEM (Splunk, Elastic, OpenSearch). If a machine is completely destroyed or wiped by an attacker, the forensic trail is already securely archived off-site.

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Editing `/etc/audit/audit.rules` directly | Changes are wiped out on reboot; `augenrules` rebuilds it from files in `/etc/audit/rules.d/`. | Edit drop-ins in `/etc/audit/rules.d/*.rules` and run `augenrules --load`. |
| Setting `-e 2` during testing/debugging | Audit rules become completely locked; you cannot clear or edit rules without rebooting the node. | Leave `-e 2` commented out until configurations are fully tested and ready for production. |
| Over-auditing common syscalls without filters (e.g. `read`, `write`, `open`) | Generates millions of events per second, filling disks and saturating CPU cores. | Audit only sensitive syscalls (`execve`, `unlink`, `chmod`) filtered by `auid>=1000`. |
| Editing `/etc/fail2ban/jail.conf` directly | Package updates overwrite `jail.conf`, erasing all custom bans and configurations. | Place custom overrides in `/etc/fail2ban/jail.local` or `/etc/fail2ban/jail.d/`. |
| Running Fail2ban with iptables actions on an nftables-only system | Spawns conflicting legacy translation rules that fail to block traffic. | Set `banaction = nftables-multiport` in `jail.local`. |
| Failing to initialize AIDE after legitimate package upgrades | Legitimate system updates change hundreds of binary checksums; subsequent AIDE checks spam false positives. | Run `aideinit` and update `aide.db` as part of your post-patching workflow. |

---

## Performance, Security & Cost Implications

### Performance
- **Syscall Auditing Overhead**: Adding syscall rules on high-frequency calls (`read`, `write`, `stat`) on a database server can degrade performance by **20%–40%**. Restrict syscall auditing strictly to process execution (`execve`) and filesystem modifications (`unlinkat`, `chmod`), filtered by `auid>=1000`.
- **Fail2ban Regex Evaluation**: Processing 10,000 requests per second with inefficient regular expressions in Python can saturate a CPU core. For multi-gigabit traffic, offload volumetric rate-limiting to kernel `nftables` meters or cloud WAFs.

### Security
- **Forensic Non-Repudiation**: `auditd` provides legally binding non-repudiation. Even if a rogue employee changes passwords or clears `.bash_history`, the kernel's `auid` parameter indisputably links every executed command back to their original authenticated credentials.

### Cost
- Ingesting raw, unfiltered audit logs into cloud SIEM platforms (e.g., Splunk, AWS CloudWatch) costs significant storage and ingestion fees ($0.50 to $2.50 per GB). Using precise `key` tags in `audit.rules` allows selective ingestion of security events while discarding routine operational noise.

---

## Comparison Table / Alternatives

| Dimension | `auditd` | `fail2ban` | AIDE (File Integrity) | `syslog` / `journald` |
| :--- | :--- | :--- | :--- | :--- |
| **Primary Scope** | Kernel-level syscall & file auditing | Automated brute-force intrusion prevention | Cryptographic file modification detection | General application and system logging |
| **Enforcement Point** | Linux Kernel Netlink Socket | Userspace log parser -> Firewall | Periodic cryptographic scan | Userspace logging daemon |
| **Active Blocking** | Passive (Logging only) | **Active** (Drops IPs in firewall) | Passive (Alerting only) | Passive |
| **Tamper Resistance** | High (Kernel auid, `-e 2` immutable) | Moderate (Standard daemon) | High (if db stored off-site) | Low (Easily truncated by root) |
| **Compliance Value** | Mandatory for PCI-DSS, SOC 2, HIPAA | Recommended for exposed services | Mandatory for PCI-DSS 11.5 | General operational health |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: An administrator suspects an unauthorized user deleted a database file `/var/lib/data/prod.db`. How do you find out who did it using `ausearch`?
<details>
<summary>View Solution</summary>

Search for file deletion syscalls (`unlink`, `unlinkat`) matching that filename:
```bash
sudo ausearch -f /var/lib/data/prod.db -m SYSCALL -ts recent
```
**Key fields to inspect in the output**:
- `comm`: The command used (e.g., `comm="rm"`).
- `pid`: Process ID.
- `uid`: Current UID (likely `0` if run as root).
- **`auid`**: The **Audit UID** of the person who originally logged in (e.g., `auid=1002` -> check `/etc/passwd` to identify the employee username).
</details>

### Q2: What causes `auditd: audit_lost=[number] rate_limit=0` messages in kernel logs?
<details>
<summary>View Solution</summary>

**Cause**: The rate at which the kernel generated audit records exceeded the queue buffer capacity (`-b backlog`) or `auditd` was unable to write events to disk fast enough (e.g. disk I/O saturation). When the backlog buffer fills up, newly generated audit events are discarded.

**Fix**:
1. Increase the in-kernel backlog buffer size in `/etc/audit/rules.d/audit.rules`:
   ```text
   -b 16384
   ```
2. Tune `/etc/audit/auditd.conf`:
   ```text
   priority_boost = 4
   flush = INCREMENTAL_ASYNC
   freq = 50
   ```
3. Audit rules: Eliminate excessively noisy rules on high-volume syscalls.
</details>

### Q3: How do you unban an IP address that was accidentally locked out by Fail2ban?
<details>
<summary>View Solution</summary>

1. Check which jail banned the IP:
   ```bash
   sudo fail2ban-client status sshd
   ```
2. Unban the specific IP:
   ```bash
   sudo fail2ban-client set sshd unbanip 203.0.113.88
   ```
3. Alternatively, unban across all jails:
   ```bash
   sudo fail2ban-client unban 203.0.113.88
   ```
</details>

### Q4: Why is `AUID=4294967295` (`unset`) common in audit logs?
<details>
<summary>View Solution</summary>

`4294967295` is `(uint32_t)-1`, which signifies **"unset"**.
System services, cron jobs, background daemons, and processes spawned during system boot prior to user login have no human login session, so their `auid` is unset.
In audit rules, you typically filter out these events using `-F auid!=unset` or `-F auid>=1000` to focus exclusively on human user activity.
</details>

### Q5: How does Fail2ban avoid dropping persistent connections when banning an IP?
<details>
<summary>View Solution</summary>

Fail2ban injects firewall rules that drop **new incoming connection packets** (e.g. TCP `SYN` packets). Because firewall configurations permit established connections via `ct state established,related accept`, already-established sessions are not immediately severed unless the action explicitly runs `conntrack -D` or drops all packets regardless of state.
</details>

### Q6: What does the audit flag `-e 2` do and how can it be undone?
<details>
<summary>View Solution</summary>

`-e 2` locks the audit configuration into **immutable mode**.
- Once enabled, no further audit rules can be added, modified, or deleted.
- The `auditd` daemon cannot be stopped.
- **It CANNOT be undone at runtime by any user, including root.**
- The **only way** to alter rules is to edit the configuration files on disk and **reboot the operating system**.
</details>

### Q7: How do you test a Fail2ban regular expression against an existing log file without banning any IPs?
<details>
<summary>View Solution</summary>

Use the `fail2ban-regex` diagnostic utility:
```bash
fail2ban-regex /var/log/auth.log /etc/fail2ban/filter.d/sshd.conf
```
It parses the entire log file, prints matches, and displays summary statistics of how many addresses would be flagged without touching the firewall.
</details>

### Q8: What is the difference between `ausearch` and `aureport`?
<details>
<summary>View Solution</summary>

- **`ausearch`**: A raw search and query tool. It extracts individual, raw event records matching specific filters (by user, executable, file, or key).
- **`aureport`**: An analytical summary and reporting tool. It aggregates audit logs and outputs high-level tabular metrics (e.g., summary of failed logins, top executables run, user account modifications).
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Creating a File Watch and Auditing File Access
**Task**: Use `auditctl` to place a temporary file watch on `/tmp/secret.txt`. Read the file and locate the corresponding event in `ausearch`.
<details>
<summary>View Solution</summary>

```bash
# 1. Create target file
echo "Classified" > /tmp/secret.txt

# 2. Add audit watch for read and write with tag 'lab_watch'
sudo auditctl -w /tmp/secret.txt -p rwa -k lab_watch

# 3. Read the file
cat /tmp/secret.txt

# 4. Search audit log for the event
sudo ausearch -k lab_watch -ts recent

# 5. Clean up
sudo auditctl -W /tmp/secret.txt -p rwa -k lab_watch
rm -f /tmp/secret.txt
```
</details>

### Exercise 2: Generating Authentication Summaries with `aureport`
**Task**: Generate an authentication report showing successful and failed login attempts on your machine.
<details>
<summary>View Solution</summary>

```bash
# Display summary of all authentication attempts
sudo aureport -au --summary

# Display detailed list of failed logins
sudo aureport -l --failed
```
</details>

### Exercise 3: Inspecting Fail2ban Jail Status
**Task**: Check the active status of Fail2ban. Inspect which jails are currently running and how many IPs are banned.
<details>
<summary>View Solution</summary>

```bash
if command -v fail2ban-client &>/dev/null; then
    sudo fail2ban-client status
    # If sshd jail is active:
    sudo fail2ban-client status sshd 2>/dev/null || true
else
    echo "Fail2ban not installed; skipping test."
fi
```
</details>

### Exercise 4: Validating Audit Rule Syntax with `augenrules`
**Task**: Create a test rule file `/etc/audit/rules.d/test.rules`. Validate its compilation using `augenrules --check`.
<details>
<summary>View Solution</summary>

```bash
# 1. Add mock rule
echo "-w /etc/hostname -p wa -k hostname_change" | sudo tee /etc/audit/rules.d/99-test.rules

# 2. Check rules syntax
if command -v augenrules &>/dev/null; then
    sudo augenrules --check
fi

# 3. Clean up
sudo rm -f /etc/audit/rules.d/99-test.rules
```
</details>

### Exercise 5: Testing Fail2ban Filter Regex with `fail2ban-regex`
**Task**: Test the default `sshd` filter against `/var/log/auth.log` or systemd journal.
<details>
<summary>View Solution</summary>

```bash
if command -v fail2ban-regex &>/dev/null; then
    sudo fail2ban-regex /var/log/auth.log /etc/fail2ban/filter.d/sshd.conf 2>/dev/null || \
    sudo fail2ban-regex systemd-journal /etc/fail2ban/filter.d/sshd.conf
fi
```
</details>

### Exercise 6: Simulating Temporary Fail2ban IP Ban and Unban
**Task**: Use `fail2ban-client` to manually ban `192.0.2.1` in the `sshd` jail. Verify it is banned, then unban it.
<details>
<summary>View Solution</summary>

```bash
if systemctl is-active fail2ban &>/dev/null; then
    # Ban IP
    sudo fail2ban-client set sshd banip 192.0.2.1
    sudo fail2ban-client status sshd | grep "192.0.2.1"

    # Unban IP
    sudo fail2ban-client set sshd unbanip 192.0.2.1
    echo "IP unbanned cleanly."
fi
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Operation | Command Example |
| :--- | :--- |
| **List Active Audit Rules** | `sudo auditctl -l` |
| **Audit Status & Capacity** | `sudo auditctl -s` |
| **Search by Key Tag** | `sudo ausearch -k <tag_name> -ts recent` |
| **Search by Original User** | `sudo ausearch -ua <uid>` |
| **Authentication Summary** | `sudo aureport -au` |
| **Compile Rule Drop-Ins** | `sudo augenrules --load` |
| **Fail2ban Global Status** | `sudo fail2ban-client status` |
| **Fail2ban Jail Status** | `sudo fail2ban-client status sshd` |
| **Unban IP Address** | `sudo fail2ban-client set <jail> unbanip <ip>` |
| **AIDE Integrity Check** | `sudo aide --check` |

### Further Reading
- `man 8 auditd` and `man 8 auditctl`
- `man 8 ausearch` and `man 8 aureport`
- `man 1 fail2ban-client` and `man 5 jail.conf`
- `man 1 aide` and `man 5 aide.conf`
- [Linux Audit Subsystem Documentation - Red Hat](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/9/html/security_hardening/auditing-the-system)
- [Fail2ban Official Manual & Wiki](https://github.com/fail2ban/fail2ban/wiki)
