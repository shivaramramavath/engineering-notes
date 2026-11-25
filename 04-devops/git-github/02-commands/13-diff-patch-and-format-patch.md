<!--
================================================================================
Metadata:
  Title: Patch-Based Collaboration: git diff, git format-patch & git am
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 12-tags-and-releases.md
  Key Terminology: git diff, git apply, git format-patch, git am (Apply Mailbox), mbox format, git send-email, Offline Collaboration
================================================================================
-->

# Patch-Based Collaboration: git diff, git format-patch & git am

[<- 12-tags-and-releases.md](./12-tags-and-releases.md) | [Back to Module Overview](./README.md) | [14-blame-and-log-search.md ->](./14-blame-and-log-search.md)

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Troubleshooting](#7-common-mistakes--troubleshooting)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table](#9-comparison-table)
- [10. Real-World Interview Questions](#10-real-world-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference Cheat Sheet](#12-summary--quick-reference-cheat-sheet)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

Before GitHub introduced web-based Pull Requests in 2008, Git was designed by Linus Torvalds to manage the worldwide, distributed development of the Linux kernel entirely through **plain-text email patches**.

Even today, major foundational open-source ecosystems (including the Linux Kernel, Git itself, PostgreSQL, QEMU, and GCC) do not use GitHub PRs; they collaborate via **`git format-patch`** and **`git am`**.

Furthermore, patch-based workflows are a critical skill for modern enterprise engineers:
- **Disaster Recovery**: If GitHub or your company's network forge is down during a production incident, patches allow engineers to export, review, and apply emergency hotfixes peer-to-peer over Slack, SCP, or email.
- **Air-Gapped Environments**: Secure defense, banking, and embedded systems often operate behind strict air-gapped security boundaries with no internet access. Patches provide the standard vehicle for moving code across security enclaves.
- **Testing Untrusted Contributions**: Easily review and test external code without adding untrusted remotes or checking out third-party branches.

---

## 2. Core Concepts

### Raw Diff (`git apply`) vs. Formatted Mailbox Patch (`git am`)
Understanding the difference between a raw unified diff and a formatted patch is the foundational concept:

1. **Raw Unified Diff (`git diff > change.patch`)**:
   - Contains ONLY file modifications (insertions, deletions, line numbers).
   - Contains **NO commit author**, **NO author email**, **NO timestamp**, and **NO commit message**.
   - Applied using **`git apply`**: Modifies physical files in the working directory (or index), but **does NOT create any commit objects**.
2. **Formatted Mailbox Patch (`git format-patch`)**:
   - Formatted using standard RFC 2822 email headers (`From:`, `Date:`, `Subject:`, body).
   - Captures the complete metadata of the commit: original author name, author email, exact author date, full commit message, and unified diff.
   - Applied using **`git am` ("Apply Mailbox")**: Reads the patch, applies the diff, and **recreates the exact commit object** in the local DAG with original author and timestamp attribution!

```
+-----------------------------------------------------------------------------------+
|                        Raw Diff vs. Formatted Patch Workflow                      |
|                                                                                   |
|  1. RAW DIFF:                                                                     |
|     git diff > fix.patch ──────► git apply fix.patch ──────► Working Tree Edited  |
|     (No author, no message)      (Does NOT commit!)          (Must commit manually|
|                                                                                   |
|  2. FORMATTED PATCH:                                                              |
|     git format-patch -1 HEAD ──► git am < 0001-fix.patch ──► Brand-New Commit!    |
|     (Captures complete author,   (Recreates exact commit     (Original author,    |
|      email, date, and message)    object in local DAG)        date & msg intact!) |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### Anatomy of an mbox Patch File
When `git format-patch` generates a file (e.g. `0001-fix-auth.patch`), inspect its contents:

```text
From 4b825dc642cb6eb9a060e54bf8d69288fbee4904 Mon Sep 17 00:00:00 2001
From: Alex Morgan <alex.morgan@company.com>
Date: Mon, 25 Nov 2024 10:00:00 -0500
Subject: [PATCH] fix(auth): resolve session timeout race condition

Ensure session token renewal acquires an exclusive mutex before
updating in-memory token state to prevent parallel request race.

Closes #1042
---
 src/auth/session.ts | 4 +++-
 1 file changed, 3 insertions(+), 1 deletion(-)

diff --git a/src/auth/session.ts b/src/auth/session.ts
index 7a8b9c..d4e5f6 100644
--- a/src/auth/session.ts
+++ b/src/auth/session.ts
@@ -15,4 +15,6 @@ export class SessionManager {
+  private mutex = new Mutex();
...
-- 
2.45.2
```

When `git am` processes this file:
1. It parses the `From:` line to extract author name and email.
2. It parses the `Date:` line to extract the exact author timestamp.
3. It parses the `Subject:` line and email body to construct the commit message.
4. It applies the diff hunks to `.git/index` and the working tree.
5. It invokes low-level `git write-tree` and `git commit-tree`, generating a commit attributed to the original patch author while recording the applying engineer as the **Committer**!

---

## 4. Syntax & Usage

### 1. Generating Patches
```bash
# Export the latest single commit as an mbox patch file
git format-patch -1 HEAD

# Export all commits on current branch that are not in main
git format-patch main..HEAD -o patches/

# Export commits from the last 3 revisions into a specific folder
git format-patch -3 HEAD -o /tmp/patch-bundle/

# Generate a raw unified diff file of uncommitted changes
git diff > uncommitted.patch

# Generate a raw unified diff between two branches
git diff main..feature > feature-drift.diff
```

---

### 2. Inspecting and Applying Patches
```bash
# Test whether a raw patch applies cleanly without actually modifying files (--check)
git apply --check fix.patch

# Apply raw patch to working tree files
git apply fix.patch

# Apply raw patch directly to the staging index (ready to commit)
git apply --cached fix.patch

# Apply a single formatted mbox patch (creates commit!)
git am < patches/0001-fix-auth.patch

# Apply an entire series of numbered patches in order
git am patches/*.patch

# Abort an in-progress patch application that encountered conflicts
git am --abort

# Skip the current conflicting patch in a series
git am --skip

# Resume patch application after resolving conflicts and running git add
git am --resolved
# (Or synonym: git am --continue)
```

---

## 5. Practical Examples

### Example 1: Emergency Air-Gapped Hotfix Transfer
Simulate exporting a production hotfix on one machine and applying it on an isolated bastion:

```bash
# Machine A (Developer Workstation):
mkdir /tmp/dev-repo && cd /tmp/dev-repo
git init -b main
echo "v1.0" > app.txt && git add app.txt && git commit -m "init"

# Developer fixes critical bug
echo "Critical Security Patch" >> app.txt
git commit -am "fix(sec): patch SQL injection vulnerability (CVE-2024-9999)"

# Export patch
git format-patch -1 HEAD -o /tmp/export/
# Generated: /tmp/export/0001-fix-sec-patch-SQL-injection-vulnerability-CVE-2024.patch

# Transfer patch file via SCP / secure USB to Machine B (Production Bastion):
mkdir /tmp/prod-repo && cd /tmp/prod-repo
git clone /tmp/dev-repo .
git reset --hard HEAD~1 # Simulate production being 1 commit behind

# On Production: Apply the patch directly!
git am < /tmp/export/0001-*.patch

# Verify: The commit is recreated with original author, timestamp, and message!
git log -1 --format=fuller
```

### Example 2: Testing an Uncommitted Patch with `git apply --check`
Safely verify patch compatibility before touching working files:

```bash
# Check if patch applies cleanly
if git apply --check /tmp/export/0001-*.patch; then
    echo "Patch is clean! Applying..."
    git apply /tmp/export/0001-*.patch
else
    echo "ERROR: Patch conflicts with local files! Aborting."
fi
```

### Example 3: Resolving Conflicts During `git am`
When a patch series conflicts with local code:

```bash
# Apply series
git am patches/*.patch
# Output:
# Applying: feat: add billing
# error: patch failed: app.txt:1
# error: app.txt: patch does not apply
# Patch failed at 0002 feat: add billing

# 1. Inspect conflict state
git status -s
# Conflicting file marked

# 2. Resolve conflict in editor, remove conflict markers
echo "Resolved Content" > app.txt

# 3. Stage the resolved file
git add app.txt

# 4. Resume patch application (DO NOT run git commit!)
git am --continue
# Output: Applying: feat: add billing
# Complete!
```

### Example 4: Creating a 3-Way Patch Application
If a patch fails to apply due to slight line drifts, instruct `git apply` or `git am` to fall back to a 3-way merge using the blob hashes recorded in the patch:

```bash
git am -3 < patch-with-drift.patch
# The -3 / --3way flag uses recorded index hashes to find the common ancestor
# and run a 3-way merge, successfully applying patches despite shifted lines!
```

---

## 6. Best Practices

1. **Always Use `git format-patch` for Commit Sharing**: Never send raw `git diff` files if commit provenance matters. `format-patch` preserves author attribution, timestamps, and commit descriptions.
2. **Always Run `git apply --check` Before `git apply`**: Dry-run verification prevents leaving half-applied patch fragments (`.rej` files) in your working tree.
3. **Use `-3` (3-Way Fallback) When Applying Drifted Patches**: If lines shifted due to subsequent edits, `git am -3` utilizes Git's object database to resolve minor drift automatically.
4. **Never Run `git commit` to Resume `git am`**: After resolving a patch conflict, stage the file with `git add` and run `git am --continue` (or `--resolved`). Running `git commit` desynchronizes Git's internal mailbox tracker!
5. **Number Patch Series Cleanly**: When generating multiple patches, `format-patch` automatically numbers them (`0001-...`, `0002-...`). Apply them using `git am *.patch` to preserve chronological execution order.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `error: patch failed: ... patch does not apply` | Local file has drifted from the base version the patch was generated against. | Run with 3-way merge fallback: `git am -3 < patch.patch` or resolve conflicts manually. |
| Developer ran `git commit` during `git am` conflict | Thought manual commit was required; broke `git am` state. | Run `git reset --soft HEAD~1`, then run `git am --continue`. |
| Raw patch applied with `git apply` but author attribution is lost | `git apply` only edits files; it does not read or create commit objects. | Use `git format-patch` and `git am` instead if author metadata is required. |
| `fatal: previous rebase directory .git/rebase-apply still exists` | An earlier `git am` or `git rebase` command crashed or was abandoned mid-way. | Clean up with `git am --abort` or remove `.git/rebase-apply/`. |
| Patch contains DOS carriage returns (`\r\n`) and fails on Linux | Line ending formatting mismatch. | Run `dos2unix <patch-file>` before applying, or configure `git config am.keepcr true`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Zero Network Overhead**: Patch files are lightweight plain text (typically 2KB to 50KB). Generating, compressing, and emailing a patch takes milliseconds and requires zero Git daemon or server connections.

### Security
- **Reviewability of Plain Text**: In zero-trust security postures, binary Git packfiles sent over the wire are opaque. Formatted patches are 100% human-readable plain text, allowing automated security static analyzers and firewalls to scan every line of incoming code before applying.

---

## 9. Comparison Table

| Attribute | Raw Diff (`git diff`) | Formatted Patch (`git format-patch`) |
| :--- | :--- | :--- |
| **File Format** | Unified Diff | RFC 2822 Mailbox (`.patch`) |
| **Contains Commit Author?**| No | Yes (Name + Email) |
| **Contains Timestamp?** | No | Yes (Original Author Date) |
| **Contains Commit Message?**| No | Yes (Subject + Body) |
| **Application Command** | `git apply` | `git am` |
| **Creates Commit Object?** | No (modifies files only) | Yes (recreates exact commit) |
| **Supports 3-Way Fallback?**| Yes (`-3`) | Yes (`-3`) |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between `git apply` and `git am`?</strong></summary>

- `git apply` is a low-level utility that takes a raw unified diff patch and applies the file modifications directly to the working tree or staging index. It does not read commit metadata and **does not create any commit objects**. The developer must manually review and commit the changes.
- `git am` ("Apply Mailbox") is designed to process patches generated by `git format-patch`. It parses email headers to extract the original author name, author email, author timestamp, and commit message, applies the diff, and **automatically creates a new commit object** in the local DAG with full historical attribution.
</details>

<details>
<summary><strong>2. Why does the Linux kernel development community continue to use email patches (`format-patch`/`am`) instead of GitHub Pull Requests?</strong></summary>

1. **Decentralization & Vendor Independence**: Email is an open, federated protocol not controlled by any single commercial entity (Microsoft/GitHub).
2. **Review Granularity**: Email allows inline, line-by-line discussion across threaded email clients with complete offline accessibility.
3. **Tooling & Automation**: Patch series can be filtered, categorized, and applied automatically using standard Unix mail tools and specialized daemons (e.g. `b4`, patchwork, lore.kernel.org).
4. **Permanent Public Archive**: The entire historical review discourse is archived publicly in plain text across thousands of independent mirrors.
</details>

<details>
<summary><strong>3. What does the `-3` (or `--3way`) flag accomplish in `git am -3`?</strong></summary>

When a patch does not apply cleanly because the target codebase has modified nearby lines, standard `git am` fails. Passing `-3` instructs Git to perform a 3-way merge. Git reads the blob SHA hashes embedded in the patch's `index <hash>..<hash>` line, searches its local object database for the base version, and runs a 3-way merge between the common ancestor base, the current file version, and the patch, successfully resolving minor line drift.
</details>

<details>
<summary><strong>4. If a conflict occurs during `git am`, what is the correct sequence of commands to resolve and proceed?</strong></summary>

1. Inspect conflicting files using `git status`.
2. Open the conflicting files in an editor, resolve the differences, and remove conflict markers.
3. Stage the resolved files using `git add <file>`.
4. Run `git am --continue` (or `git am --resolved`) to proceed with the patch application.
*(Crucial: Never run `git commit` to resolve a `git am` conflict!)*
</details>

<details>
<summary><strong>5. What is the purpose of `git apply --check`?</strong></summary>

`git apply --check <patch>` tests whether a patch can be applied cleanly against the current working directory without actually modifying any files on disk. If the patch would cause conflicts or fail, it outputs error details and exits with a non-zero exit code.
</details>

<details>
<summary><strong>6. How can an engineer generate patches for all commits on a feature branch that do not exist on `main`?</strong></summary>

By specifying a revision range:
```bash
git format-patch main..feature-branch -o ./patches/
```
Git writes numbered patch files (`0001-...`, `0002-...`) for each commit in that range into the `./patches/` directory.
</details>

<details>
<summary><strong>7. How does Git distinguish between the Author and the Committer when `git am` is used?</strong></summary>

When `git am` applies a patch:
- The **Author** is extracted from the `From:` and `Date:` headers inside the patch file (the person who originally wrote the code).
- The **Committer** is set to the current user executing `git am` (`user.name`, `user.email`), with the current timestamp recorded as the `CommitDate`.
</details>

<details>
<summary><strong>8. What is `git send-email`?</strong></summary>

`git send-email` is a Git utility that sends a series of patches generated by `git format-patch` directly to a mailing list or recipient via SMTP. It preserves email threading (`Message-ID`, `In-Reply-To`), handles character encodings, and ensures patches are not mangled by graphical email client word-wrapping.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Generating and Inspecting an mbox Patch</strong></summary>

**Task**: Initialize a repository, create a commit, and use `git format-patch -1 HEAD` to export it. View the file headers with `head`.

**Solution**:
```bash
mkdir /tmp/patch-lab && cd /tmp/patch-lab
git init -b main
echo "Version 1.0" > app.py && git add app.py && git commit -m "feat: initial python app"

# Make commit
echo "print('hello world')" >> app.py
git commit -am "feat(core): add print greeting"

# Export patch
git format-patch -1 HEAD

# Inspect generated file
cat 0001-feat-core-add-print-greeting.patch | head -n 15
# Notice From:, Date:, Subject:, and unified diff headers!
```
</details>

<details>
<summary><strong>Exercise 2: Applying a Patch with `git am`</strong></summary>

**Task**: Clone the test repository into `/tmp/patch-dest`, reset it backwards by 1 commit, and apply the patch using `git am`.

**Solution**:
```bash
git clone /tmp/patch-lab /tmp/patch-dest
cd /tmp/patch-dest
git reset --hard HEAD~1

# Apply patch
git am < /tmp/patch-lab/0001-*.patch

# Verify commit is recreated with original metadata
git log -1 --format=fuller
```
</details>

<details>
<summary><strong>Exercise 3: Safe Testing with `git apply --check`</strong></summary>

**Task**: Test whether a patch applies cleanly against a modified file using `git apply --check`.

**Solution**:
```bash
# Verify clean check
git apply --check /tmp/patch-lab/0001-*.patch
# Returns 0 exit code if clean!
```
</details>

<details>
<summary><strong>Exercise 4: Generating a Raw Diff Patch</strong></summary>

**Task**: Make uncommitted changes and export a raw unified diff using `git diff > change.diff`. Inspect its contents to confirm it lacks commit metadata.

**Solution**:
```bash
echo "debug code" >> app.py
git diff > change.diff

cat change.diff
# Notice: No From:, Date:, or Subject: headers! Pure diff only.
```
</details>

<details>
<summary><strong>Exercise 5: Applying a Raw Diff Directly to the Index</strong></summary>

**Task**: Discard your working tree edits, and apply `change.diff` directly to the staging index using `git apply --cached`.

**Solution**:
```bash
git restore app.py

# Apply raw diff to index directly
git apply --cached change.diff

git status -s
# Output: M  app.py (Staged directly in index!)
```
</details>

<details>
<summary><strong>Exercise 6: Aborting a Failed `git am` Session</strong></summary>

**Task**: Simulate a corrupted or conflicting patch, attempt `git am`, observe the conflict, and abort cleanly using `git am --abort`.

**Solution**:
```bash
echo "conflicting line" > conflict.patch
git am < conflict.patch 2>/dev/null || echo "git am failed as expected!"

# Cleanly abort
git am --abort
git status
# Clean!
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Command | Result |
| :--- | :--- | :--- |
| **Export 1 Commit** | `git format-patch -1 <hash>` | Generates numbered `.patch` file with full metadata |
| **Export Branch Series**| `git format-patch main..feature -o dir/`| Generates patch series for all branch commits |
| **Export Raw Diff** | `git diff > change.diff` | Exports unified diff without commit metadata |
| **Test Patch Cleanliness**| `git apply --check <patch>` | Verifies patch applicability without touching files |
| **Apply Raw Patch** | `git apply <patch>` | Applies diff to working tree (no commit created) |
| **Apply to Staging** | `git apply --cached <patch>` | Applies diff directly to staging index |
| **Apply Mailbox Patch**| `git am < <patch>` | Recreates commit object with author, date, and message |
| **3-Way Fallback** | `git am -3 < <patch>` | Uses index hashes for 3-way merge on drifted lines |
| **Resume After Conflict**| `git am --continue` | Resumes patch series after staging resolved files |
| **Abort Patch Series** | `git am --abort` | Cancels patch application and restores clean state |

---

## 13. Further Reading & Official Documentation

- [Official Git Documentation - git-format-patch](https://git-scm.com/docs/git-format-patch)
- [Official Git Documentation - git-am](https://git-scm.com/docs/git-am)
- [Official Git Documentation - git-apply](https://git-scm.com/docs/git-apply)
- [Submitting Patches: The Essential Guide to Getting Your Code into the Kernel](https://docs.kernel.org/process/submitting-patches.html)

---

[<- 12-tags-and-releases.md](./12-tags-and-releases.md) | [Back to Module Overview](./README.md) | [14-blame-and-log-search.md ->](./14-blame-and-log-search.md)
