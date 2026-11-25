<!--
================================================================================
Metadata:
  Title: Stashing Internals & Advanced Workflows: git stash
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 10-tracking-upstream.md
  Key Terminology: git stash push, git stash pop vs apply, Stash Commit Anatomy, Three-Parent Stash, git stash -p, git stash branch, --keep-index
================================================================================
-->

# Stashing Internals & Advanced Workflows: git stash

[<- 10-tracking-upstream.md](./10-tracking-upstream.md) | [Back to Module Overview](./README.md) | [12-tags-and-releases.md ->](./12-tags-and-releases.md)

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

In software engineering, urgent context switches are frequent: you are mid-way through implementing a complex feature with uncommitted, non-compiling code across four files when a critical P0 production outage strikes. You must switch branches immediately to investigate and hotfix the bug.

If you attempt to switch branches while your working tree has uncommitted edits that conflict with the target branch, Git blocks the switch:
```text
error: Your local changes to the following files would be overwritten by checkout:
Please commit your changes or stash them before you switch branches.
```

Creating a messy "wip - broken" commit clutters repository history and risks accidentally pushing incomplete work. Discarding the changes loses hours of effort.

**`git stash`** solves this dilemma. It takes the dirty, uncommitted state of your working directory (both staged modifications and unstaged edits) and saves it onto a local LIFO (Last-In, First-Out) stack, reverting your working tree and staging index back to a pristine `HEAD` state. When your context switch is complete, you can restore your in-flight work with a single command.

---

## 2. Core Concepts

### Pop vs. Apply
When retrieving work from the stash stack:
- **`git stash apply`**: Re-applies the stashed changes onto your current working tree, **but keeps the stash entry on the stash stack**. (Safest: if applying causes unexpected merge conflicts, your original stash snapshot remains safely stored in the stack!).
- **`git stash pop`**: Re-applies the stashed changes onto your working tree **AND immediately deletes** the stash entry from the stack (if application succeeds cleanly).

### Managing Stash Scope
By default, `git stash` only stores **tracked modifications**.
- Untracked files (new files you created but haven't run `git add` on) are left behind in your working tree!
- To stash untracked files as well, you must explicitly pass **`-u` / `--include-untracked`**.
- To stash untracked files *and* ignored files (e.g. build outputs), pass **`-a` / `--all`**.

---

## 3. How It Works Internally

### The Stash Commit Anatomy: A Multi-Parent Marvel
A common myth is that `git stash` writes changes to some temporary stash text file outside Git. In reality, **a stash is an actual commit object stored in `.git/objects/`**, referenced by the reflog of `.git/refs/stash`!

In fact, a stash is a special **multi-parent commit** that separates your staged index changes from your unstaged working tree changes:

- **Commit $W$ (The Stash Tip)**: Holds the state of the **Working Tree** at stash time.
- **Parent 1 ($H$)**: The `HEAD` commit where you were standing when you stashed.
- **Parent 2 ($I$)**: A commit object recording the state of the **Staging Area (Index)** when stashed.
- **Parent 3 ($U$)** *(optional)*: A commit object recording all **Untracked files** (created when `-u` is passed).

```
+-----------------------------------------------------------------------------------+
|                            Anatomy of a Git Stash Object                          |
|                                                                                   |
|                               [ Stash Commit (W) ] <──── .git/refs/stash          |
|                               (Working Tree State)                                |
|                                 │       │       │                                 |
|               ┌─────────────────┘       │       └─────────────────┐               |
|               ▼                         ▼                         ▼               |
|        Parent 1: [ HEAD ]        Parent 2: [ Index (I) ]   Parent 3: [ Untracked ]|
|        (Base Commit at           (Staged changes at        (New untracked files   |
|         stash time)               stash time)               when -u is passed)    |
+-----------------------------------------------------------------------------------+
```

Because Git isolates the staged index state into Parent 2 ($I$), you can run `git stash pop --index` to restore both your working tree changes *and* your staged index changes to their exact previous trees!

---

## 4. Syntax & Usage

### 1. Stashing Operations
```bash
# Standard stash with descriptive label (modern syntax replacing 'git stash save')
git stash push -m "WIP: refactoring user session token parsing"

# Stash including all untracked files (-u / --include-untracked)
git stash push -u -m "WIP: auth module with new untracked files"

# Stash everything including files ignored by .gitignore (-a / --all)
git stash push -a -m "WIP: complete project snapshot including node_modules"

# Interactive patch stashing: stash only specific hunks (-p / --patch)
git stash push -p -m "Stash only billing debug lines"

# Stash working tree while leaving staged index files untouched (--keep-index)
git stash push --keep-index -m "Stash unstaged edits only"
```

---

### 2. Inspection & Restoration
```bash
# List all stashed entries on the stack
git stash list

# View summary diff of the latest stash (stash@{0})
git stash show

# View full line-by-line unified diff of a specific stash
git stash show -p stash@{1}

# Apply latest stash and remove from stack
git stash pop

# Apply latest stash and RESTORE staged index states (--index)
git stash pop --index

# Apply a specific stash while keeping it on the stack
git stash apply stash@{2}

# Delete a specific stash entry
git stash drop stash@{1}

# Delete ALL stash entries on the stack
git stash clear

# Create a new branch directly from a stash entry (ideal for resolving stash conflicts!)
git stash branch feature-recovered-stash stash@{0}
```

---

## 5. Practical Examples

### Example 1: Emergency P0 Hotfix Workflow
Walk through the classic developer scenario:

```bash
# 1. You are working on feature-x, files are dirty and uncommitted
git status -s
# Output: M  src/api.ts, ?? src/temp.ts

# 2. Urgent hotfix needed on production main! Stash all work including untracked:
git stash push -u -m "WIP: feature-x half-implemented api"

# 3. Working directory is now 100% clean!
git status
# Clean!

# 4. Switch to main, create hotfix branch, commit and push fix:
git switch -c hotfix-auth main
echo "hotfix" >> src/security.ts && git commit -am "fix: patch auth vulnerability"
git switch main && git merge --no-ff hotfix-auth

# 5. Return to feature branch and restore stashed work:
git switch feature-x
git stash pop

# Your in-flight work is restored seamlessly!
git status -s
```

### Example 2: Interactive Hunk Stashing (`git stash -p`)
Suppose you made two edits to `worker.py`: one is a temporary print debug statement and the other is a genuine fix. You want to stash the debug print without touching the fix:

```bash
git stash push -p -m "temporary print statements"
# Git prompts hunk-by-hunk:
# Press 'y' to stash the print statement, 'n' to keep the genuine fix in working tree!
```

### Example 3: Solving Stash Pop Conflicts with `git stash branch`
Sometimes running `git stash pop` triggers massive merge conflicts because the branch has drifted significantly since you stashed. Git provides a purpose-built escape hatch:

```bash
# Attempt pop triggers conflict:
# git stash pop -> CONFLICT!

# Instead: create a brand new branch starting from the exact commit where you stashed!
git stash branch recovery-feature stash@{0}
```
*What happens:* Git creates and checks out a new branch named `recovery-feature` based on the parent commit of the stash, applies the stash cleanly with **zero conflicts**, and drops the stash from the stack!

### Example 4: Testing Staged Changes via `--keep-index`
Ensure your staged changes compile and pass unit tests before committing, without losing your unstaged work:

```bash
# Stage the code you intend to commit
git add src/feature.ts

# Stash your unstaged scratch work while keeping staged changes in the index
git stash push --keep-index -m "scratch work"

# Run your test suite against ONLY what is staged!
npm test

# If tests pass, commit staged changes:
git commit -m "feat: verified feature passing all tests"

# Restore your unstaged scratch work:
git stash pop
```

---

## 6. Best Practices

1. **Always Label Stashes with `-m "message"`**: An unlabelled stash list (`stash@{0}: WIP on main...`) becomes completely unrecognizable after a few days. Always give stashes clear, descriptive names.
2. **Prefer `git stash apply` Over `git stash pop` When Unsure**: If applying a stash causes complex merge conflicts, `pop` does not drop the stash, but your working tree becomes dirty. Using `apply` ensures you can easily abort and inspect the stash again.
3. **Always Use `-u` When Creating New Files**: Remember that standard `git stash` ignores untracked files. If you added new files to your project, use `git stash -u` so they aren't left behind.
4. **Do Not Treat the Stash as Long-Term Storage**: The stash stack is intended for short context switches (minutes to hours). Stashes stored for months often fail to apply cleanly due to codebase drift. For long-term work, create a named feature branch!
5. **Use `git stash branch` for Conflicted Stashes**: Never spend hours fighting conflict markers during a stash pop—create a branch directly from the stash instead.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| Newly created files still appear in `git status` after stashing | Standard `git stash` ignores untracked files. | Run `git stash push -u -m "msg"` to include untracked files. |
| `git stash pop` results in merge conflicts and stash remains on stack | Git refuses to drop the stash entry if conflict resolution is required. | Resolve conflicts manually, stage with `git add`, and run `git stash drop stash@{0}` once clean. |
| `fatal: unknown option 'save'` | `git stash save` was deprecated in favor of `git stash push`. | Use modern syntax: `git stash push -m "msg"`. |
| Stashed changes restored, but previously staged files are now unstaged | Standard `git stash pop` applies changes to working tree only. | Use `git stash pop --index` to restore staging index status. |
| Stash stack contains 40 unlabelled entries and you can't find your work | Never labelled stashes or cleared old entries. | Search stashes by content: `git stash list --grep="keyword"` or inspect diffs with `git stash show -p stash@{N}`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Zero File Copying**: Stashing does not copy physical files into backup folders. It creates standard Git tree and commit objects in `.git/objects/`, taking **<10 milliseconds** even on projects with 50,000 files.

### Security
- **Accidental Secret Persistence**: Developers frequently stash `.env` files or temporary API tokens. Even if you switch branches or push, those credentials remain stored in plain text inside `.git/objects/`. Periodically audit and clean your stash stack via `git stash clear`.

---

## 9. Comparison Table

| Attribute | `git stash pop` | `git stash apply` | `git stash branch <name>` |
| :--- | :--- | :--- | :--- |
| **Applies Changes?** | Yes | Yes | Yes (in new branch) |
| **Drops from Stack?** | Yes (if clean) | No (retained on stack) | Yes (drops upon success) |
| **Conflict Handling** | Leaves dirty working tree | Leaves dirty working tree | Zero conflicts (starts from base) |
| **Safe for Experiments?** | Low (deletes record) | High (record preserved) | Highest (completely isolated) |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. How is a Git stash represented internally in the object database?</strong></summary>

A Git stash is not stored as a flat diff file. It is represented as an actual **multi-parent commit object** in `.git/objects/`, referenced by `.git/refs/stash` and its reflog.
- The primary stash commit represents the state of the **Working Tree**.
- **Parent 1** points to the `HEAD` commit at the time of stashing.
- **Parent 2** points to a separate commit object capturing the state of the **Staging Area (Index)**.
- **Parent 3** (if `-u` was used) points to a commit capturing all **Untracked files**.
This multi-parent architecture allows Git to independently reconstruct working tree, staged index, and untracked file states upon restoration.
</details>

<details>
<summary><strong>2. What is the difference between `git stash pop` and `git stash apply`?</strong></summary>

- `git stash apply`: Applies the changes recorded in the stash entry onto the current working directory, but **leaves the stash entry intact on the stash stack**.
- `git stash pop`: Applies the changes recorded in the stash entry onto the current working directory, and **automatically deletes (drops) the entry from the stash stack**, provided the application succeeds cleanly without merge conflicts.
</details>

<details>
<summary><strong>3. Why does running `git stash` often leave newly created files behind in the working tree?</strong></summary>

By default, `git stash` only captures **tracked files** (files that have been modified or staged). Newly created files that have never been added to Git's index are classified as "untracked". To force Git to stash untracked files alongside tracked files, you must explicitly pass the `-u` or `--include-untracked` flag (`git stash push -u`).
</details>

<details>
<summary><strong>4. What does the `--keep-index` option do in `git stash` and what is a common use case?</strong></summary>

`git stash push --keep-index` stashes all uncommitted changes from the working tree, but **leaves all changes currently in the staging index untouched**. A common use case is verifying that staged changes pass tests independently before committing: you stage the files you want to commit, stash everything else with `--keep-index`, run your test suite against the staged code, commit, and then restore the unstaged work via `git stash pop`.
</details>

<details>
<summary><strong>5. What does `git stash branch <branchname> [<stash>]` do and when is it useful?</strong></summary>

`git stash branch <branchname>` creates and checks out a new branch starting from the exact commit where the stash was originally created, and then applies the stash onto it. Because the new branch starts from the exact commit state where the files were originally modified, the stash applies cleanly with **zero merge conflicts**. It is the ideal recovery command when running `git stash pop` on a diverged branch causes massive conflict markers.
</details>

<details>
<summary><strong>6. How does `git stash pop --index` differ from standard `git stash pop`?</strong></summary>

Standard `git stash pop` restores all stashed changes as **unstaged working tree modifications**, even if some of those changes were staged in the index when the stash was originally created. Passing the `--index` flag instructs Git to reconstruct the exact staging status, re-staging files that were previously in the index.
</details>

<details>
<summary><strong>7. How do you view the full unified diff of an older stash entry without applying it?</strong></summary>

By passing the `-p` (patch) flag to `git stash show`:
```bash
git stash show -p stash@{2}
```
This prints the complete unified line-by-line diff of that specific stash entry.
</details>

<details>
<summary><strong>8. What is the difference between `git stash push` and the legacy `git stash save`?</strong></summary>

`git stash save` was deprecated because it did not support pathspec filtering (stashing specific files) and had ambiguous argument parsing when options were supplied. Modern `git stash push` fully supports pathspecs (e.g. `git stash push -m "msg" -- src/controllers/`), interactive patch staging (`-p`), and structured flags.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Basic Stash Push and Pop with Descriptive Label</strong></summary>

**Task**: Modify a tracked file, stash it with a descriptive message, verify that working tree is clean, and restore the work with `git stash pop`.

**Solution**:
```bash
mkdir /tmp/stash-drill && cd /tmp/stash-drill
git init -b main
echo "Initial code" > app.js && git add app.js && git commit -m "init"

# Make modification
echo "console.log('wip');" >> app.js

# Stash with message
git stash push -m "WIP: added debug logging"

# Verify clean state
git status -s
# Clean!

# Check list
git stash list
# Shows: stash@{0}: On main: WIP: added debug logging

# Restore
git stash pop
cat app.js
# Confirms modification restored!
```
</details>

<details>
<summary><strong>Exercise 2: Stashing Untracked Files with `-u`</strong></summary>

**Task**: Create an untracked file `new-script.py`. Verify that standard `git stash` ignores it. Then stash it successfully using `-u`.

**Solution**:
```bash
echo "print('untracked')" > new-script.py

# Standard stash ignores untracked files
git stash push -m "test standard"
git status -s
# new-script.py is STILL in working tree!

# Stash including untracked:
git stash push -u -m "stashing untracked files"
git status -s
# Clean! new-script.py was safely stashed!

# Restore
git stash pop
ls new-script.py
# Restored!
```
</details>

<details>
<summary><strong>Exercise 3: Preserving Staged State with `--index`</strong></summary>

**Task**: Stage one change, leave another change unstaged. Stash. Pop with `--index` and verify that the staged change returns to the staged index.

**Solution**:
```bash
echo "staged line" >> app.js && git add app.js
echo "unstaged line" >> app.js

# Stash
git stash push -m "mixed staging test"

# Pop preserving index
git stash pop --index

git status -s
# Output: MM app.js (Both staged and unstaged states preserved!)
```
</details>

<details>
<summary><strong>Exercise 4: Creating a Branch from a Stash</strong></summary>

**Task**: Stash changes, and use `git stash branch recovery-branch` to branch off the stash.

**Solution**:
```bash
echo "experimental feature" >> app.js
git stash push -m "feature draft"

# Create branch from stash
git stash branch recovery-branch

git branch --show-current
# Output: recovery-branch
git status -s
# Output:  M app.js
```
</details>

<details>
<summary><strong>Exercise 5: Inspecting Stash Contents with `git stash show -p`</strong></summary>

**Task**: View the exact unified diff of the latest stash on the stack without applying it.

**Solution**:
```bash
git stash push -m "diff test"
git stash show -p stash@{0}
# Prints full colorized diff!
git stash drop stash@{0}
```
</details>

<details>
<summary><strong>Exercise 6: Interactive Hunk Stashing (`-p`)</strong></summary>

**Task**: Edit two distinct parts of a file. Use `git stash push -p` to stash only one hunk while keeping the other in the working tree.

**Solution**:
```bash
seq 1 10 > seq.txt && git add seq.txt && git commit -m "add seq"
sed -i 's/2/TWO/' seq.txt
sed -i 's/9/NINE/' seq.txt

# Interactively stash
git stash push -p -m "stash two only"
# Press 's' to split, 'y' to stash TWO, 'n' to keep NINE in working tree

git diff seq.txt
# Shows only NINE remaining in working tree!
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Command | Role / Target |
| :--- | :--- |
| `git stash push -m "msg"` | Stashes tracked modifications with custom label |
| `git stash push -u -m "msg"` | Stashes tracked modifications AND untracked files |
| `git stash push -a -m "msg"` | Stashes tracked, untracked, AND ignored files |
| `git stash push -p` | Interactively select specific hunks to stash |
| `git stash push --keep-index` | Stashes working tree changes while leaving index staged |
| `git stash list` | Displays all stashes on the stack |
| `git stash show -p stash@{N}` | Prints full line-by-line diff of stash entry |
| `git stash pop` | Applies latest stash and drops from stack |
| `git stash pop --index` | Applies latest stash and restores staging index states |
| `git stash apply stash@{N}` | Applies specific stash without removing from stack |
| `git stash branch <name> <stash>`| Creates new branch from stash parent and applies cleanly |
| `git stash drop stash@{N}` | Permanently deletes a specific stash entry |
| `git stash clear` | Wipes entire stash stack |

---

## 13. Further Reading & Official Documentation

- [Official Git Documentation - git-stash](https://git-scm.com/docs/git-stash)
- [Pro Git Book - Git Stashing and Cleaning](https://git-scm.com/book/en/v2/Git-Tools-Stashing-and-Cleaning)
- [Git Internals - Stash Objects](https://git-scm.com/book/en/v2/Git-Internals-Git-Objects)

---

[<- 10-tracking-upstream.md](./10-tracking-upstream.md) | [Back to Module Overview](./README.md) | [12-tags-and-releases.md ->](./12-tags-and-releases.md)
