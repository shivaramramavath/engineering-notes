---
title: "Multi-Platform Container Images: Cross-Compilation, QEMU, and Buildx Architectures"
description: "Mastering multi-architecture container images: solving exec format errors, OCI Image Indexes, QEMU emulation via binfmt_misc, native Go/Rust cross-compilation, and distributed Buildx nodes."
category: "DevOps / Containers"
tags: ["docker", "multi-platform", "arm64", "amd64", "buildx", "cross-compilation", "qemu", "binfmt"]
difficulty: "Advanced"
prerequisites: ["01-fundamentals/07-registries-and-image-naming.md", "03-dockerfile/06-buildkit-and-buildx.md"]
estimated_read_time: "25 min"
---

# Multi-Platform Container Images: Cross-Compilation, QEMU, and Buildx Architectures

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. The Underlying Crisis: `exec format error`](#2-the-underlying-crisis-exec-format-error)
- [3. Anatomy of an OCI Multi-Architecture Index](#3-anatomy-of-an-oci-multi-architecture-index)
- [4. The Three Multi-Platform Build Strategies](#4-the-three-multi-platform-build-strategies)
- [5. Automatic Platform Arguments (`TARGETPLATFORM`, `BUILDPLATFORM`)](#5-automatic-platform-arguments-targetplatform-buildplatform)
- [6. Practical Diagnostic Demonstrations](#6-practical-diagnostic-demonstrations)
- [7. Best Practices & Production Guidelines](#7-best-practices--production-guidelines)
- [8. Common Pitfalls & Anti-Patterns](#8-common-pitfalls--anti-patterns)
- [9. Comparison Table: Multi-Platform Strategies](#9-comparison-table-multi-platform-strategies)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

The computing world is no longer monocultural.

A decade ago, virtually all developer laptops, CI/CD runners, and cloud production servers shared a single CPU architecture: **Intel/AMD x86_64 (`linux/amd64`)**. 

Today, production engineering takes place across heterogeneous CPU architectures:
- **Developer Workstations**: Millions of software engineers write code on Apple Silicon Macs (M1/M2/M3/M4) powered by **ARM64 (`linux/arm64/v8`)**.
- **Cloud Infrastructure**: Cloud providers offer high-efficiency, cost-effective ARM-based instances (AWS Graviton, Google Tau T2A, Azure Ampere Altra) delivering up to $40\%$ better price-performance than x86.
- **Edge Computing & IoT**: Smart gateways, medical devices, and robotics run on **ARMv7** or **RISC-V**.
- **Enterprise Mainframes**: Financial mainframes execute on **IBM s390x** or **PowerPC (ppc64le)**.

If you build an image on an Apple Silicon laptop and push it to production without multi-architecture support, the cloud server will immediately crash. 

Modern DevOps teams must produce **Multi-Platform Images**: single, unified image tags (e.g., `my-service:1.0.0`) that automatically and transparently serve native machine code matching whatever CPU architecture pulls the image.

```
+---------------------------------------------------------------------------------+
|                       MULTI-PLATFORM COMPILATION & DELIVERY                     |
|                                                                                 |
|  [Developer on Apple Silicon (ARM64)]                                           |
|        |                                                                        |
|        | docker buildx build --platform linux/amd64,linux/arm64 -t app:v1       |
|        v                                                                        |
|  [OCI Multi-Architecture Index (Registry)]                                      |
|  +---------------------------------------------------------------------------+  |
|  | OCI Image Index (Manifest List: app:v1)                                  |  |
|  |  |-- Descriptors:                                                         |  |
|  |  |   |-- linux/amd64 -> Manifest SHA: 0x111... (x86_64 binary)            |  |
|  |  |   \-- linux/arm64 -> Manifest SHA: 0x222... (AArch64 binary)          |  |
|  +---------------------------------------------------------------------------+  |
|        |                                                   |                    |
|        | docker pull app:v1                                | docker pull app:v1 |
|        v                                                   v                    |
|  [AWS Graviton Server (ARM64)]                       [AWS Intel Server (AMD64)] |
|  - Pulls Manifest 0x222...                           - Pulls Manifest 0x111...  |
|  - Runs Native ARM64 Machine Code                    - Runs Native x86_64 Code  |
|  - 100% Native Speed / Zero Emulation                - 100% Native Speed        |
+---------------------------------------------------------------------------------+
```

---

## 2. The Underlying Crisis: `exec format error`

A compiled binary (such as an ELF binary in Linux) contains machine code instructions formatted for a specific CPU instruction set architecture (ISA). 

An x86_64 CPU understands CISC instructions (like `mov`, `push`, `jmp` formatted in x86 opcodes). An ARM64 CPU understands RISC instructions (like `ldr`, `str`, `b` formatted in 32-bit fixed opcodes).

If an x86_64 Linux kernel attempts to execute an ARM64 binary:
```bash
standard_init_linux.go:228: exec user process caused: exec format error
```
The Linux kernel system call `execve()` reads the binary's ELF header (`/bin/app`), detects an incompatible machine architecture (`EM_AARCH64` on an `EM_X86_64` CPU), refuses to execute, and terminates with exit code 126 or 127.

---

## 3. Anatomy of an OCI Multi-Architecture Index

When an image supports multiple architectures, the tag points to an **OCI Image Index** (historically called a "Manifest List" in Docker v2 Schema 2):

```json
{
  "schemaVersion": 2,
  "mediaType": "application/vnd.oci.image.index.v1+json",
  "manifests": [
    {
      "mediaType": "application/vnd.oci.image.manifest.v1+json",
      "size": 7143,
      "digest": "sha256:7f3a91b2c4e5...",
      "platform": {
        "architecture": "amd64",
        "os": "linux"
      }
    },
    {
      "mediaType": "application/vnd.oci.image.manifest.v1+json",
      "size": 7143,
      "digest": "sha256:8b4c2e1a90d3...",
      "platform": {
        "architecture": "arm64",
        "os": "linux"
      }
    }
  ]
}
```
When a client runs `docker pull`, the Docker Engine inspects its host kernel architecture (`uname -m`), searches the index for a matching `platform` block, and downloads only the layers associated with that specific platform digest.

---

## 4. The Three Multi-Platform Build Strategies

There are three architectural methods for compiling multi-platform container images:

```
+---------------------------------------------------------------------------------+
|                       3 MULTI-PLATFORM BUILD STRATEGIES                         |
+-------------------------+-------------------------+-----------------------------+
| 1. QEMU USER EMULATION  | 2. NATIVE CROSS-COMPIL  | 3. DISTRIBUTED NATIVE NODES |
+-------------------------+-------------------------+-----------------------------+
| Host (ARM64)            | Host (ARM64)            | Master Builder (ARM64)      |
|   |                     |   |                     |   |-- (SSH) --> Node 1(ARM64|
|   v                     |   v                     |   \-- (SSH) --> Node 2(AMD64|
| [QEMU binfmt_misc]      | [Go/Rust Cross-Compiler]|                             |
| Translates x86 opcodes  | Runs natively on ARM64, | Work dispatched to native   |
| instruction by instr.   | outputs x86_64 binary!  | physical hardware!          |
|                         |                         |                             |
| Pros: Works anywhere    | Pros: Blazing fast      | Pros: Maximum possible speed|
| Cons: 5x-10x SLOWER     | Cons: Language-specific | Cons: Requires extra servers|
+-------------------------+-------------------------+-----------------------------+
```

### Strategy 1: QEMU User-Mode Emulation (`binfmt_misc`)
The Linux kernel feature `binfmt_misc` allows registering userspace interpreters for non-native binary formats. 
- When an ARM64 host attempts to run an x86_64 binary during `docker build`, the kernel intercepts the execution and invokes **`qemu-x86_64`**.
- QEMU translates every x86 CPU instruction into ARM64 instructions on the fly.
- **Drawback**: Emulation is CPU-intensive. Compiling C++, Rust, or heavy Node packages under QEMU is **$5\times\text{ to }10\times$ slower** than native execution.

### Strategy 2: Native Cross-Compilation (Go, Rust, C++)
Modern compilers (especially Go and Rust) can run natively on an ARM64 CPU and directly emit machine code for an AMD64 CPU in seconds without QEMU emulation!
- In Go: `GOOS=linux GOARCH=amd64 go build` running on an M3 Mac takes 3 seconds, whereas running under QEMU takes 45 seconds.

### Strategy 3: Distributed Buildx Nodes over Remote SSH
Buildx can connect to multiple remote physical machines simultaneously:
- Machine 1: An ARM64 instance (e.g., an AWS Graviton server).
- Machine 2: An AMD64 instance (e.g., an Intel Xeon server).
Buildx dispatches the ARM64 stage to Machine 1 and the AMD64 stage to Machine 2, assembling the final multi-architecture manifest list in the registry at 100% native hardware speed.

---

## 5. Automatic Platform Arguments (`TARGETPLATFORM`, `BUILDPLATFORM`)

BuildKit automatically injects several predefined `ARG` variables into every build stage to facilitate cross-compilation:

```
+---------------------------------------------------------------------------------+
|                       BUILDKIT PLATFORM BUILT-IN ARGS                           |
+-------------------+-------------------------------+-----------------------------+
| VARIABLE          | DESCRIPTION                   | EXAMPLE VALUE               |
+-------------------+-------------------------------+-----------------------------+
| `TARGETPLATFORM`  | Target OS and architecture    | `linux/amd64`, `linux/arm64`|
+-------------------+-------------------------------+-----------------------------+
| `TARGETOS`        | Operating system of target    | `linux`, `windows`          |
+-------------------+-------------------------------+-----------------------------+
| `TARGETARCH`      | Architecture of target        | `amd64`, `arm64`, `arm`     |
+-------------------+-------------------------------+-----------------------------+
| `TARGETVARIANT`   | Architecture variant          | `v7`, `v8`                  |
+-------------------+-------------------------------+-----------------------------+
| `BUILDPLATFORM`   | OS/Arch of the host building  | `linux/arm64` (on Mac M3)   |
+-------------------+-------------------------------+-----------------------------+
| `BUILDARCH`       | Architecture of host builder  | `arm64`                     |
+-------------------+-------------------------------+-----------------------------+
```

### High-Performance Go Cross-Compilation Pattern:
```dockerfile
# syntax=docker/dockerfile:1
# Force builder stage to run NATIVELY on the host CPU (No QEMU penalty!)
FROM --platform=$BUILDPLATFORM golang:1.22-alpine AS builder

WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download
COPY . .

# Automatically injected by BuildKit!
ARG TARGETOS
ARG TARGETARCH

# Go compiles natively for the target architecture in seconds!
RUN CGO_ENABLED=0 GOOS=$TARGETOS GOARCH=$TARGETARCH \
    go build -ldflags="-s -w" -o /bin/server .

# Final stage matches the target platform
FROM scratch
COPY --from=builder /bin/server /server
ENTRYPOINT ["/server"]
```

---

## 6. Practical Diagnostic Demonstrations

### Example 1: Inspecting Remote Multi-Arch Manifests with Buildx
Inspect the official Python image to verify multi-platform manifest coverage:

```bash
docker buildx imagetools inspect python:3.12-alpine
```
*Output:*
```text
Name:      docker.io/library/python:3.12-alpine
MediaType: application/vnd.docker.distribution.manifest.list.v2+json
Digest:    sha256:d8a9...

Manifests:
  Platform: linux/amd64
  Digest:   sha256:111...
  
  Platform: linux/arm64/v8
  Digest:   sha256:222...
  
  Platform: linux/386
  Digest:   sha256:333...
```

### Example 2: Verifying QEMU `binfmt_misc` Registration
Check if your Linux host has QEMU emulation registered for non-native binaries:

```bash
# Verify registered binfmt interpreters
ls -l /proc/sys/fs/binfmt_misc/ 2>/dev/null || echo "Running on non-Linux or VM"

# If missing, register QEMU handlers in one command:
# docker run --privileged --rm tonistiigi/binfmt --install all
```

### Example 3: Compiling a Multi-Platform Image Locally
Create a `docker-container` builder and build for both `linux/amd64` and `linux/arm64`:

```bash
# 1. Create container builder (Required for multi-arch buildx)
docker buildx create --name multi-builder --driver docker-container --use --bootstrap

# 2. Build multi-arch image
cat <<'EOF' > Dockerfile.multiarch
FROM alpine:3.20
ARG TARGETARCH
RUN echo "I am running on CPU architecture: $TARGETARCH" > /arch.txt
CMD ["cat", "/arch.txt"]
EOF

# Build for both platforms and export to tarball (or registry with --push)
docker buildx build \
  -f Dockerfile.multiarch \
  --platform linux/amd64,linux/arm64 \
  -t my-multiarch-demo:1.0 \
  .
```

### Example 4: Forcing Execution of a Foreign Architecture via Docker CLI
Run an AMD64 container on an Apple Silicon Mac (or vice-versa) to verify emulation:

```bash
# Force AMD64 execution
docker run --rm --platform linux/amd64 alpine uname -m
# Output: x86_64

# Force ARM64 execution
docker run --rm --platform linux/arm64 alpine uname -m
# Output: aarch64
```

### Example 5: Testing `BUILDPLATFORM` vs `TARGETPLATFORM`
Observe the values passed into the build engine:

```bash
docker buildx build --platform linux/amd64,linux/arm64 - <<'EOF'
FROM alpine
ARG BUILDPLATFORM
ARG TARGETPLATFORM
RUN echo "Building on $BUILDPLATFORM, targeting $TARGETPLATFORM"
EOF
```
*Notice BuildKit prints: `Building on linux/arm64, targeting linux/amd64` and `targeting linux/arm64` simultaneously!*

---

## 7. Best Practices & Production Guidelines

1. **Always Use `FROM --platform=$BUILDPLATFORM` for Compilers**: When using Go, Rust, or .NET in multi-stage builds, force the compiler stage to execute on the native host architecture (`--platform=$BUILDPLATFORM`). Let the compiler handle cross-compilation; never force the compiler to run under slow QEMU emulation.
2. **Build and Push Atomically (`--push`)**: Local Docker daemons cannot store multi-architecture manifest lists in their legacy content stores. Always push multi-platform images directly to a registry in a single atomic command:
   ```bash
   docker buildx build --platform linux/amd64,linux/arm64 -t reg.corp/app:1.0 --push .
   ```
3. **Pin Base Images with Multi-Arch Support**: Verify that every base image specified in your Dockerfile natively supports all target platforms using `docker buildx imagetools inspect <base_image>`.
4. **Avoid Architecture-Specific Package Names in `RUN`**: Avoid hardcoding `RUN apt install -y libssl-dev:amd64`. Use dynamic platform arguments or generic package names.
5. **Set Up Dedicated Native CI Runners for Heavy Builds**: If compiling massive C++ or Rust codebases for multiple architectures, do not rely on QEMU emulation in CI. Configure a distributed Buildx builder containing native physical AMD64 and ARM64 worker nodes.

---

## 8. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Proper Solution |
|:---|:---|:---|:---|
| **`exec format error` in production** | Built image on Apple Silicon without specifying platform; deployed to x86 cloud. | Production pods crash immediately upon startup. | Build for `linux/amd64` or publish multi-arch images. |
| **Running Go/Rust compilers under QEMU** | Omitting `--platform=$BUILDPLATFORM` in builder stage. | BuildKit runs Go compiler under QEMU emulation; build takes 15 minutes instead of 30 seconds. | Use `FROM --platform=$BUILDPLATFORM golang:...` and set `GOARCH=$TARGETARCH`. |
| **Building multi-arch without `--push` or custom exporter** | Running `docker buildx build --platform linux/amd64,linux/arm64 -t app .`. | Build finishes, but output cannot be saved to local `docker images` store. | Pass `--push` to upload to registry, or `--output type=tar` to export. |
| **Using `docker` driver for multi-arch builds** | Attempting multi-platform build with default embedded builder. | Fails with `multi-platform build is not supported for the docker driver`. | Create container builder: `docker buildx create --driver docker-container --use`. |
| **Missing `binfmt_misc` on Linux host** | Building foreign architectures without QEMU handlers installed. | Build fails with `cannot execute binary file: Exec format error` during `RUN`. | Run `docker run --privileged --rm tonistiigi/binfmt --install all`. |
| **Hardcoding architecture in URLs** | Writing `curl https://.../binary-x86_64` inside Dockerfile. | Fails when compiling for ARM64. | Use `${TARGETARCH}` interpolation: `curl https://.../binary-${TARGETARCH}`. |

---

## 9. Comparison Table: Multi-Platform Strategies

| Strategy | Performance / Build Speed | Implementation Complexity | Hardware Requirements | Best Suited For |
|:---|:---|:---|:---|:---|
| **QEMU Emulation (`binfmt_misc`)**| **Slow** ($5\times-10\times$ penalty) | Very Low (Zero Dockerfile changes) | Single Developer Machine | Simple interpreted apps (Python, Node, Ruby)|
| **Native Cross-Compilation** | **Ultra-Fast** (100% native speed) | Moderate (Requires `$TARGETARCH` args)| Single Developer Machine | Compiled languages (Go, Rust, Swift, .NET) |
| **Distributed Remote Builders** | **Ultra-Fast** (100% native speed) | High (Requires provisioning nodes) | Physical nodes for each architecture | Massive enterprise C++/Rust codebases in CI |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. What causes an <code>exec format error</code> when launching a container, and how is it resolved?</strong></summary>

An `exec format error` occurs when the Linux kernel's `execve()` system call attempts to execute a binary whose compiled machine code architecture (recorded in the ELF binary header) does not match the CPU instruction set architecture (ISA) of the host processor (e.g., executing an ARM64 binary on an AMD64 CPU). It is resolved by:
1. Rebuilding the image targeting the host's native architecture (`--platform linux/amd64`).
2. Running the container via QEMU user-mode emulation (`binfmt_misc`).
3. Publishing an OCI multi-architecture manifest list so the host pulls its matching binary automatically.
</details>

<details>
<summary><strong>2. How does an OCI Image Index resolve platform architectures at pull time?</strong></summary>

An OCI Image Index is a JSON manifest list containing an array of sub-manifest descriptors. Each descriptor explicitly specifies an operating system (`os`), a CPU architecture (`architecture`), an optional variant (`variant`), and the unique cryptographic SHA256 digest of that platform's specific image manifest. When a client executes `docker pull`, the Docker daemon inspects its local kernel architecture, queries the index, matches the exact architecture, and downloads only the layer blobs associated with that platform digest.
</details>

<details>
<summary><strong>3. Why is compiling Go code under QEMU considered an anti-pattern when BuildKit supports cross-compilation?</strong></summary>

QEMU emulation translates foreign CPU instructions sequentially in software, introducing a massive $5\times\text{ to }10\times$ performance penalty on CPU-intensive tasks like compilation. The Go compiler natively supports cross-compilation out of the box via environment variables (`GOOS` and `GOARCH`). By declaring `FROM --platform=$BUILDPLATFORM golang:...`, the Go compiler runs natively on the host CPU at 100% speed while directly producing target binaries for the foreign architecture in seconds.
</details>

<details>
<summary><strong>4. What is the role of <code>binfmt_misc</code> in Linux multi-platform container execution?</strong></summary>

`binfmt_misc` is a Linux kernel capability that allows operating system administrators to register userspace interpreter handlers for non-native executable formats based on magic byte signatures in file headers. When a container attempts to execute a non-native binary (e.g., ARM64 on x86), the kernel intercepts the execution, detects the ARM ELF header, and transparently passes the binary to the registered QEMU userspace emulator (`/usr/bin/qemu-aarch64`), enabling cross-architecture execution without modifying application code.
</details>

<details>
<summary><strong>5. Why can't a local Docker daemon (using default storage) store a multi-platform image locally without pushing to a registry?</strong></summary>

The default Docker Engine storage architecture (`graphdriver` / `overlay2`) is designed to store single-platform unpacked root filesystems matching the host operating system's CPU architecture. It has no native mechanism to store multiple disparate root filesystems under a single tag in its local content cache. To store multi-platform images, BuildKit must push the multi-architecture OCI index directly to a remote registry (`--push`) or export it as a multi-manifest tarball archive.
</details>

<details>
<summary><strong>6. What is the difference between <code>BUILDPLATFORM</code> and <code>TARGETPLATFORM</code> in a BuildKit Dockerfile?</strong></summary>

- **`BUILDPLATFORM`**: The platform (OS + Architecture) of the physical node where the build is actively executing (e.g., `linux/arm64` on an Apple Silicon laptop).
- **`TARGETPLATFORM`**: The platform of the resulting image being compiled (e.g., `linux/amd64` when compiling an image intended for deployment to an Intel cloud cluster).
</details>

<details>
<summary><strong>7. How do you create a Buildx builder instance that connects to multiple physical hardware architectures over SSH?</strong></summary>

Using `docker buildx create` with multiple nodes:
```bash
# Create builder with primary AMD64 node
docker buildx create --name cluster-builder --node amd64-node --platform linux/amd64 ssh://user@intel-server --bootstrap

# Append native ARM64 node to the same builder!
docker buildx create --name cluster-builder --append --node arm64-node --platform linux/arm64 ssh://user@graviton-server

docker buildx use cluster-builder
```
Buildx will now dispatch AMD64 compilation tasks natively to `intel-server` and ARM64 tasks natively to `graviton-server`!
</details>

<details>
<summary><strong>8. What tool can be used to inspect a remote multi-platform manifest without downloading any layer files?</strong></summary>

The command:
```bash
docker buildx imagetools inspect <image:tag>
```
Or the standard Docker manifest inspection tool:
```bash
docker manifest inspect <image:tag>
```
Both query the remote registry distribution endpoint and display the raw JSON Image Index detailing all supported OS/architecture descriptors.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Multi-Architecture Manifest of the Official Redis Image</strong></summary>

**Goal**: Inspect all CPU architectures supported by Redis.

```bash
docker buildx imagetools inspect redis:alpine
```
</details>

<details>
<summary><strong>Exercise 2: Build a Cross-Platform Go Binary Using Native Cross-Compilation</strong></summary>

**Goal**: Compile for both AMD64 and ARM64 at native speed using `$BUILDPLATFORM`.

```bash
mkdir -p /tmp/cross-lab && cd /tmp/cross-lab

cat <<'EOF' > main.go
package main
import (
    "fmt"
    "runtime"
)
func main() {
    fmt.Printf("Compiled for: %s/%s\n", runtime.GOOS, runtime.GOARCH)
}
EOF

cat <<'EOF' > Dockerfile
# syntax=docker/dockerfile:1
FROM --platform=$BUILDPLATFORM golang:1.22-alpine AS builder
WORKDIR /src
COPY main.go .
ARG TARGETOS
ARG TARGETARCH
RUN CGO_ENABLED=0 GOOS=$TARGETOS GOARCH=$TARGETARCH go build -o /bin/demo .

FROM scratch
COPY --from=builder /bin/demo /demo
ENTRYPOINT ["/demo"]
EOF

# Build using docker-container builder
docker buildx create --name test-bld --use 2>/dev/null || true
docker buildx build --platform linux/amd64,linux/arm64 -t cross-demo:1.0 .

# Cleanup
rm -rf /tmp/cross-lab
```
</details>

<details>
<summary><strong>Exercise 3: Test Dynamic Platform Argument Printing</strong></summary>

**Goal**: Observe BuildKit automatically populating `TARGETARCH`.

```bash
cat <<'EOF' > /tmp/arch.dockerfile
FROM alpine
ARG TARGETARCH
RUN echo "Targeting Architecture: $TARGETARCH" > /arch.txt
CMD cat /arch.txt
EOF

docker buildx build -f /tmp/arch.dockerfile --platform linux/amd64,linux/arm64 .
rm -f /tmp/arch.dockerfile
```
</details>

<details>
<summary><strong>Exercise 4: Test Emulation Execution via `--platform`</strong></summary>

**Goal**: Verify that your Docker client can execute non-native binaries via emulation.

```bash
# Execute ARM64 container
docker run --rm --platform linux/arm64 alpine uname -m
```
</details>

<details>
<summary><strong>Exercise 5: Inspect Local Buildx Builder Capabilities</strong></summary>

**Goal**: Query the list of platforms supported by your active builder.

```bash
docker buildx inspect --bootstrap
```
*Look at the `Platforms:` line in the output.*
</details>

<details>
<summary><strong>Exercise 6: Export Multi-Architecture Build as a Tarball Archive</strong></summary>

**Goal**: Package a multi-platform build to disk without pushing to a registry.

```bash
docker buildx build --platform linux/amd64,linux/arm64 --output type=tar,dest=/tmp/multi.tar - <<'EOF'
FROM alpine
ARG TARGETARCH
RUN echo "Arch is $TARGETARCH" > /arch.txt
EOF

ls -lh /tmp/multi.tar
rm -f /tmp/multi.tar
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                         MULTI-PLATFORM IMAGES CHEAT SHEET                             |
+---------------------------------------------------------------------------------------+
| The Problem:           exec format error (CPU architecture mismatch between binary & host)|
| Multi-Arch Standard:   OCI Image Index pointing to platform-specific manifests        |
| Inspect Architectures: docker buildx imagetools inspect <image>                       |
| Create Builder:        docker buildx create --driver docker-container --use           |
| Build Multi-Arch:      docker buildx build --platform linux/amd64,linux/arm64         |
|                        -t <reg>/<app>:1.0 --push .                                    |
| Cross-Compile Pattern: FROM --platform=$BUILDPLATFORM golang:alpine AS builder        |
|                        ARG TARGETOS TARGETARCH                                        |
|                        RUN GOOS=$TARGETOS GOARCH=$TARGETARCH go build ...             |
| Force Specific Arch:   docker run --platform linux/amd64 <image>                      |
| Golden Rule:           Never compile under slow QEMU if the compiler supports cross-  |
|                        compilation (Go, Rust); use --platform=$BUILDPLATFORM!         |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Docker Documentation: Multi-Platform Builds Overview](https://docs.docker.com/build/building/multi-platform/)
- [Moby Project: BuildKit Multi-Platform Specification](https://github.com/moby/buildkit/blob/master/docs/multi-platform.md)
- [Open Container Initiative (OCI) Image Index Specification](https://github.com/opencontainers/image-spec/blob/main/image-index.md)
- [Debian Wiki: QEMU User Emulation and binfmt_misc Setup](https://wiki.debian.org/QemuUserEmulation)

---

| [< Previous: 06-buildkit-and-buildx.md](./06-buildkit-and-buildx.md) | [Next: 08-base-image-choice.md](./08-base-image-choice.md) > |
| :--- | :--- |
