# Linux Track Architecture Decision Records (ADRs)

**Date:** 2026-11-25  
**Scope:** `04-devops/linux/`  

---

## ADR-001: Distribution Baseline Strategy
- **Context**: Linux administration commands vary across distributions (e.g., package managers, network tools, and security modules).
- **Decision**: Designate **Ubuntu 24.04 LTS (Noble Numbat)** as the primary baseline distribution, paired with **RHEL 9 / Rocky Linux 9 / AlmaLinux 9** as the secondary enterprise standard.
- **Consequences**: Every note must document both ecosystems (`apt` vs `dnf`, `ufw` vs `firewalld`, `AppArmor` vs `SELinux`). Outdated Debian/SysV init conventions are replaced with standard systemd.

---

## ADR-002: Deprecation of Legacy Networking Tools
- **Context**: `ifconfig`, `netstat`, `route`, and `arp` have been unmaintained for over a decade, yet remain common in legacy tutorials.
- **Decision**: Strictly prohibit teaching `net-tools` as primary tools. All networking notes must use `iproute2` (`ip address`, `ip route`, `ip link`, `ss`) and explain why Netlink socket access makes `ss` superior to `/proc/net/` scanning.
- **Consequences**: Outdated advice flagged in audit is completely corrected in rewritten notes.

---

## ADR-003: Modern Firewall Backend Standard
- **Context**: `iptables` has been replaced by `nftables` in both Ubuntu 22.04+/24.04 and RHEL 9.
- **Decision**: Present `nftables` as the modern core packet filtering engine. Treat `ufw` (Ubuntu) and `firewalld` (RHEL) as user-friendly frontends, and clearly document `iptables-nft` translation layers.
- **Consequences**: Developers understand the in-kernel bytecode VM architecture of nftables rather than treating firewalls as magic black boxes.

---

## ADR-004: Decommissioning `reference/` in Favor of Numbered Modules
- **Context**: `reference/cheatsheet.md` and `reference/interview.md` violate the `NN-kebab-case` naming rule and lack modular granularity.
- **Decision**: Decommission `reference/`. Split and expand `reference/interview.md` into 8 specialized Q&A notes under `08-interview/`. Split and expand `reference/cheatsheet.md` into 9 high-density cheatsheets under `09-cheatsheets/`.
- **Consequences**: Full consistency with the 12-module structure across the `04-devops/` area.

---

## ADR-005: Markdown-Only File Invariant
- **Context**: Previous repository structures sometimes placed loose shell scripts or configs in subfolders.
- **Decision**: All configurations, shell scripts, unit files, and code must reside strictly within fenced code blocks inside markdown `.md` files. Zero loose scripts or raw config files in repository folders.
- **Consequences**: Clean repository tree; eliminates execution or compilation issues in repository root.

---

## ADR-006: Structure of Reference Projects (Module 07)
- **Context**: DevOps reference projects need realistic implementations without polluting repo state.
- **Decision**: All 8 projects in `07-projects/` adhere to the 5-section reference project template with simulated execution logs from isolated containers/sandboxes.
- **Consequences**: Students and engineers receive production-ready code with realistic expected outputs.
