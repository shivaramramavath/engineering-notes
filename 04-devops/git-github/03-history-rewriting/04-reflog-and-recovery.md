---
title: "Git Reflog and Recovery: Rescuing Lost History"
category: "04-devops"
subcategory: "git-github / 03-history-rewriting"
tags: ["git", "reflog", "recovery", "dangling-commits", "lost-work", "undo", "disaster-recovery"]
difficulty: "Advanced"
estimated_read_time: "26 min"
prerequisites:
  - "01-fundamentals/05-git-objects"
  - "01-fundamentals/06-refs-branches-and-head"
  - "02-commands/04-undoing-changes"
last_updated: "2026-11-25"
---

# Git Reflog and Recovery: Rescuing Lost History

> **Navigation**: [← Previous: 03-cherry-pick.md](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/03-cherry-pick.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/README.md) | [Next: 05-filter-repo-and-removing-secrets.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/05-filter-repo-and-removing-secrets.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes](#7-common-mistakes)
8. [Performance, Security & Cost](#8-performance-security--cost)
9. [Comparison Table](#9-comparison-table)
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is the Git Reflog?
The **Reference Log** (or `reflog`) is Git's internal flight recorder. It tracks every movement of reference pointers (`HEAD`, local branches, and stash entries) within your local repository. Whenever you commit, switch branches, rebase, reset, amend, or pull, Git writes an entry into the reflog logging the old SHA, the new SHA, the identity of who performed the action, a timestamp, and a human-readable explanation of the action.

### Why Does It Matter?
1. **The Ultimate Safety Net**: Destructive operations like `git reset --hard`, accidental branch deletions (`git branch -D`), aborted rebases, or botched amends seem to destroy commits. Because Git rarely deletes objects immediately, the reflog provides the exact SHA identifiers needed to resurrect "lost" work.
2. **Local and Private**: Reflogs are strictly local to your machine. They are never cloned, fetched, or pushed to remotes, ensuring you have a granular private record of your personal developer activities.
3. **Time-Travel Debugging**: You can query your repository's state as it was 2 hours ago (`HEAD@{2 hours ago}`) or before yesterday's failed deployment (`HEAD@{yesterday}`).

---

## 2. Core Concepts

### 1. Reflog Types
- **HEAD Reflog** (`.git/logs/HEAD`): Records every single movement of the `HEAD` pointer (switching branches, checkouts, commits, rebases, resets).
- **Branch Reflogs** (`.git/logs/refs/heads/<branch>`): Records changes specifically affecting that branch pointer (commits added to the branch, fast-forwards, rebases).
- **Stash Reflog** (`.git/logs/refs/stash`): Records the stack history of `git stash`.

### 2. Qualified Reflog Syntax: `ref@{specifier}`
You can address any historical point in the reflog using the `@{...}` syntax:
- `HEAD@{0}`: The current position of `HEAD`.
- `HEAD@{1}`: The position of `HEAD` immediately prior to the latest action.
- `main@{yesterday}`: The state of branch `main` at 00:00:00 yesterday.
- `main@{1.hour.ago}`: The state of `main` exactly one hour prior.
- `feature@{2026-11-20}`: The state of `feature` on that specific date.

### 3. Reachability and Dangling Commits
When a branch is deleted or reset, its commits are no longer reachable by traversing downwards from any named branch or tag reference. Such commits become **dangling commits**. However, as long as a reflog entry references their SHA, Git considers them reachable from the reflog and will **not** garbage-collect them.

### 4. Reflog Expiration Rules
Reflogs do not grow indefinitely. Git automatically expires entries according to two configuration timeouts:
- `gc.reflogExpire` (default: 90 days): Entries that lead to reachable commits expire after 90 days.
- `gc.reflogExpireUnreachable` (default: 30 days): Entries that reference unreachable/dangling commits expire after 30 days.
Once an entry expires, subsequent `git gc` runs will prune the underlying commit objects permanently.

---

## 3. How It Works Internally

```
Every time a ref moves (e.g., git commit, git reset, git switch):

             [ Action: git reset --hard HEAD~1 ]
                           │
                           ▼
 1. Calculate New Pointer:
    Old SHA: 8a4c1e0  ──►  New SHA: 2d9f3b1
                           │
                           ▼
 2. Update Ref File:
    .git/refs/heads/main  <-- 2d9f3b1
                           │
                           ▼
 3. Append to Reflog File (.git/logs/HEAD and .git/logs/refs/heads/main):
    [old-sha] [new-sha] [Author <email>] [epoch-timestamp tz] [reason]
    e.g.:
    8a4c1e0 2d9f3b1 Shiva Ram <dev@org.com> 1732512000 +0530 reset: moving to HEAD~1
```

### Disk Format in `.git/logs/`
If you inspect `.git/logs/HEAD` directly:
```text
0000000000000000000000000000000000000000 1f8e7d6... Dev <dev@org> 1700000000 +0530 commit (initial): init
1f8e7d6... 4a5e62f... Dev <dev@org> 1700000060 +0530 commit: feat: user auth
4a5e62f... 9e2cf31... Dev <dev@org> 1700000120 +0530 reset: moving to HEAD~1
```
Each line is a space-delimited record storing the exact cryptographic transition.

---

## 4. Syntax & Usage

```bash
# Display recent HEAD reflog entries (compact format)
git reflog

# Display detailed reflog with dates and author information
git reflog show --date=relative

# Display reflog for a specific branch
git reflog show main

# Display reflog with commit subject and ISO timestamps
git reflog --date=iso

# View difference between current state and previous reflog state
git diff HEAD@{1}

# Inspect a commit referenced in the reflog
git show HEAD@{3}

# Restore branch to an exact reflog position
git reset --hard HEAD@{2}

# Re-create an accidentally deleted branch from reflog SHA
git branch recovered-branch HEAD@{4}

# Inspect dangling objects not referenced by any ref
git fsck --lost-found
```

---

## 5. Practical Examples

### Example 1: Rescuing Work After an Accidental `git reset --hard`
```bash
# 1. Developer runs catastrophic hard reset by mistake
git reset --hard HEAD~3
# Output: HEAD is now at 1a2b3c4 feat: initial setup

# 2. All 3 recent commits seem gone from `git log`
git log --oneline

# 3. Inspect reflog to find the commit SHA before the reset
git reflog -n 5
# 1a2b3c4 HEAD@{0}: reset: moving to HEAD~3
# 7f8e9d0 HEAD@{1}: commit: feat(checkout): complete payment integration
# ...

# 4. Instantly restore the pre-reset commit!
git reset --hard HEAD@{1}

# 5. Verify commits are fully restored
git log -1 --oneline
# 7f8e9d0 feat(checkout): complete payment integration
```

### Example 2: Recovering an Accidentally Deleted Branch
```bash
# 1. Developer deletes feature branch forcefully
git branch -D feature/payment-gateway
# Output: Deleted branch feature/payment-gateway (was 9a8b7c6).

# 2. Find the last commit on that branch via reflog
git reflog | grep "checkout: moving from feature/payment-gateway"
# or inspect the branch reflog before deletion
# e.g.: 9a8b7c6 HEAD@{2}: commit: feat(gateway): stripe webhook handler

# 3. Resurrect the branch at that exact commit SHA
git branch feature/payment-gateway 9a8b7c6

# 4. Verify branch exists with all commits intact
git switch feature/payment-gateway
git log -1 --oneline
```

### Example 3: Rescuing Commits from a Botched or Aborted Rebase
```bash
# During an interactive rebase, commits were dropped or corrupted
# Find the state of HEAD immediately before the rebase started:
git reflog | grep "rebase (start)" -A 1
# 3c4d5e6 HEAD@{12}: rebase (start): checkout origin/main
# 8f9a0b1 HEAD@{13}: commit: feat: prior working commit

# Reset directly back to the pre-rebase commit
git reset --hard HEAD@{13}
```

### Example 4: Rescuing Unreachable Commits Using `git fsck`
```bash
# If reflog was expired or cleared, search the object database for dangling commits
git fsck --lost-found --unreachable

# Output:
# dangling commit 4f5a6b7c8d9e0f1a2b3c4d5e6f7a8b9c0d1e2f3a
# dangling blob 1a2b3c...

# Inspect the dangling commit
git show 4f5a6b7c8d9e0f1a2b3c4d5e6f7a8b9c0d1e2f3a

# Recover into a branch
git branch rescued-work 4f5a6b7c8d9e0f1a2b3c4d5e6f7a8b9c0d1e2f3a
```

### Example 5: Viewing File State from 3 Hours Ago Without Checking Out
```bash
# Inspect the contents of a specific file as it existed 3 hours ago
git show HEAD@{3.hours.ago}:src/config/database.ts

# Compare current file against the state at 10:00 AM
git diff HEAD@{10:00}:src/config/database.ts src/config/database.ts
```

---

## 6. Best Practices

1. **Check the Reflog First Before Panicking**: Almost no commit in Git is permanently lost immediately. Before running desperate commands, check `git reflog`.
2. **Never Run `git gc --prune=now` During an Incident**: Aggressive garbage collection purges unreferenced dangling objects. Avoid running `git gc` or `git prune` while troubleshooting.
3. **Use Descriptive Commit Messages**: The reflog displays commit subjects. Good commit messages make identifying the target commit among hundreds of reflog lines trivial.
4. **Create a Safety Branch Before Risky History Rewrites**: Before initiating complex rebases or filter scripts, tag or branch your current state:
   ```bash
   git branch backup/pre-rebase-$(date +%Y%m%d)
   ```
5. **Remember Reflog is Machine-Local**: Do not rely on reflog for disaster recovery on remote build machines or when switching laptops. Push work to private remote branches for true backup.
6. **Prefer `git branch <new-name> <sha>` Over Immediate Hard Resets**: When recovering lost commits, create a new branch at that commit first to inspect it safely without disturbing your current working directory.

---

## 7. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Panicking and re-cloning repo | Truly destroys local unpushed commits and reflog | Believing work is permanently erased | Stop, run `git reflog`, and recover commits |
| Running `git gc --prune=now` | Permanently deletes dangling objects | Attempting to "clean up" during recovery | Never run prune or gc when recovering work |
| Expecting uncommitted files in reflog | Reflog cannot rescue unstaged, uncommitted files | Reflog only records commit/ref movements | Stash or commit WIP work frequently |
| Relying on reflog on freshly cloned repo | Clone has empty reflog | Reflogs are strictly local and not transferred over network | Push WIP branches to remote |
| Overwriting uncommitted edits during reset | Destroys dirty working tree | Running `git reset --hard` while uncommitted files exist | Run `git stash` before resetting |

---

## 8. Performance, Security & Cost

- **Reflog Disk Footprint**: Reflog log files (`.git/logs/`) are append-only plain text files. They consume minimal disk space (typically a few hundred kilobytes even in heavily active repositories).
- **Security Implications**: Because reflogs preserve dangling commits for 30–90 days, sensitive credentials or keys accidentally committed and subsequently amended/removed **still exist in the local object store and reflog**. Complete eradication requires purging reflogs and running `git gc --prune=now`.
- **Garbage Collection Overhead**: Large numbers of dangling objects retained by reflogs can marginally increase `git status` or `git pack-objects` time. Default 30/90-day pruning keeps performance optimal.

---

## 9. Comparison Table

| Recovery Mechanism | Scope | Data Rescued | Survives Branch Deletion? | Retention Window |
|:---|:---|:---|:---:|:---|
| **Git Reflog** (`git reflog`) | Local repository | Any committed state reached by HEAD or refs | **Yes** | 30–90 days (configurable) |
| **Git Stash** (`git stash list`) | Local repository | Explicitly stashed uncommitted changes | **Yes** | Indefinite (until dropped/popped) |
| **`git fsck --lost-found`** | Local object database | Dangling blobs and commits without reflog entries | **Yes** | Until pruned by `git gc` |
| **Remote Tracking** (`origin/...`) | Remote server | Pushed commits | **Yes** | Dependent on server backup policies |
| **OS File Recovery / IDE Local History** | Editor / OS filesystem | Unstaged, uncommitted text edits | **Yes** | Dependent on editor cache settings |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the fundamental difference between `git log` and `git reflog`?</strong></summary>

`git log` traverses the commit graph backwards starting from the current commit (`HEAD`) through parent links (`parent` pointer in commit objects). It displays the public chronological history of the branch.
`git reflog` is an append-only transaction ledger that records every time `HEAD` or a branch pointer was updated locally on your machine. It shows what commits were checked out, amended, rebased, or reset, regardless of whether those commits still belong to an active branch hierarchy.
</details>

<details>
<summary><strong>Q2: An engineer ran `git reset --hard HEAD~5` and realized they lost 5 critical commits. Walk through the exact recovery steps.</strong></summary>

1. Run `git reflog -n 10` to view the most recent pointer transitions.
2. Identify the entry immediately preceding the reset, which will be labeled `HEAD@{1}` with reason `commit: <subject>` or similar.
3. Verify the commit content using `git show HEAD@{1}`.
4. Execute `git reset --hard HEAD@{1}` to restore the branch pointer directly back to the state before the reset.
All 5 commits and working directory files are completely restored.
</details>

<details>
<summary><strong>Q3: How does Git prevent orphaned commits from being immediately purged by garbage collection?</strong></summary>

Git's garbage collection (`git gc`) performs a reachability analysis. Any commit reachable by traversing references in `.git/refs/heads/`, `.git/refs/tags/`, or `.git/refs/remotes/` is marked reachable. Additionally, Git checks all log files in `.git/logs/` (the reflogs). If an orphaned commit's SHA is present in any reflog file, Git treats it as reachable from the reflog and protects it from pruning until the entry exceeds the `gc.reflogExpireUnreachable` timeout (default 30 days).
</details>

<details>
<summary><strong>Q4: Why does a newly cloned repository have an empty reflog?</strong></summary>

Reflogs are strictly private to the local repository instance. The Git transport protocol (`git fetch`, `git clone`, `git push`) only transfers packfiles containing commits, trees, blobs, tags, and ref pointers (`refs/heads/*`, `refs/tags/*`). Git log transactions recorded in `.git/logs/` reflect individual developer machine activity and are never serialized or transmitted across the wire.
</details>

<details>
<summary><strong>Q5: Can `git reflog` recover code changes that were never committed or stashed? Why or why not?</strong></summary>

**No.** Git's reflog and object database only store objects that have been committed or stashed (which creates commit and tree objects internally). If a developer writes code in their working directory and runs `git reset --hard` or `git checkout .` without running `git commit` or `git stash`, Git has never recorded those byte changes into an object. In that scenario, recovery is only possible through IDE local history (e.g., VS Code Timeline / IntelliJ Local History) or OS filesystem snapshots.
</details>

<details>
<summary><strong>Q6: How do you recover an accidentally deleted branch whose name you don't remember?</strong></summary>

1. Run `git reflog show --date=relative` and look for the last commit messages you were working on or search for checkout events (`checkout: moving from <branch> to ...`).
2. Once the SHA of the last commit on that branch is located (e.g. `e4f5a6b`), create a new branch from that SHA:
   ```bash
   git branch recovered-work e4f5a6b
   ```
3. Checkout the recovered branch and resume work.
</details>

<details>
<summary><strong>Q7: What is the difference between `gc.reflogExpire` and `gc.reflogExpireUnreachable`?</strong></summary>

- `gc.reflogExpire` sets the retention duration for reflog entries that point to commits that are still reachable from a current branch or tag reference (default 90 days).
- `gc.reflogExpireUnreachable` sets the retention duration for reflog entries that reference dangling, unreachable commits (e.g. commits from deleted branches, amended commits, or commits discarded during resets; default 30 days).
</details>

<details>
<summary><strong>Q8: If the reflog has been deleted or expired, how can you find and rescue lost commits in Git?</strong></summary>

You run `git fsck --lost-found --unreachable`. Git traverses the entire `.git/objects/` database, identifying every commit and blob that has no references pointing to it. Git writes unreachable commits and blobs into `.git/lost-found/commit/` and `.git/lost-found/other/`. You can inspect each commit hash using `git show <hash>` and point a branch to the desired commit using `git branch rescue <hash>`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Recover from an Accidental Hard Reset</strong></summary>

**Scenario**: Commit two files, execute a hard reset, and rescue the commit via reflog.
```bash
mkdir reflog-lab && cd reflog-lab && git init
echo "first" > file1.txt && git add file1.txt && git commit -m "feat: first commit"
echo "second" > file2.txt && git add file2.txt && git commit -m "feat: second commit"

# Accidental reset
git reset --hard HEAD~1
git log --oneline

# Rescue
RESCUE_SHA=$(git reflog -n 2 --format="%h" | sed -n '2p')
git reset --hard $RESCUE_SHA
git log --oneline
```
</details>

<details>
<summary><strong>Exercise 2: Resurrect a Force-Deleted Branch</strong></summary>

**Scenario**: Create a branch, commit code, switch to main, delete the branch with `-D`, and resurrect it.
```bash
git switch -c feature/ai-service
echo "ai model" > model.py && git add model.py && git commit -m "feat(ai): add model interface"
LAST_SHA=$(git rev-parse HEAD)

git switch main
git branch -D feature/ai-service

# Resurrect branch
git branch feature/ai-service $LAST_SHA
git switch feature/ai-service
git log -1 --oneline
```
</details>

<details>
<summary><strong>Exercise 3: Time-Travel Diffing</strong></summary>

**Scenario**: Use relative time specifiers in reflog to inspect changes between current state and previous state.
```bash
echo "update 1" >> file1.txt && git commit -am "chore: update 1"
git diff HEAD@{1} HEAD
```
</details>

<details>
<summary><strong>Exercise 4: Recover from a Failed Interactive Rebase</strong></summary>

**Scenario**: Start a rebase, drop a commit by mistake, and restore using `ORIG_HEAD` or reflog.
```bash
echo "c1" >> file1.txt && git commit -am "c1"
echo "c2" >> file1.txt && git commit -am "c2"
PRE_REBASE=$(git rev-parse HEAD)

# Simulate bad rebase by resetting to parent
git reset --hard HEAD~2

# Restore immediately using reflog
git reset --hard $PRE_REBASE
git log -2 --oneline
```
</details>

<details>
<summary><strong>Exercise 5: Finding Dangling Objects with `git fsck`</strong></summary>

**Scenario**: Create an unreachable commit and discover it using `git fsck`.
```bash
echo "orphan" > orphan.txt && git add orphan.txt && git commit -m "wip: orphan work"
ORPHAN_SHA=$(git rev-parse HEAD)
git reset --hard HEAD~1

# Discover unreachable commit
git fsck --lost-found | grep "$ORPHAN_SHA"
```
</details>

<details>
<summary><strong>Exercise 6: Configuring Custom Reflog Expiration Timeouts</strong></summary>

**Scenario**: Configure local repository reflog retention to 180 days for reachable entries and 60 days for unreachable entries.
```bash
git config gc.reflogExpire "180 days"
git config gc.reflogExpireUnreachable "60 days"

git config --get gc.reflogExpire
git config --get gc.reflogExpireUnreachable
```
</details>

---

## 12. Summary & Cheat Sheet

```bash
# Common Reflog Commands
git reflog                     # Show HEAD reflog
git reflog show main           # Show branch reflog
git reflog --date=relative     # Show relative timestamps (e.g. "2 hours ago")
git reflog --date=iso          # Show ISO timestamps

# Recovery Operations
git reset --hard HEAD@{1}      # Undo the last operation immediately
git branch <name> HEAD@{n}     # Recover lost commit to a new branch
git switch -c rescue <sha>     # Checkout dangling commit directly into new branch
git diff HEAD@{1} HEAD         # Compare previous state with current state

# Deep Disaster Recovery
git fsck --lost-found          # Locate all unreferenced dangling commits & blobs
git show <dangling-sha>        # Inspect contents of a dangling commit
```

---

## 13. Further Reading
- [Official Git Documentation - git-reflog](https://git-scm.com/docs/git-reflog)
- [Pro Git Book - Maintenance and Data Recovery](https://git-scm.com/book/en/v2/Git-Internals-Maintenance-and-Data-Recovery)
- [Git fsck Documentation](https://git-scm.com/docs/git-fsck)
- [03-history-rewriting/05-filter-repo-and-removing-secrets.md](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/05-filter-repo-and-removing-secrets.md)
