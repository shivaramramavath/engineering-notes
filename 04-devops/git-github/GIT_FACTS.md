# Git & GitHub Technical Facts & Baseline Assertions

**Date Established**: November 2026  
**Curriculum Scope**: `04-devops/git-github/`  

---

## 1. Tool Versions & Platform Baseline

| Tool / Component | Baseline Version | Release / LTS Status | Key Innovations & Support Horizon |
|:-----------------|:----------------:|:--------------------:|:----------------------------------|
| **Git Core** | `2.49.x` (min `2.44+`) | Active Release | Reftable backend (`--ref-format=reftable`), SHA-256 transition (`--object-format=sha256`), `scalar`, `git switch`/`git restore`. |
| **GitHub Actions Runner** | `v2.320+` | Node 20 Execution Default | Node 12 and Node 16 runtimes fully deprecated. ARM64 Ubuntu & macOS larger runners generally available. |
| **GitHub CLI (`gh`)** | `v2.60+` | Active Release | Full extensions support, GraphQL v4, Ruleset management, secret scanning integration. |

---

## 2. Core Git Architectural Facts

1. **Reference Backend (`reftable`)**:
   - Introduced in Git 2.45 as experimental binary-searchable reference storage backend (`git init --ref-format=reftable`), enhanced in Git 2.46/2.47/2.48.
   - Replaces traditional one-file-per-ref in `.git/refs/` and monolithic `.git/packed-refs` with compact, block-indexed binary tables. Delivers near $O(1)$ lookups, tombstones for atomic deletions, and transactional writes.
2. **Object Format (SHA-1 to SHA-256 Transition)**:
   - Git supports SHA-256 repositories via `git init --object-format=sha256`.
   - Interoperability between SHA-1 and SHA-256 remote repos is managed through object translation tables.
3. **Modern Navigation Commands**:
   - `git switch <branch>` (branch switching/creation) and `git restore <file>` (working tree and index restoration) are the modern, recommended commands replacing the overloaded `git checkout`.
4. **Large Monorepo Tooling**:
   - `git sparse-checkout` (cone mode) and `scalar` are built directly into Git for high-performance monorepo operations, sparse cloning, and automated background maintenance (`git maintenance start`).

---

## 3. GitHub Actions Runtime & API Baselines

1. **Official Core Actions Major Versions (as of 2026)**:
   - `actions/checkout@v4` (Node 20 runtime, sparse checkout support)
   - `actions/setup-node@v4`
   - `actions/setup-python@v5`
   - `actions/setup-java@v4`
   - `actions/setup-go@v5`
   - `actions/cache@v4`
   - `actions/upload-artifact@v4` & `actions/download-artifact@v4`
2. **Artifacts v4 Immutability**:
   - Artifacts are **strictly immutable** and scoped to the job that uploaded them.
   - Matrix builds attempting to upload with the same artifact name fail with `409 Conflict`. Unique names per matrix slice are required (`name: my-artifact-${{ matrix.os }}`).
   - To update an artifact within a sequential job, `overwrite: true` must be specified, which replaces the entity rather than mutating it in place.
3. **Environment Command Deprecations**:
   - `::set-output` and `::save-state` workflow commands are **completely disabled**. Workflows must write to `$GITHUB_OUTPUT` and `$GITHUB_STATE` (`echo "key=val" >> "$GITHUB_OUTPUT"`).
   - `::set-env` is permanently disabled due to critical code injection vulnerabilities. Workflows must use `$GITHUB_ENV`.
4. **Runner Runtimes**:
   - Node 12 and Node 16 runner execution environments are deprecated; action definitions (`action.yml`) must declare `runs.using: 'node20'`.
5. **Security & Zero-Trust Authentication**:
   - **OIDC (OpenID Connect)**: Standard mechanism for authenticating with cloud providers (AWS, GCP, Azure, HashiCorp Vault) without static long-lived credentials. Requires `permissions: id-token: write`.
   - **SHA Pinning**: Production workflows must pin third-party actions by immutable commit SHA (`uses: actions/checkout@b4ffde... # v4.1.1`).
   - **Rulesets vs. Classic Branch Protection**: GitHub Rulesets provide layered, organization-wide or repo-wide branch and tag protection rules, bypass lists, and evaluation modes ("evaluate" without blocking).

---

## 4. Verification Sources

- [Git Official Documentation & Release Notes](https://git-scm.com/docs)
- [GitHub Blog: Introducing the reftable format](https://github.blog/2024-05-13-the-reftable-format/)
- [GitHub Blog: Immutable Artifacts v4 in GitHub Actions](https://github.blog/2023-12-14-notice-of-upcoming-deprecations-and-breaking-changes-in-github-actions/)
- [GitHub Actions Security Hardening & OIDC Guides](https://docs.github.com/en/actions/deployment/security-hardening-your-deployments/about-security-hardening-with-openid-connect)
