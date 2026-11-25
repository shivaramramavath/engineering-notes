<!--
================================================================================
Metadata:
  Title: References, Branches & HEAD Internals
  Track: Git & GitHub / DevOps
  Difficulty: Advanced
  Estimated Time: 50 minutes
  Prerequisites: 05-git-objects.md
  Key Terminology: References (refs), Symbolic Ref (symref), HEAD, Pseudo-refs (ORIG_HEAD, MERGE_HEAD), packed-refs, Reftable Backend, git update-ref
================================================================================
-->

# References, Branches & HEAD Internals

[<- 05-git-objects.md](./05-git-objects.md) | [Back to Module Overview](./README.md) | [07-packfiles-and-garbage-collection.md ->](./07-packfiles-and-garbage-collection.md)

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

In Git's object database, every commit is indexed by a 40-character hexadecimal cryptographic hash (e.g., `4b825dc642cb6eb9a060e54bf8d69288fbee4904`). While mathematically perfect for collision resistance and integrity checking, humans cannot comfortably memorize, type, or converse in 40-character hexadecimal strings.

To make the commit Directed Acyclic Graph usable by software engineers, Git introduces **References (Refs)**.

A reference is simply a human-friendly name that points directly to a commit hash.
- What is a branch in Git? A branch is **NOT** a copy of your files, nor is it a container of commits. A branch is literally a **41-byte plain text file** containing a 40-character commit hash followed by a newline character.
- Creating a branch takes less than 1 millisecond and consumes 41 bytes of disk space, regardless of whether your project contains 10 files or 10,000,000 files.

Understanding how Git resolves, updates, and packs references is vital for mastering advanced Git automation, branch management, and repository scaling.

---

## 2. Core Concepts

### Types of References in Git

1. **Local Branches (`refs/heads/*`)**:
   - Reside in `.git/refs/heads/`.
   - Mutable pointers to the tip of active lines of development (e.g. `refs/heads/main`).
   - Automatically advance forward when a new commit is created while on that branch.
2. **Remote-Tracking Branches (`refs/remotes/*`)**:
   - Reside in `.git/refs/remotes/<remote>/`.
   - Read-only local caches reflecting the state of branches on a remote server (e.g. `refs/remotes/origin/main`).
   - You cannot check out or commit directly onto a remote-tracking branch; they are updated only by network operations (`fetch`, `pull`, `push`).
3. **Tags (`refs/tags/*`)**:
   - Reside in `.git/refs/tags/`.
   - Fixed, permanent pointers marking specific releases or milestones (e.g. `refs/tags/v1.0.0`).
   - By convention, tags do not move when new commits are created.
4. **Symbolic References (Symrefs - `HEAD`)**:
   - A reference that points to *another reference* rather than directly to a commit hash.
   - The primary symref is `.git/HEAD`, which points to `refs/heads/<current-branch>`.

```
+---------------------------------------------------------------------------------+
|                               The Reference Topology                            |
|                                                                                 |
|   .git/HEAD                                                                     |
|      │                                                                          |
|      ▼ (Symbolic Ref)                                                           |
|   .git/refs/heads/main ──────┐                                                  |
|      (41-byte text file)     │ (Contains SHA hash)                              |
|                              ▼                                                  |
|                      [ Commit 3: 7a8b9c... ] <──── .git/refs/tags/v1.2.0        |
|                              ▲                     (Immutable Tag Pointer)      |
|                              │ (parent)                                         |
|                      [ Commit 2: d4e5f6... ] <──── .git/refs/remotes/origin/main|
|                              ▲                     (Remote Tracking Cache)      |
|                              │ (parent)                                         |
|                      [ Commit 1: 1a2b3c... ]                                    |
+---------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### The Mechanics of `HEAD` and Branch Advancing
When you run `git commit`, Git performs the following sequence under the hood:
1. Writes the new commit object $C_{new}$ to `.git/objects/`, setting its parent to the hash currently pointed to by `HEAD`.
2. Inspects `.git/HEAD`.
3. If `HEAD` is a symbolic ref (`ref: refs/heads/main`), Git updates the file `.git/refs/heads/main` to contain the hash of $C_{new}$.
4. If `HEAD` is detached (contains a raw hash), Git updates `.git/HEAD` directly with the new hash, leaving all branch pointers unchanged.

```
BEFORE COMMIT:
HEAD -> refs/heads/main -> [Commit 2: d4e5f6]

AFTER COMMIT:
1. Git writes Commit 3 (parent: d4e5f6)
2. Git writes new hash (7a8b9c) into .git/refs/heads/main
3. HEAD still points to refs/heads/main, which now points to Commit 3!
```

### Ephemeral Pseudo-Refs
Git creates transient reference files in `.git/` to remember state across complex operations:
- **`ORIG_HEAD`**: Records the position of `HEAD` before a dangerous operation (such as `merge`, `rebase`, or `reset`). Allows instant recovery via `git reset --hard ORIG_HEAD`.
- **`FETCH_HEAD`**: Records the branches and commit hashes fetched from the remote during the last `git fetch`.
- **`MERGE_HEAD`**: Exists only during an active merge conflict; records the commit hash being merged into `HEAD`.
- **`CHERRY_PICK_HEAD`**: Exists during a cherry-pick conflict.

### Packed Refs and the Reftable Backend
In large repositories with tens of thousands of branches and tags, having thousands of individual loose text files in `.git/refs/` creates filesystem inode congestion.
1. **Packed Refs (`.git/packed-refs`)**:
   - Git consolidates loose reference files into a single sorted text file via `git pack-refs`.
   - When reading a ref, Git checks loose files first; if absent, it scans `.git/packed-refs`.
2. **Reftable Backend (Git 2.45+)**:
   - Modern binary format storing refs in block-based lookup tables, providing $O(\log N)$ lookups and atomic transactions across massive monorepos.

---

## 4. Syntax & Usage

### Low-Level Plumbing Commands for Reference Manipulation

```bash
# Resolve any reference, branch, or tag to its exact 40-character commit hash
git rev-parse HEAD
git rev-parse main
git rev-parse v1.0.0

# Inspect or update a symbolic reference (HEAD)
git symbolic-ref HEAD

# Atomically update a reference to a new commit hash
git update-ref refs/heads/main <new-commit-hash>

# Delete a reference cleanly
git update-ref -d refs/heads/feature-branch

# List all references in the repository with their target hashes
git show-ref

# Consolidate loose refs into .git/packed-refs
git pack-refs --all --prune
```

---

## 5. Practical Examples

### Example 1: Creating a Branch Without Using `git branch`
Prove that a branch is literally just a reference file by creating one with raw plumbing:

```bash
mkdir /tmp/refs-lab && cd /tmp/refs-lab
git init -b main
echo "Initial code" > app.txt
git add app.txt && git commit -m "feat: initial commit"

HASH=$(git rev-parse HEAD)

# Create a new branch named 'feature-plumbing' using git update-ref
git update-ref refs/heads/feature-plumbing $HASH

# Verify that standard Git recognizes it as a full-fledged branch
git branch
# Output:
# * main
#   feature-plumbing

# Verify the file on disk
cat .git/refs/heads/feature-plumbing
# Exactly matches $HASH!
```

### Example 2: Inspecting and Healing `HEAD` with `git symbolic-ref`
Diagnose and repair a corrupted or disconnected `HEAD` pointer:

```bash
# Inspect current symbolic target
git symbolic-ref HEAD
# Output: refs/heads/main

# Switch HEAD symbolically to feature-plumbing without checking out files
git symbolic-ref HEAD refs/heads/feature-plumbing

# Verify active branch
git branch --show-current
# Output: feature-plumbing
```

### Example 3: Reverting an Accidental Hard Reset via `ORIG_HEAD`
Understand how pseudo-refs provide safety nets during destructive operations:

```bash
echo "Commit 2" >> app.txt && git commit -am "feat: commit 2"
echo "Commit 3" >> app.txt && git commit -am "feat: commit 3"

# Catastrophe: Developer runs hard reset by mistake!
git reset --hard HEAD~2
git log --oneline
# Only Commit 1 is visible!

# Check ORIG_HEAD
cat .git/ORIG_HEAD
# Contains the hash of Commit 3!

# Instantly restore repository state to Commit 3
git reset --hard ORIG_HEAD
git log --oneline -3
# All 3 commits are fully restored!
```

### Example 4: Compacting Thousands of Loose Refs with `git pack-refs`
Simulate reference consolidation in production:

```bash
# Create 50 simulated branches
for i in $(seq 1 50); do
  git update-ref refs/heads/branch-$i HEAD
done

# Notice 50 loose files in .git/refs/heads/
ls .git/refs/heads/ | wc -l
# Output: 52

# Pack all loose refs into a single file
git pack-refs --all --prune

# Notice loose files have been cleanly deleted!
ls .git/refs/heads/ | wc -l
# Output: 0

# Inspect .git/packed-refs
head -n 10 .git/packed-refs
# Formatted as: <hash> <ref-path>
```

### Example 5: Initializing a Repository with the Modern Reftable Backend (Git 2.45+)
Take advantage of next-generation reference performance:

```bash
# Initialize with reftable reference storage
git init --ref-format=reftable /tmp/reftable-repo
cd /tmp/reftable-repo

# Notice .git/refs/ is empty; .git/reftable/ holds the binary tables!
ls -l .git/reftable/
# tables.list, 0x000000000001-0x000000000001-...ref
```

---

## 6. Best Practices

1. **Delete Merged Remote Branches Promptly**: Stale tracking refs clutter `git branch -a`. Configure `fetch.prune = true` to clean them automatically.
2. **Use `ORIG_HEAD` as Your Immediate Emergency Brake**: If a `merge`, `rebase`, or `reset --hard` goes wrong, reach for `git reset --hard ORIG_HEAD` before running complex reflog commands.
3. **Never Directly Edit Files in `.git/refs/`**: Always use `git update-ref` or Porcelain commands (`git branch`, `git tag`). Directly editing ref files bypasses locking mechanisms and can corrupt packed references.
4. **Pack References on High-Activity Repositories**: Periodically run `git pack-refs --all --prune` (or let `git gc` do it) to maintain optimal reference lookup performance.
5. **Use Branch Namespacing for Large Teams**: Organize branches hierarchically with slashes (e.g., `feature/login`, `bugfix/issue-102`, `release/v2.1`).

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `fatal: Cannot lock ref 'refs/heads/...': is a directory` | Naming collision: attempting to create branch `feature` when `feature/login` already exists as a directory. | Delete the parent or rename the child branch (Git cannot store a file and folder with the same name). |
| Commits created disappear after switching branches | You committed while in a **Detached HEAD** state. | Run `git reflog`, identify the commit hash, and create a branch pointing to it: `git branch recovered-work <hash>`. |
| `fatal: ref HEAD is not a symbolic ref` | Attempted to run `git symbolic-ref HEAD` while in a detached HEAD state. | Check out a branch (`git switch main`) to restore `HEAD` as a symbolic ref. |
| Remote tracking branch still appears locally after remote deletion | `fetch.prune` is disabled. | Run `git fetch --prune` or delete manually: `git branch -dr origin/<branch>`. |
| Accidental `git reset --hard` wiped recent work | `HEAD` was moved backwards. | Recover immediately via `git reset --hard ORIG_HEAD` or locate hash via `git reflog`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Loose Refs vs Packed Refs**: Scanning 100,000 loose ref files requires 100,000 `open()`/`read()` system calls. Packing refs into `.git/packed-refs` reduces this to a single sequential file read using binary search.
- **Reftable Scalability**: On ultra-large monorepos (e.g. at Google/Meta scale with millions of refs), Git's **reftable** format provides constant-time reading and atomic multithreaded ref updates without global lockfiles.

### Security
- **Branch Hijacking Prevention**: Ref updates in Git are atomic. When `git update-ref` runs, it writes to a lockfile (`refs/heads/main.lock`) first and verifies that the old value matches expected history before renaming, preventing race conditions or concurrent overwrite attacks.

---

## 9. Comparison Table

| Reference Type | Storage Path | Mutability | Auto-Advances on Commit? |
| :--- | :--- | :--- | :--- |
| **Local Branch** | `.git/refs/heads/<name>` | Mutable | Yes (when checked out) |
| **Remote Branch** | `.git/refs/remotes/<remote>/<name>`| Read-only (updated by fetch)| No (fixed until next fetch) |
| **Tag** | `.git/refs/tags/<name>` | Immutable by convention | No (never advances) |
| **Symbolic Ref (HEAD)**| `.git/HEAD` | Points to a ref | Yes (follows active branch) |
| **Pseudo-Ref (`ORIG_HEAD`)**| `.git/ORIG_HEAD` | Overwritten by operations | No (holds single snapshot) |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What is a Git reference (ref) and what is a branch physically on the filesystem?</strong></summary>

A Git reference is a human-readable nickname pointing to a 40-character (or 64-character) object hash. Physically on the filesystem, a local branch is simply a 41-byte plain text file located inside `.git/refs/heads/<branch-name>`. The file contains the 40-character hexadecimal SHA hash of the latest commit on that branch, followed by a newline byte (`\n`). Creating or deleting a branch involves only creating or deleting this tiny text file.
</details>

<details>
<summary><strong>2. What is a symbolic reference (symref) and how does it relate to `.git/HEAD`?</strong></summary>

A symbolic reference is a special reference that does not point directly to an object hash, but instead points to another reference. The most important symref is `.git/HEAD`, which typically contains `ref: refs/heads/<branch>`. It tells Git which branch is currently checked out in the working directory so that when a new commit is created, Git knows which branch reference in `refs/heads/` to update.
</details>

<details>
<summary><strong>3. What is a "Detached HEAD" state and what are its dangers?</strong></summary>

A Detached HEAD state occurs when `.git/HEAD` points directly to an exact commit hash rather than a symbolic branch reference (e.g., after `git checkout <hash>` or `git checkout <tag>`). While in this state, an engineer can still write code and commit. However, because no branch pointer references these new commits, switching away to another branch (`git checkout main`) leaves those commits "dangling" (unreferenced). Eventually, Git's garbage collection (`git gc`) will permanently delete them unless rescued into a named branch.
</details>

<details>
<summary><strong>4. What is `ORIG_HEAD` and how does it assist in disaster recovery?</strong></summary>

`ORIG_HEAD` is a pseudo-ref created automatically by Git prior to executing potentially destructive operations that move `HEAD` significantly (such as `git merge`, `git rebase`, `git reset`, or `git pull`). Git writes the commit hash that `HEAD` pointed to *before* the operation into `.git/ORIG_HEAD`. If the operation causes unwanted changes or breakages, the developer can undo it in one command: `git reset --hard ORIG_HEAD`.
</details>

<details>
<summary><strong>5. Why does Git refuse to create a branch named `feature/login` if a branch named `feature` already exists?</strong></summary>

Because Git stores loose references directly on the host operating system's filesystem inside `.git/refs/heads/`. If a branch named `feature` exists, there is a physical file named `.git/refs/heads/feature`. In standard filesystems (POSIX, NTFS, ext4), a path cannot simultaneously be a regular file and a directory. Creating `feature/login` requires a directory named `feature`, which is blocked by the existing file of the same name.
</details>

<details>
<summary><strong>6. What is `.git/packed-refs` and why does Git pack references?</strong></summary>

When a repository accumulates thousands of branches and tags, storing each as an individual loose text file creates filesystem performance bottlenecks (inode exhaustion, slow directory traversals, and many small file reads). Git consolidates loose references into a single sorted text file named `.git/packed-refs` during `git pack-refs` or `git gc`. Git can search this single file rapidly using binary search algorithms.
</details>

<details>
<summary><strong>7. How does the modern Reftable backend (Git 2.45+) improve upon `packed-refs`?</strong></summary>

While `packed-refs` improves upon loose files, updating a single packed reference still requires rewriting the entire `packed-refs` file and taking an exclusive file lock, creating write contention on high-concurrency servers. The **Reftable** storage backend replaces text files with a series of immutable binary block tables with index records, supporting $O(\log N)$ lookups, concurrent atomic transactions, and logarithmic compaction without global lockfiles.
</details>

<details>
<summary><strong>8. What plumbing command is used to resolve a human-friendly name or relative reference to its exact commit SHA?</strong></summary>

`git rev-parse`. For example:
- `git rev-parse HEAD` returns the current commit hash.
- `git rev-parse HEAD~1` returns the parent commit hash.
- `git rev-parse --symbolic-full-name @{u}` returns the full upstream tracking ref name.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Inspecting the Plain Text Content of a Branch Ref</strong></summary>

**Task**: Initialize a repository, make a commit, and use standard filesystem tools (`cat`, `wc`) to inspect `.git/refs/heads/main` to confirm its size and contents.

**Solution**:
```bash
mkdir /tmp/ref-anatomy-lab && cd /tmp/ref-anatomy-lab
git init -b main
echo "Ref Anatomy" > file.txt && git add file.txt && git commit -m "feat: initial"

# Inspect branch file
cat .git/refs/heads/main
# Displays 40-character SHA hash

# Check byte size (41 bytes: 40 hex chars + 1 newline)
wc -c < .git/refs/heads/main
# Output: 41
```
</details>

<details>
<summary><strong>Exercise 2: Resolving Revision Selectors with `git rev-parse`</strong></summary>

**Task**: Make 3 commits. Use `git rev-parse` to find the exact hash of the current commit, the parent commit, and the grandparent commit.

**Solution**:
```bash
echo "Commit 2" >> file.txt && git commit -am "Commit 2"
echo "Commit 3" >> file.txt && git commit -am "Commit 3"

# Current commit
git rev-parse HEAD

# Parent commit (HEAD~1 or HEAD^)
git rev-parse HEAD~1

# Grandparent commit (HEAD~2 or HEAD^^)
git rev-parse HEAD~2
```
</details>

<details>
<summary><strong>Exercise 3: Creating and Switching Branches via `git update-ref` and `git symbolic-ref`</strong></summary>

**Task**: Without using `git branch` or `git checkout`, create a branch named `plumbing-branch` and switch `HEAD` to it.

**Solution**:
```bash
# 1. Create the branch ref
git update-ref refs/heads/plumbing-branch HEAD

# 2. Update symbolic-ref HEAD
git symbolic-ref HEAD refs/heads/plumbing-branch

# 3. Verify
git branch --show-current
# Output: plumbing-branch
```
</details>

<details>
<summary><strong>Exercise 4: Simulating and Resolving Ref Locking Conflicts</strong></summary>

**Task**: Create a branch named `release`. Attempt to create `release/v1.0` and observe the directory/file naming conflict error.

**Solution**:
```bash
git branch release

# Attempt to create sub-branch
git branch release/v1.0
# Output: fatal: cannot lock ref 'refs/heads/release/v1.0': 'refs/heads/release' exists; cannot create 'refs/heads/release/v1.0'

# Fix by deleting or renaming the conflicting branch:
git branch -m release release-base
git branch release/v1.0
# Now succeeds!
```
</details>

<details>
<summary><strong>Exercise 5: Emergency Rollback via `ORIG_HEAD`</strong></summary>

**Task**: Perform a hard reset to `HEAD~1`, observe the updated `ORIG_HEAD`, and undo the reset.

**Solution**:
```bash
PREV_HASH=$(git rev-parse HEAD)
git reset --hard HEAD~1

# Inspect ORIG_HEAD
cat .git/ORIG_HEAD
# Matches PREV_HASH!

# Undo hard reset
git reset --hard ORIG_HEAD
```
</details>

<details>
<summary><strong>Exercise 6: Packing and Inspecting Refs</strong></summary>

**Task**: Pack all loose refs in your test repository and examine `.git/packed-refs`.

**Solution**:
```bash
git pack-refs --all --prune
cat .git/packed-refs
# Notice header: # pack-refs with: sorted-keys
# Followed by list of commit hashes and ref names
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Reference Concept | Path / File | Role |
| :--- | :--- | :--- |
| **Active Target** | `.git/HEAD` | Points to currently checked-out branch ref |
| **Local Branch** | `.git/refs/heads/<name>` | 41-byte pointer to commit; advances on commit |
| **Remote Tracking** | `.git/refs/remotes/<remote>/<name>` | Local read-only mirror of remote branch tip |
| **Tag** | `.git/refs/tags/<name>` | Permanent, fixed milestone pointer |
| **Undo Safety Net** | `.git/ORIG_HEAD` | Stores previous HEAD position before dangerous ops |
| **Packed Store** | `.git/packed-refs` | Consolidated single-file storage of loose refs |

| Command | Purpose |
| :--- | :--- |
| `git rev-parse <ref>` | Translates any ref/expression to full 40-character hash |
| `git symbolic-ref HEAD` | Displays or updates symbolic target of HEAD |
| `git update-ref <ref> <hash>` | Atomically creates or updates a reference pointer |
| `git update-ref -d <ref>` | Deletes a reference cleanly |
| `git pack-refs --all --prune` | Consolidates loose refs into `.git/packed-refs` |
| `git show-ref` | Lists all references and hashes in repository |

---

## 13. Further Reading & Official Documentation

- [Git Internals - Git References (Pro Git Book)](https://git-scm.com/book/en/v2/Git-Internals-Git-References)
- [Official Git Documentation - git-rev-parse](https://git-scm.com/docs/git-rev-parse)
- [Official Git Documentation - git-update-ref](https://git-scm.com/docs/git-update-ref)
- [The Reftable Format Specification](https://git-scm.com/docs/reftable)

---

[<- 05-git-objects.md](./05-git-objects.md) | [Back to Module Overview](./README.md) | [07-packfiles-and-garbage-collection.md ->](./07-packfiles-and-garbage-collection.md)
