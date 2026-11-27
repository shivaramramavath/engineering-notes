# Module 09: High-Density Linux Production Cheatsheets

[<- Back to Linux Track Root](../README.md) | [<- Previous Module: 08-interview](../08-interview/README.md)

---

## 1. Overview & Scope

When incident alerts trigger in production, engineers need immediate, syntax-accurate command references rather than verbose conceptual explanations. Module 09 delivers nine high-density, battle-tested cheatsheets engineered for fast terminal lookups during on-call rotations, outage triage, server provisioning, and performance tuning.

Every cheatsheet is organized by operational goal, cross-referencing command options, exit states, and real-world examples.

---

## 2. Cheatsheet Catalog

| # | Cheatsheet | Domain | Primary Quick-Lookup Content |
| :---: | :--- | :---: | :--- |
| **01** | [Linux Commands by Operational Task](./01-commands-by-task.md) | Universal | File ops, search (`find`/`locate`), compression, system info, user management, disk inspection. |
| **02** | [Permissions, Users & Access Control Reference](./02-permissions-and-users.md) | Security | Octal & symbolic permissions, `chmod`, `chown`, `chgrp`, umask cheat table, SUID/SGID/Sticky bits, `setfacl`/`getfacl`. |
| **03** | [Shell, Bash & Scripting Syntax Reference](./03-shell-and-bash.md) | Scripting | Parameter expansions (`${var:-def}`), array syntax, test conditions (`[[ ... ]]`), loops, exit traps, regex operators. |
| **04** | [Text Processing Fast Reference: Awk, Sed & Grep](./04-text-tools.md) | Text | `grep` regex flags, `sed` search-and-replace one-liners, `awk` columns, field separators, associative arrays, `cut`, `sort`, `uniq`. |
| **05** | [Systemd & Journalctl Operations Cheatsheet](./05-systemd-and-journalctl.md) | Services | `systemctl` lifecycle commands, unit creation syntax, dependency directives, `journalctl` time/unit/priority filters, log clearing. |
| **06** | [Linux Networking & Port Troubleshooting Reference](./06-networking-commands.md) | Networking | `ip address`, `ip route`, `ip link`, `ss -tulpn` socket filters, `tcpdump` filters, `curl` timing format, `dig` queries, UFW/nftables rules. |
| **07** | [Troubleshooting & Emergency Incident Flowcharts](./07-troubleshooting-flowcharts.md) | SRE Ops | Flowchart-driven emergency runbooks: High CPU, Out-of-Memory / Swap Thrashing, Disk Space Full, Network Connection Drops. |
| **08** | [Kernel Tuning & Sysctl Quick Reference](./08-kernel-and-sysctl-quick-ref.md) | Performance | Key `/etc/sysctl.conf` parameters: virtual memory (`swappiness`, `dirty_ratio`), network (`somaxconn`, `tcp_max_syn_backlog`, `bbr`), IPC limits. |
| **09** | [Package Managers Comparison: APT, DNF, Pacman & APK](./09-package-managers-compared.md) | Systems | Rosetta Stone comparison table of package operations across Ubuntu/Debian (`apt`), RHEL/Rocky (`dnf`), Arch (`pacman`), and Alpine (`apk`). |
