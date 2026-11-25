<!--
================================================================================
Metadata:
  Title: Anatomy of a Git Repository (.git Directory & Bare Repos)
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 01-git-vs-github.md
  Key Terminology: .git directory, Bare Repository, Non-bare Repository, HEAD, refs, objects, index, git clone --mirror
================================================================================
-->

# Anatomy of a Git Repository (.git Directory & Bare Repos)

[<- 01-git-vs-github.md](./01-git-vs-github.md) | [Back to Module Overview](./README.md) | [03-working-tree-and-staging-area.md ->](./03-working-tree-and-staging-area.md)

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

To most developers, a Git repository appears to be just a regular project folder containing source code files. However, from the perspective of Git, the physical source files you edit are merely an ephemeral projection called the **Working Tree**.

The real repository is the hidden `.git/` folder residing at the project root. If you delete the `.git/` directory, your project ceases to be a Git repository; all history, branches, tags, stashes, and commit records vanish instantly, leaving only the currently checked-out flat files. Conversely, if you delete every source code file in your project but retain `.git/`, you can restore the entire project, every historical branch, and every revision in seconds via `git restore .` or `git checkout HEAD`.

Furthermore, servers like GitHub, GitLab, and central CI bastions do not store projects as standard working-tree repositories; they store them as **Bare Repositories**. Understanding repository internals demystifies how Git stores snapshots and enables engineers to recover corrupted repositories.

---

## 2. Core Concepts

### Non-Bare vs. Bare Repositories
1. **Non-Bare Repository (Standard Developer Repo)**:
   - Contains a **Working Tree** (where developer files are edited) AND a `.git/` directory holding version control data.
   - Designed for human interaction: editing files, running tests, staging changes, and committing.
   - Pushing to a non-bare repository on the currently checked-out branch is rejected by Git by default to prevent desynchronizing the working tree with the index.
2. **Bare Repository (`--bare`)**:
   - Contains **NO Working Tree**. The repository directory *is* the contents of `.git/` directly (objects, refs, config, HEAD).
   - Files cannot be edited or viewed directly; commands like `git status` or `git add` fail because there is no working tree.
   - Designed exclusively as a **central synchronization hub** for network operations (`git push` and `git fetch`). GitHub and GitLab host only bare repositories.

```
NON-BARE REPOSITORY (Developer Laptop)        BARE REPOSITORY (GitHub / Remote Hub)
+--------------------------------------+     +--------------------------------------+
| my-project/                          |     | my-project.git/                      |
|   ├── src/                           |     |   ├── HEAD                           |
|   ├── package.json                   |     |   ├── config                         |
|   ├── README.md                      |     |   ├── description                    |
|   └── .git/  <-- Entire Git database |     |   ├── hooks/                         |
|         ├── HEAD                     |     |   ├── info/                          |
|         ├── config                   |     |   ├── objects/ (Database)            |
|         ├── index                    |     |   └── refs/    (Pointers)            |
|         ├── objects/                 |     |                                      |
|         └── refs/                    |     | (NO working tree files checked out!) |
+--------------------------------------+     +--------------------------------------+
```

---

## 3. How It Works Internally

### Deep Dive into the `.git/` Directory Structure

When a repository is initialized with `git init`, Git creates a standardized directory hierarchy:

```
.git/
├── HEAD                     # Plain text file pointing to current active ref (e.g. ref: refs/heads/main)
├── config                   # INI-style repository-specific configuration file
├── description              # Plain text repository description (used by GitWeb)
├── hooks/                   # Executable client/server hook scripts (pre-commit, post-merge, etc.)
│   ├── pre-commit.sample
│   └── ...
├── info/                    # Auxiliary metadata
│   └── exclude              # Private unversioned ignore rules
├── index                    # Binary file: staging area caching tree objects, timestamps, file modes
├── objects/                 # Content-Addressable Object Database
│   ├── [0-9a-f]{2}/         # Loose objects keyed by first 2 hex digits of SHA hash
│   ├── info/                # Packfile metadata
│   └── pack/                # Delta-compressed packfiles (.pack) and index files (.idx)
└── refs/                    # Reference pointers (human-readable names pointing to commit hashes)
    ├── heads/               # Local branches (e.g., refs/heads/main)
    ├── tags/                # Local tags (e.g., refs/tags/v1.0.0)
    └── remotes/             # Remote tracking branches (e.g., refs/remotes/origin/main)
```

### Key Components Explained:
- **`HEAD`**: A reference pointer. When on a branch, it contains a symbolic ref like `ref: refs/heads/main`. In "detached HEAD" state, it contains a raw 40-character commit hash directly.
- **`index`**: A binary cache mapping physical file paths, file modification times (`mtime`), file modes (`100644`, `100755`), and object hashes. It serves as the staging preparation area before creating a tree object.
- **`objects/`**: The core immutable storage engine. All code snapshots, directories, commit metadata, and annotated tags are stored here, compressed with `zlib`.
- **`refs/heads/`**: Local branches are simply 41-byte plain text files. Inside `refs/heads/main` is literally just a 40-character commit hash followed by a newline `\n`.

---

## 4. Syntax & Usage

### Initializing Repositories

```bash
# Initialize a standard non-bare repository with default branch 'main'
git init -b main my-repo

# Initialize a bare repository (conventionally named with .git suffix)
git init --bare /shared/git/my-repo.git

# Initialize with shared group permissions for multi-user Linux servers
git init --bare --shared=group /srv/git/team-repo.git
```

### Cloning Repositories

```bash
# Standard clone: checks out working tree and sets remote 'origin'
git clone git@github.com:octocat/Hello-World.git

# Clone into a specific target folder
git clone git@github.com:octocat/Hello-World.git my-local-folder

# Clone as a bare repository (no working files checked out)
git clone --bare git@github.com:octocat/Hello-World.git Hello-World.git

# Clone a complete exact mirror (includes all remote branches, tags, notes, and refs)
git clone --mirror git@github.com:octocat/Hello-World.git Hello-World-mirror.git

# Shallow clone: downloads only the last N commits (saves bandwidth/time)
git clone --depth 1 git@github.com:octocat/Hello-World.git

# Blob-less clone (partial clone): downloads all commits/trees, but fetches file blobs on-demand
git clone --filter=blob:none git@github.com:octocat/Hello-World.git
```

---

## 5. Practical Examples

### Example 1: Creating a Production Bare Remote Hub on Linux
Set up an on-premises Git remote hub with proper POSIX permissions:

```bash
# 1. As root or admin on Linux server:
sudo mkdir -p /var/git/analytics.git
sudo groupadd git-developers
sudo chown -R root:git-developers /var/git/analytics.git
sudo chmod -R 2775 /var/git/analytics.git # SETGID bit ensures new files inherit group

# 2. Initialize shared bare repository
cd /var/git/analytics.git
git init --bare --shared=group

# 3. Developer pushes to the new remote from workstation:
cd ~/projects/analytics
git remote add central ssh://deploy@git-server.internal/var/git/analytics.git
git push -u central main
```

### Example 2: Inspecting the Internal State of `HEAD` and `refs`
Explore how Git tracks the active branch under the hood:

```bash
mkdir /tmp/repo-inspect && cd /tmp/repo-inspect
git init -b main

# Inspect HEAD before any commits
cat .git/HEAD
# Output: ref: refs/heads/main

# Notice that refs/heads/main does not exist yet!
ls .git/refs/heads/
# Output: (empty - a branch does not physically exist until its first commit!)

# Create first commit
echo "Hello Git" > file.txt
git add file.txt
git commit -m "Initial commit"

# Now inspect refs/heads/main
cat .git/refs/heads/main
# Output: 4b825dc642cb6eb9a060e54bf8d69288fbee4904 (raw commit hash)
```

### Example 3: Detached HEAD State Internals
Observe what happens to `.git/HEAD` when checking out a specific commit:

```bash
# Make a second commit
echo "Second line" >> file.txt
git commit -am "Second commit"

# Check out the first commit directly
FIRST_COMMIT=$(git rev-parse HEAD~1)
git checkout $FIRST_COMMIT

# Inspect HEAD now
cat .git/HEAD
# Output: 4b825dc642cb6eb9a060e54bf8d69288fbee4904
# (HEAD now points directly to a hash, not a symbolic ref! This is Detached HEAD.)

# Return to safety
git checkout main
cat .git/HEAD
# Output: ref: refs/heads/main
```

### Example 4: Mirroring a Repository for Full Disaster Recovery Backup
Create an exact backup mirror of a GitHub repository:

```bash
# 1. Mirror clone repository
git clone --mirror git@github.com:myorg/critical-service.git critical-service.git

# 2. Inspect mirror refs (contains refs/pull/*, refs/tags/*, refs/heads/*)
cd critical-service.git
git show-ref

# 3. In disaster recovery, push mirror to secondary cloud provider (e.g. AWS CodeCommit or GitLab)
git remote add backup-target git@gitlab.com:myorg-backup/critical-service.git
git push --mirror backup-target
```

### Example 5: Recovering from an Accidental Working Tree Wipe
Demonstrate that the `.git/` directory contains your entire repository:

```bash
cd /tmp/repo-inspect

# Accidental catastrophe: Delete all working tree files!
rm -rf *.txt

# git status shows file as deleted
git status -s
# Output:  D file.txt

# Instant full recovery from .git index/objects
git restore .
ls file.txt
# Output: file.txt restored completely intact!
```

---

## 6. Best Practices

1. **Never Commit to a Non-Bare Remote**: Never configure a developer's working repository as a remote push target. Always push to bare repositories (`--bare`).
2. **Back Up the `.git` Directory**: Any backup strategy that copies files must include hidden directories (`.git/`). Backing up source files without `.git/` discards all history.
3. **Use `--mirror` for True Replication**: When migrating repositories between forges or archiving codebases, use `git clone --mirror` rather than `git clone --bare`, as `--mirror` replicates exact refspecs including pull request refs and tags.
4. **Use Shallow Clones in CI/CD**: In automated CI build jobs where history is irrelevant, use `git clone --depth 1` to reduce network transfer and build initialization times by up to 90%.
5. **Protect the `.git` Directory in Production Web Deployments**: Never deploy the `.git/` folder to publicly accessible web server document roots (e.g., Nginx `html/` or Apache `www/`). Attackers can download `.git/` and reconstruct entire source code trees and embedded secrets.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `fatal: this operation must be run in a work tree` | Executed a working-tree command (e.g. `git status`, `git add`) inside a bare repository. | Switch to a non-bare clone or specify `--work-tree` and `--git-dir`. |
| `remote: error: refusing to update checked out branch: refs/heads/main` | Attempted to push to a non-bare repository that currently has that branch checked out. | Convert the target remote into a bare repository (`git clone --bare`). |
| `.git` folder takes up 50GB on disk | Repository contains large uncompressed binaries or years of stale objects. | Run `git gc --prune=now --aggressive` or migrate large assets to Git LFS. |
| `fatal: not a git repository (or any of the parent directories): .git` | Current working directory is not inside an initialized Git repository. | Run `git init` or `cd` into the correct project folder. |
| Public web server exposes source code at `domain.com/.git/` | Web server configuration allows public HTTP access to hidden `.git` folder. | Add Nginx rule: `location ~ /\.git { deny all; return 404; }`. |
| Accidental commit while in "Detached HEAD" disappears after branch switch | Commits created in detached HEAD are unreferenced by any branch; garbage collection will discard them. | Run `git branch <new-branch-name>` before switching away to retain the commits. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Partial Clones (`--filter=blob:none`)**:
  For multi-gigabyte enterprise repositories, partial clones download only the commit history and tree metadata (~50MB), downloading file blobs only when specific files are opened or edited. This enables sub-minute clones on 100GB repositories.
- **Packfile Optimization**:
  Git stores objects loose initially, but packs them into delta-compressed packfiles during `git gc` or `git push`. A repository with 100,000 loose files experiences significant filesystem inode exhaustion until packed.

### Security
- **Production Vulnerability (Exposed `.git`)**:
  Exposing `.git` over HTTP enables automated scanners (e.g. `git-dumper`) to extract complete application source code, API keys, database credentials, and internal architecture.
- **Permissions on Shared Repositories**:
  Bare repositories shared across multiple Linux accounts must have `core.sharedRepository = group` to prevent permission denied errors when developers with differing umasks push commits.

---

## 9. Comparison Table

| Attribute | Non-Bare Repository | Bare Repository (`--bare`) | Mirror Repository (`--mirror`) |
| :--- | :--- | :--- | :--- |
| **Working Tree** | Yes (editable files) | No (object database only) | No (object database only) |
| **Primary Use Case** | Daily developer coding | Central push/fetch hub (GitHub) | Complete backup & forge migration |
| **Default Folder Name**| `project/` | `project.git/` | `project.git/` |
| **Can Run `git status`?**| Yes | No (`fatal: must be run in work tree`) | No |
| **Remote Refs Copied** | Local branches + tracking refs | Cloned branches only | ALL refs (`refs/*` verbatim) |
| **Safe for `git push`?**| No (pushes to checked-out ref fail)| Yes (designed specifically for push)| Yes |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between a bare Git repository and a non-bare Git repository?</strong></summary>

A non-bare Git repository contains both a working tree (the checked-out directory where developers write, view, and edit code) and a `.git` administrative subdirectory holding the object database and refs. A bare repository (`git init --bare`) contains only the version control database without any working tree files. Bare repositories are used exclusively on central servers and remotes (like GitHub) as shared synchronization endpoints because pushing to a checked-out branch in a non-bare repository can corrupt or desynchronize the working tree.
</details>

<details>
<summary><strong>2. What is inside the `.git/HEAD` file and what is a "Detached HEAD" state?</strong></summary>

The `.git/HEAD` file defines what is currently checked out in the working directory. Under normal circumstances, it contains a symbolic reference pointing to a branch, such as `ref: refs/heads/main`. A "Detached HEAD" state occurs when `HEAD` points directly to an exact commit hash rather than a named branch reference (e.g. after running `git checkout <commit-hash>` or checking out a tag). Commits made in this state are orphan commits and will eventually be garbage-collected unless a named branch is created to point to them.
</details>

<details>
<summary><strong>3. Why does Git refuse pushes to a non-bare repository by default?</strong></summary>

In a non-bare repository, the working tree and index represent the state of the currently checked-out branch. If another developer were allowed to push commits to that same branch on the remote, Git's object store and branch ref would update, but the physical files in the working tree and the index would remain unchanged. This creates an inconsistent state where running `git status` on that machine would show unexpected unstaged changes or merge conflicts. Git blocks this via `receive.denyCurrentBranch = refuse`.
</details>

<details>
<summary><strong>4. How does a shallow clone (`--depth`) differ from a partial clone (`--filter`)?</strong></summary>

- **Shallow Clone (`--depth N`)**: Truncates repository history horizontally. Git fetches only the most recent $N$ commits, creating a shallow boundary commit. Historical commits prior to depth $N$ do not exist locally.
- **Partial Clone (`--filter=blob:none`)**: Filters objects vertically. Git downloads the entire historical commit graph and all directory tree structures, but omits file blobs (contents). File contents are downloaded dynamically on-demand only when a developer checks out or diffs a specific commit.
</details>

<details>
<summary><strong>5. What is the difference between `git clone --bare` and `git clone --mirror`?</strong></summary>

`git clone --bare` creates a bare repository containing local branches from the source, but it remaps remote-tracking branches into `refs/heads/*`. `git clone --mirror` sets up an exact 1:1 replica of the source repository: it copies all refs verbatim (`refs/heads/*`, `refs/tags/*`, `refs/remotes/*`, `refs/notes/*`, and platform-specific refs like GitHub's `refs/pull/*`), and configures the refspec so that `git remote update` will overwrite and delete local refs to match the upstream source exactly.
</details>

<details>
<summary><strong>6. What is the purpose of `.git/index`?</strong></summary>

The `.git/index` is a binary file acting as the staging area. It caches file paths, permissions, file size, timestamps (`mtime`), and the SHA hashes of staged blobs. It decouples the physical working tree from the commit history, allowing engineers to incrementally assemble the exact snapshot that will constitute the next commit before writing the commit object.
</details>

<details>
<summary><strong>7. What happens if a developer deletes the `.git` directory in their project?</strong></summary>

The project immediately ceases to be a Git repository. All commit history, branch pointers, stashes, tags, reflogs, and remote connection configurations are destroyed. The physical source code currently in the working tree remains intact as unversioned flat files, but all past revisions are permanently lost unless restored from a remote clone or backup.
</details>

<details>
<summary><strong>8. Why is exposing the `.git` directory on a production web server considered a critical security vulnerability?</strong></summary>

If an Apache or Nginx server misconfiguration serves hidden dotfiles, an external attacker can download the `.git/` folder structure over HTTP. Using readily available open-source tools (like `git-dumper` or `dvcs-ripper`), the attacker can reconstruct the entire Git repository, browse the complete commit history, access source code, retrieve hardcoded API keys and database credentials, and inspect internal infrastructure configurations.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Exploring the Raw Contents of the `.git` Directory</strong></summary>

**Task**: Initialize a Git repository, make a commit, and use standard filesystem inspection commands (`cat`, `ls`, `file`) to examine `HEAD`, `config`, and `refs/heads/`.

**Solution**:
```bash
mkdir /tmp/git-lab-repo && cd /tmp/git-lab-repo
git init -b main
echo "Version 1.0" > app.js
git add app.js && git commit -m "feat: initial app"

# Inspect HEAD:
cat .git/HEAD
# ref: refs/heads/main

# Inspect local branch ref:
cat .git/refs/heads/main
# Displays 40-character SHA hash

# Check file type of index:
file .git/index
# Output: Git index, version 2 or 4
```
</details>

<details>
<summary><strong>Exercise 2: Converting a Non-Bare Repo into a Bare Repo</strong></summary>

**Task**: Take an existing repository with commits and create a corresponding bare repository from it without cloning over the network.

**Solution**:
```bash
# Clone local non-bare repo as bare
git clone --bare /tmp/git-lab-repo /tmp/git-lab-repo.git

# Verify bare repository contents
ls /tmp/git-lab-repo.git
# Output: HEAD config description hooks info objects refs (no source files!)
```
</details>

<details>
<summary><strong>Exercise 3: Observing Detached HEAD State</strong></summary>

**Task**: Create two commits. Check out the first commit by its hash. Verify the contents of `.git/HEAD`. Then create a branch from this state to save your position.

**Solution**:
```bash
cd /tmp/git-lab-repo
echo "Version 2.0" >> app.js
git commit -am "feat: update to v2"

# Get hash of initial commit
INITIAL_HASH=$(git rev-parse HEAD~1)
git checkout $INITIAL_HASH

# Inspect HEAD
cat .git/HEAD
# It contains the literal hash: $INITIAL_HASH

# Save position to new branch
git switch -c fix-v1-patch
cat .git/HEAD
# ref: refs/heads/fix-v1-patch
```
</details>

<details>
<summary><strong>Exercise 4: Performing a Shallow Clone and Unshallowing It</strong></summary>

**Task**: Perform a shallow clone of depth 1 from a local repository with multiple commits. Inspect commit count. Then unshallow the repository to retrieve complete history.

**Solution**:
```bash
# Clone with depth 1
git clone --depth 1 /tmp/git-lab-repo /tmp/shallow-repo
cd /tmp/shallow-repo

# Check commit log (shows only 1 commit)
git rev-list --count HEAD
# Output: 1

# Unshallow to restore full history
git fetch --unshallow
git rev-list --count HEAD
# Output: 2
```
</details>

<details>
<summary><strong>Exercise 5: Diagnosing Web Server Exposure of `.git`</strong></summary>

**Task**: Write an Nginx configuration snippet that blocks all public access to `.git` folders and hidden files.

**Solution**:
```nginx
# Nginx security block to prevent .git leaks
location ~ /\.(?!well-known) {
    deny all;
    access_log off;
    log_not_found off;
    return 404;
}
```
</details>

<details>
<summary><strong>Exercise 6: Inspecting `.git/config` and Adding Custom Tracking</strong></summary>

**Task**: Manually view `.git/config`. Add a custom configuration entry via `git config --local` and verify how it is written to disk.

**Solution**:
```bash
cd /tmp/git-lab-repo

# Set custom parameter
git config --local project.environment "staging"

# Inspect raw config file
cat .git/config
# Contains:
# [project]
#     environment = staging
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Command | Description |
| :--- | :--- |
| `git init -b <branch>` | Initializes a standard non-bare Git repository with default branch |
| `git init --bare <path.git>`| Initializes a bare repository (no working tree, remote hub) |
| `git clone --bare <url>` | Clones repository into a bare object repository |
| `git clone --mirror <url>` | Exact 1:1 replica of all refs, branches, and tags |
| `git clone --depth 1 <url>` | Shallow clone retrieving only latest commit |
| `git clone --filter=blob:none <url>` | Partial clone omitting file contents until accessed |
| `cat .git/HEAD` | Inspects current branch ref or detached commit hash |
| `cat .git/refs/heads/<name>` | Inspects 40-character commit hash of a local branch |
| `git fetch --unshallow` | Converts a shallow clone back into a full-history repository |

---

## 13. Further Reading & Official Documentation

- [Git Internals - Plumbing and Porcelain](https://git-scm.com/book/en/v2/Git-Internals-Plumbing-and-Porcelain)
- [Official Git Documentation - git-init](https://git-scm.com/docs/git-init)
- [Official Git Documentation - git-clone options](https://git-scm.com/docs/git-clone)
- [OWASP - Source Code Disclosure via Exposed .git Directory](https://owasp.org/www-project-web-security-testing-guide/latest/4-Web_Application_Security_Testing/02-Configuration_and_Deployment_Management_Testing/05-Enumerate_Infrastructure_and_Application_Admin_Interfaces)

---

[<- 01-git-vs-github.md](./01-git-vs-github.md) | [Back to Module Overview](./README.md) | [03-working-tree-and-staging-area.md ->](./03-working-tree-and-staging-area.md)
