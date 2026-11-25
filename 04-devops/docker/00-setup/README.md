# Module 00: Setup & Installation

## Overview

Welcome to **Module 00: Setup & Installation** of the Docker and Container Engineering track. This module establishes the operational, theoretical, and architectural foundation required to install, configure, verify, and secure Docker across diverse operating systems and cloud environments.

Whether deploying natively on enterprise Linux, leveraging Docker Desktop on macOS (Apple Silicon or Intel), integrating with the Windows Subsystem for Linux (WSL 2), configuring unprivileged Rootless Docker, or exploring modern daemonless alternatives like Podman and Colima, this module provides the precise technical blueprints and production guidelines needed for modern engineering environments.

---

## Learning Objectives

By the end of this module, you will be able to:
- Understand the architectural distinction between OS-level containerization and hypervisor-based hardware virtualization.
- Install and configure Docker Engine Community Edition from official upstream repositories on Debian/Ubuntu and RHEL/Fedora distributions.
- Configure Docker Desktop on macOS and Windows with optimal filesystem acceleration (VirtioFS) and resource allocations.
- Architect and deploy **Rootless Docker** utilizing Linux User Namespaces and `slirp4netns` to eliminate host root escalation vectors.
- Evaluate and operate modern Docker alternatives, including **Podman**, **nerdctl/containerd**, **OrbStack**, and **Colima**.
- Manage multi-host infrastructure and remote daemons securely using **Docker Contexts**, SSH tunneling, and Mutual TLS (mTLS).
- Master the fundamental container lifecycle: pulling, running, port-mapping, executing, logging, inspecting, and cleanly terminating containerized applications.

---

## Module Roadmap & Notes

| Note | Title | Focus & Core Competencies | Est. Read Time |
|:---|:---|:---|:---|
| **[01-getting-started.md](./01-getting-started.md)** | **Docker Getting Started** | The containerization mental model, client-server daemon architecture, `dockerd`/`containerd`/`runc` execution chain, and baseline verification. | 25 min |
| **[02-install-on-linux-macos-windows.md](./02-install-on-linux-macos-windows.md)** | **Cross-Platform Installation** | Official Linux repository installation, macOS Docker Desktop (Apple Silicon vs Intel, Rosetta 2, VirtioFS), Windows WSL 2 backend integration, and host daemon tuning. | 25 min |
| **[03-rootless-and-post-install-steps.md](./03-rootless-and-post-install-steps.md)** | **Rootless Docker & Post-Install Hardening** | Unprivileged rootless container architecture, User Namespaces, `/etc/subuid` mapping, systemd user services, proxy integration, and CIS daemon hardening. | 25 min |
| **[04-docker-alternatives.md](./04-docker-alternatives.md)** | **Docker Alternatives** | Deep architectural comparison of Podman (daemonless/rootless, Pods, Quadlet), nerdctl/containerd, OrbStack, and Colima. Licensing and tool selection matrix. | 25 min |
| **[05-docker-context-and-remote-hosts.md](./05-docker-context-and-remote-hosts.md)** | **Docker Context & Remote Hosts** | CLI context management (`docker context`), tunneling over secure SSH (`ssh://`), Mutual TLS (`tcp://:2376`), profile exports, and mitigating dangerous unencrypted ports. | 25 min |
| **[06-first-container-walkthrough.md](./06-first-container-walkthrough.md)** | **First Container Walkthrough** | Complete end-to-end hands-on lifecycle: pull, run detached vs interactive, port publishing (`-p`), `docker exec`, live log streaming, `docker cp`, and clean teardown. | 25 min |

---

## Recommended Learning Flow

```
[01-getting-started.md] 
         |
         v
[02-install-on-linux-macos-windows.md]
         |
         +----------------------------------+
         |                                  |
         v                                  v
[03-rootless-and-post-install-steps.md]  [04-docker-alternatives.md]
         |                                  |
         +----------------------------------+
         |
         v
[05-docker-context-and-remote-hosts.md]
         |
         v
[06-first-container-walkthrough.md]
         |
         v
[Proceed to Module 01: Core Fundamentals]
```

---

## Prerequisites

Before starting this module, you should have:
1. Familiarity with standard Linux shell commands (`cd`, `ls`, `curl`, `chmod`, `systemctl`).
2. Administrative privileges (`sudo` on Linux/macOS or Administrator on Windows) to install container runtimes.
3. A basic understanding of TCP/IP networking (IP addresses, ports, DNS resolution).

---

## Module Verification & Healthcheck

To verify that your local environment is fully prepared for this curriculum, run:

```bash
# Check Docker CLI and Daemon connectivity
docker version

# Verify BuildKit and experimental capabilities
docker info --format 'Docker Version: {{.ServerVersion}} | Storage Driver: {{.Driver}} | Cgroups: {{.CgroupVersion}}'

# Verify Docker Compose v2 plugin
docker compose version

# Run an ephemeral test container
docker run --rm alpine uname -a
```

---

| [Next: Module 01 Core Fundamentals >](../01-fundamentals/README.md) |
| :--- |
