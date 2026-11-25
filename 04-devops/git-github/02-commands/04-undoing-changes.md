<!--
================================================================================
Metadata:
  Title: Undoing Changes: git restore, git reset & git revert
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 55 minutes
  Prerequisites: 03-add-commit.md
  Key Terminology: git restore, git reset (--soft, --mixed, --hard), git revert, Mainline Parent (-m 1), Safe Undos vs Destructive Undos, Three Trees Undo
================================================================================
-->

# Undoing Changes: git restore, git reset & git revert

[<- 03-add-commit.md](./03-add-commit.md) | [Back to Module Overview](./README.md) | [05-clean-rm-mv.md ->](./05-clean-rm-mv.md)

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

One of the greatest strengths of Git is that almost nothing is truly lost once committed to the object database. However, for many developers, knowing *how* to undo a mistake without destroying unrelated work is the most stressful aspect of using Git.

Mistakes occur across all layers of the Git workflow:
- You edited a file on disk and broke it, and need to revert back to clean code.
- You accidentally staged a 200MB binary or secret file and need to unstage it.
- You committed code too early with typos or missing files.
- A teammate pushed a broken commit to the production `main` branch, and you must roll it back immediately without rewriting history or confusing other developers.

To handle these distinct scenarios safely, Git provides three distinct primitives:
- **`git restore`**: Restores working tree files or unstages index entries without modifying commit history.
- **`git reset`**: Rewinds branch pointers backwards across commits (private local history).
- **`git revert`**: Creates a brand-new commit that applies the exact inverse diff of an earlier commit (public shared history).

---

## 2. Core Concepts

### The Three Primitives: restore vs. reset vs. revert

1. **`git restore` (Scope: Files across Working Tree & Index)**:
   - Does NOT touch the commit DAG or move branch pointers.
   - Reverts physical files on disk or un-stages files in `.git/index`.
   - Replaces the dangerous, overloaded file usage of legacy `git checkout -- <file>`.
2. **`git reset` (Scope: Local Branch Pointers & Index)**:
   - Rewinds the active branch reference backward to an earlier commit in the DAG.
   - **Rewrites local history**. Safe ONLY on private feature branches that have not been pushed to a remote!
   - Can operate in `--soft`, `--mixed`, or `--hard` mode.
3. **`git revert` (Scope: Public Commit DAG)**:
   - Does NOT rewrite history.
   - Records a **new forward commit** that mathematically cancels out the changes introduced by a past commit.
   - Safe for public, shared branches (`main`, `production`) because it preserves linear history without requiring force-pushes.

```
+-----------------------------------------------------------------------------------+
|                           The Undoing Decision Tree                               |
|                                                                                   |
|  Where does the change you want to undo exist?                                    |
|                                                                                   |
|  1. In the WORKING TREE only (unstaged)?                                          |
|     └──► git restore <file>                                                       |
|                                                                                   |
|  2. In the STAGING AREA (Index)?                                                  |
|     └──► git restore --staged <file>                                              |
|                                                                                   |
|  3. In a LOCAL COMMIT (not yet pushed to remote)?                                 |
|     ├── Keep changes staged?       ──► git reset --soft HEAD~1                    |
|     ├── Keep changes unstaged?     ──► git reset --mixed HEAD~1 (default)         |
|     └── Destroy changes completely?──► git reset --hard HEAD~1                    |
|                                                                                   |
|  4. In a PUSHED COMMIT (shared with team / production)?                           |
|     └──► git revert <commit-hash> (Creates safe inverse commit!)                  |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### The Three Modes of `git reset`
Understanding `git reset` requires analyzing which of the Three Trees are altered:

```
Command                  HEAD Pointer Moved?    Index Updated?    Working Tree Updated?
---------------------------------------------------------------------------------------
git reset --soft HEAD~1         YES                   NO                    NO
git reset --mixed HEAD~1        YES                  YES                    NO
git reset --hard HEAD~1         YES                  YES                   YES (Destructive!)
```

```
BEFORE RESET:
HEAD -> [Commit B: 2222] (parent: [Commit A: 1111])
Index: Matches Commit B
Working Tree: Matches Commit B

1. git reset --soft HEAD~1:
   - HEAD -> [Commit A: 1111]
   - Index: REMAINS at Commit B (Changes from B are STAGED!)
   - Working Tree: REMAINS at Commit B

2. git reset --mixed HEAD~1 (Default):
   - HEAD -> [Commit A: 1111]
   - Index: RESET to Commit A (Changes from B are UNSTAGED!)
   - Working Tree: REMAINS at Commit B (Your code edits are safe on disk!)

3. git reset --hard HEAD~1:
   - HEAD -> [Commit A: 1111]
   - Index: RESET to Commit A
   - Working Tree: OVERWRITTEN to Commit A (All uncommitted edits in B are DESTROYED!)
```

### The Inversion Mechanics of `git revert`
When you execute `git revert <commit-hash>`, Git does not delete the commit. Instead:
1. Git calculates the patch that created `<commit-hash>` ($P = Commit - Parent$).
2. Git computes the inverse patch ($-P$: lines added become deleted, lines deleted become added).
3. Git applies $-P$ onto the tip of the current branch.
4. Git creates a new commit object: `"Revert 'feat: add broken feature'"`.

---

## 4. Syntax & Usage

### 1. Modern File Restoration (`git restore`)
```bash
# Discard unstaged changes in working tree (reverts file to index/HEAD state)
git restore src/index.ts

# Discard unstaged changes across ALL files in current directory
git restore .

# Unstage a staged file (keeps file modified on disk)
git restore --staged secrets.env

# Completely restore file, discarding BOTH staged and unstaged edits
git restore --source=HEAD --staged --worktree src/index.ts
```

---

### 2. Rewinding History (`git reset`)
```bash
# Soft Reset: Undo commit, keeping changes staged in index
git reset --soft HEAD~1

# Mixed Reset (default): Undo commit, keeping changes in working tree as unstaged edits
git reset HEAD~1

# Hard Reset: Undo commit and completely wipe working tree and index changes
git reset --hard HEAD~1

# Reset branch pointer to match remote upstream exactly
git reset --hard origin/main
```

---

### 3. Safe Public Undos (`git revert`)
```bash
# Revert a single commit (opens editor for revert commit message)
git revert 8a1e2f7

# Revert without prompting for commit message
git revert --no-edit 8a1e2f7

# Revert a range of commits (from oldest to newest)
git revert --no-commit HEAD~3..HEAD
git commit -m "chore: rollback releases 1.2 through 1.4"

# Revert a merge commit (requires specifying mainline parent -m)
git revert -m 1 <merge-commit-hash>
```

---

## 5. Practical Examples

### Example 1: Soft Reset to Re-craft a Commit
You committed code, but realized you forgot to include tests and the commit message had typos:

```bash
# Make mistake commit
echo "feature code" > feature.ts
git add feature.ts && git commit -m "wip"

# Soft reset moves HEAD back, keeping feature.ts staged!
git reset --soft HEAD~1

# Add missing test file
echo "test code" > feature.test.ts
git add feature.test.ts

# Re-commit cleanly
git commit -m "feat(core): implement feature with complete unit tests"
```

### Example 2: Recovering from an Accidental `git reset --hard`
What happens if you accidentally run `git reset --hard HEAD~1` and lose your latest commit? **Don't panic!** The commit still exists in `.git/objects/`:

```bash
# Check reflog for recent HEAD movements
git reflog -n 5
```
Output:
```text
3b4a5c6 HEAD@{0}: reset: moving to HEAD~1
8f1e2d4 HEAD@{1}: commit: feat: critical algorithm logic
```
Restore the commit instantly:
```bash
# Jump directly back to the commit before the reset!
git reset --hard HEAD@{1}
# Or:
# git reset --hard 8f1e2d4
```

### Example 3: Reverting a Public Merge Commit (`-m 1`)
A feature branch was merged into `main` via PR, but caused an immediate production outage. You must revert the merge commit:

```bash
# Identify merge commit hash
MERGE_HASH=$(git rev-parse HEAD)

# Attempting standard revert fails:
# git revert $MERGE_HASH
# Output: fatal: commit is a merge but no -m option was given.

# Revert specifying parent 1 (the mainline branch: main)
git revert -m 1 $MERGE_HASH -m "revert: rollback broken feature merge (#482)"

# Push the revert forward safely!
git push origin main
```

### Example 4: Cleaning Up Untracked Garbage (`git clean`)
When builds create untracked artifacts (`.o`, `.pyc`, build dirs):

```bash
# 1. ALWAYS perform a dry run first to see what WOULD be deleted!
git clean -nd

# 2. Force deletion of untracked files and directories (-f force, -d directories)
git clean -fd

# 3. Force deletion including ignored files (-x)
git clean -fdx
```

### Example 5: Restoring a Single Deleted File
You accidentally deleted `src/auth.ts` from disk using `rm src/auth.ts`:

```bash
rm src/auth.ts
git status -s
# Output:  D src/auth.ts

# Restore the deleted file from HEAD
git restore src/auth.ts

ls src/auth.ts
# Restored intact!
```

---

## 6. Best Practices

1. **Never `git reset --hard` on Public Branches**: Rewriting history on shared branches (`main`, `develop`) forces teammates to reconcile divergent histories. Always use `git revert` on pushed commits.
2. **Always Dry-Run `git clean`**: Never run `git clean -fd` blindly. Always execute `git clean -nd` first to verify you aren't deleting uncommitted new source files.
3. **Use `git reset --soft` to Combine Local Commits**: Before pushing a branch, use `git reset --soft HEAD~N` to collapse $N$ messy local commits into a single staged snapshot.
4. **Use `ORIG_HEAD` for Immediate Rollback**: If you ran `git reset --hard` by mistake, immediately run `git reset --hard ORIG_HEAD`.
5. **Understand Reverting a Reverted Merge**: If you revert a merge commit (`git revert -m 1`), Git cancels the code changes, but the branch history still considers those commits merged. To re-introduce the branch later, you must revert the revert!

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `fatal: commit ... is a merge but no -m option was given` | Attempted to run `git revert` on a merge commit without selecting the mainline parent. | Specify parent number: `git revert -m 1 <commit-hash>`. |
| Ran `git reset --hard` and lost uncommitted working tree edits | `git reset --hard` overwrites the working tree. Uncommitted changes were not in the object store. | Check editor's local history (VS Code Timeline); uncommitted edits cannot be retrieved from Git. |
| Re-merging a branch after reverting it introduces zero code changes | Git sees the branch commits already exist in history. | Revert the revert commit: `git revert <revert-commit-hash>`. |
| Pushed after `git reset` and push was rejected | Rewound local branch; remote is ahead. | If private branch, push with `git push --force-with-lease`. If public, use `git revert`. |
| `git restore --staged` did not change the file on disk | Working tree files are intentionally preserved when unstaging. | Run `git restore <file>` if you also want to discard local edits. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Reflog Safety Window**: Objects unlinked by `git reset` remain accessible in `.git/objects/` for 90 days (`gc.reflogExpire`, default 90 days for reachable, 30 days for unreachable). You have weeks to recover lost commits before garbage collection runs.

### Security
- **Hard Deletion of Leaked Secrets**: Neither `git reset` nor `git revert` deletes a secret from repository history! `git revert` merely adds a new commit removing the secret, leaving the secret visible in historical commits. Removing secrets requires `git-filter-repo` (Module 03).

---

## 9. Comparison Table

| Feature / Attribute | `git restore` | `git reset` | `git revert` |
| :--- | :--- | :--- | :--- |
| **Primary Target** | Individual files / paths | Branch pointers & Index | Past commits |
| **Modifies Commit History?**| No | Yes (rewinds branch pointer)| No (appends new commit) |
| **Safe for Public Branches?**| Yes (local only) | NO (local only!) | YES (recommended) |
| **Can Recover Lost Edits?** | No (for unstaged edits) | Yes (via `git reflog`) | Yes (clean commit history) |
| **Requires Force Push?** | No | Yes (if previously pushed) | No (standard forward push)|

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain the difference between `git reset` and `git revert`. When should each be used?</strong></summary>

- `git reset` moves the current branch pointer backwards to an earlier commit in the DAG. It rewrites commit history by abandoning intermediate commits. It should be used **only on private local branches** before pushing to a remote.
- `git revert` records a brand-new commit that applies the exact inverse diff of a targeted past commit. It does not alter existing history. It should be used on **public, shared branches** (`main`, `production`) to safely roll back changes without disrupting teammates or requiring force pushes.
</details>

<details>
<summary><strong>2. What is the difference between `git reset --soft`, `--mixed`, and `--hard`?</strong></summary>

- `--soft`: Moves the `HEAD` and branch pointer to the target commit. Leaves the **Staging Area (Index)** and **Working Tree** untouched. All changes from the undone commits remain staged in the index ready for re-committing.
- `--mixed` (default): Moves `HEAD` and branch pointer, and resets the **Staging Area (Index)** to match the target commit. Leaves the **Working Tree** untouched. Changes from undone commits remain on disk as unstaged modifications.
- `--hard`: Moves `HEAD`, resets the **Staging Area (Index)**, and resets the **Working Tree**. All modifications in the working tree and index are completely overwritten to match the target commit.
</details>

<details>
<summary><strong>3. Why does reverting a merge commit require the `-m` (mainline) flag?</strong></summary>

A standard commit has exactly one parent, so Git automatically knows what diff to invert. A merge commit has two (or more) parents: Parent 1 (the branch that was merged into, usually `main`) and Parent 2 (the topic branch being merged). Git cannot guess which branch represents the "mainline" you wish to keep and which side represents the changes to invert. Passing `-m 1` specifies that Parent 1 is the mainline, instructing Git to invert all changes introduced by Parent 2.
</details>

<details>
<summary><strong>4. If a developer accidentally executes `git reset --hard HEAD~1`, how can they recover their lost commit?</strong></summary>

By using the **Reflog** (`git reflog`). The reflog records every movement of `HEAD`.
1. Run `git reflog` to locate the commit hash before the reset occurred (e.g. `HEAD@{1}`).
2. Restore the commit by running `git reset --hard HEAD@{1}` or `git reset --hard <commit-hash>`.
</details>

<details>
<summary><strong>5. What is the difference between `git restore <file>` and `git restore --staged <file>`?</strong></summary>

- `git restore <file>`: Targets the **Working Tree**. It overwrites the physical file on disk with the version currently in the index (or HEAD). Any unstaged local edits to that file are permanently discarded.
- `git restore --staged <file>`: Targets the **Staging Area (Index)**. It removes the file from the index (copying state from HEAD), effectively unstaging it. The physical file on disk remains modified and untouched.
</details>

<details>
<summary><strong>6. What happens if you revert a revert commit (`git revert <revert-commit-hash>`)?</strong></summary>

Reverting a revert commit re-applies the original code changes. This is standard procedure when a feature was merged, reverted due to an issue, fixed in a follow-up commit, and needs to be re-introduced into the codebase without merge history conflicts.
</details>

<details>
<summary><strong>7. What is `git clean` and what does the `-fd` flag do?</strong></summary>

`git clean` removes untracked files from the working tree that are not managed by Git. The `-f` flag stands for force (required by default configuration to prevent accidental deletion), and `-d` instructs Git to remove whole untracked directories in addition to individual untracked files.
</details>

<details>
<summary><strong>8. Why is `git checkout -- <file>` discouraged in modern Git?</strong></summary>

In Git versions prior to 2.23, `git checkout` was heavily overloaded: it handled switching branches, creating branches, checking out detached heads, and discarding file changes. If a developer attempted to discard changes to a file named `main` (`git checkout -- main`) but made a syntax mistake, Git might switch branches instead. Git 2.23 introduced `git restore` for files and `git switch` for branches to cleanly separate these responsibilities and prevent accidental data loss.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Discarding Working Tree Changes with `git restore`</strong></summary>

**Task**: Modify a tracked file, verify the modification in `git status`, and discard the modification using `git restore`.

**Solution**:
```bash
mkdir /tmp/undo-lab && cd /tmp/undo-lab
git init -b main
echo "Original Content" > app.js && git add app.js && git commit -m "init"

# Break the file
echo "Broken Code" >> app.js
git status -s
# Output:  M app.js

# Discard edits
git restore app.js

cat app.js
# Confirms "Original Content" restored!
```
</details>

<details>
<summary><strong>Exercise 2: Safely Unstaging with `git restore --staged`</strong></summary>

**Task**: Stage an unwanted file. Unstage it without losing your local modifications.

**Solution**:
```bash
echo "Config" > config.json
git add config.json
git status -s
# Output: A  config.json

# Unstage
git restore --staged config.json

git status -s
# Output: ?? config.json (file preserved on disk!)
```
</details>

<details>
<summary><strong>Exercise 3: Comparing Soft, Mixed, and Hard Resets</strong></summary>

**Task**: Create 3 commits. Test `git reset --soft`, inspect status, commit again, test `git reset --mixed`, and finally test `git reset --hard`.

**Solution**:
```bash
echo "v1" > f.txt && git add f.txt && git commit -m "v1"
echo "v2" >> f.txt && git commit -am "v2"

# 1. Soft Reset
git reset --soft HEAD~1
git status -s
# Output: M  f.txt (staged!)

# Recommit
git commit -m "v2 again"

# 2. Mixed Reset
git reset --mixed HEAD~1
git status -s
# Output:  M f.txt (unstaged!)

# Recommit
git commit -am "v2 again"

# 3. Hard Reset
git reset --hard HEAD~1
git status -s
# Output: clean! v2 completely discarded.
```
</details>

<details>
<summary><strong>Exercise 4: Reflog Recovery from a Destructive Reset</strong></summary>

**Task**: Accidentally run `git reset --hard HEAD~1`. Locate the lost commit in `git reflog` and restore it.

**Solution**:
```bash
echo "v2" >> f.txt && git commit -am "v2 restored"
git reset --hard HEAD~1

# Check reflog
git reflog -n 2

# Restore using HEAD@{1}
git reset --hard HEAD@{1}
git log -1 --oneline
# Confirms v2 restored is back!
```
</details>

<details>
<summary><strong>Exercise 5: Safe Public Undo with `git revert`</strong></summary>

**Task**: Create a commit and roll it back using `git revert --no-edit`. Inspect `git log` to verify history is preserved.

**Solution**:
```bash
echo "Bad feature" > bad.txt && git add bad.txt && git commit -m "feat: bad feature"
git revert --no-edit HEAD

git log --oneline -2
# Shows:
# <hash> Revert "feat: bad feature"
# <hash> feat: bad feature
```
</details>

<details>
<summary><strong>Exercise 6: Cleaning Untracked Artifacts with Dry Run</strong></summary>

**Task**: Create several untracked files and directories. Dry run `git clean -nd`, then execute `git clean -fd`.

**Solution**:
```bash
touch temp1.tmp temp2.tmp
mkdir build && touch build/output.bin

# Dry run:
git clean -nd
# Shows what will be removed

# Execute clean:
git clean -fd
git status -s
# Clean!
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Modern Command | Scope / Impact |
| :--- | :--- | :--- |
| **Discard Unstaged File Edits** | `git restore <file>` | Overwrites disk file with index/HEAD version |
| **Unstage a Staged File** | `git restore --staged <file>` | Moves file from index back to unstaged; keeps edits |
| **Discard Staged & Unstaged** | `git restore --source=HEAD --staged --worktree <file>` | Completely resets file to latest commit |
| **Soft Reset (Keep Staged)** | `git reset --soft HEAD~1` | Undoes commit; leaves all changes staged in index |
| **Mixed Reset (Keep Unstaged)**| `git reset HEAD~1` | Undoes commit; leaves all changes unstaged on disk |
| **Hard Reset (Destroy Changes)**| `git reset --hard HEAD~1` | Undoes commit; wipes all working tree and index changes |
| **Recover Lost Reset** | `git reset --hard HEAD@{1}` | Restores commit using reflog pointer |
| **Revert Public Commit** | `git revert <hash>` | Records new inverse commit; safe for shared branches |
| **Revert Merge Commit** | `git revert -m 1 <hash>` | Reverts merge commit against mainline parent 1 |
| **Clean Untracked Files** | `git clean -fd` | Force deletes untracked files and directories |

---

## 13. Further Reading & Official Documentation

- [Official Git Documentation - git-restore](https://git-scm.com/docs/git-restore)
- [Official Git Documentation - git-reset](https://git-scm.com/docs/git-reset)
- [Official Git Documentation - git-revert](https://git-scm.com/docs/git-revert)
- [Pro Git Book - Undoing Things](https://git-scm.com/book/en/v2/Git-Basics-Undoing-Things)

---

[<- 03-add-commit.md](./03-add-commit.md) | [Back to Module Overview](./README.md) | [05-clean-rm-mv.md ->](./05-clean-rm-mv.md)
