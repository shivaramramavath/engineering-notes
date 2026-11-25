# Git & GitHub Track Changelog

All notable changes, restructuring, file movements, and additions to the Git & GitHub curriculum track are documented in this file.

---

## [2.0.0] - 2026-11-25

### Curriculum Completion (100% Delivered)
- **Total Notes Delivered**: 124 in-depth concept & reference project notes across 12 modules.
- **Total Module Guides**: 12 comprehensive module READMEs + 1 track root README.
- **Total Deliverables**: 137 markdown documentation files.

### Restructuring & Normalization
- Renamed all legacy `snake_case` directories to standard `NN-kebab-case`:
  - `00_setup/` $\rightarrow$ `00-setup/`
  - `01_git_fundamentals/` $\rightarrow$ `01-fundamentals/`
  - `02_git_commands/` $\rightarrow$ `02-commands/`
  - `03_history_rewriting/` $\rightarrow$ `03-history-rewriting/`
  - `04_github_actions/` $\rightarrow$ `04-github-actions/`
- Renamed all legacy filenames to standard `NN-kebab-case.md` format.
- Relocated `04_github_actions/CHEATSHEET.md` $\rightarrow$ `11-cheatsheets/04-github-actions-syntax.md`.
- Folded all 5 raw YAML files in `04_github_actions/examples/` into concept notes and safely deleted the raw files.

### Module Implementations
- **Module 00: Setup & Configuration**: 5 notes + README (SSH/GPG signing, gitconfig, gitignore, gitattributes).
- **Module 01: Core Fundamentals**: 10 notes + README (Three trees, blobs, trees, commits, packfiles, SHA-256).
- **Module 02: Essential Commands**: 21 notes + README (From init/clone to bisect, worktrees, LFS, submodules, and hooks).
- **Module 03: History Rewriting & Recovery**: 8 notes + README (Interactive rebase, reflog, git-filter-repo, reset vs revert vs restore).
- **Module 04: GitHub Actions CI/CD**: 26 notes + README (Comprehensive CI/CD reference, matrix, caching, reusable workflows, OIDC, provenance).
- **Module 05: GitHub Collaboration & Governance**: 11 notes + README (PR lifecycle, Rulesets, CODEOWNERS, issue forms, GHCR, RBAC).
- **Module 06: Workflows, Branching & Release Strategies**: 8 notes + README (Trunk-Based Development, SemVer, Conventional Commits, Feature Flags, Monorepos).
- **Module 07: GitHub Enterprise Security & Compliance**: 8 notes + README (Push protection, CodeQL SAST, Dependabot, SBOM, Sigstore, leaked secret runbook).
- **Module 08: GitHub CLI & Developer APIs**: 4 notes + README (gh CLI, REST v3, GraphQL v4, webhooks, GitHub Apps).
- **Module 09: Production Reference Projects**: 8 complete projects + README (5-section template with runnable code, scripts, and verification).
- **Module 10: Technical Interview Preparation**: 8 notes + README (140+ Q&As, disaster troubleshooting, architectural trade-offs, Staff behavioral cases).
- **Module 11: Quick Reference Cheatsheets**: 7 cheatsheets + README (Commands, undo matrix, branching, actions syntax, gh CLI, config & aliases, errors & fixes).

### Integration & Verification
- Updated `04-devops/README.md`, `INDEX.md`, `ROADMAP.md`, `GLOSSARY.md`.
- Regenerated root `tree.txt`.
