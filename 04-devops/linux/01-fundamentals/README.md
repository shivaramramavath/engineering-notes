# Module 01: Linux Core Fundamentals

[<- Back to Linux Track Root](../README.md) | [Next Module: 02-shell ->](../02-shell/README.md)

---

## 1. Overview & Scope

Module 01 provides the rigorous architectural and operational foundation of the Linux operating system. In modern DevOps, cloud infrastructure, and SRE roles, Linux is not merely an interactive desktop or simple server host; it is the substrate powering containers, orchestrators, hypervisors, and distributed microservices.

This module dispels common abstractions and builds technical mastery from the hardware silicon and firmware layer up through the kernel, virtual filesystem (VFS), identity models, privilege management, and initialization daemons.

---

## 2. Core Architecture & Mental Model

```
+--------------------------------------------------------------------------+
| LINUX FOUNDATIONAL ARCHITECTURE                                          |
|                                                                          |
| [ User Space (Ring 3) ]                                                  |
| ├── Applications & Daemons (Nginx, PostgreSQL, Node.js, Shell)           |
| ├── Standard C Library (glibc / musl)                                    |
| └── User & Group Credential Context (RUID, EUID, SUID, PAM)               |
+==========================================================================+
| [ System Call Interface (sys_enter, syscall dispatch table) ]            |
+==========================================================================+
| [ Linux Kernel Space (Ring 0) ]                                          |
| ├── Process Manager (CFS Scheduler, task_struct, Namespaces, Cgroups)    |
| ├── Virtual Memory Manager (MMU Page Tables, Page Cache, Swap)          |
| ├── Virtual File System (VFS: Inodes, Dentries, Mounts, FHS Hierarchy)   |
| ├── Security Engine (Mode bits, POSIX ACLs, AppArmor, SELinux)           |
| └── Network Stack & Device Drivers (Block, Char, Netfilter)              |
+==========================================================================+
| [ Hardware & Firmware Substrate ]                                        |
| └── CPU / MMU, Physical RAM, NVMe/SATA Storage, NICs, UEFI / BIOS        |
+--------------------------------------------------------------------------+
```

---

## 3. Module Notes Catalog

| # | Note | Level | Time | Key Concepts & Topics |
| :---: | :--- | :---: | :---: | :--- |
| **01** | [Getting Started with Linux](./01-getting-started.md) | Beginner | 45m | Linux vs GNU, distros (Ubuntu 24.04 vs RHEL 9 vs Alpine), WSL2, lab provisioning, POSIX compliance |
| **02** | [Linux Architecture: Kernel, User Space & System Calls](./02-linux-architecture.md) | Intermediate | 50m | Ring 0 vs Ring 3, system call lifecycle, VFS objects, `task_struct`, `strace` profiling, `io_uring` |
| **03** | [Filesystem Hierarchy Standard (FHS) & Navigation](./03-filesystem-hierarchy-and-navigation.md) | Beginner-Int | 50m | FHS 3.0 standard, UsrMerge (`/bin -> /usr/bin`), `/proc`, `/sys`, `/dev`, `/run`, `pwd -P`, `pushd`/`popd` |
| **04** | [Working with Files: Operations, Descriptors & Inspection](./04-working-with-files.md) | Beginner-Int | 50m | Atomic `mv` vs cross-device copy, timestamps (`mtime`/`ctime`/`atime`), `mktemp`, `tail -F`, sparse files |
| **05** | [Links, Inodes & File Types in Linux](./05-links-inodes-and-file-types.md) | Intermediate | 55m | Inode architecture, hard links vs symlinks, 7 Unix file types, atomic symlink deploy cutovers, inode exhaustion (`df -i`) |
| **06** | [Users, Groups & Identity Management](./06-users-and-groups.md) | Intermediate | 50m | UIDs/GIDs, `/etc/passwd`, `/etc/shadow`, service accounts (`nologin`), `usermod -aG`, PAM stack, RUID/EUID |
| **07** | [Linux Permissions: Mode Bits, Umask & POSIX ACLs](./07-permissions.md) | Intermediate | 55m | rwx octal & symbolic, file vs directory permissions, umask calculation, SUID/SGID/Sticky bit, POSIX ACLs (`getfacl`/`setfacl`) |
| **08** | [Sudo, Privilege Escalation & Least Privilege](./08-sudo-and-privilege.md) | Intermediate | 50m | Least privilege, `sudo` vs `su -`, `/etc/sudoers` grammar, `visudo`, drop-ins in `/etc/sudoers.d/`, GTFOBins, `sudo-rs` |
| **09** | [Man Pages, Info Pages & Terminal Documentation](./09-man-pages-and-getting-help.md) | Beginner-Int | 45m | The 8 manual sections, `apropos`, `mandb`, shell builtins (`help`) vs binary flags, Section 5 config specs |
| **10** | [Environment Variables & Startup Profiles](./10-environment-variables-and-startup-files.md) | Intermediate | 50m | Process inheritance (`/proc/<pid>/environ`), login vs non-login shells, `/etc/profile`, `~/.bashrc`, `/etc/environment`, systemd `EnvironmentFile` |
| **11** | [The Linux Boot Process: Firmware to Systemd](./11-boot-process.md) | Advanced | 60m | BIOS vs UEFI, GRUB2 stages, `vmlinuz`, initramfs/dracut root pivot, PID 1 systemd targets, `systemd-analyze` |

---

## 4. Key Milestones & Learning Objectives

By completing Module 01, you will be able to:
1. **Explain the Microarchitectural Boundary**: Trace an application request across the System Call Interface into Ring 0 and understand kernel context switching tax.
2. **Diagnose Storage & Metadata Failures**: Distinguish between disk capacity exhaustion (`df -h`) and inode table depletion (`df -i`), and manage open unlinked descriptors.
3. **Architect Collaborative Security Boundaries**: Structure team directories with SGID bits and default POSIX ACLs to ensure uniform group inheritance without manual intervention.
4. **Engineer Least-Privilege Automation**: Formulate locked-down, passwordless `sudoers` drop-in files for CI/CD runners while eliminating GTFOBins shell-escape risks.
5. **Optimize Boot Performance**: Profile and decompose server startup latencies using `systemd-analyze` to accelerate autoscaling cold-boot times.

---

[<- Back to Linux Track Root](../README.md) | [Next Module: 02-shell ->](../02-shell/README.md)
