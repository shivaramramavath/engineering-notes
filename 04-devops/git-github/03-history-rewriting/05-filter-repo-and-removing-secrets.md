---
title: "Purging Secrets and Large Files: git-filter-repo"
category: "04-devops"
subcategory: "git-github / 03-history-rewriting"
tags: ["git", "git-filter-repo", "secrets", "security", "history-rewriting", "bfg", "credential-leak"]
difficulty: "Advanced"
estimated_read_time: "28 min"
prerequisites:
  - "01-fundamentals/05-git-objects"
  - "01-fundamentals/07-packfiles-and-garbage-collection"
  - "03-history-rewriting/02-rebase"
last_updated: "2026-11-25"
---

# Purging Secrets and Large Files: `git-filter-repo`

> **Navigation**: [← Previous: 04-reflog-and-recovery.md](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/04-reflog-and-recovery.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/README.md) | [Next: 06-rewriting-shared-history-safely.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/06-rewriting-shared-history-safely.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices & Incident Response](#6-best-practices--incident-response)
7. [Common Mistakes](#7-common-mistakes)
8. [Performance, Security & Cost](#8-performance-security--cost)
9. [Tool Comparison: filter-repo vs BFG vs filter-branch](#9-tool-comparison-filter-repo-vs-bfg-vs-filter-branch)
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is `git-filter-repo`?
`git-filter-repo` is the modern, official Git-recommended Python tool for rewriting repository history at scale. It replaces the deprecated and dangerous `git filter-branch` command and provides a high-performance, versatile utility for stripping sensitive credentials, deleting accidentally committed large binaries, restructuring directories, or splitting monorepos into separate repositories.

### Why Do We Need It?
1. **Permanently Eradicating Leaked Secrets**: If an AWS secret access key, database password, or private TLS key is committed and pushed, creating a subsequent commit that deletes the file (`git rm`) does **not** remove the secret from history. The leaked credential remains indefinitely in Git's object graph across prior commits.
2. **Purging Repository Bloat**: Large files (database dumps, videos, compiled binaries) bloat clone and fetch times for all developers forever. Removing the file from recent commits leaves the multi-gigabyte blob in packfiles unless history is rewritten.
3. **Speed and Safety**: Unlike legacy `git filter-branch`, which was painfully slow (often taking hours on medium repositories) and prone to mangling merge commits and reflogs, `git-filter-repo` runs orders of magnitude faster using Git's `fast-export`/`fast-import` stream engine.

---

## 2. Core Concepts

### 1. Complete History Rewriting
`git-filter-repo` operates across every branch, tag, and commit in the repository. When you instruct it to purge a file:
- It removes the file entry from every tree object across history.
- Every commit that touched that file receives a new SHA hash.
- All downstream descendant commits and tags are rewritten with new hashes.
- Original remotes are wiped locally as a safety measure to prevent accidental pushing.

### 2. The Danger of Deprecated `git filter-branch`
Git 2.24+ formally deprecated `git filter-branch` with explicit warning banners:
- It uses subshells and disk-based file checkouts per commit, resulting in abysmal performance ($O(N \cdot M)$).
- It regularly corrupts signed tags, committer timestamps, and merge topologies.
- It leaves backup refs (`refs/original/`) that preserve the sensitive data locally.

### 3. Immediate Revocation Principle
> **Rule 0 of Secret Leaks**: Rewriting history removes the credential from the Git graph, but it does **not** neutralize the leak if the commit was already pushed to a public or shared remote. You must **revoke and rotate the credential immediately** before spending time rewriting Git history.

---

## 3. How It Works Internally

```
                   Fast-Export / Fast-Import Architecture

┌─────────────────┐       ┌──────────────────────┐       ┌─────────────────┐
│ Git Repository  │       │   git-filter-repo    │       │ Cleaned Git     │
│ (Object DB)     ├──────►│   Python Filter Engine ├─────►│ Repository     │
└─────────────────┘       └──────────────────────┘       └─────────────────┘
  git fast-export          1. Parse stream in memory       git fast-import
  (Stream of commits,      2. Strip matching paths         (Rebuilds clean
   trees, blobs)           3. Redact secret text regex     packfiles and refs)
                           4. Recompute tree & commit SHAs
```

### Steps Executed by `git-filter-repo`:
1. **Safety Preflight**: Verifies that the working copy is a fresh clone and has no uncommitted changes. By default, it refuses to run unless the clone is fresh or `--force` is specified.
2. **Fast-Export Stream Parsing**: Streams the commit graph via `git fast-export` without checking out files to disk.
3. **In-Memory Filtering**: Python callbacks or regex engines inspect file paths and blob contents, filtering out targeted items.
4. **Fast-Import Stream Ingestion**: Feeds the sanitized stream to `git fast-import`, which generates optimized new packfiles.
5. **Remote Reference Disconnection**: Automatically drops all configured remotes (`origin`) to prevent developers from accidentally pushing mangled history until thoroughly inspected.

---

## 4. Syntax & Usage

### Installation
```bash
# Via pip
pip install git-filter-repo

# Via Homebrew (macOS)
brew install git-filter-repo

# Via apt (Ubuntu/Debian)
sudo apt-get install git-filter-repo
```

### Core Invocations
```bash
# 1. Analyze repository sizes and find biggest historical files
git-filter-repo --analyze
# (Generates detailed reports in .git/filter-repo/analysis/)

# 2. Delete a specific file across all commits and branches
git-filter-repo --path "path/to/credentials.json" --invert-paths

# 3. Delete an entire directory across history
git-filter-repo --path "secret-folder/" --invert-paths

# 4. Strip files larger than 50 megabytes
git-filter-repo --strip-blobs-bigger-than 50M

# 5. Replace leaked secret string with a sanitized placeholder
git-filter-repo --replace-text expressions.txt
```

---

## 5. Practical Examples

### Example 1: Completely Eradicating an Accidental `.env` File
```bash
# 1. Clone a fresh bare or mirror copy of the repo
git clone --mirror git@github.com:org/app.git app-clean
cd app-clean

# 2. Run git-filter-repo to purge .env across all branches & tags
git-filter-repo --path .env --invert-paths

# 3. Verify .env is completely absent from all history
git log --all --full-history -- ".env"
# (Output must be empty!)

# 4. Re-add origin remote and force push rewritten refs to server
git remote add origin git@github.com:org/app.git
git push origin --force --all
git push origin --force --tags
```

### Example 2: Replacing Leaked API Keys Across History (`--replace-text`)
```bash
# 1. Create a replacements file 'secrets-to-scrub.txt'
# Format: regex_literal_match==>replacement
cat << 'EOF' > secrets-to-scrub.txt
AIzaSyD-FakeGoogleApiKey12345678==>REDACTED_API_KEY
AKIAIOSFODNN7EXAMPLE==>REDACTED_AWS_KEY
wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY==>REDACTED_AWS_SECRET
EOF

# 2. Run text replacement
git-filter-repo --replace-text secrets-to-scrub.txt

# 3. Verify text has been scrubbed
git grep "AKIAIOSFODNN7EXAMPLE" $(git rev-list --all)
# (Output will be empty)
```

### Example 3: Purging Accidental 100MB Database Dump
```bash
# Analyze where bloat originates
git-filter-repo --analyze
cat .git/filter-repo/analysis/blob-shas-and-paths.txt | head -n 20

# Strip any blob exceeding 20MB
git-filter-repo --strip-blobs-bigger-than 20M

# Re-pack repository and purge unreferenced objects
git reflog expire --expire=now --all
git gc --prune=now --aggressive
```

### Example 4: Extracting a Single Subdirectory into Its Own Standalone Repository
```bash
# Convert a monolithic subfolder into the root of a new repository
git-filter-repo --subdirectory-filter packages/auth-service

# Result:
# The repository now only contains packages/auth-service, relocated to the repo root!
# All unrelated commits outside this subdirectory are pruned!
```

---

## 6. Best Practices & Incident Response

### Secret Leak Emergency Protocol (The 5-Step Runbook):
```
[ Step 1: REVOKE IMMEDIATELY ]
  ├── Invalidate API Key, IAM role, or TLS cert in Cloud Console
  └── Generate new credential for active services
         │
         ▼
[ Step 2: NOTIFY SECURITY TEAM ]
  └── Open incident ticket; assess access logs for compromise
         │
         ▼
[ Step 3: FRESH CLONE BACKUP ]
  └── git clone --mirror git@github.com:org/repo.git repo-backup
         │
         ▼
[ Step 4: PURGE WITH git-filter-repo ]
  └── git-filter-repo --path <secret-file> --invert-paths
  └── Run verification: git log --all --full-history -- <secret-file>
         │
         ▼
[ Step 5: COORDINATED FORCE PUSH & RESYNC ]
  ├── Push cleaned refs: git push origin --force --all --tags
  ├── Notify team: All collaborators must re-clone or rebase
  └── Contact GitHub Support to purge orphaned cached commit views
```

---

## 7. Common Mistakes

| Mistake | Consequence | Root Cause | Correct Procedure |
|:---|:---|:---|:---|
| Deleting file with `git rm` and committing | Secret is still present in history | `git rm` only alters HEAD, leaving past commits intact | Use `git-filter-repo` to rewrite history |
| Rewriting history without revoking key | Attacker scans GitHub archive and steals key anyway | Public push exposes key within seconds to automated scrapers | Revoke key first, clean Git second |
| Running `git filter-branch` | Slow execution, corrupted tags, incomplete deletion | Using deprecated legacy command | Use `git-filter-repo` |
| Collaborators running `git pull` after rewrite | Old history merged right back into repo | Pull merges remote with old local branches | Team must re-clone or reset hard |
| Forgetting GitHub commit cache | Commits still viewable via direct URL on GitHub | GitHub retains loose objects in internal caches | Request GitHub Support cache purge |
| Running on dirty working tree | Overwrites or loses unstaged local work | Non-clean repository | Always run on a fresh `--mirror` clone |

---

## 8. Performance, Security & Cost

- **Execution Speed**: Benchmarks demonstrate `git-filter-repo` processes 100,000 commits in approximately 10–20 seconds, whereas `git filter-branch` requires several hours.
- **Repository Size Reduction**: Purging accidental large binaries and running `git gc --prune=now --aggressive` typically reduces repository clone size by 50% to 95%.
- **GitHub Cached Commit Views**: Even after force-pushing rewritten branches, GitHub caches commit objects for a period of time. Anyone with the direct 40-character SHA can view the commit in GitHub UI. Submitting a ticket to GitHub Support requesting an immediate cache purge is required for high-risk leaks.

---

## 9. Tool Comparison: filter-repo vs BFG vs filter-branch

| Feature | `git-filter-repo` | BFG Repo-Cleaner | `git filter-branch` |
|:---|:---|:---|:---|
| **Official Status** | **Official Git Recommended** | Third-party Java tool | **Officially Deprecated** |
| **Runtime Dependency** | Python 3 | Java Runtime (JRE) | Git built-in (Shell) |
| **Speed** | Extremely Fast ($O(N)$ stream) | Very Fast ($O(N)$ multi-threaded) | Extremely Slow ($O(N \cdot M)$) |
| **Text Redaction** | Built-in regex (`--replace-text`) | Simple literal replacement | Complex sed commands |
| **Tag Handling** | Preserves annotated & signed tags | Updates tags | Regularly corrupts tags |
| **Safety Defaults** | Wipes remotes, verifies fresh clone | Requires manual cleanup | Leaves `refs/original/` |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: Why does running `git rm secret.key && git commit -m "delete secret"` fail to resolve a security leak?</strong></summary>

Git is an append-only content-addressable database. A commit object references a tree representing the repository state at that instant, which points back to parent commits. Running `git rm` creates a new commit whose tree no longer includes `secret.key`. However, all preceding commits in the history still contain tree objects pointing to the blob containing the secret. Anyone who clones the repository can inspect past commits (`git checkout <past-commit>` or `git cat-file -p <blob-sha>`) and recover the secret.
</details>

<details>
<summary><strong>Q2: Why was `git filter-branch` officially deprecated by the Git project?</strong></summary>

1. **Abysmal Performance**: It checks out trees to the filesystem and launches subshells for every single commit. On repositories with thousands of commits, it takes hours or days.
2. **Data Corruption**: It frequently corrupts complex merge commit graphs, mangles author/committer timestamps, and strips annotated tags.
3. **Safety Hazards**: It leaves backup references (`refs/original/`) that preserve the sensitive files locally, leading users to believe the file was purged when it remains on disk.
</details>

<details>
<summary><strong>Q3: Why must you immediately revoke a leaked credential before attempting to scrub it from Git history?</strong></summary>

Automated bots continuously monitor the GitHub public events stream (firehose) and public repositories. Studies show leaked AWS keys and tokens are scanned and compromised within seconds of being pushed. Rewriting history takes time to execute, review, and force-push. If the key is not immediately revoked at the cloud provider, attackers will already have leveraged it before the commit can be erased.
</details>

<details>
<summary><strong>Q4: Why does `git-filter-repo` automatically remove your remote (`origin`) upon completion?</strong></summary>

Rewriting history changes the SHA hashes of every affected commit and its descendants. Pushing these changes requires a destructive force push (`--force`). To prevent an engineer from accidentally running `git push` before inspecting and validating the filtered repository, `git-filter-repo` intentionally deletes all remote definitions as a fail-safe measure.
</details>

<details>
<summary><strong>Q5: What steps must team members take after a repository's history has been rewritten with `git-filter-repo`?</strong></summary>

Team members must **NOT** simply run `git pull`. Running `git pull` will attempt a merge or rebase between the newly rewritten upstream commits and their local obsolete commits, which will reintroduce the purged files and duplicate commits.
Collaborators must either:
1. Re-clone the repository from scratch (recommended), or
2. Fetch the new refs and hard reset their local branches:
   ```bash
   git fetch origin
   git reset --hard origin/main
   ```
</details>

<details>
<summary><strong>Q6: How does `git-filter-repo --replace-text` handle secret replacement?</strong></summary>

It accepts a configuration file containing patterns and replacements formatted as `regex==>replacement`. When processing the `git fast-export` stream, it applies these substitutions directly against blob payloads in memory before writing new blob objects. Any commit that contained the leaked string is updated so the blob contains the replacement string instead.
</details>

<details>
<summary><strong>Q7: What is a "ghost" or "cached" commit on GitHub, and how do you purge it after rewriting history?</strong></summary>

When you force-push rewritten branches to GitHub, the old commits become unreachable by normal branch pointers. However, GitHub's backend caches commit objects for web UI views and pull request diffs. If someone has the exact 40-character SHA of the old commit, they can still view it in a web browser (`https://github.com/org/repo/commit/<sha>`). To completely eradicate the commit, the repository admin must open a ticket with GitHub Support asking to clear the cached views and run an immediate garbage collection.
</details>

<details>
<summary><strong>Q8: How do you extract a single directory into its own repository while preserving its complete history using `git-filter-repo`?</strong></summary>

```bash
git-filter-repo --subdirectory-filter path/to/subfolder
```
This command filters the entire repository such that `path/to/subfolder` becomes the new repository root. Commits that only touched files outside this subfolder are pruned entirely, while commits that modified files within the subfolder have their paths rewritten to the root.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Analyze Repository Space with `git-filter-repo`</strong></summary>

**Scenario**: Generate analysis reports on a repository to locate the largest historical blobs.
```bash
git-filter-repo --analyze
ls -la .git/filter-repo/analysis/
head -n 20 .git/filter-repo/analysis/blob-shas-and-paths.txt
```
</details>

<details>
<summary><strong>Exercise 2: Purge a Leaked Password File Across All Commits</strong></summary>

**Scenario**: Simulate an accidental commit of `passwords.txt`, then purge it across history.
```bash
mkdir secret-lab && cd secret-lab && git init
echo "code" > main.py && git add main.py && git commit -m "feat: initial commit"

# Accidental leak
echo "supersecret_password_123" > passwords.txt
git add passwords.txt && git commit -m "wip: add credentials"
echo "more code" >> main.py && git commit -am "feat: finish service"

# Purge file using git-filter-repo
git-filter-repo --path passwords.txt --invert-paths --force

# Verify passwords.txt is gone from all commits
git log --all --full-history -- passwords.txt
```
</details>

<details>
<summary><strong>Exercise 3: Scrub Leaked API Key String with Replacement</strong></summary>

**Scenario**: Replace an inlined API key in source code across all historical commits.
```bash
echo "KEY=AKIAIOSFODNN7SECRET123" >> main.py && git commit -am "feat: hardcoded key"

cat << 'EOF' > scrub.txt
AKIAIOSFODNN7SECRET123==>REDACTED_KEY
EOF

git-filter-repo --replace-text scrub.txt --force

# Verify key string is replaced
git grep "AKIAIOSFODNN7SECRET123" $(git rev-list --all) || echo "Scrubbed successfully!"
```
</details>

<details>
<summary><strong>Exercise 4: Purge Blobs Exceeding Size Threshold</strong></summary>

**Scenario**: Create a large dummy file, commit it, and use `--strip-blobs-bigger-than` to remove it.
```bash
# Create 10MB dummy file
dd if=/dev/urandom of=large_asset.bin bs=1M count=10 2>/dev/null || fsutil file createnew large_asset.bin 10485760
git add large_asset.bin && git commit -m "feat: commit large binary"

# Strip blobs bigger than 5MB
git-filter-repo --strip-blobs-bigger-than 5M --force

# Verify binary was deleted from history
git log --all --full-history -- large_asset.bin
```
</details>

<details>
<summary><strong>Exercise 5: Monorepo Subdirectory Split</strong></summary>

**Scenario**: Filter a repository down to a single subfolder.
```bash
mkdir -p services/auth services/billing
echo "auth" > services/auth/auth.ts
echo "billing" > services/billing/billing.ts
git add . && git commit -m "feat: monorepo init"

git-filter-repo --subdirectory-filter services/auth --force
ls -la
# auth.ts is now at the repository root!
```
</details>

<details>
<summary><strong>Exercise 6: Deep Cleanup and Garbage Collection</strong></summary>

**Scenario**: Expire reflogs and aggressively prune loose objects to reclaim disk space.
```bash
git reflog expire --expire=now --all
git gc --prune=now --aggressive
```
</details>

---

## 12. Summary & Cheat Sheet

```bash
# Step 0: Rotate and revoke leaked credentials immediately!

# Analyze repository size & contents
git-filter-repo --analyze

# Remove a specific file from all commits & branches
git-filter-repo --path <filename> --invert-paths

# Remove a specific folder from all commits & branches
git-filter-repo --path <folder>/ --invert-paths

# Replace sensitive text across history
git-filter-repo --replace-text replacements.txt

# Remove any blob larger than 50MB
git-filter-repo --strip-blobs-bigger-than 50M

# Isolate a subdirectory into a new repo
git-filter-repo --subdirectory-filter <folder-path>

# Force push cleaned repo to remote
git remote add origin <url>
git push origin --force --all
git push origin --force --tags
```

---

## 13. Further Reading
- [Official git-filter-repo Documentation](https://github.com/newren/git-filter-repo)
- [GitHub Docs - Removing Sensitive Data from a Repository](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/removing-sensitive-data-from-a-repository)
- [Pro Git Book - Rewriting History](https://git-scm.com/book/en/v2/Git-Tools-Rewriting-History)
- [03-history-rewriting/06-rewriting-shared-history-safely.md](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/06-rewriting-shared-history-safely.md)
