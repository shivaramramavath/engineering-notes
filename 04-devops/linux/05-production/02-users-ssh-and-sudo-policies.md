# Linux User Administration, Sudo Policies, and PAM Architecture

| Metadata | Details |
| :--- | :--- |
| **Topic** | Enterprise User Administration, Sudoers Policies, Pluggable Authentication Modules (PAM), Password Hashing |
| **Module** | `05-production` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `01-fundamentals/06-users-and-groups.md`, `01-fundamentals/08-sudo-and-privilege.md`, `04-networking/04-ssh.md` |
| **Target Audience** | Security Engineers, Systems Administrators, SREs, DevSecOps Practitioners |
| **Target Distribution** | Ubuntu 24.04 LTS (`yescrypt` hashing), Rocky Linux / RHEL 9.4 |
| **Key Tools / Files** | `sudo`, `visudo`, `chage`, `passwd`, `/etc/sudoers.d/`, `/etc/pam.d/`, `/etc/shadow`, `/etc/security/` |

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

In enterprise infrastructure, unmanaged root access and lax user policies are the fastest path to security breaches and compliance failure. Common failure modes include:
- Giving developers blanket `ALL=(ALL) ALL` sudo permissions, allowing arbitrary root shell escapes via `sudo vim`, `sudo less`, or `sudo find`.
- Orphaned contractor accounts remaining active months after contract termination.
- Storing password hashes in obsolete algorithms (`MD5` or single-round `SHA-256`) susceptible to rapid GPU-accelerated hash cracking.
- Lack of multi-factor authentication (MFA) on interactive sudo escalations or bastion SSH logins.

Modern Linux systems enforce access control through two decoupled subsystems:
1. **The Sudoers Engine (`sudo`, `visudo`)**: Enables granular, command-level privilege delegation with complete audit logging.
2. **Pluggable Authentication Modules (PAM)**: The modular authentication framework in userspace that mediates password validation, account locking, session resource limits, and MFA integration without modifying application source code.

Mastering sudoers drop-in policies, modern memory-hard password hashing (`yescrypt`), and PAM stack configuration ensures strict adherence to the Principle of Least Privilege across enterprise Linux environments.

---

## Core Concepts

### Sudoers Syntax Grammar
The grammar of `/etc/sudoers` follows a strict specification:
```text
who    where = (as_whom)    what
user   host  = (run_as_user:run_as_group) [TAGS] command [arguments]
```
- **User Alias / User**: Specific username (`alice`), group (`%devops`), or alias (`User_Alias ADMINS = alice, bob`).
- **Host**: Which machines the rule applies to (almost always `ALL`).
- **Runas Spec**: Which target user the command executes as (default is `(root)` or `(ALL:ALL)`).
- **Tags**: Modifiers altering execution behavior:
  - `NOPASSWD:` / `PASSWD:`: Require or waive password prompt.
  - `NOEXEC:`: Intercepts `execve()` calls to prevent shell escapes (subshell spawning).
  - `SETENV:`: Permits passing environment variables (`sudo -E`).
- **Commands**: Absolute paths to approved binaries (`/usr/bin/systemctl restart nginx`).

### Modern Password Hashing: Yescrypt vs SHA-512
The `/etc/shadow` file stores password hashes formatted as:
```text
$id$salt$hashed_value
```
- `$1$`: MD5 (Completely broken; crackable in seconds via GPUs).
- `$5$`: SHA-256 (Vulnerable to ASIC acceleration).
- `$6$`: SHA-512 (Standard enterprise default since RHEL 6 / Ubuntu 12).
- **`$y$` (`yescrypt`)**: The modern default in Ubuntu 22.04+, Debian 12, and RHEL 9. Yescrypt is a memory-hard password-hashing scheme designed to resist mass-scale cracking attacks powered by GPUs, FPGAs, and ASICs.

### The Pluggable Authentication Module (PAM) Architecture
Located in `/etc/pam.d/`, PAM decouples programs (e.g. `sshd`, `sudo`, `login`, `su`) from authentication mechanisms. Each PAM stack evaluates four management groups:
1. **`auth`**: Validates user identity (prompts for password, checks MFA token).
2. **`account`**: Validates account eligibility (is account expired? is login permitted at this hour? is password expired?).
3. **`password`**: Manages password updates (enforces complexity, dictionary checks).
4. **`session`**: Tasks executed before and after granting access (mounting home directories, logging audit records, setting resource limits via `pam_limits.so`).

Control flags determine flow evaluation:
- `required`: Must succeed. If it fails, PAM continues evaluating the remaining stack, but access will be denied.
- `requisite`: Must succeed. If it fails, PAM **aborts immediately** without executing subsequent modules.
- `sufficient`: If this succeeds and no prior required module failed, PAM **grants access immediately**.
- `optional`: Informational; result only matters if no other module succeeds.

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                                    User Executes `sudo cmd`                             |
+----------------------------------------------------+------------------------------------+
                                                     |
                                                     v
+-----------------------------------------------------------------------------------------+
|                                      PAM Stack (/etc/pam.d/sudo)                        |
|   1. auth    requisite   pam_faillock.so preauth     (Check if account locked)          |
|   2. auth    sufficient  pam_google_authenticator.so (Optional MFA Token)              |
|   3. auth    required    pam_unix.so                 (Verify /etc/shadow password)      |
|   4. account required    pam_unix.so                 (Check expiration date)            |
+----------------------------------------------------+------------------------------------+
                                                     | Success?
                                                     v
+-----------------------------------------------------------------------------------------+
|                                   Sudoers Policy Engine                                 |
|                       Reads /etc/sudoers and /etc/sudoers.d/*                           |
|   - Matches User & Group (%devops)                                                      |
|   - Validates Absolute Command Path (/usr/bin/systemctl)                                |
|   - Validates Argument Constraints (restart nginx)                                      |
|   - Checks for NOEXEC restrictions                                                      |
+----------------------------------------------------+------------------------------------+
                                                     | Approved?
                                                     v
+-----------------------------------------------------------------------------------------+
|                              Linux Kernel: execve() Execution                           |
|   - Sets Effective UID (EUID = 0)                                                       |
|   - Logs execution to Syslog / Auditd (COMMAND=/usr/bin/systemctl restart nginx)        |
|   - Spawns process with restricted environment variables                                |
+-----------------------------------------------------------------------------------------+
```

### Preventing Sudo Shell Escapes
A common vulnerability occurs when administrators grant sudo access to text editors or pagers:
```text
# VULNERABLE RULE:
alice ALL=(ALL) /usr/bin/vim /etc/nginx/nginx.conf
```
Alice can simply run `sudo vim /etc/nginx/nginx.conf`, type `:!/bin/bash` or `:sh` inside Vim, and spawn an unconstrained interactive root shell!
To prevent this, hardened rules employ **`NOEXEC:`**:
```text
alice ALL=(ALL) NOEXEC: /usr/bin/vim /etc/nginx/nginx.conf
```
`NOEXEC` preloads a wrapper library (`sudo_noexec.so`) that intercepts `execve()`, `execl()`, and `fork()` calls, preventing child processes from being spawned from within the executed binary.

---

## Syntax & CLI Tools

### Safely Managing Sudoers with `visudo`
**Never edit `/etc/sudoers` directly with standard text editors**. Always use `visudo`, which locks the file and validates syntax before writing to disk.

```bash
# Edit main sudoers file
sudo visudo

# Edit or create a drop-in file under /etc/sudoers.d/
sudo visudo -f /etc/sudoers.d/99-devops-team

# Validate syntax of all sudoers files non-interactively (ideal for CI/CD)
sudo visudo -c
```

### Managing User Account Aging and Expiration with `chage`
```bash
# Display account aging and expiration details for a user
sudo chage -l developer_bob

# Set account to expire on a specific date (YYYY-MM-DD)
sudo chage -E 2026-12-31 developer_bob

# Force user to change password upon their very next login
sudo chage -d 0 developer_bob

# Enforce maximum password age of 90 days with a 7-day warning period
sudo chage -M 90 -W 7 developer_bob

# Lock an account immediately
sudo passwd -l developer_bob
sudo usermod -L developer_bob
```

### Auditing User Privileges
```bash
# List all allowed (and forbidden) sudo commands for current user
sudo -l

# List allowed sudo commands for another user
sudo -l -U alice
```

---

## Progressive Practical Examples

### Example 1: Creating Modular, Least-Privilege Sudoers Drop-Ins
Granting a junior DevOps engineer permission to inspect logs and restart Nginx, but strictly prohibiting arbitrary root commands or shell escapes.

```bash
# Create drop-in file using visudo
sudo visudo -f /etc/sudoers.d/developer-webops
```

```text
# /etc/sudoers.d/developer-webops

# Define Command Aliases for clarity
Cmnd_Alias NGINX_MGT = /usr/bin/systemctl restart nginx, \
                       /usr/bin/systemctl reload nginx, \
                       /usr/bin/systemctl status nginx
Cmnd_Alias LOG_VIEW  = /usr/bin/journalctl -u nginx*, \
                       /usr/bin/tail -n [0-9]* /var/log/nginx/*

# Grant developer user permissions with NOEXEC to block subshells
devops_dave ALL=(ALL) NOEXEC: NGINX_MGT, LOG_VIEW

# Allow non-privileged restart without repeatedly prompting for password
devops_dave ALL=(ALL) NOPASSWD: /usr/bin/systemctl reload nginx
```

```bash
# Verify syntax
sudo visudo -c
```

### Example 2: Implementing Automatic Account Lockout on Failed Logins (PAM `faillock`)
Defend against local and SSH brute-force password attacks by locking accounts after 5 failed authentication attempts for 15 minutes.

```bash
# Configure in /etc/security/faillock.conf
sudo tee /etc/security/faillock.conf << 'EOF'
dir = /var/run/faillock
audit
silent
deny = 5
fail_interval = 900
unlock_time = 900
even_deny_root
root_unlock_time = 300
EOF
```

```bash
# Inspect locked accounts
sudo faillock

# Manually unlock an account after verification
sudo faillock --user developer_bob --reset
```

### Example 3: Enforcing Strong Password Hashing with `yescrypt`
Ensure new passwords utilize the memory-hard `yescrypt` algorithm.

```bash
# In /etc/pam.d/common-password (Ubuntu/Debian) or /etc/pam.d/system-auth (RHEL)
# Verify the pam_unix.so line includes yescrypt:
# password  [success=1 default=ignore]  pam_unix.so obscure yescrypt rounds=11

# Update an existing user's password to generate a yescrypt hash:
sudo passwd developer_bob

# Inspect /etc/shadow to confirm hash starts with $y$:
sudo grep developer_bob /etc/shadow
# Output: developer_bob:$y$j9T$...:19842:0:90:7:::
```

### Example 4: Configuring Restricted Shells (`rbash`) for Auditing / Maintenance
Restrict contract engineers to specific diagnostic binaries without permission to change directories (`cd`), alter environment variables, or redirect output.

```bash
# 1. Create dedicated restricted user with rbash
sudo useradd -m -s /bin/rbash contractor_sam
sudo passwd contractor_sam

# 2. Restrict user's PATH to a sandboxed bin directory
sudo mkdir -p /home/contractor_sam/bin
sudo chown root:root /home/contractor_sam/.bash_profile
sudo chmod 755 /home/contractor_sam/.bash_profile

# 3. Populate only approved commands as symlinks
sudo ln -s /usr/bin/uptime /home/contractor_sam/bin/uptime
sudo ln -s /usr/bin/top /home/contractor_sam/bin/top
sudo ln -s /usr/bin/df /home/contractor_sam/bin/df

# 4. Enforce PATH in .bash_profile
echo 'export PATH=$HOME/bin' | sudo tee -a /home/contractor_sam/.bash_profile
```

### Example 5: Automated Account Expiration Audit Script
Production script that flags dormant accounts, users without expiration dates, and accounts with empty passwords.

```bash
#!/usr/bin/env bash
set -euo pipefail

echo "=== Linux User Security & Expiration Audit ==="

# 1. Check for accounts with empty passwords
EMPTY_PW=$(sudo awk -F: '($2 == "") {print $1}' /etc/shadow)
if [ -n "${EMPTY_PW}" ]; then
    echo "CRITICAL: Accounts with EMPTY passwords detected:"
    echo "${EMPTY_PW}"
fi

# 2. Check for UID 0 accounts other than root
EXTRA_ROOTS=$(awk -F: '($3 == 0 && $1 != "root") {print $1}' /etc/passwd)
if [ -n "${EXTRA_ROOTS}" ]; then
    echo "CRITICAL: Non-root accounts with UID 0 detected:"
    echo "${EXTRA_ROOTS}"
fi

# 3. List expiring accounts in the next 14 days
echo "Checking expiring accounts..."
while IFS=: read -r USERNAME _ _ _ _ _ EXPIRE_DAYS _; do
    if [ -n "${EXPIRE_DAYS}" ] && [ "${EXPIRE_DAYS}" -gt 0 ]; then
        EXPIRE_EPOCH=$(( EXPIRE_DAYS * 86400 ))
        NOW_EPOCH=$(date +%s)
        DAYS_REMAINING=$(( (EXPIRE_EPOCH - NOW_EPOCH) / 86400 ))
        
        if [ "${DAYS_REMAINING}" -ge 0 ] && [ "${DAYS_REMAINING}" -le 14 ]; then
            echo "NOTICE: User ${USERNAME} expires in ${DAYS_REMAINING} days."
        fi
    fi
done < /etc/shadow

echo "Audit completed."
```

---

## Best Practices

1. **Always Use Modular Drop-Ins in `/etc/sudoers.d/`**: Never edit the main `/etc/sudoers` file directly. Package management upgrades often prompt to overwrite `/etc/sudoers`. Place team policies in isolated files (`/etc/sudoers.d/40-infra-team`) validated via `visudo -f`.
2. **Never Grant Sudo to Interpreters, Pagers, or Editors Without Flags**: Binaries like `vim`, `nano`, `less`, `more`, `python`, `perl`, `awk`, `find`, and `bash` permit shell escapes. If administrators need to edit a file, grant permission via `sudoedit /etc/target.conf` rather than `sudo vim /etc/target.conf`. `sudoedit` copies the file to a temporary location, opens it as an unprivileged user, and writes changes back upon exit.
3. **Always Specify Absolute Binary Paths in Sudoers**: Specifying `systemctl restart nginx` allows an attacker to alter their local `$PATH` (e.g. `PATH=/tmp:$PATH`), place a malicious `systemctl` script in `/tmp`, and execute arbitrary code as root. Always specify `/usr/bin/systemctl restart nginx`.
4. **Use `NOEXEC` on Any Delegated Binary Capable of Spawning Children**: Prevent command substitution and subshells by appending the `NOEXEC:` tag to delegated commands.
5. **Set Account Expiration Dates on Provisioning**: When creating temporary contractor or onboarding user accounts, always specify an expiration date immediately: `useradd -e 2026-12-31 <username>`.
6. **Enforce `yescrypt` or `sha512` with High Iteration Counts**: Ensure `/etc/login.defs` specifies `ENCRYPT_METHOD YESCRYPT` or `ENCRYPT_METHOD SHA512` with at least 50,000 rounds (`SHA_CRYPT_MIN_ROUNDS 50000`).

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Editing `/etc/sudoers` with standard `nano` or `vim` | A syntax typo locks all users out of sudo escalation permanently. | Always edit via `sudo visudo` or validate with `visudo -c`. |
| Delegating `sudo vim` or `sudo less` | Users execute `:!/bin/sh` from within the pager to acquire a full interactive root shell. | Use `sudoedit` for file editing; use `tail` or `journalctl` for logs. |
| Granting `NOPASSWD: ALL` for developer convenience | Eliminates authentication safeguards; compromised developer laptops result in instant fleet-wide root compromise. | Restrict `NOPASSWD` strictly to non-destructive automated scripts. |
| Placing wildcards improperly in sudoers (e.g. `/usr/bin/kill *`) | Allows users to kill arbitrary system processes (like PID 1 `systemd`) or pass flags like `--signal 9 1`. | Restrict arguments tightly: `/usr/bin/systemctl restart myapp`. |
| Sudoers drop-in files with periods or backup suffixes (`99-team.bak`) | Sudoers ignores any file in `/etc/sudoers.d/` that ends with `~` or contains a `.` (dot) character. | Use names containing only alphanumeric characters and underscores/hyphens. |
| Forgetting to lock both password and SSH keys when offboarding | Running `passwd -l` locks password authentication, but **does not disable SSH public key authentication**. | Run `usermod -L -s /usr/sbin/nologin <user>` and rename their `~/.ssh/authorized_keys`. |

---

## Performance, Security & Cost Implications

### Performance
- **PAM Module Overhead**: Every execution of `sudo` traverses the PAM stack. Stacking slow remote authentication modules (e.g. un-cached LDAP, slow Active Directory domain controllers) can add 200ms–2000ms delay to every single sudo command. Keep local PAM stacks streamlined with `pam_nscd` or `sssd` caching.
- **Yescrypt Memory Cost**: Yescrypt deliberately consumes 64KB–1MB of RAM per hash evaluation to defeat GPU parallelism. For human login rates (tens of logins per minute), CPU impact is negligible; for high-frequency automated authentication, use SSH keys rather than password authentication.

### Security
- **Defense Against Credential Stuffing**: PAM `faillock` completely thwarts brute-force dictionary attacks against local console and SSH authentication.
- **Privilege Creep Mitigation**: Maintaining sudo policies in source-controlled git repositories deployed via Ansible ensures that administrative rights are versioned, reviewed, and audited.

### Cost
- In regulatory audits (SOC 2 Type II, ISO 27001, PCI-DSS), lack of granular sudo delegation and shared root passwords is an automatic audit failure, resulting in lost enterprise contracts and expensive remediation consulting.

---

## Comparison Table / Alternatives

| Privilege Escalation Tool | Privilege Model | Syntax Validation | Shell Escape Prevention | Audit Logging |
| :--- | :--- | :--- | :--- | :--- |
| **`sudo`** | Fine-grained per-command & per-argument | Built-in (`visudo`) | Built-in (`NOEXEC:`, `sudoedit`) | Detailed Syslog / Journald events |
| **`su`** | Full root account switch (Binary All-or-Nothing) | None | None | Basic PAM log only |
| **Polkit (`pkexec`)** | Desktop / D-Bus Action-based authorization | XML / JavaScript | Action policies | D-Bus system bus logging |
| **`sudoedit`** | Dedicated safe file modification mode | Validated via sudoers | Complete (edits temporary file as user) | Logs target file and user |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: An administrator edited `/etc/sudoers` directly, introduced a syntax error, and now `sudo` throws `syntax error near line 25`. How do you recover?
<details>
<summary>View Solution</summary>

**Recovery Strategies**:
1. **If an existing root shell is open**: Simply run `visudo` from the open root shell to fix the typo.
2. **If `pkexec` is installed**: Run `pkexec visudo`. Polkit evaluates authentication independently of sudoers.
3. **If root password is known**: Switch to root directly using `su -` and run `visudo`.
4. **Cloud Console / Boot Recovery**:
   - Reboot into GRUB, append `init=/bin/bash` or `rd.break` to the kernel command line to spawn an emergency root shell.
   - Remount root read-write: `mount -o remount,rw /`.
   - Repair the file: `visudo -f /etc/sudoers`.
</details>

### Q2: Why is `sudoedit` significantly safer than granting `sudo vim /etc/nginx/nginx.conf`?
<details>
<summary>View Solution</summary>

When a user executes `sudo vim /etc/nginx/nginx.conf`:
- The entire `vim` process runs as UID 0 (root).
- The user can execute shell escapes (`:!sh`, `:py import os; os.system("/bin/sh")`), edit other files (`:e /etc/shadow`), or execute arbitrary plugins with full root privileges.

When a user executes `sudoedit /etc/nginx/nginx.conf`:
- Sudo copies `/etc/nginx/nginx.conf` to a temporary file in `/var/tmp/`.
- Sudo drops privileges and launches the user's preferred editor **as the unprivileged user**.
- Even if the user executes `:!sh`, they get a standard unprivileged shell.
- When the editor exits, sudo validates that only the temporary file changed and copies the content back to `/etc/nginx/nginx.conf` as root.
</details>

### Q3: What is the difference between `passwd -l` and `usermod -L`?
<details>
<summary>View Solution</summary>

Both commands lock a user's password by prepending an exclamation point (`!`) to the password hash in `/etc/shadow`.
However:
- **Neither command disables SSH public key authentication!** If the user has keys in `~/.ssh/authorized_keys`, they can still log in over SSH.
- To completely lock an account, you must also:
  ```bash
  sudo usermod -L -e 1 -s /usr/sbin/nologin <username>
  ```
  (`-e 1` sets account expiration to epoch day 1; `-s /usr/sbin/nologin` prevents shell access).
</details>

### Q4: Explain the difference between `required`, `requisite`, and `sufficient` in a PAM configuration.
<details>
<summary>View Solution</summary>

- **`required`**: The module must succeed. If it fails, the overall PAM operation will ultimately fail, but PAM continues executing the remaining modules in the stack so the attacker cannot determine which specific check failed.
- **`requisite`**: The module must succeed. If it fails, PAM **aborts immediately** and returns an error without evaluating the rest of the stack.
- **`sufficient`**: If this module succeeds and no earlier `required` module has failed, PAM **grants access immediately**, skipping all remaining modules in that management group.
</details>

### Q5: What does the `/etc/sudoers.d/` naming convention require?
<details>
<summary>View Solution</summary>

Files inside `/etc/sudoers.d/` must:
1. **NOT end with a `~`** (backup files created by vim/nano).
2. **NOT contain a `.` (dot)** (e.g., `team.conf` or `policy.bak` are ignored by sudo!).
3. Contain only alphanumeric characters, underscores (`_`), and hyphens (`-`).
4. Have strict permissions: owned by `root:root` with mode `0440` (`-r--r-----`).
</details>

### Q6: How do you configure `sudo` to require a password prompt only once every 30 minutes instead of the default 15 minutes?
<details>
<summary>View Solution</summary>

In `/etc/sudoers` or a drop-in file in `/etc/sudoers.d/`, configure the `timestamp_timeout` parameter:
```text
Defaults timestamp_timeout=30
```
*(Setting `timestamp_timeout=0` forces a password prompt on every single execution; setting a negative number like `-1` keeps the ticket active indefinitely until reboot).*
</details>

### Q7: What does the `faillock` utility do and where does it track authentication failures?
<details>
<summary>View Solution</summary>

`faillock` (part of `pam_faillock.so`, replacing legacy `pam_tally2`) tracks failed authentication attempts in binary tally files stored under `/var/run/faillock/` (or `/var/log/tallylog`).
When failed attempts exceed the configured threshold (e.g. `deny = 5`), `pam_faillock` denies further authentication attempts until the `unlock_time` expires or an administrator runs `faillock --user <username> --reset`.
</details>

### Q8: How can you pass custom environment variables through `sudo` without `sudo -E`?
<details>
<summary>View Solution</summary>

By default, `sudo` resets environment variables to a clean, minimal baseline (`Defaults env_reset`).
To permit specific variables (like `HTTP_PROXY`, `EDITOR`, or `AWS_REGION`) without allowing unrestricted `sudo -E`:
```text
Defaults env_keep += "HTTP_PROXY HTTPS_PROXY NO_PROXY"
```
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Creating a Validated Sudoers Drop-In File
**Task**: Use `visudo -f` to create `/etc/sudoers.d/99-test-policy`. Grant your current user permission to execute `/usr/bin/uptime` without a password. Verify with `sudo -l` and `visudo -c`.
<details>
<summary>View Solution</summary>

```bash
# 1. Create drop-in using visudo
echo "${USER} ALL=(ALL) NOPASSWD: /usr/bin/uptime" | sudo tee /tmp/99-test-policy
sudo visudo -c -f /tmp/99-test-policy
sudo cp /tmp/99-test-policy /etc/sudoers.d/99-test-policy
sudo chmod 0440 /etc/sudoers.d/99-test-policy
rm -f /tmp/99-test-policy

# 2. Verify with sudo -l
sudo -l | grep uptime

# 3. Test execution
sudo /usr/bin/uptime

# 4. Clean up
sudo rm -f /etc/sudoers.d/99-test-policy
```
</details>

### Exercise 2: Auditing User Aging and Setting Password Expiry
**Task**: Create a test user `expiring_user`. Set their account to expire in 7 days. Verify expiration parameters with `chage -l`. Remove the user.
<details>
<summary>View Solution</summary>

```bash
# 1. Create user
sudo useradd -m expiring_user

# 2. Set expiration date 7 days from today
EXPIRY_DATE=$(date -d "+7 days" +%Y-%m-%d)
sudo chage -E "${EXPIRY_DATE}" expiring_user

# 3. Inspect aging parameters
sudo chage -l expiring_user

# 4. Clean up
sudo userdel -r expiring_user
```
</details>

### Exercise 3: Inspecting Password Hashes in `/etc/shadow`
**Task**: Inspect the password hash of your local user in `/etc/shadow`. Identify the hashing algorithm identifier (`$y$`, `$6$`, or `$1$`).
<details>
<summary>View Solution</summary>

```bash
sudo grep "^${USER}:" /etc/shadow | awk -F: '{print $1, $2}' | cut -d'$' -f1,2,3
# If output shows $y$, yescrypt is active!
# If output shows $6$, SHA-512 is active!
```
</details>

### Exercise 4: Validating Sudo Shell Escape Prevention
**Task**: Review a rule permitting `sudo /usr/bin/less /var/log/syslog`. Demonstrate why this is vulnerable to a shell escape, and rewrite the rule safely using `NOEXEC` or `tail`.
<details>
<summary>View Solution</summary>

```text
# VULNERABLE:
# alice ALL=(ALL) /usr/bin/less /var/log/syslog
# Inside less, typing !/bin/bash spawns a root shell.

# SECURE ALTERNATIVE 1 (Using NOEXEC):
alice ALL=(ALL) NOEXEC: /usr/bin/less /var/log/syslog

# SECURE ALTERNATIVE 2 (Restricting to tail/cat):
alice ALL=(ALL) /usr/bin/tail -n [0-9]* /var/log/syslog
```
</details>

### Exercise 5: Auditing Sudoers Syntax with `visudo -c`
**Task**: Run `visudo -c` across your entire system. Verify that all sudoers files pass syntax validation.
<details>
<summary>View Solution</summary>

```bash
sudo visudo -c
# Output:
# /etc/sudoers: parsed OK
# /etc/sudoers.d/README: parsed OK
```
</details>

### Exercise 6: Complete Account Deactivation Simulation
**Task**: Write an idempotent Bash snippet that locks an account, sets expiration to epoch day 1, disables its shell, and archives its authorized keys.
<details>
<summary>View Solution</summary>

```bash
#!/usr/bin/env bash
set -euo pipefail

TARGET_USER="test_offboard"

# Create dummy user for testing
sudo useradd -m "${TARGET_USER}"

echo "==> Offboarding user ${TARGET_USER}..."
sudo passwd -l "${TARGET_USER}"
sudo usermod -L -e 1 -s /usr/sbin/nologin "${TARGET_USER}"

if [ -f "/home/${TARGET_USER}/.ssh/authorized_keys" ]; then
    sudo mv "/home/${TARGET_USER}/.ssh/authorized_keys" "/home/${TARGET_USER}/.ssh/authorized_keys.revoked"
fi

echo "Account locked and disabled successfully."
sudo chage -l "${TARGET_USER}"

# Clean up
sudo userdel -r "${TARGET_USER}"
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Operation | Command Example |
| :--- | :--- |
| **Edit Sudoers Safely** | `sudo visudo` |
| **Edit Drop-in Sudoers** | `sudo visudo -f /etc/sudoers.d/team-ops` |
| **Validate Sudoers Syntax** | `sudo visudo -c` |
| **List My Sudo Permissions**| `sudo -l` |
| **Inspect User Aging** | `sudo chage -l <username>` |
| **Set Account Expiration** | `sudo chage -E YYYY-MM-DD <username>` |
| **Force Password Change** | `sudo chage -d 0 <username>` |
| **Lock Account Password** | `sudo passwd -l <username>` |
| **Unlock Failed Logins** | `sudo faillock --user <username> --reset` |

### Further Reading
- `man 5 sudoers` and `man 8 visudo`
- `man 1 chage` and `man 8 pam_faillock`
- `man 5 pam.conf` and `man 7 pam`
- [Sudo Project Official Manual](https://www.sudo.ws/docs/man/sudoers.man/)
- [The Linux-PAM System Administrator's Guide](http://www.linux-pam.org/Linux-PAM-html/Linux-PAM_SAG.html)
- [Yescrypt Password Hashing Specification](https://www.openwall.com/yescrypt/)
