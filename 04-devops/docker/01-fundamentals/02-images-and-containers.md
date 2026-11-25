---
title: "Docker Images and Containers: Anatomies, Immutability, and Lifecycles"
description: "In-depth technical breakdown of Docker Images and Containers: layer stacking, UpperDir writable layer, OCI manifests, image immutability, and state transitions."
category: "DevOps / Containers"
tags: ["docker", "images", "containers", "layers", "overlayfs", "immutability", "lifecycle"]
difficulty: "Beginner"
prerequisites: ["01-docker-vs-vm.md", "Basic Linux filesystem familiarity"]
estimated_read_time: "25 min"
---

# Docker Images and Containers: Anatomies, Immutability, and Lifecycles

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. Core Concepts & Terminology](#2-core-concepts--terminology)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Anatomy of an Image vs a Container](#4-anatomy-of-an-image-vs-a-container)
- [5. Practical Diagnostic Demonstrations](#5-practical-diagnostic-demonstrations)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table: Images vs Containers](#9-comparison-table-images-vs-containers)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

The relationship between a **Docker Image** and a **Docker Container** is the central paradigm of containerization. It mirrors the fundamental relationship between a program binary on disk and an executing process in operating system memory:

$$\text{Class} : \text{Object} \quad \Longleftrightarrow \quad \text{Program Binary} : \text{Process} \quad \Longleftrightarrow \quad \text{Docker Image} : \text{Docker Container}$$

- A **Docker Image** is an immutable, read-only package containing application binaries, runtime libraries, dependencies, default environment configurations, and execution metadata. Images are cryptographically content-addressable and structured as a directed acyclic graph (DAG) of read-only tarball layers.
- A **Docker Container** is an instantiated, stateful execution environment. It consists of the read-only image layers mounted as an OverlayFS root filesystem, combined with a thin read-write scratch layer (`UpperDir`), executing within a set of isolated kernel namespaces governed by control group resource quotas.

Confusing these two concepts leads to severe operational errors: storing dynamic production data in ephemeral container layers, attempting to mutate production images at runtime, or failing to appreciate why multiple containers sharing one base image consume negligible host disk space.

```
+---------------------------------------------------------------------------------+
|                       IMAGE LAYERS TO RUNNING CONTAINER                         |
|                                                                                 |
|  [Running Container: web-worker-1]                                              |
|  +---------------------------------------------------------------------------+  |
|  | Container Read-Write Layer (UpperDir)  <-- Ephemeral, destroyed on rm     |  |
|  +---------------------------------------------------------------------------+  |
|  | ========================= OverlayFS Mount =============================== |  |
|  | Image Layer 3 (Read-Only): COPY app/ /opt/app                             |  |
|  | Image Layer 2 (Read-Only): RUN npm install --production                  |  |
|  | Image Layer 1 (Read-Only): Base OS (node:20-alpine)                       |  |
|  +---------------------------------------------------------------------------+  |
|                                                                                 |
|  [Running Container: web-worker-2] (Shares identical Image Layers 1, 2, 3!)     |
|  +---------------------------------------------------------------------------+  |
|  | Container Read-Write Layer (UpperDir)  <-- Independent scratch space       |  |
|  +---------------------------------------------------------------------------+  |
|  | ========================= OverlayFS Mount =============================== |  |
|  | (Shared Read-Only Layers 1, 2, 3 cached once on host storage)              |  |
|  +---------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------+
```

---

## 2. Core Concepts & Terminology

- **Image Immutability**: Once an image layer is built and assigned a cryptographic SHA256 digest, its contents can never be altered. Any change produces a new layer with a new hash.
- **Content-Addressable Storage (CAS)**: Layers are identified by the SHA256 checksum of their uncompressed archive contents (`diffID`) or compressed distribution payload (`blob digest`), preventing tampering.
- **Copy-on-Write (CoW)**: When a process inside a container modifies an existing file from a read-only image layer, the storage driver copies the file up into the container's writable layer before allowing modifications.
- **Ephemeral Writable Layer (`UpperDir`)**: The top filesystem layer allocated exclusively to a specific container instance. When the container is deleted (`docker rm`), this layer is purged.
- **Image Manifest**: A JSON document adhering to the OCI Image Specification describing an image's layers, media types, configuration descriptor, and architecture.
- **`docker commit`**: A command that serializes the current writable layer of a running container into a new read-only image layer. (Considered an anti-pattern in production pipelines).

---

## 3. How It Works Internally

### 3.1. How Docker Spawns a Container from an Image
When `docker run` is executed:
1. **Manifest & Layer Lookup**: The daemon retrieves the image metadata and locates the on-disk directory paths for each layer in `/var/lib/docker/overlay2/<layer-id>`.
2. **OverlayFS Composition**: The kernel mounts an OverlayFS filesystem where:
   - `lowerdir`: A colon-separated list of the immutable image layer directories.
   - `upperdir`: A brand-new empty read-write directory allocated for this specific container.
   - `workdir`: An internal directory used by the kernel to stage atomic copy-on-write operations.
   - `merged`: The unified virtual view presented to the container as root (`/`).
3. **Namespaces Allocation**: The runtime calls `clone()` with flags:
   - `CLONE_NEWPID` (Isolated process IDs)
   - `CLONE_NEWNET` (Dedicated loopback, routing table, virtual ethernet)
   - `CLONE_NEWNS` (Dedicated mount table)
   - `CLONE_NEWIPC` (Isolated system IPC and message queues)
   - `CLONE_NEWUTS` (Dedicated hostname)
4. **Root Pivoting (`pivot_root`)**: The runtime changes the process root filesystem to the `merged` OverlayFS mount directory and unmounts the host root filesystem from the container's mount namespace.
5. **Cgroup Assignment**: The PID is placed into cgroup controllers for memory, CPU, and PIDs.
6. **Execution (`execve`)**: The runtime replaces the setup process with the specified `ENTRYPOINT` and `CMD`.

---

## 4. Anatomy of an Image vs a Container

### 4.1. Inside an OCI Image
An image consists strictly of tar archives and JSON metadata:

```
+-- OCI Image Structure --------------------------------------------------------+
|                                                                               |
|  index.json          -> Lists manifests for multi-arch platforms              |
|  manifest.json       -> Schema version, config descriptor, layer descriptors  |
|  config.json         -> Architecture, OS, env vars, working dir, entrypoint   |
|  blobs/sha256/                                                                |
|    |-- a83f2...      -> Layer 1: Compressed tarball of rootfs binaries        |
|    |-- d74b1...      -> Layer 2: Compressed tarball of application code       |
|    \-- f920e...      -> Layer 3: Configuration metadata                       |
+-------------------------------------------------------------------------------+
```

### 4.2. Inside a Container Instance
A container is a live kernel object:
```
+-- Live Container State -------------------------------------------------------+
|  Container ID:    3e7389ab294e...                                             |
|  Host PID:        18492 (PID 1 inside container)                              |
|  State:           Running (Up 12 minutes)                                     |
|  Network IP:      172.17.0.2                                                  |
|  Ports Mapped:    0.0.0.0:8080 -> 80/tcp                                      |
|  Mounts:          /var/lib/docker/overlay2/3e73.../merged                      |
|  Cgroups Path:    /sys/fs/cgroup/docker/3e7389ab294e...                       |
+-------------------------------------------------------------------------------+
```

---

## 5. Practical Diagnostic Demonstrations

### Example 1: Inspecting Image Layer History
Examine the exact build instructions and size contributions of each immutable layer in an image:

```bash
docker history nginx:alpine
```
*Output:*
```text
IMAGE          CREATED        CREATED BY                                      SIZE      COMMENT
c926b61bad3c   2 days ago     CMD ["nginx" "-g" "daemon off;"]                0B        buildkit.dockerfile.v0
<missing>      2 days ago     STOPSIGNAL SIGQUIT                              0B        buildkit.dockerfile.v0
<missing>      2 days ago     EXPOSE 80                                       0B        buildkit.dockerfile.v0
<missing>      2 days ago     RUN /bin/sh -c set -x ...                       18.5MB    buildkit.dockerfile.v0
<missing>      5 days ago     ADD alpine-minirootfs-3.20.0-x86_64.tar.gz /    7.8MB     
```

### Example 2: Demonstrating Copy-on-Write (`UpperDir` Mutation)
Prove that modifying a file inside a container creates a copy in the writable layer without mutating the original image.

```bash
# 1. Run a container
docker run -d --name cow-demo alpine sleep 60

# 2. Modify an existing file inherited from the base image (/etc/issue)
docker exec cow-demo sh -c "echo 'Customized Host Banner' >> /etc/issue"

# 3. Inspect container filesystem diff using docker diff
docker diff cow-demo
```
*Output:*
```text
C /etc
A /etc/issue
```
- `C` = Changed directory
- `A` = Added / copied-up file in `UpperDir`

```bash
# 4. Verify that a new container spawned from the same image remains completely unaffected!
docker run --rm alpine cat /etc/issue
# The original image file remains pristine and untouched!

# Cleanup
docker rm -f cow-demo
```

### Example 3: Inspecting Container UpperDir on Host Storage
Inspect where the host Linux kernel physically stores container modifications (requires root or sudo):

```bash
# 1. Launch a background container
docker run -d --name mount-inspect alpine sleep 60

# 2. Extract UpperDir path using docker inspect
UPPER_DIR=$(docker inspect --format '{{.GraphDriver.Data.UpperDir}}' mount-inspect)
echo "Container Writable Directory: $UPPER_DIR"

# 3. Create a file inside container
docker exec mount-inspect touch /root/hello_from_container.txt

# 4. Inspect that exact directory from the host filesystem!
sudo ls -l $UPPER_DIR/root/
# Output: hello_from_container.txt

# Cleanup
docker rm -f mount-inspect
```

### Example 4: Demonstrating Shared Layer Storage Efficiency
Demonstrate that running 100 containers from the same image incurs zero incremental image disk storage.

```bash
# Check current disk usage
docker system df

# Spin up 5 containers sharing the same image
docker run -d --name c1 alpine sleep 60
docker run -d --name c2 alpine sleep 60
docker run -d --name c3 alpine sleep 60

# Check disk usage again
docker system df
```
*Notice*: The image size remains constant; only a few kilobytes of metadata are allocated for each container's `UpperDir`.

```bash
# Cleanup
docker rm -f c1 c2 c3
```

### Example 5: Why `docker commit` is an Anti-Pattern
Demonstrate how `docker commit` captures non-reproducible operational baggage:

```bash
# 1. Run container and manually install curl
docker run -name dirty-container alpine apk add --no-cache curl

# 2. Commit container state to a new image
docker commit dirty-container my-dirty-image:v1

# 3. Inspect history of the committed image
docker history my-dirty-image:v1
```
*Notice*: The history shows an opaque layer with no record of package versions, dependencies, or security configuration. In production, always use a declarative `Dockerfile`.

```bash
# Cleanup
docker rm dirty-container && docker rmi my-dirty-image:v1
```

---

## 6. Best Practices & Production Guidelines

1. **Treat Images as Immutable Artifacts**: Once an image tag is published to a registry, it should never be re-tagged or overwritten with different contents. Pin exact versions or SHA256 digests.
2. **Never Treat Container Writable Layers as Persistent Storage**: Anything written to a container layer is deleted upon container removal. Use persistent Docker Volumes for database files, user uploads, and application state.
3. **Minimize Layer Count & Intermediate Bloat**: Combine sequential shell commands into single `RUN` statements (`apt-get update && apt-get install -y ... && rm -rf /var/lib/apt/lists/*`) to prevent caching unnecessary cache files in intermediate read-only layers.
4. **Enforce Read-Only Root Filesystems**: For high-security environments, run containers with the `--read-only` flag. This prevents attackers from installing backdoors or malware into the container's writable layer.
5. **Ban `docker commit` from CI/CD Pipelines**: Always produce images deterministically from source control via automated BuildKit pipelines, never by snapshotting ad-hoc modified containers.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Production Impact | Proper Solution |
|:---|:---|:---|:---|
| **Using `docker commit` to deploy code** | Manually SSH-ing into a container, running `apt upgrade`, and committing. | Unreproducible builds, untracked security vulnerabilities, opaque history. | Build images strictly via version-controlled Dockerfiles. |
| **Writing high-frequency logs to container root** | Applications logging to `/var/log/app.log` inside the container writable layer. | OverlayFS Copy-on-Write slows I/O; container layer bloats host root disk. | Stream logs to `stdout`/`stderr` or mount an external volume. |
| **Assuming deleting a file in a later layer reduces image size** | Running `RUN rm large_file.tar.gz` in a subsequent Dockerfile line. | The file remains permanently stored in the earlier immutable layer. | Delete temporary files in the same `RUN` command where they were created. |
| **Confusing stopped containers with deleted containers** | Running `docker stop` and believing disk space has been reclaimed. | Container metadata and writable layers remain on disk indefinitely. | Execute `docker rm` or start containers with `--rm`. |
| **Storing database files inside container layer** | Running PostgreSQL without a volume. | Upgrading the container or recreating it destroys all database records. | Always mount a named Docker volume to `/var/lib/postgresql/data`. |
| **Mutating files in place on large datasets** | Performing random in-place updates on a 10GB database file inside OverlayFS. | Triggers full 10GB copy-up into `UpperDir`, causing massive I/O stalls. | Bypass OverlayFS completely by using volumes for large stateful files. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Read Latency**: Reading unchanged files from image layers occurs at native speed (direct page cache access).
- **Copy-On-Write (CoW) Latency**: The first time a container modifies an existing file from a read-only lower layer, the kernel must locate the file, allocate blocks in `UpperDir`, and copy the entire file upwards. Modifying small configuration files takes microseconds; modifying large binary files incurs substantial I/O latency.

### Security
- **Immutability as Defense**: Because base image layers are mounted read-only (`ro`), a compromised container process cannot tamper with base OS binaries (e.g., modifying `/bin/ls` or `/lib/x86_64-linux-gnu/libc.so.6`).
- **Read-Only Containers**: Deploying microservices with `docker run --read-only --tmpfs /tmp` prevents attackers from writing executables anywhere in the container filesystem.

### Cost
- **Storage Deduplication**: If 50 microservice containers share the same `alpine:3.20` base layer ($7\text{ MB}$), the host stores that $7\text{ MB}$ layer once on disk and loads shared libraries into kernel page cache once, saving gigabytes of disk and RAM.

---

## 9. Comparison Table: Images vs Containers

| Dimension | Docker Image | Docker Container |
|:---|:---|:---|
| **State** | Static, read-only, immutable | Dynamic, stateful, executing |
| **Storage Nature** | Stack of compressed tarball layers (DAG) | OverlayFS mount (`lowerdir` + `upperdir`) |
| **Content Address**| Cryptographic SHA256 content digest | Container ID (UUID) & Container Name |
| **Persistence** | Persists permanently in registry / local store| Ephemeral; destroyed on removal (`docker rm`)|
| **Process Model** | Passive files on disk | Active Linux processes bound to namespaces |
| **Creation Method**| Built via Dockerfile (`docker build`) | Instantiated via `docker run` / `docker create` |
| **Modification** | Cannot be modified (only rebuilt) | Writable layer modified via copy-on-write |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. What happens at the filesystem level when a container modifies a file in a read-only layer?</strong></summary>

The storage driver (OverlayFS) executes a **Copy-on-Write (CoW)** operation. The kernel intercepts the write request, checks if the file exists in the container's writable layer (`UpperDir`), and upon seeing it only in the read-only layer (`LowerDir`), copies the entire file from `LowerDir` up into `UpperDir`. Once copied, the write operation is applied to the copy in `UpperDir`. Subsequent reads and writes target the `UpperDir` version, while the lower layer file remains completely unchanged.
</details>

<details>
<summary><strong>2. Why doesn't running <code>RUN rm -rf /tmp/build-cache</code> in a separate Dockerfile instruction reduce the image size?</strong></summary>

Docker images are structured as a stack of immutable, append-only layers. Each line in a Dockerfile generates a distinct layer. When a file is created in Layer $N$ and deleted in Layer $N+1$, Layer $N+1$ merely records a special "whiteout" character device (`.wh.<filename>`) in OverlayFS masking the file from view. However, the original file remains permanently embedded in the immutable Layer $N$ archive, contributing to the overall image download size.
</details>

<details>
<summary><strong>3. How does Docker calculate an image layer's DiffID versus its compressed Blob Digest?</strong></summary>

- The **DiffID** is the SHA256 checksum of the *uncompressed* layer tar archive content. It represents the logical state of the filesystem changes.
- The **Blob Digest** is the SHA256 checksum of the *compressed* tar archive (typically gzip or zstd) as distributed across the network from a container registry.
The image configuration JSON maps compressed blob digests to their corresponding uncompressed DiffIDs.
</details>

<details>
<summary><strong>4. What is the difference between <code>docker stop</code>, <code>docker rm</code>, and <code>docker rmi</code>?</strong></summary>

- `docker stop` halts the running container process by sending `SIGTERM` followed by `SIGKILL`. The container state transitions from *Running* to *Exited*, but its writable layer and metadata remain on disk.
- `docker rm` deletes an *Exited* container instance and permanently purges its associated read-write `UpperDir` storage layer from the host disk.
- `docker rmi` (or `docker image rm`) removes the read-only image layers from the local image store, provided no existing containers (running or stopped) depend on those layers.
</details>

<details>
<summary><strong>5. Explain how the <code>--read-only</code> flag enhances container security.</strong></summary>

The `--read-only` flag mounts the container's root filesystem (including the top `UpperDir` layer) as strictly read-only. This prevents processes inside the container from creating, modifying, or deleting any files anywhere on root. If an attacker discovers an arbitrary file upload or remote code execution vulnerability, they cannot write malicious scripts to `/tmp`, alter binaries, or modify system configs. Writable locations must be explicitly provided via temporary RAM mounts (`--tmpfs /tmp`) or volumes.
</details>

<details>
<summary><strong>6. What is a "dangling" image and how is it created?</strong></summary>

A dangling image (displayed as `<none>:<none>`) is an image layer that is no longer tagged by any named repository reference. This commonly occurs when you build an image with a tag that already exists (e.g., `docker build -t my-app:latest .`). Docker assigns the tag `my-app:latest` to the new image layers, leaving the previous image layers untagged ("dangling"). They can be safely reclaimed using `docker image prune`.
</details>

<details>
<summary><strong>7. How does OverlayFS handle file deletion inside a container?</strong></summary>

When a file inherited from a read-only `lowerdir` is deleted inside a container, OverlayFS cannot delete the file from the read-only underlying layer. Instead, it creates a **whiteout file** in the container's `upperdir`. A whiteout file is a character device with major/minor device numbers set to 0/0 and named `.wh.<filename>`. When the kernel merges the directory views, any filename matching the whiteout prefix is hidden from the container's virtual directory view.
</details>

<details>
<summary><strong>8. Why is <code>docker commit</code> considered an anti-pattern for production software delivery?</strong></summary>

`docker commit` violates the principles of Infrastructure as Code (IaC) and reproducible builds:
1. **Lack of Traceability**: There is no source code repository or reviewable commit history documenting how the image was constructed.
2. **Environmental Baggage**: It captures ephemeral artifacts, temporary cache files, bash history, and potential secrets residing in memory or temporary directories.
3. **Impossibility of Automated Patching**: When base operating systems or dependencies need security updates, a committed image cannot be cleanly rebuilt.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect OCI Image Config via `docker image inspect`</strong></summary>

**Goal**: Inspect the raw JSON metadata of an image, locating the layer DiffIDs and environment variables.

```bash
# Query layer DiffIDs of Alpine image
docker image inspect alpine:latest --format '{{json .RootFS.Layers}}'

# Query default container execution architecture
docker image inspect alpine:latest --format 'Arch: {{.Architecture}} | OS: {{.Os}}'
```
</details>

<details>
<summary><strong>Exercise 2: Create and Inspect an Immutable Read-Only Container</strong></summary>

**Goal**: Verify that `--read-only` prevents any write operations to the container rootfs.

```bash
# Attempt to create a file inside a read-only container
docker run --rm --read-only alpine touch /tmp/test.txt 2>&1 || echo "Write rejected as expected!"

# Correct approach: Mount an ephemeral tmpfs on /tmp
docker run --rm --read-only --tmpfs /tmp alpine touch /tmp/test.txt && echo "Write succeeded on tmpfs!"
```
</details>

<details>
<summary><strong>Exercise 3: Simulate Whiteout File Creation in OverlayFS</strong></summary>

**Goal**: Observe how deleting a file in a container records a change in `docker diff`.

```bash
# Start container
docker run -d --name whiteout-lab alpine sleep 60

# Delete a standard file from the base layer
docker exec whiteout-lab rm /etc/hosts

# Inspect diff output
docker diff whiteout-lab | grep "hosts"
# Output shows "D /etc/hosts" (Deleted)

# Cleanup
docker rm -f whiteout-lab
```
</details>

<details>
<summary><strong>Exercise 4: Prune Dangling Images</strong></summary>

**Goal**: Intentionally create a dangling image and clean it up using `docker image prune`.

```bash
# Build image tagged v1
echo "FROM alpine\nRUN echo 'v1' > /version.txt" | docker build -t prune-test:latest -

# Rebuild with same tag but different contents to orphan the previous image
echo "FROM alpine\nRUN echo 'v2' > /version.txt" | docker build -t prune-test:latest -

# View dangling images
docker images -f "dangling=true"

# Clean up dangling images
docker image prune -f

# Clean up test image
docker rmi prune-test:latest
```
</details>

<details>
<summary><strong>Exercise 5: Calculate Exact Layer Size Contributions</strong></summary>

**Goal**: Format image history to display human-readable layer sizes.

```bash
docker history --format "table {{.CreatedBy}}\t{{.Size}}" nginx:alpine
```
</details>

<details>
<summary><strong>Exercise 6: Test Container Execution from Image Content Digest</strong></summary>

**Goal**: Launch a container using an immutable SHA256 digest reference instead of a mutable tag.

```bash
# Obtain exact digest of alpine:latest
DIGEST=$(docker inspect --format '{{index .RepoDigests 0}}' alpine:latest 2>/dev/null || echo "alpine@sha256:beef")

# Run container referencing the immutable digest directly
docker run --rm $DIGEST uname -a
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                          IMAGES VS CONTAINERS CHEAT SHEET                             |
+---------------------------------------------------------------------------------------+
| Formula:             Image (Static, Read-Only) + UpperDir (RW) = Container (Living)   |
| View Image Layers:   docker history <image>                                           |
| Inspect Metadata:    docker inspect <image_or_container>                              |
| View Changes in RW:  docker diff <container>                                          |
| Purge Container:     docker rm -f <container>                                         |
| Clean Dangling:      docker image prune -f                                            |
| Enforce Security:    docker run --read-only --tmpfs /tmp <image>                      |
| Golden Rule:         Never mutate production images with docker commit;               |
|                      always build reproducibly via Dockerfiles and BuildKit.          |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Open Container Initiative (OCI) Image Format Specification](https://github.com/opencontainers/image-spec)
- [Docker Documentation: Manage Data in Docker](https://docs.docker.com/storage/)
- [Linux Kernel Documentation: Overlay Filesystem Architecture](https://docs.kernel.org/filesystems/overlayfs.html)
- [Docker CLI Command Reference: docker image](https://docs.docker.com/reference/cli/docker/image/)
- [Google Cloud Architecture: Best Practices for Building Containers](https://cloud.google.com/architecture/best-practices-for-building-containers)

---

| [< Previous: 01-docker-vs-vm.md](./01-docker-vs-vm.md) | [Next: 03-docker-architecture.md](./03-docker-architecture.md) > |
| :--- | :--- |
