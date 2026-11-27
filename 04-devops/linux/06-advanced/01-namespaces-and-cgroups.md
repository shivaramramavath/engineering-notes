# Linux Namespaces and Control Groups (Cgroups v2)

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Kernel Namespaces, Cgroups v2 Unified Hierarchy, Resource Throttling, and Container Foundations |
| **Module** | `06-advanced` |
| **Track** | `04-devops/linux` |
| **Prerequisites** | `01-fundamentals/02-linux-architecture.md`, `03-system/01-processes-and-signals.md`, `03-system/04-systemd-and-services.md` |
| **Target Audience** | Systems Architects, Container Engineers, Kernel Engineers, Principal SREs |
| **Target Distribution** | Ubuntu 24.04 LTS (cgroups v2 default), Rocky Linux / RHEL 9.4 (cgroups v2 default) |
| **Key Tools / Syscalls** | `unshare`, `nsenter`, `ip netns`, `/sys/fs/cgroup/`, `systemd-cgls`, `systemd-cgtop` |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [Internal Working](#internal-working)
4. [Syntax & CLI Tools](#syntax--cli-tools)
5. [Progressive Practical Examples](#progressive-practical-examples)
6. [Best Practices](#best-practices)
7. [Common Mistakes & Gotchas](#common-mistakes--gotchas)
8. [Performance, Security & Cost Implications](#performance-security--cost-implications)
9. [Comparison Table / Alternatives](#comparison-table--alternatives)
10. [Real-World Troubleshooting & Interview Q&A](#real-world-troubleshooting--interview-qa)
11. [Hands-On Practice Exercises](#hands-on-practice-exercises)
12. [Summary Cheat Sheet & Further Reading](#summary-cheat-sheet--further-reading)

---

## What & Why

"Containers are not real things." There is no single "container" object in the Linux kernel source code. What developers call a Docker container or Kubernetes pod is simply an ordinary Linux process wrapped in two core kernel isolation and resource-management primitives:
1. **Linux Namespaces**: Provide **Isolation** (what a process can **see**).
2. **Control Groups (Cgroups v2)**: Provide **Resource Metering and Throttling** (what a process can **use**).

Without namespaces, every process shares the same global view of PID numbers, network interfaces, IP addresses, mount points, and hostnames. Without cgroups, a single runaway thread with an infinite loop or memory leak can consume 100% of host CPU and trigger system-wide Out-Of-Memory (OOM) crashes.

Modern Linux distributions (Ubuntu 22.04+, RHEL 9+) fully standardize on **Cgroups v2 (Unified Hierarchy)**, abandoning the fragmented, multi-rooted architecture of Cgroups v1. Mastering namespaces and Cgroups v2 unlocks complete visibility into how Docker, containerd, and Kubernetes enforce multi-tenant container isolation at the kernel level.

---

## Core Concepts

### The 8 Linux Kernel Namespaces
A namespace wraps a global system resource in an abstraction that makes it appear to processes within the namespace that they have their own isolated instance of that resource:

| Namespace | Kernel Flag | Introduced | What It Isolates |
| :--- | :--- | :--- | :--- |
| **Mount (`mnt`)** | `CLONE_NEWNS` | Linux 2.4.19 (2002) | Filesystem mount points and directory hierarchy (`mount`, `umount`, `pivot_root`) |
| **UTS** | `CLONE_NEWUTS` | Linux 2.6.19 (2006) | Hostname and NIS domain name (`sethostname`, `setdomainname`) |
| **IPC** | `CLONE_NEWIPC` | Linux 2.6.19 (2006) | System V IPC objects and POSIX message queues |
| **PID** | `CLONE_NEWPID` | Linux 2.6.24 (2008) | Process ID numbers; the containerized process sees itself as **PID 1** |
| **Network (`net`)**| `CLONE_NEWNET` | Linux 2.6.29 (2009) | Network devices, IP addresses, routing tables, port bindings, firewall rules |
| **User (`user`)** | `CLONE_NEWUSER`| Linux 3.8 (2013) | User and Group IDs (maps unprivileged host UID 1000 to root UID 0 inside namespace) |
| **Cgroup** | `CLONE_NEWCGROUP`| Linux 4.6 (2016) | Virtualized view of `/sys/fs/cgroup/` hierarchy |
| **Time** | `CLONE_NEWTIME`| Linux 5.6 (2020) | Offsets for monotonic (`CLOCK_MONOTONIC`) and boot (`CLOCK_BOOTTIME`) clocks |

### Control Groups: Cgroups v1 vs Cgroups v2
- **Cgroups v1 (Legacy)**: Featured independent, orthogonal hierarchies for every resource controller (`/sys/fs/cgroup/cpu`, `/sys/fs/cgroup/memory`, `/sys/fs/cgroup/blkio`). Different controllers could not coordinate, leading to deadlocks, severe memory-accounting bugs, and inability to attribute disk I/O to memory page writebacks.
- **Cgroups v2 (Unified Hierarchy)**: Standardized into a **single unified tree** rooted at `/sys/fs/cgroup/`. A process belongs to exactly one cgroup node. Resource controllers (`cpu`, `memory`, `io`, `pids`) are enabled top-down via `cgroup.subtree_control`.

### Key Cgroups v2 Controllers
- **`memory`**: Hard memory limit (`memory.max`), soft throttle limit (`memory.high`), minimum guaranteed memory (`memory.min`), and swap limits (`memory.swap.max`).
- **`cpu`**: CFS bandwidth throttling via `cpu.max` (quota and period in microseconds, e.g. `200000 100000` = 2 cores) and proportional shares via `cpu.weight`.
- **`io`**: Linear I/O rate limits per block device (`io.max`: read/write IOPS and bytes per second) and proportional weights (`io.weight`).
- **`pids`**: Maximum number of tasks and threads allowed in the cgroup (`pids.max`), preventing Fork Bomb denial-of-service attacks.

---

## Internal Working

```
+-----------------------------------------------------------------------------------------+
|                                    Process: struct task_struct                          |
+----------------------------------------------------+------------------------------------+
                                                     |
                      +------------------------------+------------------------------+
                      |                                                             |
                      v (Namespaces)                                                v (Cgroups v2)
+-------------------------------------------+                 +-------------------------------------------+
|              struct nsproxy               |                 |                struct css_set             |
|   - mnt_ns  -> Isolated / mount table     |                 |  Points to node in unified tree:          |
|   - uts_ns  -> Hostname: "container-app"  |                 |  /sys/fs/cgroup/system.slice/docker-.../  |
|   - ipc_ns  -> Isolated POSIX Msg Queues  |                 |                                           |
|   - pid_ns  -> Process sees itself as PID 1                 |  Controllers Applied:                     |
|   - net_ns  -> Isolated veth0, IP: 10.88.0.2                |  - memory.max = 536870912 (512MB)         |
|   - user_ns -> Host UID 1001 mapped to 0  |                 |  - cpu.max    = "100000 100000" (1 vCPU)  |
|   - cgroup_ns / time_ns                   |                 |  - pids.max   = 100 (Anti-fork bomb)      |
+-------------------------------------------+                 +-------------------------------------------+
                      |                                                             |
                      v                                                             v
        Kernel filters Visibility:                                    Kernel throttles Consumption:
        ps aux shows only internal PIDs                               CFS scheduler pauses threads if quota hit
        ip a shows only local veth0                                   OOM killer terminates container if memory.max hit
```

### The System Call Trio: `clone`, `unshare`, and `setns`
Namespaces are controlled via three fundamental Linux syscalls:
1. **`clone(2)`**: Creates a brand-new child process while passing `CLONE_NEW*` flags, placing the child in new namespaces at birth (how `docker run` starts containers).
2. **`unshare(2)`**: Detaches the **current calling process** from its current namespaces and moves it into newly allocated ones without forking.
3. **`setns(2)`**: Attaches the calling process to an **existing namespace** referenced by an open file descriptor in `/proc/<pid>/ns/` (how `docker exec` enters a running container).

---

## Syntax & CLI Tools

### Managing Namespaces with `unshare` and `nsenter`
```bash
# Launch a new bash shell with isolated UTS (hostname) and PID namespaces
sudo unshare --uts --pid --fork --mount-proc /bin/bash

# Launch an isolated Network namespace
sudo unshare --net /bin/bash

# Enter an existing container's namespaces using its PID
sudo nsenter --target <PID> --mount --uts --ipc --net --pid /bin/bash

# Inspect namespace inodes for a process
ls -la /proc/$$/ns/
```

### Managing Network Namespaces with `ip netns`
```bash
# List all persistent network namespaces
ip netns list

# Create a new network namespace named 'red'
sudo ip netns add red

# Execute a command inside namespace 'red'
sudo ip netns exec red ip link show

# Delete a network namespace
sudo ip netns del red
```

### Inspecting Cgroups v2 with Systemd Tools
```bash
# Display the complete Cgroups v2 hierarchical tree
systemd-cgls

# Real-time top-like monitor for Cgroups v2 resource consumption
systemd-cgtop

# Query cgroup path and slice of current shell
cat /proc/self/cgroup
```

---

## Progressive Practical Examples

### Example 1: Creating an Isolated Network Namespace with a Virtual Cable (`veth`)
How Docker and Kubernetes connect container network namespaces to the host.

```bash
#!/usr/bin/env bash
set -euo pipefail

# 1. Create network namespace
sudo ip netns add net-client

# 2. Create virtual ethernet pair (veth-host <---> veth-guest)
sudo ip link add veth-host type veth peer name veth-guest

# 3. Move one end of the cable into the namespace
sudo ip link set veth-guest netns net-client

# 4. Configure host end
sudo ip addr add 192.168.99.1/24 dev veth-host
sudo ip link set veth-host up

# 5. Configure namespace end inside net-client
sudo ip netns exec net-client ip addr add 192.168.99.2/24 dev veth-guest
sudo ip netns exec net-client ip link set veth-guest up
sudo ip netns exec net-client ip link set lo up

# 6. Test bidirectional connectivity across the namespace boundary!
ping -c 2 192.168.99.2
sudo ip netns exec net-client ping -c 2 192.168.99.1

# 7. Clean up
sudo ip netns del net-client
sudo ip link del veth-host 2>/dev/null || true
```

### Example 2: Creating a Process Isolation Bubble with Isolated PID & UTS
Demonstrating how a process sees itself as PID 1 while appearing as a regular PID on the host.

```bash
# Terminal 1: Launch unshared shell
sudo unshare --uts --pid --mount --fork /bin/bash

# Inside unshared shell:
# 1. Change hostname (UTS namespace)
hostname container-sandbox
hostname
# Output: container-sandbox

# 2. Mount private /proc filesystem (Mount + PID namespace)
mount -t proc proc /proc

# 3. Inspect process tree
ps aux
# Output:
# USER         PID %CPU %MEM    VSZ   RSS TTY      STAT START   TIME COMMAND
# root           1  0.0  0.0   9420  4812 pts/1    S    10:30   0:00 /bin/bash
# root           4  0.0  0.0  10240  3210 pts/1    R+   10:30   0:00 ps aux
# (Notice the shell is PID 1!)
```
```bash
# Terminal 2: On the host machine
hostname
# (Host hostname remains completely untouched!)
ps aux | grep /bin/bash
# (The unshared shell appears as a standard unprivileged PID, e.g. PID 18429, on the host!)
```

### Example 3: Enforcing Hard CPU and Memory Limits Manually via Cgroups v2
Creating a custom cgroup without Docker or Kubernetes, directly interacting with the kernel virtual filesystem.

```bash
#!/usr/bin/env bash
set -euo pipefail

CGROUP_PATH="/sys/fs/cgroup/sandbox-cgroup"

echo "==> 1. Creating cgroup node..."
sudo mkdir -p "${CGROUP_PATH}"

echo "==> 2. Setting memory limit to 100 MB..."
# Hard memory limit in bytes: 100 * 1024 * 1024 = 104857600
echo "104857600" | sudo tee "${CGROUP_PATH}/memory.max"

echo "==> 3. Setting CPU quota to 50% of one core (50,000us per 100,000us period)..."
echo "50000 100000" | sudo tee "${CGROUP_PATH}/cpu.max"

echo "==> 4. Setting max processes (fork-bomb defense)..."
echo "20" | sudo tee "${CGROUP_PATH}/pids.max"

echo "==> 5. Launching stress test process inside cgroup..."
# Attach current shell to the cgroup by writing PID to cgroup.procs
sudo sh -c "echo \$\$ > ${CGROUP_PATH}/cgroup.procs && exec python3 -c 'while True: pass'" &
STRESS_PID=$!
sleep 1

echo "==> 6. Measuring CPU consumption of sandboxed PID..."
ps -p "${STRESS_PID}" -o pid,%cpu,cmd
# (Notice %CPU is throttled exactly to ~50%!)

# Clean up
kill "${STRESS_PID}" 2>/dev/null || true
# Wait for process exit, then remove cgroup directory
sleep 1
sudo rmdir "${CGROUP_PATH}"
```

### Example 4: Testing OOM Killer Invocation in a Cgroup v2 Sandbox
Observe how the Linux kernel OOM killer terminates an offending process inside a memory-constrained cgroup without impacting host stability.

```bash
#!/usr/bin/env bash
set -euo pipefail

CGROUP_PATH="/sys/fs/cgroup/oom-test"
sudo mkdir -p "${CGROUP_PATH}"

# Restrict to 50MB RAM and disable swap allocation
echo "52428800" | sudo tee "${CGROUP_PATH}/memory.max"
echo "0" | sudo tee "${CGROUP_PATH}/memory.swap.max"

echo "==> Launching memory consumer allocating 120MB..."
sudo sh -c "echo \$\$ > ${CGROUP_PATH}/cgroup.procs && exec python3 -c 'a = bytearray(120 * 1024 * 1024)'" || true

# Inspect kernel logs to verify cgroup-scoped OOM kill
sudo dmesg -T | grep -i -E "oom-test|killed process" | tail -n 2

# Inspect cgroup OOM counter
cat "${CGROUP_PATH}/memory.events"
# Output shows: oom 1, oom_kill 1

sudo rmdir "${CGROUP_PATH}"
```

### Example 5: Non-Root Containers via User Namespaces (`CLONE_NEWUSER`)
Mapping an unprivileged host user (`UID 1000`) to `UID 0` (root) inside the namespace, eliminating container root-escalation attacks.

```bash
# Launch user namespace as an unprivileged user (NO SUDO!)
unshare --user --map-root-user /bin/bash

# Inside user namespace:
whoami
# Output: root
id
# Output: uid=0(root) gid=0(root)

# Attempt to write to a host-protected file:
touch /etc/hacked.txt
# Output: touch: cannot touch '/etc/hacked.txt': Permission denied!
# (The kernel knows you are still unprivileged UID 1000 on the real host!)
```

---

## Best Practices

1. **Verify Cgroups v2 Unified Mode on Modern Clusters**: Ensure your Linux kernel boots with Cgroups v2. Check `stat -fc %T /sys/fs/cgroup/`. If it returns `cgroup2fs`, the system is in unified v2 mode. If it returns `tmpfs`, legacy v1 is still active (enable v2 via kernel command line: `systemd.unified_cgroup_hierarchy=1`).
2. **Always Enforce `pids.max` in Production Cgroups**: An unconstrained container can trigger a Fork Bomb (`:(){ :|:& };:`), exhausting the host's PID table (`/proc/sys/kernel/pid_max`) and freezing the entire physical node. Kubernetes sets default Pod PID limits (`PodPidsLimit`); ensure custom cgroups set `pids.max`.
3. **Use User Namespaces for Rootless Containers**: Whenever possible, run container engines (Podman, Docker Rootless) with user namespaces enabled. If a containerized process breaks out of its mount namespace, it lands on the host as an unprivileged UID (> 100,000), preventing host takeover.
4. **Monitor `memory.high` to Trigger Graceful Throttling**: In Cgroups v2, `memory.max` triggers a hard OOM kill. Setting `memory.high` establishes a soft threshold: when crossed, the kernel throttles the container's page allocation rate and reclaims caches proactively, alerting monitoring systems before an OOM kill occurs.
5. **Always Mount Private Filesystems with Mount Propagation Flags**: When creating mount namespaces, specify `--propagation private` (or `mount --make-rprivate /`) to prevent private container mounts from leaking back into the host's global mount table.
6. **Clean Up Empty Cgroups via `rmdir`**: A cgroup directory cannot be deleted with `rm -rf`. To remove a cgroup, ensure all processes inside `cgroup.procs` are terminated or moved, then delete the directory using standard `rmdir`.

---

## Common Mistakes & Gotchas

| Mistake | Why It Fails | Correct Pattern |
| :--- | :--- | :--- |
| Attempting `rm -rf /sys/fs/cgroup/<group>` | The cgroup virtual filesystem rejects recursive directory deletion. | Move/kill processes in `cgroup.procs`, then run `rmdir`. |
| Unsharing PID namespace without `--fork` | The calling process cannot change its own PID in-flight; only child processes get new PIDs. | Always pass `--fork` when unsharing PID: `unshare -p -f --mount-proc`. |
| Forgetting to mount `/proc` after unsharing PID namespace | `ps aux` continues reading the host's `/proc`, displaying all host processes! | Run `mount -t proc proc /proc` (inside a mount namespace) or pass `--mount-proc`. |
| Setting `cpu.max` quota without understanding period | Setting `cpu.max = 50000` without period fails syntax validation. | Pass both quota and period: `echo "50000 100000" > cpu.max`. |
| Using legacy Cgroups v1 controllers on modern systemd | Causes conflicts with systemd v250+ which manages the unified hierarchy natively. | Standardize on Cgroups v2. |
| Forgetting loopback interface in new network namespaces | `127.0.0.1` is unconfigured (`DOWN`); local IPC, health checks, and unix-domain proxies fail. | Run `ip link set lo up` inside the network namespace. |

---

## Performance, Security & Cost Implications

### Performance
- **Zero Runtime Virtualization Overhead**: Unlike Hardware Virtual Machines (KVM, ESXi) which incur CPU instruction emulation and nested page table overhead, namespaces and cgroups introduce **0% instruction translation overhead**. A process inside a container executes natively on bare-metal CPU silicon.
- **CFS Throttling Latency Spikes**: If `cpu.max` is set too tightly, multi-threaded applications (e.g. Java GC threads) can exhaust their 100ms quota in 10ms, leaving threads paused for the remaining 90ms of the period, causing severe API tail latency spikes. Use CPU shares (`cpu.weight`) rather than strict hard quotas where appropriate.

### Security
- **Multi-Tenant Hardening**: Combining Mount, User, and Network namespaces creates defense-in-depth isolation. Even if a process suffers a remote buffer overflow, it is trapped inside an isolated network stack, cannot see other processes, cannot access host storage, and lacks root capabilities on the parent host.

### Cost
- By replacing full hardware VMs with lightweight namespace/cgroup containers, systems achieve **10x to 50x higher workload density** per server, dramatically cutting physical datacenter footprints and cloud VM instance counts.

---

## Comparison Table / Alternatives

| Isolation Primitive | Hardware Virtual Machine (KVM/VMware) | Linux Container (Namespaces + Cgroups) | Chroot Sandbox |
| :--- | :--- | :--- | :--- |
| **Isolation Level** | Complete Hardware & Kernel Isolation | **OS Kernel Abstraction (Shared Kernel)** | Filesystem Path Only |
| **Startup Time** | 15–60 Seconds | **10–100 Milliseconds** | Instantaneous |
| **Memory Overhead** | ~500MB–1GB per VM (Guest OS kernel) | **Zero (Shares Host Kernel)** | Zero |
| **CPU Performance** | Near-native (VT-x / AMD-V) | **100% Native Bare-Metal** | 100% Native Bare-Metal |
| **Attack Surface** | Hypervisor boundary (small) | Kernel system call table (large) | Easily bypassed (no isolation) |

---

## Real-World Troubleshooting & Interview Q&A

### Q1: Explain the difference between how Docker uses Namespaces vs how it uses Cgroups.
<details>
<summary>View Solution</summary>

- **Namespaces provide ISOLATION (Visibility)**:
  They dictate what a containerized process can **see**. A process in a PID namespace only sees processes in its own tree; in a Network namespace, it only sees its own `eth0` and routing table; in a Mount namespace, it only sees its own filesystem root.
- **Cgroups provide LIMITATION (Resource Allocation)**:
  They dictate what a containerized process can **consume**. Cgroups meter, prioritize, and limit physical resources: capping memory to 1GB (`memory.max`), capping CPU to 2 cores (`cpu.max`), and restricting IOPS (`io.max`).
</details>

### Q2: What happens if a process in a Cgroups v2 container exceeds its `memory.max` limit?
<details>
<summary>View Solution</summary>

1. When memory usage reaches `memory.max`, the Linux kernel first attempts to **reclaim memory** aggressively by dropping filesystem page caches associated with that cgroup.
2. If swap is enabled (`memory.swap.max > 0`), the kernel swaps anonymous pages out to swap storage.
3. If no memory can be reclaimed and swap is full:
   - The kernel invokes the **Cgroup-Level OOM Killer**.
   - It selects the process with the highest memory consumption within that specific cgroup and terminates it with `SIGKILL` (`kill -9`).
   - Crucially, **the host operating system and all other cgroups/containers continue running completely undisturbed**.
</details>

### Q3: Why does a process inside a PID namespace think it is PID 1, and what special responsibilities does that entail?
<details>
<summary>View Solution</summary>

When a child is spawned with `CLONE_NEWPID`, the kernel allocates a new PID namespace. The first process created inside this namespace is assigned PID 1 within its local mapping, while retaining its normal PID on the host.

**Special Responsibilities of PID 1**:
1. **Default Signal Handling**: The kernel will **not** terminate PID 1 with default signal handlers (`SIGTERM`, `SIGINT`) unless the process has explicitly registered an active signal handler function.
2. **Zombie Reaping**: When child processes terminate, they remain as "zombies" (`<defunct>`) until their parent calls `wait()` or `waitpid()`. If an application spawns background workers and exits, the orphaned children are reparented to PID 1. If PID 1 does not reap them, the PID table fills up and crashes the container.
</details>

### Q4: How do you verify whether your Linux host is running Cgroups v1 or Cgroups v2?
<details>
<summary>View Solution</summary>

Run:
```bash
stat -fc %T /sys/fs/cgroup/
```
- If output is **`cgroup2fs`**: The system is running in **pure Cgroups v2** mode (modern standard).
- If output is **`tmpfs`**: The system is running in legacy **Cgroups v1** mode.
</details>

### Q5: What is the purpose of the `nsenter` command?
<details>
<summary>View Solution</summary>

`nsenter` (Namespace Enter) allows an administrator to enter the namespaces of an existing, running process.
This is the fundamental mechanism behind `docker exec` and `kubectl exec`.
Example:
```bash
sudo nsenter -t <container_pid> -n ip a
```
Allows an engineer to execute diagnostic network commands directly inside a container's private network namespace without needing diagnostic tools installed inside the container image itself.
</details>

### Q6: How does Cgroups v2 enforce CPU throttling via `cpu.max`?
<details>
<summary>View Solution</summary>

`cpu.max` contains two integers: `QUOTA` and `PERIOD` (measured in microseconds, default period is 100,000 us = 100ms).
```text
echo "250000 100000" > cpu.max
```
- `PERIOD = 100000` (100ms).
- `QUOTA = 250000` (250ms of CPU runtime).
This allows the processes in the cgroup to consume up to 250ms of CPU execution time across all cores during every 100ms wall-clock window (equivalent to **2.5 CPU cores**). If the quota is exhausted before the period ends, the Completely Fair Scheduler (CFS) suspends execution of all threads in the cgroup until the next 100ms window begins.
</details>

### Q7: What is a User Namespace and why does it enable "Rootless Containers"?
<details>
<summary>View Solution</summary>

User namespaces (`CLONE_NEWUSER`) decouple a process's internal UID/GID from its external host UID/GID.
- An unprivileged user (e.g. UID 1000) on the host can map their UID to `UID 0` (root) inside the user namespace.
- Inside the container, the process has full root privileges: it can change file permissions, create user accounts, and install packages.
- However, if the process attempts any system-wide action outside its namespace (e.g. loading a kernel module, modifying host network interfaces, touching `/etc/shadow`), the kernel evaluates the real host UID (1000) and rejects the operation.
</details>

### Q8: What is the purpose of `cgroup.procs` in Cgroups v2?
<details>
<summary>View Solution</summary>

`cgroup.procs` is the interface file through which processes are assigned to a cgroup.
- To move a process into a cgroup, write its PID to the file:
  ```bash
  echo 1234 | sudo tee /sys/fs/cgroup/my_cgroup/cgroup.procs
  ```
- Reading `cgroup.procs` lists all PIDs currently belonging to that cgroup node.
- In Cgroups v2, moving a parent process automatically moves all of its child threads atomically.
</details>

---

## Hands-On Practice Exercises

### Exercise 1: Querying Active Cgroups Version and Inode Types
**Task**: Check which Cgroups version is mounted on your system and inspect the root `/sys/fs/cgroup` directory.
<details>
<summary>View Solution</summary>

```bash
# 1. Check filesystem type
stat -fc %T /sys/fs/cgroup/

# 2. View controllers enabled at root level
cat /sys/fs/cgroup/cgroup.subtree_control
# Expected: cpu memory io pids
```
</details>

### Exercise 2: Launching a Sandbox with `unshare`
**Task**: Launch an unshared bash shell with a private hostname (UTS namespace). Change the hostname inside, verify the host is unaffected, and exit.
<details>
<summary>View Solution</summary>

```bash
# 1. Unshare UTS namespace
sudo unshare --uts /bin/bash

# 2. Inside shell: change hostname
hostname private-bubble
hostname
# Output: private-bubble

# 3. Exit shell
exit

# 4. On host: verify original hostname
hostname
```
</details>

### Exercise 3: Creating and Inspecting Network Namespaces with `ip netns`
**Task**: Create a network namespace named `blue`. Verify its isolated loopback interface state, bring it up, and clean up.
<details>
<summary>View Solution</summary>

```bash
# 1. Create namespace
sudo ip netns add blue

# 2. Inspect links inside (lo is DOWN by default)
sudo ip netns exec blue ip -br link

# 3. Bring loopback UP
sudo ip netns exec blue ip link set lo up
sudo ip netns exec blue ping -c 2 127.0.0.1

# 4. Clean up
sudo ip netns del blue
```
</details>

### Exercise 4: Testing Cgroups v2 Process Limits (PIDs Max)
**Task**: Create a test cgroup with `pids.max = 3`. Attempt to launch 5 subshells inside to observe how the kernel blocks process creation.
<details>
<summary>View Solution</summary>

```bash
CGROUP="/sys/fs/cgroup/pid-limit-lab"
sudo mkdir -p "${CGROUP}"
echo "3" | sudo tee "${CGROUP}/pids.max"

# Launch subshells inside cgroup
sudo sh -c "echo \$\$ > ${CGROUP}/cgroup.procs && (sleep 10 & sleep 10 & sleep 10 & sleep 10 & wait)" 2>&1 | grep "Resource temporarily unavailable" || true

# Clean up
sudo rmdir "${CGROUP}"
```
</details>

### Exercise 5: Inspecting a Process Namespace File Descriptors
**Task**: Inspect your current shell's namespace symlinks in `/proc/self/ns/`.
<details>
<summary>View Solution</summary>

```bash
ls -l /proc/self/ns/
# Notice symlinks: cgroup, ipc, mnt, net, pid, pid_for_children, time, time_for_children, user, uts
```
</details>

### Exercise 6: Real-Time Cgroup Monitoring with `systemd-cgtop`
**Task**: Run `systemd-cgtop` for 5 seconds to inspect live CPU, memory, and I/O consumption across all systemd cgroups.
<details>
<summary>View Solution</summary>

```bash
sudo systemd-cgtop -n 2 -b
```
</details>

---

## Summary Cheat Sheet & Further Reading

### Command Reference
| Operation | Command Example |
| :--- | :--- |
| **Verify Cgroups v2** | `stat -fc %T /sys/fs/cgroup/` (returns `cgroup2fs`) |
| **Launch Isolated Shell** | `sudo unshare --uts --pid --mount-proc --fork /bin/bash` |
| **Enter Process Namespaces** | `sudo nsenter -t <PID> -m -u -i -n -p /bin/bash` |
| **Create Network Namespace** | `sudo ip netns add <name>` |
| **Run in Network Namespace** | `sudo ip netns exec <name> <command>` |
| **Create Cgroup v2 Node** | `sudo mkdir -p /sys/fs/cgroup/<name>` |
| **Set Memory Limit** | `echo "536870912" \| sudo tee /sys/fs/cgroup/<name>/memory.max` |
| **Set CPU Quota (1 vCPU)** | `echo "100000 100000" \| sudo tee /sys/fs/cgroup/<name>/cpu.max` |
| **Attach PID to Cgroup** | `echo <PID> \| sudo tee /sys/fs/cgroup/<name>/cgroup.procs` |
| **View Cgroup Tree** | `systemd-cgls` |

### Further Reading
- `man 7 namespaces` and `man 1 unshare`
- `man 1 nsenter` and `man 8 ip-netns`
- [Linux Kernel Documentation: Control Group v2](https://docs.kernel.org/admin-guide/cgroup-v2.html)
- [Michael Kerrisk: Namespaces in Operation (LWN.net Series)](https://lwn.net/Articles/531114/)
- [Brendan Gregg: Cgroups and Resource Control](https://www.brendangregg.com/)
