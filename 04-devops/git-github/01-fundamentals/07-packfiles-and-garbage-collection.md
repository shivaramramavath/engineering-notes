<!--
================================================================================
Metadata:
  Title: Packfiles, Delta Compression & Garbage Collection (git gc)
  Track: Git & GitHub / DevOps
  Difficulty: Advanced
  Estimated Time: 55 minutes
  Prerequisites: 06-refs-branches-and-head.md
  Key Terminology: Loose Objects, Packfiles (.pack), Pack Index (.idx), Delta Compression, Reverse Deltas, git gc, git prune, git maintenance
================================================================================
-->

# Packfiles, Delta Compression & Garbage Collection (git gc)

[<- 06-refs-branches-and-head.md](./06-refs-branches-and-head.md) | [Back to Module Overview](./README.md) | [08-the-index-in-depth.md ->](./08-the-index-in-depth.md)

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

When you initialize a repository and make commits, Git stores each object as a separate, individually compressed file in `.git/objects/`. These are called **Loose Objects**.

While loose objects make writes extremely fast during daily coding (just compress and write a single file to disk), this storage model exhibits two severe scaling limits over time:
1. **Inode and Filesystem Inefficiency**: Storing 500,000 loose files exhausts operating system filesystem inodes, degrades directory traversal speeds, and wastes storage block slack space.
2. **Storage Redundancy**: If you modify 1 line in a 10MB text file across 20 commits, Git initially creates twenty separate 10MB loose blobs, consuming 200MB of storage.

To solve this, Git introduces **Packfiles (`.pack`)**, an advanced **Delta Compression** engine, and automated **Garbage Collection (`git gc`)**. Through packfiles, Git compresses related versions of files into deltas, reducing repository disk size by **90% to 98%**, while accelerating network pushes and fetches.

---

## 2. Core Concepts

### Loose Objects vs. Packed Objects
- **Loose Object**: A single zlib-compressed object stored at `.git/objects/xx/yyyy...`. Represents a full, independent snapshot of that object with zero delta encoding.
- **Packfile (`.pack`)**: A single binary archive file located in `.git/objects/pack/` that stores hundreds or millions of compressed objects concatenated together using delta compression.
- **Pack Index (`.idx`)**: A companion binary lookup table providing binary-search indexing ($O(\log N)$) into the packfile so Git can retrieve any object by its SHA hash without reading the entire archive.

```
+-----------------------------------------------------------------------------------+
|                        Loose Objects vs. Packfile Storage                         |
|                                                                                   |
|  LOOSE STORAGE (.git/objects/):                                                   |
|  ├── 4b/825dc... (Blob v1: 100 KB)                                                |
|  ├── 7c/1a2b3... (Blob v2: 101 KB)                                                |
|  └── 9e/f4d5e... (Blob v3: 102 KB)                                                |
|  Total on Disk: 303 KB                                                            |
|                                                                                   |
|                   │                                                               |
|                   ▼ Executing: git gc / git repack                                |
|                                                                                   |
|  PACKFILE STORAGE (.git/objects/pack/):                                           |
|  ├── pack-1a2b3c...idx (Binary search index)                                      |
|  └── pack-1a2b3c...pack (Single compressed archive)                               |
|        ├── [Blob v3: 102 KB (Full base object - Latest Revision)]                 |
|        ├── [Blob v2: Delta against v3 (1.2 KB)]                                   |
|        └── [Blob v1: Delta against v2 (0.8 KB)]                                   |
|  Total on Disk: ~104 KB (65% Space Reduction!)                                    |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### Reverse Delta Compression: Optimizing for the Future
Traditional backup and VCS systems often store an initial base version and calculate forward deltas into the future:
`Base v1 -> Delta v2 -> Delta v3 -> Delta v4`.

Git does the exact opposite: it employs **Reverse Delta Compression**.
- Git stores the **newest version** (HEAD) as a complete, full-size object.
- Git stores **older versions** as deltas pointing to newer versions.

**Why?** In software engineering, 95%+ of all checkouts, diffs, and builds query the latest commit or recent history. By storing the newest version intact, Git never has to reconstruct deltas for the active working code—it is an instant $O(1)$ read. Reconstructing a revision from 5 years ago requires following the delta chain backwards, trading compute time on rare historical queries to keep modern operations blazingly fast.

### Anatomy of Garbage Collection (`git gc`)
When `git gc` runs, it executes a rigorous multi-stage pipeline:
1. **Reference Packing**: Executes `git pack-refs --all --prune`, packing loose branch and tag files into `.git/packed-refs`.
2. **Object Packing**: Runs `git pack-objects`, consolidating loose objects into packfiles while running delta-compression heuristics (sliding window matching).
3. **Commit Graph Generation**: Updates the binary commit-graph file (`.git/objects/info/commit-graph`) to accelerate graph traversal operations.
4. **Pruning Unreachable Objects (`git prune`)**: Scans all objects in `.git/objects/`. Any object that cannot be reached by traversing backwards from any branch, tag, reflog, or index is an **unreachable (dangling) object**.
5. **The Grace Period**: Git protects against deleting in-flight developer work. Unreachable objects younger than the expiry window (`gc.pruneExpire`, default 2 weeks) are preserved in loose storage. Only unreachable objects older than 2 weeks are deleted from disk.

---

## 4. Syntax & Usage

### Running and Tuning Garbage Collection

```bash
# Standard garbage collection (packs refs, packs loose objects, prunes expired objects)
git gc

# Aggressive optimization: uses larger sliding window to find tighter deltas (takes more CPU/RAM)
git gc --aggressive

# Force immediate pruning of ALL unreachable objects, bypassing the 2-week safety window
git gc --prune=now

# Inspect packfile health and display compression statistics
git verify-pack -v .git/objects/pack/*.idx

# Count loose vs. packed objects and view total on-disk size
git count-objects -vH
```

---

### Configuring Automated Git Maintenance (Git 2.30+)
Modern Git includes background maintenance daemons that eliminate the need to run manual `git gc`:

```bash
# Register current repository in system background scheduler (systemd timer on Linux, LaunchAgents on macOS, Task Scheduler on Windows)
git maintenance start

# Run maintenance tasks once immediately
git maintenance run

# Stop background maintenance for this repository
git maintenance stop
```

---

## 5. Practical Examples

### Example 1: Observing Object Packing and Compression in Real Time
Watch loose objects collapse into a packfile:

```bash
mkdir /tmp/pack-lab && cd /tmp/pack-lab
git init

# Generate 5 commits editing a text document
for i in $(seq 1 5); do
  echo "This is a document line $i with random padding $(openssl rand -hex 100)" >> doc.txt
  git add doc.txt && git commit -m "feat: revision $i"
done

# Inspect object count before packing
git count-objects -vH
# Notice: "count: 15" (loose objects), "in-pack: 0"

# Pack all objects manually via git gc
git gc

# Inspect object count after packing
git count-objects -vH
# Notice: "count: 0" (zero loose objects!), "in-pack: 15"
# All objects now live inside .git/objects/pack/!
```

### Example 2: Analyzing Packfile Contents with `git verify-pack`
Peek inside the binary packfile to view delta chains and object sizes:

```bash
IDX_FILE=$(ls .git/objects/pack/*.idx | head -1)

# List objects with type, size, in-pack size, offset, and delta parent
git verify-pack -v $IDX_FILE | head -n 15
```
Example Output Columns:
```text
SHA-1                                    type   size  in-pack-size offset-in-pack [depth/base-SHA]
-------------------------------------------------------------------------------------------------
4b825dc642cb6eb9a060e54bf8d69288fbee4904 commit  185   140         12
7a8b9c1d2e3f4a5b6c7d8e9f0a1b2c3d4e5f6a7b blob    1024  210         152
c1d2e3f4a5b6c7d8e9f0a1b2c3d4e5f6a7b8c9d0 blob    120   45          362    1 7a8b9c1d2...
```
*Notice that the third object is a delta of depth 1 pointing to `7a8b9c1d2...`!*

### Example 3: Simulating Dangling Objects and Forcing Prune
Demonstrate how unreachable objects are created and pruned:

```bash
# Create an experimental commit
echo "Sensitive Scratchpad" > scratch.txt
git add scratch.txt && git commit -m "temp scratchpad"

SCRATCH_HASH=$(git rev-parse HEAD)

# Hard reset backwards to abandon the commit
git reset --hard HEAD~1

# The commit still exists in the object store!
git cat-file -p $SCRATCH_HASH
# Output: Returns the scratchpad commit cleanly!

# Find dangling objects with git fsck
git fsck --lost-found
# Shows: dangling commit $SCRATCH_HASH

# Expire reflogs and force-prune immediately
git reflog expire --expire=now --all
git prune --expire=now

# Try reading the object now
git cat-file -p $SCRATCH_HASH
# Output: fatal: Not a valid object name $SCRATCH_HASH
# Permanently expunged from disk!
```

### Example 4: Diagnosing Large Blobs Bloating Repository History
Find the top 5 largest files in a repository's entire history:

```bash
git verify-pack -v .git/objects/pack/*.idx \
  | grep blob \
  | sort -k3nr \
  | head -n 5 \
  | while read -r hash type size rest; do
      path=$(git rev-list --objects --all | grep "$hash" | awk '{print $2}')
      echo "$size bytes | Hash: $hash | Path: $path"
    done
```

### Example 5: Configuring Automatic GC Tuning in `.gitconfig`
Prevent disruptive mid-day GC freezes by tuning thresholds:

```ini
[gc]
    # Do not auto-gc unless loose objects exceed 10,000
    auto = 10000
    # Do not repack unless packfiles exceed 50
    autoPackLimit = 50
    # Retain unreachable objects for 30 days before pruning
    pruneExpire = 30.days.ago
    # Parallelize pack compression across all CPU cores
    threads = 0
```

---

## 6. Best Practices

1. **Enable `git maintenance` on Developer Laptops**: Run `git maintenance start` to allow Git to perform hourly pre-fetching, loose-object packing, and commit-graph updates silently in the background.
2. **Never Run `git prune --now` in Multi-User or Concurrent Environments**: If another process or developer is staging files via `git add`, their newly written unreferenced blobs could be destroyed before they commit.
3. **Use `git gc --aggressive` Sparingly**: Aggressive repacking recalculates delta windows across the entire repository history. It can take hours and consume 100% CPU on large codebases; run it only during scheduled maintenance or before publishing open-source release mirrors.
4. **Monitor Repository Bloat Before Pushing**: Use `git count-objects -vH` to ensure accidental large binaries weren't committed before pushing to GitHub.
5. **Pair `git gc` with Reflog Expiration for Hard Deletions**: If removing leaked credentials or giant files, running `git gc` alone is insufficient because reflog entries keep objects reachable for 90 days. Run `git reflog expire --expire=now --all` prior to `git gc --prune=now`.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `git gc --prune=now` did not delete an abandoned commit | The commit is still reachable via a reflog entry (`HEAD@{...}`) or branch pointer. | Expire reflogs first: `git reflog expire --expire=now --all && git gc --prune=now`. |
| Git commands pause for 30+ seconds with `Auto packing the repository in background for optimum performance` | Loose object count exceeded `gc.auto` threshold (default 6700), triggering auto-gc. | Allow it to complete once, or run `git gc` during idle time. |
| Packfile creation crashes with `fatal: Out of memory` | Delta search window exhausted system RAM on large binary files. | Set `git config pack.windowMemory 256m` and `git config pack.threads 2`. |
| `git fsck` reports hundreds of `dangling blob` and `dangling commit` warnings | Normal consequence of rebase, amend, and staging operations; objects are safely awaiting GC expiry. | Harmless; run `git gc` if you need to reclaim disk space immediately. |
| Repository clone size on disk is larger than actual project files | Accumulation of historical deltas and old packfiles. | Run `git repack -a -d -f --depth=250 --window=250` to generate a single compact packfile. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Network Transfer Acceleration**: When running `git push` or `git fetch`, Git reuses existing packfile deltas directly ("thin packs") without recomputing compression over the network, achieving near line-speed transfer.
- **Commit-Graph File (`commit-graph`)**: Introduced in Git 2.18+, this binary acceleration file stores topological commit graph metadata (parents, generations). It accelerates log walks, merge-base calculations, and branch filtering by up to **5000%**.

### Cost
- **Cloud Bandwidth & Storage Savings**: A 10GB raw repository typically packs down to **300MB - 500MB**. At scale across thousands of CI runner clones per day, delta compression saves terabytes of cloud egress bandwidth and thousands of dollars in cloud object storage bills.

---

## 9. Comparison Table

| Attribute | Loose Objects | Packed Objects (`.pack`) |
| :--- | :--- | :--- |
| **Storage Structure** | Individual file per object | Single concatenated binary file |
| **Delta Compression** | None (100% full snapshot) | Reverse delta compression |
| **Filesystem Overhead** | High (1 inode per object) | Ultra-low (2 files: `.pack` and `.idx`) |
| **Write Speed** | Instantaneous ($O(1)$ single write)| Slower (requires delta calculation) |
| **Network Transfer** | Inefficient (cannot stream) | Highly optimized (streamed over wire) |
| **Lookup Speed** | Direct directory hash lookup | Binary search lookup via `.idx` |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain the difference between loose objects and packed objects in Git.</strong></summary>

Loose objects are individual, `zlib`-compressed files stored directly in `.git/objects/xx/yyyy...`. Each represents a single standalone object (blob, tree, commit, tag) with no delta compression. Packed objects are consolidated into a single binary archive file called a Packfile (`.pack`), accompanied by an index file (`.idx`). Packfiles use sliding-window delta compression to store file revisions as byte-level differences against other similar objects, reducing disk consumption by up to 90% and dramatically accelerating network operations.
</details>

<details>
<summary><strong>2. What is "Reverse Delta Compression" in Git packfiles and why was it designed this way?</strong></summary>

Traditional systems store an original base version and calculate forward deltas into the future. Git reverses this: it stores the **most recent version** of a file (the tip of the branch) as a complete, un-delta'd object, and encodes older historical versions as deltas relative to newer ones. Because the vast majority of operations (checkout, build, test, diff) target the latest revision, this ensures $O(1)$ instantaneous retrieval for active development without needing to reconstruct delta chains.
</details>

<details>
<summary><strong>3. How does `git gc` distinguish between reachable and unreachable objects?</strong></summary>

Git performs a reachability traversal starting from all known **reference tips** (local branches in `refs/heads/*`, tags in `refs/tags/*`, remote tracking refs in `refs/remotes/*`, and reflog entries). Git traverses backwards along parent commit pointers and descends into tree and blob objects. Any object in `.git/objects/` that cannot be reached through this graph traversal from any active ref is declared "unreachable" (dangling) and is eligible for pruning once its age exceeds the `gc.pruneExpire` threshold.
</details>

<details>
<summary><strong>4. Why does running `git gc --prune=now` fail to delete a commit you just abandoned via `git reset --hard HEAD~1`?</strong></summary>

Because Git maintains a **Reflog** (`.git/logs/`). When `git reset` moves `HEAD`, the reflog records the transition (`HEAD@{0}: reset: moving to HEAD~1`, `HEAD@{1}: commit: ...`). As long as an entry in the reflog references that commit, Git considers the commit **reachable** through the reflog reference tree. To delete it permanently, you must expire the reflog first (`git reflog expire --expire=now --all`) before running `git prune`.
</details>

<details>
<summary><strong>5. What is the role of the Packfile Index (`.idx`) file?</strong></summary>

The `.idx` file is a 256-bucket fan-out table and sorted binary lookup index for its corresponding `.pack` file. Given a 40-character SHA hash, Git reads the `.idx` file to determine whether the object exists within the packfile and, if so, the exact byte offset where the compressed object data begins within the `.pack` file. This prevents Git from having to sequentially scan multi-gigabyte packfiles to locate an object.
</details>

<details>
<summary><strong>6. What is the difference between `git prune` and `git gc`?</strong></summary>

`git gc` (Garbage Collection) is a high-level Porcelain command that orchestrates the entire repository optimization pipeline: it packs loose references (`git pack-refs`), packs loose objects into delta-compressed packfiles (`git pack-objects`), updates commit-graph caches, and finally invokes `git prune`. `git prune` is a low-level plumbing command whose sole responsibility is to scan the object database and delete unreachable loose objects older than the expiry window.
</details>

<details>
<summary><strong>7. How does the modern `git maintenance` command improve upon traditional `git gc`?</strong></summary>

Historically, `git gc` was triggered reactively during regular commands when loose object thresholds were exceeded (`gc.auto`), causing unexpected, frustrating pauses during developer workflows. `git maintenance` registers the repository into operating system background scheduling daemons (cron, systemd timers, or Windows Task Scheduler) to perform non-disruptive incremental optimizations (hourly loose object packing, daily commit-graph generation, and weekly pack repacking) during idle system time.
</details>

<details>
<summary><strong>8. What is a "Thin Pack" in Git network transfer protocols?</strong></summary>

When pushing or fetching, Git generates a "thin pack": a packfile containing deltas against base objects that are *not* included in the packfile itself because Git negotiated that the recipient already possesses those base objects. Once received, the receiving Git server or client "indexes" the thin pack by finding the local base objects and appending them to create a standard, fully self-contained packfile.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Inspecting Loose vs. Packed Object Counts</strong></summary>

**Task**: Query your repository to inspect the exact number of loose objects, packed objects, and total disk usage.

**Solution**:
```bash
mkdir /tmp/gc-lab && cd /tmp/gc-lab
git init
echo "Hello GC" > file.txt && git add file.txt && git commit -m "feat: init"

git count-objects -vH
# Displays count, size, in-pack, packs, and size-pack
```
</details>

<details>
<summary><strong>Exercise 2: Triggering Manual Garbage Collection</strong></summary>

**Task**: Create 5 loose commits, run `git gc`, and verify that loose objects have been reduced to 0 while packed objects increased.

**Solution**:
```bash
for i in {1..5}; do
  echo "line $i" >> file.txt
  git commit -am "Commit $i"
done

# Check count before GC
git count-objects -vH | grep "^count:"

# Run GC
git gc

# Check count after GC
git count-objects -vH | grep "^count:"
# Output: count: 0
```
</details>

<details>
<summary><strong>Exercise 3: Inspecting Packfile In-Pack Sizes with `git verify-pack`</strong></summary>

**Task**: Run `git verify-pack` on the newly generated packfile index and verify object types and delta depths.

**Solution**:
```bash
IDX=$(ls .git/objects/pack/*.idx)
git verify-pack -v $IDX | head -n 10
# Note the type, size, and in-pack-size columns
```
</details>

<details>
<summary><strong>Exercise 4: Finding and Permanently Pruning a Dangling Commit</strong></summary>

**Task**: Create a commit, hard-reset away from it, locate it with `git fsck`, expire the reflogs, and prune it from disk permanently.

**Solution**:
```bash
echo "Temporary Secret" > secret.txt
git add secret.txt && git commit -m "secret commit"
TARGET=$(git rev-parse HEAD)

git reset --hard HEAD~1

# Check fsck
git fsck --lost-found | grep "$TARGET"

# Expire reflog and prune
git reflog expire --expire=now --all
git prune --expire=now

# Verify object is gone
git cat-file -t $TARGET
# Output: fatal: Not a valid object name
```
</details>

<details>
<summary><strong>Exercise 5: Setting Up Background Git Maintenance</strong></summary>

**Task**: Register the repository with `git maintenance start` and verify the configured maintenance schedule.

**Solution**:
```bash
git maintenance start

# Check configuration
git config --get-regexp maintenance.*

# Run tasks immediately
git maintenance run --task=commit-graph
```
</details>

<details>
<summary><strong>Exercise 6: Finding the Largest Blob in a Repository</strong></summary>

**Task**: Write a shell pipeline using `git verify-pack` to output the single largest blob object in your repository.

**Solution**:
```bash
git verify-pack -v .git/objects/pack/*.idx \
  | grep blob \
  | sort -k3nr \
  | head -1
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Command | Description |
| :--- | :--- | :--- |
| **Run Standard GC** | `git gc` | Packs refs, consolidates loose objects, prunes >2wk unreachable |
| **Aggressive Repack** | `git gc --aggressive` | High-CPU deep delta window recalculation |
| **Force Immediate Prune**| `git gc --prune=now` | Deletes all unreachable objects without 2-week grace period |
| **Verify Packfile** | `git verify-pack -v <file.idx>`| Displays internal delta hierarchy and object sizes |
| **Object Statistics** | `git count-objects -vH` | Reports loose vs in-pack object counts and human-readable sizes |
| **Find Dangling Objects**| `git fsck --lost-found` | Identifies unreachable commits and blobs in object store |
| **Expire Reflogs** | `git reflog expire --expire=now --all` | Clears reflog history, unlinking dangling commits |
| **Background Maintenance**| `git maintenance start` | Hooks hourly/daily background maintenance into OS scheduler |

---

## 13. Further Reading & Official Documentation

- [Git Internals - Packfiles (Pro Git Book)](https://git-scm.com/book/en/v2/Git-Internals-Packfiles)
- [Official Git Documentation - git-gc](https://git-scm.com/docs/git-gc)
- [Official Git Documentation - git-maintenance](https://git-scm.com/docs/git-maintenance)
- [GitHub Engineering - Scaling Git's garbage collection](https://github.blog/2022-09-13-scaling-gits-garbage-collection/)

---

[<- 06-refs-branches-and-head.md](./06-refs-branches-and-head.md) | [Back to Module Overview](./README.md) | [08-the-index-in-depth.md ->](./08-the-index-in-depth.md)
