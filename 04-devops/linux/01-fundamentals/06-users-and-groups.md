<!--
================================================================================
Metadata:
  Title: Users, Groups & Identity Management in Linux
  Track: Linux & Systems Engineering / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 01-fundamentals/01-getting-started.md
  Key Terminology: UID, GID, /etc/passwd, /etc/shadow, /etc/group, useradd, usermod, PAM, nologin, ruid, euid
================================================================================
-->

# Users, Groups & Identity Management in Linux

[<- 05-links-inodes-and-file-types.md](./05-links-inodes-and-file-types.md) | [Back to Module Overview](./README.md) | [07-permissions.md ->](./07-permissions.md)

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

Linux is fundamentally a multi-user operating system designed around the principle of isolated process credentials. Every running process, network socket, and file on disk is bound to a specific numeric **User Identifier (UID)** and **Group Identifier (GID)**.

In production infrastructure and DevOps automation:
- **Service Isolation**: Daemons (Nginx, PostgreSQL, Redis) must run under dedicated, unprivileged system service accounts to constrain the blast radius if an application is compromised via a Remote Code Execution (RCE) exploit.
- **Container UID Mapping**: Containers run on the host Linux kernel; a container process executing as `root` (UID 0) inside an unhardened container is UID 0 on the host kernel, creating severe container breakout risks.
- **Authentication Stacks**: Centralized identity systems (LDAP, Active Directory, FreeIPA, Okta) integrate into Linux via the **Pluggable Authentication Modules (PAM)** and Name Service Switch (NSS) subsystem.

---

## 2. Core Concepts

### UID and GID Allocation Standards
Linux assigns standard numerical ranges to delineate user classes:

| UID Range | Account Type | Purpose | Login Shell |
| :--- | :--- | :--- | :--- |
| **0** | Root Superuser | Full, unconstrained administrative privilege across the kernel | `/bin/bash` |
| **1 – 999** | System Accounts | Daemon service execution (`systemd`, `nginx`, `postgres`, `ssh`) | `/usr/sbin/nologin` or `/bin/false` |
| **1000 – 59999**| Regular Users | Human operators and developers created via `useradd` | `/bin/bash` or `/bin/zsh` |
| **60000 – 65534**| Special Accounts| Dynamic nobody accounts (`nobody` / `nogroup`: UID 65534) | `/usr/sbin/nologin` |

### Primary vs. Supplementary Groups
- **Primary Group**: Defined in the 4th field of `/etc/passwd`. Every file created by the user is initially assigned to this group.
- **Supplementary (Secondary) Groups**: Defined in `/etc/group`. Grants the user additional access rights (e.g., adding a user to `docker` group or `sudo` group).

### Anatomy of Identity Files
1. **`/etc/passwd`** (Publicly readable, `0644`):
   ```text
   ubuntu:x:1000:1000:Ubuntu User,,,:/home/ubuntu:/bin/bash
     [1]  [2] [3]  [4]         [5]         [6]        [7]
   ```
   - `[1]` Username (`ubuntu`)
   - `[2]` Password indicator (`x` = password hash stored securely in `/etc/shadow`)
   - `[3]` User ID (UID = `1000`)
   - `[4]` Primary Group ID (GID = `1000`)
   - `[5]` GECOS comment field (Full name, room, contact)
   - `[6]` User home directory (`/home/ubuntu`)
   - `[7]` Login shell (`/bin/bash`)

2. **`/etc/shadow`** (Restricted read, root only, `0640` or `0600`):
   ```text
   ubuntu:$y$j9T$SaltString$HashValue:19820:0:90:7:::
     [1]               [2]             [3]  [4][5][6][7][8][9]
   ```
   - `[1]` Username
   - `[2]` Encrypted password hash (Format: `$id$salt$hash`; `$6$` = SHA-512, `$y$` = modern Yescrypt on Ubuntu 24.04)
   - `[3]` Date of last password change (days since Jan 1, 1970)
   - `[4]` Minimum password age (days before user can change password)
   - `[5]` Maximum password age (password expiration days)
   - `[6]` Warning days prior to password expiration
   - `[7]` Inactivity days allowed after expiration before account lock
   - `[8]` Explicit account expiration date
   - `[9]` Reserved flag

3. **`/etc/group`** (Publicly readable, `0644`):
   ```text
   docker:x:998:ubuntu,devops_lead
     [1]  [2] [3]       [4]
   ```
   - `[1]` Group name
   - `[2]` Group password flag (`x`)
   - `[3]` Group ID (GID)
   - `[4]` Comma-separated list of supplementary members

---

## 3. How It Works Internally

### Process Credentials: Real, Effective & Saved UIDs
In the Linux kernel, every process represented by `struct task_struct` contains a `struct cred` record tracking multiple UID descriptors:

```
+-------------------------------------------------------------------------+
| PROCESS CREDENTIAL MODEL (struct cred)                                  |
|                                                                         |
| Real UID (ruid):      Identifies who launched the process.              |
| Effective UID (euid): Evaluated by the kernel during VFS permission    |
|                       checks (e.g., file opens, network socket binds).  |
| Saved UID (suid):     Stores prior euid during privilege drops.         |
|                                                                         |
| SCENARIO: Normal execution of 'cat /etc/hosts' by user (UID 1000)       |
|   ruid = 1000 | euid = 1000 | suid = 1000                               |
|                                                                         |
| SCENARIO: User (UID 1000) runs SUID binary 'passwd' (Owned by root)     |
|   ruid = 1000 | euid = 0 (ROOT) | suid = 0 (ROOT)                       |
|   Kernel checks euid (0) -> Permits read/write to /etc/shadow!          |
+-------------------------------------------------------------------------+
```

### Pluggable Authentication Modules (PAM)
When an application authenticates a user (SSH login, `sudo` execution, graphical lock), it does not read `/etc/shadow` directly. It delegates to the PAM subsystem located in `/etc/pam.d/`.

PAM breaks authentication into 4 distinct phases:
1. **`auth`**: Verifies user identity (password, MFA token, hardware key).
2. **`account`**: Checks if the account is allowed to log in (password expiration, time restrictions).
3. **`password`**: Updates user authentication credentials.
4. **`session`**: Configures the environment before handing control to the user (mounting homes, setting resource limits via `pam_limits.so`).

---

## 4. Syntax & Usage

### Creating and Managing Users
```bash
# Production command: Create system service user without home directory or login shell
sudo useradd --system --no-create-home --shell /usr/sbin/nologin prometheus

# Create regular human operator with standard home and bash shell
sudo useradd -m -s /bin/bash -c "Developer Account" john

# Add a user to supplementary groups (CRUCIAL: always include -a for append!)
sudo usermod -aG docker,sudo john

# Change user's login shell
sudo chsh -s /bin/zsh john

# Lock an account (prepends '!' to password hash in /etc/shadow)
sudo usermod -L john

# Delete a user and purge their home directory and mail spool
sudo userdel -r john
```

### Inspecting User and Group Membership
```bash
# Print current real and effective UID, GID, and all group memberships
id

# Print specific user credentials
id prometheus
# Output: uid=995(prometheus) gid=995(prometheus) groups=995(prometheus)

# List all groups the current user belongs to
groups

# View active logged in users
w
who
```

---

## 5. Practical Examples

### Example 1: Creating a Hardened Application Service Account
```bash
# Production requirement: Run Node.js API service as an isolated unprivileged user
# 1. Create dedicated group with fixed GID
sudo groupadd -g 2001 app-runner

# 2. Create system user with matching UID, custom home, and no interactive login
sudo useradd --system \
  -u 2001 \
  -g app-runner \
  -d /opt/app \
  -s /usr/sbin/nologin \
  -c "Application Daemon Runner" \
  app-runner

# 3. Verify account parameters in system files
getent passwd app-runner
# Output: app-runner:x:2001:2001:Application Daemon Runner:/opt/app:/usr/sbin/nologin

# 4. Verify user cannot establish interactive shell
sudo su - app-runner
# Output: This account is currently not available.
```

### Example 2: Safely Appending Groups to an Existing Engineer
```bash
# DANGER: Running 'usermod -G docker john' wipes all other supplementary groups!
# SAFE PATTERN: Always pass -a (append) along with -G

# Inspect groups before
id john
# uid=1001(john) gid=1001(john) groups=1001(john),27(sudo)

# Add to docker and adm groups
sudo usermod -aG docker,adm john

# Inspect groups after
id john
# uid=1001(john) gid=1001(john) groups=1001(john),4(adm),27(sudo),998(docker)
```

### Example 3: Auditing Insecure Accounts with Empty Passwords or Root UIDs
```bash
# Security Audit 1: Find any unauthorized accounts sharing UID 0 (root backdoor check)
awk -F: '($3 == 0) { print "UID 0 Account: " $1 }' /etc/passwd
# Expected output: UID 0 Account: root (ONLY root should appear!)

# Security Audit 2: Find accounts with empty password hashes in /etc/shadow
sudo awk -F: '($2 == "" || $2 == "!") { print "Locked or Empty: " $1 }' /etc/shadow

# Security Audit 3: Find accounts with interactive login shells that are not human users
awk -F: '($3 < 1000 && $7 !~ /nologin|false/) { print "System account with shell: " $1 " (" $7 ")" }' /etc/passwd
```

### Example 4: Testing PAM Account Expiration Policy
```bash
# Create temporary test user
sudo useradd -m testuser
echo "testuser:TempPassword123!" | sudo chpasswd

# Inspect aging details
sudo chage -l testuser

# Set password expiration to 30 days and warning period to 7 days
sudo chage -M 30 -W 7 testuser

# Force user to change password upon very next login
sudo chage -d 0 testuser

# Verify updated policy
sudo chage -l testuser | grep "Last password change"
# Output: Last password change: password must be changed

# Cleanup
sudo userdel -r testuser
```

### Example 5: Configuring Per-User Resource Limits via PAM (`limits.conf`)
```bash
# Prevent a service user from triggering a fork bomb or exhausting open files
sudo tee -a /etc/security/limits.d/99-apprunner.conf << 'EOF'
# <domain>     <type>   <item>     <value>
app-runner     soft     nofile     65536
app-runner     hard     nofile     65536
app-runner     soft     nproc      2048
app-runner     hard     nproc      4096
EOF

# Verify limits applied under the target user
sudo -u app-runner -s /bin/bash -c "ulimit -n; ulimit -u" 2>/dev/null || true
```

---

## 6. Best Practices

- **Always Create Service Accounts with `--system` and `/usr/sbin/nologin`**: Never run background services (`systemd` units, custom Python daemons) as `root` or as a regular human user account. Service accounts should not have a valid interactive login shell.
- **Always Use `-aG` When Appending Groups**: Missing the `-a` flag when running `usermod -G` silently strips the user from all other supplementary groups (including `sudo`), instantly locking developers out of administrative privileges upon logout.
- **Never Share Generic Accounts**: Avoid shared `deploy` or `admin` accounts. Every human engineer should have an individual named account mapped to their personal SSH public key for auditability.
- **Standardize UIDs Across Multi-Server Fleets**: If using NFS or shared storage, ensure UIDs and GIDs match across all servers. NFS maps permissions strictly by raw numeric UID, not by username string.
- **Automate Deprovisioning**: When an engineer leaves an organization, revoke access instantly by locking the account (`usermod -L`) and terminating all active sessions (`pkill -u <username>`).

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| Running `usermod -G group user` (omitting `-a`) | Overwrites supplementary group array with ONLY the listed group, removing user from `sudo`, `docker`, etc. | Always use `usermod -aG group user` to append safely. |
| User added to `docker` group cannot run `docker ps` | Group memberships are instantiated during the PAM login session; existing shells do not reflect group changes. | Run `newgrp docker` in the current terminal or log out and log back in. |
| Manually editing `/etc/passwd` or `/etc/shadow` with `vim` | Lacks syntax validation and file locking; concurrent writes or typos corrupt authentication across the entire OS. | Use `vipw` and `vipw -s` which enforce lockfiles and syntax checks. |
| Daemon fails with `Failed to set credentials` | Systemd service references a `User=` or `Group=` that has not been created on the system. | Verify user exists with `getent passwd <username>` before starting service. |
| Setting login shell to `/bin/false` instead of `/usr/sbin/nologin` | `/bin/false` exits silently with code 1; `/usr/sbin/nologin` politely notifies the user that the account is disabled. | Prefer `/usr/sbin/nologin` for clarity during SSH connection attempts. |
| NFS file permissions show incorrect username | The client machine and NFS server have different usernames mapped to the same numeric UID. | Harmonize UIDs/GIDs using LDAP/FreeIPA or align local user IDs. |

---

## 8. Performance, Security & Cost Implications

- **Performance**: Name lookups (`getpwnam`, `getpwuid`) query `/etc/nsswitch.conf`. If LDAP or Active Directory is configured as an NSS backend and the remote directory server experiences latency, every file listing (`ls -l`) that resolves UIDs to usernames will stall. Run `nscd` (Name Service Cache Daemon) or SSSD local caching to avoid network lookups.
- **Security**: The principle of least privilege mandates that processes hold only the credentials necessary to perform their task. Root compromises grant complete control over kernel memory, network interfaces, and storage. Running containers with non-root users (`USER 10001` in Dockerfiles) neutralizes a vast majority of container breakout vulnerabilities.
- **Cost**: Uncontrolled user accounts and unmonitored group grants (e.g., adding developers to the local `docker` group) grant effective root access, undermining compliance frameworks (SOC2, ISO 27001) and risking costly audit remediation.

---

## 9. Comparison Table

| Attribute | `useradd` (Low-Level Core Utility) | `adduser` (Debian/Ubuntu High-Level Wrapper) |
| :--- | :--- | :--- |
| **Origin** | Standard Linux `shadow-utils` package | Debian Perl wrapper script |
| **Cross-Distro Support** | Standard across all Linux (Ubuntu, RHEL, Alpine, Arch) | Ubuntu & Debian specific |
| **Interactive Prompts**| None (driven purely by flags; script-friendly) | Prompts for password, full name, room info |
| **Default Home Creation**| Does NOT create home unless `-m` is specified | Creates home directory automatically by default |
| **Shell Default** | Defaults to `/bin/sh` or system default | Defaults to `/bin/bash` |
| **Best Used For** | Automated CI/CD, Ansible, Dockerfiles, cloud-init | Interactive workstation user creation |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain the difference between Real UID (RUID), Effective UID (EUID), and Saved UID (SUID).</strong></summary>

**Answer**:
- **Real UID (RUID)**: Identifies the actual user who spawned the process. Set during login and inherited across `fork()`.
- **Effective UID (EUID)**: The identity evaluated by the Linux kernel when determining permissions for system calls (e.g., opening a file, binding to port 80, sending a signal to another process). Under normal conditions, EUID equals RUID. When executing a file with the SUID permission bit set (like `/usr/bin/passwd`), the kernel sets the EUID to the file owner's UID (root).
- **Saved UID (SUID)**: Used when a privileged process needs to temporarily drop privileges. A root process (EUID 0) can set its EUID to an unprivileged user (1000) to safely perform untrusted operations, and later restore its EUID back to 0 because the original privileged ID was preserved in the Saved UID.
</details>

<details>
<summary><strong>2. What is the difference between `/etc/passwd` and `/etc/shadow`? Why was `/etc/shadow` introduced?</strong></summary>

**Answer**:
Historically, password hashes were stored directly in the second field of `/etc/passwd`. However, `/etc/passwd` must be world-readable (`0644`) so standard utilities (`ls -l`, `ps`) can map numeric UIDs to human usernames. Any unprivileged local user could copy the password hashes and execute offline dictionary or brute-force cracking attacks.

`/etc/shadow` was introduced as part of the shadow password suite. Password hashes were moved from `/etc/passwd` (replaced by the placeholder `x`) into `/etc/shadow`, which has strict permissions (`0640` or `0600`, owned by `root:shadow`). Only root and privileged authentication binaries can read the hashes, protecting them from unauthorized extraction.
</details>

<details>
<summary><strong>3. Why is granting a user membership in the local `docker` group considered equivalent to granting root access?</strong></summary>

**Answer**:
The Docker daemon (`dockerd`) executes as `root` (UID 0) and listens on the Unix domain socket `/run/docker.sock`. By default, this socket is owned by `root:docker`.

Any user in the `docker` group can communicate with the daemon without `sudo`. A user can easily launch a container that binds the entire host filesystem to an internal directory with root permissions:
```bash
docker run -v /:/host -it ubuntu chroot /host
```
Inside this container, the user has an interactive root shell on the host filesystem, able to edit `/etc/shadow`, inject SSH keys into `/root/.ssh/authorized_keys`, or modify system binaries. Therefore, the `docker` group must be treated with the exact same security classification as full `sudo` privilege.
</details>

<details>
<summary><strong>4. How does `/usr/sbin/nologin` differ from `/bin/false`?</strong></summary>

**Answer**:
Both binaries are used to block interactive shell logins for service accounts.
- `/bin/false` is a basic POSIX binary that simply exits immediately with status code `1`. When a user attempts to log in over SSH, the connection terminates silently without explanation.
- `/usr/sbin/nologin` prints a polite, descriptive error message (e.g., *"This account is currently not available."* or custom text from `/etc/nologin.txt`) to stdout before exiting with a non-zero status code. This provides clear diagnostic feedback to engineers attempting to SSH into a service account.
</details>

<details>
<summary><strong>5. Explain the architecture of PAM (Pluggable Authentication Modules).</strong></summary>

**Answer**:
PAM decouples applications that require authentication (SSH, sudo, login, su) from the underlying authentication technologies (local `/etc/shadow`, LDAP, Kerberos, RADIUS, Duo MFA).

Instead of hardcoding authentication logic inside each application, the application calls the PAM API. PAM reads configuration files inside `/etc/pam.d/<service>` and executes configured shared libraries (`.so` modules) through four functional management groups:
1. `auth`: Authenticates user credentials.
2. `account`: Verifies account validity (e.g., expiration, time-of-day checks).
3. `password`: Handles password updates and complexity enforcement.
4. `session`: Configures environment upon login (e.g., mounting homes, setting ulimits).

Control flags (`required`, `requisite`, `sufficient`, `optional`) govern whether failure in one module blocks the entire chain.
</details>

<details>
<summary><strong>6. What happens if two different usernames in `/etc/passwd` share the same UID?</strong></summary>

**Answer**:
The Linux kernel operates exclusively on numeric UIDs, not string usernames. If two accounts share UID 0 (e.g., `root` and a rogue account named `admin`), both accounts have identical, complete administrative privilege across the kernel.

In userspace, commands that resolve UID to username (such as `ls -l` or `whoami`) will typically return whichever username appears first in `/etc/passwd`. Having duplicate UIDs is a severe security finding and common signature of a rootkit backdoor.
</details>

<details>
<summary><strong>7. Why do newly added group memberships not take effect immediately in an open terminal session?</strong></summary>

**Answer**:
When a user logs into Linux, the login process (via PAM) reads `/etc/group`, builds the supplementary group array, and assigns it to the login process's kernel credential structure (`struct cred`).

When child processes (such as a bash shell) are spawned, they inherit a copy of that credential structure via `fork()`. Modifying `/etc/group` updates the file on disk, but does not alter the credentials of already running processes in kernel memory. The user must initiate a new login session (or run the `newgrp <group>` command) to launch a new shell with refreshed group credentials.
</details>

<details>
<summary><strong>8. What is the GECOS field in `/etc/passwd`?</strong></summary>

**Answer**:
GECOS stands for General Electric Comprehensive Operating Supervisor, a legacy 1970s mainframe operating system. In early Unix installations at Bell Labs, Unix machines spooled batch print jobs to a GECOS mainframe. The 5th field in `/etc/passwd` was used to store the user's GECOS accounting identity.

Today, the GECOS field is a comma-separated string used to store administrative metadata about the user: Full Name, Building/Room Number, Office Phone, Home Phone, and Other information. Modern tools like `finger` or `chfn` read and update this field.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Query Process Credentials with `id`</strong></summary>

**Task**: Inspect your current user identity, primary group, and supplementary groups. Compare the output of `id -u`, `id -g`, and `id -Gn`.

**Solution**:
```bash
echo "Numeric UID:    $(id -u)"
echo "Numeric GID:    $(id -g)"
echo "Username:       $(id -un)"
echo "Primary Group:  $(id -gn)"
echo "All Group Names: $(id -Gn)"
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Provision a System Service User</strong></summary>

**Task**: Write the exact command to create a service user named `metrics-exporter` with UID 991, primary group `metrics-exporter`, home directory `/var/lib/metrics`, and no interactive login shell.

**Solution**:
```bash
sudo groupadd -g 991 metrics-exporter
sudo useradd --system \
  -u 991 \
  -g metrics-exporter \
  -d /var/lib/metrics \
  -m \
  -s /usr/sbin/nologin \
  metrics-exporter

# Verify
getent passwd metrics-exporter
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Audit Users with Sudo Access</strong></summary>

**Task**: Write a shell command to list all usernames that have administrative access via membership in the `sudo` (Ubuntu/Debian) or `wheel` (RHEL/CentOS) groups.

**Solution**:
```bash
getent group sudo wheel 2>/dev/null | awk -F: '{print "Group: " $1 " | Members: " $4}'
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Identify Orphaned Files Lacking Valid User Owners</strong></summary>

**Task**: When a user is deleted without `-r`, their files remain on disk with orphaned numeric UIDs. Write a `find` command to discover all files on the filesystem that do not map to any existing user in `/etc/passwd`.

**Solution**:
```bash
sudo find /home /var /tmp -nouser -ls 2>/dev/null
# Files showing raw numbers instead of usernames indicate orphaned files.
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Scripted Password Aging Audit and Expiration Alert</strong></summary>

**Task**: Author a script that parses `/etc/shadow` and reports all accounts whose passwords will expire within the next 14 days.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

CURRENT_DAYS=$(( $(date +%s) / 86400 ))
WARN_WINDOW=14

sudo awk -F: -v now="$CURRENT_DAYS" -v win="$WARN_WINDOW" '
$5 != "" && $5 != "99999" && $2 !~ /^(!|\*)/ {
    last_change = $3
    max_days = $5
    expire_day = last_change + max_days
    days_left = expire_day - now
    if (days_left <= win && days_left >= 0) {
        printf "WARNING: Account %s expires in %d days!\n", $1, days_left
    } else if (days_left < 0) {
        printf "EXPIRED: Account %s expired %d days ago!\n", $1, -days_left
    }
}' /etc/shadow
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Validate SUID Execution Privilege Shifts</strong></summary>

**Task**: Write a minimal C program that prints its Real, Effective, and Saved UIDs. Compile it, set the SUID bit as root, execute it as a regular user, and observe the privilege transition.

**Solution**:
```bash
# 1. Author test C program
cat << 'EOF' > /tmp/uid_test.c
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>

int main() {
    uid_t ruid, euid, suid;
    getresuid(&ruid, &euid, &suid);
    printf("Real UID: %d | Effective UID: %d | Saved UID: %d\n", ruid, euid, suid);
    return 0;
}
EOF

# 2. Compile binary
gcc /tmp/uid_test.c -o /tmp/uid_test

# 3. Elevate ownership and set SUID bit
sudo chown root:root /tmp/uid_test
sudo chmod 4755 /tmp/uid_test

# 4. Execute as regular user
/tmp/uid_test
# Output: Real UID: 1000 | Effective UID: 0 | Saved UID: 0
# Notice Effective UID is elevated to 0 (root)!

# Cleanup
sudo rm -f /tmp/uid_test /tmp/uid_test.c
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Command | Description |
| :--- | :--- | :--- |
| **Create System User** | `sudo useradd --system -s /usr/sbin/nologin <name>` | Unprivileged service runner |
| **Create Regular User**| `sudo useradd -m -s /bin/bash <name>` | Human developer account with home |
| **Append Group** | `sudo usermod -aG <group> <user>` | Safely append supplementary group |
| **Lock Account** | `sudo usermod -L <user>` | Disables password login immediately |
| **Delete User & Home** | `sudo userdel -r <user>` | Deletes account and purges `/home/<user>` |
| **Check Credentials** | `id <user>` | Displays UID, GID, and groups |
| **Check Password Aging**| `sudo chage -l <user>` | Displays expiration and warning schedule |
| **Safe File Edits** | `sudo vipw` / `sudo vipw -s` | Validates syntax and locks `/etc/passwd` |

---

## 13. Further Reading & Official Documentation

- [man 5 passwd: Password File Format](https://man7.org/linux/man-pages/man5/passwd.5.html)
- [man 5 shadow: Shadow Password File Format](https://man7.org/linux/man-pages/man5/shadow.5.html)
- [man 8 useradd: Create a New User](https://man7.org/linux/man-pages/man8/useradd.8.html)
- [Linux-PAM System Administrator Guide](https://www.linux-pam.org/Linux-PAM-html/Linux-PAM_SAG.html)
- [Red Hat Enterprise Linux 9: Managing Users and Groups](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/9/html/configuring_basic_system_settings/managing-users-and-groups_configuring-basic-system-settings)

---

[<- 05-links-inodes-and-file-types.md](./05-links-inodes-and-file-types.md) | [Back to Module Overview](./README.md) | [07-permissions.md ->](./07-permissions.md)
