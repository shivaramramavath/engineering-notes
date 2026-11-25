<!--
================================================================================
Metadata:
  Title: Links, Inodes & File Types in Linux
  Track: Linux & Systems Engineering / DevOps
  Difficulty: Intermediate
  Estimated Time: 55 minutes
  Prerequisites: 01-fundamentals/04-working-with-files.md
  Key Terminology: Inode, Inode Exhaustion, Hard Link, Symbolic Link, Dentry, FIFO, Socket, Block Device, Character Device
================================================================================
-->

# Links, Inodes & File Types in Linux

[<- 04-working-with-files.md](./04-working-with-files.md) | [Back to Module Overview](./README.md) | [06-users-and-groups.md ->](./06-users-and-groups.md)

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

In Unix architecture, the human-readable name of a file and the actual data stored on disk are completely disconnected. A file does not "have" a name in the way human beings intuitively expect. Instead, a directory is simply a special lookup table that pairs human-readable strings to an integer index: the **Inode Number** (Index Node).

Understanding inodes and link mechanics is essential for solving critical production issues:
- **Inode Exhaustion Outages**: A system can report 500 GB of free disk space on `df -h`, yet crash with `No space left on device` because millions of tiny session files have depleted all allocated filesystem inodes (`df -i`).
- **Zero-Downtime Deployments**: Software release pipelines (Capistrano, Git-based symlink swaps) rely on atomic symbolic link switching to transition web traffic instantaneously between release directories without dropping active HTTP connections.
- **The Seven Unix File Types**: Linux abstracts devices, inter-process communication channels, and network endpoints as files, each represented by a distinct file type flag in the inode.

---

## 2. Core Concepts

### What is an Inode?
An **inode** is a fixed-size data structure (typically 256 bytes in ext4) stored in dedicated tables across the filesystem. The inode records all metadata about a filesystem object, including:
- File type (regular file, directory, symlink, device, socket, FIFO).
- Permission mode bits (rwxrwxrwx, SUID, SGID, Sticky bit).
- Owner User ID (UID) and Group ID (GID).
- File size in bytes.
- Link count (`i_nlink`): the number of directory entries referencing this inode.
- Timestamps: `atime`, `mtime`, `ctime`.
- Pointers to physical storage blocks (or ext4 extent tree structures).

*Crucial Fact*: The inode **does not** contain the file name or file path. The name exists exclusively inside directory data blocks (dentries).

### Hard Links vs. Symbolic Links
1. **Hard Link**: A new directory entry that points directly to an **existing inode number**. Both entries are completely equal peers. Deleting one hard link does not destroy the data; the storage blocks are only reclaimed when the inode's link count reaches 0. Hard links cannot span across different filesystem partitions and cannot point to directories (to prevent infinite cycles).
2. **Symbolic Link (Symlink / Soft Link)**: A distinct, independent file that has its **own unique inode**. Its data block simply stores a text string: the path (relative or absolute) to the target file. If the target file is moved or deleted, the symlink remains, becoming a "dangling" or "broken" symlink. Symlinks can cross filesystem boundaries and can point to directories.

### The 7 Standard Linux File Types
When executing `ls -l`, the very first character of the permission string indicates the underlying file type:

| Symbol | File Type | Description | Common Locations |
| :---: | :--- | :--- | :--- |
| `-` | Regular File | Standard data stream: text files, binary executables, images, archives | `/etc/hosts`, `/usr/bin/bash` |
| `d` | Directory | A file containing a list of filename-to-inode mappings (dentries) | `/var/log`, `/home` |
| `l` | Symbolic Link | A pointer file containing the target path string of another file | `/bin -> usr/bin` |
| `c` | Character Device | Unbuffered, sequential byte stream hardware interface | `/dev/null`, `/dev/urandom`, `/dev/tty` |
| `b` | Block Device | Buffered, random-access hardware device communicating in fixed blocks | `/dev/sda`, `/dev/nvme0n1` |
| `s` | Local Socket | Unix domain socket used for high-speed local Inter-Process Communication | `/run/docker.sock`, `/run/systemd/notify` |
| `p` | Named Pipe (FIFO) | First-In, First-Out inter-process communication byte queue | `/run/systemd/initctl/fifo` |

---

## 3. How It Works Internally

### Inode and Link Topology Diagram
The following architectural diagram illustrates how directory entries, inodes, and physical data blocks connect for regular files, hard links, and symlinks:

```
DIRECTORY ENTRIES (Dentries)           INODE TABLE                      DATA BLOCKS ON DISK
+-------------------------------+      +-------------------------+      +-------------------------+
| Name: "app.log"               |      | Inode #104250           |      | Block #89201            |
| Inode: 104250                 |----->| - Type: Regular (-)     |----->| [ 2026-11-25 12:00:00   |
+-------------------------------+      | - Links: 2              |      |   System initialized ]  |
                                       | - Size: 4096 bytes      |      +-------------------------+
+-------------------------------+      | - UID: 1000, GID: 1000  |
| Name: "app_link.log"          |      +-------------------------+
| Inode: 104250 (HARD LINK)     |-------------^
+-------------------------------+

+-------------------------------+      +-------------------------+      +-------------------------+
| Name: "app_symlink.log"       |      | Inode #104299           |      | Block #89344            |
| Inode: 104299 (SOFT LINK)     |----->| - Type: Symlink (l)     |----->| String: "app.log"       |
+-------------------------------+      | - Links: 1              |      +-------------------------+
                                       | - Size: 7 bytes         |
                                       +-------------------------+
```

When an application reads `app_link.log`, the kernel directly reads Inode `#104250` and accesses Block `#89201`.
When an application reads `app_symlink.log`, the kernel reads Inode `#104299`, sees type `l`, fetches the target path `"app.log"`, traverses the directory table to find Inode `#104250`, and finally accesses Block `#89201`.

---

## 4. Syntax & Usage

### Creating and Managing Links
```bash
# Create a Hard Link: ln <target> <link_name>
ln /var/log/app.log /var/log/app_backup.log

# Create a Symbolic (Soft) Link: ln -s <target> <link_name>
ln -s /etc/nginx/sites-available/api.conf /etc/nginx/sites-enabled/api.conf

# Force create / overwrite existing destination symlink atomically
ln -sfn /opt/releases/v2.4.0 /var/www/current
# (-s: symbolic, -f: force, -n: treat destination symlink to directory as normal file)

# Inspect inode numbers and link counts
ls -li /var/log/app*
# Example output:
# 104250 -rw-r--r-- 2 ubuntu ubuntu 4096 Nov 25 12:00 app.log
# 104250 -rw-r--r-- 2 ubuntu ubuntu 4096 Nov 25 12:00 app_backup.log
# 104299 lrwxrwxrwx 1 ubuntu ubuntu    7 Nov 25 12:00 app_symlink.log -> app.log
```

### Inspecting Inodes and Filesystem Inode Limits
```bash
# Inspect comprehensive inode metadata for a single file
stat /etc/hosts

# Check filesystem inode consumption (total, used, free, percentage)
df -i

# Find all files pointing to a specific inode number
find / -inum 104250 2>/dev/null
```

---

## 5. Practical Examples

### Example 1: Creating and Verifying Hard vs. Soft Links
```bash
# 1. Create a workspace
mkdir -p /tmp/link_lab && cd /tmp/link_lab

# 2. Create original file
echo "Production Database Payload" > original.txt

# 3. Create hard link and soft link
ln original.txt hardlink.txt
ln -s original.txt softlink.txt

# 4. Inspect inodes (First column is Inode number; third column is link count)
ls -li
# Notice original.txt and hardlink.txt share the exact same Inode number!
# Notice the link count for that inode increased to 2.
# Notice softlink.txt has a completely separate Inode number and link count of 1.

# 5. Delete original file
rm original.txt

# 6. Test reads
cat hardlink.txt
# Output: "Production Database Payload" (Data remains fully intact!)

cat softlink.txt
# Output: cat: softlink.txt: No such file or directory (Broken/Dangling symlink!)

# Cleanup
rm -f hardlink.txt softlink.txt
```

### Example 2: Atomic Zero-Downtime Deployment Symlink Switch
```bash
# Production pattern: Atomic cutover between software release versions
mkdir -p /opt/app/releases/v1 /opt/app/releases/v2

# Populate versions
echo '{"version": "1.0.0"}' > /opt/app/releases/v1/meta.json
echo '{"version": "2.0.0"}' > /opt/app/releases/v2/meta.json

# Point active symlink to v1
ln -sfn /opt/app/releases/v1 /opt/app/current
cat /opt/app/current/meta.json  # Output: {"version": "1.0.0"}

# Atomic Cutover to v2: Create temporary symlink and rename atomically
ln -sfn /opt/app/releases/v2 /opt/app/current_new
mv -Tf /opt/app/current_new /opt/app/current

# The running web server experiences zero dropped packets during the switch
cat /opt/app/current/meta.json  # Output: {"version": "2.0.0"}
```

### Example 3: Diagnosing and Remediating 100% Inode Exhaustion
```bash
# Scenario: Developer reports "No space left on device" when creating a file,
# but df -h shows 200 GB available!

# Step 1: Check inode usage
df -i /
# Filesystem      Inodes   IUsed   IFree IUse% Mounted on
# /dev/nvme0n1p1 6553600 6553600       0  100% /   <-- INODE EXHAUSTION!

# Step 2: Identify directory with millions of tiny files
sudo find /var/spool /tmp /var/cache -xdev -printf '%h\n' | sort | uniq -c | sort -k1 -n | tail -n 5
# Sample output:
#   12500 /var/spool/postfix/maildrop
# 6245000 /var/spool/clientmqueue   <-- Root cause: Unhandled mail queue backlog!

# Step 3: Rapid cleanup using find -delete (avoids shell arg limit errors)
sudo find /var/spool/clientmqueue -type f -delete

# Step 4: Verify recovery
df -i /
```

### Example 4: Inter-Process Communication with a Named Pipe (FIFO)
```bash
# Create a Named Pipe (FIFO)
mkfifo /tmp/log_stream.pipe

# Inspect file type (Notice 'p' in permissions)
ls -l /tmp/log_stream.pipe
# prw-rw-r-- 1 dev dev 0 Nov 25 12:00 /tmp/log_stream.pipe

# Background reader process waiting on the pipe
(cat < /tmp/log_stream.pipe | grep --line-buffered "ERROR") &
READER_PID=$!

# Writer sends data through the pipe
echo "INFO: Normal operation" > /tmp/log_stream.pipe
echo "ERROR: Database connection timeout" > /tmp/log_stream.pipe
# Output: ERROR: Database connection timeout

# Cleanup
kill $READER_PID 2>/dev/null || true
rm -f /tmp/log_stream.pipe
```

### Example 5: Finding and Cleaning Broken / Dangling Symbolic Links
```bash
# Create a test dangling symlink
ln -s /path/that/does/not/exist /tmp/broken_link.txt

# Audit and list all broken symlinks across a directory tree
find /tmp -xtype l -ls

# Automatically purge all broken symlinks in a deployment folder
find /tmp -xtype l -delete

# Verify cleanup
ls -l /tmp/broken_link.txt 2>&1
# ls: cannot access '/tmp/broken_link.txt': No such file or directory
```

---

## 6. Best Practices

- **Always Use Relative Targets for Symlinks Inside Application Trees**: When linking files within a repository or container (e.g., `ln -s ../config/app.json active.json`), use relative paths. Absolute symlinks break when mounted into Docker containers or alternate staging directories.
- **Always Use `ln -sfn` + `mv -Tf` for Atomic Symlink Switches**: Never use `rm symlink && ln -s target symlink`. That introduces a millisecond window where the symlink does not exist, causing incoming HTTP requests to fail with `404 Not Found`.
- **Monitor Both `df -h` and `df -i` in Monitoring Dashboards**: Alert on inode consumption thresholds (`>= 85%`) in Prometheus / Datadog. Inode exhaustion crashes systems identically to raw disk exhaustion.
- **Audit Directory Tree Inode Usage**: High inode consumption is almost always caused by:
  - PHP/Node uncleaned session files in `/tmp` or `/var/lib/php/sessions`.
  - Cron daemon unrouted failure mail backlogs in `/var/spool/postfix` or `/var/spool/clientmqueue`.
  - Excessive Git cache or npm cache directories.
- **Never Try to Hard Link Across Filesystems**: Hard links rely on the target inode residing within the *same* physical partition's inode table. Cross-partition links must always be symbolic links.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| `ln: failed to create hard link: Invalid cross-device link` | Hard links point to an inode number; inode numbers are only unique within a single filesystem. | Use a symbolic link (`ln -s`) instead. |
| `ln: hard link not allowed for directory` | Hard linking directories could introduce circular graphs into the filesystem tree, causing filesystem corruption and infinite kernel loops. | Use directory symlinks (`ln -s`) or bind mounts (`mount --bind`). |
| `Too many levels of symbolic links (ELOOP)` | Symlink points to itself directly or through a circular chain; kernel halts traversal after 40 iterations. | Inspect target path with `readlink` and recreate the symlink with correct non-recursive path. |
| `No space left on device` despite `df -h` showing plenty of free gigabytes | Inode table is 100% full (`df -i`); no new files or directories can be created regardless of storage capacity. | Run `find` to identify directories with millions of small files and purge them. |
| `rm symlink/` (with trailing slash) | Deletes content *inside* the target directory rather than removing the symlink itself, or fails with `Not a directory`. | Always omit the trailing slash when removing a symlink: `rm symlink`. |
| Assuming `chmod 777 symlink` makes the target accessible | In Linux, symbolic link permissions are completely ignored by the kernel; permissions are governed solely by the target file. | Modify permissions directly on the target file, not the symlink. |

---

## 8. Performance, Security & Cost Implications

- **Performance**: Hard links incur zero resolution penalty; accessing a hard link executes at identical speed to the original file. Soft links require the kernel's VFS layer to resolve the path string, consuming additional dcache lookups and potential disk I/O. The kernel bounds symlink resolution to 40 traversals to prevent CPU spin exhaustion.
- **Security**: Symlinks in world-writable directories (`/tmp`) historically enabled privilege escalation attacks. Modern Linux kernels enforce `fs.protected_symlinks = 1` and `fs.protected_hardlinks = 1` via `sysctl`:
  - Users cannot follow symlinks in world-writable sticky directories unless they own either the symlink or the target file.
  - Users cannot create hard links to files they do not own unless they already have read/write access.
- **Cost**: Inode count is fixed at format time for ext4 filesystems. If an application requires billions of tiny files (e.g., machine learning image snippets or cache blocks), choosing XFS or Btrfs (which allocate inodes dynamically) avoids the necessity of re-partitioning disks.

---

## 9. Comparison Table

| Feature / Trait | Hard Link | Symbolic (Soft) Link |
| :--- | :--- | :--- |
| **Inode Number** | Shares the original file's inode | Allocates its own independent inode |
| **Cross-Filesystem Support** | No (strictly confined to same filesystem) | Yes (can span disks, network mounts) |
| **Directory Support** | Prohibited (prevents directory loops) | Allowed |
| **Original File Deleted** | Content remains readable; link count drops | Link becomes broken / dangling |
| **Storage Consumption** | Consumes 0 additional data blocks | Consumes minimal data block for path string |
| **Creation Command** | `ln target link_name` | `ln -s target link_name` |
| **Identification in `ls -l`**| Regular file `-` with link count $\ge 2$ | Prefix `l` with `-> target_path` |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What is an inode, and why does an inode not store the file's name?</strong></summary>

**Answer**:
An inode (index node) is a filesystem metadata structure that describes a file or directory's technical attributes: file type, permissions, owner UID, group GID, byte size, link count, timestamps (`atime`, `mtime`, `ctime`), and pointers to data blocks.

The inode does not store the file name to enforce clean separation of concerns and allow multiple directory paths (hard links) to reference the exact same data payload. In Linux, a directory is simply a special file whose data blocks consist of an array of directory entries (`struct dirent`), each containing a filename string and an associated inode number. This design allows files to be renamed atomically without touching the inode or data blocks.
</details>

<details>
<summary><strong>2. Can a hard link point to a file on a different filesystem or disk partition? Why or why not?</strong></summary>

**Answer**:
No. A hard link is simply an entry in a directory table pointing to an integer inode number. Inode numbers are only unique and meaningful within the scope of a single filesystem partition.

If hard links could cross partitions, Inode `#104250` on `/dev/sda1` would collide with Inode `#104250` on `/dev/sdb1`, which represents a completely different file. Therefore, the Linux VFS enforces the rule that hard links cannot cross filesystem boundaries; invoking `ln` across mounts returns the `EXDEV` error (`Invalid cross-device link`).
</details>

<details>
<summary><strong>3. How does the kernel know when to physically delete a file from disk blocks?</strong></summary>

**Answer**:
The kernel tracks two reference counters for every file:
1. **`i_nlink` (Hard Link Count)**: Stored inside the inode; tracks how many directory entries on disk point to this inode. Decremented when `unlink()` or `rm` is invoked.
2. **`i_count` (Open Descriptor Count)**: Tracked in kernel memory; tracks how many active processes hold an open file descriptor referencing this inode.

The kernel only frees the physical storage blocks and returns the inode to the free pool when **both counters reach zero** (`i_nlink == 0` AND `i_count == 0`). If a file is unlinked while a daemon still has it open, the blocks remain allocated on disk until the process closes the descriptor or terminates.
</details>

<details>
<summary><strong>4. What is Inode Exhaustion, and how can it occur when a disk has hundreds of gigabytes of free space?</strong></summary>

**Answer**:
In filesystems like ext4, the total number of inodes is determined statically at format time (based on partition size and inode density ratio, typically one inode per 16 KB of disk space). Once formatted, the number of inodes is fixed.

If an application generates millions of tiny files (such as 10-byte cache keys, uncollected session files, or email queue fragments), each file consumes one full inode. The system will deplete 100% of available inodes (`df -i` shows 100% usage) while `df -h` may show that only a small fraction of physical disk storage blocks are occupied. At that point, any system call creating a new file (`open(O_CREAT)`, `mkdir`) fails with `ENOSPC (No space left on device)`.
</details>

<details>
<summary><strong>5. Why does Linux forbid regular users from creating hard links to directories?</strong></summary>

**Answer**:
Allowing hard links to directories would break the Directed Acyclic Graph (DAG) hierarchy of the filesystem. If a directory could be hard-linked:
1. Cyclic references could easily be created (e.g., `/a/b` hard-linked to `/a`), causing infinite recursion loops in kernel path resolution, backup utilities, and recursive commands (`rm -rf`, `find`).
2. Calculating parent directory references (`..`) would become mathematically ambiguous: a directory could have multiple different parents, breaking POSIX path resolution.

To prevent filesystem tree corruption, the kernel strictly disallows hard links to directories.
</details>

<details>
<summary><strong>6. Explain the seven standard Unix file types and how to recognize them.</strong></summary>

**Answer**:
1. **Regular file (`-`)**: Standard user data or binary code.
2. **Directory (`d`)**: Contains filename-to-inode mappings.
3. **Symbolic link (`l`)**: Pointer file storing target path string.
4. **Character device (`c`)**: Hardware device streaming unbuffered individual bytes (e.g., `/dev/urandom`, `/dev/tty`).
5. **Block device (`b`)**: Hardware device transferring data in fixed-size buffered blocks (e.g., `/dev/nvme0n1`, `/dev/sda`).
6. **Unix domain socket (`s`)**: High-performance local IPC channel between processes (e.g., `/run/docker.sock`).
7. **Named pipe / FIFO (`p`)**: One-way inter-process byte queue (created with `mkfifo`).

They are identified by the first character in `ls -l` output or queried using `find -type <-,d,l,c,b,s,p>`.
</details>

<details>
<summary><strong>7. How does an atomic symlink switch work during production zero-downtime releases?</strong></summary>

**Answer**:
In deployment workflows, applications are placed in versioned directories (e.g., `/opt/app/releases/v1`, `/opt/app/releases/v2`), while the web server or systemd service points to `/opt/app/current`.

To switch versions atomically without downtime:
1. Create a new temporary symlink pointing to the target release:
   `ln -sfn /opt/app/releases/v2 /opt/app/current_new`
2. Atomically rename the temporary symlink over the active symlink:
   `mv -Tf /opt/app/current_new /opt/app/current`

Under the hood, `mv -Tf` invokes the atomic `rename()` system call. At the kernel VFS level, directory entry substitution happens instantaneously: incoming I/O requests read either the old version or the new version, eliminating any intermediate window where the file does not exist.
</details>

<details>
<summary><strong>8. What is the kernel mechanism `fs.protected_symlinks` and why is it enabled by default?</strong></summary>

**Answer**:
`fs.protected_symlinks` is a Linux kernel security setting designed to stop "symlink race condition" vulnerabilities. In world-writable directories with the sticky bit (like `/tmp`), an attacker could create a symlink named `app.log` pointing to `/etc/shadow`. If a privileged daemon or root script wrote to `/tmp/app.log`, it would overwrite the shadow password database.

When `fs.protected_symlinks = 1`, the kernel only follows symlinks in sticky world-writable directories if:
1. The user following the link owns the symlink.
2. Or the owner of the symlink matches the owner of the directory.
Otherwise, the kernel refuses to traverse the symlink, returning `EACCES (Permission denied)`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Identify File Types Across the System</strong></summary>

**Task**: Write a shell command using `find` that locates at least one example of each of the 7 standard Linux file types (`-`, `d`, `l`, `c`, `b`, `s`, `p`) on your machine.

**Solution**:
```bash
echo "Regular file: $(find /etc -maxdepth 1 -type f | head -n 1)"
echo "Directory:    $(find /var -maxdepth 1 -type d | head -n 1)"
echo "Symlink:      $(find /bin -maxdepth 1 -type l | head -n 1)"
echo "Char device:  $(find /dev -maxdepth 1 -type c | head -n 1)"
echo "Block device: $(find /dev -maxdepth 1 -type b | head -n 1)"
echo "Socket:       $(find /run -maxdepth 2 -type s 2>/dev/null | head -n 1)"
echo "Named pipe:   $(find /run -maxdepth 2 -type p 2>/dev/null | head -n 1)"
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Track Hard Link Counts</strong></summary>

**Task**: Create a test file, track its link count using `stat` as you add two hard links, and verify what happens to the link count when you delete one link.

**Solution**:
```bash
touch /tmp/counter.txt
stat -c "Links: %h | Inode: %i" /tmp/counter.txt
# Output: Links: 1

ln /tmp/counter.txt /tmp/counter_h1.txt
ln /tmp/counter.txt /tmp/counter_h2.txt
stat -c "Links: %h | Inode: %i" /tmp/counter.txt
# Output: Links: 3

rm /tmp/counter_h1.txt
stat -c "Links: %h | Inode: %i" /tmp/counter.txt
# Output: Links: 2

# Cleanup
rm -f /tmp/counter.txt /tmp/counter_h2.txt
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Detect and List All Inode Peers of a File</strong></summary>

**Task**: Given a file path, write a bash script that locates every other filename on the same filesystem that shares that file's inode (its hard link siblings).

**Solution**:
```bash
#!/usr/bin/env bash
TARGET="${1:-/etc/passwd}"
INODE=$(stat -c %i "$TARGET")
MOUNT_POINT=$(df --output=target "$TARGET" | tail -n 1)

echo "Finding hard link siblings for $TARGET (Inode: $INODE) on mount $MOUNT_POINT:"
find "$MOUNT_POINT" -xdev -inum "$INODE" 2>/dev/null
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Build an Inode Consumption Audit Script</strong></summary>

**Task**: Write a bash script that inspects a specified directory tree and prints the top 5 subdirectories consuming the greatest number of inodes.

**Solution**:
```bash
#!/usr/bin/env bash
DIR="${1:-/var}"
echo "Auditing top inode consumers in $DIR..."
find "$DIR" -xdev -type f 2>/dev/null | awk -F/ '{
    NF--
    path=""
    for(i=1; i<=NF; i++) {
        if(i>1) path=path "/" $i
        else path=$i
    }
    count[path]++
}
END {
    for (p in count) print count[p], p
}' | sort -nr | head -n 5
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Reproduce and Fix a Dangling Symlink Chain</strong></summary>

**Task**: Create a chain of three symlinks (`a -> b -> c -> target.txt`). Delete the target file, verify the broken chain with `find -xtype l`, and restore it.

**Solution**:
```bash
# Setup chain
mkdir -p /tmp/chain_lab && cd /tmp/chain_lab
echo "Hello Chain" > target.txt
ln -s target.txt c
ln -s c b
ln -s b a

# Verify chain resolution
cat a
# Output: Hello Chain

# Break chain
rm target.txt

# Audit broken links
find . -xtype l -ls
# Shows all 3 symlinks marked as broken!

# Repair chain
echo "Restored Content" > target.txt
cat a
# Output: Restored Content

# Cleanup
cd /tmp && rm -rf /tmp/chain_lab
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): High-Throughput Named Pipe Worker Pool</strong></summary>

**Task**: Implement a producer-consumer processing loop using a named pipe (`mkfifo`) where a background consumer reads lines and timestamps them into an output file.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

PIPE="/tmp/work_queue.fifo"
OUT="/tmp/processed.log"
rm -f "$PIPE" "$OUT"
mkfifo "$PIPE"

# Background worker consumer
(
    while IFS= read -r line; do
        echo "[$(date +'%Y-%m-%d %H:%M:%S')] PROCESSED: $line" >> "$OUT"
    done < "$PIPE"
) &
WORKER_PID=$!

# Producer sends tasks
echo "TASK-1: Sync database" > "$PIPE"
echo "TASK-2: Invalidate CDN cache" > "$PIPE"
sleep 1

# Inspect output
cat "$OUT"

# Cleanup
kill $WORKER_PID 2>/dev/null || true
rm -f "$PIPE" "$OUT"
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Command / Flag | Action | Example |
| :--- | :--- | :--- |
| `ln <target> <link>` | Create Hard Link | `ln file.txt hard.txt` |
| `ln -s <target> <link>` | Create Symbolic Link | `ln -s file.txt soft.txt` |
| `ln -sfn <target> <link>` | Overwrite directory symlink cleanly | `ln -sfn /opt/v2 /opt/active` |
| `mv -Tf <temp> <active>` | Atomic symlink replacement | `mv -Tf /tmp/new_sym /var/active` |
| `stat -c %i <file>` | Print inode number of file | `stat -c %i /etc/hosts` |
| `stat -c %h <file>` | Print hard link count | `stat -c %h /etc/hosts` |
| `df -i` | Display filesystem inode utilization | `df -i /` |
| `find -type l` | Find all symbolic links | `find /etc -type l` |
| `find -xtype l` | Find all broken/dangling symlinks | `find /tmp -xtype l` |
| `mkfifo <name>` | Create a Named Pipe (FIFO) | `mkfifo /tmp/event.pipe` |

---

## 13. Further Reading & Official Documentation

- [man 2 stat: Inode Metadata System Call](https://man7.org/linux/man-pages/man2/stat.2.html)
- [man 7 inode: Linux Inode Structure and Flags](https://man7.org/linux/man-pages/man7/inode.7.html)
- [man 1 ln: Make Links Between Files](https://man7.org/linux/man-pages/man1/ln.1.html)
- [Linux Kernel Documentation: VFS Inodes and Dentries](https://www.kernel.org/doc/html/latest/filesystems/vfs.html)
- [LWN.net: Protecting Symlinks and Hardlinks in Linux](https://lwn.net/Articles/390048/)

---

[<- 04-working-with-files.md](./04-working-with-files.md) | [Back to Module Overview](./README.md) | [06-users-and-groups.md ->](./06-users-and-groups.md)
