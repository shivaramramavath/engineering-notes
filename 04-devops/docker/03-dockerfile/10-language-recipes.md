# Production Language Recipes: Node.js, Python, Go, Java, and Rust

```
Track: DevOps & Containerization
Module: 03-dockerfile
Prerequisites: 03-dockerfile/04-multi-stage-builds.md, 03-dockerfile/08-base-image-choice.md
Difficulty: Advanced
Estimated Duration: 50 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Universal Production Directives Checklist](#4-universal-production-directives-checklist)
5. [Production Recipes](#5-production-recipes)
   - [5.1 Node.js / TypeScript (Distroless)](#51-nodejs--typescript-distroless)
   - [5.2 Python / FastAPI (Debian Slim)](#52-python--fastapi-debian-slim)
   - [5.3 Go Microservice (Zero-Byte Scratch)](#53-go-microservice-zero-byte-scratch)
   - [5.4 Java Spring Boot (Layer Tools & JRE)](#54-java-spring-boot-layer-tools--jre)
   - [5.5 Rust High-Performance Binary (Cargo Chef)](#55-rust-high-performance-binary-cargo-chef)
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

Containerizing enterprise applications across multiple programming languages requires stack-specific optimizations. A generic `FROM node` or `FROM python` Dockerfile copied from basic tutorials introduces severe production vulnerabilities:
- **Massive Attack Surface**: Shipping compilers (`gcc`, `cargo`, `tsc`, `javac`), package managers, and development dependencies into runtime images.
- **Cache Invalidation Storms**: Rebuilding entire dependency trees (`node_modules`, `.m2`, `cargo target`, `venv`) on every trivial code change.
- **Unoptimized Process Execution**: Running Node or Python as PID 1 without init shims, ignoring JVM container memory limits, or failing to statically link native binaries.

This guide provides battle-tested, enterprise-grade Dockerfile templates for the five predominant server-side ecosystems: Node.js/TypeScript, Python/FastAPI, Go, Java/Spring Boot, and Rust.

---

## 2. Core Concepts

### Universal Pipeline Model

Regardless of language, a production container build pipeline follows a 3-tier stage pattern:

```
+-------------------------------------------------------------+
| Stage 1: Dependency Resolver                                |
|  - Copies dependency manifests only (package.json,          |
|    requirements.txt, go.mod, pom.xml, Cargo.toml)           |
|  - Downloads and caches external libraries                  |
+------------------------------+------------------------------+
                               |
                               v
+-------------------------------------------------------------+
| Stage 2: Compiler / Builder                                 |
|  - Copies application source code                           |
|  - Runs compilation (tsc, go build, mvn package, cargo)     |
|  - Generates compiled artifacts / prunes dev dependencies   |
+------------------------------+------------------------------+
                               |
                               v
+-------------------------------------------------------------+
| Stage 3: Minimal Runtime                                    |
|  - Uses scratch, Distroless, or Slim base                   |
|  - Imports ONLY final runtime artifact                      |
|  - Enforces unprivileged USER (nonroot)                     |
|  - Contains 0 compilers, 0 package managers, 0 shells       |
+-------------------------------------------------------------+
```

---

## 3. How It Works Internally

### Layer Cache Strategy Per Language

| Ecosystem | Manifest File(s) | Lockfile | Cache Isolation Mechanism |
|:---|:---|:---|:---|
| **Node.js** | `package.json` | `package-lock.json` | `RUN --mount=type=cache,target=/root/.npm npm ci` |
| **Python** | `requirements.txt` / `pyproject.toml`| `poetry.lock` / `uv.lock` | `RUN --mount=type=cache,target=/root/.cache/pip pip install` |
| **Go** | `go.mod` | `go.sum` | `RUN --mount=type=cache,target=/go/pkg/mod go mod download` |
| **Java** | `pom.xml` / `build.gradle` | `gradle.lockfile` | `RUN --mount=type=cache,target=/root/.m2 mvn dependency:go-offline`|
| **Rust** | `Cargo.toml` | `Cargo.lock` | `cargo-chef` recipe preparation stage |

---

## 4. Universal Production Directives Checklist

Every production Dockerfile must satisfy these 6 requirements:
1. `syntax=docker/dockerfile:1.7` enabled for modern BuildKit cache mounting.
2. Build stages strictly isolated from final runtime layers.
3. Explicit `USER` directive declaring a non-root UID (e.g., `10001` or `nonroot`).
4. Exec-form `ENTRYPOINT` or `CMD` using JSON arrays (`["binary", "arg"]`).
5. `.dockerignore` configured to strip `.git`, `node_modules`, `tests`, `target`, and local artifacts.
6. Container memory/CPU flags configured for language runtimes (e.g., `-XX:MaxRAMPercentage`, `UV_THREADPOOL_SIZE`).

---

## 5. Production Recipes

### 5.1 Node.js / TypeScript (Distroless)

```dockerfile
# syntax=docker/dockerfile:1.7

# -------------------------------------------------------------
# Stage 1: Dependencies & Build
# -------------------------------------------------------------
FROM node:22-bookworm-slim AS builder

WORKDIR /app

# Copy dependency manifests first to leverage Docker layer cache
COPY package.json package-lock.json ./

# Install all dependencies (including devDependencies for tsc)
RUN --mount=type=cache,target=/root/.npm \
    npm ci

# Copy source code and config
COPY tsconfig.json ./
COPY src ./src

# Compile TypeScript to JavaScript in /app/dist
RUN npm run build

# Prune devDependencies to keep only production modules
RUN npm prune --omit=dev

# -------------------------------------------------------------
# Stage 2: Minimal Distroless Runtime
# -------------------------------------------------------------
FROM gcr.io/distroless/nodejs22-debian12:nonroot AS runtime

WORKDIR /app

# Copy production artifacts with nonroot ownership (UID 65532)
COPY --from=builder --chown=nonroot:nonroot /app/package.json ./
COPY --from=builder --chown=nonroot:nonroot /app/node_modules ./node_modules
COPY --from=builder --chown=nonroot:nonroot /app/dist ./dist

ENV NODE_ENV=production \
    PORT=3000

USER nonroot
EXPOSE 3000

CMD ["dist/main.js"]
```

---

### 5.2 Python / FastAPI (Debian Slim)

```dockerfile
# syntax=docker/dockerfile:1.7

# -------------------------------------------------------------
# Stage 1: Build Wheels & Virtualenv
# -------------------------------------------------------------
FROM python:3.12-slim-bookworm AS builder

WORKDIR /app

# Install compilation build dependencies if native C wheels are required
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
 && rm -rf /var/lib/apt/lists/*

# Create isolated virtual environment
RUN python -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

COPY requirements.txt .

RUN --mount=type=cache,target=/root/.cache/pip \
    pip install --no-cache-dir --upgrade pip setuptools wheel && \
    pip install --no-cache-dir -r requirements.txt

# -------------------------------------------------------------
# Stage 2: Minimal Runtime
# -------------------------------------------------------------
FROM python:3.12-slim-bookworm AS runtime

WORKDIR /app

# Create unprivileged user
RUN groupadd -g 10001 appgroup && \
    useradd -u 10001 -g appgroup -s /sbin/nologin -d /app appuser

# Copy virtualenv from builder
COPY --from=builder --chown=appuser:appgroup /opt/venv /opt/venv

# Configure Python execution environment
ENV PATH="/opt/venv/bin:$PATH" \
    PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    PORT=8000

COPY --chown=appuser:appgroup src/ ./src/

USER appuser
EXPOSE 8000

HEALTHCHECK --interval=15s --timeout=3s --start-period=5s --retries=3 \
    CMD python3 -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8000/healthz')" || exit 1

ENTRYPOINT ["uvicorn", "src.main:app", "--host", "0.0.0.0", "--port", "8000", "--workers", "2"]
```

---

### 5.3 Go Microservice (Zero-Byte Scratch)

```dockerfile
# syntax=docker/dockerfile:1.7

# -------------------------------------------------------------
# Stage 1: Compile Statically
# -------------------------------------------------------------
FROM golang:1.24-alpine AS builder

WORKDIR /src

# Install CA certificates and timezone database for scratch runtime
RUN apk add --no-cache ca-certificates tzdata

COPY go.mod go.sum ./
RUN --mount=type=cache,target=/go/pkg/mod \
    go mod download

COPY . .

# Statically compile Go binary (CGO disabled, strip debug symbols)
RUN --mount=type=cache,target=/go/pkg/mod \
    --mount=type=cache,target=/root/.cache/go-build \
    CGO_ENABLED=0 GOOS=linux GOARCH=amd64 \
    go build -a -installsuffix cgo \
    -ldflags="-s -w -extldflags '-static'" \
    -o /bin/server ./cmd/server

# Create minimal /etc/passwd for non-root execution
RUN echo "appuser:x:10001:10001:App User:/:/sbin/nologin" > /etc/passwd.scratch

# -------------------------------------------------------------
# Stage 2: Pure Scratch Runtime (0MB base)
# -------------------------------------------------------------
FROM scratch AS runtime

# Copy system certificates, timezones, and user record
COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/
COPY --from=builder /usr/share/zoneinfo /usr/share/zoneinfo
COPY --from=builder /etc/passwd.scratch /etc/passwd

# Copy statically compiled binary
COPY --from=builder /bin/server /server

USER 10001:10001
EXPOSE 8080

ENTRYPOINT ["/server"]
```

---

### 5.4 Java Spring Boot (Layer Tools & JRE)

Spring Boot's `layertools` splits a fat JAR into 4 distinct layers: dependencies, spring-boot-loader, snapshot-dependencies, and application code. Changing 1 line of Java code rebuilds ONLY the tiny application layer instead of the entire 100MB JAR.

```dockerfile
# syntax=docker/dockerfile:1.7

# -------------------------------------------------------------
# Stage 1: Build JAR via Maven
# -------------------------------------------------------------
FROM maven:3.9-eclipse-temurin-21-alpine AS builder

WORKDIR /build
COPY pom.xml .

RUN --mount=type=cache,target=/root/.m2 \
    mvn dependency:go-offline -B

COPY src ./src
RUN --mount=type=cache,target=/root/.m2 \
    mvn clean package -DskipTests -B

# -------------------------------------------------------------
# Stage 2: Extract Spring Boot Layertools
# -------------------------------------------------------------
FROM eclipse-temurin:21-jre-alpine AS extractor

WORKDIR /extract
COPY --from=builder /build/target/*.jar app.jar
RUN java -Djarmode=layertools -jar app.jar extract

# -------------------------------------------------------------
# Stage 3: Minimal Production JRE Runtime
# -------------------------------------------------------------
FROM eclipse-temurin:21-jre-alpine AS runtime

WORKDIR /app

RUN addgroup -S appgroup && adduser -S appuser -G appgroup

# Copy layers in order of least frequent change to most frequent change
COPY --from=extractor --chown=appuser:appgroup /extract/dependencies/ ./
COPY --from=extractor --chown=appuser:appgroup /extract/spring-boot-loader/ ./
COPY --from=extractor --chown=appuser:appgroup /extract/snapshot-dependencies/ ./
COPY --from=extractor --chown=appuser:appgroup /extract/application/ ./

# Tune JVM container ergonomics
ENV JAVA_TOOL_OPTIONS="-XX:MaxRAMPercentage=75.0 -XX:+UseG1GC -XX:+ExitOnOutOfMemoryError"

USER appuser
EXPOSE 8080

ENTRYPOINT ["java", "org.springframework.boot.loader.launch.JarLauncher"]
```

---

### 5.5 Rust High-Performance Binary (Cargo Chef)

Rust builds suffer from prolonged compilation times because changing `src/main.rs` invalidates dependency caching in standard Dockerfiles. `cargo-chef` analyzes `Cargo.lock` and builds dependencies into a reusable layer cache.

```dockerfile
# syntax=docker/dockerfile:1.7

# -------------------------------------------------------------
# Stage 1: Cargo Chef Planner
# -------------------------------------------------------------
FROM lukemathwalker/cargo-chef:latest-rust-1.84-alpine AS chef
WORKDIR /app

FROM chef AS planner
COPY . .
RUN cargo chef prepare --recipe-path recipe.json

# -------------------------------------------------------------
# Stage 2: Cargo Chef Builder
# -------------------------------------------------------------
FROM chef AS builder
WORKDIR /app
COPY --from=planner /app/recipe.json recipe.json

# Cache and build dependencies only
RUN --mount=type=cache,target=/usr/local/cargo/registry \
    --mount=type=cache,target=/usr/local/cargo/git \
    cargo chef cook --release --target x86_64-unknown-linux-musl --recipe-path recipe.json

# Copy application source and build actual binary
COPY . .
RUN --mount=type=cache,target=/usr/local/cargo/registry \
    --mount=type=cache,target=/usr/local/cargo/git \
    cargo build --release --target x86_64-unknown-linux-musl --bin api-service

# -------------------------------------------------------------
# Stage 3: Scratch Runtime
# -------------------------------------------------------------
FROM scratch AS runtime

COPY --from=builder /app/target/x86_64-unknown-linux-musl/release/api-service /api-service
USER 10001:10001
EXPOSE 8080

ENTRYPOINT ["/api-service"]
```

---

## 6. Production Best Practices & Antipatterns

### Best Practices
- **Separate Build Dependencies from Runtime**: Compilers (`cargo`, `tsc`, `javac`) must never be present in production runtime images.
- **Run as Unprivileged UID**: Always declare a non-root `USER`. Distroless provides `USER nonroot:nonroot` (`65532`), while alpine/debian allow custom user creation.
- **Implement Language-Specific Cache Mounts**: Always use `RUN --mount=type=cache,target=...` for package managers (`npm`, `pip`, `m2`, `cargo`) to speed up CI/CD workflows.
- **Leverage Spring Boot Layer Tools**: Never ship monolithic fat JARs when layer tools can cache unchanged dependencies across releases.

### Anti-Patterns
- **Copying Local `node_modules` or `.venv`**: Never copy local development folders into the image; rely exclusively on `.dockerignore` and clean in-container installations.
- **Running Python as Root with `CMD python main.py`**: Lacks signal forwarding, logs buffer without `PYTHONUNBUFFERED=1`, and runs under dangerous root privileges.
- **Hardcoding JVM Heap via `-Xmx4g`**: Bypasses container cgroup detection. Use `-XX:MaxRAMPercentage=75.0` so the JVM automatically scales to Docker container memory limits.

---

## 7. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| Node.js container ignores `SIGINT`/`SIGTERM` on `docker stop` | Node runs as PID 1 via `npm start` wrapper script which doesn't forward signals | `docker top <container>` | Use exec-form `CMD ["node", "dist/main.js"]` or use `tini`. |
| Python logs do not appear in `docker logs` until container exits | Python default stdout/stderr buffering is enabled | `docker logs -f <id>` shows blank output | Add `ENV PYTHONUNBUFFERED=1`. |
| Rust binary panics on `scratch` with missing dynamic libraries | Rust compiled against default `gnu` target instead of `musl` | Run `file target/release/app` | Target `x86_64-unknown-linux-musl` with static linkage. |
| Spring Boot killed by OS with exit code 137 | JVM exceeded container cgroup memory limits | `docker inspect <id> --format='{{.State.OOMKilled}}'` | Set `-XX:MaxRAMPercentage=75.0` to leave memory for OS/metaspace. |
| `npm ci` fails with `package-lock.json out of date` | Discrepancy between `package.json` and `package-lock.json` | Run `npm ci` locally | Run `npm install` locally and commit updated `package-lock.json`. |
| Distroless Node cannot resolve DNS hostnames | Static binary lacks NSS or missing `/etc/hosts` resolution | `docker logs` reports `ENOTFOUND` | Use `gcr.io/distroless/nodejs22-debian12` which includes glibc NSS. |

---

## 8. Performance Tuning & Resource Optimization

1. **Spring Boot Memory Allocation**:
   Containers allocate a hard memory ceiling (e.g., `--memory=2g`). Java needs memory not just for heap, but also for Metaspace, threads, and Native Memory. Setting `-XX:MaxRAMPercentage=75.0` gives 1.5GB to the heap and preserves 512MB for native execution, completely avoiding OOM kills.
2. **Node.js Threadpool Tuning**:
   Node's `libuv` default threadpool size is 4. For heavy cryptographic or file system workloads, increase it via container environment variable:
   `ENV UV_THREADPOOL_SIZE=16`
3. **Python Worker Sizing**:
   For FastAPI under Uvicorn/Gunicorn, follow the standard formula:
   $$\text{Workers} = 2 \times \text{vCPU Cores} + 1$$
   Avoid over-allocating workers in small container limits (`--cpus=1`).

---

## 9. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK SURFACE COMPARISON: NAIVE VS HARDENED RECIPES          |
+--------------------------------------------------------------+
| Language    | Naive Image Footprint   | Hardened Recipe Footprint |
|-------------+-------------------------+---------------------------|
| Node.js     | 1.1 GB (full ubuntu+npm)| 140 MB (Distroless)       |
| Python      | 950 MB (full debian+gcc)| 120 MB (Debian Slim venv) |
| Go          | 850 MB (golang SDK full)| 12 MB (Scratch Static)    |
| Java        | 680 MB (Full JDK+Maven) | 160 MB (JRE + Layertools) |
| Rust        | 1.4 GB (Rust toolchain) | 8 MB (Scratch Musl)       |
+--------------------------------------------------------------+
```

Every recipe in this guide strips out package managers, shell access (in Go/Rust/Node Distroless), and drops root capabilities (`USER nonroot`).

---

## 10. Architectural Comparison Table

| Metric | Node.js (Distroless) | Python (Slim) | Go (Scratch) | Java (Temurin JRE) | Rust (Scratch) |
|:---|:---|:---|:---|:---|:---|
| **Base Image** | Distroless Node | Debian Slim | Scratch | Alpine JRE | Scratch |
| **Final Size** | ~140 MB | ~120 MB | ~12 MB | ~160 MB | ~8 MB |
| **Shell Included?**| No | Yes (`/bin/sh`)| No | Yes (`/bin/sh`)| No |
| **Package Manager?**| No | No | No | No | No |
| **Startup Time** | ~400 ms | ~300 ms | ~10 ms | ~3,500 ms | ~5 ms |
| **Memory Footprint**| ~45 MB | ~35 MB | ~8 MB | ~180 MB | ~6 MB |

---

## 11. Real-World Scenario Walkthrough

### Scenario: Slashing CI Build Times for a Rust Monorepo

#### Background
A fintech team building microservices in Rust suffered 18-minute Docker build times in GitHub Actions because any modification to a single handler file invalidated the entire `target/` cache, recompiling 300+ crates from scratch.

#### Solution
Adopted `cargo-chef`:
1. Added stage `planner` extracting `recipe.json` (capturing solely dependency metadata).
2. Added stage `builder` executing `cargo chef cook --release --recipe-path recipe.json`.
3. Cached the dependency compilation layer via BuildKit cache mounts.

#### Results
- Rebuild times on code changes dropped from **18 minutes 24 seconds** to **42 seconds**.
- Final production image size dropped from 1.6GB to 14MB by shipping on `scratch`.

---

## 12. Self-Assessment & Interview Questions

<details>
<summary><strong>1. Why is `npm ci` strictly preferred over `npm install` inside Dockerfiles?</strong></summary>

**Answer:**
`npm ci` (Clean Install) installs dependencies directly from `package-lock.json` with strict version checking. It throws an error if `package.json` and `package-lock.json` are out of sync, guarantees deterministic reproducible builds across environments, deletes any pre-existing `node_modules` for purity, and does not write back to `package-lock.json`, making it significantly faster and safer for automated CI/CD builds.
</details>

<details>
<summary><strong>2. How does Spring Boot's `layertools` optimize Docker layer caching compared to a standard fat JAR?</strong></summary>

**Answer:**
A standard fat JAR packages dependencies and code into a single archive. Changing one line of application code invalidates the entire Docker layer, forcing Docker to upload and store a full 80-150MB layer. Spring Boot `layertools` extracts the archive into four separate directories: `dependencies`, `spring-boot-loader`, `snapshot-dependencies`, and `application`. In Dockerfile layers, dependencies (which rarely change) are cached permanently, and only the tiny `application` layer (~50KB) is rebuilt on daily code updates.
</details>

<details>
<summary><strong>3. What compiler flags are required to build a standalone Go binary suitable for `FROM scratch`?</strong></summary>

**Answer:**
1. `CGO_ENABLED=0`: Disables dynamic linking against system C libraries (`glibc`).
2. `GOOS=linux GOARCH=amd64` (or target arch): Sets target OS and architecture.
3. `-ldflags="-s -w -extldflags '-static'"`: Strips DWARF debug symbols (`-w`) and symbol tables (`-s`) to minimize size, and forces static linking.
</details>

<details>
<summary><strong>4. Why should `ENV PYTHONUNBUFFERED=1` always be set in Python Dockerfiles?</strong></summary>

**Answer:**
By default, Python buffers standard output and standard error streams when not connected directly to a TTY terminal. In containers, this causes application logs to be held in memory buffers rather than immediately flushed to Docker daemon stdout/stderr. If the container crashes or freezes, logs are lost or delayed. `PYTHONUNBUFFERED=1` forces immediate unbuffered flushing.
</details>

<details>
<summary><strong>5. What problem does `cargo-chef` solve in Rust Docker builds?</strong></summary>

**Answer:**
Unlike Go or Node where dependency manifests (`go.mod`, `package.json`) can be copied separately from source code to pre-cache dependencies, `cargo build` in Rust requires the presence of source files (`src/main.rs` or `src/lib.rs`). Dummy file hacks are fragile. `cargo-chef` inspects `Cargo.lock` to generate a lightweight `recipe.json`, allowing dependencies to be fully compiled and cached in an isolated Docker layer before the real application source code is copied.
</details>

<details>
<summary><strong>6. Why is `-XX:MaxRAMPercentage=75.0` preferred over `-Xmx` in modern containerized Java applications?</strong></summary>

**Answer:**
Hardcoding `-Xmx2g` makes the image inflexible across different deployment environments and ignores container cgroup limits. Setting `-XX:MaxRAMPercentage=75.0` instructs the JVM's container ergonomics engine to dynamically calculate the heap ceiling as 75% of whatever memory limit is assigned to the container by Docker/Kubernetes (`--memory`), leaving the remaining 25% for JVM off-heap allocations, Metaspace, thread stacks, and OS buffers to avoid kernel Out-Of-Memory (OOM) kills.
</details>

<details>
<summary><strong>7. Why should you avoid using `npm start` as the `CMD` in a production Node.js Dockerfile?</strong></summary>

**Answer:**
`npm start` spawns `npm` as PID 1, which in turn spawns the actual Node process as a child process. `npm` does not forward POSIX operating system signals (`SIGTERM`, `SIGINT`) to child processes. When Docker stops the container (`docker stop`), Node never receives the shutdown signal, cannot close active database connections or in-flight HTTP requests, hangs for 10 seconds, and is ungracefully killed with `SIGKILL`.
</details>

<details>
<summary><strong>8. What is the security advantage of using a dedicated `builder` stage with virtualenv in Python?</strong></summary>

**Answer:**
Compiling Python wheels frequently requires installing system build packages (`build-essential`, `gcc`, `g++`, `libpq-dev`). By building the virtualenv inside a disposable `builder` stage and copying only `/opt/venv` to the clean `runtime` stage, all build tools, compiler toolchains, header files, and package caches are completely omitted from the final production image.
</details>

---

## 13. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Building and Validating a Static Go Scratch Container</strong></summary>

**Goal:** Create a minimal Go HTTP server, compile it statically, package into `scratch`, and verify its image size is under 15MB.

**Step-by-step Solution:**
1. Create `main.go`:
   ```go
   package main
   import ("fmt"; "net/http")
   func main() {
       http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
           fmt.Fprintf(w, "Hello from Scratch!")
       })
       http.ListenAndServe(":8080", nil)
   }
   ```
2. Build with multi-stage Dockerfile:
   ```dockerfile
   FROM golang:1.24-alpine AS builder
   WORKDIR /app
   COPY main.go .
   RUN CGO_ENABLED=0 go build -ldflags="-s -w" -o server main.go
   FROM scratch
   COPY --from=builder /app/server /server
   EXPOSE 8080
   USER 10001
   ENTRYPOINT ["/server"]
   ```
3. Verify size:
   ```bash
   docker build -t go-scratch-test .
   docker images go-scratch-test --format "{{.Repository}}: {{.Size}}"
   # Result: ~7MB
   ```
</details>

<details>
<summary><strong>Exercise 2: Setting up Spring Boot Layertools Extraction</strong></summary>

**Goal:** Inspect the 4 distinct layer folders generated by Spring Boot Layertools.

**Step-by-step Solution:**
1. Execute layertools inspection on any Spring Boot 3+ JAR:
   ```bash
   java -Djarmode=layertools -jar target/my-app.jar list
   ```
2. Observe output:
   ```
   dependencies
   spring-boot-loader
   snapshot-dependencies
   application
   ```
3. Test extraction into temporary directory:
   ```bash
   mkdir layers && cd layers
   java -Djarmode=layertools -jar ../target/my-app.jar extract
   ls -la
   ```
</details>

<details>
<summary><strong>Exercise 3: Verifying Signal Handling in Node.js</strong></summary>

**Goal:** Compare shutdown behavior between `CMD ["npm", "start"]` and `CMD ["node", "server.js"]`.

**Step-by-step Solution:**
1. Create `server.js` with graceful shutdown hook:
   ```javascript
   const http = require('http');
   const server = http.createServer((req, res) => res.end('ok')).listen(3000);
   process.on('SIGTERM', () => {
       console.log('Received SIGTERM, closing gracefully...');
       server.close(() => process.exit(0));
   });
   ```
2. Test container termination time:
   ```bash
   docker run -d --name test-sig node-graceful
   time docker stop test-sig
   # Notice clean exit in under 500ms instead of 10s timeout
   ```
</details>

<details>
<summary><strong>Exercise 4: Creating a Non-Root Virtualenv Python Image</strong></summary>

**Goal:** Configure a Python image where the application runs under UID 10001 with read-only root filesystem capabilities.

**Step-by-step Solution:**
1. Configure `Dockerfile` with `--read-only` readiness:
   ```dockerfile
   FROM python:3.12-slim-bookworm
   RUN groupadd -g 10001 appgroup && useradd -u 10001 -g appgroup appuser
   WORKDIR /app
   COPY --chown=appuser:appgroup app.py .
   USER 10001
   CMD ["python3", "app.py"]
   ```
2. Test running with `--read-only` and a tmpfs mount for `/tmp`:
   ```bash
   docker run --read-only --tmpfs /tmp --rm test-py-ro
   ```
</details>

<details>
<summary><strong>Exercise 5: Building a Multi-Stage Rust Container with Musl</strong></summary>

**Goal:** Compile a Rust binary using `x86_64-unknown-linux-musl` target and package into `scratch`.

**Step-by-step Solution:**
1. Setup `Cargo.toml` and `src/main.rs`.
2. In Dockerfile:
   ```dockerfile
   FROM rust:1.84-alpine AS builder
   RUN apk add --no-cache musl-dev
   WORKDIR /app
   COPY . .
   RUN cargo build --release --target x86_64-unknown-linux-musl
   FROM scratch
   COPY --from=builder /app/target/x86_64-unknown-linux-musl/release/app /app
   USER 10001
   ENTRYPOINT ["/app"]
   ```
3. Run container and verify static execution.
</details>

<details>
<summary><strong>Exercise 6: Benchmarking Docker Layer Rebuilds with BuildKit Mounts</strong></summary>

**Goal:** Measure build times with and without `RUN --mount=type=cache` when modifying a single code file.

**Step-by-step Solution:**
1. Compare `npm ci` vs `RUN --mount=type=cache,target=/root/.npm npm ci`.
2. Change code, rebuild, and inspect BuildKit output showing `CACHED` steps.
</details>

---

## Summary & Reference Cheat Sheet

```
+-------------------------------------------------------------------------+
|                  PRODUCTION RUNTIME SPECIFICATION CHEATSHEET            |
+-------------------------------------------------------------------------+
| Language    | Base Image        | Process Manager | User Declared       |
|-------------+-------------------+-----------------+---------------------|
| Node.js     | Distroless Node22 | Direct node CLI | nonroot (65532)     |
| Python      | Debian Slim       | Uvicorn/Gunicorn| appuser (10001)     |
| Go          | Scratch           | Direct Binary   | appuser (10001)     |
| Java        | Eclipse Temurin JRE| JarLauncher     | appuser (10001)    |
| Rust        | Scratch           | Direct Binary   | appuser (10001)     |
+-------------------------------------------------------------------------+
```

### Critical Golden Rules:
1. Always compile with static linkage when targeting `scratch`.
2. Always use `npm ci` and `layertools` to maximize cache efficiency.
3. Never use `npm start` in production.
4. Always set `PYTHONUNBUFFERED=1` in Python.
5. Always set `-XX:MaxRAMPercentage` in Java.

---

[← Previous: 09-healthchecks.md](09-healthchecks.md) | [Next: 11-dockerfile-linting-with-hadolint.md →](11-dockerfile-linting-with-hadolint.md)
