<!--
================================================================================
Metadata:
  Title: Technical Interview Preparation - Docker Basics and Architecture Q&A
  Track: Docker / DevOps
  Difficulty: Junior to Senior
  Estimated Time: 60 minutes
  Key Terminology: Namespaces, cgroups, UnionFS, containerd, runc, OCI, dockerd, IPC, UTS, Mount, PID, Network Namespaces
================================================================================
-->

# Docker Basics and Architecture: 25 In-Depth Interview Questions & Answers

[<- Back to Interview Overview](./README.md) | [Next Note: 02-images-and-dockerfile-qa.md ->](./02-images-and-dockerfile-qa.md)

---

## Overview

This guide provides 25 deep, production-tested interview questions covering Docker engine architecture, Linux kernel primitives, container process lifecycles, and core command mechanics. Questions are tiered from foundational operational mechanics to Staff-level kernel internals.

---

## Questions Index

1. [Q01: What is a container at the Linux kernel level?](#q01-what-is-a-container-at-the-linux-kernel-level)
2. [Q02: Explain the architectural relationship between dockerd, containerd, containerd-shim, and runc.](#q02-explain-the-architectural-relationship-between-dockerd-containerd-containerd-shim-and-runc)
3. [Q03: What are Linux namespaces, and which namespaces are created for a default container?](#q03-what-are-linux-namespaces-and-which-namespaces-are-created-for-a-default-container)
4. [Q04: How do control groups (cgroups) differ from namespaces?](#q04-how-do-control-groups-cgroups-differ-from-namespaces)
5. [Q05: What is the exact sequence of events when you execute `docker run`?](#q05-what-is-the-exact-sequence-of-events-when-you-execute-docker-run)
6. [Q06: How do virtual machines and containers differ at the hardware and kernel layers?](#q06-how-do-virtual-machines-and-containers-differ-at-the-hardware-and-kernel-layers)
7. [Q07: What is the Open Container Initiative (OCI) and why does it matter?](#q07-what-is-the-open-container-initiative-oci-and-why-does-it-matter)
8. [Q08: What is containerd-shim and why is it necessary?](#q08-what-is-containerd-shim-and-why-is-it-necessary)
9. [Q09: Explain the difference between `docker stop` and `docker kill`.](#q09-explain-the-difference-between-docker-stop-and-docker-kill)
10. [Q10: What does Docker exit code 137 mean, and how do you differentiate SIGKILL from OOMKilled?](#q10-what-does-docker-exit-code-137-mean-and-how-do-you-differentiate-sigkill-from-oomkilled)
11. [Q11: What is PID 1 inside a container and what are the consequences of PID 1 zombie reaping failure?](#q11-what-is-pid-1-inside-a-container-and-what-are-the-consequences-of-pid-1-zombie-reaping-failure)
12. [Q12: How does Docker interact with systemd on modern Linux distributions?](#q12-how-does-docker-interact-with-systemd-on-modern-linux-distributions)
13. [Q13: What happens when you run `docker exec` under the hood?](#q13-what-happens-when-you-run-docker-exec-under-the-hood)
14. [Q14: Explain the difference between Docker Host, Client, and Daemon.](#q14-explain-the-difference-between-docker-host-client-and-daemon)
15. [Q15: How does Docker achieve file isolation without virtual disks?](#q15-how-does-docker-achieve-file-isolation-without-virtual-disks)
16. [Q16: What is the role of the User (userns) namespace in Docker security?](#q16-what-is-the-role-of-the-user-userns-namespace-in-docker-security)
17. [Q17: Why does `ps aux` inside a container only show a few processes, while on the host it shows all?](#q17-why-does-ps-aux-inside-a-container-only-show-a-few-processes-while-on-the-host-it-shows-all)
18. [Q18: What is the difference between `docker pause` and `docker stop`?](#q18-what-is-the-difference-between-docker-pause-and-docker-stop)
19. [Q19: How can you recover a container that is stuck in a `Removal In Progress` or `Dead` state?](#q19-how-can-you-recover-a-container-that-is-stuck-in-a-removal-in-progress-or-dead-state)
20. [Q20: What are Docker event streams and how can you monitor them programmatically?](#q20-what-are-docker-event-streams-and-how-can-you-monitor-them-programmatically)
21. [Q21: How does Docker implement the Copy-on-Write (CoW) mechanism?](#q21-how-does-docker-implement-the-copy-on-write-cow-mechanism)
22. [Q22: Can a container run a different Linux distribution than the host OS? Can it run Windows binaries on Linux?](#q22-can-a-container-run-a-different-linux-distribution-than-the-host-os-can-it-run-windows-binaries-on-linux)
23. [Q23: What is the difference between `docker attach` and `docker exec`?](#q23-what-is-the-difference-between-docker-attach-and-docker-exec)
24. [Q24: How does Rootless Docker work and what kernel features does it require?](#q24-how-does-rootless-docker-work-and-what-kernel-features-does-it-require)
25. [Q25: What happens when the host kernel encounters an Out-Of-Memory condition involving containers?](#q25-what-happens-when-the-host-kernel-encounters-an-out-of-memory-condition-involving-containers)

---

## Technical Q&A

<details>
<summary><strong>Q01: What is a container at the Linux kernel level?</strong></summary>

At the Linux kernel level, **there is no distinct kernel object called a "container."** A container is simply a standard Linux process (or group of processes) whose visibility and resource consumption are restricted using native Linux kernel subsystems:
1. **Linux Namespaces**: Provide isolation of global system resources (PID, NET, MNT, IPC, UTS, USER, CGROUP).
2. **Control Groups (cgroups)**: Enforce resource budgeting and metering (CPU, memory, disk I/O, PIDs).
3. **Security Profiles**: Mandatory Access Control (AppArmor, SELinux) and system call filtering (`seccomp`).
4. **Union Filesystems (`overlay2`)**: Provide layered root filesystem assembly via `pivot_root` or `chroot`.

From the host kernel's perspective, running `docker run alpine sleep 100` simply spawns an ordinary process executing `/bin/sleep 100` visible in `ps aux` on the host, but isolated inside dedicated namespace pointers in `/proc/<pid>/ns/`.
</details>

<details>
<summary><strong>Q02: Explain the architectural relationship between dockerd, containerd, containerd-shim, and runc.</strong></summary>

Modern Docker is structured as a modular stack conforming to OCI standards:
- **`dockerd` (Docker Daemon)**: The high-level management daemon. Exposes the Docker REST API, handles user authentication, CLI requests, image building (BuildKit), volume management, and bridge networking.
- **`containerd`**: A CNCF graduated runtime daemon. Manages the complete container lifecycle: image pulling/unpacking, rootfs snapshot management, and supervisory execution of container runtimes via gRPC.
- **`containerd-shim`**: A lightweight process spawned for every running container. It holds the container's standard I/O (stdin/stdout/stderr) file descriptors open, captures the container exit code, and keeps the container running even if `dockerd` and `containerd` restart (`live-restore`).
- **`runc`**: The OCI reference implementation CLI tool. It is invoked briefly by the shim to interact directly with the Linux kernel: sets up cgroups, configures namespaces, mounts the root filesystem, calls `pivot_root`, and executes the container binary, then exits immediately.
</details>

<details>
<summary><strong>Q03: What are Linux namespaces, and which namespaces are created for a default container?</strong></summary>

Linux namespaces partition kernel resources such that one set of processes sees a completely distinct set of resources from another. When a container starts, Docker creates up to seven namespaces:
1. **PID (Process ID)**: Isolates the process ID space. The container's primary process becomes PID 1 inside the namespace, independent of its host PID.
2. **NET (Networking)**: Isolates network devices, IP routing tables, port bindings, and firewall rules.
3. **MNT (Mount)**: Isolates filesystem mount points. The container sees its own `/` mount via `pivot_root`.
4. **IPC (Inter-Process Communication)**: Isolates System V IPC and POSIX message queues and shared memory.
5. **UTS (UNIX Timesharing System)**: Isolates the hostname and NIS domain name.
6. **USER (User ID)**: Maps container UIDs/GIDs to different host UIDs/GIDs (enabled with `userns-remap`).
7. **CGROUP (Control Group)**: Hides the host cgroup hierarchy from the container's `/proc/self/cgroup`.
</details>

<details>
<summary><strong>Q04: How do control groups (cgroups) differ from namespaces?</strong></summary>

- **Namespaces determine what a process can SEE**: They provide spatial isolation. A process in a private PID namespace cannot see or signal host processes; a process in a private NET namespace cannot see host network interfaces.
- **Control Groups (cgroups) determine what a process can USE**: They enforce volumetric resource allocation. Cgroups govern how much CPU time (CFS scheduler quota), physical RAM, swap space, disk block I/O throughput, and maximum concurrent threads/PIDs a container may consume.
In short: **Namespaces = Isolation; cgroups = Metering and Throttling.**
</details>

<details>
<summary><strong>Q05: What is the exact sequence of events when you execute `docker run`?</strong></summary>

1. **CLI Phase**: `docker` CLI parses flags, contacts Docker Engine via UNIX socket `/var/run/docker.sock` over HTTP REST API.
2. **Image Resolution**: `dockerd` checks local image store. If absent, sends pull request to `containerd`.
3. **Containerd Pull**: `containerd` downloads OCI image manifests and layer tarballs from registry, verifies SHA256 digests, and unpacks layers into an overlay2 snapshot.
4. **Container Creation**: `dockerd` creates container metadata (allocates IP from IPAM network driver, creates writable layer, generates OCI `config.json` bundle).
5. **Shim Execution**: `containerd` spawns `containerd-shim`.
6. **runc Invocation**: The shim calls `runc create` passing `config.json`.
7. **Kernel Configuration**: `runc` calls Linux `clone()` with namespace flags (`CLONE_NEWPID`, `CLONE_NEWNET`, etc.), writes cgroup limits into `/sys/fs/cgroup/`, mounts rootfs, calls `pivot_root`, and executes the target binary.
8. **Handoff**: `runc` exits; `containerd-shim` stays alive managing process I/O and signals.
</details>

<details>
<summary><strong>Q06: How do virtual machines and containers differ at the hardware and kernel layers?</strong></summary>

```
VIRTUAL MACHINE ARCHITECTURE:
[ App A ]  [ App B ]
[ Guest OS Kernel A ]  [ Guest OS Kernel B ]   <-- High Memory Overhead (1-2GB each)
[ Hypervisor (Type 1 KVM/ESXi or Type 2 VirtualBox) ]
[ Bare Metal Hardware / Host Kernel ]

CONTAINER ARCHITECTURE:
[ App A (PID 1) ]   [ App B (PID 1) ]
[ Namespaces / cgroups ] [ Namespaces / cgroups ]
[ SINGLE SHARED HOST LINUX KERNEL ]            <-- Near Zero Overhead (Megabytes)
[ Bare Metal Hardware ]
```

- **Kernel**: VMs execute independent virtualized guest kernels. Containers share the host operating system kernel directly.
- **Boot Time**: VMs take 30–90 seconds to boot a virtual BIOS, kernel, and init daemons. Containers start in milliseconds because they are simply spawned processes.
- **Performance**: VMs incur hypervisor CPU and memory translation overhead (EPT/SLAT). Containers execute at native bare-metal hardware instruction speeds.
</details>

<details>
<summary><strong>Q07: What is the Open Container Initiative (OCI) and why does it matter?</strong></summary>

The Open Container Initiative (OCI) is a Linux Foundation governance project established in 2015 to create open, vendor-neutral industry standards for container infrastructure. It defines two core specifications:
1. **Image Specification (`image-spec`)**: Dictates image layer formatting, manifest schemas, configuration JSON, and cryptographic digests.
2. **Runtime Specification (`runtime-spec`)**: Defines the filesystem bundle layout and lifecycle operations (`create`, `start`, `stop`, `delete`) for running a container.
**Why it matters**: It guarantees interoperability. Images built using Docker can run on Kubernetes using `CRI-O` or `containerd`, or inside alternative runtimes like AWS Firecracker or Kata Containers, without modification or vendor lock-in.
</details>

<details>
<summary><strong>Q08: What is containerd-shim and why is it necessary?</strong></summary>

`containerd-shim` is a lightweight daemon process that sits between `containerd` and `runc`. It exists to solve three critical problems:
1. **Daemonless Containers (`live-restore`)**: It allows `dockerd` and `containerd` to be restarted or upgraded without terminating running containers. The shim remains running and keeps the container process alive.
2. **File Descriptor Persistence**: It holds container `stdin`, `stdout`, and `stderr` pipes open even if the Docker daemon restarts, preventing processes from crashing with `SIGPIPE`.
3. **Exit Code & PID Reaping**: It waits on the container PID to capture its exact exit status code, preventing zombie process accumulation when PID 1 exits.
</details>

<details>
<summary><strong>Q09: Explain the difference between `docker stop` and `docker kill`.</strong></summary>

- **`docker stop` (Graceful)**:
  1. Sends POSIX signal `SIGTERM` (signal 15) to container PID 1.
  2. Starts a grace period countdown timer (default 10 seconds, configurable via `-t <seconds>`).
  3. Gives the application time to drain active network connections, write pending buffers to disk, and exit cleanly.
  4. If the process does not terminate before the timeout expires, Docker sends `SIGKILL` (signal 9) to forcibly abort it.
- **`docker kill` (Immediate)**:
  1. Sends `SIGKILL` (signal 9) immediately by default, terminating the container process without grace period.
  2. Can optionally send custom signals (e.g., `docker kill --signal=SIGHUP <id>`).
</details>

<details>
<summary><strong>Q10: What does Docker exit code 137 mean, and how do you differentiate SIGKILL from OOMKilled?</strong></summary>

In standard UNIX/POSIX shells, when a process is killed by an unhandled signal, the exit code is $128 + \text{Signal Number}$.
Because `SIGKILL` is signal 9, $128 + 9 = 137$.
However, exit code 137 occurs under two distinct scenarios:
1. An administrator or script executed `docker kill` or `kill -9`.
2. The Linux kernel **Out-Of-Memory (OOM) Killer** terminated the container.

To differentiate:
Execute `docker inspect <container> --format '{{.State.OOMKilled}}'`:
- If `true`: The container exceeded its cgroup memory limit (`-m`) and was killed by the kernel OOM killer.
- If `false`: The container was killed externally via `SIGKILL` or timed out during `docker stop`.
</details>

<details>
<summary><strong>Q11: What is PID 1 inside a container and what are the consequences of PID 1 zombie reaping failure?</strong></summary>

In UNIX, PID 1 (traditionally `systemd` or `init`) has two unique responsibilities:
1. **Signal Handling**: By default, the Linux kernel ignores standard signals (like `SIGTERM`) for PID 1 unless the process explicitly registers an explicit signal handler. If an application (like Node.js or Python) does not register a `SIGTERM` handler, `docker stop` will fail to stop it gracefully.
2. **Zombie Reaping**: When child processes terminate, they remain in the process table as "zombies" (`defunct`) until their parent reads their exit status via `wait()`. If the parent exits first, orphans are reparented to PID 1. If PID 1 does not implement a reaping loop, zombie processes accumulate until the host process table (`kernel.pid_max`) is saturated, preventing the host machine from spawning any new processes.
*Remediation*: Use an init system inside the container (`docker run --init` or `tini`).
</details>

<details>
<summary><strong>Q12: How does Docker interact with systemd on modern Linux distributions?</strong></summary>

Docker interacts with systemd in two ways:
1. **Service Management & Socket Activation**: Docker Engine is managed as a systemd unit (`docker.service`) activated via `docker.socket` on `/var/run/docker.sock`.
2. **Cgroup Management (`native.cgroupdriver=systemd`)**: On modern distributions running cgroups v2, both Docker and the OS must coordinate cgroup allocations. Setting `systemd` as the cgroup driver ensures that container cgroups are structured under systemd slices (`system.slice`), preventing resource accounting conflicts between the kernel and container managers.
</details>

<details>
<summary><strong>Q13: What happens when you run `docker exec` under the hood?</strong></summary>

When you execute `docker exec -it <container> sh`:
1. The CLI calls the `dockerd` API `POST /containers/{id}/exec`.
2. `dockerd` directs `containerd` to spawn a new process inside the existing container.
3. `runc exec` calls the Linux kernel system call `setns()`.
4. `setns()` attaches the newly spawned `sh` process to the existing Linux namespaces (PID, NET, MNT, IPC, UTS) and cgroups of the target container.
5. The process is executed within the container's environment without restarting or disrupting the container's PID 1 process.
</details>

<details>
<summary><strong>Q14: Explain the difference between Docker Host, Client, and Daemon.</strong></summary>

Docker uses a Client-Server architecture:
- **Docker Client (`docker`)**: The command-line interface tool used by humans or CI/CD pipelines. It parses commands and sends JSON HTTP requests to the Docker Engine REST API.
- **Docker Daemon (`dockerd`)**: The background service running on the host. It listens for API requests on `/var/run/docker.sock` (or TCP port 2375/2376), manages containers, images, networks, and storage.
- **Docker Host**: The physical machine or virtual cloud instance (e.g., AWS EC2) running the Linux kernel, systemd, the Docker daemon, and the container workloads.
</details>

<details>
<summary><strong>Q15: How does Docker achieve file isolation without virtual disks?</strong></summary>

Docker uses **Union Filesystems** (specifically `overlay2`) combined with the `pivot_root` kernel system call:
1. Read-only image layers are unpacked onto the host filesystem under `/var/lib/docker/overlay2/`.
2. When a container runs, Docker mounts these read-only layers (`lowerdir`) merged with an ephemeral read-write directory (`upperdir`) into a unified view (`merged`).
3. `runc` calls `chroot` or `pivot_root`, making the `merged` directory the root filesystem (`/`) of the container mount namespace.
Because changes write only to `upperdir`, the underlying image layers remain immutable and shared across thousands of containers.
</details>

<details>
<summary><strong>Q16: What is the role of the User (userns) namespace in Docker security?</strong></summary>

By default, Docker containers share the host's user namespace: UID 0 (root) inside the container is identical to UID 0 (root) on the host kernel. If a container process escapes the container, it possesses full root administrative privileges on the host.
The **User Namespace (`userns`)** solves this via UID/GID mapping:
- UID 0 inside the container is mapped to an unprivileged high UID (e.g., UID 100000) on the host.
- If a root process breaks out of the container filesystem, it finds itself running as an unprivileged, restricted user on the host kernel with zero privileges.
</details>

<details>
<summary><strong>Q17: Why does `ps aux` inside a container only show a few processes, while on the host it shows all?</strong></summary>

This is the direct effect of the **Linux PID Namespace**:
- Inside the container PID namespace, process IDs are numbered starting at 1. The container process tree has no visibility into ancestor or sibling namespaces. Reading `/proc` returns only processes belonging to that namespace.
- On the host, the root PID namespace sees all processes across the entire machine. A container process running as PID 1 inside its namespace will appear in the host's `ps aux` with its real, globally unique host PID (e.g., PID 28419).
</details>

<details>
<summary><strong>Q18: What is the difference between `docker pause` and `docker stop`?</strong></summary>

- **`docker pause`**: Uses the Linux kernel **cgroups freezer subsystem** (`cgroup.freeze`). It suspends execution of all threads in the container instantaneously. The processes remain resident in memory, retaining network connections and file descriptors, but receive zero CPU cycles. Resumed instantly via `docker unpause`.
- **`docker stop`**: Sends POSIX signals (`SIGTERM`, then `SIGKILL`) to terminate process execution entirely. Memory is freed, processes exit, and network sockets are closed.
</details>

<details>
<summary><strong>Q19: How can you recover a container that is stuck in a `Removal In Progress` or `Dead` state?</strong></summary>

Containers get stuck in `Dead` or `Removal In Progress` when the Docker daemon cannot unmount the container rootfs due to an external process holding an open file descriptor or locked mount point.
**Remediation Steps**:
1. Identify the container full ID: `docker inspect <name> --format '{{.Id}}'`.
2. Search for host processes holding open mounts in that overlay path:
   ```bash
   grep "/var/lib/docker/overlay2/<id>" /proc/*/mountinfo
   ```
3. Terminate the offending host process holding the lock (`kill -9 <pid>`).
4. Force unmount the filesystem: `umount -l /var/lib/docker/overlay2/<id>/merged`.
5. Remove the container: `docker rm -f <name>`.
6. If still wedged, restart Docker daemon: `sudo systemctl restart docker`.
</details>

<details>
<summary><strong>Q20: What are Docker event streams and how can you monitor them programmatically?</strong></summary>

The Docker daemon emits real-time JSON event streams documenting state transitions across containers, images, volumes, networks, and plugins.
- CLI: `docker events --filter 'type=container' --filter 'event=die'`.
- REST API: `GET /events`.
- Emitted events include `create`, `start`, `kill`, `die`, `oom`, `pause`, and `destroy`. Monitoring event streams allows monitoring agents (like Datadog or custom autoscalers) to respond reactively to container lifecycle transitions without polling.
</details>

<details>
<summary><strong>Q21: How does Docker implement the Copy-on-Write (CoW) mechanism?</strong></summary>

Docker implements Copy-on-Write using storage drivers like `overlay2`:
- When a container reads a file that exists in an image layer (`lowerdir`), the kernel reads directly from the read-only layer on disk.
- When a container attempts to modify or delete that file, the overlay2 driver intercepts the write, performs a **copy-up** operation (copying the file from `lowerdir` into the container's writable `upperdir`), and applies modifications exclusively to the copy.
- The original file in the image layer remains untouched, enabling instant container startup and massive disk deduplication.
</details>

<details>
<summary><strong>Q22: Can a container run a different Linux distribution than the host OS? Can it run Windows binaries on Linux?</strong></summary>

- **Different Linux Distros**: **Yes.** You can run an Alpine Linux container on an Ubuntu host, or CentOS on Debian. This works because all Linux distributions share the same Linux system call interface. The container image merely supplies userspace utilities (glibc, musl, apk, apt, busybox), while the host kernel services all system calls.
- **Windows Binaries on Linux**: **No (natively).** Windows binaries require the Windows NT kernel system call interface. A standard Linux kernel cannot execute Windows binaries without hypervisor virtualization (such as a full VM running Windows).
</details>

<details>
<summary><strong>Q23: What is the difference between `docker attach` and `docker exec`?</strong></summary>

- **`docker attach`**: Connects your local terminal standard input, output, and error streams directly to the container's **already running PID 1 process**. If you press `Ctrl+C` while attached, you send `SIGINT` to PID 1, terminating the entire container.
- **`docker exec`**: Spawns a **brand-new, independent process** (e.g. `/bin/sh`) inside the container's existing namespaces. Exiting or terminating the exec session does not terminate the container's PID 1 process.
</details>

<details>
<summary><strong>Q24: How does Rootless Docker work and what kernel features does it require?</strong></summary>

Rootless Docker runs both the Docker daemon (`dockerd`) and containers under an unprivileged regular user account without requiring `sudo` or root privileges.
**Kernel Requirements**:
1. User Namespaces (`newuidmap` and `newgidmap` binaries) allowing UID mapping without root.
2. RootlessKit to provide pseudo-root environment and network isolation.
3. `slirp4netns` or `veth` pairs to emulate userspace network stacks.
4. Kernel cgroups v2 delegation (`systemd --user`).
*Security Benefit*: Even if an attacker achieves complete container breakout, they have zero root access to the host machine.
</details>

<details>
<summary><strong>Q25: What happens when the host kernel encounters an Out-Of-Memory condition involving containers?</strong></summary>

When host physical memory is exhausted, the Linux kernel invokes `out_of_memory()`. The kernel scans all processes and computes an `oom_score` (0 to 1000) for each based on:
1. Percentage of RAM consumed by the process.
2. Administrative bias defined in `/proc/[pid]/oom_score_adj`.
Docker automatically assigns higher `oom_score_adj` values to containers compared to critical host daemons (like `dockerd`, `sshd`, and `systemd`). The kernel selects the process with the highest score, sends `SIGKILL` (signal 9), and reclaims its memory pages. Docker records the termination and marks the container with `ExitCode: 137` and `OOMKilled: true`.
</details>

---

[<- Back to Interview Overview](./README.md) | [Next Note: 02-images-and-dockerfile-qa.md ->](./02-images-and-dockerfile-qa.md)
