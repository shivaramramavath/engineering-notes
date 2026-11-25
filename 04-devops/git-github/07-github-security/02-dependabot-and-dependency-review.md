---
title: "Dependabot & Dependency Review"
category: "04-devops"
subcategory: "git-github / 07-github-security"
tags: ["git", "github", "dependabot", "dependency-review", "supply-chain", "vulnerabilities", "sca", "devsecops"]
---

# Dependabot & Dependency Review

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Considerations](#8-performance-security--cost-considerations)
- [9. Comparison Table](#9-comparison-table)
- [10. Real-World Interview Questions](#10-real-world-interview-questions)
- [11. Hands-On Exercises](#11-hands-on-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

Modern software applications are predominantly constructed from open-source dependencies; typically, proprietary code accounts for only 10% to 20% of an application's total codebase. Consequently, the vast majority of software vulnerabilities are inherited through transitive and direct third-party packages.

**Dependabot** is GitHub's automated Software Composition Analysis (SCA) and automated dependency management engine. It operates across three primary functions:
1. **Dependabot Alerts**: Monitors the GitHub Advisory Database and notifies repository maintainers when vulnerable package versions are detected in dependency lockfiles.
2. **Dependabot Security Updates**: Automatically generates pull requests to upgrade vulnerable dependencies to the minimum patched version required to remediate CVEs.
3. **Dependabot Version Updates**: Proactively submits PRs on scheduled intervals (daily, weekly, monthly) to keep dependencies up to date with upstream releases.
4. **Dependency Review**: Evaluates incoming pull requests to block the introduction of new vulnerable packages, undesirable licenses, or malicious dependencies before merge.

Without automated dependency scanning, organizations suffer from severe technical debt, unpatched zero-day vulnerabilities, and compliance failures under NIST SP 800-218 and Executive Order 14028.

---

## 2. Core Concepts

### GitHub Advisory Database
Dependabot queries GitHub's curated vulnerability database, which aggregates Common Vulnerabilities and Exposures (CVEs) from the National Vulnerability Database (NVD), security advisories published by maintainers, and community security researchers. Each advisory is scored using Common Vulnerability Scoring System (CVSS) metrics (Low, Medium, High, Critical).

### Dependency Graph
GitHub builds a persistent dependency graph for each repository by parsing manifest and lock files (`package-lock.json`, `pom.xml`, `requirements.txt`, `go.sum`, `Cargo.lock`, etc.). The graph maps both direct dependencies (explicitly declared) and transitive dependencies (dependencies of dependencies).

### Grouped Updates
To combat "notification fatigue" and PR spam, Dependabot allows teams to group related updates (e.g., all minor/patch updates for `@angular/*` or `spring-boot-*`) into a single consolidated pull request with unified CI validation.

### Dependency Review Action
While Dependabot updates existing code, the **Dependency Review Action** (`actions/dependency-review-action`) acts as a PR gatekeeper. When a developer adds or bumps a dependency in a PR, the action compares the base and head lockfiles, blocking the PR if the added dependency has known vulnerabilities or unacceptable software licenses (e.g., GPL in a proprietary codebase).

---

## 3. How It Works Internally

```
+-------------------------------------------------------------------------+
| GitHub Advisory Database (NVD, CVEs, GHSA alerts)                      |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| Repository Manifests (package-lock.json, pom.xml, poetry.lock, etc.)   |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| Dependabot Analysis Engine                                              |
| 1. Matches lockfile versions against known GHSAs                        |
| 2. Computes minimal non-vulnerable version                              |
| 3. Resolves dependency graph & generates lockfile patch                 |
| 4. Dispatches Automated Pull Request                                    |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| Developer PR Pipeline                                                   |
| - CI Tests Run                                                          |
| - Dependency Review Action verifies licenses and new vulnerabilities    |
| - Automated Merge (@dependabot merge) or Engineer Review                |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

Dependabot Version Updates are configured via a version-controlled YAML file located at `.github/dependabot.yml`.

### Minimal `.github/dependabot.yml` Configuration
```yaml
version: 2
updates:
  # Maintain npm dependencies
  - package-ecosystem: "npm"
    directory: "/"
    schedule:
      interval: "weekly"
      day: "monday"
      time: "09:00"
      timezone: "America/New_York"
    open-pull-requests-limit: 10
    target-branch: "main"
    labels:
      - "dependencies"
      - "javascript"

  # Maintain Dockerfile base images
  - package-ecosystem: "docker"
    directory: "/"
    schedule:
      interval: "weekly"
```

---

## 5. Practical Examples

### Example 1: Grouped Updates Configuration
Group non-breaking minor and patch updates to minimize PR noise:
```yaml
version: 2
updates:
  - package-ecosystem: "npm"
    directory: "/"
    schedule:
      interval: "daily"
    groups:
      dev-dependencies:
        dependency-type: "development"
        update-types:
          - "minor"
          - "patch"
      production-dependencies:
        dependency-type: "production"
        update-types:
          - "patch"
```

### Example 2: Configuring Private Registry Authentication
Authenticate Dependabot against a private npm registry (e.g., Artifactory or GHCR):
```yaml
version: 2
registries:
  corp-npm:
    type: "npm-registry"
    url: "https://npm.pkg.github.com"
    token: "${{ secrets.DEPENDABOT_PAT }}"

updates:
  - package-ecosystem: "npm"
    directory: "/"
    registries:
      - "corp-npm"
    schedule:
      interval: "weekly"
```

### Example 3: Enforcing PR Dependency Review in GitHub Actions
Block PRs introducing dependencies with vulnerabilities or non-compliant licenses:
```yaml
name: "Dependency Review"
on: [pull_request]

permissions:
  contents: read
  pull-requests: write

jobs:
  dependency-review:
    runs-on: ubuntu-latest
    steps:
      - name: "Checkout Repository"
        uses: actions/checkout@v4

      - name: "Run Dependency Review"
        uses: actions/dependency-review-action@v4
        with:
          fail-on-severity: "high"
          deny-licenses: "GPL-2.0, GPL-3.0, AGPL-3.0"
          comment-summary-in-pr: true
```

### Example 4: Automating Dependabot PR Auto-Merge
Use GitHub Actions to automatically approve and merge patch updates submitted by Dependabot:
```yaml
name: "Dependabot Auto-Merge"
on: pull_request

permissions:
  contents: write
  pull-requests: write

jobs:
  dependabot:
    runs-on: ubuntu-latest
    if: github.actor == 'dependabot[bot]'
    steps:
      - name: Fetch Dependabot metadata
        id: metadata
        uses: dependabot/fetch-metadata@v2
        with:
          github-token: "${{ secrets.GITHUB_TOKEN }}"

      - name: Approve and Enable Auto-Merge for Patch Updates
        if: steps.metadata.outputs.update-type == 'version-update:semver-patch'
        run: |
          gh pr review --approve "$PR_URL"
          gh pr merge --auto --squash "$PR_URL"
        env:
          PR_URL: ${{ github.event.pull_request.html_url }}
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

### Example 5: Listing Dependabot Alerts via GitHub CLI
```bash
# Query all open High and Critical Dependabot alerts
gh api -H "Accept: application/vnd.github+json" \
  /repos/my-org/backend-service/dependabot/alerts \
  --jq '.[] | select(.state == "open" and (.security_advisory.severity == "high" or .security_advisory.severity == "critical")) | {id: .number, package: .dependency.package.name, severity: .security_advisory.severity, summary: .security_advisory.summary}'
```

---

## 6. Best Practices

- **Pair Security Updates with Grouped Updates**: Always enable automated security updates immediately, and group version updates so developers are not overwhelmed by dozens of individual PRs.
- **Enforce Dependency Review as a Required Check**: Add the Dependency Review Action to your branch protection rulesets to stop risky packages before merge.
- **Pin Direct Dependencies, Automate Patches**: Use semantic versioning ranges prudently in libraries, but pin lockfiles in end-user applications to ensure deterministic builds.
- **Audit Licenses Automatically**: Block copyleft licenses (GPL/AGPL) in commercial repositories via `dependency-review-action` to protect proprietary IP.
- **Leverage Dependabot Comments**: Interact with Dependabot PRs using inline comment commands: `@dependabot rebase`, `@dependabot recreate`, `@dependabot ignore this major version`.

---

## 7. Common Mistakes

| Mistake | Impact | Correct Approach |
|:---|:---|:---|
| Ignoring Dependabot PRs until dozens pile up | Creates massive merge conflicts and blocks future automated updates | Schedule weekly triage or configure auto-merge for tested patch bumps |
| Disabling lockfiles from Git tracking | Dependabot cannot build accurate dependency graphs or generate atomic updates | Always commit `package-lock.json`, `pnpm-lock.yaml`, or equivalent |
| Setting schedule interval to daily without grouping | Spams team with 20+ individual PRs per week | Use `interval: "weekly"` and configure `groups:` in `dependabot.yml` |
| Failing to configure private registry tokens | Dependabot fails silently or throws build errors when resolving internal packages | Add registry secrets in `.github/dependabot.yml` using `registries:` block |
| Relying only on post-merge Dependabot alerts | Vulnerabilities enter `main` before being detected | Run `actions/dependency-review-action` on all pull requests |
| Manually editing Dependabot lockfile PRs incorrectly | Causes subsequent Dependabot runs to fail or conflict | Instruct Dependabot to update via `@dependabot rebase` |

---

## 8. Performance, Security & Cost Considerations

- **Performance**: Dependabot updates run on GitHub-hosted infrastructure and do not consume GitHub Actions runner minutes unless triggering CI on the resulting PR.
- **Security**: Dependabot PRs from forks or on public repositories run with restricted `GITHUB_TOKEN` permissions (`contents: read`) to prevent malicious pull requests from exfiltrating repository secrets.
- **Cost**: Dependabot alerts, security updates, and version updates are free for all public and private repositories across all GitHub plan tiers.

---

## 9. Comparison Table

| Capability | Dependabot Security Updates | Dependabot Version Updates | Snyk / Renovate |
|:---|:---|:---|:---|
| **Trigger** | Advisory published in GHSA | Scheduled cron (`interval: weekly`) | CLI, CI, or daemon schedule |
| **Primary Goal**| Patch immediate known CVE | Eliminate technical debt | Broad SCA, container & IaC scan |
| **Configuration**| Automatic (UI toggle) | Requires `.github/dependabot.yml` | Custom config files (`renovate.json`) |
| **Ecosystem Support**| Broad (npm, pip, maven, nuget, etc.)| Broad | Broad + extensive custom managers |
| **License Cost** | Included free with GitHub | Included free with GitHub | Commercial / Freemium tiers |

---

## 10. Real-World Interview Questions

<details>
<summary>1. What is the difference between Dependabot Security Updates and Dependabot Version Updates?</summary>
Dependabot Security Updates trigger automatically whenever a known vulnerability (CVE/GHSA) affects a package version declared in your repository, submitting a targeted PR to bump to the minimum safe version. Dependabot Version Updates are configured via `.github/dependabot.yml` on a recurring schedule (e.g., daily or weekly) to proactively bump dependencies to their newest releases, regardless of whether a security vulnerability exists.
</details>

<details>
<summary>2. How does the Dependency Review Action differ from Dependabot?</summary>
Dependabot works asynchronously in the background, scanning the default branch and submitting pull requests to update outdated or vulnerable dependencies. The Dependency Review Action runs synchronously within a PR workflow, inspecting the delta between base and head branches to block any new vulnerable dependencies or non-compliant licenses from being introduced before the PR is merged.
</details>

<details>
<summary>3. Why do Dependabot PR workflows often fail when accessing repository secrets?</summary>
For security reasons, when Dependabot triggers a workflow on a pull request, GitHub restricts permissions to read-only access and does not expose repository secrets to the run by default. To provide secrets to Dependabot, administrators must specifically configure "Dependabot secrets" under repository security settings or use `secrets: inherit` in reusable workflows.
</details>

<details>
<summary>4. What command would you give Dependabot if a generated PR has merge conflicts?</summary>
You comment `@dependabot rebase` directly on the pull request. Dependabot will pull the latest changes from the base branch, re-resolve the dependency lockfile, and update the PR branch automatically.
</details>

<details>
<summary>5. How can you prevent Dependabot from submitting major breaking upgrades?</summary>
You can configure the `ignore` directive in `.github/dependabot.yml` or comment `@dependabot ignore this major version` on the PR. In `dependabot.yml`:
```yaml
ignore:
  - dependency-name: "webpack"
    update-types: ["version-update:semver-major"]
```
</details>

<details>
<summary>6. How does Dependabot resolve transitive dependencies in npm projects?</summary>
Dependabot parses `package-lock.json` or `yarn.lock`. When a transitive vulnerability is flagged, Dependabot attempts to bump the direct top-level dependency whose range encompasses a safe transitive version, or modifies the lockfile's resolved transitive entries directly if permissible by semantic ranges.
</details>

<details>
<summary>7. What permissions are required in a workflow to allow Dependabot auto-merging?</summary>
The workflow needs `contents: write` (to merge commits) and `pull-requests: write` (to submit approving reviews and merge commands via GitHub CLI or API).
</details>

<details>
<summary>8. How do you configure Dependabot to authenticate against private Docker registries?</summary>
Declare a `docker-registry` entry in the `registries:` section of `.github/dependabot.yml`, referencing credentials stored in Dependabot Secrets (e.g., username and password or auth token), and link the registry to the docker update configuration.
</details>

---

## 11. Hands-On Exercises

<details>
<summary>Exercise 1: Check Dependabot status via GitHub CLI</summary>

**Goal:** Determine if Dependabot alerts are enabled on a repository.

```bash
gh api /repos/:owner/:repo --jq '.security_and_analysis.dependabot_security_updates.status'
```
</details>

<details>
<summary>Exercise 2: Create a minimal Dependabot configuration</summary>

**Goal:** Create `.github/dependabot.yml` for automated GitHub Actions and npm tracking.

```yaml
version: 2
updates:
  - package-ecosystem: "github-actions"
    directory: "/"
    schedule:
      interval: "weekly"
  - package-ecosystem: "npm"
    directory: "/"
    schedule:
      interval: "weekly"
```
</details>

<details>
<summary>Exercise 3: Test Dependency Review Action locally</summary>

**Goal:** Add the Dependency Review Action to `.github/workflows/dependency-review.yml`.

```yaml
name: Dependency Review
on: [pull_request]
jobs:
  review:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/dependency-review-action@v4
```
</details>

<details>
<summary>Exercise 4: Group development dependencies in dependabot.yml</summary>

**Goal:** Configure a group named `linters` for eslint and prettier.

```yaml
groups:
  linters:
    patterns:
      - "eslint*"
      - "prettier*"
```
</details>

<details>
<summary>Exercise 5: Interact with Dependabot via comment</summary>

**Goal:** Request Dependabot to recreate an existing PR from scratch.

Leave a comment on the open Dependabot PR:
```text
@dependabot recreate
```
</details>

<details>
<summary>Exercise 6: Filter Dependabot alerts by CVE identifier</summary>

**Goal:** Query the GitHub API for alerts related to a specific package name.

```bash
gh api /repos/:owner/:repo/dependabot/alerts \
  --jq '.[] | select(.dependency.package.name == "axios")'
```
</details>

---

## 12. Summary / Cheat Sheet

```text
================================================================================
                        DEPENDABOT & DEPENDENCY REVIEW
================================================================================
Config File        : .github/dependabot.yml (version: 2)
Alerts             : Automatic notifications based on GitHub Advisory Database
Security Updates   : Targeted PRs to patch active CVEs to minimum safe version
Version Updates    : Scheduled PRs (daily, weekly, monthly) to eliminate tech debt
PR Gatekeeper      : actions/dependency-review-action@v4
Commands           : @dependabot rebase | recreate | merge | ignore this version
Grouped Updates    : groups: <name>: dependency-type: "development" | "production"
================================================================================
```

---

## 13. Further Reading
- [GitHub Docs: Configuring Dependabot version updates](https://docs.github.com/en/code-security/dependabot/dependabot-version-updates/configuring-dependabot-version-updates)
- [GitHub Actions: Dependency Review Action](https://github.com/actions/dependency-review-action)
- [GitHub Advisory Database](https://github.com/advisories)
- [NIST NVD Vulnerability Metrics (CVSS)](https://nvd.nist.gov/vuln-metrics/cvss)

---

[<- Previous: Secret Scanning & Push Protection](./01-secret-scanning-and-push-protection.md) | [Module 07 Overview](./README.md) | [Next: CodeQL & Static Analysis ->](./03-code-scanning-codeql.md)
