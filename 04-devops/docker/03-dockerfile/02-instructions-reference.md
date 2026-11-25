---
title: "Exhaustive Dockerfile Instructions Reference: All 18 Directives Mastered"
description: "Comprehensive technical reference for all 18 Dockerfile instructions: FROM, RUN, CMD, ENTRYPOINT, COPY, ADD, ENV, ARG, WORKDIR, EXPOSE, VOLUME, USER, LABEL, STOPSIGNAL, HEALTHCHECK, SHELL, ONBUILD, and MAINTAINER."
category: "DevOps / Containers"
tags: ["docker", "dockerfile", "reference", "entrypoint", "cmd", "copy", "add", "healthcheck", "arg", "env"]
difficulty: "Advanced"
prerequisites: ["03-dockerfile/01-dockerfile-basics.md", "Understanding of Linux processes, filesystems, and signals"]
estimated_read_time: "30 min"
---

# Exhaustive Dockerfile Instructions Reference: All 18 Directives Mastered

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. The 18 Dockerfile Instructions at a Glance](#2-the-18-dockerfile-instructions-at-a-glance)
- [3. Deep Dive: The Execution Pair (`ENTRYPOINT` vs `CMD`)](#3-deep-dive-the-execution-pair-entrypoint-vs-cmd)
- [4. Deep Dive: Context Ingestion (`COPY` vs `ADD`)](#4-deep-dive-context-ingestion-copy-vs-add)
- [5. Deep Dive: Scoping Variables (`ARG` vs `ENV`)](#5-deep-dive-scoping-variables-arg-vs-env)
- [6. Deep Dive: Health, Lifecycle & Signals (`HEALTHCHECK`, `STOPSIGNAL`, `VOLUME`)](#6-deep-dive-health-lifecycle--signals-healthcheck-stopsignal-volume)
- [7. Complete Reference of Remaining Instructions (`WORKDIR`, `USER`, `EXPOSE`, `LABEL`, `SHELL`, `ONBUILD`, `MAINTAINER`)](#7-complete-reference-of-remaining-instructions)
- [8. Practical Diagnostic Demonstrations](#8-practical-diagnostic-demonstrations)
- [9. Best Practices & Production Guidelines](#9-best-practices--production-guidelines)
- [10. Common Pitfalls & Anti-Patterns](#10-common-pitfalls--anti-patterns)
- [11. Technical Interview Questions](#11-technical-interview-questions)
- [12. Hands-On Practice Exercises](#12-hands-on-practice-exercises)
- [13. Summary / Cheat Sheet](#13-summary--cheat-sheet)

---

## 1. What It Is & Why It Matters

Every behavior exhibited by a container—how it starts, how it shuts down, what user runs its code, what network ports it documents, how its health is verified, and how its filesystem is laid out—originates from the instructions defined in its `Dockerfile`.

The Docker specification defines **18 distinct instructions**. While beginners typically rely on just four or five (`FROM`, `COPY`, `RUN`, `CMD`), authoring production-grade, enterprise-hardened containers requires knowing the exact nuances of all eighteen:
- Knowing when to pair `ENTRYPOINT` with `CMD` to create CLI-like container utilities.
- Understanding why `COPY` is preferred over `ADD` for local files, but why modern BuildKit `ADD --checksum` is revolutionary for remote dependencies.
- Managing variable scoping rules for `ARG` before and after the initial `FROM` line.
- Implementing container self-healing using native `HEALTHCHECK` directives.

```
+---------------------------------------------------------------------------------+
|                       THE 18 DOCKERFILE INSTRUCTIONS                            |
|                                                                                 |
|  [Foundational Setup]      [Filesystem Mutation]        [Environment & State]   |
|  - FROM                    - COPY                       - ENV                   |
|  - WORKDIR                 - ADD                        - ARG                   |
|  - USER                    - RUN                        - VOLUME                |
|                                                                                 |
|  [Runtime Execution]       [Observability & Signals]    [Meta & Advanced]       |
|  - ENTRYPOINT              - EXPOSE                     - LABEL                 |
|  - CMD                     - HEALTHCHECK                - SHELL                 |
|                            - STOPSIGNAL                 - ONBUILD               |
|                                                         - MAINTAINER (Deprec)   |
+---------------------------------------------------------------------------------+
```

---

## 2. The 18 Dockerfile Instructions at a Glance

| Instruction | Primary Role | Creates Layer? | Runtime Overridable? |
|:---|:---|:---|:---|
| **`FROM`** | Sets base image for stage | Yes (Base rootfs)| Via `--build-arg` |
| **`RUN`** | Executes build commands | **Yes** (Filesystem diff)| No |
| **`CMD`** | Default arguments for container | No (Metadata)| Via CLI arguments |
| **`ENTRYPOINT`** | Primary executable binary | No (Metadata)| Via `--entrypoint` |
| **`WORKDIR`** | Current working directory | If directory created | Via `-w / --workdir` |
| **`COPY`** | Ingests context files | **Yes** (Filesystem diff)| No |
| **`ADD`** | Ingests context/URLs/tar | **Yes** (Filesystem diff)| No |
| **`ENV`** | Persistent environment vars | No (Metadata)| Via `-e / --env` |
| **`ARG`** | Build-time variables | No (Metadata)| Via `--build-arg` |
| **`EXPOSE`** | Documents network ports | No (Metadata)| Via `-p / -P` |
| **`VOLUME`** | Declares persistent mount | No (Metadata)| Via `-v / --volume` |
| **`USER`** | Sets UID:GID for execution | No (Metadata)| Via `-u / --user` |
| **`LABEL`** | Arbitrary key-value metadata| No (Metadata)| Via `--label` |
| **`STOPSIGNAL`** | System shutdown signal | No (Metadata)| Via `--stop-signal` |
| **`HEALTHCHECK`**| Container health probe | No (Metadata)| Via `--no-healthcheck`|
| **`SHELL`** | Default shell for `RUN` | No (Metadata)| No |
| **`ONBUILD`** | Trigger for downstream child| No (Metadata)| No |
| **`MAINTAINER`** | Author info (**Deprecated**)| No (Metadata)| No (Use `LABEL`) |

---

## 3. Deep Dive: The Execution Pair (`ENTRYPOINT` vs `CMD`)

The interaction between `ENTRYPOINT` and `CMD` is the single most misunderstood concept in Dockerfile authoring.

### 3.1. Mental Model: Executable vs Arguments
Think of the final command executed by the container as a simple equation:

$$\text{Final Container Command} = \text{ENTRYPOINT} + \text{CMD}$$

- **`ENTRYPOINT`**: The fixed binary executable you want the container to run (e.g., `["python", "manage.py"]` or `["nginx"]`).
- **`CMD`**: The default arguments passed to that executable (e.g., `["runserver", "0.0.0.0:8000"]`).

```dockerfile
ENTRYPOINT ["git"]
CMD ["--help"]
```
- If run without arguments (`docker run git-cli`): executes `git --help`.
- If run with arguments (`docker run git-cli status`): executes `git status`! The CLI argument overrides `CMD`, but `ENTRYPOINT` remains fixed.

### 3.2. Matrix of `ENTRYPOINT` and `CMD` Interactions

| Dockerfile Setting | No `ENTRYPOINT` | `ENTRYPOINT ["exec_p1"]` (Exec Form) | `ENTRYPOINT exec_p1` (Shell Form) |
|:---|:---|:---|:---|
| **No `CMD`** | Error (Must have CMD or ENTRYPOINT) | `exec_p1` | `/bin/sh -c exec_p1` |
| **`CMD ["cmd_p1", "cmd_p2"]`** | `cmd_p1 cmd_p2` | `exec_p1 cmd_p1 cmd_p2` | `/bin/sh -c exec_p1` (CMD ignored!) |
| **`CMD cmd_p1` (Shell Form)** | `/bin/sh -c cmd_p1` | `exec_p1 /bin/sh -c cmd_p1` | `/bin/sh -c exec_p1` (CMD ignored!) |

> **Critical Production Rule**: Always use the **JSON Exec Form** for both `ENTRYPOINT` and `CMD`. Shell forms wrap execution in `/bin/sh -c`, swallowing system signals (`SIGTERM`) and ignoring `CMD` overrides completely.

---

## 4. Deep Dive: Context Ingestion (`COPY` vs `ADD`)

Both instructions copy files into the image, but they have fundamentally different capabilities:

### 4.1. `COPY` (The Transparent Standard)
- Copies files and directories strictly from the local build context.
- **Rule**: Always prefer `COPY` over `ADD` for standard application files. It is transparent, predictable, and does not perform unexpected side-effect transformations.
- **Modern Flags**:
  - `--chown=<user>:<group>`: Sets ownership during copy.
  - `--chmod=<mode>`: Sets file permissions (e.g., `--chmod=0755`) without running a separate `chmod` layer!
  - `--link`: BuildKit feature that links layers independently of preceding layer caches.

### 4.2. `ADD` (The Magic Extractor & Downloader)
`ADD` has two unique "magic" behaviors that `COPY` lacks:
1. **Auto-Tar Extraction**: If `<src>` is a recognized local tar archive (tar, gzip, bzip2, xz), `ADD` automatically decompresses and unpacks it directly into the target directory.
2. **Remote URL Download**: If `<src>` is an HTTP/HTTPS URL, `ADD` downloads the file directly into the image.
   - *Modern BuildKit Feature*: `ADD --checksum=sha256:<hash> https://... /dest` validates the cryptographic hash of remote files during build, guaranteeing supply-chain integrity!

---

## 5. Deep Dive: Scoping Variables (`ARG` vs `ENV`)

```
+---------------------------------------------------------------------------------+
|                         ARG VS ENV SCOPING BOUNDARIES                           |
|                                                                                 |
|  [docker build --build-arg VERSION=3.12]                                        |
|        |                                                                        |
|  ARG VERSION=3.12 (Global scope, before FROM)                                   |
|        |                                                                        |
|  FROM python:${VERSION}-alpine                                                  |
|  +---------------------------------------------------------------------------+  |
|  | Build Stage Scope:                                                        |  |
|  | (Global ARG is lost unless re-declared!)                                  |  |
|  |                                                                           |  |
|  | ARG VERSION                     <-- Re-declare to access inside stage     |  |
|  | ENV APP_VERSION=${VERSION}     <-- Baked into running container!         |  |
|  | RUN echo "Building $VERSION"                                              |  |
|  +---------------------------------------------------------------------------+  |
|        |                                                                        |
|        v [docker run my-app]                                                    |
|  Inside Running Container:                                                      |
|  - $VERSION is NOT accessible (ARG expired)                                     |
|  - $APP_VERSION IS accessible (ENV persisted!)                                  |
+---------------------------------------------------------------------------------+
```

### Key Differences:
- **`ARG`**: Accessible **only during build execution**. Never available inside the running container. Values can be passed via `--build-arg`.
- **`ENV`**: Accessible **both during build and inside running containers**. Values persist permanently in container environment metadata. Can be overridden at runtime via `docker run -e`.

---

## 6. Deep Dive: Health, Lifecycle & Signals (`HEALTHCHECK`, `STOPSIGNAL`, `VOLUME`)

### 6.1. `HEALTHCHECK` (Container Liveness Probes)
Instructs Docker how to test if the application inside the container is genuinely functioning:
```dockerfile
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
  CMD curl -f http://localhost:8080/health || exit 1
```
- **Exit Code 0**: Healthy.
- **Exit Code 1**: Unhealthy.
- `start-period`: Grace period during boot where probe failures do not count against the retry threshold (essential for slow-booting Java/Spring Boot apps).

### 6.2. `STOPSIGNAL`
Defines the system call signal sent to PID 1 during `docker stop`. Default is `SIGTERM`.
```dockerfile
STOPSIGNAL SIGQUIT
```
- Nginx requires `SIGQUIT` for graceful stop.
- Systemd requires `SIGRTMIN+3`.

### 6.3. `VOLUME` (Managed Mount Point)
Declares an anonymous volume mount point:
```dockerfile
VOLUME ["/var/lib/postgresql/data"]
```
- **The Immutability Warning**: Any build instructions that attempt to write to `/var/lib/postgresql/data` *after* the `VOLUME` instruction is declared will be discarded by the builder! Always populate directories before declaring them as `VOLUME`.

---

## 7. Complete Reference of Remaining Instructions

### 7.1. `WORKDIR`
Sets working directory. Creates parent directories automatically. Always use absolute paths.

### 7.2. `USER`
Switches execution identity to an unprivileged user (UID:GID). All subsequent `RUN`, `CMD`, and `ENTRYPOINT` instructions execute with this identity:
```dockerfile
USER appuser:appgroup
```

### 7.3. `LABEL`
Adds arbitrary key-value metadata adhering to the OCI Image Specification annotations standard:
```dockerfile
LABEL org.opencontainers.image.title="Payment Gateway" \
      org.opencontainers.image.version="2.4.0" \
      org.opencontainers.image.authors="platform-team@acme.corp"
```

### 7.4. `SHELL`
Overrides the default shell used by shell form `RUN`, `CMD`, and `ENTRYPOINT`:
- Linux Default: `SHELL ["/bin/sh", "-c"]`
- Change to Bash: `SHELL ["/bin/bash", "-o", "pipefail", "-c"]`
- Windows Default: `SHELL ["cmd", "/S", "/C"]` or `SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop';"]`

### 7.5. `ONBUILD`
Registers a trigger instruction that is executed **only when the image is used as a base image for another downstream build** (`FROM this-image`):
```dockerfile
ONBUILD COPY . /app/src
ONBUILD RUN npm install
```

### 7.6. `MAINTAINER` (Deprecated)
Historically set author information. Deprecated since Docker 1.13. **Do not use.** Use `LABEL org.opencontainers.image.authors="..."` instead.

---

## 8. Practical Diagnostic Demonstrations

### Example 1: Creating a CLI Utility with `ENTRYPOINT` + `CMD`
Create a portable `cURL` utility image that accepts arbitrary arguments:

```dockerfile
FROM alpine:3.20
RUN apk add --no-cache curl
ENTRYPOINT ["curl"]
CMD ["--help"]
```
Build and run:
```bash
docker build -t my-curl .

# Running with no arguments executes CMD: curl --help
docker run --rm my-curl

# Passing an argument overrides CMD: curl -sI https://example.com
docker run --rm my-curl -sI https://example.com
```

### Example 2: Verifying BuildKit `ADD --checksum` for Supply-Chain Security
Download a verified remote binary directly during build without curling and manually hashing:

```dockerfile
# syntax=docker/dockerfile:1
FROM alpine:3.20

# Download Tini and verify cryptographic hash atomically during build!
ADD --chmod=0755 \
    --checksum=sha256:e68f1937ff665348e652d2116b3faaf99c5feb7f75971029b66309e6c3079d3e \
    https://github.com/krallin/tini/releases/download/v0.19.0/tini-amd64 \
    /sbin/tini

ENTRYPOINT ["/sbin/tini", "--"]
CMD ["echo", "Tini verified and ready!"]
```

### Example 3: Global vs Stage `ARG` Scoping Demonstration
Demonstrate the scoping boundary across `FROM`:

```dockerfile
ARG BASE_TAG=3.20
FROM alpine:${BASE_TAG}

# BASE_TAG is out of scope here! We must re-declare it:
ARG BASE_TAG
ENV RUNTIME_TAG=${BASE_TAG}

CMD ["sh", "-c", "echo Base Alpine tag was: $RUNTIME_TAG"]
```

### Example 4: Hardening with `SHELL ["/bin/bash", "-o", "pipefail", "-c"]`
Demonstrate how `pipefail` catches silent failures in shell pipelines:

```dockerfile
FROM debian:bookworm-slim

# Enforce pipefail so piped commands fail if the first command fails!
SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# If wget fails (404), the entire RUN command fails immediately instead of passing silently to tar!
RUN wget -qO- https://example.com/nonexistent.tar.gz | tar -xz -C /tmp
```

### Example 5: Implementing a Robust Microservice `HEALTHCHECK`
Configure a health probe with startup grace periods:

```dockerfile
FROM nginx:alpine

# Healthcheck testing HTTP endpoint every 5s with 3s timeout
HEALTHCHECK --interval=5s --timeout=3s --start-period=5s --retries=2 \
  CMD wget -qO- http://localhost:80/ || exit 1
```

---

## 9. Best Practices & Production Guidelines

1. **Always Use JSON Exec Form for `ENTRYPOINT` and `CMD`**: Never use string form. Exec form ensures proper signal handling and argument overrides.
2. **Favor `COPY` over `ADD` for Local Files**: Use `ADD` strictly when you need auto-tar extraction or BuildKit verified checksum downloads (`ADD --checksum=...`).
3. **Always Run as Non-Root**: Define `USER appuser:appgroup` before your `CMD` instruction.
4. **Enable `pipefail` on Bash Shells**: When using shell pipelines in Debian/Ubuntu images, set `SHELL ["/bin/bash", "-o", "pipefail", "-c"]` so failing commands inside pipes (`curl ... | tar ...`) fail the build.
5. **Always Set `start-period` on `HEALTHCHECK`**: Avoid killing slow-starting JVM or database applications during boot by granting an adequate initial grace period (`--start-period=30s`).

---

## 10. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Proper Solution |
|:---|:---|:---|:---|
| **Using `ADD` to fetch remote tarballs and extracting manually** | Running `ADD url.tar.gz .` then `RUN tar -xzf`. | Downloads tarball into layer, then duplicates unpacked files in second layer. | Use `ADD` directly (auto-extracts if local) or use `RUN curl ... \| tar ...`. |
| **Writing to a `VOLUME` path in later layers** | Declaring `VOLUME /data` in line 4, running `RUN echo "init" > /data/file` in line 8. | The write operation is discarded by the builder; `/data` remains empty. | Populate files before declaring `VOLUME`. |
| **Referencing pre-`FROM` `ARG` inside a build stage** | Expecting `ARG VERSION` before `FROM` to be visible in `RUN`. | Variable evaluates to empty string. | Re-declare `ARG VERSION` inside the stage. |
| **Using `MAINTAINER` in modern Dockerfiles** | Deprecated instruction. | Lacks OCI compliance; produces linter warnings. | Use `LABEL org.opencontainers.image.authors="..."`. |
| **Missing `STOPSIGNAL SIGQUIT` in Nginx images** | Nginx shutting down abruptly. | Active HTTP connections dropped on `docker stop`. | Add `STOPSIGNAL SIGQUIT` to Dockerfile. |
| **Using Shell Form for `ENTRYPOINT` with arguments** | Writing `ENTRYPOINT /app/bin` and expecting `CMD` to pass flags. | Shell form ignores all CLI arguments and `CMD` overrides completely. | Use JSON Exec Form: `ENTRYPOINT ["/app/bin"]`. |

---

## 11. Technical Interview Questions

<details>
<summary><strong>1. Explain the exact mechanism by which Docker executes a container when both <code>ENTRYPOINT</code> and <code>CMD</code> are declared in exec form.</strong></summary>

When both are specified in exec form:
- `ENTRYPOINT` defines the executable binary (e.g., `["/bin/app"]`).
- `CMD` defines the default parameters passed to that binary (e.g., `["--port", "8080"]`).
Docker merges the two arrays: `["/bin/app", "--port", "8080"]`.
If the user passes arguments on the command line (`docker run my-image --port 9000`), the CLI arguments completely replace the `CMD` array while preserving the `ENTRYPOINT`, resulting in `["/bin/app", "--port", "9000"]`.
</details>

<details>
<summary><strong>2. What is the difference between <code>COPY</code> and <code>ADD</code>, and why is <code>COPY</code> generally preferred?</strong></summary>

- `COPY` copies files and directories strictly from the local build context into the container filesystem. It is simple, explicit, and transparent.
- `ADD` has two additional implicit behaviors:
  1. It automatically decompresses and extracts local tar archives.
  2. It can fetch files from remote URLs.
`COPY` is preferred for standard application code because `ADD`'s automatic decompression can lead to unexpected side effects (e.g., accidentally unpacking a `.tar.gz` archive that was intended to remain compressed).
</details>

<details>
<summary><strong>3. Explain the scoping rules of an <code>ARG</code> declared before the <code>FROM</code> line.</strong></summary>

An `ARG` declared before the `FROM` line is outside of any build stage. It is available **only** for use inside `FROM` instructions (e.g., setting dynamic base image tags: `FROM python:${VERSION}-slim`). Once the `FROM` line executes, that `ARG` falls out of scope and cannot be used in `RUN`, `ENV`, or other stage instructions unless explicitly re-declared inside the stage without a default value:
```dockerfile
ARG VERSION=3.12
FROM python:${VERSION}-alpine
ARG VERSION
RUN echo "Version is: $VERSION"
```
</details>

<details>
<summary><strong>4. What occurs if a Dockerfile attempts to modify files in a directory after declaring it with <code>VOLUME</code>?</strong></summary>

The `VOLUME` instruction instructs the build engine that the specified path represents an external, persistent storage mount. Any subsequent `RUN` instructions that attempt to write, modify, or delete files inside that volume directory will execute in a temporary mount that is **discarded immediately upon completion of the instruction**. The changes will not be committed to the resulting image layer. All initial files must be placed in the directory *before* declaring `VOLUME`.
</details>

<details>
<summary><strong>5. What is the purpose of the <code>SHELL</code> instruction, and why is it especially critical on Windows containers?</strong></summary>

The `SHELL` instruction overrides the default shell binary used for evaluating shell form `RUN`, `CMD`, and `ENTRYPOINT` instructions. On Linux, it allows switching from default `/bin/sh` to `/bin/bash` with strict error handling (`-o pipefail`). On Windows containers, the default shell is legacy `cmd.exe`. Setting `SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop';"]` enables modern PowerShell scripting with strict error traps.
</details>

<details>
<summary><strong>6. How does <code>ONBUILD</code> work, and what is a classic use case?</strong></summary>

`ONBUILD` registers a "trigger" instruction in the image metadata. The instruction is **not** executed during the build of the current Dockerfile. Instead, it is executed later, as if it were inserted immediately after the `FROM` instruction of any **downstream child Dockerfile** that uses this image as its base. A classic use case is creating standardized corporate base images for languages (e.g., a Maven base image that runs `ONBUILD COPY pom.xml .` and `ONBUILD RUN mvn dependency:go-offline` whenever a development team bases their app on it).
</details>

<details>
<summary><strong>7. Why was the <code>MAINTAINER</code> instruction deprecated, and what replaces it?</strong></summary>

`MAINTAINER` was a single-purpose instruction that simply stored author information. It was deprecated in Docker 1.13 because the `LABEL` instruction provides a universal, extensible mechanism for storing arbitrary key-value metadata. Authors should use standard OCI annotations via labels:
```dockerfile
LABEL org.opencontainers.image.authors="Name <email@corp.com>"
```
</details>

<details>
<summary><strong>8. What are the four configuration parameters of a <code>HEALTHCHECK</code> instruction?</strong></summary>

1. **`--interval`** (default `30s`): The time elapsed between consecutive health probe executions.
2. **`--timeout`** (default `30s`): The maximum duration allowed for a single probe command to complete before it is marked as failed.
3. **`--start-period`** (default `0s`): A bootstrap grace period during which probe failures do not count toward the failure threshold.
4. **`--retries`** (default `3`): The number of consecutive probe failures required for the container state to transition from `healthy` to `unhealthy`.
</details>

---

## 12. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Specialized CLI Container with `ENTRYPOINT` + `CMD`</strong></summary>

**Goal**: Package `jq` as a standalone command-line tool.

```bash
mkdir -p /tmp/jq-lab && cd /tmp/jq-lab

cat <<'EOF' > Dockerfile
FROM alpine:3.20
RUN apk add --no-cache jq
ENTRYPOINT ["jq"]
CMD ["--help"]
EOF

docker build -t my-jq .

# Test default help
docker run --rm my-jq

# Test JSON filtering override
echo '{"status": "ready", "code": 200}' | docker run --rm -i my-jq '.status'

# Cleanup
rm -rf /tmp/jq-lab && docker rmi my-jq
```
</details>

<details>
<summary><strong>Exercise 2: Test BuildKit `ADD --chmod` Feature</strong></summary>

**Goal**: Copy an executable shell script and set permissions atomically during build.

```bash
mkdir -p /tmp/chmod-lab && cd /tmp/chmod-lab

# Create script WITHOUT execute permissions
echo 'echo "Executed successfully!"' > run.sh
chmod 644 run.sh

# Use BuildKit COPY --chmod=0755
cat <<'EOF' > Dockerfile
# syntax=docker/dockerfile:1
FROM alpine
WORKDIR /app
COPY --chmod=0755 run.sh .
CMD ["./run.sh"]
EOF

docker build -t chmod-demo .
docker run --rm chmod-demo

# Cleanup
rm -rf /tmp/chmod-lab && docker rmi chmod-demo
```
</details>

<details>
<summary><strong>Exercise 3: Inspect OCI Image Annotations Added via `LABEL`</strong></summary>

**Goal**: Inspect custom labels via `docker inspect`.

```bash
cat <<'EOF' > /tmp/label.dockerfile
FROM alpine
LABEL org.opencontainers.image.version="1.0.0" \
      maintainer="platform@corp.internal"
EOF

docker build -f /tmp/label.dockerfile -t label-lab /tmp
docker inspect --format '{{json .Config.Labels}}' label-lab | python3 -m json.tool

# Cleanup
rm -f /tmp/label.dockerfile && docker rmi label-lab
```
</details>

<details>
<summary><strong>Exercise 4: Test Custom `STOPSIGNAL`</strong></summary>

**Goal**: Verify that Docker sends `SIGINT` on `docker stop` when configured via `STOPSIGNAL`.

```bash
cat <<'EOF' > /tmp/sig.dockerfile
FROM alpine
STOPSIGNAL SIGINT
CMD ["sleep", "100"]
EOF

docker build -f /tmp/sig.dockerfile -t sig-lab /tmp
docker inspect --format 'StopSignal: {{.Config.StopSignal}}' sig-lab

# Cleanup
rm -f /tmp/sig.dockerfile && docker rmi sig-lab
```
</details>

<details>
<summary><strong>Exercise 5: Build Downstream Child Using `ONBUILD`</strong></summary>

**Goal**: Create a base image with `ONBUILD` and observe child execution.

```bash
# 1. Build Base
echo "FROM alpine\nONBUILD RUN echo 'Triggered ONBUILD in child!'" | docker build -t onbuild-base -

# 2. Build Child
echo "FROM onbuild-base" | docker build -t onbuild-child -

# Cleanup
docker rmi onbuild-child onbuild-base
```
</details>

<details>
<summary><strong>Exercise 6: Test Changing Default Shell with `SHELL`</strong></summary>

**Goal**: Switch to `/bin/sh -e` to ensure commands fail immediately on first error.

```bash
cat <<'EOF' > /tmp/shell.dockerfile
FROM alpine
SHELL ["/bin/sh", "-e", "-c"]
RUN echo "Step 1 OK" && echo "Step 2 OK"
EOF

docker build -f /tmp/shell.dockerfile -t shell-lab /tmp
docker rmi shell-lab && rm -f /tmp/shell.dockerfile
```
</details>

---

## 13. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                       18 DOCKERFILE INSTRUCTIONS CHEAT SHEET                          |
+---------------------------------------------------------------------------------------+
| FROM:                  FROM <image>:<tag> [AS <stage>]                                |
| WORKDIR:               WORKDIR /app (Absolute path!)                                  |
| COPY:                  COPY [--chown=u:g] [--chmod=755] <src> <dest>                  |
| ADD:                   ADD <tar.gz> <dest> (Auto-extracts) / ADD --checksum=... <url> |
| RUN:                   RUN ["exec", "arg"] OR RUN cmd1 && cmd2                        |
| ENV:                   ENV KEY=VAL (Persists in running container)                    |
| ARG:                   ARG KEY=VAL (Build-time only; expires after build)             |
| EXPOSE:                EXPOSE 8080 (Documentation only!)                              |
| VOLUME:                VOLUME ["/data"] (External mount declaration)                  |
| USER:                  USER appuser:appgroup (Drops root privileges)                  |
| LABEL:                 LABEL key="val" (OCI annotations)                              |
| ENTRYPOINT:            ENTRYPOINT ["executable"] (Fixed binary)                       |
| CMD:                   CMD ["param1", "param2"] (Default overridable arguments)       |
| STOPSIGNAL:            STOPSIGNAL SIGQUIT / SIGTERM                                   |
| HEALTHCHECK:           HEALTHCHECK --interval=30s --timeout=5s CMD curl || exit 1     |
| SHELL:                 SHELL ["/bin/bash", "-o", "pipefail", "-c"]                    |
| ONBUILD:               ONBUILD RUN ... (Triggers on downstream child builds)          |
| MAINTAINER:            DEPRECATED -> Use LABEL instead                                |
+---------------------------------------------------------------------------------------+
```

---

| [< Previous: 01-dockerfile-basics.md](./01-dockerfile-basics.md) | [Next: 03-layers-and-caching.md](./03-layers-and-caching.md) > |
| :--- | :--- |
