# Linux Security Modules (LSM), Capabilities & Seccomp

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Security Modules (LSM), POSIX Capabilities, Seccomp & Kernel Sandboxing |
| **Module** | `06-advanced` — Advanced Systems & Kernel Mechanics |
| **Track** | `04-devops/linux` |
| **Target OS** | Ubuntu 24.04 LTS (Kernel 6.8+), RHEL 9.4+ (Kernel 5.14+) |
| **Prerequisites** | `01-fundamentals/05-permissions.md`, `05-production/03-selinux-and-apparmor.md` |
| **Difficulty** | Advanced |
| **Last Updated** | 2026 |

---

## Table of Contents
1. [What & Why: Deconstructing the All-Powerful Root User](#what--why-deconstructing-the-all-powerful-root-user)
2. [Core Concepts & Architecture](#core-concepts--architecture)
3. [Internal Mechanics: Capability Sets & LSM Hook Interception](#internal-mechanics-capability-sets--lsm-hook-interception)
4. [Command Syntax & Capability Tools Reference](#command-syntax--capability-tools-reference)
5. [Progressive Practical Examples](#progressive-practical-examples)
   - [Example 1: Granting Port 80 Binding without Root Privileges](#example-1-granting-port-80-binding-without-root-privileges)
   - [Example 2: Inspecting and Dropping Capabilities with capsh](#example-2-inspecting-and-dropping-capabilities-with-capsh)
   - [Example 3: Auditing Host Binaries for Dangerous File Capabilities](#example-3-auditing-host-binaries-for-dangerous-file-capabilities)
   - [Example 4: Enforcing Syscall Filtering with Seccomp BPF](#example-4-enforcing-syscall-filtering-with-seccomp-bpf)
   - [Example 5: Automated Non-Root Microservice Sandbox Script](#example-5-automated-non-root-microservice-sandbox-script)
6. [Production Best Practices & Hardening](#production-best-practices--hardening)
7. [Common Failure Modes & Debugging](#common-failure-modes--debugging)
8. [Performance, Security & Scale Considerations](#performance-security--scale-considerations)
9. [Comparison Matrix: Linux Sandboxing Mechanisms](#comparison-matrix-linux-sandboxing-mechanisms)
10. [Real-World Interview Scenarios](#real-world-interview-scenarios)
11. [Hands-On Practice Labs](#hands-on-practice-labs)
12. [Cheat Sheet & Quick Reference](#cheat-sheet--quick-reference)

---

## What & Why: Deconstructing the All-Powerful Root User

In traditional UNIX permission models, privilege is binary: a process either runs with User ID 0 (`root`) and possesses unrestricted power over the entire machine, or it runs as an unprivileged user (UID > 0) and cannot bind to low-numbered network ports (< 1024), mount filesystems, or inspect other processes. This all-or-nothing approach is dangerous: if an unprivileged web server needs port 80, it historically had to start as `root`, meaning any remote code execution (RCE) vulnerability yielded complete host takeover.

Modern Linux decomposes the monolithic authority of root into distinct **POSIX Capabilities** (`CAP_*`). Furthermore, the **Linux Security Modules (LSM)** framework and **Seccomp (Secure Computing Mode)** allow security engineers to enforce Mandatory Access Control (MAC) policies, restrict which system calls a binary can execute, and sandbox microservices and containers according to the principle of least privilege.

---

## Core Concepts & Architecture

```
+-------------------------------------------------------------------------+
|                    Process Security Architecture                        |
|                                                                         |
|  +--------------------+                                                 |
|  | Application Binary | (e.g., /usr/bin/node running under non-root)    |
|  +--------------------+                                                 |
|             |                                                           |
|             v System Call: bind(fd, port 443)                           |
+-------------|-----------------------------------------------------------+
|  KERNEL GATE 1: Seccomp BPF Filter                                      |
|  - Checks syscall number: Is bind() allowed in whitelist?               |
|  - If blocked: Return -EPERM or send SIGSYS.                            |
|  - If allowed: Pass to DAC & Capabilities.                              |
+-------------|-----------------------------------------------------------+
|             v                                                           |
|  KERNEL GATE 2: Capabilities Check                                      |
|  - Is port < 1024? Yes -> Check task_struct->cap_effective:             |
|  - Does thread possess CAP_NET_BIND_SERVICE?                           |
|  - If missing: Return -EACCES.                                          |
+-------------|-----------------------------------------------------------+
|             v                                                           |
|  KERNEL GATE 3: LSM (Linux Security Modules) Hook                       |
|  - Calls security_socket_bind(sock, address, addrlen)                   |
|  - Enforces SELinux / AppArmor / Landlock / BPF-LSM policy:             |
|    "Does context httpd_t have permission to bind to http_port_t?"       |
|  - If denied: Write audit log entry and return -EACCES.                 |
+-------------|-----------------------------------------------------------+
|             v                                                           |
|  [ Execution Allowed: Socket bound successfully ]                       |
+-------------------------------------------------------------------------+
```

1. **Linux Capabilities**: Divides root power into ~40 discrete privileges (e.g., `CAP_NET_BIND_SERVICE`, `CAP_SYS_ADMIN`, `CAP_CHOWN`, `CAP_DAC_OVERRIDE`).
2. **Capability Sets**: Each thread has five capability bitmasks: **Permitted**, **Effective**, **Inheritable**, **Bounding**, and **Ambient**.
3. **Seccomp (Secure Computing Mode)**: A kernel facility using Berkeley Packet Filter (BPF) programs to intercept and filter system calls made by a process before execution.
4. **LSM Framework**: A hook-based framework in the kernel allowing security modules (SELinux, AppArmor, BPF-LSM, Landlock, Yama) to mediate access to core kernel objects (inodes, tasks, files, sockets).

---

## Internal Mechanics: Capability Sets & LSM Hook Interception

### 1. The Five Capability Sets
Every thread's `task_struct` contains five 64-bit capability bitmasks:
- **Effective (`CapEff`)**: The capabilities currently active and checked by kernel functions when privileged operations occur.
- **Permitted (`CapPrm`)**: The ceiling of capabilities the thread is allowed to assume. A thread can enable or disable capabilities in `CapEff` as long as they are present in `CapPrm`.
- **Inheritable (`CapInh`)**: Capabilities preserved across an `execve()` system call (rarely used without ambient capabilities).
- **Bounding Set (`CapBnd`)**: A system-wide or container-level security ceiling. No capability can be added to `CapPrm` across `execve` if it is absent from `CapBnd`.
- **Ambient (`CapAmb`)**: Introduced in Linux 4.3 to allow unprivileged parent processes to pass capabilities to unprivileged child binaries without setting file capabilities.

### 2. File Capabilities vs. Setuid Binaries
Historically, tools like `ping` used the `setuid-root` bit (`chmod 4755 /bin/ping`) to open raw ICMP sockets. This granted the entire binary full root privileges. Today, Linux uses extended file attributes (`security.capability`):
```bash
sudo setcap cap_net_raw+ep /bin/ping
```
This grants *only* `CAP_NET_RAW` to `ping`, rendering it incapable of modifying files, changing passwords, or escaping into root.

### 3. The `no_new_privs` Security Bit
Once a process sets the `PR_SET_NO_NEW_PRIVS` flag via `prctl(2)`:
- The process and all its children can **never** acquire new privileges.
- `setuid` bits and file capabilities on executed binaries are completely ignored.
- Seccomp filters can be safely loaded without requiring `CAP_SYS_ADMIN`.

---

## Command Syntax & Capability Tools Reference

| Command / Utility | Common Flags | Purpose |
| :--- | :--- | :--- |
| `getcap` | `-r <dir>`, `-v` | Recursively inspect file capabilities set on binaries. |
| `setcap` | `<caps>=<flags> <file>` | Assign capabilities to an ELF binary file (e.g., `cap_net_bind_service+ep`). |
| | `-r <file>` | Remove all capabilities from target file. |
| `capsh` | `--print` | Print the current process's capability sets and securebits. |
| | `--drop=<caps>` | Execute a program after permanently removing specified capabilities. |
| | `--caps="<caps>"` | Set permitted and effective capabilities before running command. |
| `getpcaps <pid>` | `<pid>` | Query the capability sets of an active running process. |

---

## Progressive Practical Examples

### Example 1: Granting Port 80 Binding without Root Privileges

Allow a custom Node.js, Python, or Go microservice to bind directly to privileged port 80/443 without running as root.

```bash
#!/usr/bin/env bash
# Step 1: Assign CAP_NET_BIND_SERVICE to binary
set -euo pipefail

APP_BIN="/usr/local/bin/microservice"

# Build dummy binary if not present
if [[ ! -f "${APP_BIN}" ]]; then
    sudo cp /usr/bin/python3 "${APP_BIN}"
fi

# 1. Assign effective (+e) and permitted (+p) capability
sudo setcap 'cap_net_bind_service=+ep' "${APP_BIN}"

# 2. Verify capability extended attribute
getcap "${APP_BIN}"
# Expected: /usr/local/bin/microservice cap_net_bind_service=ep

# 3. Test binding to port 80 as unprivileged user
sudo -u nobody "${APP_BIN}" -c "
import socket
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind(('0.0.0.0', 80))
s.listen(1)
print('Successfully bound to port 80 as unprivileged user nobody!')
"
```

### Example 2: Inspecting and Dropping Capabilities with capsh

Inspect running capability bitmasks and run a shell with dangerous capabilities stripped.

```bash
#!/usr/bin/env bash
# Step 2: Use capsh to inspect and drop capabilities
set -euo pipefail

echo "=== Current Shell Capabilities ==="
capsh --print

echo "=== Dropping CAP_SYS_ADMIN and CAP_NET_RAW ==="
# Launch subshell without sys_admin or raw networking
capsh --drop=cap_sys_admin,cap_net_raw -- -c "
  echo 'Current capabilities inside sandboxed subshell:'
  capsh --print | grep Current
"
```

### Example 3: Auditing Host Binaries for Dangerous File Capabilities

Scan the operating system for binaries possessing dangerous capabilities that could allow privilege escalation.

```bash
#!/usr/bin/env bash
# Step 3: Audit all binaries with capabilities on the host
set -euo pipefail

echo "Scanning filesystem for binaries with file capabilities..."
# Scan standard binary directories
sudo getcap -r /bin /sbin /usr/bin /usr/sbin /usr/local/bin 2>/dev/null

# Identify high-risk capabilities: CAP_SYS_ADMIN, CAP_DAC_OVERRIDE, CAP_SETUID
echo "=== Checking for High-Risk Escalation Capabilities ==="
sudo getcap -r / 2>/dev/null | grep -E "cap_sys_admin|cap_dac_override|cap_setuid|cap_setgid" || echo "No high-risk capabilities found."
```

### Example 4: Enforcing Syscall Filtering with Seccomp BPF

Demonstrate how a process can lock itself down using Seccomp to reject dangerous system calls.

```bash
#!/usr/bin/env bash
# Step 4: Run a command under strict Seccomp filtering
set -euo pipefail

# In modern Linux systems, systemd provides native Seccomp sandboxing for services
echo "Demonstrating systemd-run with Seccomp SystemCallFilter..."

# Run a transient process that is blocked from calling ptrace or reboot
sudo systemd-run --wait --pipe \
  -p SystemCallFilter=~@mount \
  -p SystemCallFilter=~@reboot \
  -p SystemCallFilter=~@privileged \
  /bin/bash -c "
    echo 'Running under systemd Seccomp filter!'
    # Attempting to mount will trigger EPERM / Bad System Call
    mount -t tmpfs none /mnt 2>&1 || echo 'Mount successfully blocked by Seccomp!'
  "
```

### Example 5: Automated Non-Root Microservice Sandbox Script

A production hardening wrapper that executes a backend daemon under an unprivileged user, strips all capabilities except required network bindings, locks down memory, and activates `no_new_privs`.

```bash
#!/usr/bin/env bash
# Step 5: Enterprise sandbox executor for application services
set -euo pipefail

TARGET_USER="apprunner"
APP_CMD="/usr/bin/python3 -m http.server 80"

# 1. Create unprivileged application user if not present
if ! id "${TARGET_USER}" >/dev/null 2>&1; then
    sudo useradd -r -s /usr/sbin/nologin -d /var/empty "${TARGET_USER}"
fi

echo "=================================================="
echo "    INITIALIZING PRIVILEGE-SEPARATED SANDBOX"
echo "    User: ${TARGET_USER} | Command: ${APP_CMD}"
echo "=================================================="

# 2. Use systemd-run to launch with kernel capabilities and seccomp enforcement
sudo systemd-run \
  --unit="microservice-sandbox-$$" \
  --wait --pipe \
  -p User="${TARGET_USER}" \
  -p AmbientCapabilities="CAP_NET_BIND_SERVICE" \
  -p CapabilityBoundingSet="CAP_NET_BIND_SERVICE" \
  -p NoNewPrivileges=true \
  -p ProtectSystem=strict \
  -p ProtectHome=true \
  -p PrivateTmp=true \
  -p PrivateDevices=true \
  -p MemoryMax=256M \
  -p CPUQuota=50% \
  ${APP_CMD} &

SANDBOX_PID=$!
sleep 2

echo "Sandbox running. Verifying capabilities of spawned process:"
TARGET_PID=$(pgrep -u "${TARGET_USER}" -f "http.server" | head -n 1 || true)

if [[ -n "${TARGET_PID}" ]]; then
    getpcaps "${TARGET_PID}"
fi

# Cleanup
sudo kill "${SANDBOX_PID}" || true
```

---

## Production Best Practices & Hardening

- **Never grant `CAP_SYS_ADMIN`**: Often called "the new root." `CAP_SYS_ADMIN` contains dozens of unrestricted privileges (mounting, namespaces, eBPF, device configuration). Granting it to a container or binary is equivalent to granting full root.
- **Beware of `CAP_DAC_OVERRIDE`**: Bypasses all standard Unix read/write/execute permission checks on any file. A process with this capability can overwrite `/etc/shadow` or `/etc/sudoers`.
- **Enforce `NoNewPrivileges=true` in systemd units**: Prevents child processes from escalating privileges via `su`, `sudo`, or setuid binaries.
- **Use Ambient Capabilities for Daemons**: Instead of setting permanent file capabilities (`setcap`) on binaries on disk (which can be executed by anyone on the host), use systemd's `AmbientCapabilities=` to grant capabilities dynamically at launch time strictly to that service.
- **Enable Seccomp Default Profiles in Containers**: Always ensure Docker or Kubernetes uses the default Seccomp profile (blocking ~44 dangerous syscalls out of ~350, including `clone3`, `kexec_load`, `keyctl`, and `reboot`).

---

## Common Failure Modes & Debugging

| Symptom / Error | Root Cause | Diagnosis Command | Resolution |
| :--- | :--- | :--- | :--- |
| `Permission denied` when binding to port 80 | Missing `CAP_NET_BIND_SERVICE` or process lacks ambient capabilities. | `getpcaps <pid>` | Set `AmbientCapabilities=CAP_NET_BIND_SERVICE` in systemd service or use `setcap`. |
| `Bad system call (core dumped)` | Process invoked a system call forbidden by the active Seccomp profile. | `journalctl -xe \| grep -i audit` | Inspect `SECCOMP` audit log; add required syscall to whitelist. |
| Capabilities lost after binary update / package upgrade | Package managers (`apt`, `dnf`) recreate binary files, wiping extended attributes. | `getcap <binary>` | Use systemd `AmbientCapabilities=` instead of disk file capabilities. |
| `setcap: Failed to set capabilities on file: Operation not permitted` | Filesystem mounted with `nosuid` or lacking extended attribute (`xattr`) support. | `findmnt -T <binary>` | Ensure mount options include `user_xattr` and do not include `nosuid`. |
| Setuid binary behaves as normal binary without root | Target binary executed from a partition mounted with `nosuid` or `no_new_privs` set. | `cat /proc/<pid>/status \| grep NoNewPrivs` | Check filesystem mount flags in `/etc/fstab`. |

---

## Performance, Security & Scale Considerations

```
+--------------------------------------------------------------------------+
| Privilege Containment Hierarchy                                          |
|                                                                          |
| 1. Monolithic Root (Legacy):                                             |
|    RCE -> Full Kernel & Host Compromise (Access to all disks, memory)    |
|                                                                          |
| 2. POSIX Capabilities (Least Privilege):                                 |
|    RCE -> Restricted strictly to defined domain (e.g., CAP_NET_RAW only) |
|                                                                          |
| 3. Capabilities + Seccomp BPF + LSM (Defense in Depth):                  |
|    RCE -> Cannot execute unapproved syscalls; blocked by SELinux/AppArmor|
+--------------------------------------------------------------------------+
```

1. **Seccomp BPF Overhead**: Seccomp filters are executed for *every* system call made by a sandboxed process. A poorly structured Seccomp BPF program with dozens of linear comparisons can slow down high-throughput syscall loops. Modern kernels optimize this with BPF bitmap jumps.
2. **File Capabilities vs Container Images**: Extended attributes (`xattr`) used by `setcap` can sometimes fail to serialize properly across older container image tarball layers. Standardize on ambient capabilities in OCI specs.

---

## Comparison Matrix: Linux Sandboxing Mechanisms

| Mechanism | Scope / Domain | Enforcement Level | Config Tooling | Production Overhead |
| :--- | :--- | :--- | :--- | :--- |
| **Linux Capabilities** | POSIX root operations | Kernel permissions | `setcap`, `capsh` | Zero |
| **Seccomp BPF** | System call numbers & args| Kernel syscall trap | systemd, OCI JSON | ~1% per syscall |
| **AppArmor** | File paths, networking, caps| Mandatory Access Control| `/etc/apparmor.d` | Negligible |
| **SELinux** | Inode types & security contexts| Type Enforcement / MLS | `semanage`, `setsebool`| ~2% - 5% |
| **Landlock LSM** | Unprivileged path sandboxing| Kernel filesystem sandbox| Native C / Rust APIs | Negligible |

---

## Real-World Interview Scenarios

### Scenario 1: Why is running an application with `setuid-root` considered far more hazardous than using Linux Capabilities?
<details>
<summary>Detailed Answer</summary>
A `setuid-root` binary (`chmod 4755`) elevates the effective UID to 0 upon execution, granting the binary complete, unconstrained administrative authority over the entire operating system. If the binary contains a buffer overflow, format string vulnerability, or command injection, an attacker achieves complete root compromise. Linux Capabilities split root's authority into ~40 discrete privileges. Assigning a specific capability (e.g., `CAP_NET_BIND_SERVICE`) grants only the specific privilege needed, ensuring that an attacker cannot alter files, inspect other memory, or reboot the host.
</details>

### Scenario 2: What is the purpose of the Ambient Capability set introduced in Linux 4.3?
<details>
<summary>Detailed Answer</summary>
Before Ambient capabilities, an unprivileged process (UID > 0) could not execute a non-privileged binary and pass its capabilities down without setting file capabilities (`setcap`) on the target binary file itself. The Ambient capability set solves this: capabilities marked in the ambient set are automatically preserved and copied into the permitted and effective sets across `execve()` calls by unprivileged processes, provided the binary does not have setuid or file capabilities configured.
</details>

### Scenario 3: How does `no_new_privs` prevent privilege escalation?
<details>
<summary>Detailed Answer</summary>
When a process sets `PR_SET_NO_NEW_PRIVS` via `prctl()`, the kernel guarantees that neither the process nor any of its descendants can ever acquire privileges greater than they had when the flag was set. Crucially, executing binaries with `setuid`, `setgid`, or file capabilities will not elevate privileges; the kernel executes them with the caller's existing rights. This prevents attackers from escaping an unprivileged sandbox by invoking `sudo` or `pkexec`.
</details>

### Scenario 4: What is Seccomp-BPF and what happens when a blocked system call is made?
<details>
<summary>Detailed Answer</summary>
Seccomp-BPF (Secure Computing with Berkeley Packet Filter) allows a process to attach a BPF filter program that is executed on every system call attempt. Depending on the filter configuration, when a prohibited system call is invoked, Seccomp can:
1. `SECCOMP_RET_KILL_PROCESS`: Instantly terminate the entire process.
2. `SECCOMP_RET_TRAP`: Send a `SIGSYS` signal to the process for in-app handling.
3. `SECCOMP_RET_ERRNO`: Return an immediate error code (e.g., `-EPERM` or `-ENOSYS`) without executing the call.
4. `SECCOMP_RET_LOG`: Log the violation to auditd and allow the call (useful for dry-run auditing).
</details>

### Scenario 5: Which capability allows a process to bypass all filesystem read and write permissions?
<details>
<summary>Detailed Answer</summary>
`CAP_DAC_OVERRIDE` (Discretionary Access Control Override). A process with this capability completely bypasses standard Linux file read, write, and execute permission checks. It can read `/etc/shadow`, write directly to `/etc/passwd` or `/etc/sudoers`, and overwrite sensitive system binaries, effectively making it equivalent to full root access.
</details>

### Scenario 6: How do you identify which capabilities are held by a currently running daemon?
<details>
<summary>Detailed Answer</summary>
By inspecting `/proc/<pid>/status`. Look at the fields `CapInh`, `CapPrm`, `CapEff`, `CapBnd`, and `CapAmb`. These are displayed as 64-bit hexadecimal bitmasks. You can decode them into human-readable capability names using:
`capsh --decode=<hex_mask>` or using the utility `getpcaps <pid>`.
</details>

### Scenario 7: Why do package managers wipe out file capabilities set via `setcap`?
<details>
<summary>Detailed Answer</summary>
File capabilities are stored as extended attributes (`xattr`) in the filesystem inode (`security.capability`). When a package manager (`apt`, `rpm`, `dnf`) updates an application package, it typically creates a new file, writes the updated binary, and replaces the old file via an atomic `rename()` system call. This creates a brand-new inode, wiping out the extended attributes of the previous version. To maintain capabilities across updates, use systemd unit drop-ins with `AmbientCapabilities=`.
</details>

### Scenario 8: What is BPF-LSM and how does it improve upon traditional LSMs?
<details>
<summary>Detailed Answer</summary>
BPF-LSM (introduced in Linux 5.7) combines the Linux Security Modules framework with eBPF. Instead of writing complex, static SELinux policies or kernel modules, security engineers can write dynamic eBPF programs that attach directly to LSM hooks (such as `bprm_check_security`, `file_open`, or `socket_connect`). This enables real-time, programmable security enforcement with direct access to rich kernel context and negligible overhead.
</details>

---

## Hands-On Practice Labs

### Lab 1: Inspect Shell Capabilities and Decode Bitmasks
<details>
<summary>Lab Instructions & Verification</summary>

1. Inspect the capability status of your current shell process:
   ```bash
   grep Cap /proc/$$/status
   ```
2. Decode the effective capability bitmask:
   ```bash
   capsh --decode=$(grep CapEff /proc/$$/status | awk '{print $2}')
   ```
</details>

### Lab 2: Set and Verify File Capabilities on a Binary
<details>
<summary>Lab Instructions & Verification</summary>

1. Copy the `cat` binary to `/tmp/mycat`:
   ```bash
   cp /bin/cat /tmp/mycat
   ```
2. Assign `CAP_DAC_READ_SEARCH` (allows reading any file bypassing read checks):
   ```bash
   sudo setcap cap_dac_read_search+ep /tmp/mycat
   ```
3. Verify capability assignment:
   ```bash
   getcap /tmp/mycat
   ```
4. Read a file normally restricted to root (e.g., `/etc/shadow`):
   ```bash
   /tmp/mycat /etc/shadow | head -n 3
   ```
5. Clean up:
   ```bash
   rm -f /tmp/mycat
   ```
</details>

### Lab 3: Strip All Capabilities Using capsh
<details>
<summary>Lab Instructions & Verification</summary>

1. Launch a subshell where all capabilities in the bounding set are dropped:
   ```bash
   sudo capsh --drop=all -- -c "capsh --print"
   ```
2. Verify that `Current:` displays `empty` and `Bounding set:` is empty.
</details>

### Lab 4: Audit SUID Binaries and Compare with Capabilities
<details>
<summary>Lab Instructions & Verification</summary>

1. Find all setuid-root binaries on the host:
   ```bash
   find /usr/bin -perm -4000 -user root 2>/dev/null | head -n 15
   ```
2. Compare with binaries utilizing modern Linux capabilities:
   ```bash
   getcap /usr/bin/* 2>/dev/null
   ```
</details>

### Lab 5: Test NoNewPrivileges Protection
<details>
<summary>Lab Instructions & Verification</summary>

1. Execute a command with `no_new_privs` active and attempt to use `sudo`:
   ```bash
   systemd-run --user --wait --pipe -p NoNewPrivileges=true sudo whoami || echo "Privilege escalation blocked as expected!"
   ```
</details>

### Lab 6: Query Active Linux Security Modules (LSM)
<details>
<summary>Lab Instructions & Verification</summary>

1. Check which security modules are active and in what order:
   ```bash
   cat /sys/kernel/security/lsm
   ```
2. Verify if AppArmor, SELinux, Yama, or BPF are enabled.
</details>

---

## Cheat Sheet & Quick Reference

```bash
# === Capability Inspection & Decoding ===
getcap /path/to/binary                         # Query file capabilities
getpcaps <pid>                                 # Query capabilities of running process
capsh --print                                  # Print current thread capabilities
capsh --decode=0000000000000400                # Decode hexadecimal mask to name

# === Setting File Capabilities ===
sudo setcap 'cap_net_bind_service=+ep' <file>  # Allow non-root port binding (<1024)
sudo setcap 'cap_net_raw,cap_net_admin=+ep' <f># Allow raw sockets & network config
sudo setcap -r <file>                          # Remove all capabilities from file

# === systemd Unit Sandboxing Directives ===
User=apprunner
AmbientCapabilities=CAP_NET_BIND_SERVICE
CapabilityBoundingSet=CAP_NET_BIND_SERVICE
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
SystemCallFilter=@system-service
```

---
*Next Note: [08-systemtap-ftrace-overview.md](file:///04-devops/linux/06-advanced/08-systemtap-ftrace-overview.md)*
