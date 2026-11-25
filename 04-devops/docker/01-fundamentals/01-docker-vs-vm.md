# Docker vs Virtual Machines

The question that motivates everything about Docker: why not just use a virtual machine?

## The short answer

A **virtual machine** virtualizes an entire computer, including its own operating system kernel. A **container** virtualizes only the application layer, sharing the host machine's kernel. That difference is why containers are dramatically lighter and faster.

```
Virtual Machines                    Containers
─────────────────                   ──────────

┌─────────┐ ┌─────────┐            ┌─────────┐ ┌─────────┐
│  App A  │ │  App B  │            │  App A  │ │  App B  │
├─────────┤ ├─────────┤            ├─────────┤ ├─────────┤
│ Guest OS│ │ Guest OS│            │ (no guest OS —      │
│ (full!) │ │ (full!) │            │  shares host kernel)│
├─────────┴─┴─────────┤            ├─────────┴─┴─────────┤
│      Hypervisor      │            │    Docker Engine     │
├───────────────────────┤            ├───────────────────────┤
│      Host OS          │            │      Host OS          │
├───────────────────────┤            ├───────────────────────┤
│      Hardware         │            │      Hardware         │
└───────────────────────┘            └───────────────────────┘
```

## What each one actually is

### Virtual machine

A hypervisor (like VMware, VirtualBox, or Hyper-V) emulates hardware, and a **full guest operating system** — kernel included — runs on top of that emulated hardware. Each VM is a complete, independent OS installation.

### Container

Docker uses features already built into the Linux kernel (namespaces for isolation, cgroups for resource limits) to give a process the _illusion_ of running in its own isolated environment — its own filesystem, network stack, and process tree — while actually sharing the host's kernel with every other container.

## Why this difference matters in practice

|                                       | Virtual Machine                            | Container                                            |
| ------------------------------------- | ------------------------------------------ | ---------------------------------------------------- |
| Includes a full OS kernel             | Yes                                        | No — shares the host kernel                          |
| Typical size                          | Gigabytes                                  | Megabytes                                            |
| Startup time                          | Minutes (booting an OS)                    | Seconds (or less)                                    |
| Resource overhead                     | High — each VM reserves its own memory/CPU | Low — containers share the host's resources directly |
| Isolation strength                    | Very strong (separate kernel)              | Strong, but weaker than a VM (shared kernel)         |
| Density (how many fit on one machine) | Tens                                       | Hundreds to thousands                                |

## Isolation: strong, but not absolute

Because containers share the host kernel, the isolation is enforced by the kernel's namespace/cgroup features rather than by fully separate hardware-level virtualization. In practice this is more than sufficient for the vast majority of use cases, but it's why:

- A container escape (a bug allowing a process to break out of its container) is a more direct security concern than a VM escape, since there's no second OS layer to also break through
- **Docker on Windows/macOS still runs a lightweight Linux VM under the hood** — because Docker containers are Linux containers, and Windows/macOS don't have a compatible kernel to share directly. Docker Desktop hides this from you, but it's there.

## Why containers won for application deployment

- **Fast startup** — a container starts in about as long as it takes to launch a process, not boot an OS
- **Consistency** — "works on my machine" problems shrink dramatically, since the container packages the exact runtime environment (libraries, dependencies) along with the app
- **Density** — you can run far more containers than VMs on the same hardware, since there's no duplicated OS overhead per instance
- **Portability** — an image built once runs identically on a laptop, a CI server, or a cloud VM, as long as they all run Docker

## When you'd still want a VM

- You need to run a genuinely different OS (e.g. Windows workloads on a Linux host, without Docker's Windows-container mode)
- You need the strongest possible isolation boundary (e.g. running fully untrusted code)
- You're virtualizing at the infrastructure level itself (which is, not coincidentally, usually how the "VM" your containers run inside a cloud provider's servers is provided in the first place — containers and VMs are often used _together_, not as a strict either/or)

## Quick summary

- A VM virtualizes hardware and runs a full guest OS per instance; a container shares the host's kernel and only virtualizes the application layer
- This makes containers far smaller, faster to start, and denser than VMs
- Container isolation is strong but relies on the shared kernel's namespace/cgroup features, not separate hardware virtualization
- Docker on Windows/macOS runs inside a lightweight Linux VM, since containers need a Linux kernel to share

## Next

**`02_images-containers.md`** covers the core distinction Docker itself is built on: the difference between an image and a container.
