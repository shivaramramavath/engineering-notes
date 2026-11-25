<!--
================================================================================
Metadata:
  Title: End-to-End Git Internals Walkthrough (Plumbing from Scratch)
  Track: Git & GitHub / DevOps
  Difficulty: Advanced
  Estimated Time: 60 minutes
  Prerequisites: 09-hashes-and-sha-256.md
  Key Terminology: Plumbing vs Porcelain, git hash-object, git update-index, git write-tree, git commit-tree, git update-ref, git read-tree, Merkle DAG
================================================================================
-->

# End-to-End Git Internals Walkthrough (Plumbing from Scratch)

[<- 09-hashes-and-sha-256.md](./09-hashes-and-sha-256.md) | [Back to Module Overview](./README.md) | [Next Module: 02-commands ->](../02-commands/README.md)

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

In software engineering, true mastery of a system comes from understanding its primitives.
- High-level Git commands like `git add`, `git commit`, `git checkout`, `git branch`, and `git merge` are known as **Porcelain commands**. They provide a user-friendly, abstracted interface for everyday developer workflows.
- Underneath the porcelain sits a suite of low-level, atomic commands called **Plumbing commands** (`git hash-object`, `git update-index`, `git write-tree`, `git commit-tree`, `git update-ref`, `git read-tree`).

In this capstone walkthrough for Module 01, we demystify Git completely. We will construct a fully functional Git repository—creating multiple files, nested directories, a multi-commit history, branches, and merges—**using ONLY low-level plumbing commands**. By removing the porcelain abstraction, you will see exactly how blobs, trees, commits, refs, and the index interact at the byte and pointer level.

---

## 2. Core Concepts

### Porcelain vs. Plumbing Commands
- **Porcelain**: High-level user interface designed for humans. Handles safety checks, interactive prompts, and complex multi-step workflows automatically (e.g. `git commit` stages the index, writes a tree, creates a commit object, updates the branch ref, and records the reflog).
- **Plumbing**: Low-level Unix-style utilities designed for scripting, automation, and internal Git engine execution. Each plumbing command performs exactly one atomic task.

```
+-------------------------------------------------------------------------------+
|                        Porcelain vs. Plumbing Mapping                         |
|                                                                               |
|  USER PORCELAIN COMMAND             UNDERLYING PLUMBING ENGINE                |
|  +---------------------+            +---------------------------------------+ |
|  | git add <file>      | ─────────► | git hash-object -w (Writes blob)      | |
|  |                     |            | git update-index (Updates .git/index) | |
|  +---------------------+            +---------------------------------------+ |
|  +---------------------+            +---------------------------------------+ |
|  | git commit -m "msg" | ─────────► | git write-tree (Creates Tree object)  | |
|  |                     |            | git commit-tree (Creates Commit obj)  | |
|  |                     |            | git update-ref (Advances branch ref)  | |
|  +---------------------+            +---------------------------------------+ |
|  +---------------------+            +---------------------------------------+ |
|  | git branch <name>   | ─────────► | git update-ref refs/heads/<name> HEAD | |
|  +---------------------+            +---------------------------------------+ |
|  +---------------------+            +---------------------------------------+ |
|  | git checkout <name> | ─────────► | git symbolic-ref HEAD refs/heads/...  | |
|  |                     |            | git read-tree -u -m HEAD (Updates WT) | |
|  +---------------------+            +---------------------------------------+ |
+-------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### The Complete Bottom-Up Object Construction Pipeline
When building a commit from scratch without porcelain:
1. **Raw Bytes $\to$ Blobs**:
   - `git hash-object -w <file>` writes file content into `.git/objects/`.
2. **Blobs $\to$ Index Entries**:
   - `git update-index --add --cacheinfo <mode> <sha> <path>` records the blob and path into `.git/index`.
3. **Index Entries $\to$ Tree Objects**:
   - `git write-tree` reads `.git/index`, generates necessary subdirectory trees, writes them to `.git/objects/`, and returns the root tree hash.
4. **Tree Objects $\to$ Commit Objects**:
   - `git commit-tree <tree-sha> -p <parent-sha> -m "msg"` writes a new commit object pointing to the root tree and parent.
5. **Commit Objects $\to$ References**:
   - `git update-ref refs/heads/main <commit-sha>` points the branch to the new commit.

```
1. File Data ("Hello World")
       │
       ▼ git hash-object -w
2. Blob Object (Hash: 557db0...)
       │
       ▼ git update-index --add --cacheinfo 100644 557db0... index.js
3. Staged in .git/index
       │
       ▼ git write-tree
4. Tree Object (Hash: e39290...)
       │
       ▼ git commit-tree e39290... -m "feat: initial commit"
5. Commit Object (Hash: 4b825d...)
       │
       ▼ git update-ref refs/heads/main 4b825d...
6. Branch Ref Updated (.git/refs/heads/main)
```

---

## 4. Syntax & Usage

### The Essential Plumbing Command Arsenal

| Plumbing Command | Purpose / Operation |
| :--- | :--- |
| `git hash-object -w [--stdin \| file]` | Hashes content, writes blob to `.git/objects/`, outputs hash |
| `git update-index --add --cacheinfo <mode> <hash> <path>` | Registers a blob with permissions and path in `.git/index` |
| `git write-tree` | Compiles current `.git/index` into a permanent tree object |
| `git commit-tree <tree-hash> [-p <parent>] -m "msg"` | Creates a commit object from a tree and parent hash |
| `git update-ref <ref> <commit-hash>` | Atomically updates a reference file to point to a commit |
| `git symbolic-ref HEAD <ref>` | Points `HEAD` symbolically to a branch ref |
| `git read-tree -u -m <commit-hash>` | Reads tree into index and updates working tree files |
| `git mktree` | Builds a tree object from a formatted text definition |
| `git cat-file -p <hash>` | Pretty-prints the uncompressed contents of any object |

---

## 5. Practical Examples

### Step-by-Step Walkthrough: Building a Complete Repository with Plumbing Only

#### Step 1: Initialize the Empty Repository
```bash
mkdir /tmp/plumbing-masterclass && cd /tmp/plumbing-masterclass
git init -b main

# At this stage, .git/ contains only template directories and HEAD pointing to refs/heads/main
cat .git/HEAD
# Output: ref: refs/heads/main
```

#### Step 2: Create Blobs Manually
Let's create two files without using the filesystem or editor:
```bash
# 1. Create a blob for a README file
README_BLOB=$(echo "# Enterprise Architecture" | git hash-object -w --stdin)
echo "README Blob: $README_BLOB"

# 2. Create a blob for a source code file
SRC_BLOB=$(echo "export const run = () => 'System Running';" | git hash-object -w --stdin)
echo "SRC Blob: $SRC_BLOB"
```

#### Step 3: Populate the Index (.git/index)
Place both blobs into the staging index, giving one a root path and the other a nested path:
```bash
# Add README.md at root
git update-index --add --cacheinfo 100644 $README_BLOB README.md

# Add code into a nested folder 'src/engine.ts'
git update-index --add --cacheinfo 100644 $SRC_BLOB src/engine.ts

# Inspect the staging area
git ls-files --stage
```
Output:
```text
100644 19f9... 0       README.md
100644 8a2c... 0       src/engine.ts
```

#### Step 4: Write the Tree Hierarchy
Convert the index into immutable tree objects in `.git/objects/`:
```bash
ROOT_TREE=$(git write-tree)
echo "Root Tree: $ROOT_TREE"

# Inspect root tree
git cat-file -p $ROOT_TREE
```
Output:
```text
100644 blob 19f9...    README.md
040000 tree 4c1a...    src
```
*Notice that Git automatically constructed the nested `src` directory tree object for us!*

#### Step 5: Create Commit #1 (Root Commit)
```bash
COMMIT_1=$(echo "feat: initial system setup" | git commit-tree $ROOT_TREE)
echo "Commit 1: $COMMIT_1"

# Inspect the raw commit object
git cat-file -p $COMMIT_1
```
Output:
```text
tree 6f2a...
author Engineering Team <dev@company.com> 1700000000 +0000
committer Engineering Team <dev@company.com> 1700000000 +0000

feat: initial system setup
```

#### Step 6: Advance the Branch Ref
```bash
# Point refs/heads/main to our new commit
git update-ref refs/heads/main $COMMIT_1

# Check with high-level porcelain:
git log --oneline
# Output: <hash> feat: initial system setup
```

#### Step 7: Synchronize the Physical Working Tree
Notice that our physical working folder is still completely empty because we operated only on Git's database! Let's materialize the files onto disk:
```bash
ls -la
# (No files except .git!)

# Checkout files from HEAD using read-tree
git read-tree -u -m HEAD
ls -R
# Output: README.md, src/engine.ts are physically created on disk!
```

---

#### Step 8: Create a Branch and Commit #2 via Plumbing
Let's create a feature branch and commit an update:
```bash
# 1. Create branch 'feature-cache' pointing to Commit 1
git update-ref refs/heads/feature-cache $COMMIT_1

# 2. Point HEAD to feature-cache
git symbolic-ref HEAD refs/heads/feature-cache

# 3. Create updated file blob
CACHE_BLOB=$(echo "export const cache = new Map();" | git hash-object -w --stdin)

# 4. Update index with new file
git update-index --add --cacheinfo 100644 $CACHE_BLOB src/cache.ts

# 5. Write new tree
TREE_2=$(git write-tree)

# 6. Create Commit 2 pointing to Commit 1 as parent (-p)
COMMIT_2=$(echo "feat(cache): implement in-memory cache" | git commit-tree $TREE_2 -p $COMMIT_1)

# 7. Update branch ref
git update-ref refs/heads/feature-cache $COMMIT_2

# 8. Materialize files
git read-tree -u -m HEAD
```

#### Step 9: Visualize the Result in the Porcelain Log
```bash
git log --graph --oneline --all --decorate
```
Output:
```text
* 8f1e2d4 (HEAD -> feature-cache) feat(cache): implement in-memory cache
* 3b4a5c6 (main) feat: initial system setup
```
*We have engineered a complete multi-branch, multi-file Git repository from scratch without invoking `git add` or `git commit` once!*

---

## 6. Best Practices

1. **Use Plumbing in CI/CD Automation Scripts**: When building internal developer tools, use plumbing commands (`git rev-parse`, `git cat-file`, `git update-ref`) instead of parsing volatile human-oriented porcelain text output.
2. **Never Directly Modify `.git/refs/` with Shell Redirection**: Always use `git update-ref`. Direct writes like `echo $HASH > .git/refs/heads/main` bypass atomic file locking and fail if packed-refs are in use.
3. **Always Check Working Tree Sync After `git update-ref`**: Changing a ref pointer with `git update-ref` updates Git's branch pointer, but leaves your physical working files unchanged. Use `git read-tree -u -m HEAD` to synchronize disk files.
4. **Inspect Corrupted Repositories with Plumbing**: When Git porcelain commands fail with cryptic error messages, use `git cat-file -p` and `git fsck` to isolate corrupted objects directly.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `git write-tree` returns empty tree (`4b825dc...`) | The index (`.git/index`) has no staged entries. | Populate the index first with `git update-index --add --cacheinfo`. |
| Files disappeared from disk after running `git update-ref` | `git update-ref` only updates the pointer; it does not update the working tree. | Run `git read-tree -u -m HEAD` to check out working files. |
| `fatal: git cat-file: could not get object info` | Invalid or mistyped SHA hash, or object was pruned. | Verify hash using `git rev-list --objects --all`. |
| Merge conflict in `git read-tree` | Staged changes in index conflict with incoming tree. | Clean index with `git read-tree --reset HEAD`. |
| `fatal: Not a valid object name refs/heads/main` | Repository was initialized, but no commits have been created yet (branch does not exist). | Create a root commit first with `git commit-tree` and point ref to it. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Scripting Efficiency**: Automated deployment scripts that invoke plumbing commands avoid the overhead of porcelain user-interface formatting, colorization, pager initialization, and hook execution, completing operations up to **10x faster**.

### Security
- **Immutability Verification**: Every step in the plumbing pipeline is mathematically bound by cryptographic hashes. Tampering with any byte in a blob alters the blob hash, which breaks the tree hash, which breaks the commit hash, providing end-to-end cryptographic integrity.

---

## 9. Comparison Table

| High-Level Porcelain | Underlying Plumbing Equivalent | Action Performed |
| :--- | :--- | :--- |
| `git add <file>` | `git hash-object -w` + `git update-index` | Writes blob to `.git/objects/`, updates `.git/index` |
| `git commit -m "msg"` | `git write-tree` + `git commit-tree` + `git update-ref` | Writes tree, creates commit object, advances ref |
| `git branch <name>` | `git update-ref refs/heads/<name> HEAD` | Creates 41-byte ref file pointing to commit |
| `git checkout <name>` | `git symbolic-ref HEAD` + `git read-tree -u` | Updates HEAD pointer and extracts files to disk |
| `git status` | Compares stats in `.git/index` vs disk and `HEAD` | Zero-read filesystem timestamp comparison |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between Git "Porcelain" and "Plumbing" commands?</strong></summary>

Porcelain commands are high-level user interface commands designed for human convenience (such as `git add`, `git commit`, `git checkout`, `git rebase`). They combine multiple operations, execute safety checks, and output formatted, colorized text. Plumbing commands are low-level Unix-style utilities (such as `git hash-object`, `git write-tree`, `git commit-tree`, `git update-ref`) designed for automation, scripting, and modular execution. Porcelain commands are simply wrappers around plumbing commands.
</details>

<details>
<summary><strong>2. How does `git commit` convert physical files into a commit object behind the scenes?</strong></summary>

1. For each staged file, Git hashes and compresses the file into a **blob** object in `.git/objects/` via `git hash-object -w`.
2. Git records the blob hash, mode, and path into `.git/index` via `git update-index`.
3. When `git commit` is invoked, Git calls `git write-tree`, compiling `.git/index` into a hierarchical tree of **tree** objects.
4. Git calls `git commit-tree`, writing a **commit** object that references the root tree hash, current `HEAD` as parent, author/committer data, and commit message.
5. Git calls `git update-ref` to advance the currently checked-out branch pointer to the new commit hash.
</details>

<details>
<summary><strong>3. What does `git update-ref` do and why is it preferred over `echo $SHA > .git/refs/heads/main`?</strong></summary>

`git update-ref` atomically updates reference pointers in Git. It is vastly superior to direct filesystem echo commands because:
1. It uses transactional locking (`.lock` files), preventing race conditions in concurrent environments.
2. It handles `.git/packed-refs` seamlessly (direct filesystem writes fail if the ref is packed).
3. It updates reflog history (`.git/logs/`) automatically.
4. It performs verification checks ensuring the target is a valid object.
</details>

<details>
<summary><strong>4. How does `git write-tree` build hierarchical directory trees from a flat index?</strong></summary>

The `.git/index` stores file entries as a sorted list of relative paths (e.g. `src/utils/math.ts`, `README.md`). When `git write-tree` executes, it parses path delimiters (`/`), creates child tree objects for each distinct directory level (`utils`, then `src`), writes them into `.git/objects/`, and embeds their hashes into parent trees, ultimately outputting a single SHA hash for the root tree.
</details>

<details>
<summary><strong>5. What is the role of `git read-tree`?</strong></summary>

`git read-tree` reads tree information specified by a tree or commit object and writes it into the staging index (`.git/index`). When executed with the `-u` (update) and `-m` (merge) flags, it also synchronizes the physical files in the working directory to match the target tree state.
</details>

<details>
<summary><strong>6. Why is a newly initialized Git repository on branch `main` in an unborn branch state?</strong></summary>

When you run `git init -b main`, Git creates `.git/HEAD` containing `ref: refs/heads/main`. However, the physical file `.git/refs/heads/main` **does not exist yet** because there are no commits. The branch is "unborn" (or unborn head). It only materializes once the very first commit object is written and its hash is stored in `.git/refs/heads/main`.
</details>

<details>
<summary><strong>7. How can you create an orphan branch using low-level Git commands?</strong></summary>

By updating `.git/HEAD` symbolically to point to a nonexistent branch reference without changing existing working tree files:
```bash
git symbolic-ref HEAD refs/heads/my-orphan-branch
```
Subsequent commits will have zero parents, making the commit the root of an independent, disconnected DAG history.
</details>

<details>
<summary><strong>8. What is the significance of the empty tree hash `4b825dc642cb6eb9a060e54bf8d69288fbee4904`?</strong></summary>

`4b825dc642cb6eb9a060e54bf8d69288fbee4904` is the universal SHA-1 hash of an empty tree object (a tree containing zero entries). In Git scripting, diffing any tree against this specific hash allows you to view the entire content of that tree as if comparing against a completely empty repository (e.g. `git diff-tree -p 4b825dc... HEAD`).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Creating a File Blob and Reading It Back</strong></summary>

**Task**: Use `git hash-object -w` to store a string in `.git/objects/` and `git cat-file -p` to print it back to terminal.

**Solution**:
```bash
mkdir /tmp/plumbing-drill && cd /tmp/plumbing-drill
git init

HASH=$(echo "Mastering Plumbing" | git hash-object -w --stdin)
echo "Hash: $HASH"

git cat-file -p $HASH
# Outputs: Mastering Plumbing
```
</details>

<details>
<summary><strong>Exercise 2: Staging a File into the Index with `git update-index`</strong></summary>

**Task**: Take the blob from Exercise 1 and stage it as `docs/guide.txt` with standard non-executable permissions (`100644`). Inspect with `git ls-files`.

**Solution**:
```bash
git update-index --add --cacheinfo 100644 $HASH docs/guide.txt

git ls-files --stage
# Shows: 100644 <hash> 0 docs/guide.txt
```
</details>

<details>
<summary><strong>Exercise 3: Writing a Tree Object from the Index</strong></summary>

**Task**: Run `git write-tree` to generate the root tree object, and inspect the resulting tree structure with `git cat-file -p`.

**Solution**:
```bash
ROOT_TREE=$(git write-tree)
echo "Root Tree: $ROOT_TREE"

git cat-file -p $ROOT_TREE
# Shows: 040000 tree <hash> docs
```
</details>

<details>
<summary><strong>Exercise 4: Creating a Commit Object with `git commit-tree`</strong></summary>

**Task**: Create a commit object from the root tree with author name "Plumbing Engineer" and commit message "feat: low level commit".

**Solution**:
```bash
export GIT_AUTHOR_NAME="Plumbing Engineer"
export GIT_AUTHOR_EMAIL="plumbing@engineer.org"
export GIT_COMMITTER_NAME="Plumbing Engineer"
export GIT_COMMITTER_EMAIL="plumbing@engineer.org"

COMMIT_HASH=$(echo "feat: low level commit" | git commit-tree $ROOT_TREE)
echo "Commit: $COMMIT_HASH"

git cat-file -p $COMMIT_HASH
```
</details>

<details>
<summary><strong>Exercise 5: Pointing the Branch to the Commit and Checking Status</strong></summary>

**Task**: Point `refs/heads/main` to your new commit using `git update-ref` and run `git log` to verify that porcelain recognizes the commit.

**Solution**:
```bash
git update-ref refs/heads/main $COMMIT_HASH

git log -1
# Complete valid commit displayed!
```
</details>

<details>
<summary><strong>Exercise 6: Materializing Files onto the Working Tree</strong></summary>

**Task**: Observe that `docs/guide.txt` does not physically exist on disk yet. Run `git read-tree -u -m HEAD` to check out the files onto the filesystem.

**Solution**:
```bash
ls docs/ 2>/dev/null || echo "Directory does not exist yet!"

# Materialize files
git read-tree -u -m HEAD

cat docs/guide.txt
# Output: Mastering Plumbing!
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Step | Plumbing Command | What Happens Under the Hood |
| :--- | :--- | :--- |
| **1. Store File** | `git hash-object -w <file>` | Calculates SHA, deflates via zlib, saves to `.git/objects/` |
| **2. Stage in Index** | `git update-index --add --cacheinfo <mode> <sha> <path>` | Records permissions, blob SHA, and path in `.git/index` |
| **3. Build Trees** | `git write-tree` | Compiles `.git/index` into hierarchical tree objects |
| **4. Create Commit**| `git commit-tree <tree> [-p <parent>] -m "msg"` | Writes commit object with tree hash, parent, metadata |
| **5. Advance Ref** | `git update-ref refs/heads/<branch> <commit>` | Atomically writes commit SHA into `.git/refs/heads/<branch>` |
| **6. Check Out Files**| `git read-tree -u -m HEAD` | Synchronizes physical working tree files with commit tree |

---

## 13. Further Reading & Official Documentation

- [Git Internals - Plumbing and Porcelain (Pro Git Book)](https://git-scm.com/book/en/v2/Git-Internals-Plumbing-and-Porcelain)
- [Official Git Documentation - git-hash-object](https://git-scm.com/docs/git-hash-object)
- [Official Git Documentation - git-write-tree](https://git-scm.com/docs/git-write-tree)
- [Official Git Documentation - git-commit-tree](https://git-scm.com/docs/git-commit-tree)
- [Official Git Documentation - git-update-ref](https://git-scm.com/docs/git-update-ref)

---

[<- 09-hashes-and-sha-256.md](./09-hashes-and-sha-256.md) | [Back to Module Overview](./README.md) | [Next Module: 02-commands ->](../02-commands/README.md)
