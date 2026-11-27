# Building Containers from Scratch: Linux Primitives in Practice

| Metadata | Details |
| :--- | :--- |
| **Topic** | Building Containers from Scratch with Linux Primitives |
| **Module** | `06-advanced` — Advanced Systems & Kernel Mechanics |
| **Track** | `04-devops/linux` |
| **Target OS** | Ubuntu 24.04 LTS (Kernel 6.8+), RHEL 9.4+ (Kernel 5.14+) |
| **Prerequisites** | `01-namespaces-and-cgroups.md`, `04-permissions.md`, basic C or Bash shell scripting |
| **Difficulty** | Advanced |
| **Last Updated** | 2026 |

---

## Table of Contents
1. [What & Why: The Container Illusion](#what--why-the-container-illusion)
2. [Core Concepts & Architecture](#core-concepts--architecture)
3. [Internal Mechanics & Kernel Execution Flow](#internal-mechanics--kernel-execution-flow)
4. [Command Syntax & Primitive Tooling Reference](#command-syntax--primitive-tooling-reference)
5. [Progressive Practical Examples](#progressive-practical-examples)
   - [Example 1: Minimal Alpine Rootfs Extraction](#example-1-minimal-alpine-rootfs-extraction)
   - [Example 2: Filesystem Isolation with pivot_root and chroot](#example-2-filesystem-isolation-with-pivot_root-and-chroot)
   - [Example 3: Namespace Bundling with unshare](#example-3-namespace-bundling-with-unshare)
   - [Example 4: Cgroups v2 Resource Throttling](#example-4-cgroups-v2-resource-throttling)
   - [Example 5: Full Container Runtime Script (Pure Bash)](#example-5-full-container-runtime-script-pure-bash)
6. [Production Best Practices & Hardening](#production-best-practices--hardening)
7. [Common Failure Modes & Debugging](#common-failure-modes--debugging)
8. [Performance, Security & Scale Considerations](#performance-security--scale-considerations)
9. [Comparison Matrix: Runtimes & Container Engines](#comparison-matrix-runtimes--container-engines)
10. [Real-World Interview Scenarios](#real-world-interview-scenarios)
11. [Hands-On Practice Labs](#hands-on-practice-labs)
12. [Cheat Sheet & Quick Reference](#cheat-sheet--quick-reference)

---

## What & Why: The Container Illusion

There is technically no single entity called a "container" inside the Linux kernel. Unlike virtualization hypervisors (KVM, Xen) that emulate physical hardware devices and run distinct operating system kernels, a container is simply a standard Linux user-space process subject to three primary kernel subsystems:
1. **Namespaces**: Limit what a process can **see** (UTS, PID, NET, MNT, IPC, USER, CGROUP).
2. **Control Groups (cgroups v2)**: Limit what a process can **use** (CPU, RAM, IO, PIDs).
3. **Security Constraints (Capabilities, Seccomp, LSMs)**: Limit what a process can **do** (kernel system calls, privilege escalation, file access).

Understanding how these low-level primitives combine allows DevOps engineers, SREs, and container platform architects to debug container runtime failures (OCI runc, containerd, Kubernetes CRI), diagnose "cannot fork" or OOM errors, and construct locked-down ephemeral sandboxes without heavy daemon dependencies.

---

## Core Concepts & Architecture

### The Building Blocks of a Container

```
+-------------------------------------------------------------------------+
|                           Host Linux Kernel                             |
|                                                                         |
|  +-------------------------------------------------------------------+  |
|  |                 Control Groups v2 (/sys/fs/cgroup)                |  |
|  |        cpu.max=50000 100000        memory.max=256M                |  |
|  +-------------------------------------------------------------------+  |
|                                  ^                                      |
|                                  | Controls usage                       |
|  +-------------------------------------------------------------------+  |
|  |                       Container Process                           |  |
|  |    PID 1 (inside container) <---> PID 49201 (on host root PID ns) |  |
|  +-------------------------------------------------------------------+  |
|          |                 |                  |                 |       |
|          v                 v                  v                 v       |
|    [ Mount NS ]       [ PID NS ]          [ NET NS ]        [ UTS NS ]  |
|    pivot_root         Private PID 1       veth0 <-> br0     Private     |
|    Isolated rootfs    No host visibility  Isolated IP/routes hostname   |
+-------------------------------------------------------------------------+
```

1. **Root Filesystem (rootfs)**: A clean directory hierarchy containing system binaries, libraries, and configuration files (e.g., standard Alpine, Debian, or Ubuntu rootfs).
2. **`chroot` vs `pivot_root`**: `chroot` only changes the current process root path variable, leaving old root paths vulnerable to escaping via open file descriptors (`fchdir`). `pivot_root` atomically swaps the entire root mount of the mount namespace, moving the old root to a staging directory to be unmounted (`umount -l`) and completely detached.
3. **Pseudo-filesystems (`/proc`, `/sys`, `/dev`)**: A containerized PID namespace requires its own mounted instance of `procfs` (`mount -t proc proc /proc`) so commands like `ps` and `top` inspect only the container's isolated PID namespace.

---

## Internal Mechanics & Kernel Execution Flow

When an OCI runtime (such as `runc`) or an automated script executes a containerized process, the kernel proceeds through five sequential phases:

```
  1. Clone / Unshare
     +--------------------------------------------------------+
     | clone(CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | ...) |
     +--------------------------------------------------------+
                                |
                                v
  2. Setup Cgroups v2
     +--------------------------------------------------------+
     | mkdir /sys/fs/cgroup/mycontainer                       |
     | echo $PID > /sys/fs/cgroup/mycontainer/cgroup.procs    |
     | echo "250M" > /sys/fs/cgroup/mycontainer/memory.max    |
     +--------------------------------------------------------+
                                |
                                v
  3. Mount & pivot_root
     +--------------------------------------------------------+
     | mount --bind $ROOTFS $ROOTFS                           |
     | cd $ROOTFS && mkdir .old_root                          |
     | pivot_root . .old_root && umount -l /.old_root         |
     | mount -t proc proc /proc && mount -t sysfs sys /sys    |
     +--------------------------------------------------------+
                                |
                                v
  4. Network Configuration
     +--------------------------------------------------------+
     | ip link add veth-host type veth peer name veth-guest   |
     | ip link set veth-guest netns $CONTAINER_PID            |
     | ip netns exec ... config IP, routing, default gateway  |
     +--------------------------------------------------------+
                                |
                                v
  5. Executive Drop & Launch
     +--------------------------------------------------------+
     | capsh --drop=cap_sys_admin -- ...                      |
     | exec /bin/sh                                           |
     +--------------------------------------------------------+
```

---

## Command Syntax & Primitive Tooling Reference

| Utility / Command | Primary Flags | Purpose |
| :--- | :--- | :--- |
| `unshare` | `-m` (mount), `-u` (uts), `-i` (ipc), `-n` (net), `-p` (pid), `-U` (user), `-f` (fork) | Disassociate specified namespaces from the parent process before launching a program. |
| `nsenter` | `-t <pid>`, `-m`, `-u`, `-i`, `-n`, `-p` | Enter existing namespaces of a target running process. |
| `pivot_root` | `pivot_root <new_root> <put_old>` | Swaps current root filesystem with `new_root` and mounts previous root at `put_old`. |
| `capsh` | `--drop=<caps>`, `--secbits=<bits>`, `--chroot=<dir>` | Inspect, set, or drop Linux POSIX capabilities. |
| `ip netns` | `add`, `del`, `exec <name> <cmd>`, `set <dev> netns <pid>` | Manipulates network namespaces tied to `/var/run/netns`. |

---

## Progressive Practical Examples

### Example 1: Minimal Alpine Rootfs Extraction

Prepare a minimal root filesystem directory without needing Docker installed.

```bash
#!/usr/bin/env bash
# Step 1: Download and unpack Alpine mini rootfs
set -euo pipefail

WORKDIR="/tmp/container-lab"
ROOTFS="${WORKDIR}/rootfs"

mkdir -p "${ROOTFS}"
cd "${WORKDIR}"

# Fetch official verified Alpine minirootfs 3.20
curl -fsSL -o alpine.tar.gz "https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.0-x86_64.tar.gz"
tar -xzf alpine.tar.gz -C "${ROOTFS}"

# Verify extracted filesystem layout
ls -l "${ROOTFS}"
# Output shows bin, dev, etc, home, lib, proc, sbin, sys, usr, var
```

### Example 2: Filesystem Isolation with pivot_root and chroot

Demonstrate why `pivot_root` is required over simple `chroot` to prevent directory traversal escapes.

```bash
#!/usr/bin/env bash
# Step 2: Safe pivot_root demonstration
set -euo pipefail

ROOTFS="/tmp/container-lab/rootfs"

# Create mount namespace
unshare --mount --uts /bin/bash -c "
  # pivot_root requires the new root to be a mount point
  mount --bind ${ROOTFS} ${ROOTFS}
  cd ${ROOTFS}
  
  # Create temporary old root mount directory
  mkdir -p .old_root
  
  # Pivot current root into .old_root and switch to new rootfs
  pivot_root . .old_root
  
  # Detach and unmount old host root
  umount -l /.old_root
  rmdir /.old_root
  
  # Change hostname inside UTS namespace
  hostname isolated-guest
  echo 'Active hostname inside container:' \$(hostname)
"
```

### Example 3: Namespace Bundling with unshare

Launch an interactive container shell isolating UTS, PID, Mount, and IPC namespaces in one atomic command.

```bash
#!/usr/bin/env bash
# Step 3: Atomic unshare invocation
ROOTFS="/tmp/container-lab/rootfs"

sudo unshare --mount --uts --ipc --pid --fork /bin/bash -c "
  # Make mounts private so host is unaffected
  mount --make-rprivate /

  # Bind mount target rootfs
  mount --bind ${ROOTFS} ${ROOTFS}
  cd ${ROOTFS}
  mkdir -p .old_root
  pivot_root . .old_root
  umount -l /.old_root
  rmdir /.old_root

  # Mount private procfs for clean PID isolation
  mkdir -p /proc /sys /dev
  mount -t proc proc /proc
  mount -t sysfs sysfs /sys
  mount -t devtmpfs devtmpfs /dev

  # Verify process view
  echo '=== Inside Container PID Namespace ==='
  ps aux
  exec /bin/sh
"
```

### Example 4: Cgroups v2 Resource Throttling

Constrain CPU and memory usage using the unified cgroups v2 filesystem before handing control to the container process.

```bash
#!/usr/bin/env bash
# Step 4: Apply strict memory and CPU quotas
set -euo pipefail

CGROUP_NAME="sandbox-demo"
CGROUP_PATH="/sys/fs/cgroup/${CGROUP_NAME}"

# Create cgroup v2 controller directory
sudo mkdir -p "${CGROUP_PATH}"

# Limit memory to 128 MB (hard ceiling)
echo "134217728" | sudo tee "${CGROUP_PATH}/memory.max" > /dev/null

# Limit CPU to 20% of 1 core (20,000 us per 100,000 us period)
echo "20000 100000" | sudo tee "${CGROUP_PATH}/cpu.max" > /dev/null

# Max process count limit
echo "20" | sudo tee "${CGROUP_PATH}/pids.max" > /dev/null

echo "Cgroup limits configured at ${CGROUP_PATH}:"
grep . "${CGROUP_PATH}/memory.max" "${CGROUP_PATH}/cpu.max" "${CGROUP_PATH}/pids.max"
```

### Example 5: Full Container Runtime Script (Pure Bash)

A complete, self-contained container runtime implementation written entirely in Bash that prepares network interfaces, cgroups, rootfs, and launches an isolated shell.

```bash
#!/usr/bin/env bash
# Step 5: Pure Bash mini-container runtime (Production Reference)
set -euo pipefail

CONTAINER_NAME="tinybox-01"
ROOTFS="/tmp/container-lab/rootfs"
CGROUP="/sys/fs/cgroup/${CONTAINER_NAME}"
VETH_HOST="veth_${CONTAINER_NAME}"
VETH_GUEST="veth_guest"
BRIDGE="br-containers"
HOST_IP="10.200.1.1/24"
CONTAINER_IP="10.200.1.2/24"

# 1. Prepare Bridge on Host
if ! ip link show "${BRIDGE}" >/dev/null 2>&1; then
    sudo ip link add "${BRIDGE}" type bridge
    sudo ip addr add "${HOST_IP}" dev "${BRIDGE}"
    sudo ip link set "${BRIDGE}" up
fi

# 2. Setup Cgroups v2
sudo mkdir -p "${CGROUP}"
echo "256M" | sudo tee "${CGROUP}/memory.max" > /dev/null
echo "50000 100000" | sudo tee "${CGROUP}/cpu.max" > /dev/null
echo "50" | sudo tee "${CGROUP}/pids.max" > /dev/null

echo "Starting container: ${CONTAINER_NAME}..."

# 3. Launch isolated container process
sudo unshare --mount --uts --ipc --pid --net --fork bash -c "
    # Enroll current PID into Cgroup v2
    echo \$\$ > '${CGROUP}/cgroup.procs'

    # Set container hostname
    hostname '${CONTAINER_NAME}'

    # Setup Private Rootfs & Pivot
    mount --make-rprivate /
    mount --bind '${ROOTFS}' '${ROOTFS}'
    cd '${ROOTFS}'
    mkdir -p .old_root
    pivot_root . .old_root
    umount -l /.old_root
    rmdir /.old_root

    # Mount virtual filesystems
    mkdir -p /proc /sys /dev
    mount -t proc proc /proc
    mount -t sysfs sys /sys
    mount -t devtmpfs devtmpfs /dev

    # Drop into container shell
    echo 'Container initialized successfully! PID=1:'
    ps aux
    exec /bin/sh
" &

CONTAINER_PID=$!
echo "Container running with Host PID: ${CONTAINER_PID}"

# 4. Attach veth pair between host bridge and container netns
sudo ip link add "${VETH_HOST}" type veth peer name "${VETH_GUEST}"
sudo ip link set "${VETH_HOST}" master "${BRIDGE}"
sudo ip link set "${VETH_HOST}" up
sudo ip link set "${VETH_GUEST}" netns "${CONTAINER_PID}"

# Configure networking inside the container's namespace
sudo nsenter -t "${CONTAINER_PID}" -n ip addr add "${CONTAINER_IP}" dev "${VETH_GUEST}"
sudo nsenter -t "${CONTAINER_PID}" -n ip link set dev "${VETH_GUEST}" name eth0
sudo nsenter -t "${CONTAINER_PID}" -n ip link set dev eth0 up
sudo nsenter -t "${CONTAINER_PID}" -n ip link set dev lo up
sudo nsenter -t "${CONTAINER_PID}" -n ip route add default via 10.200.1.1

echo "Networking configured. Waiting for container exit..."
wait "${CONTAINER_PID}" || true

# Cleanup
echo "Cleaning up cgroup and veth..."
sudo rmdir "${CGROUP}" || true
sudo ip link del "${VETH_HOST}" || true
echo "Container teardown complete."
```

---

## Production Best Practices & Hardening

- **Never rely on `chroot` alone**: A process holding an open file descriptor outside the chroot jail can call `fchdir()` to escape. Always isolate the mount namespace and use `pivot_root`.
- **Enforce private mount propagation**: Always execute `mount --make-rprivate /` before mounting container filesystems to prevent child mounts from leaking back into the host system mount table.
- **Drop `CAP_SYS_ADMIN` and `CAP_NET_ADMIN`**: Containers should run with unprivileged user mappings (`CLONE_NEWUSER`) or strictly dropped Linux capabilities using `capsh --drop=cap_sys_admin`.
- **Use Read-Only Root Filesystems**: Mount the primary root filesystem as read-only (`mount -o remount,ro /`), mounting ephemeral `tmpfs` volumes only on designated writable directories (`/tmp`, `/run`).
- **Enforce Cgroups v2 `pids.max`**: Protect the host kernel against fork-bombs by setting a hard limit on process count (e.g., 50–200) in `/sys/fs/cgroup/<group>/pids.max`.

---

## Common Failure Modes & Debugging

| Symptom / Error | Root Cause | Diagnosis Command | Resolution |
| :--- | :--- | :--- | :--- |
| `pivot_root: Invalid argument` | New root is not an independent mount point, or old root is on the same filesystem. | `findmnt -T .` | Execute `mount --bind "${ROOTFS}" "${ROOTFS}"` prior to executing `pivot_root`. |
| `ps: /proc: No such file or directory` | Mount namespace created without mounting a private instance of `procfs`. | `ls -la /proc` | Run `mount -t proc proc /proc` inside the container namespace. |
| Host processes visible in `ps aux` inside container | Process was not forked after `CLONE_NEWPID` or `/proc` was not remounted. | `echo $$` and `cat /proc/self/status` | Pass `--fork` to `unshare` and remount private `/proc`. |
| Container network unreachable from host | Veth interface not attached to host bridge or missing default route. | `ip link show master br-containers` | Verify veth pair mapping and execute `ip route add default` in netns. |
| `write error: Resource temporarily unavailable` (Fork bomb) | Process limit reached in `pids.max`. | `cat /sys/fs/cgroup/<name>/pids.current` | Increase `pids.max` or inspect runaway looping scripts. |

---

## Performance, Security & Scale Considerations

```
+--------------------------------------------------------------------------+
| Container Runtime Overhead Comparison                                   |
|                                                                          |
| Hypervisor VM (KVM):                                                     |
| [ App ] -> [ Guest OS Kernel ] -> [ QEMU/KVM ] -> [ Host OS Kernel ]     |
| Memory overhead: 200MB - 1GB per VM. Boot time: 5-25 seconds.           |
|                                                                          |
| Linux Container (runc / unshare):                                        |
| [ App ] -> [ Host OS Kernel (isolated via Namespaces + Cgroups) ]         |
| Memory overhead: < 1MB per container. Boot time: 10-50 milliseconds.     |
+--------------------------------------------------------------------------+
```

1. **Kernel Shared Surface**: Because containers share the host Linux kernel, any unpatched kernel vulnerability (`CVE-2024-...` in dirty pipe, use-after-free in netfilter) can permit complete host compromise. Mitigate by pairing containers with Seccomp BPF filters.
2. **Mount Namespace Leaks**: Every container launch adds entries to the kernel mount table. Failure to clean up mounts upon container death degrades host VFS performance over time.

---

## Comparison Matrix: Runtimes & Container Engines

| Feature / Engine | Native `unshare` / Script | OCI `runc` | `containerd` | Docker Engine | Podman |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Architecture** | Direct CLI syscalls | Low-level OCI CLI | High-level Daemon | Monolithic Daemon | Daemonless (Fork/Exec) |
| **Image Unpacking** | Manual (`tar`) | Via caller (bundle) | Integrated OCI/Docker | Integrated | Integrated |
| **Rootless Support** | Manual User NS | Yes | Yes | Yes (Rootless mode) | Native Default |
| **Cgroups Support** | Direct sysfs write | v1 & v2 OCI spec | v1 & v2 OCI spec | v1 & v2 | v1 & v2 |
| **Primary Use Case** | Education / Sandbox | Kubernetes (via CRI) | Kubernetes CRI | Developer Workstations| Rootless Production |

---

## Real-World Interview Scenarios

### Scenario 1: How does the Linux kernel differentiate between host PID and container PID?
<details>
<summary>Detailed Answer</summary>
The kernel manages processes using hierarchical PID namespaces. Every process has a separate PID value for each namespace level it belongs to. In the root PID namespace (host), a container process might have PID `49201`. However, within its own child PID namespace, it is assigned PID `1`. The kernel `task_struct` structure maintains an array of `upid` (unique PID) structs mapping each namespace pointer to its localized PID integer. System calls originating within that child namespace query localized PID structs.
</details>

### Scenario 2: Why is `CLONE_NEWUSER` significant for rootless containers?
<details>
<summary>Detailed Answer</summary>
`CLONE_NEWUSER` decouples user and group IDs inside the namespace from those on the host. An unprivileged user (UID 1000) on the host can map their UID to UID 0 (root) inside the container's user namespace. Inside the container, the process possesses full POSIX capabilities (e.g., `CAP_NET_BIND_SERVICE`, `CAP_SYS_CHROOT`) strictly bounded by that namespace, while retaining zero privileges on the host kernel outside.
</details>

### Scenario 3: Explain the difference between `chroot` and `pivot_root`.
<details>
<summary>Detailed Answer</summary>
`chroot` modifies the `fs->root` pointer of the calling process to point to a new directory. It does not alter mount table entries. A malicious process with `CAP_SYS_CHROOT` can break out by creating a nested folder, chrooting into it, and calling `fchdir()` on an open file descriptor pointing outside the jail. `pivot_root` changes the actual root mount point (`vfs_mount`) of the entire mount namespace. The old root is moved to a directory, unmounted with `umount -l`, and removed completely, leaving no reference for directory traversal.
</details>

### Scenario 4: What happens if PID 1 in a container exits?
<details>
<summary>Detailed Answer</summary>
In Linux, PID 1 inside a PID namespace is designated as the init process for that namespace. If PID 1 terminates, the kernel immediately sends `SIGKILL` to all other processes residing inside that namespace, destroys the PID namespace, and terminates the container. Furthermore, PID 1 must properly reap orphaned zombie processes (`waitpid()`); otherwise, uncollected child processes remain as zombies in the container process table.
</details>

### Scenario 5: How do containers communicate over the network without a physical interface?
<details>
<summary>Detailed Answer</summary>
Containers utilize virtual Ethernet (`veth`) device pairs. A `veth` pair behaves like a bidirectional virtual cable: packets transmitted into one peer instantly emerge from the other. The container runtime leaves one end (`veth-host`) in the host network namespace attached to a Linux bridge (`br0`), while pushing the other end (`veth-guest`) into the container's network namespace and renaming it to `eth0`.
</details>

### Scenario 6: How does Cgroups v2 differ from Cgroups v1 in resource accounting?
<details>
<summary>Detailed Answer</summary>
Cgroups v1 implemented separate, uncoordinated hierarchies for each resource controller (`/sys/fs/cgroup/cpu`, `/sys/fs/cgroup/memory`), causing race conditions and inability to account for buffered IO (where page cache writes were charged to IO, but not memory). Cgroups v2 implements a single unified tree (`/sys/fs/cgroup`), enforces single-ownership per process, and natively links memory writeback caching to block IO controllers.
</details>

### Scenario 7: What kernel security mechanism prevents a container root user from loading kernel modules?
<details>
<summary>Detailed Answer</summary>
Linux POSIX capabilities and Seccomp filters. By default, container runtimes drop `CAP_SYS_MODULE` (which permits loading and unloading kernel modules via `init_module`/`finit_module`) and enforce Seccomp BPF rules blocking the `create_module`, `init_module`, and `delete_module` system calls.
</details>

### Scenario 8: How do you enter and debug a running container lacking standard tools like `curl` or `sh`?
<details>
<summary>Detailed Answer</summary>
Using `nsenter` from the host. By running `nsenter -t <container_host_pid> -m -u -i -n -p /host/bin/bash`, you can execute host debugging binaries (such as `gdb`, `tcpdump`, `strace`) directly inside the target container's namespaces without modifying the container's image or filesystem.
</details>

---

## Hands-On Practice Labs

### Lab 1: Inspect Process Namespaces via `/proc`
<details>
<summary>Lab Instructions & Verification</summary>

1. Inspect the namespace symlinks for your current shell:
   ```bash
   ls -l /proc/$$/ns/
   ```
2. Spawn an isolated UTS namespace:
   ```bash
   sudo unshare --uts /bin/bash
   hostname temporary-sandbox
   hostname
   ```
3. Open a second terminal and verify that the host hostname remains unchanged:
   ```bash
   hostname
   ```
</details>

### Lab 2: Construct an Isolated Network Namespace
<details>
<summary>Lab Instructions & Verification</summary>

1. Create a network namespace named `net-test`:
   ```bash
   sudo ip netns add net-test
   sudo ip netns list
   ```
2. Verify it starts with only a loopback interface in the `DOWN` state:
   ```bash
   sudo ip netns exec net-test ip link show
   ```
3. Bring loopback up:
   ```bash
   sudo ip netns exec net-test ip link set lo up
   sudo ip netns exec net-test ping -c 2 127.0.0.1
   ```
4. Teardown:
   ```bash
   sudo ip netns del net-test
   ```
</details>

### Lab 3: Enforce Memory Ceilings with Cgroups v2
<details>
<summary>Lab Instructions & Verification</summary>

1. Create a cgroup named `mem-cap`:
   ```bash
   sudo mkdir -p /sys/fs/cgroup/mem-cap
   echo "50M" | sudo tee /sys/fs/cgroup/mem-cap/memory.max
   ```
2. Run a command inside that cgroup that attempts to allocate 100MB RAM using Python or Perl:
   ```bash
   sudo sh -c 'echo $$ > /sys/fs/cgroup/mem-cap/cgroup.procs && python3 -c "a = bytearray(100 * 1024 * 1024)"'
   ```
3. Inspect kernel logs to observe the OOM killer event:
   ```bash
   dmesg -T | grep -i oom
   ```
4. Clean up:
   ```bash
   sudo rmdir /sys/fs/cgroup/mem-cap
   ```
</details>

### Lab 4: Rootfs Extraction and chroot Isolation
<details>
<summary>Lab Instructions & Verification</summary>

1. Create a temporary folder `/tmp/jail` and populate it with a static binary or busybox:
   ```bash
   mkdir -p /tmp/jail/bin
   cp /bin/busybox /tmp/jail/bin/sh
   ```
2. Enter the jail:
   ```bash
   sudo chroot /tmp/jail /bin/sh
   ```
3. Run `ls` or `pwd` to observe the isolated view. Type `exit` to return to the host.
</details>

### Lab 5: Namespace Injection with `nsenter`
<details>
<summary>Lab Instructions & Verification</summary>

1. Run a background `unshare` command holding a UTS namespace:
   ```bash
   sudo unshare --uts sleep 300 &
   SLEEP_PID=$!
   echo "Target PID: $SLEEP_PID"
   ```
2. Inject into its UTS namespace using `nsenter`:
   ```bash
   sudo nsenter -t "${SLEEP_PID}" -u hostname test-injection
   ```
3. Verify the background process's UTS namespace hostname changed:
   ```bash
   sudo nsenter -t "${SLEEP_PID}" -u hostname
   ```
4. Kill the process:
   ```bash
   sudo kill -9 "${SLEEP_PID}"
   ```
</details>

### Lab 6: Trace Container System Calls with `strace`
<details>
<summary>Lab Instructions & Verification</summary>

1. Run `unshare` while tracing calls to `clone`, `unshare`, and `mount`:
   ```bash
   strace -e clone,unshare,mount unshare --mount --uts hostname demo-box
   ```
2. Observe how `unshare(CLONE_NEWNS|CLONE_NEWUTS)` is invoked before `execve`.
</details>

---

## Cheat Sheet & Quick Reference

```bash
# === Namespace Inspection & Manipulation ===
ls -la /proc/<pid>/ns/                         # View namespace inodes
unshare -m -u -i -n -p -f --mount-proc bash    # Standard container shell
nsenter -t <pid> -m -u -i -n -p /bin/bash      # Enter container namespaces

# === Cgroups v2 Quick Reference ===
/sys/fs/cgroup/<group>/cgroup.procs            # Assign PIDs (echo $PID > ...)
/sys/fs/cgroup/<group>/memory.max              # Hard memory limit (e.g., 256M)
/sys/fs/cgroup/<group>/cpu.max                 # Quota & period (e.g., 50000 100000)
/sys/fs/cgroup/<group>/pids.max                # Process limit (e.g., 100)

# === Network Namespaces ===
ip netns list                                  # List network namespaces
ip netns add <name>                            # Create network namespace
ip netns exec <name> <cmd>                     # Run command inside namespace
ip link set <dev> netns <pid>                  # Move network device to namespace
```

---
*Next Note: [03-advanced-debugging.md](file:///04-devops/linux/06-advanced/03-advanced-debugging.md)*
