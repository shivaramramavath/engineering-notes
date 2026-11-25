<!--
================================================================================
Metadata:
  Title: Sudo, Privilege Escalation & Least Privilege Engineering
  Track: Linux & Systems Engineering / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 01-fundamentals/07-permissions.md
  Key Terminology: sudo, visudo, /etc/sudoers, /etc/sudoers.d, su, NOPASSWD, env_reset, sudo-rs, Shell Escapes
================================================================================
-->

# Sudo, Privilege Escalation & Least Privilege Engineering

[<- 07-permissions.md](./07-permissions.md) | [Back to Module Overview](./README.md) | [09-man-pages-and-getting-help.md ->](./09-man-pages-and-getting-help.md)

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

In enterprise Linux administration, direct interactive logins as the superuser (`root`) are strictly forbidden across production environments. Operating continuously as root violates the **Principle of Least Privilege (PoLP)**, eliminates individual operator auditability, and bypasses safety guardrails against accidental commands (such as running `rm -rf /` or unmounting active storage volumes).

Instead, Linux infrastructure relies on **`sudo`** (SuperUser DO). Sudo allows a system administrator to delegate granular administrative authority to specific users or groups on a per-command basis, requiring operators to authenticate with their own credentials while logging every executed command and argument to centralized audit logs.

For DevOps engineers and SREs:
- Automated CI/CD deployment agents (GitHub Actions runners, GitLab runners, Ansible) require carefully scoped, passwordless `sudo` rules (`NOPASSWD`) to restart services or reload configurations without granting full root shell access.
- Insecure sudoers specifications (such as granting sudo rights to binaries with built-in shell escapes like `less`, `vim`, or `find`) provide immediate, trivial pathways to complete root compromise.
- Modern memory-safe implementations, such as **`sudo-rs`** (written in Rust and adopted by modern distributions), eliminate legacy C buffer overflow vulnerabilities.

---

## 2. Core Concepts

### `sudo` vs. `su -`
Understanding the fundamental technical difference between `sudo` and `su -` is essential:

```
+--------------------------------------------------------------------------+
| ELEVATION MECHANISM COMPARISON                                           |
|                                                                          |
| 1. The 'su -' Mechanism (Substitute User)                                |
|    - Requires the ROOT password.                                         |
|    - Spawns an interactive login root shell.                             |
|    - Completely obscures which human operator is executing commands.     |
|    - Violates enterprise compliance (SOC2, PCI-DSS).                     |
|                                                                          |
| 2. The 'sudo' Mechanism (SuperUser DO)                                   |
|    - Requires the USER'S personal password (or SSH key / agent).         |
|    - Executes ONLY the explicitly authorized binary.                     |
|    - Preserves audit trails: records human username, TTY, PID, and       |
|      exact command string in /var/log/auth.log or systemd journal.       |
|    - Enforces granular command whitelisting.                             |
+--------------------------------------------------------------------------+
```

### The Grammar of `/etc/sudoers`
The `/etc/sudoers` configuration file follows a strict formal grammar:

```text
who    where = (as_whom:as_which_group) [TAGS:] what
---    -----   ------------------------ ------- ----
ubuntu ALL   = (ALL:ALL)                ALL
%devops prod = (root)                   NOPASSWD: /usr/bin/systemctl restart nginx
```

- **`who`**: A specific username (`ubuntu`) or a group prefixed by `%` (`%devops` or `%sudo`).
- **`where`**: The hostnames where this rule applies (`ALL`, or specific host alias).
- **`(as_whom:as_which_group)`**: The target user and group the command runs as (defaults to `(root)` if omitted).
- **`[TAGS:]`**: Optional execution modifiers:
  - `NOPASSWD:` Run without prompting for a password.
  - `PASSWD:` Enforce password prompt (default).
  - `NOEXEC:` Prevent executed binary from spawning child subshells.
- **`what`**: Comma-separated list of absolute paths to permitted binaries, optionally with exact argument constraints.

---

## 3. How It Works Internally

### Sudo Execution Flow and Environment Sanitization
When an engineer runs `sudo /usr/bin/systemctl restart nginx`, a complex security negotiation occurs in the kernel and PAM stack:

```
[ Unprivileged User (UID 1000) runs: sudo systemctl restart nginx ]
                           |
                           v
1. Sudo Binary Starts: /usr/bin/sudo has SUID bit set (mode 4755, owned by root)
   -> Kernel elevates Effective UID (euid) to 0 (root).
                           |
                           v
2. Security & Environment Sanitization:
   -> Enforces 'env_reset': wipes dangerous environment variables
      (LD_PRELOAD, LD_LIBRARY_PATH, IFS, PATH) to prevent shared library hijacking.
                           |
                           v
3. Sudoers Parsing & Policy Evaluation:
   -> Parses /etc/sudoers and all drop-ins inside /etc/sudoers.d/
   -> Validates user identity, host, target binary path, and argument constraints.
                           |
                           v
4. Authentication via PAM (/etc/pam.d/sudo):
   -> Checks timestamp cache (/run/sudo/ts/<user>).
   -> If cached token valid (< 15 mins): skips password prompt.
   -> If expired: prompts for caller's password, validates against /etc/shadow.
                           |
                           v
5. Audit Logging:
   -> Writes structured log entry to syslog/journald:
      "sudo: ubuntu : TTY=pts/0 ; PWD=/home/ubuntu ; USER=root ; COMMAND=/usr/bin/systemctl restart nginx"
                           |
                           v
6. Execution:
   -> Calls setresuid(0, 0, 0) and setresgid(0, 0, 0) to permanently become root.
   -> Calls execve("/usr/bin/systemctl", args, sanitized_env).
```

---

## 4. Syntax & Usage

### The `visudo` Command
**Never edit `/etc/sudoers` with standard editors (`nano`, `vim`) directly!** A single syntax typo in `/etc/sudoers` can instantly break `sudo` across the entire operating system, permanently locking administrators out of root access.

```bash
# Safely edit the main sudoers configuration (validates syntax before saving)
sudo visudo

# Safely edit or create a modular drop-in file in /etc/sudoers.d/
sudo visudo -f /etc/sudoers.d/99-deploy-runner

# Check existing sudoers configuration files for syntax errors without opening an editor
sudo visudo -c
# Output: /etc/sudoers: parsed OK
#         /etc/sudoers.d/99-deploy-runner: parsed OK
```

### Sudo CLI Flags
```bash
# Execute command with root privileges
sudo apt update

# Execute command as a specific alternate user (e.g., postgres)
sudo -u postgres psql

# Execute command as a specific user and group
sudo -u apprunner -g webapps /opt/app/bin/start

# List all sudo privileges granted to the current user
sudo -l

# Invalidate the cached sudo password token immediately (lock sudo)
sudo -k

# Preserve user environment variables (e.g., HTTP_PROXY, EDITOR)
sudo -E vim /etc/hosts
```

---

## 5. Practical Examples

### Example 1: Scoping Sudo Privileges for a CI/CD Deployment Runner
```bash
# Requirement: A service user 'github-runner' needs passwordless privilege to
# reload Nginx and restart the API systemd service, but NOTHING ELSE.

# Use visudo to create a secure drop-in configuration
sudo visudo -f /etc/sudoers.d/20-ci-runner

# Add the following lines inside the file:
# Define command aliases for clarity
Cmnd_Alias APP_SERVICE = /usr/bin/systemctl restart api.service, /usr/bin/systemctl status api.service
Cmnd_Alias WEB_RELOAD  = /usr/bin/systemctl reload nginx, /usr/bin/nginx -t

# Grant scoped passwordless execution
github-runner ALL=(root) NOPASSWD: APP_SERVICE, WEB_RELOAD

# Verify syntax
sudo visudo -c
```

### Example 2: Inspecting Sudo Audit Logs
```bash
# View all recent sudo invocations using journalctl
journalctl _COMM=sudo -n 20 --no-pager

# Sample Output:
# Nov 25 12:00:15 dev-srv sudo[4120]:   ubuntu : TTY=pts/0 ; PWD=/home/ubuntu ; USER=root ; COMMAND=/usr/bin/apt update
# Nov 25 12:01:22 dev-srv sudo[4188]:   john : user NOT in sudoers ; TTY=pts/1 ; PWD=/home/john ; USER=root ; COMMAND=/bin/cat /etc/shadow

# Filter specifically for unauthorized elevation attempts
journalctl _COMM=sudo | grep "user NOT in sudoers"
```

### Example 3: The Danger of Shell Escapes (The `less` Anti-Pattern)
```bash
# BAD CONFIGURATION IN SUDOERS:
# developer ALL=(ALL) NOPASSWD: /usr/bin/less /var/log/app.log

# HOW AN ATTACKER ESCAPES TO A ROOT SHELL:
# 1. Developer runs the authorized sudo command:
#    sudo less /var/log/app.log
# 2. Inside the less pager, the user types:
#    !/bin/bash
# 3. 'less' forks and executes /bin/bash inheriting effective UID 0!
# 4. Result: The user has an interactive root shell, bypassing all restrictions!

# REMEDIATION: Use the NOEXEC tag to disable subshell spawning
# developer ALL=(ALL) NOEXEC: NOPASSWD: /usr/bin/less /var/log/app.log
```

### Example 4: Restricting Argument Wildcards Safely
```bash
# VULNERABLE CONFIGURATION:
# operator ALL=(root) NOPASSWD: /bin/cat /var/log/*
# An attacker runs:
# sudo cat /var/log/../../etc/shadow
# Result: Reads root password hashes!

# HARDENED REMEDIATION:
# Write a dedicated shell script with strict input validation,
# store it in /usr/local/bin/view-logs.sh (owned by root, mode 0700),
# and grant sudo access ONLY to that script.
sudo tee /usr/local/bin/view-logs.sh << 'EOF'
#!/usr/bin/env bash
set -euo pipefail
FILE="$1"
# Ensure target path starts strictly within /var/log and contains no traversal dots
REAL_PATH=$(realpath "/var/log/$FILE")
if [[ "$REAL_PATH" =~ ^/var/log/ && ! "$REAL_PATH" =~ \.\. ]]; then
    /bin/cat "$REAL_PATH"
else
    echo "ERROR: Path traversal detected or unauthorized path!" >&2
    exit 1
fi
EOF
sudo chmod 0750 /usr/local/bin/view-logs.sh
sudo chown root:admin /usr/local/bin/view-logs.sh

# Sudoers rule:
# %admin ALL=(root) NOPASSWD: /usr/local/bin/view-logs.sh *
```

### Example 5: Implementing `sudo-rs` (Memory-Safe Sudo) on Ubuntu 24.04
```bash
# Check if modern Rust-based sudo-rs is installed or available
which sudo-rs 2>/dev/null || echo "Standard sudo active"

# In distributions packaging sudo-rs (Prossimo project), verify version:
sudo --version
# Sudo version 1.9.15p5 (or sudo-rs v0.2.x)

# Sudoers drop-in files conform to standard syntax, guaranteeing 100% compatibility
```

---

## 6. Best Practices

- **Always Use `visudo`**: Never open `/etc/sudoers` or `/etc/sudoers.d/*` directly in a text editor without syntax checking. `visudo` locks the file against concurrent edits and parses the grammar before writing to disk.
- **Use Drop-In Files (`/etc/sudoers.d/`)**: Keep the main `/etc/sudoers` clean and package-managed. Create isolated, purpose-built drop-in files (e.g., `/etc/sudoers.d/20-ansible`, `/etc/sudoers.d/30-developers`) with permissions strictly set to `0440`.
- **Drop-In File Naming Rule**: File names inside `/etc/sudoers.d/` must **NOT** contain a dot (`.`) or end with a tilde (`~`). Files like `myuser.conf` or `backup~` are silently ignored by `sudo`.
- **Never Grant Sudo to Interpreters or Editors**: Never grant sudo rights to `bash`, `sh`, `python`, `perl`, `vim`, `nano`, or `find`. These programs allow users to execute arbitrary shell commands or read/write arbitrary files, converting partial privilege into unconditional root access.
- **Always Specify Absolute Binary Paths**: In sudoers rules, write `/usr/bin/systemctl`, never `systemctl`. Relative paths allow PATH-hijacking exploits.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| Editing `/etc/sudoers` with `nano` and introducing a syntax error | Sudo fails immediately with `syntax error in /etc/sudoers`; no user can run sudo to repair it. | Boot into recovery mode, or use single-user root shell to fix with `visudo`. |
| Sudoers drop-in file `devops.conf` is ignored | Sudo explicitly ignores any file in `/etc/sudoers.d/` containing a period (`.`) to ignore backup files. | Rename the file to omit the extension: `/etc/sudoers.d/devops`. |
| Sudoers drop-in file fails with `sudo: /etc/sudoers.d/test is world writable` | Sudo enforces strict permission checks; any drop-in with mode other than `0440` or `0400` is rejected. | Fix permissions: `sudo chmod 0440 /etc/sudoers.d/test`. |
| `sudo echo "text" > /etc/privileged.conf` fails with `Permission denied` | The shell handles redirection (`>`) in user space *before* sudo executes; the shell process lacks write permission. | Use tee: `echo "text" \| sudo tee /etc/privileged.conf > /dev/null`. |
| Environment variables (e.g. `AWS_PROFILE`) vanish under `sudo` | Sudo enforces `env_reset` by default for security, stripping all non-whitelisted environment variables. | Pass the variable explicitly (`sudo AWS_PROFILE=prod aws ...`) or configure `env_keep`. |
| Sudo rule for command with arguments bypassed by passing flags | Rule `/usr/bin/systemctl restart nginx` can be bypassed if user passes flags like `--no-block` unless strict match is configured. | Test argument matching rules thoroughly or wrap in a validation script. |

---

## 8. Performance, Security & Cost Implications

- **Performance**: Sudo incurs minimal CPU overhead (~10–15 milliseconds per invocation) primarily driven by PAM authentication modules, parsing `/etc/sudoers`, and appending to disk logs. In high-frequency automated loops, avoid running `sudo` 10,000 times inside a loop; elevate the script once.
- **Security**: The `/etc/sudoers` configuration represents the primary privilege boundary on Linux. Poorly configured sudo rules (granting `tar`, `zip`, `awk`, or `less`) represent the top finding on external penetration tests, enabling immediate root escalation via GTFOBins.
- **Cost**: Comprehensive sudo audit logging provides non-repudiation for financial compliance audits (PCI-DSS, SOC2). Lacking granular sudo controls can lead to costly compliance failure penalties.

---

## 9. Comparison Table

| Feature / Trait | `sudo <command>` | `su -` (Switch User) | `sudo -i` / `sudo -s` | `pkexec` (Polkit) |
| :--- | :--- | :--- | :--- | :--- |
| **Password Required** | Caller's user password | Target user's (root) password | Caller's user password | Caller's user password |
| **Session Lifetime** | Single command execution | Persistent interactive shell | Persistent interactive shell | Single command execution |
| **Command Granularity**| Granular (whitelisted binaries)| None (full root shell) | None (full root shell) | Granular (D-Bus policy rules) |
| **Audit Logging** | Detailed (commands & args) | Minimal (session open/close) | Logs shell launch only | Detailed D-Bus events |
| **Compliance Readiness**| Fully SOC2/PCI-DSS compliant | Non-compliant (shared password)| Generally discouraged | Compliant |
| **Primary Domain** | Automation, CLI servers | Legacy sysadmin login | Interactive maintenance | GUI desktop / systemd D-Bus |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Why is running `sudo echo "payload" >> /etc/hosts` rejected with "Permission denied", and how do you fix it?</strong></summary>

**Answer**:
When you execute `sudo echo "payload" >> /etc/hosts`, the command line is evaluated in two phases:
1. **Redirection Phase**: The active, unprivileged user shell processes the output redirection operator (`>>`) *first*. The shell attempts to open `/etc/hosts` in append mode using the unprivileged user's credentials (UID 1000). Because `/etc/hosts` requires root write permissions, the shell immediately emits `Permission denied` and aborts.
2. **Execution Phase**: The `sudo` command is never even executed.

To resolve this, the process performing the write must run with elevated privileges. The standard solutions are:
- Using `tee`:
  ```bash
  echo "payload" | sudo tee -a /etc/hosts > /dev/null
  ```
- Spawning a subshell with sudo:
  ```bash
  sudo sh -c 'echo "payload" >> /etc/hosts'
  ```
</details>

<details>
<summary><strong>2. What is `visudo` and what specific failure mode does it protect against?</strong></summary>

**Answer**:
`visudo` is the dedicated administrative utility used to edit `/etc/sudoers` and drop-ins in `/etc/sudoers.d/`.

It protects against:
1. **Syntax Errors**: `visudo` edits a temporary working file (e.g., `/etc/sudoers.tmp`). When you save and exit, it parses the configuration using the internal sudoers grammar compiler. If a syntax error exists, it refuses to commit the changes to disk, prompting the operator to re-edit (`e`) or quit without saving (`x`). Without this, a single missing colon or typo renders `sudo` completely unusable, locking administrators out of root access on machines with disabled root passwords.
2. **Concurrent Edits**: `visudo` enforces an advisory lockfile. If two administrators attempt to edit the sudoers file simultaneously, the second invocation is blocked with a message that the file is busy, preventing race-condition overwrites.
</details>

<details>
<summary><strong>3. What are "GTFOBins" and why is granting sudo access to `find` or `vim` dangerous?</strong></summary>

**Answer**:
GTFOBins is a curated catalog of standard Unix binaries that can be exploited by an attacker to bypass security restrictions or escalate privileges if granted elevated execution rights (`sudo` or SUID).

Many standard utilities have built-in shell escape mechanisms or arbitrary file read/write functionality:
- **`find`**: Can execute arbitrary commands via the `-exec` flag:
  `sudo find . -exec /bin/sh \; -quit` (spawns an immediate root shell).
- **`vim`**: Can execute external shell commands from within its command buffer:
  `sudo vim -c ':!/bin/sh'` (spawns an immediate root shell).
- **`awk`**: Can invoke system commands:
  `sudo awk 'BEGIN {system("/bin/sh")}'`.

Granting sudo access to any binary capable of executing arbitrary commands is mathematically equivalent to granting full, unrestricted root access (`ALL`).
</details>

<details>
<summary><strong>4. What is the significance of the `NOPASSWD:` tag in `/etc/sudoers` and how should it be secured?</strong></summary>

**Answer**:
`NOPASSWD:` instructs sudo to execute the specified command without prompting the calling user for a password. It is commonly required for automated service accounts (such as CI/CD deployment runners, monitoring agents, or backup daemons) that run non-interactively in headless environments.

To secure `NOPASSWD:` rules:
1. **Never use wildcards on binaries**: Do not write `NOPASSWD: ALL` or `NOPASSWD: /usr/bin/*`.
2. **Restrict to specific, hardened binaries**: Only grant access to tightly constrained commands (e.g., `/usr/bin/systemctl restart my-app`).
3. **Prevent parameter injection**: Ensure the authorized binary cannot take arbitrary arguments that allow file reads/writes or subshell spawning.
4. **Enforce `NOEXEC`**: Prevent the authorized program from executing child processes.
</details>

<details>
<summary><strong>5. Why does `/etc/sudoers.d/` reject filenames containing a period (`.`)?</strong></summary>

**Answer**:
The `sudo` parser uses the standard Debian `run-parts` naming convention when scanning `/etc/sudoers.d/`. Under this specification:
- Files containing a period (`.`), such as `custom.conf` or `backup.old`.
- Files ending with a tilde (`~`) or `.bak` (common temporary swap files created by text editors like vim or nano).
- Files containing special symbols like `#` or `!`.

are **silently ignored** by the sudo parser. This prevents partially written editor swap files or backup copies from inadvertently altering system security policies or introducing syntax errors. Valid drop-in filenames must consist entirely of alphanumeric characters, hyphens, and underscores (e.g., `20-deploy-runner`).
</details>

<details>
<summary><strong>6. What is `env_reset` and why does sudo strip environment variables by default?</strong></summary>

**Answer**:
`env_reset` is a default security directive in `/etc/sudoers`. When enabled, sudo resets the executing process's environment, constructing a minimal, clean environment containing only safe variables (`PATH`, `USER`, `HOME`, `LOGNAME`, `SHELL`) and discarding all custom variables set in the calling user's shell.

This prevents **Environment Variable Hijacking Attacks**:
If an unprivileged user could pass arbitrary environment variables into a root-privileged execution:
- Setting `LD_PRELOAD=/tmp/evil.so` would force the dynamic linker to load an attacker's malicious shared library into the root process upon startup, granting instant root execution.
- Manipulating `IFS` (Internal Field Separator) or `PATH` could trick root scripts into executing malicious binaries placed in `/tmp`.
</details>

<details>
<summary><strong>7. How does modern `sudo-rs` improve security over classic C-based `sudo`?</strong></summary>

**Answer**:
Classic `sudo` is a large, legacy C codebase originally written in the 1980s. Over the decades, numerous critical vulnerabilities (such as CVE-2021-3156 "Baron Samedit", a heap-based buffer overflow allowing unprivileged local privilege escalation to root) have stemmed from memory unsafety bugs inherent to C (buffer overflows, use-after-free, integer underflows).

`sudo-rs` is a complete rewrite of `sudo` in **Rust**, initiated by the Prossimo project (Internet Security Research Group) and supported by modern Linux distributions. Because Rust enforces memory safety, bounds checking, and thread safety at compile time, `sudo-rs` mathematically eliminates entire classes of memory corruption vulnerabilities while maintaining strict 1:1 behavioral and syntax compatibility with the original sudo utility.
</details>

<details>
<summary><strong>8. What is the difference between `sudo -i` and `sudo -s`?</strong></summary>

**Answer**:
- **`sudo -i` (Interactive Login Shell)**: Simulates a complete, initial root login. It changes the working directory to `/root`, initializes a clean login environment, and sources root's startup files (`/etc/profile`, `/root/.bash_profile`, `/root/.bashrc`).
- **`sudo -s` (Shell Execution)**: Spawns the shell specified by the caller's `$SHELL` environment variable (or `/bin/bash` if unset) as root, but does **not** simulate a full login. It preserves the caller's current working directory and does not source root's login profiles.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Query Active Sudo Permissions</strong></summary>

**Task**: Inspect all administrative privileges granted to your current user account using `sudo -l`. Identify whether a password is required.

**Solution**:
```bash
sudo -l
# Output lists user specifications and matching entries in /etc/sudoers.
# Look for 'PASSWD:' or 'NOPASSWD:' tags.
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Invalidate Sudo Timestamp Cache</strong></summary>

**Task**: Authenticate with `sudo`, verify that subsequent sudo commands run without prompting for a password, then manually invalidate the cached credential.

**Solution**:
```bash
# 1. Elevate and cache credentials
sudo ls /root >/dev/null

# 2. Verify subsequent run does not prompt for password
sudo whoami
# Output: root (runs instantly)

# 3. Invalidate timestamp cache
sudo -k

# 4. Verify that running sudo now prompts for password again
sudo whoami
# System prompts: [sudo] password for user:
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Author a Validated Drop-in Sudoers File</strong></summary>

**Task**: Create a modular drop-in file `/etc/sudoers.d/30-webadmin` that allows group `www-data` to reload and check the status of Nginx without a password. Validate syntax with `visudo -c`.

**Solution**:
```bash
# 1. Author drop-in rule using visudo temporary buffer
sudo tee /etc/sudoers.d/30-webadmin << 'EOF'
%www-data ALL=(root) NOPASSWD: /usr/bin/systemctl reload nginx, /usr/bin/systemctl status nginx
EOF

# 2. Fix required strict permissions
sudo chmod 0440 /etc/sudoers.d/30-webadmin

# 3. Verify syntax across all sudoers files
sudo visudo -c
# Output must indicate parsed OK
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Run Commands as an Unprivileged Service User</strong></summary>

**Task**: Use `sudo` to execute a directory creation command inside `/tmp` as system user `daemon`, verifying ownership via `ls -ld`.

**Solution**:
```bash
sudo -u daemon mkdir -p /tmp/daemon_workspace
ls -ld /tmp/daemon_workspace
# Output: drwxr-xr-x 2 daemon daemon 4096 Nov 25 12:00 /tmp/daemon_workspace

# Cleanup
sudo rm -rf /tmp/daemon_workspace
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Identify GTFOBins Escalation Risks in Sudoers</strong></summary>

**Task**: Write a shell script that scans all active sudo privileges returned by `sudo -l` and alerts if any high-risk binaries known for shell escapes (`vim`, `less`, `find`, `python`, `awk`, `nano`, `bash`, `tar`) are permitted.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

RISKY_BINARIES=("vim" "vi" "less" "more" "find" "python" "python3" "awk" "nano" "bash" "sh" "tar")

echo "Auditing sudo permissions for GTFOBins escape vectors..."
SUDO_RULES=$(sudo -l 2>/dev/null || true)

for bin in "${RISKY_BINARIES[@]}"; do
    if echo "$SUDO_RULES" | grep -qE "bin/$bin(\s|$)"; then
        echo "CRITICAL WARNING: Risky binary '$bin' found in sudo rules! High risk of root shell escape!"
    fi
done
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build an Automated Sudo Audit Log Parser</strong></summary>

**Task**: Author a Python or Bash script that inspects `/var/log/auth.log` or `journalctl _COMM=sudo`, parsing all elevated commands executed in the last 24 hours into a formatted CSV report.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

echo "Timestamp,User,TargetUser,TTY,Command"
journalctl _COMM=sudo --since "24 hours ago" --no-pager 2>/dev/null | \
awk '/COMMAND=/ {
    # Extract timestamp
    ts = $1 " " $2 " " $3
    
    # Extract User
    match($0, /([a-zA-Z0-9_-]+) : TTY/, u)
    user = u[1]
    
    # Extract Target User
    match($0, /USER=([a-zA-Z0-9_-]+)/, tu)
    target = tu[1]
    
    # Extract TTY
    match($0, /TTY=([^ ]+)/, t)
    tty = t[1]
    
    # Extract Command
    idx = index($0, "COMMAND=")
    cmd = substr($0, idx+8)
    gsub(/"/, "\"\"", cmd)
    
    printf "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"\n", ts, user, target, tty, cmd
}'
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Command | Key Notes |
| :--- | :--- | :--- |
| **Edit Main Sudoers** | `sudo visudo` | Validates syntax and prevents lockout |
| **Edit Drop-in Rule** | `sudo visudo -f /etc/sudoers.d/<file>` | Name must NOT contain a dot (`.`) |
| **Check Syntax** | `sudo visudo -c` | Verifies all files without editing |
| **List Permissions** | `sudo -l` | Displays all rules for current user |
| **Run As Other User** | `sudo -u <user> <cmd>` | Executes command under alternate UID |
| **Invalidate Cache** | `sudo -k` | Forces password prompt on next run |
| **Preserve Env** | `sudo -E <cmd>` | Passes user environment variables |
| **File Append Trick** | `echo "..." \| sudo tee -a <file>` | Correct pattern for privileged redirection |

---

## 13. Further Reading & Official Documentation

- [man 8 sudo: Execute a Command as Another User](https://man7.org/linux/man-pages/man8/sudo.8.html)
- [man 5 sudoers: Sudoers Configuration Syntax](https://man7.org/linux/man-pages/man5/sudoers.5.html)
- [man 8 visudo: Safely Edit the Sudoers File](https://man7.org/linux/man-pages/man8/visudo.8.html)
- [GTFOBins: Unix Binaries Used to Bypass Local Security Restrictions](https://gtfobins.github.io/)
- [Prossimo Project: Memory-Safe sudo-rs Implementation](https://github.com/trifecta-tech-org/sudo-rs)

---

[<- 07-permissions.md](./07-permissions.md) | [Back to Module Overview](./README.md) | [09-man-pages-and-getting-help.md ->](./09-man-pages-and-getting-help.md)
