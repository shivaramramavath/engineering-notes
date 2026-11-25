# Module 07: GitHub Enterprise Security & Compliance

## Overview
This module explores enterprise-grade code security, vulnerability management, and supply chain integrity on GitHub. It covers the full lifecycle of DevSecOps: proactive pre-receive push protection to block hardcoded credentials at the network boundary, automated Software Composition Analysis (SCA) via Dependabot, semantic relational Static Application Security Testing (SAST) with CodeQL, cryptographic commit identity verification using GPG and SSH keys with Vigilant Mode, Coordinated Vulnerability Disclosure (CVD) through Private Vulnerability Reporting and Security Advisories, Software Bill of Materials (SBOM) generation and artifact attestations via Sigstore, Least Privilege Access Control using Fine-Grained PATs and federated OIDC cloud credentials, and an actionable incident response runbook for history surgery using `git-filter-repo`.

---

## Learning Objectives
By completing this module, you will:
- Configure GitHub Secret Scanning and enforce Push Protection to block credentials from ever reaching remote Git history.
- Automate dependency patching and vulnerability triage with `.github/dependabot.yml` and the Dependency Review Action.
- Deploy semantic CodeQL static analysis workflows across multi-language repositories, interpreting SARIF results.
- Implement cryptographic commit and tag signing using modern SSH keys, configuring Vigilant Mode and Ruleset enforcement.
- Manage coordinated vulnerability disclosure using `SECURITY.md`, Private Vulnerability Reporting, and Temporary Private Forks.
- Generate standard SBOMs (SPDX and CycloneDX) and attest container images and binaries with Sigstore keyless provenance.
- Eliminate static cloud secrets in CI/CD pipelines by implementing OpenID Connect (OIDC) federation with AWS, GCP, and Azure.
- Execute the Leaked Secret Incident Response Runbook, performing surgical history rewriting using `git-filter-repo`.

---

## Curriculum Roadmap

| # | Topic | File | Summary |
|---|---|---|---|
| 01 | Secret Scanning & Push Protection | [`01-secret-scanning-and-push-protection.md`](./01-secret-scanning-and-push-protection.md) | Pre-receive blocking, partner patterns, custom regex rules, and bypass audit logs. |
| 02 | Dependabot & Dependency Review | [`02-dependabot-and-dependency-review.md`](./02-dependabot-and-dependency-review.md) | GHSA database, grouped version updates, private registries, and PR review action gates. |
| 03 | CodeQL & Static Analysis (SAST) | [`03-code-scanning-codeql.md`](./03-code-scanning-codeql.md) | Semantic relational code analysis, taint tracking, AST parsing, and SARIF ingestion. |
| 04 | Signed Commits & Vigilant Mode | [`04-signed-commits-and-vigilant-mode.md`](./04-signed-commits-and-vigilant-mode.md) | SSH/GPG signing, commit spoofing prevention, verification badges, and ruleset enforcement. |
| 05 | Security Policies & Advisories | [`05-security-policies-and-advisories.md`](./05-security-policies-and-advisories.md) | `SECURITY.md`, Private Vulnerability Reporting, Temporary Private Forks, and CNA CVE issuance. |
| 06 | Software Supply Chain & SBOM | [`06-sbom-and-supply-chain-security.md`](./06-sbom-and-supply-chain-security.md) | SPDX/CycloneDX exports, SLSA framework, Sigstore keyless artifact attestations, and SHA pinning. |
| 07 | Least Privilege Tokens & OIDC | [`07-least-privilege-tokens-and-permissions.md`](./07-least-privilege-tokens-and-permissions.md) | Fine-grained PATs, scoped `GITHUB_TOKEN`, GitHub Apps, and keyless cloud federation. |
| 08 | Leaked Secret Incident Runbook | [`08-leaked-secret-response-runbook.md`](./08-leaked-secret-response-runbook.md) | Emergency credential revocation, forensic log auditing, mirror cloning, and `git-filter-repo` surgery. |

---

## Enterprise DevSecOps Triad
1. **Prevent at the Boundary**: Enforce Push Protection, commit signature verification, and immutable action SHA pinning.
2. **Scan in the Pipeline**: Run CodeQL taint analysis, Dependabot SCA, and Dependency Review checks on every pull request.
3. **Attest at the Output**: Generate cryptographic build provenance (`actions/attest-build-provenance`) and SBOMs for every released artifact.
