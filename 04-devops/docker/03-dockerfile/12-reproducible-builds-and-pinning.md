# Reproducible Builds and Supply Chain Pinning

```
Track: DevOps & Containerization
Module: 03-dockerfile
Prerequisites: 03-dockerfile/01-dockerfile-basics.md, 03-dockerfile/08-base-image-choice.md
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
4. [Syntax & Implementation Reference](#4-syntax--implementation-reference)
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

In modern software supply chain security, **reproducibility** means that compiling the exact same source code with the exact same inputs at two different points in time (or on two completely separate machines) yields bit-for-bit identical cryptographic container image digests.

Most Dockerfiles in the wild are non-deterministic:
- **Mutable Base Tags**: Building `FROM alpine:3.21` today downloads a different base OS image than building it six months from now after Alpine releases security updates.
- **Unpinned External Dependencies**: `pip install fastapi` or `apt-get install -y curl` downloads whatever the mirror serves at that exact millisecond.
- **Non-Deterministic File Timestamps & UIDs**: Git does not store file creation timestamps. Building on different machines creates different file modification timestamps (`mtime`), which changes the tar layer hash and produces completely different image SHA256 hashes.
- **Dynamic Timestamps in Toolchains**: Compilers and package managers often embed build timestamps into compiled binaries.

Achieving hermetic, reproducible container builds is foundational for **SLSA (Supply-chain Levels for Software Artifacts) Level 3+ compliance**, ensuring zero unvetted code enters your production clusters.

---

## 2. Core Concepts

### Cryptographic Digest vs Mutable Tag
- **Tag (`node:22-slim`)**: A human-readable pointer. It is completely mutable; registry administrators or automated CI bots can overwrite it with new binaries at any moment.
- **Digest (`node:22-slim@sha256:4a8b...`)**: An immutable cryptographic SHA256 hash of the image manifest. Once pushed, it can never be altered. If an upstream maintainer updates `node:22-slim`, your Dockerfile continues to pull the exact byte-identical base image.

### `SOURCE_DATE_EPOCH` Specification
The standardized Unix timestamp specification ([reproducible-builds.org](https://reproducible-builds.org/specs/source-date-epoch/)). BuildKit, compilers (GCC, Rust, Go), and archive utilities read `SOURCE_DATE_EPOCH` from the environment and clamp file timestamps to that exact second rather than using the host's current wall-clock time (`time()`).

### BuildKit Determinism Controls
Docker BuildKit (v0.11+) natively supports:
- Stripping non-deterministic metadata.
- Setting explicit output timestamps via `--opt build-arg:SOURCE_DATE_EPOCH=...` or export flags.
- Hermetic network access isolation (`--network none` during build steps).

---

## 3. How It Works Internally

### Image Layer Hash Calculation Pipeline

```
+-------------------------------------------------------------+
| Source Files (main.go, config.json)                         |
+------------------------------+------------------------------+
                               |
                               v
+-------------------------------------------------------------+
| Tar Archive Creation                                        |
|  - File Contents                                            |
|  - File Permissions (chmod 644 vs 755)                      |
|  - File Ownership (UID / GID)                               |
|  - File Timestamps (mtime / atime)  <-- MAJOR VARIABILITY!  |
+------------------------------+------------------------------+
                               |
                               v
+-------------------------------------------------------------+
| SHA256 Hash of Compressed Layer (DiffID)                    |
+------------------------------+------------------------------+
                               |
                               v
+-------------------------------------------------------------+
| Image Manifest SHA256 Digest                                |
|  - Manifest points to ordered list of Layer DiffIDs         |
|  - Any single bit change in ANY file or mtime changes       |
|    the entire final Image Digest!                           |
+-------------------------------------------------------------+
```

---

## 4. Syntax & Implementation Reference

### Base Image Digest Pinning Syntax

```dockerfile
# Option 1: Mutable tag (Non-reproducible)
FROM debian:bookworm-slim

# Option 2: Immutable Digest Pinning (Deterministic & SLSA Compliant)
# Form: <image>:<tag>@sha256:<64-character-hex-digest>
FROM debian:bookworm-slim@sha256:d6b9f4a132e013bf69b50bc6a12b...
```

### BuildKit Timestamp Normalization via Build CLI

```bash
# Obtain current Git commit timestamp
SOURCE_DATE_EPOCH=$(git log -1 --pretty=%ct)

# Pass timestamp to BuildKit to clamp all layer timestamps
docker buildx build \
  --build-arg SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH}" \
  --output type=image,name=my-app:v1.0.0,rewrite-timestamp=true \
  .
```

---

## 5. Practical Implementation & Examples

### Example 1: Fully Hermetic & Deterministic Multi-Stage Dockerfile

```dockerfile
# syntax=docker/dockerfile:1.7

# -------------------------------------------------------------
# 1. Base Image Pinned by Immutable Cryptographic SHA256 Digest
# -------------------------------------------------------------
FROM golang:1.24.0-alpine3.21@sha256:2d40d43f76686adb228048d95ab12b27549761ac084810ff47f86b31693c2f09 AS builder

# Enforce deterministic build timestamp via build argument
ARG SOURCE_DATE_EPOCH
ENV SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-1700000000}

WORKDIR /src

# 2. Strict Lockfile Pinning
COPY go.mod go.sum ./
RUN go mod download && go mod verify

COPY . .

# 3. Strip compiler timestamps, build paths, and debug symbols
# -trimpath strips host file system paths (/home/runner/work/...)
RUN CGO_ENABLED=0 GOOS=linux GOARCH=amd64 \
    go build -trimpath \
    -ldflags="-s -w -buildid= -extldflags '-static'" \
    -o /bin/api-server .

# -------------------------------------------------------------
# 4. Scratch Runtime Pinned
# -------------------------------------------------------------
FROM scratch AS runtime

COPY --from=builder /bin/api-server /api-server

USER 10001:10001
ENTRYPOINT ["/api-server"]
```

### Example 2: Strict Package Pinning across Package Managers

**Debian (`apt-get`)**:
```dockerfile
# Pin package version explicitly
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl=8.5.0-2ubuntu10.6 \
    ca-certificates=20240203 \
 && rm -rf /var/lib/apt/lists/*
```

**Alpine (`apk`)**:
```dockerfile
# Pin package version explicitly
RUN apk add --no-cache \
    curl=8.11.1-r0 \
    ca-certificates=20241121-r1
```

**Python (`pip`)**:
```bash
# Pin exact wheel hash using requirements.txt hash-checking mode
fastapi==0.115.6 \
    --hash=sha256:1a84f479bb3ec41... \
    --hash=sha256:9cb819f03d92f78...
uvicorn==0.34.0 \
    --hash=sha256:b1d830b1...
```

**Node.js (`npm`)**:
```dockerfile
# Enforce strictly package-lock.json with integrity SHA512 checks
RUN npm ci --ignore-scripts
```

### Example 3: BuildKit Network Sandboxing for Hermetic Builds
Preventing network access during compilation ensures no external dependencies can be silently fetched outside declared lockfiles.

```dockerfile
# Stage 1: Download dependencies (Network permitted)
FROM node:22-bookworm-slim AS deps
WORKDIR /app
COPY package*.json ./
RUN npm ci

# Stage 2: Compilation (Network completely disabled!)
FROM node:22-bookworm-slim AS build
WORKDIR /app
COPY --from=deps /app/node_modules ./node_modules
COPY . .
# --network=none prevents tsc from making any outbound internet calls
RUN --network=none npm run build
```

---

## 6. Production Best Practices & Antipatterns

### Best Practices
- **Pin Image Digests in Production Pipelines**: Tags like `node:22` or `python:3.12-slim` should be restricted to local prototyping. Production Dockerfiles must specify `@sha256:...`.
- **Automate Digest Updates via Renovate or Dependabot**: Digest pinning manually causes image stagnation. Use automated bots that open weekly pull requests updating digests alongside vulnerability scans.
- **Pass `-trimpath` to Go and Rust Compilers**: Strips absolute directory paths (e.g., `/Users/dev/project` or `/home/runner`) from compiled binaries, ensuring identical binary hashes across different machines.
- **Normalize File Permissions and Timestamps**: When copying files into images, use BuildKit flags or `find . -exec touch -t ...` to establish uniform timestamps.

### Anti-Patterns
- **Relying Solely on Semantic Tags (`v1.2.3`)**: Docker Hub and third-party registries permit authors to re-push over existing semantic tags without warning.
- **Using Unlocked `pip install -r requirements.txt`**: Without `pip-tools` or `poetry.lock`, sub-dependencies resolve to latest available versions, breaking builds unexpectedly.
- **Embedding `date +%s` or Git Commit Hashes into Binaries**: Stamping dynamic compile timestamps into binary headers permanently breaks bit-for-bit reproducibility.

---

## 7. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| Identical Git commit produces different Image SHA on two developer laptops | File modification timestamps (`mtime`) differ across machines | Compare `docker inspect --format='{{json .RootFS.Layers}}'` | Pass `SOURCE_DATE_EPOCH` and use BuildKit timestamp rewriting. |
| `apt-get install` fails with `Version not found` after pinning | Upstream mirror dropped older patch release after a security update | `apt-cache madison <pkg>` inside image | Use snapshot debian mirrors (`snapshot.debian.org`) or pin to minor wildcard `8.5.*`. |
| Go binary hash differs between macOS and Linux build machines | Binary paths embedded into debug tables | Run `strings /bin/binary \| grep "/Users/"` | Add `-trimpath` flag to `go build`. |
| `npm ci` fails with `checksum failed` | `package-lock.json` integrity hash does not match registry response | Inspect npm log | Regenerate lockfile cleanly from official npm registry. |
| Renovate bot fails to parse `@sha256:` digest | Digest format syntax error or missing original tag prefix | Inspect regex in Renovate logs | Use standard convention `image:tag@sha256:<hash>`. |
| Multi-arch build produces divergent digest on same arch | Host kernel architecture leak or dynamic endianness checks in build scripts | Inspect BuildKit manifest list | Isolate architecture-specific logic using Docker build arguments `TARGETARCH`. |

---

## 8. Performance Tuning & Resource Optimization

1. **Deterministic Layer Caching**:
   - When base images and packages are pinned by digest and exact versions, Docker's build cache hit rate approaches 100% across all developer machines and CI runners. Builds that previously took minutes complete in milliseconds because cache keys remain completely stable.
2. **Registry Bandwidth Savings**:
   - Because layer DiffIDs do not change when timestamps and files are normalized, container registries store layers only once, saving terabytes of network transfer and registry storage costs across large engineering teams.

---

## 9. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK: Supply Chain Substitution / Registry Compromise      |
+--------------------------------------------------------------+
| Scenario: Malicious actor compromises upstream base tag      |
|           `node:22-bookworm-slim` on public Docker Hub.      |
+--------------------------------------------------------------+
| If using mutable tags:                                       |
|  - Your CI pulls compromised layer on next build.            |
|  - Malicious backdoor deployed to production.                |
|  - OUTCOME: TOTAL SYSTEM COMPROMISE.                         |
+--------------------------------------------------------------+
| If using SHA256 digest pinning:                              |
|  - Your CI requests exact hash @sha256:4a8b...               |
|  - Registry must serve original uncompromised bytes.         |
|  - If registry attempts to swap contents, hash fails!        |
|  - OUTCOME: ATTACK COMPLETELY THWARTED.                      |
+--------------------------------------------------------------+
```

---

## 10. Architectural Comparison Table

| Strategy | Reproducibility Level | Maintenance Overhead | Security Level | SLSA Level |
|:---|:---|:---|:---|:---|
| **Floating Tags (`:latest`, `:22`)** | 0% (Non-reproducible) | Zero | Dangerously Low | None |
| **Semantic Tags (`:22.4.1`)** | ~40% (Semi-stable) | Low | Medium | Level 1 |
| **Digest Pinning (`@sha256:...`)** | ~85% (Pinned base) | Medium (Needs Renovate) | High | Level 2 |
| **Hermetic Builds + `SOURCE_DATE_EPOCH`** | **100% (Bit-for-bit identical)**| High | Maximum | **Level 3+** |

---

## 11. Real-World Scenario Walkthrough

### Scenario: Resolving a Disastrous "Works on My Machine" Production Outage

#### Incident
A microservice failed in production after a routine deployment, throwing `Fatal: Incompatible openssl library version`. The engineer's local build worked perfectly. Git history showed zero code changes in the last 48 hours.

#### Root Cause Analysis
The Dockerfile contained:
```dockerfile
FROM alpine:3.21
RUN apk add --no-cache openssl
```
Between the engineer's local build on Tuesday and the CI deployment on Thursday, Alpine published a new minor revision of OpenSSL that introduced a breaking ABI change with the application's legacy native bindings. Because neither the Alpine base image nor the `apk` package was pinned, the production build silently pulled the new version.

#### Remediation
1. Pinned Alpine base to immutable SHA256 digest.
2. Pinned exact package release: `openssl=3.3.2-r1`.
3. Integrated Renovate Bot to automate controlled version bumps via Pull Requests with full test coverage.

---

## 12. Self-Assessment & Interview Questions

<details>
<summary><strong>1. What is the difference between an image tag and an image digest in Docker?</strong></summary>

**Answer:**
An image tag (e.g., `redis:7-alpine`) is a mutable label that can be updated to point to different image manifests over time. An image digest (e.g., `redis:7-alpine@sha256:323b...`) is an immutable cryptographic hash of the image's manifest. Pulling by digest guarantees byte-for-byte identity regardless of when or where the pull occurs.
</details>

<details>
<summary><strong>2. What is `SOURCE_DATE_EPOCH`, and why is it essential for reproducible container builds?</strong></summary>

**Answer:**
`SOURCE_DATE_EPOCH` is a standardized environment variable representing a Unix timestamp (typically the timestamp of the last Git commit). Compilers, tar archivers, and Docker BuildKit use this variable to clamp file modification timestamps (`mtime`) to a deterministic point in time rather than the host system's current clock. Since tar layer hashes factor in file timestamps, fixing the timestamp allows identical source files to generate identical layer hashes.
</details>

<details>
<summary><strong>3. How does the `-trimpath` flag in Go improve build reproducibility?</strong></summary>

**Answer:**
By default, the Go compiler embeds absolute host file system paths (e.g., `/home/runner/work/app/main.go` or `/Users/alice/app/main.go`) into stack trace tables and debugging symbols. When built on different machines, these absolute paths differ, producing different binary hashes. `-trimpath` removes all host file system prefixes from the compiled binary, making the output binary hash completely independent of the build host's directory structure.
</details>

<details>
<summary><strong>4. How do you find the SHA256 digest of an image that is currently on your local machine or registry?</strong></summary>

**Answer:**
Using `docker inspect`:
```bash
docker inspect --format='{{index .RepoDigests 0}}' <image_name>:<tag>
```
Or querying the registry directly using `skopeo inspect docker://<image>:<tag>`.
</details>

<details>
<summary><strong>5. Why is `--network none` useful in multi-stage Docker builds?</strong></summary>

**Answer:**
In multi-stage builds, the compilation stage (e.g., `go build`, `cargo build`, `npm run build`) should ideally operate in a completely hermetic sandbox where all dependencies have already been pre-fetched into a local directory or cache. Running `RUN --network=none <compile-command>` physically blocks the container process from initiating outbound network connections, guaranteeing that the compiler cannot download unverified third-party code at compile time.
</details>

<details>
<summary><strong>6. What is the operational downside of pinning base images by digest, and how do engineering teams mitigate it?</strong></summary>

**Answer:**
The primary downside is **image stagnation**: because the digest never changes, the container will never automatically receive upstream security patches (e.g., critical Linux kernel or OpenSSL CVE fixes). Teams mitigate this by utilizing automated dependency bots like **Renovate** or **Dependabot**, which monitor registries for new digests, run automated security scans, and automatically submit Pull Requests to update the digests.
</details>

<details>
<summary><strong>7. How does BuildKit's `rewrite-timestamp=true` output option work?</strong></summary>

**Answer:**
Introduced in BuildKit v0.11+, the `rewrite-timestamp=true` option rewrites all file modification timestamps within created image layers to match `SOURCE_DATE_EPOCH` (or a default Unix epoch). This ensures that even if local files were created at different times on different developer machines, the final tar archive layers match byte-for-byte.
</details>

<details>
<summary><strong>8. What is SLSA, and at what level is reproducible container building required?</strong></summary>

**Answer:**
SLSA (Supply-chain Levels for Software Artifacts) is a security framework created by Google to prevent software tampering. SLSA Level 3 requires authenticated source provenance, hermetic build environments, and isolated build processes. Achieving SLSA Level 4 requires bit-for-bit reproducible builds verified by two independent build systems.
</details>

---

## 13. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Extracting and Locking an Immutable Base Image Digest</strong></summary>

**Goal:** Pull an image, extract its cryptographic SHA256 digest, and author a pinned Dockerfile.

**Step-by-step Solution:**
1. Pull standard alpine:
   ```bash
   docker pull alpine:3.21
   ```
2. Extract digest:
   ```bash
   DIGEST=$(docker inspect --format='{{index .RepoDigests 0}}' alpine:3.21)
   echo "Pinned target: ${DIGEST}"
   ```
3. Create `Dockerfile`:
   ```dockerfile
   FROM ${DIGEST}
   CMD ["echo", "Reproducible base locked!"]
   ```
4. Verify build succeeds.
</details>

<details>
<summary><strong>Exercise 2: Verifying Go Binary Determinism with `-trimpath`</strong></summary>

**Goal:** Build a Go binary in two completely different directory paths and verify their SHA256 hashes are identical.

**Step-by-step Solution:**
1. Create identical `main.go` in `/tmp/dir_a` and `/tmp/dir_b`.
2. Compile in directory A:
   ```bash
   cd /tmp/dir_a
   CGO_ENABLED=0 go build -trimpath -ldflags="-s -w -buildid=" -o app_a main.go
   ```
3. Compile in directory B:
   ```bash
   cd /tmp/dir_b
   CGO_ENABLED=0 go build -trimpath -ldflags="-s -w -buildid=" -o app_b main.go
   ```
4. Compare hashes:
   ```bash
   sha256sum /tmp/dir_a/app_a /tmp/dir_b/app_b
   # Hashes match exactly!
   ```
</details>

<details>
<summary><strong>Exercise 3: Building Hermetically with Network Isolation</strong></summary>

**Goal:** Configure a multi-stage build that disables network access during the build phase.

**Step-by-step Solution:**
1. Write Dockerfile:
   ```dockerfile
   # syntax=docker/dockerfile:1.7
   FROM alpine:3.21 AS builder
   WORKDIR /app
   RUN echo "console.log('offline build')" > build.js
   # Attempting curl here will fail because network is disabled
   RUN --network=none node -v || echo "Built offline"
   ```
2. Build with BuildKit enabled:
   ```bash
   DOCKER_BUILDKIT=1 docker build .
   ```
</details>

<details>
<summary><strong>Exercise 4: Normalizing Build Timestamps via SOURCE_DATE_EPOCH</strong></summary>

**Goal:** Pass an explicit epoch to BuildKit and verify layer creation timestamps.

**Step-by-step Solution:**
1. Build with BuildKit argument:
   ```bash
   docker buildx build \
     --build-arg SOURCE_DATE_EPOCH=1700000000 \
     -t timestamp-test .
   ```
2. Inspect image history:
   ```bash
   docker inspect --format='{{json .Created}}' timestamp-test
   ```
</details>

<details>
<summary><strong>Exercise 5: Hash Checking with Python Pip</strong></summary>

**Goal:** Generate a hash-checked `requirements.txt` using `pip-tools` or `pip hash`.

**Step-by-step Solution:**
1. Download a package wheel:
   ```bash
   pip download --no-deps certifi==2024.12.14
   ```
2. Compute hash:
   ```bash
   pip hash certifi-2024.12.14-py3-none-any.whl
   ```
3. Add to `requirements.txt`:
   ```text
   certifi==2024.12.14 --hash=sha256:...
   ```
4. Run `pip install --require-hashes -r requirements.txt`.
</details>

<details>
<summary><strong>Exercise 6: Configuring Renovate for Base Image Digests</strong></summary>

**Goal:** Author a `renovate.json` configuration that automatically updates pinned Docker digests.

**Step-by-step Solution:**
1. Create `renovate.json`:
   ```json
   {
     "$schema": "https://docs.renovatebot.com/renovate-schema.json",
     "extends": ["config:recommended"],
     "dockerfile": {
       "pinDigests": true
     },
     "packageRules": [
       {
         "matchDatasources": ["docker"],
         "automerge": false
       }
     ]
   }
   ```
</details>

---

## 14. Summary & Reference Cheat Sheet

### Golden Rules of Supply Chain Pinning
1. **Always pin base images by digest**: `image:tag@sha256:<hash>`.
2. **Always use strict lockfiles**: `package-lock.json`, `poetry.lock`, `go.sum`, `Cargo.lock`.
3. **Pass `-trimpath`** to Go/Rust compilers to eliminate host path discrepancies.
4. **Enforce `SOURCE_DATE_EPOCH`** to normalize file metadata timestamps.
5. **Pair digest pinning with Renovate/Dependabot** to continuously receive security updates.

---

[← Previous: 11-dockerfile-linting-with-hadolint.md](11-dockerfile-linting-with-hadolint.md) | [Next: Module 04 Storage & Networking →](../04-volumes-and-networking/README.md)
