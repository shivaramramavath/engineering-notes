---
title: "BuildKit and Docker Buildx: Low-Level Build (LLB), Cache Mounts, and Builders"
description: "Comprehensive guide to BuildKit and Docker Buildx: LLB graph solver, builder instances (docker-container, kubernetes), cache/secret/ssh mounts, and advanced exporters."
category: "DevOps / Containers"
tags: ["docker", "buildkit", "buildx", "llb", "cache-mounts", "secrets", "ssh", "exporters"]
difficulty: "Advanced"
prerequisites: ["03-dockerfile/01-dockerfile-basics.md", "03-dockerfile/03-layers-and-caching.md"]
estimated_read_time: "25 min"
---

# BuildKit and Docker Buildx: Low-Level Build (LLB), Cache Mounts, and Builders

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. The BuildKit Architecture: Low-Level Build (LLB) and Solvers](#2-the-buildkit-architecture-low-level-build-llb-and-solvers)
- [3. Docker Buildx and Builder Instances](#3-docker-buildx-and-builder-instances)
- [4. Advanced Mount Types: Cache, Secret, SSH, and Bind](#4-advanced-mount-types-cache-secret-ssh-and-bind)
- [5. Heredoc Syntax and Inline Scripts](#5-heredoc-syntax-and-inline-scripts)
- [6. Advanced Exporters: Local, Tar, and Registry Output](#6-advanced-exporters-local-tar-and-registry-output)
- [7. Practical Diagnostic Demonstrations](#7-practical-diagnostic-demonstrations)
- [8. Best Practices & Production Guidelines](#8-best-practices--production-guidelines)
- [9. Common Pitfalls & Anti-Patterns](#9-common-pitfalls--anti-patterns)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

In 2018, Docker introduced **BuildKit**, a ground-up redesign of the container image compilation engine. In modern Docker (v23+), BuildKit has completely replaced the legacy builder.

The legacy builder had critical architectural limitations:
- It executed instructions **strictly sequentially** (single-threaded).
- It compiled every intermediate stage in multi-stage builds, even if the final image never used them.
- It lacked the ability to cache package manager directories across builds without bloating layers.
- It could not pass secrets (SSH keys, tokens) to build steps without permanently baking them into intermediate layer history.

BuildKit solves these challenges by converting high-level Dockerfile instructions into a **Directed Acyclic Graph (DAG)** of **Low-Level Build (LLB)** operations. Under the hood, BuildKit acts like an optimizing compiler (analogous to LLVM): it reorders operations, executes independent stages in parallel, prunes dead code paths, and supports pluggable frontends and exporters.

Combined with **Docker Buildx** (the CLI plugin orchestrating BuildKit instances), engineers can build multi-platform images, target remote Kubernetes build clusters, and leverage advanced mount mechanisms.

```
+---------------------------------------------------------------------------------+
|                       BUILDKIT LLB EXECUTION ARCHITECTURE                       |
|                                                                                 |
|  [Dockerfile (High-Level Frontend)]                                             |
|        |                                                                        |
|        | Converted by frontend parser (# syntax=docker/dockerfile:1)            |
|        v                                                                        |
|  [BuildKit LLB (Low-Level Build Intermediate Representation)]                   |
|  - Content-addressable operations graph (DAG)                                   |
|        |                                                                        |
|        +---> [Branch A: Compile Backend]      \                                 |
|        |     (Executes on Core 1)              \ (Concurrent                    |
|        |                                        \ Execution!)                   |
|        +---> [Branch B: Transpile Frontend]     /                               |
|        |     (Executes on Core 2)              /                                |
|        |                                      /                                 |
|        v                                                                        |
|  [Merge & Export Layer]                                                         |
|        |                                                                        |
|        +---> Output: OCI Image (Container storage / Registry push)              |
|        +---> Output: Local Directory (Extracted artifacts to host)              |
|        +---> Output: Tarball Archive                                            |
+---------------------------------------------------------------------------------+
```

---

## 2. The BuildKit Architecture: Low-Level Build (LLB) and Solvers

### 2.1. What is LLB?
**Low-Level Build (LLB)** is BuildKit's intermediate representation (IR), conceptually identical to how LLVM IR represents C++ or Rust code before compiling to machine assembly:
- An LLB graph is an acyclic dependency graph where every node is a deterministic operation (executing a process, unpacking a file, mounting a volume).
- Every node is content-addressable: if its inputs, environment, and parent nodes have not changed, it is retrieved from the cache in $0\text{ ms}$.
- LLB is completely decoupled from Dockerfile syntax: you can build LLB graphs using Go SDKs, Python, or alternative frontends (like Mocker or Buildpacks).

### 2.2. The BuildKit Daemon (`buildkitd`)
BuildKit runs either embedded inside the standard Docker daemon (`dockerd`) or as an independent container/pod (`buildkitd`). The `docker buildx` CLI acts as a gRPC client communicating with `buildkitd`.

---

## 3. Docker Buildx and Builder Instances

`docker buildx` manages **Builder Instances**. By default, Docker uses the `docker` driver (embedded inside the local Docker daemon). However, to unlock advanced features (such as true multi-platform concurrent builds and remote cache backends), you must create a dedicated builder instance.

### Builder Drivers:
1. **`docker` (Default)**: Embedded inside `dockerd`. Standard local builds. Does not support advanced multi-arch cross-compilation without pushing to a registry.
2. **`docker-container`**: Runs a dedicated `buildkitd` daemon inside a container. Supports all advanced BuildKit features, true multi-platform output, and custom `buildkitd.toml` configurations.
3. **`kubernetes`**: Deploys `buildkitd` pods into a Kubernetes cluster, allowing CI/CD runners to scale build workers dynamically.
4. **`remote`**: Connects directly to a standalone, persistent BuildKit daemon running on bare-metal hardware.

### Managing Builders:
```bash
# List available builders (* denotes active)
docker buildx ls

# Create a new builder using the docker-container driver
docker buildx create --name my-builder --driver docker-container --bootstrap --use

# Inspect builder capabilities and supported CPU architectures
docker buildx inspect my-builder

# Switch active builder
docker buildx use my-builder

# Stop and delete a builder
docker buildx stop my-builder
docker buildx rm my-builder
```

---

## 4. Advanced Mount Types: Cache, Secret, SSH, and Bind

BuildKit's `RUN` instruction supports five specialized mount types configured via `--mount`:

### 4.1. Cache Mounts (`--mount=type=cache`)
Mounts a persistent directory on the host builder into the container during command execution, skipping network re-downloads across builds without contributing to image size:
```dockerfile
# syntax=docker/dockerfile:1
FROM python:3.12-alpine
WORKDIR /app
COPY requirements.txt .

RUN --mount=type=cache,target=/root/.cache/pip \
    pip install -r requirements.txt
```

### 4.2. Secret Mounts (`--mount=type=secret`)
Exposes private tokens, credentials, or keys temporarily in an in-memory filesystem (`/run/secrets/<id>`) during a single `RUN` execution:
```dockerfile
# syntax=docker/dockerfile:1
FROM alpine
RUN --mount=type=secret,id=gh_token \
    curl -H "Authorization: Bearer $(cat /run/secrets/gh_token)" https://api.github.com/user
```
Build execution:
```bash
docker build --secret id=gh_token,src=/home/user/.token.txt -t secure-demo .
```
The secret exists only in RAM during execution and is **completely excluded from all image layers and history**.

### 4.3. SSH Agent Forwarding (`--mount=type=ssh`)
Allows the container to clone private Git repositories using your host machine's SSH agent without copying private SSH keys into the container:
```dockerfile
# syntax=docker/dockerfile:1
FROM alpine
RUN apk add --no-cache openssh-client git
# Mount host SSH agent socket!
RUN --mount=type=ssh \
    mkdir -p -m 0700 ~/.ssh && ssh-keyscan github.com >> ~/.ssh/known_hosts && \
    git clone git@github.com:my-org/private-repo.git
```
Build execution forwarding your active SSH agent:
```bash
docker build --ssh default -t private-app .
```

### 4.4. Bind Mounts (`--mount=type=bind`)
Mounts files directly from the build context or an earlier stage into the build container **without copying them into the layer**:
```dockerfile
# syntax=docker/dockerfile:1
FROM golang:1.22-alpine
WORKDIR /src
# Mount source code directly from context into container without COPY!
RUN --mount=type=bind,target=. \
    go build -o /bin/server .
```

---

## 5. Heredoc Syntax and Inline Scripts

BuildKit allows authoring multi-line configuration files and inline shell scripts directly inside the Dockerfile using **Heredocs** (`<<EOF`):

### Inline Configuration Files:
```dockerfile
# syntax=docker/dockerfile:1
FROM nginx:alpine
COPY <<'EOF' /etc/nginx/conf.d/default.conf
server {
    listen 80;
    server_name localhost;
    location / {
        return 200 "Hello from Inline Heredoc Config!\n";
    }
}
EOF
```

### Multi-Line Shell Scripts:
Eliminate cumbersome `&& \` line continuations:
```dockerfile
# syntax=docker/dockerfile:1
FROM alpine
RUN <<'EOF'
set -e
echo "Starting multi-line script..."
apk update
apk add --no-cache curl jq
echo "Build setup complete!"
EOF
```

---

## 6. Advanced Exporters: Local, Tar, and Registry Output

By default, `docker build` exports the resulting image into the local Docker daemon image store. BuildKit supports custom **exporters** via the `-o` / `--output` flag:

```bash
# 1. Export as raw local filesystem directory on host (Great for static site generators!)
docker buildx build --output type=local,dest=./dist .

# 2. Export as an uncompressed tarball archive
docker buildx build --output type=tar,dest=./my-image.tar .

# 3. Build and push directly to remote registry without storing locally!
docker buildx build --platform linux/amd64,linux/arm64 -t my-reg.corp/app:1.0 --push .
```

---

## 7. Practical Diagnostic Demonstrations

### Example 1: Creating and Bootstrapping a `docker-container` Builder
Provision a high-performance BuildKit container builder:

```bash
# Create and start builder
docker buildx create --name advanced-builder --driver docker-container --bootstrap --use

# Verify builder status and supported architectures
docker buildx inspect advanced-builder
```
*Output confirms active builder running inside a dedicated BuildKit container with QEMU multi-arch emulation enabled.*

### Example 2: Compiling Static Binary Directly to Host Disk via `--output`
Compile a binary inside a container and output the artifact directly to your host directory without creating an image:

```bash
mkdir -p /tmp/export-lab && cd /tmp/export-lab

cat <<'EOF' > Dockerfile
# syntax=docker/dockerfile:1
FROM alpine AS builder
RUN echo "Compiled host utility v2.0" > /artifact.txt

FROM scratch
COPY --from=builder /artifact.txt /artifact.txt
EOF

# Output artifact directly to host ./out directory!
docker buildx build --output type=local,dest=./out .

# Verify file exists on host!
cat ./out/artifact.txt

# Cleanup
rm -rf /tmp/export-lab
```

### Example 3: Verifying Secret Masking in History
Prove that BuildKit secret mounts leave zero traces in image layer history:

```bash
mkdir -p /tmp/sec-lab && cd /tmp/sec-lab

echo "super_secret_token_12345" > secret.txt

cat <<'EOF' > Dockerfile
# syntax=docker/dockerfile:1
FROM alpine
RUN --mount=type=secret,id=my_secret \
    echo "Secret length: $(wc -c < /run/secrets/my_secret)" > /len.txt
CMD ["cat", "/len.txt"]
EOF

# Build passing secret
docker buildx build --load --secret id=my_secret,src=secret.txt -t sec-test .

# Run container and verify
docker run --rm sec-test

# Inspect history to prove the secret is nowhere in the layer metadata!
docker history --no-trunc sec-test

# Cleanup
rm -rf /tmp/sec-lab && docker rmi sec-test
```

### Example 4: Testing Heredoc Shell Script Execution
Build an image using clean heredoc shell script formatting:

```bash
docker build -t heredoc-test - <<'EOF'
# syntax=docker/dockerfile:1
FROM alpine
RUN <<'SCRIPT'
set -e
echo "Executing step 1"
echo "Executing step 2"
echo "Multi-line script succeeded!" > /result.txt
SCRIPT
CMD ["cat", "/result.txt"]
EOF

docker run --rm heredoc-test
docker rmi heredoc-test
```

### Example 5: Benchmarking BuildKit Cache Mounts with Go
Demonstrate caching the Go compiler package cache:

```dockerfile
# syntax=docker/dockerfile:1
FROM golang:1.22-alpine AS builder
WORKDIR /app
COPY go.mod ./
# Cache both Go module downloads and Go compiler build cache across builds!
RUN --mount=type=cache,target=/go/pkg/mod \
    --mount=type=cache,target=/root/.cache/go-build \
    go build -o /server .
```

---

## 8. Best Practices & Production Guidelines

1. **Always Declare the BuildKit Parser Directive**: Start every modern Dockerfile with `# syntax=docker/dockerfile:1`.
2. **Use `docker-container` Builder for Multi-Platform Builds**: The default `docker` driver cannot build and export multi-architecture images locally. Always create a dedicated `docker buildx create --driver docker-container` instance.
3. **Never Hardcode Secrets in Dockerfiles**: Always use `--mount=type=secret`. Never pass tokens via `ARG` or `ENV`.
4. **Use `type=local` Exporter for Artifact Extraction**: When compiling static web frontends (React, Vue) or binaries for host execution, use `--output type=local,dest=./dist` to extract files without generating throwaway container images.
5. **Set `sharing=locked` on Apt Cache Mounts**: When using `--mount=type=cache` with `apt`, specify `sharing=locked` to prevent concurrent builds from corrupting the dpkg lock database.

---

## 9. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Proper Solution |
|:---|:---|:---|:---|
| **Heredoc syntax failing to parse** | Missing `# syntax=docker/dockerfile:1` directive. | Parser fails with syntax error on `<<EOF`. | Add `# syntax=docker/dockerfile:1` to line 1. |
| **Multi-platform build failing locally** | Using default `docker` driver instead of `docker-container`. | Fails with `multi-platform build is not supported for the docker driver`. | Create builder: `docker buildx create --driver docker-container --use`. |
| **Leaking SSH private keys into images** | Copying `~/.ssh/id_rsa` into the build context. | Private key baked into public image layer. | Use SSH agent forwarding: `--mount=type=ssh` with `docker build --ssh default`. |
| **Package cache mounts persisting inside image** | Expecting cache mounts to stay in the layer. | Cache is host-only; runtime application fails if it expects cache files to exist. | Cache mounts are strictly for build-time caches (pip, npm, apt), not application data. |
| **Secret mount path missing** | Default secret mount path is `/run/secrets/<id>`. | Script looks in wrong path and fails with file not found. | Read secret from `/run/secrets/<id>` or specify `target=/custom/path`. |
| **Forgetting `--load` or `--push` with buildx** | Building multi-platform images with BuildKit. | Build completes, but resulting image is not loaded into local `docker images` store. | Pass `--load` for local architectures or `--push` for registries. |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. What is Low-Level Build (LLB) in BuildKit, and why is it architecturally significant?</strong></summary>

Low-Level Build (LLB) is BuildKit's intermediate representation (IR) of container build operations. It is a content-addressable directed acyclic graph (DAG) where nodes represent isolated operations (executing commands, mounting snapshots, transferring files). 
LLB is significant because it decouples the build engine from Dockerfile syntax:
1. It enables parallel, multi-threaded stage execution.
2. It allows automatic pruning of unused/dead stages.
3. It allows alternative frontends (like Moby, Buildpacks, or custom compilers) to compile to the same underlying OCI image targets.
</details>

<details>
<summary><strong>2. How does <code>--mount=type=secret</code> securely pass credentials during a Docker build?</strong></summary>

When `--mount=type=secret,id=token` is invoked:
1. The host Docker CLI streams the secret value in memory via a secure gRPC pipe to the BuildKit daemon.
2. BuildKit mounts an ephemeral, in-memory `tmpfs` filesystem at `/run/secrets/token` strictly inside the container process executing that specific `RUN` instruction.
3. The secret is accessible only during the lifetime of that process.
4. Once the process terminates, the tmpfs mount is unmounted. 
5. Because tmpfs resides in RAM and is excluded from the OverlayFS snapshot, the secret is completely absent from the committed image layer, metadata, and history.
</details>

<details>
<summary><strong>3. Why is the <code>docker-container</code> driver required for local multi-platform builds with Buildx?</strong></summary>

The default `docker` driver is embedded inside the standard Docker daemon, which historically uses the local storage graph driver and can only hold images matching the single CPU architecture of the host OS. 
The `docker-container` driver runs a full-featured, dedicated `buildkitd` daemon inside an isolated container. It can build multi-platform manifest lists (e.g., AMD64 and ARM64 concurrently via QEMU or binfmt_misc) and export multi-architecture OCI indexes directly to remote registries or tarballs.
</details>

<details>
<summary><strong>4. What is the difference between <code>COPY</code> and <code>--mount=type=bind</code> in a modern Dockerfile?</strong></summary>

- **`COPY`** reads files from the build context on the host and physically writes copies of those files into the container's filesystem layer, contributing to the resulting image layer size.
- **`--mount=type=bind`** temporarily mounts files from the build context (or an earlier stage) directly into the running build container for the duration of a single `RUN` instruction. The files are readable during execution, but are **not copied into the layer**, saving disk space and layer generation overhead.
</details>

<details>
<summary><strong>5. How does SSH agent forwarding (<code>--mount=type=ssh</code>) work in BuildKit?</strong></summary>

`--mount=type=ssh` connects the container's `SSH_AUTH_SOCK` directly to the host machine's active OpenSSH authentication agent via a secure UNIX domain socket bridge. When Git runs inside the container (`git clone git@github.com:...`), it delegates cryptographic challenge signing directly to the host's SSH agent in memory. The container never possesses the private key file, and no credentials are ever written to disk or image layers.
</details>

<details>
<summary><strong>6. What is the purpose of BuildKit's <code>type=local</code> exporter?</strong></summary>

The `type=local` exporter (`docker buildx build --output type=local,dest=./dist .`) bypasses container image generation entirely. Instead of packaging the final stage into an image in Docker's content store, BuildKit extracts the raw directory contents of the final stage directly onto the host machine's filesystem. This is ideal for static site generators, compilers, and documentation builders that run inside containers but output host files.
</details>

<details>
<summary><strong>7. How does the <code># syntax=docker/dockerfile:1</code> directive decouple Dockerfile features from the Docker engine version?</strong></summary>

BuildKit treats the Dockerfile parser as a pluggable container image (`docker/dockerfile:1`). When BuildKit encounters this directive, it pulls the official frontend image from Docker Hub and executes it as a parser plugin. This allows developers to use brand-new Dockerfile language features (e.g., heredocs, cache mounts) on older Docker Engine daemons without waiting for enterprise system administrators to upgrade the host Docker installation.
</details>

<details>
<summary><strong>8. What is the function of the <code>--link</code> flag on <code>COPY</code> in BuildKit?</strong></summary>

`COPY --link` creates an independent, standalone snapshot layer for the copied files that is decoupled from preceding layer hashes. If preceding instructions change, Docker does not need to re-copy or re-commit the linked files; it simply links the existing layer blob directly into the manifest DAG, enabling instantaneous zero-copy builds.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Available Buildx Builders</strong></summary>

**Goal**: Inspect existing builder configurations and driver backends.

```bash
docker buildx ls
```
</details>

<details>
<summary><strong>Exercise 2: Create an Isolated `docker-container` Builder</strong></summary>

**Goal**: Provision a dedicated container-based builder instance.

```bash
# Create builder
docker buildx create --name lab-builder --driver docker-container --bootstrap --use

# Inspect running buildkitd container
docker ps --filter "name=buildx_buildkit_lab-builder"

# Switch back to default
docker buildx use default

# Cleanup
docker buildx rm lab-builder
```
</details>

<details>
<summary><strong>Exercise 3: Test Heredoc Inline File Generation</strong></summary>

**Goal**: Use heredoc syntax to embed a multi-line HTML page.

```bash
mkdir -p /tmp/heredoc-test && cd /tmp/heredoc-test

cat <<'EOF' > Dockerfile
# syntax=docker/dockerfile:1
FROM alpine
COPY <<'HTML' /var/www/index.html
<!DOCTYPE html>
<html>
<body>
  <h1>Rendered via BuildKit Heredoc!</h1>
</body>
</html>
HTML
CMD ["cat", "/var/www/index.html"]
EOF

docker build -t heredoc-demo .
docker run --rm heredoc-demo

# Cleanup
rm -rf /tmp/heredoc-test && docker rmi heredoc-demo
```
</details>

<details>
<summary><strong>Exercise 4: Test Local Exporter (`type=local`)</strong></summary>

**Goal**: Extract a generated report directly to host storage without creating an image.

```bash
mkdir -p /tmp/local-export && cd /tmp/local-export

cat <<'EOF' > Dockerfile
# syntax=docker/dockerfile:1
FROM alpine
RUN echo "Report Generated: $(date)" > /report.txt

FROM scratch
COPY --from=0 /report.txt /report.txt
EOF

docker buildx build --output type=local,dest=./output .
cat ./output/report.txt

# Cleanup
rm -rf /tmp/local-export
```
</details>

<details>
<summary><strong>Exercise 5: Test Bind Mount Execution (`--mount=type=bind`)</strong></summary>

**Goal**: Execute a script from context without copying it into the layer.

```bash
mkdir -p /tmp/bind-test && cd /tmp/bind-test

echo 'echo "Executed via ephemeral bind mount!"' > script.sh

cat <<'EOF' > Dockerfile
# syntax=docker/dockerfile:1
FROM alpine
RUN --mount=type=bind,target=/context \
    sh /context/script.sh > /output.txt
CMD ["cat", "/output.txt"]
EOF

docker build -t bind-demo .
docker run --rm bind-demo

# Cleanup
rm -rf /tmp/bind-test && docker rmi bind-demo
```
</details>

<details>
<summary><strong>Exercise 6: Inspect BuildKit Cache Records</strong></summary>

**Goal**: Query detailed BuildKit disk usage.

```bash
docker builder du
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                          BUILDKIT & BUILDX CHEAT SHEET                                |
+---------------------------------------------------------------------------------------+
| Enable Parser:         # syntax=docker/dockerfile:1 (Line 1 of Dockerfile)            |
| Create Builder:        docker buildx create --name bld --driver docker-container --use|
| Cache Mount:           RUN --mount=type=cache,target=/root/.cache ...                 |
| Secret Mount:          RUN --mount=type=secret,id=tok cat /run/secrets/tok            |
|                        docker build --secret id=tok,src=token.txt .                   |
| SSH Forwarding:        RUN --mount=type=ssh git clone ...                             |
|                        docker build --ssh default .                                   |
| Bind Mount:            RUN --mount=type=bind,target=/src ...                          |
| Heredoc Syntax:        COPY <<'EOF' /path/to/file ... EOF                             |
| Export to Host Disk:   docker buildx build --output type=local,dest=./out .           |
| Cross-Platform Build:  docker buildx build --platform linux/amd64,linux/arm64 --push .|
| Golden Rule:           Never bake credentials into images; use --mount=type=secret!   |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Docker Documentation: BuildKit Overview](https://docs.docker.com/build/buildkit/)
- [Moby Project: BuildKit Official Repository and Specifications](https://github.com/moby/buildkit)
- [Docker Documentation: Building with Docker Buildx](https://docs.docker.com/build/architecture/)
- [Docker Documentation: BuildKit Mount Types Reference](https://docs.docker.com/build/building/secrets/)

---

| [< Previous: 05-dockerignore.md](./05-dockerignore.md) | [Next: 07-multi-platform-images.md](./07-multi-platform-images.md) > |
| :--- | :--- |
