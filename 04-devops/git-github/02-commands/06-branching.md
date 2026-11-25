<!--
================================================================================
Metadata:
  Title: Branching Architecture & Operations: git branch & git switch
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 05-clean-rm-mv.md
  Key Terminology: git branch, git switch, git checkout, Upstream Tracking, Orphan Branch, Fast Branch Creation, Branch Deletion (-d vs -D)
================================================================================
-->

# Branching Architecture & Operations: git branch & git switch

[<- 05-clean-rm-mv.md](./05-clean-rm-mv.md) | [Back to Module Overview](./README.md) | [07-merging.md ->](./07-merging.md)

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

In centralized version control systems (like SVN or Perforce), branching is an expensive, heavyweight operation. Creating a branch requires making a physical copy of every single file in the project on a central server, consuming gigabytes of disk space and taking several minutes. Consequently, developers in older systems branched infrequently, resulting in massive, high-risk "big-bang" merges.

In Git, **branching is virtually instantaneous and practically free**.
- A branch in Git is simply a movable pointer to a commit in the Directed Acyclic Graph (DAG).
- Physically on disk, a branch is a **41-byte plain text file** in `.git/refs/heads/`.
- Creating, switching, or deleting a branch takes less than 1 millisecond, whether your repository contains 10 files or 5,000,000 files.

This revolutionary architecture enables **frictionless feature branching**, Trunk-Based Development, isolated experimentation, and safe parallel collaboration across distributed teams.

---

## 2. Core Concepts

### Modern Branch Switching: `git switch` vs. Legacy `git checkout`
Prior to Git 2.23 (released August 2019), `git checkout` was the universal command for both branch management and file restoration. This led to dangerous mistakes (e.g., typing `git checkout file.txt` intending to switch to a branch, but instead accidentally destroying all local modifications in that file!).

Git 2.23 resolved this by introducing **`git switch`**:
- **`git switch`**: Dedicated exclusively to branch operations (creating, switching, detaching). It will never overwrite or discard your uncommitted file edits.
- **`git restore`**: Dedicated exclusively to file restoration and unstaging.

```
+-------------------------------------------------------------------------------+
|                        Modern Command Separation (Git 2.23+)                  |
|                                                                               |
|   LEGACY OVERLOADED: git checkout                                             |
|   ├── git checkout -b feature  ──► Creates & switches branch                  |
|   ├── git checkout feature     ──► Switches branch                            |
|   └── git checkout -- file.txt ──► DANGEROUS! Destroys file edits on disk!    |
|                                                                               |
|   MODERN PURPOSE-BUILT:                                                       |
|   ├── git switch -c feature    ──► Dedicated to branches only                 |
|   ├── git switch feature       ──► Dedicated to branches only                 |
|   └── git restore file.txt     ──► Dedicated to file restoration              |
+-------------------------------------------------------------------------------+
```

### Local vs. Remote-Tracking Branches
- **Local Branch (`refs/heads/*`)**: A mutable pointer on your machine that advances forward every time you commit while on that branch.
- **Remote-Tracking Branch (`refs/remotes/origin/*`)**: A local, read-only cache of where the branch tip sits on the remote server. It updates only during network synchronization (`fetch`, `pull`, `push`).

---

## 3. How It Works Internally

### The Life of a Branch Pointer
When you create a branch named `feature`:
```bash
git switch -c feature
```
Git executes three low-level actions:
1. Reads `.git/HEAD` to find the currently checked-out commit hash.
2. Creates the file `.git/refs/heads/feature` and writes that exact 40-character hash into it.
3. Updates `.git/HEAD` symbolically: `ref: refs/heads/feature`.

```
BEFORE COMMIT ON 'feature':
[ Commit A: 1111 ] <──── refs/heads/main
       ▲
       │
[ Commit B: 2222 ] <──── refs/heads/feature <──── HEAD (Active)

DEVELOPER COMMITS:
1. Git writes new Commit C (parent: 2222)
2. Git advances refs/heads/feature to Commit C
3. refs/heads/main remains stationary at Commit A!

AFTER COMMIT:
[ Commit A: 1111 ] <──── refs/heads/main
       ▲
       │
[ Commit B: 2222 ]
       ▲
       │
[ Commit C: 3333 ] <──── refs/heads/feature <──── HEAD (Active)
```

---

## 4. Syntax & Usage

### 1. Branch Inspection Commands
```bash
# List local branches with active branch highlighted
git branch

# List both local AND remote-tracking branches (-a)
git branch -a

# Verbose branch listing showing commit hash, subject, and upstream ahead/behind counts (-vv)
git branch -vv

# List branches sorted by most recent commit date first
git branch --sort=-committerdate

# List branches that have been merged into the current branch
git branch --merged

# List branches containing unmerged commits
git branch --no-merged
```

---

### 2. Modern Branch Switching & Creation (`git switch`)
```bash
# Switch to an existing branch
git switch feature-login

# Create and immediately switch to a new branch (-c / --create)
git switch -c feature-billing

# Create a branch off a specific commit or tag
git switch -c hotfix-102 v1.4.0

# Create an orphan branch (completely empty, disconnected root with no parent history!)
git switch --orphan docs-portal

# Detach HEAD at a specific commit (inspect past code without creating a branch)
git switch --detach 8a1e2f7
```

---

### 3. Branch Renaming & Deletion (`git branch`)
```bash
# Rename current active branch (-m)
git branch -m feature-new-name

# Rename a specific branch from anywhere
git branch -m old-name new-name

# Safe deletion: deletes branch ONLY if fully merged into upstream/HEAD (-d)
git branch -d feature-login

# Force deletion: deletes branch even if it contains unmerged commits (-D)
git branch -D feature-abandoned

# Delete a branch on the remote server (GitHub)
git push origin --delete feature-login
```

---

## 5. Practical Examples

### Example 1: Auditing Upstream Drift with `git branch -vv`
Inspect how far your local branches have drifted ahead or behind the remote server:

```bash
git branch -vv
```
Example Output:
```text
* feature-auth   8a1e2f7 [origin/feature-auth: ahead 2] feat: add jwt validation
  main           4b825dc [origin/main: behind 5] fix: database migration
  hotfix-patch   1c2d3e4 [origin/hotfix-patch] docs: update readme
```
Insights:
- `feature-auth` has 2 commits not yet pushed to GitHub.
- `main` is 5 commits behind GitHub; run `git pull` or `git rebase` to catch up!

### Example 2: Safely Cleaning Up Merged Branches
In active repositories, dozens of merged feature branches accumulate over time:

```bash
# 1. Switch to main and update from remote
git switch main
git pull

# 2. Delete all local branches that have been safely merged into main
# (Protects main and master from deletion)
git branch --merged | grep -vE '^\*|main|master' | xargs -r git branch -d
```

### Example 3: Creating an Orphan Branch (e.g. `gh-pages`)
Orphan branches have no parent commits, creating a completely independent DAG history inside the same repository (ideal for documentation, GitHub Pages, or isolated release assets):

```bash
# Create orphan branch
git switch --orphan gh-pages

# Notice: All files are staged as new additions!
git status -s

# Clear working tree and index
git rm -rf .

# Create fresh documentation content
echo "# Project Documentation" > index.html
git add index.html
git commit -m "docs: initialize orphan documentation tree"

# Verify that this commit has NO parents (Root Commit)
git log --oneline
# Only 1 commit exists in this branch's entire history!
```

### Example 4: Recovering an Accidentally Deleted Branch
You ran `git branch -D feature-important` by mistake:

```bash
# 1. Find the commit hash where the branch tip sat using reflog
git reflog
```
Output:
```text
3b4a5c6 HEAD@{0}: checkout: moving from feature-important to main
8f1e2d4 HEAD@{1}: commit: feat: complete critical microservice
```
Recreate the branch instantly:
```bash
git branch feature-important 8f1e2d4
# Completely recovered with all commits intact!
```

### Example 5: Configuring Upstream Tracking on an Existing Branch
Link an unlinked local branch to a remote branch:

```bash
# Explicitly set upstream tracking
git branch -u origin/feature-payment feature-payment

# Or while on the branch:
git branch --set-upstream-to=origin/feature-payment
```

---

## 6. Best Practices

1. **Use `git switch` Exclusively**: Retire `git checkout` for branch navigation. `git switch` is safer and eliminates the risk of file destruction.
2. **Keep Feature Branches Short-Lived**: Adhere to Trunk-Based Development. Long-lived feature branches (lasting weeks or months) lead to massive, painful merge conflicts. Merge into `main` frequently.
3. **Use Descriptive Namespacing**: Standardize branch names using category prefixes:
   - `feat/user-auth`
   - `fix/null-pointer-exception`
   - `refactor/payment-gateway`
   - `chore/upgrade-node-20`
4. **Never Force Delete Without Checking `git branch --no-merged`**: If `git branch -d` fails, inspect `git log` on that branch before resorting to `git branch -D` to verify you aren't discarding unmerged work.
5. **Sort Branches by Activity**: Configure `git config --global branch.sort -committerdate` so `git branch` displays your most recently active branches at the top.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `error: The branch '...' is not fully merged` | `git branch -d` prevents accidental loss of unmerged commits. | Review commits with `git log <branch> ^HEAD`. If truly abandoned, use `git branch -D <branch>`. |
| `fatal: A branch named '...' already exists` | Attempted to create a branch using a name already in use. | Choose a new name or switch to the existing branch via `git switch <name>`. |
| `fatal: cannot lock ref 'refs/heads/feature/login': 'refs/heads/feature' exists` | Directory/file naming collision in `.git/refs/heads/`. | Delete or rename the conflicting `feature` branch first. |
| Cannot switch branches due to uncommitted changes | Working tree changes conflict with files in the target branch. | Stash changes with `git stash`, switch branches, and `git stash pop`. |
| New local branch fails to push: `fatal: The current branch has no upstream branch` | Local branch has no tracking reference configured on remote. | Run `git push -u origin <branch>` or enable `push.autoSetupRemote = true`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Zero Storage Overhead**: Creating 1,000 branches in Git consumes less than 50 kilobytes of disk space because branches are 41-byte text pointers that reference existing commits.
- **Fast Traversal with Packed Refs**: Packing branches via `git pack-refs` keeps branch resolution times sub-millisecond even on repositories with tens of thousands of active branches.

### Security
- **Branch Protection Rulesets**: Local Git does not restrict branch creation, but GitHub enforces **Branch Rulesets** on shared repositories (e.g. blocking force-pushes, requiring PR reviews, requiring signed commits on `main`).

---

## 9. Comparison Table

| Feature / Aspect | `git switch <branch>` | `git checkout <branch>` | `git switch -c <branch>` |
| :--- | :--- | :--- | :--- |
| **Primary Role** | Switch to existing branch | Switch branch OR restore files | Create & switch to new branch |
| **Risk of File Overwrite**| Zero (halts if dirty conflict) | High (can overwrite files if path matches)| Zero (halts if dirty conflict) |
| **Introduced In** | Git 2.23 (Modern) | Git 1.0 (Legacy) | Git 2.23 (Modern) |
| **Supports Detached HEAD**| Yes (`--detach`) | Yes | No (creates named branch) |
| **Recommended?** | YES (Standard) | Deprecated for branch ops | YES (Standard) |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What is a Git branch at the physical filesystem level?</strong></summary>

At the physical filesystem level, a Git branch is literally a **41-byte plain text file** located in `.git/refs/heads/<branch-name>`. The file contains the 40-character hexadecimal SHA hash of the commit at the tip of that branch, followed by a newline byte (`\n`). Creating a branch simply creates this tiny file, and advancing a branch simply overwrites the 40-character hash.
</details>

<details>
<summary><strong>2. Why did Git introduce `git switch` in version 2.23 to replace `git checkout` for branch navigation?</strong></summary>

Historically, `git checkout` was heavily overloaded with two fundamentally different responsibilities: switching branches/HEAD and restoring/discarding working tree files (`git checkout -- <file>`). This caused severe accidental data loss. For instance, if a developer had a file named `login.js` and a branch named `login.js`, typing `git checkout login.js` could either switch branches or permanently overwrite local uncommitted file modifications depending on subtle syntax variations. Git 2.23 cleanly separated these concerns into `git switch` (for branches) and `git restore` (for files).
</details>

<details>
<summary><strong>3. What is the difference between `git branch -d` and `git branch -D`?</strong></summary>

- `git branch -d` (safe delete): Deletes the branch **only if** its commits have already been fully merged into the currently checked-out branch or its upstream tracking branch. If unmerged commits exist, Git aborts with an error to prevent accidental loss of work.
- `git branch -D` (force delete): Equivalent to `git branch --delete --force`. It deletes the branch pointer immediately, regardless of whether its commits have been merged anywhere.
</details>

<details>
<summary><strong>4. What is an "Orphan Branch" and what are common use cases for it?</strong></summary>

An orphan branch (created via `git switch --orphan <name>`) is a branch that is completely disconnected from the existing commit history. The first commit created on an orphan branch has **zero parent commits**, making it a new root commit in an independent Directed Acyclic Graph (DAG) within the same repository. Common use cases include:
- Hosting documentation or static web assets on GitHub Pages (`gh-pages`).
- Maintaining an empty release distribution or binaries branch.
- Managing independent sub-projects within a monorepo structure.
</details>

<details>
<summary><strong>5. What does the `-vv` flag in `git branch -vv` display?</strong></summary>

The `-vv` (very verbose) flag displays:
1. The 7-character abbreviated commit hash at the tip of each local branch.
2. The remote-tracking upstream branch it is tracking (e.g. `[origin/main]`).
3. The exact synchronization drift relative to the upstream (e.g. `[ahead 2, behind 3]`).
4. The subject line of the latest commit on each branch.
</details>

<details>
<summary><strong>6. How can an engineer recover a branch that was accidentally deleted with `git branch -D`?</strong></summary>

Branch deletion in Git only deletes the 41-byte pointer file in `.git/refs/heads/`. The actual commit objects remain completely unharmed in `.git/objects/`.
1. Run `git reflog` to find the commit hash where the branch tip sat before deletion.
2. Re-create the branch at that exact commit: `git branch <branch-name> <commit-hash>`.
</details>

<details>
<summary><strong>7. Why does Git refuse to create a branch named `feature/login` if a branch named `feature` already exists?</strong></summary>

Because Git stores loose branch references as physical filesystem paths inside `.git/refs/heads/`. If a branch named `feature` exists, `.git/refs/heads/feature` is a regular file. In operating systems (POSIX, Windows, macOS), a path cannot simultaneously be a file and a directory. Creating `feature/login` would require creating a directory named `feature`, which is blocked by the existing file of the same name.
</details>

<details>
<summary><strong>8. What is the difference between a local branch and a remote-tracking branch?</strong></summary>

- A **local branch** (`refs/heads/*`) is an active workspace pointer that advances forward every time a developer commits locally.
- A **remote-tracking branch** (`refs/remotes/origin/*`) is a local, read-only mirror reflecting where that branch pointed on the remote server during the last network operation (`fetch`, `pull`, `push`). Developers cannot check out or commit directly onto remote-tracking branches.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Creating and Switching Branches with `git switch -c`</strong></summary>

**Task**: Initialize a repository, create a commit on `main`, create and switch to a branch named `feature-auth`, make a commit, and verify branch pointers with `cat .git/refs/heads/`.

**Solution**:
```bash
mkdir /tmp/branch-lab && cd /tmp/branch-lab
git init -b main
echo "Main v1" > app.txt && git add app.txt && git commit -m "init"

# Create and switch
git switch -c feature-auth

echo "Auth logic" > auth.txt && git add auth.txt && git commit -m "feat: auth"

# Verify distinct commit hashes on disk
cat .git/refs/heads/main
cat .git/refs/heads/feature-auth
# Different hashes!
```
</details>

<details>
<summary><strong>Exercise 2: Testing Safe Delete Protection (`-d` vs `-D`)</strong></summary>

**Task**: Switch back to `main`. Attempt to delete `feature-auth` using `git branch -d` to observe safe deletion rejection. Then merge the branch and successfully delete it with `-d`.

**Solution**:
```bash
git switch main

# Attempt delete (should fail because feature-auth is not merged!)
git branch -d feature-auth
# Output: error: The branch 'feature-auth' is not fully merged.

# Merge feature-auth
git merge feature-auth

# Now safe delete succeeds!
git branch -d feature-auth
# Output: Deleted branch feature-auth
```
</details>

<details>
<summary><strong>Exercise 3: Creating and Inspecting an Orphan Branch</strong></summary>

**Task**: Create an orphan branch named `docs`, create a single file, and verify that `git log` shows it as a root commit with zero parents.

**Solution**:
```bash
git switch --orphan docs
git rm -rf .

echo "Documentation Home" > index.html
git add index.html && git commit -m "docs: root commit"

git rev-list --parents -n 1 HEAD
# Outputs ONLY the commit hash with NO parent hashes!
```
</details>

<details>
<summary><strong>Exercise 4: Renaming a Branch Locally</strong></summary>

**Task**: Switch to `main` and rename it to `production`. Then rename it back to `main`.

**Solution**:
```bash
git switch main

# Rename active branch
git branch -m production
git branch --show-current
# Output: production

# Rename back
git branch -m main
git branch --show-current
# Output: main
```
</details>

<details>
<summary><strong>Exercise 5: Recovering an Accidentally Deleted Branch via Reflog</strong></summary>

**Task**: Create a branch `temp-experiment`, add a commit, delete it with `-D`, and recover it using `git reflog`.

**Solution**:
```bash
git switch -c temp-experiment
echo "exp" > exp.txt && git add exp.txt && git commit -m "experimental test"
EXP_HASH=$(git rev-parse HEAD)

git switch main
git branch -D temp-experiment

# Recover branch using the recorded hash
git branch temp-experiment $EXP_HASH

git log -1 --oneline temp-experiment
# Recovered!
```
</details>

<details>
<summary><strong>Exercise 6: Sorting Branches by Latest Commit Date</strong></summary>

**Task**: Configure Git to sort branches by commit date in descending order, and verify the output.

**Solution**:
```bash
git config --global branch.sort -committerdate

git branch
# Most recently modified branches appear at the top!
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Modern Command (Git 2.23+) | Legacy Command |
| :--- | :--- | :--- |
| **Switch Branch** | `git switch <branch>` | `git checkout <branch>` |
| **Create & Switch** | `git switch -c <branch>` | `git checkout -b <branch>` |
| **Create from Tag/Commit**| `git switch -c <branch> <target>` | `git checkout -b <branch> <target>` |
| **Create Orphan Branch** | `git switch --orphan <branch>` | `git checkout --orphan <branch>` |
| **Detach HEAD** | `git switch --detach <hash>` | `git checkout <hash>` |
| **List with Tracking** | `git branch -vv` | `git branch -vv` |
| **Sort by Latest Date** | `git branch --sort=-committerdate` | `git branch --sort=-committerdate` |
| **Safe Delete Branch** | `git branch -d <branch>` | `git branch -d <branch>` |
| **Force Delete Branch** | `git branch -D <branch>` | `git branch -D <branch>` |
| **Delete Remote Branch** | `git push origin --delete <branch>` | `git push origin :<branch>` |

---

## 13. Further Reading & Official Documentation

- [Official Git Documentation - git-switch](https://git-scm.com/docs/git-switch)
- [Official Git Documentation - git-branch](https://git-scm.com/docs/git-branch)
- [Pro Git Book - Git Branching Basics](https://git-scm.com/book/en/v2/Git-Branching-Branches-in-a-Nutshell)
- [GitHub Blog - Highlights from Git 2.23 (Introduction of git switch)](https://github.blog/2019-08-16-highlights-from-git-2-23/)

---

[<- 05-clean-rm-mv.md](./05-clean-rm-mv.md) | [Back to Module Overview](./README.md) | [07-merging.md ->](./07-merging.md)
