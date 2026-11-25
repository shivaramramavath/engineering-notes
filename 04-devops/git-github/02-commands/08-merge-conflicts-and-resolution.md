<!--
================================================================================
Metadata:
  Title: Merge Conflicts & Resolution Mastery (zdiff3, Stage Slots & RERERE)
  Track: Git & GitHub / DevOps
  Difficulty: Advanced
  Estimated Time: 55 minutes
  Prerequisites: 07-merging.md
  Key Terminology: Merge Conflicts, Conflict Markers, zdiff3, diff3, Stage Slots (1, 2, 3), git mergetool, RERERE (rerere.enabled), Tree Conflicts
================================================================================
-->

# Merge Conflicts & Resolution Mastery (zdiff3, Stage Slots & RERERE)

[<- 07-merging.md](./07-merging.md) | [Back to Module Overview](./README.md) | [09-remotes.md ->](./09-remotes.md)

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

In distributed software engineering, merge conflicts are an inevitable reality. When two developers independently edit the same line of a file, or when one developer refactors a function while another renames the file, Git cannot guess which change takes precedence.

Rather than making an arbitrary assumption that could introduce silent bugs or corrupt data, Git halts execution, marks the affected paths in `.git/index`, and inserts conflict markers directly into your working tree files.

To inexperienced engineers, merge conflicts cause dread and panic. But once you understand the underlying mechanics—how to read **3-way conflict markers (`zdiff3`)**, how to inspect the **index stage slots (1, 2, 3)**, how to invoke automated merge tools (`git mergetool`), and how to leverage **RERERE (Reuse Recorded Resolution)** to automatically replay resolutions—resolving merge conflicts becomes a routine, mechanical operation.

---

## 2. Core Concepts

### Types of Merge Conflicts
1. **Content Conflicts (Textual)**:
   - Both branches modified overlapping lines in the same file differently.
   - Resolved by editing file text to combine or choose the correct code.
2. **Structural / Tree Conflicts (Filesystem)**:
   - **Modify / Delete Conflict**: Branch A modified a file, but Branch B deleted it.
   - **Rename / Delete Conflict**: Branch A renamed a file, but Branch B deleted it.
   - **Rename / Rename Collision**: Both branches created new files with the same name, or renamed different files into the same destination path.

### Reading Conflict Markers: Standard vs. `zdiff3`
By default, Git uses standard 2-way conflict markers:
```text
<<<<<<< HEAD
Our version (changes on current branch)
=======
Their version (changes on incoming branch)
>>>>>>> feature-branch
```
*The Flaw*: You see what both sides changed, but you have no idea what the code looked like *originally* before either developer edited it!

Modern Git provides **`zdiff3` (Zealous Diff3)**:
```text
<<<<<<< HEAD
Our version (changes on current branch)
||||||| base
The original common ancestor base code
=======
Their version (changes on incoming branch)
>>>>>>> feature-branch
```
With `zdiff3`, you can immediately analyze the divergence:
- "Base had `timeout = 1000`. HEAD changed it to `2000`. Theirs changed it to `5000`."
- Resolution is obvious: pick the appropriate value (e.g. `5000`) and delete the marker lines!

---

## 3. How It Works Internally

### The Four Stage Slots in `.git/index`
During normal operation, every clean file in the staging index occupies **Stage Slot 0**.
When a merge conflict occurs, Git clears Slot 0 for the conflicting file and populates up to three separate slots in `.git/index`:

- **Slot 1 (Ancestor Base)**: The blob from the common ancestor commit (`git merge-base`).
- **Slot 2 (Ours)**: The blob from the currently checked-out branch (`HEAD`).
- **Slot 3 (Theirs)**: The blob from the incoming branch being merged.

```
+-----------------------------------------------------------------------------------+
|                        Stage Slots in .git/index During Conflict                  |
|                                                                                   |
|  git ls-files -u                                                                  |
|  ├── 100644 1a2b3c... 1  src/auth.ts  <-- Slot 1: Common Ancestor Base Version   |
|  ├── 100644 4d5e6f... 2  src/auth.ts  <-- Slot 2: Our Version (HEAD)             |
|  └── 100644 7a8b9c... 3  src/auth.ts  <-- Slot 3: Their Version (Incoming Branch)|
|                                                                                   |
|  RESOLUTION ACTION:                                                               |
|  Developer edits src/auth.ts and runs: git add src/auth.ts                        |
|                                                                                   |
|  RESULT:                                                                          |
|  Slots 1, 2, and 3 are PURGED from the index!                                     |
|  The resolved file is saved into Stage Slot 0 (clean)!                            |
+-----------------------------------------------------------------------------------+
```

### RERERE: Reuse Recorded Resolution
RERERE ("Reuse Recorded Resolution") is an embedded Git mechanism that remembers how you resolved a merge conflict.
1. When a conflict occurs, Git hashes the conflicting pre-image and stores it in `.git/rr-cache/<hash>/preimage`.
2. When you resolve the conflict and stage it, Git records your resolution in `.git/rr-cache/<hash>/postimage`.
3. If that identical conflict recurs (e.g., during a rebase, cherry-pick, or topic branch sync), Git checks the pre-image hash, automatically applies your recorded post-image resolution, and stages the file!

---

## 4. Syntax & Usage

### Configuring Advanced Conflict Tools

```bash
# 1. Enable modern 3-way conflict markers globally
git config --global merge.conflictStyle zdiff3

# 2. Enable RERERE globally so you never resolve the same conflict twice
git config --global rerere.enabled true
git config --global rerere.autoUpdate true

# 3. Configure VS Code as default 3-way graphical merge tool
git config --global merge.tool vscode
git config --global mergetool.vscode.cmd 'code --wait "$MERGED"'
git config --global mergetool.keepBackup false
```

---

### Conflict Resolution Command Workflow

```bash
# 1. Trigger merge
git merge feature-payment

# 2. Inspect conflicting paths
git status -sb
# Conflicting files display status 'UU' (Unmerged, both modified)

# 3. Launch configured 3-way graphical merge tool
git mergetool

# 4. Or checkout whole file versions directly from stage slots:
# Keep our version completely:
git checkout --ours src/config.json

# Keep their incoming version completely:
git checkout --theirs src/config.json

# 5. Stage the resolved files (clears stage slots 1, 2, 3 -> writes to slot 0)
git add src/config.json

# 6. Finalize merge commit
git commit
```

---

## 5. Practical Examples

### Example 1: Resolving a 3-Way Conflict with `zdiff3`
Demonstrate how `zdiff3` makes conflict resolution intuitive:

```bash
mkdir /tmp/conflict-lab && cd /tmp/conflict-lab
git init -b main
git config merge.conflictStyle zdiff3

cat << 'EOF' > server.js
const port = 3000;
const timeout = 1000;
EOF
git add server.js && git commit -m "feat: initial server config"

# Branch A modifies timeout
git switch -c branch-a
sed -i 's/1000/2000/' server.js
git commit -am "fix: increase timeout to 2000"

# Branch B modifies timeout differently
git switch main && git switch -c branch-b
sed -i 's/1000/5000/' server.js
git commit -am "fix: increase timeout to 5000"

# Attempt merge (triggers conflict)
git merge branch-a
cat server.js
```
Output:
```javascript
const port = 3000;
<<<<<<< HEAD
const timeout = 5000;
||||||| base
const timeout = 1000;
=======
const timeout = 2000;
>>>>>>> branch-a
```
Resolution: We want the higher timeout (`5000`). Edit `server.js` to keep `const timeout = 5000;` and remove markers.
```bash
cat << 'EOF' > server.js
const port = 3000;
const timeout = 5000;
EOF

git add server.js
git commit -m "merge: resolve timeout conflict favoring 5000ms"
```

### Example 2: Resolving a Modify/Delete Tree Conflict
Branch A edited `analytics.js`, but Branch B deleted it:

```bash
git switch -c branch-edit
echo "new metric" >> analytics.js
git add analytics.js && git commit -m "feat: add metric"

git switch main && git switch -c branch-delete
git rm analytics.js
git commit -m "chore: remove deprecated analytics"

# Merge triggers tree conflict
git merge branch-edit
git status -s
```
Output:
```text
UD analytics.js (Updated in theirs, Deleted in ours)
```
Resolution Choices:
```bash
# Option A: We agree it should be deleted -> remove it:
git rm analytics.js
git commit -m "merge: confirm deletion of analytics.js"

# Option B: We want to keep the modified file -> stage it:
git add analytics.js
git commit -m "merge: retain analytics.js with new metrics"
```

### Example 3: Extracting Raw Slot Blobs with Plumbing
Inspect the exact historical versions of a conflicting file:

```bash
# Extract Slot 1 (Base)
git show :1:server.js > server.base.js

# Extract Slot 2 (Ours / HEAD)
git show :2:server.js > server.ours.js

# Extract Slot 3 (Theirs / Incoming)
git show :3:server.js > server.theirs.js

# Diff our version against base
diff -u server.base.js server.ours.js
```

### Example 4: Testing RERERE Automatic Replay
Watch RERERE automatically resolve a recurring conflict during rebase:

```bash
# Enable RERERE
git config rerere.enabled true

# Trigger conflict, resolve it, and commit
# Git records: "Recorded resolution for 'server.js'"

# Now reset and re-attempt merge:
git reset --hard HEAD~1
git merge branch-a
```
Output:
```text
Auto-merging server.js
CONFLICT (content): Merge conflict in server.js
Resolved 'server.js' using previous resolution.
```
*RERERE automatically recognized the conflict and applied your exact resolution!*

---

## 6. Best Practices

1. **Always Set `merge.conflictStyle = zdiff3`**: The single most effective configuration change you can make. The common ancestor (`||||||| base`) provides the missing context needed to resolve conflicts accurately.
2. **Always Enable `rerere.enabled = true`**: Eliminates the frustration of re-resolving the same conflicts repeatedly during long-running feature branch rebases.
3. **Run Tests Immediately After Resolving Conflicts**: Never blindly commit a merge resolution. Always execute `npm test`, `pytest`, or `mvn test` to ensure syntactic and logical correctness.
4. **Resolve Conflicts with the Author When Unsure**: If you are resolving conflicts against complex business logic written by another engineer, pair-program the resolution to avoid introducing regressions.
5. **Clean Up `.orig` Backup Files**: Configure `git config --global mergetool.keepBackup false` so external merge tools don't leave cluttered `.orig` files in your workspace.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| Committed conflict markers into the codebase | Developer forgot to search for and remove `<<<<<<<`, `=======`, `>>>>>>>` lines. | Run `git grep -E '^(<{7}|={7}|>{7})'` before committing; configure a pre-commit hook to block markers. |
| Mergetool leaves dozens of `.orig` files cluttering git status | Default mergetool backup setting creates `.orig` files. | Run `git clean -fd` and set `git config --global mergetool.keepBackup false`. |
| `git checkout --ours` took the wrong branch | Under `git rebase`, "ours" and "theirs" are swapped because the upstream is checked out! | Remember: in `rebase`, `HEAD` (`ours`) is the upstream branch, and `theirs` is your feature branch. |
| Cannot complete commit: `fatal: Exiting because of an unresolved conflict.` | Conflicting files were edited, but not staged via `git add`. | Stage resolved files with `git add <file>`, then run `git commit`. |
| Want to cancel everything and return to clean pre-merge state | Stuck in complex conflict resolution. | Execute `git merge --abort`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Zero Disk Re-computation with RERERE**: RERERE stores conflict pre-images and resolutions in `.git/rr-cache/`. Applying a cached resolution takes **<1 millisecond**, saving hours of manual developer time during long rebases.

### Security
- **Accidental Accidental Vulnerability Introduction**: Merge conflicts are the #1 source of regression vulnerabilities in production software (e.g. accidentally reverting a security patch while resolving an authentication conflict). Always review `git diff --staged` before committing merge resolutions.

---

## 9. Comparison Table

| Attribute | Standard Merge Style (`merge`) | 3-Way Merge Style (`zdiff3`) |
| :--- | :--- | :--- |
| **Sections Displayed** | 2 (`HEAD` and `Theirs`) | 3 (`HEAD`, `Base`, and `Theirs`) |
| **Shows Common Ancestor?**| No | Yes (`||||||| base`) |
| **Line Trimming** | No trimming | Trims identical common preamble/postscript |
| **Cognitive Load** | High (must guess what changed) | Low (divergence is visually obvious) |
| **Configuration Key** | Default (`merge.conflictStyle = merge`)| `merge.conflictStyle = zdiff3` |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between standard conflict markers and `zdiff3` conflict markers?</strong></summary>

Standard conflict markers (`merge.conflictStyle = merge`) display only two sections: `<<<<<<< HEAD` (our version) and `>>>>>>> branch` (their version). They omit the original common ancestor code, making it difficult to determine whether a line was added, deleted, or edited by either party. `zdiff3` (Zealous Diff3) inserts a third section: `||||||| base` containing the common ancestor code, while trimming away identical common lines. This gives the engineer full visibility into what the code looked like before either branch diverged.
</details>

<details>
<summary><strong>2. Explain what the four stage slots (0, 1, 2, 3) represent in `.git/index` during a merge conflict.</strong></summary>

During standard operation, tracked clean files reside in **Slot 0**. When a merge conflict occurs:
- **Slot 0**: Cleared for the conflicting file.
- **Slot 1**: Holds the blob representing the **common ancestor base** version.
- **Slot 2**: Holds the blob representing **our version** (the commit at `HEAD`).
- **Slot 3**: Holds the blob representing **their version** (the commit on the incoming branch).
When the conflict is resolved and staged via `git add`, slots 1, 2, and 3 are purged, and the resolved file is written into Slot 0.
</details>

<details>
<summary><strong>3. What is RERERE in Git and how does it operate internally?</strong></summary>

RERERE stands for "Reuse Recorded Resolution". When enabled (`rerere.enabled = true`):
1. When a merge or rebase results in a conflict, Git hashes the conflicting hunk (pre-image) and stores it in `.git/rr-cache/`.
2. When the developer resolves the conflict and stages it, Git records the resolved hunk (post-image).
3. If that exact conflict recurs in the future (e.g. during another rebase or cherry-pick), Git identifies the matching pre-image hash and automatically applies the recorded post-image resolution without human intervention.
</details>

<details>
<summary><strong>4. Why do `--ours` and `--theirs` swap meanings during an interactive `git rebase` compared to a `git merge`?</strong></summary>

- During `git merge feature`: You are currently checked out on `main`. Therefore, `HEAD` (`--ours`) is `main`, and `--theirs` is the incoming `feature` branch.
- During `git rebase main`: Git temporarily checks out the upstream `main` branch and replays your feature commits on top of it one by one. Therefore, while replaying, `HEAD` (`--ours`) is actually `main` (the upstream base), and `--theirs` is your feature branch commit!
</details>

<details>
<summary><strong>5. How do you resolve a "Modify / Delete" tree conflict in Git?</strong></summary>

A modify/delete conflict occurs when one branch modified a file while another branch deleted it. Git marks the file with status `UD` or `DU`. To resolve:
- If you decide to **keep the file**: run `git add <file>` to stage the modified version.
- If you decide to **delete the file**: run `git rm <file>` to confirm the deletion.
- Finalize the resolution with `git commit`.
</details>

<details>
<summary><strong>6. What does `git checkout --ours <file>` do during a conflict?</strong></summary>

It overwrites the conflicting file in your working tree with the exact version stored in **Stage Slot 2** (`HEAD` / our branch), completely ignoring all changes from the incoming branch for that specific file.
</details>

<details>
<summary><strong>7. How can you verify that no unresolved conflict markers remain in the repository before committing?</strong></summary>

Run a repository-wide grep checking for conflict marker prefixes:
```bash
git grep -E '^(<{7}|={7}|>{7})'
```
If this returns zero output, no raw conflict marker lines exist in tracked files.
</details>

<details>
<summary><strong>8. What is the role of `.git/MERGE_HEAD` during a conflict?</strong></summary>

`.git/MERGE_HEAD` is a temporary pseudo-ref that stores the commit hash of the incoming branch being merged. Its existence signals to Git that an in-progress merge is active. When you run `git commit`, Git reads `.git/MERGE_HEAD` to automatically populate the second parent pointer in the merge commit object, and then deletes the file.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Activating and Inspecting `zdiff3` Markers</strong></summary>

**Task**: Configure `merge.conflictStyle = zdiff3`, create a deliberate conflict between two branches, and inspect the resulting 3-section conflict marker.

**Solution**:
```bash
mkdir /tmp/zdiff-lab && cd /tmp/zdiff-lab
git init -b main
git config merge.conflictStyle zdiff3

echo "Common Base Code" > code.txt
git add code.txt && git commit -m "init: base"

# Branch A
git switch -c branch-a
echo "Code modified by Alice" > code.txt
git commit -am "alice commit"

# Branch B
git switch main && git switch -c branch-b
echo "Code modified by Bob" > code.txt
git commit -am "bob commit"

# Trigger merge conflict
git merge branch-a
cat code.txt
# Notice <<<<<<< HEAD, ||||||| base, and >>>>>>> branch-a markers!
```
</details>

<details>
<summary><strong>Exercise 2: Querying Stage Slots with `git ls-files -u`</strong></summary>

**Task**: Using the conflicted repository from Exercise 1, run `git ls-files -u` to inspect slots 1, 2, and 3.

**Solution**:
```bash
git ls-files -u code.txt
# Shows Slot 1 (Base), Slot 2 (Bob/HEAD), Slot 3 (Alice)
```
</details>

<details>
<summary><strong>Exercise 3: Resolving Conflict Using `--theirs`</strong></summary>

**Task**: Resolve the conflict by accepting Alice's incoming version completely using `git checkout --theirs`. Stage and commit the result.

**Solution**:
```bash
git checkout --theirs code.txt
cat code.txt
# Shows: "Code modified by Alice"

git add code.txt
git commit -m "merge: resolve favoring Alice's version"

git status
# Clean!
```
</details>

<details>
<summary><strong>Exercise 4: Handling a Modify/Delete Conflict</strong></summary>

**Task**: Create a conflict where Branch A modifies `service.js` and Branch B deletes it. Resolve it by confirming the deletion.

**Solution**:
```bash
echo "service" > service.js && git add service.js && git commit -m "add service"

git switch -c edit-branch
echo "updated service" > service.js && git commit -am "update service"

git switch main && git switch -c delete-branch
git rm service.js && git commit -m "delete service"

git merge edit-branch
git status -s
# Output: UD service.js

# Confirm deletion
git rm service.js
git commit -m "merge: confirmed deletion of service.js"
```
</details>

<details>
<summary><strong>Exercise 5: Testing RERERE Conflict Memory</strong></summary>

**Task**: Enable `rerere.enabled = true`. Trigger a conflict, resolve it, reset, and re-merge to verify that Git auto-resolves it.

**Solution**:
```bash
git config rerere.enabled true

# Trigger conflict between branch-a and branch-b from Exercise 1
git reset --hard branch-b
git merge branch-a

# Resolve manually
echo "Resolved unified code" > code.txt
git add code.txt
git commit -m "merge: manual resolution"

# Reset and re-trigger
git reset --hard branch-b
git merge branch-a
# Output confirms: "Resolved 'code.txt' using previous resolution."
```
</details>

<details>
<summary><strong>Exercise 6: Pre-Commit Conflict Marker Detection Script</strong></summary>

**Task**: Write a shell command to verify that no files contain accidental conflict markers.

**Solution**:
```bash
if git grep -E '^(<{7}|={7}|>{7})'; then
    echo "ERROR: Unresolved conflict markers found!"
    exit 1
else
    echo "SUCCESS: No conflict markers detected."
fi
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Command | Result |
| :--- | :--- | :--- |
| **Enable 3-Way Diffs** | `git config --global merge.conflictStyle zdiff3` | Inserts `||||||| base` ancestor marker |
| **Enable RERERE** | `git config --global rerere.enabled true` | Auto-remembers & replays conflict fixes |
| **Inspect Unmerged Files**| `git status -sb` | Lists conflicting files with `UU` or `UD`/`DU` |
| **Inspect Stage Slots** | `git ls-files -u` | Displays hashes for slots 1 (base), 2 (ours), 3 (theirs) |
| **Take Our Version** | `git checkout --ours <file>` | Overwrites file with Stage Slot 2 (HEAD) |
| **Take Their Version** | `git checkout --theirs <file>` | Overwrites file with Stage Slot 3 (Incoming) |
| **Launch GUI Merge Tool**| `git mergetool` | Opens visual 3-way conflict resolver |
| **Abort Merge** | `git merge --abort` | Clears all conflict markers and resets to pre-merge state |
| **Extract Slot File** | `git show :<slot>:<path>` | Dumps exact stage slot content to stdout |

---

## 13. Further Reading & Official Documentation

- [Official Git Documentation - git-mergetool](https://git-scm.com/docs/git-mergetool)
- [Official Git Documentation - git-rerere](https://git-scm.com/docs/git-rerere)
- [Git Tools - Advanced Merging](https://git-scm.com/book/en/v2/Git-Tools-Advanced-Merging)
- [Take the Pain Out of Git Conflict Resolution with zdiff3](https://ductile.systems/zdiff3/)

---

[<- 07-merging.md](./07-merging.md) | [Back to Module Overview](./README.md) | [09-remotes.md ->](./09-remotes.md)
