<!--
================================================================================
Metadata:
  Title: Parallel Workspaces: Git Worktrees (git worktree)
  Track: Git & GitHub / DevOps
  Difficulty: Advanced
  Estimated Time: 50 minutes
  Prerequisites: 16-submodules-and-subtrees.md
  Key Terminology: git worktree, Linked Worktree, Main Worktree, .git/worktrees/, extensions.worktreeConfig, Simultaneous Branch Development
================================================================================
-->

# Parallel Workspaces: Git Worktrees (git worktree)

[<- 16-submodules-and-subtrees.md](./16-submodules-and-subtrees.md) | [Back to Module Overview](./README.md) | [18-sparse-checkout-and-partial-clone.md ->](./18-sparse-checkout-and-partial-clone.md)

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

In traditional Git workflows, you can only check out **one branch at a time** per repository clone. If you are running a long frontend build (`npm run build`) or running a local test server on `feature-auth`, and an urgent production hotfix request arrives:
- You must stop your running dev server.
- You must run `git stash` to hide your uncommitted work.
- You switch to `main`, wait for packages to re-install, fix the bug, commit, and push.
- You switch back to `feature-auth`, run `git stash pop`, rebuild node modules, and restart your server.

This context-switching friction costs hours of developer productivity every week.

Many developers attempt to solve this by creating duplicate clones of the repository in different folders (`my-repo-1`, `my-repo-2`). But duplicate clones waste gigabytes of disk space, duplicate network fetches, and create desynchronized local branches.

**`git worktree`** is the ultimate architectural solution. Introduced in Git 2.5, worktrees allow you to attach **multiple physical working directories to a single Git repository**. You can check out `main` in one folder, `feature-auth` in a second folder, and `pr-review` in a third folder—all sharing the exact same underlying `.git/objects/` database!

---

## 2. Core Concepts

### Main Worktree vs. Linked Worktrees
- **Main Worktree**: The original directory created when you ran `git init` or `git clone`. It owns the central `.git/` directory.
- **Linked Worktree**: An auxiliary directory created via `git worktree add <path> <branch>`. It has its own isolated working tree, its own staging index (`.git/index`), and its own `HEAD`, but shares the object database, remotes, and reflog with the main repository.

```
+-----------------------------------------------------------------------------------+
|                            Git Worktree Architecture                              |
|                                                                                   |
|  MAIN WORKTREE (/projects/app):                                                   |
|  ├── src/ (Files on 'main')                                                       |
|  └── .git/ <───────────────────────────────────────────┐ (Central Object Database)|
|        ├── objects/ (Shared Blobs, Trees, Commits)     │                          |
|        ├── refs/    (Shared Branches & Tags)           │                          |
|        └── worktrees/                                  │                          |
|              ├── hotfix-p0/                            │                          |
|              │     ├── HEAD (ref: refs/heads/hotfix)   │                          |
|              │     └── index (Private Staging Area)    │                          |
|              └── feature-auth/                         │                          |
|                    ├── HEAD (ref: refs/heads/auth)     │                          |
|                    └── index (Private Staging Area)    │                          |
|                                                        │                          |
|  LINKED WORKTREE 1 (/projects/hotfix-p0):              │                          |
|  ├── src/ (Files on 'hotfix')                          │                          |
|  └── .git ──► (Tiny file with gitdir: pointer) ────────┤                          |
|                                                        │                          |
|  LINKED WORKTREE 2 (/projects/feature-auth):           │                          |
|  ├── src/ (Files on 'feature-auth')                    │                          |
|  └── .git ──► (Tiny file with gitdir: pointer) ────────┘                          |
+-----------------------------------------------------------------------------------+
```

### The Cardinal Rule of Worktrees
> [!IMPORTANT]
> **A branch cannot be checked out in more than one worktree at the same time.**
> If `main` is checked out in your main worktree, attempting to run `git worktree add ../hotfix main` will be rejected by Git to prevent multiple working trees from conflicting on the same branch ref and desynchronizing the index.

---

## 3. How It Works Internally

### Directory Redirection via `.git` File
Inside a linked worktree, `.git` is **not a directory**. It is a single-line plain text file:
```text
gitdir: /projects/app/.git/worktrees/hotfix-p0
```
When any Git command runs inside that folder:
1. Git reads the `gitdir:` redirection.
2. It uses the private `HEAD` and `index` inside `.git/worktrees/hotfix-p0/` for staging and active branch tracking.
3. It reads and writes all commit, tree, and blob objects directly into the parent repository's shared `.git/objects/` store.

### Worktree Configuration (`extensions.worktreeConfig`)
By default, all worktrees share the same `.git/config`. If you need worktree-specific configurations (such as different `user.email` or build configurations per worktree):
```bash
git config extensions.worktreeConfig true
```
Git will read `.git/config.worktree` inside the linked worktree's administrative directory.

---

## 4. Syntax & Usage

### Core Worktree Operations

```bash
# Add a linked worktree checking out an existing branch
git worktree add ../my-app-hotfix hotfix-branch

# Add a linked worktree and create a new branch in one step (-b)
git worktree add -b feat-payments ../my-app-payments main

# Add a detached HEAD worktree at a specific tag or commit (ideal for quick PR review)
git worktree add --detach ../my-app-review v1.4.0

# List all active worktrees attached to this repository
git worktree list

# Remove a worktree and clean its administrative metadata cleanly
git worktree remove ../my-app-hotfix

# Force remove a worktree containing uncommitted edits
git worktree remove --force ../my-app-hotfix

# Prune stale worktree metadata (e.g. if you manually ran 'rm -rf' on a worktree folder)
git worktree prune

# Lock a worktree on removable storage to prevent accidental pruning
git worktree lock ../my-app-payments --reason "Long-running test execution"
git worktree unlock ../my-app-payments
```

---

## 5. Practical Examples

### Example 1: Parallel Long-Running Build & Emergency Hotfix
Walk through zero-friction multitasking:

```bash
mkdir /tmp/wt-demo && cd /tmp/wt-demo
git init -b main
echo "v1.0" > app.js && git add app.js && git commit -m "init main"

# 1. You are running a dev server on feature-billing
git switch -c feature-billing
echo "billing in progress" >> app.js
# [Working tree is dirty, dev server is active in terminal 1...]

# 2. Urgent hotfix needed on main! Do NOT stash, do NOT stop dev server!
# Open terminal 2 and create a linked worktree:
git worktree add -b hotfix-urgent ../wt-hotfix main

# 3. Enter the linked worktree folder:
cd ../wt-hotfix
ls -la
# Notice .git is a text file!
cat .git
# Output: gitdir: /tmp/wt-demo/.git/worktrees/wt-hotfix

# 4. Make, commit, and test your hotfix in complete isolation:
echo "security fix" >> app.js
git commit -am "fix(sec): urgent production patch"

# 5. Return to main terminal and continue feature-billing uninterrupted!
cd /tmp/wt-demo
git status -s
# Your uncommitted feature-billing edits are 100% untouched!
```

### Example 2: Inspecting Active Worktrees with `git worktree list`
```bash
git worktree list
```
Output:
```text
/tmp/wt-demo      8a1e2f7 [feature-billing]
/tmp/wt-hotfix    3c9d1e2 [hotfix-urgent]
```
Shows the absolute directory path, current commit hash, and checked-out branch for every workspace!

### Example 3: Pruning Stale Worktrees
If a developer accidentally deleted a linked worktree using the operating system's `rm -rf ../wt-hotfix` instead of `git worktree remove`:

```bash
# Delete directory with OS command
rm -rf ../wt-hotfix

# Notice git worktree list still thinks it exists:
git worktree list
# Output: /tmp/wt-hotfix (prunable)

# Prune administrative metadata:
git worktree prune

git worktree list
# Clean! Stale worktree metadata removed from .git/worktrees/!
```

### Example 4: Testing Pull Requests in Isolated Worktrees
Review a teammate's PR without touching your active coding branch:

```bash
# Fetch teammate's PR branch
git fetch origin pull/42/head:pr-42-review

# Create temporary worktree for the PR
git worktree add ../pr-42 pr-42-review

# Open VS Code directly in the review worktree
code ../pr-42

# Once review is complete, clean up:
git worktree remove ../pr-42
git branch -D pr-42-review
```

---

## 6. Best Practices

1. **Use Worktrees for Long-Running Background Builds**: Keep your main branch running tests in one worktree while developing new features in another.
2. **Organize Worktrees in a Consistent Sibling Folder Structure**: Place linked worktrees alongside the parent project folder:
   ```text
   ~/projects/
     ├── my-app/          (Main worktree on main)
     ├── my-app-auth/     (Linked worktree on feature/auth)
     └── my-app-hotfix/   (Linked worktree on hotfix)
   ```
3. **Always Remove Worktrees via `git worktree remove`**: Avoid deleting worktree directories with raw `rm -rf`. Using `git worktree remove` ensures the directory is clean and clears `.git/worktrees/` automatically.
4. **Never Check Out the Same Branch in Multiple Worktrees**: Git strictly prevents this to preserve index integrity.
5. **Run `git worktree prune` After Manual Deletions**: If directories were moved or deleted by external tools, run prune to clean up orphaned metadata.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `fatal: '...' is already checked out at '...'` | Attempted to check out a branch that is already active in another worktree. | Switch the other worktree to a different branch first, or use a new branch (`-b`). |
| Deleted worktree folder with `rm -rf`, but branch remains locked | Git still retains metadata in `.git/worktrees/`. | Run `git worktree prune`. |
| `fatal: '...' contains modified or untracked files, use --force to delete it` | `git worktree remove` protects uncommitted developer edits. | Commit or stash changes inside the worktree, or run `git worktree remove --force`. |
| Dependencies (e.g. `node_modules/`, `venv/`) missing in new worktree | A worktree is a clean checkout of files in that branch. | Run your dependency install command (`npm install`, `poetry install`) in the new worktree. |
| Cannot lock ref error when creating worktree | Stale worktree lockfile in `.git/worktrees/`. | Run `git worktree unlock <path>` or inspect `.git/worktrees/`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Zero Object Duplication**: A linked worktree consumes only the disk space required for the physical checked-out files and dependency modules (e.g. 50MB). It shares the entire multi-gigabyte `.git/objects/` database with the main worktree.
- **Instant Creation**: Creating a worktree takes **<500 milliseconds**, compared to cloning a new repository which can take several minutes over the network.

### Security
- **Strict Isolation**: Because each worktree maintains its own independent staging index (`.git/index`), changes staged in one worktree are completely invisible to other worktrees, preventing cross-contamination of uncommitted security tokens or test fixtures.

---

## 9. Comparison Table

| Attribute | `git stash` Workflow | Duplicate `git clone` | `git worktree add` |
| :--- | :--- | :--- | :--- |
| **Simultaneous Branches** | No (1 branch at a time) | Yes (independent clones) | Yes (independent folders) |
| **Disk Space Overhead** | Zero | High (duplicates `.git/` database)| Low (shares `.git/objects/`) |
| **Shared Local Branches?**| N/A | No (desynchronized clones) | YES (single unified repo refs) |
| **Dev Server Interruption**| Must stop & restart | No interruption | No interruption |
| **Creation Speed** | Instant | Slow (network download) | Instant (local checkout) |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What is a Git worktree and what problem does it solve?</strong></summary>

A Git worktree (introduced in Git 2.5) allows a developer to attach multiple physical working directories to a single Git repository. It solves the friction of context-switching: instead of stashing changes, terminating local dev servers, and switching branches in a single directory, a developer can have multiple branches checked out simultaneously in separate folders. All linked worktrees share the exact same `.git/objects/` database, saving gigabytes of disk space compared to creating duplicate clones.
</details>

<details>
<summary><strong>2. Why does Git refuse to allow the same branch to be checked out in two different worktrees simultaneously?</strong></summary>

Git's tracking and branch advancing mechanics rely on strict atomicity. If two worktrees were checked out on `main` at the same time, both would read from `.git/refs/heads/main`. If an engineer committed in Worktree A, the `main` ref would advance, but Worktree B's working tree and staging index would immediately become stale and out of sync with its own `HEAD`. To prevent race conditions, merge corruption, and index desynchronization, Git strictly forbids checking out the same branch in more than one worktree.
</details>

<details>
<summary><strong>3. What is the physical structure of a linked worktree on disk?</strong></summary>

In a linked worktree:
- The `.git` entry is a **file**, not a directory, containing a pointer: `gitdir: <path-to-main>/.git/worktrees/<name>`.
- In the main repository's `.git/worktrees/<name>/` folder, Git creates private administrative files:
  - `HEAD`: Tracks the active branch of that worktree.
  - `index`: The private staging area for that worktree.
  - `commondir`: Points back to the shared `.git/` folder holding `objects/` and `refs/`.
</details>

<details>
<summary><strong>4. How do you cleanly delete a Git worktree?</strong></summary>

Using `git worktree remove <path>`. This command verifies that the worktree contains no uncommitted modifications, deletes the physical working directory from disk, and removes the administrative metadata from `.git/worktrees/`. If the directory was deleted using standard OS commands (`rm -rf`), running `git worktree prune` cleans up the orphaned metadata.
</details>

<details>
<summary><strong>5. What is the purpose of `git worktree lock`?</strong></summary>

If a linked worktree resides on portable media, a network share, or a path that is temporarily unmounted, running `git worktree prune` would assume the directory was deleted and discard its administrative metadata. Running `git worktree lock <path> --reason "..."` prevents Git from automatically pruning the worktree metadata until explicitly unlocked.
</details>

<details>
<summary><strong>6. Can you have separate configuration settings (e.g. user email) for different worktrees?</strong></summary>

Yes. By enabling `git config extensions.worktreeConfig true`, Git supports per-worktree configuration files. Settings placed in `.git/config.worktree` inside the linked worktree's metadata directory apply exclusively to that worktree, overriding shared settings in `.git/config`.
</details>

<details>
<summary><strong>7. How does a worktree differ from a clone?</strong></summary>

- A **clone** is an entirely independent repository with its own separate object database, its own remote configurations, and its own local branches. Commits made in one clone do not exist in another clone until pushed and pulled over the network.
- A **worktree** is an auxiliary window into an *existing* repository. All worktrees share the exact same object store and branches instantly without network operations.
</details>

<details>
<summary><strong>8. What happens to dependencies (e.g. `node_modules/` or `.venv/`) when creating a new worktree?</strong></summary>

Because `.gitignore` prevents dependencies from being tracked in Git, a newly created worktree checks out only the version-controlled source files. Dependencies are not automatically copied or shared. The developer must run the project's dependency installer (`npm install`, `pip install`, etc.) in the new worktree directory.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Creating and Inspecting a Linked Worktree</strong></summary>

**Task**: In a test repository, create a linked worktree named `hotfix-workspace` on a new branch `hotfix-101`. Verify the `.git` file contents.

**Solution**:
```bash
mkdir /tmp/wt-exercise && cd /tmp/wt-exercise
git init -b main
echo "Main Code" > app.txt && git add app.txt && git commit -m "init"

# Create linked worktree with new branch
git worktree add -b hotfix-101 ../hotfix-workspace

# Inspect .git file inside linked worktree
cat ../hotfix-workspace/.git
# Displays: gitdir: /tmp/wt-exercise/.git/worktrees/hotfix-workspace
```
</details>

<details>
<summary><strong>Exercise 2: Listing Active Worktrees</strong></summary>

**Task**: Use `git worktree list` to inspect active worktree paths, commits, and branches.

**Solution**:
```bash
git worktree list
# Displays:
# /tmp/wt-exercise        <hash> [main]
# /tmp/hotfix-workspace   <hash> [hotfix-101]
```
</details>

<details>
<summary><strong>Exercise 3: Testing Same-Branch Checkout Rejection</strong></summary>

**Task**: Attempt to check out branch `main` in another worktree to verify Git's built-in conflict protection.

**Solution**:
```bash
git worktree add ../duplicate-main main
# Output: fatal: 'main' is already checked out at '/tmp/wt-exercise'
```
</details>

<details>
<summary><strong>Exercise 4: Making Commits Across Worktrees</strong></summary>

**Task**: Make a commit in `hotfix-workspace`. Return to main worktree and verify that the commit object is immediately visible in the shared repository.

**Solution**:
```bash
cd ../hotfix-workspace
echo "Hotfix patch" >> app.txt
git commit -am "fix: hotfix 101"
HOTFIX_HASH=$(git rev-parse HEAD)

# Return to main repo
cd /tmp/wt-exercise
git log -1 --oneline $HOTFIX_HASH
# The commit is immediately accessible without any fetch or push!
```
</details>

<details>
<summary><strong>Exercise 5: Safe Worktree Removal</strong></summary>

**Task**: Remove `hotfix-workspace` cleanly using `git worktree remove` and verify `git worktree list`.

**Solution**:
```bash
git worktree remove ../hotfix-workspace

git worktree list
# Only the main worktree remains!
ls -d ../hotfix-workspace 2>/dev/null || echo "Folder cleanly removed!"
```
</details>

<details>
<summary><strong>Exercise 6: Manual Deletion and Prune Recovery</strong></summary>

**Task**: Create a worktree, delete its folder with `rm -rf`, observe the prunable status, and clean it with `git worktree prune`.

**Solution**:
```bash
git worktree add ../temp-wt -b temp-branch
rm -rf ../temp-wt

git worktree list
# Shows ../temp-wt as prunable

git worktree prune
git worktree list
# Clean!
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Command | Usage | Role |
| :--- | :--- | :--- |
| `git worktree add <path> <branch>` | Existing Branch | Creates linked worktree checking out existing branch |
| `git worktree add -b <new> <path>` | New Branch | Creates linked worktree and creates new branch |
| `git worktree add --detach <path> <tag>`| Detached HEAD | Creates worktree for temporary PR review |
| `git worktree list` | CLI | Lists all active worktree paths and active branches |
| `git worktree remove <path>` | CLI | Deletes directory and cleans administrative metadata |
| `git worktree remove --force <path>` | Force Clean | Deletes worktree containing uncommitted modifications |
| `git worktree prune` | Maintenance | Clears orphaned metadata for deleted worktree folders |
| `git worktree lock <path>` | Safety | Protects worktree on unmounted/removable disk from prune |
| `git worktree unlock <path>` | Safety | Unlocks worktree allowing standard removal/pruning |

---

## 13. Further Reading & Official Documentation

- [Official Git Documentation - git-worktree](https://git-scm.com/docs/git-worktree)
- [Git Tools - Worktrees (Pro Git Book)](https://git-scm.com/docs/git-worktree#_description)
- [Git Worktree: Parallel Workspaces Without Extra Clones](https://github.blog/2015-07-29-git-2-5-including-multiple-worktrees-and-triangular-workflows/)

---

[<- 16-submodules-and-subtrees.md](./16-submodules-and-subtrees.md) | [Back to Module Overview](./README.md) | [18-sparse-checkout-and-partial-clone.md ->](./18-sparse-checkout-and-partial-clone.md)
