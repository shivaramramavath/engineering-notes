# Linux Systems Engineering & DevOps Curriculum

Welcome to the comprehensive, production-grade Linux curriculum for systems engineers, site reliability engineers (SREs), DevOps practitioners, and backend developers.

Linux is the foundational substrate of the modern computing landscape: powering more than 90% of public cloud infrastructure, container runtimes (Docker, containerd, Kubernetes), high-performance database engines, edge gateways, and automated CI/CD runners. 

This track delivers end-to-end technical mastery from silicon, kernel space (Ring 0), and system call interfaces up to shell programming, systems administration, production networking, security hardening, container internals, hands-on production labs, technical interview preparation, and quick-reference cheatsheets.

---

## 1. Curriculum Architecture & Module Map

The curriculum is structured across 9 progressively sequenced modules comprising **92 detailed notes, reference projects, and cheatsheets**:

```
04-devops/linux/
├── README.md                                 # Track root architectural roadmap (this file)
├── LINUX_COMPLETION_PLAN.md                  # Comprehensive track specification & execution blueprint
├── LINUX_PROGRESS.md                         # Real-time progress and verification tracker
├── LINUX_FACTS.md                            # Verified 2026 technical facts & distribution baselines
├── LINUX_DECISIONS.md                        # Architecture Decision Records (ADRs)
├── LINUX_CHANGELOG.md                        # Revision history and mutation log
├── LINUX_AUDIT.md                            # Quality and consistency audit report
│
├── 01-fundamentals/                         # Core Fundamentals & Kernel Architecture (11 notes)
│   ├── README.md                             # Module 01 overview & mental model
│   └── 01-getting-started.md ... 11-boot-process.md
│
├── 02-shell/                                # Shell, Command Line & Scripting Mastery (11 notes)
│   ├── README.md                             # Module 02 overview & stream processing model
│   └── 01-shell-basics.md ... 11-dotfiles-and-productivity.md
│
├── 03-system/                               # System Administration, Storage & Services (15 notes)
│   ├── README.md                             # Module 03 overview & OS service lifecycle
│   └── 01-processes-and-signals.md ... 15-backups-and-restore-strategies.md
│
├── 04-networking/                           # Networking, Netfilter & Remote Protocols (11 notes)
│   ├── README.md                             # Module 04 overview & network stack mental model
│   └── 01-networking-basics.md ... 11-vpn-wireguard-basics.md
│
├── 05-production/                           # Production Engineering, Hardening & Troubleshooting (11 notes)
│   ├── README.md                             # Module 05 overview & SRE operational methodology
│   └── 01-server-hardening.md ... 11-capacity-planning.md
│
├── 06-advanced/                             # Advanced Internals, Containers & Observability (8 notes)
│   ├── README.md                             # Module 06 overview & kernel observability stack
│   └── 01-namespaces-and-cgroups.md ... 08-systemtap-ftrace-overview.md
│
├── 07-projects/                             # Production Reference Projects & Labs (8 projects)
│   ├── README.md                             # Module 07 overview & project guidelines
│   └── 01-harden-a-fresh-server.md ... 08-monitoring-with-node-exporter.md
│
├── 08-interview/                            # Technical Interview Preparation & Scenario Q&A (8 notes)
│   ├── README.md                             # Module 08 overview & interview framework
│   └── 01-linux-basics-qa.md ... 08-behavioural-and-project-discussion.md
│
└── 09-cheatsheets/                          # High-Density Production Cheatsheets (9 sheets)
    ├── README.md                             # Module 09 overview & command lookup index
    └── 01-commands-by-task.md ... 09-package-managers-compared.md
```

---

## 2. Module Overview Table

| Module | Focus Area | Notes | Target Audience & Competencies |
|:---|:---|:---:|:---|
| [**01-fundamentals**](./01-fundamentals/README.md) | **Core Fundamentals & Kernel** | 11 | Kernel vs user space (Ring 0/3), VFS, FHS, permissions, POSIX ACLs, sudoers, boot stages. |
| [**02-shell**](./02-shell/README.md) | **Shell, CLI & Scripting** | 11 | Streams, file descriptors, quoting, regex, `sed`/`awk`, `jq`, robust bash (`set -euo pipefail`), ShellCheck. |
| [**03-system**](./03-system/README.md) | **System Administration & Services** | 15 | Process lifecycle, signals, systemd units, timers, journald, storage, LVM, swap, sysctl, NTP. |
| [**04-networking**](./04-networking/README.md) | **Networking & Protocols** | 11 | `iproute2` (`ip`/`ss`), routing, DNS (`resolvectl`), OpenSSH, `nftables`/`ufw`, `tcpdump`, WireGuard. |
| [**05-production**](./05-production/README.md) | **Hardening & SRE Operations** | 11 | CIS benchmarks, SELinux/AppArmor, auditd, USE method, panic triage, Ansible, DR strategies. |
| [**06-advanced**](./06-advanced/README.md) | **Advanced Internals & eBPF** | 8 | 8 namespaces, cgroups v2, DIY containers via `unshare`, `perf`, flamegraphs, `bpftrace`, sysctl tuning. |
| [**07-projects**](./07-projects/README.md) | **Production Reference Projects** | 8 | Fully implemented runnable projects: server hardening, automated backups, log parsers, systemd apps, DIY container runtime. |
| [**08-interview**](./08-interview/README.md) | **Technical Interview QA** | 8 | 150+ in-depth scenario questions covering junior to principal SRE/DevOps interview rounds. |
| [**09-cheatsheets**](./09-cheatsheets/README.md) | **High-Density Cheatsheets** | 9 | Fast terminal lookups for commands by task, permissions, shell syntax, text tools, systemd, and networking. |

---

## 3. Distribution Standards Baseline (2026)

All code blocks, system configs, package manager instructions, and command outputs throughout this track align strictly with modern enterprise Linux releases:

- **Primary Reference**: **Ubuntu 24.04 LTS (Noble Numbat)**
  - Linux Kernel 6.8+ (HWE updates)
  - `systemd 255.4`
  - Cgroup Hierarchy: Unified **cgroup v2** (`/sys/fs/cgroup`)
  - Network Backend: Netplan v1.0 + `systemd-networkd`
  - Firewall: `ufw` powered by `nftables`
  - Security Module: **AppArmor 4.0**
- **Secondary Reference**: **Rocky Linux 9 / RHEL 9 / AlmaLinux 9**
  - Linux Kernel 5.14.0 (enterprise backported)
  - Package Management: `dnf v4` / RPM
  - Network: `NetworkManager` (`nmcli`)
  - Firewall: `firewalld` with native `nftables`
  - Security Module: **SELinux** in Enforcing mode

### Deprecated Tools vs Modern Replacements

| Legacy / Deprecated Tool | Modern Standard | Rationale |
|:---|:---|:---|
| `ifconfig`, `route`, `arp` | `ip address`, `ip route`, `ip neigh` | `net-tools` is obsolete; `iproute2` natively integrates with Linux netlink sockets and namespaces. |
| `netstat` | `ss -tulpn` | `netstat` parses slow `/proc/net/` files; `ss` fetches socket statistics directly from kernel Netlink. |
| `iptables-legacy` | `nftables` (`nft`) | In-kernel bytecode engine replaces linear chain evaluations with $O(1)$ set/map lookups. |
| `service <name> start` | `systemctl start <name>.service` | Direct systemd D-Bus communication with process supervision and cgroup isolation. |
| `/etc/init.d/` scripts | `/etc/systemd/system/*.service` | Declarative dependency management, restart policies, and sandboxing directives. |
| System V cron alone | `systemd.timer` units | Unified journal logging, randomized delays (`RandomizedDelaySec`), and monotonic wakeups. |
| `nslookup` | `dig`, `resolvectl query` | Modern DNS queries support DNSSEC, EDNS0, and systemd-resolved integration. |

---

## 4. Cross-Curriculum Integration

This Linux track serves as the systems bedrock across the entire engineering repository:

- **Containers & Docker** (`04-devops/docker/`): Builds upon namespaces (`06-advanced/01`), cgroups v2, and VFS storage drivers.
- **Web Servers & Nginx** (`04-devops/nginx/`): Connects to systemd socket activation, file descriptors, ephemeral port exhaustion (`sysctl`), and TLS termination.
- **Cloud Infrastructure & AWS** (`04-devops/aws/`): Directly underpins AWS EC2 instance bootstrap (`cloud-init`), IAM role federation, EBS volume attachment, and VPC interface routing.
- **Git & GitHub CI/CD** (`04-devops/git-github/`): Informs self-hosted runner configuration, SSH deploy keys, and runner hardening.
- **Backend Languages** (`01-languages/python/`, `01-languages/java/`, `03-backend/nestjs/`): Bridges OS process limits (`ulimit`/`systemd Limits`), signal traps (`SIGTERM`), and containerized runtime optimization.

---

## 5. Note Format & Quality Standard

Every conceptual note in this track follows a rigorous 13-section template:
1. **Metadata Header**: Difficulty, prerequisites, estimated time, and key terminology.
2. **Table of Contents**: Deep markdown anchor links.
3. **1. What & Why**: Architectural justification, enterprise use cases, and cloud contexts.
4. **2. Core Concepts**: Exhaustive mechanical decomposition with ASCII/Unicode diagrams.
5. **3. How It Works Internally**: Kernel structures (`task_struct`, VFS, page tables) and Mermaid workflow diagrams.
6. **4. Syntax & Usage**: Comprehensive flags, commands, and configuration schemas.
7. **5. Practical Examples**: 5 progressive examples (Basic -> Real-World -> Edge Case -> Failure Case -> Production Grade).
8. **6. Best Practices**: Battle-tested industry guidelines.
9. **7. Common Mistakes & Troubleshooting**: Minimum 6-row matrix of errors, causes, and verified fixes.
10. **8. Performance, Security & Cost Implications**: Resource consumption, attack surface, and sizing trade-offs.
11. **9. Comparison Matrix**: Structured trade-off analysis vs alternatives.
12. **10. Real-World Interview Questions**: 8 detailed scenario questions with expandable `<details>` answers.
13. **11. Practice Exercises**: 6 hands-on exercises (2 Easy, 2 Medium, 2 Hard) with full solution code.
14. **12. Summary & Quick Reference**: Dense markdown cheat sheet table.
15. **13. Further Reading**: Canonical links to Linux kernel docs, man pages, and official distro documentation.
