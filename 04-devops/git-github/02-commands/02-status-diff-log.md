<!--
================================================================================
Metadata:
  Title: Inspection & Observability: git status, git diff & git log
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 55 minutes
  Prerequisites: 01-init-clone.md
  Key Terminology: git status -sb, XY Status Codes, git diff --staged, Two-Dot vs Three-Dot Diff, git log -S (Pickaxe), git log -L, --color-moved
================================================================================
-->

# Inspection & Observability: git status, git diff & git log

[<- 01-init-clone.md](./01-init-clone.md) | [Back to Module Overview](./README.md) | [03-add-commit.md ->](./03-add-commit.md)

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

In software development, making changes blindly without continuous observability guarantees errors: accidentally committing hardcoded test credentials, staging unintentional whitespace modifications, or misjudging where your branch diverged from upstream.

The three primary inspection commands—`git status`, `git diff`, and `git log`—form the **core observability triage loop** in Git:
- **`git status`** provides instantaneous situational awareness: What branch am I on? How does my local branch relate to upstream? What files have been modified, staged, or left untracked?
- **`git diff`** provides high-resolution line-by-line and character-by-character change analysis across the Three Trees (Working Tree, Index, and HEAD) or between arbitrary branches and tags.
- **`git log`** queries the historical commit graph: Who changed this function? When was this bug introduced? What commits exist on my feature branch that have not yet landed in `main`?

Mastering the advanced flags of these three commands transforms developer velocity from tentative guessing to surgical precision.

---

## 2. Core Concepts

### The Two-Letter XY Status Codes in `git status -s`
While standard `git status` outputs verbose prose, running `git status -s` (short format) outputs compact two-character status indicators: `XY PATH`.
- **Column X (Left Character)**: Represents the status of the **Staging Area (Index)** relative to `HEAD`.
- **Column Y (Right Character)**: Represents the status of the **Working Tree** relative to the **Staging Area (Index)**.

| Code | Meaning |
| :--- | :--- |
| `??` | Untracked file (never staged, not ignored) |
| ` M` | Modified in working tree, **NOT** staged |
| `M ` | Modified and **staged** in index (ready to commit) |
| `MM` | Modified, staged, and then modified **again** in working tree! |
| `A ` | Newly added file, staged in index |
| `AM` | Added and staged, then modified in working tree |
| ` D` | Deleted in working tree, deletion not staged |
| `D ` | Deleted and deletion staged in index |
| `UU` | Unresolved merge conflict (both modified) |

```
+---------------------------------------------------------------------------------+
|                          XY Status Column Dissection                            |
|                                                                                 |
|       X        Y       Path                                                     |
|       │        │                                                                |
|       ▼        ▼                                                                |
|     [Index]  [Disk]                                                             |
|       M        _       src/server.ts  <-- Staged in index; clean on disk        |
|       _        M       src/client.ts  <-- Clean in index; modified on disk      |
|       M        M       src/utils.ts   <-- Staged changes, PLUS newer disk edits! |
|       ?        ?       src/temp.log   <-- Completely untracked                  |
+---------------------------------------------------------------------------------+
```

### Two-Dot vs. Three-Dot Diffs
Understanding range notation in `git diff` is one of the most critical distinctions in Git:
- **Two-Dot Diff (`git diff branchA..branchB` or `git diff branchA branchB`)**:
  Compares the direct tips of `branchA` and `branchB`. It answers: *"What is the exact difference between the final snapshots of both branches right now?"*
- **Three-Dot Diff (`git diff branchA...branchB`)**:
  Calculates the **common ancestor** ($merge\_base$) of `branchA` and `branchB`, and compares the common ancestor against `branchB`. It answers: *"What changes were introduced on `branchB` since it diverged from `branchA`?"* **This is exactly how GitHub Pull Request diffs are calculated!**

```
Two-Dot: git diff A..B                Three-Dot: git diff A...B
(Compares Tip A to Tip B directly)   (Compares Merge-Base to Tip B)

      A (main)                             A (main)
     /                                    /
... o ──────────────┐               ... [o] <─── Merge Base (Calculated)
     \              │                    \
      B (feature)   │                     B (feature)
                    ▼                           │
              Direct Diff                       ▼
                                          Feature PR Diff
```

---

## 3. How It Works Internally

### The Myers vs. Histogram Diff Engines
When computing diffs, Git compares sequences of lines to find the Shortest Edit Script (SES).
- **Myers Algorithm (Default)**: Greedy algorithm that attempts to find the minimum number of insertions and deletions. However, for refactored code, Myers often aligns common syntactic tokens (like empty lines or single closing braces `}`) across unrelated functions, resulting in visually confusing, fragmented diffs.
- **Histogram Algorithm (`--diff-algorithm=histogram`)**: Prioritizes low-frequency (unique) lines as anchors first, and recursively diffs the regions between anchors. This produces vastly superior, logically coherent diffs when functions are refactored or moved.

### Commit History Walks in `git log`
When you execute `git log`, Git does not execute a linear scan across files. It performs a **topological graph walk** starting from `HEAD`. It reads the commit object, parses parent hashes, and inserts parent commits into a priority queue (ordered by commit timestamp or topological order `--topo-order`). This graph walk is accelerated by the binary **commit-graph** cache (`.git/objects/info/commit-graph`).

---

## 4. Syntax & Usage

### 1. `git status` Power Commands
```bash
# Clean, readable short format showing active branch and upstream sync status
git status -sb

# Show untracked files including files inside untracked directories (-uall)
git status -uall

# Show ignored files matching .gitignore patterns
git status --ignored
```

---

### 2. `git diff` Power Commands
```bash
# Working Tree vs Index (Unstaged edits)
git diff

# Index vs HEAD (Staged edits ready to commit)
git diff --staged

# Working Tree vs HEAD (All uncommitted edits combined)
git diff HEAD

# Word-by-word diff (highlights edited words inline rather than entire lines)
git diff --word-diff

# Highlight moved lines in a distinct color instead of delete/add
git diff --color-moved

# Ignore all whitespace and line ending differences
git diff -w --ignore-space-at-eol

# Summary stat showing files changed and inserted/deleted line counts
git diff --stat HEAD~3 HEAD
```

---

### 3. `git log` Power Commands
```bash
# Standard compact visual graph across all branches
git log --graph --oneline --decorate --all

# Filter commits by author (case-insensitive regex)
git log --author="Alex" -n 5

# Filter commits by date range
git log --since="2 weeks ago" --until="yesterday"

# Filter commits by commit message text
git log --grep="JIRA-1042"

# Pickaxe Search (-S): Find commits that added or removed the exact string "API_KEY"
git log -S "API_KEY" -p

# Regex Search (-G): Find commits whose diff hunks match a regex pattern
git log -G "def calculate_tax\(" -p

# Follow file history across renames
git log --follow -p -- src/config.ts

# Function-level history: trace changes to a specific function in a file
git log -L :validateUserSession:src/auth.ts
```

---

## 5. Practical Examples

### Example 1: Deconstructing the `MM` (Modified-Modified) Status Code
Understand what occurs when you stage a file and then make additional edits:

```bash
mkdir /tmp/status-lab && cd /tmp/status-lab
git init
echo "Line 1" > app.txt && git add app.txt && git commit -m "feat: initial"

# Make edit 1 and stage it
echo "Line 2 - Staged" >> app.txt
git add app.txt

# Make edit 2 WITHOUT staging it
echo "Line 3 - Unstaged" >> app.txt

# Inspect status
git status -sb
```
Output:
```text
## main
MM app.txt
```
Explanation:
- First `M` (Index column): `app.txt` has staged modifications relative to `HEAD`.
- Second `M` (Working tree column): `app.txt` has unstaged modifications relative to the Index!

### Example 2: Three-Dot Pull Request Comparison in Action
Demonstrate why `git diff main...feature` matches GitHub PR reviews:

```bash
# Create feature branch off main
git checkout -b feature-analytics
echo "analytics code" > analytics.py
git add analytics.py && git commit -m "feat: add analytics module"

# Meanwhile, main branch receives a commit from another developer
git checkout main
echo "bugfix on main" > bugfix.txt
git add bugfix.txt && git commit -m "fix: production patch on main"

# Two-dot diff: compares tips (shows both analytics.py AND bugfix.txt deletion!)
git diff main feature-analytics --stat
# Shows analytics.py added, bugfix.txt missing!

# Three-dot diff: compares merge-base to feature (shows ONLY the feature's PR diff!)
git diff main...feature-analytics --stat
# Shows ONLY analytics.py added (1 file changed, 1 insertion)!
```

### Example 3: Tracking Function Evolution with `git log -L`
Trace how a single function changed over time without seeing unrelated commits:

```bash
cat << 'EOF' > service.js
function calculateDiscount(price, user) {
    if (user.isVip) {
        return price * 0.20;
    }
    return price * 0.05;
}
EOF
git add service.js && git commit -m "feat: add discount calculation"

# Update function in second commit
sed -i 's/0.20/0.25/' service.js
git commit -am "fix: increase VIP discount to 25%"

# Trace function history across time:
git log -L :calculateDiscount:service.js
# Git displays only the diff hunks affecting calculateDiscount!
```

### Example 4: Diagnosing Code Moves with `--color-moved`
When refactoring large functions between files:

```bash
git diff --color-moved
# Moved code is rendered in distinct magenta/cyan blocks,
# visually proving to the reviewer that code was moved, not rewritten!
```

### Example 5: Pickaxe (`-S`) vs Regex (`-G`) Search
```bash
# Pickaxe (-S): Finds commits where the literal count of "processOrder" changed
git log -S "processOrder" --oneline

# Regex Search (-G): Finds commits where the diff added or removed lines matching a pattern
git log -G "^export async function" --stat
```

---

## 6. Best Practices

1. **Use `git status -sb` in Your Terminal Prompt or Aliases**: Eliminates clutter while clearly showing tracking differences (e.g. `## main...origin/main [ahead 2, behind 1]`).
2. **Review `git diff --staged` Before Every Single Commit**: Treat `--staged` as your pre-flight safety checklist.
3. **Always Use Three-Dot Diffs (`main...feature`) to Preview PRs**: Never use two-dot diffs to review a branch intended for a Pull Request, as two-dot diffs include divergent changes from the target branch.
4. **Use `--follow` When Inspecting Moved or Renamed Files**: Running `git log <path>` stops at the point a file was renamed. `git log --follow <path>` continues tracing history past the rename boundary.
5. **Set `diff.algorithm = histogram` Globally**: Produces significantly more readable diffs across refactored functions.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `git diff` shows no output after editing a file | Modifications were already staged with `git add`. | Run `git diff --staged` (or `--cached`). |
| Two-dot diff shows hundreds of files you never touched in your PR | Used `git diff main feature` instead of three-dot `git diff main...feature`. | Use three-dot notation `main...feature` to compare against merge base. |
| `git log <file>` shows history starting only 2 days ago | The file was renamed 2 days ago, breaking standard history walk. | Run `git log --follow <file>`. |
| `git log -S` returns commits that only reformatted whitespace | The string occurrence count didn't change, but whitespace surrounding it did. | Use `git log -S "string" -w` to ignore whitespace changes. |
| `git status` reports every file modified after cloning on Windows | Line ending mismatch (`CRLF` vs `LF`). | Configure `.gitattributes` with `* text=auto eol=lf` and run `git add --renormalize .`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Commit-Graph Acceleration**: In repositories with 500,000 commits (e.g. Chromium, Linux kernel), running `git log --graph` takes seconds without optimization. The **commit-graph** file precomputes generation numbers, reducing log traversal times by **95%**.
- **Pickaxe (`-S`) Overhead**: Pickaxe diffs require Git to inspect every commit's diff hunks. On massive repositories, constrain Pickaxe searches with pathspecs or date limits (`git log -S "key" --since="6 months ago" -- src/`).

### Security
- **Audit Logging & Secret Detection**: Use `git log -S` and `git log -G` to audit historical breaches (e.g. scanning commit history to verify when an AWS key or password was introduced).

---

## 9. Comparison Table

| Command | Compares From | Compares To | Primary Question Answered |
| :--- | :--- | :--- | :--- |
| `git diff` | Working Tree | Staging Area (Index) | "What have I changed on disk that is not yet staged?" |
| `git diff --staged` | Staging Area (Index) | HEAD Commit | "What will be committed if I run `git commit`?" |
| `git diff HEAD` | Working Tree | HEAD Commit | "What has changed since the last commit (staged + unstaged)?" |
| `git diff A B` (Two-Dot) | Tip of Branch A | Tip of Branch B | "What is the absolute difference between these two points?" |
| `git diff A...B` (Three-Dot)| Merge-Base(A, B) | Tip of Branch B | "What changes did Branch B introduce since diverging from A?" |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain the difference between `git diff branchA..branchB` and `git diff branchA...branchB`.</strong></summary>

- **Two-Dot Diff (`branchA..branchB`)**: Compares the direct tips of `branchA` and `branchB`. It displays all differences between the final snapshots of both branches, including commits that occurred on `branchA` after `branchB` diverged.
- **Three-Dot Diff (`branchA...branchB`)**: Finds the common ancestor (the merge base) of `branchA` and `branchB`, and calculates the diff between that common ancestor and the tip of `branchB`. It displays only the changes introduced on `branchB` since it branched off `branchA`. This matches the behavior of GitHub Pull Request diffs.
</details>

<details>
<summary><strong>2. In `git status -s`, what does the status code `MM` indicate?</strong></summary>

`git status -s` uses a two-column status format `XY`. The left column $X$ represents the Staging Area (Index) relative to `HEAD`, while the right column $Y$ represents the Working Tree relative to the Index. An `MM` status indicates that the file was modified and staged into the index ($X=M$), and subsequently modified *again* in the working tree without being staged ($Y=M$). The index and the disk file hold two different versions of the modification.
</details>

<details>
<summary><strong>3. How does the Pickaxe search (`git log -S`) differ from a standard `git log --grep`?</strong></summary>

- `git log --grep="string"` searches through **commit messages** only. It does not inspect file diffs or code changes.
- `git log -S "string"` (Pickaxe) inspects the **diff hunks** of commits. It searches for commits where the number of occurrences of `"string"` changed (i.e. where the string was added or deleted). It is used to trace the introduction or removal of specific variables, functions, or keys in the codebase.
</details>

<details>
<summary><strong>4. Why does `git log <file>` stop showing history after a file rename and how do you resolve it?</strong></summary>

By default, Git traces path history based on the literal filename path. When a file is renamed (e.g. from `utils.js` to `helpers.ts`), Git sees that `helpers.ts` did not exist prior to that commit and stops walking history. Passing the `--follow` flag instructs Git to detect renames and continue walking history through the previous path.
</details>

<details>
<summary><strong>5. What does `git log -L` allow a developer to do?</strong></summary>

`git log -L` performs line-range and function-level history tracing. For example:
- `git log -L 15,30:src/index.js` traces changes strictly to lines 15 through 30 over time.
- `git log -L :authMiddleware:src/auth.js` uses regex heuristics to locate the function `authMiddleware` and outputs only commits and diff hunks that modified that specific function.
</details>

<details>
<summary><strong>6. What is the difference between `git diff --staged` and `git diff --cached`?</strong></summary>

They are exact synonyms. `git diff --cached` was the original command in early Git versions. Git later introduced `git diff --staged` as a more intuitive, human-friendly alias because new developers frequently found the term "cached" confusing.
</details>

<details>
<summary><strong>7. How does `git diff --color-moved` assist during code refactoring reviews?</strong></summary>

When code blocks or functions are moved between locations or files, standard diffs render them as separate red deletions and green additions, making it hard to see whether the code was modified or merely moved. `--color-moved` detects lines that were deleted in one place and added in another, rendering them in a distinct color (e.g. cyan/magenta) so reviewers instantly know the code was simply relocated without internal modifications.
</details>

<details>
<summary><strong>8. What does `git log --oneline --graph --decorate --all` accomplish?</strong></summary>

It generates an ASCII visualization of the entire commit DAG:
- `--oneline`: Compresses each commit to short hash and subject line.
- `--graph`: Draws ASCII branching and merge graph lines on the left.
- `--decorate`: Displays ref pointers (HEAD, local branches, remote tracking branches, tags) next to their target commits.
- `--all`: Traverses all references across the repository, not just the currently checked-out branch.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Decoding Short Status Flags</strong></summary>

**Task**: In a test repository, create files that produce status codes `??`, `A `, ` M`, and `MM`. Verify with `git status -sb`.

**Solution**:
```bash
mkdir /tmp/status-drill && cd /tmp/status-drill
git init -b main
echo "v1" > base.txt && git add base.txt && git commit -m "init"

# 1. Produce ?? (untracked)
touch untracked.txt

# 2. Produce A  (staged add)
echo "new" > added.txt && git add added.txt

# 3. Produce  M (unstaged modify)
echo "v2" >> base.txt

# 4. Produce MM (staged, then modified again)
echo "staged" > multi.txt && git add multi.txt
echo "unstaged modification" >> multi.txt

# Verify:
git status -s
# Output:
#  M base.txt
# A  added.txt
# MM multi.txt
# ?? untracked.txt
```
</details>

<details>
<summary><strong>Exercise 2: Isolating Working Tree vs. Staging Diffs</strong></summary>

**Task**: Using `multi.txt` from Exercise 1, run `git diff` and `git diff --staged` to prove that two different diffs exist for the same file.

**Solution**:
```bash
# Staged change
git diff --staged multi.txt
# Shows "+staged"

# Unstaged change
git diff multi.txt
# Shows "+unstaged modification"
```
</details>

<details>
<summary><strong>Exercise 3: Three-Dot vs. Two-Dot Verification</strong></summary>

**Task**: Create a feature branch, commit to it, switch to main, make a commit to main, and compare two-dot vs three-dot diffs.

**Solution**:
```bash
git commit -am "cleanup"
git checkout -b feat-branch
echo "Feature work" > feat.txt && git add feat.txt && git commit -m "feat commit"

git checkout main
echo "Main work" > main.txt && git add main.txt && git commit -m "main commit"

# Two-dot: shows both feat.txt and main.txt
git diff main feat-branch --name-only

# Three-dot: shows ONLY feat.txt
git diff main...feat-branch --name-only
```
</details>

<details>
<summary><strong>Exercise 4: Tracking History Across a File Rename</strong></summary>

**Task**: Rename a file using `git mv`. Run `git log` with and without `--follow` to verify that history is preserved.

**Solution**:
```bash
git mv base.txt foundation.txt
git commit -m "refactor: rename base to foundation"

# Without --follow (shows only rename commit):
git log --oneline foundation.txt

# With --follow (shows both rename and initial commit):
git log --oneline --follow foundation.txt
```
</details>

<details>
<summary><strong>Exercise 5: Pickaxe Code Archaeology</strong></summary>

**Task**: Find the exact commit that introduced the word "Foundation" using Pickaxe `-S`.

**Solution**:
```bash
git log -S "foundation" --oneline
```
</details>

<details>
<summary><strong>Exercise 6: Word-by-Word Diff Inspection</strong></summary>

**Task**: Edit a single word in a long sentence and inspect the difference using `git diff --word-diff`.

**Solution**:
```bash
echo "The quick brown fox jumps over the lazy dog" > sentence.txt
git add sentence.txt && git commit -m "add sentence"

sed -i 's/brown/red/' sentence.txt

git diff --word-diff
# Shows: The quick [-brown-]{+red+} fox jumps over the lazy dog
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Command | Usage / Flags | Role |
| :--- | :--- | :--- |
| `git status -sb` | Short format | Displays branch sync and two-letter XY file statuses |
| `git diff` | Working tree vs Index | Shows unstaged changes |
| `git diff --staged` | Index vs HEAD | Shows changes staged for next commit |
| `git diff HEAD` | Working tree vs HEAD | Shows all uncommitted changes |
| `git diff A...B` | Merge-base to B | Pull Request diff (changes introduced on B) |
| `git diff -w` | Ignore whitespace | Filters out tab/space and line ending differences |
| `git diff --color-moved`| Moved code detection| Renders moved code blocks in distinct colors |
| `git log --graph --oneline --all`| Visual DAG | Complete ASCII commit history graph |
| `git log -S "<str>"` | Pickaxe search | Finds commits that added or removed `<str>` |
| `git log --follow <file>`| Trace renames | Follows file history past rename boundaries |
| `git log -L :<func>:<file>`| Function trace | Traces modifications to a specific function over time |

---

## 13. Further Reading & Official Documentation

- [Official Git Documentation - git-status](https://git-scm.com/docs/git-status)
- [Official Git Documentation - git-diff](https://git-scm.com/docs/git-diff)
- [Official Git Documentation - git-log](https://git-scm.com/docs/git-log)
- [Pro Git Book - Viewing the Commit History](https://git-scm.com/book/en/v2/Git-Basics-Viewing-the-Commit-History)

---

[<- 01-init-clone.md](./01-init-clone.md) | [Back to Module Overview](./README.md) | [03-add-commit.md ->](./03-add-commit.md)
