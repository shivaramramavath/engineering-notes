# Linux Server Hardening: CIS Benchmarks, Kernel Security, and Lynis

| Metadata | Details |
| :--- | :--- |
| **Topic** | Enterprise Linux Server Hardening, CIS Benchmarks, Kernel Sysctl Security, Lynis Auditing |
| **Module** | `05-production` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `01-fundamentals/07-permissions.md`, `03-system/11-kernel-modules-and-sysctl.md`, `04-networking/05-firewall.md` |
| **Target Audience** | Security Engineers, DevSecOps Engineers, SREs, Systems Administrators |
| **Target Distribution** | Ubuntu 24.04 LTS, Rocky Linux / RHEL 9.4 |
| **Key Tools / Standards** | CIS Benchmarks, Lynis, `sysctl`, `systemd-analyze security`, `/etc/sysctl.d/`, `/etc/security/` |

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

A default installation of a modern Linux operating system prioritizes hardware compatibility, developer convenience, and broad usability over strict defense-in-depth. Out-of-the-box servers frequently feature:
- Unused legacy network protocols (DCCP, SCTP, RDS, TIPC) compiled into kernel modules.
- Unrestricted core dumps exposing memory secrets and credentials.
- Insecure filesystem mount options (`exec` on `/tmp`, missing `nodev` on `/var/tmp`).
- World-readable diagnostic logs and unmasked user file creation permissions (`umask 022`).
- Default network kernel parameters vulnerable to ICMP redirect attacks and SYN floods.

**Server Hardening** is the methodical process of minimizing the system's attack surface, enforcing strict least-privilege configurations, and eliminating attack vectors. Standardized industry frameworks like the **Center for Internet Security (CIS) Benchmarks** and the **US DoD DISA Security Technical Implementation Guides (STIGs)** provide comprehensive blueprints. Automated compliance scanners such as **Lynis** allow continuous verification of server hardening posture across cloud and bare-metal fleets.

---

## Core Concepts

### Defense in Depth and Attack Surface Reduction
Hardening operates across multiple distinct operating system layers:
1. **Hardware & Firmware Layer**: Secure Boot (UEFI) verification, disabled legacy USB storage modules, locked BIOS/UEFI passwords.
2. **Filesystem Layer**: Partition segregation (`/tmp`, `/var`, `/var/log`, `/var/tmp`, `/home`), disabling SUID/SGID binaries, enforcing mount options (`noexec`, `nosuid`, `nodev`).
3. **Kernel Layer**: Disabling unused network protocol modules, enabling Address Space Layout Randomization (ASLR), restricting `dmesg` access, disabling unprivileged BPF.
4. **Network Layer**: Host-level firewalling (`nftables`), disabling ICMP redirects, source routing verification, TCP SYN cookie protection.
5. **Application & Service Layer**: Removing unneeded packages, running daemons inside sandboxed systemd units (`DynamicUser=yes`, `ProtectSystem=strict`), removing compilers (`gcc`) from production nodes.

### The CIS Benchmark Levels
The Center for Internet Security publishes consensus-based configuration baselines categorized into:
- **Level 1 (Benchmark)**: Essential security controls that can be deployed across production systems with minimal or zero risk of breaking normal application operations.
- **Level 2 (High Security / Defense in Depth)**: Stringent security controls intended for environments requiring defense against advanced persistent threats (APTs). Can cause compatibility issues if not carefully tailored to application workloads.

### Lynis Automated Security Auditing
**Lynis** is an open-source, non-destructive security auditing engine written in modular POSIX shell. It inspects:
- Authentication, PAM policies, password strengths.
- Kernel configurations, loaded modules, and sysctl flags.
- Network listening ports and firewall rules.
- File permissions, SUID binaries, and filesystem mounts.
It generates a comprehensive **Hardening Index** score (0–100) along with prioritized actionable warnings and suggestions.

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                                    Hardened Linux Architecture                          |
+-----------------------------------------------------------------------------------------+
| [Userspace Applications / Containers]                                                   |
|    - Restricted umask (027)                                                             |
|    - No SUID/SGID vulnerabilities                                                       |
|    - Sandboxed systemd service units (ProtectSystem=strict, PrivateTmp=yes)             |
+----------------------------------------------------+------------------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                                  Filesystem Boundary Controls                           |
|   /tmp       -> mounted: [rw, nosuid, nodev, noexec]                                    |
|   /var/tmp   -> mounted: [rw, nosuid, nodev, noexec]                                    |
|   /dev/shm   -> mounted: [rw, nosuid, nodev, noexec]                                    |
+----------------------------------------------------+------------------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                                Linux Kernel Security Layer                              |
|   - ASLR = 2 (Full Address Space Layout Randomization)                                  |
|   - kernel.kptr_restrict = 2 (Hide kernel pointers from unprivileged users)             |
|   - kernel.dmesg_restrict = 1 (Restrict dmesg ring buffer to root)                      |
|   - fs.protected_symlinks = 1 / fs.protected_hardlinks = 1                              |
|   - kernel.unprivileged_bpf_disabled = 1                                                |
+----------------------------------------------------+------------------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                                Kernel Module Blacklisting                               |
|   /etc/modprobe.d/blacklist.conf -> install dccp /bin/true                              |
|   (Unused legacy protocol modules permanently disabled from dynamic loading)             |
+-----------------------------------------------------------------------------------------+
```

### Module Probing Lockout (`/bin/true`)
In Linux, simply adding a module to `/etc/modprobe.d/blacklist.conf` stops automatic hardware detection loading, but **does not prevent** another program or unprivileged user from triggering dynamic loading via `modprobe` or socket creation (e.g. `socket(AF_INET, SOCK_DCCP, 0)`).
To permanently disable a kernel module, bind its installation command to `/bin/true`:
```text
install dccp /bin/true
install sctp /bin/true
install usb-storage /bin/true
```
When the kernel attempts to load the module, it executes `/bin/true`, which exits immediately with status `0` without loading the binary driver into kernel space.

---

## Syntax & CLI Tools

### Auditing System Health with `lynis`
```bash
# Run complete automated audit
sudo lynis audit system

# Run audit non-interactively for CI/CD automation and cron reporting
sudo lynis audit system --quick --warnings-only

# Inspect audit report and log files
cat /var/log/lynis-report.dat | grep -E "hardening_index|warning"
cat /var/log/lynis.log
```

### Auditing Systemd Service Sandboxing
```bash
# Evaluate security exposure scores (0.0=best, 10.0=worst) across all systemd units
systemd-analyze security

# Evaluate security profile of a specific service
systemd-analyze security nginx.service
```

### Kernel Hardening with `sysctl`
```bash
# Query active kernel security parameters
sysctl kernel.randomize_va_space
sysctl fs.protected_symlinks
sysctl net.ipv4.conf.all.rp_filter

# Apply hardened drop-in configuration file
sudo sysctl --system
```

### File Integrity and SUID Hunting
```bash
# Locate all SUID binaries on root filesystem
find / -xdev -perm -4000 -type f -exec ls -ld {} +

# Locate all SGID binaries
find / -xdev -perm -2000 -type f -exec ls -ld {} +

# Locate world-writable files
find / -xdev -type f -perm -0002 -exec ls -ld {} +
```

---

## Progressive Practical Examples

### Example 1: Enforcing Partition Mount Security Options in `/etc/fstab`
Attackers and malware frequently write exploit scripts into `/tmp` and execute them. Hardening `/tmp` with `noexec,nosuid,nodev` neutralizes these attacks.

```text
# /etc/fstab entries for hardened mount points
tmpfs       /tmp        tmpfs   rw,nosuid,nodev,noexec,relatime,size=2G   0  0
tmpfs       /var/tmp    tmpfs   rw,nosuid,nodev,noexec,relatime,size=2G   0  0
tmpfs       /dev/shm    tmpfs   rw,nosuid,nodev,noexec,relatime,size=1G   0  0
```

```bash
# Remount existing partitions with hardened security options
sudo mount -o remount,noexec,nosuid,nodev /tmp
sudo mount -o remount,noexec,nosuid,nodev /dev/shm

# Verify active mount options
mount | grep -E "/tmp|/dev/shm"
```

### Example 2: Hardening Kernel Network and Memory via `/etc/sysctl.d/99-hardened.conf`
Production-grade CIS Level 1 and Level 2 kernel hardening parameters.

```ini
# /etc/sysctl.d/99-hardened.conf

# 1. Memory and Core Security
kernel.randomize_va_space = 2
kernel.kptr_restrict = 2
kernel.dmesg_restrict = 1
kernel.unprivileged_bpf_disabled = 1
kernel.core_uses_pid = 1
fs.suid_dumpable = 0

# 2. File Link Protection (Prevents TOCTOU symlink races in /tmp)
fs.protected_hardlinks = 1
fs.protected_symlinks = 1
fs.protected_fifos = 2
fs.protected_regular = 2

# 3. IP Spoofing and Reverse Path Filtering
net.ipv4.conf.all.rp_filter = 1
net.ipv4.conf.default.rp_filter = 1

# 4. Disable ICMP Redirects (Prevents Man-in-the-Middle routing poisoning)
net.ipv4.conf.all.accept_redirects = 0
net.ipv4.conf.default.accept_redirects = 0
net.ipv4.conf.all.send_redirects = 0
net.ipv4.conf.default.send_redirects = 0
net.ipv6.conf.all.accept_redirects = 0
net.ipv6.conf.default.accept_redirects = 0

# 5. Disable Source Routing
net.ipv4.conf.all.accept_source_route = 0
net.ipv4.conf.default.accept_source_route = 0
net.ipv6.conf.all.accept_source_route = 0

# 6. TCP SYN Flood Protection and Logging
net.ipv4.tcp_syncookies = 1
net.ipv4.conf.all.log_martians = 1
net.ipv4.conf.default.log_martians = 1
net.ipv4.icmp_echo_ignore_broadcasts = 1
```

```bash
# Apply parameters immediately without reboot:
sudo sysctl -p /etc/sysctl.d/99-hardened.conf
```

### Example 3: Disabling Unused Kernel Modules and Storage Devices
Neutralize attack vectors in obsolete networking stacks and prevent USB rogue device insertions.

```bash
# Create modular disablement file
sudo tee /etc/modprobe.d/99-security-disable.conf << 'EOF'
# Disable legacy network protocols
install dccp /bin/true
install sctp /bin/true
install rds /bin/true
install tipc /bin/true

# Disable legacy/uncommon filesystems
install cramfs /bin/true
install freevxfs /bin/true
install jffs2 /bin/true
install hfs /bin/true
install hfsplus /bin/true
install squashfs /bin/true
install udf /bin/true

# Disable physical USB mass storage (prevents BadUSB / exfiltration on bare-metal)
install usb-storage /bin/true
EOF

# Update initramfs to persist across early boot
sudo update-initramfs -u
```

### Example 4: Enforcing Restrictive System Umask and Password Policies
Prevent newly created files from being world-readable.

```bash
# 1. Enforce umask 027 (User: rwx, Group: r-x, Other: none) in /etc/login.defs
sudo sed -i 's/^UMASK.*/UMASK\t\t027/' /etc/login.defs

# 2. Enforce umask in system-wide profile drop-in
sudo tee /etc/profile.d/99-umask.sh << 'EOF'
# Default secure umask for all interactive shells
umask 027
EOF

# 3. Harden password aging policies in /etc/login.defs
sudo sed -i 's/^PASS_MAX_DAYS.*/PASS_MAX_DAYS\t90/' /etc/login.defs
sudo sed -i 's/^PASS_MIN_DAYS.*/PASS_MIN_DAYS\t7/' /etc/login.defs
sudo sed -i 's/^PASS_WARN_AGE.*/PASS_WARN_AGE\t14/' /etc/login.defs
```

### Example 5: Automated Daily Hardening Verification Script
A script running Lynis and auditing SUID drift for alerting engines.

```bash
#!/usr/bin/env bash
set -euo pipefail

REPORT_DIR="/var/log/security_audits"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
mkdir -p "${REPORT_DIR}"

echo "==> Starting automated Lynis audit..."
lynis audit system --quick --auditor "DevSecOps-Automated" > "${REPORT_DIR}/lynis_${TIMESTAMP}.log"

# Extract Hardening Index
SCORE=$(grep "hardening_index=" /var/log/lynis-report.dat | cut -d= -f2)
WARNINGS=$(grep "warning\[\]=" /var/log/lynis-report.dat | wc -l)

echo "Lynis Hardening Score: ${SCORE} / 100"
echo "Active Warnings: ${WARNINGS}"

if [ "${SCORE}" -lt 70 ]; then
    echo "ALERT: Hardening index dropped below threshold (${SCORE} < 70)!"
    # Alert via Slack webhook or pagerduty here
fi

echo "==> Checking for new SUID binaries..."
find / -xdev -perm -4000 -type f 2>/dev/null | sort > "${REPORT_DIR}/suid_${TIMESTAMP}.txt"
```

---

## Best Practices

1. **Adopt CIS Benchmark Automation via Ansible**: Rather than hardening servers by executing ad-hoc manual scripts, utilize community-vetted, idempotent automation (e.g., `dev-sec.ansible-collection-hardening` or official CIS Ansible roles) to maintain consistent baseline compliance.
2. **Never Execute `chmod -R 777` on Any Production Directory**: If an application requires shared permissions, utilize dedicated system groups, POSIX ACLs (`setfacl`), or Unix Domain Sockets with restricted group ownership.
3. **Audit SUID/SGID Binaries Regularly**: Attackers exploit vulnerabilities in SUID binaries (like `pkexec`, `sudo`, `chfn`, or legacy utilities) to achieve local privilege escalation to root. Remove the SUID bit on unneeded utilities: `chmod u-s /usr/bin/chsh /usr/bin/chfn`.
4. **Enforce `noexec` on `/tmp` and `/var/tmp`**: Ensure applications that require temporary execution (like Java compiler or package managers) specify an explicit alternate temporary directory (`-Djava.io.tmpdir=/var/app/tmp`) mounted with execution rights, keeping global `/tmp` non-executable.
5. **Disable Core Dumps Globally**: Process core dumps contain raw application memory, which often holds decrypted private keys, database passwords, and API tokens. Disable core dumps via `/etc/security/limits.conf` (`* hard core 0`) and sysctl (`fs.suid_dumpable = 0`).
6. **Restrict Direct Access to Kernel Logs (`dmesg`)**: Unprivileged users should not read kernel logs, which expose ASLR memory offsets and hardware pointers useful for buffer overflow exploitation. Set `sysctl -w kernel.dmesg_restrict=1`.

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Applying `noexec` on `/tmp` when package managers use it | `apt` or `dpkg` pre-installation scripts execute binaries from `/tmp`, failing package installs with `Permission denied`. | Configure `/etc/apt/apt.conf.d/` with `DPkg::Tools::Options::/usr/lib/cnf-update-db::Version "2";` or temporarily remount during maintenance. |
| Blacklisting a module via `blacklist <name>` only | Kernel automatically loads module if an unprivileged socket call requests it. | Use `install <module> /bin/true` to completely disable dynamic loading. |
| Forgetting to update `initramfs` after modprobe changes | The module is baked into early boot RAM disk and loads before root filesystem configurations are read. | Run `sudo update-initramfs -u` (Debian/Ubuntu) or `dracut -f` (RHEL). |
| Setting `UMASK 027` without auditing service user impacts | Third-party services running under shared service users fail to read configuration files. | Validate application permissions and use group read (`027`) rather than restrictive `077`. |
| Setting `kernel.randomize_va_space = 0` | Strips ASLR protection entirely; stack and heap memory addresses become deterministic, facilitating buffer overflow exploits. | Always keep `kernel.randomize_va_space = 2` (full ASLR). |
| Neglecting to remove development compilers from production | Attackers who compromise a web user (e.g. `www-data`) can compile kernel privilege escalation exploits locally. | Remove `gcc`, `make`, and `g++` from production VM images. |

---

## Performance, Security & Cost Implications

### Performance
- **Zero Performance Penalty**: The vast majority of kernel hardening measures (`sysctl` network parameter tuning, disabling unused modules, enforcing mount security flags, restricting ASLR) incur **0% runtime performance overhead**.
- **SYN Cookie Impact**: `net.ipv4.tcp_syncookies = 1` only activates when the SYN backlog queue overflows under an active attack. During normal operation, TCP connections proceed with zero latency penalty.

### Security
- **Defense in Depth**: Hardening assumes that the perimeter will eventually be breached. Even if an attacker gains Remote Code Execution (RCE) in a web framework, filesystem `noexec`, restricted SUIDs, and disabled unprivileged BPF prevent them from escalating privileges or achieving persistence.

### Cost
- Implementing automated CIS hardening baselines upfront prevents costly regulatory compliance fines (PCI-DSS, SOC 2, HIPAA, ISO 27001) and drastically cuts incident response engineering hours.

---

## Comparison Table / Alternatives

| Security Control | Default Distribution State | Hardened Production State | Primary Attack Vector Mitigated |
| :--- | :--- | :--- | :--- |
| **`/tmp` Mount Options** | `rw,relatime` (Permits execution) | `rw,nosuid,nodev,noexec` | Script/malware execution from world-writable directories |
| **Kernel Pointer Visibility**| `kptr_restrict = 0` (Exposed) | `kptr_restrict = 2` (Hidden) | Memory disclosure; exploitation of kernel buffer overflows |
| **ASLR Status** | `randomize_va_space = 2` | `randomize_va_space = 2` | Return-Oriented Programming (ROP) and memory hijacking |
| **ICMP Redirects** | `accept_redirects = 1` | `accept_redirects = 0` | Man-in-the-Middle network route poisoning |
| **Legacy Network Protocols** | Modules auto-load on demand | Permanently disabled via `/bin/true` | Kernel vulnerabilities in unmaintained stacks (DCCP, SCTP) |
| **Core Dumps** | Enabled (dumps to disk) | Disabled (`fs.suid_dumpable = 0`) | Credential and memory secret exfiltration |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: Why does `apt-get install` or a database migration script fail with `Permission denied` after mounting `/tmp` with `noexec`?
<details>
<summary>View Solution</summary>

**Root Cause**: Certain package installation scripts (`dpkg`), Python wheel builders, and database backup routines write temporary executable helper scripts or binary shared objects into `/tmp` and attempt to execute them directly. With `noexec`, the kernel's virtual filesystem (VFS) layer rejects the `execve(2)` system call immediately.

**Fix**:
1. For application processes: Redirect their temporary directory to a private execution path via environment variables:
   ```bash
   export TMPDIR=/var/run/app-temp/
   ```
2. For APT / DPKG: Configure APT to use an alternate temporary path in `/etc/apt/apt.conf.d/99tmpdir`:
   ```text
   APT::ExtractTemplates::TempDir "/var/tmp";
   ```
   (Ensure `/var/tmp` is mounted without `noexec`, or mount it with `noexec` and remount during system upgrades).
</details>

### Q2: What is Address Space Layout Randomization (ASLR) and how do you verify its status in Linux?
<details>
<summary>View Solution</summary>

**ASLR** is a memory protection mechanism that randomizes the memory address positions of key data areas (stack, heap, shared libraries, and executable code) in a process's virtual address space every time it executes. This makes Return-Oriented Programming (ROP) and buffer-overflow exploits extremely difficult because the attacker cannot predict target function pointers in memory.

**Verification**:
```bash
cat /proc/sys/kernel/randomize_va_space
```
- `0`: Disabled (insecure).
- `1`: Conservative Randomization (randomizes shared libraries, stack, and mmap).
- `2`: **Full Randomization** (randomizes shared libraries, stack, mmap, and data segment / brk heap). **Mandatory in production**.
</details>

### Q3: How do you identify all world-writable files on a Linux server and why are they dangerous?
<details>
<summary>View Solution</summary>

World-writable files (`o+w` / permissions ending in `2`, `3`, `6`, or `7`) can be modified or overwritten by any unprivileged user or compromised service account on the system. If an attacker modifies a world-writable script executed by root or cron, they gain instant root access.

**Locate with find**:
```bash
find / -xdev -type f -perm -0002 -exec ls -ld {} +
```
*Note*: Exclude standard legitimate temporary directories (`/tmp`, `/var/tmp`) and symlinks.
</details>

### Q4: What is the purpose of `fs.protected_symlinks` and `fs.protected_hardlinks`?
<details>
<summary>View Solution</summary>

These kernel parameters defend against **Time-of-Check to Time-of-Use (TOCTOU) symlink attacks** in sticky world-writable directories (like `/tmp`):
- `fs.protected_symlinks = 1`: Disallows following a symlink in a world-writable sticky directory unless the owner of the symlink matches either the follower or the owner of the directory. Prevents unprivileged users from creating symlinks pointing to `/etc/shadow` that a root cron job accidentally overwrites.
- `fs.protected_hardlinks = 1`: Disallows creating a hardlink to a file unless the user owns the target file or has write access. Prevents users from pinning sensitive files or tricking programs into modifying unauthorized files.
</details>

### Q5: How does Lynis compute its Hardening Index and how do you integrate it into CI/CD pipelines?
<details>
<summary>View Solution</summary>

Lynis evaluates hundreds of individual security tests categorized across subsystems. Each test carries an internal weight. Passed tests add positive points; failed tests or missing hardening baselines deduct points, resulting in a composite score from 0 to 100 stored in `/var/log/lynis-report.dat` under `hardening_index=<score>`.

**CI/CD Integration**:
When building golden VM images (via HashiCorp Packer):
```bash
sudo lynis audit system --quick
SCORE=$(grep "hardening_index=" /var/log/lynis-report.dat | cut -d= -f2)
if [ "${SCORE}" -lt 80 ]; then
    echo "Golden image failed hardening threshold! Score: ${SCORE}"
    exit 1
fi
```
</details>

### Q6: Why is `kernel.kptr_restrict = 2` recommended for production servers?
<details>
<summary>View Solution</summary>

Kernel pointers printed in `/proc/kallsyms`, `/proc/modules`, and system error logs reveal the exact memory addresses where kernel functions and drivers reside. Attackers use these addresses to bypass kernel ASLR (KASLR) during privilege escalation exploits.
- `kptr_restrict = 1`: Hides pointers from users without `CAP_SYSLOG`.
- `kptr_restrict = 2`: **Hides kernel pointers from ALL users**, replacing printed addresses with zeros (`0000000000000000`), regardless of administrative privileges.
</details>

### Q7: What are the risks of leaving development tools (`gcc`, `make`, `gdb`) on production servers?
<details>
<summary>View Solution</summary>

If an external attacker exploits a web vulnerability (e.g., PHP or Node.js remote execution) and lands a low-privileged shell (`www-data`), they typically download C source code for recent Linux kernel local privilege escalation (LPE) exploits (e.g. Dirty COW, Dirty Pipe). 
If a compiler (`gcc`) is installed on the machine, the attacker can compile and execute the root exploit locally in seconds. Removing compilers forces the attacker to cross-compile externally, significantly raising the barrier to entry and alerting security detection systems.
</details>

### Q8: What does the SUID bit do, and why should unneeded SUID binaries be stripped?
<details>
<summary>View Solution</summary>

When a binary has the **SUID (Set User ID)** permission bit set (`-rwsr-xr-x`), it executes with the privileges of the **file owner** (usually `root`), regardless of which unprivileged user runs it. Common examples: `passwd`, `sudo`, `ping`.
If a vulnerability (buffer overflow, path traversal, environment variable injection) exists in an SUID binary, an unprivileged user can exploit it to spawn an interactive root shell. Auditing and removing SUID from non-essential utilities (`chmod u-s <binary>`) directly eliminates local privilege escalation vectors.
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Auditing and Applying Secure Mount Options to `/tmp`
**Task**: Check your current `/tmp` mount options. If `/tmp` is not mounted as a separate partition or tmpfs with `noexec,nosuid,nodev`, create a temporary ramdisk mount and verify the security flags.
<details>
<summary>View Solution</summary>

```bash
# 1. Inspect current /tmp mount
mount | grep "/tmp"

# 2. Mount /tmp as secure tmpfs for testing
sudo mount -t tmpfs -o rw,nosuid,nodev,noexec,size=512M tmpfs /tmp

# 3. Test that execution is blocked:
cat << 'EOF' > /tmp/test_script.sh
#!/bin/sh
echo "Execution Allowed"
EOF
chmod +x /tmp/test_script.sh

# 4. Attempt to execute
/tmp/test_script.sh
# Expected output: /bin/sh: /tmp/test_script.sh: Permission denied

rm -f /tmp/test_script.sh
```
</details>

### Exercise 2: Auditing Active Kernel ASLR Status
**Task**: Query the current ASLR configuration. Write a short C program or python script printing memory addresses of variables across multiple runs to verify address randomization.
<details>
<summary>View Solution</summary>

```bash
# 1. Query sysctl
sysctl kernel.randomize_va_space
# Must return 2

# 2. Run Python snippet multiple times and observe stack address change
python3 -c "import sys; a=1; print(hex(id(a)))"
python3 -c "import sys; a=1; print(hex(id(a)))"
# Hex addresses should differ on every execution!
```
</details>

### Exercise 3: Finding and Cataloging SUID Binaries
**Task**: Scan the root filesystem for all binaries with the SUID bit enabled. Save the list to `/tmp/suid_baseline.txt`.
<details>
<summary>View Solution</summary>

```bash
sudo find / -xdev -perm -4000 -type f -exec ls -la {} + > /tmp/suid_baseline.txt
cat /tmp/suid_baseline.txt | awk '{print $NF}' | head -n 15
rm -f /tmp/suid_baseline.txt
```
</details>

### Exercise 4: Testing Kernel Module Blacklisting
**Task**: Verify that an unused protocol module (e.g. `dccp`) is blacklisted and cannot be loaded by `modprobe`.
<details>
<summary>View Solution</summary>

```bash
# 1. Attempt to load module
sudo modprobe dccp 2>/dev/null || true

# 2. Verify module is NOT present in loaded modules
lsmod | grep dccp
# Should return zero output!
```
</details>

### Exercise 5: Running a Lynis Security Audit
**Task**: Install and run a quick Lynis audit. Inspect the resulting Hardening Index and top suggestions.
<details>
<summary>View Solution</summary>

```bash
# Install lynis if missing (sudo apt install -y lynis)
if command -v lynis &>/dev/null; then
    sudo lynis audit system --quick
    grep -E "hardening_index|suggestion" /var/log/lynis-report.dat | head -n 15
else
    echo "Lynis not installed; test skipped."
fi
```
</details>

### Exercise 6: Hardening Process Core Dumps
**Task**: Verify that core dumps are disabled across the system via sysctl and pam limits.
<details>
<summary>View Solution</summary>

```bash
# 1. Check sysctl dumpable
sysctl fs.suid_dumpable
# Must return 0

# 2. Check shell core file size limit
ulimit -c
# Must return 0
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Operation | Command Example |
| :--- | :--- |
| **Run Lynis Audit** | `sudo lynis audit system` |
| **Apply Hardened Sysctl** | `sudo sysctl --system` |
| **Find All SUID Files** | `find / -xdev -perm -4000 -type f` |
| **Find World-Writable Files**| `find / -xdev -type f -perm -0002` |
| **Disable Unused Module** | `echo "install sctp /bin/true" \| sudo tee /etc/modprobe.d/sctp.conf` |
| **Audit Service Security** | `systemd-analyze security <service>` |
| **Verify ASLR Status** | `cat /proc/sys/kernel/randomize_va_space` |
| **Check Mount Flags** | `mount \| grep "/tmp"` |

### Further Reading
- [Center for Internet Security (CIS) Linux Benchmarks](https://www.cisecurity.org/cis-benchmarks/)
- [Lynis Official Documentation & GitHub](https://cisofy.com/lynis/)
- [DISA Security Technical Implementation Guides (STIGs)](https://public.cyber.mil/stigs/)
- [Kernel Self-Protection Project (KSPP)](https://kernsec.org/wiki/index.php/Kernel_Self_Protection_Project)
- [National Vulnerability Database (NVD)](https://nvd.nist.gov/)
