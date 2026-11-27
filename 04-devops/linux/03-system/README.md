# Module 03: Linux System Administration & Services

[<- Back to Linux Track Root](../README.md) | [<- Previous Module: 02-shell](../02-shell/README.md) | [Next Module: 04-networking ->](../04-networking/README.md)

---

## 1. Overview & Scope

System administration is the discipline of managing operating system state, supervising daemons, provisioning storage volumes, tuning kernel parameters, and guaranteeing service availability. In the cloud-native era, Linux system administration underpins container worker nodes, virtual machine golden images, and bare-metal server clusters.

Module 03 covers modern Linux administration using modern standards: process lifecycle management, signal propagation, software packaging (`apt` and `dnf`), systemd unit authoring and socket activation, unified journal logging (`journald`), disk partitioning, Logical Volume Management (LVM), memory allocation and swap tuning, runtime kernel tuning with `sysctl`, and enterprise backup and disaster recovery strategies.

---

## 2. Core Architecture: Systemd Supervision & Operating State

```
+---------------------------------------------------------------------------------------+
| SYSTEMD SERVICE SUPERVISION & OS RUNTIME MANAGEMENT                                   |
|                                                                                       |
|   Hardware & Kernel Bootstrap                                                         |
|         │                                                                             |
|         v                                                                             |
|   PID 1 (systemd)                                                                     |
|   ├── Unit Dependency Graph (.target, .service, .socket, .timer, .mount, .path)       |
|   ├── D-Bus IPC Broker                                                                |
|   └── Cgroup Management Slice (/system.slice, /user.slice)                            |
|         │                                                                             |
|         ├──> Web Application Service (`ExecStart=/usr/bin/app`, `Restart=always`)      |
|         │    ├── Cgroup Resource Limits: `MemoryMax=2G`, `CPUQuota=200%`              |
|         │    └── Security Sandbox: `ProtectSystem=strict`, `NoNewPrivileges=yes`      |
|         │                                                                             |
|         ├──> Unified Journal Daemon (`systemd-journald`)                             |
|         │    └── Structured, indexed binary log storage (`/var/log/journal/`)         |
|         │                                                                             |
|         └──> Timers & Schedulers (`systemd.timer` replaces legacy cron)               |
+---------------------------------------------------------------------------------------+
```

---

## 3. Module Notes Catalog

| # | Note | Level | Time | Key Concepts & Topics |
| :---: | :--- | :---: | :---: | :--- |
| **01** | [Processes, Signals & Lifecycle Management](./01-processes-and-signals.md) | Intermediate | 55m | `ps`, `top`, `htop`, process states (R, S, D, Z, T), PID allocation, signal handling (`SIGTERM`, `SIGKILL`, `SIGHUP`), orphan & zombie triage |
| **02** | [Job Control, Priorities & Nice Levels](./02-job-control-and-nice.md) | Intermediate | 45m | Foreground/background jobs (`&`, `fg`, `bg`, `jobs`), `nohup`, process scheduling priority (`nice`, `renice`), Real-Time priorities (`chrt`) |
| **03** | [Package Management: APT, DNF & Repositories](./03-package-management.md) | Intermediate | 55m | Debian `apt`/`dpkg`, RHEL `dnf`/`rpm`, repository configuration, GPG signing keys, package pin priorities, unattended security upgrades |
| **04** | [Systemd & Service Management](./04-systemd-and-services.md) | Intermediate | 60m | PID 1 architecture, unit types (`.service`, `.target`), lifecycle commands (`systemctl`), unit files, restart policies, sandboxing directives |
| **05** | [Journald, Syslog & Centralized Logging](./05-journald-and-logging.md) | Intermediate | 50m | `systemd-journald`, binary logs, `journalctl` filtering (`-u`, `-f`, `-p`, `--since`), log persistence, `rsyslog`, logrotate policies |
| **06** | [Automated Scheduling: Cron & Systemd Timers](./06-cron-and-timers.md) | Intermediate | 50m | Legacy cron syntax (`/etc/crontab`), anacron, `systemd.timer` units, `OnCalendar`, monotonic timers (`OnBootSec`), persistent timers |
| **07** | [Storage, Filesystems & Block Devices](./07-storage-and-filesystems.md) | Intermediate | 60m | Block devices (`lsblk`, `fdisk`, `parted`), GPT vs MBR, filesystems (ext4, XFS, Btrfs), inode vs block exhaustion, filesystem repair (`fsck`) |
| **08** | [Mounting, /etc/fstab & Disk Quotas](./08-mounting-fstab-and-disks.md) | Intermediate | 50m | Mounting & unmounting (`mount`, `umount`), `/etc/fstab` structure, UUIDs vs device names, mount options (`noatime`, `nodev`, `nosuid`), systemd auto-mounts |
| **09** | [Logical Volume Management (LVM) & Software RAID](./09-lvm-and-raid.md) | Advanced | 65m | Physical Volumes (PV), Volume Groups (VG), Logical Volumes (LV), online resizing (`lvextend`, `resize2fs`, `xfs_growfs`), LVM snapshots, RAID 0/1/5 (`mdadm`) |
| **10** | [Memory Management, Swap & OOM Killer](./10-swap-and-memory-management.md) | Advanced | 55m | Virtual memory, page cache vs anonymous memory, swap space & zram, `vm.swappiness`, Out-of-Memory (OOM) killer scoring (`oom_score_adj`) |
| **11** | [Kernel Modules & Sysctl Runtime Tuning](./11-kernel-modules-and-sysctl.md) | Advanced | 55m | Dynamic kernel modules (`lsmod`, `modprobe`, `rmmod`), module parameters, `/etc/sysctl.conf`, networking & virtual memory kernel parameters |
| **12** | [System Time, NTP & Chrony](./12-time-and-ntp.md) | Intermediate | 45m | Hardware clock (RTC) vs system clock, UTC standard, `timedatectl`, Network Time Protocol (NTP), `chrony` vs `systemd-timesyncd`, leap seconds |
| **13** | [System Identification: Hostnames, Locales & Timezones](./13-locale-timezone-and-hostnames.md) | Beginner-Int | 40m | `hostnamectl`, `/etc/hosts`, cloud hostname persistence, locale configurations (`localectl`, `UTF-8`), timezone symlinks |
| **14** | [Archiving & Compression: Tar, Gzip, Xz & Zstd](./14-archiving-and-compression.md) | Beginner-Int | 45m | Archiving with `tar`, compression algorithms compared (gzip vs bzip2 vs xz vs zstd), compression ratios vs CPU throughput, streaming archives |
| **15** | [Enterprise Backup & Restore Strategies](./15-backups-and-restore-strategies.md) | Intermediate | 55m | 3-2-1 backup rule, full vs differential vs incremental backups, snapshot backups (LVM/Btrfs), remote synchronization (`rsync`), restic, disaster recovery |

---

## 4. Key Milestones & Learning Objectives

By completing Module 03, you will be able to:
1. **Supervise Production Microservices**: Author robust, secure systemd unit files with process isolation, automated restarts, and resource constraints.
2. **Expand Storage Live**: Expand virtual disk capacity on a running production server using LVM without downtime or data corruption.
3. **Prevent & Mitigate OOM Crashes**: Tune kernel memory behavior, configure appropriate swap buffers, and shield critical database engines from kernel OOM termination.
4. **Transition to Modern Timers**: Migrate legacy cron jobs to systemd timers with built-in logging, failure alerting, and dependency control.
5. **Tune Kernel Subsystems**: Optimize kernel TCP network buffers, file handle limits, and memory commit heuristics via `/etc/sysctl.d/`.
