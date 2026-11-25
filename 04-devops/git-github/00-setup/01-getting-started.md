<!--
================================================================================
Metadata:
  Title: Git Getting Started & Verification
  Track: Git & GitHub / DevOps
  Difficulty: Beginner
  Estimated Time: 45 minutes
  Prerequisites: None
  Key Terminology: Distributed Version Control System (DVCS), Working Directory, Git Repository, System Architecture, Initial Configuration
================================================================================
-->

# Git Getting Started & Verification

[Back to Track Overview](../README.md) | [Back to Module Overview](./README.md) | [02-install-and-configuration.md ->](./02-install-and-configuration.md)

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

In software engineering, version control is the foundational mechanism that records modifications to a file or collection of files over time, enabling engineers to recall specific versions later, compare changes, identify regressions, and collaborate seamlessly across distributed teams without overwriting one another's work.

Before Git, centralized version control systems (CVCS) such as Subversion (SVN) and CVS stored all version history on a central server. If the server suffered an outage or network connectivity was lost, developers could not commit changes, view historical diffs, or branch effectively.

Created in 2005 by Linus Torvalds to manage Linux kernel development, **Git** is a **Distributed Version Control System (DVCS)**. In Git, clients do not merely check out the latest snapshot of the files; rather, every local clone is a full-fledged repository containing the complete revision history of the project.

### Why Git Dominates Modern Engineering:
1. **Speed & Local Execution**: Nearly all operations (commits, diffs, log queries, branch creation) execute entirely offline against local disk in milliseconds without network roundtrips.
2. **Cryptographic Integrity**: Every object, commit, and file revision is indexed and verified by a cryptographic hash, making silent data corruption or tampering mathematically detectable.
3. **Cheap, High-Speed Branching**: Branches in Git are lightweight pointers (41-byte text files), allowing frictionless feature branching, experimentation, and merging.

---

## 2. Core Concepts

### Centralized vs. Distributed Version Control
- **Centralized (CVCS)**: Single repository server holds all historical commits. Clients check out working copies. A database corruption on the central server risks total project loss without off-site backups.
- **Distributed (DVCS)**: Every developer workstation contains a clone with the complete object database and historical commit graph. If the remote server (e.g. GitHub) crashes, any developer repository can restore the master copy.

### The Three Local Tree States
Every local Git workspace contains three distinct zones:
1. **Working Tree (Working Directory)**: The physical files on your local filesystem that you currently view and edit.
2. **Staging Area (Index)**: A binary file (`.git/index`) that prepares and stages the snapshot destined for the next commit.
3. **Git Directory (Repository / Object Store)**: The `.git/` folder containing the immutable object database (blobs, trees, commits, tags) and reference pointers (`refs/heads/*`).

### Identity & Authorship
Git requires author identification for every commit:
- `user.name`: Human-readable name attached to commit metadata.
- `user.email`: Email address associated with commits (used by GitHub to map contributions to user accounts).

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                               LOCAL REPOSITORY ARCHITECTURE                       |
|                                                                                   |
|  +---------------------+      git add      +-----------------------------------+  |
|  |  WORKING DIRECTORY  | ----------------> |       STAGING AREA (INDEX)        |  |
|  |  Files on disk      |                   |   .git/index (Binary cache)       |  |
|  |  (Untracked/Modified|                   |   Prepares upcoming commit        |  |
|  +---------------------+                   +-----------------+-----------------+  |
|            ^                                                 |                    |
|            |                                                 | git commit         |
|            | git restore / checkout                          v                    |
|            |                               +-----------------------------------+  |
|            +------------------------------ |     GIT REPOSITORY (.git/)        |  |
|                                            |  - Object Database (.git/objects) |  |
|                                            |    (Blobs, Trees, Commits)        |  |
|                                            |  - Reference Pointers (.git/refs) |  |
|                                            |  - HEAD pointer (.git/HEAD)       |  |
|                                            +-----------------------------------+  |
+-----------------------------------------------------------------------------------+
```

When you execute `git init`:
1. Git creates a hidden directory named `.git/`.
2. It initializes empty object storage (`.git/objects/`), reference directories (`.git/refs/heads/`, `.git/refs/tags/`), and configuration file (`.git/config`).
3. It sets `HEAD` to point to the default branch (e.g., `ref: refs/heads/main`).

---

## 4. Syntax & Usage

### Verification and System Inspection Commands

```bash
# Verify installed Git version
git --version

# Display system execution path and binary location
which git          # Linux / macOS
where.exe git      # Windows PowerShell / CMD

# Inspect comprehensive Git environment and active configuration
git config --list --show-origin

# Output all compiled-in defaults and runtime variables
git var -l
```

### Essential Identity Configuration

```bash
# Configure global author identity
git config --global user.name "Alex SRE"
git config --global user.email "alex@example.com"

# Configure default branch name for all new repositories
git config --global init.defaultBranch main

# Set default text editor (VS Code, Nano, or Vim)
git config --global core.editor "code --wait"
```

---

## 5. Practical Examples

### Example 1: Verifying Version and Tool Capabilities

```bash
# Check version
git --version
```
*Output*:
```
git version 2.49.0.windows.1
```

```bash
# Verify supported object formats and core features
git var GIT_DEFAULT_BRANCH
```
*Output*:
```
main
```

### Example 2: Initializing a Local Repository and Inspecting `.git`

```bash
# 1. Create a scratch directory
mkdir git-verification-lab && cd git-verification-lab

# 2. Initialize fresh repository
git init
```
*Output*:
```
Initialized empty Git repository in E:/Learning/engineering-notes/git-verification-lab/.git/
```

```bash
# 3. Inspect the newly initialized internal structure
ls -la .git
```
*Output*:
```
total 7
drwxr-xr-x 1 user 197121   0 Nov 25 10:30 .
drwxr-xr-x 1 user 197121   0 Nov 25 10:30 ..
-rw-r--r-- 1 user 197121  23 Nov 25 10:30 HEAD
-rw-r--r-- 1 user 197121 130 Nov 25 10:30 config
-rw-r--r-- 1 user 197121  73 Nov 25 10:30 description
drwxr-xr-x 1 user 197121   0 Nov 25 10:30 hooks
drwxr-xr-x 1 user 197121   0 Nov 25 10:30 info
drwxr-xr-x 1 user 197121   0 Nov 25 10:30 objects
drwxr-xr-x 1 user 197121   0 Nov 25 10:30 refs
```

### Example 3: Inspecting Initial Configuration and Scopes

```bash
# Inspect all configuration values alongside the file they originated from
git config --list --show-origin | head -n 8
```
*Output*:
```
file:C:/Program Files/Git/etc/gitconfig  core.autocrlf=true
file:C:/Program Files/Git/etc/gitconfig  core.fscache=true
file:C:/Users/user/.gitconfig           user.name=Alex SRE
file:C:/Users/user/.gitconfig           user.email=alex@example.com
file:C:/Users/user/.gitconfig           init.defaultbranch=main
file:.git/config                        core.repositoryformatversion=0
file:.git/config                        core.filemode=false
file:.git/config                        core.bare=false
```

### Example 4: Creating First Tracked File and Inspecting Object Creation

```bash
# 1. Create a new markdown file
echo "# Welcome to Git" > README.md

# 2. Check status
git status
```
*Output*:
```
On branch main

No commits yet

Untracked files:
  (use "git add <file>..." to include in what will be committed)
	README.md

nothing added to commit but untracked files present (use "git add" to track)
```

```bash
# 3. Stage the file
git add README.md

# 4. Check status again
git status
```
*Output*:
```
On branch main

No commits yet

Changes to be committed:
  (use "git rm --cached <file>..." to unstage)
	new file:   README.md
```

### Example 5: Clean Teardown of Verification Lab

```bash
cd ..
rm -rf git-verification-lab
```

---

## 6. Best Practices

1. **Always Set `init.defaultBranch` to `main`**: Setting `git config --global init.defaultBranch main` ensures compliance with modern open-source ecosystems.
2. **Verify Author Email Prior to Committing**: Committing with a personal email on a corporate repository (or vice-versa) leaks private data and breaks enterprise audit trails.
3. **Use `--show-origin` to Troubleshoot Config Clashes**: If Git behavior is unexpected, running `git config --list --show-origin` immediately identifies which configuration file (system, global, or local) is applying the conflicting rule.
4. **Never Commit into Uninitialized Repositories**: Verify `.git` exists before staging files to avoid writing commits to unintended parent directories.

---

## 7. Common Mistakes & Troubleshooting

| Mistake / Symptom | Root Cause | Diagnosis Command | Remediation |
|:------------------|:-----------|:------------------|:------------|
| **`fatal: not a git repository`** | Current working directory is not inside a repository with a `.git/` folder | `git rev-parse --is-inside-work-tree` | Navigate to repo root or execute `git init`. |
| **Commit author shows `user@DESKTOP-XYZ`** | `user.name` and `user.email` were never configured globally | `git config user.email` returns empty | Run `git config --global user.name "..."` and `user.email "..."`. |
| **New repo defaults to branch `master`** | Global `init.defaultBranch` is unset, falling back to legacy default | `git config init.defaultBranch` | Run `git config --global init.defaultBranch main`. |
| **`git: command not found`** | Git binary is missing from operating system `$PATH` | `echo $PATH` | Add Git installation folder (`/usr/bin/git` or `C:\Program Files\Git\cmd`) to system PATH. |
| **Permission denied creating `.git`** | Insufficient filesystem privileges in target directory | Check folder permissions (`ls -ld .`) | Adjust directory ownership (`chown -R $USER .`) or run in user home directory. |
| **Git commands extremely slow** | Antivirus scanning every file access in `.git/` | Check disk I/O with Task Manager / top | Add repository folder or `.git` to Antivirus scanning exclusion list. |

---

## 8. Performance, Security & Cost Implications

### Performance
Local DVCS architecture eliminates network overhead during core development. Creating a branch in Git is an $O(1)$ disk write of a 41-byte text file containing a SHA hash.

### Security
Git computes SHA-1 or SHA-256 hashes across file contents and directory hierarchies (Merkle Trees). If a bit flips on disk due to hardware failure or malicious tampering, Git detects the corruption immediately during checksum verification (`git fsck`).

### Cost
Git is free open-source software (GPL v2). Storing full history locally uses efficient compression (Zlib packfiles) that typically consumes only 20–30% of raw source code size.

---

## 9. Comparison Table

| Feature | Centralized VCS (SVN) | Distributed VCS (Git) |
|:--------|:----------------------|:----------------------|
| **Repository Location** | Single Central Server | Cloned on every workstation |
| **Offline Work** | Read-only; cannot commit or branch | 100% full capabilities offline |
| **Branching Cost** | Heavy directory copy on server | Lightweight pointer update (41 bytes) |
| **Speed** | Network bound ($O(N)$ network latency) | Local disk bound ($O(1)$ to $O(\text{files})$) |
| **Disaster Recovery** | Catastrophic if server backup lost | Any developer clone can restore full history |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>Q1: Why is Git described as a Directed Acyclic Graph (DAG) rather than a linear history?</strong></summary>

Git's commit history is not a flat linear list; it is a Directed Acyclic Graph:
- **Directed**: Every commit points backward to its parent commit(s) via cryptographic parent hashes.
- **Acyclic**: The graph has a definitive arrow of time; it is impossible for a descendant commit to loop back and become an ancestor of itself.
- **Branching & Merging**: Because branches can diverge and merge commits can have multiple parents (2 or more), the commit structure forms a branching and converging graph topology.
</details>

<details>
<summary><strong>Q2: What is the exact purpose of the `.git` folder and what happens if you delete it?</strong></summary>

The `.git` folder contains the **entire repository**:
1. The immutable object database (`.git/objects/`).
2. All branch pointers and tags (`.git/refs/`).
3. The Staging Area cache (`.git/index`).
4. Configuration and commit logs (`.git/config`, `.git/logs/`).
If you delete `.git`, your working directory files remain on disk, but your project is **no longer a Git repository**. All commit history, branches, tags, stashes, and version tracking are permanently destroyed.
</details>

<details>
<summary><strong>Q3: Explain the difference between `git init` and `git clone`.</strong></summary>

- `git init`: Initializes a brand new, empty Git repository in the current local directory, creating a default `.git` scaffold with zero commits.
- `git clone <url>`: Copies an existing remote repository onto your local machine. It initializes `.git`, downloads the complete remote object database, checks out the default branch into the working tree, and configures a remote pointer named `origin`.
</details>

<details>
<summary><strong>Q4: What is the difference between author and committer in Git commit metadata?</strong></summary>

- **Author**: The person who originally wrote the code changes (`GIT_AUTHOR_NAME`, `GIT_AUTHOR_EMAIL`, `GIT_AUTHOR_DATE`).
- **Committer**: The person (or automated bot) who actually applied or committed the change into the repository (`GIT_COMMITTER_NAME`, `GIT_COMMITTER_EMAIL`, `GIT_COMMITTER_DATE`).
When you cherry-pick a colleague's commit, rebase someone's pull request, or apply a patch, the Author remains your colleague, while the Committer becomes you.
</details>

<details>
<summary><strong>Q5: How does Git guarantee cryptographic content addressability?</strong></summary>

Git uses cryptographic hashing (SHA-1 or SHA-256). Every piece of content (blob, tree, commit, annotated tag) is prepended with a type and size header, hashed, and stored in `.git/objects/` indexed by its hash string. If a single character in a file changes, its hash changes, which cascades up and changes the enclosing tree hash, which changes the commit hash. It is mathematically impossible to alter historical content without invalidating every subsequent commit hash.
</details>

<details>
<summary><strong>Q6: What is a detached HEAD state and how do you recover from it?</strong></summary>

Normally, `HEAD` points to a symbolic branch reference (e.g. `ref: refs/heads/main`), which in turn points to a commit.
A **detached HEAD** occurs when `HEAD` points directly to a commit SHA or tag rather than a branch (e.g. `git checkout <commit_sha>`).
If you make commits in detached HEAD state, they are not attached to any branch. If you switch branches, those commits become orphaned and subject to garbage collection (`git gc`).
*Recovery*: Create a branch pointing to your current detached commit:
```bash
git switch -c new-feature-branch
```
</details>

<details>
<summary><strong>Q7: What configuration levels exist in Git and which level takes precedence?</strong></summary>

Git supports three primary configuration scopes:
1. **System** (`--system`): Applies to all users and repos on the machine (stored in `/etc/gitconfig`).
2. **Global** (`--global`): Applies to all repos for the current operating system user (stored in `~/.gitconfig`).
3. **Local** (`--local`): Applies exclusively to the specific repository (stored in `.git/config`).
*Precedence*: **Local overrides Global, which overrides System.**
</details>

<details>
<summary><strong>Q8: What is the difference between a bare repository and a normal working repository?</strong></summary>

- **Normal Repository**: Contains both the working directory (editable files) and the `.git` folder. Used by engineers to write and test code.
- **Bare Repository (`git init --bare`)**: Contains **only** the contents of the `.git` directory without a working tree. Files cannot be edited or committed to directly. Bare repositories are used exclusively as central remotes on servers (GitHub, GitLab, central servers) to receive pushes and serve clones safely without working tree synchronization conflicts.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Initialize, configure, and inspect an isolated Git sandbox</strong></summary>

**Task**: Create a directory `/tmp/git-test-lab`, initialize it with default branch `main`, configure a local author identity (overriding global), and verify configuration scope.

**Solution**:
```bash
mkdir -p /tmp/git-test-lab && cd /tmp/git-test-lab
git init -b main
git config --local user.name "Sandbox Engineer"
git config --local user.email "sandbox@internal.test"

# Verify local override
git config --local user.name
# Output: Sandbox Engineer

# Verify global is bypassed
git config user.name
# Output: Sandbox Engineer
```
</details>

<details>
<summary><strong>Exercise 2: Inspect HEAD pointer and branch references</strong></summary>

**Task**: Inspect the exact text contents of `.git/HEAD` in an empty repository before and after creating an initial commit.

**Solution**:
```bash
# Before commit:
cat .git/HEAD
# Output: ref: refs/heads/main

# Create file and commit
echo "Initial content" > file.txt
git add file.txt
git commit -m "feat: initial commit"

# Check HEAD again
cat .git/HEAD
# Output: ref: refs/heads/main

# Check where the branch pointer actually points:
cat .git/refs/heads/main
# Output: Displays 40-character commit hash!
```
</details>

<details>
<summary><strong>Exercise 3: Inspect raw Git object files using plumbing commands</strong></summary>

**Task**: Use `git hash-object` and `git cat-file` to manually hash a string, inspect its type, and view its contents from the object database.

**Solution**:
```bash
# 1. Write object to database and retrieve SHA-1
HASH=$(echo "Hello Git Internals" | git hash-object -w --stdin)
echo "Hash: $HASH"

# 2. Inspect object type
git cat-file -t $HASH
# Output: blob

# 3. Inspect object content
git cat-file -p $HASH
# Output: Hello Git Internals
```
</details>

<details>
<summary><strong>Exercise 4: Query configuration variables with origins</strong></summary>

**Task**: Run a one-line command that queries the active Git editor and shows exactly which configuration file set it.

**Solution**:
```bash
git config --show-origin core.editor
```
</details>

<details>
<summary><strong>Exercise 5: Simulate and recover from a detached HEAD commit</strong></summary>

**Task**: Check out the parent commit of a repository to enter detached HEAD, make a commit, and safely recover it onto a new branch named `recovered-work`.

**Solution**:
```bash
# Move to previous commit in detached state
git checkout HEAD~1

# Make a commit
echo "Detached experiment" > experiment.txt
git add experiment.txt
git commit -m "feat: experimental fix"

# Recover onto new branch
git switch -c recovered-work
# Your commit is now safely attached to 'recovered-work'!
```
</details>

<details>
<summary><strong>Exercise 6: Verify repository integrity with git fsck</strong></summary>

**Task**: Execute a complete repository integrity verification check and confirm zero dangling or corrupt objects exist.

**Solution**:
```bash
git fsck --full --strict
# Output: notice: Checking object directory... notice: Checking objects...
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

### Core Initialization & Config Commands

```bash
# Repository Initialization
git init                             # Initialize repo in current folder (uses default branch)
git init -b main                     # Initialize explicitly with branch 'main'
git init --bare /srv/git/repo.git    # Initialize central bare remote repository
git clone <url> [target_dir]         # Clone remote repository

# Global Configuration
git config --global user.name "Name"         # Set global commit author name
git config --global user.email "email@corp"  # Set global commit author email
git config --global init.defaultBranch main  # Set default branch to main
git config --global core.editor "vim"        # Set default text editor
git config --global core.autocrlf input      # Linux/macOS line endings
git config --global core.autocrlf true       # Windows line endings

# Inspection
git config --list --show-origin      # View all active configs and file sources
git status                           # View working tree and staging state
git var -l                           # List all compiled engine variables
```

---

## 13. Further Reading & Official Documentation

- [Git Official Documentation: Getting Started](https://git-scm.com/book/en/v2/Getting-Started-About-Version-Control)
- [Pro Git Book: Git Basics](https://git-scm.com/book/en/v2/Git-Basics-Getting-a-Git-Repository)
- [GitHub Docs: Setting your commit email address](https://docs.github.com/en/account-and-profile/setting-up-and-managing-your-personal-account-on-github/managing-email-preferences/setting-your-commit-email-address)
- [Git SCM: git-init documentation](https://git-scm.com/docs/git-init)

---

[Back to Track Overview](../README.md) | [Back to Module Overview](./README.md) | [02-install-and-configuration.md ->](./02-install-and-configuration.md)
