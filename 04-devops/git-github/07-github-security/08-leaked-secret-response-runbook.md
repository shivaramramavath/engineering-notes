---
title: "Leaked Secret Incident Response Runbook & History Surgery"
category: "04-devops"
subcategory: "git-github / 07-github-security"
tags: ["git", "github", "incident-response", "secret-leak", "runbook", "git-filter-repo", "forensics", "security"]
---

# Leaked Secret Incident Response Runbook & History Surgery

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

When a production API key, database password, or private certificate is pushed to a remote GitHub repository, **assume the credential is immediately compromised**. Malicious automated scrapers continuously monitor public GitHub event streams and commit feeds, attempting to exploit leaked cloud credentials within seconds to provision cryptominers, exfiltrate databases, or pivot laterally.

A frantic or haphazard response often exacerbates the damage:
- Simply deleting the secret in a new commit (`git commit -m "remove secret"`) leaves the credential fully readable in past commit history.
- Rewriting Git history without first revoking the credential gives developers a false sense of security while adversaries continue using the active key.
- Uncoordinated force-pushes overwrite team branches, corrupt active pull requests, and disrupt production deployments.

This runbook establishes a rigorous, production-tested incident response protocol: **Revoke -> Assess -> Purge History -> Verify -> Prevent**.

---

## 2. Core Concepts

### The Golden Rule of Secret Remediation
> **Revoke the credential first, purge Git history second.**

Rewriting Git history is purely cosmetic hygiene to prevent future exposure. The moment a secret reaches the remote Git server, its confidentiality is lost. Security is restored exclusively by **invalidating and rotating** the credential in the downstream service provider (AWS, Stripe, Database, etc.).

### Why `git-filter-repo`?
Historically, developers used `git filter-branch` (officially deprecated due to severe data loss bugs and glacial performance) or the third-party Java tool BFG Repo-Cleaner. Today, **`git-filter-repo`** is the officially recommended, high-performance Python tool for Git repository surgery. It rewrites Git fast-import streams in C-speed, accurately purging blob contents, updating commit SHAs, and recalculating tree objects.

### Force-Push Coordination & Cache Invalidation
Rewriting repository history changes all subsequent commit SHAs. In GitHub:
1. All local and remote branches must be synchronized.
2. The rewritten tree is pushed using `git push origin --force --all --tags`.
3. GitHub caches dangling unreferenced commits and pull request diffs for days. Maintainers must contact GitHub Enterprise Support to request an explicit **reflog purge and cache invalidation** if the repository is public.

---

## 3. How It Works Internally

```
Incident Timeline & Execution Sequence
+-------------------------------------------------------------------------+
| PHASE 1: IMMEDIATE CONTAINMENT (T + 0 to 5 Minutes)                     |
| 1. Identify Leaked Credential (AWS IAM, Database URL, Stripe Key, etc.)|
| 2. Immediately REVOKE / DEACTIVATE the credential in the vendor console |
| 3. Issue new credential and update production secrets vault             |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| PHASE 2: FORENSIC INVESTIGATION (T + 5 to 30 Minutes)                   |
| 1. Query vendor audit logs (CloudTrail, Stripe Logs, DB access logs)    |
|    - Time window: Between commit timestamp and revocation timestamp     |
|    - Look for unauthorized IP addresses, API calls, or data exports     |
| 2. Determine blast radius (Did unauthorized reads/writes occur?)       |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| PHASE 3: HISTORY PURGING SURGERY (T + 30 to 60 Minutes)                 |
| 1. Fresh mirror clone: git clone --mirror git@github.com:org/repo.git   |
| 2. Run git-filter-repo to surgically scrub literal secret string        |
| 3. Inspect rewritten repository integrity                               |
| 4. Force-push rewritten refs: git push origin --force --mirror          |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| PHASE 4: POST-INCIDENT REVIEW & PREVENTION (T + 24 Hours)               |
| 1. Contact GitHub Support to flush cached pull request commits          |
| 2. Audit team: Re-clone instructions to prevent re-introducing bad refs |
| 3. Enable Push Protection & local pre-commit hooks                      |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Installing `git-filter-repo`
```bash
# Via Python pip
pip install git-filter-repo

# Verify installation
git filter-repo --version
```

### Scrubbing Literal String from All Commits
Create an expression file `replace-expressions.txt`:
```text
sk_live_51M0...92kd==>REDACTED_SECRET
password123==>REDACTED_SECRET
AKIAIOSFODNN7EXAMPLE==>REDACTED_AWS_KEY
```

Run `git-filter-repo`:
```bash
git filter-repo --replace-text replace-expressions.txt
```

---

## 5. Practical Examples

### Step-by-Step Incident Walkthrough

#### Step 1: Immediate Revocation & Rotation
```bash
# Example: Disable leaked AWS IAM access key immediately via AWS CLI
aws iam update-access-key \
  --access-key-id AKIAIOSFODNN7EXAMPLE \
  --status Inactive \
  --user-name deployer-service-account
```

#### Step 2: Fresh Mirror Clone of Repository
Never perform history surgery on an existing dirty developer working copy:
```bash
# Create temporary working directory
mkdir -p /tmp/git-surgery && cd /tmp/git-surgery

# Create a clean bare mirror clone containing all branches and tags
git clone --mirror git@github.com:my-org/payment-service.git
cd payment-service.git
```

#### Step 3: Purge File or Scrub Secret Literal
Option A: Completely delete an accidentally committed `.env` file from all commits:
```bash
git filter-repo --invert-paths --path .env --path config/secrets.json
```

Option B: Scrub the literal string across all files and commit messages:
```bash
echo "AKIAIOSFODNN7EXAMPLE==>REDACTED" > /tmp/expressions.txt
git filter-repo --replace-text /tmp/expressions.txt
```

#### Step 4: Verify the Purge
Confirm that the secret no longer exists in any object or branch:
```bash
# Search uncompressed Git objects for the leaked key
git log -S "AKIAIOSFODNN7EXAMPLE" --all
# (Should return zero output)

git grep "AKIAIOSFODNN7EXAMPLE" $(git rev-list --all)
# (Should return zero output)
```

#### Step 5: Force Push Rewritten Refs
```bash
# Re-add remote (git-filter-repo removes remotes as a safety precaution)
git remote add origin git@github.com:my-org/payment-service.git

# Force-push all rewritten branches and tags
git push origin --force --mirror
```

#### Step 6: Team Synchronization Playbook
Instruct all team members to re-clone the repository to prevent accidentally pushing old objects back:
```bash
# Developers must back up uncommitted changes and re-clone:
cd ~/workspace
mv payment-service payment-service-backup
git clone git@github.com:my-org/payment-service.git
```

---

## 6. Best Practices

- **Never Prioritize History Rewriting Over Key Revocation**: Invalidate the compromised token immediately. Even 60 seconds of key exposure can allow an automated bot to exfiltrate database contents.
- **Always Perform Surgery on a Bare Mirror Clone**: Cloning with `--mirror` ensures all remote tracking branches, release tags, and notes are processed uniformly.
- **Back Up the Original Clone Before Surgery**: Keep an offline zip backup of `payment-service.git` before executing `git-filter-repo` in case an essential commit is accidentally deleted.
- **Notify Affected Third Parties**: If customer data, production databases, or partner tokens were exposed, initiate organizational legal and compliance breach notification protocols.
- **Enable GitHub Push Protection Globally**: After the incident, enable Push Protection across the entire organization so no engineer can push that format of secret again.

---

## 7. Common Mistakes

| Mistake | Impact | Correct Approach |
|:---|:---|:---|
| Making a new commit deleting the secret | Secret remains in past commit objects; easily read via git log | Immediately revoke credential and scrub history with `git-filter-repo` |
| Using `git filter-branch` | Incredibly slow, corrupts annotated tags, leaves dangling backup refs | Use modern `git-filter-repo` |
| Running history surgery in an active working tree | Conflicts with uncommitted work, unmerged stashes, and dirty index | Run surgery in an isolated bare mirror clone (`git clone --mirror`) |
| Allowing developers to re-push from un-synced clones | Resurrects deleted commit objects back onto GitHub remote | Require all developers to back up and re-clone fresh copies |
| Forgetting to check pull request cache | Commit diffs remain viewable on GitHub via cached PR URLs | Contact GitHub Support to run an internal garbage collection |
| Skipping cloud audit log review | Fails to detect if an attacker used the leaked key during exposure | Conduct full audit log investigation (CloudTrail, DB access logs) |

---

## 8. Performance, Security & Cost Considerations

- **Performance**: `git-filter-repo` processes 100,000 commits in under 15 seconds because it operates directly on Git's internal fast-import binary stream.
- **Security**: History surgery permanently changes commit SHAs. Any downstream repositories, forks, or submodules referencing the old commit hashes will experience broken references.
- **Cost**: `git-filter-repo` is open source and free. Incident response time and forensic investigation represent the primary business costs.

---

## 9. Comparison Table

| Tool | Speed | Safety & Tag Handling | Status | Recommendation |
|:---|:---|:---|:---|:---|
| **`git-filter-repo`** | Extremely Fast (C-speed Python) | Preserves tags, rewrites commit SHAs cleanly | Modern Official Standard | **Use for all history surgery** |
| **BFG Repo-Cleaner** | Fast (Java) | Simple syntax, but handles edge cases poorly | Third-party / Maintenance mode | Viable for simple file deletions |
| **`git filter-branch`**| Extremely Slow (Shell scripts) | Frequently corrupts refs and submodules | Officially Deprecated | **Never use** |

---

## 10. Real-World Interview Questions

<details>
<summary>1. A developer accidentally commits an AWS secret access key to a public GitHub repo. What is your exact first action?</summary>
Immediately deactivate and revoke the AWS secret access key in the AWS IAM management console. Purging Git history is a secondary cleanup step; the secret must be rendered invalid in AWS first to prevent immediate exploitation by automated threat actors.
</details>

<details>
<summary>2. Why does deleting a file in a new commit fail to remove the secret from Git?</summary>
Git does not store diffs; it stores snapshots of content trees. Every committed file is stored as an immutable blob object in the `.git/objects` database. Even if a subsequent commit deletes the file, earlier commit objects still point to the blob. Anyone who clones the repo or views commit history can view the secret.
</details>

<details>
<summary>3. Why is `git-filter-repo` preferred over `git filter-branch`?</summary>
`git filter-branch` is deprecated by the Git project because it executes shell subprocesses per commit, making it excruciatingly slow on large repos, and it frequently corrupts annotated tags, merges, and commit metadata. `git-filter-repo` utilizes Git's high-performance fast-export/fast-import engine, executing in seconds while accurately maintaining tree integrity.
</details>

<details>
<summary>4. What happens when a team member runs `git pull` after an administrator has force-pushed a rewritten history?</summary>
Git will attempt to merge the rewritten remote branch with the developer's local branch containing the old commit history. This can resurrect the deleted commits, create massive merge conflicts, and push the scrubbed secret back up to the remote on the developer's next push.
</details>

<details>
<summary>5. Why do commit SHAs change across the entire repository history after running `git-filter-repo`?</summary>
Git commit hashes are calculated cryptographically using the tree SHA, parent commit SHAs, author, committer, and message. When a secret is modified or a file is removed in commit `N`, commit `N` receives a new SHA. Because commit `N+1` references commit `N` as its parent, its hash changes as well, causing a cascading recalculation of all subsequent descendant commits.
</details>

<details>
<summary>6. How does GitHub's Pull Request cache affect leaked secret remediation?</summary>
Even after force-pushing rewritten branches, GitHub retains cached commit objects associated with open or closed Pull Requests on its backend servers. To completely remove the leaked blob from GitHub's servers, repository administrators must contact GitHub Support to manually run `git gc --prune=now` and invalidate web caches.
</details>

<details>
<summary>7. What forensic steps must follow the revocation of a leaked database credential?</summary>
1. Query database query logs and connection logs for the exact timeframe between commit creation and credential revocation.  
2. Filter for IP addresses not belonging to authorized VPCs, NAT gateways, or VPNs.  
3. Assess whether data exfiltration (`SELECT * FROM users`) occurred.  
4. Document findings for the Post-Incident Review (PIR) and regulatory compliance teams.
</details>

<details>
<summary>8. How do you prevent developers from re-pushing scrubbed commits back to the server?</summary>
Enforce GitHub Push Protection with custom regex rules matching the leaked credential, and establish branch protection rulesets requiring strict status checks and linear history, blocking non-fast-forward pushes.
</details>

---

## 11. Hands-On Exercises

<details>
<summary>Exercise 1: Install `git-filter-repo` and verify environment</summary>

**Goal:** Ensure Python and `git-filter-repo` are installed.

```bash
pip install git-filter-repo
git filter-repo --version
```
</details>

<details>
<summary>Exercise 2: Create a dummy repo with a secret</summary>

**Goal:** Set up a test repository with a sensitive `.env` file.

```bash
mkdir /tmp/leak-test && cd /tmp/leak-test
git init
echo "API_KEY=super_secret_12345" > .env
git add .env && git commit -m "add env file"
echo "console.log('app running');" > app.js
git add app.js && git commit -m "add app code"
```
</details>

<details>
<summary>Exercise 3: Purge `.env` using `git-filter-repo`</summary>

**Goal:** Completely eliminate the `.env` file from all commits.

```bash
git filter-repo --invert-paths --path .env --force
```
</details>

<details>
<summary>Exercise 4: Verify the secret is completely gone</summary>

**Goal:** Check Git log and objects to verify zero trace of the secret.

```bash
git log --all --full-history -- .env
# (Must return nothing)
git log -S "super_secret_12345"
# (Must return nothing)
```
</details>

<details>
<summary>Exercise 5: Scrub literal text strings across all files</summary>

**Goal:** Replace sensitive IP addresses or passwords while keeping the files.

```bash
echo "192.168.1.100==>REDACTED_IP" > /tmp/replace.txt
git filter-repo --replace-text /tmp/replace.txt --force
```
</details>

<details>
<summary>Exercise 6: Draft a Post-Incident Review (PIR) template</summary>

**Goal:** Author a standard incident document covering Timeline, Root Cause, Impact, and Action Items.

```markdown
# PIR: Leaked Secret Incident [DATE]
- **Incident Summary**: Leaked Stripe secret key in commit 4b2a1c.
- **Timeline**: Pushed at 10:14 UTC, Revoked at 10:22 UTC (8 min exposure).
- **Forensic Assessment**: Zero unauthorized API calls detected in vendor logs.
- **Action Items**: Enable Push Protection organization-wide; implement pre-commit hooks.
```
</details>

---

## 12. Summary / Cheat Sheet

```text
================================================================================
                    LEAKED SECRET INCIDENT RUNBOOK
================================================================================
STEP 1 (IMMEDIATE) : REVOKE / ROTATE credential in vendor console immediately
STEP 2 (FORENSICS) : Inspect vendor audit logs (CloudTrail, Stripe, DB logs)
STEP 3 (CLONE)     : git clone --mirror git@github.com:org/repo.git
STEP 4 (SURGERY)   : git filter-repo --invert-paths --path <file>
                     OR git filter-repo --replace-text replace.txt
STEP 5 (VERIFY)    : git log -S "leaked_secret" --all (confirm empty)
STEP 6 (PUSH)      : git remote add origin <url> && git push origin --force --mirror
STEP 7 (TEAM)      : All engineers must backup dirty work and RE-CLONE fresh
STEP 8 (CLEANUP)   : Contact GitHub Support to invalidate cached PR diffs
================================================================================
```

---

## 13. Further Reading
- [Git Documentation: git-filter-repo](https://github.com/newren/git-filter-repo)
- [GitHub Docs: Removing sensitive data from a repository](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/removing-sensitive-data-from-a-repository)
- [CISA: Incident Response Principles & Runbooks](https://www.cisa.gov/resources-tools/resources/incident-response-plan-basics)
- [AWS Security: What to do if an AWS access key is exposed](https://aws.amazon.com/premiumsupport/knowledge-center/potential-account-compromise/)

---

[<- Previous: Least Privilege Tokens & Permissions](./07-least-privilege-tokens-and-permissions.md) | [Module 07 Overview](./README.md) | [Next: Module 08 Overview ->](../08-gh-cli-and-api/README.md)
