# Git & GitHub Engineering Curriculum

[<- Back to DevOps Area Overview](../README.md) | [Track Progress](./GIT_PROGRESS.md) | [Audit Report](./GIT_AUDIT.md) | [Completion Plan](./GIT_COMPLETION_PLAN.md)

---

## Track Overview

Welcome to the **Git & GitHub Engineering Curriculum**. This learning track is an exhaustive, production-grade reference system designed to take software engineers, DevOps specialists, and Site Reliability Engineers from foundational version control concepts to advanced internal graph data structures, enterprise collaboration governance, automated CI/CD pipelines via GitHub Actions, DevSecOps supply chain security, and Staff-level disaster recovery.

Every note in this curriculum follows rigorous production standards: conceptual foundations, internal graph mechanics, ASCII architectural diagrams, verified CLI workflows, practical examples, common mistakes and anti-patterns, performance and security implications, technical interview preparation Q&As in `<details>` blocks, and hands-on practice exercises with complete solutions.

---

## Architectural Blueprint

```
+----------------------------------------------------------------------------------------------------+
|                                  GIT & GITHUB CURRICULUM ARCHITECTURE                               |
|                                                                                                    |
|  [ 00-SETUP ] -----------------> [ 01-FUNDAMENTALS ] --------------> [ 02-COMMANDS ]               |
|  - Git & GitHub CLI Installation - Three-Tree State Machine           - Porcelain & Plumbing Ops   |
|  - SSH/GPG Signing Configuration - DAG & Object Store Mechanics      - Bisect, Worktrees, LFS      |
|  - .gitconfig & Attributes       - Packfiles & Index Internals        - Submodules & Aliases       |
|            |                                                                    |                  |
|            v                                                                    v                  |
|  [ 03-HISTORY-REWRITING ] -----> [ 04-GITHUB-ACTIONS ] ------------> [ 05-COLLABORATION ]          |
|  - Interactive Rebase Surgery    - 26 In-Depth CI/CD Notes            - PR Review Lifecycle        |
|  - Reflog Forensic Recovery      - Matrix, Caching, Runners, OIDC     - Rulesets & CODEOWNERS      |
|  - git-filter-repo Secret Purge  - Security Hardening & Attestations  - Projects v2, GHCR, RBAC    |
|            |                                                                    |                  |
|            v                                                                    v                  |
|  [ 06-WORKFLOWS & STRATEGIES ] > [ 07-GITHUB-SECURITY ] -----------> [ 08-GH-CLI-AND-API ]         |
|  - Trunk-Based Dev (<24h)        - Secret Scanning & Push Protection  - GitHub CLI Daily Flow      |
|  - Conventional Commits & SemVer - CodeQL Semantic SAST Taint Analysis- REST (v3) & GraphQL (v4)  |
|  - Feature Flags & Monorepos     - Dependabot, SBOM & Sigstore        - Webhooks & GitHub Apps     |
|            |                                                                    |                  |
|            v                                                                    v                  |
|  [ 09-PROJECTS ] --------------> [ 10-INTERVIEW ] -----------------> [ 11-CHEATSHEETS ]            |
|  - 8 Reference Stacks            - 140+ Technical Q&As & Scenarios    - 7 Quick Reference Guides   |
|  - Conflict & Rebase Labs        - Git Internals & Data Structures    - Commands, Undo Matrix,     |
|  - Docker Multi-Arch to GHCR     - DevSecOps & Actions Architecture   - Branching, Actions Syntax, |
|  - Zero-Secret OIDC Deployments  - Staff Behavioral Case Studies      - Errors & Fixes             |
+----------------------------------------------------------------------------------------------------+
```

---

## Curriculum Modules Summary (124 Notes + 12 Module Guides = 136 Files)

| Module | Title | Notes | Focus Areas |
|:-------|:------|:-----:|:------------|
| **[00-setup](./00-setup/README.md)** | Setup & Installation | 5 | Linux/macOS/Windows install, SSH authentication, GPG/SSH commit signing, `.gitconfig` hierarchy, `.gitignore`, and `.gitattributes`. |
| **[01-fundamentals](./01-fundamentals/README.md)** | Core Fundamentals | 10 | Git vs GitHub, repositories, working tree vs index vs HEAD, commit snapshots, object database, refs, packfiles, SHA-1 vs SHA-256. |
| **[02-commands](./02-commands/README.md)** | Essential Commands | 21 | Init, clone, status, diff, log, add, commit, undoing, clean, branching, merging, conflicts, remotes, stash, tags, patches, blame, bisect, submodules, worktrees, sparse-checkout, LFS, hooks, aliases. |
| **[03-history-rewriting](./03-history-rewriting/README.md)** | History Rewriting & Recovery | 8 | Commit amend, interactive rebase surgery (`squash`, `fixup`, `drop`), cherry-pick, `git reflog`, `git-filter-repo` secret purging, shared history safety, reset vs revert vs restore, lost work playbook. |
| **[04-github-actions](./04-github-actions/README.md)** | GitHub Actions CI/CD | 26 | Complete CI/CD reference: syntax, events, jobs/steps, runners, variables/secrets, expressions, caching, matrix, permissions, environments, reusable workflows, composite actions, pipelines, Docker CI/CD, hardening, debugging, releases, OIDC cloud auth, ARC runners, cost optimization, local testing (`act`), monorepo filters, and provenance. |
| **[05-github-collaboration](./05-github-collaboration/README.md)** | Collaboration & Governance | 11 | PR lifecycle, code review, issues, milestones, Projects (v2), branch protection & modern Rulesets, `CODEOWNERS`, YAML issue forms, Discussions, forking, GitHub Pages, GHCR packages, and enterprise organization RBAC. |
| **[06-workflows-and-strategies](./06-workflows-and-strategies/README.md)** | Workflows & Release Strategies | 8 | Trunk-Based Development (<24h), GitFlow vs GitHub Flow, release branching, hotfixes, Conventional Commits, Semantic Versioning, Feature Flags, dark launching, monorepo vs polyrepo, environment branching anti-patterns, and team operational playbooks. |
| **[07-github-security](./07-github-security/README.md)** | Enterprise Security & Compliance | 8 | Secret scanning, pre-receive push protection, Dependabot SCA, CodeQL semantic SAST, signed commits & Vigilant Mode, `SECURITY.md`, Private Vulnerability Reporting, SBOM (SPDX/CycloneDX), Sigstore attestations, least-privilege tokens, OIDC, and leaked secret runbook. |
| **[08-gh-cli-and-api](./08-gh-cli-and-api/README.md)** | GitHub CLI & Developer APIs | 4 | `gh` CLI essentials, `--json` and `--jq` formatting, REST (v3) vs GraphQL (v4) APIs, cursor pagination, rate limits, webhooks with HMAC validation, GitHub Apps architecture, and `gh` automation in Actions. |
| **[09-projects](./09-projects/README.md)** | Production Reference Projects | 8 | 8 complete reference implementations: Conflict resolution lab, rebase & recovery lab, complete Node CI pipeline, multi-arch Docker buildx to GHCR, release automation with conventional commits, monorepo selective CI, reusable workflow library, and zero-secret OIDC cloud deployment. |
| **[10-interview](./10-interview/README.md)** | Technical Interview Preparation | 8 | 140+ rigorous questions and case studies: fundamentals, branching/merging, internals & data structures, 15 troubleshooting scenarios, collaboration governance, Actions CI/CD architecture, security engineering, and 12 Staff-level behavioral case studies. |
| **[11-cheatsheets](./11-cheatsheets/README.md)** | Quick Reference Cheatsheets | 7 | High-density rapid lookups: Git commands, undo & recovery matrix, branching models, Actions syntax, GitHub CLI (`gh`), `.gitconfig` baseline & aliases, and top 20 common errors & fixes. |

---

## Cross-Curriculum Integration

This Git & GitHub track serves as the collaborative and deployment backbone across this engineering repository:
- **[04-devops/docker](../docker/README.md)**: Integrates Docker Buildx, layer caching, and multi-architecture image publishing to GitHub Container Registry (`ghcr.io`).
- **[04-devops/aws](../aws/README.md)**: Connects GitHub Actions pipelines to AWS cloud resources (ECS, EKS, S3, Lambda) using keyless OpenID Connect (OIDC) authentication.
- **[04-devops/linux](../linux/README.md)**: Deepens knowledge of SSH key authentication, file permissions (`chmod`, file modes `100755`), line endings, and local shell scripting.
- **[04-devops/nginx](../nginx/README.md)**: Deploys reverse proxy configurations and automated TLS certificate renewers via GitOps delivery pipelines.
- **[01-languages/](../../01-languages/) & [03-backend/](../../03-backend/)**: Provides continuous integration templates, linting gates, and automated test runners for Node.js, Python, Java, and Go codebases.
