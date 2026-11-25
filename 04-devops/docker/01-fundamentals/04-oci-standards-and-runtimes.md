---
title: "OCI Standards and Container Runtimes: Image Spec, Runtime Spec, and Distribution"
description: "Comprehensive guide to Open Container Initiative (OCI) standards: Image, Runtime, and Distribution specifications, low-level runtimes (runc, crun, gVisor, Kata), and the Kubernetes CRI."
category: "DevOps / Containers"
tags: ["docker", "oci", "runc", "crun", "gvisor", "kata-containers", "standards", "cri"]
difficulty: "Intermediate"
prerequisites: ["03-docker-architecture.md", "Understanding of operating system processes"]
estimated_read_time: "25 min"
---

# OCI Standards and Container Runtimes: Image Spec, Runtime Spec, and Distribution

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. The Three Core OCI Specifications](#2-the-three-core-oci-specifications)
- [3. High-Level vs Low-Level Runtimes](#3-high-level-vs-low-level-runtimes)
- [4. The Sandboxed & Virtualized Runtimes: gVisor & Kata Containers](#4-the-sandboxed--virtualized-runtimes-gvisor--kata-containers)
- [5. Practical Examples & Runtime Inspection](#5-practical-examples--runtime-inspection)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table: Low-Level Container Runtimes](#9-comparison-table-low-level-container-runtimes)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

In the early days of containerization (2013–2015), Docker was the de facto standard. However, because Docker controlled both the image archive format and the runtime engine, the industry faced vendor lock-in. Competitors (such as CoreOS with `rkt`) emerged, threatening to fragment the container ecosystem into incompatible, competing image formats and execution runtimes.

In 2015, Docker, CoreOS, Google, Red Hat, Microsoft, and IBM joined under the auspices of the Linux Foundation to establish the **Open Container Initiative (OCI)**. Docker donated its container execution code (`libcontainer`, refactored as `runc`) and its image format specifications to the OCI.

Today, the OCI guarantees that **any image built by any compliant builder** (Docker, Buildah, Kaniko, ko, Bazel) can be stored in **any compliant registry** (Docker Hub, AWS ECR, GitHub Packages, Harbor) and executed by **any compliant runtime** (`runc`, `crun`, `gVisor`, `Kata Containers`, `containerd`, `CRI-O`).

```
+---------------------------------------------------------------------------------+
|                         THE OCI COMPLIANCE PIPELINE                             |
|                                                                                 |
|  [OCI Image Specification]                                                      |
|  Builders: Docker BuildKit, Buildah, Kaniko, ko                                 |
|        |                                                                        |
|        | Produces OCI Compliant Image (tarballs + config JSON + manifest JSON)  |
|        v                                                                        |
|  [OCI Distribution Specification]                                               |
|  Registries: Docker Hub, AWS ECR, Quay, Harbor, GHCR                            |
|        |                                                                        |
|        | Transports Blobs & Manifests via standardized REST HTTP API            |
|        v                                                                        |
|  [High-Level Runtimes (CRI / Management)]                                       |
|  containerd, CRI-O, Docker Engine, Podman                                       |
|        |                                                                        |
|        | Unpacks layer blobs, generates rootfs and OCI config.json bundle       |
|        v                                                                        |
|  [OCI Runtime Specification (Low-Level Execution)]                              |
|  runc (Standard Go)  |  crun (High-Perf C)  |  gVisor (Sandbox)  |  Kata (VM)  |
|        |                     |                     |                  |         |
|        v                     v                     v                  v         |
|  Standard Linux NS   Standard Linux NS     App Kernel in User  MicroVM via KVM  |
+---------------------------------------------------------------------------------+
```

---

## 2. The Three Core OCI Specifications

### 2.1. OCI Image Specification (`image-spec`)
The Image Specification defines the schema and serialization of container images:
1. **Layer Blobs**: Tar archives containing filesystem differences (additions, modifications, and whiteouts for deleted files), compressed via gzip, zstd, or uncompressed.
2. **Image Configuration (`config.json`)**: An immutable JSON document detailing container execution parameters: entrypoint, command, default environment variables, exposed ports, volume targets, and layer DiffIDs.
3. **Image Manifest (`manifest.json`)**: A JSON descriptor linking the configuration blob and the ordered list of layer blobs by their cryptographic SHA256 digests.
4. **Image Index (`index.json`)**: A multi-architecture manifest list grouping platform-specific manifests (e.g., `linux/amd64`, `linux/arm64`, `windows/amd64`) under a single unified tag.

### 2.2. OCI Runtime Specification (`runtime-spec`)
The Runtime Specification defines the configuration, execution environment, and lifecycle of a container on disk:
- It specifies the **Filesystem Bundle**: A directory on disk containing an OCI `config.json` and a directory called `rootfs` containing the unpacked filesystem.
- It defines standard runtime operations: `create`, `start`, `kill`, `delete`, and `state`.
- It defines standard lifecycle hooks: `prestart`, `createRuntime`, `createContainer`, `startContainer`, and `poststop`.

### 2.3. OCI Distribution Specification (`distribution-spec`)
The Distribution Specification standardizes the HTTP REST API for discovering, pulling, pushing, and querying container image manifests, tags, and content blobs between clients and registries:
- `GET /v2/<name>/manifests/<reference>`: Retrieves the manifest.
- `GET /v2/<name>/blobs/<digest>`: Downloads layer tarballs.
- `POST /v2/<name>/blobs/uploads/`: Uploads layer blobs.

---

## 3. High-Level vs Low-Level Runtimes

```
+---------------------------------------------------------------------------------+
|                       RUNTIME TAXONOMY & DIVISION OF RESPONSIBILITY             |
+------------------------------------+--------------------------------------------+
|     HIGH-LEVEL RUNTIMES (CRI)      |       LOW-LEVEL RUNTIMES (OCI)             |
| (containerd, CRI-O, dockerd)       | (runc, crun, youki, gVisor, Kata)          |
+------------------------------------+--------------------------------------------+
| - Implements Kubernetes CRI gRPC   | - Implements OCI Runtime Spec CLI          |
| - Authenticates with registries    | - Interacts directly with Linux kernel     |
| - Pulls & unpacks image layers     | - Invokes clone(), setns(), pivot_root()   |
| - Manages snapshots & storage      | - Applies cgroup memory & CPU limits       |
| - Manages virtual network attachments- Drops Linux capabilities & sets seccomp  |
| - Supervised long-running daemons  | - Executes target binary and exits         |
+------------------------------------+--------------------------------------------+
```

### 3.1. High-Level Runtimes
- **`containerd`**: The CNCF-graduated runtime used inside Docker and as the primary Kubernetes runtime on cloud providers (GKE, EKS, AKS).
- **`CRI-O`**: A lightweight runtime purpose-built exclusively for Kubernetes. It implements the Kubernetes CRI and executes OCI runtimes directly, with zero legacy Docker overhead.

### 3.2. Low-Level Runtimes
- **`runc`**: The original OCI reference runtime written in Go. Solid, battle-tested, used by default across Linux distributions.
- **`crun`**: A modern low-level runtime written in pure C by Red Hat. Consumes $10\times$ less memory than `runc`, starts containers twice as fast, and provides first-class support for Linux cgroups v2.
- **`youki`**: A modern OCI runtime written in Rust, offering memory safety and high execution speed.

---

## 4. The Sandboxed & Virtualized Runtimes: gVisor & Kata Containers

Standard runtimes (`runc`, `crun`) share the host Linux kernel directly. If an attacker exploits a kernel zero-day vulnerability, they can escape the container. To protect against untrusted multi-tenant workloads, cloud providers created **Sandboxed and Virtualized Runtimes**:

```
+---------------------------------------------------------------------------------+
|                       CONTAINER ISOLATION SPECTRUM                              |
+-------------------------+-------------------------+-----------------------------+
|    STANDARD (runc/crun) |     SANDBOX (gVisor)    |   MICROVM (Kata Containers) |
+-------------------------+-------------------------+-----------------------------+
| [App]                   | [App]                   | [App]                       |
|   | (Direct Syscalls)   |   | (Intercepted Syscall)|   | (Native Syscalls)       |
|   v                     |   v                     |   v                         |
| [Host Linux Kernel]     | [Sentry: User Space KNL]| [Guest Linux Kernel]        |
|                         |   | (Filtered Syscalls) |   | (Virtual Hardware)      |
|                         |   v                     |   v                         |
|                         | [Host Linux Kernel]     | [KVM Hypervisor / Host]     |
|                         |                         |                             |
| Security: Standard      | Security: High          | Security: Hardware VT-x Max |
| Overhead: ~0%           | Overhead: 10-25% (I/O)  | Overhead: 5-10% (Memory)    |
+-------------------------+-------------------------+-----------------------------+
```

### 4.1. gVisor (`runsc`)
Developed by Google (used in Google Cloud Run and GKE Sandbox), gVisor intercepts all system calls made by the container and handles them inside **Sentry**—a complete Linux kernel written in memory-safe Go operating in user space. The host kernel is shielded from direct contact with untrusted container code.

### 4.2. Kata Containers
Kata Containers packages every container pod inside a dedicated, ultra-lightweight hardware-isolated virtual machine using KVM and QEMU/Cloud-Hypervisor. It boots in under $100\text{ ms}$, delivering true hardware-enforced CPU virtualization isolation.

---

## 5. Practical Examples & Runtime Inspection

### Example 1: Inspecting an OCI Image Manifest via Docker CLI
Retrieve the raw OCI manifest of an image from a remote registry without pulling layer files:

```bash
docker manifest inspect nginx:alpine
```
*Output reveals the OCI schema version, media types (`application/vnd.oci.image.manifest.v1+json`), config digest, and layer descriptors.*

### Example 2: Configuring Docker to Support Alternative Runtimes (`crun`)
Configure Docker daemon to allow launching containers with `crun` in addition to `runc`:

```json
// In /etc/docker/daemon.json
{
  "runtimes": {
    "crun": {
      "path": "/usr/bin/crun"
    }
  }
}
```
Reload daemon and launch a container explicitly using the `crun` runtime:
```bash
sudo systemctl reload docker
docker run --rm --runtime=crun alpine echo "Executed via crun OCI runtime!"
```

### Example 3: Extracting and Manually Inspecting an OCI Bundle
Manually construct an OCI bundle and observe how `runc` operates independently of Docker:

```bash
# 1. Create a workspace directory
mkdir -p /tmp/oci-demo/rootfs && cd /tmp/oci-demo

# 2. Export Alpine rootfs into rootfs/
docker export $(docker create alpine) | tar -C rootfs -xpf -

# 3. Generate a standard OCI config.json template using runc
runc spec

# 4. Inspect the generated config.json
head -n 25 config.json
```
*Notice*: You now have a complete OCI bundle (`rootfs` + `config.json`). `runc` can run this bundle on a bare Linux server with zero Docker packages installed!

```bash
# Cleanup
rm -rf /tmp/oci-demo
```

### Example 4: Querying containerd CRI Plugins in Kubernetes
Observe how Kubernetes interacts with containerd via the Container Runtime Interface:

```bash
# Check CRI status inside containerd
sudo ctr plugins ls | grep cri
```

### Example 5: Inspecting Multi-Architecture Manifest Lists
Query the platform support of an official image to observe multi-arch OCI indexing:

```bash
docker buildx imagetools inspect python:3.12-slim
```
*Output shows entries for `linux/amd64`, `linux/arm64/v8`, `linux/arm/v7`, `linux/s390x`, and `linux/ppc64le`.*

---

## 6. Best Practices & Production Guidelines

1. **Adopt OCI Standard Media Types**: When authoring build pipelines or self-hosting registries, use standard OCI media types (`application/vnd.oci.image.*`) instead of legacy Docker v2 Schema 2 types.
2. **Use Sandboxed Runtimes for Multi-Tenant Untrusted Code**: If your platform executes customer-submitted code or untrusted batch scripts, do not use default `runc`. Run them inside **gVisor (`runsc`)** or **Kata Containers**.
3. **Upgrade to `crun` on Cgroups v2 Hosts**: On modern Linux distributions (RHEL 9, Fedora, Ubuntu 24.04), configure `crun` as your default OCI runtime for lower memory footprints and faster startup.
4. **Pin Multi-Architecture Builds to Exact Digests**: When deploying across mixed clusters (x86_64 and Graviton/ARM64), use multi-arch manifests pinned by digest to prevent architectures from drifting across deployments.
5. **Enforce Read-Only OCI Specifications**: Ensure your orchestrator sets `root.readonly: true` in the OCI spec whenever stateful local writes are not required.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Solution |
|:---|:---|:---|:---|
| **Relying on Docker for Kubernetes node runtimes** | Running Docker on Kubernetes $\ge 1.24$. | Dockershim is removed; kubelet cannot communicate with Docker daemon. | Migrate Kubernetes nodes to native `containerd` or `CRI-O`. |
| **Running untrusted tenant code on standard `runc`** | Believing standard container namespaces prevent kernel privilege escalation. | A single kernel exploit allows root takeover of the entire cluster node. | Enforce sandboxed runtimes (gVisor or Kata Containers). |
| **Assuming all OCI runtimes support identical syscalls** | Running system-level utilities (e.g., WireGuard, eBPF) inside gVisor. | gVisor's Sentry kernel only implements common Linux syscalls; un-implemented syscalls fail. | Run system-level workloads on `runc`/`crun` or Kata Containers. |
| **Mixing Docker v2 Schema 1 images with modern OCI runtimes** | Pulling legacy images created before 2016. | Modern registries and containerd reject deprecated Schema 1 manifests. | Rebuild images using modern BuildKit and modern base images. |
| **Hardcoding host paths in OCI `config.json`** | Manually writing OCI bundles with rigid host paths. | Bundle fails to execute across different host operating systems. | Generate bundles dynamically via high-level runtimes (`containerd`). |
| **Ignoring OCI Distribution rate limits** | Repeatedly pulling manifests without caching. | Public registries throttle CI/CD pipelines with HTTP 429 Too Many Requests. | Deploy self-hosted pull-through cache registries (Harbor, Docker Registry). |

---

## 8. Performance, Security & Cost Implications

### Performance
- **`runc` vs `crun`**: `crun` starts containers in approximately $1.5\text{ ms}$ compared to $5\text{ ms}$ for `runc`, and uses $1.5\text{ MB}$ memory compared to $15\text{ MB}$ for `runc` during instantiation.
- **gVisor Overhead**: Because gVisor intercepts system calls in user space, applications with high syscall frequencies (heavy disk I/O or network socket creation) experience a $15-30\%$ performance penalty. Compute-heavy workloads (machine learning, data processing) experience $<2\%$ penalty.

### Security
- **Defense-in-Depth**: OCI standards allow swapping runtimes transparently on a per-pod or per-container basis without altering the container image.
- **Hypervisor Isolation**: Kata Containers provides true hardware-assisted VT-x/AMD-V isolation, matching the security posture of traditional virtual machines.

### Cost
- Standardizing on OCI prevents vendor lock-in, enabling organizations to switch cloud container registries (e.g., from Docker Hub to AWS ECR or self-hosted Harbor) with zero code modifications.

---

## 9. Comparison Table: Low-Level Container Runtimes

| Feature | `runc` | `crun` | `gVisor` (`runsc`) | `Kata Containers` |
|:---|:---|:---|:---|:---|
| **Author / Sponsor** | OCI / Docker | Red Hat | Google | OpenInfra Foundation |
| **Implementation Language**| Go | C | Go | Rust / Go |
| **Isolation Mechanism** | Namespaces + cgroups | Namespaces + cgroups | User Space Kernel (Sentry) | MicroVM (KVM Hypervisor)|
| **Startup Overhead** | $\sim 5\text{ ms}$ | $\sim 1.5\text{ ms}$ | $\sim 25\text{ ms}$ | $\sim 100\text{ ms}$ |
| **Memory Overhead** | Low ($\sim 15\text{ MB}$ setup) | Ultra-Low ($\sim 1.5\text{ MB}$) | Moderate ($\sim 30\text{ MB}$) | High ($\sim 60-120\text{ MB}$) |
| **Syscall Compatibility** | 100% Linux Kernel | 100% Linux Kernel | Partial ($\sim 300+$ syscalls) | 100% Linux Guest Kernel |
| **Security Posture** | Standard Isolation | Standard Isolation | High (Kernel shielding) | Maximum (Hardware VT-x) |
| **Primary Use Case** | General Production | High-Density / Edge | Untrusted Multi-Tenant | Strict Compliance / Cloud |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. Why was the Open Container Initiative (OCI) formed, and what problem did it solve?</strong></summary>

The OCI was formed in 2015 to establish vendor-neutral open industry standards for container image formats and runtime execution. Prior to the OCI, Docker held proprietary control over image formats and runtimes, creating risks of fragmentation and vendor lock-in when competitors like CoreOS launched alternative formats. The OCI standardized the Image Specification, Runtime Specification, and Distribution Specification, ensuring interoperability across all builders, registries, and runtimes.
</details>

<details>
<summary><strong>2. What is an OCI Bundle?</strong></summary>

An OCI Bundle is a standardized on-disk directory structure consumed by low-level OCI runtimes (like `runc`). It consists of two essential components:
1. `config.json`: The specification document defining namespaces, environment variables, mounts, cgroups resource limits, capabilities, and execution arguments.
2. `rootfs/`: A directory containing the complete unpacked filesystem layers representing the container's root directory.
</details>

<details>
<summary><strong>3. Explain the architectural difference between containerd and runc.</strong></summary>

- **`containerd`** is a **high-level container runtime**. It manages image pulling, decompression, storage snapshots, network attachment, and task monitoring across the host system. It runs continuously as a background daemon.
- **`runc`** is a **low-level OCI container runtime**. It is a transient command-line utility invoked by `containerd` to configure kernel namespaces, apply cgroups, pivot root, and execute the container binary. `runc` exits immediately after spawning the container process.
</details>

<details>
<summary><strong>4. Why did Kubernetes deprecate and remove "Dockershim" in Kubernetes v1.24?</strong></summary>

Kubernetes requires container runtimes to implement the **Container Runtime Interface (CRI)**. Docker was created before the CRI existed and does not natively implement it. To support Docker, Kubernetes had to maintain a complex translation layer called `dockershim`. This added latency, memory overhead, and maintenance drag. By removing Dockershim, Kubernetes interacts directly with native CRI-compliant runtimes (`containerd` or `CRI-O`), eliminating the redundant Docker daemon layer completely.
</details>

<details>
<summary><strong>5. How does gVisor protect the host kernel compared to standard runtimes like runc?</strong></summary>

Standard runtimes pass application system calls directly to the host Linux kernel. If a container application exploits a kernel vulnerability, it can compromise the host. gVisor replaces the host kernel interface with **Sentry**—an application kernel written in memory-safe Go operating in user space. Sentry handles system calls inside the sandbox and only forwards a heavily restricted subset of safe system calls to the underlying host kernel, shielding the host from zero-day exploits.
</details>

<details>
<summary><strong>6. What is an OCI Image Index (Manifest List)?</strong></summary>

An OCI Image Index (or multi-architecture manifest list) is a top-level JSON document pointing to multiple individual image manifests, each tailored for a specific CPU architecture (e.g., `amd64`, `arm64`, `riscv64`) and operating system (`linux`, `windows`). When a client pulls an image tag, the runtime inspects the index, matches the host's CPU architecture and OS, and downloads only the relevant image manifest and layers.
</details>

<details>
<summary><strong>7. What advantages does `crun` offer over `runc`?</strong></summary>

`crun` is written in pure C, whereas `runc` is written in Go. Advantages include:
1. **Faster Startup**: Instantiates containers up to $2-3\times$ faster than `runc`.
2. **Smaller Memory Footprint**: Uses $\sim 10\times$ less RAM during process setup.
3. **Native Cgroups v2**: Implements advanced cgroups v2 features and systemd integration natively without Go runtime baggage.
4. **No Go Runtime Overhead**: Eliminates the Go garbage collector and scheduler initialization overhead during invocation.
</details>

<details>
<summary><strong>8. What is the OCI Distribution Specification?</strong></summary>

The OCI Distribution Specification is an open standard defining the HTTP/1.1 and HTTP/2 REST API for content-addressable container registries. It defines endpoints for pushing, pulling, checking existence (`HEAD`), and querying cryptographic blobs and manifests, ensuring that any compliant container client (Docker, Podman, Skopeo, containerd) can interact with any compliant registry (Harbor, ECR, Docker Hub).
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Multi-Architecture Manifest Descriptors</strong></summary>

**Goal**: Inspect the architectures supported by the official Redis image.

```bash
docker manifest inspect redis:alpine
```
</details>

<details>
<summary><strong>Exercise 2: Create a Minimal OCI Configuration Specification</strong></summary>

**Goal**: Use `runc` to generate and inspect a default OCI `config.json` bundle.

```bash
# If runc is installed:
mkdir -p /tmp/spec-test && cd /tmp/spec-test
runc spec
python3 -c "import json; data=json.load(open('config.json')); print('Process Args:', data['process']['args'])"
rm -rf /tmp/spec-test
```
</details>

<details>
<summary><strong>Exercise 3: Inspect Image Digest vs Tag Using Skopeo (if available) or Docker</strong></summary>

**Goal**: Verify that image content digests remain identical regardless of the tag name.

```bash
# Query image repo digest
docker inspect --format '{{index .RepoDigests 0}}' alpine:latest
```
</details>

<details>
<summary><strong>Exercise 4: Test Architecture Emulation Execution</strong></summary>

**Goal**: Run an image built for a different architecture via QEMU user emulation.

```bash
# Run ARM64 container on an AMD64 machine (or vice versa)
docker run --rm --platform linux/arm64 alpine uname -m
```
</details>

<details>
<summary><strong>Exercise 5: Inspect Runtime Configuration in Docker Daemon</strong></summary>

**Goal**: Check which OCI runtimes are registered with the local Docker daemon.

```bash
docker info --format '{{json .Runtimes}}'
```
</details>

<details>
<summary><strong>Exercise 6: Query OCI Media Types via cURL</strong></summary>

**Goal**: Make a raw HTTP HEAD request to Docker Hub to verify OCI media headers.

```bash
curl -I https://registry-1.docker.io/v2/ 2>&1 | grep -i "Www-Authenticate"
```
Notice the standard OCI V2 authentication challenge header.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                             OCI STANDARDS CHEAT SHEET                                 |
+---------------------------------------------------------------------------------------+
| OCI Image Spec:        Layers (tarballs) + config.json + manifest.json                |
| OCI Runtime Spec:      Unpacked bundle (rootfs/ + config.json)                        |
| OCI Distribution Spec: REST API endpoints (/v2/<name>/manifests/, /v2/<name>/blobs/)  |
| runc:                  Standard OCI reference runtime (Go)                            |
| crun:                  High-performance OCI runtime (C, fast, low-memory)            |
| gVisor (runsc):        User space kernel sandbox for untrusted code                   |
| Kata Containers:       MicroVM hardware-isolated runtime                              |
| Kubernetes CRI:        gRPC interface separating orchestrator from container runtimes |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Open Container Initiative (OCI) Official Website](https://opencontainers.org/)
- [OCI Image Format Specification Repository](https://github.com/opencontainers/image-spec)
- [OCI Runtime Specification Repository](https://github.com/opencontainers/runtime-spec)
- [OCI Distribution Specification Repository](https://github.com/opencontainers/distribution-spec)
- [gVisor: Application Kernel for Containers Documentation](https://gvisor.dev/)
- [Kata Containers Architecture Documentation](https://katacontainers.io/)

---

| [< Previous: 03-docker-architecture.md](./03-docker-architecture.md) | [Next: 05-how-isolation-works.md](./05-how-isolation-works.md) > |
| :--- | :--- |
