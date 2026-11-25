---
title: "Image Layers and Union Filesystems: Deep Dive into OverlayFS and Storage Drivers"
description: "Exhaustive exploration of Docker storage architecture: Union Mounts, OverlayFS mechanics (lowerdir, upperdir, workdir, merged), Copy-on-Write (CoW), whiteout files, and storage drivers."
category: "DevOps / Containers"
tags: ["docker", "overlayfs", "storage", "unionfs", "layers", "cow", "whiteout"]
difficulty: "Advanced"
prerequisites: ["02-images-and-containers.md", "Basic Linux filesystem inodes and directory tree concepts"]
estimated_read_time: "25 min"
---

# Image Layers and Union Filesystems: Deep Dive into OverlayFS and Storage Drivers

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. Core Concepts & Terminology](#2-core-concepts--terminology)
- [3. How It Works Internally: OverlayFS Architecture](#3-how-it-works-internally-overlayfs-architecture)
- [4. Anatomy of Filesystem Operations (Read, Write, Delete, Truncate)](#4-anatomy-of-filesystem-operations-read-write-delete-truncate)
- [5. Evolution of Docker Storage Drivers](#5-evolution-of-docker-storage-drivers)
- [6. Practical Diagnostic Demonstrations](#6-practical-diagnostic-demonstrations)
- [7. Best Practices & Production Guidelines](#7-best-practices--production-guidelines)
- [8. Common Pitfalls & Anti-Patterns](#8-common-pitfalls--anti-patterns)
- [9. Comparison Table: Docker Storage Drivers](#9-comparison-table-docker-storage-drivers)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

When you download a 500MB Docker image, launch ten simultaneous containers from it, create temporary files inside each container, and delete three of them, your host hard drive does **not** consume 5GB of storage. It consumes barely 502MB.

This efficiency is powered by **Union Mounts** and **Union Filesystems (UnionFS)**. A Union Filesystem is a filesystem service that takes multiple physical directory trees located on underlying disk partitions and combines them into a single, cohesive, unified virtual directory hierarchy.

In modern Docker, the universal standard union filesystem is **OverlayFS** (implemented via the `overlay2` storage driver in the Linux kernel). OverlayFS enables:
1. **Dramatically Fast Container Creation**: Starting a container requires zero file copying—only mounting an OverlayFS directory structure in memory.
2. **Extreme Storage Density**: Thousands of containers can safely share identical underlying immutable OS layers.
3. **Layer Caching**: Building images reuses previously compiled layers instantly without recalculating unchanged filesystem trees.

```
+---------------------------------------------------------------------------------+
|                         OVERLAYFS DIRECTORY HIERARCHY                           |
|                                                                                 |
|  [Container Virtual View: / (Merged Directory)]                                 |
|  Presents unified filesystem to running container processes                     |
|  +---------------------------------------------------------------------------+  |
|  | /bin /etc /lib /opt/app /tmp                                              |  |
|  +---------------------------------------------------------------------------+  |
|        ^                                             ^                          |
|        | (Reads/Writes)                              | (Read-Only Fallback)     |
|        |                                             |                          |
|  +-----+-------------------+                 +-------+-----------------------+  |
|  | Upper Directory (upper) |                 | Lower Directory (lower)       |  |
|  | Read-Write Scratch Dir  |                 | Read-Only Image Layers        |  |
|  | (Ephemeral Container)   |                 | (Cached on Host Disk)         |  |
|  |                         |                 |                               |  |
|  | - Modified files        |                 | Layer 3: Application Code     |  |
|  | - Newly created files   |                 | Layer 2: Node.js Runtime      |  |
|  | - Whiteout markers (.wh)|                 | Layer 1: Alpine Base OS       |  |
|  +-------------------------+                 +-------------------------------+  |
|               ^                                                                 |
|               | (Staging area for atomic operations)                            |
|  +------------+------------+                                                    |
|  | Work Directory (work)   |                                                    |
|  +-------------------------+                                                    |
+---------------------------------------------------------------------------------+
```

---

## 2. Core Concepts & Terminology

- **LowerDir (`lowerdir`)**: The colon-separated list of read-only directory paths representing the image layers stacked from top to bottom.
- **UpperDir (`upperdir`)**: The single read-write directory allocated exclusively to an active container instance. All newly created or modified files reside here.
- **WorkDir (`workdir`)**: An internal scratchpad directory used by the Linux kernel to stage atomic file operations (such as copy-ups and whiteouts) before committing them to `upperdir`.
- **MergedDir (`merged`)**: The unified mount point directory exposed directly to the container as its root (`/`) filesystem.
- **Copy-on-Write (CoW)**: An optimization strategy where an existing read-only file is only duplicated into the writable `upperdir` at the exact moment a process attempts to modify it.
- **Whiteout File (`.wh.<name>`)**: A special character device created in `upperdir` to mask and hide a file that exists in an underlying `lowerdir` layer when a container deletes it.
- **Opaque Directory**: A directory in `upperdir` configured with the extended attribute `trusted.overlay.opaque=y`, hiding all files in underlying matching directories.

---

## 3. How It Works Internally: OverlayFS Architecture

When Docker executes `docker run`, the engine sets up an OverlayFS mount directly using the Linux `mount` syscall:

```bash
mount -t overlay overlay \
  -o lowerdir=/layer3:/layer2:/layer1,upperdir=/container/upper,workdir=/container/work \
  /container/merged
```

### Layer Path Traversal Rules:
1. **Layer Precedence**: If a file exists in multiple layers (e.g., `/etc/hosts` in both `layer1` and `upperdir`), the version in the uppermost layer takes precedence and is returned to the process.
2. **Directory Merging**: If a directory exists across multiple layers (e.g., `/usr/bin`), OverlayFS combines the file listings from all layers into a single virtual directory view.

---

## 4. Anatomy of Filesystem Operations (Read, Write, Delete, Truncate)

### 4.1. Reading Files (`open(..., O_RDONLY)`)
- **Case 1 (File in UpperDir)**: The kernel opens the file directly from `upperdir`.
- **Case 2 (File only in LowerDir)**: The kernel walks the `lowerdir` path chain from top to bottom. As soon as it locates the first matching filename, it opens the file directly from the immutable image layer. Zero copying occurs.

### 4.2. Writing to Existing Files (`open(..., O_WRONLY | O_RDWR)`)
1. The kernel checks if the file exists in `upperdir`. If yes, writes occur directly.
2. If the file exists only in `lowerdir`:
   - The kernel allocates a temporary file in `workdir`.
   - It performs a **Copy-up**: copying the entire file and its metadata (permissions, ownership, timestamps) from `lowerdir` into `workdir`.
   - It atomically renames the file into `upperdir`.
   - The write operation is executed against the new copy in `upperdir`.
   - The original file in `lowerdir` remains completely untouched.

```
[Write to /app/config.json]
         |
         +--> Exists in upperdir? ---> YES: Write directly to upperdir copy
         |
         \--> NO: Copy-up sequence:
              1. Copy from lowerdir -> workdir
              2. Atomic rename workdir -> upperdir
              3. Apply write to upperdir copy!
```

### 4.3. Deleting Files (`unlink()`)
A container cannot modify or delete files from read-only image layers in `lowerdir`. When an application executes `rm /etc/issue`:
1. OverlayFS creates a **Whiteout File** in `upperdir`.
2. The whiteout file is named `.wh.issue` and is created as a character device with major/minor device numbers `0/0`.
3. When any process inside the container calls `readdir()` or `stat()` on `/etc/issue`, OverlayFS sees the `.wh.issue` marker and returns `ENOENT` (No such file or directory).

---

## 5. Evolution of Docker Storage Drivers

Over Docker's history, several storage drivers were developed to manage copy-on-write filesystems on Linux:

```
+---------------------------------------------------------------------------------+
|                        DOCKER STORAGE DRIVER EVOLUTION                          |
|                                                                                 |
|  2013-2015: AUFS (Another UnionFS)                                              |
|  - Out-of-tree kernel patch; unstable; required custom Ubuntu kernels           |
|        |                                                                        |
|        v                                                                        |
|  2014-2017: DeviceMapper (direct-lvm)                                           |
|  - Block-level CoW; used in RHEL/CentOS; complex thin-pool provisioning         |
|        |                                                                        |
|        v                                                                        |
|  2016-Present: OverlayFS (overlay2)                                             |
|  - Native in-tree Linux kernel module; fast inode sharing; industry standard    |
|        |                                                                        |
|        +---> Modern Alternative: Btrfs / ZFS (Advanced snapshotting / quotas)   |
+---------------------------------------------------------------------------------+
```

### Why `overlay2` Won the Storage Wars:
- **In-Tree Kernel Module**: Merged into mainline Linux kernel in 3.18, eliminating custom out-of-tree kernel patches.
- **Page Cache Sharing**: If 100 containers read `/lib/libc.so` from an image, the Linux kernel loads it into memory page cache **once**. All 100 containers share the same physical RAM pages.
- **Inode Efficiency**: Unlike DeviceMapper (which operates on raw virtual block devices), `overlay2` operates at the file/directory level, utilizing host inodes efficiently.

---

## 6. Practical Diagnostic Demonstrations

### Example 1: Inspecting the Physical OverlayFS Mount on the Host
Examine the active mount options configured by Docker for a running container:

```bash
# 1. Start a container
docker run -d --name overlay-demo alpine sleep 3600

# 2. Extract container ID
CID=$(docker inspect --format '{{.Id}}' overlay-demo)

# 3. Inspect host kernel mount table (Linux host)
mount | grep $CID
```
*Output (Linux Host):*
```text
overlay on /var/lib/docker/overlay2/.../merged type overlay (rw,relatime,lowerdir=...,upperdir=...,workdir=...)
```

### Example 2: Inspecting the Physical `UpperDir` Directory
Prove where modified container data physically lives on the host filesystem:

```bash
# Extract UpperDir path
UPPER=$(docker inspect --format '{{.GraphDriver.Data.UpperDir}}' overlay-demo)
echo "Physical Host Path: $UPPER"

# Write a secret file inside the container
docker exec overlay-demo sh -c "echo 'SuperSecretKey' > /root/secret.env"

# Read the file directly from the HOST hard drive (requires sudo)
sudo cat $UPPER/root/secret.env
# Output: SuperSecretKey

# Cleanup
docker rm -f overlay-demo
```

### Example 3: Simulating and Verifying a Whiteout File
Observe the creation of a `.wh` character device upon file deletion:

```bash
# 1. Launch a container
docker run -d --name whiteout-demo alpine sleep 60

# 2. Extract UpperDir path
UPPER=$(docker inspect --format '{{.GraphDriver.Data.UpperDir}}' whiteout-demo)

# 3. Delete /etc/alpine-release from inside the container
docker exec whiteout-demo rm /etc/alpine-release

# 4. Inspect the UpperDir from the host operating system
sudo ls -la $UPPER/etc/
```
*Output shows:*
```text
c--------- 1 root root 0, 0 Nov 25 08:48 .wh.alpine-release
```
Notice the `c` prefix indicating a character device with major/minor numbers `0, 0`!

```bash
# Cleanup
docker rm -f whiteout-demo
```

### Example 4: Demonstrating the Copy-on-Write Penalty
Demonstrate the latency of modifying a large file stored in a lower layer versus modifying a volume:

```bash
# Create a container with a 200MB file embedded in an image layer
docker run -d --name cow-bench alpine sh -c "dd if=/dev/zero of=/data.bin bs=1M count=200; sleep 60"
docker commit cow-bench cow-image:latest
docker rm -f cow-bench

# Run container from cow-image and time the first byte modification
docker run --rm cow-image:latest time sh -c "dd if=/dev/zero of=/data.bin bs=1 count=1 conv=notrunc"
```
*Notice*: Even though only 1 byte was modified, the kernel was forced to copy the entire 200MB file up into `upperdir`, introducing measurable I/O latency!

```bash
# Cleanup
docker rmi cow-image:latest
```

### Example 5: Inspecting Host Disk Consumption per Container
Query the size of the container's writable layer versus the virtual image size:

```bash
docker ps -s --filter "name=overlay-demo"
```
*Output columns show*:
- `SIZE`: The bytes written to the container's `UpperDir`.
- `virtual`: The cumulative size of the read-only image layers plus the `UpperDir`.

---

## 7. Best Practices & Production Guidelines

1. **Bypass OverlayFS for High-Throughput I/O**: Never store high-frequency databases (PostgreSQL, MySQL, MongoDB, Redis RDB) inside the container writable layer. OverlayFS copy-up latencies degrade random write performance. Always mount **Docker Volumes**.
2. **Clean Up Build Dependencies in the Same Layer**: If you run `apk add gcc` and compile code, run `apk del gcc` in the **exact same `RUN` directive**. If deleted in a subsequent directive, the compiler remains permanently stored in the lower layer archive.
3. **Monitor Inode Utilization**: Docker hosts can run out of disk space via inode exhaustion even when gigabytes of disk capacity remain. Monitor host inodes using `df -i`.
4. **Prune Orphaned Layers Periodically**: Set up automated maintenance jobs running `docker system prune -af` to clean up dangling layers and detached snapshots.
5. **Use Dedicated Disks for `/var/lib/docker`**: Mount a dedicated high-speed NVMe partition to `/var/lib/docker` formatted with `ext4` or `xfs` (with `ftype=1`) to prevent container write spikes from filling the host OS root partition.

---

## 8. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Proper Fix |
|:---|:---|:---|:---|
| **Logging high-frequency events to `/var/log`** | Application writing logs to local files inside container rootfs. | Container's `UpperDir` fills the host root partition; copy-on-write slows down CPU. | Stream logs to `stdout`/`stderr` or mount a dedicated logging volume. |
| **Random in-place writes on large files** | Modifying large static files embedded in image layers. | Triggers full-file copy-up into `UpperDir`, exhausting disk bandwidth and RAM page cache. | Mount files from volumes or object storage (S3). |
| **Formatting underlying filesystem without `ftype=1`** | Installing Docker on legacy XFS filesystems formatted with `ftype=0`. | Overlay2 fails to mount because XFS cannot store d_type directory entry metadata. | Format XFS with `mkfs.xfs -n ftype=1 /dev/sdX`. |
| **Deleting a file in a later layer expecting size reduction** | Adding `RUN rm -rf /large-dir` in line 5 after `ADD` in line 2. | File is hidden via whiteout marker, but still takes up full disk space in the image layer. | Delete in the same `RUN` command or use multi-stage builds. |
| **Running out of disk space due to stopped containers** | Leaving hundreds of dead containers on host (`docker ps -a`). | Each stopped container holds its `UpperDir` and `workdir` on disk. | Use `docker run --rm` or run `docker container prune`. |
| **Exhausting host inodes** | Applications generating millions of tiny cache files in `UpperDir`. | Host reports `No space left on device` even with 50% free disk capacity. | Mount an external volume with high inode capacity or clear caches in memory (`tmpfs`). |

---

## 9. Comparison Table: Docker Storage Drivers

| Storage Driver | Architecture | Inode Efficiency | Page Cache Sharing | Modern Status |
|:---|:---|:---|:---|:---|
| **`overlay2`** | File-level UnionFS (In-Tree) | Excellent | Yes (Shared RAM pages) | **Standard Industry Default** |
| **`btrfs`** | Subvolume / Filesystem CoW | Good | No | Supported (Specialized setups) |
| **`zfs`** | Dataset CoW / ARC Cache | Good | No | Supported (Enterprise ZFS storage)|
| **`devicemapper`**| Block-level thin provisioning | Poor | No | **Deprecated & Removed** |
| **`aufs`** | File-level UnionFS (Out-of-tree)| Good | Yes | **Deprecated & Removed** |
| **`vfs`** | Naive directory copy (No CoW) | Terrible | No | Testing / Fallback only |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. Explain the four directories that comprise an OverlayFS mount in Docker.</strong></summary>

An OverlayFS mount in Docker consists of:
1. **`lowerdir`**: A list of read-only directories containing the immutable image layers stacked in order.
2. **`upperdir`**: The single read-write directory allocated for the active container instance where all new files and modifications are stored.
3. **`workdir`**: An internal scratchpad directory used by the Linux kernel to stage atomic file operations (such as copy-ups) before committing them to `upperdir`.
4. **`merged`**: The unified virtual mount directory presented directly to the container process as its root filesystem (`/`).
</details>

<details>
<summary><strong>2. What is a "Whiteout File" and how does OverlayFS represent deleted files?</strong></summary>

Because image layers in `lowerdir` are read-only, OverlayFS cannot physically delete a file from lower layers when a process inside a container calls `unlink()`. Instead, OverlayFS creates a **Whiteout File** in `upperdir`. A whiteout file is a character device with major and minor device numbers set to `0/0` and named `.wh.<filename>`. When the kernel merges the directory views, any file whose name matches the whiteout marker is hidden from the container.
</details>

<details>
<summary><strong>3. How does the Linux kernel optimize RAM usage when multiple containers share the same base image on `overlay2`?</strong></summary>

Because `overlay2` operates at the file level on standard Linux filesystems (ext4/xfs), files in `lowerdir` share identical host kernel inodes. When Container A reads `/usr/lib/libssl.so`, the Linux kernel loads those memory pages into the host's unified **Page Cache**. When Container B reads the exact same file from the same image layer, the kernel recognizes the shared inode and maps the existing memory pages directly into Container B's address space. 100 containers share the exact same physical RAM pages for shared libraries.
</details>

<details>
<summary><strong>4. What is the Copy-on-Write (CoW) penalty, and how do you mitigate it for databases?</strong></summary>

The CoW penalty is the I/O latency incurred the first time a process modifies an existing file residing in a read-only lower layer. The kernel must allocate blocks in `workdir`, copy the entire file from `lowerdir`, atomically move it to `upperdir`, and then perform the modification. For large files (e.g., a 20GB database file), this causes massive disk I/O pauses. You mitigate this by mounting **Docker Volumes**, which bypass OverlayFS completely and write directly to host filesystem directories.
</details>

<details>
<summary><strong>5. Why is XFS `ftype=1` required for Docker's `overlay2` storage driver?</strong></summary>

OverlayFS relies on directory entry file types (`d_type`) to detect whether an entry is a whiteout character device without having to perform an expensive `stat()` system call on every single file in every layer. Older XFS filesystems were formatted with `ftype=0` (which disabled `d_type` recording). If Docker runs on `ftype=0`, OverlayFS fails to detect whiteout devices, causing deleted files to reappear inside containers.
</details>

<details>
<summary><strong>6. What is an "Opaque Directory" in OverlayFS?</strong></summary>

When a container deletes an entire directory tree that exists in a lower layer and creates a new empty directory with the same name, OverlayFS cannot practically create individual whiteout files for every file that ever existed. Instead, it creates the new directory in `upperdir` and tags it with the extended attribute `trusted.overlay.opaque=y`. When the kernel merges the view, this opaque flag instructs OverlayFS to completely ignore all underlying directories with that name in `lowerdir`.
</details>

<details>
<summary><strong>7. How does <code>docker diff</code> determine filesystem changes?</strong></summary>

`docker diff <container>` simply inspects the contents of the container's physical `UpperDir` directory on the host. Any file present in `UpperDir` that is not a whiteout is marked as **Added (A)** or **Changed (C)**. Any whiteout character device (`.wh.<name>`) present in `UpperDir` is marked as **Deleted (D)**.
</details>

<details>
<summary><strong>8. What is the difference between image layer DiffIDs and layer ChainIDs?</strong></summary>

- A **DiffID** is the SHA256 checksum of an individual layer's uncompressed tar archive changes in isolation.
- A **ChainID** is a composite SHA256 hash representing a layer stacked on top of its parent layers: $\text{ChainID}(L_n) = \text{SHA256}(\text{ChainID}(L_{n-1}) + " " + \text{DiffID}(L_n))$. ChainIDs ensure that layers are uniquely identified within the exact DAG context in which they are stacked.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Storage Driver Configuration via `docker info`</strong></summary>

**Goal**: Verify that your engine is utilizing the `overlay2` storage driver and inspect the backing filesystem.

```bash
docker info --format 'Storage Driver: {{.Driver}} | Backing Filesystem: {{.DriverStatus}}'
```
</details>

<details>
<summary><strong>Exercise 2: Trace Inode Sharing Across Containers</strong></summary>

**Goal**: Prove that two separate containers reading the same file share the exact same host inode.

```bash
# Launch two containers from alpine
docker run -d --name c-one alpine sleep 60
docker run -d --name c-two alpine sleep 60

# Inspect the inode of /bin/busybox inside container 1
docker exec c-one ls -i /bin/busybox

# Inspect the inode of /bin/busybox inside container 2
docker exec c-two ls -i /bin/busybox
# Notice that the inode numbers are 100% IDENTICAL!

# Cleanup
docker rm -f c-one c-two
```
</details>

<details>
<summary><strong>Exercise 3: Inspect UpperDir, LowerDir, and WorkDir Paths</strong></summary>

**Goal**: Extract the physical layer paths using Go templates.

```bash
# Start container
docker run -d --name paths-demo alpine sleep 60

# Query graph driver metadata
docker inspect paths-demo --format '
MergedDir: {{.GraphDriver.Data.MergedDir}}
UpperDir:  {{.GraphDriver.Data.UpperDir}}
WorkDir:   {{.GraphDriver.Data.WorkDir}}
LowerDir:  {{.GraphDriver.Data.LowerDir}}
'

# Cleanup
docker rm -f paths-demo
```
</details>

<details>
<summary><strong>Exercise 4: Verify Filesystem Diff States (A, C, D)</strong></summary>

**Goal**: Trigger Add, Change, and Delete operations and verify with `docker diff`.

```bash
# Start container
docker run -d --name diff-lab alpine sleep 60

# Add a file
docker exec diff-lab touch /new_file.txt

# Change a file
docker exec diff-lab sh -c "echo 'mod' >> /etc/hosts"

# Delete a file
docker exec diff-lab rm /etc/issue

# Check diff output
docker diff diff-lab

# Cleanup
docker rm -f diff-lab
```
</details>

<details>
<summary><strong>Exercise 5: Benchmark Copy-on-Write vs Direct Volume Write</strong></summary>

**Goal**: Measure the throughput difference between writing to a container layer versus writing to a mounted volume.

```bash
# Benchmark container layer write (OverlayFS CoW)
docker run --rm alpine dd if=/dev/zero of=/root/test.img bs=1M count=100 conv=fdatasync

# Benchmark volume write (Bypasses OverlayFS)
docker run --rm -v /tmp:/data alpine dd if=/dev/zero of=/data/test.img bs=1M count=100 conv=fdatasync && rm -f /tmp/test.img
```
</details>

<details>
<summary><strong>Exercise 6: Query Storage Consumption with `docker system df -v`</strong></summary>

**Goal**: Inspect layer deduplication across images and containers.

```bash
docker system df -v
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                         OVERLAYFS & STORAGE DRIVERS CHEAT SHEET                       |
+---------------------------------------------------------------------------------------+
| OverlayFS Components:  LowerDir (Read-Only Image Layers)                              |
|                        UpperDir (Read-Write Container Layer)                          |
|                        WorkDir (Kernel scratchpad for atomic staging)                 |
|                        MergedDir (Unified virtual rootfs mounted into container)      |
| Whiteout File:         .wh.<name> character device (0/0) masking deleted lower files  |
| Inspect Container Diff:docker diff <container> (A=Added, C=Changed, D=Deleted)        |
| Inspect Storage Paths: docker inspect --format '{{json .GraphDriver.Data}}' <id>      |
| Production Rule:       Always use named volumes for databases; OverlayFS CoW stalls  |
|                        random write I/O on large files.                               |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Linux Kernel Documentation: Overlay Filesystem Architecture](https://docs.kernel.org/filesystems/overlayfs.html)
- [Docker Documentation: Use the OverlayFS Storage Driver](https://docs.docker.com/storage/storagedriver/overlayfs-driver/)
- [Docker Documentation: About Storage Drivers](https://docs.docker.com/storage/storagedriver/)
- [Brendan Gregg: Linux Performance and Container Storage Overhead](https://www.brendangregg.com/)
- [Red Hat Enable Sysadmin: Understanding OverlayFS in Linux Containers](https://www.redhat.com/sysadmin/overlayfs-linux-containers)

---

| [< Previous: 05-how-isolation-works.md](./05-how-isolation-works.md) | [Next: 07-registries-and-image-naming.md](./07-registries-and-image-naming.md) > |
| :--- | :--- |
