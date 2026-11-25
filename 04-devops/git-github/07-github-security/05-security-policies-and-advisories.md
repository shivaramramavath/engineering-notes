---
title: "GitHub Security Policies & Coordinated Vulnerability Advisories"
category: "04-devops"
subcategory: "git-github / 07-github-security"
tags: ["git", "github", "security-md", "security-advisories", "cve", "ghsa", "coordinated-disclosure", "vulnerability-reporting"]
---

# GitHub Security Policies & Coordinated Vulnerability Advisories

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

When external security researchers, customers, or community members discover a critical vulnerability in a project, public disclosure in open GitHub Issues or pull requests creates immediate zero-day exposure. Malicious threat actors actively monitor public issue trackers to reverse-engineer exploits before maintainers can release patches.

GitHub provides a standardized **Coordinated Vulnerability Disclosure (CVD)** workflow:
1. **`SECURITY.md`**: A standardized security policy file defining supported versions, disclosure procedures, PGP keys, and expected SLAs for response.
2. **Private Vulnerability Reporting (PVR)**: Enables researchers to securely report vulnerabilities directly to repository maintainers via a private form without creating public issues.
3. **GitHub Security Advisories (GHSA)**: A private collaboration workspace where maintainers and reporters discuss findings, test patches in a temporary private fork, calculate CVSS scores, and directly request official **CVE** identifiers from GitHub (a certified CVE Numbering Authority).
4. **Coordinated Publication**: Publishes the advisory publicly only when patched releases are ready, simultaneously propagating the fix to downstream consumers via Dependabot alerts.

---

## 2. Core Concepts

### The `SECURITY.md` Standard
GitHub automatically surfaces a repository's `SECURITY.md` file when users attempt to open new issues. It communicates:
- Currently supported versions receiving security patches.
- Confidential contact channels (e.g., security email or Private Vulnerability Reporting).
- Disclosure timelines (e.g., standard 90-day coordinated disclosure).
- Safe harbor / bug bounty guidelines.

### Private Vulnerability Reporting (PVR)
When enabled under repository settings, an interactive **"Report a vulnerability"** button appears on the repository's Security tab. Reports submitted through this interface are completely invisible to the public and accessible only to repository security managers and owners.

### Temporary Private Forks
Fixing a security vulnerability in a standard public fork or branch leaks the patch before release, allowing attackers to diff commits and craft exploits. GitHub Security Advisories provide **Temporary Private Forks** hosted on private GitHub infrastructure. Maintainers and trusted collaborators can push commits, run CI tests, and review code in total secrecy until publication day.

### GitHub as a CVE Numbering Authority (CNA)
GitHub is an authorized CVE Numbering Authority. Maintainers can request an official CVE ID (e.g., `CVE-2026-12345`) directly through the GHSA form with zero external paperwork. Once published, GitHub synchronizes the CVE with the National Vulnerability Database (NVD) and MITRE.

---

## 3. How It Works Internally

```
Security Researcher                  GitHub Security Workspace                  Public Ecosystem
+----------------------+             +--------------------------+              +-------------------+
| Discovers Flaw       |             |                          |              |                   |
|                      |             |                          |              |                   |
| Submits Report via   | === PVR ===>| [Draft Security Advisory]|              |                   |
| Private Vuln Form    |             | - Private discussion     |              |                   |
+----------------------+             | - CVSS Score: 8.8 (High) |              |                   |
                                     +--------------------------+              |                   |
                                                  |                            |                   |
                                                  v                            |                   |
                                     +--------------------------+              |                   |
                                     | [Temporary Private Fork] |              |                   |
                                     | - Write confidential fix |              |                   |
                                     | - Run CI test suite      |              |                   |
                                     | - Request CVE Number     |              |                   |
                                     +--------------------------+              |                   |
                                                  |                            |                   |
                                                  v                            |                   |
                                     +--------------------------+              |                   |
                                     | Merge Fix & Publish Tag  |              |                   |
                                     | Publish GHSA & CVE       |              |                   |
                                     +--------------------------+              |                   |
                                                  |                            |                   |
                                                  +===========================>| Dependabot Alerts |
                                                  |                            | NVD Database Sync |
                                                  |                            | Package Registry  |
                                                  |                            +-------------------+
```

---

## 4. Syntax & Usage

### Standard `SECURITY.md` Specification
Place this file in the repository root, `.github/`, or `docs/`:

```markdown
# Security Policy

## Supported Versions
We provide security updates for the following releases:

| Version | Supported          |
|:--------|:-------------------|
| 3.x     | :white_check_mark: |
| 2.x     | :white_check_mark: |
| < 2.0   | :x:                |

## Reporting a Vulnerability
**Do not report security vulnerabilities via public GitHub issues.**

Please report vulnerabilities using GitHub's [Private Vulnerability Reporting](https://github.com/my-org/my-repo/security/advisories/new).

Alternatively, send an encrypted email to `security@example.com` using our PGP key:
- Key ID: `0x71567BD2`
- Fingerprint: `4A3B 1C2D 5E6F 7A8B ...`

### Our Commitment
- We will acknowledge receipt of your vulnerability report within 48 hours.
- We will provide a remediation timeline within 7 business days.
- We adhere to a 90-day coordinated disclosure timeline.
```

---

## 5. Practical Examples

### Example 1: Enabling Private Vulnerability Reporting via CLI
```bash
# Enable Private Vulnerability Reporting on a repository
gh api --method PUT -H "Accept: application/vnd.github+json" \
  /repos/my-org/payment-service/private-vulnerability-reporting
```

### Example 2: Creating a Draft Security Advisory via API
```bash
gh api --method POST -H "Accept: application/vnd.github+json" \
  /repos/my-org/payment-service/security-advisories \
  -f summary="SQL Injection in billing report generator" \
  -f description="Unsanitized parameters in /api/v1/reports allow unauthorized data exfiltration." \
  -f severity="high" \
  -f cwe_ids[]="CWE-89"
```

### Example 3: Adding a Temporary Private Fork
1. In the open Security Advisory, click **"New temporary private fork"**.
2. GitHub provisions a private sandbox repository (e.g., `my-org/payment-service-ghsa-xxxx`).
3. Maintainers clone the fork locally, author the patch, and push:
```bash
git remote add ghsa git@github.com:my-org/payment-service-ghsa-xxxx.git
git checkout -b fix-cve-injection
# make fixes
git commit -m "fix(security): sanitize report query parameter"
git push ghsa fix-cve-injection
```

### Example 4: Calculating CVSS 3.1 Severity Score
GitHub Advisories integrate the CVSS calculator:
- **Vector String**: `CVSS:3.1/AV:N/AC:L/PR:N/UI:N/S:U/C:H/I:N/A:N`
  - Attack Vector (AV): Network
  - Attack Complexity (AC): Low
  - Privileges Required (PR): None
  - User Interaction (UI): None
  - Scope (S): Unchanged
  - Confidentiality (C): High
  - Integrity (I): None
  - Availability (A): None
- **Base Score**: **7.5 (High)**

### Example 5: Publishing Advisory and Awarding Researcher Credit
When ready to release:
1. Tag and release the patched version (`v3.2.1`).
2. Merge the temporary private fork into default branch `main`.
3. In the advisory, add credit: Select researcher username and choose role (e.g., `Finder`, `Remediation developer`).
4. Click **"Publish advisory"**.
5. GitHub broadcasts the advisory to the global GitHub Advisory Database, alerting all Dependabot users worldwide.

---

## 6. Best Practices

- **Never Fix Vulnerabilities in Public PRs**: Public branches and PRs are indexed immediately. Always use Temporary Private Forks for security patches.
- **Provide a Global `SECURITY.md` in `.github`**: If an organization maintains hundreds of microservices, place a default `SECURITY.md` in the organization's `.github` repository (`my-org/.github/SECURITY.md`) to apply across all repos.
- **Request CVE Early in the Process**: Request the CVE number while the draft advisory is still private. This allows you to reference the official CVE number directly in your release notes and changelogs.
- **Acknowledge Researchers**: Always credit external security researchers in the GHSA credit section; this fosters community goodwill and promotes responsible disclosure.
- **Maintain Clear Version Support Tables**: Clearly document which major/minor versions receive backported security patches to prevent support ambiguity during incidents.

---

## 7. Common Mistakes

| Mistake | Impact | Correct Approach |
|:---|:---|:---|
| Directing users to public GitHub Issues for bugs | Vulnerability details exposed to blackhat scrapers | Direct users to Private Vulnerability Reporting or security email |
| Committing security fix to public `main` branch before release | Attackers diff commit history and exploit unpatched deployments | Stage fix in Temporary Private Fork and publish synchronously with release |
| Delaying patch release to wait for MITRE bureaucracy | Users remain exposed for weeks | Use GitHub's built-in CNA integration to generate CVEs within hours |
| Publishing advisory before release binaries are published | Zero-day vulnerability active without a downloadable patch | Publish binaries and packages to registries first, then publish the advisory |
| Omitting CVSS vectors in advisories | Downstream enterprise scanners cannot triage priority accurately | Always compute the full CVSS 3.1/4.0 vector string |
| Failing to update `SECURITY.md` when EOL occurs | Users report bugs for unsupported versions expecting fixes | Update version support matrix upon every major release |

---

## 8. Performance, Security & Cost Considerations

- **Performance**: PVR and Security Advisories have zero performance footprint on git operations.
- **Security**: Temporary private forks are strictly isolated; collaborators added to the advisory obtain access only to the fork and advisory discussion, not to the parent organization.
- **Cost**: `SECURITY.md`, Private Vulnerability Reporting, Security Advisories, and CVE requests are completely **free** for all public and private repositories across all plan tiers.

---

## 9. Comparison Table

| Feature | GitHub Security Advisory (GHSA) | Traditional Email Disclosure | Bug Bounty Platform (HackerOne) |
|:---|:---|:---|:---|
| **Platform Integration** | Direct into repo & Dependabot | External inbox | Third-party portal |
| **Patch Development** | Native Temporary Private Fork | Out-of-band git bundles | External git setups |
| **CVE Issuance** | Built-in CNA (Direct 1-click) | Manual application to MITRE | Handled via platform / CNA |
| **Cost** | Free for all repos | Free | Substantial platform fee |
| **Global Alert Sync** | Auto-propagates to all Dependabot users | Manual outreach | Manual or sync integration |

---

## 10. Real-World Interview Questions

<details>
<summary>1. What is Coordinated Vulnerability Disclosure (CVD) and why is it critical in open source?</summary>
CVD is a security industry practice where a security researcher privately notifies software maintainers of a flaw, giving them an agreed-upon timeframe (typically 90 days) to develop, test, and release a patched version before public disclosure. This prevents malicious actors from exploiting zero-day vulnerabilities against unprotected users.
</details>

<details>
<summary>2. How does GitHub Private Vulnerability Reporting protect open-source projects?</summary>
Prior to PVR, researchers had to find maintainer emails, navigate complex PGP encryption, or risk posting findings in public GitHub issues. PVR provides a built-in, authenticated private reporting channel directly inside GitHub, preventing public exposure and automating advisory creation.
</details>

<details>
<summary>3. Why should maintainers avoid developing security fixes in standard public pull requests?</summary>
Standard PRs and branches are publicly visible and stream through the GitHub Events API. Sophisticated attackers continuously monitor open-source repositories for commit messages containing "fix", "cve", or "overflow", reverse-engineering the patch within hours to attack downstream users who have not yet updated.
</details>

<details>
<summary>4. What role does GitHub play as a CVE Numbering Authority (CNA)?</summary>
As an authorized CNA, GitHub has the authority to directly assign CVE identifiers to vulnerabilities affecting open-source and proprietary software hosted on GitHub. This eliminates the bureaucratic friction of applying to MITRE or third-party CNAs, accelerating the disclosure timeline.
</details>

<details>
<summary>5. How does a Temporary Private Fork operate during advisory triage?</summary>
A Temporary Private Fork is an ephemeral private repository cloned from the parent repository, accessible only to users explicitly added to the Security Advisory. It allows maintainers to collaborate, review PRs, and run automated CI tests without leaking code or commit history to the public. Once the advisory is published, the fix is merged into the parent repo, and the temporary fork is deleted.
</details>

<details>
<summary>6. How does publishing a GitHub Security Advisory impact downstream consumers?</summary>
When a maintainer publishes an advisory with affected version ranges and patched versions, GitHub automatically ingests it into the global GitHub Advisory Database. Dependabot immediately scans all dependent repositories across the world, generating high-priority security alerts and automated pull requests to upgrade to the patched version.
</details>

<details>
<summary>7. What is the organization `.github` repository and how does it relate to `SECURITY.md`?</summary>
If an organization creates a repository named `.github` (e.g., `my-org/.github`), any `SECURITY.md` file placed inside its root or `.github/` folder serves as the default security policy for all repositories in the organization that do not define their own `SECURITY.md`.
</details>

<details>
<summary>8. What metrics determine a CVSS 3.1 Base Score in an advisory?</summary>
The score is calculated based on Exploitability metrics (Attack Vector, Attack Complexity, Privileges Required, User Interaction) and Impact metrics (Confidentiality, Integrity, Availability), yielding a severity rating from 0.0 (None) to 10.0 (Critical).
</details>

---

## 11. Hands-On Exercises

<details>
<summary>Exercise 1: Create a repository SECURITY.md file</summary>

**Goal:** Create a standardized `.github/SECURITY.md` file.

```bash
mkdir -p .github
cat << 'EOF' > .github/SECURITY.md
# Security Policy
## Supported Versions
| Version | Supported |
|---|---|
| 1.x | Yes |

## Reporting
Report flaws privately via GitHub Security Advisories.
EOF
git add .github/SECURITY.md && git commit -m "docs: add security policy"
```
</details>

<details>
<summary>Exercise 2: Enable Private Vulnerability Reporting</summary>

**Goal:** Turn on PVR using GitHub CLI.

```bash
gh api --method PUT /repos/:owner/:repo/private-vulnerability-reporting
```
</details>

<details>
<summary>Exercise 3: Inspect active Security Advisories</summary>

**Goal:** List all security advisories for a repository.

```bash
gh api /repos/:owner/:repo/security-advisories
```
</details>

<details>
<summary>Exercise 4: Query the GitHub Global Advisory Database</summary>

**Goal:** Search global advisories for a specific CVE.

```bash
gh api /advisories?cve_id=CVE-2023-44487
```
</details>

<details>
<summary>Exercise 5: Add a researcher credit to an advisory</summary>

**Goal:** Understand the JSON payload for awarding credit in an advisory.

```json
{
  "credits": [
    {
      "login": "octocat",
      "type": "finder"
    }
  ]
}
```
</details>

<details>
<summary>Exercise 6: Verify organization default security policy</summary>

**Goal:** Check if the organization has an active default `.github/SECURITY.md`.

```bash
gh api /repos/:owner/.github/contents/SECURITY.md
```
</details>

---

## 12. Summary / Cheat Sheet

```text
================================================================================
                    SECURITY POLICIES & ADVISORIES
================================================================================
Policy File        : SECURITY.md in repo root, .github/, or org default (.github/.github)
Intake Channel     : Private Vulnerability Reporting (PVR) -> Private intake form
Collaboration Space: GitHub Security Advisories (GHSA) -> Discussions & triage
Confidential Patch : Temporary Private Fork -> Isolated build & CI environment
CVE Issuance       : GitHub is an authorized CNA -> 1-click CVE generation
Global Propagation : Publishing GHSA auto-triggers Dependabot alerts worldwide
Calculation Engine : CVSS 3.1 / 4.0 Vector calculator built into advisory form
================================================================================
```

---

## 13. Further Reading
- [GitHub Docs: Adding a security policy to your repository](https://docs.github.com/en/code-security/getting-started/adding-a-security-policy-to-your-repository)
- [GitHub Docs: About private vulnerability reporting](https://docs.github.com/en/code-security/security-advisories/guidance-on-reporting-and-writing/about-private-vulnerability-reporting)
- [GitHub Docs: About repository security advisories](https://docs.github.com/en/code-security/security-advisories/repository-security-advisories/about-repository-security-advisories)
- [FIRST: Common Vulnerability Scoring System (CVSS)](https://www.first.org/cvss/)

---

[<- Previous: Signed Commits & Vigilant Mode](./04-signed-commits-and-vigilant-mode.md) | [Module 07 Overview](./README.md) | [Next: Software Supply Chain & SBOM ->](./06-sbom-and-supply-chain-security.md)
