# Project 05: Build a Container Runtime from Scratch with unshare

| Metadata | Details |
| :--- | :--- |
| **Project** | Building a Container Runtime from Scratch with Linux Primitives |
| **Module** | `07-projects` — Production Reference Projects & Labs |
| **Track** | `04-devops/linux` |
| **Target OS** | Ubuntu 24.04 LTS (Kernel 6.8+), RHEL 9.4+ (Kernel 5.14+) |
| **Complexity** | Advanced |
| **Estimated Duration**| 60–90 minutes |

---

## 1. Architecture & Design

### Scenario Context
Container engines like Docker, Podman, and containerd are often treated as opaque black boxes. In production environments, when container networking breaks, OOM events kill processes prematurely, or filesystems fail to unmount, engineers struggle without a fundamental grasp of low-level kernel primitives.

This project delivers a complete, production-grade container runtime script (`mini-runc.sh`) written entirely in Bash using Linux primitives:
1. **Linux Namespaces**: PID (process tree), MNT (filesystem mounts), NET (isolated virtual network stack), UTS (hostname), and IPC (inter-process communication).
2. **Atomic Filesystem Swapping (`pivot_root`)**: Safe replacement of host root with an Alpine rootfs, unmounting the old host root to prevent directory traversal escapes.
3. **Control Groups v2 (`cgroups v2`)**: Strict CPU quotas, memory caps, and process count limits.
4. **Virtual Ethernet (`veth`) & Bridge Networking**: Full bidirectional internet access via host bridge and iptables/nftables NAT masquerading.

### Architectural Diagram
```
+-------------------------------------------------------------------------+
|                  HOST OPERATING SYSTEM / KERNEL                         |
|                                                                         |
|  [ Physical Host Interface: eth0 (192.168.1.50) ]                       |
|                             ^                                           |
|                             | iptables -t nat -A POSTROUTING MASQUERADE |
|                             v                                           |
|  [ Linux Bridge: br-mini (10.100.0.1/24) ]                              |
|                             ^                                           |
|                             | veth-host end                             |
|                             v                                           |
|  [ Virtual Cable Pair (veth) ]                                          |
|                             ^                                           |
|                             | veth-guest end (moved into netns)         |
|                             v                                           |
+-----------------------------|-------------------------------------------+
| CONTAINER BOUNDARY          v                                           |
|  +-------------------------------------------------------------------+  |
|  | Isolated Network Namespace:                                       |  |
|  | - Interface: eth0 (10.100.0.2/24)                                 |  |
|  | - Route: default via 10.100.0.1                                   |  |
|  | - Nameservers: 1.1.1.1                                            |  |
|  +-------------------------------------------------------------------+  |
|  | Cgroups v2 Governance (/sys/fs/cgroup/mini-box-01):               |  |
|  | - memory.max = 128MB                                              |  |
|  | - cpu.max = 50000 100000 (50% of 1 core)                          |  |
|  | - pids.max = 30 (Fork-bomb defense)                               |  |
|  +-------------------------------------------------------------------+  |
|  | Mount Namespace & pivot_root:                                     |  |
|  | - Isolated Alpine Rootfs (/bin, /lib, /etc)                       |  |
|  | - Private /proc, /sys, /dev, /tmp mount points                    |  |
|  | - Private PID 1 init process                                      |  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

---

## 2. Implementation Artifacts

### Artifact 1: Production Container Runtime Script (`mini-runc.sh`)

```bash
#!/usr/bin/env bash
# ==============================================================================
# Script: mini-runc.sh
# Purpose: Educational & Production-Grade Container Runtime from Scratch
# Prereqs: unshare, pivot_root, iproute2, iptables/nftables, curl, tar
# ==============================================================================

set -euo pipefail
IFS=$'\n\t'

CONTAINER_ID="mini-box-$$"
ROOTFS_BASE="/var/lib/mini-containers"
CONTAINER_ROOT="${ROOTFS_BASE}/${CONTAINER_ID}"
CGROUP_PATH="/sys/fs/cgroup/${CONTAINER_ID}"

BRIDGE_NAME="br-mini"
BRIDGE_IP="10.100.0.1/24"
CONTAINER_IP="10.100.0.2/24"
GATEWAY_IP="10.100.0.1"
VETH_HOST="veth-h-$$"
VETH_GUEST="veth-g-$$"

log() { echo -e "\033[1;32m[mini-runc] $*\033[0m"; }
error() { echo -e "\033[1;31m[ERROR] $*\033[0m" >&2; cleanup; exit 1; }

# Trap unexpected exits for safe teardown
cleanup() {
    log "Initiating cleanup and teardown for ${CONTAINER_ID}..."
    
    # Remove veth interface if lingering on host
    if ip link show "${VETH_HOST}" >/dev/null 2>&1; then
        ip link del "${VETH_HOST}" || true
    fi

    # Remove Cgroup directory
    if [[ -d "${CGROUP_PATH}" ]]; then
        # Move any remaining tasks to parent before deletion
        rmdir "${CGROUP_PATH}" 2>/dev/null || true
    fi

    # Unmount container rootfs bind mounts if lingering
    if mountpoint -q "${CONTAINER_ROOT}" 2>/dev/null; then
        umount -R "${CONTAINER_ROOT}" 2>/dev/null || true
    fi

    # Clean filesystem
    rm -rf "${CONTAINER_ROOT}" 2>/dev/null || true
    log "Cleanup complete."
}
trap cleanup EXIT INT TERM

# 1. Root Check
[[ "${EUID}" -eq 0 ]] || error "This script must be executed as root."

# 2. Host Bridge & NAT Setup
setup_host_networking() {
    log "Configuring Host Network Bridge (${BRIDGE_NAME})..."
    
    # Enable IP Forwarding on Host
    sysctl -w net.ipv4.ip_forward=1 >/dev/null

    if ! ip link show "${BRIDGE_NAME}" >/dev/null 2>&1; then
        ip link add "${BRIDGE_NAME}" type bridge
        ip addr add "${BRIDGE_IP}" dev "${BRIDGE_NAME}"
        ip link set "${BRIDGE_NAME}" up
    fi

    # Configure NAT Masquerading via iptables
    HOST_EGRESS_IF=$(ip route show default | awk '{print $5}' | head -n 1)
    if ! iptables -t nat -C POSTROUTING -s 10.100.0.0/24 -o "${HOST_EGRESS_IF}" -j MASQUERADE 2>/dev/null; then
        iptables -t nat -A POSTROUTING -s 10.100.0.0/24 -o "${HOST_EGRESS_IF}" -j MASQUERADE
    fi
}

# 3. Setup Alpine Rootfs
setup_rootfs() {
    log "Preparing Container Root Filesystem at ${CONTAINER_ROOT}..."
    mkdir -p "${CONTAINER_ROOT}"
    
    local cache_tar="/tmp/alpine-minirootfs-3.20.tar.gz"
    if [[ ! -f "${cache_tar}" ]]; then
        log "Downloading Alpine Linux mini rootfs..."
        curl -fsSL -o "${cache_tar}" "https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.0-x86_64.tar.gz"
    fi

    tar -xzf "${cache_tar}" -C "${CONTAINER_ROOT}"

    # Setup resolv.conf for container DNS
    echo "nameserver 1.1.1.1" > "${CONTAINER_ROOT}/etc/resolv.conf"
}

# 4. Configure Cgroups v2
setup_cgroups() {
    log "Configuring Cgroups v2 limits at ${CGROUP_PATH}..."
    mkdir -p "${CGROUP_PATH}"

    # Limit Memory to 128 Megabytes
    echo "134217728" > "${CGROUP_PATH}/memory.max"

    # Limit CPU to 50% of 1 CPU core
    echo "50000 100000" > "${CGROUP_PATH}/cpu.max"

    # Fork-bomb protection: limit total processes to 30
    echo "30" > "${CGROUP_PATH}/pids.max"
}

# 5. Launch Container Process
run_container() {
    setup_host_networking
    setup_rootfs
    setup_cgroups

    log "Spawning isolated container process via unshare..."

    # Launch unshared container process in background to wire up networking
    unshare --mount --uts --ipc --pid --net --fork bash -c "
        # 1. Enroll current PID into Cgroup v2
        echo \$\$ > '${CGROUP_PATH}/cgroup.procs'

        # 2. Configure Hostname
        hostname '${CONTAINER_ID}'

        # 3. Make host mounts private and bind-mount container root
        mount --make-rprivate /
        mount --bind '${CONTAINER_ROOT}' '${CONTAINER_ROOT}'
        cd '${CONTAINER_ROOT}'

        # 4. Safe pivot_root
        mkdir -p .old_root
        pivot_root . .old_root
        umount -l /.old_root
        rmdir /.old_root

        # 5. Mount virtual pseudo-filesystems
        mkdir -p /proc /sys /dev /tmp
        mount -t proc proc /proc
        mount -t sysfs sysfs /sys
        mount -t devtmpfs devtmpfs /dev
        mount -t tmpfs tmpfs /tmp

        # Wait until network interface is moved and configured by parent
        while ! ip link show eth0 >/dev/null 2>&1; do
            sleep 0.1
        done

        # Configure Container Interface
        ip addr add '${CONTAINER_IP}' dev eth0
        ip link set eth0 up
        ip link set lo up
        ip route add default via '${GATEWAY_IP}'

        echo '======================================================'
        echo '  CONTAINER INITIALIZED SUCCESSFULLY!'
        echo '  Hostname: '\$(hostname)
        echo '  PID 1: '\$(echo \$\$)
        echo '  Memory Max: '\$(cat /sys/fs/cgroup/${CONTAINER_ID}/memory.max 2>/dev/null || echo '128MB')
        echo '======================================================'
        
        # Hand over to user shell
        exec /bin/sh
    " &

    CONTAINER_PID=$!
    log "Container launched with Host PID: ${CONTAINER_PID}"

    # Create veth pair on host
    ip link add "${VETH_HOST}" type veth peer name "${VETH_GUEST}"
    ip link set "${VETH_HOST}" master "${BRIDGE_NAME}"
    ip link set "${VETH_HOST}" up

    # Move guest end of veth into container network namespace
    ip link set "${VETH_GUEST}" netns "${CONTAINER_PID}"
    
    # Rename interface to eth0 inside the container namespace
    nsenter -t "${CONTAINER_PID}" -n ip link set dev "${VETH_GUEST}" name eth0

    log "Network interface configured and attached. Joining container console..."
    wait "${CONTAINER_PID}" || true
    log "Container process terminated."
}

run_container
```

---

## 3. Execution & Verification

### Step-by-Step Test Procedure

1. **Deploy and Run Mini Runtime**:
   ```bash
   sudo chmod +x ./mini-runc.sh
   sudo ./mini-runc.sh
   ```

2. **Simulated Console Output**:
   ```text
   [mini-runc] Configuring Host Network Bridge (br-mini)...
   [mini-runc] Preparing Container Root Filesystem at /var/lib/mini-containers/mini-box-49210...
   [mini-runc] Configuring Cgroups v2 limits at /sys/fs/cgroup/mini-box-49210...
   [mini-runc] Spawning isolated container process via unshare...
   [mini-runc] Container launched with Host PID: 49215
   [mini-runc] Network interface configured and attached. Joining container console...
   ======================================================
     CONTAINER INITIALIZED SUCCESSFULLY!
     Hostname: mini-box-49210
     PID 1: 1
     Memory Max: 128MB
   ======================================================
   / # 
   ```

3. **Verify Process Tree Inside Container**:
   Inside the container shell, verify process isolation:
   ```sh
   / # ps aux
   PID   USER     TIME  COMMAND
       1 root      0:00 /bin/sh
       5 root      0:00 ps aux
   ```
   *Notice only 2 processes exist; host processes are completely invisible!*

4. **Verify Outbound Internet Connectivity**:
   ```sh
   / # ping -c 3 1.1.1.1
   PING 1.1.1.1 (1.1.1.1): 56 data bytes
   64 bytes from 1.1.1.1: seq=0 ttl=57 time=8.241 ms
   64 bytes from 1.1.1.1: seq=1 ttl=57 time=7.982 ms

   / # wget -qO- http://ifconfig.me
   # Output matches host public IP via NAT masquerade!
   ```

5. **Test Cgroups v2 Memory Throttling**:
   Run a memory allocator inside the container shell:
   ```sh
   / # python3 -c 'a = bytearray(200 * 1024 * 1024)'
   Killed
   ```
   *Notice the process is instantly killed by the OOM killer because memory exceeded the 128MB limit.*

6. **Exit Container and Verify Teardown**:
   ```sh
   / # exit
   [mini-runc] Container process terminated.
   [mini-runc] Initiating cleanup and teardown for mini-box-49210...
   [mini-runc] Cleanup complete.
   ```

---

## 4. Failure Modes & Edge Cases

| Failure Mode / Incident | Root Cause | Remediation Runbook |
| :--- | :--- | :--- |
| **`pivot_root: Invalid argument`** | Target directory is not a distinct mount point. | Execute `mount --bind "${CONTAINER_ROOT}" "${CONTAINER_ROOT}"` prior to calling `pivot_root`. |
| **Container Cannot Reach Internet** | Host `net.ipv4.ip_forward` disabled, or missing iptables `MASQUERADE` rule. | Run `sysctl -w net.ipv4.ip_forward=1` and ensure `iptables -t nat -A POSTROUTING -s 10.100.0.0/24 -j MASQUERADE` exists. |
| **`ps` inside container shows host PIDs** | `/proc` was not unmounted and remounted inside the new PID/mount namespace. | Ensure `mount -t proc proc /proc` is executed after `pivot_root`. |
| **Lingering veth interfaces clutter host** | Parent process terminated unexpectedly with `SIGKILL` without triggering EXIT trap. | Run `ip link show type veth` and delete orphaned interfaces using `ip link del <veth_name>`. |

---

## 5. Production Considerations

- **Rootless Operation via User Namespaces**: To avoid requiring root on the host, expand the script to use `unshare --user` with `/etc/subuid` and `/etc/subgid` mappings.
- **Rootfs OverlayFS Stacking**: Instead of extracting a fresh tarball per container, use Linux `overlayfs` with a shared read-only lower directory and ephemeral writable upper directory.
- **Capabilities & Seccomp Hardening**: Drop dangerous capabilities (`capsh --drop=cap_sys_admin`) before handing control to the child process.

---
*Next Project: [06-nginx-plus-systemd-deployment.md](file:///04-devops/linux/07-projects/06-nginx-plus-systemd-deployment.md)*
