<!--
================================================================================
Metadata:
  Title: Staging & Committing: git add & git commit Mastery
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 02-status-diff-log.md
  Key Terminology: git add -p, git add -N (intent-to-add), git commit -v, Atomic Commits, Co-authored-by, Conventional Commits, Hunk Editing
================================================================================
-->

# Staging & Committing: git add & git commit Mastery

[<- 02-status-diff-log.md](./02-status-diff-log.md) | [Back to Module Overview](./README.md) | [04-undoing-changes.md ->](./04-undoing-changes.md)

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

In modern software engineering, code changes are rarely produced in clean, orderly sequences. A typical coding session involves introducing a feature, fixing a minor typo in an unrelated utility, adding a temporary debug logging statement, and updating a package dependency—all within the span of thirty minutes across five files.

If an engineer simply runs `git add . && git commit -m "updates"`, they create a monolithic, unreviewable commit:
- PR reviewers cannot understand what changed or why.
- Automated bisect tools (`git bisect`) cannot isolate which specific line caused a production regression.
- Cherry-picking the bug fix to a release branch brings along half-finished feature code.

The staging and committing workflow (`git add` and `git commit`) is specifically engineered to solve this. Git allows you to decouple **how you write code** from **how you record history**. Through tools like **interactive patch staging (`git add -p`)**, **intent-to-add (`git add -N`)**, and **verbose commits (`git commit -v`)**, you can decompose complex multi-file sessions into clean, atomic commits.

---

## 2. Core Concepts

### Staging Variations: `git add .` vs `git add -A` vs `git add -u`
Understanding how pathspecs and flags scope staging is crucial:
- **`git add -u` (`--update`)**: Stages modifications and deletions of **currently tracked files only**. Completely ignores any new untracked files on disk!
- **`git add .`**: Stages modifications, deletions, and new files within the **current working directory and its subdirectories**. In older Git versions (pre-2.0), it omitted deletions, but in modern Git it includes deletions within the subtree.
- **`git add -A` (`--all`)**: Stages all modifications, deletions, and untracked files across the **entire repository root**, regardless of your current working directory.

```
+-------------------------------------------------------------------------------+
|                             Scoping of git add Flags                          |
|                                                                               |
|   FILE STATE                      git add -u       git add .      git add -A  |
|   -------------------------------------------------------------------------   |
|   Tracked Modified File           YES              YES            YES         |
|   Tracked Deleted File            YES              YES            YES         |
|   New Untracked File (cwd)        NO               YES            YES         |
|   New Untracked File (parent dir) NO               NO             YES         |
+-------------------------------------------------------------------------------+
```

### The Concept of Atomic Commits
An **Atomic Commit** is a commit that does exactly one logical thing, and does it completely.
- If it introduces a feature, it includes the implementation, tests, and documentation updates.
- If it fixes a bug, it touches only the code causing the bug and the corresponding regression test.
- The project builds and passes all automated tests at every single commit in the history.

---

## 3. How It Works Internally

### What Happens During `git add -p` (Patch Staging)
When you run `git add -p`, Git runs an internal diff between your working tree and the index. It segments contiguous lines of differences into discrete blocks called **hunks**.

For each hunk, Git displays the diff and enters an interactive prompt. If you type `y`, Git formats an ephemeral patch representing only that specific hunk and applies it directly to `.git/index`. The new blob object is created in `.git/objects/`, while the unselected hunks remain intact in your working tree!

### What `git add -N` (`--intent-to-add`) Does
Normally, an untracked file cannot be diffed with `git diff` (it only shows up in `git status`). Running `git add -N <file>`:
1. Adds an entry for `<file>` into `.git/index` with file size 0 and a special empty/dummy hash.
2. Marks the file as "intent-to-add".
3. Now, `git diff` displays the entire contents of the new file as an addition, allowing you to use `git add -p` on a brand-new file!

---

## 4. Syntax & Usage

### Advanced Staging Commands

```bash
# Stage all changes across entire repository root
git add -A

# Stage tracked changes only (skip all untracked files)
git add -u

# Interactively stage hunks
git add -p src/auth.ts

# Mark new file as intent-to-add (enables diffing before staging)
git add -N new-feature.ts

# Stage files matching a specific pathspec/extension
git add "*.ts" ":!*.test.ts"  # Add all TypeScript files EXCEPT tests
```

---

### Advanced Committing Commands

```bash
# Standard commit with multi-line message (Subject + Body)
git commit -m "feat(billing): add stripe webhook signature verification" \
           -m "Validates HMAC-SHA256 signatures on incoming webhook payloads to prevent replay attacks."

# Open $EDITOR with full diff preview in comments (Recommended!)
git commit -v

# Stage all modified tracked files and commit in one step (skips untracked!)
git commit -am "fix(auth): update session cookie max-age"

# Amend: add newly staged changes to the latest commit and update message
git commit --amend --no-edit

# Automatically assign Co-Authors for GitHub pairing credit
git commit -m "feat: implement caching layer

Co-authored-by: Jane Doe <jane@company.com>
Co-authored-by: Bob Smith <bob@company.com>"

# Create fixup commit targeting a previous commit hash
git commit --fixup=<commit-hash>
```

---

## 5. Practical Examples

### Example 1: Interactive Hunk Staging and Splitting (`git add -p`)
Suppose `config.json` has two distinct changes in close proximity: one setting `debug: true` (a local testing hack) and another updating `timeoutMs: 5000` (the actual production fix).

```bash
git add -p config.json
```
Prompt:
```text
@@ -10,4 +10,4 @@
-  "debug": false,
+  "debug": true,
-  "timeoutMs": 1000,
+  "timeoutMs": 5000,
Stage this hunk [y,n,q,a,d,s,e,?]?
```
Actions available:
- `y`: Stage the entire hunk.
- `n`: Skip the entire hunk.
- `s`: **Split** the hunk into two smaller hunks if separated by unchanged lines.
- `e`: **Manually edit** the hunk. In the editor, delete the `+ "debug": true,` line and restore `- "debug": false,`. Save and exit.

Result: Only the `timeoutMs` change is staged in the index, leaving `debug: true` safe on disk!

### Example 2: The Anatomy of a Perfect Conventional Commit
Write a standardized, enterprise-grade commit message:

```bash
git commit -v
```
Inside your editor:
```text
feat(auth)!: require multi-factor authentication for admin routes

Enforce TOTP validation across all /admin/* API endpoints.
Users lacking verified MFA keys receive HTTP 403 Forbidden
with an onboarding challenge payload.

BREAKING CHANGE: All existing API tokens accessing /admin/* must be
re-issued with MFA claims enabled.

Closes: #482
Co-authored-by: Alex Rivera <alex@security.io>
```

### Example 3: Staging a Brand-New File with Intent-to-Add (`-N`)
```bash
# Create brand new file
echo "const token = 'xyz';" > src/new-token.ts
echo "const debug = true;" >> src/new-token.ts

# Mark intent to add
git add -N src/new-token.ts

# Now git diff works on the new file!
git diff src/new-token.ts

# Interactively stage only line 1!
git add -p src/new-token.ts
```

### Example 4: Pathspec Magic for Scoped Staging
Use advanced pathspec globbing to include or exclude files:

```bash
# Add all python files in src/ except tests
git add "src/*.py" ":(exclude)src/*_test.py"

# Add all files modified in the last directory level
git add "**/controllers/*.ts"
```

### Example 5: Creating Fixup Commits for Autosquash
Streamline pull request revisions:

```bash
# Fix a typo in the previous commit
echo "Correct spelling" > doc.txt
git add doc.txt

# Create a fixup commit referencing HEAD~1
git commit --fixup=HEAD~1

# Inspect log
git log --oneline -2
# Outputs:
# fixup! feat: initial documentation
# feat: initial documentation
```

---

## 6. Best Practices

1. **Follow the 50/72 Rule**:
   - Limit the subject line to **50 characters** or fewer, written in the imperative mood ("Add feature", not "Added feature").
   - Follow the subject with a blank line.
   - Wrap the commit body text at **72 characters** per line to ensure clean terminal formatting.
2. **Commit Early, Commit Often Locally**: Create small, focused commits as you work. You can always squash, reorder, or reword them before pushing via `git rebase -i`.
3. **Always Use `git commit -v`**: Configure `git config --global commit.verbose true`. Seeing the full diff right beneath your commit message prompt prevents committing unwanted changes.
4. **Never Use `git commit -am` When Untracked Files Exist**: `-a` stages only tracked modifications; it completely misses new untracked files, leading to broken commits that fail CI builds.
5. **Use Conventional Commits**: Standardize on `feat:`, `fix:`, `docs:`, `refactor:`, `perf:`, `test:`, and `chore:`.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| Untracked files were not committed when running `git commit -a -m "msg"` | `-a` / `--all` only stages **tracked** files. Untracked files are ignored. | Run `git add <new-file>` explicitly before committing. |
| Accidentally committed debugging logs or local secrets | Blindly staged everything via `git add .` without checking diffs. | Run `git reset --soft HEAD~1`, unstage the debug lines, and re-commit. |
| Subject line wraps awkwardly in GitHub UI | Subject exceeded 72 characters or lacked a blank line before the body. | Run `git commit --amend` to format subject under 50 chars with a separating newline. |
| `fatal: pathspec '...' did not match any files` | Missing quotes around shell globs caused the terminal shell to expand the glob before Git saw it. | Wrap globs in quotes: `git add "*.ts"`. |
| Interactive patch prompt says `Cannot split this hunk` | The changes in the hunk are on contiguous, adjacent lines without unchanged separator lines. | Choose `e` to edit the hunk manually in your text editor. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Blob Generation During Staging**: Every `git add` immediately compresses and writes blobs into `.git/objects/`. Staging large 500MB test files repeatedly generates gigabytes of orphan objects until pruned.
- **Fast Status After Staging**: The index stat cache updates immediately upon `git add`, keeping subsequent `git status` queries fast.

### Security
- **Defense Against Leaked Secrets**: The staging area is your final gate of inspection. Never bypass pre-commit hooks (`--no-verify`) unless resolving an active critical production incident under supervisor sign-off.
- **Audit Trails via Co-Authors**: Using `Co-authored-by:` trailers provides verified cryptographic and audit-compliant attribution for pair programming and AI-assisted engineering.

---

## 9. Comparison Table

| Feature / Command | `git add -u` | `git add .` | `git add -A` | `git add -p` |
| :--- | :--- | :--- | :--- | :--- |
| **Tracked Modifications** | Yes | Yes (in cwd) | Yes (entire repo) | Interactive selection |
| **Tracked Deletions** | Yes | Yes (in cwd) | Yes (entire repo) | Interactive selection |
| **New Untracked Files** | No | Yes (in cwd) | Yes (entire repo) | No (unless `-N` run) |
| **Granularity** | Entire file | Entire file | Entire file | Line-by-line / hunk |
| **Recommended Use** | Quick tracked updates | Directory-scoped | Full repo staging | Precision atomic commits |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What is an atomic commit and why is it considered an engineering best practice?</strong></summary>

An atomic commit is a commit that makes a single, indivisible logical change to the codebase. It includes all necessary code, tests, and documentation for that specific change, and leaves the repository in a fully compiling, green-test state. Atomic commits are critical because they:
1. Make code reviews significantly faster and easier to comprehend.
2. Enable automated regression debugging via `git bisect` without landing on broken intermediary builds.
3. Allow clean, conflict-free cherry-picking and reverting of specific features or bug fixes.
</details>

<details>
<summary><strong>2. How does `git add -u` differ from `git add .` and `git add -A`?</strong></summary>

- `git add -u` (`--update`): Stages modifications and deletions of **currently tracked files only**. It never stages newly created untracked files.
- `git add .`: Stages tracked modifications, deletions, and untracked files located within the **current working directory and its subdirectories**.
- `git add -A` (`--all`): Stages all modifications, deletions, and untracked files across the **entire repository**, regardless of what subdirectory the user is currently located in.
</details>

<details>
<summary><strong>3. Explain the mechanism and benefits of `git add -p` (patch staging).</strong></summary>

`git add -p` compares the working tree against the index, breaks differences into logical blocks called "hunks", and presents each hunk interactively to the developer. The developer can accept (`y`), reject (`n`), split (`s`), or manually edit (`e`) each hunk. This allows an engineer to work on multiple unrelated tasks in the same file and selectively stage only the relevant lines for a clean, atomic commit while leaving the remaining changes in the working tree.
</details>

<details>
<summary><strong>4. What is `git add -N` (intent-to-add) and when should it be used?</strong></summary>

`git add -N` (`--intent-to-add`) adds an entry for an untracked file to `.git/index` with an empty hash. The file is officially registered in the index as an empty file, but its physical contents remain uncommitted. This allows Git commands like `git diff` and `git add -p` to treat the untracked file as an addition, enabling line-by-line interactive staging on brand-new files.
</details>

<details>
<summary><strong>5. What is the "50/72 rule" for Git commit messages?</strong></summary>

The 50/72 rule is a formatting standard:
- The **subject line** should be 50 characters or fewer, summarizing the change in the imperative mood ("Fix memory leak", not "Fixed memory leak").
- A **blank line** must separate the subject from the body.
- The **commit body** should be manually wrapped at 72 characters per line. This ensures commit messages render cleanly without awkward text wrapping across terminal pagers, `git log`, email patches, and GitHub web UIs.
</details>

<details>
<summary><strong>6. What does `git commit -v` (verbose) do and why is it recommended?</strong></summary>

`git commit -v` opens the configured text editor for the commit message and includes the full unified diff of all staged changes in the commented-out section below the message template. This provides the author with a final, line-by-line review of the exact code about to be committed, helping catch unwanted debug statements, commented code, or credentials before recording the commit.
</details>

<details>
<summary><strong>7. How does GitHub recognize multiple authors on a single commit?</strong></summary>

GitHub parses commit message trailers for the `Co-authored-by:` key at the bottom of the commit body:
```text
Co-authored-by: Name <email@example.com>
```
If the email matches a verified GitHub account, GitHub displays all co-authors' avatars on the commit badge and attributes contribution statistics to each developer.
</details>

<details>
<summary><strong>8. What is the purpose of `git commit --fixup`?</strong></summary>

`git commit --fixup=<commit-hash>` creates a commit with a formatted message: `fixup! <target commit subject>`. When running `git rebase -i --autosquash`, Git automatically identifies this marker, moves the fixup commit directly beneath its target commit, and marks it to be automatically squashed without opening an interactive editor prompt.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Interactive Hunk Staging Drill</strong></summary>

**Task**: Create a file with 10 lines of code. Modify lines 2 and 9. Use `git add -p` to stage only line 2. Verify that line 9 remains unstaged.

**Solution**:
```bash
mkdir /tmp/staging-lab && cd /tmp/staging-lab
git init -b main
seq 1 10 > numbers.txt
git add numbers.txt && git commit -m "init: numbers 1 to 10"

# Edit lines 2 and 9
sed -i 's/2/TWO/' numbers.txt
sed -i 's/9/NINE/' numbers.txt

# Run patch staging
git add -p numbers.txt
# When prompted, press 's' to split if necessary, then 'y' for TWO and 'n' for NINE

# Verify staged vs unstaged diffs:
git diff --staged numbers.txt # Shows only TWO
git diff numbers.txt         # Shows only NINE
```
</details>

<details>
<summary><strong>Exercise 2: Using Intent-to-Add (`-N`) on a Fresh File</strong></summary>

**Task**: Create a new file `script.py`. Mark it with `git add -N`. Verify that `git diff` displays the full file content as an addition.

**Solution**:
```bash
echo "import os" > script.py
echo "print('hello')" >> script.py

git add -N script.py

# Check diff
git diff script.py
# Outputs diff with "+import os" and "+print('hello')"
```
</details>

<details>
<summary><strong>Exercise 3: Crafting a Multi-Line Commit with Body and Issue References</strong></summary>

**Task**: Commit changes with a formatted subject, body paragraph, and issue closing tag using multiple `-m` flags.

**Solution**:
```bash
git add numbers.txt
git commit -m "fix(numbers): replace digit 2 with word TWO" \
           -m "Improves readability for text parsing downstream." \
           -m "Closes #104"

git log -1
```
</details>

<details>
<summary><strong>Exercise 4: Pathspec Exclusion Staging</strong></summary>

**Task**: Create 3 files: `app.ts`, `service.ts`, and `app.test.ts`. Use a pathspec to stage all `.ts` files *except* `.test.ts`.

**Solution**:
```bash
touch app.ts service.ts app.test.ts

# Stage with exclusion pattern
git add "*.ts" ":!*.test.ts"

git status -s
# A  app.ts
# A  service.ts
# ?? app.test.ts
```
</details>

<details>
<summary><strong>Exercise 5: Adding Co-Authors to a Commit</strong></summary>

**Task**: Create a commit that includes two co-author credits in its message. Verify with `git log`.

**Solution**:
```bash
git add app.test.ts
git commit -m "test: add unit test suite

Co-authored-by: Alice Smith <alice@company.com>
Co-authored-by: Bob Jones <bob@company.com>"

git log -1
```
</details>

<details>
<summary><strong>Exercise 6: Amending Commit Without Changing Message</strong></summary>

**Task**: You forgot to include a documentation file in the last commit. Stage it and amend the commit without altering the original commit message.

**Solution**:
```bash
echo "# Docs" > README.md
git add README.md

# Amend silently
git commit --amend --no-edit

git show --stat HEAD
# README.md is now part of the latest commit!
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Command | Usage | Role |
| :--- | :--- | :--- |
| `git add -A` | Full repository | Stages all modifications, deletions, and untracked files |
| `git add -u` | Tracked only | Stages tracked modifications and deletions; ignores untracked |
| `git add -p <file>` | Interactive | Staged changes hunk-by-hunk |
| `git add -N <file>` | Intent-to-add | Allows untracked files to be diffed and patch-staged |
| `git commit -m "msg"` | Inline message | Creates commit with subject line |
| `git commit -v` | Verbose editor | Displays full diff in commit editor comments |
| `git commit --amend` | Update commit | Adds staged changes to HEAD commit and updates message |
| `git commit --fixup=<hash>` | Rebase helper | Prepares commit for automated autosquash |
| `Co-authored-by: Name <email>` | Commit trailer | Awards pair programming credit on GitHub |

---

## 13. Further Reading & Official Documentation

- [Official Git Documentation - git-add](https://git-scm.com/docs/git-add)
- [Official Git Documentation - git-commit](https://git-scm.com/docs/git-commit)
- [Conventional Commits 1.0.0 Specification](https://www.conventionalcommits.org/en/v1.0.0/)
- [GitHub Docs - Creating a commit with multiple authors](https://docs.github.com/en/pull-requests/committing-changes-to-your-project/creating-and-editing-commits/creating-a-commit-with-multiple-authors)

---

[<- 02-status-diff-log.md](./02-status-diff-log.md) | [Back to Module Overview](./README.md) | [04-undoing-changes.md ->](./04-undoing-changes.md)
