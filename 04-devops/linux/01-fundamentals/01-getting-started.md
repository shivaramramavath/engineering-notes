<!--
================================================================================
Metadata:
  Title: Getting Started with Linux
  Track: Linux & Systems Engineering / DevOps
  Difficulty: Beginner
  Estimated Time: 45 minutes
  Prerequisites: None
  Key Terminology: Kernel, Distribution, GNU Userland, Shell, WSL2, POSIX, ABI
================================================================================
-->

# Getting Started with Linux

[Back to DevOps Track Overview](../README.md) | [Back to Module Overview](./README.md) | [02-linux-architecture.md ->](./02-linux-architecture.md)

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Troubleshooting](#7-common-mistakes--troubleshooting)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table](#9-comparison-table)
- [10. Real-World Interview Questions](#10-real-world-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference Cheat Sheet](#12-summary--quick-reference-cheat-sheet)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

Linux powers over 90% of the world's public cloud infrastructure, containerized microservices, high-performance computing clusters, and embedded systems. Whether deploying a Node.js API to AWS ECS, debugging a database replica on bare metal, or securing a Kubernetes ingress, system engineers operate in Linux environments.

Understanding Linux at a production level means dispelling common abstractions:
- Linux is not an all-in-one operating system like macOS or Windows; it is an open-source, monolithic kernel originally authored by Linus Torvalds in 1991.
- A functional operating system requires the Linux kernel plus a complete userspace ecosystem: the GNU C library (`glibc`), core system utilities (`coreutils`), an initialization daemon (`systemd`), networking daemons, and package managers.
- Modern DevOps workflows rely directly on Linux kernel primitives: containers are not virtual machines, but isolated Linux processes bound by **namespaces** and throttled by **control groups (cgroups)**.

Establishing a solid foundation begins with selecting an appropriate distribution, provisioning a compliant local or cloud lab environment, and navigating the command line with operational discipline.

---

## 2. Core Concepts

### Kernel vs. Userland
The Linux operating environment is strictly partitioned into two fundamental privilege levels:
1. **Kernel Space (Ring 0)**: Executes with unrestricted access to the underlying CPU, physical RAM, interrupts, and hardware registers. The kernel exposes its capabilities to userspace via system calls (`syscalls`).
2. **User Space (Ring 3)**: Where user applications, shells, language runtimes, background services, and utilities run. Applications cannot touch hardware directly; they must make system calls through the standard C library (`glibc` or `musl`).

```
+-------------------------------------------------------------+
| User Applications (Nginx, PostgreSQL, Python, Node.js)       |
+-------------------------------------------------------------+
| Standard C Library (glibc / musl) & GNU Core Utilities       |
+-------------------------------------------------------------+
| System Call Interface (sys_enter, syscall table)            |
+=============================================================+  <-- Privilege Boundary
| Linux Kernel (Scheduler, Memory Manager, VFS, Netfilter)    |  <-- Ring 0 (Kernel Space)
+-------------------------------------------------------------+
| Hardware Architecture (x86_64, aarch64, RAM, Disks, NICs)    |
+-------------------------------------------------------------+
```

### Distributions (Distros)
A Linux distribution combines:
- A specific kernel build (e.g., Linux 6.8).
- A userland software collection (GNU utilities, bash, systemd).
- A package manager (`apt`, `dnf`, `apk`, `pacman`) and official repositories.
- Default system configurations, security policies (AppArmor vs. SELinux), and release schedules.

| Distro Family | Reference Distributions | Package System | Init System | Security Engine | Default Target Environment |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Debian** | Ubuntu 24.04 LTS, Debian 12 | `apt` / `dpkg` | `systemd` | AppArmor | Cloud VMs, Docker, Web Servers |
| **Red Hat** | RHEL 9, Rocky Linux, Fedora | `dnf` / `rpm` | `systemd` | SELinux | Enterprise Bare-Metal, Finance |
| **Alpine** | Alpine 3.19+ | `apk` | OpenRC | Seccomp | Ultra-minimalist container images |
| **Arch** | Arch Linux | `pacman` | `systemd` | Optional | Rolling-release, Developer workstations |

---

## 3. How It Works Internally

### WSL2 vs. Hypervisors vs. Bare Metal
When provisioning a local Linux development environment, engineers primarily choose between Windows Subsystem for Linux 2 (WSL2), type-2 hypervisors (VirtualBox, VMware, UTM), or cloud compute instances:

```
+--------------------------------------------------------------------------+
| ARCHITECTURE COMPARISON                                                  |
|                                                                          |
| 1. Bare Metal / Cloud VM (AWS EC2 / Hetzner)                             |
| [ Hardware / Bare-Metal NIC / Disk ] -> [ Linux Kernel 6.8 ] -> [ Apps ] |
|                                                                          |
| 2. WSL2 (Lightweight VM on Hyper-V)                                      |
| [ Windows Host Hardware ]                                                |
|   -> [ Hyper-V Micro-Hypervisor ]                                        |
|        -> [ Microsoft-tuned Linux Kernel ] -> [ Ubuntu Userland ]        |
|        -> [ 9P Protocol VFS Bridge ] <-> [ Windows C:\ Filesystem ]      |
|                                                                          |
| 3. Type-2 Local VM (VirtualBox / UTM)                                    |
| [ Host OS (macOS/Win) ] -> [ Hypervisor App ] -> [ Virtual Disk Image ]  |
|                                                  -> [ Guest Linux Kernel]|
+--------------------------------------------------------------------------+
```

WSL2 uses a lightweight virtualization utility (Hyper-V architecture) that boots a customized Linux kernel within ~1 second. However, accessing files across the boundary (e.g., accessing `/mnt/c/` from Linux) passes through a Plan 9 (`9P`) network file protocol bridge, incurring significant I/O latency. Production code and git repositories must always reside in the native Linux filesystem (e.g., `~/projects`).

---

## 4. Syntax & Usage

### Baseline Verification Commands
After booting into any Linux environment, execute these commands to establish system facts:

```bash
# 1. Print kernel release, architecture, and hostname
uname -a
# Example output: Linux dev-srv 6.8.0-45-generic #45-Ubuntu SMP PREEMPT_DYNAMIC x86_64 GNU/Linux

# 2. Inspect distribution metadata and version release
cat /etc/os-release

# 3. Verify CPU architecture and core allocation
lscpu | grep -E "Model name|Architecture|CPU\(s\):"

# 4. Check available and total RAM
free -h

# 5. Inspect root disk space and mount filesystem types
df -hT /

# 6. Verify init system is systemd (PID 1)
ps -p 1 -o comm=
# Expected output: systemd
```

---

## 5. Practical Examples

### Example 1: Installing Ubuntu 24.04 on WSL2 (Windows Host)
```powershell
# Run in Administrative PowerShell on Windows 11
wsl --update
wsl --install -d Ubuntu-24.04

# Verify installed distributions and virtualization backend version (must be version 2)
wsl --list --verbose

# Configure default WSL settings in %USERPROFILE%\.wslconfig
# [wsl2]
# memory=8GB
# processors=4
# swap=2GB
```

### Example 2: Provisioning a Clean Ubuntu 24.04 Cloud Lab (EC2 / Multipass)
```bash
# Launching locally with Canonical Multipass
multipass launch 24.04 --name linux-lab --cpus 2 --memory 4G --disk 20G
multipass shell linux-lab

# Verify package repositories and bring system up to date
sudo apt update && sudo apt upgrade -y

# Install essential baseline diagnostic packages
sudo apt install -y curl wget git jq htop iotop net-tools lsof strace
```

### Example 3: Verifying POSIX Compliance and glibc Version
```bash
# Check glibc version (crucial for binary compatibility and CGo/Rust compiling)
ldd --version
# Output: ldd (Ubuntu GLIBC 2.39-0ubuntu8) 2.39

# Inspect POSIX configuration limits
getconf PAGESIZE
# Output: 4096 (4 KiB memory page size)

getconf OPEN_MAX
# Output: 1048576 (Maximum open file descriptors per process)
```

### Example 4: Diagnosing WSL2 Cross-Filesystem I/O Degradation (Anti-Pattern)
```bash
# BAD: Running intensive disk operations on Windows 9P mount
cd /mnt/c/Users/Dev/my-project
time git status
# Elapsed real time: 4.82s (Severe 9P translation overhead)

# REMEDIATION: Move repository to native Linux ext4 filesystem
mkdir -p ~/projects
cd ~/projects
git clone /mnt/c/Users/Dev/my-project
time git status
# Elapsed real time: 0.02s (240x speedup)
```

### Example 5: Bootstrapping an Alpine Linux Container for Comparison
```bash
# Run lightweight Alpine container
docker run --rm -it alpine:3.20 sh

# Check C standard library in Alpine (Notice musl, NOT glibc)
/lib/ld-musl-x86_64.so.1 --version
# Output: Version 1.2.5

# Check package manager syntax difference
apk update && apk add curl jq

# Notice difference in core utilities (Busybox implementation)
ls --version
# BusyBox v1.36.1 multi-call binary
exit
```

---

## 6. Best Practices

- **Standardize on LTS**: For production servers and developer workstations, always choose Long Term Support (LTS) releases (e.g., Ubuntu 24.04 LTS or RHEL 9). They receive 5 years of security updates and vendor stability.
- **Isolate Workloads from Host**: Never install database daemons, caches, or multi-tenant runtimes directly on your primary host OS without containerization or virtualization.
- **Maintain Dotfiles in Git**: Store your shell configurations (`~/.bashrc`, `~/.vimrc`, `~/.tmux.conf`) in a version-controlled git repository to replicate environments seamlessly across servers.
- **Verify Architecture Compatibility**: Always check `uname -m` before pulling precompiled binaries. `x86_64` (AMD64) binaries will fail on `aarch64` (AWS Graviton, Apple Silicon VMs) with `Exec format error`.
- **Never Run As Unrestricted Root**: Create an unprivileged user with scoped `sudo` privileges. Running interactive terminals as `root` eliminates guardrails against catastrophic file deletions.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Issue | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| Storing git repos in `/mnt/c/...` on WSL2 | WSL2 uses 9P protocol over virtual network for Windows drives; file metadata and file watchers (`inotify`) fail or crawl. | Store all code inside the Linux root filesystem: `/home/<user>/...` |
| `Exec format error` when running downloaded binary | Architecture mismatch (e.g., executing an `x86_64` binary on an `aarch64` or ARM64 VM). | Run `uname -m` and download the matching architecture binary. |
| `systemctl: command not found` or `system has not been booted with systemd` | WSL2 instance booted without systemd init configured, or running inside Docker container. | Add `[boot]\nsystemd=true` to `/etc/wsl.conf` and restart WSL via `wsl --shutdown`. |
| Assuming `sh` is always `bash` | On Ubuntu/Debian, `/bin/sh` is symlinked to `dash` (lightweight POSIX shell). "Bashisms" like `[[` or `array=(1 2)` fail. | Always set the shebang to `#!/usr/bin/env bash` if using bash-specific syntax. |
| Running `sudo npm install -g` or `sudo pip install` | Pollutes system Python/Node libraries managed by `apt` or `dnf`, breaking OS package manager hooks. | Use virtual environments (`python3 -m venv .venv`) or Node version managers (`nvm`, `fnm`). |
| Modifying `/etc/resolv.conf` directly | Modern distributions use `systemd-resolved` or NetworkManager. Manual edits get overwritten on reboot or DHCP renewal. | Configure DNS in `/etc/systemd/resolved.conf` or netplan configs. |

---

## 8. Performance, Security & Cost Implications

- **Performance**: Bare-metal Linux provides lowest latency and direct PCIe hardware access. KVM/Hyper-V hypervisors incur < 2% virtualization penalty. WSL2 achieves near-native CPU performance, but disk throughput degrades significantly when crossing the `/mnt/` host boundary.
- **Security**: Linux isolates processes through kernel user IDs (UIDs). Process memory spaces are strictly segmented via the hardware Memory Management Unit (MMU). Leaving root passwords default or using password-based SSH instead of Ed25519 keys represents the primary attack vector for cloud VMs.
- **Cost**: Open-source distributions (Ubuntu, Rocky Linux, Debian) carry $0 licensing costs compared to Windows Server. RHEL incurs subscription costs for enterprise SLA support, certifications, and compliance tooling.

---

## 9. Comparison Table

| Attribute | Ubuntu 24.04 LTS | RHEL 9 / Rocky Linux 9 | Alpine Linux 3.20 |
| :--- | :--- | :--- | :--- |
| **Primary Domain** | Cloud, Web, Containers | Enterprise, Banking, Gov | Micro-containers, Edge |
| **Package Manager** | `apt` (Debian format `.deb`) | `dnf` (RPM format `.rpm`) | `apk` (`.apk`) |
| **C Standard Library**| GNU C Library (`glibc`) | GNU C Library (`glibc`) | `musl` libc |
| **Default Shell** | Bash (interactive), Dash (`/bin/sh`) | Bash (interactive & `/bin/sh`) | BusyBox Almquist Shell (`ash`) |
| **Init System** | `systemd` v255 | `systemd` v252 | `OpenRC` |
| **Default Mandatory Access Control** | AppArmor | SELinux | Disabled / Seccomp |
| **Base Image Size (Docker)** | ~77 MB | ~180 MB | ~7 MB |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What is the fundamental technical difference between the Linux Kernel and a Linux Distribution?</strong></summary>

**Answer**:
The Linux kernel is a single software program: the monolithic core responsible for process scheduling, memory allocation, virtual filesystem abstractions, device drivers, and network socket management. It executes in Ring 0 (kernel space).

A Linux distribution is a complete, deployable operating system built around that kernel. It packages the kernel with:
1. The GNU userland utilities (`ls`, `cp`, `grep`).
2. The C standard library runtime (`glibc` or `musl`).
3. An initialization daemon and service manager (`systemd`).
4. A software repository ecosystem and package manager (`apt`, `dnf`, `pacman`).
5. Default security frameworks (SELinux, AppArmor) and networking configurations.
</details>

<details>
<summary><strong>2. How does WSL2 differ architecturally from WSL1?</strong></summary>

**Answer**:
WSL1 was a syscall translation layer: when a Linux binary invoked a syscall (e.g., `fork` or `epoll`), the Windows NT kernel driver (`lxcore.sys`) translated that call into a corresponding Windows NT API call. It ran without a real Linux kernel, suffering from imperfect syscall compatibility and poor socket/filesystem performance.

WSL2 uses a lightweight Type-1 Hyper-V utility VM running an actual open-source Linux kernel customized by Microsoft. This provides 100% system call compatibility (including real eBPF, Docker daemon execution, and cgroup v2 support) and native ext4 filesystem performance inside the virtual hard disk (`ext4.vhdx`).
</details>

<details>
<summary><strong>3. Why might a statically compiled Go or Rust binary fail to run inside an Alpine Linux container?</strong></summary>

**Answer**:
If a Go or Rust binary uses `CGo` or links dynamically against the GNU C library (`glibc`), it expects the dynamic linker at `/lib64/ld-linux-x86-64.so.2`. Alpine Linux does not use `glibc`; it uses `musl` libc, where the dynamic linker is located at `/lib/ld-musl-x86_64.so.1`. When executed, the kernel fails to resolve the ELF dynamic interpreter, emitting the misleading error `sh: ./mybinary: not found` despite the file existing on disk.

Remedy: Compile the binary with `CGO_ENABLED=0` for pure static compilation, or build against `musl` (e.g., `cargo build --target x86_64-unknown-linux-musl`).
</details>

<details>
<summary><strong>4. What is the significance of PID 1 in a Linux system?</strong></summary>

**Answer**:
PID 1 is the first userspace process launched by the Linux kernel after mounting the root filesystem during the boot phase. Historically `SysV init`, in modern distributions it is `systemd`. PID 1 has unique responsibilities:
1. It initializes all system services, daemons, mounts, and networking.
2. It adopts orphaned child processes when their parent terminates and reaps them (`waitpid`), preventing zombie process accumulation.
3. If PID 1 crashes or exits unexpectedly, the Linux kernel panics and halts the system immediately.
</details>

<details>
<summary><strong>5. Explain the difference between user space (Ring 3) and kernel space (Ring 0).</strong></summary>

**Answer**:
Modern x86 and ARM processors enforce hardware privilege rings.
- **Ring 0 (Supervisor / Kernel Mode)**: Unrestricted hardware execution. The CPU can execute any instruction, manipulate page tables directly, service hardware interrupts, and write to raw memory addresses.
- **Ring 3 (User Mode)**: Constrained execution. Applications have isolated virtual address spaces and cannot directly communicate with hardware peripherals or inspect memory belonging to other processes.

When an application needs to read a file or send network traffic, it loads syscall arguments into CPU registers and triggers a software trap or `syscall` instruction, safely transferring execution to the kernel dispatch table in Ring 0.
</details>

<details>
<summary><strong>6. What is the difference between `/bin/sh` and `/bin/bash` on Ubuntu?</strong></summary>

**Answer**:
On Ubuntu and Debian systems, `/bin/sh` is a symbolic link to `/bin/dash` (Debian Almquist Shell), not `/bin/bash`. `dash` is a lightweight, strictly POSIX-compliant shell optimized for fast script execution during system boot.

Bash (Bourne-Again SHell) is a feature-rich interactive shell that includes non-POSIX extensions ("bashisms") such as associative arrays, regex matching via `=~`, substring indexing, and double brackets `[[ ]]`. If a script begins with `#!/bin/sh` but contains bashisms, it will fail on Ubuntu while succeeding on systems like Arch or Alpine where `/bin/sh` might point to bash or ash.
</details>

<details>
<summary><strong>7. How do you identify which Linux distribution and kernel version a machine is running without a GUI?</strong></summary>

**Answer**:
1. Kernel version and architecture: `uname -srm` (e.g., `Linux 6.8.0-45-generic x86_64`).
2. Distribution release details: `cat /etc/os-release` (parse `NAME`, `VERSION`, `ID`, and `VERSION_CODENAME`).
3. Standard systemd tool: `hostnamectl` (provides unified view of static hostname, kernel version, architecture, and OS release).
</details>

<details>
<summary><strong>8. What is the role of POSIX in Linux system design?</strong></summary>

**Answer**:
POSIX (Portable Operating System Interface) is an IEEE standard family designed to ensure source-code level compatibility between Unix-like operating systems. It defines standard API interfaces for system calls (such as `open`, `read`, `fork`, `kill`), utility syntax (`ls`, `grep`), thread libraries (`pthreads`), and shell scripting standards. Linux adheres very closely to POSIX, allowing C and C++ programs written for FreeBSD, macOS, or Solaris to compile on Linux with minimal modifications.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Query System Architecture and Environment Baseline</strong></summary>

**Task**: Write a one-liner shell script that prints the operating system name, kernel release, total physical RAM in megabytes, and the current active shell.

**Solution**:
```bash
echo "OS: $(grep '^PRETTY_NAME=' /etc/os-release | cut -d= -f2 | tr -d '\"') | Kernel: $(uname -r) | RAM: $(free -m | awk '/^Mem:/{print $2}') MB | Shell: $SHELL"
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Identify the Initialization System</strong></summary>

**Task**: Prove programmatically whether the current Linux machine is running `systemd` or an alternate init system like `OpenRC` or `SysVinit`.

**Solution**:
```bash
if [ "$(ps -p 1 -o comm=)" = "systemd" ]; then
    echo "Init system is systemd (PID 1 confirmed)"
else
    echo "Alternate init system detected: $(ps -p 1 -o comm=)"
fi
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Validate WSL2 Filesystem Performance Gap</strong></summary>

**Task**: Create a test script that measures the time required to write 2,000 small files to the native Linux filesystem (`/tmp`) versus the mounted Windows filesystem (`/mnt/c/Temp` or `/mnt/c/Users/Public`).

**Solution**:
```bash
#!/usr/bin/env bash
mkdir -p /tmp/perf_test /mnt/c/Temp/perf_test 2>/dev/null || true

echo "Testing native ext4 filesystem (/tmp)..."
time for i in $(seq 1 2000); do echo "data" > "/tmp/perf_test/file_$i.txt"; done

if [ -d "/mnt/c/Temp/perf_test" ]; then
    echo "Testing 9P Windows mounted filesystem (/mnt/c/Temp)..."
    time for i in $(seq 1 2000); do echo "data" > "/mnt/c/Temp/perf_test/file_$i.txt"; done
fi

# Cleanup
rm -rf /tmp/perf_test /mnt/c/Temp/perf_test 2>/dev/null || true
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Detect Dynamic Linker and Missing Shared Libraries</strong></summary>

**Task**: Inspect the standard `curl` binary on your system to determine which dynamic linker it uses and list all shared library dependencies.

**Solution**:
```bash
# Locate binary path
CURL_PATH=$(which curl)

# Inspect ELF header for interpreter (dynamic linker)
readelf -l "$CURL_PATH" | grep "program interpreter"

# Inspect shared library bindings
ldd "$CURL_PATH"
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Replicate a Minimal Userland with BusyBox and Chroot</strong></summary>

**Task**: Create an isolated minimal root filesystem using `chroot` and a static BusyBox binary, without using Docker.

**Solution**:
```bash
# 1. Create directory structure
sudo mkdir -p /tmp/mini-root/{bin,dev,proc,sys,etc}

# 2. Install busybox-static (Ubuntu/Debian)
sudo apt update && sudo apt install -y busybox-static

# 3. Copy static binary into jail
sudo cp /bin/busybox /tmp/mini-root/bin/busybox

# 4. Create symlinks inside jail
sudo chroot /tmp/mini-root /bin/busybox --install -s /bin

# 5. Execute isolated shell
sudo chroot /tmp/mini-root /bin/sh
# Inside isolated environment:
ls -la /
echo "Hello from minimal chroot jail"
exit
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Automated Distribution and Hardware Profiler Script</strong></summary>

**Task**: Author a hardened bash script that generates a JSON report containing CPU model, core count, memory topology, virtualization technology, and OS security state.

**Solution**:
```bash
#!/usr/bin/env bash
set -euo pipefail

CPU_MODEL=$(lscpu | awk -F: '/Model name/ {gsub(/^[ \t]+/, "", $2); print $2}')
CPU_CORES=$(nproc)
TOTAL_RAM_MB=$(free -m | awk '/Mem:/ {print $2}')
OS_NAME=$(grep '^PRETTY_NAME=' /etc/os-release | cut -d= -f2 | tr -d '"')
KERNEL=$(uname -r)
VIRT_TYPE=$(systemd-detect-virt || echo "none/bare-metal")

cat <<EOF
{
  "system": {
    "os": "$OS_NAME",
    "kernel": "$KERNEL",
    "virtualization": "$VIRT_TYPE"
  },
  "hardware": {
    "cpu_model": "$CPU_MODEL",
    "logical_cores": $CPU_CORES,
    "total_ram_mb": $TOTAL_RAM_MB
  }
}
EOF
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Command | Description |
| :--- | :--- | :--- |
| **Check Kernel** | `uname -r` | Prints kernel release version |
| **Check OS Release** | `cat /etc/os-release` | Distribution name, version, and family |
| **Check Hardware Arch** | `uname -m` / `lscpu` | Displays x86_64, aarch64, or armv7l |
| **Check Virtualization** | `systemd-detect-virt` | Identifies KVM, WSL, VMware, or bare-metal |
| **Check Memory** | `free -h` | RAM and swap space utilization |
| **Check Disk Usage** | `df -hT` | Mounted filesystems, types, and free storage |
| **Verify Init System** | `ps -p 1 -o comm=` | Identifies PID 1 program name (usually systemd) |
| **Check C Library** | `ldd --version` | Checks installed glibc release |

---

## 13. Further Reading & Official Documentation

- [Linux Kernel Documentation](https://www.kernel.org/doc/html/latest/)
- [Ubuntu Server 24.04 LTS Guide](https://ubuntu.com/server/docs)
- [Microsoft Learn: WSL2 Architecture Overview](https://learn.microsoft.com/en-us/windows/wsl/compare-versions)
- [The Linux Foundation - POSIX Standards Overview](https://standards.ieee.org/ieee/1003.1/7101/)
- [Red Hat Enterprise Linux 9 Documentation](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/9)

---

[Back to DevOps Track Overview](../README.md) | [Back to Module Overview](./README.md) | [02-linux-architecture.md ->](./02-linux-architecture.md)
