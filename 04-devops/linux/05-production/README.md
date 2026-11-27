# Module 05: Production Engineering, Security Hardening & Troubleshooting

[<- Back to Linux Track Root](../README.md) | [<- Previous Module: 04-networking](../04-networking/README.md) | [Next Module: 06-advanced ->](../06-advanced/README.md)

---

## 1. Overview & Scope

Production systems demand a fundamentally different operational posture than local development environments. In production, systems must resist targeted adversary attacks, recover automatically from component degradation, log forensic evidence of every privileged action, and withstand unpredictable traffic surges.

Module 05 covers the rigorous operational discipline required to run Linux fleets in production. You will explore Center for Internet Security (CIS) server hardening benchmarks, Mandatory Access Control (MAC) via AppArmor and SELinux, intrusion detection and automated banning (`fail2ban`), Linux auditing (`auditd`), systematic incident triage methodologies (USE method, RED method), performance bottleneck diagnosis, configuration automation with Ansible, automated security patching, and disaster recovery.

---

## 2. Core Architecture: Defensive Security In-Depth & Observability

```
+---------------------------------------------------------------------------------------+
| DEFENSE-IN-DEPTH PRODUCTION ARCHITECTURE                                              |
|                                                                                       |
|   Untrusted Internet Traffic / Adversary                                              |
|         │                                                                             |
|         v                                                                             |
|   [ Network Boundary ] ─────────> Packet Filter (nftables / UFW, fail2ban drops)      |
|         │                                                                             |
|         v                                                                             |
|   [ Transport & Authentication ] -> SSH Ed25519 Keys, No Root Login, PAM Duo 2FA      |
|         │                                                                             |
|         v                                                                             |
|   [ Discretionary Access ] ─────> POSIX Mode Bits (0600/0750), Strict Umask 027       |
|         │                                                                             |
|         v                                                                             |
|   [ Mandatory Access Control ] ──> AppArmor / SELinux (Confines Compromised Daemons)   |
|         │                                                                             |
|         v                                                                             |
|   [ Kernel Subsystem ] ─────────> Capabilities (drop CAP_SYS_ADMIN), seccomp-bpf       |
|         │                                                                             |
|         v                                                                             |
|   [ Forensic Audit Engine ] ────> auditd (Kernel syscall audit trail to /var/log/audit)
+---------------------------------------------------------------------------------------+
```

---

## 3. Module Notes Catalog

| # | Note | Level | Time | Key Concepts & Topics |
| :---: | :--- | :---: | :---: | :--- |
| **01** | [Server Hardening: CIS Benchmarks & Security Baselines](./01-server-hardening.md) | Intermediate | 60m | CIS Benchmark Level 1 & 2, attack surface reduction, disabling unused protocols/filesystems, sysctl network hardening, Lynis audits |
| **02** | [Users, SSH & Sudo Policies in Production](./02-users-ssh-and-sudo-policies.md) | Intermediate | 55m | Enterprise identity lifecycle, password hashing algorithms (SHA-512, yescrypt), sudoer drop-in policies, SSH key rotation, bastion architecture |
| **03** | [Mandatory Access Control: SELinux & AppArmor](./03-selinux-and-apparmor.md) | Advanced | 65m | DAC vs MAC, AppArmor profiles (enforce vs complain), `aa-status`, SELinux contexts (user:role:type:level), `semanage`, `setsebool`, `audit2allow` |
| **04** | [Auditing & Intrusion Detection: Auditd & Fail2ban](./04-auditing-and-fail2ban.md) | Intermediate | 60m | Linux Audit Daemon (`auditd`), audit rules (`auditctl`), `ausearch`, `aureport`, Fail2ban jail configurations, regex filters, nftables ban actions |
| **05** | [Production Troubleshooting & Incident Response](./05-troubleshooting.md) | Advanced | 65m | Brendan Gregg's USE Method, 60-second triage checklist (`uptime`, `dmesg`, `vmstat`, `iostat`), kernel oops, I/O hung task recovery |
| **06** | [System Performance Tuning & Profiling](./06-performance.md) | Advanced | 65m | CPU bottlenecks, memory pressure (`psi` - Pressure Stall Information), storage I/O queues, network saturation, tuned profiles |
| **07** | [Monitoring Basics: Metrics, Exporters & Logs](./07-monitoring-basics.md) | Intermediate | 55m | Push vs pull metrics, Prometheus Node Exporter architecture, key OS metrics (load, memory, disk I/O, network errors), systemd-journal-remote |
| **08** | [Configuration Automation: Ansible Overview](./08-automation-with-ansible-overview.md) | Intermediate | 55m | Infrastructure as Code for OS configuration, agentless SSH execution, Ansible playbooks, idempotency, managing users/firewalls/packages |
| **09** | [Patching & Unattended Security Upgrades](./09-patching-and-unattended-upgrades.md) | Intermediate | 50m | Zero-downtime maintenance, `unattended-upgrades` (Ubuntu), `dnf-automatic` (RHEL), kernel live patching (`kpatch`/Canonical Livepatch), reboot handling |
| **10** | [Disaster Recovery & Boot Rescue Operations](./10-disaster-recovery-and-boot-rescue.md) | Advanced | 60m | GRUB rescue mode, initramfs emergency shell, recovering corrupted `/etc/fstab`, chroot from Live USB, root password recovery, fsck repair |
| **11** | [Capacity Planning & Hardware Sizing](./11-capacity-planning.md) | Advanced | 50m | Resource trend modeling, sizing CPU/RAM/IOPS for databases and app tiers, cloud instance selection (compute vs memory vs storage optimized), headroom math |

---

## 4. Key Milestones & Learning Objectives

By completing Module 05, you will be able to:
1. **Apply Automated CIS Hardening**: Harden a raw cloud image to pass CIS Level 1 benchmarks by eliminating legacy protocols, locking permissions, and configuring kernel defenses.
2. **Confine Breached Processes with MAC**: Author and enforce AppArmor and SELinux profiles that prevent compromised web applications from traversing the host filesystem.
3. **Resolve Critical Incidents Under Pressure**: Execute a systematic 60-second diagnostic triage during production outages to isolate CPU starvation, memory leaks, and I/O lockups.
4. **Automate Continuous Compliance**: Enforce OS configuration state across multi-node server clusters using idempotent Ansible automation.
5. **Recover Broken Systems**: Rescue unbootable Linux instances using GRUB rescue targets, initramfs chroot environments, and offline filesystem repair.
