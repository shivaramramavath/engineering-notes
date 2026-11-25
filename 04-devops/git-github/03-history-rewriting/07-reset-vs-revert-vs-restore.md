---
title: "Git Reset vs Revert vs Restore: The Definitive Undo Matrix"
category: "04-devops"
subcategory: "git-github / 03-history-rewriting"
tags: ["git", "reset", "revert", "restore", "undo", "three-trees", "porcelain"]
difficulty: "Intermediate-Advanced"
estimated_read_time: "25 min"
prerequisites:
  - "01-fundamentals/03-working-tree-and-staging-area"
  - "01-fundamentals/04-commits"
  - "02-commands/04-undoing-changes"
last_updated: "2026-11-25"
---

# Git Reset vs Revert vs Restore: The Definitive Undo Matrix

> **Navigation**: [← Previous: 06-rewriting-shared-history-safely.md](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/06-rewriting-shared-history-safely.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/README.md) | [Next: 08-recovering-lost-work-playbook.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/08-recovering-lost-work-playbook.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts & The Three Trees](#2-core-concepts--the-three-trees)
3. [How Each Command Operates Internally](#3-how-each-command-operates-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices & Decision Flowchart](#6-best-practices--decision-flowchart)
7. [Common Mistakes](#7-common-mistakes)
8. [Performance & Safety Implications](#8-performance--safety-implications)
9. [The Definitive Comparison Matrix](#9-the-definitive-comparison-matrix)
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### The Historical Confusion
For over a decade, developers struggled with `git checkout` and `git reset` because both commands performed overloaded, disparate duties: checking out branches, restoring individual files, moving HEAD pointers, and manipulating the index.

### The Modern Decomposition (Git 2.23+)
To eliminate ambiguity and prevent accidental data destruction, Git 2.23 cleanly separated these responsibilities into distinct porcelain commands:
- **`git switch`**: Exclusively changes branches and creates new branches.
- **`git restore`**: Exclusively undoes changes in the working directory and staging area without touching the commit history.
- **`git reset`**: Moves the branch `HEAD` pointer and optionally resets the index or working tree (history rewriting).
- **`git revert`**: Creates an entirely new forward commit that mathematically inverts a previous commit's patch (safe for public branches).

---

## 2. Core Concepts & The Three Trees

Understanding undo operations requires understanding Git's **Three Trees Architecture**:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. HEAD (Repository)                                        │
│    The commit graph: last committed state on current branch │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. Index (Staging Area)                                     │
│    The proposed next commit: prepared files in .git/index   │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. Working Directory                                        │
│    The sandbox: physical files on disk you actively edit    │
└─────────────────────────────────────────────────────────────┘
```

### The Three Reset Modes:
1. **`git reset --soft <target>`**:
   - Moves `HEAD` ref to `<target>`.
   - Leaves **Index** unchanged.
   - Leaves **Working Directory** unchanged.
   - Result: All changes between the old HEAD and target become staged in the index!
2. **`git reset --mixed <target>`** (Default):
   - Moves `HEAD` ref to `<target>`.
   - Overwrites **Index** to match `<target>`.
   - Leaves **Working Directory** unchanged.
   - Result: All changes remain as unstaged edits on disk.
3. **`git reset --hard <target>`**:
   - Moves `HEAD` ref to `<target>`.
   - Overwrites **Index** to match `<target>`.
   - Overwrites **Working Directory** to match `<target>`.
   - Result: All uncommitted work is discarded.

---

## 3. How Each Command Operates Internally

```
Command Operations on the Three Trees:

===================================================================
1. git restore src/app.ts
   Index ──────────────────────────► Working Tree
   (Copies file from index to working directory; HEAD untouched)

2. git restore --staged src/app.ts
   HEAD ───────────────────────────► Index
   (Copies file from HEAD to index; working directory untouched)

3. git reset --soft HEAD~1
   HEAD moves back 1 commit
   Index & Working Tree untouched

4. git reset --hard HEAD~1
   HEAD moves back 1 commit
   Index is updated to match new HEAD
   Working Tree is forcefully overwritten to match new HEAD

5. git revert <commit-sha>
   HEAD moves FORWARD by 1 new commit
   Applies inverted diff of <commit-sha>
   Index & Working Tree updated to new commit state
===================================================================
```

---

## 4. Syntax & Usage

```bash
# RESTORE (Target: Files & Staging Area)
# Discard uncommitted changes in a specific file
git restore path/to/file.ts

# Discard all unstaged changes across entire repo
git restore .

# Unstage a file (remove from index while keeping working tree edits)
git restore --staged path/to/file.ts

# Discard both staged and unstaged changes for a file simultaneously
git restore --staged --worktree path/to/file.ts

# RESET (Target: HEAD Pointer & History)
# Undo last commit, keep changes staged
git reset --soft HEAD~1

# Undo last commit, unstage changes, keep edits on disk (Default)
git reset HEAD~1
git reset --mixed HEAD~1

# Discard last commit and all local edits completely
git reset --hard HEAD~1

# REVERT (Target: Public History Safe Undos)
# Invert the patch of a specific commit and create a new commit
git revert 4b2e8d1

# Revert without opening commit editor
git revert --no-edit 4b2e8d1

# Revert multiple commits into staging without auto-committing
git revert -n 4b2e8d1 3c7a8b9
git commit -m "revert: rollback payment refactoring"

# Revert a merge commit specifying mainline parent
git revert -m 1 <merge-commit-sha>
```

---

## 5. Practical Examples

### Example 1: Discarding Accidental File Edits (`git restore`)
```bash
# You edited config.json and broke it; you want the version from git
git status -s
#  M config.json

# Discard changes in working directory:
git restore config.json

# Working tree is clean again!
git status
```

### Example 2: Unstaging Files Without Losing Edits (`git restore --staged`)
```bash
# You accidentally ran 'git add .' which staged an unwanted file
git status -s
# M  src/app.ts
# A  debug.log

# Unstage debug.log:
git restore --staged debug.log

# debug.log is now untracked/unstaged; changes preserved on disk
git status -s
# M  src/app.ts
# ?? debug.log
```

### Example 3: Consolidating the Last 3 Commits into One (`git reset --soft`)
```bash
# You made 3 messy local commits
git log --oneline -n 3
# 3c4d5e6 wip: formatting
# 2b3c4d5 wip: fixing tests
# 1a2b3c4 feat: implement feature

# Soft reset back 3 commits
git reset --soft HEAD~3

# All changes are now cleanly staged in the index!
git status -s
# M  src/feature.ts
# A  tests/feature.spec.ts

# Commit once with Conventional Commits message:
git commit -m "feat(core): implement complete feature with unit tests"
```

### Example 4: Safely Rolling Back a Bad Production Commit on `main` (`git revert`)
```bash
# Commit 8a9b0c1 was merged to main and broke production
# NEVER reset or force push main!

# Revert the commit cleanly:
git switch main
git pull origin main
git revert 8a9b0c1 -m "revert: rollback buggy pricing algorithm"

# Push the new forward commit:
git push origin main
# Production CI deploys the rollback safely!
```

---

## 6. Best Practices & Decision Flowchart

```
                 How to choose the right undo command:

                        What do you need to undo?
                                  │
         ┌────────────────────────┼────────────────────────┐
         ▼                        ▼                        ▼
  Uncommitted Edits       Private Commits          Public Commits
 (Staging / Workdir)     (Not Pushed to Remote)   (Pushed / Shared)
         │                        │                        │
         ▼                        ▼                        ▼
   `git restore`            `git reset`              `git revert`
 ─────────────────       ──────────────────       ──────────────────
 • Discard unstaged:     • Keep staged:           • Create forward
   `restore <file>`        `reset --soft HEAD~1`    inverted commit:
 • Unstage from index:   • Keep uncommitted:        `revert <sha>`
   `restore --staged`      `reset --mixed HEAD~1` • Safe for main!
                         • Discard completely:
                           `reset --hard HEAD~1`
```

---

## 7. Common Mistakes

| Mistake | Consequence | Root Cause | Correct Alternative |
|:---|:---|:---|:---|
| Running `git reset --hard` on public `main` | Forces non-fast-forward divergence for all engineers | Misunderstanding public vs private history | Use `git revert <sha>` |
| Running `git checkout <file>` in modern Git | Confusing branch checkout with file restore | Legacy command syntax | Use `git restore <file>` |
| Unstaged edits destroyed by `reset --hard` | Permanent loss of uncommitted code | `--hard` wipes working directory | Run `git stash` before resetting |
| Reverting a merge commit without `-m` | `fatal: commit is a merge but no -m option was given` | Merge commits have multiple parents | Pass `-m 1` |
| Re-merging a branch after reverting its merge | Reverted changes fail to re-appear | Git considers the original commits already merged | Revert the revert commit first |

---

## 8. Performance & Safety Implications

- **Data Safety Hierarchy**:
  - `git revert`: **100% Safe**. Only appends new commits; never modifies history or working tree unsaved state.
  - `git restore`: **Medium Risk**. Discarding unstaged working tree changes permanently erases disk modifications that were never recorded in Git objects.
  - `git reset --hard`: **High Risk**. Moves HEAD and discards unstaged files. Can be salvaged via reflog for committed state, but uncommitted edits are lost permanently.
- **Merge Engine & Inversion Mechanics**: `git revert` invokes the 3-way merge engine to compute the inverse diff. If subsequent commits have modified neighboring lines, reverts can produce merge conflicts that require manual resolution.

---

## 9. The Definitive Comparison Matrix

| Dimension | `git restore` | `git reset` | `git revert` |
|:---|:---|:---|:---|
| **Primary Target** | Individual files / Staging area | HEAD pointer / History | Existing commit object |
| **Changes Commit History?** | **No** (History untouched) | **Yes** (Rewrites/moves branch tip) | **No** (Appends new commit) |
| **Creates New Commits?** | No | No | **Yes** (1 new commit) |
| **Safe for Public Branches?** | Yes (local files only) | **No** (Requires force push) | **Yes** (Append-only) |
| **Can Recover Lost Edits?** | No (if unstaged) | Committed edits via reflog | N/A (Nothing lost) |
| **Modern Replacement For** | `git checkout -- <file>` | Overloaded reset flags | N/A |
| **Command Level** | File-level porcelain | Branch/History-level | History-level |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What are the differences between `git reset --soft`, `--mixed`, and `--hard` in terms of the Three Trees?</strong></summary>

1. **`--soft`**: Only updates **HEAD**. The branch reference moves to the target commit. The **Index** and **Working Directory** remain completely unchanged. The changes between the target commit and the original commit remain staged in the index.
2. **`--mixed`** (default): Updates **HEAD** and overwrites the **Index** to match the target commit's tree. The **Working Directory** remains unchanged. Changes appear as unstaged modifications.
3. **`--hard`**: Updates **HEAD**, overwrites the **Index**, and forcefully overwrites the **Working Directory** to match the target commit. Any uncommitted local edits are wiped out.
</details>

<details>
<summary><strong>Q2: Why did Git 2.23 introduce `git restore` and `git switch`?</strong></summary>

Historically, `git checkout` was heavily overloaded. It was used to switch branches (`git checkout main`), create branches (`git checkout -b feature`), discard working tree changes (`git checkout -- file.txt`), and inspect detached commits (`git checkout <sha>`). This led to accidental data loss (e.g. typing a file name that matched a branch name). Git 2.23 split these concerns: `git switch` handles branch navigation, and `git restore` handles file recovery and index unstaging.
</details>

<details>
<summary><strong>Q3: Why is `git revert` safe for shared public branches while `git reset` is dangerous?</strong></summary>

`git reset` moves the branch pointer backwards, rewriting the commit history. Pushing this requires a non-fast-forward force push, which disrupts collaborators and can overwrite teammates' work.
`git revert` does not rewrite past history. Instead, it creates a brand-new commit at the tip of the branch whose diff is the mathematical opposite of the target commit. Because it is strictly append-only, it pushes as a clean fast-forward commit without requiring force-push permissions.
</details>

<details>
<summary><strong>Q4: If you revert a merge commit (`git revert -m 1 <merge-sha>`), why does re-merging the feature branch later produce an empty merge? How is this solved?</strong></summary>

When you merge a branch, Git records that all commits on that branch are ancestors of the merge commit. When you revert the merge, you invert the code changes, but Git still records that those commits were already merged. If you later try to re-merge the feature branch, Git sees that the commits are already in history and introduces no new diff.
**Solution**: To re-incorporate the feature, you must revert the original revert commit:
```bash
git revert <revert-commit-sha>
```
This re-applies the code and allows future merges to continue cleanly.
</details>

<details>
<summary><strong>Q5: How do you unstage a file using `git restore` versus `git reset`?</strong></summary>

- Modern porcelain (Git $\ge 2.23$):
  ```bash
  git restore --staged path/to/file.ts
  ```
- Legacy command:
  ```bash
  git reset HEAD path/to/file.ts
  ```
Both commands copy the file's blob from `HEAD` into the `Index` without modifying the physical working directory file.
</details>

<details>
<summary><strong>Q6: Can `git restore` discard both staged and unstaged changes simultaneously? How?</strong></summary>

Yes. By combining the `--staged` and `--worktree` flags:
```bash
git restore --staged --worktree path/to/file.ts
```
Or for the entire working tree:
```bash
git restore --staged --worktree .
```
This resets both the index and working directory to match `HEAD` in a single command.
</details>

<details>
<summary><strong>Q7: What happens when you run `git revert -n` (or `--no-commit`)?</strong></summary>

`git revert -n <commit>` calculates the inverse patch of the specified commit and applies it directly to the staging area and working directory without automatically creating a commit object. This allows developers to bundle multiple reverted commits into a single rollback commit or inspect and adjust the inverted changes before committing.
</details>

<details>
<summary><strong>Q8: What is the risk of using `git restore .` without checking `git status` first?</strong></summary>

`git restore .` permanently discards all unstaged changes in tracked files across the entire repository. Because these modifications were never committed or stashed, they exist nowhere in Git's object database and cannot be recovered via `git reflog`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Safe File Discard with `git restore`</strong></summary>

**Scenario**: Modify a file, discard unstaged changes with `git restore`, and verify clean status.
```bash
mkdir undo-lab && cd undo-lab && git init
echo "original" > app.txt && git add app.txt && git commit -m "feat: init"

echo "broken code" > app.txt
git status -s
git restore app.txt
cat app.txt
# Output: original
```
</details>

<details>
<summary><strong>Exercise 2: Unstage with `git restore --staged`</strong></summary>

**Scenario**: Stage changes and use `restore --staged` to return them to unstaged state.
```bash
echo "staged edit" >> app.txt
git add app.txt
git status -s
# M  app.txt

git restore --staged app.txt
git status -s
#  M app.txt (unstaged!)
```
</details>

<details>
<summary><strong>Exercise 3: Squash Recent History with `reset --soft`</strong></summary>

**Scenario**: Make 3 commits, soft reset to base, and make one clean commit.
```bash
echo "1" >> app.txt && git commit -am "c1"
echo "2" >> app.txt && git commit -am "c2"
echo "3" >> app.txt && git commit -am "c3"

git reset --soft HEAD~3
git status -s
git commit -m "feat: consolidated commit"
git log -1 --oneline
```
</details>

<details>
<summary><strong>Exercise 4: Reverting a Public Commit</strong></summary>

**Scenario**: Create a commit and invert it cleanly using `git revert`.
```bash
echo "buggy code" > bug.txt && git add bug.txt && git commit -m "feat: introduce bug"
BUG_SHA=$(git rev-parse HEAD)

git revert --no-edit $BUG_SHA
git log -2 --oneline
# Notice new 'Revert "feat: introduce bug"' commit!
ls bug.txt 2>/dev/null || echo "bug.txt successfully deleted!"
```
</details>

<details>
<summary><strong>Exercise 5: Restoring a File from a Specific Past Commit</strong></summary>

**Scenario**: Restore a single file as it existed two commits ago without moving `HEAD`.
```bash
echo "v1" > service.txt && git add service.txt && git commit -m "feat: v1"
PAST_SHA=$(git rev-parse HEAD)
echo "v2" > service.txt && git commit -am "feat: v2"

# Restore service.txt from PAST_SHA
git restore --source=$PAST_SHA service.txt
cat service.txt
# Output: v1
```
</details>

<details>
<summary><strong>Exercise 6: Simultaneous Staged and Worktree Discard</strong></summary>

**Scenario**: Stage changes and discard both index and worktree state in one command.
```bash
echo "bad changes" >> app.txt && git add app.txt
git restore --staged --worktree app.txt
git status
```
</details>

---

## 12. Summary & Cheat Sheet

```bash
# DISCARD CHANGES (Working Tree & Index)
git restore <file>                    # Discard unstaged changes in file
git restore .                         # Discard all unstaged changes in repo
git restore --staged <file>           # Unstage file (keep edits on disk)
git restore --staged --worktree .     # Discard all staged & unstaged changes
git restore --source=<sha> <file>     # Restore file from specific historical commit

# REWRITE LOCAL HISTORY (Private Commits)
git reset --soft HEAD~1               # Undo commit, keep changes staged
git reset HEAD~1                      # Undo commit, keep changes unstaged
git reset --hard HEAD~1               # Undo commit, discard all changes

# UNDO PUBLIC HISTORY (Shared Commits)
git revert <sha>                      # Create new forward commit inverting <sha>
git revert -n <sha>                   # Invert <sha> into staging area without committing
git revert -m 1 <merge-sha>           # Revert merge commit using parent 1 as base
```

---

## 13. Further Reading
- [Official Git Documentation - git-restore](https://git-scm.com/docs/git-restore)
- [Official Git Documentation - git-reset](https://git-scm.com/docs/git-reset)
- [Official Git Documentation - git-revert](https://git-scm.com/docs/git-revert)
- [03-history-rewriting/08-recovering-lost-work-playbook.md](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/08-recovering-lost-work-playbook.md)
