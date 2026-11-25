<!--
================================================================================
Metadata:
  Title: Working with Files: Operations, Descriptors & Inspection
  Track: Linux & Systems Engineering / DevOps
  Difficulty: Beginner to Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 01-fundamentals/03-filesystem-hierarchy-and-navigation.md
  Key Terminology: File Descriptors, touch, cp, mv, rm, mktemp, shred, tail, less, Sparse Files
================================================================================
-->

# Working with Files: Operations, Descriptors & Inspection

[<- 03-filesystem-hierarchy-and-navigation.md](./03-filesystem-hierarchy-and-navigation.md) | [Back to Module Overview](./README.md) | [05-links-inodes-and-file-types.md ->](./05-links-inodes-and-file-types.md)

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

In Linux systems engineering, file operations extend far beyond casual desktop file manipulation. Configuration files, system logs, TLS certificates, software binaries, database tablespaces, Unix domain sockets, and hardware devices are all accessed as byte streams across the Virtual File System (VFS).

Engineering production systems requires precise mastery of file lifecycles:
- Creating files securely without race conditions (`mktemp`).
- Preserving file metadata, timestamps, and extended attributes during deployments (`cp -a`).
- Understanding when a file rename (`mv`) is an instantaneous, atomic pointer update versus a slow cross-device block copy.
- Streaming and diagnosing live logs across log rotations (`tail -F` vs. `tail -f`).
- Identifying sparse files where apparent size differs by gigabytes from actual disk block allocation.

---

## 2. Core Concepts

### File Metadata vs. Content
In Unix architecture, a file consists of two decoupled components:
1. **Metadata (The Inode)**: Stores ownership (UID/GID), permission mode bits, size in bytes, hardware link counts, and timestamps (`atime`, `mtime`, `ctime`). It does not store the human-readable filename.
2. **Data Blocks**: The raw payload bytes allocated on the physical storage device or generated dynamically by the kernel.

### The Three Timestamps
Every Linux file tracks three distinct timestamps:
- **`mtime` (Modification Time)**: Updated when the file's *content* is modified (e.g., appending data to a log).
- **`ctime` (Change Time)**: Updated when the file's *metadata* or inode properties change (e.g., changing permissions with `chmod`, altering owner with `chown`, or renaming).
- **`atime` (Access Time)**: Updated when the file is *read* by an application. (Modern filesystems use the `relatime` mount option to minimize disk write amplification).

```
+--------------------------------------------------------------------------+
| FILE OPERATION METADATA IMPACT                                           |
|                                                                          |
| Action                      | Content Changes? | mtime | ctime | atime   |
| --------------------------- | ---------------- | ----- | ----- | -----   |
| Read file (cat / less)      | No               | -     | -     | Updated |
| Edit content (echo >> file) | Yes              | Updated| Updated| -     |
| Change permissions (chmod)  | No               | -     | Updated| -     |
| Change owner (chown)        | No               | -     | Updated| -     |
| Rename file (same mount)    | No (dir entry)   | -     | Updated| -     |
+--------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### Atomic `mv` vs. Cross-Device `mv`
The behavior of the `mv` command changes fundamentally depending on whether the source and destination paths reside on the same filesystem mount or cross different mount points:

```
SCENARIO A: Same Filesystem (e.g., /var/app/temp -> /var/app/prod)
  Operation: sys_renameat2()
  Action:
    1. Creates new directory entry (dentry) pointing to existing inode.
    2. Unlinks old directory entry.
  Result: ATOMIC operation. Zero data blocks copied. Takes 1 millisecond
          regardless of whether the file is 10 KB or 500 GB.

SCENARIO B: Cross-Filesystem (e.g., /tmp/build.tar -> /var/releases/build.tar)
  Operation: Copy + Unlink
  Action:
    1. Opens source file and creates new destination file on second disk.
    2. Copies every block across the VFS layer.
    3. Verifies copy and deletes source file via sys_unlink().
  Result: NON-ATOMIC operation. High I/O overhead.
          If interrupted mid-flight, leaves partial file at destination!
```

---

## 4. Syntax & Usage

### File Creation & Allocation
```bash
# Update timestamps or create an empty file if non-existent
touch config.yaml

# Set specific timestamp: YYYYMMDDhhmm.ss
touch -t 202611251200.00 deployment.marker

# Preallocate an empty file of exact size instantly (metadata only, sparse)
truncate -s 10G sparse_test.img

# Securely generate unique temporary files and directories in scripts
TEMP_FILE=$(mktemp /tmp/deploy.XXXXXX.json)
TEMP_DIR=$(mktemp -d /tmp/workspace.XXXXXX)
```

### High-Fidelity Copying (`cp`)
```bash
# Archive mode (-a): Preserves permissions, ownership, timestamps, symlinks, and recurses
cp -a /etc/nginx/ /backup/nginx-backup/

# Update mode (-u): Copy only when SOURCE is newer than DESTINATION or missing
cp -au /src/static/ /var/www/html/static/

# Force sparse file preservation during copy
cp --sparse=always disk.raw /var/lib/libvirt/images/
```

### Viewing and Streaming File Content
```bash
# Concatenate and display content with line numbers
cat -n /etc/hosts

# Stream live log updates (stops if file is rotated or replaced)
tail -f /var/log/syslog

# Follow log by NAME (robust against logrotate recreating the file)
tail -F /var/log/nginx/access.log

# Paginated viewer without loading the entire multi-gigabyte file into memory
less -N +F /var/log/application.log
# (Inside less: press Ctrl+C to stop streaming, Shift+F to resume streaming)
```

### File Identification & Secure Erasing
```bash
# Inspect true MIME type regardless of file extension
file -b --mime-type archive.dat
# Output: application/gzip (even if named archive.txt)

# Securely overwrite file data blocks before unlinking to prevent recovery
shred -u -z -n 3 sensitive_private_key.pem
# (-u: remove file after overwrite; -z: final overwrite with zeros; -n 3: 3 passes)
```

---

## 5. Practical Examples

### Example 1: Creating Robust Ephemeral Workspaces with `mktemp` and Traps
```bash
#!/usr/bin/env bash
set -euo pipefail

# Create isolated temporary directory
WORK_DIR=$(mktemp -d -t app_build.XXXXXX)

# Guarantee cleanup on exit, error, or termination signal
cleanup() {
    echo "Purging temporary build workspace: $WORK_DIR"
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT INT TERM

# Perform operations inside the workspace
echo '{"status": "compiled"}' > "$WORK_DIR/manifest.json"
tar -czf /tmp/release.tar.gz -C "$WORK_DIR" .
echo "Build complete. Artifact created at /tmp/release.tar.gz"
```

### Example 2: Atomic Deployment Configuration Swap
```bash
# Production pattern: Never edit live configs directly in-place
CONFIG_DIR="/etc/my-service"
TEMP_CONF=$(mktemp "$CONFIG_DIR/config.XXXXXX.yaml")

# 1. Render new configuration to temporary file on the SAME filesystem
cat << 'EOF' > "$TEMP_CONF"
database:
  host: "db-primary.internal"
  pool_size: 50
EOF

# 2. Set strict security permissions before exposing to service
chmod 0640 "$TEMP_CONF"
chown root:service-app "$TEMP_CONF"

# 3. Atomically overwrite live config via rename
mv -f "$TEMP_CONF" "$CONFIG_DIR/config.yaml"

# The running daemon never sees a half-written or partially flushed file!
```

### Example 3: Handling Log Rotation Gracefully with `tail -F`
```bash
# Simulate a rotating application log
LOG_FILE="/tmp/streaming_app.log"
touch "$LOG_FILE"

# Start log follower in background using -F (follow by descriptor name)
tail -F "$LOG_FILE" &
TAIL_PID=$!

# Append logs
echo "Log message 1" >> "$LOG_FILE"
sleep 1

# Simulate logrotate: Move existing file and touch a new empty file
mv "$LOG_FILE" "$LOG_FILE.1"
touch "$LOG_FILE"
# tail -F automatically detects: 'tail: streaming_app.log has become inaccessible: No such file'
# followed immediately by: 'tail: streaming_app.log has appeared; following new file'

echo "Log message 2 after rotation" >> "$LOG_FILE"

# Cleanup
kill $TAIL_PID
rm -f "$LOG_FILE" "$LOG_FILE.1"
```

### Example 4: Diagnosing Sparse Files (Apparent vs. Actual Disk Usage)
```bash
# Create a 5 GB sparse disk image file
truncate -s 5G /tmp/sparse_disk.img

# Compare standard size listing vs actual disk block allocation
ls -lh /tmp/sparse_disk.img
# Output: -rw-rw-r-- 1 dev dev 5.0G Nov 25 12:00 /tmp/sparse_disk.img

# Check actual storage consumed on physical filesystem
du -h /tmp/sparse_disk.img
# Output: 0    /tmp/sparse_disk.img (Zero physical blocks allocated!)

# Copy without sparse awareness (creates real 5 GB physical allocation)
cp --sparse=never /tmp/sparse_disk.img /tmp/dense_disk.img
du -h /tmp/dense_disk.img
# Output: 5.0G /tmp/dense_disk.img (Consumes real disk capacity)

# Cleanup
rm -f /tmp/sparse_disk.img /tmp/dense_disk.img
```

### Example 5: High-Performance Mass File Cleanup via `find` and `delete`
```bash
# Anti-pattern: rm -rf /var/cache/app/* (fails with 'Argument list too long' if 100k+ files)
# Remediation: Use find with native kernel unlink
find /tmp/app_cache -type f -name "*.tmp" -mtime +7 -delete

# For extreme directory sizes, pass through xargs in parallel chunks
find /tmp/app_cache -type f -print0 | xargs -0 -P 4 rm -f
```

---

## 6. Best Practices

- **Always Use `cp -a` When Backing Up**: Plain `cp -r` alters file ownership to the executing user, changes timestamps to `now`, and dereferences symlinks into full file copies. `cp -a` (archive) preserves UIDs, GIDs, mode bits, timestamps, and symlink references.
- **Never Use Insecure `/tmp` Paths in Scripts**: Never write to static paths like `/tmp/app.log` or `/tmp/payload.json`. Attackers can pre-create a malicious symlink pointing to `/etc/shadow`, causing your script to overwrite critical system files (symlink race condition). Always use `mktemp`.
- **Use `tail -F` (Capital F) for Production Log Streaming**: `tail -f` (lowercase) follows the open file descriptor. When `logrotate` moves the file and creates a new one, lowercase `-f` continues following the old rotated file until it is deleted. Capital `-F` tracks the file by name, reconnecting automatically when a new file appears.
- **Inspect Large Files with `less`, Never `cat` or `vim`**: Opening a 20 GB database dump with `vim` attempts to load the entire document into virtual memory, triggering system-wide paging and OOM crashes. `less` reads chunks on-demand via streaming VFS system calls.
- **Use `rsync -avP` for Large Transfers**: For large files or directory synchronization, `rsync` provides resume capability (`--partial`), progress tracking, and checksum verification that standard `cp` lacks.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| `rm: argument list too long` | The shell expands `*` into CPU memory; the combined command line exceeds `ARG_MAX` (typically 2 MB). | Use `find . -type f -delete` or `find . -type f -print0 \| xargs -0 rm -f`. |
| Editing live configuration directly on disk | If the text editor or script crashes mid-write, the config is truncated or half-written, crashing daemon reloads. | Write to a temporary file on the same filesystem, then execute an atomic `mv`. |
| `tail -f` stops receiving updates after midnight | `logrotate` renamed the original log file; `tail -f` remains bound to the stale rotated inode. | Always use `tail -F` (capital F) to follow files across inode rotations. |
| Deleting a file with `rm` does not free disk space | A running process (e.g., Nginx or Java) still holds the file descriptor open; blocks are not marked free until closed. | Identify the process holding the file via `lsof +L1` and restart/reload the daemon. |
| Using `rm -rf *` inside a directory variable that evaluates to empty (`rm -rf $DIR/*`) | If `$DIR` is unset, the shell executes `rm -rf /*`, destroying the entire operating system root. | Use bash parameter expansion guard: `rm -rf "${DIR:?Variable unset}/*"`. |
| Preserving sparse files over network copies | Traditional tar or scp reads null blocks as real zeroes, inflating a 10 GB sparse image into 10 GB of network traffic and disk writes. | Use `tar --sparse` or `rsync -S`. |

---

## 8. Performance, Security & Cost Implications

- **Performance**: Disk I/O speed is bounded by filesystem block size (typically 4096 bytes). Writing small files in tight loops triggers thousands of individual inode updates. Batching writes or using in-memory tmpfs (`/dev/shm`) reduces physical storage wear and I/O wait (`%wa`).
- **Security**: The `shred` utility prevents forensic data recovery on magnetic hard drives. However, on modern Solid State Drives (SSDs) and cloud virtual disks (AWS EBS), copy-on-write wear leveling and flash translation layers (FTL) mean `shred` cannot guarantee physical erasure. Encrypt block volumes with LUKS or KMS at rest.
- **Cost**: Accidental expansion of sparse files during image backups can rapidly blow past cloud backup storage budgets and saturate network egress bandwidth.

---

## 9. Comparison Table

| Tool / Command | Primary Use Case | Memory Footprint | Streaming Support | Metadata Preservation |
| :--- | :--- | :--- | :--- | :--- |
| `cat` | Quick dump of small files | Low | No | N/A |
| `less` | Interactive inspection of huge files | Extremely Low (paged) | Yes (`+F`) | N/A |
| `head` / `tail` | Header or trailer inspection | Extremely Low | Yes (`-f` / `-F`) | N/A |
| `cp -r` | Recursive file copy | Low | No | No (resets ownership/time) |
| `cp -a` | Production archive copy | Low | No | Complete (all attributes) |
| `rsync -a` | Incremental & remote synchronization | Low to Medium | No | Complete + Checksums |
| `truncate` | Instantaneous sparse allocation | Zero (metadata only) | No | Preserves existing data |
| `shred` | Overwrite data before delete | Low | No | Unlinks upon completion |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What happens at the kernel and filesystem level when you delete a file using `rm`?</strong></summary>

**Answer**:
Executing `rm <file>` invokes the `unlink()` or `unlinkat()` system call.
1. The kernel navigates to the directory containing the file and removes the directory entry (dentry) that maps the filename to its underlying inode number.
2. The kernel decrements the hard link count (`i_nlink`) stored in the file's inode.
3. The kernel checks two conditions before deallocating disk blocks:
   - Is `i_nlink == 0`?
   - Is the open file descriptor reference count (`i_count`) equal to 0 across all processes in the system?
4. If a running process still has the file open, the kernel preserves the inode and data blocks on disk. Only when the last process terminates or executes `close()` does the kernel mark the filesystem blocks and inode as free in the allocation bitmap.
</details>

<details>
<summary><strong>2. Why is `mv` within the same filesystem considered atomic, whereas `cp` followed by `rm` is not?</strong></summary>

**Answer**:
Within the same filesystem, `mv` translates to the `rename()` or `renameat2()` system call. The kernel updates the directory entries: it inserts the target name in the destination directory and unlinks the source name in the source directory, pointing both to the existing, unchanged inode. No data blocks are copied or modified. The VFS and filesystem drivers lock the relevant directory inodes during this operation, ensuring the transition is atomic: any concurrent process either reads the old file or the new file, never a partial state.

`cp` followed by `rm` allocates a new inode, creates a new file, and copies blocks sequentially through user buffers. Any crash, process kill, or concurrent read during this window observes an incomplete file.
</details>

<details>
<summary><strong>3. Explain why `tail -f` fails to stream logs after logrotate runs, and why `tail -F` resolves this.</strong></summary>

**Answer**:
`tail -f` (lowercase) opens the file once, retrieves its file descriptor and underlying inode, and invokes `read()` or `inotify` on that descriptor. When `logrotate` runs, it renames the active file (e.g., `access.log` -> `access.log.1`) and signals the daemon to create a new `access.log` with a completely new inode. `tail -f` remains attached to the old, renamed inode (`access.log.1`) and never sees writes directed to the new file.

`tail -F` (capital F) enables `--follow=name --retry`. It tracks the file by its string path name. When `logrotate` moves the file, `tail -F` detects that the inode corresponding to the pathname has changed, closes the old descriptor, and automatically reopens the new file.
</details>

<details>
<summary><strong>4. What is a sparse file and how can it cause production disk exhaustion incidents?</strong></summary>

**Answer**:
A sparse file is a file containing large sequences of zero bytes where the filesystem allocates metadata for the apparent size, but does not allocate actual physical storage blocks on disk for the empty ranges (unallocated holes). The blocks are only allocated dynamically when non-zero bytes are written.

Risk: A backup utility (like legacy `tar` or `scp` without sparse flags) or an un-flagged `cp` will read the unallocated holes as actual zeroes and write explicit 4 KB data blocks of zeroes to the target filesystem. A 100 GB sparse file consuming only 500 MB on the source system can instantly fill a 100 GB target disk, triggering unexpected disk-full outages.
</details>

<details>
<summary><strong>5. How does the Linux kernel optimize access time updates using `relatime`?</strong></summary>

**Answer**:
Under strict POSIX rules, every read operation (`cat`, web server reads) must update the file's `atime` (access time) in its inode. On busy systems, this means every read I/O triggers a subsequent metadata write I/O, degrading disk performance and wearing SSD flash cells.

Modern Linux mounts filesystems with the `relatime` (relative access time) flag by default. Under `relatime`, the kernel only updates `atime` if:
1. The previous `atime` is older than the file's current `mtime` or `ctime`.
2. Or the previous `atime` is older than 24 hours.

This reduces metadata write amplification by over 90% while maintaining compatibility with utilities like `mutt` or backup tools that track whether files have been read since modification.
</details>

<details>
<summary><strong>6. Why should shell scripts avoid writing directly to fixed filenames in `/tmp`?</strong></summary>

**Answer**:
Fixed temporary file paths (e.g., `/tmp/output.log`) introduce critical **Time-of-Check to Time-of-Use (TOCTOU)** race conditions and symlink attacks:
1. An unprivileged local attacker can pre-create a symbolic link named `/tmp/output.log` pointing to `/etc/passwd` or `/etc/shadow`.
2. When an administrative or root automation script runs and writes to `/tmp/output.log`, the kernel follows the symlink and overwrites the sensitive target file with the script's output, corrupting the system.

Remedy: Always use `mktemp`, which securely creates a file with randomized characters and restrictive permissions (`0600`) using the `O_CREAT | O_EXCL` atomic system call flags.
</details>

<details>
<summary><strong>7. How do you recover disk space held by a deleted file that is still open by a process without rebooting?</strong></summary>

**Answer**:
1. Identify the holding process and file descriptor using `lsof`:
   ```bash
   sudo lsof +L1 /
   # Identifies COMMAND=app, PID=1420, FD=4, SIZE=15GB, NAME=/var/log/app.log (deleted)
   ```
2. If the daemon supports hot-reloading or reopen signals (like Nginx `kill -USR1` or logrotate hooks), send the signal to close and reopen the log descriptor.
3. If the daemon cannot be restarted immediately, truncate the open file descriptor directly through the `/proc` filesystem:
   ```bash
   sudo truncate -s 0 /proc/1420/fd/4
   ```
   This zeroes the data blocks allocated to the open descriptor, releasing disk space immediately while keeping the daemon alive.
</details>

<details>
<summary><strong>8. What is the technical difference between `mtime` and `ctime`?</strong></summary>

**Answer**:
- `mtime` (Modification Time): Tracks changes to the *file data content*. It changes when bytes are written, truncated, or appended to the file.
- `ctime` (Change Time): Tracks changes to the *file metadata (the inode)*. It changes whenever permissions (`chmod`), ownership (`chown`), hard link counts, extended attributes, or file content (`mtime`) change.

Note: Users can manually alter `mtime` and `atime` using the `touch` utility. However, users *cannot* forge or manually set `ctime` to an arbitrary timestamp; the kernel always sets `ctime` to the current system clock whenever an inode modification occurs.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Timestamp Inspection with `stat`</strong></summary>

**Task**: Create a test file, inspect all three timestamps using `stat`, update permissions, and observe which timestamp changes.

**Solution**:
```bash
# Create file
touch /tmp/time_test.txt
stat /tmp/time_test.txt

# Modify permissions (metadata change)
chmod 600 /tmp/time_test.txt
stat /tmp/time_test.txt
# Notice: 'Change' (ctime) is updated to current time; 'Modify' (mtime) remains unchanged!

# Cleanup
rm -f /tmp/time_test.txt
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Safe Scripted Temp Files with Traps</strong></summary>

**Task**: Write a self-contained bash script that creates a temporary directory using `mktemp`, writes a test payload, and guarantees deletion when the script exits.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

TEMP_DIR=$(mktemp -d)
trap 'rm -rf "$TEMP_DIR"' EXIT

echo "Created temporary directory: $TEMP_DIR"
echo "payload data" > "$TEMP_DIR/data.txt"
cat "$TEMP_DIR/data.txt"
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Simulate and Remediate Unlinked Open File Disk Leak</strong></summary>

**Task**: Write a background bash process that continuously writes to a log file, delete the file with `rm`, verify with `df` and `lsof` that disk space is not freed, and remediate using `/proc`.

**Solution**:
```bash
# 1. Start writer process
python3 -c "import time; f=open('/tmp/big_leak.log', 'w'); [f.write('X'*1024*1024 + '\n') or f.flush() or time.sleep(0.5) for _ in range(100)]" &
PY_PID=$!

# 2. Wait 3 seconds, then delete file
sleep 3
rm -f /tmp/big_leak.log

# 3. Verify file is unlinked but still consuming disk
lsof -p $PY_PID | grep "(deleted)"

# 4. Truncate via /proc descriptor
FD=$(lsof -p $PY_PID | awk '/big_leak.log/ {print $4}' | tr -d 'w')
truncate -s 0 "/proc/$PY_PID/fd/$FD"

# 5. Cleanup
kill $PY_PID 2>/dev/null || true
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): High-Performance Archive Copy</strong></summary>

**Task**: Compare the execution speed and attribute preservation of copying `/etc` to `/tmp/backup_regular` using `cp -r` versus `/tmp/backup_archive` using `cp -a`.

**Solution**:
```bash
# Test cp -a (preserves symlinks and permissions)
sudo cp -a /etc /tmp/backup_archive
sudo ls -ld /tmp/backup_archive/shadow
# Notice original root:shadow ownership and strict mode preserved

# Cleanup
sudo rm -rf /tmp/backup_archive
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Identify and Convert Sparse Files</strong></summary>

**Task**: Create a 2 GB sparse file, write a script that checks whether a given file is sparse, and copy it preserving its sparse nature.

**Solution**:
```bash
# Create sparse file
truncate -s 2G /tmp/sparse_sample.raw

# Detection script logic
is_sparse() {
    FILE="$1"
    APPARENT=$(stat -c %s "$FILE")
    ACTUAL=$(($(stat -c %b "$FILE") * 512))
    if [ "$ACTUAL" -lt "$APPARENT" ]; then
        echo "$FILE is SPARSE (Apparent: $APPARENT bytes, Actual: $ACTUAL bytes)"
    else
        echo "$FILE is DENSE"
    fi
}

is_sparse /tmp/sparse_sample.raw

# Copy with sparse preservation
cp --sparse=always /tmp/sparse_sample.raw /tmp/sparse_copy.raw
is_sparse /tmp/sparse_copy.raw

# Cleanup
rm -f /tmp/sparse_sample.raw /tmp/sparse_copy.raw
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Batch File Processing Guarding Against Argument Overflow</strong></summary>

**Task**: Author a robust bash script that generates 10,000 small test files in a directory and cleans them up without triggering `Argument list too long` errors.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

TEST_DIR="/tmp/scale_test"
mkdir -p "$TEST_DIR"

echo "Generating 10,000 files..."
python3 -c "import os; [open(f'$TEST_DIR/file_{i}.tmp', 'w').close() for i in range(10000)]"

echo "Attempting cleanup using find -delete (avoids exec arg limits)..."
find "$TEST_DIR" -type f -name "*.tmp" -delete

echo "Verifying cleanup:"
ls -1 "$TEST_DIR" | wc -l
rm -rf "$TEST_DIR"
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Command | Key Flags |
| :--- | :--- | :--- |
| **Touch File** | `touch <file>` | `-t YYYYMMDDhhmm.ss` (set custom time) |
| **Atomic Replace** | `mv -f <temp> <target>` | Guaranteed atomic on same filesystem |
| **Archive Copy** | `cp -a <src> <dst>` | Preserves ownership, mode, links, time |
| **Sparse Copy** | `cp --sparse=always` | Preserves unallocated zero holes |
| **Follow Live Log** | `tail -F <file>` | Follows by name; survives logrotate |
| **Read Big File** | `less -N <file>` | Paged streaming view; zero RAM bloat |
| **Detect File Type** | `file -b --mime-type` | Inspects magic bytes, ignores extension |
| **Secure Shred** | `shred -u -z -n 3` | Overwrites 3x with random + zeroes, deletes |

---

## 13. Further Reading & Official Documentation

- [GNU Coreutils: Working with Files](https://www.gnu.org/software/coreutils/manual/html_node/Basic-operations.html)
- [man 2 renameat2: Atomic File Renaming System Call](https://man7.org/linux/man-pages/man2/renameat2.2.html)
- [man 1 mktemp: Create Temporary Files Safely](https://man7.org/linux/man-pages/man1/mktemp.1.html)
- [Brendan Gregg: Linux Filesystem Latency & I/O Analysis](https://www.brendangregg.com/perf.html)
- [LWN.net: Filesystem relatime and atime mechanics](https://lwn.net/Articles/244829/)

---

[<- 03-filesystem-hierarchy-and-navigation.md](./03-filesystem-hierarchy-and-navigation.md) | [Back to Module Overview](./README.md) | [05-links-inodes-and-file-types.md ->](./05-links-inodes-and-file-types.md)
