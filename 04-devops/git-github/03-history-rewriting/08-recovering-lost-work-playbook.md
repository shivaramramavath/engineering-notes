---
title: "Recovering Lost Work Playbook: Emergency Triage Runbook"
category: "04-devops"
subcategory: "git-github / 03-history-rewriting"
tags: ["git", "disaster-recovery", "playbook", "runbook", "lost-commits", "triage", "emergency"]
difficulty: "Advanced"
estimated_read_time: "26 min"
prerequisites:
  - "03-history-rewriting/02-rebase"
  - "03-history-rewriting/04-reflog-and-recovery"
  - "03-history-rewriting/07-reset-vs-revert-vs-restore"
last_updated: "2026-11-25"
---

# Recovering Lost Work Playbook: Emergency Triage Runbook

> **Navigation**: [← Previous: 07-reset-vs-revert-vs-restore.md](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/07-reset-vs-revert-vs-restore.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/README.md) | [Next Module: 04-github-actions →](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/README.md)

---

## Table of Contents
1. [Emergency Triage Protocol: STOP AND READ FIRST](#1-emergency-triage-protocol-stop-and-read-first)
2. [Incident Matrix](#2-incident-matrix)
3. [Scenario 1: Accidental `git reset --hard`](#3-scenario-1-accidental-git-reset---hard)
4. [Scenario 2: Accidental Branch Deletion (`git branch -D`)](#4-scenario-2-accidental-branch-deletion-git-branch--d)
5. [Scenario 3: Botched Interactive Rebase or Dropped Commits](#5-scenario-3-botched-interactive-rebase-or-dropped-commits)
6. [Scenario 4: Commits Made in Detached HEAD State](#6-scenario-4-commits-made-in-detached-head-state)
7. [Scenario 5: Accidentally Dropped or Cleared Stashes](#7-scenario-5-accidentally-dropped-or-cleared-stashes)
8. [Scenario 6: Uncommitted Work Overwritten (IDE Local History)](#8-scenario-6-uncommitted-work-overwritten-ide-local-history)
9. [Scenario 7: Remote Branch Overwritten by Teammate Force Push](#9-scenario-7-remote-branch-overwritten-by-teammate-force-push)
10. [Garbage Collection Prevention & fsck Deep Scan](#10-garbage-collection-prevention--fsck-deep-scan)
11. [Interview Questions & Answers](#11-interview-questions--answers)
12. [Practice Exercises](#12-practice-exercises)
13. [Summary & Emergency Cheat Sheet](#13-summary--emergency-cheat-sheet)
14. [Further Reading](#14-further-reading)

---

## 1. Emergency Triage Protocol: STOP AND READ FIRST

```
                       EMERGENCY TRIAGE RULE ZERO:
           DO NOT CLOSE YOUR TERMINAL. DO NOT RUN 'git clean'.
                  DO NOT RUN 'git gc' OR 'git prune'.
         DO NOT DELETE THE FOLDER. DO NOT RE-CLONE THE REPOSITORY.
```

### The Three Golden Rules of Git Disaster Recovery:
1. **Rule 1: Git Rarely Deletes Anything Immediately**: Every commit, tree, and blob written to Git's object store remains physically on disk in `.git/objects/` for at least 30 to 90 days.
2. **Rule 2: Check the Reflog Immediately**: `git reflog` records all movements of `HEAD`. 95% of all Git disasters can be solved in under 60 seconds with `git reflog` and `git reset --hard HEAD@{n}`.
3. **Rule 3: Create a Safety Backup Before Touching Anything**:
   ```bash
   # Create a physical tar archive of your entire repo including .git directory
   tar -czf ../repo-emergency-backup-$(date +%s).tar.gz .
   ```

---

## 2. Incident Matrix

| Incident Code | Symptom | Primary Tool | Resolution Time |
|:---|:---|:---|:---:|
| **INC-01** | Ran `git reset --hard` by accident | `git reflog` | $< 1$ min |
| **INC-02** | Deleted local branch with `git branch -D` | `git reflog` | $< 2$ min |
| **INC-03** | Aborted/corrupted interactive rebase | `git reflog` / `ORIG_HEAD` | $< 2$ min |
| **INC-04** | Switched branches from detached HEAD; commits vanished | `git reflog` | $< 1$ min |
| **INC-05** | Ran `git stash drop` or `git stash clear` | `git fsck` | $< 5$ min |
| **INC-06** | Uncommitted working directory edits overwritten | IDE Local History | $< 5$ min |
| **INC-07** | Teammate ran `push --force` over your branch | Remote Reflog / GitHub Events | $< 5$ min |

---

## 3. Scenario 1: Accidental `git reset --hard`

### The Problem
You meant to run `git reset --soft HEAD~1` but typed `git reset --hard HEAD~1` (or `HEAD~5`), obliterating your recent commits from `git log` and resetting your working tree.

### Recovery Runbook
```bash
# Step 1: Open the flight recorder
git reflog -n 10

# Output example:
# 3b4c1a2 HEAD@{0}: reset: moving to HEAD~1
# 7f8e9d0 HEAD@{1}: commit: feat(billing): add stripe webhook handler
# 1a2b3c4 HEAD@{2}: commit: feat(billing): add invoice model

# Step 2: Verify the lost commit
git show HEAD@{1}

# Step 3: Instantly restore branch pointer to the pre-reset commit
git reset --hard HEAD@{1}

# Step 4: Verify your commits and files are back!
git log -1 --oneline
```

---

## 4. Scenario 2: Accidental Branch Deletion (`git branch -D`)

### The Problem
You ran `git branch -D feature/payment-v2`, believing it was merged, only to discover unmerged production code was on that branch.

### Recovery Runbook
```bash
# Step 1: Search reflog for the branch checkout or commit event
git reflog | grep "feature/payment-v2"

# Output example:
# 8c9d01e HEAD@{4}: commit: feat(payment): implement refund API
# 4a5e62f HEAD@{5}: checkout: moving from main to feature/payment-v2

# Step 2: Note the commit SHA (8c9d01e)
# Step 3: Resurrect the branch at that exact SHA
git branch feature/payment-v2 8c9d01e

# Step 4: Checkout the resurrected branch
git switch feature/payment-v2
git log -2 --oneline
```

---

## 5. Scenario 3: Botched Interactive Rebase or Dropped Commits

### The Problem
During an interactive rebase (`git rebase -i`), you accidentally marked commits as `drop`, resolved conflicts incorrectly, or quit the editor with errors.

### Recovery Runbook
```bash
# Approach A: If rebase is currently halted in conflict:
git rebase --abort

# Approach B: If rebase completed and committed the bad history:
# Option 1: Use ORIG_HEAD (Git automatically records the pre-rebase tip in ORIG_HEAD)
git reset --hard ORIG_HEAD

# Option 2: If ORIG_HEAD was overwritten, use reflog
git reflog show --date=relative | grep "rebase (start)" -A 1
# 9e2cf31 HEAD@{6}: rebase (start): checkout origin/main
# 3a7b942 HEAD@{7}: commit: feat: pre-rebase commit tip

git reset --hard HEAD@{7}
```

---

## 6. Scenario 4: Commits Made in Detached HEAD State

### The Problem
You checked out a commit directly (`git checkout 4b2e8d1`), added two commits, and then ran `git switch main`. Git warned: `Warning: you are leaving 2 commits behind, not connected to any of your branches`.

### Recovery Runbook
```bash
# Step 1: Query reflog for the detached commits
git reflog -n 5

# Output example:
# 1f8e7d6 HEAD@{0}: checkout: moving from 9e2cf31 to main
# 9e2cf31 HEAD@{1}: commit: feat(api): detached commit 2
# 4a5e62f HEAD@{2}: commit: feat(api): detached commit 1

# Step 2: Create a branch directly from the detached HEAD tip (HEAD@{1})
git branch feature/detached-work HEAD@{1}

# Step 3: Switch to the newly named branch
git switch feature/detached-work
git log -2 --oneline
```

---

## 7. Scenario 5: Accidentally Dropped or Cleared Stashes

### The Problem
You ran `git stash drop` or `git stash clear` and lost uncommitted work that was stored in the stash stack. (Stashes are not tracked in `HEAD` reflog!)

### Recovery Runbook
```bash
# Step 1: Scan for dangling commits created by git stash
git fsck --lost-found --unreachable | grep "commit"

# Output example:
# dangling commit 4f5a6b7c8d9e0f1a2b3c4d5e6f7a8b9c0d1e2f3a
# dangling commit 7a8b9c0d1e2f3a4b5c6d7e8f9a0b1c2d3e4f5a6b

# Step 2: Find the stash commit by inspecting commit subjects
for sha in $(git fsck --lost-found 2>/dev/null | awk '{print $3}'); do
    git log -1 --format="%h %ci: %s" $sha 2>/dev/null | grep "WIP on"
done

# Output example:
# 4f5a6b7 2026-11-25 10:14:02 +0530: WIP on feature: 8c9d01e feat: auth logic

# Step 3: Apply the recovered stash commit!
git stash apply 4f5a6b7
```

---

## 8. Scenario 6: Uncommitted Work Overwritten (IDE Local History)

### The Problem
You modified files on disk and ran `git restore .` or `git checkout -- .` before running `git add` or `git commit`. Git has **no object** representing these edits.

### Recovery Runbook
1. **Visual Studio Code**:
   - Open the affected file in VS Code.
   - Look at the bottom of the Explorer panel for the **Timeline** view.
   - VS Code maintains an internal `Local History` SQLite database recording every save.
   - Click on the prior timestamp to view and restore the historical version.
2. **JetBrains IDEs (IntelliJ / WebStorm / PyCharm)**:
   - Right-click the project folder or file -> **Local History** -> **Show History**.
   - Select the version prior to the Git command -> click **Revert**.
3. **macOS Time Machine / Windows File History**:
   - Right-click file in Windows Explorer -> **Restore previous versions** / **Shadow Copies**.

---

## 9. Scenario 7: Remote Branch Overwritten by Teammate Force Push

### The Problem
A collaborator ran `git push --force` and replaced the remote branch `feature/auth` with an older version, wiping out 10 commits pushed earlier.

### Recovery Runbook
```bash
# Step 1: If ANY collaborator still has the branch locally:
# Switch to their machine:
git log -1 --format="%H" feature/auth
# Output: a1b2c3d4e5f6...

# Re-push from their local copy:
git push --force-with-lease origin feature/auth

# Step 2: If NO collaborator has the commits locally:
# Query GitHub Events API (GitHub retains push events for 90 days):
curl -s -H "Authorization: token $GITHUB_TOKEN" \
  "https://api.github.com/repos/org/repo/events" | \
  grep -A 10 "PushEvent" | grep -B 2 -A 5 "refs/heads/feature/auth"

# Locate the "before" SHA in the payload:
# "before": "a1b2c3d4e5f67a8b9c0d1e2f3a4b5c6d7e8f9a0b"

# Fetch the orphaned commit directly from GitHub into a new branch:
git fetch origin a1b2c3d4e5f67a8b9c0d1e2f3a4b5c6d7e8f9a0b:recovered-auth
git push origin recovered-auth:feature/auth --force-with-lease
```

---

## 10. Garbage Collection Prevention & fsck Deep Scan

### Emergency Object Extraction Script
When everything else fails, dump all dangling commits to text files:
```bash
#!/usr/bin/env bash
mkdir -p .git/salvaged_commits
for sha in $(git fsck --lost-found 2>/dev/null | awk '{print $3}'); do
    if git cat-file -t $sha 2>/dev/null | grep -q "commit"; then
        echo "Extracting commit $sha..."
        git log -1 --stat $sha > .git/salvaged_commits/$sha.txt
    fi
done
echo "Inspection complete. Check .git/salvaged_commits/ for recovered patches."
```

---

## 11. Interview Questions & Answers

<details>
<summary><strong>Q1: What are the three primary sources for recovering lost work in Git, ranked by priority?</strong></summary>

1. **Git Reflog (`git reflog`)**: Recovers any committed or stashed state that was pointed to by `HEAD`, branches, or stash within the last 30–90 days.
2. **Git Database Integrity Scan (`git fsck --lost-found`)**: Discovers dangling commits and blobs in `.git/objects/` that have lost their reflog pointers but have not yet been purged by garbage collection.
3. **IDE Local History / File System Snapshots**: The only mechanism capable of recovering uncommitted, unstaged modifications that were erased before Git objects could be created.
</details>

<details>
<summary><strong>Q2: Why does `git stash drop` require `git fsck` rather than `git reflog` for recovery?</strong></summary>

`HEAD` reflog only tracks commits checked out by `HEAD`. Stashes are stored in a dedicated reference (`refs/stash`) as commit objects with two or three parents. When `git stash drop` is executed, Git removes the entry from `refs/stash`. While the commit object still exists in `.git/objects/`, it is no longer referenced by `HEAD` or any reflog. Therefore, `git fsck --lost-found` must be used to locate the dangling commit.
</details>

<details>
<summary><strong>Q3: Explain how `ORIG_HEAD` works during operations like `rebase`, `merge`, or `reset`.</strong></summary>

Before executing operations that move `HEAD` in a drastic or non-linear way (`rebase`, `merge`, `reset`), Git automatically writes the current SHA of `HEAD` to a file called `.git/ORIG_HEAD`. If the operation completes with undesirable results, running `git reset --hard ORIG_HEAD` instantly returns `HEAD` to its exact pre-operation state.
</details>

<details>
<summary><strong>Q4: Under what conditions is work truly, permanently irrecoverable in Git?</strong></summary>

Work is permanently lost when:
1. Files were modified in the working directory and discarded (`git restore .` or `git checkout -- .`) without ever being staged (`git add`) or committed, and the IDE lacks Local History.
2. Unreachable objects were explicitly purged by executing `git reflog expire --expire=now --all && git gc --prune=now`.
3. The `.git` directory itself was deleted from the physical disk without backups.
</details>

<details>
<summary><strong>Q5: Can you recover a file that was staged with `git add` but never committed before a `git reset --hard`? How?</strong></summary>

Yes! Running `git add <file>` writes the file's content as a blob object into `.git/objects/` and computes its SHA hash. Even if `git reset --hard` clears the index, the blob object remains in the object store. Run:
```bash
git fsck --lost-found
```
Locate the dangling blobs in `.git/lost-found/other/` and inspect them using `git cat-file -p <blob-sha>` to recover the staged code!
</details>

<details>
<summary><strong>Q6: Why is running `git clean -fdx` particularly dangerous during disaster recovery?</strong></summary>

`git clean -fdx` deletes all untracked files and directories, including ignored files (like `.env`, build artifacts, and newly created files). Because untracked files are not tracked by Git, they have no blobs in `.git/objects/` and cannot be recovered by Git tools.
</details>

<details>
<summary><strong>Q7: How do you prevent Git from automatically running garbage collection during an active recovery session?</strong></summary>

Disable auto-gc in the repository configuration:
```bash
git config gc.auto 0
```
This ensures Git will not trigger background repacking or pruning while you are inspecting dangling objects.
</details>

<details>
<summary><strong>Q8: What GitHub feature allows you to recover a force-pushed branch if all local copies are destroyed?</strong></summary>

GitHub's Events API (`/repos/:owner/:repo/events`). Push events record both the `before` SHA and the `after` SHA. As long as GitHub hasn't garbage-collected the loose object (typically retained for weeks), you can fetch the `before` SHA directly into your local repository using `git fetch origin <before-sha>:restored-branch`.
</details>

---

## 12. Practice Exercises

<details>
<summary><strong>Exercise 1: Rescue Staged Uncommitted Blob After Hard Reset</strong></summary>

**Scenario**: Stage a new file without committing, run `git reset --hard`, and rescue the blob with `git fsck`.
```bash
mkdir triage-lab && cd triage-lab && git init
echo "base" > base.txt && git add base.txt && git commit -m "feat: init"

# Stage critical file without committing
echo "SECRET_ALGORITHM_SOURCE_CODE" > algo.py
git add algo.py

# Accidental hard reset
git reset --hard HEAD
ls algo.py 2>/dev/null || echo "File is gone from disk!"

# Rescue from dangling blob
BLOB_SHA=$(git fsck --lost-found | awk '{print $3}')
git cat-file -p $BLOB_SHA > algo_recovered.py
cat algo_recovered.py
```
</details>

<details>
<summary><strong>Exercise 2: Rescue a Dropped Stash</strong></summary>

**Scenario**: Stash changes, drop the stash, locate it with `git fsck`, and reapply.
```bash
echo "wip edits" >> base.txt
git stash push -m "critical-wip"
git stash drop
git stash list
# Stash list is empty!

# Locate dropped stash commit
STASH_SHA=$(git fsck --lost-found 2>/dev/null | awk '{print $3}' | while read sha; do
    if git log -1 --format="%s" $sha 2>/dev/null | grep -q "WIP on master:.*critical-wip"; then
        echo $sha
        break
    fi
done)

git stash apply $STASH_SHA
cat base.txt
```
</details>

<details>
<summary><strong>Exercise 3: Rescue Work Committed in Detached HEAD</strong></summary>

**Scenario**: Commit in detached HEAD, switch to master, and recover via branch creation.
```bash
git checkout HEAD~0
echo "detached code" > detached.txt && git add detached.txt && git commit -m "feat: detached code"
DETACHED_SHA=$(git rev-parse HEAD)
git switch master

# Recover
git branch rescue-detached $DETACHED_SHA
git switch rescue-detached
git log -1 --oneline
```
</details>

<details>
<summary><strong>Exercise 4: Test ORIG_HEAD Recovery</strong></summary>

**Scenario**: Execute a multi-commit reset and undo instantly with `ORIG_HEAD`.
```bash
echo "c1" >> base.txt && git commit -am "c1"
echo "c2" >> base.txt && git commit -am "c2"
git reset --hard HEAD~2

# Undo reset using ORIG_HEAD
git reset --hard ORIG_HEAD
git log -2 --oneline
```
</details>

<details>
<summary><strong>Exercise 5: Prevent Auto-GC During Incidents</strong></summary>

**Scenario**: Set `gc.auto` to 0 to safeguard dangling objects.
```bash
git config gc.auto 0
git config --get gc.auto
```
</details>

<details>
<summary><strong>Exercise 6: Inspect Reflog Author and Action Logs</strong></summary>

**Scenario**: Query reflog formatted with action labels and relative timestamps.
```bash
git reflog --format="%Cred%h%Creset %C(yellow)%gd%Creset %gs %Cgreen(%cr)%Creset"
```
</details>

---

## 13. Summary & Emergency Cheat Sheet

```bash
# ===================================================================
# EMERGENCY GIT RESCUE CHEAT SHEET
# ===================================================================

# 1. UNDO ACCIDENTAL HARD RESET
git reflog -n 5
git reset --hard HEAD@{1}

# 2. RESTORE FORCE-DELETED BRANCH
git reflog | grep "branch-name"
git branch <branch-name> <commit-sha>

# 3. UNDO BAD REBASE OR MERGE
git reset --hard ORIG_HEAD

# 4. RESCUE DETACHED HEAD WORK
git reflog | grep "checkout: moving"
git branch <new-branch> <detached-commit-sha>

# 5. RESCUE DROPPED STASH
git fsck --lost-found
git stash apply <dangling-commit-sha>

# 6. RESCUE STAGED BUT UNCOMMITTED FILE
git fsck --lost-found
git cat-file -p <dangling-blob-sha> > rescued-file.txt

# 7. SAFETY LOCK (PREVENT PRUNING)
git config gc.auto 0
```

---

## 14. Further Reading
- [Official Git Documentation - git-reflog](https://git-scm.com/docs/git-reflog)
- [Official Git Documentation - git-fsck](https://git-scm.com/docs/git-fsck)
- [Pro Git Book - Maintenance and Data Recovery](https://git-scm.com/book/en/v2/Git-Internals-Maintenance-and-Data-Recovery)
- [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/README.md)
