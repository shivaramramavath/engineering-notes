<!--
================================================================================
Metadata:
  Title: The Three Trees: Working Tree, Staging Area & HEAD
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 02-repositories.md
  Key Terminology: Three Trees, Working Tree, Staging Area (Index), HEAD, git restore, git diff --staged, Stage Slots
================================================================================
-->

# The Three Trees: Working Tree, Staging Area & HEAD

[<- 02-repositories.md](./02-repositories.md) | [Back to Module Overview](./README.md) | [04-commits.md ->](./04-commits.md)

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

In traditional version control systems (such as SVN or Perforce), there are only two states: the physical file on your disk and the remote repository database. When you commit, whatever is modified in your workspace is blasted straight into the repository.

Git introduces an intermediate layer between your physical workspace and the permanent commit history: the **Staging Area** (also referred to internally as the **Index** or the **Cache**).

This design provides software engineers with fine-grained precision:
- You can work on three different bug fixes simultaneously across a single file, and selectively stage only the relevant hunks (`git add -p`) for the next atomic commit.
- You can craft clean, purposeful, reviewable commits that each do exactly one thing, rather than monolithic "kitchen-sink" commits.
- It enables rapid comparison across three distinct states of code: what is currently saved on disk, what is prepared for the next commit, and what was recorded in the last commit.

---

## 2. Core Concepts

### The Three Trees Architecture
In Git terminology, a "tree" represents a collection of files and directories at a specific point in time. Git constantly arbitrates between three distinct trees:

1. **The Working Tree (Sandbox / Filesystem)**:
   - Physical directories and files on your workstation disk.
   - Where you write code, install dependencies, compile builds, and introduce changes.
   - Git does not track these changes until you explicitly instruct it to do so.
2. **The Staging Area (Index / `.git/index`)**:
   - The preparation buffer for the next commit snapshot.
   - It caches paths, permissions, and cryptographic hashes of files that you have staged via `git add`.
   - Represents: *"What will the next commit look like if I commit right now?"*
3. **The HEAD Commit (Last Committed Snapshot)**:
   - The root tree object of the currently checked-out commit in `.git/objects/`.
   - Immutable; represents the official, permanent state of the repository at the current revision.

```
+-----------------------------------------------------------------------------------+
|                            Git Three Trees Dataflow                               |
|                                                                                   |
|  [ Working Tree ]           [ Staging Area (Index) ]          [ HEAD Commit ]     |
|   (Disk Files)                 (.git/index)                   (.git/objects)      |
|                                                                                   |
|        │                              │                              │            |
|        │────── git add <file> ───────>│                              │            |
|        │                              │────── git commit ───────────>│            |
|        │                              │                              │            |
|        │<───── git restore ───────────│                              │            |
|        │                              │<── git restore --staged ─────│            |
|        │                                                             │            |
|        │<─────────────── git restore --source=HEAD ──────────────────│            |
|                                                                                   |
|  DIFF INSPECTION:                                                                 |
|        │<====== git diff ============>│                              │            |
|        │                              │<==== git diff --staged =====>│            |
|        │<====================== git diff HEAD ======================>│            |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### The File Lifecycle States
Within a Git repository, any file in your working tree occupies one of four states:
1. **Untracked**: The file exists on disk, but has never been added to Git's index and is not ignored.
2. **Unmodified**: The file in the working tree is identical to the file recorded in the HEAD commit.
3. **Modified**: The file has been altered in the working tree, but the changes have not yet been staged.
4. **Staged**: The modified file has been processed by `git add`, its contents have been compressed into a blob object in `.git/objects/`, and its hash is recorded in `.git/index`.

```
                    +------------------------------------+
                    |             Untracked              |
                    +------------------------------------+
                                      │
                                      │ git add
                                      ▼
+----------------+  git add   +----------------+  git commit   +----------------+
|    Modified    | ---------> |     Staged     | ------------> |   Unmodified   |
+----------------+            +----------------+               +----------------+
        ▲                                                              │
        │                       Edit file                              │
        +──────────────────────────────────────────────────────────────+
```

### The Anatomy of `.git/index`
When you run `git add file.txt`:
1. Git hashes the file content using SHA-1/SHA-256 and compresses it via `zlib`.
2. Git writes the object directly to `.git/objects/xx/yyyy...` as a **blob**.
3. Git updates `.git/index`, storing:
   - File path (`file.txt`)
   - File mode (`100644` for standard file, `100755` for executable)
   - Object hash (points to the newly created blob)
   - Stat cache data: `ctime`, `mtime`, file size, device/inode numbers.
   - **Stage slot**: Normally `0`. During a merge conflict, slots `1` (common ancestor), `2` (our version / `HEAD`), and `3` (their version / incoming branch) are populated.

---

## 4. Syntax & Usage

### Transitioning Between Trees

#### 1. Staging Changes (Working Tree -> Staging Area)
```bash
# Stage a specific file
git add src/index.ts

# Stage all modified, deleted, and new files in the entire repository
git add .

# Interactively stage hunks (review each change block before staging)
git add -p src/index.ts
```

#### 2. Committing Changes (Staging Area -> HEAD Commit)
```bash
# Commit staged changes with message
git commit -m "feat(auth): add jwt verification middleware"

# Open configured $EDITOR for a multi-line commit message with diff preview
git commit -v
```

#### 3. Modern Undoing & Restoring (Git 2.23+ `git restore`)
```bash
# Discard changes in Working Tree (revert file back to staged or HEAD state)
git restore src/index.ts

# Unstage changes (remove from Staging Area, keeping modifications in Working Tree)
git restore --staged src/index.ts

# Revert working tree directly back to HEAD, discarding both staged and unstaged edits
git restore --source=HEAD --staged --worktree src/index.ts
```

---

### Diffs Across the Three Trees

```bash
# Compare Working Tree against Staging Area (What have I edited but not yet staged?)
git diff

# Compare Staging Area against HEAD (What will be included in the next commit?)
git diff --staged
# Or synonym:
git diff --cached

# Compare Working Tree directly against HEAD (All changes made since last commit)
git diff HEAD
```

---

## 5. Practical Examples

### Example 1: Interactive Hunk Staging with `git add -p`
Suppose you made two unrelated changes to `app.js`: one fixing an authentication bug and another adding console debug logging. You only want to commit the bug fix:

```bash
git add -p app.js
```
Git displays the first hunk and prompts:
```text
Stage this hunk [y,n,q,a,d,s,e,?]?
```
Key choices:
- `y`: Stage this hunk.
- `n`: Do not stage this hunk.
- `s`: Split hunk into smaller sub-hunks.
- `e`: Manually edit the hunk in your text editor.
- `q`: Quit; do not stage this or any remaining hunks.

Result: Only the bug fix is staged in the index, leaving the console debug lines in your working tree!

### Example 2: Inspecting the Raw Binary Index with `git ls-files`
Use low-level plumbing commands to inspect the exact entries cached in `.git/index`:

```bash
mkdir /tmp/index-lab && cd /tmp/index-lab
git init

echo "console.log('alpha');" > script.js
git add script.js

# Inspect staged index records
git ls-files --stage
```
Output:
```text
100644 7b91d977464a974b8686121665a3d4dc69ff0bd7 0       script.js
```
Explanation of columns:
1. `100644`: Standard non-executable file mode.
2. `7b91d9...`: SHA hash of the blob stored in `.git/objects/`.
3. `0`: Merge stage slot (0 = clean, normal file).
4. `script.js`: Relative path.

### Example 3: Unstaging vs. Discarding Working Tree Changes
Understand the crucial difference between losing changes forever vs. keeping them on disk:

```bash
# Scenario: You staged an unwanted file 'secrets.json'
git add secrets.json

# SAFE: Unstage it. The file remains on your disk with all modifications!
git restore --staged secrets.json
git status
# Output: ?? secrets.json (untracked, changes preserved!)

# DESTRUCTIVE: Discarding changes in working tree
echo "Temporary hack" >> script.js
git status
# Output: modified: script.js

# This permanently overwrites script.js on disk with the staged/HEAD version!
git restore script.js
```

### Example 4: Verifying Diffs Between All Three Trees
```bash
echo "Line 1 - Initial" > doc.txt
git add doc.txt && git commit -m "Initial commit"

# Make change 1 and stage it
echo "Line 2 - Staged" >> doc.txt
git add doc.txt

# Make change 2 without staging it
echo "Line 3 - Unstaged" >> doc.txt

# 1. Inspect unstaged changes (Working Tree vs Index)
git diff
# Shows addition of "Line 3 - Unstaged"

# 2. Inspect staged changes (Index vs HEAD)
git diff --staged
# Shows addition of "Line 2 - Staged"

# 3. Inspect total drift (Working Tree vs HEAD)
git diff HEAD
# Shows addition of both Line 2 and Line 3
```

### Example 5: Resolving Stage Slots During a Merge Conflict
Inspect how `.git/index` handles conflicts:

```bash
# Create two conflicting branches
git checkout -b branch-x
echo "Feature X" > feature.txt && git add feature.txt && git commit -m "X"

git checkout main && git checkout -b branch-y
echo "Feature Y" > feature.txt && git add feature.txt && git commit -m "Y"

# Attempt merge
git merge branch-x
# Output: CONFLICT (add/add): Merge conflict in feature.txt

# Inspect the 3 active stage slots in .git/index
git ls-files -u
```
Output:
```text
100644 8a73... 2       feature.txt   <-- Stage 2: Our version (HEAD / branch-y)
100644 5c12... 3       feature.txt   <-- Stage 3: Their version (incoming / branch-x)
```
Once you edit `feature.txt` and run `git add feature.txt`, Git resolves the conflict, deletes slots 2 and 3, and writes the unified result into stage slot `0`.

---

## 6. Best Practices

1. **Craft Atomic Commits with `git add -p`**: Never blind-stage entire directories with `git add .` without checking `git status` first. Break complex sessions into focused, atomic commits.
2. **Always Run `git diff --staged` Before Committing**: Review the exact snapshot about to be written to history. Catch accidental debug statements, credentials, or misplaced whitespace before it enters the commit DAG.
3. **Prefer Modern `git restore` Over Overloaded `git checkout`**: Prior to Git 2.23, `git checkout` was dangerously overloaded to switch branches, restore files, and manipulate detached heads. Use `git restore` for files and `git switch` for branches.
4. **Use `git commit -v` for Diff Visibility**: In your `.gitconfig`, enable `commit.verbose = true` to view the full diff inside your commit message editor template.
5. **Protect Uncommitted Work**: Remember that unstaged changes in the working tree are **untracked by Git's object store**. If you run `git restore .` or `git checkout -- .`, uncommitted edits cannot be recovered via `git reflog`.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `git diff` shows no output even though files were modified | The modifications were already staged via `git add`. `git diff` only checks working tree vs index. | Run `git diff --staged` (or `git diff --cached`) to view staged changes. |
| Accidentally ran `git restore <file>` and lost critical code | `git restore` permanently rewrote the file on disk from the index/HEAD. | Check your code editor's local history (e.g. VS Code Timeline or JetBrains Local History); Git cannot recover unstaged files. |
| Ran `git commit -a` and accidentally committed untracked secrets | `-a` / `--all` automatically stages modified tracked files, but doesn't protect against intended exclusions. | Avoid `-a` for sensitive repos; use explicit `git add` and review with `git status`. |
| `fatal: pathspec '...' did not match any files` | Typo in path or file was deleted. | Check filename with `git status` or use `git restore --staged <path>`. |
| Staged a massive 500MB binary file accidentally | File object was created in `.git/objects/` during `git add`. | Run `git restore --staged <file>` and run `rm <file>`. Run `git gc --prune=now` if repository size remains inflated. |
| Legacy command `git reset HEAD <file>` confused user | Legacy syntax resets index state to HEAD. | Use the modern, intuitive syntax: `git restore --staged <file>`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Stat Cache Acceleration**: The `.git/index` caches the `mtime`, `size`, and inode of each file. When `git status` runs, Git compares filesystem stats against the index. If `mtime` and `size` have not changed, Git skips reading and hashing the file content entirely, allowing status checks across 100,000 files to complete in milliseconds.
- **Garbage Created by `git add`**: Every time you run `git add`, a compressed zlib blob object is created immediately in `.git/objects/`. If you stage large files repeatedly, these orphan blobs remain on disk until pruned by `git gc`.

### Security
- **Accidental Staging of Secrets**: The staging area is your critical security inspection checkpoint. Using `git diff --staged` before every commit is the primary defense against committing passwords, AWS keys, or `.env` files into permanent commit history.

---

## 9. Comparison Table

| Attribute | Working Tree | Staging Area (Index) | HEAD Commit |
| :--- | :--- | :--- | :--- |
| **Physical Location** | Project workspace directory | `.git/index` (binary file) | `.git/objects/` (zlib blobs/trees) |
| **Persistence** | Ephemeral filesystem files | Mutable cache file | Immutable cryptographically verified DAG |
| **Editable By** | Text editor, IDE, shell | `git add`, `git restore --staged`| `git commit` (creates new snapshot) |
| **Status in `git status`**| "Changes not staged for commit" | "Changes to be committed" | Clean / No changes |
| **Recoverable if Lost?** | NO (unless cached in editor) | Partially (blobs exist in objects) | YES (always recoverable via commit hash/reflog) |
| **Diff Command** | `git diff` (vs Index) | `git diff --staged` (vs HEAD) | `git diff HEAD` (vs Working Tree) |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain the architectural role of the Staging Area (Index) in Git.</strong></summary>

The Staging Area (stored in `.git/index`) is an intermediate caching layer that decouples a developer's physical working directory from the permanent repository commit history. It stores the path, permissions, and cryptographic hash of every file intended for the next commit. This enables engineers to craft atomic commits by selectively staging specific files or hunks (`git add -p`), review proposed snapshots prior to recording (`git diff --staged`), and arbitrate multi-way merge conflicts via index stage slots.
</details>

<details>
<summary><strong>2. What is the difference between `git diff`, `git diff --staged`, and `git diff HEAD`?</strong></summary>

- `git diff`: Compares changes between the **Working Tree** and the **Staging Area**. It answers: *"What modifications have I made on disk that are not yet staged for commit?"*
- `git diff --staged` (or `--cached`): Compares changes between the **Staging Area** and the **HEAD commit**. It answers: *"What modifications are currently staged and will be included if I run `git commit`?"*
- `git diff HEAD`: Compares changes between the **Working Tree** directly against the **HEAD commit**, displaying all uncommitted changes (both staged and unstaged combined).
</details>

<details>
<summary><strong>3. Why did Git introduce `git restore` in version 2.23 to replace `git checkout` for file operations?</strong></summary>

Historically, `git checkout` was heavily overloaded with multiple unrelated responsibilities: switching branches, creating new branches (`-b`), updating detached HEADs, discarding changes in working tree files (`git checkout -- <file>`), and restoring files from other commits. This caused frequent developer errors (e.g. accidentally discarding file changes when trying to switch to a branch with the same name). Git 2.23 split these concerns into two purpose-built, safer commands: `git switch` (for branches) and `git restore` (for files across working tree and index).
</details>

<details>
<summary><strong>4. What occurs in `.git/objects/` when `git add` is executed on a modified file?</strong></summary>

When `git add <file>` executes:
1. Git calculates the SHA-1 or SHA-256 hash of the file contents.
2. Git compresses the file contents using `zlib`.
3. Git writes a new **blob** object directly into `.git/objects/` under the corresponding hash directory.
4. Git updates `.git/index` to map the file path to this new blob hash.
*Crucial detail*: The blob is stored in the object database immediately upon staging, even if the developer never actually runs `git commit`!
</details>

<details>
<summary><strong>5. What are the merge stage slots (0, 1, 2, 3) in `.git/index` during a conflict?</strong></summary>

During standard operation, all files in the index reside in slot `0` (clean/normal). When a merge conflict occurs, slot `0` for the conflicting path is cleared, and up to three versions are stored in separate slots:
- **Slot 1**: The common ancestor base version of the file prior to either branch's changes.
- **Slot 2**: Our version (the file as it exists in `HEAD` / the current branch).
- **Slot 3**: Their version (the file as it exists in the incoming branch being merged).
Running `git checkout --ours` or `git checkout --theirs` retrieves data from slots 2 or 3. Staging the resolved file via `git add` clears slots 1, 2, and 3 and writes the unified file into slot `0`.
</details>

<details>
<summary><strong>6. How does `git restore --staged <file>` differ from `git restore <file>`?</strong></summary>

- `git restore --staged <file>`: Modifies the **Staging Area (Index)**. It copies the state of `<file>` from `HEAD` into the index, effectively unstaging it. The physical working tree file remains completely untouched.
- `git restore <file>`: Modifies the **Working Tree**. It overwrites the physical file on disk with the version currently in the index (or HEAD if unstaged). Any unstaged local edits made to that file are permanently destroyed.
</details>

<details>
<summary><strong>7. How can you stage only part of a file in Git?</strong></summary>

By using interactive patch staging: `git add -p <file>`. Git breaks the file's modifications into distinct "hunks" and interactively prompts the user for each hunk. Engineers can accept (`y`), reject (`n`), split (`s`), or manually edit (`e`) each hunk, allowing precise staging of specific lines while leaving surrounding changes in the working tree.
</details>

<details>
<summary><strong>8. Can changes discarded via `git restore <file>` be recovered using `git reflog`?</strong></summary>

No. `git reflog` tracks changes to reference pointers (`HEAD`, branches) when commits, rebases, or checkouts occur. If a change was only in the working tree and was never staged or committed, it was never recorded in Git's object database. Once overwritten by `git restore`, the data is permanently lost from Git (recovery is only possible if the text editor or IDE has a local history/undo buffer).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Moving a File Through the Three Trees</strong></summary>

**Task**: Create a new file, inspect its status as Untracked, stage it (inspect as Staged), commit it (inspect as Unmodified), edit it (inspect as Modified), and verify status at each step.

**Solution**:
```bash
mkdir /tmp/three-trees-lab && cd /tmp/three-trees-lab
git init
echo "Hello World" > hello.txt

# 1. Untracked
git status -s
# Output: ?? hello.txt

# 2. Staged
git add hello.txt
git status -s
# Output: A  hello.txt

# 3. Committed (Unmodified)
git commit -m "feat: initial commit"
git status -s
# Output: (clean)

# 4. Modified
echo "New line" >> hello.txt
git status -s
# Output:  M hello.txt
```
</details>

<details>
<summary><strong>Exercise 2: Comparing Working Tree, Index, and HEAD Diffs</strong></summary>

**Task**: In a repository, create staged and unstaged modifications in the same file. Run the appropriate diff commands to isolate each layer.

**Solution**:
```bash
echo "Staged modification" >> hello.txt
git add hello.txt
echo "Unstaged modification" >> hello.txt

# View only the unstaged change:
git diff
# Shows "+Unstaged modification"

# View only the staged change:
git diff --staged
# Shows "+Staged modification"

# View combined total changes since HEAD:
git diff HEAD
# Shows both additions
```
</details>

<details>
<summary><strong>Exercise 3: Safely Unstaging Changes with `git restore`</strong></summary>

**Task**: Stage an unwanted file `config.local`. Unstage it without losing your local modifications.

**Solution**:
```bash
touch config.local
git add config.local
git status -s
# Output: A  config.local

# Unstage using modern git restore:
git restore --staged config.local

git status -s
# Output: ?? config.local (file exists on disk unharmed!)
```
</details>

<details>
<summary><strong>Exercise 4: Inspecting Low-Level Index Entries with `git ls-files`</strong></summary>

**Task**: Use `git ls-files --stage` to inspect the SHA hash and stage slot of tracked files in your repository.

**Solution**:
```bash
git ls-files --stage
# Prints file mode, 40-character SHA hash, stage slot 0, and relative file path
```
</details>

<details>
<summary><strong>Exercise 5: Restoring a File to an Older Revision Directly</strong></summary>

**Task**: Make two commits to `app.js`. Use `git restore` with the `--source` option to restore `app.js` to the version from the first commit without switching branches.

**Solution**:
```bash
echo "const version = 1;" > app.js
git add app.js && git commit -m "feat: v1"

echo "const version = 2;" > app.js
git add app.js && git commit -m "feat: v2"

# Restore app.js in working tree to v1 state
git restore --source=HEAD~1 app.js

cat app.js
# Output: const version = 1;
git status -s
# Output:  M app.js
```
</details>

<details>
<summary><strong>Exercise 6: Interactive Staging Drill</strong></summary>

**Task**: Create a file with 10 lines. Modify lines 2 and 8. Use `git add -p` to stage only line 2. Verify that line 8 remains unstaged in `git status`.

**Solution**:
```bash
seq 1 10 > count.txt
git add count.txt && git commit -m "feat: numbers 1-10"

# Edit lines 2 and 8
sed -i 's/2/TWO/' count.txt
sed -i 's/8/EIGHT/' count.txt

# Interactively stage
git add -p count.txt
# When prompted, if Git groups them into one hunk, press 's' to split!
# Select 'y' for the first hunk (TWO) and 'n' for the second hunk (EIGHT).

git diff --staged
# Displays only TWO
git diff
# Displays only EIGHT
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Modern Command (Git 2.23+) | Legacy Command |
| :--- | :--- | :--- |
| **Stage Changes** | `git add <file>` | `git add <file>` |
| **Interactive Hunk Staging** | `git add -p <file>` | `git add -p <file>` |
| **Commit Staged Changes** | `git commit -m "msg"` | `git commit -m "msg"` |
| **Diff Working Tree vs Index** | `git diff` | `git diff` |
| **Diff Index vs HEAD** | `git diff --staged` | `git diff --cached` |
| **Diff Working Tree vs HEAD** | `git diff HEAD` | `git diff HEAD` |
| **Discard Working Tree Edits**| `git restore <file>` | `git checkout -- <file>` |
| **Unstage Staged Changes** | `git restore --staged <file>` | `git reset HEAD <file>` |
| **Discard Staged & Unstaged** | `git restore --source=HEAD --staged --worktree <file>` | `git checkout HEAD -- <file>` |
| **Inspect Staging Index** | `git ls-files --stage` | `git ls-files --stage` |

---

## 13. Further Reading & Official Documentation

- [Git Documentation - Reset Demystified (The Three Trees)](https://git-scm.com/book/en/v2/Git-Tools-Reset-Demystified)
- [Official Git Documentation - git-restore](https://git-scm.com/docs/git-restore)
- [Official Git Documentation - git-diff](https://git-scm.com/docs/git-diff)
- [Pro Git Book - Recording Changes to the Repository](https://git-scm.com/book/en/v2/Git-Basics-Recording-Changes-to-the-Repository)

---

[<- 02-repositories.md](./02-repositories.md) | [Back to Module Overview](./README.md) | [04-commits.md ->](./04-commits.md)
