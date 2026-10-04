# Getting Started with Linux

Linux is the operating system most servers, containers, and cloud workloads run on. If you deploy software, you will end up on a Linux box over SSH sooner or later. This note gets you a working environment and a mental map of what "Linux" actually means, so the rest of the repo has somewhere to run.

## What "Linux" actually is

Strictly, **Linux is a kernel**: the part that manages CPU, memory, devices, and processes. Everything else you touch (shell, `ls`, package manager, init system) comes from other projects, mostly GNU. A **distribution** (distro) bundles the kernel with those tools, a package manager, and defaults.

```text
┌──────────────────────────────┐
│  Your apps (node, nginx, …)  │
├──────────────────────────────┤
│  Shell + userland tools      │  ← bash, coreutils, systemd, apt
├──────────────────────────────┤
│  libc (glibc / musl)         │
├──────────────────────────────┤
│  Linux kernel                │  ← same kernel family everywhere
├──────────────────────────────┤
│  Hardware / hypervisor       │
└──────────────────────────────┘
```

The distro is what changes between machines. The kernel interface stays largely the same, which is why most skills in this repo transfer.

## Choosing a distro

Use **the latest Ubuntu LTS** as your base throughout this repo. LTS releases come out every two years (April of even years) and get five years of standard support, so tutorials, Stack Overflow answers, and cloud images all line up with them.

Distros group into families, mostly defined by package format and manager:

| Family | Examples | Package manager | Where you meet it |
|---|---|---|---|
| Debian | Debian, Ubuntu, Mint | `apt` / `dpkg` | Most tutorials, most cloud images |
| Red Hat | RHEL, Rocky, AlmaLinux, Fedora | `dnf` / `rpm` | Enterprise servers |
| Arch | Arch, Manjaro | `pacman` | Desktops, enthusiasts |
| Alpine | Alpine | `apk` | Tiny container images |

Alpine is worth knowing about: it uses **musl** instead of glibc and BusyBox instead of GNU coreutils, so some binaries and flags behave differently there. Keep that in mind when a script works on Ubuntu but fails in an Alpine container.

## Ways to get a Linux environment

### WSL2 (Windows)

WSL2 runs a real Linux kernel in a lightweight VM, integrated with Windows. Good for daily development.

```powershell
# PowerShell (admin)
wsl --install -d Ubuntu
wsl -l -v          # list distros and their WSL version
```

Things that bite people:

- Keep your projects **inside the Linux filesystem** (`~/projects`), not under `/mnt/c/...`. Cross-filesystem access is much slower and permissions behave oddly.
- WSL2 may not run systemd by default on older installs. If `systemctl` fails, check `/etc/wsl.conf` for `[boot] systemd=true`. We use systemd heavily in [03-system](../03-system/03-systemd-and-services.md).
- Networking differs from a real server (NAT by default), so firewall and port exercises may not match production exactly.

### Local VM

Best for learning administration, because you can break things and restore a snapshot. Options: VirtualBox, VMware, UTM (Apple Silicon), or Multipass for quick Ubuntu VMs.

```bash
multipass launch --name lab --cpus 2 --memory 2G --disk 20G
multipass shell lab
```

### Cloud VM

The closest thing to production. Any provider works; a small instance is enough. You get an IP and log in over SSH with a key (covered properly in [SSH](../04-networking/02-ssh.md)).

```bash
ssh -i ~/.ssh/id_ed25519 ubuntu@<server-ip>
```

Watch the cost: stop or delete the instance when you are done, and never leave a password-login SSH server exposed (see [hardening](../05-production/01-server-hardening.md)).

## First five minutes on any machine

Whatever you picked, orient yourself:

```bash
cat /etc/os-release     # which distro and version
uname -r                # kernel version
whoami                  # who you are
pwd                     # where you are
sudo apt update         # refresh package index (Debian/Ubuntu)
```

`/etc/os-release` is the reliable way to identify a distro in scripts; `ID` and `VERSION_ID` are the fields to read.

Then install the basics you will need:

```bash
sudo apt install -y curl git vim tmux htop tree
```

## Common mistakes

- **Saying "Linux" when you mean "Ubuntu".** Commands like `apt` are Debian-family, not Linux-wide. Check the distro before copying a command.
- **Running everything as root** because it avoids permission errors. It hides the real problem and is dangerous. See [users and groups](./04-users-and-groups.md).
- **Following a tutorial for a different distro version.** Package names and defaults change between releases. Check `VERSION_ID`.
- **Dual-booting or reformatting a real machine** just to learn. A VM or WSL2 is safer and reversible.

## Quick Summary

- Linux is the kernel; a distro is kernel + userland + package manager.
- Use the latest Ubuntu LTS as the default; know the Debian, Red Hat, Arch, and Alpine families exist.
- WSL2 for daily dev, a local VM for experiments, a cloud VM for production-like practice.
- In WSL2, work inside the Linux filesystem, not `/mnt/c`.
- Orient yourself with `cat /etc/os-release`, `uname -r`, `whoami`.

**Next:** [Filesystem and Navigation](./02-filesystem-and-navigation.md)
