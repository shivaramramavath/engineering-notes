---
title: "GitHub Secret Scanning & Push Protection"
category: "04-devops"
subcategory: "git-github / 07-github-security"
tags: ["git", "github", "secret-scanning", "push-protection", "security", "devsecops", "ghas", "credentials"]
---

# GitHub Secret Scanning & Push Protection

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

Hardcoded credentials—API keys, database connection strings, SSH private keys, and OAuth access tokens—are the single most common attack vector leading to cloud infrastructure compromises. Once pushed to a remote Git repository, a secret is immediately exposed to anyone with read access and is permanently stored in Git object history unless surgically rewritten.

**GitHub Secret Scanning** continuously analyzes repositories across commits, branches, issues, and pull request comments to identify known secret formats. It operates in two complementary modes:
1. **Secret Scanning for Partners (Post-receive)**: Enabled for all public repositories by default and available for private repositories with GitHub Advanced Security (GHAS). When a partner secret (AWS, Stripe, Slack, etc.) is found, GitHub immediately alerts the provider to automatically revoke or restrict the credential.
2. **Push Protection (Pre-receive)**: Blocks the developer's `git push` directly at the network boundary before commits ever land on GitHub's servers, eliminating incident response and history rewrite requirements.

Without proactive push protection, teams endure severe operational friction: credential rotation, audit log reviews, service downtime, and legal disclosures under regulations like GDPR, SOC 2, and PCI-DSS.

---

## 2. Core Concepts

### Partner Patterns vs Custom Patterns
- **Partner Patterns**: GitHub collaborates with over 150 industry partners (AWS, OpenAI, Datadog, HashiCorp, etc.) who supply proprietary regular expressions and validation webhooks. When an AWS key (`AKIA...`) is detected in a public repository, AWS is notified via high-priority webhook within seconds.
- **Custom Patterns**: Organizations can define regular expressions, pre-substrings, post-substrings, and Shannon entropy thresholds to detect internal proprietary tokens, database URIs, or custom JWT patterns specific to their business.

### Push Protection Mechanics
When push protection is enabled on an organization or repository:
1. The developer executes `git push origin feature-branch`.
2. GitHub runs a pre-receive hook evaluating all incoming commits, trees, and blobs against secret detection rules.
3. If a pattern matches, the push is **rejected** with an actionable terminal message containing a direct URL to bypass the block if the finding is confirmed to be a false positive or test fixture.

### Bypass Reasons & Audit Trails
If push protection blocks a commit, a developer can only bypass it by selecting one of three explicit justifications:
- `False positive`: The string looks like a secret but is not (e.g., placeholder or documentation).
- `Used in tests`: The string is a mock secret or fixture incapable of production authentication.
- `To be fixed later`: The developer acknowledges the real secret and will rotate or remove it shortly (triggers an alert to security admins).

All bypasses generate immediate security alerts and append immutable entries to the enterprise audit log.

---

## 3. How It Works Internally

```
Developer Local Environment                  GitHub Remote Infrastructure
+-------------------------+                 +-------------------------------+
| git commit -m "add key" |                 |                               |
|            |            |                 |                               |
|      [Local Blob]       |                 |                               |
|            |            |                 |                               |
| git push origin main    | === SSH/HTTPS =>| [Pre-Receive Hook Boundary]   |
+-------------------------+                 |   |                           |
                                            |   v                           |
                                            | Scan incoming packfile blobs  |
                                            | against Pattern Engine        |
                                            |   |                           |
                                            |   +--> MATCH FOUND?           |
                                            |        |                      |
                                            |   +----+----+                 |
                                            |   |         |                 |
                                            |  YES        NO                |
                                            |   |         |                 |
                                            |   |         v                 |
                                            |   |    Accept Push & Update   |
                                            |   |    Branch Ref (refs/heads)|
                                            |   v                           |
                                            | Reject push with 403 / Hook   |
                                            | failure & generate bypass URL |
                                            +-------------------------------+
```

The Git server parses the uncompressed object packfile. It iterates through the blob objects introduced by new commits. Because the scan occurs during the `pre-receive` hook execution, no refs are updated, no webhooks trigger, and no other users can fetch or see the commit until the hook completes with exit code `0`.

---

## 4. Syntax & Usage

### Enabling Secret Scanning via GitHub CLI
```bash
# Check GHAS and Secret Scanning status on a repository
gh repo view my-org/payment-service --json security_and_analysis

# Enable Secret Scanning on a repository
gh api --method PATCH -H "Accept: application/vnd.github+json" \
  /repos/my-org/payment-service \
  -f security_and_analysis[secret_scanning][status]=enabled

# Enable Push Protection on a repository
gh api --method PATCH -H "Accept: application/vnd.github+json" \
  /repos/my-org/payment-service \
  -f security_and_analysis[secret_scanning_push_protection][status]=enabled
```

### Terminal Output During Push Protection Rejection
```text
remote: error: GH013: Repository rule violations found for refs/heads/feature-payments.
remote: 
remote: - GITHUB SECRET SCANNING
remote:   ------------------------------------------------------------------
remote:   [To resolve, follow the instructions in the link below]
remote: 
remote:   - (1/1) Stripe API Secret Key
remote:     Commit: 3b1a8f9c2d1e0f4a7b8c9d0e1f2a3b4c5d6e7f8a
remote:     Path: services/billing/stripe.ts:14
remote:     Secret: sk_live_51M0...92kd
remote: 
remote:   Resolve the issue by removing the secret from your commit history:
remote:     https://docs.github.com/articles/removing-sensitive-data-from-a-repository
remote: 
remote:   Or, if this is a false positive, unblock this push by visiting:
remote:     https://github.com/my-org/payment-service/security/secret-scanning/unblock-secret/2bK9...
```

---

## 5. Practical Examples

### Example 1: Local Pre-Commit Secret Prevention with Gitleaks
Prevent secrets before pushing by running `gitleaks` via Git pre-commit hooks:
```yaml
# .pre-commit-config.yaml
repos:
  - repo: https://github.com/gitleaks/gitleaks
    rev: v8.18.2
    hooks:
      - id: gitleaks
```
Run manually:
```bash
gitleaks detect --source . --verbose
```

### Example 2: Defining Custom Secret Pattern via Organization API
Define a custom regex pattern to detect internal API tokens prefixed with `acme_sec_`:
```bash
gh api --method POST -H "Accept: application/vnd.github+json" \
  /orgs/my-org/secret-scanning/custom-patterns \
  -f name="Acme Internal API Token" \
  -f pattern="acme_sec_[0-9a-zA-Z]{32}" \
  -f push_protection=true
```

### Example 3: Listing Active Secret Alerts via GitHub CLI
```bash
# List all open secret scanning alerts in JSON
gh api -H "Accept: application/vnd.github+json" \
  /repos/my-org/payment-service/secret-scanning/alerts \
  --jq '.[] | {number: .number, secret_type: .secret_type, state: .state, created_at: .created_at}'
```

### Example 4: Closing an Alert Programmatically
```bash
# Resolve alert #42 as revoked
gh api --method PATCH -H "Accept: application/vnd.github+json" \
  /repos/my-org/payment-service/secret-scanning/alerts/42 \
  -f state="resolved" \
  -f resolution="revoked"
```

### Example 5: Auditing Push Protection Bypasses via Enterprise Audit Log
```bash
# Query all push protection bypass events in the organization
gh api -H "Accept: application/vnd.github+json" \
  "/orgs/my-org/audit-log?phrase=action:secret_scanning_push_protection.bypass" \
  --jq '.[] | {actor: .actor, repo: .repo, created_at: .created_at, reason: .reason}'
```

---

## 6. Best Practices

- **Enforce Push Protection at the Organization Level**: Do not leave push protection as an opt-in per-repository setting. Enable it globally across all enterprise repositories.
- **Mandate Delegate Approval for Bypasses**: Configure GHAS settings so junior engineers cannot unilaterally bypass push protection without security team approval.
- **Automate Local Scanning**: Use local hooks (`gitleaks`, `trufflehog`) in developer environments to catch secrets even before the network push attempt.
- **Use Mock Secrets in Unit Tests**: Never use expired or truncated production keys in tests. Generate synthetic alphanumeric strings (`test_dummy_token_000000000000`).
- **Leverage Secret Stores**: Inject secrets at runtime using environment variables from HashiCorp Vault, AWS Secrets Manager, or GitHub Secrets rather than local config files.

---

## 7. Common Mistakes

| Mistake | Impact | Correct Approach |
|:---|:---|:---|
| Bypassing push protection with "fix later" | Secret lands on GitHub and is immediately cached in reflogs and search index | Undo local commit using `git reset HEAD~1` and remove secret before pushing |
| Overwriting a secret in a subsequent commit | Secret remains in earlier Git history and is easily retrieved via blob SHA | Rewrite commit history using `git-filter-repo` before pushing |
| Pushing dummy keys that match real vendor formats | Blocks developer pipelines on false positives | Use vendor-sanctioned test formats or custom mock strings |
| Ignoring partner alerts for public repos | Adversaries scrape GitHub event feeds in sub-second latency and compromise keys | Treat every post-receive alert as an active breach; rotate credential immediately |
| Assuming `.gitignore` protects existing files | Tracked files are never ignored by `.gitignore` | Untrack file using `git rm --cached <file>` and purge history |
| Disabling push protection to unblock urgent release | High probability of severe credential leak under pressure | Remove key from config and inject via CI environment variables |

---

## 8. Performance, Security & Cost Considerations

- **Performance**: Pre-receive scanning adds between 100ms and 500ms to `git push` execution times depending on the packfile size and the number of custom regex patterns configured.
- **Security**: Push protection blocks leakage at the boundary. However, it does not scan existing git history retroactively; retroactive scanning occurs asynchronously upon GHAS activation.
- **Cost**: Secret scanning for partner alerts on public repositories is free. Push protection and custom patterns on private repositories require GitHub Enterprise Cloud and a GitHub Advanced Security (GHAS) license.

---

## 9. Comparison Table

| Feature | Secret Scanning (Post-Receive) | Push Protection (Pre-Receive) | Gitleaks (Client-Side Hook) |
|:---|:---|:---|:---|
| **Execution Point** | After push lands on server | During `git push` before ref update | Before commit on local workstation |
| **Prevents Leak?** | No (Generates alert after leak) | Yes (Blocks push completely) | Yes (Blocks commit locally) |
| **License Requirement**| Free for Public; GHAS for Private | GHAS for Private repos | Free & Open Source |
| **Bypass Trail** | N/A (Secret is already published) | Recorded in Enterprise Audit Log | No central audit (local bypass) |
| **Partner Notification**| Automatic partner revokes | Never triggered (secret never leaves client)| Never triggered |

---

## 10. Real-World Interview Questions

<details>
<summary>1. How does GitHub Push Protection physically block a developer from pushing a secret?</summary>
GitHub hooks into Git's native server-side `pre-receive` hook mechanism. When a developer pushes, Git transmits packfiles containing new objects. The pre-receive hook inspects the blobs against partner and custom regular expressions. If a match occurs without a valid bypass token, the hook exits with a non-zero code, causing the Git daemon to abort ref updates and reject the push over the wire.
</details>

<details>
<summary>2. If a secret is committed, pushed, and then deleted in the very next commit, why is it still vulnerable?</summary>
Git is an append-only content-addressable database. Commits store snapshots of trees, and every file is preserved as an immutable blob. Deleting the file in commit `B` merely points tree `B` away from the blob, but commit `A` still references the blob object. Anyone who clones the repository can inspect commit `A` or grep the loose packfiles to read the raw secret.
</details>

<details>
<summary>3. What is the difference between a partner pattern and an enterprise custom pattern?</summary>
Partner patterns are maintained by GitHub in partnership with third-party vendors (AWS, Stripe, Azure, etc.) and include automated remediation protocols (e.g., AWS can auto-revoke compromised IAM keys). Custom patterns are defined by organization administrators using regular expressions to detect internal proprietary tokens, with configurable push protection and entropy criteria.
</details>

<details>
<summary>4. How does an enterprise detect if an engineer bypassed Push Protection using 'False Positive' for a real secret?</summary>
Push protection bypasses emit an immutable event to the GitHub Enterprise Audit Log (`secret_scanning_push_protection.bypass`). Security teams can stream this audit log to an external SIEM (Splunk, Datadog) to alert immediately on any bypass event, requiring the engineer to provide evidence or triggering a security review.
</details>

<details>
<summary>5. Can secret scanning detect secrets inside binary files or zip archives?</summary>
GitHub Secret Scanning primarily inspects text files and known code structures. It does not inspect compiled binaries, encrypted payloads, or deeply nested compressed archives (`.zip`, `.tar.gz`) during pre-receive push protection due to latency constraints.
</details>

<details>
<summary>6. How can a team safely test Push Protection without triggering genuine security alerts?</summary>
GitHub supports vendor-specific test tokens (such as GitHub personal access tokens formatted specifically for testing, e.g., starting with `ghp_` followed by test characters) that trigger push protection without initiating actual token revocation webhooks.
</details>

<details>
<summary>7. What should be done immediately if Secret Scanning flags an AWS root key in public history?</summary>
1. Treat the credential as fully compromised.  
2. Immediately rotate or deactivate the key in the AWS IAM console.  
3. Review AWS CloudTrail logs for unauthorized API calls originated from the leak timestamp.  
4. Purge the commit history using `git-filter-repo` and force-push, or delete and recreate the repository if public exposure was extensive.
</details>

<details>
<summary>8. How do you configure Push Protection to require managerial or security delegate approval?</summary>
Under GitHub Enterprise settings for Code Security, administrators can enable "Push protection delegation". When enabled, selecting "Bypass" in the terminal does not automatically permit the push; instead, it generates an approval request routed to designated repository security managers.
</details>

---

## 11. Hands-On Exercises

<details>
<summary>Exercise 1: Inspect Secret Scanning settings via CLI</summary>

**Goal:** Check if secret scanning and push protection are enabled on your repository.

```bash
gh api /repos/:owner/:repo/security_and_analysis \
  --jq '.secret_scanning.status, .secret_scanning_push_protection.status'
```
</details>

<details>
<summary>Exercise 2: Simulate Push Protection Rejection</summary>

**Goal:** Commit a dummy credential formatted to trigger a partner pattern and verify push failure.

```bash
# Add a dummy Stripe test key
echo "STRIPE_TEST=sk_test_51Abcdefghijklmnopqrstuvwx" >> .env
git add .env
git commit -m "add test key"
git push origin feature-test
# Observe GH013 error output in terminal
```
</details>

<details>
<summary>Exercise 3: Clean up the blocked commit locally</summary>

**Goal:** Remove the offending commit without bypassing push protection.

```bash
# Reset head by 1 commit while keeping files in working directory
git reset HEAD~1

# Remove the secret from .env and stage proper changes
git checkout .env
git status
```
</details>

<details>
<summary>Exercise 4: Install and run Gitleaks locally</summary>

**Goal:** Catch secrets on your developer workstation before pushing.

```bash
# Run gitleaks against local repository
gitleaks detect --verbose --no-git
```
</details>

<details>
<summary>Exercise 5: Create a custom secret pattern definition</summary>

**Goal:** Define a JSON payload for a proprietary internal token format `CORP_KEY_[A-Z0-9]{20}`.

```json
{
  "name": "Internal Corp Key",
  "pattern": "CORP_KEY_[A-Z0-9]{20}",
  "push_protection": true
}
```
</details>

<details>
<summary>Exercise 6: Query open secret alerts via GitHub CLI</summary>

**Goal:** Filter open alerts by secret type.

```bash
gh api /repos/:owner/:repo/secret-scanning/alerts \
  --jq '.[] | select(.state == "open") | {id: .number, type: .secret_type}'
```
</details>

---

## 12. Summary / Cheat Sheet

```text
================================================================================
                    GITHUB SECRET SCANNING & PUSH PROTECTION
================================================================================
Post-Receive Scanning : Scans commits after push -> Generates alert / Notifies partner
Push Protection       : Pre-receive hook -> Rejects git push at server boundary
Bypass Reasons        : False positive | Used in tests | Fix later
Audit Logging         : Every bypass emits event secret_scanning_push_protection.bypass
Key CLI Command       : gh api /repos/:owner/:repo/secret-scanning/alerts
Remediation           : Rotate secret immediately -> Purge git history with git-filter-repo
================================================================================
```

---

## 13. Further Reading
- [GitHub Documentation: About secret scanning](https://docs.github.com/en/code-security/secret-scanning/about-secret-scanning)
- [GitHub Documentation: Protecting pushes with secret scanning](https://docs.github.com/en/code-security/secret-scanning/protecting-pushes-with-secret-scanning)
- [GitHub Documentation: Custom patterns for secret scanning](https://docs.github.com/en/code-security/secret-scanning/defining-custom-patterns-for-secret-scanning)
- [Gitleaks Documentation & Rulesets](https://github.com/gitleaks/gitleaks)

---

[<- Previous: Module 06 Overview](../06-workflows-and-strategies/README.md) | [Module 07 Overview](./README.md) | [Next: Dependabot & Dependency Review ->](./02-dependabot-and-dependency-review.md)
