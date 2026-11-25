---
title: "Git Cherry-Pick: Selectively Porting Commits"
category: "04-devops"
subcategory: "git-github / 03-history-rewriting"
tags: ["git", "cherry-pick", "history-rewriting", "hotfix", "backport", "merge-commits"]
difficulty: "Intermediate"
estimated_read_time: "22 min"
prerequisites:
  - "01-fundamentals/04-commits"
  - "02-commands/06-branching"
  - "02-commands/07-merging"
last_updated: "2026-11-25"
---

# Git Cherry-Pick: Selectively Porting Commits

> **Navigation**: [← Previous: 02-rebase.md](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/02-rebase.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/README.md) | [Next: 04-reflog-and-recovery.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/04-reflog-and-recovery.md)

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

### What is `git cherry-pick`?
`git cherry-pick` is a Git command that applies the changes introduced by one or more existing commits onto the current working branch as new, distinct commits. Instead of merging or rebasing an entire branch containing dozens of unrelated commits, cherry-picking surgically extracts specific commits by SHA and creates equivalent new commits on `HEAD`.

### Why Do We Use It?
1. **Production Hotfix Backporting**: A critical bug fix implemented on the bleeding-edge `main` branch needs to be backported to an older, stable maintenance release branch (`release/v2.4.x`) without pulling in unreleased features.
2. **Salvaging Abandoned Work**: A feature branch was discarded, but two specific utility functions or bug fixes inside it are valuable. Cherry-picking extracts just those commits onto your current branch.
3. **Accidental Commit on Wrong Branch**: A developer committed work onto `main` instead of a feature branch. They can switch to the feature branch, cherry-pick the commit, and reset `main`.
4. **Coordinating Across Microservices or Multi-Branch Environments**: Quickly pulling a shared schema update or urgent configuration patch across isolated long-lived environment branches.

---

## 2. Core Concepts

### 1. Patch Replay Mechanism
Like rebase, cherry-pick computes the difference between the target commit and its immediate parent (`diff = commit - parent`). It then takes that diff and applies it onto the current `HEAD` commit.
Because the parent of the newly created commit is `HEAD` rather than the original commit's parent, the cherry-picked commit receives a brand-new SHA hash.

### 2. Traceability with `-x`
When cherry-picking, Git generates a new commit with the original commit message. However, the connection between the original commit and the cherry-picked commit is lost unless you pass `-x`. With `-x`, Git appends a standardized metadata line to the commit message:
```text
(cherry picked from commit 7a8b9c0d1e2f3a4b5c6d7e8f9a0b1c2d3e4f5a6b)
```
This enables auditability, git log cross-referencing, and automated tooling tracking backports.

### 3. Cherry-Picking Merge Commits (`-m`)
Standard commits have exactly one parent. A merge commit has two (or more) parents. Git cannot calculate what diff to apply unless you explicitly tell it which parent should serve as the baseline:
```bash
git cherry-pick -m 1 <merge-commit-sha>
```
Specifying `-m 1` instructs Git to apply the changes introduced by the merged branch relative to the mainline parent.

### 4. Non-Committing Cherry-Pick (`-n` / `--no-commit`)
Passing `-n` applies the patch changes directly to the staging area and working directory without generating a commit object. This allows batching multiple changes or amending them before creating a single unified commit.

---

## 3. How It Works Internally

```
Scenario: Backporting bug fix Commit F from 'main' to 'release-1.0'

Initial Topology:
  release-1.0:  A ─── B  [release-1.0, HEAD]
               /
  main:       A ─── C ─── D ─── E ─── F ─── G
                                      │
                               (Target Commit to pick)

============================================================
Execution: git switch release-1.0
           git cherry-pick -x F
============================================================

Internal Steps:
1. Git calculates diff between Commit F and its parent Commit E:
   diff = F - E
2. Git attempts a 3-way merge applying diff onto Commit B.
3. Git creates NEW Commit F' with parent Commit B.
4. Git appends "(cherry picked from commit F)" to message.

Final Topology:
  release-1.0:  A ─── B ─── F' [release-1.0, HEAD]  (F' has new SHA!)
               /
  main:       A ─── C ─── D ─── E ─── F ─── G
```

### Git State Directory
If conflicts occur during a cherry-pick, Git saves intermediate transaction state in `.git/sequencer/`:
- `todo`: Remaining commits to be picked (if a range was specified).
- `head`: Current HEAD SHA before the operation.
- `abort-safety`: State check to ensure safe rollback.

---

## 4. Syntax & Usage

```bash
# Cherry-pick a single commit onto current branch
git cherry-pick <commit-sha>

# Cherry-pick with audit trail reference (-x)
git cherry-pick -x <commit-sha>

# Cherry-pick without committing (stages changes in index)
git cherry-pick -n <commit-sha>

# Cherry-pick a sequence of commits (A is NOT included, B is included)
git cherry-pick <commit-A>..<commit-B>

# Cherry-pick a sequence including commit A
git cherry-pick <commit-A>^..<commit-B>

# Cherry-pick multiple individual commits in order
git cherry-pick <sha1> <sha2> <sha3>

# Cherry-pick a merge commit using parent 1 as baseline
git cherry-pick -m 1 <merge-commit-sha>

# Resolving conflicts during cherry-pick
git add <resolved-files>
git cherry-pick --continue

# Aborting a cherry-pick and restoring working tree
git cherry-pick --abort

# Skipping the current problematic commit in a range
git cherry-pick --skip
```

---

## 5. Practical Examples

### Example 1: Backporting a Security Hotfix to a Production Release Branch
```bash
# 1. Developer fixed SQL injection on main (commit 4b2e8d1)
# 2. Need to backport to release/v2.0
git switch release/v2.0
git pull origin release/v2.0

# 3. Cherry-pick with audit traceability
git cherry-pick -x 4b2e8d1

# 4. Verify commit has reference to main SHA
git log -1
# Output includes: (cherry picked from commit 4b2e8d1f7c8a...)

# 5. Push hotfix to production release branch
git push origin release/v2.0
```

### Example 2: Moving a Commit Accompanying Wrong Branch
```bash
# Scenario: You made commit 9f1a2b3 on 'main' by mistake instead of 'feature-checkout'
git switch feature-checkout

# Cherry-pick the commit onto the correct branch
git cherry-pick 9f1a2b3

# Switch back to main and rewind pointer to remove the accidental commit
git switch main
git reset --hard HEAD~1
```

### Example 3: Extracting Changes into Staging Without Committing (`-n`)
```bash
# Bring changes from an experimental commit without creating a commit yet
git cherry-pick -n 3c7a8b9

# Check status: changes are staged in index
git status -sb

# Modify code, run tests, and create a custom commit
git commit -m "feat(perf): adapt database indexing logic from experiment"
```

### Example 4: Cherry-Picking a Range of Commits
```bash
# Cherry-pick 3 consecutive commits: 1a2b, 3c4d, 5e6f
# Using inclusive range syntax:
git cherry-pick 1a2b^..5e6f

# Inspect log to verify all 3 commits were applied sequentially
git log --oneline -n 3
```

### Example 5: Resolving Conflicts During Cherry-Pick
```bash
# Cherry-pick fails due to conflict
git cherry-pick 8e9a1b2
# Auto-merging src/api.ts
# CONFLICT (content): Merge conflict in src/api.ts
# error: could not apply 8e9a1b2... fix(api): rate limit

# Check status
git status -s
# UU src/api.ts

# Manually edit src/api.ts to resolve conflict markers, then stage:
git add src/api.ts

# Continue cherry-pick (Git completes the commit using original message)
git cherry-pick --continue
```

---

## 6. Best Practices

1. **Always Use `-x` for Production Backports**: The `-x` flag provides critical supply chain and audit traceability showing which commit generated the backport.
2. **Favor Merges Over Cherry-Picks for Feature Branches**: Cherry-picking creates duplicate commit objects with identical diffs but distinct SHAs. If the two branches are merged later, duplicate commits complicate history and can cause false merge conflicts.
3. **Prefer Small, Atomic Commits for Easy Picking**: If a bug fix is coupled with unrelated refactorings in a 500-line commit, cherry-picking it cleanly onto an older branch is nearly impossible.
4. **Use `-n` When Amending or Consolidating**: When adapting changes from another branch that require adjustments for the target environment, use `-n` so you can verify and commit cleanly.
5. **Check `git cherry` Before Picking**: Use `git cherry <upstream> <head>` to see which commits from your branch have already been applied to upstream.
6. **Ensure Clean Working Tree Before Cherry-Picking**: Always ensure working directory and index are clean (`git status`) before initiating cherry-pick to prevent conflict complications.

---

## 7. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Cherry-picking a merge commit without `-m` | `fatal: commit ... is a merge but no -m option was given` | Merge commits have multiple parents | Pass `-m 1` (or appropriate parent index) |
| Omitting `-x` during release backports | Lost provenance; hard to verify if CVE fix was ported | Forgot `-x` flag | Use `git cherry-pick -x <sha>` |
| Over-cherry-picking between feature branches | Duplicate commits and false conflicts on eventual merge | Using cherry-pick instead of proper branching/rebase | Merge or rebase common branch instead |
| Running `git commit` instead of `--continue` | Creates orphan sequencer state | Habitual command execution | Stage with `git add`, then run `git cherry-pick --continue` |
| Cherry-picking out of chronological order | Cascading syntax/dependency conflicts | Dependent commits were omitted | Pick all dependent prerequisite commits or use range |

---

## 8. Performance, Security & Cost

- **Duplicate Commits in Object Database**: Every cherry-pick duplicates the commit and tree objects in `.git/objects/`. While negligible in small repositories, high-volume automated bot backporting across hundreds of releases can bloat repository object count.
- **Merge Tracking Overhead**: Unlike tools like Mercurial or SVN, standard Git does not record cherry-pick metadata in refs or tree metadata (only in the commit message when `-x` is used). Future branch merges rely on patch-id matching (`git patch-id`) to detect duplicate changes.
- **CVE & Patch Auditing**: In regulated enterprise systems (SOC2, ISO27001), missing `-x` backport flags can trigger audit compliance flags because automated scanners cannot correlate CVE patches across release branches.

---

## 9. Comparison Table

| Dimension | `git cherry-pick` | `git merge` | `git rebase` |
|:---|:---|:---|:---|
| **Granularity** | Commit-by-commit (selective) | Entire branch history | Branch patch sequence |
| **New Commits Created** | Yes (1 new SHA per picked commit) | Yes (1 merge commit) | Yes (rewrites all replayed SHAs) |
| **Branch Topology** | Independent parallel commit | Connects two branch histories | Straightens branch into linear line |
| **Preserves Original Hashes** | No (generates new SHA) | Yes (original commits preserved) | No (rewrites SHA) |
| **Best Used For** | Hotfix backports, salvaging single commits | Integrating completed features | Synchronizing private branch with upstream |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: Why does `git cherry-pick` fail with an error when run against a merge commit without flags, and how is it resolved?</strong></summary>

A standard commit has one parent, so Git calculates the change as `diff = commit - parent`. A merge commit has two or more parents (e.g. `HEAD` of main and `HEAD` of feature). Git cannot determine which parent represents the mainline baseline from which changes should be measured.
To resolve this, you must supply the `-m <parent-number>` flag (usually `-m 1` to indicate the mainline branch):
```bash
git cherry-pick -m 1 <merge-commit-hash>
```
</details>

<details>
<summary><strong>Q2: What is the risk of using `git cherry-pick` extensively between two active feature branches instead of merging?</strong></summary>

Cherry-picking creates a new commit object with a different SHA that introduces the exact same diff. If both branches are later merged into `main`, Git has to evaluate both sets of commits. While Git's merge engines (`ort`/`recursive`) use patch-id analysis to detect and skip identical diffs, any slight divergence or subsequent modification can cause false merge conflicts and cluttered `git log` histories containing duplicated commit messages.
</details>

<details>
<summary><strong>Q3: What does the `-x` flag do during a cherry-pick, and why is it standard practice in open-source and enterprise workflows?</strong></summary>

The `-x` flag automatically appends the line `(cherry picked from commit <sha>)` to the original commit message. This creates an immutable audit trail linking the new backported commit to its original commit on the primary development branch. This is essential for:
1. Security auditing (confirming which release branches have received a vulnerability fix).
2. Code reviews (allowing reviewers to inspect the original context and discussion on `main`).
3. Automated release notes and changelog tooling.
</details>

<details>
<summary><strong>Q4: How does `git cherry` (the command, not cherry-pick) work and what is it used for?</strong></summary>

`git cherry <upstream> <head>` compares the commits between `<head>` and `<upstream>` using their patch-ids rather than their commit hashes. It outputs a list of commit hashes prefixed by either:
- `+`: The commit exists in `<head>` but its patch is NOT present in `<upstream>`.
- `-`: The commit exists in `<head>`, but an equivalent patch with the same diff has already been applied to `<upstream>` (e.g. via cherry-pick or rebase).
It helps developers determine which local commits still need to be pushed or backported.
</details>

<details>
<summary><strong>Q5: What is the difference between `git cherry-pick A..B` and `git cherry-pick A^..B`?</strong></summary>

In Git revision range syntax:
- `A..B` represents all commits reachable from `B` that are NOT reachable from `A`. This is half-open: commit `A` is **excluded**, and commits from `A`'s child up to `B` are picked.
- `A^..B` starts from the parent of `A` (`A^`), which means commit `A` is **included** in the cherry-pick range up through `B`.
</details>

<details>
<summary><strong>Q6: Can you cherry-pick a commit from an entirely different local repository without pushing to a remote? How?</strong></summary>

Yes. You can add the second local repository as a local file-based remote or fetch its objects directly:
```bash
git remote add other-repo /path/to/other/repository
git fetch other-repo
git cherry-pick <commit-sha-from-other-repo>
```
Git will access the objects from the other repo's object database and apply the commit onto your branch.
</details>

<details>
<summary><strong>Q7: How do you abort a cherry-pick that encountered unexpected conflicts, and how do you skip an unneeded commit during a multi-commit cherry-pick?</strong></summary>

- To cancel completely and return the branch and working tree to its exact pre-cherry-pick state:
  ```bash
  git cherry-pick --abort
  ```
- To skip the current problematic commit in a multi-commit range and proceed with the remaining commits in the sequencer queue:
  ```bash
  git cherry-pick --skip
  ```
</details>

<details>
<summary><strong>Q8: What happens if you cherry-pick a commit that introduces changes that already exist on your current branch?</strong></summary>

Git detects that the patch produces an empty diff (`The previous cherry-pick is now empty, possibly due to conflict resolution`). Git halts and prompts you:
- If you run `git cherry-pick --skip`, Git skips creating the empty commit and continues.
- If you pass `--allow-empty`, Git creates a commit object with the original message but pointing to the identical tree object as its parent.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Backporting a Fix with `-x`</strong></summary>

**Scenario**: Create a release branch, make a bug fix on main, and backport it to release using `-x`.
```bash
mkdir cherry-lab && cd cherry-lab && git init
echo "v1.0" > app.txt && git add app.txt && git commit -m "feat: release 1.0"
git branch release-1.0

# Add new feature and a bug fix on main
echo "feature v2" >> app.txt && git commit -am "feat: add v2 feature"
echo "bug fix" >> app.txt && git commit -am "fix(core): critical security patch"

FIX_SHA=$(git rev-parse HEAD)

# Switch to release branch and cherry-pick
git switch release-1.0
git cherry-pick -x $FIX_SHA

# Inspect log to verify audit trail:
git log -1
```
</details>

<details>
<summary><strong>Exercise 2: Non-Committing Cherry-Pick (`-n`)</strong></summary>

**Scenario**: Extract changes from an experimental commit into the staging area and inspect with `git status`.
```bash
git switch main
echo "exp code" > exp.txt && git add exp.txt && git commit -m "exp: test feature"
EXP_SHA=$(git rev-parse HEAD)

git switch release-1.0
git cherry-pick -n $EXP_SHA

# Verify changes are staged without commit:
git status -s
git diff --staged
git reset --hard HEAD
```
</details>

<details>
<summary><strong>Exercise 3: Cherry-Picking a Commit Range</strong></summary>

**Scenario**: Make two consecutive commits on a topic branch and cherry-pick both in a single command.
```bash
git switch -c topic
echo "t1" > t1.txt && git add t1.txt && git commit -m "feat: topic 1"
SHA1=$(git rev-parse HEAD)
echo "t2" > t2.txt && git add t2.txt && git commit -m "feat: topic 2"
SHA2=$(git rev-parse HEAD)

git switch release-1.0
git cherry-pick $SHA1^..$SHA2
git log -2 --oneline
```
</details>

<details>
<summary><strong>Exercise 4: Resolving Conflicts During Cherry-Pick</strong></summary>

**Scenario**: Create conflicting changes on two branches, attempt cherry-pick, resolve conflict, and continue.
```bash
git switch main
echo "line on main" >> app.txt && git commit -am "change on main"
MAIN_SHA=$(git rev-parse HEAD)

git switch release-1.0
echo "conflicting line" >> app.txt && git commit -am "change on release"

# Cherry pick will conflict:
git cherry-pick $MAIN_SHA || true

# Inspect conflict
git status -s
# Fix file, stage and continue
echo "resolved line" > app.txt
git add app.txt
git cherry-pick --continue
```
</details>

<details>
<summary><strong>Exercise 5: Aborting a Cherry-Pick</strong></summary>

**Scenario**: Trigger a cherry-pick conflict and cleanly abort.
```bash
git switch main
echo "conflict 2" >> app.txt && git commit -am "another conflict"
CONF_SHA=$(git rev-parse HEAD)

git switch release-1.0
git cherry-pick $CONF_SHA || true
git cherry-pick --abort
git status
```
</details>

<details>
<summary><strong>Exercise 6: Inspecting Applied Patches with `git cherry`</strong></summary>

**Scenario**: Use `git cherry` to determine whether a commit on topic branch has already been cherry-picked onto main.
```bash
git switch topic
git cherry main topic
# Notice '+' indicates commits not yet in main
```
</details>

---

## 12. Summary & Cheat Sheet

```bash
# Standard cherry-pick
git cherry-pick <commit-sha>

# Cherry-pick with provenance audit trail (Recommended for backports)
git cherry-pick -x <commit-sha>

# Cherry-pick changes to staging area without committing
git cherry-pick -n <commit-sha>

# Cherry-pick multiple individual commits
git cherry-pick <sha1> <sha2> <sha3>

# Cherry-pick inclusive range
git cherry-pick <start-sha>^..<end-sha>

# Cherry-pick a merge commit using parent 1 as base
git cherry-pick -m 1 <merge-commit-sha>

# Sequencer Conflict Controls
git add <resolved-files>
git cherry-pick --continue
git cherry-pick --abort
git cherry-pick --skip
```

---

## 13. Further Reading
- [Official Git Documentation - git-cherry-pick](https://git-scm.com/docs/git-cherry-pick)
- [Pro Git Book - Rebasing and Cherry-Picking](https://git-scm.com/book/en/v2/Git-Branching-Rebasing)
- [Git Cherry-Pick Merge Option (-m) Explained](https://git-scm.com/blog/2010/03/02/undoing-merges.html)
- [03-history-rewriting/04-reflog-and-recovery.md](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/04-reflog-and-recovery.md)
