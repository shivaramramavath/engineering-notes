# Linux Track Changelog

All notable changes, restructuring, file movements, and additions to the Linux curriculum track are documented in this file.

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
