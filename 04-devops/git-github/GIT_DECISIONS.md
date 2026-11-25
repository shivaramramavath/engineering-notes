# Git & GitHub Architecture Decision Records (ADRs)

**Track**: `04-devops/git-github/`  
**Date**: November 2026  

---

## ADR 001: Directory and File Naming Standard (`NN-kebab-case`)

- **Context**: The existing track used `snake_case` directory names (`00_setup`, `01_git_fundamentals`, etc.) and uppercase filenames (`CHEATSHEET.md`). This violates repository-wide conventions established in `CONVENTIONS.md`.
- **Decision**: Rename all directories to `NN-kebab-case` (`00-setup`, `01-fundamentals`, etc.) and all markdown files to lowercase `NN-kebab-case.md`. All module directories start numbering at `00-`.
- **Status**: Accepted.

---

## ADR 002: Default Branch Standardization (`main`)

- **Context**: Older tutorials frequently reference `master` as the default branch name, leading to confusion and broken scripts on modern GitHub/GitLab installations.
- **Decision**: Standardize all CLI commands, configurations, and CI/CD workflow triggers on `main` (`branches: [ main ]`, `git checkout -b main`, `git branch -M main`). The historical context of `master` is explained in fundamentals for legacy maintenance, but never used in modern examples.
- **Status**: Accepted.

---

## ADR 003: Modern Navigation Commands (`switch` & `restore`)

- **Context**: The `git checkout` command was historically overloaded with multiple responsibilities: switching branches, creating branches, checking out specific commits, restoring working tree files, and restoring index entries.
- **Decision**: Promote `git switch` (branch management) and `git restore` (file un-staging and discard) as the primary, modern commands. Document `git checkout` for backward compatibility, but clearly explain why modern teams use the decoupled commands.
- **Status**: Accepted.

---

## ADR 004: GitHub Actions Modern Runtime & Actions Version Baseline

- **Context**: Legacy Actions workflows in the repo referenced Node 12/16 runtimes, outdated action versions (`v2`/`v3`), and deprecated workflow commands (`::set-output`).
- **Decision**:
  1. Mandate `actions/checkout@v4`, `actions/setup-node@v4`, `actions/cache@v4`, `actions/upload-artifact@v4`.
  2. Forbid `::set-output` and `::save-state`; use `$GITHUB_OUTPUT` and `$GITHUB_STATE` exclusively.
  3. Enforce immutability requirements for `upload-artifact@v4` (unique artifact names in matrix builds).
- **Status**: Accepted.

---

## ADR 005: GitHub Rulesets Prioritization over Classic Branch Protection

- **Context**: GitHub introduced Repository Rulesets, providing granular rule layering, tag protection, bypass permissions, and evaluation modes that supersede legacy branch protection.
- **Decision**: Dedicate comprehensive coverage to modern Rulesets while documenting classic branch protection settings for organizations maintaining legacy enterprise configurations.
- **Status**: Accepted.

---

## ADR 006: OpenID Connect (OIDC) Zero-Trust Cloud Authentication

- **Context**: Traditional CI/CD deployments stored static long-lived AWS IAM Access Keys and Secrets in GitHub repository secrets, representing a major credential leakage hazard.
- **Decision**: Mandate OIDC (`permissions: id-token: write`) using short-lived JWT tokens as the primary enterprise cloud deployment pattern across AWS, GCP, and Azure.
- **Status**: Accepted.

---

## ADR 007: Fenced Code Block Encapsulation (Markdown-Only Delivery)

- **Context**: Directory `04_github_actions/examples/` contained 5 raw `.yml` files, violating the repository-wide markdown-only constraint.
- **Decision**: Migrate all raw YAML workflow examples into fenced code blocks within matching markdown concept notes and reference projects. Delete the raw `.yml` files and remove `examples/`.
- **Status**: Accepted.
