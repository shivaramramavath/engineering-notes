# Namespaces and cgroups

A container isn't a mini virtual machine. It is an ordinary Linux process (or a few) that the kernel has been told to **show a restricted view of the system** (namespaces) and **limit in resource use** (cgroups). Docker, Podman, Kubernetes, and systemd's sandboxing are all built on these two kernel features. Once you understand them, container behavior that looks odd (PID 1 ignoring signals, OOM kills at a memory limit, `free` showing host memory) stops being mysterious.

Prerequisites: [Processes and Signals](../03-system/01-processes-and-signals.md), [Networking Basics](../04-networking/01-networking-basics.md), and [Performance](../05-production/03-performance.md) (OOM killer, limits).

## The big picture

```text
  "container" =  process(es)
               +  namespaces      what it can SEE        (pid, net, mount, uts, ipc, user…)
               +  cgroups         what it can USE        (CPU, memory, I/O, pids)
               +  filesystem      its own root           (image layers via overlayfs)
               +  security        what it may DO         (capabilities, seccomp, AppArmor/SELinux)
```

The key consequence: **containers share the host kernel.** There's no guest OS. That's why they start in milliseconds and use little overhead, and also why a kernel-level escape or misconfiguration affects the host, in a way a VM boundary generally doesn't.

## Namespaces: isolating what a process sees

A namespace gives a process its own private instance of some global resource. Processes in the same namespace share it; others can't see in.

| Namespace | Isolates | Effect |
|---|---|---|
| `pid` | process IDs | the container's first process is PID 1; it can't see host processes |
| `net` | network stack | own interfaces, IPs, routing table, firewall rules, ports |
| `mnt` | mount points | own filesystem tree / root |
| `uts` | hostname and domain name | container has its own hostname |
| `ipc` | System V IPC, POSIX queues | no shared memory with the host |
| `user` | UID/GID mappings | root inside can map to an unprivileged user outside |
| `cgroup` | view of the cgroup tree | container sees its own cgroup as the root |
| `time` | clock offsets (newer kernels) | rarely used |

Every process belongs to one namespace of each type. See yours:

```bash
ls -l /proc/$$/ns
# lrwxrwxrwx ... net -> 'net:[4026531840]'
# lrwxrwxrwx ... pid -> 'pid:[4026531836]'
lsns                       # list all namespaces and the processes in them
lsns -t net                # only network namespaces
```

Two processes are in the same namespace when the number in brackets matches.

### Try it: `unshare`

`unshare` runs a command in new namespaces, which lets you feel each one without Docker.

```bash
# New PID namespace: this shell believes it's PID 1 and sees only its own processes
sudo unshare --pid --fork --mount-proc bash
ps aux            # only bash and ps are listed
echo $$           # 1
exit
```

```bash
# New UTS namespace: change hostname without affecting the host
sudo unshare --uts bash
hostname demo-container
hostname          # demo-container  (host's name is unchanged)
exit
```

```bash
# New network namespace: starts with only a down loopback interface
sudo unshare --net bash
ip addr           # just "lo", state DOWN: no connectivity at all
exit
```

`--fork` is needed for PID namespaces because the new namespace applies to the *children* of the calling process; `--mount-proc` remounts `/proc` so `ps` reflects the new PID namespace instead of the host's.

### Network namespaces and how containers get networking

A fresh network namespace has no interfaces. Docker connects it to the outside using a **veth pair**: a virtual cable with one end inside the container and the other on the host, plugged into a bridge.

```text
 ┌────────── container (net ns) ─────────┐
 │  eth0  172.17.0.2                      │
 └───────┬────────────────────────────────┘
         │ veth pair (virtual cable)
 ┌───────┴──────── host ──────────────────────────────┐
 │  vethXXXX ── docker0 bridge 172.17.0.1 ── NAT ── eth0 ──► internet │
 └─────────────────────────────────────────────────────┘
```

Outbound traffic is NAT'd (masqueraded) by iptables/nftables rules Docker manages, and `-p 8080:80` adds a DNAT rule. That is also why Docker-published ports can bypass ufw ([firewall note](../04-networking/03-firewall.md)).

Build the same thing by hand:

```bash
sudo ip netns add demo
sudo ip link add veth-host type veth peer name veth-demo
sudo ip link set veth-demo netns demo

sudo ip addr add 10.10.0.1/24 dev veth-host
sudo ip link set veth-host up

sudo ip netns exec demo ip addr add 10.10.0.2/24 dev veth-demo
sudo ip netns exec demo ip link set veth-demo up
sudo ip netns exec demo ip link set lo up

ping -c 1 10.10.0.2                          # host reaches the "container"
sudo ip netns exec demo ping -c 1 10.10.0.1  # and back

sudo ip netns del demo                       # cleanup (also removes the veth pair)
```

### User namespaces

A user namespace maps UIDs inside to different UIDs outside. Root (UID 0) inside can be an ordinary unprivileged UID on the host.

```bash
unshare --user --map-root-user bash
id                # uid=0(root) ... but only inside this namespace
```

This is how **rootless** containers work. Some distros (recent Ubuntu releases among them) restrict unprivileged user namespaces by default, so this may fail until policy allows it.

By default, **Docker does not use user namespaces**: root in a typical container is really UID 0 on the host, held back only by dropped capabilities, seccomp, and the other layers below. That's a major reason container-root is not harmless.

### Mount namespaces and the container filesystem

Each container gets its own mount namespace and a root filesystem assembled from image **layers** using **overlayfs**: read-only lower layers (the image) plus a thin writable upper layer, merged into one view.

```bash
mount | grep overlay          # on a Docker host: one overlay mount per running container
docker inspect -f '{{.GraphDriver.Data.MergedDir}}' <container>   # where the merged view lives on the host
```

The writable layer is **discarded when the container is removed**. Anything that must persist belongs in a **volume** or **bind mount**, which are other host paths mounted into the container's mount namespace.

### Entering a container's namespaces from the host

Containers are just processes, so the host sees them. `nsenter` runs a command *inside* another process's namespaces, which is great for debugging when the container image has no tools.

```bash
PID=$(docker inspect -f '{{.State.Pid}}' mycontainer)    # container's main process, as the host sees it
sudo nsenter -t "$PID" -n ip addr            # container's network view, using HOST's `ip`
sudo nsenter -t "$PID" -n ss -tlnp           # what is it listening on?
sudo nsenter -t "$PID" -n tcpdump -ni eth0   # capture in its network namespace
sudo nsenter -t "$PID" -m -u -i -n -p sh     # a shell in its mount/uts/ipc/net/pid namespaces
ps -ef --forest | grep -A3 containerd-shim   # host-side view of the container process tree
```

Flags: `-n` net, `-m` mount, `-u` uts, `-i` ipc, `-p` pid.

### PID 1 inside a container

The first process in a PID namespace is PID 1, and the kernel treats PID 1 specially: **signals without an installed handler are ignored**, so a program that doesn't handle SIGTERM may not stop on `docker stop` and gets SIGKILL after the timeout. PID 1 is also responsible for reaping orphaned child processes; if it doesn't, zombies accumulate ([processes note](../03-system/01-processes-and-signals.md)).

Fixes: handle SIGTERM in your app, or run a tiny init as PID 1:

```bash
docker run --init myimage
```

Also use the **exec form** of `CMD`/`ENTRYPOINT` (`["node","server.js"]`) so your program is PID 1 rather than a shell wrapper that doesn't forward signals.

## cgroups: limiting what a process can use

Control groups organize processes into a hierarchy and attach **limits and accounting** to each group: CPU, memory, I/O, number of processes, and more. Namespaces hide things; cgroups ration them.

Modern distros use **cgroup v2** (a single unified hierarchy mounted at `/sys/fs/cgroup`). Check:

```bash
stat -fc %T /sys/fs/cgroup        # cgroup2fs  → v2
cat /proc/self/cgroup             # which cgroup is this shell in?
```

Everything is files. Important ones inside a cgroup directory:

| File | Meaning |
|---|---|
| `cgroup.procs` | PIDs in the group |
| `memory.max` | hard memory limit (`max` = unlimited) |
| `memory.current` | current usage |
| `memory.high` | soft limit; above this the group is throttled and reclaimed |
| `memory.events` | counters including `oom_kill` |
| `cpu.max` | `quota period` in µs; `50000 100000` = 50% of one CPU |
| `cpu.stat` | usage and `nr_throttled` / `throttled_usec` |
| `cpu.weight` | relative share when CPUs are contended |
| `pids.max` | max processes (stops fork bombs) |
| `io.max` | per-device I/O limits |

### systemd manages cgroups for you

Every systemd service gets its own cgroup, and unit settings map straight onto these files:

```bash
systemd-cgls                       # tree of cgroups and their processes
systemd-cgtop                      # live per-cgroup CPU/memory/I/O

# try limits on a throwaway service
sudo systemd-run --unit=demo -p MemoryMax=100M -p CPUQuota=50% -p TasksMax=50 sleep 600
systemctl status demo
cat /sys/fs/cgroup/system.slice/demo.service/memory.max     # 104857600
cat /sys/fs/cgroup/system.slice/demo.service/cpu.max        # 50000 100000
sudo systemctl stop demo
```

In a real unit file these are `MemoryMax=`, `CPUQuota=`, `TasksMax=`, `IOWeight=` ([systemd note](../03-system/03-systemd-and-services.md)). That's how you stop one service from starving the rest of the machine.

### Docker uses the same mechanism

```bash
docker run -d --name web --memory=256m --cpus=0.5 --pids-limit=100 nginx
docker stats web                                 # live usage vs limits
docker inspect -f '{{.HostConfig.Memory}} {{.HostConfig.NanoCpus}}' web
cat /proc/$(docker inspect -f '{{.State.Pid}}' web)/cgroup     # its cgroup path
```

### What happens at the limit

| Resource | Exceeding the limit | Symptom |
|---|---|---|
| **Memory** (`memory.max`) | kernel OOM-kills a process **in that cgroup**, even though the host has free memory | exit code **137**; `docker inspect` shows `OOMKilled: true`; `dmesg` shows "Memory cgroup out of memory" |
| **CPU** (`cpu.max`) | the group is **throttled** (paused until the next period); not killed | slow responses and latency spikes; `nr_throttled` in `cpu.stat` keeps rising |
| **PIDs** (`pids.max`) | new `fork()`/thread creation fails | "Resource temporarily unavailable" |

Two things to remember: a memory limit means *killed*, a CPU limit means *slowed*. And **no limit means no protection**: a container without a memory limit can consume the whole host's memory and trigger OOM kills of other things, including the Docker daemon or sshd. Set limits on production containers and services.

### "Container sees host resources" gotcha

Inside a container, `free`, `top`, and `/proc/meminfo` typically still report **host** totals, and `nproc` may show every host CPU, because those files aren't namespaced by cgroup limits. Applications sizing thread pools or heaps from them can overshoot the limit and get OOM-killed. Modern runtimes (recent JVMs, Go, Node with explicit flags) can read cgroup limits; verify rather than assume, and look at `/sys/fs/cgroup/memory.max` and `cpu.max` for the truth.

## Security layers that complete the picture

Namespaces and cgroups aren't a security sandbox on their own. Container runtimes add:

- **Capabilities**: root's power is split into ~40 pieces (`CAP_NET_BIND_SERVICE`, `CAP_SYS_ADMIN`, …). Containers get a reduced default set. Drop all and add back only what's needed.
- **seccomp**: a syscall filter that blocks dangerous system calls; Docker applies a default profile.
- **AppArmor / SELinux**: a mandatory access policy per container ([hardening](../05-production/01-server-hardening.md)).
- **Read-only root filesystem, no-new-privileges**, non-root `USER`.

```bash
docker run --rm \
  --user 1000:1000 \
  --cap-drop=ALL --cap-add=NET_BIND_SERVICE \
  --read-only --tmpfs /tmp \
  --security-opt no-new-privileges \
  --memory=256m --pids-limit=100 \
  myimage
```

**`--privileged` switches most of this off** and gives the container nearly full access to the host's devices and capabilities. Likewise, mounting `/var/run/docker.sock` into a container is equivalent to giving it root on the host. And adding a user to the `docker` group is effectively granting root ([users and groups](../01-fundamentals/04-users-and-groups.md)).

## What `docker run` actually does

```text
docker CLI ─► dockerd ─► containerd ─► runc (OCI runtime)
                                         │
   1. unshare namespaces: pid, net, mnt, uts, ipc (+ user/cgroup if configured)
   2. create a cgroup and write limits (memory.max, cpu.max, …)
   3. mount the overlayfs root, pivot_root into it
   4. set capabilities, seccomp filter, AppArmor/SELinux profile
   5. exec your command as PID 1 of the new PID namespace
```

Kubernetes pods layer on top of the same primitives (containers in a pod share a network namespace, for example).

## Common mistakes

- **Treating a container as a VM-grade security boundary**, then running as root, `--privileged`, or with the Docker socket mounted.
- **No memory limit**, letting one container threaten the whole host.
- **Confusing CPU throttling with OOM kills** (slow vs dead) when diagnosing.
- **Trusting `free`/`nproc` inside a container** to reflect its limits.
- **Shell-form `CMD`** so the app isn't PID 1 and misses SIGTERM; or no init and a growing zombie list.
- **Storing data in the writable layer**, then losing it on `docker rm`.
- **Assuming "root in the container" is harmless** when no user namespace remapping is in use.
- **Debugging with `docker exec` only**, when the image lacks tools; `nsenter` from the host brings yours along.

## Quick Summary

- Container = process + namespaces (what it sees) + cgroups (what it uses) + overlayfs root + security filters; it shares the host kernel.
- Explore with `lsns`, `/proc/<pid>/ns`, `unshare`, `ip netns`, and `nsenter`.
- Networking is a veth pair into a bridge with NAT; the container filesystem is an overlay with an ephemeral top layer.
- cgroup v2 limits are plain files (`memory.max`, `cpu.max`, `pids.max`); systemd and Docker both write them.
- Memory limit → OOM kill (137); CPU limit → throttling; no limit → no protection.
- PID 1 ignores unhandled signals: handle SIGTERM or use `--init`; drop capabilities and avoid `--privileged`.

**Next:** [Advanced Debugging](./02-advanced-debugging.md)