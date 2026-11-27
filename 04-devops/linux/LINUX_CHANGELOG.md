# Linux Track Changelog

All notable changes, restructuring, file movements, and additions to the Linux curriculum track are documented in this file.

---

## [1.0.0] - 2026-11-26 - Complete Curriculum Delivery

### Major Milestones Completed (100% - 102/102 Files Delivered)
- **Module 01: Core Fundamentals (`01-fundamentals/`)**: 11 notes + 1 module README. Completed 13-section concept format covering boot processes, architecture, filesystems, and permissions.
- **Module 02: Shell & Command Line Mastery (`02-shell/`)**: 11 notes + 1 module README. Completed shell mechanics, pipes, expansions, jq, regex, defensive bash scripting, tmux/vim, modern CLI.
- **Module 03: System Administration & Services (`03-system/`)**: 15 notes + 1 module README. Completed process supervision, systemd units, storage/LVM/swap, logrotate/journald, locales, archiving, and backup strategies.
- **Module 04: Linux Networking & Remote Access (`04-networking/`)**: 11 notes + 1 module README. Completed iproute2, routing, DNS, SSH hardening, UFW/nftables firewalls, tcpdump, ss, OpenSSL TLS, and WireGuard.
- **Module 05: Production Engineering, Security & Troubleshooting (`05-production/`)**: 11 notes + 1 module README. Completed server hardening, sudoers policies, SELinux/AppArmor, auditing/fail2ban, troubleshooting methodologies, performance, monitoring, Ansible, patching, and capacity planning.
- **Module 06: Advanced Systems & Kernel Mechanics (`06-advanced/`)**: 8 notes + 1 module README. Authored containers from scratch (`unshare`, `pivot_root`, cgroups v2), advanced debugging (`strace`, `ltrace`, GDB core dumps), `perf` and flamegraphs, eBPF & `bpftrace`, 100GbE kernel/network tuning, Linux capabilities/Seccomp, and Ftrace/SystemTap.
- **Module 07: Production Reference Projects & Labs (`07-projects/`)**: 8 production projects + 1 module README. Delivered CIS server hardening script, enterprise backup pipeline with GFS rotation, Awk/Jq log analyzer, hardened systemd service & timer with watchdog, mini container runtime from scratch, production Nginx reverse proxy with zero-downtime reloads, SRE disk/OOM troubleshooting labs, and Prometheus Node Exporter with custom textfile collectors.
- **Module 08: Technical Interview Preparation & Scenario Q&A (`08-interview/`)**: 8 notes + 1 module README. 155+ scenario questions across Linux architecture, filesystems, shell, systemd, networking, performance, security, and senior SRE behavioral system design.
- **Module 09: High-Density Production Cheatsheets (`09-cheatsheets/`)**: 9 cheatsheets + 1 module README. Delivered operational task lookups, permissions/users, bash syntax, text tools, systemd/journalctl, networking/ss/tcpdump, emergency incident flowcharts, sysctl quick ref, and package managers Rosetta stone.
- **Track Root (`04-devops/linux/README.md`)**: Full track roadmap, sitemap, study tracks, and verification matrix.

---

## [Unreleased] - 2026-11-25

### Governance & Baseline Documentation
- Created `LINUX_FACTS.md`: Technical facts, version baselines (Ubuntu 24.04 LTS, RHEL 9, Kernel 6.8, systemd 255, cgroup v2, nftables).
- Created `LINUX_AUDIT.md`: Line-by-line audit and gap analysis of legacy 24 markdown files.
- Created `LINUX_COMPLETION_PLAN.md`: Full 10-module curriculum architecture and file mapping plan.
- Created `LINUX_DECISIONS.md`: Architecture Decision Records (ADRs 001–006).
- Created `LINUX_PROGRESS.md`: Real-time tracking checklist for all 92 notes and 10 module/track READMEs.

### Planned Restructuring & Renames
- Decommission `reference/` directory:
  - `reference/interview.md` $\rightarrow$ split and expanded into `08-interview/` (8 notes + README).
  - `reference/cheatsheet.md` $\rightarrow$ split and expanded into `09-cheatsheets/` (9 cheatsheets + README).
- Renumber and expand existing notes across `01-fundamentals/`, `02-shell/`, `03-system/`, `04-networking/`, `05-production/`, and `06-advanced/`.
- Author 8 production reference projects under `07-projects/`.
