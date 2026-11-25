<!--
================================================================================
Metadata:
  Title: Commits & The Directed Acyclic Graph (DAG)
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 03-working-tree-and-staging-area.md
  Key Terminology: Commit Object, Tree Pointer, Parent Pointers, Directed Acyclic Graph (DAG), Conventional Commits, Commit Immutability, Author vs Committer
================================================================================
-->

# Commits & The Directed Acyclic Graph (DAG)

[<- 03-working-tree-and-staging-area.md](./03-working-tree-and-staging-area.md) | [Back to Module Overview](./README.md) | [05-git-objects.md ->](./05-git-objects.md)

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

A widespread misconception among developers is that Git stores commits as a sequential list of file differences or diff deltas (e.g., "Commit B adds 5 lines to file X and removes 2 from file Y").

In reality, **Git stores complete snapshots, not deltas**. Every single commit object in Git points to a root tree object representing the entire directory state of the project at that exact millisecond. If a file did not change in a commit, Git simply reuses the pointer to the existing blob in the object database without duplicating the file.

Furthermore, Git organizes commits not into a flat linear tape, but into a **Directed Acyclic Graph (DAG)**. Each commit points backwards to its parent(s). This mathematical graph architecture enables Git's lightning-fast branch creation, nonlinear development, distributed merging, and cryptographically tamper-evident history.

---

## 2. Core Concepts

### Snapshots, Not Deltas
- **Delta-Based Systems (SVN, CVS)**: Store initial base files and subsequent file diffs over time. Calculating the state of a file in version 50 requires reading the base file and sequentially applying 49 patches.
- **Snapshot-Based System (Git)**: Every commit points directly to a root tree object that maps all files and directories. Retrieving the project state at commit 50 is an instant lookup of that commit's root tree.

```
DELTA-BASED (SVN):
File A: [Base File] ---> [Delta 1] ---> [Delta 2] ---> [Delta 3] (Must compute sequentially)

SNAPSHOT-BASED (GIT):
Commit 1: Root Tree ---> [Blob A (v1)] , [Blob B (v1)]
Commit 2: Root Tree ---> [Blob A (v2)] , [Blob B (v1)] (Reuses unchanged Blob B pointer!)
Commit 3: Root Tree ---> [Blob A (v2)] , [Blob B (v2)] , [Blob C (v1)]
```

### The Directed Acyclic Graph (DAG)
Git commits form a directed graph where edges point **backwards** from child commits to parent commits:
- **Directed**: Edges have a direction (child points to parent).
- **Acyclic**: It is mathematically impossible for a commit path to loop back on itself, because a parent cannot reference a child that has not yet been created.
- **Parent Types**:
  - **Root Commit**: Has 0 parents (the very first commit in a repo).
  - **Standard Commit**: Has 1 parent (linear commit).
  - **Merge Commit**: Has 2 parents (combines two diverging branch histories).
  - **Octopus Merge Commit**: Has 3 or more parents (combines multiple topic branches).

```
[Commit A: Root (0 Parents)]
      ▲
      │
[Commit B: Standard (1 Parent)]
      ▲
      ├───────────────────────────────┐
      │                               │
[Commit C: Feature Branch]      [Commit D: Main Branch]
      ▲                               ▲
      │                               │
      └──────────────┬────────────────┘
                     │
         [Commit E: Merge (2 Parents)]
```

---

## 3. How It Works Internally

### Anatomy of a Raw Commit Object
A commit object is a plain text file compressed with `zlib` stored in `.git/objects/`. You can view its raw uncompressed content using `git cat-file -p <commit-hash>`:

```text
tree e3929009ff5a0c0e0cf476901844eef7eb1f07f8
parent 8a1e2f7b824319451996518176510e4a775f0a02
author Alex Morgan <alex@example.com> 1700000000 -0500
committer GitHub <noreply@github.com> 1700000000 +0000
gpgsig -----BEGIN SSH SIGNATURE-----
 ...
 -----END SSH SIGNATURE-----

feat(auth): implement oauth2 refresh token rotation

Add background token rotation middleware to ensure sessions
remain authenticated without requiring user re-login.
```

### Deconstructing the Commit Fields:
1. **`tree`**: The SHA-1/SHA-256 hash of the top-level directory snapshot for this commit.
2. **`parent`**: The SHA hash of the preceding commit. (Omitted for root commit; multiple lines for merge commits).
3. **`author`**: The person who originally wrote the code, along with Unix timestamp and timezone offset.
4. **`committer`**: The person or system entity that actually committed the code to the repository (e.g. cherry-picker, rebase runner, or GitHub web interface).
5. **`gpgsig`** *(optional)*: Cryptographic signature proving provenance.
6. **Commit Message**: A blank line followed by the subject and optional multi-line description.

### Absolute Immutability of Commits
The SHA hash of a commit is computed by hashing the **entire** raw commit payload (tree hash + parent hashes + author name/email/date + committer name/email/date + commit message).

> [!IMPORTANT]
> Because every field contributes to the cryptographic hash, **you can never modify an existing commit**. Any action that claims to "edit" a commit (such as `git commit --amend` or `git rebase -i`) actually creates a brand-new commit object with a different hash and abandons the old one!

---

## 4. Syntax & Usage

### Creating Commits

```bash
# Standard commit with inline message
git commit -m "feat: implement payment webhook handler"

# Commit opening $EDITOR with diff preview
git commit -v

# Amend: replace the latest commit with staged changes and/or updated message
git commit --amend -m "feat: implement payment webhook handler with HMAC validation"

# Create a fixup commit intended for automated rebasing later
git commit --fixup=<target-commit-hash>

# Bypass pre-commit hooks (USE WITH CAUTION - ONLY IN EMERGENCIES)
git commit --no-verify -m "fix: emergency hotfix"
```

### Inspecting Commit History

```bash
# Clean one-line graph view with branches and tags
git log --graph --oneline --decorate --all

# View the full commit object metadata including author, committer, and raw dates
git log --format=fuller -1

# Show files changed, inserted, and deleted per commit
git log --stat -n 5

# Search commit messages for a specific keyword
git log --grep="CVE-2024"

# Search for commits that added or removed a specific string in the codebase (Pickaxe search)
git log -S "SECRET_ENCRYPTION_KEY"
```

---

## 5. Practical Examples

### Example 1: Dissecting Author vs. Committer
Demonstrate how author and committer diverge during a cherry-pick or rebase:

```bash
mkdir /tmp/commit-lab && cd /tmp/commit-lab
git init

# Developer Alice authors and commits code
export GIT_AUTHOR_NAME="Alice Smith"
export GIT_AUTHOR_EMAIL="alice@company.com"
export GIT_COMMITTER_NAME="Alice Smith"
export GIT_COMMITTER_EMAIL="alice@company.com"

echo "Feature Alpha" > alpha.txt
git add alpha.txt && git commit -m "feat: feature alpha by Alice"

# Engineer Bob cherry-picks Alice's commit
export GIT_COMMITTER_NAME="Bob Jones"
export GIT_COMMITTER_EMAIL="bob@company.com"

git commit --amend -m "feat: feature alpha amended by Bob"

# Inspect fuller log
git log --format=fuller -1
```
Output:
```text
Author:     Alice Smith <alice@company.com>
AuthorDate: Mon Nov 25 10:00:00 2024 -0500
Commit:     Bob Jones <bob@company.com>
CommitDate: Mon Nov 25 10:05:00 2024 -0500
```

### Example 2: Inspecting Raw Objects with `git cat-file`
Use plumbing commands to inspect the exact bytes stored in `.git/objects/`:

```bash
COMMIT_HASH=$(git rev-parse HEAD)
echo "Current Commit: $COMMIT_HASH"

# Inspect object type
git cat-file -t $COMMIT_HASH
# Output: commit

# Inspect object size in bytes
git cat-file -s $COMMIT_HASH

# Inspect raw object payload
git cat-file -p $COMMIT_HASH
```

### Example 3: Conventional Commits in Practice
Adopt the standardized Conventional Commits format to drive automated changelogs and semantic versioning:

```bash
# Feature addition (triggers MINOR version bump: 1.1.0 -> 1.2.0)
git commit -m "feat(api): add endpoint for user profile export"

# Bug fix (triggers PATCH version bump: 1.2.0 -> 1.2.1)
git commit -m "fix(auth): handle expired token edge-case gracefully"

# Breaking change (triggers MAJOR version bump: 1.2.1 -> 2.0.0)
git commit -m "feat(api)!: remove deprecated v1 search endpoint

BREAKING CHANGE: The /api/v1/search endpoint has been retired.
Consumers must migrate to /api/v2/search with paginated payloads."
```

### Example 4: Creating Fixup Commits for Clean Review History
Keep working history clean before merging PRs:

```bash
# Identify target commit you need to correct
TARGET=$(git log --oneline | grep "feat: initial" | awk '{print $1}')

# Make bug fix
echo "Fixed null pointer bug" >> alpha.txt
git add alpha.txt

# Create automatic fixup commit
git commit --fixup=$TARGET

git log --oneline -2
# Outputs:
# fixup! feat: initial
# feat: initial

# Automatically squash fixup commit into target during interactive rebase:
git rebase -i --autosquash HEAD~3
```

### Example 5: Visualizing Divergent Branch Histories in the DAG
```bash
git checkout -b feature-payment
echo "payment logic" > pay.txt && git add pay.txt && git commit -m "feat: payment"

git checkout main
echo "user profile" > user.txt && git add user.txt && git commit -m "feat: user"

# Inspect DAG divergence
git log --graph --all --oneline --decorate
```
ASCII Output:
```text
* 7f2a1b4 (HEAD -> main) feat: user
| * 3c9d1e2 (feature-payment) feat: payment
|/  
* 8a1e2f7 feat: initial commit
```

---

## 6. Best Practices

1. **Follow Conventional Commits**: Structure all messages as `<type>(<scope>): <subject>` (`feat`, `fix`, `docs`, `refactor`, `perf`, `test`, `chore`). This enables automated changelog generators and semantic release tools.
2. **Keep the Subject Line Under 50 Characters**: Write in the imperative mood ("Add feature" not "Added feature" or "Adds feature").
3. **Use the Body for Context**: Use the commit body (separated from subject by a blank line) to answer *why* the change was made, not *what* lines changed (the diff already shows what changed).
4. **Make Atomic Commits**: Each commit should represent a single logical unit of work. Never bundle unrelated changes (e.g. whitespace reformatting + critical database query fix) into one commit.
5. **Never Rewrite Shared History**: Avoid amending (`git commit --amend`) or rebasing commits that have already been pushed to a shared collaboration branch unless agreed upon by your team.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| Commit attributed to wrong username or email | `user.name` or `user.email` was incorrect when `git commit` ran. | Run `git commit --amend --reset-author` to update to currently configured identity. |
| Subject and body squashed into one long subject line | Missing blank line between subject and body in commit message. | Always separate subject and body with a completely empty line. |
| Ran `git commit --amend` and now `git push` is rejected | Amending created a new commit hash; local and remote have diverged. | If on a private feature branch, push with `git push --force-with-lease`. |
| Pushed commit message with a typo or internal secret | Developer committed sensitive data. | Amend locally if unpushed; if pushed, follow secret rotation and run `git-filter-repo` (Module 03). |
| Pre-commit hook prevents commit with errors | Linter or unit test failed in `.git/hooks/pre-commit`. | Fix the lint/test errors; avoid using `--no-verify` unless in an outage hotfix situation. |
| Commit timestamp shows future or incorrect date | System clock on developer machine was desynchronized. | Reset author date: `git commit --amend --date="now"`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Snapshot Storage Efficiency**: Because unchanged files share identical SHA hashes across commits, a commit touching 1 file in a 100,000-file project creates only 1 new blob and a few directory tree objects. The other 99,999 files are referenced by lightweight hash pointers.
- **DAG Traversal Speed**: Finding the common ancestor of two branches (`merge-base`) takes milliseconds even on repositories with 500,000 commits because Git traverses the DAG backwards using graph algorithms (lowest common ancestor).

### Security
- **Tamper-Evident History**: Because every commit's hash includes the parent commit's hash, altering any past commit breaks the cryptographic chain for all descendant commits.
- **Cryptographic Provenance**: Combining Conventional Commits with cryptographic signing guarantees non-repudiation and supply chain security for compliance (SOC 2, ISO 27001).

---

## 9. Comparison Table

| Attribute | Git Commit Model | Legacy VCS (SVN / CVS) Model |
| :--- | :--- | :--- |
| **Storage Paradigm** | Complete repository snapshot | Sequential diff / delta chain |
| **Data Structure** | Directed Acyclic Graph (DAG) | Linear revision numbers (r1, r2, r3) |
| **Immutability** | Cryptographically immutable | Revisions can be retroactively edited |
| **Identification** | 40-char SHA-1 or 64-char SHA-256 | Integer sequence counter |
| **Network Requirement**| 100% Offline execution | Must contact centralized database server |
| **Branch Creation Cost**| Free (41-byte text pointer to commit) | Expensive (copies folder tree on server) |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain why Git is described as storing snapshots rather than file deltas.</strong></summary>

Unlike older version control systems (like SVN) that record an initial file followed by subsequent diff patches, every commit in Git contains a pointer to a root tree object that represents the entire directory structure of the project at that moment. For files that were not modified in the commit, Git does not duplicate storage; it simply reuses the existing SHA hash pointing to the previously stored blob in `.git/objects/`. This ensures constant-time checkout speeds regardless of history depth.
</details>

<details>
<summary><strong>2. What is the difference between the Author and the Committer in a Git commit object?</strong></summary>

- **Author**: The individual who originally wrote the code changes (`GIT_AUTHOR_NAME`, `GIT_AUTHOR_EMAIL`, and `GIT_AUTHOR_DATE`).
- **Committer**: The person or automated system that actually committed or applied the snapshot to the repository (`GIT_COMMITTER_NAME`, `GIT_COMMITTER_EMAIL`, and `GIT_COMMITTER_DATE`).
These diverge during operations like `git cherry-pick`, `git rebase`, applying email patches via `git am`, or when merging pull requests via the GitHub web UI.
</details>

<details>
<summary><strong>3. Why is it mathematically impossible to alter a commit in Git without changing its hash and the hashes of all its descendants?</strong></summary>

A commit's SHA identifier is calculated by cryptographically hashing the commit object's contents, which includes: the root tree hash, the author/committer data, the commit message, and **the SHA hash of its parent commit(s)**. If any data in commit $A$ is altered, $A$'s hash changes to $A'$. Because child commit $B$ contains $A$'s hash in its `parent` header, $B$'s payload is now different, changing $B$'s hash to $B'$. This cascading effect ripples through every descendant commit in the DAG.
</details>

<details>
<summary><strong>4. How many parents can a Git commit have?</strong></summary>

- **0 Parents**: Root commit (the initial commit in a repository, or a new orphan branch).
- **1 Parent**: Standard linear commit.
- **2 Parents**: Standard merge commit (created when merging two branches).
- **3 or more Parents**: An "Octopus Merge" commit (created via `git merge branch1 branch2 branch3`), merging multiple branches into a single commit.
</details>

<details>
<summary><strong>5. What is the Conventional Commits specification and why is it valuable in modern CI/CD?</strong></summary>

Conventional Commits is a lightweight convention on top of commit messages that provides a standardized format: `<type>[optional scope]: <description>`. Common types include `feat`, `fix`, `docs`, `refactor`, `perf`, `test`, and `chore`. By establishing machine-readable commit histories, CI/CD pipelines can automatically calculate semantic versions (SemVer bumps: PATCH for `fix`, MINOR for `feat`, MAJOR for `BREAKING CHANGE`) and generate release changelogs without human intervention.
</details>

<details>
<summary><strong>6. What does `git commit --amend` actually do under the hood?</strong></summary>

`git commit --amend` does not modify the existing commit in place. Instead, it reads the current `HEAD` commit's parent, combines staged index changes with the current commit snapshot, prompts for a new commit message, and writes a **brand-new commit object** to `.git/objects/`. It then updates the active branch reference to point to this new commit. The old commit remains in `.git/objects/` as an unreachable dangling object until pruned by garbage collection.
</details>

<details>
<summary><strong>7. How does the Pickaxe search (`git log -S`) work?</strong></summary>

`git log -S <string>` searches through the commit history to find commits that introduced or removed an exact string from files in the repository. Unlike a simple text grep on file contents or commit messages, the Pickaxe algorithm compares the diff of each commit against its parent and returns only commits where the number of occurrences of the string changed.
</details>

<details>
<summary><strong>8. What is a "Fixup" commit and how does it streamline code review?</strong></summary>

Created via `git commit --fixup=<commit-hash>`, a fixup commit automatically sets its message to `fixup! <original commit subject>`. When running `git rebase -i --autosquash`, Git automatically recognizes this prefix, repositions the fixup commit directly below the target commit in the rebase todo list, and squashes it into the target commit without prompting for a new message. This allows developers to push review corrections during PR review and cleanly squash them before merging.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Inspecting a Raw Commit Object with `git cat-file`</strong></summary>

**Task**: Initialize a repository, make a commit, find its hash, and print the raw uncompressed commit payload to view tree, author, and committer headers.

**Solution**:
```bash
mkdir /tmp/dag-lab && cd /tmp/dag-lab
git init
echo "print('DAG test')" > script.py
git add script.py
git commit -m "feat: initial commit"

HASH=$(git rev-parse HEAD)
git cat-file -p $HASH
# Observe the tree pointer, author, committer, and commit message
```
</details>

<details>
<summary><strong>Exercise 2: Observing Hash Cascading on Commit Modification</strong></summary>

**Task**: Create commit A and commit B. Record both hashes. Amend commit A (or change its message via rebase) and observe how the hashes of both A and B completely change.

**Solution**:
```bash
echo "Line 1" > file.txt && git add file.txt && git commit -m "Commit A"
HASH_A1=$(git rev-parse HEAD)

echo "Line 2" >> file.txt && git commit -am "Commit B"
HASH_B1=$(git rev-parse HEAD)

# Amend the latest commit
git commit --amend -m "Commit B amended"
HASH_B2=$(git rev-parse HEAD)

echo "B1: $HASH_B1 vs B2: $HASH_B2"
# The hashes are completely different!
```
</details>

<details>
<summary><strong>Exercise 3: Creating and Inspecting a Merge Commit</strong></summary>

**Task**: Create a branch, make a commit, switch back to main, make a commit, merge the branch, and use `git cat-file -p` to verify that the merge commit has two `parent` headers.

**Solution**:
```bash
git checkout -b feature
echo "feat" > feat.txt && git add feat.txt && git commit -m "feat commit"

git checkout main
echo "main" > main.txt && git add main.txt && git commit -m "main commit"

# Merge with explicit merge commit
git merge --no-ff feature -m "Merge feature branch"

# Inspect merge commit parents
git cat-file -p HEAD | grep parent
# Output contains two distinct "parent <hash>" lines!
```
</details>

<details>
<summary><strong>Exercise 4: Resetting Author Identity on an Existing Commit</strong></summary>

**Task**: Amend a commit to reset its author to match updated global git configuration using `--reset-author`.

**Solution**:
```bash
git config user.name "Updated Identity"
git commit --amend --reset-author --no-edit

git log -1 --format=fuller
# Confirms Author is now "Updated Identity" with updated timestamp
```
</details>

<details>
<summary><strong>Exercise 5: Automated Fixup Squashing</strong></summary>

**Task**: Create a base commit, make a correction using `git commit --fixup`, and squash it automatically using `git rebase -i --autosquash`.

**Solution**:
```bash
echo "Initial draft" > draft.txt
git add draft.txt && git commit -m "feat: write draft"
BASE_HASH=$(git rev-parse HEAD)

echo "Draft typo fix" > draft.txt
git add draft.txt
git commit --fixup=$BASE_HASH

# Check log
git log --oneline -2

# Run non-interactive rebase with autosquash enabled
GIT_SEQUENCE_EDITOR=: git rebase -i --autosquash HEAD~2

# Verify history is clean: only 1 combined commit remains!
git log --oneline -1
```
</details>

<details>
<summary><strong>Exercise 6: Graph Visualization Formatting</strong></summary>

**Task**: Construct a custom `git log` command that renders an ASCII graph displaying branch decorations, relative commit age, short hash, and commit subject.

**Solution**:
```bash
git log --graph --pretty=format:'%C(yellow)%h%Creset -%C(auto)%d%Creset %s %Cgreen(%cr)%Creset' --all
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Command | Purpose |
| :--- | :--- |
| `git commit -m "msg"` | Create snapshot with inline message |
| `git commit -v` | Create snapshot opening `$EDITOR` with full diff preview |
| `git commit --amend` | Replace latest commit with new snapshot and message |
| `git commit --amend --reset-author` | Update author metadata on latest commit to match active config |
| `git commit --fixup=<hash>` | Create a `fixup!` commit targeting an earlier commit |
| `git rebase -i --autosquash` | Automatically reorder and squash fixup commits into targets |
| `git cat-file -p <hash>` | Print raw contents of commit object (tree, parents, author, msg) |
| `git log --graph --oneline --all` | Visualize commit DAG across all branches and tags |
| `git log -S "<string>"` | Search history for commits that added or removed `<string>` |

---

## 13. Further Reading & Official Documentation

- [Git Internals - Git Objects](https://git-scm.com/book/en/v2/Git-Internals-Git-Objects)
- [Conventional Commits 1.0.0 Specification](https://www.conventionalcommits.org/en/v1.0.0/)
- [A Note About Git Commit Messages (Tim Pope)](https://tbaggery.com/2008/04/19/a-note-about-git-commit-messages.html)
- [Official Git Documentation - git-commit](https://git-scm.com/docs/git-commit)

---

[<- 03-working-tree-and-staging-area.md](./03-working-tree-and-staging-area.md) | [Back to Module Overview](./README.md) | [05-git-objects.md ->](./05-git-objects.md)
