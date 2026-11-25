# Container Security Hardening: Non-Root Execution, Capabilities, Seccomp, and AppArmor

```
Track: DevOps & Containerization
Module: 06-production
Prerequisites: 01-fundamentals/05-how-isolation-works.md, 03-dockerfile/08-base-image-choice.md
Difficulty: Advanced
Estimated Duration: 55 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [The Container Isolation Myth: Root in Container == Root on Host](#3-the-container-isolation-myth-root-in-container--root-on-host)
4. [The Non-Root Mandate](#4-the-non-root-mandate)
5. [User Namespaces Remapping (`userns-remap`)](#5-user-namespaces-remapping-userns-remap)
6. [Linux Capabilities: Principle of Least Privilege](#6-linux-capabilities-principle-of-least-privilege)
7. [Syscall Filtering with Seccomp](#7-syscall-filtering-with-seccomp)
8. [Mandatory Access Control: AppArmor and SELinux](#8-mandatory-access-control-apparmor-and-selinux)
9. [The `no-new-privileges` Protection](#9-the-no-new-privileges-protection)
10. [Read-Only Root Filesystems (`--read-only`)](#10-read-only-root-filesystems---read-only)
11. [Production Best Practices & Antipatterns](#11-production-best-practices--antipatterns)
12. [Common Pitfalls & Troubleshooting](#12-common-pitfalls--troubleshooting)
13. [Performance Tuning & Resource Optimization](#13-performance-tuning--resource-optimization)
14. [Security Hardening & Threat Model](#14-security-hardening--threat-model)
15. [Architectural Comparison Table: Defense-in-Depth Layers](#15-architectural-comparison-table-defense-in-depth-layers)
16. [Real-World Scenario Walkthrough](#16-real-world-scenario-walkthrough)
17. [Self-Assessment & Interview Questions](#17-self-assessment--interview-questions)
18. [Hands-on Practice Exercises](#18-hands-on-practice-exercises)
19. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

Containers do **not** run on a hypervisor; they are standard Linux processes executing directly on the shared host Linux kernel.

Because containers share the host kernel:
- **Default root in a container is UID 0 on the host kernel**. If an attacker escapes a container running as root, they possess full root control over the host server, its storage, network, and neighboring workloads.
- **Excessive Linux capabilities** (e.g. `CAP_SYS_ADMIN`, `CAP_NET_RAW`, `CAP_SYS_PTRACE`) grant processes the ability to load kernel modules, manipulate routing tables, and inject code into host processes.
- **Unrestricted System Calls**: Over 300+ Linux system calls (`syscalls`) exist. A compromised process can exploit unpatched kernel vulnerabilities via obscure, unnecessary syscalls (e.g. `keyctl`, `bpf`).

Hardening container security requires implementing **Defense-in-Depth**: dropping root privileges, filtering kernel syscalls via Seccomp, restricting filesystem writes, and stripping Linux capabilities.

---

## 2. Core Concepts

### The 5 Pillars of Container Hardening
1. **Unprivileged User (`USER 10001`)**: Ensuring process execution runs under an unprivileged UID/GID.
2. **User Namespace Remapping (`userns-remap`)**: Translating container UID 0 to an unprivileged host UID (e.g. 100000).
3. **Capability Dropping (`--cap-drop=ALL`)**: Removing unnecessary kernel privileges.
4. **Seccomp (Secure Computing Mode)**: Blocking dangerous Linux system calls.
5. **Immutable Filesystem (`--read-only`)**: Preventing attackers from downloading malware or modifying binaries.

---

## 3. The Container Isolation Myth: Root in Container == Root on Host

```
+-------------------------------------------------------------------------+
|                  THE KERNEL IDENTITY REALITY                            |
+-------------------------------------------------------------------------+
| Host Kernel checks Process Credentials:                                 |
|                                                                         |
| Process in Container A: UID = 0 (root)                                  |
| Process on Host OS:     UID = 0 (root)                                  |
|                                                                         |
| BOTH PROCESSES ARE THE IDENTICAL SECURITY PRINCIPAL TO THE LINUX KERNEL!|
| If Container A breaks out (via /sys, /proc, or volume mount),           |
| IT HAS FULL ROOT ACCESS OVER THE PHYSICAL HOST HARDWARE!                |
+-------------------------------------------------------------------------+
```

---

## 4. The Non-Root Mandate

Never permit an application to execute as `root` (UID 0) in production.

### Alpine Linux Non-Root Setup
```dockerfile
FROM alpine:3.21
# Create unprivileged group and user with explicit high UID
RUN addgroup -g 10001 -S appgroup && \
    adduser -u 10001 -S appuser -G appgroup
WORKDIR /app
COPY --chown=appuser:appgroup . .
USER 10001:10001
CMD ["./server"]
```

### Debian / Ubuntu Non-Root Setup
```dockerfile
FROM debian:bookworm-slim
RUN groupadd -g 10001 appgroup && \
    useradd -u 10001 -g appgroup -s /sbin/nologin -d /app appuser
WORKDIR /app
COPY --chown=appuser:appgroup . .
USER 10001:10001
CMD ["./server"]
```

---

## 5. User Namespaces Remapping (`userns-remap`)

User namespaces provide the ultimate architectural defense: **mapping container root (UID 0) to an unprivileged high UID on the host (e.g., UID 100000)**.
- Inside the container, the process believes it is `root` (UID 0).
- To the host kernel, the process is UID `100000` with zero host privileges. If a container breakout occurs, the attacker lands on the host as an unprivileged nobody!

### Enabling in `/etc/docker/daemon.json`
```json
{
  "userns-remap": "default"
}
```
Reload daemon:
```bash
sudo systemctl restart docker
```
Docker automatically creates an unprivileged user `dockremap` and manages subuid/subgid mappings in `/etc/subuid` and `/etc/subgid`.

---

## 6. Linux Capabilities: Principle of Least Privilege

Linux divides the traditional monolithic power of root into **approx. 40 distinct capabilities** (`CAP_*`).
By default, Docker drops most dangerous capabilities, but retains ~14 capabilities (such as `CAP_CHOWN`, `CAP_FOWNER`, `CAP_SETUID`, `CAP_NET_RAW`).

### The Gold Standard: Drop ALL and Add Selectively
```bash
docker run --rm -d \
  --cap-drop=ALL \
  --cap-add=NET_BIND_SERVICE \
  -p 80:80 \
  my-web-app
```

### Essential Capabilities Reference
- `CAP_NET_BIND_SERVICE`: Allows binding to privileged ports (< 1024).
- `CAP_CHOWN`: Allows modifying file ownership.
- `CAP_SETUID` / `CAP_SETGID`: Allows process to change its own UID/GID (used by `sudo`).
- `CAP_NET_RAW`: Allows crafting raw ICMP/IP packets (used by `ping`). Attacker vector for ARP/IP spoofing!
- `CAP_SYS_ADMIN`: **The "god-mode" capability**. Equivalent to full root; never grant this!

---

## 7. Syscall Filtering with Seccomp

Seccomp (Secure Computing Mode) uses Berkeley Packet Filters (BPF) to intercept and restrict which Linux system calls a process can invoke.
- Linux has ~350+ system calls.
- Docker's default seccomp profile disables ~44 dangerous system calls (including `reboot`, `sys_ptrace`, `keyctl`, `acct`, `bpf`, `clock_adjtime`).

### CLI Invocation

```bash
# 1. Use default secure profile (Default out of the box)
docker run --security-opt seccomp=default ...

# 2. Apply custom hardened seccomp JSON profile
docker run --security-opt seccomp=/path/to/custom-profile.json ...

# 3. DANGEROUS: Disable seccomp completely (Never in production!)
docker run --security-opt seccomp=unconfined ...
```

---

## 8. Mandatory Access Control: AppArmor and SELinux

### AppArmor (Ubuntu, Debian, SUSE)
Docker automatically applies the `docker-default` AppArmor profile to all containers. It restricts:
- Writing directly to `/proc/sys` or `/sys`.
- Mounting host filesystems inside the container.
- Accessing raw disk device nodes (`/dev/sda`).

Verify active profile:
```bash
docker inspect <id> --format '{{.AppArmorProfile}}'
# Displays: docker-default
```

### SELinux (RHEL, Fedora, Rocky, CentOS)
SELinux uses Type Enforcement to restrict containers. When bind-mounting host directories into containers on SELinux hosts, append labels:
- `-v /host/path:/container/path:z`: Configures shared access across multiple containers.
- `-v /host/path:/container/path:Z`: Configures private unshared access strictly to this container.

---

## 9. The `no-new-privileges` Protection

By default, an unprivileged user inside a container might execute a `setuid` binary (e.g., an unstripped utility) to elevate permissions.
Setting `--security-opt=no-new-privileges:true` enables the kernel `PR_SET_NO_NEW_PRIVS` flag:
- Prevents processes from acquiring new privileges through `execve`.
- Disables `setuid` and `setgid` bit execution permanently for the container process tree.

```bash
docker run --security-opt=no-new-privileges:true my-app
```

---

## 10. Read-Only Root Filesystems (`--read-only`)

An immutable container filesystem is the most effective deterrent against web malware, crypto-miners, and persistence backdoors. If an attacker exploits an application vulnerability (e.g. Log4j, PHP RCE), they cannot download binaries, write scripts to `/tmp`, or modify source files.

```bash
docker run -d \
  --read-only \
  --mount type=tmpfs,target=/tmp,tmpfs-size=32M \
  --mount type=tmpfs,target=/var/run,tmpfs-size=16M \
  -p 8080:80 \
  my-app
```

---

## 11. Production Best Practices & Antipatterns

### Best Practices
- **Always Declare Explicit High UID**: Use `USER 10001:10001` rather than named `USER nobody` or relying on defaults.
- **Drop All Capabilities**: Set `--cap-drop=ALL` and selectively grant only what is strictly required (`NET_BIND_SERVICE`).
- **Enforce `--security-opt=no-new-privileges:true`**: Prevent setuid privilege escalation.
- **Deploy with `--read-only`**: Protect root filesystem integrity.
- **Enable `userns-remap` on Shared Production Hosts**: Protect host root against container breakout.

### Anti-Patterns
- **Running Containers with `--privileged`**: Completely strips all namespaces, cgroups, seccomp, AppArmor, and capability limits. Equivalent to giving root on host.
- **Mounting the Docker Socket (`/var/run/docker.sock`)**: Grants root takeover over the host machine.
- **Running `seccomp=unconfined`**: Allows processes to invoke dangerous syscalls.

---

## 12. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| Application fails with `EACCES: permission denied` on boot | Container runs as non-root (UID 10001), but application files owned by root | `ls -la /app` inside container | Add `--chown=10001:10001` to `COPY` instructions in Dockerfile. |
| Container fails with `Read-only file system` | `--read-only` enabled, but app needs to write temporary PID or cache files | `docker logs <id>` | Mount in-memory `tmpfs` at `/tmp` and `/var/run`. |
| `ping` fails inside hardened container | `CAP_NET_RAW` capability was dropped via `--cap-drop=ALL` | Check ping exit status | Expected behavior; ping requires raw sockets. Do not grant unless required. |
| Host cannot bind port 80 with unprivileged user | Linux prohibits non-root users from binding ports < 1024 | Check container error log | Add `--cap-add=NET_BIND_SERVICE` or use ports > 1024 (e.g. 8080). |
| SELinux blocks host directory access with `Permission Denied` | Missing SELinux context label on bind mount | Check `audit2allow -a` on host | Append `:z` or `:Z` to volume mount: `-v ./data:/data:Z`. |
| `userns-remap` breaks existing local container storage | Enabling user namespaces shifts filesystem ownership to high UIDs | Check `/var/lib/docker/<uid>` | Migrate volume ownership or chown data to matching subordinate UID. |

---

## 13. Performance Tuning & Resource Optimization

1. **Zero Runtime Overhead for Security Controls**:
   Neither `USER`, `--read-only`, `--cap-drop`, nor `no-new-privileges` introduce any runtime CPU or memory overhead. They are kernel security flags checked during process instantiation and syscall traps.
2. **Seccomp BPF JIT**:
   The Linux kernel compiles Seccomp filter rules using an internal BPF Just-In-Time (JIT) compiler, executing syscall checks in sub-microsecond time.

---

## 14. Security Hardening & Threat Model

```
+--------------------------------------------------------------+
| ATTACK: Container Breakout via Dirty CoW / Kernel Vulnerability|
+--------------------------------------------------------------+
| An attacker exploits an unpatched Linux kernel bug.          |
+--------------------------------------------------------------+
| IF UNHARDENED (Default Docker run):                          |
| - Process runs as UID 0 (root).                              |
| - Capabilities include CAP_SYS_PTRACE, CAP_NET_RAW.          |
| - Syscall filtering unconfined.                              |
| - OUTCOME: TOTAL ROOT HOST COMPROMISE!                       |
+--------------------------------------------------------------+
| IF HARDENED:                                                 |
| - USER 10001 (Unprivileged UID).                             |
| - userns-remap: Container root is UID 100000 on host.        |
| - cap_drop: [ALL].                                           |
| - Seccomp blocks exploit syscalls.                           |
| - Read-only filesystem blocks writing exploit payload.       |
| - OUTCOME: EXPLOIT COMPLETELY THWARTED!                      |
+--------------------------------------------------------------+
```

---

## 15. Architectural Comparison Table: Defense-in-Depth Layers

| Layer | Technology | Primary Defense Vector | Implementation Effort |
|:---|:---|:---|:---|
| **Identity** | `USER 10001:10001` | Drops root execution privileges | Low |
| **Namespace Isolation**| `userns-remap` | Remaps UID 0 to unprivileged host UID | Moderate |
| **Privilege Boundaries**| `cap_drop: [ALL]` | Removes 40+ kernel superpowers | Low |
| **Syscall Filtering** | Seccomp BPF | Restricts kernel attack surface | Low (Default active)|
| **Filesystem Integrity**| `--read-only` | Prevents disk persistence & malware | Low |
| **Privilege Escalation**| `no-new-privileges`| Blocks `setuid` binary exploitation | Low |

---

## 16. Real-World Scenario Walkthrough

### Scenario: Mitigating an RCE Vulnerability in a Production Node.js Service

#### Incident
A zero-day Remote Code Execution (RCE) vulnerability was discovered in a popular open-source image processing library used by an enterprise web API. The vulnerability allowed remote attackers to execute arbitrary shell commands by uploading a crafted SVG file.

#### What Happened in Hardened Containers
The production service was deployed with strict hardening:
```bash
docker run -d \
  --read-only \
  --user 10001:10001 \
  --cap-drop=ALL \
  --security-opt=no-new-privileges:true \
  --mount type=tmpfs,target=/tmp,tmpfs-size=16M \
  my-api:prod
```

#### Exploit Failure Analysis
1. Attacker triggered RCE: The shell tried to download a miner via `curl` -> **Failed: No curl in image**.
2. Attacker tried to write a reverse shell script to `/var/app` -> **Failed: Read-only filesystem**.
3. Attacker wrote script to `/tmp` and attempted to execute with `setuid` escalation -> **Failed: `no-new-privileges` blocked execution**.
4. Attacker attempted raw network ping -> **Failed: `CAP_NET_RAW` dropped**.
5. Result: **Zero data leaked, zero host compromise.** The vulnerability was completely neutralized by runtime defense-in-depth controls.

---

## 17. Self-Assessment & Interview Questions

<details>
<summary><strong>1. Why is running a container process as `root` (UID 0) dangerous even though it runs inside a container?</strong></summary>

**Answer:**
Containers share the host Linux kernel. The kernel does not distinguish between a containerized root process and a host root process unless user namespace remapping is explicitly enabled. If an attacker discovers a kernel vulnerability, container escape bug, or access to sensitive mounted volumes (like the Docker socket or `/proc`), they immediately possess full root privileges over the physical host machine.
</details>

<details>
<summary><strong>2. How does `userns-remap` protect the host system?</strong></summary>

**Answer:**
`userns-remap` maps the container's root user (UID 0) to an unprivileged high UID on the host (e.g., UID 100000). Inside the container, the process has root semantics for container tasks. However, if the process breaks out of the container onto the host filesystem, the host kernel treats it strictly as UID 100000, possessing zero administrative privileges on the host.
</details>

<details>
<summary><strong>3. What does `--security-opt=no-new-privileges:true` do?</strong></summary>

**Answer:**
It sets the Linux kernel `PR_SET_NO_NEW_PRIVS` flag on the container process and all its children. This prevents processes from gaining additional privileges via `setuid` or `setgid` binaries (like `sudo` or `passwd`), preventing privilege escalation attacks even if vulnerable setuid binaries exist in the container image.
</details>

<details>
<summary><strong>4. How can you deploy an immutable container with `--read-only` if the application requires writing temporary files?</strong></summary>

**Answer:**
Deploy the container with `--read-only` and mount dedicated, size-bounded in-memory `tmpfs` volumes at the specific writable paths required by the application (typically `/tmp` and `/var/run`):
`docker run --read-only --mount type=tmpfs,target=/tmp,tmpfs-size=32m ...`
</details>

<details>
<summary><strong>5. What is Seccomp, and what does Docker's default profile accomplish?</strong></summary>

**Answer:**
Seccomp (Secure Computing Mode) is a Linux kernel feature that filters system calls using Berkeley Packet Filters. Docker's default seccomp profile whitelists ~300 safe system calls while blocking ~44 dangerous or obscure system calls (including `reboot`, `sys_ptrace`, `bpf`, `keyctl`), dramatically reducing the kernel attack surface without breaking standard user applications.
</details>

<details>
<summary><strong>6. Why is `--privileged` strictly forbidden in production enterprise environments?</strong></summary>

**Answer:**
`--privileged` completely disables all container security boundaries: it grants all Linux capabilities (including `CAP_SYS_ADMIN`), disables Seccomp filtering, disables AppArmor/SELinux restrictions, and exposes all host device nodes in `/dev`. A privileged container can immediately execute `chroot` or mount host drives to achieve total host takeover.
</details>

<details>
<summary><strong>7. What capability is required for an unprivileged container user (UID 10001) to bind to port 80?</strong></summary>

**Answer:**
`CAP_NET_BIND_SERVICE`. By default, Linux reserves ports below 1024 for root. Granting `--cap-add=NET_BIND_SERVICE` allows an unprivileged process to bind to port 80 or 443 without running as root.
</details>

<details>
<summary><strong>8. How does Docker handle SELinux context labeling on volume bind mounts?</strong></summary>

**Answer:**
SELinux enforces mandatory access control labels. When mounting a host directory on an SELinux-enforced host, appending `:z` instructs Docker to relabel the directory for shared access across multiple containers, while appending `:Z` relabels the directory for private, unshared access by that specific container only.
</details>

---

## 18. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Dropping Capabilities and Verifying Ping Failure</strong></summary>

**Goal:** Drop `CAP_NET_RAW` and verify that `ping` fails.

**Step-by-step Solution:**
1. Test normal ping:
   ```bash
   docker run --rm alpine ping -c 1 8.8.8.8
   # Succeeds
   ```
2. Drop all capabilities:
   ```bash
   docker run --rm --cap-drop=ALL alpine ping -c 1 8.8.8.8
   # Fails: ping: permission denied (Operation not permitted)
   ```
</details>

<details>
<summary><strong>Exercise 2: Hardening with `--read-only` and Tmpfs</strong></summary>

**Goal:** Boot an immutable container and verify file modification blocks.

**Step-by-step Solution:**
1. Run container:
   ```bash
   docker run --rm -it --read-only --mount type=tmpfs,target=/tmp alpine sh
   ```
2. Attempt write to root: `touch /test.txt` -> `Read-only file system`.
3. Attempt write to tmp: `touch /tmp/test.txt` -> Succeeds!
</details>

<details>
<summary><strong>Exercise 3: Verifying Non-Root UID Execution</strong></summary>

**Goal:** Run an image with explicit user and inspect process UID.

**Step-by-step Solution:**
1. Run:
   ```bash
   docker run --rm --user 10001:10001 alpine id
   # uid=10001 gid=10001
   ```
</details>

<details>
<summary><strong>Exercise 4: Verifying `no-new-privileges`</strong></summary>

**Goal:** Ensure privilege escalation flags are set on container processes.

**Step-by-step Solution:**
1. Run:
   ```bash
   docker run -d --name priv-test --security-opt=no-new-privileges:true alpine sleep 3600
   ```
2. Inspect on host:
   ```bash
   docker inspect priv-test --format '{{.HostConfig.SecurityOpt}}'
   # [no-new-privileges:true]
   docker rm -f priv-test
   ```
</details>

<details>
<summary><strong>Exercise 5: Inspecting Active AppArmor Profile</strong></summary>

**Goal:** Validate default AppArmor profile attachment.

**Step-by-step Solution:**
1. Run:
   ```bash
   docker run --rm alpine cat /proc/self/attr/current
   # docker-default (enforce)
   ```
</details>

<details>
<summary><strong>Exercise 6: Testing High-Port Binding with Unprivileged User</strong></summary>

**Goal:** Bind port 8080 as non-root user without capabilities.

**Step-by-step Solution:**
1. Run:
   ```bash
   docker run --rm -d --user 10001:10001 --cap-drop=ALL -p 8080:8080 python:3.12-alpine python -m http.server 8080
   ```
2. Test curl: `curl http://localhost:8080` -> Succeeds cleanly!
</details>

---

## 19. Summary & Reference Cheat Sheet

### Golden Hardening One-Liner
```bash
docker run -d \
  --read-only \
  --user 10001:10001 \
  --cap-drop=ALL \
  --cap-add=NET_BIND_SERVICE \
  --security-opt=no-new-privileges:true \
  --mount type=tmpfs,target=/tmp,tmpfs-size=32M \
  -p 80:80 \
  my-app:prod
```

### Top 5 Security Rules
1. **Never run as root**: Always use `USER 10001:10001`.
2. **Never use `--privileged`**: It strips all security boundaries.
3. **Drop all capabilities**: `--cap-drop=ALL`.
4. **Make rootfs read-only**: `--read-only` + `tmpfs` at `/tmp`.
5. **Prevent privilege escalation**: `--security-opt=no-new-privileges:true`.

---

[← Previous: 03-docker-in-ci.md](03-docker-in-ci.md) | [Next: 05-scanning-and-sbom.md →](05-scanning-and-sbom.md)
