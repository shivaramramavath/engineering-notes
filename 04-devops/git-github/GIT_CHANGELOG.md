# Git & GitHub Track Changelog

All notable changes, restructuring, file movements, and additions to the Git & GitHub curriculum track are documented in this file.

---

## [Unreleased] - 2026-11-25

### Governance & Baseline Documentation
- Created `GIT_FACTS.md`: Technical facts, version baselines (Git 2.49+, GitHub Actions v4 actions, Node 20 runtime, Artifacts v4 immutability).
- Created `GIT_AUDIT.md`: Line-by-line audit and gap analysis of legacy 43 markdown files.
- Created `GIT_COMPLETION_PLAN.md`: Full 12-module curriculum architecture and file mapping plan.
- Created `GIT_DECISIONS.md`: Architecture Decision Records (ADRs 001–007).
- Created `GIT_PROGRESS.md`: Real-time tracking checklist for all 124 notes and 12 module READMEs.

### Planned Restructuring
- Rename all 5 legacy `snake_case` folders to standard `NN-kebab-case`:
  - `00_setup` $\rightarrow$ `00-setup`
  - `01_git_fundamentals` $\rightarrow$ `01-fundamentals`
  - `02_git_commands` $\rightarrow$ `02-commands`
  - `03_history_rewriting` $\rightarrow$ `03-history-rewriting`
  - `04_github_actions` $\rightarrow$ `04-github-actions`
- Relocate `04_github_actions/CHEATSHEET.md` $\rightarrow$ `11-cheatsheets/04-github-actions-syntax.md`.
- Embed 5 YAML files from `04_github_actions/examples/` into matching markdown notes and delete the raw YAML files.
- Author 7 new modules: `05-github-collaboration`, `06-workflows-and-strategies`, `07-github-security`, `08-gh-cli-and-api`, `09-projects`, `10-interview`, `11-cheatsheets`.
