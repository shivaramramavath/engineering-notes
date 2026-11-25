---
title: "Docker Image Management Commands: pull, images, tag, push, rmi, save, and load"
description: "Mastering Docker image commands: image listing, tag aliasing, registry pushes, layer history inspection, dangling image pruning, and offline tarball archiving."
category: "DevOps / Containers"
tags: ["docker", "images", "commands", "pull", "push", "tag", "rmi", "save", "load", "cli"]
difficulty: "Beginner"
prerequisites: ["01-fundamentals/02-images-and-containers.md", "01-fundamentals/07-registries-and-image-naming.md"]
estimated_read_time: "25 min"
---

# Docker Image Management Commands: pull, images, tag, push, rmi, save, and load

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. Core Image Management Workflow](#2-core-image-management-workflow)
- [3. Deep Dive: `docker pull` and `docker push`](#3-deep-dive-docker-pull-and-docker-push)
- [4. Deep Dive: `docker images` Listing and Filtering](#4-deep-dive-docker-images-listing-and-filtering)
- [5. Deep Dive: `docker tag` and `docker rmi`](#5-deep-dive-docker-tag-and-docker-rmi)
- [6. Deep Dive: Air-Gapped Archiving with `docker save` and `docker load`](#6-deep-dive-air-gapped-archiving-with-docker-save-and-docker-load)
- [7. Practical Diagnostic Demonstrations](#7-practical-diagnostic-demonstrations)
- [8. Best Practices & Production Guidelines](#8-best-practices--production-guidelines)
- [9. Common Pitfalls & Anti-Patterns](#9-common-pitfalls--anti-patterns)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

Images are the currency of the container ecosystem. They represent the compiled, immutable artifacts produced by continuous integration pipelines and distributed across global registries to staging and production clusters.

Managing images efficiently requires deep mastery of the CLI image commands:
- Knowing how `docker pull` optimizes bandwidth by skipping layers already present in the local content store.
- Understanding why `docker rmi` untags an image rather than immediately deleting files if other references exist.
- Leveraging `docker save` and `docker load` to transfer containerized workloads into strictly isolated, air-gapped environments (defense systems, banking mainframes, edge hardware) without internet or registry access.
- Inspecting image lineage with `docker history` to identify bloated intermediate layers.

```
+---------------------------------------------------------------------------------+
|                         IMAGE LIFECYCLE COMMAND SUITE                           |
|                                                                                 |
|  [Remote Registry (Docker Hub / ECR / GHCR)]                                    |
|        ^                                 |                                      |
|        | docker push                     | docker pull                          |
|        |                                 v                                      |
|  +-----+---------------------------------+-----------------------------------+  |
|  | Local Image Content Store (/var/lib/docker/overlay2/ / containerd)        |  |
|  |                                                                           |  |
|  | - docker images / docker image ls (List and filter local images)          |  |
|  | - docker tag                      (Create alias pointer to existing ID)   |  |
|  | - docker history                  (Inspect layer sizes and directives)    |  |
|  | - docker rmi / docker image rm    (Untag or purge unreferenced layers)    |  |
|  +-----+---------------------------------+-----------------------------------+  |
|        |                                 ^                                      |
|        | docker save (tarball export)    | docker load (tarball import)         |
|        v                                 |                                      |
|  [Local Disk Tarball Archive: my-app.tar.gz]                                    |
|  (Portable, air-gapped distribution via USB or secure S3 transfer)              |
+---------------------------------------------------------------------------------+
```

---

## 2. Core Image Management Workflow

1. **Pull**: Retrieve an image from an OCI registry (`docker pull`).
2. **Inspect**: List and inspect locally cached images (`docker images`, `docker history`).
3. **Tag**: Assign a structured semantic version and remote registry target (`docker tag`).
4. **Push**: Upload newly created layers to an authenticated registry (`docker push`).
5. **Archive**: Export to portable tar archives for offline environments (`docker save`/`load`).
6. **Clean**: Reclaim disk space by purging unused or dangling layers (`docker rmi`).

---

## 3. Deep Dive: `docker pull` and `docker push`

### 3.1. Layer Deduplication in Transit
When you execute `docker pull` or `docker push`:
- Docker contacts the registry distribution endpoint and requests the manifest.
- It iterates through each layer's SHA256 blob digest.
- If a layer blob already exists in local storage (or on the remote registry), Docker outputs **`Already exists`** and skips network transfer entirely!
- This content-addressable deduplication allows microservices sharing a common base layer (e.g., Ubuntu or Alpine) to upload and download in seconds.

### Essential Flags:
```bash
# Pull by explicit platform architecture
docker pull --platform linux/arm64 node:20-alpine

# Pull by immutable cryptographic digest
docker pull alpine@sha256:beef1234...

# Pull all tagged variants of a repository
docker pull -a my-org/backend-service

# Quiet pull (prints only image digest, ideal for CI/CD logs)
docker pull -q nginx:alpine
```

---

## 4. Deep Dive: `docker images` Listing and Filtering

The command `docker images` (aliased to `docker image ls`) inspects the local content store.

### Useful Filtering Flags:
```bash
# List all images including intermediate layers
docker images -a

# Show full un-truncated 64-character Image IDs and Digests
docker images --no-trunc --digests

# Filter for dangling images (unreferenced layers)
docker images -f "dangling=true"

# Filter by repository reference substring
docker images -f "reference=*alpine*"

# Filter images created before or since another image
docker images -f "before=my-app:v2"
docker images -f "since=my-app:v1"
```

### Go Template Formatting:
```bash
# Print custom table showing Repository, Tag, and Size
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}"

# Output only image IDs for scripting
docker images -q
```

---

## 5. Deep Dive: `docker tag` and `docker rmi`

### 5.1. The Nature of `docker tag`
`docker tag` does **not** copy files, rebuild images, or create new filesystem layers. It simply creates an additional pointer in Docker's internal tag database referencing the same underlying 64-character Image ID:

```bash
docker tag source_image[:tag] target_image[:tag]
```

### 5.2. Untagging vs Deletion in `docker rmi`
When you execute `docker rmi <name:tag>`:
1. Docker checks how many tag references point to that Image ID.
2. If multiple tags point to the same Image ID, Docker performs an **Untag** operation (`Untagged: ...`), removing only the specified name alias. The physical layer files remain on disk.
3. If the removed tag was the **last remaining reference** pointing to that Image ID, and no running or stopped containers depend on it, Docker performs a **Delete** operation (`Deleted: sha256:...`), physically purging the layer tarballs and OverlayFS directories from the host disk.

```
Tag 1: my-app:latest -----\
                            +----> [Image ID: 7f3a... (Layers 1, 2, 3)]
Tag 2: my-app:v1.0.0 -----/

docker rmi my-app:latest  --> UNTAGS my-app:latest (Layers remain safe!)
docker rmi my-app:v1.0.0  --> DELETES Image ID and purges physical layers!
```

---

## 6. Deep Dive: Air-Gapped Archiving with `docker save` and `docker load`

In high-security enterprise environments (banking, SCADA, defense networks, air-gapped staging servers), servers have zero outbound internet connectivity and cannot reach public or cloud registries.

Docker provides `save` and `load` to stream complete images as raw, uncompressed or gzipped tar archives:

### 6.1. Exporting Images (`docker save`)
```bash
# Save a single image to a tarball
docker save -o nginx-alpine.tar nginx:alpine

# Save multiple images into a single combined tarball!
docker save -o bundle.tar.gz nginx:alpine redis:alpine postgres:16-alpine
```

### 6.2. Importing Images (`docker load`)
```bash
# Load images on the air-gapped server from the tar archive
docker load -i bundle.tar.gz
```
*Advantage*: `docker load` restores all original image layers, tags, history, and metadata with 100% cryptographic fidelity without requiring an intermediate registry.

---

## 7. Practical Diagnostic Demonstrations

### Example 1: Inspecting Image Layer Construction via `docker history`
Analyze the size contributions and Dockerfile instructions of an existing image:

```bash
docker history --no-trunc nginx:alpine
```
*Output reveals every build directive, layer size, and creation timestamp.*

### Example 2: Retagging and Pushing to Local Registry
Demonstrate tag aliasing and pushing to a private registry:

```bash
# Start local registry
docker run -d --name reg-lab -p 5005:5000 registry:2

# Tag existing alpine image
docker tag alpine:latest localhost:5005/lab-alpine:1.0.0

# Push to local registry
docker push localhost:5005/lab-alpine:1.0.0

# Cleanup
docker rm -f reg-lab
```

### Example 3: Simulating Untagging vs Physical Layer Deletion
Prove that removing an alias does not delete physical layers:

```bash
# 1. Pull minimal image
docker pull alpine:latest

# 2. Create alias tag
docker tag alpine:latest test-alias:v1

# 3. Remove alias tag; observe that Docker only outputs "Untagged:"!
docker rmi test-alias:v1
# Output: Untagged: test-alias:v1

# 4. Verify original image remains intact and cached
docker images | grep alpine
```

### Example 4: Creating and Restoring an Air-Gapped Image Tarball
Export an image to disk, purge it from Docker, and restore it from the archive:

```bash
# 1. Save image to compressed tar archive
docker save alpine:latest | gzip > /tmp/alpine-airgap.tar.gz

# 2. Force remove image from Docker cache
docker rmi -f alpine:latest

# 3. Confirm image is gone from local store
docker images | grep alpine || echo "Alpine image successfully purged."

# 4. Restore image from tar archive
docker load < /tmp/alpine-airgap.tar.gz

# 5. Confirm image is restored with tag and layers intact!
docker images | grep alpine

# Cleanup
rm -f /tmp/alpine-airgap.tar.gz
```

### Example 5: Bulk Pruning of Dangling Images
Clean up unreferenced intermediate image layers in a single command:

```bash
docker image prune -f
```

---

## 8. Best Practices & Production Guidelines

1. **Tag with Git Commit SHAs in CI/CD**: In addition to semantic versions (`v1.4.0`), always tag images with the short git commit SHA (`v1.4.0-g8f9a2b`). This ensures instantaneous traceability from production back to the exact line of code.
2. **Never Rely on Untagged Images**: Avoid running containers from raw Image IDs (`docker run 7f3a91...`). Always assign meaningful repository and tag names.
3. **Use Gzip/Zstandard for `docker save`**: `docker save` outputs uncompressed tar archives by default. Always pipe through `gzip` or `zstd` (`docker save <img...> | gzip > file.tar.gz`) to reduce network transfer size by up to 70%.
4. **Regularly Prune Dangling Images**: Incorporate `docker image prune -f` into daily maintenance cron jobs to reclaim disk space from overridden build tags.
5. **Differentiate `docker export` from `docker save`**:
   - `docker save` exports the **complete image**, including all historical layers, configuration, environment variables, and metadata.
   - `docker export` exports only a container's **flat filesystem snapshot**, stripping all image history, metadata, and layer ancestry.

---

## 9. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Proper Solution |
|:---|:---|:---|:---|
| **Confusing `docker export` with `docker save`** | Exporting a container instead of saving an image. | Loses `ENTRYPOINT`, `ENV`, exposed ports, and all layer history. | Use `docker save` to backup images; use `docker export` only for flat filesystem dumps. |
| **Using `docker rmi -f` aggressively** | Forcing deletion of images used by stopped containers. | Leaves stopped containers orphaned without backing layers; corrupts graph driver state. | Remove stopped containers first (`docker rm`), then remove image cleanly. |
| **Accumulating dangling images during local builds** | Rebuilding `my-app:latest` repeatedly without pruning. | Hundreds of `<none>:<none>` layers consume tens of gigabytes of disk space. | Run `docker image prune -f` or build with BuildKit cache mounts. |
| **Pushing huge multi-gigabyte layers unnecessarily** | Installing build tools (gcc, make) in the final image layer. | Slow registry uploads; wastes bandwidth and cloud storage costs. | Use multi-stage builds to discard build tools before pushing. |
| **Pushing unauthenticated images** | Omitting `docker login` before pushing to private registries. | Fails with HTTP 401 Unauthorized or confusing 403 Forbidden. | Run `docker login <registry-domain>` or configure credential helpers. |
| **Attempting to load a container tarball with `docker load`** | Loading a tarball generated by `docker export`. | Fails with `open /var/lib/docker/tmp/.../manifest.json: no such file or directory`. | Use `docker import` for exported containers, `docker load` for saved images. |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between <code>docker save</code> and <code>docker export</code>?</strong></summary>

- **`docker save`** operates on an **image**. It packages the complete image specification: all underlying immutable read-only layer tarballs, the manifest JSON, configuration JSON (entrypoint, cmd, env vars), and layer history. It is restored using `docker load`.
- **`docker export`** operates on a **container**. It takes a single, flat snapshot of the container's current unified filesystem, stripping away all layer history, parent relationships, configuration metadata, and entrypoints. It is restored as a single-layer flat image using `docker import`.
</details>

<details>
<summary><strong>2. What occurs under the hood when you run <code>docker rmi</code> on an image that has multiple tags?</strong></summary>

When an image has multiple tags pointing to the same Image ID, running `docker rmi <tag>` does **not** delete any image layers from disk. Docker simply executes an **Untag** operation: it removes the specified tag reference from its internal local repository map. The physical layer tarballs and OverlayFS directories remain untouched. Only when the *last remaining tag* pointing to that Image ID is removed (and no containers reference it) does Docker delete the physical layers.
</details>

<details>
<summary><strong>3. How does <code>docker push</code> optimize bandwidth when uploading an image to a registry?</strong></summary>

Before uploading layer files, `docker push` queries the registry distribution API with the cryptographic SHA256 content digest of each layer blob. If the registry already possesses that blob (e.g., from an earlier build or a shared base image like `alpine`), the registry returns a positive existence response. Docker marks the layer as **`Layer already exists`** and skips network transmission, uploading only newly modified layers.
</details>

<details>
<summary><strong>4. What is a "dangling" image in Docker, and how does it differ from an "unused" image?</strong></summary>

- A **dangling image** is an image that is untagged (displayed with `<none>:<none>` in `docker images`). It typically occurs when you build an image with a tag that already exists; the new image takes the tag, leaving the old image layers untagged. Cleaned via `docker image prune`.
- An **unused image** is a fully tagged image (e.g., `nginx:alpine`) that is simply not currently referenced by any running or stopped container. Cleaned via `docker image prune -a`.
</details>

<details>
<summary><strong>5. Why is <code>docker tag</code> considered an instantaneous, zero-cost operation?</strong></summary>

`docker tag` does not copy files, compress archives, or touch the filesystem on disk. In Docker's architecture, an image tag is merely a lightweight key-value mapping entry in the local image metadata store associating a human-readable repository/tag string with a 64-character SHA256 Image ID. Creating a tag merely inserts a pointer into memory.
</details>

<details>
<summary><strong>6. How can an administrator inspect the exact command that created each layer of a downloaded image?</strong></summary>

By executing `docker history <image>`. Passing the `--no-trunc` flag ensures that long shell commands and buildkit metadata are printed in full:
```bash
docker history --no-trunc <image>
```
</details>

<details>
<summary><strong>7. What happens if you run <code>docker rmi</code> on an image that is currently used by a stopped container?</strong></summary>

Docker will reject the deletion and output an error:
`Error response from daemon: conflict: unable to delete <image-id> (must be forced) - image is being used by stopped container <container-id>`.
Docker refuses to delete the image because the stopped container's metadata and read-write layer depend on those read-only image layers. To delete the image, the administrator must either remove the stopped container first (`docker rm`) or supply the force flag (`docker rmi -f`), which untags the image while leaving the orphaned layers until the container is removed.
</details>

<details>
<summary><strong>8. How do you save multiple different Docker images into a single tar archive file?</strong></summary>

`docker save` accepts multiple image arguments:
```bash
docker save -o release-bundle.tar nginx:alpine redis:alpine postgres:16-alpine
```
The resulting tarball packages all three images, their shared base layers, and their manifests into a single cohesive distribution bundle.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Layer History of Alpine Image</strong></summary>

**Goal**: Inspect the build directives and size contributions of Alpine Linux.

```bash
docker history alpine:latest
```
</details>

<details>
<summary><strong>Exercise 2: Create Multiple Tags for an Existing Image</strong></summary>

**Goal**: Prove that multiple tags share the exact same underlying Image ID.

```bash
docker tag alpine:latest my-alpine:1.0
docker tag alpine:latest my-alpine:stable
docker tag alpine:latest my-alpine:production

# Verify that all 3 tags have identical IMAGE ID
docker images --filter "reference=my-alpine" --format "table {{.Repository}}\t{{.Tag}}\t{{.ID}}"

# Cleanup
docker rmi my-alpine:1.0 my-alpine:stable my-alpine:production
```
</details>

<details>
<summary><strong>Exercise 3: Filter Images by Creation Age (`before` / `since`)</strong></summary>

**Goal**: Query images created before another reference image.

```bash
docker images --filter "before=alpine:latest"
```
</details>

<details>
<summary><strong>Exercise 4: Test Multi-Image Tarball Archive (`docker save`)</strong></summary>

**Goal**: Package two distinct images into a single compressed tarball and restore them.

```bash
# Pull images
docker pull alpine:latest
docker pull busybox:latest

# Save both into single archive
docker save alpine:latest busybox:latest | gzip > /tmp/dual-bundle.tar.gz

# Inspect archive size
ls -lh /tmp/dual-bundle.tar.gz

# Cleanup archive
rm -f /tmp/dual-bundle.tar.gz
```
</details>

<details>
<summary><strong>Exercise 5: Inspect Uncompressed Image Sizes with Go Templates</strong></summary>

**Goal**: Format image output to sort and display raw size numbers.

```bash
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}"
```
</details>

<details>
<summary><strong>Exercise 6: Test Pruning of Unused Images (`docker image prune -a`)</strong></summary>

**Goal**: Reclaim disk space by purging all images not currently referenced by active containers.

```bash
# Test dry run / prompt of image prune
docker image prune --help
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                         IMAGE MANAGEMENT COMMAND CHEAT SHEET                          |
+---------------------------------------------------------------------------------------+
| Pull Image:            docker pull <image>:<tag> / <image>@sha256:<digest>            |
| List Images:           docker images / docker image ls                                |
| List with Digests:     docker images --digests                                        |
| Filter Dangling:       docker images -f "dangling=true"                               |
| Inspect Layers:        docker history --no-trunc <image>                              |
| Tag Image:             docker tag <source>:<tag> <target>:<tag>                       |
| Push Image:            docker push <target>:<tag>                                     |
| Untag / Delete Image:  docker rmi <image>:<tag>                                       |
| Save to Tarball:       docker save <image> | gzip > <archive>.tar.gz                  |
| Load from Tarball:     docker load < <archive>.tar.gz                                 |
| Clean Dangling:        docker image prune -f                                          |
| Clean All Unused:      docker image prune -af                                         |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Docker CLI Command Reference: docker image](https://docs.docker.com/reference/cli/docker/image/)
- [Docker Documentation: docker save Reference](https://docs.docker.com/reference/cli/docker/image/save/)
- [Docker Documentation: docker load Reference](https://docs.docker.com/reference/cli/docker/image/load/)
- [Docker Documentation: docker history Reference](https://docs.docker.com/reference/cli/docker/image/history/)
- [Open Container Initiative (OCI) Image Specification Format](https://github.com/opencontainers/image-spec)

---

| [< Previous: 02-logs-exec-inspect.md](./02-logs-exec-inspect.md) | [Next: 04-build.md](./04-build.md) > |
| :--- | :--- |
