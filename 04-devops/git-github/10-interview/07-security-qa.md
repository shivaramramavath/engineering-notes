<!--
================================================================================
Metadata:
  Title: Technical Interview Preparation - Git & GitHub Security Engineering Q&A
  Track: Git & GitHub / DevOps
  Difficulty: Staff to Principal
  Estimated Time: 55 minutes
  Key Terminology: GHAS, Secret Scanning, Push Protection, CodeQL, SAST, Taint Tracking, Dependabot, SCA, SBOM, SLSA, Sigstore, OIDC
================================================================================
-->

# Git & GitHub Security Engineering: 20 In-Depth Interview Questions & Answers

[<- Back to Interview Overview](./README.md) | [Next Note: 08-behavioural-and-project-discussion.md ->](./08-behavioural-and-project-discussion.md)

---

## Overview

This guide provides 20 senior- and staff-level interview questions analyzing DevSecOps, GitHub Advanced Security (GHAS), static code analysis, software supply chain security, cryptographic identity, and enterprise incident response.

---

## Questions Index

1. [Q01: What are the three foundational pillars of GitHub Advanced Security (GHAS)?](#q01-what-are-the-three-foundational-pillars-of-github-advanced-security-ghas)
2. [Q02: How does GitHub Push Protection physically block a developer from pushing a secret?](#q02-how-does-github-push-protection-physically-block-a-developer-from-pushing-a-secret)
3. [Q03: How does CodeQL perform semantic taint tracking, and how does it differ from pattern-matching linters?](#q03-how-does-codeql-perform-semantic-taint-tracking-and-how-does-it-differ-from-pattern-matching-linters)
4. [Q04: What is SARIF, and why is it considered the universal standard in DevSecOps pipelines?](#q04-what-is-sarif-and-why-is-considered-the-universal-standard-in-devsecops-pipelines)
5. [Q05: Explain the difference between Dependabot Alerts, Security Updates, and Version Updates.](#q05-explain-the-difference-between-dependabot-alerts-security-updates-and-version-updates)
6. [Q06: How does the Dependency Review Action protect the default branch on pull requests?](#q06-how-does-the-dependency-review-action-protect-the-default-branch-on-pull-requests)
7. [Q07: What is commit spoofing in Git, and how does Vigilant Mode eliminate this threat?](#q07-what-is-commit-spoofing-in-git-and-how-does-vigilant-mode-eliminate-this-threat)
8. [Q08: Why is SSH key signing increasingly preferred over GPG in enterprise engineering teams?](#q08-why-is-ssh-key-signing-increasingly-preferred-over-gpg-in-enterprise-engineering-teams)
9. [Q09: What is a Software Bill of Materials (SBOM), and contrast SPDX vs CycloneDX.](#q09-what-is-a-software-bill-of-materials-sbom-and-contrast-spdx-vs-cyclonedx)
10. [Q10: What is the SLSA framework, and what does SLSA Level 3 mandate for CI/CD pipelines?](#q10-what-is-the-slsa-framework-and-what-does-slsa-level-3-mandate-for-cicd-pipelines)
11. [Q11: How does `actions/attest-build-provenance` use Sigstore to generate keyless build attestations?](#q11-how-does-actionsattest-build-provenance-use-sigstore-to-generate-keyless-build-attestations)
12. [Q12: Why is pinning third-party GitHub Actions to a commit SHA safer than pinning to a release tag?](#q12-why-is-pinning-third-party-github-actions-to-a-commit-sha-safer-than-pinning-to-a-release-tag)
13. [Q13: Explain the attack vector known as "Dependency Confusion" and how to mitigate it.](#q13-explain-the-attack-vector-known-as-dependency-confusion-and-how-to-mitigate-it)
14. [Q14: An AWS root key was pushed to a public GitHub repo. What is your exact step-by-step incident response?](#q14-an-aws-root-key-was-pushed-to-a-public-github-repo-what-is-your-exact-step-by-step-incident-response)
15. [Q15: Why is `git-filter-repo` superior to `git filter-branch` and BFG for history surgery?](#q15-why-is-git-filter-repo-superior-to-git-filter-branch-and-bfg-for-history-surgery)
16. [Q16: How do Fine-Grained Personal Access Tokens (FG-PATs) prevent the blast radius of Classic PAT leaks?](#q16-how-do-fine-grained-personal-access-tokens-fg-pats-prevent-the-blast-radius-of-classic-pat-leaks)
17. [Q17: What security risks are introduced by using `pull_request_target` with an unvetted checkout?](#q17-what-security-risks-are-introduced-by-using-pull_request_target-with-an-unvetted-checkout)
18. [Q18: What is Coordinated Vulnerability Disclosure (CVD), and how does Private Vulnerability Reporting support it?](#q18-what-is-coordinated-vulnerability-disclosure-cvd-and-how-does-private-vulnerability-reporting-support-it)
19. [Q19: How does GitHub verify that an OIDC token issued to Actions was not spoofed or intercepted?](#q19-how-does-github-verify-that-an-oidc-token-issued-to-actions-was-not-spoofed-or-intercepted)
20. [Q20: What is GitHub's Temporary Private Fork, and why is it used for developing security fixes?](#q20-what-is-githubs-temporary-private-fork-and-why-is-it-used-for-developing-security-fixes)

---

## Questions & Answers

### Q01: What are the three foundational pillars of GitHub Advanced Security (GHAS)?
**Answer:**  
1. **Secret Scanning & Push Protection**: Scans commit history, issues, and PRs for partner and custom secret formats; push protection blocks secrets pre-receive at the server boundary.
2. **Code Scanning (Powered by CodeQL)**: Semantic Static Application Security Testing (SAST) that models source code as a relational database to detect deep vulnerabilities (SQLi, XSS, RCE) via taint analysis.
3. **Dependabot (Software Composition Analysis)**: Automated scanning of direct and transitive dependencies against the GitHub Advisory Database, issuing alerts, automated security PRs, and dependency review gates.

---

### Q02: How does GitHub Push Protection physically block a developer from pushing a secret?
**Answer:**  
Push Protection executes within Git's native server-side `pre-receive` hook boundary on GitHub's infrastructure. When a developer pushes, Git transmits object packfiles. Before updating branch references, the pre-receive hook inspects the uncompressed blob payloads against regex patterns and Shannon entropy rules. If a known secret pattern matches, the hook terminates with a non-zero exit code, rejecting the push over the wire. The secret never lands on GitHub's disks or reflogs.

---

### Q03: How does CodeQL perform semantic taint tracking, and how does it differ from pattern-matching linters?
**Answer:**  
Linters scan text files for regex keywords (e.g., searching for `exec()` or `innerHTML`), producing high false positive rates and missing multi-file vulnerabilities.
**CodeQL** compiles source code into a relational database capturing Abstract Syntax Trees, Control Flow Graphs, and Data Flow Graphs. It executes declarative QL queries tracking **taint propagation**:
- **Source**: Untrusted user input (e.g., `req.body.search`).
- **Sink**: Dangerous execution function (e.g., `db.query()`).
- **Sanitizer**: Validation/escaping logic.
CodeQL computes reachability paths from Source to Sink across functions, classes, and files, flagging an alert only if an unbroken path exists without passing through an effective sanitizer.

---

### Q04: What is SARIF, and why is it considered the universal standard in DevSecOps pipelines?
**Answer:**  
SARIF (Static Analysis Results Interchange Format) is an OASIS standard JSON schema defining how static analysis findings are represented. It standardizes rule IDs, file locations, line ranges, CWE classifications, and remediation steps. GitHub Code Scanning ingests SARIF natively, allowing tools like CodeQL, Trivy, Snyk, SonarQube, and Checkmarx to export findings into a single consolidated GitHub Security dashboard.

---

### Q05: Explain the difference between Dependabot Alerts, Security Updates, and Version Updates.
**Answer:**  
- **Dependabot Alerts**: Passive notifications informing maintainers that a declared dependency in a manifest/lockfile has a known CVE in the GitHub Advisory Database.
- **Dependabot Security Updates**: Automated pull requests triggered immediately by an active advisory to upgrade a vulnerable package to the minimum safe version.
- **Dependabot Version Updates**: Proactive, scheduled pull requests (configured via `.github/dependabot.yml`) to keep dependencies up to date with upstream releases regardless of security flaws, eliminating technical debt.

---

### Q06: How does the Dependency Review Action protect the default branch on pull requests?
**Answer:**  
While Dependabot scans code already merged in `main`, the **Dependency Review Action** (`actions/dependency-review-action`) acts as a synchronous PR quality gate. It compares the dependency graph of the PR branch against the base branch, blocking the PR if any *newly added or bumped* package contains known CVEs or non-compliant licenses (e.g., GPL in proprietary repos).

---

### Q07: What is commit spoofing in Git, and how does Vigilant Mode eliminate this threat?
**Answer:**  
In Git, `user.name` and `user.email` are arbitrary string headers configured in `.gitconfig`. Anyone can configure Linus Torvalds' name and email and push commits; GitHub will display Linus' avatar and username, creating a severe impersonation risk.
Commit signing (GPG/SSH) solves this by appending a digital signature. **Vigilant Mode** enforces zero-trust: it explicitly marks all unsigned commits authored under your email address with a yellow **Unverified** warning badge, alerting reviewers that the author was not cryptographically authenticated.

---

### Q08: Why is SSH key signing increasingly preferred over GPG in enterprise engineering teams?
**Answer:**  
Historically, GPG was the standard for Git signing, but it suffered from steep learning curves, clunky GPG agent daemons, difficult key expiration ceremonies, and platform incompatibilities. Git 2.34+ natively supports signing with standard SSH keys (`gpg.format ssh`). Developers can sign commits using the same SSH keys (`ed25519`) they already use for Git transport, enabling hardware token integration (Touch ID, Windows Hello, 1Password, YubiKey) with zero additional software overhead.

---

### Q09: What is a Software Bill of Materials (SBOM), and contrast SPDX vs CycloneDX.
**Answer:**  
An SBOM is a formal, machine-readable inventory of every software component, library, compiler, and dependency that constitutes an application.
- **SPDX (Software Package Data Exchange)**: An ISO standard (ISO/IEC 5962:2021) developed by the Linux Foundation, heavily focused on open-source licensing compliance, copyright attribution, and contract requirements.
- **CycloneDX**: An OWASP standard specifically optimized for modern application security, container composition, and vulnerability exploitability exchange (VEX).

---

### Q10: What is the SLSA framework, and what does SLSA Level 3 mandate for CI/CD pipelines?
**Answer:**  
SLSA (Supply-chain Levels for Software Artifacts) is an OpenSSF security framework.
**SLSA Level 3 Mandates**:
- The build must execute on a dedicated, hardened, and **ephemeral build platform** (e.g., isolated virtual runners).
- The build produces **non-falsifiable provenance** (cryptographically signed metadata proving which source repo, commit SHA, and workflow generated the artifact).
- Provenance cannot be modified or forged by the build script itself.

---

### Q11: How does `actions/attest-build-provenance` use Sigstore to generate keyless build attestations?
**Answer:**  
1. The runner requests an OpenID Connect (OIDC) identity token from GitHub containing run claims (repo, commit, workflow).
2. The runner generates an ephemeral key pair in-memory.
3. The runner exchanges the OIDC token with Sigstore's Certificate Authority (Fulcio), which issues a short-lived X.509 certificate (valid ~10 minutes) binding the public key to the GitHub workflow identity.
4. The runner signs an in-toto provenance statement binding the artifact hash to the build metadata, logs the entry to the Rekor transparency log, and discards the private key.

---

### Q12: Why is pinning third-party GitHub Actions to a commit SHA safer than pinning to a release tag?
**Answer:**  
Release tags in Git (`@v4`) are mutable pointers. If an open-source action maintainer's account is compromised or malicious code is merged upstream, the attacker can force-update the tag `v4` to point to a malicious commit. Pinning to an immutable 40-character commit SHA (`@b4ffde...`) guarantees the exact code being executed cannot be altered without an explicit change to your workflow file.

---

### Q13: Explain the attack vector known as "Dependency Confusion" and how to mitigate it.
**Answer:**  
Dependency Confusion occurs when an application consumes an internal private package (e.g., `@corp/auth`). If an external attacker registers a public package with the identical name on public npm/PyPI with an excessively high version number (e.g., `99.9.9`), misconfigured package managers will fetch the malicious public package instead of the internal registry.
**Mitigation**: Enforce scoped packages (`@my-org/`), configure private registry lockfiles, and block unauthenticated public package fallbacks.

---

### Q14: An AWS root key was pushed to a public GitHub repo. What is your exact step-by-step incident response?
**Answer:**  
1. **Immediate Revocation (T+0)**: Deactivate and revoke the AWS key in the AWS IAM console immediately. (Never prioritize history surgery over key revocation; assume the key was exploited within seconds).
2. **Forensic Audit**: Query AWS CloudTrail logs for all API actions initiated under that key between the commit timestamp and the revocation timestamp.
3. **History Surgery**: Clone a bare mirror (`git clone --mirror`), purge the key using `git-filter-repo --replace-text`, and force-push (`git push --force --mirror`).
4. **Cache Invalidation**: Contact GitHub Support to invalidate dangling commit caches and reflogs on public servers.
5. **Post-Mortem**: Enable GitHub Push Protection globally to prevent recurrence.

---

### Q15: Why is `git-filter-repo` superior to `git filter-branch` and BFG for history surgery?
**Answer:**  
`git filter-branch` is officially deprecated because it executes subshells per commit, taking hours on large repos, and frequently corrupts annotated tags and merge metadata. BFG is a third-party Java tool with limited options. `git-filter-repo` is written in Python over Git's high-performance fast-import binary engine, processing 100,000 commits in seconds while accurately maintaining tree integrity and references.

---

### Q16: How do Fine-Grained Personal Access Tokens (FG-PATs) prevent the blast radius of Classic PAT leaks?
**Answer:**  
Classic PATs grant account-wide permissions across every repository the user can access with broad scopes (e.g., `repo` grants full write access to all code, releases, and settings). Fine-Grained PATs allow users and organizations to scope tokens to **specific repositories**, select **exact granular permissions** (e.g., Read issues, Write deployments), and mandate **enterprise administrator approval** before tokens become active.

---

### Q17: What security risks are introduced by using `pull_request_target` with an unvetted checkout?
**Answer:**  
`pull_request_target` executes in the context of the base default branch (`main`) with full access to repository secrets and write permissions. If the workflow checks out the head of the PR branch (`ref: ${{ github.event.pull_request.head.sha }}`) and runs commands (tests, builds, linter), an attacker can submit a PR containing arbitrary code execution in `package.json` scripts, exfiltrating all repository secrets to an external server.

---

### Q18: What is Coordinated Vulnerability Disclosure (CVD), and how does Private Vulnerability Reporting support it?
**Answer:**  
CVD is the practice of privately disclosing security flaws to maintainers, granting them a confidential window (typically 90 days) to patch the vulnerability before public release. **Private Vulnerability Reporting (PVR)** provides a secure, built-in communication channel on GitHub, allowing researchers to submit vulnerability reports directly to maintainers without exposing zero-day flaws in public GitHub issues.

---

### Q19: How does GitHub verify that an OIDC token issued to Actions was not spoofed or intercepted?
**Answer:**  
GitHub Actions signs the OIDC JSON Web Token (JWT) using an asymmetric private key whose public certificate is published at a well-known OpenID discovery endpoint (`https://token.actions.githubusercontent.com/.well-known/openid-configuration`). Cloud providers (AWS, GCP, Azure) download and cache these public keys, cryptographically verifying the token's signature, issuer (`iss`), and audience (`aud`) claims before exchanging temporary credentials.

---

### Q20: What is GitHub's Temporary Private Fork, and why is it used for developing security fixes?
**Answer:**  
When patching a security vulnerability reported via a GitHub Security Advisory, developing the fix in a public PR or public branch exposes the fix to adversaries before release binaries are available. A **Temporary Private Fork** is an ephemeral, isolated private repository linked to the advisory. Maintainers and researchers can collaborate, run CI tests, and review patches confidentially until the advisory and patched release are published simultaneously.

---

[<- Back to Interview Overview](./README.md) | [Next Note: 08-behavioural-and-project-discussion.md ->](./08-behavioural-and-project-discussion.md)
