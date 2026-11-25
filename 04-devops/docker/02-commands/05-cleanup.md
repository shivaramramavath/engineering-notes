---
title: "Docker Disk Hygiene and Resource Reclamation: prune, df, and Maintenance"
description: "Mastering Docker storage reclamation: analyzing disk consumption with system df, selective pruning of containers, images, volumes, and BuildKit caches, and virtual disk compaction."
category: "DevOps / Containers"
tags: ["docker", "cleanup", "prune", "disk-usage", "maintenance", "system-df", "devops"]
difficulty: "Beginner"
prerequisites: ["02-commands/01-run-ps-stop.md", "02-commands/03-images.md", "02-commands/04-build.md"]
estimated_read_time: "25 min"
---

# Docker Disk Hygiene and Resource Reclamation: prune, df, and Maintenance

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. Analyzing Disk Consumption with `docker system df`](#2-analyzing-disk-consumption-with-docker-system-df)
- [3. Deep Dive: Selective Pruning vs Nuclear Pruning](#3-deep-dive-selective-pruning-vs-nuclear-pruning)
- [4. The Danger of Pruning Volumes (`--volumes`)](#4-the-danger-of-pruning-volumes---volumes)
- [5. Time-Windowed Automated Garbage Collection](#5-time-windowed-automated-garbage-collection)
- [6. Virtual Disk Compaction on macOS and Windows](#6-virtual-disk-compaction-on-macos-and-windows)
- [7. Practical Diagnostic Demonstrations](#7-practical-diagnostic-demonstrations)
- [8. Best Practices & Production Guidelines](#8-best-practices--production-guidelines)
- [9. Common Pitfalls & Anti-Patterns](#9-common-pitfalls--anti-patterns)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

One of the most common operational emergencies encountered by platform engineers is the dreaded:

```text
ERROR: failed to register layer: write /var/lib/docker/overlay2/...: no space left on device
```

By design, Docker is an **append-mostly, cache-heavy system**:
- Every time you rebuild an image without a new tag, the old layers become untagged **dangling images**.
- Every time a container stops, its read-write **`UpperDir`** layer and JSON log files remain on disk indefinitely unless created with `--rm`.
- BuildKit retains gigabytes of compiler, package manager, and intermediate layer caches to accelerate subsequent builds.
- Anonymous volumes created by database or testing containers persist on disk forever even after their parent containers are deleted.

Left unmaintained, development workstations, CI/CD runners, and production cluster nodes will inevitably exhaust their disk space. Mastering Docker's cleanup commands—and knowing which commands safely reclaim space versus which accidentally destroy production databases—is a mandatory operational discipline.

```
+---------------------------------------------------------------------------------+
|                         DOCKER DISK RECLAMATION MATRIX                          |
|                                                                                 |
|  [Total Docker Disk Usage (/var/lib/docker)]                                    |
|        |                                                                        |
|        +---> [Active Containers & Writable Layers]                              |
|        |     - Reclaimed via: docker rm (when stopped)                          |
|        |                                                                        |
|        +---> [Dangling / Unused Images]                                         |
|        |     - Reclaimed via: docker image prune / docker image prune -a        |
|        |                                                                        |
|        +---> [Unused Volumes]                                                   |
|        |     - Reclaimed via: docker volume prune (CAUTION: State Loss!)        |
|        |                                                                        |
|        +---> [BuildKit Cache]                                                   |
|        |     - Reclaimed via: docker builder prune                              |
|        |                                                                        |
|        v                                                                        |
|  [Nuclear Cleanup: docker system prune -af --volumes]                           |
|  - Resets Docker daemon to near-pristine clean state                            |
+---------------------------------------------------------------------------------+
```

---

## 2. Analyzing Disk Consumption with `docker system df`

Before deleting anything, you must understand exactly which subsystem is consuming your disk space.

```bash
docker system df
```
*Output (Standard Table):*
```text
TYPE            TOTAL     ACTIVE    SIZE      RECLAIMABLE
Images          14        5         3.42GB    2.15GB (62%)
Containers      8         2         420MB     310MB (73%)
Local Volumes   6         1         12.8GB    11.2GB (87%)
Build Cache     42        0         4.10GB    4.10GB (100%)
```

### Deep Inspection with `-v` (Verbose):
To identify the specific culprit containers, large images, or abandoned volumes, supply the `-v` flag:
```bash
docker system df -v
```
This prints:
1. Every individual image with its tag, unique size, and shared layer size.
2. Every container with its name, status, and physical writable layer size.
3. Every local volume with its name, links count, and size.
4. Every BuildKit cache record.

---

## 3. Deep Dive: Selective Pruning vs Nuclear Pruning

Docker categorizes cleanup commands into **scoped object prunes** and **system-wide prunes**:

### 3.1. Scoped Object Pruning

#### Container Pruning (`docker container prune`)
Removes all stopped containers (state `exited` or `created`). Does not touch running containers.
```bash
docker container prune -f
```

#### Image Pruning (`docker image prune`)
- Default (`docker image prune`): Removes only **dangling images** (untagged images displaying `<none>:<none>`). Safe and non-destructive.
- All (`docker image prune -a`): Removes all images that are not currently referenced by at least one container (running or stopped). **Warning**: Forces re-downloading base images on subsequent runs.

#### Volume Pruning (`docker volume prune`)
Removes all local volumes that are not currently used by at least one existing container. **CRITICAL WARNING**: If you stopped a database container and deleted it, its volume is now considered "unused" and `volume prune` will permanently destroy your database!

#### Network Pruning (`docker network prune`)
Removes all user-defined bridge and overlay networks that are not currently connected to active containers.

#### Builder Pruning (`docker builder prune`)
Purges BuildKit's internal cache records:
```bash
# Purge build cache older than 24 hours
docker builder prune --filter "until=24h" -f
```

---

## 4. The Danger of Pruning Volumes (`--volumes`)

The most dangerous command in the Docker CLI is:
```bash
docker system prune -a --volumes
```

```
+---------------------------------------------------------------------------------+
|                       THE ACCIDENTAL DATA LOSS DISASTER                         |
|                                                                                 |
|  1. Engineer runs: docker compose down (Stops and removes database container)   |
|  2. The named volume 'pgdata' still exists on host disk containing customer DB. |
|  3. Engineer runs: docker system prune --volumes -f                             |
|  4. Docker scans volumes: 'pgdata' is not currently attached to a container!    |
|  5. DOCKER PERMANENTLY ERASES 'pgdata' FROM DISK!                               |
|  6. CUSTOMER DATABASE DESTROYED.                                                |
+---------------------------------------------------------------------------------+
```

> **Mandatory Rule for Production**: Never pass `--volumes` to `docker system prune` on any machine hosting persistent state, databases, or stateful volumes.

---

## 5. Time-Windowed Automated Garbage Collection

Running aggressive prunes indiscriminately flushes valuable cache layers, slowing down local development and CI/CD pipelines.

Instead, use **Time-Windowed Filters (`until`)**:

```bash
# Clean stopped containers older than 48 hours
docker container prune --filter "until=48h" -f

# Clean dangling images created more than 7 days ago
docker image prune --filter "until=168h" -f

# Clean BuildKit cache older than 72 hours
docker builder prune --filter "until=72h" -f
```

### Production Maintenance Cron Job:
Add a safe, automated daily maintenance job in `/etc/cron.daily/docker-gc`:
```bash
#!/bin/bash
# Reclaim containers and caches older than 3 days without touching volumes
docker container prune --filter "until=72h" -f > /dev/null
docker image prune --filter "until=72h" -f > /dev/null
docker builder prune --filter "until=72h" -f > /dev/null
```

---

## 6. Virtual Disk Compaction on macOS and Windows

On macOS and Windows, Docker Desktop runs inside a lightweight Linux virtual machine backed by a virtual disk file:
- **macOS**: `~/Library/Containers/com.docker.docker/Data/vms/0/data/Docker.raw`
- **Windows**: `C:\Users\<user>\AppData\Local\Docker\wsl\data\ext4.vhdx`

### The Sparse Virtual Disk Problem:
When Docker inside the VM writes 20GB of data, the host virtual disk file expands by 20GB. However, when you run `docker system prune` inside the VM, **the virtual disk file on your Mac or Windows host does not automatically shrink!** The host OS still sees a 60GB file even if Docker inside reports only 5GB used.

### Reclaiming Host Disk Space:

#### On macOS:
In modern Docker Desktop for Mac, enable **"VirtioFS"** and **"Automatic virtual disk compaction"** in **Settings > Resources > Virtual disk limit**.
To compact manually:
```bash
# In Docker Desktop GUI:
# Go to Settings > Resources > Virtual disk limit > Click "Clean / Purge data"
```

#### On Windows (WSL 2):
Compact the WSL 2 `ext4.vhdx` file using PowerShell and `diskpart`:
```powershell
# 1. Shutdown WSL completely
wsl --shutdown

# 2. Launch diskpart as Administrator
diskpart

# Inside diskpart:
select vdisk file="C:\Users\<User>\AppData\Local\Docker\wsl\data\ext4.vhdx"
attach vdisk readonly
compact vdisk
detach vdisk
exit
```

---

## 7. Practical Diagnostic Demonstrations

### Example 1: Evaluating System Disk Usage
Inspect system storage breakdowns before and after creating workloads:

```bash
docker system df
```

### Example 2: Pruning Dangling Images vs All Unused Images
Demonstrate the difference between standard image prune and `--all`:

```bash
# Build dummy image
echo "FROM alpine\nRUN echo 'test' > /f" | docker build -t prune-lab:v1 -

# Build again with same tag to make the first build dangling
echo "FROM alpine\nRUN echo 'test2' > /f" | docker build -t prune-lab:v1 -

# Prune ONLY dangling images
docker image prune -f

# Notice prune-lab:v1 remains safe because it has an active tag!
docker images | grep prune-lab

# Cleanup
docker rmi prune-lab:v1
```

### Example 3: Selective Volume Cleanup by Label
Tag temporary volumes and prune only labeled volumes while preserving databases:

```bash
# Create temporary labeled volume
docker volume create --label environment=test temp-test-vol

# Create persistent database volume
docker volume create prod-database-vol

# Prune only volumes matching test label
docker volume prune --filter "label=environment=test" -f

# Confirm prod-database-vol remains safe and untouched!
docker volume ls | grep prod-database-vol

# Cleanup
docker volume rm prod-database-vol
```

### Example 4: Pruning BuildKit Cache by Target Size
Instruct BuildKit to reduce its on-disk cache to a maximum threshold (e.g., 2GB):

```bash
docker builder prune --keep-storage 2GB -f
```

### Example 5: Emergency Recovery from Disk Full
If a Docker host reaches 100% disk capacity, `docker run` fails because the daemon cannot even write temporary lock files. Execute this ordered recovery sequence:

```bash
# 1. Purge all stopped containers immediately
docker container prune -f

# 2. Purge BuildKit build cache
docker builder prune -af

# 3. Purge all unreferenced images
docker image prune -af

# 4. Check disk recovery
df -h /var/lib/docker
```

---

## 8. Best Practices & Production Guidelines

1. **Never Run `--volumes` in Production**: Prevent accidental erasure of customer database volumes. Prune volumes individually by name (`docker volume rm <name>`).
2. **Always Use `--rm` for Transient Jobs**: Run CI/CD tests, data migrations, and debugging containers with `--rm` so stopped containers never accumulate.
3. **Automate Scheduled Builder Pruning**: BuildKit caches can grow to 50GB+ on active developer machines. Configure a weekly cron job or set `"keep-storage"` caps in BuildKit settings.
4. **Monitor Host Inodes (`df -i`)**: Union filesystems generate millions of tiny layer metadata files. A host can run out of disk space via inode saturation while reporting 50% free disk bytes!
5. **Use Dedicated Partitions for `/var/lib/docker`**: Keep Docker's data directory on an isolated disk partition or LVM volume so a sudden container log or layer spike does not freeze the host OS root filesystem.

---

## 9. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Proper Solution |
|:---|:---|:---|:---|
| **Accidental database deletion via `prune --volumes`** | Running `docker system prune -a --volumes`. | Unattached persistent database volumes are permanently deleted. | Never use `--volumes`; remove volumes explicitly by name. |
| **Running `prune -a` before offline travel** | Pruning all unused images on a laptop. | Removes all base images; developer cannot build or run containers offline on airplanes/trains. | Run standard `docker image prune` (dangling only). |
| **Host disk remains full after pruning on Mac/Win** | Virtual disk (`Docker.raw` / `ext4.vhdx`) does not automatically shrink. | Host OS still reports 0 bytes free. | Compact the virtual disk file via Docker Desktop GUI or `diskpart`. |
| **Host out-of-inodes crash (`df -i` 100%)** | Millions of intermediate layer files. | Kernel refuses to create new files despite available gigabytes. | Purge old images and build caches: `docker system prune -af`. |
| **Pruning running containers** | Expecting `prune` to stop and remove running services. | `prune` only affects stopped containers; running services continue unaffected. | Stop containers first (`docker stop`), then prune. |
| **Ignoring BuildKit cache accumulation** | Developers only running `docker image prune`. | BuildKit cache in `/var/lib/docker/buildkit` continues to consume 30GB+. | Run `docker builder prune -af`. |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. What is the difference between <code>docker image prune</code> and <code>docker image prune -a</code>?</strong></summary>

- `docker image prune` (without `-a`) removes only **dangling images** (untagged layers displayed as `<none>:<none>` resulting from image rebuilds). It does not touch tagged images.
- `docker image prune -a` removes all **unused images**—both dangling images AND fully tagged images (such as `python:3.12-slim` or `nginx:alpine`) that are not currently associated with at least one existing running or stopped container.
</details>

<details>
<summary><strong>2. Why doesn't running <code>docker system prune</code> on macOS immediately free up disk space on the Mac host?</strong></summary>

On macOS, Docker runs inside a Linux virtual machine backed by a virtual disk file (`Docker.raw`). When you delete containers and images inside the VM, the ext4 filesystem inside the VM marks those blocks as free. However, the host macOS hypervisor does not automatically shrink the sparse `.raw` disk file unless background TRIM/UNMAP commands are supported and executed. The virtual disk file must be compacted manually or via Docker Desktop's "Clean / Purge data" facility.
</details>

<details>
<summary><strong>3. What is the danger of executing <code>docker system prune --volumes</code>?</strong></summary>

`docker system prune --volumes` deletes all local Docker volumes that are not currently referenced by an active or stopped container. If an engineer temporarily destroys a database container (e.g., via `docker compose down`), its persistent volume becomes unattached. Running `--volumes` will permanently delete that database volume from the host hard drive, resulting in catastrophic unrecoverable data loss.
</details>

<details>
<summary><strong>4. How can you prune only images and containers created more than 48 hours ago?</strong></summary>

By supplying the `--filter "until=<duration>"` flag:
```bash
docker container prune --filter "until=48h" -f
docker image prune --filter "until=48h" -f
```
Docker evaluates the creation timestamps and purges only objects created prior to the 48-hour cutoff.
</details>

<details>
<summary><strong>5. What is the difference between <code>docker system df</code> and the standard Linux <code>df -h</code>?</strong></summary>

- Standard Linux `df -h` inspects the host's underlying physical filesystem and mounts, reporting total blocks and available gigabytes across disk partitions. It has no understanding of container abstractions.
- `docker system df` queries the Docker daemon's internal database, categorizing space specifically by Docker subsystem: Images (shared vs unique), Containers (writable layers), Local Volumes, and BuildKit Build Cache, explicitly calculating reclaimable percentages.
</details>

<details>
<summary><strong>6. How does <code>docker builder prune</code> differ from <code>docker image prune</code>?</strong></summary>

- `docker image prune` removes image layers from Docker's local image storage (images that can be launched via `docker run`).
- `docker builder prune` removes cache records maintained specifically by **BuildKit** (the compiler engine), including cached snapshot stages, downloaded source tarballs, package manager cache mounts, and git build contexts that are not tagged as runnable images.
</details>

<details>
<summary><strong>7. What happens if a Docker host runs out of inodes while reporting 20GB of free disk space?</strong></summary>

In Linux filesystems, every file, directory, and symlink requires an **inode** (index node) data structure. If an application generates millions of tiny temporary files, all available filesystem inodes are exhausted. Even if gigabytes of free disk capacity remain, the Linux kernel will reject all file creation calls with `No space left on device`. Docker will crash, unable to create layer directories or sockets. Resolved by purging unused images and containers (`docker system prune -af`).
</details>

<details>
<summary><strong>8. How can an administrator enforce a hard cap on BuildKit cache size?</strong></summary>

By passing the `--keep-storage` flag during pruning:
```bash
docker builder prune --keep-storage 5GB -f
```
Or permanently in BuildKit configuration (`buildkitd.toml`):
```toml
[worker.oci]
  gckeepstorage = "5GB"
```
BuildKit will automatically run garbage collection to ensure cache storage stays below the configured cap.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect System Storage Breakdown</strong></summary>

**Goal**: Inspect disk usage categorized by Docker subsystem.

```bash
docker system df
```
</details>

<details>
<summary><strong>Exercise 2: Perform Verbose Storage Inspection with `docker system df -v`</strong></summary>

**Goal**: Identify the largest individual images and volumes on your machine.

```bash
docker system df -v
```
</details>

<details>
<summary><strong>Exercise 3: Test Safe Dangling Prune</strong></summary>

**Goal**: Safely reclaim dangling intermediate layers without risking tagged production images.

```bash
docker image prune -f
```
</details>

<details>
<summary><strong>Exercise 4: Prune Stopped Containers with Time Filter</strong></summary>

**Goal**: Prune stopped containers older than 1 hour.

```bash
docker container prune --filter "until=1h" -f
```
</details>

<details>
<summary><strong>Exercise 5: Inspect BuildKit Cache Size and Prune Safely</strong></summary>

**Goal**: Check build cache usage and reclaim space.

```bash
docker builder du
docker builder prune --filter "until=24h" -f
```
</details>

<details>
<summary><strong>Exercise 6: Filtered Network Prune</strong></summary>

**Goal**: Clean up unused bridge networks while preserving active service networks.

```bash
docker network prune -f
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                          DOCKER DISK CLEANUP CHEAT SHEET                              |
+---------------------------------------------------------------------------------------+
| View Storage Breakdown:docker system df                                               |
| Verbose Inspection:    docker system df -v                                            |
| Clean Stopped Containers: docker container prune -f                                   |
| Clean Dangling Images: docker image prune -f (Safe!)                                  |
| Clean All Unused Images:docker image prune -af (Warning: Re-downloads needed!)        |
| Clean BuildKit Cache:  docker builder prune -f                                        |
| Safe Time Filter:      docker system prune --filter "until=48h" -f                    |
| Nuclear Factory Reset: docker system prune -af (Preserves Volumes!)                  |
| EXTREME DANGER:        docker system prune -a --volumes (PERMANENTLY ERASES DATABASES)|
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Docker Documentation: Prune Unused Docker Objects](https://docs.docker.com/config/pruning/)
- [Docker CLI Command Reference: docker system df](https://docs.docker.com/reference/cli/docker/system/df/)
- [Docker CLI Command Reference: docker system prune](https://docs.docker.com/reference/cli/docker/system/prune/)
- [Docker Documentation: Manage Build Cache with Buildx](https://docs.docker.com/build/cache/prune/)
- [Microsoft Learn: How to Compact WSL 2 Virtual Disks](https://learn.microsoft.com/en-us/windows/wsl/vhd-size)

---

| [< Previous: 04-build.md](./04-build.md) | [Next: 06-resource-limits-cpu-memory.md](./06-resource-limits-cpu-memory.md) > |
| :--- | :--- |
