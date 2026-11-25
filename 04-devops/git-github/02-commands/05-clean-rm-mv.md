<!--
================================================================================
Metadata:
  Title: Repository File Management: git rm, git mv & git clean
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 45 minutes
  Prerequisites: 04-undoing-changes.md
  Key Terminology: git rm, git rm --cached, git mv, Dynamic Rename Detection, Similarity Index, git clean (-fdx), Case-Sensitivity Renames
================================================================================
-->

# Repository File Management: git rm, git mv & git clean

[<- 04-undoing-changes.md](./04-undoing-changes.md) | [Back to Module Overview](./README.md) | [06-branching.md ->](./06-branching.md)

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

Managing the lifecycle of files in a Git repository involves more than just creating and editing content. As projects evolve, files must be permanently deleted, moved across directory structures, untracked to protect sensitive credentials, and cleaned of build artifacts.

Executing these operations using standard operating system utilities (like standard `rm` or `mv`) often creates friction:
- Using `rm file.txt` removes the file from disk, but leaves the deletion unstaged in the Git index, requiring an additional `git add file.txt` to stage the deletion.
- Moving files on case-insensitive filesystems (Windows and macOS) can confuse Git, leading to duplicate tracking or missed renames.
- Stale untracked compilation outputs and test artifacts can pollute test runs, leading to false "it works on my machine" bugs.

The trio of commands—**`git rm`**, **`git mv`**, and **`git clean`**—provides atomic, index-aware file lifecycle management across all operating systems.

---

## 2. Core Concepts

### 1. `git rm`: Synchronized Deletion
When you want to stop tracking a file and delete it:
- **`git rm <file>`**: Deletes the file from the **Working Tree** AND immediately stages the deletion in the **Staging Area (Index)**.
- **`git rm --cached <file>`**: Removes the file from the **Staging Area (Index)**, but **LEAVES the physical file intact on disk**. This is the standard method for untracking files that were committed by mistake (like `.env` or IDE folders).

### 2. `git mv`: Atomic Rename & Move
Instead of manually moving a file on disk and staging both a deletion and an addition, `git mv <old> <new>` executes both operations in a single atomic step:
```
git mv old.txt new.txt
  ├── Physically renames old.txt to new.txt on disk
  └── Stages the deletion of old.txt and addition of new.txt in .git/index
```

### 3. `git clean`: Purging Untracked Noise
While `git restore` and `git reset` operate exclusively on *tracked* files, `git clean` is purpose-built to purge *untracked* files and directories that are not under version control.

---

## 3. How It Works Internally

### How Git Detects Renames (Dynamic Similarity Detection)
A surprising architectural fact about Git is that **Git does not record renames in commit objects**.
- Unlike systems that maintain a explicit "rename" metadata entry, Git tree objects record only path-to-blob mappings.
- When you run `git log -M`, `git diff -M`, or `git status`, Git calculates renames **dynamically** at runtime!

Git's rename detection algorithm:
1. **Exact Hash Match**: If a file at path $A$ was deleted and a new file at path $B$ was added with the **exact same blob SHA hash**, Git instantly declares a 100% rename.
2. **Similarity Index Heuristic**: If the contents differ slightly, Git compares the deleted blob against newly added blobs using an edit-distance algorithm. If the similarity exceeds a threshold (default 50%, tunable via `-M<percentage>`), Git reports it as a rename (e.g. `renamed: old.ts -> new.ts (85%)`).

```
Commit 1: Root Tree ---> Blob SHA: 7a8b9c ("User Service Logic") at path "src/user.js"
Commit 2: Root Tree ---> Blob SHA: 7a8b9c ("User Service Logic") at path "src/userService.ts"

Git compares hashes at runtime:
  Hash is identical! -> Git reports: "renamed: src/user.js -> src/userService.ts"
```

---

## 4. Syntax & Usage

### 1. File Removal (`git rm`)
```bash
# Delete file from disk and stage deletion
git rm src/legacy-code.ts

# Delete entire directory recursively
git rm -r dist/

# Untrack file from Git while preserving physical file on disk (CRITICAL COMMAND!)
git rm --cached .env

# Untrack directory recursively while keeping files on disk
git rm -r --cached .idea/

# Force delete file even if it has staged uncommitted edits
git rm -f src/modified-file.ts
```

---

### 2. File Relocation & Renaming (`git mv`)
```bash
# Rename file within same directory
git mv old-name.ts new-name.ts

# Move file into a subdirectory
git mv app.ts src/controllers/app.ts

# Force rename (useful for case-only renames on Windows/macOS)
git mv -f utils.ts Utils.ts
```

---

### 3. Purging Untracked Garbage (`git clean`)
```bash
# Dry run: preview what files WOULD be deleted without touching disk
git clean -n

# Dry run including untracked directories
git clean -nd

# Force delete all untracked files and directories
git clean -fd

# Force delete ALL untracked files, INCLUDING files ignored by .gitignore
git clean -fdx

# Force delete ONLY ignored files, keeping untracked non-ignored files
git clean -fdX

# Interactive cleaning prompt
git clean -i
```

---

## 5. Practical Examples

### Example 1: Safely Untracking Accidentally Committed Secrets
Suppose `.env` was committed to the repository. Adding `.env` to `.gitignore` does not remove it from tracking! Use `git rm --cached`:

```bash
# 1. Untrack from index without deleting file from your laptop
git rm --cached .env

# 2. Add to .gitignore so it isn't tracked again
echo ".env" >> .gitignore
git add .gitignore

# 3. Commit the removal
git commit -m "chore: untrack .env file and add to gitignore"

# Verify file still exists on disk
ls -l .env
# File is intact!
```

### Example 2: Renaming Files with Case-Only Changes on Windows/macOS
On Windows (NTFS) and macOS (APFS by default), filesystems are case-insensitive. Running `mv config.js Config.js` is ignored by the OS because both names refer to the same inode:

```bash
# The Two-Step Rename Workaround:
git mv config.js temp-config.js
git mv temp-config.js Config.js
git commit -m "refactor: capitalize Config.js"

# Or using the force flag:
git mv -f config.js Config.js
git commit -m "refactor: capitalize Config.js"
```

### Example 3: Verifying Dynamic Rename Detection Thresholds
Observe Git dynamically calculating a rename with modified content:

```bash
mkdir /tmp/rename-lab && cd /tmp/rename-lab
git init
echo -e "line1\nline2\nline3\nline4\nline5\nline6\nline7\nline8\nline9\nline10" > original.txt
git add original.txt && git commit -m "feat: initial document"

# Move file and modify 2 lines
git mv original.txt refactored.txt
sed -i 's/line2/MODIFIED_LINE/' refactored.txt
sed -i 's/line8/ANOTHER_MODIFICATION/' refactored.txt

# Inspect status
git status -s
```
Output:
```text
R  original.txt -> refactored.txt
```
Run `git diff --staged -M` to view the calculated similarity percentage:
```text
similarity index 80%
rename from original.txt
rename to refactored.txt
```

### Example 4: Sanitizing CI Runner Workspaces with `git clean -fdx`
Ensure zero contamination from previous test runs in CI/CD pipelines:

```bash
# Completely restore repository to pristine clone state:
# 1. Discard all tracked modifications
git reset --hard HEAD

# 2. Delete ALL untracked files, build outputs, node_modules, and cache files
git clean -fdx

# 3. Verify absolute cleanliness
git status
# Output: nothing to commit, working tree clean
```

---

## 6. Best Practices

1. **Always Use `git rm --cached` for Repository Secrets**: If an engineer commits a `.env` or credential file, immediately use `--cached` so they do not delete their active local development environment.
2. **Never Run `git clean -fd` Without Running `git clean -nd` First**: `git clean` is permanent and non-reversible. A dry run (`-n`) prevents accidentally destroying unsaved scratch files.
3. **Use `git mv` Instead of Manual Moves**: While Git detects renames dynamically, using `git mv` guarantees atomic staging and prevents confusing commit histories.
4. **Use `-fdX` to Clear Build Artifacts Without Losing Scratch Files**: The uppercase `-X` flag removes *only* files matching `.gitignore` (such as `dist/` or `node_modules/`), leaving any new uncommitted source files unharmed!
5. **Beware Case Renames in Mixed Teams**: When renaming files across case boundaries, always test on Windows/macOS to ensure file paths resolve cleanly for all engineers.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `fatal: git rm: '...' has changes staged in the index` | Attempted to delete a file that has uncommitted modifications. | Use `git rm -f <file>` to force removal. |
| File still tracked after adding to `.gitignore` | `.gitignore` only applies to untracked files; tracked files remain tracked. | Run `git rm --cached <file>` and commit. |
| Running `git mv` on Windows says `destination exists` | Case-insensitive collision (e.g. `app.js` to `App.js`). | Use two-step rename (`git mv app.js temp.js && git mv temp.js App.js`). |
| Accidentally deleted an important untracked file with `git clean` | `git clean` deletes files outside version control; Git cannot recover them. | Check OS trash/recycle bin or IDE local history; Git object store has no record of untracked files. |
| `git status` shows delete + add instead of rename | File content was modified too heavily during the move (<50% similarity). | Commit the move first via `git mv`, and modify the contents in a subsequent commit. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Rename Detection Tuning (`diff.renameLimit`)**:
  On massive repositories with tens of thousands of files, calculating similarity indices across all deleted and added files can exhaust CPU cycles. Git limits rename detection checks via `diff.renameLimit` (default 1000). On powerful developer workstations, increase this to `diff.renameLimit = 5000` to prevent missed rename detections during major refactorings.

### Security
- **Untracked Secret Cleanup**: Running `git clean -fdx` is standard practice before packaging Docker images to guarantee that local developer `.env` files or debug logs are not accidentally copied into container layers.

---

## 9. Comparison Table

| Command | Working Tree File | Staging Index Entry | Target Audience |
| :--- | :--- | :--- | :--- |
| **`git rm <file>`** | Deleted | Deleted (Staged) | Permanent file deletion |
| **`git rm --cached <file>`**| Preserved on disk | Deleted (Staged) | Stop tracking private/secret files |
| **`git mv <old> <new>`** | Renamed on disk | Staged as rename | Restructuring directories/files |
| **`git clean -fd`** | Deleted (Untracked) | Untouched | Purging untracked garbage |
| **`git clean -fdx`** | Deleted (Untracked + Ignored)| Untouched | Full clean-slate CI builds |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What does `git rm --cached <file>` do and when is it used?</strong></summary>

`git rm --cached <file>` removes the specified file from the **Staging Area (Index)**, instructing Git to stop tracking it in version control, while leaving the physical file completely intact in the **Working Tree** on disk. It is most commonly used when a file that should have been ignored (such as a `.env` credentials file, an IDE folder, or compiled build output) was accidentally committed to the repository, and the developer wants to stop tracking it without deleting their local configuration.
</details>

<details>
<summary><strong>2. Does Git have a "rename" object type in its internal database? How does Git track renames?</strong></summary>

No. Git does not have a rename object type, nor does it store explicit rename metadata in commit objects. Git's tree objects record only the mapping of paths to blob hashes. When commands like `git diff`, `git log`, or `git status` run, Git calculates renames **dynamically** at runtime using heuristic algorithms. If a deleted path and an added path have identical blob hashes (or a similarity index exceeding a threshold, typically 50%), Git reports the operation as a rename.
</details>

<details>
<summary><strong>3. What is the difference between `git clean -fdx` and `git clean -fdX`?</strong></summary>

- `git clean -fdx` (lowercase x): Force-deletes **ALL** untracked files and directories, including those that match patterns in `.gitignore`. It resets the workspace to an absolute pristine state.
- `git clean -fdX` (uppercase X): Force-deletes **ONLY** untracked files and directories that match `.gitignore` (such as build artifacts, cache files, and dependency folders), leaving unignored untracked files (such as newly created scratch code) completely unharmed.
</details>

<details>
<summary><strong>4. Why does running `git mv file.txt File.txt` often fail on macOS and Windows? How do you solve it?</strong></summary>

macOS (APFS/HFS+ by default) and Windows (NTFS) use case-insensitive (though case-preserving) filesystems. To the underlying operating system, `file.txt` and `File.txt` point to the exact same file entry. When `git mv` attempts to rename it, the OS reports that the destination already exists. The standard solution is a two-step rename through an intermediary name:
```bash
git mv file.txt temp.txt && git mv temp.txt File.txt
```
Or using the force flag: `git mv -f file.txt File.txt`.
</details>

<details>
<summary><strong>5. What happens if you delete a file using the standard OS command `rm file.txt` versus `git rm file.txt`?</strong></summary>

- If you run `rm file.txt`: The file is deleted from your physical disk, but the deletion is **not staged** in `.git/index`. `git status` will show ` D file.txt` under "Changes not staged for commit". You must follow up with `git add file.txt` to stage the deletion.
- If you run `git rm file.txt`: The file is deleted from your disk AND the deletion is **immediately staged** in `.git/index` in a single atomic step.
</details>

<details>
<summary><strong>6. Can files deleted via `git clean -fd` be recovered using `git reflog`?</strong></summary>

No. `git clean` deletes untracked files that were never committed or staged in Git. Because their content was never stored in `.git/objects/` and no references pointed to them, Git has no record of their existence. Once deleted via `git clean`, they cannot be recovered through Git (recovery is only possible via OS undelete tools or filesystem block scanners).
</details>

<details>
<summary><strong>7. How can you configure Git to detect renames when content has changed drastically?</strong></summary>

By passing a custom similarity threshold to the `-M` flag. By default, Git requires 50% similarity (`-M50%`). If a file was refactored significantly during a move, you can lower the threshold:
```bash
git diff -M30%
git log -M30% --stat
```
</details>

<details>
<summary><strong>8. What is the danger of running `git rm -r --cached .`?</strong></summary>

Running `git rm -r --cached .` unstages every single file in the entire repository from Git's index. If committed, this deletes every file in the project from the repository's commit history moving forward. While physical files remain on disk, the next commit would represent a completely empty repository.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Untracking a File While Preserving Local Disk Copy</strong></summary>

**Task**: Create and commit a file named `credentials.json`. Use `git rm --cached` to stop tracking it, verify that it still exists on disk, and verify the staged deletion in `git status`.

**Solution**:
```bash
mkdir /tmp/file-mgmt-lab && cd /tmp/file-mgmt-lab
git init -b main
echo '{"token": "xyz"}' > credentials.json
git add credentials.json && git commit -m "feat: add config"

# Untrack from index
git rm --cached credentials.json

# Check status
git status -s
# Output: D  credentials.json
# ?? credentials.json (deleted in index, untracked on disk!)

# Verify file still exists on disk
cat credentials.json
# Output: {"token": "xyz"}
```
</details>

<details>
<summary><strong>Exercise 2: Atomic Renaming with `git mv`</strong></summary>

**Task**: Rename `credentials.json` to `config.template.json` using `git mv` and inspect the staged status.

**Solution**:
```bash
# Re-stage credentials.json first
git add credentials.json

# Rename atomically
git mv credentials.json config.template.json

git status -s
# Output: R  credentials.json -> config.template.json
```
</details>

<details>
<summary><strong>Exercise 3: Safe Untracked Cleanup with Dry Run</strong></summary>

**Task**: Create 3 dummy untracked files and an untracked directory. Perform a dry run with `git clean -nd`, then clean the workspace with `git clean -fd`.

**Solution**:
```bash
touch a.tmp b.tmp c.tmp
mkdir scratch && touch scratch/junk.log

# 1. Dry run
git clean -nd
# Shows a.tmp, b.tmp, c.tmp, and scratch/

# 2. Force clean
git clean -fd

# Verify clean workspace
git status -s
# Output: R  credentials.json -> config.template.json (only tracked staged rename remains!)
```
</details>

<details>
<summary><strong>Exercise 4: Case-Only Rename Simulation</strong></summary>

**Task**: Create a file named `service.js`. Rename it to `Service.js` using the two-step workaround and commit the result.

**Solution**:
```bash
echo "console.log('service');" > service.js
git add service.js && git commit -m "add service.js"

# Two-step rename
git mv service.js temp.js
git mv temp.js Service.js

git status -s
# Output: R  service.js -> Service.js
git commit -m "refactor: capitalize Service.js"
```
</details>

<details>
<summary><strong>Exercise 5: Testing `git clean -fdX` (Purging Ignored Files Only)</strong></summary>

**Task**: Configure `.gitignore` to ignore `*.log`. Create `app.log` and an untracked file `notes.txt`. Run `git clean -fdX` and verify that `app.log` is deleted while `notes.txt` survives.

**Solution**:
```bash
echo "*.log" > .gitignore
git add .gitignore && git commit -m "ignore logs"

touch app.log notes.txt

# Purge ONLY ignored files
git clean -fdX

ls app.log 2>/dev/null || echo "app.log was cleanly deleted!"
ls notes.txt
# notes.txt survived intact!
```
</details>

<details>
<summary><strong>Exercise 6: Recursive Directory Deletion with `git rm -r`</strong></summary>

**Task**: Create a nested directory `legacy/modules/` with files, commit it, and remove the entire tree using `git rm -r`.

**Solution**:
```bash
mkdir -p legacy/modules
echo "old" > legacy/modules/old.js
git add legacy && git commit -m "add legacy code"

# Recursive deletion
git rm -r legacy/

git status -s
# Output: D  legacy/modules/old.js
git commit -m "chore: purge legacy directory"
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Command | Action | Impact on Disk |
| :--- | :--- | :--- |
| `git rm <file>` | Removes file from index & disk | File physically deleted |
| `git rm -r <dir>` | Recursively removes directory from index & disk | Folder physically deleted |
| `git rm --cached <file>` | Removes from index; keeps physical file | File preserved on disk |
| `git mv <old> <new>` | Renames file in index & disk | Atomic rename |
| `git mv -f <old> <new>` | Forces rename across case-insensitive filesystems | Case-only rename |
| `git clean -n` | Previews files that would be deleted (dry run) | Zero disk modification |
| `git clean -fd` | Force deletes all untracked files & directories | Permanently deletes untracked |
| `git clean -fdx` | Force deletes untracked AND ignored files | Complete clean slate |
| `git clean -fdX` | Force deletes ONLY ignored files | Preserves untracked code |

---

## 13. Further Reading & Official Documentation

- [Official Git Documentation - git-rm](https://git-scm.com/docs/git-rm)
- [Official Git Documentation - git-mv](https://git-scm.com/docs/git-mv)
- [Official Git Documentation - git-clean](https://git-scm.com/docs/git-clean)
- [Pro Git Book - Removing Files](https://git-scm.com/book/en/v2/Git-Basics-Recording-Changes-to-the-Repository#_removing_files)

---

[<- 04-undoing-changes.md](./04-undoing-changes.md) | [Back to Module Overview](./README.md) | [06-branching.md ->](./06-branching.md)
