# Linux Track Comprehensive Audit & Gap Analysis

**Date:** 2026-11-25  
**Auditor:** Linux Systems Curriculum Architect & Technical Reviewer  
**Curriculum Scope:** `04-devops/linux/`  

---

## 1. Executive Summary

An exhaustive technical audit was conducted on all 25 existing files in `04-devops/linux/`.
- **Existing Files:** 24 concept/reference notes + 1 empty track `README.md`.
- **Module Overview READMEs:** Zero (all 6 module folders lack a `README.md`).
- **Template Conformance:** 0% (none of the 24 notes adhere to the mandatory 13-section concept template or include the metadata table, 5 structured practical examples, 6+ common mistakes, 8 interview questions in `<details>`, or 6 practice exercises).
- **Average Note Length:** 120–200 lines (significantly below the required 400–700 line target).
- **Outdated Advice & Deprecations:**
  - Scattered usage of deprecated network tools (`netstat`, `ifconfig`, `route`).
  - Reliance on legacy SysV init concepts without proper systemd contrast.
  - Insufficient coverage of modern standards: `nftables` vs legacy `iptables`, cgroup v2 unified hierarchy, Netplan / NetworkManager, modern logging via `systemd-journald`, POSIX ACLs, eBPF, and modern shell ergonomics (`set -euo pipefail`).
- **Unnumbered Reference Directory:** `reference/cheatsheet.md` and `reference/interview.md` violate the `NN-kebab-case` convention and must be expanded, split, and moved into dedicated numbered modules (`08-interview/` and `09-cheatsheets/`).

---

## 2. Detailed Note-by-Note Audit Table

| File Path | Lines | Template Sections | Status | Technical Issues & Gaps |
|:---|:---:|:---:|:---:|:---|
| `README.md` | 0 | 0/13 | Empty | Empty file; needs master track blueprint, architectural map, and cross-links. |
| `01-fundamentals/01-getting-started.md` | 78 | 4/13 | Partial / Needs Rewrite | Lacks 13-section format, WSL2 caveats thin, missing Rocky/Alma baseline, no interview/exercises. |
| `01-fundamentals/02-filesystem-and-navigation.md` | 102 | 4/13 | Partial / Needs Rewrite | FHS coverage is superficial; missing `/sys`, `/proc`, `/run`, `/dev` virtual filesystems deep dive. |
| `01-fundamentals/03-working-with-files.md` | 123 | 5/13 | Partial / Needs Rewrite | Standard file operations; missing file descriptors, sparse files, and atomic file creation (`mktemp`). |
| `01-fundamentals/04-users-and-groups.md` | 113 | 4/13 | Partial / Needs Rewrite | Basic `useradd`/`groupadd`; missing UID ranges (system vs human), `/etc/login.defs`, and PAM overview. |
| `01-fundamentals/05-permissions.md` | 132 | 5/13 | Partial / Needs Rewrite | Lacks POSIX ACLs (`getfacl`/`setfacl`), umask inheritance details, and attribute flags (`chattr +i`). |
| `02-shell/01-shell-basics.md` | 166 | 5/13 | Partial / Needs Rewrite | Missing subshells, job control (`bg`/`fg`/`disown`), environment inheritance, and login vs non-login shells. |
| `02-shell/02-text-processing.md` | 140 | 5/13 | Partial / Needs Rewrite | Covers grep/sed/awk at high level; missing advanced regex, multiline awk, and `xargs -P` concurrency. |
| `02-shell/03-bash-scripting.md` | 208 | 6/13 | Partial / Needs Rewrite | Missing traps (`trap cleanup EXIT`), strict mode breakdown, robust error handling, and `getopts`. |
| `02-shell/04-vim-and-tmux.md` | 167 | 5/13 | Partial / Needs Rewrite | Superficial shortcut lists; lacks `.tmux.conf` production configuration and remote server workflows. |
| `03-system/01-processes-and-signals.md` | 132 | 4/13 | Partial / Needs Rewrite | Missing process states (R, S, D, Z, T), zombie handling, `/proc/[pid]/` anatomy, and OOM killer score. |
| `03-system/02-package-management.md` | 148 | 4/13 | Partial / Needs Rewrite | Compares apt vs dnf; lacks repository configuration (`deb822`), GPG keyrings, pinning, and clean builds. |
| `03-system/03-systemd-and-services.md` | 175 | 5/13 | Partial / Needs Rewrite | Missing systemd drop-in overrides (`systemctl edit`), resource limits in units, and sandboxing directives. |
| `03-system/04-cron-and-timers.md` | 178 | 4/13 | Partial / Needs Rewrite | Compares cron vs timers; lacks calendar event syntax (`OnCalendar`), monotonic timers, and persistence. |
| `03-system/05-storage-and-filesystems.md` | 169 | 5/13 | Partial / Needs Rewrite | Lacks LVM (PV, VG, LV), disk partitioning with `parted`, filesystem tuning (`tune2fs`), and fstab flags. |
| `04-networking/01-networking-basics.md` | 169 | 5/13 | Partial / Needs Rewrite | Mentions `ip`; lacks modern Netplan, NetworkManager keyfile formats, and routing policy databases. |
| `04-networking/02-ssh.md` | 178 | 5/13 | Partial / Needs Rewrite | Good basic coverage; lacks certificate-based SSH, modern ed25519-sk hardware keys, and bastion tunnels. |
| `04-networking/03-firewall.md` | 162 | 5/13 | Partial / Needs Rewrite | Treats ufw/firewalld; lacks deep `nftables` VM architecture, tables/chains/rules, and set optimizations. |
| `05-production/01-server-hardening.md` | 154 | 5/13 | Partial / Needs Rewrite | Generic checklist; lacks CIS benchmark specifics, AppArmor vs SELinux enforcement, and auditd policies. |
| `05-production/02-troubleshooting.md` | 169 | 6/13 | Partial / Needs Rewrite | Needs systematic flowcharts: CPU saturation, memory exhaustion, disk full (inodes vs blocks), network loss. |
| `05-production/03-performance.md` | 183 | 5/13 | Partial / Needs Rewrite | Needs Brendan Gregg's USE method, vmstat/iostat breakdown, sar accounting, and kernel sysctl tuning. |
| `06-advanced/01-namespaces-and-cgroups.md` | 208 | 5/13 | Partial / Needs Rewrite | Good starting point; needs cgroups v2 controller breakdown (memory.max, cpu.weight), and `unshare` scripts. |
| `06-advanced/02-advanced-debugging.md` | 206 | 5/13 | Partial / Needs Rewrite | Covers strace/ltrace; lacks eBPF (`bpftrace`), perf profiling, core dump generation, and flamegraphs. |
| `reference/cheatsheet.md` | 378 | N/A | Needs Split & Relocate | Flat unnumbered reference list; must be decomposed into 9 targeted cheatsheets in `09-cheatsheets/`. |
| `reference/interview.md` | 311 | N/A | Needs Split & Relocate | Contains ~20 high-level Q&As; must be expanded to 140+ Q&As across 8 specialized notes in `08-interview/`. |

---

## 3. Curriculum Gaps Identified

The following mandatory enterprise topics are completely missing from the existing notes:
1. **Boot Process**: BIOS vs UEFI, GRUB2 configuration, initramfs/initrd, and systemd target transitions.
2. **File Inodes & Links**: Hard links vs symbolic links, inode exhaustion vs block exhaustion, directory links restriction.
3. **Privilege & Sudo Architecture**: `/etc/sudoers` syntax, alias definitions, `sudo -i` vs `su -`, memory-safe `sudo-rs`.
4. **Structured Data in Shell**: `jq` for JSON manipulation, `yq` for YAML, stream editing with `tr` and `xargs`.
5. **Logging & Journald**: Binary journal mechanics, `journalctl` query filtering, `logrotate`, and rsyslog forwarding.
6. **LVM & Storage Subsystems**: Physical Volumes, Volume Groups, Logical Volumes, snapshots, resizing, and RAID levels.
7. **Time Synchronization**: Network Time Protocol (NTP), `systemd-timesyncd`, `chrony`, and hardware RTC clocks.
8. **Network Packet Tracing**: `tcpdump` packet capture, BPF filters, Wireshark integration, and TLS inspection with OpenSSL.
9. **Port Diagnostics & Sockets**: Advanced `ss` flags, TCP state transitions, socket buffers, and listening backlogs.
10. **VPN & WireGuard**: Kernel WireGuard interface configuration, public/private key generation, peer routing.
11. **Production Reference Projects**: Zero projects currently exist. Need 8 hands-on reference projects in `07-projects/`.
12. **Systematic Cheatsheets**: Need 9 standardized cheatsheets in `09-cheatsheets/`.
