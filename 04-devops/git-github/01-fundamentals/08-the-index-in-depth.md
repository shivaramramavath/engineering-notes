<!--
================================================================================
Metadata:
  Title: The Git Index In-Depth (.git/index Binary Anatomy & Extensions)
  Track: Git & GitHub / DevOps
  Difficulty: Advanced
  Estimated Time: 55 minutes
  Prerequisites: 07-packfiles-and-garbage-collection.md
  Key Terminology: .git/index, DIRC Signature, Stat Cache, Tree Cache (TREE), Untracked Cache (UNTR), assume-unchanged, skip-worktree, git update-index
================================================================================
-->

# The Git Index In-Depth (.git/index Binary Anatomy & Extensions)

[<- 07-packfiles-and-garbage-collection.md](./07-packfiles-and-garbage-collection.md) | [Back to Module Overview](./README.md) | [09-hashes-and-sha-256.md ->](./09-hashes-and-sha-256.md)

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

To high-level users, the staging area is merely an abstract concept summoned via `git add` and `git commit`. However, to the Git engineering engine, the **Index** (the physical binary file located at `.git/index`) is the single most critical, performance-sensitive data structure in the entire system.

The index acts as the high-speed bridge between your physical filesystem and the immutable object database. It performs two foundational roles:
1. **The Staging Preparation Canvas**: It accumulates and organizes file paths, permissions, and blob hashes destined for the next tree object.
2. **The High-Speed Stat Cache**: It caches operating system file metadata (file size, device, inode, modification timestamps). Without the index, Git would have to re-read and re-hash every single file on disk on every `git status` or `git diff`, making operations on 100,000-file repositories completely unusable.

Understanding the internal binary layout and extension blocks of `.git/index` unlocks advanced performance tuning (`--skip-worktree`, `core.untrackedCache`, `core.fsmonitor`) and deep troubleshooting skills.

---

## 2. Core Concepts

### High-Level Roles of the Index
- **Stat Cache**: Maps filesystem `stat()` outputs to Git blobs. If a file's `mtime` and `size` match the index cache, Git guarantees the file has not changed without reading its bytes from disk!
- **Merge Conflict Resolver**: Holds up to four version slots per file during merge conflicts (Slot 0: normal; Slot 1: ancestor base; Slot 2: HEAD; Slot 3: incoming).
- **Tree Builder**: Enables $O(1)$ commit creation via cached subtrees (`TREE` extension) rather than scanning the entire filesystem.

```
+---------------------------------------------------------------------------------+
|                         The Index as High-Speed Stat Cache                      |
|                                                                                 |
|   1. DEVELOPER RUNS: git status                                                 |
|                                                                                 |
|   2. GIT CALLS OS: stat("src/server.ts")                                        |
|      - Returns: mtime = 1700001234, size = 4096 bytes                           |
|                                                                                 |
|   3. GIT CHECKS .git/index ENTRY:                                               |
|      - Cached: mtime = 1700001234, size = 4096 bytes                             |
|                                                                                 |
|   4. DO STATS MATCH?                                                            |
|      ├── YES ──► File is unmodified! (0 disk reads, 0 crypto hashes calculated!) |
|      └── NO  ──► Stats differ! Git reads file, hashes content, checks for diff. |
+---------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### Binary Layout of `.git/index`
The index file is structured into a fixed 12-byte header, an array of sorted index entries, optional binary extension blocks, and a trailing 20-byte SHA-1/SHA-256 checksum:

```
+-------------------------------------------------------------------+
| 12-Byte Header:                                                   |
|   4-byte Signature: 'DIRC' (Directory Cache: 0x44 0x49 0x52 0x43) |
|   4-byte Version Number (2, 3, or 4)                              |
|   32-bit Integer: Total count of index entries                    |
+-------------------------------------------------------------------+
| Sorted Index Entries (repeated N times):                          |
|   ctime: seconds (4 bytes) + nanoseconds (4 bytes)                |
|   mtime: seconds (4 bytes) + nanoseconds (4 bytes)                |
|   dev (4 bytes), ino (4 bytes), mode (4 bytes)                    |
|   uid (4 bytes), gid (4 bytes), file_size (4 bytes)               |
|   20-byte / 32-byte Object Hash (Blob SHA)                        |
|   16-bit Flags: assume-unchanged, extended, stage (2 bits), length|
|   Null-terminated File Path (e.g. "src/index.ts\0")               |
+-------------------------------------------------------------------+
| Optional Binary Extension Blocks:                                 |
|   'TREE': Cached Tree (Cached tree objects for instant commit)   |
|   'REUC': Resolve Undo (Restores conflicts if merge is undone)    |
|   'UNTR': Untracked Cache (Caches mtimes of directories)          |
|   'FSMN': File System Monitor (Integrates OS event watchers)      |
+-------------------------------------------------------------------+
| Trailing Checksum: 20-byte SHA-1 hash over all preceding bytes    |
+-------------------------------------------------------------------+
```

### Critical Extension Blocks:
- **`TREE` (Cached Tree Extension)**:
  Stores precomputed SHA hashes for subdirectories whose contents have not changed since the last commit. When `git write-tree` runs during `git commit`, Git does not traverse those folders—it writes the cached tree hash immediately!
- **`UNTR` (Untracked Cache Extension)**:
  Stores the `mtime` of directories. If a directory's `mtime` has not changed since the last scan, Git knows no new files have been added or removed, allowing it to skip directory reads entirely during `git status`.

---

## 4. Syntax & Usage

### Low-Level Index Plumbing Commands

```bash
# List all files currently tracked in the index with permissions and hashes
git ls-files --stage

# Display files with special flags (e.g. assume-unchanged, modified, untracked)
git ls-files -v

# Manually register or update a file in the index without touching working tree
git update-index --add --cacheinfo 100644 <blob-hash> <path>

# Tell Git to assume a tracked file has not changed (skips stat checks)
git update-index --assume-unchanged <path>

# Revert assume-unchanged
git update-index --no-assume-unchanged <path>

# Tell Git to skip checking the worktree for this file (used for sparse checkouts)
git update-index --skip-worktree <path>

# Revert skip-worktree
git update-index --no-skip-worktree <path>

# Write current index into a tree object in .git/objects/
git write-tree
```

---

## 5. Practical Examples

### Example 1: Inspecting the DIRC Header with `hexdump` or `xxd`
Confirm the binary signature of your index file:

```bash
mkdir /tmp/index-deep && cd /tmp/index-deep
git init

# View the first 12 bytes of .git/index
# In a fresh repo, make a commit first so .git/index is created
echo "hello" > test.txt && git add test.txt

# Inspect header bytes:
xxd -l 12 .git/index
# Output:
# 00000000: 4449 5243 0000 0002 0000 0001              DIRC........
```
Deconstruction:
- `4449 5243`: ASCII string `DIRC` ("Directory Cache").
- `0000 0002`: Version 2.
- `0000 0001`: Total 1 entry tracked!

### Example 2: The Power of `--assume-unchanged` vs. `--skip-worktree`
Suppose you have a local configuration template `config/local.json` that must be tracked in the repository, but you need to modify it locally for development without committing your edits:

```bash
# Method A: --assume-unchanged (intended for slow network drives)
# If Git needs to touch the file (e.g. merge/pull), the flag may be silently reset!
git update-index --assume-unchanged config/local.json

# Method B: --skip-worktree (RECOMMENDED for local config overrides)
# Git guarantees it will not overwrite or stage local edits, even during pulls!
git update-index --skip-worktree config/local.json

# Verify with git ls-files -v:
git ls-files -v config/local.json
# Output: S config/local.json ('S' designates skip-worktree; 'h' designates assume-unchanged)

# Modify the file locally:
echo '{"debug": true}' > config/local.json
git status
# Clean! Git completely ignores your local modifications!

# To unmark when you finally want to commit:
git update-index --no-skip-worktree config/local.json
```

### Example 3: Simulating `git commit` Using Only `git write-tree` and `git commit-tree`
Demonstrate how the index is converted into a commit:

```bash
# 1. Modify file and stage it
echo "version 2.0" > test.txt
git add test.txt

# 2. Convert .git/index directly into a Tree object
TREE_SHA=$(git write-tree)
echo "Tree SHA: $TREE_SHA"

# 3. Create commit object pointing to that tree and parent HEAD
PARENT_SHA=$(git rev-parse HEAD)
COMMIT_SHA=$(echo "feat: update to v2.0" | git commit-tree $TREE_SHA -p $PARENT_SHA)

# 4. Advance branch pointer
git update-ref refs/heads/main $COMMIT_SHA

# 5. Verify status and history
git status
# Clean!
git log -1 --oneline
# Shows: <hash> feat: update to v2.0
```

### Example 4: Activating the Untracked Cache for Lightning-Fast Status Checks
On massive monorepos, turn on the index untracked cache extension (`UNTR`):

```bash
# Enable untracked cache
git config core.untrackedCache true
git update-index --test-untracked-cache
git update-index --untracked-cache

# Run status to populate the cache
git status

# Inspect index to verify UNTR extension is active
git ls-files --debug-untracked-cache
```

### Example 5: Resolving Merge Conflict Slots Directly in the Index
```bash
# If file 'service.go' has a merge conflict, check all 3 stage versions:
git ls-files -u service.go
# 100644 1a2b... 1  service.go (Base)
# 100644 3c4d... 2  service.go (Ours)
# 100644 5e6f... 3  service.go (Theirs)

# Extract "their" version directly from the index without switching branches:
git show :3:service.go > service.theirs.go

# Extract "our" version:
git show :2:service.go > service.ours.go
```

---

## 6. Best Practices

1. **Use `--skip-worktree` Instead of `--assume-unchanged`**: If you are overriding tracked configuration files locally without wanting to commit them, use `--skip-worktree`. `--assume-unchanged` was designed for slow filesystems and is volatile across merges.
2. **Never Add Local Configs to `.gitignore` if They Are Tracked**: Adding a tracked file to `.gitignore` does nothing. Use `--skip-worktree` or remove the file from tracking via `git rm --cached`.
3. **Turn On `core.untrackedCache` in Large Projects**: Significantly accelerates `git status` by skipping directory crawls where directory `mtime` hasn't changed.
4. **Use `git ls-files -v` to Audit Hidden Index Flags**: If a file stubbornly refuses to appear in `git status`, check if someone marked it with skip-worktree or assume-unchanged.
5. **Back Up or Reset Corrupted Index Files**: If your computer loses power mid-staging and `.git/index` becomes corrupted, deleting `.git/index` and running `git reset` cleanly rebuilds the index from `HEAD` without losing working tree edits.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `fatal: index file corrupt` | Power outage or system crash occurred while Git was writing `.git/index`. | Delete corrupted `.git/index` and run `git reset` to rebuild it safely from `HEAD`. |
| Local edits to a file never appear in `git status` | File was marked with `--skip-worktree` or `--assume-unchanged`. | Run `git ls-files -v` to identify flag, then run `git update-index --no-skip-worktree <file>`. |
| `git pull` overwrites file marked with `--assume-unchanged` | `--assume-unchanged` assumes file matches index; if remote changes, Git resets it. | Use `--skip-worktree` which explicitly protects local modifications. |
| `fatal: Unable to create '.git/index.lock': File exists.` | A crashed or background Git process left an orphaned lockfile. | Ensure no other Git process is running, then remove `.git/index.lock`. |
| Git status takes 15 seconds on a fast SSD | Massive number of untracked files or nested node_modules folders. | Enable `core.untrackedCache = true` and ensure ignored directories have trailing slashes in `.gitignore`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Zero-Read Verification**: Because `.git/index` stores OS filesystem stat information (`dev`, `ino`, `mtime`, `size`), Git verifies 50,000 unmodified files without reading a single byte of file content from disk.
- **`TREE` Extension Optimization**: When you commit, Git does not recursively re-hash directory structures. The cached tree extension reuses previously computed tree SHA hashes for all unmodified subdirectories, enabling commits to complete in under 5 milliseconds.

### Security
- **Index Integrity Checksum**: Every `.git/index` ends with a 20-byte SHA-1/SHA-256 checksum over the entire file contents. If a bit flips on disk or malware alters an entry, Git detects checksum divergence and halts execution.

---

## 9. Comparison Table

| Attribute | `--assume-unchanged` | `--skip-worktree` |
| :--- | :--- | :--- |
| **Intended Purpose** | Performance optimization for slow filesystems | Intentional developer override of tracked files |
| **Behavior on Remote Merge** | Can be silently cleared or cause merge failure | Git protects local version and halts merge cleanly |
| **Flag in `git ls-files -v`** | Lowercase letter (e.g. `h`) | Uppercase `S` |
| **Resets on Checkout?** | Yes, frequently reset during branch switching | No, persistent across branch checkouts |
| **Recommended For** | Legacy NFS / network shares | Local database configs, secret overrides |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between `git update-index --assume-unchanged` and `git update-index --skip-worktree`?</strong></summary>

- **`--assume-unchanged`**: Designed purely as a performance optimization for slow I/O filesystems (such as NFS). It tells Git: *"Assume the file on disk has not changed; do not waste time running `stat()` on it."* However, if Git must modify the file during a pull, merge, or branch switch, the flag is often cleared automatically.
- **`--skip-worktree`**: Designed specifically for developer workflows where a tracked file (e.g. `config.json`) needs local modifications that should never be committed. Git promises to ignore working tree changes to this file indefinitely and protects local changes during merges.
</details>

<details>
<summary><strong>2. How does Git's stat cache in `.git/index` accelerate `git status`?</strong></summary>

For every tracked file, `.git/index` records operating system stat metadata: `mtime` (modification timestamp in seconds and nanoseconds), file size, device ID, and inode. When `git status` runs, Git executes a lightweight `stat()` call on each file. If the file's `mtime` and `size` match the index record, Git mathematically guarantees that the file has not been altered and skips reading or hashing the file contents, completing status queries across 100,000 files in milliseconds.
</details>

<details>
<summary><strong>3. What does the `DIRC` binary signature signify at the beginning of `.git/index`?</strong></summary>

`DIRC` stands for **Directory Cache**, which was the original architectural name for the Git staging area / index when Linus Torvalds authored Git in 2005. The first 4 bytes of any valid Git index file are always `0x44 0x49 0x52 0x43`.
</details>

<details>
<summary><strong>4. How does the `TREE` extension block in `.git/index` optimize `git commit`?</strong></summary>

The `TREE` extension is a cached tree structure that remembers the SHA hashes of directory trees generated during past commits. When you stage a change in `src/utils/math.ts`, Git knows that `src/components/`, `docs/`, and `tests/` were not modified. When `git write-tree` runs, it reuses the precomputed tree hashes stored in the `TREE` extension for those untouched subdirectories rather than re-scanning or re-writing tree objects for the whole project.
</details>

<details>
<summary><strong>5. What causes the error `fatal: Unable to create '.git/index.lock': File exists`?</strong></summary>

Git enforces transactional atomicity when writing to the index. Before writing changes to `.git/index`, it creates a lockfile named `.git/index.lock`. If Git crashes mid-operation (e.g. terminal killed, out of memory, power loss) or if two Git processes run simultaneously (e.g. an IDE background fetch and a terminal commit), the lockfile remains. Removing `.git/index.lock` clears the block.
</details>

<details>
<summary><strong>6. How can an engineer inspect conflict versions of a file directly from the index without switching branches?</strong></summary>

Using the `git show :<stage>:<path>` revision syntax:
- `git show :1:file.txt`: Displays the common ancestor base version.
- `git show :2:file.txt`: Displays our version (`HEAD`).
- `git show :3:file.txt`: Displays their version (incoming branch).
</details>

<details>
<summary><strong>7. What happens if you delete `.git/index`? Is your repository destroyed?</strong></summary>

No. Deleting `.git/index` does not harm your commit history or working tree files. All historical objects remain safe in `.git/objects/`, and active branches remain in `.git/refs/`. Running `git reset` immediately reconstructs a fresh `.git/index` matching the `HEAD` commit.
</details>

<details>
<summary><strong>8. What is the Untracked Cache (`UNTR`) extension?</strong></summary>

Normally, `git status` must open and read every directory on disk to check for new untracked files. The Untracked Cache extension caches the `mtime` of directories. If the directory's `mtime` on disk matches the cached `mtime` in the index, Git knows that no files have been added, removed, or renamed inside that directory, allowing it to bypass traversing entire directory trees.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Inspecting the Raw Binary Signature of `.git/index`</strong></summary>

**Task**: Initialize a repository, stage a file, and use `head` and `xxd` (or `hexdump`) to verify the 12-byte header.

**Solution**:
```bash
mkdir /tmp/index-lab && cd /tmp/index-lab
git init
echo "Hello Index" > file.txt && git add file.txt

# Inspect binary header (first 12 bytes)
xxd -l 12 .git/index
# Shows DIRC signature and entry count
```
</details>

<details>
<summary><strong>Exercise 2: Listing Files with Stage Numbers and Object Hashes</strong></summary>

**Task**: Use `git ls-files --stage` to inspect tracked files and examine file modes and SHA hashes.

**Solution**:
```bash
git ls-files --stage
# Output: 100644 <sha> 0 file.txt
```
</details>

<details>
<summary><strong>Exercise 3: Overriding a Tracked File Locally with `--skip-worktree`</strong></summary>

**Task**: Mark `file.txt` with `--skip-worktree`, modify its contents on disk, verify that `git status` remains clean, and verify the `S` flag in `git ls-files -v`.

**Solution**:
```bash
git commit -m "feat: initial commit"

# Mark with skip-worktree
git update-index --skip-worktree file.txt

# Verify flag
git ls-files -v | grep file.txt
# Output: S file.txt

# Modify file
echo "Secret modification" >> file.txt
git status -s
# Clean! Nothing reported!

# Clear skip-worktree flag
git update-index --no-skip-worktree file.txt
git status -s
# Output: M file.txt
```
</details>

<details>
<summary><strong>Exercise 4: Simulating Index Corruption and Safe Recovery</strong></summary>

**Task**: Corrupt `.git/index` by overwriting it with garbage data. Observe the Git error. Then restore the index without losing working directory files.

**Solution**:
```bash
# Corrupt index
echo "GARBAGE_BYTES" > .git/index

git status
# Output: fatal: index file corrupt

# Safe recovery: delete corrupted file and reset from HEAD
rm .git/index
git reset

git status
# Clean! Index is completely restored!
```
</details>

<details>
<summary><strong>Exercise 5: Building a Tree Object from the Index</strong></summary>

**Task**: Stage two files and write them directly into a tree object using `git write-tree`. Verify the tree contents with `git cat-file -p`.

**Solution**:
```bash
echo "Alpha" > a.txt && echo "Beta" > b.txt
git add a.txt b.txt

TREE_HASH=$(git write-tree)
git cat-file -p $TREE_HASH
# Displays tree entries for a.txt and b.txt
```
</details>

<details>
<summary><strong>Exercise 6: Querying Specific Stage Slots via Plumbing</strong></summary>

**Task**: Create a simulated conflict and use `git show :2:<file>` and `git show :3:<file>` to inspect conflicting versions.

**Solution**:
```bash
git checkout -b branch-a
echo "Version A" > shared.txt && git add shared.txt && git commit -m "A"

git checkout main && git checkout -b branch-b
echo "Version B" > shared.txt && git add shared.txt && git commit -m "B"

git merge branch-a
# Triggers conflict!

# View Ours (Stage 2)
git show :2:shared.txt
# Output: Version B

# View Theirs (Stage 3)
git show :3:shared.txt
# Output: Version A
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Directive / Command | Target | Role |
| :--- | :--- | :--- |
| `DIRC` Signature | `.git/index` Header | 4-byte marker ("Directory Cache") identifying valid Git index |
| `TREE` Extension | `.git/index` Extension | Caches directory tree hashes to accelerate commit creation |
| `UNTR` Extension | `.git/index` Extension | Caches directory modification times to accelerate `git status` |
| `git ls-files --stage` | CLI | Prints tracked paths, permissions, SHA hashes, and stage slots |
| `git update-index --skip-worktree` | CLI | Tells Git to ignore working tree modifications to a tracked file |
| `git update-index --no-skip-worktree` | CLI | Restores normal modification tracking for a file |
| `git ls-files -v` | CLI | Displays index flags (`S` = skip-worktree, `H` = tracked, `h` = assume-unchanged) |
| `git write-tree` | CLI | Compiles current index into a permanent Tree object |

---

## 13. Further Reading & Official Documentation

- [Git Documentation - Index File Format Specification](https://git-scm.com/docs/index-format)
- [Official Git Documentation - git-update-index](https://git-scm.com/docs/git-update-index)
- [Official Git Documentation - git-ls-files](https://git-scm.com/docs/git-ls-files)
- [Git Tools - Reset Demystified](https://git-scm.com/book/en/v2/Git-Tools-Reset-Demystified)

---

[<- 07-packfiles-and-garbage-collection.md](./07-packfiles-and-garbage-collection.md) | [Back to Module Overview](./README.md) | [09-hashes-and-sha-256.md ->](./09-hashes-and-sha-256.md)
