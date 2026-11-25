# Docker Storage Architecture: Volumes and Bind Mounts

```
Track: DevOps & Containerization
Module: 04-volumes-and-networking
Prerequisites: 01-fundamentals/06-image-layers-and-union-filesystems.md, 02-commands/01-run-ps-stop.md
Difficulty: Intermediate
Estimated Duration: 45 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Mount Options Reference](#4-syntax--mount-options-reference)
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

By design, container filesystems are ephemeral. When a container runs, Docker creates a thin copy-on-write (CoW) layer on top of the immutable image layers (see [01-fundamentals/06-image-layers-and-union-filesystems.md](../01-fundamentals/06-image-layers-and-union-filesystems.md)). When the container is deleted (`docker rm`), this writable layer is permanently obliterated.

For stateless microservices, ephemerality is a feature. However, stateful workloads (databases, message brokers, user file uploads, caches) demand **persistence**, while developer workflows demand **live code reloading** without rebuilding images.

Docker provides two primary persistence mechanisms:
1. **Named Volumes**: Storage areas created and managed entirely by the Docker daemon in `/var/lib/docker/volumes/`. They bypass the storage driver (OverlayFS2), deliver near-native disk I/O, isolate state from host directory layout, and support remote volume storage drivers (NFS, AWS EBS, Ceph).
2. **Bind Mounts**: Direct mapping of an arbitrary directory or file on the host machine into the container's mount namespace. Highly useful for local source code mounting, configuration file injection, and host socket sharing.

---

## 2. Core Concepts

### Storage Types Overview

```
+--------------------------------------------------------------------------+
|                              HOST SYSTEM                                 |
|                                                                          |
|  +--------------------------------------------------------------------+  |
|  | /var/lib/docker/volumes/<volume_name>/_data  [DOCKER MANAGED]      |  |
|  |                                ^                                   |  |
|  +--------------------------------|-----------------------------------+  |
|                                   | (Volume Mount)                       |
|  +--------------------------------|-----------------------------------+  |
|  | /home/developer/project/src    |             [HOST PATH]           |  |
|  |                                |                                   |  |
|  +--------------------------------|-----------------------------------+  |
|                                   | (Bind Mount)                         |
|                                   v                                      |
|  +-----------------------------------------------------------------+  |  |
|  |                       CONTAINER NAMESPACE                       |  |  |
|  |                                                                 |  |  |
|  |  /app/data (Mounted Volume)      /app/src (Mounted Host Folder) |  |  |
|  |                                                                 |  |  |
|  |  [Container Ephemeral CoW Layer: /var/lib/docker/overlay2/...]  |  |  |
|  +-----------------------------------------------------------------+  |  |
+--------------------------------------------------------------------------+
```

### Key Differences at a Glance
- **Volumes**:
  - Reside in a Docker-managed partition (`/var/lib/docker/volumes/`).
  - Safe from accidental host manipulation.
  - Automatically initialized with existing container directory contents on first mount.
  - Portable across platforms (Linux, macOS, Windows).
- **Bind Mounts**:
  - Mount any absolute path from the host.
  - Tightly coupled to host filesystem structure and permissions.
  - Existing container directory contents are hidden/shadowed by the host mount.
  - File permissions match host UIDs/GIDs directly.

---

## 3. How It Works Internally

### Linux Mount Namespaces & The VFS

Docker utilizes Linux **mount namespaces** (`CLONE_NEWNS`) to provide isolated filesystem hierarchies.

1. **OverlayFS Bypass**:
   When a container accesses files within its standard root filesystem, every read/write passes through the `overlay2` storage driver, incurring a slight metadata and copy-up lookup penalty.
   When accessing a volume or bind mount, the Linux kernel binds a host VFS directory directly to a directory inside the container's mount namespace via `pivot_root` and `mount --bind`.
   **Result**: I/O operations on volumes and bind mounts achieve 100% native kernel speed, equivalent to host disk read/write throughput.

```
Container I/O
      |
      +---> Path: /etc/hosts, /app/main.py ---> Passes through OverlayFS2 (CoW)
      |
      +---> Path: /var/lib/postgresql/data ---> Direct VFS Mount (Native Kernel I/O)
```

### Volume Pre-Population Mechanics
When an empty Docker volume is mounted to a non-empty container path:
- Docker automatically copies all pre-existing files and directories from the image target directory into the newly created volume, including preserving file permissions and ownerships.
- **Bind mounts do NOT do this**: A bind mount completely shadows (hides) whatever was in the target directory inside the image.

---

## 4. Syntax & Mount Options Reference

Docker supports two syntaxes: the legacy `-v` (`--volume`) flag and the modern, explicit `--mount` flag. In modern production environments, `--mount` is strongly preferred for readability and parameter validation.

### `-v` vs `--mount` Syntax Comparison

```bash
# 1. Named Volume
# Legacy:
docker run -d -v mydata:/var/lib/postgresql/data postgres:17
# Modern:
docker run -d \
  --mount type=volume,source=mydata,target=/var/lib/postgresql/data \
  postgres:17

# 2. Read-Only Named Volume
# Legacy:
docker run -d -v mydata:/app/data:ro alpine:3.21
# Modern:
docker run -d \
  --mount type=volume,source=mydata,target=/app/data,readonly \
  alpine:3.21

# 3. Bind Mount (Host Directory to Container)
# Legacy:
docker run -d -v $(pwd)/src:/app/src node:22-alpine
# Modern:
docker run -d \
  --mount type=bind,source="$(pwd)/src",target=/app/src \
  node:22-alpine

# 4. Read-Only Bind Mount with Consistency Option
docker run -d \
  --mount type=bind,source=/etc/ssl/certs,target=/etc/ssl/certs,readonly \
  nginx:alpine
```

### Volume CLI Commands

```bash
# List all existing volumes
docker volume ls

# Filter dangling (unattached) volumes
docker volume ls -f dangling=true

# Inspect physical host path and options of a volume
docker volume inspect mydata

# Create a volume with custom driver or labels
docker volume create \
  --driver local \
  --label environment=production \
  db-backup-vol

# Remove a specific volume (must not be attached to any container)
docker volume rm mydata

# Purge all unused volumes (CAUTION: Deletes unattached state)
docker volume prune -f
```

---

## 5. Practical Implementation & Examples

### Example 1: Database Persistence with PostgreSQL Named Volume
Demonstrating survival of database records across container destruction and re-creation.

```bash
# 1. Create a dedicated volume
docker volume create pg_prod_data

# 2. Run PostgreSQL container attached to volume
docker run -d \
  --name db-instance-1 \
  -e POSTGRES_PASSWORD=supersecret \
  --mount type=volume,source=pg_prod_data,target=/var/lib/postgresql/data \
  postgres:17-alpine

# 3. Insert test record
docker exec -i db-instance-1 psql -U postgres -c "CREATE TABLE users (id SERIAL PRIMARY KEY, name VARCHAR(50));"
docker exec -i db-instance-1 psql -U postgres -c "INSERT INTO users (name) VALUES ('Alice'), ('Bob');"

# 4. Destroy container completely
docker rm -f db-instance-1

# 5. Boot a completely new container instance with the same volume
docker run -d \
  --name db-instance-2 \
  -e POSTGRES_PASSWORD=supersecret \
  --mount type=volume,source=pg_prod_data,target=/var/lib/postgresql/data \
  postgres:17-alpine

# 6. Verify data is intact
docker exec -i db-instance-2 psql -U postgres -c "SELECT * FROM users;"
# Output displays Alice and Bob intact!
```

### Example 2: Local Development with Hot-Reloading Bind Mount
Mounting local source code while preserving container-installed `node_modules`.

```bash
docker run -d \
  --name web-dev \
  -p 3000:3000 \
  -e NODE_ENV=development \
  --mount type=bind,source="$(pwd)",target=/app \
  --mount type=volume,target=/app/node_modules \
  node:22-alpine \
  npm run dev
```
> **Notice**: The anonymous volume mounted at `/app/node_modules` overrides the bind mount at `/app`, preventing the host's missing or host-OS-specific `node_modules` from clobbering the container's compiled native modules!

### Example 3: Configuration File Injection via Read-Only Bind Mount
Injecting custom Nginx configuration dynamically without baking a new Dockerfile.

```bash
docker run -d \
  --name edge-proxy \
  -p 80:80 \
  --mount type=bind,source="$(pwd)/nginx.conf",target=/etc/nginx/nginx.conf,readonly \
  nginx:1.27-alpine
```

### Example 4: Sharing Storage Between Multiple Containers (Producer / Consumer)
A logging producer writes data to a shared volume while an analytical parser consumes it concurrently.

```bash
# Create shared volume
docker volume create telemetry-vol

# Run Producer container
docker run -d \
  --name telemetry-producer \
  --mount type=volume,source=telemetry-vol,target=/var/log/telemetry \
  alpine:3.21 \
  sh -c "while true; do echo \$(date) - Event logged >> /var/log/telemetry/events.log; sleep 2; done"

# Run Consumer container (Read-Only)
docker run -d \
  --name telemetry-consumer \
  --mount type=volume,source=telemetry-vol,target=/var/log/telemetry,readonly \
  alpine:3.21 \
  sh -c "tail -f /var/log/telemetry/events.log"

# Inspect consumer logs
docker logs -f telemetry-consumer
```

### Example 5: Docker Socket Mount for CI/CD Containerization (Docker-in-Docker via Host Socket)
Mounting `/var/run/docker.sock` to allow a management container to control the host daemon.

```bash
docker run -it --rm \
  --name docker-cli-helper \
  --mount type=bind,source=/var/run/docker.sock,target=/var/run/docker.sock \
  docker:27-cli \
  docker ps
```
> **Security Warning**: Mounting the Docker socket grants full root privileges over the host machine. Never mount this in untrusted containers.

---

## 6. Production Best Practices & Antipatterns

### Best Practices
- **Prefer Named Volumes for State, Bind Mounts for Configuration**: Use volumes for persistent state (`/var/lib/mysql`) and read-only bind mounts for static config files (`/etc/config.json`).
- **Enforce Read-Only Mounts**: If a container only reads configuration files or certificates, always specify `readonly` (`:ro`).
- **Always Explicitly Name Volumes**: Avoid anonymous volumes (`-v /var/lib/data`) in production; they clutter the host with unidentifiable hashes (`/var/lib/docker/volumes/847294872938...`).
- **Use `--mount` Instead of `-v`**: `--mount` fails loudly on invalid keys and missing host directories instead of silently creating host folders as root.
- **Handle UID/GID Alignment**: Ensure host permissions allow container non-root users (`UID 10001`) to write to mounted paths.

### Anti-Patterns
- **Storing Production Databases in Bind Mounts**: Bind mounts in production tie containers to specific host paths, breaking container orchestration (Kubernetes/Swarm) and failing on permissions across OS boundaries.
- **Accidental Volume Pruning**: Running `docker volume prune -a` without a verified backup deletes all unattached state across the host.
- **Mounting Entire Host Root (`/`) or Sensitive Dirs (`/etc`, `/proc`)**: Violates security boundaries and invites privilege escalation.

---

## 7. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| `Permission denied (errno 13)` writing to bind mount | Host folder owned by root, but container runs as non-root user (e.g. UID 1000) | `ls -ld <host_path>` and container `id` | Run `chown -R 1000:1000 <host_path>` on host. |
| Container directory appears empty after bind mount | Bind mount completely shadows (masks) image contents at that path | Compare image with `docker run --rm image ls /path` | Copy needed files into host directory prior to mounting, or use named volumes. |
| Docker created an unwanted directory on host as `root` | Used `-v /missing/path:/app` instead of `--mount`; Docker auto-creates missing host paths as root | `ls -ld /missing/path` | Use `--mount type=bind`; it produces a hard error if host path does not exist. |
| macOS/Windows Docker Desktop file sync is slow | Virtualization filesystem translation layer (gRPC FUSE / VirtioFS) adds latency to thousands of small files | Benchmark `npm install` inside bind mount | Use named volumes for file-heavy directories (`node_modules`, `vendor`). |
| `docker rm -v` failed to remove volume | Volume is shared with another stopped container | `docker ps -a --filter volume=<name>` | Remove all referencing containers before removing volume. |
| SELinux blocks container access to mounted directory | Container lacks SELinux label for host directory | Check `dmesg \| grep audit` | Append `:z` (shared) or `:Z` (private) flag to `-v`, or configure SELinux contexts. |

---

## 8. Performance Tuning & Resource Optimization

1. **OverlayFS2 vs Volume Performance**:
   Heavy random I/O (database transactions, write-ahead logs) suffers significant write-amplification inside OverlayFS2 copy-on-write layers. Volumes completely bypass OverlayFS2, routing directly to Linux block storage devices with zero filesystem driver translation overhead.
2. **Docker Desktop on macOS (VirtioFS)**:
   Ensure Docker Desktop uses **VirtioFS** (Settings -> General -> Virtualization Framework -> VirtioFS). Avoid mounting `node_modules` across host macOS and Linux VM; isolate dependencies inside container volumes.

---

## 9. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK: Host Compromise via Docker Socket Bind Mount         |
+--------------------------------------------------------------+
| Attacker exploits web vulnerability -> Gets container shell. |
| Container has `/var/run/docker.sock` mounted.                |
| Attacker runs:                                               |
| `docker run -v /:/host -it alpine chroot /host`              |
|                                                              |
| OUTCOME: ATTACKER OBTAINS FULL ROOT ACCESS ON DOCKER HOST.   |
+--------------------------------------------------------------+
```

### Security Mitigations
- **Never expose `/var/run/docker.sock` to untrusted workloads**.
- **Always declare `readonly`** when mounting host files:
  `--mount type=bind,source=/etc/nginx/conf.d,target=/etc/nginx/conf.d,readonly`
- **Use User Namespaces (`userns-remap`)**: Maps container `root` (UID 0) to unprivileged UID on the host (e.g., UID 100000), mitigating host compromise even if a container breaks out through a bind mount.

---

## 10. Architectural Comparison Table

| Feature / Trait | Named Volumes | Bind Mounts | Tmpfs Mounts |
|:---|:---|:---|:---|
| **Host Storage Location** | `/var/lib/docker/volumes/` | Any user-specified host path | Host RAM / Swap only |
| **Managed by Docker?** | **Yes** (CLI tools) | No | **Yes** |
| **Initializes with Image Data?** | **Yes** | No (Shadows target) | No |
| **Portability Across Environments**| High | Low (Path-dependent) | High |
| **Remote Driver Support (EBS/NFS)**| **Yes** | No | No |
| **Ideal Use Case** | Databases, persistent app state | Local dev code, static configs| Secrets, volatile caches |
| **OS File Permissions** | Handled natively by Docker | Direct reflection of host OS | Ephemeral in-memory |

---

## 11. Real-World Scenario Walkthrough

### Scenario: High-Throughput Redis Cache Out-Of-Disk Crash

#### Incident
A production Redis cache running in a container crashed because disk space filled up instantly. The team discovered that Redis was configured with `appendonly yes` (AOF logs), but no volume was defined.

#### Root Cause
Because no volume was attached, Redis wrote its 80GB AOF log into the container's ephemeral `overlay2` writable layer on the host's root partition (`/`), exhausting disk space and crashing all services on the Linux node.

#### Solution
1. Moved Redis persistence to a dedicated named volume backed by a separate high-speed NVMe mount:
   ```bash
   docker volume create --driver local \
     --opt type=none \
     --opt device=/mnt/nvme-storage/redis \
     --opt o=bind \
     redis-nvme-vol
   ```
2. Launched Redis attached to the new volume:
   ```bash
   docker run -d --name redis-prod \
     --mount type=volume,source=redis-nvme-vol,target=/data \
     redis:7-alpine redis-server --appendonly yes
   ```
3. Configured log rotation and host disk monitoring.

---

## 12. Self-Assessment & Interview Questions

<details>
<summary><strong>1. What happens when you mount a named volume into a container directory that already contains files?</strong></summary>

**Answer:**
If the named volume is empty, Docker automatically copies all existing files, subdirectories, and their metadata (ownership, permissions) from the image target directory into the newly mounted volume. If the volume is already populated, the volume's existing contents supersede and hide the files in the image.
</details>

<details>
<summary><strong>2. How does the behavior in Question 1 differ when using a bind mount instead?</strong></summary>

**Answer:**
A bind mount **never** copies files from the image to the host. Instead, it unconditionally shadows (masks) whatever existed in the image target directory with the host directory's current contents (even if the host directory is completely empty).
</details>

<details>
<summary><strong>3. Why is `--mount` strongly preferred over `-v` in modern production Docker operations?</strong></summary>

**Answer:**
`--mount` uses explicit key-value pairs (`type=bind`, `source=...`, `target=...`, `readonly`), which makes intent immediately clear. Crucially, if you specify a non-existent host path with `-v`, Docker silently creates a new directory on the host owned by `root`. With `--mount type=bind`, Docker throws an immediate error if the host path does not exist, preventing misconfigurations.
</details>

<details>
<summary><strong>4. How does Docker handle file permissions between the host and container in bind mounts?</strong></summary>

**Answer:**
Docker uses the host kernel directly without automatic UID/GID translation (unless user namespace remapping is enabled). If a file on the host is owned by UID 1000 and the container runs as UID 10001, the container process will receive `Permission Denied` when attempting to write to that file.
</details>

<details>
<summary><strong>5. What is an anonymous volume, and how does it originate?</strong></summary>

**Answer:**
An anonymous volume is a volume created without a human-readable name (Docker generates a random 64-character hash name). It originates when running a container with `-v /path` (specifying only the destination), or when a base Dockerfile declares a `VOLUME ["/path"]` instruction and the operator does not explicitly map a named volume during `docker run`.
</details>

<details>
<summary><strong>6. What is the security risk of mounting `/var/run/docker.sock` inside a container?</strong></summary>

**Answer:**
The Docker Unix socket provides unauthenticated root-level control over the Docker daemon. Any process inside the container can command the host daemon to spawn new containers with arbitrary host mounts (e.g., `-v /:/host`), edit host system files, add sudoers entries, and achieve complete root takeover of the underlying host machine.
</details>

<details>
<summary><strong>7. How can you mount a single configuration file as read-only using Docker CLI?</strong></summary>

**Answer:**
```bash
docker run -d --mount type=bind,source=/path/to/config.yaml,target=/etc/app/config.yaml,readonly my-app
```
</details>

<details>
<summary><strong>8. Why should you avoid storing high-write database files directly in a container's writable layer?</strong></summary>

**Answer:**
Writing to a container's writable layer utilizes the OverlayFS2 storage driver, which uses Copy-on-Write (CoW). This introduces CPU and latency overhead for file modification and fragmentation. Furthermore, ephemeral layers share the host's primary root partition, risking disk exhaustion, and all data is destroyed permanently when the container is deleted.
</details>

---

## 13. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Inspecting Host Filesystem Path of a Named Volume</strong></summary>

**Goal:** Create a volume, write data into it from a container, and inspect the actual physical host path.

**Step-by-step Solution:**
1. Create and populate:
   ```bash
   docker volume create lab_vol
   docker run --rm --mount type=volume,source=lab_vol,target=/data alpine sh -c "echo 'Docker Storage' > /data/test.txt"
   ```
2. Inspect mount point:
   ```bash
   docker volume inspect lab_vol --format '{{.Mountpoint}}'
   # On Linux: /var/lib/docker/volumes/lab_vol/_data
   ```
3. (Linux with root): `cat /var/lib/docker/volumes/lab_vol/_data/test.txt`
</details>

<details>
<summary><strong>Exercise 2: Overcoming Bind Mount Shadowing with Anonymous Volumes</strong></summary>

**Goal:** Mount a host project folder into `/app` while protecting the container's installed `node_modules`.

**Step-by-step Solution:**
1. Execute multi-mount command:
   ```bash
   docker run -d --name node-app \
     --mount type=bind,source="$(pwd)",target=/app \
     --mount type=volume,target=/app/node_modules \
     node:22-alpine sleep 3600
   ```
2. Verify `/app/node_modules` remains distinct from host directory.
</details>

<details>
<summary><strong>Exercise 3: Read-Only Enforcement Verification</strong></summary>

**Goal:** Attempt to write into a read-only volume and verify failure.

**Step-by-step Solution:**
1. Run container:
   ```bash
   docker run --rm \
     --mount type=volume,source=lab_vol,target=/secure,readonly \
     alpine touch /secure/illegal.txt
   ```
2. Verify error: `touch: /secure/illegal.txt: Read-only file system`.
</details>

<details>
<summary><strong>Exercise 4: Pruning Dangling Volumes Safely</strong></summary>

**Goal:** Identify all dangling volumes without deleting active state.

**Step-by-step Solution:**
1. List unattached volumes:
   ```bash
   docker volume ls -f dangling=true
   ```
2. Filter by label:
   ```bash
   docker volume ls --filter label=environment=staging
   ```
</details>

<details>
<summary><strong>Exercise 5: Live Nginx Hot Configuration Reload</strong></summary>

**Goal:** Change an Nginx configuration file on the host and reload it without restarting the container.

**Step-by-step Solution:**
1. Create `test.conf`:
   ```nginx
   server { listen 80; location / { return 200 "Version 1\n"; } }
   ```
2. Run Nginx:
   ```bash
   docker run -d --name nginx-live -p 8080:80 \
     --mount type=bind,source="$(pwd)/test.conf",target=/etc/nginx/conf.d/default.conf,readonly \
     nginx:alpine
   ```
3. Test: `curl http://localhost:8080` -> "Version 1".
4. Modify `test.conf` on host to "Version 2".
5. Reload in container: `docker exec nginx-live nginx -s reload`.
6. Test: `curl http://localhost:8080` -> "Version 2".
</details>

<details>
<summary><strong>Exercise 6: Simulating Shared Storage between Containers</strong></summary>

**Goal:** Share a volume between two containers, verifying real-time synchronization.

**Step-by-step Solution:**
1. Create volume: `docker volume create shared-cache`.
2. Launch writer:
   ```bash
   docker run -d --name writer --mount type=volume,source=shared-cache,target=/cache alpine \
     sh -c "echo 'cached-value' > /cache/token && sleep 300"
   ```
3. Launch reader:
   ```bash
   docker run --rm --mount type=volume,source=shared-cache,target=/cache alpine cat /cache/token
   # Outputs 'cached-value'
   ```
</details>

---

## 14. Summary & Reference Cheat Sheet

### CLI Quick Reference
```bash
# Modern Mount Syntax:
--mount type=volume,source=<name>,target=<container_path>[,readonly]
--mount type=bind,source=<host_path>,target=<container_path>[,readonly]

# Volume Lifecycle:
docker volume create <name>
docker volume ls
docker volume inspect <name>
docker volume rm <name>
docker volume prune
```

### Golden Decision Rules
- **Named Volume**: Database data, user uploads, persistent app state.
- **Bind Mount**: Live source code in dev, host config files (`nginx.conf`), host sockets.
- **Tmpfs**: Temporary secrets, transient caches, RAM-disk speed requirements.

---

[← Previous: Module 03 Dockerfile](../03-dockerfile/README.md) | [Next: 02-tmpfs-and-storage-drivers.md →](02-tmpfs-and-storage-drivers.md)
