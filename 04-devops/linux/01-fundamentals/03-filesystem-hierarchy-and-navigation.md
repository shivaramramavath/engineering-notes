<!--
================================================================================
Metadata:
  Title: Filesystem Hierarchy Standard (FHS) & Navigation Mastery
  Track: Linux & Systems Engineering / DevOps
  Difficulty: Beginner to Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 01-fundamentals/01-getting-started.md
  Key Terminology: FHS 3.0, UsrMerge, Virtual Filesystem, sysfs, procfs, devtmpfs, tmpfs, pwd, pushd, popd
================================================================================
-->

# Filesystem Hierarchy Standard (FHS) & Navigation Mastery

[<- 02-linux-architecture.md](./02-linux-architecture.md) | [Back to Module Overview](./README.md) | [04-working-with-files.md ->](./04-working-with-files.md)

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

Unlike Windows, which partitions storage across drive letters (`C:`, `D:`, `E:`), Linux organizes all physical drives, network shares, virtual pseudo-filesystems, and hardware device nodes into a **single unified hierarchical tree** originating at the root directory: `/`.

To prevent arbitrary fragmentation across Linux distributions, the Linux Foundation maintains the **Filesystem Hierarchy Standard (FHS)** (currently version 3.0). The FHS defines the designated location, purpose, and lifecycle of every primary directory.

For DevOps engineers and systems architects, understanding the FHS is critical:
- Placing database data files or application logs on the root partition (`/`) can trigger a disk-full condition that halts system daemons.
- Modern distributions implement **UsrMerge** (symlinking `/bin`, `/sbin`, and `/lib` directly into `/usr`), fundamentally altering how binaries are located.
- Synthetic filesystems (`/proc`, `/sys`, `/dev`, `/run`) do not consume physical storage blocks; they are dynamic kernel abstractions essential for automation and monitoring.

---

## 2. Core Concepts

### The FHS 3.0 Directory Topology
Every directory in the Linux hierarchy has a designated contract regarding data persistence, volatility, and administrative scope:

```
/ (Root Directory)
├── bin -> usr/bin          (Essential user command binaries; UsrMerge symlink)
├── sbin -> usr/sbin        (Essential system admin binaries; UsrMerge symlink)
├── lib -> usr/lib          (Essential shared libraries & kernel modules; symlink)
├── lib64 -> usr/lib64      (64-bit architecture libraries; symlink)
├── boot/                   (Static files of the bootloader, kernel vmlinuz, initramfs)
├── dev/                    (Device nodes managed by udev and devtmpfs; e.g. /dev/sda, /dev/null)
├── etc/                    (Host-specific system-wide configuration files; text only)
├── home/                   (User personal data directories; e.g. /home/ubuntu)
├── root/                   (Home directory for the root superuser)
├── media/                  (Mount points for removable media; USB drives, CD-ROMs)
├── mnt/                    (Temporarily mounted filesystems used by sysadmins)
├── opt/                    (Add-on third-party application software packages)
├── proc/                   (Virtual filesystem exposing kernel process and system status)
├── sys/                    (Virtual filesystem exposing device drivers, buses, and cgroups)
├── run/                    (Volatile runtime data since last boot; PID files, sockets, tmpfs)
├── srv/                    (Site-specific data served by this system; web, FTP)
├── tmp/                    (Temporary files; cleared on reboot, often mounted as tmpfs)
├── usr/                    (Secondary hierarchy for read-only user data and system binaries)
│   ├── bin/                (Standard user binaries: python, git, curl)
│   ├── sbin/               (System administration binaries: iptables, useradd)
│   ├── lib/                (Object libraries and architecture-specific helpers)
│   ├── local/              (Tertiary hierarchy for locally compiled / custom software)
│   └── share/              (Architecture-independent shared data: man pages, docs)
└── var/                    (Variable data files: files whose content dynamically grows)
    ├── log/                (System and application log files: syslog, nginx, auth.log)
    ├── lib/                (Persistent application state: docker, postgresql, apt cache)
    ├── spool/              (Queued application data: mail queues, cron jobs)
    └── cache/              (Application cache data: package manager metadata)
```

### UsrMerge Architecture
In older Unix systems, `/bin` held minimal binaries needed to boot and repair the system when `/usr` resided on a separate physical disk. With modern storage, this dual-hierarchy became redundant and error-prone. Modern distributions (Ubuntu 24.04, Debian 12, RHEL 9) implement **UsrMerge**:
- `/bin` is a symbolic link to `/usr/bin`.
- `/sbin` is a symbolic link to `/usr/sbin`.
- `/lib` is a symbolic link to `/usr/lib`.
- `/lib64` is a symbolic link to `/usr/lib64`.

This unification guarantees that tools behave identically regardless of whether they are invoked from `/bin` or `/usr/bin`.

---

## 3. How It Works Internally

### Real vs. Synthetic (Pseudo) Filesystems
The root tree contains both physical storage filesystems (ext4, XFS, Btrfs) and in-memory virtual filesystems generated on-the-fly by the kernel:

```
+--------------------------------------------------------------------------+
| ROOT FILESYSTEM TREE (VFS)                                               |
|                                                                          |
| [/] -> Block Device Mount (/dev/nvme0n1p2 formatted as ext4 or XFS)      |
|  |                                                                       |
|  +-- [/etc], [/home], [/usr], [/var] -> Stored on physical NVMe storage  |
|  |                                                                       |
|  +-- [/proc] -> procfs (In-Memory Kernel Data Structures)                |
|  |     - Process table (/proc/1, /proc/$$)                               |
|  |     - Kernel tunables (/proc/sys/net/ipv4/ip_forward)                 |
|  |                                                                       |
|  +-- [/sys] -> sysfs (Kernel Object / Device Model / Cgroups)            |
|  |     - PCI bus topology, block queue parameters                        |
|  |     - Control Groups v2 hierarchy (/sys/fs/cgroup)                    |
|  |                                                                       |
|  +-- [/dev] -> devtmpfs + udev (Device Nodes)                            |
|  |     - Character devices (/dev/urandom, /dev/tty)                      |
|  |     - Block devices (/dev/sda, /dev/nvme0n1)                          |
|  |                                                                       |
|  +-- [/run] & [/tmp] -> tmpfs (RAM-backed Volatile Memory)               |
|        - Fast read/write without disk wear; lost on power off            |
+--------------------------------------------------------------------------+
```

When you read `/proc/cpuinfo`, no bytes are read from disk; the kernel's `procfs` handler dynamically formats internal CPU topology structures into ASCII text and sends them directly to your process's read buffer.

---

## 4. Syntax & Usage

### Precision Directory Navigation
```bash
# Print working directory: Physical path (-P, resolves symlinks) vs Logical path (-L)
pwd -L
pwd -P

# Navigate to user's home directory
cd ~
cd

# Jump back to the immediately previous working directory
cd -

# Move up two directory levels
cd ../..

# Directory stack management (pushd, popd, dirs)
pushd /var/log           # Adds /var/log to stack and enters it
pushd /etc/systemd       # Adds /etc/systemd to stack
dirs -v                  # Lists directory stack with index numbers
popd                     # Removes top directory from stack and jumps back
```

### Advanced Directory Listing with `ls`
```bash
# Detailed list with human-readable sizes and hidden files
ls -lah

# List files sorted by modification time (most recent first)
ls -lt

# List files sorted by size (largest first)
ls -lhS

# Show file inode numbers
ls -li

# Print file tree with depth constraint (install via apt install tree)
tree -L 2 /var
```

---

## 5. Practical Examples

### Example 1: Differentiating Logical vs. Physical Path on UsrMerge Systems
```bash
# Navigate to /bin (which is a symlink)
cd /bin

# Print logical working directory (respects shell traversal)
pwd -L
# Output: /bin

# Print physical working directory (canonical resolved filesystem path)
pwd -P
# Output: /usr/bin

# Inspect the symlink structure
ls -ld /bin /sbin /lib
# lrwxrwxrwx 1 root root 7 Apr 22 2024 /bin -> usr/bin
# lrwxrwxrwx 1 root root 8 Apr 22 2024 /sbin -> usr/sbin
# lrwxrwxrwx 1 root root 7 Apr 22 2024 /lib -> usr/lib
```

### Example 2: Inspecting Dynamic Kernel Tunables in `/proc`
```bash
# Check if IPv4 packet forwarding is enabled in the kernel
cat /proc/sys/net/ipv4/ip_forward
# Output: 0 (disabled)

# Check system uptime directly from kernel memory (seconds running, seconds idle)
cat /proc/uptime

# Read the command line arguments used to boot the running Linux kernel
cat /proc/cmdline
```

### Example 3: Verifying RAM-Backed Filesystems (`tmpfs`)
```bash
# Inspect all tmpfs mounts across the system
df -hT | grep tmpfs

# Sample Output:
# tmpfs          tmpfs     3.9G     0  3.9G   0% /dev/shm
# tmpfs          tmpfs     1.6G  1.2M  1.6G   1% /run
# tmpfs          tmpfs     5.0M     0  5.0M   0% /run/lock
# tmpfs          tmpfs     3.9G  4.0K  3.9G   1% /tmp

# Create an ephemeral RAM-disk test file that will vanish on reboot
echo "sensitive_in_memory_token" > /run/my_token.txt
```

### Example 4: Preventing Partition Exhaustion by Mapping `/var` Separately
```bash
# Problem: Docker containers and system logs grow uncontrolled, filling root /
# Inspect which directories consume the most space under /var
sudo du -h --max-depth=1 /var | sort -hr | head -n 5

# Production solution: Ensure /var/log and /var/lib/docker reside on dedicated EBS/NVMe mounts
lsblk -f
# sda1  ext4   /
# sdb1  ext4   /var/log
# sdc1  xfs    /var/lib/docker
```

### Example 5: Rapid Directory Stack Traversal in DevOps Scripts
```bash
# Complex administrative script managing multiple disparate paths
WORK_DIR="/tmp/release_build"
TARGET_DIR="/etc/nginx/sites-available"

mkdir -p "$WORK_DIR"
pushd "$WORK_DIR" > /dev/null

# Generate config file in temporary work directory
cat << 'EOF' > app.conf
server {
    listen 80;
    server_name api.internal;
}
EOF

# Move to target directory using stack
pushd "$TARGET_DIR" > /dev/null
echo "Currently configuring: $(pwd)"

# Copy generated config from previously stacked path
sudo cp "$WORK_DIR/app.conf" ./

# Unwind stack back to origin
popd > /dev/null
popd > /dev/null
echo "Returned to original directory: $(pwd)"
```

---

## 6. Best Practices

- **Never Fill Root (`/`)**: Always isolate `/var`, `/home`, and `/tmp` onto separate partitions or volumes for high-throughput production servers. If an application fills `/var/log`, the core OS and SSH daemon remain operational.
- **Store Local Custom Binaries in `/usr/local/bin`**: Never place custom compiled binaries or in-house Python scripts in `/bin` or `/usr/bin`. Package managers (`apt`/`dnf`) overwrite `/usr/bin` files without warning.
- **Use `/opt` for Self-Contained Third-Party Software**: Proprietary or multi-component vendor suites (e.g., Datadog agent, Dynatrace, Google Chrome) belong in `/opt/<vendor>`, keeping dependencies encapsulated.
- **Use `/srv` for Application Data**: Static web content or served FTP data belongs in `/srv/www` or `/srv/data` rather than arbitrary `/home` directories.
- **Always Resolve Paths with `readlink -f` or `pwd -P` in Scripts**: Scripts that rely on relative path traversals (`../../`) can fail unexpectedly when navigating through symbolic links.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| Placing large database files under `/tmp` | Modern distributions mount `/tmp` as `tmpfs` (RAM-backed); massive writes consume physical RAM, triggering OOM killer. | Configure database data directories under `/var/lib/<app>` on persistent block storage. |
| Writing logs to `/etc` | Violates FHS; `/etc` is reserved strictly for static, human-readable configuration files. | Direct all logging output to `/var/log/<app>/`. |
| Script breaks when `cd /bin` followed by `cd ..` | On UsrMerge systems, `/bin` is a symlink to `usr/bin`. Using `cd ..` in POSIX shells can navigate to `/usr` or `/` depending on logical vs physical tracking. | Always use explicit absolute paths or `cd -P ..` in automation scripts. |
| Assuming `/proc` files can be edited with `nano` or `vim` | `/proc` files are virtual kernel interfaces without physical size; editors attempting to create swap files or seek write boundaries fail with `EOPNOTSUPP`. | Use standard shell redirection: `echo 1 \| sudo tee /proc/sys/path/to/param`. |
| Leaving core dumps or temporary archives in `/root` | Consumes space on the primary root volume and prevents audit visibility for regular operations teams. | Store dumps in `/var/crash` or dedicated volume mounts. |
| Deleting files under `/dev` manually (`rm /dev/sda`) | Deletes the device node interface used by the kernel to communicate with the storage controller. | Use `udevadm trigger` or reboot to regenerate missing device nodes. |

---

## 8. Performance, Security & Cost Implications

- **Performance**: Accessing `/proc` and `/sys` incurs kernel context switches to format ASCII buffers. Querying `/proc` in high-frequency loops (e.g., polling `/proc/<PID>/io` 1,000 times a second) burns noticeable CPU. Use event-driven APIs (`inotify`, eBPF) where possible.
- **Security**: Filesystem mount options defined in `/etc/fstab` protect the system against privilege escalation:
  - Mount `/tmp` with `noexec,nosuid,nodev` to prevent attackers from executing downloaded malware binaries from the world-writable directory.
  - Mount `/var` with `nosuid,nodev`.
- **Cost**: Unmonitored `/var/log` or container cache directories (`/var/lib/docker`) on cloud instances can exhaust EBS volumes, forcing expensive out-of-band disk expansion operations.

---

## 9. Comparison Table

| Directory | Data Type | Persistence | Backing Mechanism | Typical Mount Flags |
| :--- | :--- | :--- | :--- | :--- |
| `/etc` | System configuration | Permanent | Physical disk (ext4/XFS) | `rw,relatime` |
| `/usr` | Static binaries & libraries | Permanent | Physical disk (read-only safe) | `ro` or `rw` |
| `/var` | Dynamic variable data | Permanent | Physical disk (dedicated volume recommended) | `rw,nosuid,nodev` |
| `/tmp` | Ephemeral temporary files | Cleared on reboot | `tmpfs` (RAM) or disk partition | `rw,nosuid,nodev,noexec` |
| `/run` | Runtime process state | Cleared on reboot | `tmpfs` (RAM) | `rw,nosuid,nodev,mode=755` |
| `/proc` | Process & kernel internals | Dynamic / In-Memory | `procfs` (Kernel virtual) | `rw,nosuid,nodev,noexec` |
| `/sys` | Device & hardware model | Dynamic / In-Memory | `sysfs` (Kernel virtual) | `rw,nosuid,nodev,noexec` |
| `/dev` | Hardware device nodes | Dynamic / In-Memory | `devtmpfs` (Kernel managed) | `rw,nosuid,noexec,mode=755` |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain the Filesystem Hierarchy Standard (FHS) and why it matters in enterprise Linux deployments.</strong></summary>

**Answer**:
The FHS is a standardized specification managed by the Linux Foundation that defines the directory structure and contents in Unix-like operating systems. It specifies which types of files (static vs. variable, persistent vs. transient, architecture-dependent vs. architecture-independent) belong in each path.

In enterprise deployments, adherence to FHS guarantees predictability across heterogeneous distributions (Ubuntu, RHEL, SUSE). Backup scripts know to target `/etc`, `/var`, and `/home` while excluding `/proc`, `/sys`, and `/run`. Security teams can enforce strict partition mount options (`noexec` on `/tmp`), and package managers can install software without clobbering local modifications in `/usr/local` or `/opt`.
</details>

<details>
<summary><strong>2. What is "UsrMerge" and what architectural problem does it solve?</strong></summary>

**Answer**:
Historically, early Unix systems separated `/bin`, `/sbin`, and `/lib` (located on the small root partition needed to boot the machine) from `/usr/bin`, `/usr/sbin`, and `/usr/lib` (which resided on a larger, separate disk mounted later in the boot sequence).

As hard drive capacities grew and initramfs systems evolved to mount all required filesystems before starting PID 1, this dual-directory split became redundant and caused compatibility bugs (e.g., packages installing binaries to `/usr/bin` that boot scripts expected in `/bin`). UsrMerge replaces `/bin`, `/sbin`, and `/lib` with symbolic links pointing to their corresponding directories inside `/usr`, establishing a single unified location for all system software.
</details>

<details>
<summary><strong>3. How does `/run` differ from `/var/run` and `/tmp`?</strong></summary>

**Answer**:
- `/run` is a dedicated, RAM-backed `tmpfs` filesystem mounted very early in the boot sequence (before root filesystems are mounted read-write). It stores volatile runtime state required by system daemons (PID files, Unix domain sockets, lockfiles) that must not survive a reboot.
- `/var/run` is a legacy path preserved purely for backward compatibility; on modern systemd distributions, `/var/run` is a symbolic link to `/run`.
- `/tmp` is intended for general-purpose temporary files created by users and applications. While `/run` is restricted to root and system daemons (permissions `0755`), `/tmp` is world-writable with the sticky bit enabled (`1777`).
</details>

<details>
<summary><strong>4. What is the difference between `pwd -L` and `pwd -P`?</strong></summary>

**Answer**:
- `pwd -L` (Logical): Displays the logical current working directory as tracked by the shell, preserving any symbolic links traversed to reach the path.
- `pwd -P` (Physical): Resolves all symbolic links along the path and outputs the actual canonical directory on the physical filesystem.

Example: If `/var/mail` is a symlink to `/var/spool/mail`, navigating via `cd /var/mail` will show `/var/mail` under `pwd -L`, but `/var/spool/mail` under `pwd -P`. In bash automation scripts, using `pwd -P` prevents errors when calculating relative offsets.
</details>

<details>
<summary><strong>5. Why is `/proc` described as a "pseudo-filesystem"? Can you edit files in `/proc`?</strong></summary>

**Answer**:
`/proc` does not exist on any physical storage media or block device. It is a synthetic abstraction generated dynamically in memory by the Linux kernel's `procfs` driver. File sizes in `/proc` almost universally show as `0` bytes because content is synthesized on-demand when a process issues a `read()` system call.

Certain files under `/proc/sys/` (the sysctl interface) are writable and serve as direct control interfaces for kernel parameters (e.g., `/proc/sys/net/ipv4/ip_forward`). However, they cannot be edited with standard interactive text editors like `vim` or `nano` because these editors create temporary swap files and attempt complex seek operations unsupported by virtual file handlers. Writable `/proc` entries must be modified via atomic stream writes, e.g., `echo 1 > /proc/sys/net/ipv4/ip_forward` or the `sysctl -w` command.
</details>

<details>
<summary><strong>6. Explain the operational risks of hosting `/var/log` on the primary root (`/`) filesystem.</strong></summary>

**Answer**:
If `/var/log` resides on the unpartitioned root filesystem, an unconstrained application log storm, repetitive crash loop, or denial-of-service attack can completely consume the remaining storage capacity.

When a root filesystem reaches 100% capacity:
1. System daemons cannot write PID files or sockets, crashing critical services.
2. The SSH daemon (`sshd`) cannot allocate pseudoterminals or write authentication sessions, locking engineers out of remote access.
3. Database engines (e.g., PostgreSQL, MySQL) crash or enter emergency read-only mode to prevent write-ahead log (WAL) corruption.

Best practice dictates mounting `/var/log` on a separate logical volume (LVM) with alerting thresholds at 80% capacity.
</details>

<details>
<summary><strong>7. How do `pushd` and `popd` function, and why are they preferable to `cd` in automation?</strong></summary>

**Answer**:
`pushd` and `popd` manage an in-memory Last-In-First-Out (LIFO) directory stack maintained by the shell:
- `pushd <dir>` pushes the current directory onto the stack and changes the active directory to `<dir>`.
- `popd` pops the top directory off the stack and immediately returns the shell to that directory.

In automation scripts, using raw `cd` requires manually recording the starting path in a variable to navigate back. If an intermediate command fails or an exit trap executes, the script may run subsequent destructive commands in an unintended directory. `pushd` and `popd` provide structured, clean navigation guarantees.
</details>

<details>
<summary><strong>8. What is the difference between `/opt` and `/usr/local`?</strong></summary>

**Answer**:
- `/usr/local` is designated for software built and installed locally by the system administrator (e.g., using `make install` from source code). It mirrors the standard system hierarchy: `/usr/local/bin`, `/usr/local/lib`, `/usr/local/etc`, blending individual binaries and libraries into standard search paths.
- `/opt` (Optional) is reserved for self-contained, monolithic third-party software packages. Each package installs into its own dedicated subdirectory (e.g., `/opt/google/chrome`, `/opt/datadog-agent`), keeping all of its internal binaries, libraries, configs, and documentation isolated in one self-contained tree.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Identify All Virtual and Physical Mounts</strong></summary>

**Task**: Write a shell pipeline that lists all mounted filesystems, separating physical storage block devices (ext4, xfs, btrfs) from synthetic filesystems (procfs, sysfs, tmpfs).

**Solution**:
```bash
echo "=== PHYSICAL BLOCK MOUNTS ==="
findmnt -t ext4,xfs,btrfs,vfat

echo -e "\n=== SYNTHETIC / VIRTUAL MOUNTS ==="
findmnt -t proc,sysfs,tmpfs,devtmpfs
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Investigate UsrMerge Links</strong></summary>

**Task**: Verify whether your running distribution has implemented UsrMerge by checking the file type and target of `/bin`, `/sbin`, and `/lib`.

**Solution**:
```bash
for dir in /bin /sbin /lib /lib64; do
    if [ -L "$dir" ]; then
        echo "$dir is a SYMLINK -> $(readlink "$dir")"
    else
        echo "$dir is a CLASSICAL DIRECTORY"
    fi
done
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Audit Directory Tree Disk Utilization</strong></summary>

**Task**: Write a script that audits the top 5 space-consuming directories inside `/var`, formatted in human-readable gigabytes or megabytes, while suppressing permission errors on restricted paths.

**Solution**:
```bash
sudo du -xh --max-depth=2 /var 2>/dev/null | sort -hr | head -n 6
```
*(Note: `-x` ensures `du` does not cross filesystem boundaries into mounted network shares or containers).*
</details>

<details>
<summary><strong>Exercise 4 (Medium): Implement Safe Directory Traversal with `pushd`/`popd`</strong></summary>

**Task**: Author a shell script that iterates through three separate directories (`/etc`, `/var/log`, `/usr/share`), performs an inspection task in each, and guarantees that the terminal ends in the original starting directory regardless of failures.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

ORIGIN=$(pwd)
cleanup() {
    cd "$ORIGIN"
    echo "Cleaned up. Current location: $(pwd)"
}
trap cleanup EXIT

for target in /etc /var/log /usr/share; do
    pushd "$target" > /dev/null
    echo "Inspecting $(pwd)... Total items: $(ls -1 | wc -l)"
    popd > /dev/null
done
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Identify Open Unlinked Files Consuming Disk Space</strong></summary>

**Task**: In production, deleting a file with `rm` does not free disk blocks if a running daemon still holds the file descriptor open. Write a command to detect all deleted but unreleased files in `/var` or `/tmp`.

**Solution**:
```bash
sudo lsof +L1 / | grep -E "deleted|/var|/tmp"
# Displays PID, process name, file descriptor, and size of held deleted files.
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Build a Hardened Production Mount Layout Script</strong></summary>

**Task**: Generate an `/etc/fstab` configuration snippet demonstrating security-hardened mount options for `/tmp`, `/var`, `/var/log`, and `/home`.

**Solution**:
```bash
cat << 'EOF'
# Hardened Production Filesystem Mount Layout
# <file system>                             <mount point>  <type>  <options>                                 <dump> <pass>
UUID=11111111-1111-1111-1111-111111111111  /              ext4    defaults,noatime                          0      1
UUID=22222222-2222-2222-2222-222222222222  /home          ext4    defaults,nodev,nosuid                     0      2
UUID=33333333-3333-3333-3333-333333333333  /var           ext4    defaults,nodev,nosuid                     0      2
UUID=44444444-4444-4444-4444-444444444444  /var/log       ext4    defaults,nodev,nosuid,noexec              0      2
tmpfs                                       /tmp           tmpfs   defaults,nodev,nosuid,noexec,size=2G      0      0
tmpfs                                       /dev/shm       tmpfs   defaults,nodev,nosuid,noexec,size=2G      0      0
EOF
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Command / Flag | Action | Example |
| :--- | :--- | :--- |
| `pwd -P` | Print physical path (resolves symlinks) | `pwd -P` |
| `cd -` | Jump to previous directory | `cd -` |
| `pushd <dir>` | Push directory to stack and enter it | `pushd /etc` |
| `popd` | Pop directory from stack and return | `popd` |
| `dirs -v` | Print directory stack with indices | `dirs -v` |
| `findmnt` | List all active filesystem mount points | `findmnt -l` |
| `du -xh --max-depth=1` | Analyze directory size without crossing mounts | `sudo du -xh --max-depth=1 /` |
| `readlink -f <path>` | Print canonical resolved target path | `readlink -f /bin/sh` |

---

## 13. Further Reading & Official Documentation

- [Linux Foundation Filesystem Hierarchy Standard 3.0](https://refspecs.linuxfoundation.org/FHS_3.0/fhs-3.0.html)
- [The Case for the /usr Merge (Freedesktop.org)](https://www.freedesktop.org/wiki/Software/systemd/TheCaseForTheUsrMerge/)
- [man 7 hier: Description of the Filesystem Hierarchy](https://man7.org/linux/man-pages/man7/hier.7.html)
- [man 8 findmnt: Find a Filesystem](https://man7.org/linux/man-pages/man8/findmnt.8.html)
- [CIS Linux Benchmark: Partitioning and Mount Options Guidelines](https://www.cisecurity.org/benchmark/ubuntu_linux)

---

[<- 02-linux-architecture.md](./02-linux-architecture.md) | [Back to Module Overview](./README.md) | [04-working-with-files.md ->](./04-working-with-files.md)
