---
title: "Git Rebase: Linear History and Interactive Rebasing"
category: "04-devops"
subcategory: "git-github / 03-history-rewriting"
tags: ["git", "rebase", "interactive-rebase", "history-rewriting", "linear-history", "squash", "fixup"]
difficulty: "Advanced"
estimated_read_time: "32 min"
prerequisites:
  - "01-fundamentals/04-commits"
  - "02-commands/06-branching"
  - "02-commands/07-merging"
  - "03-history-rewriting/01-amend"
last_updated: "2026-11-25"
---

# Git Rebase: Linear History and Interactive Rebasing

> **Navigation**: [← Previous: 01-amend.md](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/01-amend.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/README.md) | [Next: 03-cherry-pick.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/03-cherry-pick.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Interactive Rebase Commands (The Todo List)](#5-interactive-rebase-commands-the-todo-list)
6. [Practical Examples](#6-practical-examples)
7. [Best Practices & The Golden Rule](#7-best-practices--the-golden-rule)
8. [Common Mistakes & Conflict Resolution](#8-common-mistakes--conflict-resolution)
9. [Performance, Security & Cost](#9-performance-security--cost)
10. [Comparison: Rebase vs Merge](#10-comparison-rebase-vs-merge)
11. [Interview Questions & Answers](#11-interview-questions--answers)
12. [Practice Exercises](#12-practice-exercises)
13. [Summary & Cheat Sheet](#13-summary--cheat-sheet)
14. [Further Reading](#14-further-reading)

---

## 1. What & Why

### What is `git rebase`?
Rebasing is the process of moving or combining a sequence of commits to a new base commit. Instead of joining two divergent branches with a non-linear merge commit, Git replays the individual patchsets of your topic branch on top of the target base branch, constructing an entirely linear history.

### Why Do We Rebase?
1. **Clean, Linear Project History**: Eliminates noisy, superfluous merge bubbles (`Merge branch 'main' into feature-xyz`), resulting in a single readable progression of changes.
2. **Deterministic Debugging (`git bisect`)**: In a linear git history, `git bisect` can isolate bugs in $O(\log N)$ steps without getting derailed by side-branches or duplicate merge nodes.
3. **Curating Production PRs**: Interactive rebasing (`rebase -i`) allows engineers to clean up local WIP commits, squash checkpoints, reword commit subjects according to Conventional Commits, and reorder logical changes before submitting code review.
4. **Smooth Upstream Synchronization**: Running `git pull --rebase` avoids pollutive merge commits when pulling teammates' updates into a local branch.

---

## 2. Core Concepts

### 1. Base and Upstream
When you rebase branch `feature` onto `main`:
- `Upstream`: The target branch whose tip becomes the new parent (`main`).
- `Branch`: The sequence of commits from the common merge base up to the current branch tip (`feature`).
- `Merge Base`: The most recent common ancestor commit between `feature` and `main`.

### 2. Patch Replay Semantics
Rebase does not literally "move" existing commit objects. It calculates the diff (patch) introduced by each commit on `feature` since the common ancestor, stores these patches in temporary memory, rewinds `HEAD` to the tip of `main`, and reapplies each patch in sequence as an entirely new commit object with a new SHA hash and updated parent reference.

### 3. Interactive Rebase (`git rebase -i`)
Interactive mode opens an editor presenting a "todo list" of all commits between the specified base and `HEAD`. You can direct Git to squash multiple commits into one, discard commits, reword messages, pause execution to edit files, or reorder commits entirely.

### 4. Autosquash and Fixup Commits
By creating commits with `git commit --fixup <target-sha>` and running `git rebase -i --autosquash`, Git automatically groups each fixup commit beneath its designated parent and sets the rebase action to `fixup`, automating history cleanup without manual todo-list reordering.

---

## 3. How It Works Internally

```
Initial State: Feature branched off Commit C, Main advanced with D and E
                  D ─── E  [main]
                 /
      A ─── B ─── C  (Merge Base)
                 \
                  F ─── G  [feature, HEAD]

============================================================
Execution: On branch feature -> git rebase main
============================================================

Step 1: Git determines common ancestor: Commit C.
Step 2: Git generates diff patches: patch(F) and patch(G).
Step 3: Git resets HEAD to Commit E (tip of main).
Step 4: Git applies patch(F) onto E -> produces NEW commit F' (new SHA).
Step 5: Git applies patch(G) onto F' -> produces NEW commit G' (new SHA).
Step 6: Git moves 'feature' ref pointer to G'.

Final State (Linear):
      A ─── B ─── C ─── D ─── E [main] ─── F' ─── G' [feature, HEAD]

(Commits F and G are orphaned; kept in reflog until pruned by gc)
```

### Git Rebase Internal Engine
Modern Git uses the **ort** (Ostensibly Recursive's Twin) merge engine for rebasing (`merge.conflictStyle` and `rebase.backend = merge`). When replaying commits, Git maintains state in `.git/rebase-merge/`:
- `git-rebase-todo`: The queued list of commit instructions.
- `done`: The log of successfully applied steps.
- `head-name`: The name of the original branch being rebased.
- `onto`: The SHA of the new base.
- `patch`: The active diff being evaluated if a conflict halts the process.

---

## 4. Syntax & Usage

```bash
# Rebase current branch onto main
git rebase main

# Rebase a specific branch onto another without switching first
git rebase main feature-auth

# Rebase the last 4 commits interactively
git rebase -i HEAD~4

# Rebase onto a remote branch after fetching
git fetch origin
git rebase origin/main

# Automatically apply fixup and squash commits
git rebase -i --autosquash origin/main

# Abort an active rebase with conflicts and restore original state
git rebase --abort

# Continue rebase after manually resolving conflicts and staging files
git add <resolved-file>
git rebase --continue

# Skip the current problematic patch during rebase
git rebase --skip
```

---

## 5. Interactive Rebase Commands (The Todo List)

When `git rebase -i` opens your editor, you are presented with commands preceding each commit SHA:

```ini
pick 7f3b12a feat(auth): add login endpoint
reword 8c9d01e feat(auth): add jwt token generation
edit 4a5e62f fix(auth): correct token expiry logic
squash 2b1c890 test: add unit tests for token expiry
fixup 9e4f1a2 style: format code with prettier
drop 1c3d5e7 debug: stray console log
```

### Command Definitions:
| Command | Short | Action |
|:---|:---:|:---|
| **pick** | `p` | Keep and apply the commit as-is. |
| **reword** | `r` | Keep commit contents, but pause rebase to edit commit message. |
| **edit** | `e` | Pause rebase immediately after applying commit to amend code/index. |
| **squash** | `s` | Melt commit into the commit above it; concatenate both messages. |
| **fixup** | `f` | Melt commit into the commit above it; discard this commit's message. |
| **exec** | `x` | Run arbitrary shell command (e.g. `npm test`) at this point in history. |
| **drop** | `d` | Completely remove the commit from history. |
| **break** | `b` | Pause rebase at this step for manual inspection. |

---

## 6. Practical Examples

### Example 1: Standard Interactive Squash Workflow
```bash
# 1. Developer has 3 local commits before opening PR
git log --oneline -n 3
# 3b4c1a2 chore: fix typo in user route
# 2a1b9c8 test: add test for user creation
# 1f8e7d6 feat(user): implement create user endpoint

# 2. Launch interactive rebase
git rebase -i HEAD~3

# In the editor, change actions:
# pick 1f8e7d6 feat(user): implement create user endpoint
# squash 2a1b9c8 test: add test for user creation
# fixup 3b4c1a2 chore: fix typo in user route

# Save and exit. Git combines all three into a single cohesive commit!
git log --oneline -n 1
```

### Example 2: The Autosquash Workflow
```bash
# 1. You discover a bug in a commit 3 commits back (hash: 1f8e7d6)
# 2. Fix the bug in your code editor
# 3. Commit with --fixup targeting that commit
git commit -a --fixup 1f8e7d6

# 4. View log: a commit titled "fixup! feat(user)..." is at HEAD
git log --oneline -n 2

# 5. Run rebase with autosquash enabled
git rebase -i --autosquash HEAD~5
# Git automatically reorders the fixup commit directly below 1f8e7d6 and sets action to 'fixup'!
# Simply save and exit!
```

### Example 3: Rebase --onto for Branch Transplanting
```bash
# Scenario: You branched 'client-v2' off 'experiment', but 'experiment' was rejected.
# You want to transplant 'client-v2' directly onto 'main'.
#
# main:        A --- B
# experiment:         \--- C --- D
# client-v2:                      \--- E --- F

git rebase --onto main experiment client-v2

# Result: Commits E and F are replayed directly onto main (B)!
# main: A --- B --- E' --- F' [client-v2]
```

### Example 4: Automating Verification with `exec`
```bash
# Run test suite after every single replayed commit to ensure no commit breaks the build
git rebase -i origin/main --exec "npm test"

# If npm test fails at commit X, rebase pauses immediately, allowing you to fix it!
```

### Example 5: Resolving Conflicts During Rebase
```bash
# 1. Start rebase
git rebase origin/main

# Output:
# Auto-merging src/server.ts
# CONFLICT (content): Merge conflict in src/server.ts
# error: could not apply a1b2c3d... feat: add caching

# 2. Inspect conflict and resolve in src/server.ts
git status -s
# UU src/server.ts

# 3. Stage the resolved file (DO NOT RUN git commit!)
git add src/server.ts

# 4. Continue rebase
git rebase --continue
```

---

## 7. Best Practices & The Golden Rule

### The Golden Rule of Rebasing
> **Never rebase commits that exist outside your repository and that other people have based their work on.**

Rebasing shared public branches (such as `main` or release branches) changes all downstream commit hashes, causing massive divergence, duplicate commits, and merge conflicts for every collaborator.

### Best Practices Checklist:
1. **Rebase Local Feature Branches Regularly**: Keep feature branches current by rebasing frequently on `origin/main` (`git pull --rebase origin main`). This keeps conflicts small and incremental.
2. **Always Use `--force-with-lease` When Pushing Rebased Branches**: Never use bare `git push -f`. `--force-with-lease` verifies no teammate has pushed new commits to the remote branch before overwriting.
3. **Use `--autostash` for Uncommitted Changes**: Configure `git config --global rebase.autoStash true` so Git automatically stashes and reapplies uncommitted changes before and after rebases.
4. **Squash Micro-Commits Before Opening Pull Requests**: Clean up your PR so reviewers see well-structured, logical units of work rather than stream-of-consciousness commits.
5. **Set `pull.rebase` Globally**: Prevent accidental merge bubbles on daily pulls:
   ```bash
   git config --global pull.rebase true
   ```

---

## 8. Common Mistakes & Conflict Resolution

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Rebasing `main` on a feature branch | Inverts project branch topology | Reversed rebase argument order | Run `git rebase --abort` immediately |
| Running `git commit` during rebase conflict | Leaves rebase in detached HEAD / inconsistent state | Habitual running of `commit` instead of `rebase --continue` | `git add <files>` followed by `git rebase --continue` |
| Force pushing with `-f` | Overwrites teammate's pushed commits on PR branch | Unchecked force push | Always execute `git push --force-with-lease` |
| Panic after messy conflict | Incomplete or corrupt working tree | Trying to manually undo conflict markers without aborting | Execute `git rebase --abort` to return to clean pre-rebase state |
| Dropping commits accidentally | Code vanishes from branch history | Deleting a line in the `rebase -i` todo list | Restore from reflog (`git reset --hard ORIG_HEAD` or `HEAD@{n}`) |
| Repeated conflict on the same code | Resolving same conflict 10 times across 10 commits | Not having `git rerere` enabled | Enable `git config --global rerere.enabled true` |

---

## 9. Performance, Security & Cost

- **Merge Engine Performance**: In Git $\ge 2.33$, the `ort` engine handles rebase operations up to 10x-50x faster than the legacy `recursive` backend by caching directory renames and computing tree diffs in memory without disk touches.
- **Supply Chain & Commit Signing**: Rebasing regenerates commit objects, invalidating any existing GPG/SSH signatures on those commits. Use `git rebase --exec 'git commit -S --amend --no-edit'` or configure `git config --global commit.gpgSign true` to automatically re-sign commits during rebase.
- **CI/CD Efficiency**: Linear histories from rebasing eliminate the multi-parent traversal overhead in Git operations, reducing clone and fetch times across CI build agents.

---

## 10. Comparison: Rebase vs Merge

```
Feature Branch Integration:

Rebase (Linear History):
  main:    A ─── B ─── C ─── D ─── E ─── F' ─── G'  (Clean, single line)

Merge (Preserved History Topology):
  main:    A ─── B ─── C ─── D ─── M (Merge Commit)
                  \               /
  feature:         E ───── F ────┘
```

| Dimension | `git rebase` | `git merge` |
|:---|:---|:---|
| **History Structure** | Strictly linear; single timeline | Non-linear; preserves true chronology & branching topology |
| **Commit Hashes** | Rewrites all replayed commit hashes | Preserves original commit hashes; generates 1 new merge commit |
| **Traceability of Merges** | No explicit merge event (unless squashed) | Explicit merge commit documents when feature was merged |
| **Conflict Resolution** | Resolved per-commit during replay (can trigger multiple times) | Resolved once in the merge commit |
| **Golden Rule Applicability**| **Private local branches only** | Safe on both public and private branches |
| **`git bisect` Friendliness** | Outstanding; simple linear traversal | Complicated; bisect may jump across parallel branches |

---

## 11. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the fundamental difference between `git merge` and `git rebase`?</strong></summary>

`git merge` is a non-destructive operation that creates a new "merge commit" with two parents, tying together two diverging lines of development while preserving the original commit hashes, branching topology, and exact chronological history. `git rebase` is a history-rewriting operation that identifies the common ancestor of both branches, extracts the diffs introduced by the current branch, and replays them sequentially onto the tip of the target branch as completely new commit objects with new SHA hashes, resulting in a strictly linear history.
</details>

<details>
<summary><strong>Q2: Explain the "Golden Rule of Rebasing" and why violating it causes catastrophic issues for a team.</strong></summary>

The Golden Rule states: "Never rebase commits that exist outside your repository and that other people have based work on."
When you rebase public commits, Git creates new commits with identical changes but completely different SHA hashes and parent pointers. If collaborators have based their feature branches on the original commits, their local repository still considers the old commits valid. When they pull or merge, Git sees two diverged histories containing duplicated patches, creating massive merge conflicts, duplicate commit logs, and potentially overwriting legitimate work during force pushes.
</details>

<details>
<summary><strong>Q3: How does the `--autosquash` flag work in interactive rebase?</strong></summary>

When you make a bug fix for an earlier commit, you create a commit using `git commit --fixup <target-commit-sha>`. Git prefixes the commit subject with `fixup! <target-subject>`. When you later run `git rebase -i --autosquash <base>`, Git automatically scans the todo list, detects commits matching `fixup! <subject>` or `squash! <subject>`, moves them directly below their matching target commit in the rebase todo list, and changes their rebase action from `pick` to `fixup` or `squash`.
</details>

<details>
<summary><strong>Q4: Why should developers use `git push --force-with-lease` instead of `git push --force` after rebasing?</strong></summary>

`git push --force` unconditionally updates the remote branch pointer to match the local branch, overwriting any commits on the remote regardless of whether someone else pushed to it in the interim.
`git push --force-with-lease` checks that the remote ref on the server still points to the exact same commit SHA as your local remote-tracking branch (`refs/remotes/origin/<branch>`). If a teammate has pushed a new commit to that branch since your last fetch, the lease check fails and Git rejects the push, preventing accidental destruction of your teammate's work.
</details>

<details>
<summary><strong>Q5: What is `git rebase --onto` and in what real-world scenarios is it required?</strong></summary>

`git rebase --onto <new-base> <old-base> <branch>` cuts a range of commits starting after `<old-base>` up to `<branch>` and replays them onto `<new-base>`.
**Real-World Scenario**: Branch `client-feature` was branched off `backend-feature`. While developing, the team decides `backend-feature` is blocked, but `client-feature` is ready and can work with mock APIs on `main`. Running `git rebase --onto main backend-feature client-feature` extracts only the client commits and grafts them onto `main`, dropping all backend commits without manual cherry-picking.
</details>

<details>
<summary><strong>Q6: If a rebase encounters multiple conflicts across consecutive commits, how can `git rerere` assist?</strong></summary>

`git rerere` stands for "Reuse Recorded Resolution". When enabled (`git config --global rerere.enabled true`), Git records the pre-image and post-image of resolved conflict chunks. If the exact same conflicting code appears in subsequent commits during the rebase replay (or when rebasing again in the future), Git automatically applies the previously recorded resolution without requiring developer intervention.
</details>

<details>
<summary><strong>Q7: What is the purpose of the `exec` command in interactive rebase?</strong></summary>

`exec <command>` (or `x <command>`) executes a shell command after each replayed commit in the rebase todo list. If the command exits with a non-zero exit code (failure), Git halts the rebase immediately at that exact commit. This is heavily used to run automated tests or linters (`--exec "npm test"`) across an entire branch to verify that every single commit in history builds and passes tests cleanly (crucial for maintaining a 100% bisectable history).
</details>

<details>
<summary><strong>Q8: How do you completely abort an interactive rebase that went wrong, and how do you restore your branch if you accidentally completed a bad rebase?</strong></summary>

1. **While rebase is still active**: Run `git rebase --abort`. Git purges the `.git/rebase-merge/` directory and restores `HEAD` and the working tree to the state before the rebase started.
2. **If rebase already finished**: Git stores the pre-rebase branch tip in `ORIG_HEAD`. You can immediately undo the rebase with `git reset --hard ORIG_HEAD`. Alternatively, inspect `git reflog` to identify the commit before `rebase (start)` and run `git reset --hard HEAD@{n}`.
</details>

---

## 12. Practice Exercises

<details>
<summary><strong>Exercise 1: Basic Linear Rebase onto Main</strong></summary>

**Scenario**: Create a diverging main and feature branch, rebase feature onto main, and observe the linear log.
```bash
mkdir rebase-lab && cd rebase-lab && git init
echo "base" > base.txt && git add base.txt && git commit -m "feat: base commit"

git switch -c feature
echo "feat1" > feat.txt && git add feat.txt && git commit -m "feat: feature part 1"

git switch main
echo "main update" > main.txt && git add main.txt && git commit -m "feat: main update"

# Rebase feature onto main
git switch feature
git rebase main

# Verify linear history:
git log --graph --oneline
```
</details>

<details>
<summary><strong>Exercise 2: Interactive Squash of Three Commits</strong></summary>

**Scenario**: Add three small commits on a branch, then squash them into one using `git rebase -i`.
```bash
echo "step 1" >> feat.txt && git commit -am "wip: step 1"
echo "step 2" >> feat.txt && git commit -am "wip: step 2"
echo "step 3" >> feat.txt && git commit -am "wip: step 3"

# Run interactive rebase for last 3 commits
# git rebase -i HEAD~3
# In editor, keep top as 'pick' and change next two to 'squash' (or 'fixup')
```
</details>

<details>
<summary><strong>Exercise 3: Automated Autosquash Workflow</strong></summary>

**Scenario**: Create a fixup commit and verify `--autosquash` collapses it automatically.
```bash
git config rebase.autoSquash true

# Make a change intended for the last commit
echo "polish" >> feat.txt
TARGET_SHA=$(git rev-parse --short HEAD)
git commit -a --fixup $TARGET_SHA

# Inspect log to see fixup! commit
git log -2 --oneline

# Rebase interactively with autosquash
git rebase -i HEAD~2 --autosquash
# Notice Git automatically set fixup action! Save and verify single commit remains.
```
</details>

<details>
<summary><strong>Exercise 4: Test Suite Validation with `exec`</strong></summary>

**Scenario**: Use `--exec` to verify all commits satisfy a mock linter test.
```bash
# Add an intentional failing condition
echo "exit 0" > test.sh && chmod +x test.sh
git rebase -i HEAD~2 --exec "./test.sh"
```
</details>

<details>
<summary><strong>Exercise 5: Branch Transplant with `rebase --onto`</strong></summary>

**Scenario**: Graft a branch off an experimental branch onto main.
```bash
git switch main
git switch -c experimental
echo "exp1" > exp.txt && git add exp.txt && git commit -m "exp: experimental work"

git switch -c sub-feature
echo "sub1" > sub.txt && git add sub.txt && git commit -m "feat: sub feature"

# Transplant sub-feature directly onto main, cutting out experimental
git rebase --onto main experimental sub-feature
git log --graph --oneline
```
</details>

<details>
<summary><strong>Exercise 6: Aborting a Conflict-Ridden Rebase</strong></summary>

**Scenario**: Trigger a merge conflict during rebase and safely abort to restore original state.
```bash
git switch main
echo "conflict A" > conflict.txt && git add conflict.txt && git commit -m "conflict A"

git switch -c conflict-branch
echo "conflict B" > conflict.txt && git add conflict.txt && git commit -m "conflict B"

# Rebase and observe conflict
git rebase main

# Abort cleanly
git rebase --abort
git status
```
</details>

---

## 13. Summary & Cheat Sheet

```bash
# Common Rebase Workflows

# Rebase current branch onto main
git fetch origin
git rebase origin/main

# Pull with rebase and autostash
git pull --rebase --autostash origin main

# Interactive rebase of last N commits
git rebase -i HEAD~N

# Autosquash fixup commits
git commit -a --fixup <TARGET-SHA>
git rebase -i --autosquash <BASE-SHA>

# Conflict Handling
git status                 # Inspect conflicted files
# (Resolve conflict markers in editor)
git add <resolved-files>   # Stage resolution
git rebase --continue      # Proceed with next patch
git rebase --skip          # Discard current patch
git rebase --abort         # Cancel rebase and restore original branch state

# Safe Push After Rebase
git push --force-with-lease
```

---

## 14. Further Reading
- [Official Git Documentation - git-rebase](https://git-scm.com/docs/git-rebase)
- [Pro Git Book - Rebasing](https://git-scm.com/book/en/v2/Git-Branching-Rebasing)
- [Git Merge ORT Engine Architecture](https://git-scm.com/docs/merge-strategies#_ort)
- [03-history-rewriting/03-cherry-pick.md](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/03-cherry-pick.md)
