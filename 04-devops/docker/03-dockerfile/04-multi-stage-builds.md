---
title: "Multi-Stage Docker Builds: Architecture, Optimization, and Minimal Production Images"
description: "Mastering Multi-Stage Docker builds: eliminating build toolchain bloat, stage naming, COPY --from mechanics, external image stage borrowing, targeting, and micro-images."
category: "DevOps / Containers"
tags: ["docker", "multi-stage", "dockerfile", "optimization", "security", "scratch", "distroless"]
difficulty: "Intermediate"
prerequisites: ["03-dockerfile/01-dockerfile-basics.md", "03-dockerfile/03-layers-and-caching.md"]
estimated_read_time: "25 min"
---

# Multi-Stage Docker Builds: Architecture, Optimization, and Minimal Production Images

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. The Historical Pain: The "Builder Pattern" Anti-Pattern](#2-the-historical-pain-the-builder-pattern-anti-pattern)
- [3. Multi-Stage Build Syntax and Execution Mechanics](#3-multi-stage-build-syntax-and-execution-mechanics)
- [4. Advanced Features: Targeting and External Stage Ingestion](#4-advanced-features-targeting-and-external-stage-ingestion)
- [5. Production Language Recipes (Go, Node.js, Java, Rust)](#5-production-language-recipes-go-nodejs-java-rust)
- [6. Practical Diagnostic Demonstrations](#6-practical-diagnostic-demonstrations)
- [7. Best Practices & Production Guidelines](#7-best-practices--production-guidelines)
- [8. Common Pitfalls & Anti-Patterns](#8-common-pitfalls--anti-patterns)
- [9. Comparison Table: Single-Stage vs Multi-Stage Images](#9-comparison-table-single-stage-vs-multi-stage-images)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

Compiling modern software requires massive toolchains: compilers (`gcc`, `clang`, `rustc`), SDKs (Go compiler, OpenJDK, .NET SDK), package managers (`npm`, `cargo`, `pip`, `maven`), header files, and build utilities (`make`, `cmake`, `git`). 

A typical Go build environment consumes $1.2\text{ GB}$ of disk space. A Java Maven build environment consumes $800\text{ MB}$.

However, once the source code is compiled into a static binary or bytecode, **none of those build tools are required to run the application in production**. If you leave the build tools inside the production image:
1. **Massive Attack Surface**: An attacker who gains a web shell has access to `curl`, `gcc`, `git`, and package managers, allowing them to download and compile kernel rootkits or reverse shells directly inside your container.
2. **Huge Storage & Network Costs**: Every deployment transfers gigabytes across cloud networks, slowing down scaling events and bloating registry storage bills.
3. **Severe Vulnerability Noise**: Vulnerability scanners (Trivy, Grype, Snyk) report hundreds of CVEs in build packages that your runtime code never executes.

**Multi-Stage Builds** solve this completely. Introduced natively in Docker 17.05, multi-stage builds allow a single `Dockerfile` to contain multiple `FROM` lines. Each `FROM` instruction begins a new, isolated stage with a fresh base environment. You can compile your software in a heavy builder stage, and then selectively copy **only the final compiled artifacts** into a razor-thin, hardened production base image (`alpine`, `distroless`, or `scratch`).

```
+---------------------------------------------------------------------------------+
|                       MULTI-STAGE BUILD ARCHITECTURE                            |
|                                                                                 |
|  STAGE 1: "builder" (Heavyweight Environment)                                   |
|  FROM golang:1.22-bookworm AS builder                                           |
|  - Contains full Linux OS, Go compiler, git, gcc (Size: 1.1GB)                  |
|  - RUN go build -ldflags="-s -w" -o /app/server main.go                         |
|                                                                                 |
|  ============== Selective Boundary (BuildKit separates stages) ================ |
|                                   |                                             |
|                                   v COPY --from=builder /app/server /server     |
|                                                                                 |
|  STAGE 2: "production" (Minimalist Runtime)                                     |
|  FROM scratch                                                                   |
|  - Contains ZERO OS, zero shells, zero package managers                         |
|  - Contains ONLY the single compiled binary (/server)                           |
|  - Size: 12MB (99% reduction in size and 100% elimination of CVEs!)             |
+---------------------------------------------------------------------------------+
```

---

## 2. The Historical Pain: The "Builder Pattern" Anti-Pattern

Before multi-stage builds existed, engineering teams had to implement the awkward **"Builder Pattern"** using two separate Dockerfiles and brittle bash orchestration scripts:

1. `Dockerfile.build`: An image containing the compiler.
2. A bash script that:
   - Built the builder image: `docker build -f Dockerfile.build -t app-builder .`
   - Spun up a throwaway container: `docker create --name extract app-builder`
   - Copied the binary back to the host disk: `docker cp extract:/app/bin ./bin`
   - Removed the builder container: `docker rm extract`
   - Built a second `Dockerfile.prod` that copied the binary from the host: `docker build -f Dockerfile.prod -t app:prod .`

This process was slow, polluted the host disk with intermediate binaries, and broke cross-platform builds. Multi-stage builds collapsed this entire workflow into a single, clean, declarative Dockerfile.

---

## 3. Multi-Stage Build Syntax and Execution Mechanics

### 3.1. Naming Stages (`AS <name>`)
To name a build stage, append `AS <name>` to the `FROM` instruction:
```dockerfile
FROM node:20-alpine AS builder
```
Stage names are case-insensitive and allow other stages to reference them by name rather than arbitrary numeric indexes.

### 3.2. Copying Artifacts Across Stages (`COPY --from=...`)
The core mechanic of multi-stage builds is the `--from` flag on `COPY`:
```dockerfile
COPY --from=builder /app/dist /usr/share/nginx/html
```
- `builder`: The named stage defined earlier in the Dockerfile.
- `/app/dist`: The path to the artifact inside the `builder` stage's filesystem.
- `/usr/share/nginx/html`: The destination path inside the current stage's filesystem.

---

## 4. Advanced Features: Targeting and External Stage Ingestion

### 4.1. Stage Targeting (`--target`)
You do not have to build an entire multi-stage Dockerfile to completion. In CI/CD pipelines, you can build up to a specific intermediate stage using the `--target` CLI flag:

```dockerfile
FROM alpine AS base
RUN echo "Base setup" > /base.txt

FROM base AS unit-tests
RUN echo "Running test suite..." && true

FROM base AS linter
RUN echo "Running security linter..." && true

FROM base AS production
COPY --from=base /base.txt /prod.txt
CMD ["cat", "/prod.txt"]
```
In your CI pipeline:
```bash
# Run tests in parallel CI job 1
docker build --target unit-tests -t my-app:test .

# Run linter in parallel CI job 2
docker build --target linter -t my-app:lint .

# Build final production artifact in deployment job
docker build --target production -t my-app:prod .
```
BuildKit executes only the stages required by the specified target, skipping unrelated stages entirely!

### 4.2. Borrowing Artifacts from External Images
The `--from` flag is not limited to stages defined inside the same Dockerfile. You can pull artifacts **directly from any public or private image in a remote registry**:

```dockerfile
FROM alpine:3.20

# Copy the official Prometheus node-exporter binary directly from its official image!
COPY --from=prom/node-exporter:v1.7.0 /bin/node_exporter /usr/local/bin/

# Copy standard Docker CLI binary directly from the official docker image!
COPY --from=docker:26-cli /usr/local/bin/docker /usr/local/bin/
```
This eliminates compiling or curling third-party utilities manually.

---

## 5. Production Language Recipes (Go, Node.js, Java, Rust)

### 5.1. Static Go Microservice (`scratch`)
Compiles a statically linked Go binary with zero external OS dependencies:

```dockerfile
# Stage 1: Build & Compile
FROM golang:1.22-alpine AS builder
WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download
COPY . .
# CGO_ENABLED=0 produces a statically linked binary; -s -w strips debug symbols
RUN CGO_ENABLED=0 GOOS=linux go build -ldflags="-s -w" -o /bin/server .

# Stage 2: Minimalist Production Image
FROM scratch
# Import CA certificates so the binary can make outbound HTTPS calls!
COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/
COPY --from=builder /bin/server /server
USER 10001:10001
ENTRYPOINT ["/server"]
```
*Resulting image size: $\sim 10\text{ MB}$*.

### 5.2. Production Node.js / React Frontend (`nginx:alpine`)
Compiles a React/Vue SPA with Node and serves it using an ultra-light Nginx web server:

```dockerfile
# Stage 1: Build Assets
FROM node:20-alpine AS builder
WORKDIR /app
COPY package*.json ./
RUN npm ci
COPY . .
RUN npm run build

# Stage 2: Production Nginx Server
FROM nginx:1.27-alpine
COPY --from=builder /app/dist /usr/share/nginx/html
EXPOSE 80
CMD ["nginx", "-g", "daemon off;"]
```
*Resulting image size: $\sim 25\text{ MB}$ (down from $1.4\text{ GB}$ with Node and node_modules)*.

### 5.3. Production Java / Spring Boot (`eclipse-temurin`)
Builds a Spring Boot JAR with Maven and runs it in a minimal JRE:

```dockerfile
# Stage 1: Maven Builder
FROM maven:3.9-eclipse-temurin-21-alpine AS builder
WORKDIR /build
COPY pom.xml .
RUN mvn dependency:go-offline
COPY src ./src
RUN mvn clean package -DskipTests

# Stage 2: Minimal JRE Runtime
FROM eclipse-temurin:21-jre-alpine
WORKDIR /app
RUN addgroup -S appgroup && adduser -S appuser -G appgroup
COPY --from=builder --chown=appuser:appgroup /build/target/*.jar app.jar
USER appuser
EXPOSE 8080
ENTRYPOINT ["java", "-XX:MaxRAMPercentage=75.0", "-jar", "app.jar"]
```

---

## 6. Practical Diagnostic Demonstrations

### Example 1: Comparing Single-Stage vs Multi-Stage Go Image Sizes
Observe a $99\%$ reduction in image footprint:

```bash
mkdir -p /tmp/ms-lab && cd /tmp/ms-lab

cat <<'EOF' > main.go
package main
import (
    "fmt"
    "net/http"
)
func main() {
    http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
        fmt.Fprintf(w, "Hello from Multi-Stage Container!\n")
    })
    http.ListenAndServe(":8080", nil)
}
EOF

# Multi-stage Dockerfile
cat <<'EOF' > Dockerfile
FROM golang:1.22-alpine AS builder
WORKDIR /src
COPY main.go .
RUN CGO_ENABLED=0 go build -ldflags="-s -w" -o /server main.go

FROM scratch
COPY --from=builder /server /server
EXPOSE 8080
ENTRYPOINT ["/server"]
EOF

# Build multi-stage image
docker build -t go-multistage:prod .

# Compare sizes against base golang image
docker images | grep -E "golang|go-multistage"
```
*Output Comparison:*
```text
golang             1.22-alpine   306MB
go-multistage      prod          7.1MB
```
The multi-stage image is **$7.1\text{ MB}$**, while retaining full web server functionality!

```bash
# Cleanup
rm -rf /tmp/ms-lab && docker rmi go-multistage:prod
```

### Example 2: Ingesting Artifacts from External Registry Images
Borrow the official Nginx configuration template directly from upstream:

```dockerfile
FROM alpine:3.20
# Copy default nginx configuration directly from official nginx image
COPY --from=nginx:alpine /etc/nginx/nginx.conf /tmp/nginx.conf
CMD ["cat", "/tmp/nginx.conf"]
```

### Example 3: Verifying Stage Dead Code Elimination in BuildKit
Prove that BuildKit skips unreferenced intermediate stages:

```dockerfile
FROM alpine AS unused-stage
RUN sleep 30 && echo "This should be skipped!"

FROM alpine AS final-stage
RUN echo "Fast build execution"
```
Build the Dockerfile:
```bash
docker build -t skip-test .
```
Notice that BuildKit finishes in under $0.5\text{ seconds}$ because it recognizes that `unused-stage` is never referenced by `final-stage`, skipping the 30-second sleep entirely!

```bash
docker rmi skip-test
```

### Example 4: Debugging an Intermediate Stage Interactively
Launch an interactive shell directly into a builder stage to inspect why a compilation failed:

```bash
# Build strictly up to the builder stage
docker build --target builder -t debug-builder .

# Launch shell into the builder stage to inspect generated files
docker run --rm -it debug-builder /bin/sh
```

---

## 7. Best Practices & Production Guidelines

1. **Always Name Every Stage (`AS <name>`)**: Never reference stages by numerical index (`COPY --from=0`). Reordering instructions in a Dockerfile silently breaks numerical references; named stages are permanent and descriptive.
2. **Always Copy CA Certificates When Using `FROM scratch`**: Statically compiled binaries (Go, Rust) executing on `scratch` lack the OS root certificate bundle. Always copy certificates from the builder stage (`COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/`) so HTTPS API calls function properly.
3. **Run as Non-Root in the Final Stage**: Create an unprivileged user in the builder stage, and copy `/etc/passwd` into the final stage, or use numeric UIDs (`USER 10001:10001`).
4. **Use Multi-Stage for Testing and Security Linting**: Define intermediate stages for running unit tests (`RUN npm test`) and security scans. If tests fail, the build halts before the production stage compiles.
5. **Set Proper File Permissions During Copy**: Combine `--chown` with `--from`:
   ```dockerfile
   COPY --from=builder --chown=appuser:appgroup /src/app /app
   ```

---

## 8. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Proper Solution |
|:---|:---|:---|:---|
| **Using `scratch` for dynamically linked binaries** | Compiling C/Go binaries with standard glibc dynamically linked. | Container fails immediately with `no such file or directory` or Exit 127 because libc is missing. | Statically link binaries (`CGO_ENABLED=0` in Go) or use `distroless/base` or `alpine`. |
| **Missing CA certificates in `scratch`** | Go/Rust app attempting to connect to external HTTPS APIs. | Application crashes with `x509: certificate signed by unknown authority`. | Copy `/etc/ssl/certs/ca-certificates.crt` from builder stage. |
| **Referencing stages by numerical index (`--from=0`)** | Inserting a new stage earlier in the Dockerfile. | Stage 0 now points to a different stage; files copy from wrong stage; build fails. | Always use explicit names: `AS builder` and `--from=builder`. |
| **Accidentally including compiler in final stage** | Copying files from builder using wildcard (`COPY --from=builder /usr /usr`). | Copies entire compiler toolchain back into production image, nullifying multi-stage benefits. | Copy only the specific binary or compiled dist directory (`/bin/app`). |
| **Running tests in the production stage** | Adding `RUN npm test` in the final stage. | Test frameworks (Jest, Mocha) and test fixtures stay embedded in production image. | Run tests in a dedicated intermediate stage (`--target test`). |
| **Forgetting to set `USER` in final stage** | Assuming unprivileged user carries over across stages. | Every stage starts fresh; final stage runs as root by default. | Explicitly declare `USER <user>` in the final stage. |

---

## 9. Comparison Table: Single-Stage vs Multi-Stage Images

| Metric | Single-Stage Build (Go / Java / Node) | Multi-Stage Build (`scratch` / `alpine`) |
|:---|:---|:---|
| **Final Image Size** | $800\text{ MB} - 1.5\text{ GB}$ | $10\text{ MB} - 50\text{ MB}$ ($>95\%$ reduction) |
| **Vulnerabilities (CVEs)**| High (Hundreds reported in compilers/libs) | Near Zero (No OS or shell to exploit) |
| **Attack Surface** | High (Shells, curl, gcc, package managers)| Minimalist (Single binary or runtime only) |
| **Network Transfer Time**| $30 - 90\text{ seconds}$ per pull | $< 2\text{ seconds}$ per pull |
| **Build Configuration** | Simple | Requires understanding `--from` boundaries |
| **Debugging Accessibility**| Easy (Can exec into bash/sh) | Requires ephemeral containers or host tracing |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. What is the fundamental problem solved by Multi-Stage Docker builds?</strong></summary>

Multi-Stage builds solve the problem of image bloat and attack surface expansion caused by build toolchains. Compiling applications requires heavy tools (compilers, SDKs, package managers, header files), but running the compiled application in production requires only the binary or runtime. Multi-Stage builds allow compiling in an isolated builder stage and copying only the final executable artifacts into a minimal, hardened production base image, reducing image sizes by over 90% and eliminating CVE vulnerabilities.
</details>

<details>
<summary><strong>2. How does BuildKit optimize multi-stage builds when an intermediate stage is not needed?</strong></summary>

BuildKit constructs a Directed Acyclic Graph (DAG) of dependencies across instructions. If an intermediate stage (e.g., `AS test` or `AS linter`) is defined in the Dockerfile but its outputs are never referenced by the final stage (or by the requested `--target`), BuildKit's solver detects that the stage is dead code and completely skips executing its instructions, saving CPU cycles and build time.
</details>

<details>
<summary><strong>3. Why does a statically compiled Go binary fail to make HTTPS calls when running on <code>FROM scratch</code>?</strong></summary>

A `FROM scratch` image is completely empty—it has zero files, which means it lacks the operating system's standard **Root Certificate Authority (CA) bundle** (`/etc/ssl/certs/ca-certificates.crt`). When the Go application attempts an HTTPS handshake, it cannot verify the TLS certificate of the remote server and crashes with an x509 validation error. This is resolved by copying the CA certificates from the builder stage:
```dockerfile
COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/
```
</details>

<details>
<summary><strong>4. Can you copy files from an image on Docker Hub directly without declaring a build stage for it?</strong></summary>

Yes. The `COPY --from` instruction accepts any valid image reference from an OCI registry:
```dockerfile
COPY --from=busybox:latest /bin/sh /bin/sh
```
Docker transparently pulls the image manifest, unpacks the layer containing the requested file, copies the file into the target directory, and discards the rest of the external image.
</details>

<details>
<summary><strong>5. What is the difference between <code>FROM scratch</code> and <code>FROM alpine</code> for multi-stage targets?</strong></summary>

- **`FROM scratch`**: A zero-byte virtual base containing no operating system, no shell (`/bin/sh`), no shared libraries (libc), and no utilities. It requires statically linked binaries (Go, Rust). It offers the maximum possible security and smallest footprint.
- **`FROM alpine`**: A complete minimal Linux distribution ($\sim 7\text{ MB}$) based on musl libc and BusyBox. It provides a working shell (`/bin/sh`), a package manager (`apk`), and basic POSIX utilities, making it suitable for applications that require an interpreter (Node, Python) or dynamic linking.
</details>

<details>
<summary><strong>6. How can an administrator use <code>--target</code> to run continuous integration tests inside a Docker build?</strong></summary>

By structuring the Dockerfile with an intermediate test stage:
```dockerfile
FROM node:20-alpine AS test
WORKDIR /app
COPY . .
RUN npm ci && npm test
```
The CI pipeline executes:
```bash
docker build --target test .
```
If `npm test` fails (exits non-zero), the Docker build fails immediately and stops the CI pipeline. If tests pass, subsequent deployment jobs build the final production stage.
</details>

<details>
<summary><strong>7. Why should you avoid referencing stages by numeric index (e.g., <code>COPY --from=0</code>)?</strong></summary>

Numeric indexes reference stages by their sequential order of appearance in the Dockerfile. If a developer later inserts an architectural stage (e.g., adding a linter or security scanner) at the top of the file, all subsequent numerical indexes shift by one. `COPY --from=0` will now copy from the new linter stage instead of the compiler stage, silently breaking the build. Using named stages (`AS builder` $\to$ `COPY --from=builder`) is resilient to reordering.
</details>

<details>
<summary><strong>8. What is Google's "Distroless" image initiative, and where does it fit in multi-stage builds?</strong></summary>

**Distroless** images are language-focused runtime base images created by Google (`gcr.io/distroless/static`, `gcr.io/distroless/nodejs20`, `gcr.io/distroless/java17`). They contain only your application binary and its direct runtime dependencies (glibc, libssl, CA certificates). They contain **no package managers, no shells, and no standard Unix utilities**. They represent an ideal multi-stage target: easier to use than `scratch` for dynamically linked apps, but far more secure than standard Linux distributions.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Build a Multi-Stage Rust Microservice</strong></summary>

**Goal**: Compile a Rust program and run it inside a minimal Debian-slim runtime.

```bash
mkdir -p /tmp/rust-ms && cd /tmp/rust-ms

cat <<'EOF' > Cargo.toml
[package]
name = "rust_demo"
version = "0.1.0"
edition = "2021"
[dependencies]
EOF

mkdir src && cat <<'EOF' > src/main.go
fn main() {
    println!("Hello from High-Performance Multi-Stage Rust!");
}
EOF
mv src/main.go src/main.rs

cat <<'EOF' > Dockerfile
FROM rust:1.78-slim AS builder
WORKDIR /app
COPY . .
RUN cargo build --release

FROM debian:bookworm-slim
COPY --from=builder /app/target/release/rust_demo /usr/local/bin/
CMD ["rust_demo"]
EOF

docker build -t rust-demo .
docker run --rm rust-demo

# Cleanup
rm -rf /tmp/rust-ms && docker rmi rust-demo
```
</details>

<details>
<summary><strong>Exercise 2: Borrowing Static Binary from External Image</strong></summary>

**Goal**: Ingest the `kubectl` CLI binary into an Alpine container directly from the official Kubernetes image.

```bash
mkdir -p /tmp/borrow-lab && cd /tmp/borrow-lab

cat <<'EOF' > Dockerfile
FROM alpine:3.20
COPY --from=bitnami/kubectl:latest /opt/bitnami/kubectl/bin/kubectl /usr/local/bin/
CMD ["kubectl", "version", "--client=true"]
EOF

docker build -t kubectl-demo .
docker run --rm kubectl-demo

# Cleanup
rm -rf /tmp/borrow-lab && docker rmi kubectl-demo
```
</details>

<details>
<summary><strong>Exercise 3: Test Multi-Stage Target Selection in CI Simulation</strong></summary>

**Goal**: Build only the intermediate test stage of an application.

```bash
cat <<'EOF' > /tmp/target.dockerfile
FROM alpine AS base
RUN echo "Base setup" > /base.txt

FROM base AS test-suite
RUN echo "Running unit tests: PASS" > /test.log

FROM base AS release
COPY --from=base /base.txt /prod.txt
EOF

docker build -f /tmp/target.dockerfile --target test-suite -t test-runner /tmp
docker run --rm test-runner cat /test.log

# Cleanup
rm -f /tmp/target.dockerfile && docker rmi test-runner
```
</details>

<details>
<summary><strong>Exercise 4: Verify Dropped CVE Footprint on Distroless vs Standard Node</strong></summary>

**Goal**: Compare image layer counts between full Node and a multi-stage distroless build.

```bash
cat <<'EOF' > /tmp/distroless.dockerfile
FROM node:20-alpine AS builder
WORKDIR /app
RUN echo "console.log('Distroless ready');" > app.js

FROM gcr.io/distroless/nodejs20-debian12
COPY --from=builder /app/app.js /app/
CMD ["/app/app.js"]
EOF

docker build -f /tmp/distroless.dockerfile -t distroless-demo /tmp
docker run --rm distroless-demo
docker images | grep -E "node|distroless-demo"

# Cleanup
rm -f /tmp/distroless.dockerfile && docker rmi distroless-demo
```
</details>

<details>
<summary><strong>Exercise 5: Build a Static Web App Served by Caddy</strong></summary>

**Goal**: Compile static HTML assets and serve using Caddy server in a multi-stage pipeline.

```bash
cat <<'EOF' > /tmp/caddy.dockerfile
FROM alpine AS asset-builder
WORKDIR /dist
RUN echo "<h1>Caddy Multi-Stage Delivery</h1>" > index.html

FROM caddy:2-alpine
COPY --from=asset-builder /dist /usr/share/caddy
EXPOSE 80
EOF

docker build -f /tmp/caddy.dockerfile -t caddy-ms /tmp
docker rmi caddy-ms && rm -f /tmp/caddy.dockerfile
```
</details>

<details>
<summary><strong>Exercise 6: Inspect Multi-Stage Layer Provenance</strong></summary>

**Goal**: Trace where copied layers originated using `docker history`.

```bash
docker history go-multistage:prod 2>/dev/null || echo "Run exercise 1 to inspect provenance."
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                         MULTI-STAGE BUILDS CHEAT SHEET                                |
+---------------------------------------------------------------------------------------+
| Define Stage:          FROM <base> AS <stage_name>                                    |
| Copy Across Stages:    COPY --from=<stage_name> <src_path> <dest_path>                |
| Copy with Ownership:   COPY --from=builder --chown=user:group /app/dist /app         |
| Borrow from Registry:  COPY --from=prom/node-exporter:v1.7 /bin/node_exporter /bin/   |
| Build Target Stage:    docker build --target <stage_name> -t <name> .                 |
| Go Minimal Template:   FROM golang:alpine AS build ... FROM scratch                  |
|                        COPY --from=build /etc/ssl/certs/ca-certificates.crt ...       |
| Golden Rule:           Never leave compilers, SDKs, or git inside production images!  |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Docker Documentation: Multi-Stage Builds](https://docs.docker.com/build/building/multi-stage/)
- [Google Container Tools: Distroless Images GitHub Repository](https://github.com/GoogleContainerTools/distroless)
- [Moby BuildKit: Multi-Stage Optimization and Dead Code Elimination](https://github.com/moby/buildkit)
- [Open Container Initiative (OCI) Security Recommendations for Minimal Images](https://opencontainers.org/)

---

| [< Previous: 03-layers-and-caching.md](./03-layers-and-caching.md) | [Next: 05-dockerignore.md](./05-dockerignore.md) > |
| :--- | :--- |
