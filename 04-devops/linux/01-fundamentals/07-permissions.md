<!--
================================================================================
Metadata:
  Title: Linux Permissions: Mode Bits, Umask, Special Bits & POSIX ACLs
  Track: Linux & Systems Engineering / DevOps
  Difficulty: Intermediate
  Estimated Time: 55 minutes
  Prerequisites: 01-fundamentals/06-users-and-groups.md
  Key Terminology: rwx, Octal Notation, umask, SUID, SGID, Sticky Bit, Capital X, POSIX ACLs, getfacl, setfacl
================================================================================
-->

# Linux Permissions: Mode Bits, Umask, Special Bits & POSIX ACLs

[<- 06-users-and-groups.md](./06-users-and-groups.md) | [Back to Module Overview](./README.md) | [08-sudo-and-privilege.md ->](./08-sudo-and-privilege.md)

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

In multi-tenant cloud environments, automated deployment pipelines, and shared servers, filesystem permissions enforce the foundational security perimeter. They dictate which processes can read application configurations, write database transactions, execute automation scripts, or bind to IPC domain sockets.

Misconfigured permissions represent one of the most prevalent causes of catastrophic security breaches and operational downtime:
- Running `chmod -R 777 /var/www` exposes private cryptographic keys and database credentials to any unprivileged local process or web shell.
- Setting restrictive permissions on directories without understanding the directory execute bit (`x`) locks applications out of files even when file-level permissions are correct.
- High-scale team collaboration on shared directories requires **SGID bits** and **POSIX Access Control Lists (ACLs)** to ensure newly generated files inherit uniform group permissions without manual administrative intervention.

---

## 2. Core Concepts

### The Standard 3-Tier Permission Triad
Every Linux file and directory assigns permissions across three distinct classes:
1. **User (`u`)**: The owner of the file.
2. **Group (`g`)**: The group assigned to the file.
3. **Others (`o`)**: All other user accounts on the operating system.

Each class can possess three primary access modes:
- **Read (`r`)**: Binary value `4`
- **Write (`w`)**: Binary value `2`
- **Execute (`x`)**: Binary value `1`

```
Mode String: - r w x r - x r - -
             | \___/ \___/ \___/
             |   |     |     |
             |   |     |     +-> Others: Read only (4 = 4)
             |   |     +-------> Group: Read & Execute (4 + 1 = 5)
             |   +-------------> Owner: Read, Write & Execute (4 + 2 + 1 = 7)
             +-----------------> File Type: Regular File (-)
             Octal: 0754
```

### The Critical Difference: File vs. Directory Permissions
A common stumbling block for engineers is assuming permissions behave identically on files and directories:

| Permission Bit | Behavior on a Regular File | Behavior on a Directory |
| :---: | :--- | :--- |
| **`r` (Read)** | Allows reading byte contents (e.g. `cat`, `less`) | Allows reading the directory table / listing filenames (`ls`) |
| **`w` (Write)** | Allows modifying or appending data blocks | Allows **creating, renaming, or deleting** files within the directory |
| **`x` (Execute)**| Allows running the file as an ELF binary or script | Allows **traversing** into the directory (`cd`) and accessing file inodes |

*Crucial Takeaway*: To delete a file, you do **not** need write permission on the file itself; you need write (`w`) and execute (`x`) permissions on the **parent directory** containing the file!

---

## 3. How It Works Internally

### Inode Mode Bits Layout
In the Linux kernel, permissions and special bits are stored in the `i_mode` 16-bit integer field inside `struct inode`:

```
+-------------------------------------------------------------------------+
| INODE 16-BIT MODE ARCHITECTURE (i_mode)                                 |
|                                                                         |
| Bits:  [15 .. 12]  [11 .. 9]   [8 .. 6]    [5 .. 3]    [2 .. 0]         |
| Field: File Type   Special     Owner       Group       Others           |
| Value: (reg, dir)  (s, s, t)   (r, w, x)   (r, w, x)   (r, w, x)        |
+-------------------------------------------------------------------------+
| SPECIAL BITS BREAKDOWN:                                                 |
| - Bit 11 (Octal 4000): SUID (Set User ID)                               |
|   -> Process executes with Effective UID of file OWNER.                 |
| - Bit 10 (Octal 2000): SGID (Set Group ID)                              |
|   -> Executable runs with Effective GID of file GROUP.                  |
|   -> On a Directory: Newly created files INHERIT directory's GID!       |
| - Bit 9  (Octal 1000): Sticky Bit                                       |
|   -> On a Directory: ONLY file owner or root can delete/rename files.   |
+-------------------------------------------------------------------------+
```

### The `umask` (User Mask) Calculation
When a process creates a new file or directory via system calls (`open(O_CREAT)` or `mkdir`), it passes requested base mode bits:
- Default base mode for newly created **Files**: `0666` (`rw-rw-rw-`)
- Default base mode for newly created **Directories**: `0777` (`rwxrwxrwx`)

The kernel applies the process's active `umask` using bitwise negation (`mode & ~umask`):

```
Base File Mode:       0666  ->  (rw- rw- rw-)
Default System Umask: 0022  ->  (--- -w- -w-)
Resulting File Mode:  0644  ->  (rw- r-- r--)

Base Directory Mode:  0777  ->  (rwx rwx rwx)
Default System Umask: 0022  ->  (--- -w- -w-)
Resulting Dir Mode:   0755  ->  (rwx r-x r-x)
```

If the umask is hardened to `0027` (common in enterprise servers):
- New files: `0666 & ~0027 = 0640` (`rw-r-----`)
- New directories: `0777 & ~0027 = 0750` (`rwxr-x---`)
*Others have zero access!*

---

## 4. Syntax & Usage

### Standard Permission Management (`chmod`, `chown`, `chgrp`)
```bash
# Octal modification: Set owner=rwx (7), group=rx (5), others=rx (5)
chmod 755 /usr/local/bin/deploy.sh

# Symbolic modification: Add execute to user; remove write from group & others
chmod u+x,go-w script.sh

# Recursive adjustment using Capital 'X' (Adds execute ONLY to directories and already-executable files)
chmod -R a+rX /var/www/html/

# Change file owner and group simultaneously
sudo chown nginx:nginx /var/www/html/index.html

# Change owner recursively
sudo chown -R www-data:www-data /var/www/
```

### Special Permission Bits (SUID, SGID, Sticky)
```bash
# Set SUID on an executable (4000)
sudo chmod 4755 /usr/local/bin/custom-elevated-tool
# Displays in ls -l as: -rwsr-xr-x

# Set SGID on a collaborative team directory (2000)
sudo chmod 2775 /opt/shared-team/
# Displays in ls -l as: drwxrwsr-x

# Set Sticky Bit on a world-writable directory (1000)
sudo chmod 1777 /tmp/shared-scratch/
# Displays in ls -l as: drwxrwxrwt
```

### POSIX Access Control Lists (ACLs)
When standard 3-tier permissions are insufficient to express complex access policies, use POSIX ACLs:

```bash
# View active ACL rules on a file or directory
getfacl /var/log/nginx/access.log

# Grant specific user 'developer' read and write access
setfacl -m u:developer:rw- /var/log/nginx/access.log

# Grant specific group 'auditors' read-only access
setfacl -m g:auditors:r-- /var/log/nginx/access.log

# Set DEFAULT ACL on a directory (automatically inherited by all future child files)
setfacl -d -m g:auditors:r-x /opt/shared-repo/

# Remove a specific user ACL rule
setfacl -x u:developer /var/log/nginx/access.log

# Strip all extended ACLs from a file (revert to pure standard mode bits)
setfacl -b /var/log/nginx/access.log
```
*Note*: When a file possesses extended ACLs, `ls -l` appends a `+` sign to the permission string: `-rw-r-----+`.

---

## 5. Practical Examples

### Example 1: Creating a Multi-User Collaborative Directory with SGID
```bash
# Requirement: Members of group 'devops' need to create, edit, and delete files in /opt/devops-collab.
# Every newly created file must automatically belong to 'devops', regardless of who creates it.

# 1. Create group and directory
sudo groupadd devops
sudo mkdir -p /opt/devops-collab

# 2. Assign ownership to root and devops group
sudo chown root:devops /opt/devops-collab

# 3. Apply mode 2775 (rwxrwsr-x) -> 2 = SGID bit
sudo chmod 2775 /opt/devops-collab

# 4. Verify directory mode
ls -ld /opt/devops-collab
# drwxrwsr-x 2 root devops 4096 Nov 25 12:00 /opt/devops-collab

# When 'john' creates a file inside, it automatically inherits group 'devops' instead of john's primary group!
```

### Example 2: Hardening Shared Team Directories with Default ACLs
```bash
# Enhance Example 1: Ensure newly created files have group read-write, even if user has umask 0027
sudo setfacl -d -m g:devops:rwx /opt/devops-collab

# Inspect inherited default ACLs
getfacl /opt/devops-collab
# Output includes:
# default:user::rwx
# default:group::rwx
# default:group:devops:rwx
# default:mask::rwx
# default:other::r-x
```

### Example 3: Auditing SUID and SGID Binaries Across the System
```bash
# Security requirement: Regularly scan for unauthorized SUID/SGID binaries (privilege escalation audit)
# Find all files with SUID (perm -4000) or SGID (perm -2000) owned by root
sudo find / -xdev \( -perm -4000 -o -perm -2000 \) -type f -exec ls -la {} + 2>/dev/null | awk '{print $1, $3, $4, $9}'

# Sample standard output:
# -rwsr-xr-x root root /usr/bin/passwd
# -rwsr-xr-x root root /usr/bin/sudo
# -rwsr-xr-x root root /usr/bin/chfn
# -rwxr-sr-x root shadow /usr/sbin/unix_chkpwd
```

### Example 4: Safely Fixing Web Server Permissions with Capital `X`
```bash
# Common disaster: Running 'chmod -R 755 /var/www' makes every image, CSS, and .env executable!
# Production pattern: Make files readable (0644) and directories traversable (0755) in one command:

# 1. Standardize all files to 0644
sudo find /var/www/html -type f -exec chmod 0644 {} +

# 2. Standardize all directories to 0755
sudo find /var/www/html -type d -exec chmod 0755 {} +

# OR use Capital 'X' syntax:
sudo chmod -R u=rwX,go=rX /var/www/html
```

### Example 5: Securing SSH Keys and Configuration Files
```bash
# OpenSSH strictly rejects private keys and configs with loose permissions
# Apply standard CIS Benchmark permissions:

chmod 0700 ~/.ssh
chmod 0600 ~/.ssh/id_ed25519
chmod 0644 ~/.ssh/id_ed25519.pub
chmod 0600 ~/.ssh/authorized_keys
chmod 0600 ~/.ssh/config

# Verify permissions
ls -la ~/.ssh
# Output confirms no group or world access on private keys or authorized_keys
```

---

## 6. Best Practices

- **Never Use `chmod 777` as a "Quick Fix"**: `777` allows any arbitrary process or local attacker to overwrite, truncate, or inject malicious code into the file. Debug permission denials systematically by checking ownership and directory traversal bits.
- **Always Use the Capital `X` Flag with Recursive `chmod`**: `chmod -R +rX` ensures directories receive the execute bit so they can be traversed, while plain regular data files remain non-executable.
- **Enforce a Strict System Umask (`0027`)**: Configure `/etc/login.defs` and `/etc/profile` with `UMASK 027`. This guarantees that files created by daemons and users are invisible to untrusted local users.
- **Audit SUID Binaries in CI/CD Container Scans**: Minimize SUID binaries in container base images. If an application does not need `/usr/bin/chfn` or `passwd`, purge them (`chmod u-s /path/to/binary` or remove the package).
- **Use SGID on Shared Ingestion Directories**: Prevent cross-functional operational friction by placing SGID (`chmod 2770`) on directories shared between disparate service accounts (e.g., FTP uploads processed by background workers).

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| `Permission denied` when running `ls /dir` despite having `r` permission | The directory lacks the execute (`x`) traversal bit; `r` only allows reading names, but cannot query inodes. | Add execute permission to directory: `chmod +x /dir`. |
| Cannot delete a file despite having `rw-rw-rw-` (0666) permissions | File deletion is governed by the *parent directory's* write permission, not the file's mode. | Grant write permission on parent directory: `chmod u+w /parent_dir`. |
| Cannot delete a file in `/tmp` despite `rwxrwxrwx` (0777) permissions | `/tmp` has the Sticky Bit (`1777`) enabled; only the file owner or root can delete files inside. | Switch to file owner or root: `sudo rm /tmp/file`. |
| Running `chmod 755 ~/.ssh/id_rsa` breaks SSH logins | OpenSSH client checks private key permissions; if readable by group/others, SSH terminates with `Permissions 0755 are too open`. | Restore strict permissions: `chmod 0600 ~/.ssh/id_rsa`. |
| Setting SUID on a shell script (`#!/bin/bash`) has no effect | The Linux kernel deliberately ignores the SUID bit on interpreted scripts to prevent environment race condition exploits. | Write a compiled C wrapper or use `sudo` with fine-grained rules. |
| Mask in ACL (`mask::r--`) overrides group permissions | The POSIX ACL `mask` entry defines the maximum effective permissions for all named users and groups. | Update ACL mask: `setfacl -m m:rwx <file>`. |

---

## 8. Performance, Security & Cost Implications

- **Performance**: Evaluating standard 3-tier mode bits takes nanoseconds within the VFS layer (`vfs_permission`). Extended POSIX ACLs require extra inode lookups and parsing of extended attributes (`xattr`), incurring a minor cache lookup overhead on metadata-heavy workloads.
- **Security**: SUID binaries running as root are the classic vector for local privilege escalation (LPE). If an SUID binary contains a buffer overflow or shell escape sequence (e.g., misconfigured `/usr/bin/find` with SUID), any unprivileged user can instantly gain an interactive root shell.
- **Cost**: Restrictive umasks and proper ACL segmentation prevent unauthorized data tampering and eliminate expensive post-incident forensic cleanups resulting from leaked secrets.

---

## 9. Comparison Table

| Attribute | Standard POSIX Mode Bits | POSIX Access Control Lists (ACLs) |
| :--- | :--- | :--- |
| **Granularity** | 3 entities: Owner, Group, Others | Unlimited specific users (`u:name`) and groups (`g:name`) |
| **Inspection Tool** | `ls -l` / `stat` | `getfacl` (indicated by `+` in `ls -l`) |
| **Modification Tool**| `chmod` | `setfacl` |
| **Inheritance Support**| Minimal (SGID inherits group) | Complete (`default:` ACLs automatically apply to children) |
| **Storage Location** | Native Inode `i_mode` field (16 bits) | Filesystem Extended Attributes (`xattr`) |
| **Filesystem Support**| All Unix filesystems | ext4, XFS, Btrfs, ZFS (requires `acl` mount option) |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Why is the execute (`x`) permission bit required on a directory to read files inside it?</strong></summary>

**Answer**:
In Unix directory architecture, a directory is a table mapping filenames to inode numbers.
- The **Read (`r`)** bit on a directory permits reading the directory table itself. This allows a user to run `ls` and see the list of filename strings.
- The **Execute (`x`)** bit (also known as the *search* or *traversal* bit) allows the kernel to resolve path lookups and query the underlying inodes of files inside that directory.

If a user has `r` but lacks `x` on `/data`:
Running `ls /data` will list the filenames, but running `ls -l /data` or `cat /data/file.txt` fails with `Permission denied` because the kernel cannot traverse into the directory to fetch file sizes, timestamps, or data blocks.
</details>

<details>
<summary><strong>2. Explain the purpose and operation of the Sticky Bit on `/tmp`.</strong></summary>

**Answer**:
The `/tmp` directory must be world-writable (`rwxrwxrwx` / `0777`) so any running application or user can create temporary files. However, under standard Unix semantics, if a directory is world-writable, any user can delete or rename *any* file inside that directory, even if they do not own it.

The **Sticky Bit** (octal `1000`, represented as `t` in `drwxrwxrwt`) modifies directory deletion semantics: when enabled, the kernel permits a file to be deleted or renamed **only by**:
1. The owner of the file.
2. The owner of the directory (root).
3. The superuser (root).

This prevents malicious users from deleting other users' temporary sockets, PID files, or working documents in `/tmp`.
</details>

<details>
<summary><strong>3. How does SGID behave differently when applied to an executable file versus a directory?</strong></summary>

**Answer**:
- **On an Executable File**: When executed, the process's **Effective GID (EGID)** is set to the group ID of the executable file, rather than the GID of the user running it. This grants the process the permissions of the file's group (e.g., `/usr/bin/wall` running with `tty` group permissions to write to terminals).
- **On a Directory**: Newly created subdirectories and files automatically **inherit the group ownership** of the parent directory, rather than the primary group of the user creating the file. Furthermore, newly created subdirectories automatically inherit the SGID bit. This mechanism is the industry standard for configuring shared collaborative directories.
</details>

<details>
<summary><strong>4. Why does the Linux kernel ignore the SUID bit on interpreted scripts (`#!/bin/bash` or `#!/usr/bin/python3`)?</strong></summary>

**Answer**:
The kernel ignores SUID on interpreted scripts due to an inherent Time-of-Check to Time-of-Use (**TOCTOU**) race condition:
1. When a user executes a script `/usr/local/bin/backup.sh`, the kernel opens the file, sees the shebang (`#!/bin/bash`), and elevates privileges to root.
2. The kernel then executes the interpreter: `/bin/bash /usr/local/bin/backup.sh`.
3. Between the time the kernel reads the shebang and the interpreter opens the target script file, an attacker can replace the target script with a symlink to a malicious script. The interpreter would then execute the attacker's script with full root privileges.

To eliminate this vulnerability, modern Linux kernels ignore the SUID and SGID bits on scripts and only enforce them on compiled ELF binary executables.
</details>

<details>
<summary><strong>5. What is the difference between an ACL `mask` and standard group permissions?</strong></summary>

**Answer**:
When POSIX Access Control Lists (ACLs) are configured on a file, the traditional group permission bits (the middle triad in `ls -l`) are redefined to represent the **ACL Mask**.

The ACL mask defines the **maximum effective permissions** that can be granted to:
1. The owning group.
2. All named users (`u:username`).
3. All named groups (`g:groupname`).

If a named user is granted `rwx` via `setfacl`, but the mask is set to `r--` (`mask::r--`), the user's *effective* permission is strictly limited to read-only (`r--`). The mask acts as a dynamic ceiling to prevent accidental over-permissioning.
</details>

<details>
<summary><strong>6. Explain the function of the Capital `X` permission in `chmod`.</strong></summary>

**Answer**:
The capital `X` permission in symbolic `chmod` (e.g., `chmod -R a+rX .`) is a conditional execute flag. It applies the execute bit (`x`) **only if**:
1. The target is a **directory** (ensuring it can be traversed).
2. Or the target is a regular file that **already has execute permission** for at least one user class (owner, group, or other).

If a regular file is non-executable (e.g., a `.jpg` image or `.conf` file), capital `X` leaves it non-executable. This allows engineers to recursively grant read and directory access across entire directory trees without inadvertently converting every data file into an executable binary.
</details>

<details>
<summary><strong>7. How is a process's effective permission evaluated when accessing a file?</strong></summary>

**Answer**:
The Linux kernel's VFS layer evaluates permissions in a strict, sequential order:
1. **Root Check**: If the process has Effective UID 0 (root), all read and write permissions are immediately granted. Execute is granted if at least one execute bit is set anywhere on the file.
2. **Owner Check**: If the process EUID matches the file's Owner UID, the **Owner bits** are evaluated. *Evaluation stops here.* (If owner permissions deny access, access is denied immediately, even if group or others have access!).
3. **Named User ACLs**: If POSIX ACLs exist, checks if process EUID matches any named user.
4. **Group Check**: If process EGID or any supplementary GID matches the file's Group GID or named group ACLs, **Group bits** (filtered by mask) are evaluated. *Evaluation stops here.*
5. **Other Check**: If neither user nor group matches, **Others bits** are evaluated.
</details>

<details>
<summary><strong>8. What is the security risk of having an unconstrained `umask 0000`?</strong></summary>

**Answer**:
A `umask 0000` negates zero permission bits during file and directory creation.
- Any newly created regular file will have mode `0666` (`rw-rw-rw-`).
- Any newly created directory will have mode `0777` (`rwxrwxrwx`).

Every configuration file, private token, log file, and database dump generated by processes under this umask becomes world-readable and world-writable by any unprivileged user, local service account, or web shell on the operating system, completely destroying system integrity and confidentiality.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Calculate Octal and Umask Values</strong></summary>

**Task**: Calculate the resulting octal permission mode for:
1. A directory created under `umask 0077`.
2. A file created under `umask 0027`.
3. A file with symbolic mode `u=rwx,g=rx,o=`.

**Solution**:
1. Base directory `0777 & ~0077 = 0700` (`drwx------`)
2. Base file `0666 & ~0027 = 0640` (`-rw-r-----`)
3. Symbolic mode: `u=7, g=5, o=0` -> Octal `0750`
</details>

<details>
<summary><strong>Exercise 2 (Easy): Set Up a Collaborative SGID Folder</strong></summary>

**Task**: Create a directory `/tmp/team_collab`, assign it to group `adm`, set the SGID bit, and verify that new files created inside inherit the `adm` group.

**Solution**:
```bash
sudo mkdir -p /tmp/team_collab
sudo chgrp adm /tmp/team_collab
sudo chmod 2775 /tmp/team_collab

# Test file creation
touch /tmp/team_collab/test_file.txt
ls -l /tmp/team_collab/test_file.txt
# Output shows group ownership is 'adm' (inherited from directory)!

# Cleanup
sudo rm -rf /tmp/team_collab
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Configure Fine-Grained Access with POSIX ACLs</strong></summary>

**Task**: Create a file owned by `root:root` with mode `0600`. Use `setfacl` to grant user `daemon` read-only access and verify using `getfacl`.

**Solution**:
```bash
# Create file
sudo touch /tmp/secure_report.txt
sudo chmod 0600 /tmp/secure_report.txt

# Grant daemon user read access
sudo setfacl -m u:daemon:r-- /tmp/secure_report.txt

# Inspect ACLs
getfacl /tmp/secure_report.txt
# Verify user:daemon:r-- appears and ls -l shows '+'

# Cleanup
sudo rm -f /tmp/secure_report.txt
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Enforce Default ACLs on Directory Ingestion</strong></summary>

**Task**: Configure a directory `/tmp/incoming` such that any file created inside it automatically grants user `nobody` read access via default ACLs.

**Solution**:
```bash
mkdir -p /tmp/incoming
setfacl -d -m u:nobody:r-- /tmp/incoming

# Create test file
touch /tmp/incoming/upload.dat

# Verify inherited ACL on the child file
getfacl /tmp/incoming/upload.dat | grep "user:nobody:r--"
# Output confirms inheritance!

# Cleanup
rm -rf /tmp/incoming
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Audit World-Writable Files and Insecure Permissions</strong></summary>

**Task**: Author a hardening verification script that scans the filesystem for files that are world-writable or have SUID/SGID set.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

echo "=== SCANNING FOR INSECURE WORLD-WRITABLE FILES (excluding /proc and /sys) ==="
sudo find / -xdev -type f -perm -0002 ! -path "/proc/*" ! -path "/sys/*" -ls 2>/dev/null | head -n 10

echo -e "\n=== SCANNING FOR ROOT-OWNED SUID BINARIES ==="
sudo find / -xdev -user root -perm -4000 -type f -ls 2>/dev/null
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Demonstrate Owner Precedence Lockout</strong></summary>

**Task**: Prove that Linux evaluates Owner permissions first without falling through to Group permissions. Create a file where Owner has NO access (`000`), but Group has full access (`070`). Attempt to read the file as the Owner and observe the result.

**Solution**:
```bash
# Create file owned by current user and current group
touch /tmp/precedence_test.txt
chown $USER:$(id -gn) /tmp/precedence_test.txt

# Set owner to 0 (---), group to 7 (rwx), others to 0 (---)
chmod 070 /tmp/precedence_test.txt
ls -l /tmp/precedence_test.txt
# Output: ----rwx--- 1 ubuntu ubuntu 0 Nov 25 12:00 /tmp/precedence_test.txt

# Attempt to read as owner (who is also a member of group ubuntu)
cat /tmp/precedence_test.txt
# Output: cat: /tmp/precedence_test.txt: Permission denied!
# Proof: The kernel stopped evaluation at the Owner match and denied access immediately!

# Cleanup
rm -f /tmp/precedence_test.txt
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Command | Description |
| :--- | :--- | :--- |
| **Set Octal Mode** | `chmod 755 <path>` | Owner=rwx, Group=rx, Others=rx |
| **Set Capital X** | `chmod -R a+rX <path>` | Execute applied only to dirs & existing execs |
| **Set SUID** | `chmod 4755 <binary>` | Runs with owner's Effective UID |
| **Set SGID** | `chmod 2775 <directory>` | Children inherit directory's Group GID |
| **Set Sticky Bit** | `chmod 1777 <directory>` | Only owner or root can delete files |
| **View ACLs** | `getfacl <path>` | Displays full extended ACL matrix |
| **Set User ACL** | `setfacl -m u:<user>:rw <path>` | Grants user specific read/write access |
| **Set Default ACL** | `setfacl -d -m g:<group>:rx <dir>` | Children inherit group read/execute access |
| **Remove ACLs** | `setfacl -b <path>` | Strips all extended ACL rules |

---

## 13. Further Reading & Official Documentation

- [man 1 chmod: Change File Mode Bits](https://man7.org/linux/man-pages/man1/chmod.1.html)
- [man 2 umask: Set File Mode Creation Mask](https://man7.org/linux/man-pages/man2/umask.2.html)
- [man 5 acl: POSIX Access Control Lists](https://man7.org/linux/man-pages/man5/acl.5.html)
- [man 1 setfacl: Set File Access Control Lists](https://man7.org/linux/man-pages/man1/setfacl.1.html)
- [CIS Linux Benchmark: Filesystem Integrity and Permissions](https://www.cisecurity.org/benchmark/ubuntu_linux)

---

[<- 06-users-and-groups.md](./06-users-and-groups.md) | [Back to Module Overview](./README.md) | [08-sudo-and-privilege.md ->](./08-sudo-and-privilege.md)
