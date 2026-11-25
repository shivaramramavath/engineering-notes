# Module 01: Core Fundamentals

## Overview

Welcome to **Module 01: Core Fundamentals** of the Docker and Container Engineering track. This module demystifies the inner workings of containers, taking you beneath high-level CLI abstractions into the Linux kernel primitives, Open Container Initiative (OCI) specifications, and union filesystem architectures that make containerization possible.

Containers are not lightweight virtual machines; they are standard host processes governed by Linux **Namespaces**, **Control Groups (cgroups)**, **Linux Capabilities**, and **Seccomp** syscall filters. In this module, you will master the anatomy of images and containers, the multi-layered architecture of Docker Engine (`dockerd`, `containerd`, `containerd-shim`, and `runc`), the mechanics of OverlayFS, image distribution standards, container lifecycle state machines, and the subtle "PID 1 problem" that trips up production deployments.

---

## Learning Objectives

By completing this module, you will be able to:
- Articulate the exact architectural differences between hypervisor-based hardware virtualization and kernel-shared containerization.
- Explain the anatomy of an immutable OCI image versus the stateful, ephemeral read-write container layer (`UpperDir`).
- Trace the complete execution pipeline from CLI invocation through `dockerd`, `containerd`, `containerd-shim`, and `runc`.
- Understand the role of the Open Container Initiative (OCI) and evaluate alternative low-level runtimes (`crun`, `gVisor`, `Kata Containers`).
- Diagnose and debug kernel isolation primitives: all 8 Linux namespaces, unified cgroups v2, capability bounding sets, and seccomp BPF filters.
- Understand the internal mechanics of Union Mounts and OverlayFS: `lowerdir`, `upperdir`, `workdir`, `merged`, Copy-on-Write (CoW), and whiteout files.
- Master image reference grammar, understand why mutable tags (`:latest`) cause production incidents, and pin immutable SHA256 content digests.
- Troubleshoot container failures across the 7 lifecycle states using exit code forensics (0, 1, 126, 127, 137, 143).
- Solve the PID 1 Problem: eliminate 10-second shutdown hangs, propagate signals properly, reap zombie processes, and configure lightweight init systems like Tini.

---

## Module Roadmap & Notes

| Note | Title | Focus & Core Competencies | Est. Read Time |
|:---|:---|:---|:---|
| **[01-docker-vs-vm.md](./01-docker-vs-vm.md)** | **Docker Containers vs Virtual Machines** | Deep architectural comparison: Type 1/2 hypervisors vs kernel sharing, system call dispatch paths, memory ballooning, boot time latency, and security isolation boundaries. | 25 min |
| **[02-images-and-containers.md](./02-images-and-containers.md)** | **Images and Containers** | Anatomies, immutability, directed acyclic graphs (DAG) of layers, OCI manifests, and why mutating containers via `docker commit` is an anti-pattern. | 25 min |
| **[03-docker-architecture.md](./03-docker-architecture.md)** | **Docker Engine Architecture** | The layered client-server engine: REST APIs over UNIX sockets, `dockerd`, `containerd`, the critical role of `containerd-shim`, and `runc` execution. | 25 min |
| **[04-oci-standards-and-runtimes.md](./04-oci-standards-and-runtimes.md)** | **OCI Standards & Runtimes** | The Image, Runtime, and Distribution specifications. High-level runtimes vs low-level runtimes (`runc`, `crun`, `youki`, `gVisor`, `Kata Containers`), and the Kubernetes CRI. | 25 min |
| **[05-how-isolation-works.md](./05-how-isolation-works.md)** | **How Container Isolation Works** | The 4 pillars of container isolation: all 8 Linux namespaces, unified cgroups v2, capability bounding sets, and Seccomp BPF syscall firewalls. | 25 min |
| **[06-image-layers-and-union-filesystems.md](./06-image-layers-and-union-filesystems.md)** | **Image Layers & Union Filesystems** | Deep dive into OverlayFS (`lowerdir`, `upperdir`, `workdir`, `merged`), Copy-on-Write mechanics, whiteout deletion markers (`.wh`), and storage driver evolution. | 25 min |
| **[07-registries-and-image-naming.md](./07-registries-and-image-naming.md)** | **Registries & Image Naming** | Full grammar of image references, default resolution rules, mutable tags vs immutable SHA256 digests, multi-architecture manifest lists, and credential stores. | 25 min |
| **[08-container-lifecycle-and-states.md](./08-container-lifecycle-and-states.md)** | **Container Lifecycle & States** | The 7 container states (`created`, `running`, `paused`, `restarting`, `removing`, `exited`, `dead`), state transitions, and comprehensive exit code forensics. | 25 min |
| **[09-pid-1-signals-and-init.md](./09-pid-1-signals-and-init.md)** | **The PID 1 Problem & Init Systems** | Signal masking, zombie process accumulation, exec form vs shell form in Dockerfiles, signal forwarding, Tini, dumb-init, and `docker run --init`. | 25 min |

---

## Recommended Learning Flow

```
[01-docker-vs-vm.md] 
         |
         v
[02-images-and-containers.md]
         |
         v
[03-docker-architecture.md]
         |
         +----------------------------------+
         |                                  |
         v                                  v
[04-oci-standards-and-runtimes.md]       [05-how-isolation-works.md]
         |                                  |
         +----------------------------------+
         |
         v
[06-image-layers-and-union-filesystems.md]
         |
         v
[07-registries-and-image-naming.md]
         |
         v
[08-container-lifecycle-and-states.md]
         |
         v
[09-pid-1-signals-and-init.md]
         |
         v
[Proceed to Module 02: Essential Commands]
```

---

## Prerequisites

Before starting this module, you should have:
1. Completed **[Module 00: Setup & Installation](../00-setup/README.md)**.
2. A functioning local or remote Docker daemon.
3. Basic knowledge of Linux processes, filesystems, and permissions.

---

| [< Previous: Module 00 Setup](../00-setup/README.md) | [Next: Module 02 Essential Commands >](../02-commands/README.md) |
| :--- | :--- |
