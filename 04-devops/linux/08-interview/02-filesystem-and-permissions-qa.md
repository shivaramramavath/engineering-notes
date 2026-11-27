# Filesystems, Inodes & Permissions: Technical Interview Q&A

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Filesystems, Inodes, Directory Mechanics & Permission Systems |
| **Module** | `08-interview` — Technical Interview Preparation & Scenario Q&A |
| **Track** | `04-devops/linux` |
| **Target Level**| Mid to Senior DevOps / SRE / Systems Engineers |
| **Questions** | 20 Deep Scenario & Architecture Questions |
| **Last Updated**| 2026 |

---

## Overview

Interviewers use filesystem and permission questions to test whether a candidate understands the structural internals of the operating system or merely memorizes basic `chmod` numbers. Expect detailed inquiries into inode structures, hardlink mechanics, and filesystem space discrepancies under production load.

---

### Q1: What is an Inode, and what specific metadata does it store (and NOT store)?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**What an Inode IS:**
An **inode (index node)** is a fixed-size data structure (typically 256 bytes in Ext4/XFS) stored in an inode table on disk that uniquely identifies a filesystem object.

**Metadata Stored Inside the Inode:**
1. File type (regular file, directory, symlink, block device, socket, FIFO).
2. POSIX permissions (mode bits: read, write, execute, SUID, SGID, sticky).
3. Owner User ID (UID) and Group ID (GID).
4. File size in bytes.
5. Timestamps: Access (`atime`), Modification (`mtime`), and Change/Metadata (`ctime`), plus creation/birth (`btime`) on supported filesystems.
6. Hard link count (number of directory entries pointing to this inode).
7. Extended attributes (xattrs) / ACL pointers.
8. Pointers to physical disk data blocks (extents).

**What an Inode DOES NOT Store:**
- **The Filename**: Filenames are stored exclusively inside **Directory Entries (dentries)** in the parent directory's data blocks, mapping names to inode numbers.
- **The File Path**: Filesystems do not store full pathnames; the path is resolved dynamically by traversing the tree of directory entries.
</details>

---

### Q2: What is the fundamental difference between a Hard Link and a Symbolic (Soft) Link?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

| Metric / Property | Hard Link (`ln file link`) | Symbolic Link (`ln -s file link`) |
| :--- | :--- | :--- |
| **Target Representation** | Directly points to the existing **inode number**. | Independent new file containing the **target path string**. |
| **Inode Number** | Identical inode number as the source file. | Distinct, brand-new inode number. |
| **Cross-Filesystem?** | **No**. Inode numbers are unique only within a specific filesystem. | **Yes**. Can point across different mount points and partitions. |
| **Can Link Directories?** | **No** (prohibited to prevent infinite directory loops). | **Yes**. Directory symlinks are standard. |
| **Source File Deletion** | Target file data remains intact; link count drops by 1. | Link is broken / dangles (`dangling symlink`). |
| **Permission Bits** | Permissions are identical (shared inode). | Symlink permissions are always `lrwxrwxrwx` (target permissions apply). |

**Interview Scenario Question:**
*Can you create a hard link to an NFS mount or across different disk partitions?*
No, because inode table indexes are strictly local to individual block devices.
</details>

---

### Q3: Why does `df -h` show 100% disk usage while `du -sh /*` accounts for only 20GB on a 100GB drive?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Root Cause:**
This is the classic **Unlinked Open File Descriptor** discrepancy.

When a file is deleted (`rm /var/log/app.log`), the directory entry (dentry) is unlinked from the directory tree, making the file completely invisible to recursive directory walkers like `du` (Disk Usage). However, if an active running process (such as Nginx, Java, or an unrotated logging daemon) still holds an open file descriptor pointing to that inode, the Linux kernel **refuses to deallocate the physical data blocks** until the process closes the file descriptor or terminates. `df` (Disk Free) queries the filesystem superblock directly, which correctly reports that the blocks remain allocated.

**Diagnostic & Resolution Runbook:**
1. Identify the holding process using `lsof`:
   ```bash
   sudo lsof +L1 # Or: sudo lsof | grep deleted
   ```
2. Reclaim disk space without restarting the production process by truncating the file descriptor directly through `/proc`:
   ```bash
   sudo truncate -s 0 /proc/<PID>/fd/<FD_NUM>
   ```
</details>

---

### Q4: An application reports `No space left on device`, yet `df -h` reports 50GB of free space. What is the root cause?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Root Cause:**
**Inode Exhaustion**.
Filesystems like Ext4 allocate a fixed, finite number of inodes when formatted (`mkfs.ext4`). Every file, directory, symlink, or 0-byte file consumes exactly one inode. If an application generates millions of microscopic files (e.g., stuck email queues in `/var/spool/postfix/maildrop`, uncollected PHP session files in `/tmp`, or unpruned cache files), the filesystem exhausts its inode table even if the physical disk has hundreds of gigabytes of free block capacity.

**Diagnosis Command:**
```bash
df -ih # Inspect Inode usage percentage (IUse%)
```

**Resolution:**
1. Locate the directory hoarding inodes:
   ```bash
   sudo find / -xdev -printf '%h\n' 2>/dev/null | sort | uniq -c | sort -rn | head -n 10
   ```
2. Purge the files using `find`:
   ```bash
   find /path/to/hog -type f -delete # Never use rm * which fails with "Argument list too long"
   ```
</details>

---

### Q5: What do Read (`r`), Write (`w`), and Execute (`x`) permissions mean on a DIRECTORY vs. a REGULAR FILE?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

| Permission Bit | Meaning on Regular File | Meaning on Directory |
| :--- | :--- | :--- |
| **Read (`r`)** | Read the contents/bytes of the file. | List the names of files inside the directory (`ls`). |
| **Write (`w`)** | Modify, overwrite, or append bytes inside the file. | Create, delete, or rename entries inside the directory. |
| **Execute (`x`)** | Execute file as a program or binary. | **Traverse / Enter the directory** (`cd`), and access file metadata inside. |

**Critical Production Gotcha:**
If a directory has `r--` permissions without `x`, a user can list the filenames via `ls`, but running `ls -l` will show question marks `????` for file sizes and owners, and `cat /dir/file.txt` will fail with `Permission denied`! You must have `+x` on *every single parent directory* along a path to access a file.
</details>

---

### Q6: Who has the authority to delete a file? Can an unprivileged user delete a read-only root-owned file?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Mechanism:**
Deleting a file does **NOT** modify the file itself; it modifies the **parent directory's** data blocks by unlinking the directory entry. Therefore, permission to delete a file depends strictly on having **Write (`w`) and Execute (`x`) permissions on the parent directory**, completely regardless of who owns the file or what the file's individual permissions are!

**Scenario:**
If `/data/shared` has permissions `777` (`rwxrwxrwx`), an unprivileged user `bob` can successfully delete `/data/shared/root_config.yml` even if the file is owned by `root:root` with permissions `400` (`r--------`). The shell may prompt `rm: remove write-protected regular file?`, but typing `y` unlinks it successfully.
</details>

---

### Q7: Explain the Sticky Bit (`chmod +t` / `1777`) and why it is mandatory on `/tmp`.
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Problem It Solves:**
Because `/tmp` must be writable by every user on the system, its base permissions are `rwxrwxrwx` (777). As established in Q6, anyone with `w` on a directory can delete anyone else's files. Without special protection, User A could delete User B's active database sockets or lock files in `/tmp`.

**Mechanism:**
When the **Sticky Bit** (octal 1, displayed as `t` in `drwxrwxrwt`) is set on a directory:
- Users can still create files freely.
- A file can **only be deleted or renamed by the file's owner**, the directory's owner, or `root`.
Even if a file has `777` permissions inside `/tmp`, another unprivileged user cannot remove it.
</details>

---

### Q8: What are SUID (SetUID) and SGID (SetGID), and what are their security implications?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Mechanisms:**
- **SUID (`4000` / `-rwsr-xr-x`) on an Executable**: When executed, the process runs with the privileges of the **file owner**, not the invoking user. Example: `/usr/bin/passwd` is owned by `root`; standard users run it to update `/etc/shadow`.
- **SGID (`2000` / `-rwxr-sr-x`) on an Executable**: Runs with the privileges of the file's group.
- **SGID (`2000` / `drwxrwsr-x`) on a Directory**: Any new file or subfolder created inside this directory automatically inherits the **group ownership of the parent directory**, rather than the primary group of the creating user. Essential for shared team collaboration directories.

**Security Risks:**
SUID-root binaries are the primary vector for Linux local privilege escalation (LPE). Any buffer overflow, unquoted environment variable evaluation, or path injection in a SUID binary yields an instant root shell.
</details>

---

### Q9: What is `umask`, and how is default file and directory permission calculated?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Mechanism:**
`umask` (User Mask) is a 4-digit octal bitmask that filters out (clears) permissions when new files and directories are created.

- Default initial permissions before mask:
  - Files: `0666` (`rw-rw-rw-`) — Executable bits are never set by default.
  - Directories: `0777` (`rwxrwxrwx`).

**Calculation (Bitwise AND with bitwise NOT):**
$$\text{Created Mode} = \text{Initial Mode} \ \& \ (\sim\text{umask})$$

- Standard server `umask 022`:
  - Files: $666 - 022 = \mathbf{644}$ (`rw-r--r--`)
  - Directories: $777 - 022 = \mathbf{755}$ (`rwxr-xr-x`)
- Hardened `umask 027`:
  - Files: $666 - 027 = \mathbf{640}$ (`rw-r-----`)
  - Directories: $777 - 027 = \mathbf{750}$ (`rwxr-x---`)
</details>

---

### Q10: Why is running `chmod -R 755 /path/to/web` considered an anti-pattern, and what is the correct command?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Why It Is Bad:**
Using `chmod -R 755` recursively grants the **execute (`+x`) permission to all regular files**! Text files, `.html`, `.css`, and `.env` files become marked as executables. This is a severe security violation (violates principle of least privilege, confuses web servers, and can permit script execution).

**The Correct Approach:**
Files should be `644`, and directories should be `755`. Use capital `X` (conditional execute) or `find`:

```bash
# Method 1: Conditional execute (only applies +x to directories and already-executable files)
chmod -R u=rwX,go=rX /path/to/web

# Method 2: Standard Find pipeline
find /path/to/web -type d -exec chmod 755 {} +
find /path/to/web -type f -exec chmod 644 {} +
```
</details>

---

### Q11: What are POSIX Access Control Lists (ACLs) and how do they extend traditional permissions?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Limitation of Standard Permissions:**
Traditional Unix permissions allow only three security entities: one User (owner), one Group, and Others. If user `alice` owns a file, and group `developers` has access, you cannot grant read-only access to user `bob` or group `auditors` without making the file world-readable.

**POSIX ACLs Solution:**
ACLs allow arbitrary numbers of users and groups to be assigned explicit permissions.
- Inspect ACLs: `getfacl filename` (Files with ACLs show a `+` symbol in `ls -l`: `-rw-r--r--+`).
- Set ACL for specific user:
  ```bash
  setfacl -m u:bob:rw /var/log/app.log
  ```
- Set Default ACLs on directories (automatically inherited by newly created files):
  ```bash
  setfacl -d -m g:auditors:r /var/log/app/
  ```
</details>

---

### Q12: Compare Ext4, XFS, Btrfs, and ZFS across enterprise production criteria.
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

| Filesystem | Max Volume / File Size | Shrinking Supported? | Built-in Snapshots / CoW? | Production Domain |
| :--- | :--- | :--- | :--- | :--- |
| **Ext4** | 1 EiB / 16 TiB | **Yes** (Offline) | No | Default on Ubuntu/Debian; general purpose, robust, fast fsck. |
| **XFS** | 8 EiB / 8 EiB | **No** (Cannot shrink!)| Reflink (CoW metadata) | Default on RHEL/Rocky; superior parallel scalability on huge files. |
| **Btrfs** | 16 EiB / 16 EiB | **Yes** (Online) | **Yes** (Native CoW) | SLES, Fedora desktop; subvolumes, snapshots, bitrot scrub. |
| **ZFS** | 256 ZiB / 16 EiB | **No** | **Yes** (Native CoW) | Enterprise storage appliances, TrueNAS, heavy data integrity/RAID-Z. |
</details>

---

### Q13: Explain the role of Linux File Attributes (`chattr` / `lsattr`) and specifically `+i` and `+a`.
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Mechanism:**
Extended filesystem attributes are low-level VFS flags stored directly in the inode that override even the root user (`UID 0`).

- **Immutable (`+i`)**: The file cannot be modified, deleted, renamed, overwritten, or linked. Even `rm -f` executed by `root` will fail with `Operation not permitted`.
  ```bash
  chattr +i /etc/resolv.conf # Prevents NetworkManager from overwriting DNS
  ```
- **Append-Only (`+a`)**: The file can only be opened in append mode (`O_APPEND`). Existing bytes cannot be modified or truncated; the file cannot be deleted. Ideal for security audit logs.
  ```bash
  chattr +a /var/log/security-audit.log
  ```
</details>

---

### Q14: What is the purpose of Mount Flags (`noexec`, `nosuid`, `nodev`) in `/etc/fstab`?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Security Hardening Flags:**
- **`noexec`**: Prohibits direct execution of binaries from that filesystem. Attempting to execute `./malware` returns `Permission denied` (even if permissions are `777`). Essential on shared temporary mounts (`/tmp`, `/dev/shm`).
- **`nosuid`**: Disables SUID and SGID bits on that filesystem. Prevents attackers from copying a SUID shell to `/tmp` and executing it to escalate privileges.
- **`nodev`**: Disallows the creation or interpretation of character or block device files (e.g., `mknod /tmp/sdb1 b 8 17`). Prevents users from creating raw block device handles to read host drives.
- **`ro`**: Mounts filesystem read-only.
</details>

---

### Q15: What is a Dentry (Directory Entry) and what is the Dcache?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Mechanism:**
A **Dentry (Directory Entry)** is an in-memory kernel structure that links a string filename component (e.g., `syslog`) to its inode number (`131045`).

**The Dcache (Directory Cache):**
Resolving full pathnames like `/usr/local/bin/python3` requires walking four directories. Reading disk blocks for every path component on every system call would crush performance. The kernel maintains the **Dcache** in RAM:
- Traverses paths in nanoseconds.
- Implements "Negative Dentry Caching": remembers files that *do not exist*, so repeated failing lookups (e.g., searching `$PATH`) return `-ENOENT` instantly without hitting storage.
</details>

---

### Q16: How does the Linux VFS handle path resolution and symlink recursion limits?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Mechanism:**
When a process executes `openat("/a/b/c/d")`:
1. VFS splits the string by `/`.
2. Starts at root directory (`current->fs->root`) or working directory (`current->fs->pwd`).
3. Verifies `+x` permissions on each path component.
4. If a component is a symbolic link, VFS suspends traversal and resolves the symlink target.

**Loop Prevention (`ELOOP`):**
To prevent infinite recursion caused by circular symlinks (`ln -s a b; ln -s b a`), the kernel maintains a strict recursion counter. If symlink resolution exceeds **40 consecutive hops**, the system call aborts immediately returning `-1 ELOOP (Too many levels of symbolic links)`.
</details>

---

### Q17: What is the difference between `mtime`, `ctime`, and `atime`?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

| Timestamp | Name | Trigger Event |
| :--- | :--- | :--- |
| **`mtime`** | Modification Time | Changed when the **content/bytes** of the file are modified (`write()`). |
| **`ctime`** | Change Time (Metadata) | Changed when the file **metadata/inode** changes (`chmod`, `chown`, rename, hardlink). Cannot be spoofed via `touch`! |
| **`atime`** | Access Time | Changed when the file is **read** (`cat`, `read()`). |

**Production Impact of `relatime`:**
Updating `atime` on every single file read generates a storage write operation for every read, crippling web servers. Modern Linux defaults to `relatime` (relative atime): `atime` is only updated if the previous access time was earlier than the modification time, or once every 24 hours.
</details>

---

### Q18: What is a sparse file, and how can you determine its actual allocated size on disk?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Concept:**
A **sparse file** is a file containing large blocks of zero bytes where the filesystem allocates physical storage blocks *only for non-zero data*. When a program seeks gigabytes forward using `lseek()` and writes a single byte, the filesystem stores the byte and updates file size metadata, leaving the empty gap unallocated. Used extensively in VM disk images (`qcow2`, `raw`) and database tablespaces.

**Inspection:**
- `ls -l file`: Displays **apparent/logical** size (e.g., `100G`).
- `du -sh file` or `ls -s file`: Displays **actual physical blocks allocated** on disk (e.g., `12M`).
</details>

---

### Q19: Explain the internal mechanism of filesystem journaling (ext4/XFS) and compare `data=journal`, `data=ordered`, and `data=writeback`.
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Why Journaling Exists:**
Modifying a file requires multiple disk writes: updating the data blocks, updating the inode, and updating the free-block bitmap. If power is lost mid-write, the filesystem corrupts. A journal writes operations to a continuous circular log before applying them to permanent blocks, enabling sub-second crash recovery without full `fsck`.

**Journaling Modes in Ext4:**
1. **`data=ordered` (Default)**: Metadata is journaled. File data blocks are flushed to disk *before* metadata commits to the journal. Guarantees file integrity without logging full data payloads twice.
2. **`data=journal`**: Both metadata AND file data payloads are written to the journal before writing to disk. Maximum integrity, but cuts write throughput in half.
3. **`data=writeback`**: Only metadata is journaled; data blocks can be written before or after metadata. Fastest performance, but recently updated files may contain stale dirty data after a crash.
</details>

---

### Q20: What is OverlayFS and how do lowerdir, upperdir, merged, and workdir interact in container runtimes?
<details>
<summary>Detailed Answer & Technical Mechanics</summary>

**Core Mechanism:**
**OverlayFS** is a modern union filesystem that stacks multiple directories into a unified view. Used as the default storage driver in Docker and Podman (`overlay2`).

**Directory Roles:**
- **`lowerdir`**: Read-only base layers (e.g., standard Ubuntu image layers). Multiple lowerdirs can be stacked.
- **`upperdir`**: The read-write layer where all container modifications are written.
- **`workdir`**: Internal atomic scratchpad directory required by the kernel for Copy-on-Write preparation.
- **`merged`**: The unified mount point seen by the containerized process.

**Copy-on-Write (CoW) Mechanics:**
When a container reads a file, it reads from `lowerdir`. When it modifies or writes to a file for the first time, OverlayFS atomically copies the entire file from `lowerdir` up into `upperdir` before writing the modification.
</details>

---
*Next Note: [03-shell-and-text-processing-qa.md](file:///04-devops/linux/08-interview/03-shell-and-text-processing-qa.md)*
