<!--
================================================================================
Metadata:
  Title: Merging Strategies & Mechanics (Fast-Forward, 3-Way & ORT Engine)
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 55 minutes
  Prerequisites: 06-branching.md
  Key Terminology: Fast-Forward (--ff), Non-Fast-Forward (--no-ff), 3-Way Merge, git merge-base, ORT Engine, Squash Merge (--squash), -X ours/-X theirs
================================================================================
-->

# Merging Strategies & Mechanics (Fast-Forward, 3-Way & ORT Engine)

[<- 06-branching.md](./06-branching.md) | [Back to Module Overview](./README.md) | [08-merge-conflicts-and-resolution.md ->](./08-merge-conflicts-and-resolution.md)

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

In collaborative software engineering, branching isolates work, but **merging** integrates it back into the collective codebase. Merging is the mechanism by which two or more independent commit histories in the Directed Acyclic Graph (DAG) are reconciled into a unified state.

Historically, version control systems struggled with merging. Centralized systems frequently miscalculated common ancestors, requiring developers to manually re-apply patches and track revision numbers by hand.

Git revolutionizes this process through **automated 3-way merging** and the next-generation **ORT merge engine** (introduced as the default in Git 2.34):
- It mathematically identifies the exact Lowest Common Ancestor (LCA) between branches using `git merge-base`.
- It distinguishes between **Fast-Forward merges** (which simply slide pointers forward) and **3-Way merges** (which generate explicit merge commits).
- It enables **Squash merges** to keep mainline repository histories clean, readable, and bisectable.

Understanding these mechanics ensures seamless branch integration and eliminates the fear of merge operations.

---

## 2. Core Concepts

### Fast-Forward (`--ff`) vs. True 3-Way Merge (`--no-ff`)
1. **Fast-Forward Merge**:
   - Occurs when the target branch (`main`) has **no new commits** since the feature branch diverged.
   - There is no divergence. To merge, Git does not create a new merge commit; it simply advances the `main` pointer forward to match the tip of the feature branch.
2. **True 3-Way Merge**:
   - Occurs when both `main` and the feature branch have **diverged** (new commits exist on both branches).
   - Git calculates the common ancestor base, merges the changes from both sides, and creates a **Merge Commit** containing two parent hashes.
3. **Forced Non-Fast-Forward (`git merge --no-ff`)**:
   - Forces Git to create a dedicated merge commit even if a fast-forward was technically possible.
   - Crucial in team workflows: it creates a permanent visual record (a "merge bubble") preserving the historical boundary of when a feature was integrated.

```
FAST-FORWARD MERGE:
Before:
main ────► [ Commit A ] ────► [ Commit B ] (feature)
                                  ▲
After git merge feature:          │
main ─────────────────────────────┘ (main pointer simply advances forward!)

TRUE 3-WAY MERGE (Divergent History):
Before:
[ Base Commit A ] ───► [ Commit B (on main) ]
        │
        └───► [ Commit C (on feature) ]

After git merge feature:
[ Base Commit A ] ───► [ Commit B (on main) ] ──────┐
        │                                           │
        └───► [ Commit C (on feature) ] ────► [ Merge Commit M ] (HEAD -> main)
                                                (Parents: B and C)
```

---

## 3. How It Works Internally

### The 3-Way Merge Algorithm Explained
A naive 2-way diff (comparing file $A$ on `main` directly to file $B$ on `feature`) cannot determine intent: if a line exists in $A$ but not in $B$, did $B$ delete it, or did $A$ add it?

To resolve ambiguity, Git performs a **3-Way Merge** using three snapshots:
1. **Base ($B_{base}$)**: The common ancestor commit (found via `git merge-base HEAD feature`).
2. **Ours ($O$)**: The commit at the tip of the current branch (`HEAD`).
3. **Theirs ($T$)**: The commit at the tip of the incoming branch (`feature`).

#### The 3-Way Decision Matrix:
- If a line is modified in $O$ but unchanged in $T$ relative to $B_{base}$ $\to$ Keep $O$.
- If a line is modified in $T$ but unchanged in $O$ relative to $B_{base}$ $\to$ Keep $T$.
- If a line is identically modified in both $O$ and $T$ $\to$ Keep the shared change.
- If a line is modified differently in both $O$ and $T$ $\to$ **CONFLICT!** (Git halts and summons conflict markers).

### The Modern ORT Merge Engine (Git 2.34+)
Historically, Git utilized the `recursive` merge strategy. In Git 2.34, Git introduced **ORT** ("Ostensibly Recursive's Twin"), a complete ground-up rewrite:
- **Massive Speedup**: Up to **500x faster** on complex merges across large monorepos.
- **Conflict Resolution Caching**: Re-uses rename detection results across multi-parent rebases and merges.
- **Correctness**: Eliminates long-standing edge-case bugs in recursive ancestor resolution.

---

## 4. Syntax & Usage

### Core Merging Commands

```bash
# Standard merge (defaults to fast-forward if possible, 3-way if diverged)
git merge feature-payment

# Enforce fast-forward only: fails immediately if a true merge commit is required
git merge --ff-only feature-payment

# Force merge commit: creates explicit merge commit even if fast-forward is possible
git merge --no-ff feature-payment -m "merge: integrate payment microservice"

# Squash Merge: combines all feature commits into a single staged uncommitted change
git merge --squash feature-payment

# Abort an in-progress merge and restore working tree and index to pre-merge state
git merge --abort

# Query the common ancestor commit hash between two branches
git merge-base main feature-payment
```

---

### Merge Strategy Options (`-X`)
When running merges, you can supply strategy options to resolve non-conflicting differences automatically:

```bash
# In conflicting hunks, automatically favor changes from the current branch (Ours)
git merge -X ours feature-payment

# In conflicting hunks, automatically favor changes from the incoming branch (Theirs)
git merge -X theirs feature-payment

# Ignore whitespace differences during merge calculation
git merge -X ignore-all-space feature-payment
```

---

## 5. Practical Examples

### Example 1: Fast-Forward vs. Non-Fast-Forward (`--no-ff`)
Observe the topological difference in `git log`:

```bash
mkdir /tmp/merge-lab && cd /tmp/merge-lab
git init -b main
echo "Base" > app.txt && git add app.txt && git commit -m "feat: initial commit"

# Create feature branch and commit
git switch -c feat-auth
echo "Auth logic" >> app.txt && git commit -am "feat: implement auth"

# Switch back to main
git switch main

# 1. Test Fast-Forward:
# git merge feat-auth -> Simply advances main!

# 2. Test Non-Fast-Forward (--no-ff):
git merge --no-ff feat-auth -m "merge: integrate feat-auth branch"

# Inspect commit graph
git log --graph --oneline -2
```
Output:
```text
*   8a1e2f7 (HEAD -> main) merge: integrate feat-auth branch
|\  
| * 3c9d1e2 (feat-auth) feat: implement auth
|/  
* 4b825dc feat: initial commit
```
*The explicit merge commit provides a clear historical envelope around the feature!*

### Example 2: Squash Merging for Clean Mainline History
When a feature branch contains 20 messy intermediate commits ("wip", "fix typo", "test fix"), squash merge them into a single clean commit on `main`:

```bash
git switch -c feat-search
echo "search 1" > search.txt && git add search.txt && git commit -m "wip 1"
echo "search 2" >> search.txt && git commit -am "wip 2"
echo "search 3" >> search.txt && git commit -am "wip 3"

# Switch to main and squash merge
git switch main
git merge --squash feat-search

# Notice: Changes are staged, but NO commit has been created yet!
git status -s
# Output: A  search.txt

# Create single clean atomic commit
git commit -m "feat(search): implement search indexing engine"

# Verify log
git log --oneline -1
# Output: Only 1 clean commit on main!
```

### Example 3: Finding the Common Ancestor with `git merge-base`
Determine where a branch branched off:

```bash
BASE=$(git merge-base main feat-search)
echo "Branches diverged from commit: $BASE"
git show --oneline $BASE
```

### Example 4: Performing an Octopus Merge
Merge three independent topic branches simultaneously into a release branch:

```bash
git switch -c release-v2
# Merge three branches in one step
git merge feat-login feat-profile feat-billing -m "merge: integrate v2 feature suite"

# Inspect merge commit parents
git cat-file -p HEAD | grep parent
# Output contains 3 distinct parent lines!
```

### Example 5: Aborting a Disastrous Merge Safely
If a merge results in 50 complex conflicts and you need to reset back to a clean state immediately:

```bash
# Abort the merge operation
git merge --abort

# Verify status
git status
# Clean! HEAD and index are completely restored to pre-merge state.
```

---

## 6. Best Practices

1. **Use `--ff-only` When Updating Local `main`**: Configure `git config --global pull.ff only`. This ensures running `git pull` on `main` will never generate an accidental 3-way merge commit if local commits exist.
2. **Use `--no-ff` for Feature Branch Integrations**: When merging PRs into long-lived branches without squashing, use `--no-ff` to preserve the visual grouping of commits.
3. **Adopt Squash Merging for Pull Requests**: In GitHub team settings, enable **Allow squash merging**. This condenses noisy developer iteration commits into a single descriptive commit on `main`.
4. **Keep Branches Synchronized**: Frequently merge or rebase `main` into your feature branch to resolve small conflicts incrementally, avoiding a massive conflict storm at PR merge time.
5. **Always Verify `git merge-base` Before Blaming Teammates**: If unexpected files appear in a merge, check the common ancestor using `git merge-base` to determine when your branch last synced with `main`.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `fatal: Not possible to fast-forward, aborting.` | `--ff-only` was specified, but branches have diverged. | Rebase your local branch onto upstream (`git rebase origin/main`) or run standard merge. |
| Automatic "Merge branch 'main' of github.com" commits appear in history | Ran `git pull` with default merge settings while local commits existed. | Configure `git config --global pull.rebase true` or `pull.ff only`. |
| Merge commit created when developer intended a squash merge | Forgot `--squash` flag during `git merge`. | Reset to pre-merge state (`git reset --hard ORIG_HEAD`) and run `git merge --squash`. |
| `error: Your local changes to the following files would be overwritten by merge` | Uncommitted modifications in working tree conflict with files being merged. | Commit changes, discard with `git restore`, or stash via `git stash`. |
| Conflicting files left in broken state after failed merge | In-progress merge left uncommitted conflict markers. | Cleanly abort the merge: `git merge --abort`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **ORT Engine Acceleration**: The modern ORT engine optimizes rename detection and avoids touching filesystem directories until the entire merge calculation is resolved in memory. A merge that took 45 seconds under legacy `recursive` completes in **under 200 milliseconds** under ORT.
- **Commit Graph Benefits**: Computing `merge-base` across a DAG of 1,000,000 commits uses generation numbers stored in `.git/objects/info/commit-graph`, finding the ancestor in less than 5 milliseconds.

### Security
- **Branch Protection Merge Gates**: GitHub Enterprise rulesets enforce merge gates: requiring pull request approvals, passing CI status checks, and linear commit histories before a merge commit can be published.

---

## 9. Comparison Table

| Merge Type | Command | Creates Merge Commit? | Preserves Commit History? | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| **Fast-Forward** | `git merge --ff` | No (pointer moves) | Yes (linear tape) | Syncing local branch with remote |
| **Non-Fast-Forward** | `git merge --no-ff` | Yes (2 parents) | Yes (merge bubble) | Preserving feature group boundaries |
| **Fast-Forward Only**| `git merge --ff-only`| No (fails if diverged) | Yes | Safe updates of tracking branches |
| **Squash Merge** | `git merge --squash` | No (creates 1 commit) | No (collapses into 1) | PR merges into `main` |
| **Octopus Merge** | `git merge b1 b2 b3` | Yes (3+ parents) | Yes | Merging multiple clean sub-features |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain the difference between a Fast-Forward merge and a 3-Way merge in Git.</strong></summary>

- **Fast-Forward Merge**: Occurs when there is no divergence between branches (the target branch has no commits that do not already exist on the incoming branch). Git simply updates the target branch pointer forward to point to the incoming commit. No new commit object is created.
- **3-Way Merge**: Occurs when both branches have diverged (each branch contains commits that the other lacks). Git calculates the lowest common ancestor (the merge base), compares both branch tips against that ancestor, reconciles differences, and creates an explicit **Merge Commit** with two parent pointers.
</details>

<details>
<summary><strong>2. Why would a development team use `git merge --no-ff` even when a fast-forward merge is possible?</strong></summary>

Enforcing `--no-ff` guarantees that an explicit merge commit is created. This preserves the historical grouping and context of a feature branch in the commit DAG. It visually groups all commits belonging to that feature into a single branch bubble, makes the entire feature easily revertible in a single command (`git revert -m 1 <merge-commit>`), and clearly records who merged the feature and when.
</details>

<details>
<summary><strong>3. How does Git determine the common ancestor during a 3-Way merge?</strong></summary>

Git uses the **Lowest Common Ancestor (LCA)** algorithm on the Directed Acyclic Graph (accessible via `git merge-base`). Git traverses backwards from both branch tips along parent commit pointers until it finds the most recent commit that is an ancestor of both branches. If multiple common ancestors exist (criss-cross merges), Git's merge engine recursively generates a virtual common base commit by merging the ancestors together.
</details>

<details>
<summary><strong>4. What is the modern ORT merge engine in Git and what improvements does it provide?</strong></summary>

Introduced as the default merge engine in Git 2.34, ORT stands for "Ostensibly Recursive's Twin". It is a ground-up rewrite of the legacy `recursive` merge strategy. Key improvements include:
1. Massive performance gains (up to 500x faster on large repositories).
2. Performs all conflict and rename calculations entirely in-memory before touching disk files.
3. Caches rename detection results across multi-commit rebases and cherry-picks.
4. Correctly handles complex criss-cross merge graph resolutions without data corruption.
</details>

<details>
<summary><strong>5. What occurs during a Squash Merge (`git merge --squash`)?</strong></summary>

`git merge --squash <branch>` takes all the changes introduced on the feature branch since it diverged from `HEAD`, combines them into a single diff, and applies that diff onto the current working tree and staging index. It **does NOT create a merge commit** and does not set parent pointers to the feature branch. The developer creates a single, clean commit, keeping the mainline commit history linear and free of messy intermediary commits.
</details>

<details>
<summary><strong>6. What is the difference between `-X ours` and `-s ours` during a merge?</strong></summary>

- `-X ours`: A strategy **option** passed to the default 3-way merge engine. In non-conflicting files, it merges changes normally from both sides. It only favors our version when a specific line-by-line conflict arises.
- `-s ours`: A standalone **merge strategy**. It completely discards all incoming code from the other branch! It forces Git to record a merge commit whose code tree is 100% identical to our current branch, effectively "swallowing" the other branch without integrating any of its changes.
</details>

<details>
<summary><strong>7. How do you cleanly abort a merge that produced unwanted merge conflicts?</strong></summary>

By executing `git merge --abort`. Git inspects `.git/MERGE_HEAD` and `.git/ORIG_HEAD`, discards all conflict markers, restores the index, and resets the working tree back to the exact commit state prior to running `git merge`.
</details>

<details>
<summary><strong>8. What is an Octopus Merge?</strong></summary>

An Octopus Merge is a merge that combines more than two branches simultaneously in a single merge commit (e.g. `git merge feat-a feat-b feat-c`). It is designed for cleanly combining multiple parallel topic branches. An octopus merge automatically aborts if any manual conflict resolution is required; it succeeds only if all branches can be merged cleanly without conflicts.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Observing a Fast-Forward Merge Pointer Shift</strong></summary>

**Task**: Initialize a repo, create a feature branch with a commit, switch to main, and run a fast-forward merge. Verify that no merge commit is created.

**Solution**:
```bash
mkdir /tmp/merge-drill && cd /tmp/merge-drill
git init -b main
echo "v1" > f.txt && git add f.txt && git commit -m "init"

git switch -c feat
echo "v2" >> f.txt && git commit -am "feat commit"
FEAT_HASH=$(git rev-parse HEAD)

git switch main
git merge feat

# Check main's hash: it now matches FEAT_HASH exactly!
MAIN_HASH=$(git rev-parse HEAD)
[ "$MAIN_HASH" = "$FEAT_HASH" ] && echo "Fast-Forward Successful: Hashes match!"
```
</details>

<details>
<summary><strong>Exercise 2: Forcing a Non-Fast-Forward Merge Commit</strong></summary>

**Task**: Create another commit on `feat`. Switch to `main` and execute `git merge --no-ff feat`. Verify that a merge commit with 2 parents is created.

**Solution**:
```bash
git switch feat
echo "v3" >> f.txt && git commit -am "v3 commit"

git switch main
git merge --no-ff feat -m "merge: explicit merge commit"

# Verify 2 parents
git cat-file -p HEAD | grep parent | wc -l
# Output: 2
```
</details>

<details>
<summary><strong>Exercise 3: Executing a Squash Merge</strong></summary>

**Task**: Create a branch `feat-squash` with 3 commits. Squash merge it into `main` and verify that only 1 commit is added to `main`.

**Solution**:
```bash
git switch -c feat-squash
echo "step 1" > s.txt && git add s.txt && git commit -m "step 1"
echo "step 2" >> s.txt && git commit -am "step 2"
echo "step 3" >> s.txt && git commit -am "step 3"

git switch main
git merge --squash feat-squash

# Staged changes exist
git commit -m "feat(squash): combined all 3 steps"

git log --oneline -2
# Only 1 new commit recorded on main!
```
</details>

<details>
<summary><strong>Exercise 4: Querying Common Ancestors with `git merge-base`</strong></summary>

**Task**: Use `git merge-base` to find the exact commit where `feat-squash` diverged from `main`.

**Solution**:
```bash
BASE=$(git merge-base main feat-squash)
echo "Common Ancestor: $BASE"
git log --oneline -1 $BASE
```
</details>

<details>
<summary><strong>Exercise 5: Testing `git merge --abort`</strong></summary>

**Task**: Create a conflict between two branches, trigger the merge, verify conflict state, and abort the merge cleanly.

**Solution**:
```bash
git switch -c branch-left
echo "LEFT CONTENT" > conflict.txt && git add conflict.txt && git commit -m "left"

git switch main && git switch -c branch-right
echo "RIGHT CONTENT" > conflict.txt && git add conflict.txt && git commit -m "right"

# Trigger merge conflict
git merge branch-left
git status -s
# Output: UU conflict.txt

# Abort cleanly
git merge --abort
git status
# Clean!
```
</details>

<details>
<summary><strong>Exercise 6: Configuring Strict Fast-Forward Pulls Globally</strong></summary>

**Task**: Configure your Git environment to reject non-fast-forward pulls by default.

**Solution**:
```bash
git config --global pull.ff only

git config --global --get pull.ff
# Returns: only
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Merge Command | Behavior | When to Use |
| :--- | :--- | :--- |
| `git merge <branch>` | Fast-forward if possible; 3-way if diverged | Default local integration |
| `git merge --ff-only <branch>` | Enforces fast-forward; aborts if diverged | Safe pulling on main branches |
| `git merge --no-ff <branch>` | Forces explicit 2-parent merge commit | Preserving feature group boundaries |
| `git merge --squash <branch>` | Stages changes as single uncommitted snapshot | Clean PR merges into `main` |
| `git merge --abort` | Cancels in-progress merge and restores state | Backing out of difficult conflicts |
| `git merge -X ours <branch>` | Favors current branch in conflicting hunks | Automated conflict resolution |
| `git merge -X theirs <branch>`| Favors incoming branch in conflicting hunks | Overriding local edits with branch |
| `git merge-base A B` | Outputs Lowest Common Ancestor SHA | Diagnosing merge divergence points |

---

## 13. Further Reading & Official Documentation

- [Official Git Documentation - git-merge](https://git-scm.com/docs/git-merge)
- [Official Git Documentation - git-merge-base](https://git-scm.com/docs/git-merge-base)
- [Pro Git Book - Basic Branching and Merging](https://git-scm.com/book/en/v2/Git-Branching-Basic-Branching-and-Merging)
- [GitHub Engineering - Meet the new merge engine: ORT](https://github.blog/2021-08-16-highlights-from-git-2-33/#merge-ort)

---

[<- 06-branching.md](./06-branching.md) | [Back to Module Overview](./README.md) | [08-merge-conflicts-and-resolution.md ->](./08-merge-conflicts-and-resolution.md)
