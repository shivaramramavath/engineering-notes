# Linux Track Completion Plan & Execution Roadmap

**Date:** 2026-11-25  
**Curriculum Scope:** `04-devops/linux/`  
**Target:** 92 Concept, Project & Cheatsheet Notes + 9 Module READMEs + 1 Track Root README = **102 Files**  

---

## 1. Structural Migration & Rename Map

| Old Path | New Path | Action |
|:---|:---|:---|
| `01-fundamentals/01-getting-started.md` | `01-fundamentals/01-getting-started.md` | In-place Rewrite to 13-Section Template |
| *(New)* | `01-fundamentals/02-linux-architecture.md` | Author New Note |
| `01-fundamentals/02-filesystem-and-navigation.md` | `01-fundamentals/03-filesystem-hierarchy-and-navigation.md` | `git mv` & Rewrite |
| `01-fundamentals/03-working-with-files.md` | `01-fundamentals/04-working-with-files.md` | `git mv` & Rewrite |
| *(New)* | `01-fundamentals/05-links-inodes-and-file-types.md` | Author New Note |
| `01-fundamentals/04-users-and-groups.md` | `01-fundamentals/06-users-and-groups.md` | `git mv` & Rewrite |
| `01-fundamentals/05-permissions.md` | `01-fundamentals/07-permissions.md` | `git mv` & Rewrite |
| *(New)* | `01-fundamentals/08-sudo-and-privilege.md` | Author New Note |
| *(New)* | `01-fundamentals/09-man-pages-and-getting-help.md` | Author New Note |
| *(New)* | `01-fundamentals/10-environment-variables-and-startup-files.md` | Author New Note |
| *(New)* | `01-fundamentals/11-boot-process.md` | Author New Note |
| `02-shell/01-shell-basics.md` | `02-shell/01-shell-basics.md` | In-place Rewrite to 13-Section Template |
| *(New)* | `02-shell/02-pipes-redirection-and-file-descriptors.md` | Author New Note |
| *(New)* | `02-shell/03-globbing-quoting-and-expansion.md` | Author New Note |
| `02-shell/02-text-processing.md` | `02-shell/04-text-processing.md` | `git mv` & Rewrite |
| *(New)* | `02-shell/05-regular-expressions.md` | Author New Note |
| *(New)* | `02-shell/06-jq-and-structured-data.md` | Author New Note |
| `02-shell/03-bash-scripting.md` | `02-shell/07-bash-scripting.md` | `git mv` & Rewrite |
| *(New)* | `02-shell/08-bash-scripting-advanced.md` | Author New Note |
| *(New)* | `02-shell/09-shellcheck-and-script-quality.md` | Author New Note |
| `02-shell/04-vim-and-tmux.md` | `02-shell/10-vim-and-tmux.md` | `git mv` & Rewrite |
| *(New)* | `02-shell/11-dotfiles-and-productivity.md` | Author New Note |
| `03-system/01-processes-and-signals.md` | `03-system/01-processes-and-signals.md` | In-place Rewrite to 13-Section Template |
| *(New)* | `03-system/02-job-control-and-nice.md` | Author New Note |
| `03-system/02-package-management.md` | `03-system/03-package-management.md` | `git mv` & Rewrite |
| `03-system/03-systemd-and-services.md` | `03-system/04-systemd-and-services.md` | `git mv` & Rewrite |
| *(New)* | `03-system/05-journald-and-logging.md` | Author New Note |
| `03-system/04-cron-and-timers.md` | `03-system/06-cron-and-timers.md` | `git mv` & Rewrite |
| `03-system/05-storage-and-filesystems.md` | `03-system/07-storage-and-filesystems.md` | `git mv` & Rewrite |
| *(New)* | `03-system/08-mounting-fstab-and-disks.md` | Author New Note |
| *(New)* | `03-system/09-lvm-and-raid.md` | Author New Note |
| *(New)* | `03-system/10-swap-and-memory-management.md` | Author New Note |
| *(New)* | `03-system/11-kernel-modules-and-sysctl.md` | Author New Note |
| *(New)* | `03-system/12-time-and-ntp.md` | Author New Note |
| *(New)* | `03-system/13-locale-timezone-and-hostnames.md` | Author New Note |
| *(New)* | `03-system/14-archiving-and-compression.md` | Author New Note |
| *(New)* | `03-system/15-backups-and-restore-strategies.md` | Author New Note |
| `04-networking/01-networking-basics.md` | `04-networking/01-networking-basics.md` | In-place Rewrite to 13-Section Template |
| *(New)* | `04-networking/02-ip-routing-and-interfaces.md` | Author New Note |
| *(New)* | `04-networking/03-dns-resolution-and-tools.md` | Author New Note |
| `04-networking/02-ssh.md` | `04-networking/04-ssh.md` | `git mv` & Rewrite |
| `04-networking/03-firewall.md` | `04-networking/05-firewall.md` | `git mv` & Rewrite |
| *(New)* | `04-networking/06-tcpdump-and-wireshark-basics.md` | Author New Note |
| *(New)* | `04-networking/07-curl-wget-netcat-and-socat.md` | Author New Note |
| *(New)* | `04-networking/08-ss-and-port-troubleshooting.md` | Author New Note |
| *(New)* | `04-networking/09-tls-certificates-with-openssl.md` | Author New Note |
| *(New)* | `04-networking/10-load-and-bandwidth-testing.md` | Author New Note |
| *(New)* | `04-networking/11-vpn-wireguard-basics.md` | Author New Note |
| `05-production/01-server-hardening.md` | `05-production/01-server-hardening.md` | In-place Rewrite to 13-Section Template |
| *(New)* | `05-production/02-users-ssh-and-sudo-policies.md` | Author New Note |
| *(New)* | `05-production/03-selinux-and-apparmor.md` | Author New Note |
| *(New)* | `05-production/04-auditing-and-fail2ban.md` | Author New Note |
| `05-production/02-troubleshooting.md` | `05-production/05-troubleshooting.md` | `git mv` & Rewrite |
| `05-production/03-performance.md` | `05-production/06-performance.md` | `git mv` & Rewrite |
| *(New)* | `05-production/07-monitoring-basics.md` | Author New Note |
| *(New)* | `05-production/08-automation-with-ansible-overview.md` | Author New Note |
| *(New)* | `05-production/09-patching-and-unattended-upgrades.md` | Author New Note |
| *(New)* | `05-production/10-disaster-recovery-and-boot-rescue.md` | Author New Note |
| *(New)* | `05-production/11-capacity-planning.md` | Author New Note |
| `06-advanced/01-namespaces-and-cgroups.md` | `06-advanced/01-namespaces-and-cgroups.md` | In-place Rewrite to 13-Section Template |
| *(New)* | `06-advanced/02-containers-from-scratch.md` | Author New Note |
| `06-advanced/02-advanced-debugging.md` | `06-advanced/03-advanced-debugging.md` | `git mv` & Rewrite |
| *(New)* | `06-advanced/04-perf-and-flamegraphs.md` | Author New Note |
| *(New)* | `06-advanced/05-ebpf-and-bpftrace-intro.md` | Author New Note |
| *(New)* | `06-advanced/06-kernel-tuning-and-network-stack-tuning.md` | Author New Note |
| *(New)* | `06-advanced/07-linux-security-modules-and-capabilities.md` | Author New Note |
| *(New)* | `06-advanced/08-systemtap-ftrace-overview.md` | Author New Note |
| *(New)* | `07-projects/` (8 projects + README) | Author 8 Reference Projects |
| `reference/interview.md` | `08-interview/` (8 notes + README) | Split, Expand into 8 Deep Q&A Notes |
| `reference/cheatsheet.md` | `09-cheatsheets/` (9 cheatsheets + README) | Split, Expand into 9 Specialized Cheatsheets |

---

## 2. Execution Batches

1. **Batch 0: Renames, Directory Creation & Baseline Setup**
   - Execute `git mv` renames. Remove legacy `reference/` folder after moving content.
   - Author all 9 module `README.md` files and track root `README.md`.
2. **Batch 1: Module 01 - Fundamentals (Notes 01–06)**
3. **Batch 2: Module 01 - Fundamentals (Notes 07–11)**
4. **Batch 3: Module 02 - Shell & Scripting (Notes 01–06)**
5. **Batch 4: Module 02 - Shell & Scripting (Notes 07–11)**
6. **Batch 5: Module 03 - System Administration (Notes 01–05)**
7. **Batch 6: Module 03 - System Administration (Notes 06–10)**
8. **Batch 7: Module 03 - System Administration (Notes 11–15)**
9. **Batch 8: Module 04 - Networking (Notes 01–06)**
10. **Batch 9: Module 04 - Networking (Notes 07–11)**
11. **Batch 10: Module 05 - Production Engineering (Notes 01–06)**
12. **Batch 11: Module 05 - Production Engineering (Notes 07–11)**
13. **Batch 12: Module 06 - Advanced Systems & Internals (Notes 01–04)**
14. **Batch 13: Module 06 - Advanced Systems & Internals (Notes 05–08)**
15. **Batch 14: Module 07 - Production Reference Projects (Projects 01–04)**
16. **Batch 15: Module 07 - Production Reference Projects (Projects 05–08)**
17. **Batch 16: Module 08 - Technical Interview Preparation (Notes 01–04)**
18. **Batch 17: Module 08 - Technical Interview Preparation (Notes 05–08)**
19. **Batch 18: Module 09 - Quick Reference Cheatsheets (Cheatsheets 01–05)**
20. **Batch 19: Module 09 - Quick Reference Cheatsheets (Cheatsheets 06–09)**
21. **Batch 20: Track Integration, Global Index/Roadmap Updates, tree.txt & Final Report**
