# In-Memory Storage and Graph Storage Drivers: Tmpfs and Overlay2

```
Track: DevOps & Containerization
Module: 04-volumes-and-networking
Prerequisites: 04-volumes-and-networking/01-volumes-and-bind-mounts.md, 01-fundamentals/06-image-layers-and-union-filesystems.md
Difficulty: Advanced
Estimated Duration: 45 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Directive Reference](#4-syntax--directive-reference)
5. [Practical Implementation & Examples](#5-practical-implementation--examples)
6. [Production Best Practices & Antipatterns](#6-production-best-practices--antipatterns)
7. [Common Pitfalls & Troubleshooting](#7-common-pitfalls--troubleshooting)
8. [Performance Tuning & Resource Optimization](#8-performance-tuning--resource-optimization)
9. [Security Hardening & Threat Model](#9-security-hardening--threat-model)
10. [Architectural Comparison Table](#10-architectural-comparison-table)
11. [Real-World Scenario Walkthrough](#11-real-world-scenario-walkthrough)
12. [Self-Assessment & Interview Questions](#12-self-assessment--interview-questions)
13. [Hands-on Practice Exercises](#13-hands-on-practice-exercises)
14. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

Container storage involves two distinct layers:
1. **The Graph Storage Driver (OverlayFS2)**: Manages how immutable image layers and the container's ephemeral read-write layer are merged together on disk. Understanding this driver is vital for diagnosing container disk bloat, write-amplification, and root partition exhaustion.
2. **Tmpfs Mounts**: A specialized mount type where data is stored exclusively in host volatile memory (RAM or swap) without touching physical disk platters or SSDs.

Tmpfs mounts solve two acute architectural challenges:
- **Security for Ephemeral Secrets**: Sensitive runtime data (session tokens, TLS private keys, decrypted credentials) written to a standard container layer persist on disk and can be recovered through forensic image inspection. Tmpfs guarantees that secrets vanish immediately upon container termination.
- **Ultra-High Throughput I/O**: High-churn temporary operations (compiler workspaces, image transcoding buffers, temporary socket files) avoid disk wear and OverlayFS2 copy-up overhead.

---

## 2. Core Concepts

### Tmpfs Mounts
A `tmpfs` mount is a temporary filesystem residing entirely in the Linux host's volatile kernel page cache (RAM).
- It never touches non-volatile storage (disks).
- It is private to the container; other containers and the host cannot access it directly.
- When the container stops, the `tmpfs` mount is unmounted and all data is permanently purged from RAM.
- Linux containers only; not natively supported in Windows containers.

### Docker Storage Drivers
Docker uses storage drivers to implement Copy-on-Write (CoW) filesystems:
- **`overlay2`**: The current universal industry standard for Linux. High performance, native Linux kernel support, low page cache overhead.
- **`btrfs` / `zfs`**: Advanced copy-on-write filesystems with snapshotting and subvolume quotas. Requires dedicated host filesystem partitioning.
- **`vfs`**: Naive copy driver with no CoW (copies entire directories). Slow, massive disk consumption; used strictly for testing or rootless scenarios where kernel OverlayFS is unavailable.
- **`devicemapper` / `aufs`**: Deprecated legacy drivers. Removed in modern Docker releases.

---

## 3. How It Works Internally

### Overlay2 Architecture Deep Dive

The `overlay2` driver uses Linux kernel union mounting to stack directories on top of each other and present them as a unified single filesystem (`merged`).

```
+-------------------------------------------------------------+
|                Unified View: /var/lib/docker/               |
|                   overlay2/<id>/merged                      |
+-------------------------------------------------------------+
                               ^
                               |
              +----------------+----------------+
              |                                 |
+-------------+---------------+   +-------------+---------------+
|          Upperdir           |   |           Workdir           |
| (Container Read-Write Layer)|   |  (Internal kernel staging   |
| Changes, new files, deletes |   |   for copy-up operations)   |
+-----------------------------+   +-----------------------------+
              |
              v
+-------------------------------------------------------------+
|                          Lowerdir                           |
| (Read-Only Image Layers, stacked bottom-up, separated by :) |
| Layer N (Top layer)                                         |
| Layer 2                                                     |
| Layer 1 (Base image rootfs)                                 |
+-------------------------------------------------------------+
```

### The Copy-Up Mechanism (Write Amplification)
1. **Reading an Existing File**: The kernel looks in `upperdir`. If not found, it traverses down `lowerdir` layers. When found, it is read directly. (Cost: Low).
2. **Writing to an Existing File**: If a 2GB file exists in `lowerdir` and a process modifies 1 byte, the kernel must first **copy the entire 2GB file up** from `lowerdir` into `upperdir` before applying the modification. (Cost: Severe write-amplification and latency spike!).
3. **Deleting a File**: Deleting a file in `lowerdir` does not delete it from disk; instead, the kernel creates a **whiteout file** (character device with 0/0 device numbers) in `upperdir` to mask it from the `merged` view.

---

## 4. Syntax & Directive Reference

### Tmpfs Mount Syntax

```bash
# 1. Basic tmpfs mount via shorthand
docker run -d --name cache-app --tmpfs /app/cache redis:alpine

# 2. Modern --mount syntax with size and permissions controls
docker run -d \
  --name secure-app \
  --mount type=tmpfs,target=/app/secrets,tmpfs-size=64m,tmpfs-mode=0700 \
  alpine:3.21 sleep 3600
```

### Tmpfs Mount Parameters
- `target`: The directory path inside the container where tmpfs will be mounted.
- `tmpfs-size`: Byte ceiling for the tmpfs mount (e.g., `64m`, `512m`, `1g`). Prevents container processes from consuming all host RAM and triggering host-level OOM crashes.
- `tmpfs-mode`: POSIX octal file permissions (e.g., `0700` restricts read/write/execute strictly to file owner).

### Verifying Active Storage Driver

```bash
# Query Docker daemon storage driver configuration
docker info --format '{{.Driver}}'
# Expected output: overlay2

# Inspect underlying backing filesystem
docker info --format '{{.DriverStatus}}'
```

---

## 5. Practical Implementation & Examples

### Example 1: Securing Sensitive Tokens in In-Memory Tmpfs
Ensuring runtime session keys never write to disk storage.

```bash
docker run -d \
  --name token-broker \
  --mount type=tmpfs,target=/run/secrets/tokens,tmpfs-size=32m,tmpfs-mode=0700 \
  node:22-alpine \
  node -e "
    const fs = require('fs');
    fs.writeFileSync('/run/secrets/tokens/jwt.key', 'SUPER_SECRET_HMAC_SECRET');
    console.log('Secret stored safely in RAM only');
    setInterval(() => {}, 1000);
  "

# Verify file exists inside running container
docker exec token-broker cat /run/secrets/tokens/jwt.key

# Verify file DOES NOT exist anywhere in /var/lib/docker/overlay2/ on the host disk!
```

### Example 2: Running a High-Performance In-Memory SQLite Database
Using `tmpfs` for an in-memory SQLite database that processes 50,000 queries per second with zero disk wear.

```bash
docker run -d \
  --name sqlite-mem \
  --mount type=tmpfs,target=/db,tmpfs-size=256m \
  alpine:3.21 \
  sh -c "
    apk add --no-cache sqlite &&
    sqlite3 /db/test.db 'CREATE TABLE benchmarks (id INTEGER PRIMARY KEY, val TEXT);' &&
    echo 'SQLite in RAM ready' &&
    sleep 3600
  "
```

### Example 3: Running Read-Only Root Filesystem with Tmpfs for State
Hardening a production container so attackers cannot modify binaries, while providing writable RAM for necessary temporary files.

```bash
docker run -d \
  --name hardened-nginx \
  --read-only \
  -p 8080:80 \
  --mount type=tmpfs,target=/var/run,tmpfs-size=16m \
  --mount type=tmpfs,target=/var/cache/nginx,tmpfs-size=64m \
  --mount type=tmpfs,target=/tmp,tmpfs-size=32m \
  nginx:alpine
```
> **Security Impact**: Any attempt by malware to write to `/bin`, `/usr`, or `/etc` is immediately blocked by the Linux kernel with `Read-only file system`, while Nginx operates normally with its PID and cache in memory.

### Example 4: Inspecting Overlay2 Directory Stacking on Host Linux
Examining how Docker manages `lowerdir`, `upperdir`, and `merged` directly on disk.

```bash
# 1. Run a container and write a unique file
CONTAINER_ID=$(docker run -d alpine:3.21 sh -c "echo 'hello from upperdir' > /my-custom-file.txt && sleep 3600")

# 2. Extract GraphDriver metadata
docker inspect $CONTAINER_ID --format '{{json .GraphDriver.Data}}' | jq .

# Expected output shows:
# {
#   "LowerDir": "/var/lib/docker/overlay2/.../diff",
#   "MergedDir": "/var/lib/docker/overlay2/.../merged",
#   "UpperDir": "/var/lib/docker/overlay2/.../diff",
#   "WorkDir": "/var/lib/docker/overlay2/.../work"
# }

# 3. View the file directly in the UpperDir on the host (as root):
sudo ls -la $(docker inspect $CONTAINER_ID --format '{{.GraphDriver.Data.UpperDir}}')
# /my-custom-file.txt is visible in UpperDir!
```

### Example 5: Configuring Storage Driver Options in `daemon.json`
Tuning Overlay2 options in `/etc/docker/daemon.json`.

```json
{
  "storage-driver": "overlay2",
  "storage-opts": [
    "overlay2.size=20GB"
  ]
}
```
> **Note**: `overlay2.size` sets a per-container writable layer quota, supported only when the backing filesystem is XFS formatted with `ftype=1` and `pquota` enabled.

---

## 6. Production Best Practices & Antipatterns

### Best Practices
- **Always Bound Tmpfs Mount Sizes**: Always specify `tmpfs-size` (e.g., `tmpfs-size=128m`). Unbounded `tmpfs` mounts consume the host's entire RAM pool, causing kernel panic or host-level Out-Of-Memory crashes.
- **Combine `--read-only` with `tmpfs` for Production Hardening**: Deploy web services with `--read-only` and mount `tmpfs` on `/tmp` and `/var/run`.
- **Format XFS with `ftype=1`**: When installing host Linux machines for Docker nodes with XFS, ensure `xfs_info` reports `ftype=1`. `ftype=0` lacks `d_type` support and completely breaks Overlay2.
- **Monitor Overlay2 Disk Utilization**: Track `/var/lib/docker/overlay2` using disk monitoring tools to detect rogue containers writing unmounted state.

### Anti-Patterns
- **Writing High-Churn Temporary Data to Ephemeral Layers**: Storing temporary transcode files, logs, or cache files directly in the container without a volume or tmpfs mount causes severe copy-up write amplification and fills host root disks.
- **Using Tmpfs for Persistent Data**: Data inside `tmpfs` is wiped permanently upon container restart or stop.
- **Using Legacy Storage Drivers**: Running `devicemapper`, `vfs`, or `aufs` in modern environments introduces massive CPU/memory penalties.

---

## 7. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| Host server freezes or kills critical processes | Unbounded `tmpfs` mount consumed all available host RAM | `free -m` and `df -h /run` | Always set `tmpfs-size=...` (e.g., `64m`). |
| Container exits immediately when run with `--read-only` | Application requires write access to `/tmp` or `/var/run/app.pid` | `docker logs <container>` shows `Read-only file system` | Mount `tmpfs` at `/tmp` and `/var/run`. |
| Docker daemon fails to start with `overlay2: not supported` | Backing filesystem is XFS formatted without `d_type` support (`ftype=0`) | `xfs_info /var/lib/docker` | Reformat host partition with `mkfs.xfs -n ftype=1 /dev/...`. |
| Host disk space full, but `docker system df` reports low usage | Massive files written and deleted inside a running container, held open by process descriptors | `lsof +L1` on host | Terminate or restart container holding deleted file descriptors open. |
| Modifying a large file in container takes 30 seconds | Overlay2 copy-up mechanism copying multi-GB file from `lowerdir` to `upperdir` | Inspect disk I/O with `iostat -xz 1` | Store frequently modified large files in a Docker Volume instead of image layers. |
| Tmpfs mount fails on Windows Docker container | Windows kernel does not support Linux VFS `tmpfs` mounts | Inspect error message | Use named pipe or RAM drive software on Windows. |

---

## 8. Performance Tuning & Resource Optimization

1. **Avoid Copy-Up Penalties**:
   Never update large pre-existing files (e.g., modifying a 500MB embedded database or log file) inside the image layer. Always place high-write files in a **Docker Volume** or **tmpfs**, which bypasses the Overlay2 union mount entirely.
2. **Page Cache Sharing**:
   Because OverlayFS2 shares `lowerdir` layers in the host page cache, if 100 containers run identical `node:22-slim` base images, the host kernel loads Node.js into physical RAM **only once**. All 100 containers share the identical read-only memory pages, slashing RAM consumption compared to Virtual Machines.

---

## 9. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK SCENARIO: Forensic Extraction of Credentials from Disk|
+--------------------------------------------------------------+
| Vulnerable Setup: API token written to /app/token.txt (disk) |
| Container terminates.                                        |
| Forensic attacker with root on host inspects:                |
| `/var/lib/docker/overlay2/<id>/diff/app/token.txt`           |
| OUTCOME: Token compromised!                                  |
+--------------------------------------------------------------+
| Hardened Setup: Token written to /app/token.txt (tmpfs)      |
| Container terminates.                                        |
| Tmpfs unmounted, memory pages zeroed out by kernel.          |
| OUTCOME: Zero disk footprint, attacker finds nothing.        |
+--------------------------------------------------------------+
```

---

## 10. Architectural Comparison Table

| Feature / Trait | Tmpfs Mount | Overlay2 (Container Layer) | Docker Volume |
|:---|:---|:---|:---|
| **Storage Medium** | Host RAM / Swap | Host Non-Volatile Disk | Host Non-Volatile Disk |
| **Persistence** | Ephemeral (Lost on stop) | Ephemeral (Lost on rm) | **Permanent** |
| **Write Performance** | **Memory Speed (GB/s)** | Moderate (CoW overhead) | Native Disk Speed |
| **Host Disk Footprint**| 0 Bytes | Grows with modifications | Grows with data |
| **Ideal For** | Secrets, temp buffers | Application modifications | Databases, persistent logs |

---

## 11. Real-World Scenario Walkthrough

### Scenario: Resolving Massive Write-Amplification in Media Transcoder

#### Incident
A video transcoding microservice processing 4K MP4 files suffered severe disk I/O bottlenecks and exhausted SSD write endurance within 6 months.

#### Investigation
The application downloaded MP4 files to `/tmp/input.mp4`, invoked `ffmpeg` to split frames in `/tmp/frames/`, and uploaded encoded HLS segments to S3. Because `/tmp` was not mounted, all intermediate video chunks passed through Overlay2, causing continuous copy-up actions and generating 14TB of unnecessary writes per day on the host SSDs.

#### Remediation
Configured a 4GB `tmpfs` mount dedicated to transcoding:
```bash
docker run -d \
  --name video-worker \
  --mount type=tmpfs,target=/tmp,tmpfs-size=4096m \
  transcoder:v2
```
- Disk writes dropped by **98%**.
- Transcoding latency dropped from 42 seconds to 11 seconds per video.
- SSD write endurance stabilized.

---

## 12. Self-Assessment & Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between data stored in a container's writable layer versus a `tmpfs` mount?</strong></summary>

**Answer:**
A container's writable layer is stored on the host's physical disk using the storage driver (OverlayFS2). It incurs Copy-on-Write overhead and persists until the container is explicitly removed (`docker rm`). A `tmpfs` mount resides purely in host RAM/swap memory, completely bypasses the storage driver, achieves memory-speed I/O, and is permanently purged the moment the container stops.
</details>

<details>
<summary><strong>2. How does the Linux kernel OverlayFS2 copy-up mechanism work, and what is write amplification?</strong></summary>

**Answer:**
When a process inside a container opens an existing file from an immutable image layer (`lowerdir`) with write permissions, the Linux kernel must duplicate the entire file from `lowerdir` into the container's writable layer (`upperdir`) before modifying it. If the file is 1GB and the application modifies 1 byte, 1GB of data must be copied to disk. This is write-amplification.
</details>

<details>
<summary><strong>3. Why is it dangerous to run a `tmpfs` mount without specifying `tmpfs-size`?</strong></summary>

**Answer:**
By default, an unbounded `tmpfs` mount can consume up to 50% of the host machine's total RAM. If an application encounters a memory leak or an attacker writes massive files into the `tmpfs` mount, it can exhaust all available host memory, triggering the Linux kernel Out-Of-Memory (OOM) killer to terminate vital host processes or crash the node.
</details>

<details>
<summary><strong>4. What is a "whiteout" file in the Overlay2 storage driver?</strong></summary>

**Answer:**
Because image layers (`lowerdir`) are strictly read-only, deleting a file that exists in an image layer cannot physically remove it from disk. Instead, Overlay2 creates a character device file with major/minor number 0/0 (called a whiteout file) in the container's writable layer (`upperdir`). This whiteout file instructs the kernel to mask the underlying file from appearing in the unified `merged` view.
</details>

<details>
<summary><strong>5. How does `--read-only` improve container security, and how does `tmpfs` facilitate its use?</strong></summary>

**Answer:**
`--read-only` mounts the container's entire root filesystem as read-only, preventing attackers from downloading rootkits, modifying binaries, or altering configuration files. However, many applications fail to boot if they cannot write PID files or temp logs to `/tmp` or `/var/run`. Pairing `--read-only` with targeted `tmpfs` mounts at `/tmp` and `/var/run` allows the application to write necessary temporary state in volatile memory without compromising root filesystem immutability.
</details>

<details>
<summary><strong>6. What filesystem feature is mandatory on host Linux partitions for Overlay2 to function properly?</strong></summary>

**Answer:**
`d_type` support (directory entry file type). When formatting host filesystems with XFS, it must be created with `mkfs.xfs -n ftype=1`. If `ftype=0`, `d_type` is not supported, and the Docker daemon will fail to start or produce corrupted layer unions with Overlay2.
</details>

<details>
<summary><strong>7. How can you inspect the physical host directories corresponding to a container's `lowerdir`, `upperdir`, and `merged` layers?</strong></summary>

**Answer:**
Execute `docker inspect <container_id> --format '{{json .GraphDriver.Data}}' | jq .`
This outputs the exact filesystem paths under `/var/lib/docker/overlay2/` for `LowerDir`, `UpperDir`, `MergedDir`, and `WorkDir`.
</details>

<details>
<summary><strong>8. Does Overlay2 support deduplication in memory? Explain.</strong></summary>

**Answer:**
Yes. Because image layers in `lowerdir` are read-only and backed by standard files on the host filesystem, the Linux kernel page cache shares the memory pages across multiple containers running from the same image. If 50 containers run `python:3.12`, the Python binary is loaded into the host's physical RAM only once.
</details>

---

## 13. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Creating a Size-Bounded Tmpfs Mount</strong></summary>

**Goal:** Create a 16MB tmpfs mount, verify memory allocation, and observe write failure when exceeding the boundary.

**Step-by-step Solution:**
1. Run container with 16MB tmpfs:
   ```bash
   docker run -it --rm \
     --mount type=tmpfs,target=/ramdisk,tmpfs-size=16m \
     alpine:3.21 sh
   ```
2. Inside container, check disk space:
   ```sh
   df -h /ramdisk
   # Filesystem shows 16.0M total space
   ```
3. Attempt to write 20MB file:
   ```sh
   dd if=/dev/zero of=/ramdisk/test.bin bs=1M count=20
   # Fails with: dd: writing '/ramdisk/test.bin': No space left on device
   ```
</details>

<details>
<summary><strong>Exercise 2: Hardening an Alpine Container with `--read-only`</strong></summary>

**Goal:** Boot an immutable container that only permits writes into a dedicated `/tmp` tmpfs mount.

**Step-by-step Solution:**
1. Run container:
   ```bash
   docker run -it --rm \
     --read-only \
     --mount type=tmpfs,target=/tmp,tmpfs-size=32m \
     alpine:3.21 sh
   ```
2. Verify root is immutable:
   ```sh
   touch /test.txt
   # touch: /test.txt: Read-only file system
   ```
3. Verify `/tmp` is writable:
   ```sh
   touch /tmp/test.txt && ls -l /tmp/test.txt
   # Succeeds!
   ```
</details>

<details>
<summary><strong>Exercise 3: Observing Overlay2 Whiteout Deletion in Real Time</strong></summary>

**Goal:** Delete an inherited file from an image and locate the resulting whiteout file in `upperdir` on the host.

**Step-by-step Solution:**
1. Run container:
   ```bash
   ID=$(docker run -d alpine:3.21 sh -c "rm /etc/issue && sleep 3600")
   ```
2. Retrieve `UpperDir` path:
   ```bash
   UPPER=$(docker inspect $ID --format '{{.GraphDriver.Data.UpperDir}}')
   ```
3. (Linux with root privileges):
   ```bash
   sudo ls -l $UPPER/etc/
   # Shows character special file 'issue' with device numbers 0, 0:
   # c--------- 1 root root 0, 0 ... issue
   ```
</details>

<details>
<summary><strong>Exercise 4: Benchmark Tmpfs vs Overlay2 Write Latency</strong></summary>

**Goal:** Measure I/O speed differences between volatile RAM and the container copy-on-write layer.

**Step-by-step Solution:**
1. Benchmark Overlay2:
   ```bash
   docker run --rm alpine:3.21 dd if=/dev/zero of=/tmp_disk.img bs=1M count=100 oflag=dsync
   ```
2. Benchmark Tmpfs:
   ```bash
   docker run --rm --mount type=tmpfs,target=/ram alpine:3.21 dd if=/dev/zero of=/ram/tmp_mem.img bs=1M count=100 oflag=dsync
   ```
3. Compare the throughput metrics (MB/s).
</details>

<details>
<summary><strong>Exercise 5: Inspecting Underlying Host Filesystem for `d_type` Compatibility</strong></summary>

**Goal:** Validate that your host filesystem supports `ftype=1`.

**Step-by-step Solution:**
1. Run inspection:
   ```bash
   docker info --format '{{.Driver}}: {{.DriverStatus}}'
   ```
2. On Linux, inspect filesystem directly:
   ```bash
   df -T /var/lib/docker
   ```
</details>

<details>
<summary><strong>Exercise 6: Configuring Strict Mode Permissions on Tmpfs</strong></summary>

**Goal:** Mount a tmpfs folder restricted to user owner permissions (`0700`).

**Step-by-step Solution:**
1. Launch container:
   ```bash
   docker run --rm -it \
     --mount type=tmpfs,target=/secure,tmpfs-size=16m,tmpfs-mode=0700 \
     alpine:3.21 ls -ld /secure
   ```
2. Verify output displays `drwx------` (mode 0700).
</details>

---

## 14. Summary & Reference Cheat Sheet

### CLI Quick Reference
```bash
# Tmpfs Mount Options:
--mount type=tmpfs,target=/path,tmpfs-size=64m,tmpfs-mode=0700

# Inspect Overlay2:
docker inspect <id> --format '{{json .GraphDriver.Data}}' | jq .
```

### Core Rules:
1. **Never use unbound tmpfs**: Always declare `tmpfs-size`.
2. **Never store database logs in Overlay2**: Use Volumes to avoid copy-up penalties.
3. **Hardened containers = `--read-only` + `tmpfs` at `/tmp`**.
4. **Tmpfs is RAM-only**: All data vanishes when the container stops.

---

[← Previous: 01-volumes-and-bind-mounts.md](01-volumes-and-bind-mounts.md) | [Next: 03-backup-and-restore-of-volumes.md →](03-backup-and-restore-of-volumes.md)
