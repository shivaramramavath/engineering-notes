<!--
================================================================================
Metadata:
  Title: Monorepo Scaling: Sparse-Checkout, Partial Clones & Scalar
  Track: Git & GitHub / DevOps
  Difficulty: Advanced
  Estimated Time: 55 minutes
  Prerequisites: 17-worktrees.md
  Key Terminology: Sparse-Checkout, Cone Mode, Partial Clone (--filter=blob:none), Promisor Remote, Scalar, skip-worktree, Monorepo Performance
================================================================================
-->

# Monorepo Scaling: Sparse-Checkout, Partial Clones & Scalar

[<- 17-worktrees.md](./17-worktrees.md) | [Back to Module Overview](./README.md) | [19-git-lfs.md ->](./19-git-lfs.md)

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

In enterprise engineering, the **Monorepo** model—housing hundreds of microservices, frontend applications, shared libraries, and documentation inside a single Git repository—has become the standard architecture at companies like Google, Meta, Microsoft, and Uber.

However, as a monorepo grows to **100GB+ in size**, containing **1,000,000+ files** and **10+ years of history**, standard Git breaks down:
- `git clone` takes 45 minutes to download tens of gigabytes of historical data.
- Checking out 1,000,000 physical files onto a developer's NVMe drive exhausts filesystem inodes and crashes IDEs.
- `git status` takes 15 seconds to stat every file on disk.

To scale Git to massive monorepos, Git introduced a two-pronged solution:
1. **Partial Clones (`--filter`)**: Solves the *object storage* bottleneck. Instead of downloading all historical file contents, Git downloads only commit and tree structures, fetching file blobs dynamically on-demand.
2. **Sparse-Checkout (`git sparse-checkout`)**: Solves the *working tree* bottleneck. Instead of checking out 1,000,000 files onto disk, Git materializes *only the specific subdirectories* you are actively developing (e.g. `apps/billing/`), leaving the rest virtualized!

Combining Partial Clones with Sparse-Checkout enables developers to clone and work in a 100GB enterprise monorepo in **under 30 seconds**.

---

## 2. Core Concepts

### The Two Dimensions of Monorepo Bloat

```
                       REPOSITORY SCALING CHALLENGES
                                     │
         ┌───────────────────────────┴───────────────────────────┐
         ▼                                                       ▼
   OBJECT STORE BLOAT                                  WORKING TREE BLOAT
(Gigabytes of historical data)                      (Millions of files on disk)
         │                                                       │
         ▼ SOLUTION:                                             ▼ SOLUTION:
   PARTIAL CLONES                                      SPARSE-CHECKOUT
   git clone --filter=blob:none                        git sparse-checkout set <dir>
   (Omits file blobs until accessed)                   (Checks out ONLY your sub-project)
```

### Partial Clones: Blobless vs. Treeless
- **Blobless Clone (`--filter=blob:none`)**:
  - Downloads the complete commit DAG and all directory tree structures since project inception.
  - Omits historical file contents (blobs).
  - *Developer Experience*: `git log`, `git checkout`, `git merge`, and `git blame` work immediately. When you open or edit a file, Git transparently downloads its blob over the network via a **Promisor Remote**. (Recommended standard!).
- **Treeless Clone (`--filter=tree:0`)**:
  - Downloads only commit objects; omits both trees and blobs.
  - Minimal initial download, but requires network calls whenever navigating between branches. (Ideal for automated CI build runners).

### Sparse-Checkout: "Cone Mode"
Older Git versions allowed arbitrary regex patterns in sparse-checkouts, which caused severe performance degradation ($O(N \cdot M)$ pattern matching).
Modern Git defaults to **Cone Mode** (`--cone`):
- Restricts patterns to directory paths (e.g. `packages/ui-components/`).
- Operates at $O(1)$ hash-table speeds, allowing Git to skip traversing millions of unselected directories in the index.

---

## 3. How It Works Internally

### The Promisor Remote
When a repository is cloned with `--filter`, Git marks the remote as a **Promisor Remote** in `.git/config`:
```ini
[remote "origin"]
    promisor = true
    partialclonefilter = blob:none
```
When a local command (like `git diff` or `git checkout`) encounters a blob hash that does not physically exist in `.git/objects/`, Git does not throw an error. Instead, the promisor subsystem automatically initiates an on-demand batch fetch over the network to download the missing blob directly from the promisor remote!

### The `--skip-worktree` Bit in `.git/index`
How does sparse-checkout prevent Git from assuming un-checked-out files were deleted?
- In `.git/index`, every file entry contains a 16-bit flags field.
- For all files outside your active sparse-checkout cone, Git sets the **`skip-worktree` bit**.
- When `git status` runs, Git sees the `skip-worktree` bit and **skips checking disk entirely**. It does not expect the file to exist on the filesystem, and will not report it as deleted!

```
+---------------------------------------------------------------------------------+
|                       Sparse-Checkout Index Evaluation                          |
|                                                                                 |
|  .git/index ENTRY                 skip-worktree bit?    Action in "git status"  |
|  -----------------------------------------------------------------------------  |
|  apps/billing/index.ts            FALSE (In cone)       Checks disk stats       |
|  apps/inventory/server.ts         TRUE  (Out of cone)   SKIPPED completely!     |
|  services/ml-pipeline/model.py    TRUE  (Out of cone)   SKIPPED completely!     |
+---------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 1. Partial Clone Commands
```bash
# Clone a repository omitting historical file blobs (Recommended for developers!)
git clone --filter=blob:none <url>

# Clone a repository omitting both trees and blobs (Fastest for CI runners)
git clone --filter=tree:0 <url>

# Clone omitting blobs larger than 10 Megabytes
git clone --filter=blob:limit=10M <url>
```

---

### 2. Sparse-Checkout Commands (Cone Mode)
```bash
# Initialize sparse-checkout in cone mode (checks out only root files initially)
git sparse-checkout init --cone

# Specify the exact subdirectories to materialize in your workspace
git sparse-checkout set apps/frontend libs/shared-ui

# Add an additional directory to your existing cone
git sparse-checkout add services/auth-api

# List all directories currently configured in your active sparse-checkout cone
git sparse-checkout list

# Reapply sparse-checkout filters across the working tree
git sparse-checkout reapply

# Disable sparse-checkout and materialize all files in the repository
git sparse-checkout disable
```

---

### 3. Microsoft Scalar (The Monorepo Power Tool)
Git includes **Scalar**, an enterprise acceleration suite that automatically configures partial clones, sparse-checkout, fsmonitor, and background maintenance:
```bash
# Clone massive monorepo with all enterprise performance features enabled
scalar clone --src <url> <directory>

# Register an existing repository into Scalar management
scalar register
```

---

## 5. Practical Examples

### Example 1: Sub-Minute Onboarding in a Giant Monorepo
Simulate a developer joining a company with a massive monorepo, needing to work *only* on the billing service:

```bash
# 1. Perform blobless partial clone without checking out files immediately (-n)
git clone --filter=blob:none --no-checkout https://github.com/enterprise/mega-monorepo.git app
cd app

# 2. Initialize sparse-checkout in cone mode
git sparse-checkout init --cone

# 3. Restrict working tree strictly to the billing service and shared types
git sparse-checkout set apps/billing packages/common-types

# 4. Check out the main branch!
git checkout main

# Inspect filesystem:
ls apps/
# Output: billing (Only apps/billing is on disk! All other 50 apps are virtualized!)
```

### Example 2: Inspecting the Active Sparse Cone
Check what directories are currently selected:

```bash
git sparse-checkout list
```
Output:
```text
apps/billing
packages/common-types
```

### Example 3: Expanding the Cone Dynamically
Your billing task now requires inspecting the auth service:

```bash
# Dynamically add auth service to working tree
git sparse-checkout add services/auth-service

# Inspect directory:
ls services/
# Output: auth-service immediately materialized onto disk!
```

### Example 4: Verifying the Skip-Worktree Bit with `git ls-files -v`
Inspect how Git tracks virtualized files in the index:

```bash
# List index entries with flags
git ls-files -v | head -n 10
```
Output:
```text
H apps/billing/index.ts         <-- 'H' indicates cached and checked out in worktree
S services/ml-model/train.py     <-- 'S' indicates SKIP-WORKTREE (virtualized!)
S services/data-lake/etl.py      <-- 'S' indicates SKIP-WORKTREE (virtualized!)
```

### Example 5: Benchmarking Status Speed
In a repository with 500,000 files, observe the dramatic performance improvement:
- Full checkout `git status`: ~12.5 seconds (scanning 500,000 files).
- Sparse checkout (1 service) `git status`: **0.04 seconds** (scanning only 250 files).

---

## 6. Best Practices

1. **Always Use Cone Mode (`--cone`)**: Avoid legacy regex-based sparse-checkout patterns. Cone mode provides $O(1)$ performance and prevents pattern syntax mistakes.
2. **Standardize on `--filter=blob:none`**: Blobless clones provide the ideal balance: full commit history and branch visualization locally, with zero bloat from old unused binaries.
3. **Combine Partial Clones with Sparse-Checkout**: Never use sparse-checkout alone on a giant repository; pairing it with partial clone eliminates both network and filesystem bottlenecks.
4. **Use Scalar on Windows for Ultra-Scale Codebases**: Scalar automatically tunes `core.fsmonitor`, `core.untrackedCache`, commit-graphs, and background pre-fetching.
5. **Always Set `push.autoSetupRemote` with Sparse Clones**: Avoid unnecessary tracking prompts when pushing feature branches from sparse workspaces.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `fatal: pathspec '...' did not match any files` | Attempted to navigate to or edit a file that is outside your active sparse cone. | Add the target directory to your cone: `git sparse-checkout add <dir>`. |
| Git commands freeze while navigating commits | Git is downloading missing blobs over a slow network connection. | Normal for partial clones during initial access; blobs are cached locally once fetched. |
| `git status` reports thousands of deleted files after editing config | Manually modified `.git/info/sparse-checkout` with invalid syntax. | Run `git sparse-checkout reapply` or `git sparse-checkout init --cone`. |
| Cannot push from a partial clone | Mismatched push permissions or server lacks partial clone support. | Ensure remote forge supports promisor remotes (GitHub/GitLab fully support it). |
| Workspace contains all files after cloning with `--filter=blob:none` | Partial clone filters *object downloads*; it does not restrict the working tree without sparse-checkout! | Run `git sparse-checkout set <dir>` to restrict working tree files. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Bandwidth Reduction**: On a 50GB repository, a blobless clone downloads **~150MB to 300MB** of commit and tree data, saving **99% of initial network transfer time**.
- **Filesystem Efficiency**: Reducing checked-out files from 1,000,000 to 2,000 eliminates IDE index freezes and reduces OS disk I/O overhead to zero.

### Cost
- **CI/CD Cloud Ingress/Egress Savings**: CI build runners using `--filter=tree:0` save petabytes of data transfer costs across large enterprise engineering fleets.

---

## 9. Comparison Table

| Scaling Technology | Solves Which Bottleneck? | What is Stored Locally? | What is On Disk? |
| :--- | :--- | :--- | :--- |
| **Standard Clone** | None | 100% of History & Blobs | 100% of Checked-out Files |
| **Shallow Clone (`--depth`)**| Network Bandwidth | Only Last $N$ Commits | 100% of Latest Files |
| **Blobless Clone (`--filter`)**| Object Database Storage | Full DAG + Trees (Blobs on-demand)| 100% of Active Commit Files |
| **Sparse-Checkout** | Working Tree Inode/File Count| Full Object Database | ONLY Selected Subdirectories |
| **Partial + Sparse (Combined)**| BOTH Storage & Working Tree | Full DAG + Trees (Blobs on-demand)| ONLY Selected Subdirectories |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain the difference between a shallow clone (`--depth`) and a blobless partial clone (`--filter=blob:none`).</strong></summary>

- **Shallow Clone (`--depth N`)**: Truncates repository history horizontally. Git downloads only the last $N$ commits, creating an artificial shallow boundary. Historical commits before that boundary do not exist locally, breaking operations like `git log`, historical `git blame`, and multi-branch rebasing.
- **Blobless Partial Clone (`--filter=blob:none`)**: Filters history vertically. Git downloads the **entire commit DAG** and all directory trees since project inception, but omits file content blobs. Historical commands like `git log` and `git merge-base` work offline instantly, and Git downloads specific file blobs on-demand via promisor remotes only when checked out or diffed.
</details>

<details>
<summary><strong>2. How does Git's "Cone Mode" in sparse-checkout improve upon legacy pattern-based sparse checkout?</strong></summary>

Legacy sparse-checkout allowed arbitrary `.gitignore`-style glob and regex patterns across the entire repository. For every single file in the index, Git had to run complex regex evaluations, resulting in $O(N \cdot M)$ computational complexity that degraded status checks on large repositories. **Cone Mode** (`--cone`) restricts patterns to explicit directory prefixes. Git evaluates directory paths using $O(1)$ hash-table lookups, dramatically accelerating checkout and status operations across millions of files.
</details>

<details>
<summary><strong>3. What is a "Promisor Remote" in Git?</strong></summary>

A Promisor Remote is a remote repository configuration that promises to deliver missing objects on-demand. When a repository is initialized as a partial clone (`--filter`), Git marks `origin` as a promisor remote. If a local command (like `git checkout` or `git diff`) requires a blob that is absent from `.git/objects/`, the promisor subsystem automatically sends an on-demand batch request to the remote server to fetch and cache the required blob transparently.
</details>

<details>
<summary><strong>4. How does the `skip-worktree` bit in `.git/index` prevent Git from reporting un-checked-out sparse files as deleted?</strong></summary>

When a directory is excluded from a sparse-checkout cone, Git sets the `skip-worktree` bit in the file's index entry in `.git/index`. When `git status` or `git add` executes, Git checks this bit; if set, Git assumes the file matches the index and skips checking the physical filesystem. This prevents Git from reporting the absent file as a deletion and ensures un-checked-out files remain clean.
</details>

<details>
<summary><strong>5. What is Microsoft Scalar and how does it integrate with modern Git?</strong></summary>

Scalar is an enterprise-scale monorepo acceleration tool developed by Microsoft and contributed directly to Git core (Git 2.38+). When an engineer runs `scalar clone`, Scalar automatically configures and manages optimal performance settings: partial blobless clone, cone-mode sparse checkout, background `git maintenance` daemons, `core.fsmonitor`, `core.untrackedCache`, and commit-graph generation.
</details>

<details>
<summary><strong>6. What is a treeless partial clone (`--filter=tree:0`) and when should it be preferred over blobless?</strong></summary>

A treeless clone omits both file blobs *and* historical directory tree objects, downloading only the commit objects. It produces the absolute smallest possible initial clone size (often <10MB for giant repos). However, navigating between commits or running `git checkout` requires downloading missing trees from the remote. It is ideal for **automated CI/CD build runners** that check out a single commit, build, and terminate.
</details>

<details>
<summary><strong>7. How do you re-expand a sparse-checkout workspace to materialize the entire repository?</strong></summary>

By executing:
```bash
git sparse-checkout disable
```
Git clears all `skip-worktree` bits in `.git/index` and materializes all files in the repository onto the physical filesystem.
</details>

<details>
<summary><strong>8. Can an engineer commit and push changes while working inside a sparse-checkout cone?</strong></summary>

Yes, absolutely. Git tracks modifications normally within the active cone. When you commit, Git writes new tree objects that combine your modified subdirectories with the untouched, virtualized subdirectories from the index, and pushes the commit cleanly to the remote server.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Setting Up a Cone-Mode Sparse-Checkout</strong></summary>

**Task**: In a test repository with multiple directories (`apps/web`, `apps/api`, `libs/common`), configure sparse checkout to materialize only `apps/web`.

**Solution**:
```bash
mkdir -p /tmp/sparse-lab/apps/web /tmp/sparse-lab/apps/api /tmp/sparse-lab/libs/common
cd /tmp/sparse-lab
git init -b main
echo "web" > apps/web/page.html
echo "api" > apps/api/server.js
echo "common" > libs/common/util.js
git add . && git commit -m "init monorepo"

# Initialize sparse-checkout in cone mode
git sparse-checkout init --cone

# Set cone to apps/web only
git sparse-checkout set apps/web

# Verify filesystem
ls apps/
# Shows ONLY web! apps/api and libs/ are gone from disk!
```
</details>

<details>
<summary><strong>Exercise 2: Inspecting the Active Cone with `git sparse-checkout list`</strong></summary>

**Task**: Query the repository to inspect which directories are currently in the active cone.

**Solution**:
```bash
git sparse-checkout list
# Output: apps/web
```
</details>

<details>
<summary><strong>Exercise 3: Dynamically Adding Directories to the Cone</strong></summary>

**Task**: Add `libs/common` to your existing cone and verify that its files immediately materialize on disk.

**Solution**:
```bash
git sparse-checkout add libs/common

ls libs/
# Output: common/ is now physically present on disk!
```
</details>

<details>
<summary><strong>Exercise 4: Inspecting `skip-worktree` Flags in the Index</strong></summary>

**Task**: Run `git ls-files -v` to observe the `H` (checked out) vs `S` (skip-worktree) flags.

**Solution**:
```bash
git ls-files -v
# Shows:
# H apps/web/page.html
# S apps/api/server.js
# H libs/common/util.js
```
</details>

<details>
<summary><strong>Exercise 5: Disabling Sparse-Checkout to Restore Full Tree</strong></summary>

**Task**: Disable sparse checkout and confirm that all files in `apps/api` reappear on disk.

**Solution**:
```bash
git sparse-checkout disable

ls apps/
# Shows BOTH api and web! Full repository is restored!
```
</details>

<details>
<summary><strong>Exercise 6: Simulating a Blobless Partial Clone</strong></summary>

**Task**: Clone a local repository using `--filter=blob:none` and verify promisor remote configuration in `.git/config`.

**Solution**:
```bash
git clone --filter=blob:none file:///tmp/sparse-lab /tmp/blobless-clone

cd /tmp/blobless-clone
git config --local --get remote.origin.promisor
# Output: true
git config --local --get remote.origin.partialclonefilter
# Output: blob:none
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Command | Usage | Role |
| :--- | :--- | :--- |
| `git clone --filter=blob:none <url>` | Client Clone | Blobless clone; downloads history, fetches blobs on demand |
| `git clone --filter=tree:0 <url>` | CI Runner | Treeless clone; absolute minimal initial download |
| `git sparse-checkout init --cone` | Workspace | Enables modern high-speed cone-mode sparse checkout |
| `git sparse-checkout set <dir...>` | Workspace | Sets active working tree to specified directories |
| `git sparse-checkout add <dir...>` | Workspace | Appends additional directory to active cone |
| `git sparse-checkout list` | CLI | Lists directories currently included in cone |
| `git sparse-checkout reapply` | Maintenance | Re-applies sparse filters across working tree |
| `git sparse-checkout disable` | Reset | Restores full repository checkout across all files |
| `scalar clone <url>` | Monorepo Tool | Clones with automated enterprise scaling optimizations |

---

## 13. Further Reading & Official Documentation

- [Official Git Documentation - git-sparse-checkout](https://git-scm.com/docs/git-sparse-checkout)
- [GitHub Blog - Bring your monorepo down to size with sparse-checkout](https://github.blog/2020-01-17-bring-your-monorepo-down-to-size-with-sparse-checkout/)
- [Microsoft Scalar Documentation](https://github.com/microsoft/scalar)
- [Git Documentation - Partial Clones](https://git-scm.com/docs/partial-clone)

---

[<- 17-worktrees.md](./17-worktrees.md) | [Back to Module Overview](./README.md) | [19-git-lfs.md ->](./19-git-lfs.md)
