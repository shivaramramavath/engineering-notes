---
title: "Installing Docker on Linux, macOS, and Windows"
description: "Comprehensive installation and configuration guide for Docker Engine on Linux distributions, Docker Desktop on macOS (Apple Silicon/Intel), and Windows (WSL 2 backend)."
category: "DevOps / Containers"
tags: ["docker", "installation", "linux", "macos", "windows", "wsl2"]
difficulty: "Beginner"
prerequisites: ["Administrative/sudo rights on target OS", "Basic shell familiarity"]
estimated_read_time: "25 min"
---

# Installing Docker on Linux, macOS, and Windows

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. Core Concepts & Terminology](#2-core-concepts--terminology)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Installation Procedures](#4-syntax--installation-procedures)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table: OS Deployment Methods](#9-comparison-table-os-deployment-methods)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

Docker Engine is fundamentally a Linux technology. It relies directly on Linux kernel features—specifically control groups (`cgroups`), namespaces, OverlayFS, and `seccomp` filters. Consequently, installing Docker varies significantly between operating systems:

- On **Linux**, Docker Engine runs directly on the bare-metal host kernel with zero virtualization overhead.
- On **macOS** and **Windows**, Docker must run inside a Linux virtual machine managed transparently behind the scenes (HyperKit/Virtualization Framework on macOS, and Windows Subsystem for Linux v2 (WSL 2) or Hyper-V on Windows).

Understanding these OS-level differences is crucial for configuring filesystem performance, network addressability, resource allocation, and troubleshooting installation hurdles across engineering teams.

```
+-------------------------------------------------------------------------------+
|                             OS ARCHITECTURE DIFFERENCES                       |
+------------------------------------+------------------------------------------+
|          NATIVE LINUX              |           MACOS & WINDOWS (WSL 2)        |
+------------------------------------+------------------------------------------+
|  [Docker CLI]                      |  [Docker CLI (Host OS)]                  |
|        |                           |         | (named pipe / unix socket)     |
|        v (unix domain socket)      |         v                                |
|  [dockerd Daemon]                  |  +-------------------------------------+ |
|        |                           |  | Linux VM / WSL 2 Utility VM         | |
|        v                           |  |  [dockerd Daemon]                   | |
|  [Host Linux Kernel]               |  |  [Embedded Linux Kernel]            | |
|  - Namespaces, Cgroups, OverlayFS  |  +-------------------------------------+ |
|                                    |         |                                |
|                                    |         v                                |
|                                    |  [Host Hypervisor / Windows Kernel]      |
+------------------------------------+------------------------------------------+
```

---

## 2. Core Concepts & Terminology

- **Docker Engine (Community Edition - CE)**: The open-source client-server container runtime packaged for Linux distributions.
- **Docker Desktop**: A commercial developer productivity suite for macOS and Windows providing a GUI, automated VM lifecycle management, Kubernetes cluster, Docker Extensions, and CLI integration.
- **WSL 2 (Windows Subsystem for Linux 2)**: A true lightweight Linux utility VM managed by Windows that runs a real Microsoft-maintained Linux kernel with native system call compatibility.
- **Rosetta 2 for Linux (macOS)**: macOS Ventura+ feature enabling Apple Silicon (ARM64) Macs to emulate x86_64 binaries inside Linux virtual machines with high performance.
- **VirtioFS**: A modern shared filesystem protocol used in macOS and Linux virtualization to accelerate file sharing between the host OS and the Linux VM.

---

## 3. How It Works Internally

### Linux Installation Architecture
On native Linux (Debian, Ubuntu, RHEL, Fedora), Docker packages install systemd service units (`docker.service`, `containerd.service`). The daemon binds to the local UNIX domain socket `/var/run/docker.sock` owned by `root:docker`.

### macOS Architecture
macOS cannot run Linux ELF binaries natively. Docker Desktop on macOS launches a lightweight virtual machine using the native Apple `Virtualization.framework`. The host CLI communicates with the VM's daemon through a bridged UNIX domain socket located at `~/.docker/run/docker.sock` (or `/var/run/docker.sock` via a symlink).

### Windows Architecture with WSL 2
On Windows 10/11:
1. Docker Desktop installs two dedicated WSL distributions: `docker-desktop` (the management VM) and `docker-desktop-data` (stores container layers and volumes).
2. The Windows Docker CLI talks to `\\.\pipe\docker_engine` named pipe or bridges into WSL 2 bash sessions by injecting `/var/run/docker.sock`.
3. Files accessed under `/mnt/c/` incur a translation overhead across the 9P/Plan9 network protocol; files stored within the native Linux virtual disk (`\\wsl$\Ubuntu\home\...`) run at native ext4 speeds.

---

## 4. Syntax & Installation Procedures

### 4.1. Official Repository Installation on Ubuntu / Debian

Do not use distribution default packages (e.g., `apt install docker.io`) as they are frequently outdated. Always use the official Docker repository:

```bash
# 1. Update package index and install prerequisites
sudo apt-get update
sudo apt-get install -y ca-certificates curl gnupg lsb-release

# 2. Add Docker's official GPG key
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg

# 3. Set up the stable repository
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
  $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

# 4. Install Docker Engine, CLI, containerd, and Compose plugin
sudo apt-get update
sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

# 5. Verify systemd service status
sudo systemctl status docker --no-pager
```

### 4.2. Official Repository Installation on RHEL / AlmaLinux / Rocky / Fedora

```bash
# 1. Install yum-utils
sudo dnf install -y yum-utils

# 2. Add Docker repository
sudo yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo

# 3. Install packages
sudo dnf install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

# 4. Enable and start systemd service
sudo systemctl enable --now docker
```

### 4.3. Docker Desktop on macOS Installation

1. Download the official installer matching your processor:
   - Apple Silicon (M1/M2/M3/M4): `Docker.dmg` (ARM64)
   - Intel: `Docker.dmg` (x86_64)
2. Double-click `Docker.dmg`, drag the Docker icon into `/Applications`.
3. Launch Docker from Spotlight. Grant necessary system privileges.
4. Recommended Settings:
   - Go to **Settings > General**: Enable **Use Virtualization framework**.
   - Go to **Settings > Resources > File sharing**: Select **VirtioFS** for optimal I/O speed.
   - Go to **Settings > Features in development**: Enable **Use Rosetta for x86_64/amd64 emulation on Apple Silicon**.

### 4.4. Docker Desktop on Windows Installation (WSL 2 Backend)

1. Ensure WSL 2 is installed in an elevated PowerShell:
   ```powershell
   wsl --install
   wsl --update
   wsl --set-default-version 2
   ```
2. Download `Docker Desktop Installer.exe` from the official Docker portal.
3. Run the installer and ensure the option **"Use WSL 2 instead of Hyper-V (recommended)"** is checked.
4. Restart Windows when prompted.
5. In Docker Desktop Settings:
   - Go to **Settings > Resources > WSL Integration**.
   - Enable integration with your installed Linux distribution (e.g., `Ubuntu`).

---

## 5. Practical Examples

### Example 1: Verifying Complete Stack Installation
Run a multi-faceted verification pipeline confirming CLI, daemon, buildx, and compose plugins:

```bash
# Verify client and daemon versions
docker version

# Verify compose plugin installation
docker compose version

# Verify buildx builder availability
docker buildx ls

# Verify container execution and outbound network resolution
docker run --rm alpine nslookup docker.com
```

### Example 2: Managing Non-Root Access on Linux
Granting a standard user permission to manage containers without prefixing commands with `sudo`:

```bash
# 1. Create docker group if it does not already exist
sudo groupadd docker 2>/dev/null || true

# 2. Add current user to docker group
sudo usermod -aG docker $USER

# 3. Activate changes in current shell session (or logout and re-login)
newgrp docker

# 4. Verify passwordless execution
docker run --rm hello-world
```

### Example 3: Configuring Daemon Defaults via `daemon.json`
Apply production defaults: custom logging driver, log size limits, and DNS servers.

```bash
# Create or edit daemon configuration file
sudo tee /etc/docker/daemon.json <<'EOF'
{
  "log-driver": "json-file",
  "log-opts": {
    "max-size": "50m",
    "max-file": "3"
  },
  "dns": ["1.1.1.1", "8.8.8.8"],
  "features": {
    "buildkit": true
  }
}
EOF

# Reload systemd and restart Docker daemon
sudo systemctl daemon-reload
sudo systemctl restart docker
```

### Example 4: Diagnosing WSL 2 Filesystem Performance on Windows
Demonstrate the difference between host-mounted files and native Linux filesystems.

```bash
# SLOW (Mounted Windows NTFS drive via 9P):
cd /mnt/c/Users/<user>/my-project
docker run --rm -v $(pwd):/app -w /app alpine time dd if=/dev/zero of=test.bin bs=1M count=100

# FAST (Native WSL2 ext4 filesystem):
cd /home/<user>/my-project
docker run --rm -v $(pwd):/app -w /app alpine time dd if=/dev/zero of=test.bin bs=1M count=100
```

### Example 5: Verifying Multi-Arch Build Capability on Apple Silicon
Verify that your local Docker setup can build and run both ARM64 and AMD64 containers.

```bash
# Run native ARM64 container on Apple Silicon
docker run --rm alpine uname -m
# Output: aarch64

# Run emulated AMD64 container using Rosetta 2
docker run --rm --platform linux/amd64 alpine uname -m
# Output: x86_64
```

---

## 6. Best Practices & Production Guidelines

1. **Avoid Package Manager Defaults on Linux**: Never install `docker.io` via standard Ubuntu repos or `podman-docker` without understanding the differences. Use official `download.docker.com` repos.
2. **Restrict Non-Root Socket Access**: Adding users to the `docker` group grants root-equivalent privileges on the host (since a user can mount `/` into a privileged container). On shared servers, use **Rootless Docker** instead.
3. **Configure Log Rotation Early**: Default Docker setups allow container JSON logs to grow indefinitely until the host disk fills up. Always configure `max-size` and `max-file` in `/etc/docker/daemon.json`.
4. **Tune Virtual Machine Resources on macOS/Windows**: By default, Docker Desktop may allocate only 2GB RAM. Heavy development stacks (Java, Postgres, ElasticSearch) will crash with OOM errors. Allocate at least 6-8GB RAM in Docker Desktop settings.
5. **Keep Projects Inside the WSL 2 Root on Windows**: Never clone repositories into `/mnt/c/Users/...` when running Docker with WSL 2. Clone them into `/home/username/` inside WSL 2 for up to 10x faster I/O.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Fix / Solution |
|:---|:---|:---|
| **Permission denied on `/var/run/docker.sock`** | Current Linux user is not in the `docker` group. | Run `sudo usermod -aG docker $USER` and log out/in. |
| **Cloning Git repos into `/mnt/c/` on Windows** | WSL 2 accesses Windows NTFS drives across a network translation layer, crippling Node/Webpack builds. | Store all source code inside the native WSL filesystem (`/home/<user>/`). |
| **Docker daemon fails to start on boot** | Systemd service is disabled or port conflict on configured dockerd daemon socket. | Run `sudo systemctl enable --now docker` and inspect `journalctl -u docker -e`. |
| **Out of Memory (OOM) during builds on Mac** | Docker Desktop VM has inadequate default RAM allocated. | Adjust VM settings in Docker Desktop GUI > Settings > Resources. |
| **Disk space exhausted by Docker Desktop** | Virtual disk image (`Docker.raw` on Mac or `ext4.vhdx` on Windows) does not automatically shrink. | Run `docker system prune -af --volumes` and compact the virtual disk. |
| **Docker Desktop Licensing Non-Compliance** | Using Docker Desktop in commercial enterprises (>250 employees or >$10M revenue) without a paid subscription. | Purchase commercial license or switch to open-source alternatives like Podman or Colima. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Linux**: Bare-metal execution speed. System calls bypass virtualization layers entirely.
- **macOS**: CPU performance is near-native, but bind-mounting large directories (`node_modules`, large vendor folders) experiences latency. VirtioFS significantly mitigates this compared to legacy gRPC-FUSE.
- **Windows (WSL 2)**: Native speed when files reside in WSL ext4 filesystem. 70-80% performance penalty when accessing files through `/mnt/c/`.

### Security
- **Root Equivalent Socket**: Anyone with write access to `/var/run/docker.sock` can trivially escalate to root on the host by executing:
  ```bash
  docker run -v /:/host -it alpine chroot /host
  ```
- Secure the daemon socket using TLS authentication when exposing it over TCP.

### Cost
- **Docker Engine on Linux**: Free, 100% open-source Apache 2.0 license.
- **Docker Desktop**: Free for personal, educational, small business (<250 employees and <$10M revenue), paid subscription required for larger organizations.

---

## 9. Comparison Table: OS Deployment Methods

| Feature | Linux Native (docker-ce) | macOS (Docker Desktop) | Windows (WSL 2 Docker Desktop) |
|:---|:---|:---|:---|
| **Underlying OS** | Native Linux Kernel | Virtualization Framework VM | WSL 2 Utility VM |
| **Licensing** | Free (Apache 2.0) | Proprietary / Free Tier / Paid | Proprietary / Free Tier / Paid |
| **I/O Performance** | Native Host I/O | VirtioFS (High overhead on mounts) | Native inside WSL / Slow in `/mnt/c` |
| **Host Networking (`--net=host`)**| Works natively | Does not work (maps to VM IP) | Does not work (maps to WSL VM IP) |
| **Resource Footprint** | Low ($<100\text{ MB}$ base) | High (Requires dedicated 2-8GB VM) | Moderate (Dynamic WSL 2 memory) |
| **Multi-Arch Support** | QEMU user-mode binfmt | Rosetta 2 / QEMU | QEMU user-mode binfmt |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. Why doesn't <code>--net=host</code> behave the same way on macOS as it does on Linux?</strong></summary>

On Linux, `--net=host` places the container process directly in the host's network namespace, allowing it to bind directly to the host's physical network interfaces and localhost. On macOS, Docker runs inside a Linux virtual machine. Consequently, `--net=host` binds the container to the *virtual machine's* network stack, not the macOS host network stack. You cannot access the container on macOS localhost unless ports are explicitly published using `-p`.
</details>

<details>
<summary><strong>2. What is the security consequence of adding a developer to the local <code>docker</code> group?</strong></summary>

Adding a user to the `docker` group grants that user root privileges on the host operating system without requiring `sudo` credentials. The user can mount the host's root filesystem inside a container (`docker run -v /:/host-root alpine`) and modify `/etc/shadow`, `/etc/sudoers`, or install rootkits on the underlying host.
</details>

<details>
<summary><strong>3. How does Docker Desktop on Windows integrate with WSL 2?</strong></summary>

Docker Desktop leverages the WSL 2 API to spin up two lightweight Linux distributions: `docker-desktop` (containing `dockerd` and container runtime tools) and `docker-desktop-data` (containing persistent layer and volume virtual disks). It proxies the Docker CLI through Windows named pipes (`\\.\pipe\docker_engine`) and injects symlinks to `/var/run/docker.sock` into any user-selected WSL 2 distributions (such as Ubuntu).
</details>

<details>
<summary><strong>4. How does VirtioFS improve Docker Desktop performance on macOS?</strong></summary>

VirtioFS is a shared filesystem designed specifically for virtual machines. Unlike older technologies (such as OSXFS or gRPC-FUSE) that routed filesystem events and metadata lookups through userspace FUSE bridges across OS boundaries, VirtioFS integrates directly with the hypervisor memory map, enabling direct memory sharing (DAX) and asynchronous file I/O operations between macOS host directories and the Linux VM.
</details>

<details>
<summary><strong>5. What is the difference between Docker CE and Docker Desktop?</strong></summary>

Docker CE (Community Edition) is the open-source container engine comprising `dockerd`, `containerd`, and the `docker` CLI, designed to run natively on Linux. Docker Desktop is an integrated commercial product for macOS and Windows that packages Docker CE inside a managed virtual machine, along with a graphical user interface, local Kubernetes cluster, volume management, vulnerability scanning, and commercial extensions.
</details>

<details>
<summary><strong>6. How can an administrator verify which cgroups version Docker is utilizing?</strong></summary>

An administrator can execute `docker info | grep -i cgroup` or `docker info --format '{{.CgroupVersion}}'`. If the output is `2`, the host kernel is running unified Control Groups v2. If the output is `1`, it is running legacy Control Groups v1.
</details>

<details>
<summary><strong>7. How do you configure Docker daemon settings without restarting running containers?</strong></summary>

Certain configuration changes in `/etc/docker/daemon.json` (such as live-restore, default log drivers, or registry mirrors) can be reloaded without interrupting running containers by sending a `SIGHUP` signal to the Docker daemon (`kill -SIGHUP $(pidof dockerd)`) or executing `systemctl reload docker`, provided `"live-restore": true` is enabled in `daemon.json`.
</details>

<details>
<summary><strong>8. What is the purpose of the <code>"live-restore"</code> setting in Docker daemon configuration?</strong></summary>

The `"live-restore": true` setting instructs the Docker Engine to keep containers running even if the `dockerd` daemon process terminates, crashes, or is restarted during an upgrade. Without `live-restore`, restarting the Docker daemon terminates all child container processes.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Daemon Systemd Configuration</strong></summary>

**Goal**: Inspect the underlying systemd service file running `dockerd` on a Linux host.

```bash
# Locate systemd service definition
systemctl cat docker.service

# Inspect active daemon process arguments
ps aux | grep dockerd
```
</details>

<details>
<summary><strong>Exercise 2: Configure Live-Restore in `daemon.json`</strong></summary>

**Goal**: Enable live-restore to ensure container uptime across daemon service reloads.

```bash
# Add live-restore configuration
sudo mkdir -p /etc/docker
sudo python3 -c '
import json, os
path = "/etc/docker/daemon.json"
cfg = json.load(open(path)) if os.path.exists(path) else {}
cfg["live-restore"] = True
with open(path, "w") as f:
    json.dump(cfg, f, indent=2)
'

# Reload daemon configuration
sudo systemctl reload docker
```
</details>

<details>
<summary><strong>Exercise 3: Test Hostname and UTS Namespace Isolation</strong></summary>

**Goal**: Verify that containers isolate hostnames from the underlying host.

```bash
# Check host hostname
hostname

# Run container with custom hostname
docker run --rm -h container-alpha alpine hostname

# Confirm host hostname remained unchanged
hostname
```
</details>

<details>
<summary><strong>Exercise 4: Verify Multi-Architecture Emulation Setup</strong></summary>

**Goal**: Verify if QEMU multi-architecture binary format support (`binfmt_misc`) is configured.

```bash
# Check registered binfmt handlers
ls -l /proc/sys/fs/binfmt_misc/ 2>/dev/null || echo "Not a native Linux system"

# Test running a foreign architecture container
docker run --rm --platform linux/arm64 alpine uname -m
```
</details>

<details>
<summary><strong>Exercise 5: Benchmark Filesystem Read/Write Speeds</strong></summary>

**Goal**: Measure storage performance inside a container volume.

```bash
# Benchmark sequential write performance inside container
docker run --rm alpine dd if=/dev/zero of=/tmp/bench.dat bs=1M count=200 conv=fdatasync
```
</details>

<details>
<summary><strong>Exercise 6: Diagnose Docker Socket Permissions</strong></summary>

**Goal**: Inspect the filesystem permissions and group ownership of the Docker socket.

```bash
# Check socket ownership and permissions
ls -la /var/run/docker.sock

# Check which groups the current user belongs to
id -nG
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-----------------------------------------------------------------------------------+
|                        INSTALLATION QUICK CHEAT SHEET                             |
+-----------------------------------------------------------------------------------+
| Ubuntu / Debian:       sudo apt-get install docker-ce docker-ce-cli containerd.io |
| RHEL / Fedora:         sudo dnf install docker-ce docker-ce-cli containerd.io     |
| Non-root user access:  sudo usermod -aG docker $USER && newgrp docker             |
| Start & enable on boot:sudo systemctl enable --now docker                         |
| Daemon configuration:  /etc/docker/daemon.json                                    |
| Verify installation:   docker version && docker compose version                   |
| Mac/Windows storage:   Keep files inside WSL2 / VM disk for high performance      |
+-----------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Docker Documentation: Install Docker Engine on Ubuntu](https://docs.docker.com/engine/install/ubuntu/)
- [Docker Documentation: Docker Desktop for Mac](https://docs.docker.com/desktop/setup/install/mac-install/)
- [Microsoft Learn: Comparing WSL 1 and WSL 2](https://learn.microsoft.com/en-us/windows/wsl/compare-versions)
- [Apple Developer: Virtualization Framework](https://developer.apple.com/documentation/virtualization)
- [Docker Documentation: Daemon Configuration File (daemon.json)](https://docs.docker.com/engine/daemon/reference/#daemonjson)

---

| [< Previous: 01-getting-started.md](./01-getting-started.md) | [Next: 03-rootless-and-post-install-steps.md](./03-rootless-and-post-install-steps.md) > |
| :--- | :--- |
