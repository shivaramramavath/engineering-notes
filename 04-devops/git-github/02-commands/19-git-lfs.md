<!--
================================================================================
Metadata:
  Title: Binary Asset Management: Git LFS (Large File Storage)
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 18-sparse-checkout-and-partial-clone.md
  Key Terminology: Git LFS, Pointer File, Smudge Filter, Clean Filter, git lfs track, .gitattributes, git lfs migrate, LFS Storage Quotas
================================================================================
-->

# Binary Asset Management: Git LFS (Large File Storage)

[<- 18-sparse-checkout-and-partial-clone.md](./18-sparse-checkout-and-partial-clone.md) | [Back to Module Overview](./README.md) | [20-hooks.md ->](./20-hooks.md)

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

Git is engineered from the ground up for source code: plain-text files that can be diffed line-by-line and compressed efficiently via delta compression.

However, modern software projects frequently require large binary assets: machine learning models (`.onnx`, `.pt`), video assets (`.mp4`), high-resolution graphics (`.psd`, `.png`), audio files, CAD files, or database seed dumps.

When large binary files are committed directly to standard Git:
- **Zero Delta Compression**: Git cannot diff binary files line-by-line. If a designer changes a 100MB Photoshop file 10 times, Git creates ten separate 100MB loose blobs, permanently bloating `.git/objects/` by **1 Gigabyte**!
- **Permanent Clone Penalty**: Because Git is distributed, every future developer who runs `git clone` is forced to download all 10 historical versions of that 100MB file over the network, even if they only need the latest release.
- **Repository Size Limits**: GitHub enforces a strict **100MB per-file limit** and blocks pushes containing files exceeding this threshold.

**Git Large File Storage (Git LFS)** solves this problem. It replaces large binary files inside the Git repository with tiny **130-byte text pointer files**, storing the actual multi-gigabyte payloads on external cloud object storage (such as AWS S3 or GitHub LFS servers), and downloading them lazily on-demand.

---

## 2. Core Concepts

### The Pointer File Architecture
When a file is tracked by Git LFS, the actual binary content is never committed into Git's object store. Instead:
- Git stores a lightweight, human-readable **Pointer File** (approximately 130 bytes):
```text
version https://git-lfs.github.com/spec/v1
oid sha256:4b825dc642cb6eb9a060e54bf8d69288fbee4904a1e2f7b82431945199651817
size 104857600
```
- The pointer records three fields:
  1. The LFS specification version.
  2. The cryptographic **SHA-256 OID** (Object Identifier) of the binary payload.
  3. The exact file size in bytes (e.g. 100MB).
- The actual 100MB payload is uploaded directly to the LFS cloud storage server over HTTPS via the Git LFS transfer agent.

```
+-----------------------------------------------------------------------------------+
|                            Git LFS Storage Architecture                           |
|                                                                                   |
|   GIT REPOSITORY (.git/objects)                  GIT LFS STORAGE SERVER (S3)      |
|   +------------------------------------+         +------------------------------+ |
|   | Git Tree                           |         | External Cloud Bucket        | |
|   |  ├── src/ (Code)                   |         |                              | |
|   |  └── assets/video.mp4 ────────┐    |         |                              | |
|   |       (130-byte pointer file) │    |         |                              | |
|   +-------------------------------│----+         |                              | |
|                                   │              |                              | |
|                                   ▼ (References) |  [ Physical 500MB Video ]    | |
|                 oid sha256:7a8b9c... ──────────► |   (Downloaded only on        | |
|                 size: 524288000                  |    active checkout!)         | |
|                                                  +------------------------------+ |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### The Smudge and Clean Filters in `.gitattributes`
Git LFS hooks seamlessly into Git's native stream processing using **Smudge** and **Clean** filters configured in `.gitattributes`:

```gitattributes
*.psd filter=lfs diff=lfs merge=lfs -text
*.mp4 filter=lfs diff=lfs merge=lfs -text
```

1. **The Clean Filter (On Commit / Staging)**:
   - When you run `git add video.mp4`, Git passes the raw 500MB stream through the LFS `clean` filter.
   - The filter calculates the SHA-256 hash, stores the raw binary locally in `.git/lfs/objects/`, and passes the tiny 130-byte pointer text to Git to be staged in `.git/index`.
2. **The Smudge Filter (On Checkout)**:
   - When you run `git checkout` or `git switch`, Git encounters the 130-byte pointer in the index and passes it through the LFS `smudge` filter.
   - The filter reads the OID hash, checks `.git/lfs/objects/` (or downloads it over HTTPS from the LFS server if absent), and streams the real 500MB binary file into your working directory!

```
STAGING (git add):
Working Tree (500MB) ──► [Clean Filter] ──► Staging Index (130-byte pointer)
                              │
                              ▼ Writes payload
                       .git/lfs/objects/

CHECKOUT (git switch):
Staging Index (Pointer) ──► [Smudge Filter] ──► Working Tree (500MB Binary)
                                   ▲
                                   │ Reads payload
                       .git/lfs/objects/ (or downloads from LFS remote)
```

---

## 4. Syntax & Usage

### 1. Installation & Tracking Configuration
```bash
# Initialize Git LFS on your workstation (run once per machine)
git lfs install

# Track specific binary file extensions with Git LFS
git lfs track "*.psd"
git lfs track "*.mp4"
git lfs track "*.onnx"

# Track an entire directory of assets
git lfs track "assets/models/**"

# Crucial: Commit the generated .gitattributes file!
git add .gitattributes
git commit -m "chore: configure git-lfs tracking for media and models"
```

---

### 2. Daily LFS Operations
```bash
# List all files currently tracked by Git LFS in the repository
git lfs ls-files

# Download all LFS payloads for the current checked-out commit
git lfs pull

# Fetch LFS objects without checking them out
git lfs fetch

# Prune old local LFS payloads from .git/lfs/objects/ to reclaim disk space
git lfs prune

# Check storage consumption and lock status
git lfs status
```

---

### 3. Migrating Existing Historical Binaries to LFS
If large binaries were previously committed into raw Git history, tracking them with `git lfs track` only affects *future* commits. To rewrite history and extract historical binaries into LFS:
```bash
# Analyze repository to see which file types consume the most space
git lfs migrate info

# Rewrite historical commits to convert past binaries to LFS pointers
git lfs migrate import --include="*.zip,*.tar.gz,*.psd" --everything
```

---

## 5. Practical Examples

### Example 1: Tracking and Committing a Machine Learning Model
Walk through tracking an ONNX model:

```bash
mkdir /tmp/lfs-lab && cd /tmp/lfs-lab
git init -b main

# 1. Install LFS hooks
git lfs install

# 2. Track .onnx files
git lfs track "*.onnx"

# 3. Inspect generated .gitattributes
cat .gitattributes
# Output: *.onnx filter=lfs diff=lfs merge=lfs -text

git add .gitattributes && git commit -m "chore: configure lfs for onnx models"

# 4. Create and commit a large mock model
head -c 10M </dev/urandom > model.onnx
git add model.onnx
git commit -m "feat(ml): add trained classification model"

# 5. Verify that Git stored ONLY a pointer!
git cat-file -p HEAD:model.onnx
```
Output:
```text
version https://git-lfs.github.com/spec/v1
oid sha256:8f1e2d4a1b...
size 10485760
```
*Notice that the commit object holds only a 3-line pointer, while the 10MB payload resides safely in `.git/lfs/objects/`!*

### Example 2: Inspecting Tracked Files with `git lfs ls-files`
```bash
git lfs ls-files
```
Output:
```text
8f1e2d4a1b * model.onnx
```
Displays the SHA-256 OID, an asterisk (indicating checked-out locally), and the relative file path!

### Example 3: Reclaiming Local Disk Space with `git lfs prune`
Developers who checkout multiple branches can accumulate tens of gigabytes of cached LFS binaries in `.git/lfs/objects/`:

```bash
# Verify what can be safely deleted:
git lfs prune --dry-run
# Output: 5 local objects, 0 retained, 5 prunable (52.4 MB)

# Prune old unreferenced binaries (preserves active checked-out files!):
git lfs prune
```

### Example 4: Cloning Without Downloading Huge Binaries
If a developer on a slow mobile connection needs to review code without downloading 20GB of video assets:

```bash
# Disable automatic smudge filter during clone
GIT_LFS_SKIP_SMUDGE=1 git clone <url> app
cd app

# Working tree has tiny 130-byte pointer files on disk instead of 20GB binaries!
# When ready to download a specific asset later:
git lfs pull --include="assets/logo.png"
```

---

## 6. Best Practices

1. **Always Commit `.gitattributes` Immediately**: When adding a new file pattern via `git lfs track`, commit `.gitattributes` before committing any matching files to ensure the clean filter activates.
2. **Never Commit Files Over 100MB to Raw Git**: GitHub hard-rejects pushes containing individual files larger than 100MB. Always track them with Git LFS.
3. **Use `GIT_LFS_SKIP_SMUDGE=1` in CI Code-Only Jobs**: If a CI job only runs linters or unit tests on backend code, skip downloading gigabytes of LFS assets to accelerate build times.
4. **Prune Regularly on Developer Laptops**: Run `git lfs prune` periodically to clear out historical LFS binaries from `.git/lfs/objects/`.
5. **Use Git LFS File Locking in Multi-Designer Teams**: Binary files (like `.psd` or `.blend`) cannot be merged. Use `git lfs lock <file>` to prevent colleagues from concurrently editing the same binary asset.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `remote: error: GH001: Large files detected. File ... is 120.00 MB` | File was committed to raw Git before `git lfs track` was configured. | Untrack file, reset commit, track via `git lfs track`, and re-commit. |
| Working tree has plain text pointer file instead of real binary | Cloned without Git LFS installed or `smudge` filter skipped. | Run `git lfs install` and `git lfs pull`. |
| Repository clone size on disk is 15GB even though LFS is enabled | Large binaries were committed in past commits before migrating to LFS. | Run `git lfs migrate import` to extract historical binaries into LFS pointers. |
| GitHub bill shows unexpected overage charges | LFS bandwidth quota exceeded (downloading large binaries on every CI run). | Cache LFS objects in CI runners or use `GIT_LFS_SKIP_SMUDGE=1`. |
| `batch response: Authorization error` during push | LFS storage requires write permissions to the repository. | Re-authenticate via Personal Access Token with full `repo` permissions. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Sub-Second Clones**: Moving 20GB of assets into LFS reduces the core Git repository clone from 30 minutes to **under 10 seconds**.
- **Parallel Chunked Transfers**: The Git LFS transfer agent uploads and downloads binary assets using multi-threaded HTTP/2 parallel chunk streams.

### Cost & Quotas
- **GitHub LFS Billing**: GitHub provides **1GB of free storage and 1GB of free monthly bandwidth**. Exceeding this requires purchasing dedicated data packs ($5/month per 50GB storage + 50GB bandwidth).
- **CI/CD Bandwidth Drainage**: If 10 CI runners clone a 500MB LFS asset on every pull request, bandwidth consumption reaches **5GB per PR**, rapidly exhausting monthly quotas.

---

## 9. Comparison Table

| Attribute | Standard Raw Git | Git LFS (Large File Storage) |
| :--- | :--- | :--- |
| **Storage Location** | `.git/objects/` (Permanent) | External Cloud Bucket (S3/LFS Server) |
| **Storage in Commit Tree**| Full binary payload | 130-byte text pointer file |
| **Clone Behavior** | Downloads ALL historical versions| Downloads ONLY the active commit version |
| **Max File Size** | 100MB (GitHub limit) | Up to 2 Terabytes per file |
| **Delta Compression** | Ineffective on binaries | Not applicable (stored per-OID) |
| **Mergeability** | Unmergeable (conflict guaranteed)| Unmergeable (supports file locking) |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What problem does Git LFS solve and how does its architecture operate?</strong></summary>

Standard Git is designed for text. When large binary assets (videos, ML models, graphics) are committed, every minor modification creates a full-size uncompressed blob in `.git/objects/`, permanently bloating clone sizes for all contributors. Git LFS solves this by decoupling metadata from payload:
1. Inside the Git repository, Git stores only a tiny **130-byte text pointer file** containing the file's SHA-256 hash and size.
2. The actual large binary payload is uploaded to an external LFS storage server.
3. Git's native Smudge and Clean filters in `.gitattributes` automatically replace binaries with pointers on commit, and download real binaries on checkout.
</details>

<details>
<summary><strong>2. What is inside a Git LFS pointer file?</strong></summary>

A Git LFS pointer is a plain text file containing three lines:
```text
version https://git-lfs.github.com/spec/v1
oid sha256:<64-character-hex-hash>
size <size-in-bytes>
```
It records the LFS spec URL, the SHA-256 cryptographic hash of the raw binary payload, and the exact size in bytes.
</details>

<details>
<summary><strong>3. How do the Smudge and Clean filters in `.gitattributes` interact with Git LFS?</strong></summary>

- **Clean Filter (`filter=lfs` on `git add`)**: Intercepts the raw binary file from the working tree, calculates its SHA-256 hash, writes the binary to the local LFS cache (`.git/lfs/objects/`), and outputs the 130-byte pointer file into the Git staging index.
- **Smudge Filter (`filter=lfs` on `git checkout`)**: Intercepts the 130-byte pointer file from the index, reads the SHA-256 hash, retrieves the binary payload from `.git/lfs/objects/` (or downloads it over HTTPS from the LFS server), and streams the real binary file into the working tree.
</details>

<details>
<summary><strong>4. Why doesn't running `git lfs track "*.psd"` fix a repository that is already bloated by past Photoshop commits?</strong></summary>

`git lfs track` only configures `.gitattributes` to intercept **future** commits. It does not retroactively rewrite historical commits. The large historical binary blobs still exist inside `.git/objects/` across past commit snapshots. To extract historical binaries from past commits and replace them with LFS pointers, the repository history must be rewritten using `git lfs migrate import`.
</details>

<details>
<summary><strong>5. How can CI/CD pipelines optimize bandwidth and build time when using Git LFS?</strong></summary>

By setting `GIT_LFS_SKIP_SMUDGE=1` during repository checkout. If the CI job only runs linting, unit tests, or backend compilations that do not require multi-gigabyte media or ML model files, skipping the LFS smudge filter avoids downloading gigabytes of binary assets, reducing build times from minutes to seconds and saving cloud bandwidth costs.
</details>

<details>
<summary><strong>6. What is Git LFS File Locking (`git lfs lock`) and why is it used?</strong></summary>

Because binary assets (like 3D models or Photoshop files) cannot be merged using standard text diff tools, concurrent edits by two developers result in unresolvable merge conflicts where one person's work must be discarded. `git lfs lock <file>` reserves exclusive write access to a file on the remote server. Git enforces that other team members cannot push modifications to that file until the lock is released.
</details>

<details>
<summary><strong>7. What does `git lfs prune` do?</strong></summary>

`git lfs prune` scans `.git/lfs/objects/` and deletes cached binary files that are no longer referenced by the currently checked-out commit or recent history. It reclaims local disk space on developer laptops without deleting files from the remote LFS server.
</details>

<details>
<summary><strong>8. What happens if a developer clones a Git LFS repository on a machine that does not have Git LFS installed?</strong></summary>

The clone succeeds, but Git does not recognize the LFS smudge filter. As a result, the developer sees the raw 130-byte text pointer files on disk instead of the actual binary files. Applications attempting to read the file (e.g. an image viewer or ML runtime) fail with format corruption errors.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Initializing and Tracking Files with Git LFS</strong></summary>

**Task**: In a test repository, initialize Git LFS, track `*.bin` files, and inspect the resulting `.gitattributes` file.

**Solution**:
```bash
mkdir /tmp/lfs-exercise && cd /tmp/lfs-exercise
git init -b main
git lfs install

# Track .bin files
git lfs track "*.bin"

cat .gitattributes
# Output: *.bin filter=lfs diff=lfs merge=lfs -text
git add .gitattributes && git commit -m "chore: setup lfs tracking"
```
</details>

<details>
<summary><strong>Exercise 2: Verifying Pointer File Generation</strong></summary>

**Task**: Create a 5MB binary file, stage and commit it, and use `git cat-file` to prove that Git committed a 130-byte pointer.

**Solution**:
```bash
head -c 5M </dev/urandom > data.bin
git add data.bin
git commit -m "feat: add binary payload"

# View raw committed object in Git index
git cat-file -p HEAD:data.bin
# Notice: Displays 3-line pointer with version, oid sha256, and size!
```
</details>

<details>
<summary><strong>Exercise 3: Inspecting Local LFS Objects Storage</strong></summary>

**Task**: Inspect `.git/lfs/objects/` to verify that the real 5MB binary payload is stored in the local LFS cache.

**Solution**:
```bash
ls -lh .git/lfs/objects/*/*
# Shows the physical 5MB payload stored under its SHA-256 hash directory!
```
</details>

<details>
<summary><strong>Exercise 4: Listing Tracked Assets with `git lfs ls-files`</strong></summary>

**Task**: Run `git lfs ls-files` to inspect tracked files and their OID hashes.

**Solution**:
```bash
git lfs ls-files
# Output: <sha256> * data.bin
```
</details>

<details>
<summary><strong>Exercise 5: Testing `GIT_LFS_SKIP_SMUDGE=1`</strong></summary>

**Task**: Clone the test repository into a new folder with smudge disabled, and inspect `data.bin` to confirm it is a pointer file.

**Solution**:
```bash
cd /tmp
GIT_LFS_SKIP_SMUDGE=1 git clone /tmp/lfs-exercise /tmp/lfs-skip-test
cd /tmp/lfs-skip-test

# Check size of data.bin on disk:
ls -lh data.bin
# Size is ~130 bytes (pointer file!), not 5MB!
```
</details>

<details>
<summary><strong>Exercise 6: Pulling LFS Assets on Demand</strong></summary>

**Task**: In the skipped clone from Exercise 5, run `git lfs pull` and verify that the real 5MB payload is downloaded.

**Solution**:
```bash
git lfs pull
ls -lh data.bin
# Size is now 5.0MB! Physical binary downloaded on-demand!
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Command | Usage | Role |
| :--- | :--- | :--- |
| `git lfs install` | System Setup | Registers LFS global filters with Git engine |
| `git lfs track "<pattern>"` | Config | Configures `.gitattributes` to route file type to LFS |
| `git lfs ls-files` | CLI | Lists all files currently tracked by Git LFS |
| `git lfs status` | CLI | Displays staged and unstaged LFS objects |
| `git lfs pull` | Network | Downloads LFS payloads for the active commit |
| `git lfs fetch` | Network | Downloads LFS objects without materializing on disk |
| `git lfs prune` | Maintenance | Deletes unreferenced cached binaries from `.git/lfs/` |
| `GIT_LFS_SKIP_SMUDGE=1 git clone`| CI / Quick Clone | Clones repo without downloading large LFS payloads |
| `git lfs migrate import` | History Rewrite | Converts historical raw Git binaries to LFS pointers |

---

## 13. Further Reading & Official Documentation

- [Official Git LFS Documentation](https://git-lfs.com/)
- [GitHub Docs - About Git Large File Storage](https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-git-large-file-storage)
- [Git LFS Specification](https://github.com/git-lfs/git-lfs/blob/main/docs/spec.md)
- [Managing Billing for Git Large File Storage](https://docs.github.com/en/billing/managing-billing-for-git-large-file-storage)

---

[<- 18-sparse-checkout-and-partial-clone.md](./18-sparse-checkout-and-partial-clone.md) | [Back to Module Overview](./README.md) | [20-hooks.md ->](./20-hooks.md)
