# Enterprise Linux Patching, Unattended Upgrades, and Live Kernel Patching

| Metadata | Details |
| :--- | :--- |
| **Topic** | Automated Security Patching, `unattended-upgrades`, `dnf-automatic`, Livepatch (`kpatch`), Zero-Downtime Maintenance |
| **Module** | `05-production` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `03-system/03-package-management.md`, `03-system/06-cron-and-timers.md`, `05-production/01-server-hardening.md` |
| **Target Audience** | SREs, Systems Administrators, DevSecOps Engineers, Cloud Infrastructure Leads |
| **Target Distribution** | Ubuntu 24.04 LTS (`unattended-upgrades`), Rocky Linux / RHEL 9.4 (`dnf-automatic`, `kpatch`) |
| **Key Tools / Services** | `unattended-upgrades`, `dnf-automatic`, `kpatch`, `needrestart`, `checkrestart`, `/etc/apt/apt.conf.d/` |

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

Unpatched vulnerabilities (Common Vulnerabilities and Exposures - CVEs) are the leading entry point for enterprise intrusions, ransomware deployments, and data breaches. 
- High-severity Linux kernel vulnerabilities (e.g. Dirty COW, Dirty Pipe, PwnKit) and OpenSSL flaws transition from initial disclosure to automated public botnet exploitation within **24 to 72 hours**.
- Waiting for monthly manual patching maintenance windows leaves production fleets exposed to critical zero-day exploits for weeks.

However, uncoordinated automated patching carries risks:
- An unexpected package upgrade can restart a database during peak traffic.
- A patched shared library (`glibc`, `openssl`) leaves running services vulnerable until their background processes are restarted (`needrestart`).
- Kernel upgrades require system reboots to take effect, triggering application downtime.

Modern production operations solve this through a multi-tiered patching architecture:
1. **Automated Security Patching**: Confined strictly to security streams (`unattended-upgrades` on Ubuntu, `dnf-automatic` on RHEL).
2. **Live Kernel Patching (`kpatch` / Canonical Livepatch)**: Injects binary security fixes directly into running kernel memory without rebooting.
3. **Zero-Downtime Rolling Orchestration**: Draining traffic from nodes via load balancers or Kubernetes before rebooting sequentially.

---

## Core Concepts

### Package Update Streams: Security vs Feature Updates
Enterprise Linux distributions cleanly decouple package updates into distinct repositories:
- **Security Updates (Critical / Important)**: Backported security patches that fix CVEs **without changing library ABIs or feature versions**. Updating `openssl` from `3.0.2-0ubuntu1.12` to `1.13` patches the vulnerability while guaranteeing 100% backward compatibility.
- **Feature / Functional Updates**: Major or minor version bumps (e.g. upgrading Python 3.10 to 3.11). Must **never** be applied automatically without comprehensive staging testing.

### Automated Patching Engines
- **Ubuntu / Debian**: **`unattended-upgrades`**. Controlled via `/etc/apt/apt.conf.d/50unattended-upgrades` and triggered by `apt-daily.timer` and `apt-daily-upgrade.timer`.
- **RHEL / Rocky Linux**: **`dnf-automatic`**. Controlled via `/etc/dnf/automatic.conf` and managed by `dnf-automatic.timer`. Supports downloading patches only, generating email alerts, or applying them automatically.

### Live Kernel Patching (Kpatch / Livepatch)
Historically, applying a patched Linux kernel required a full server reboot. **Live Kernel Patching** allows patching critical kernel CVEs with zero downtime:
- It relies on kernel **`ftrace` (Function Tracer)**.
- When a patch is applied, `ftrace` redirects the execution entry point of a vulnerable kernel function to a newly compiled, safe replacement function in kernel memory.
- Running processes, network sockets, and open file descriptors continue executing seamlessly.

### Restart Identification: `needrestart` vs `checkrestart`
When a security patch updates a shared dynamic library (e.g., `libssl.so` or `libc.so`), the file is updated on disk, **but running processes continue executing the old, vulnerable copy cached in RAM!**
- Utilities like `needrestart` inspect `/proc/<pid>/maps` across all running processes.
- They identify processes holding deleted shared libraries (`libssl.so (deleted)`) and automatically restart the corresponding systemd services.

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                                    Systemd Timer: apt-daily-upgrade.timer               |
|                                       Fires daily at 06:00 UTC                          |
+----------------------------------------------------+------------------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                                   unattended-upgrades Engine                            |
|   1. Acquires APT lock (/var/lib/dpkg/lock-frontend)                                    |
|   2. Updates package index from security origin: "${distro_id}:${distro_codename}-security"|
|   3. Filters packages: Installs ONLY packages fixing recorded CVEs                      |
|   4. Evaluates Blacklist: Skips pinned packages (e.g. postgresql*, docker*)             |
+----------------------------------------------------+------------------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                                   Package Installation & DPKG                           |
|   - Installs deb/rpm packages atomically                                                |
|   - Writes transaction log to: /var/log/unattended-upgrades/unattended-upgrades.log     |
+----------------------------------------------------+------------------------------------+
                                                     |
                      +------------------------------+------------------------------+
                      | User Space Library Patched?                                 | Kernel Patched?
                      v                                                             v
+-------------------------------------------+   +-----------------------------------------+
|           needrestart Evaluation          |   |          Live Kernel Patching           |
|   - Scans /proc/*/maps for deleted .so    |   |                                         |
|   - Restarts affected systemd services    |   |   +---------------------------------+   |
|     (e.g., systemctl restart nginx)       |   |   | Canonical Livepatch / kpatch    |   |
+-------------------------------------------+   |   | Redirects ftrace hook in RAM    |   |
                                                |   | (Zero reboot needed!)           |   |
                                                |   +----------------+----------------+   |
                                                +--------------------|--------------------+
                                                                     | Non-Livepatch Fallback
                                                                     v
                                                +-----------------------------------------+
                                                |       Automatic Reboot Window           |
                                                |   If /var/run/reboot-required exists:   |
                                                |   Reboots at: Unattended-Upgrade::     |
                                                |   Automatic-Reboot-Time "03:00";        |
                                                +-----------------------------------------+
```

---

## Syntax & CLI Tools

### Ubuntu / Debian `unattended-upgrades` Operations
```bash
# Test execution dry-run (simulation without modifying packages)
sudo unattended-upgrade --dry-run --debug

# Run manual upgrade immediately using the unattended configuration
sudo unattended-upgrade -d

# Check if a system reboot is currently pending after kernel upgrades
test -f /var/run/reboot-required && echo "Reboot is required!" || echo "No reboot needed."
cat /var/run/reboot-required.pkgs 2>/dev/null

# Identify processes needing restart using needrestart
sudo needrestart -b -v
```

### RHEL / Rocky Linux `dnf-automatic` Operations
```bash
# Verify dnf-automatic configuration syntax
sudo dnf-automatic /etc/dnf/automatic.conf --check-only

# Apply security updates non-interactively
sudo dnf update --security -y

# Check active timer status
sudo systemctl status dnf-automatic.timer
```

### Live Kernel Patching with `kpatch`
```bash
# List all active live kernel patches installed in memory
sudo kpatch list

# Apply a kernel live patch module
sudo kpatch load /var/lib/kpatch/6.8.0/kpatch-cve-2026-1234.ko

# Query Canonical Livepatch status (Ubuntu)
canonical-livepatch status
```

---

## Progressive Practical Examples

### Example 1: Configuring Hardened `unattended-upgrades` on Ubuntu 24.04
Ensure the server automatically installs security updates, restarts services cleanly, and reboots only within an approved maintenance window (03:00 UTC).

```ini
// /etc/apt/apt.conf.d/50unattended-upgrades

// 1. Restrict upgrade sources strictly to security origins
Unattended-Upgrade::Allowed-Origins {
    "${distro_id}:${distro_codename}-security";
    // "${distro_id}ESMApps:${distro_codename}-apps-security"; // If Ubuntu Pro
};

// 2. Blacklist critical database and stateful packages from automated upgrades
Unattended-Upgrade::Package-Blacklist {
    "postgresql.*";
    "mysql.*";
    "redis.*";
    "docker-ce.*";
    "kube.*";
};

// 3. Cleanup unused kernel images and dependencies automatically
Unattended-Upgrade::Remove-Unused-Kernel-Packages "true";
Unattended-Upgrade::Remove-New-Unused-Dependencies "true";
Unattended-Upgrade::Remove-Unused-Dependencies "true";

// 4. Automatic Reboot Management
Unattended-Upgrade::Automatic-Reboot "true";
Unattended-Upgrade::Automatic-Reboot-WithUsers "false";
Unattended-Upgrade::Automatic-Reboot-Time "03:00";

// 5. Notifications
Unattended-Upgrade::Mail "secops-alerts@company.internal";
Unattended-Upgrade::MailReport "on-change";
```

```ini
// /etc/apt/apt.conf.d/20auto-upgrades
// Enable automated daily runs
APT::Periodic::Update-Package-Lists "1";
APT::Periodic::Unattended-Upgrade "1";
APT::Periodic::AutocleanInterval "7";
```

### Example 2: Configuring Automated Security Patching on RHEL 9 (`dnf-automatic`)
Configure RHEL / Rocky Linux to download and install security errata automatically.

```ini
# /etc/dnf/automatic.conf
[commands]
# Restrict upgrade type to security advisories only
upgrade_type = security
random_sleep = 300
download_updates = yes
apply_updates = yes

[emitters]
emit_via = motd, stdio

[email]
email_from = root@srv-db-01.internal
email_to = secops@company.internal
email_host = localhost

[base]
debuglevel = 1
```

```bash
# Enable and start the systemd timer:
sudo systemctl enable --now dnf-automatic.timer
sudo systemctl list-timers | grep dnf-automatic
```

### Example 3: Automating Service Restarts with `needrestart`
Prevent unpatched in-memory libraries from lingering after an upgrade.

```bash
# 1. Install needrestart
sudo apt-get install -y needrestart

# 2. Configure automatic restart mode in /etc/needrestart/needrestart.conf
sudo tee /etc/needrestart/conf.d/99-auto-restart.conf << 'EOF'
# Modes: (l)ist only, (i)nteractive, (a)utomatic
$nrconf{restart} = 'a';

# Ignore user desktop sessions
$nrconf{ui} = 'NeedRestart::UI::stdio';

# Override specific services to never restart automatically
$nrconf{override_rc} = {
    q(^postgresql) => 0,
    q(^mysql) => 0,
};
EOF

# 3. Test execution
sudo needrestart -r a
```

### Example 4: Verifying Live Kernel Patching with Canonical Livepatch
How enterprise servers apply kernel security fixes without server reboots.

```bash
# 1. Check Livepatch client status
canonical-livepatch status --verbose

# Sample Output:
# Client version: 10.8.2
# Architecture: x86_64
# Kernel version: 6.8.0-31-generic
# Livepatch status:
#   Patch state: ✓ all patches applied
#   Patch version: 108.1
#   Patches:
#     * CVE-2026-2184: applied (Fixes privilege escalation in netfilter)
#     * CVE-2026-1892: applied (Fixes use-after-free in ext4)
#   Fixes applied: 2
#   Reboot required: false
```

### Example 5: Zero-Downtime Rolling Patching Script (Load Balancer Drain)
An automated bash orchestration script that patches a web node gracefully without dropping customer traffic.

```bash
#!/usr/bin/env bash
set -euo pipefail

NODE_IP="10.0.1.10"
LB_API="https://lb.internal/api/v1"

echo "==> 1. Draining traffic from ${NODE_IP} at load balancer..."
curl -s -X POST "${LB_API}/targets/${NODE_IP}/drain"
sleep 15  # Allow existing in-flight connections to finish gracefully

echo "==> 2. Executing system security updates..."
sudo apt-get update
sudo apt-get upgrade -y -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold"

echo "==> 3. Restarting stale services..."
sudo needrestart -r a

echo "==> 4. Checking if kernel reboot is mandatory..."
if [ -f /var/run/reboot-required ]; then
    echo "Reboot required. Rebooting node ${NODE_IP}..."
    sudo reboot
    # (Monitoring script waits for node to return online via SSH ping)
fi

echo "==> 5. Restoring node ${NODE_IP} into load balancer rotation..."
curl -s -X POST "${LB_API}/targets/${NODE_IP}/enable"
echo "==> Node successfully patched with zero customer downtime."
```

---

## Best Practices

1. **Restrict Automated Upgrades Strictly to Security Repositories**: Never configure automated updaters to pull from standard general release repositories or unvetted PPAs. Automated patching must strictly ingest security updates (`*-security`) to prevent unintentional major library changes.
2. **Always Blacklist Stateful Database Packages**: Add database servers (PostgreSQL, MySQL, Redis, MongoDB) and container engines (Docker, Kubelet) to the `Package-Blacklist` array. Database upgrades should always be orchestrated through controlled maintenance runbooks with manual pre-upgrade snapshots.
3. **Always Run `needrestart` Post-Upgrade**: Upgrading `libssl3` or `glibc` without restarting dependent daemons (Nginx, Postfix, SSH) leaves the service executing the vulnerable in-memory library. Ensure `needrestart` runs automatically after package installations.
4. **Deploy Live Kernel Patching on Mission-Critical Hosts**: Use Canonical Livepatch (Ubuntu) or `kpatch` (RHEL) on critical production databases and hypervisors. This allows patching zero-day kernel vulnerabilities instantly without scheduling emergency maintenance outages.
5. **Enforce Coordinated Rolling Maintenance in Cloud Fleets**: In Kubernetes or auto-scaling groups, never patch all instances simultaneously. Use `kubectl drain <node> --ignore-daemonsets` or cloud rolling update policies (max unavailable: 1) to guarantee zero application downtime.
6. **Centralize Patch Compliance Monitoring**: Scrape `/var/run/reboot-required` and uninstalled security counts via Prometheus Node Exporter's textfile collector. Generate Grafana dashboards alerting SecOps when nodes have unpatched CVEs older than 7 days.

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Allowing automated reboots at random times (`Automatic-Reboot "true"` without time window) | Reboots node in the middle of business hours, dropping active user transactions. | Set `Unattended-Upgrade::Automatic-Reboot-Time "03:00";` and enforce user check. |
| Forgetting to configure package blacklists | An automated minor version bump in PostgreSQL or Docker restarts the database unexpectedly. | Add databases and runtime engines to `Package-Blacklist`. |
| Assuming a package update fixed the vulnerability without restarting the service | The vulnerable `.so` library remains loaded in running process RAM until the process restarts. | Use `needrestart` or restart affected daemons immediately. |
| Upgrading kernels without removing old kernel images | Over months, `/boot` fills to 100% with obsolete kernels, causing all future `apt` upgrades to crash with `No space left on device`. | Set `Unattended-Upgrade::Remove-Unused-Kernel-Packages "true";`. |
| Running `apt-get dist-upgrade` unattended in production | Can remove existing packages or install breaking major dependencies. | Confine to security updates: `unattended-upgrade`. |
| Overwriting custom configuration files during automated upgrades | Prompts for manual input or overwrites custom configs with default vendor files. | Enforce `-o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold"`. |

---

## Performance, Security & Cost Implications

### Performance
- **Zero Runtime Overhead**: Neither `unattended-upgrades` nor `dnf-automatic` run persistent background daemons. They execute once or twice daily via lightweight systemd timers (`apt-daily.timer`), consuming CPU only during active package installations.
- **Kpatch Function Redirection**: Live kernel patching introduces a single branch instruction per patched function call via `ftrace`. The performance impact is **imperceptible (< 0.05%)**.

### Security
- **Mean Time to Remediate (MTTR)**: Automated security patching collapses the vulnerability exposure window from weeks to **under 24 hours**. In forensic breach analysis, over 80% of compromised Linux servers fell victim to known CVEs for which security patches had been published weeks prior.

### Cost
- Automated patching eliminates hundreds of hours of manual sysadmin overtime spent conducting repetitive weekend patching drills, saving substantial operational engineering overhead while avoiding regulatory non-compliance penalties.

---

## Comparison Table / Alternatives

| Dimension | `unattended-upgrades` | `dnf-automatic` | Livepatch / `kpatch` | Manual Maintenance Window |
| :--- | :--- | :--- | :--- | :--- |
| **Primary OS** | Ubuntu / Debian | RHEL / Rocky / Fedora | Linux Kernel (Universal) | Any OS |
| **Target Scope** | Security packages & user libraries | Security / bugfix RPMs | Kernel vulnerabilities only | Full system & all packages |
| **Reboot Required** | Yes (if kernel updated) | Yes (if kernel updated) | **Zero Reboot Required** | Yes |
| **Execution Model** | Systemd Timer (Batch) | Systemd Timer (Batch) | In-Memory Dynamic Binary Patch | Manual SSH / Ansible execution |
| **Best Used For** | Standard VM & node patching | Enterprise RPM server patching | Mission-critical 99.999% databases | Major version architectural upgrades |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: An administrator updated OpenSSL via `apt update && apt upgrade openssl`, but a vulnerability scanner still reports the server as vulnerable. Why?
<details>
<summary>View Solution</summary>

**Root Cause**: While the updated shared library (`libssl.so.3`) was written to disk, all long-running services (e.g. Nginx, SSHD, Node.js) that were running before the upgrade **still have the old, vulnerable library loaded in memory**.

**Fix**:
1. Run `needrestart -b` to list all processes using deleted libraries.
2. Restart the affected services:
   ```bash
   sudo systemctl restart nginx ssh
   ```
3. Once restarted, the processes load the newly patched library from disk.
</details>

### Q2: How does Live Kernel Patching work under the hood without crashing the OS?
<details>
<summary>View Solution</summary>

Live kernel patching leverages the kernel's **`ftrace`** infrastructure:
1. A patch module containing the corrected C function is compiled into a kernel module (`.ko`).
2. When loaded, `kpatch` uses the compiler's `-pg` profiling hooks placed at the entry of every kernel function.
3. It atomically replaces the entry instruction of the vulnerable function with a jump instruction pointing to the replacement function.
4. The kernel uses Read-Copy-Update (RCU) synchronization to ensure that any CPU thread currently executing mid-way through the old function finishes cleanly before transitioning exclusively to the patched code.
</details>

### Q3: Why does `/boot` frequently run out of space on Debian/Ubuntu systems and how do you prevent it?
<details>
<summary>View Solution</summary>

Every kernel update installs a new kernel image (`vmlinuz`), initramfs (`initrd.img`), and system map into `/boot` (~100MB per version).
- By default, older distributions retained every installed kernel indefinitely.
- Over time, `/boot` reaches 100% capacity. When a new update arrives, `dpkg` crashes with `No space left on device`, leaving the package database in a broken state.

**Prevention**:
Ensure `/etc/apt/apt.conf.d/50unattended-upgrades` contains:
```text
Unattended-Upgrade::Remove-Unused-Kernel-Packages "true";
```
Or run periodically:
```bash
sudo apt-get autoremove --purge
```
</details>

### Q4: How do you identify whether an automated upgrade triggered a reboot on an Ubuntu machine?
<details>
<summary>View Solution</summary>

1. Check the dedicated unattended-upgrades log:
   ```bash
   cat /var/log/unattended-upgrades/unattended-upgrades.log | grep -i reboot
   ```
2. Check systemd reboot logs:
   ```bash
   last reboot
   journalctl -u unattended-upgrades.service
   ```
</details>

### Q5: What is the purpose of `dpkg --configure -a` in patch troubleshooting?
<details>
<summary>View Solution</summary>

If an automated upgrade or manual `apt` execution is interrupted mid-way (e.g., loss of power, process killed, out of disk space), the DPKG package manager locks up in an inconsistent state, returning `E: dpkg was interrupted, you must manually run 'sudo dpkg --configure -a'`.
Running `sudo dpkg --configure -a` instructs DPKG to resume and complete all unconfigured post-installation scripts, clearing locks and repairing package dependencies.
</details>

### Q6: How do you test whether an `unattended-upgrades` configuration has valid syntax without applying any patches?
<details>
<summary>View Solution</summary>

Run `unattended-upgrade` with `--dry-run` and `--debug`:
```bash
sudo unattended-upgrade --dry-run --debug
```
It evaluates origins, parses the blacklist, checks locks, and displays exactly which packages would be upgraded in production without installing anything.
</details>

### Q7: What is the risk of using automated patching on Kubernetes worker nodes?
<details>
<summary>View Solution</summary>

If an automated package manager updates the container runtime (`containerd`) or reboots the worker node unexpectedly:
- Running pods are abruptly terminated rather than gracefully drained.
- StatefulSets can experience data corruption if transactions are in-flight.
- If multiple worker nodes reboot simultaneously, quorum can be lost in replicated services.
*Best Practice*: Disable uncoordinated unattended reboots on Kubernetes nodes. Use automated node lifecycle operators (like Kured - KUbernetes REboot Daemon) that coordinate node cordoning, pod draining, and sequential reboots.
</details>

### Q8: How can you check what specific CVEs were fixed in a given package update on Ubuntu?
<details>
<summary>View Solution</summary>

Inspect the package changelog:
```bash
apt-get changelog <package_name> | head -n 30
```
Ubuntu changelogs explicitly list the Launchpad bug numbers and CVE IDs (e.g., `Fixes CVE-2026-1234`) addressed in each security release.
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Simulating an Unattended-Upgrades Dry Run
**Task**: Run a dry-run execution of `unattended-upgrades`. Inspect which repositories are evaluated.
<details>
<summary>View Solution</summary>

```bash
if command -v unattended-upgrade &>/dev/null; then
    sudo unattended-upgrade --dry-run --debug | head -n 30
else
    echo "unattended-upgrades not installed on this system."
fi
```
</details>

### Exercise 2: Checking System Reboot Status
**Task**: Inspect your system to determine if a reboot is currently required after recent library or kernel updates.
<details>
<summary>View Solution</summary>

```bash
if [ -f /var/run/reboot-required ]; then
    echo "ALERT: System reboot is currently REQUIRED!"
    cat /var/run/reboot-required.pkgs 2>/dev/null || true
else
    echo "OK: No reboot currently required."
fi
```
</details>

### Exercise 3: Auditing In-Memory Outdated Libraries with `needrestart`
**Task**: Install `needrestart` (if missing) and query which running processes are executing outdated shared libraries.
<details>
<summary>View Solution</summary>

```bash
if command -v needrestart &>/dev/null; then
    sudo needrestart -b
else
    echo "needrestart not installed; querying via raw lsof:"
    sudo lsof +c 0 | grep 'DEL.*lib' | awk '{print $1, $2, $NF}' | sort -u | head -n 15
fi
```
</details>

### Exercise 4: Inspecting Systemd Automatic Patch Timers
**Task**: Verify the status and next scheduled execution time of automated patching timers.
<details>
<summary>View Solution</summary>

```bash
# Query active timers
systemctl list-timers | grep -E "apt-daily|dnf-automatic"
```
</details>

### Exercise 5: Querying Package Security Changelogs
**Task**: View the security changelog for `openssl` or `curl` to inspect recently patched CVE numbers.
<details>
<summary>View Solution</summary>

```bash
if command -v apt-get &>/dev/null; then
    apt-get changelog curl 2>/dev/null | grep -i "cve" | head -n 10 || echo "Changelog not reachable"
fi
```
</details>

### Exercise 6: Verifying Automatic Kernel Cleanup Configuration
**Task**: Check your APT configuration to ensure automatic removal of obsolete kernel images is enabled.
<details>
<summary>View Solution</summary>

```bash
apt-config dump | grep -i "Remove-Unused-Kernel-Packages"
# Expected output: APT::Periodic::Unattended-Upgrade::Remove-Unused-Kernel-Packages "1";
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Operation | Command Example |
| :--- | :--- |
| **Simulate Upgrade Dry-Run** | `sudo unattended-upgrade --dry-run --debug` |
| **Check Pending Reboot** | `test -f /var/run/reboot-required && echo "Reboot needed"` |
| **List Packages Needing Reboot**| `cat /var/run/reboot-required.pkgs` |
| **Inspect Stale Processes** | `sudo needrestart -b` |
| **RHEL Security Update** | `sudo dnf update --security -y` |
| **RHEL Timer Status** | `sudo systemctl status dnf-automatic.timer` |
| **List Kernel Livepatches** | `sudo kpatch list` (or `canonical-livepatch status`) |
| **Repair Broken Package DB** | `sudo dpkg --configure -a` |
| **Purge Unused Kernels** | `sudo apt-get autoremove --purge` |

### Further Reading
- `man 8 unattended-upgrade` and `man 1 needrestart`
- `man 8 dnf-automatic` and `man 8 kpatch`
- [Debian Security Tracker & CVE Database](https://security-tracker.debian.org/tracker/)
- [Ubuntu Security Notices (USN)](https://ubuntu.com/security/notices)
- [Red Hat Product Security Center](https://access.redhat.com/security/)
