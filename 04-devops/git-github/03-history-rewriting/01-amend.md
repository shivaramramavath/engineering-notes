---
title: "Git Commit Amending: git commit --amend"
category: "04-devops"
subcategory: "git-github / 03-history-rewriting"
tags: ["git", "amend", "history-rewriting", "commit", "staging", "reflog"]
difficulty: "Beginner-Intermediate"
estimated_read_time: "20 min"
prerequisites:
  - "01-fundamentals/04-commits"
  - "01-fundamentals/05-git-objects"
  - "02-commands/03-add-commit"
last_updated: "2026-11-25"
---

# Git Commit Amending: `git commit --amend`

> **Navigation**: [← Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/README.md) | [Next: 02-rebase.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/02-rebase.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes](#7-common-mistakes)
8. [Performance, Security & Cost](#8-performance-security--cost)
9. [Comparison Table](#9-comparison-table)
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is `git commit --amend`?
`git commit --amend` is a Git porcelain command that modifies the most recent commit on the current branch. Rather than creating a subsequent child commit, it creates a new replacement commit object incorporating staged changes and/or an updated commit message, moving the branch tip to this new commit while stranding the original commit in Git's object store.

### Why Do We Use It?
1. **Fixing Immediate Oversights**: Forgetting to stage an edited file, leaving a stray debugging print statement, or introducing a typo in the commit message can be corrected instantly before sharing code.
2. **Atomic, Clean History**: Prevents messy commit histories cluttered with "oops fix typo", "forgot file", or "lint fix" commits that degrade pull request reviews and `git bisect` readability.
3. **Preserving Logical Sequence**: Keeps a self-contained unit of work as a single logical commit instead of dispersing it across micro-commits.

---

## 2. Core Concepts

### 1. Replacement, Not Modification
Git objects in `.git/objects` are strictly immutable because their 40-character SHA-1 (or 64-character SHA-256) hashes are cryptographic digests of their exact contents, tree hash, parent commit hash, author timestamp, and committer timestamp. You cannot edit an existing commit. `git commit --amend` creates an entirely new commit object with the same parent(s) as the original commit and updates the current branch ref.

### 2. Message Modification vs Content Modification
- **Message-only amend**: `git commit --amend -m "new message"` leaves the tracked tree intact and updates only the commit metadata and log message.
- **Content amend**: Staging files (`git add <file>`) followed by `git commit --amend --no-edit` includes the staged files into the new commit's tree without prompting to change the message.
- **Combined amend**: Staging files and running `git commit --amend` updates both tree contents and message simultaneously.

### 3. Committer vs Author Metadata
- `Author`: The original person who wrote the code and the timestamp when the code was originally authored (`AuthorDate`). By default, `--amend` preserves the original `Author` name, email, and timestamp.
- `Committer`: The person who performed the amendment and the exact timestamp when `--amend` was executed (`CommitDate`). This timestamp is updated to the current time.

### 4. Dangling Objects and the Reflog
The original commit is not deleted immediately. Its SHA identifier remains recorded in `.git/logs/HEAD` and `.git/logs/refs/heads/<branch>` (the reflog). It only becomes unreachable (dangling) from the branch pointer and is eventually collected by `git gc` after the reflog expiration window (default 90 days).

---

## 3. How It Works Internally

```
Before `git commit --amend`:
                       [HEAD, main]
                            │
                            ▼
    [ Commit A ] ◄──── [ Commit B ] (Hash: 3a7b)
       Parent             Tree: t1
                          Parent: Commit A
                          Msg: "feat: add user login"

============================================================
Execution: Staged changes in Index, run:
           git commit --amend -m "feat(auth): add user login with JWT"
============================================================

After `git commit --amend`:
                       [HEAD, main]
                            │
                            ▼
    [ Commit A ] ◄──── [ Commit B' ] (Hash: 9e2c - NEW COMMIT)
       Parent ▲           Tree: t2 (includes newly staged files)
              │           Parent: Commit A (SAME PARENT)
              │           CommitterDate: Updated
              │           Msg: "feat(auth): add user login with JWT"
              │
              └──────── [ Commit B ] (Hash: 3a7b - DEREFERENCED / ORPHAN)
                          (Accessible via reflog: HEAD@{1})
```

### Git Object Database Operations
1. Git writes the staged index as a new tree object into `.git/objects/`.
2. Git constructs a new commit object referencing:
   - The newly written tree SHA.
   - The parent commit(s) of the commit being amended.
   - The preserved or updated Author and AuthorDate.
   - The current user as Committer and the current time as CommitDate.
   - The new or existing commit message.
3. Git updates `.git/refs/heads/<current-branch>` to point to the new commit SHA.
4. Git appends an entry to `.git/logs/HEAD` and `.git/logs/refs/heads/<branch>` recording the action `commit (amend): <message>`.

---

## 4. Syntax & Usage

```bash
# Amend the commit message interactively in the default editor
git commit --amend

# Amend the commit message inline without launching an editor
git commit --amend -m "fix(auth): correct token expiration logic"

# Add staged changes to the last commit without changing the message
git add src/auth/token.service.ts
git commit --amend --no-edit

# Update author information (e.g., if committed with wrong work email)
git commit --amend --author="Jane Doe <jane.doe@enterprise.com>" --no-edit

# Reset author timestamp to current system time
git commit --amend --date="now" --no-edit

# Reset author timestamp to an explicit historical ISO date
git commit --amend --date="2026-11-20T14:30:00" --no-edit

# Reset both author and committer to current gitconfig identity
git commit --amend --reset-author --no-edit
```

---

## 5. Practical Examples

### Example 1: Forgot to Include a File
```bash
# 1. Developer commits work
git commit -m "feat(billing): implement invoice PDF export"

# 2. Developer realizes test file was omitted
git status -s
# ?? tests/billing/invoice.spec.ts

# 3. Stage omitted file and amend without touching commit message
git add tests/billing/invoice.spec.ts
git commit --amend --no-edit

# 4. Verify log shows a single clean commit with updated files
git show --stat
```

### Example 2: Fixing a Typo in Commit Message
```bash
# Initial flawed commit
git commit -m "feat(api): add userr authenticaiton"

# Correct immediately without opening editor
git commit --amend -m "feat(api): add user authentication"

# Confirm commit subject
git log -1 --oneline
```

### Example 3: Correcting Wrong Git Author Email
```bash
# Committed accidentally with personal email on corporate repo
git log -1 --format="Author: %an <%ae>"
# Output: Author: John Doe <johndoe@personal-gmail.com>

# Amend to corporate email
git commit --amend --author="John Doe <john.doe@company.com>" --no-edit

# Verify author metadata
git log -1 --format="Author: %an <%ae>"
```

### Example 4: Recovering an Amended Commit via Reflog
```bash
# Suppose developer mistakenly amended and lost critical code
git commit --amend --no-edit

# View reflog to identify pre-amend commit hash
git reflog -n 5
# 9e2cf31 HEAD@{0}: commit (amend): feat(billing): invoice export
# 3a7b942 HEAD@{1}: commit: feat(billing): invoice export

# Inspect original commit
git show 3a7b942

# Restore original commit as current branch tip
git reset --hard HEAD@{1}
```

### Example 5: Removing a File Accidentally Added to the Last Commit
```bash
# Accidentally committed .env or credentials file
git add .
git commit -m "feat: complete initial backend setup"

# Unstage the secret file from the last commit without losing local disk contents
git rm --cached .env

# Amend the commit so the file is omitted from git history
git commit --amend --no-edit

# Verify the file is untracked on disk
git status -s
# ?? .env
```

---

## 6. Best Practices

1. **Only Amend Local, Unpushed Commits**: Modifying a commit already pushed to a remote repository requires a force push (`--force-with-lease`), disrupting teammates collaborating on that branch.
2. **Use `--no-edit` for Pure Code Additions**: When merely patching in a missed test or formatting fix, `--no-edit` avoids unnecessary editor prompts and accidental message edits.
3. **Always Check `git status` Before Amending**: Ensure you have not accidentally staged unrelated modified files in your staging area before running `--amend`.
4. **Use `--reset-author` When Taking Over Orphaned Commits**: If pairing or rebasing commits from another machine, `--reset-author` explicitly resets author timestamps and identity to match your active configuration.
5. **Inspect the Result with `git show`**: Immediately inspect the amended commit (`git show --stat`) to verify file inclusions and diffs match expectations.
6. **Prefer Commit Amends to "Fix Typo" Commits**: Maintain high-signal git history by amending minor oversights rather than publishing micro-fix commits.

---

## 7. Common Mistakes

| Mistake | Manifestation | Root Cause | Correct Alternative |
|:---|:---|:---|:---|
| Amending pushed public branches (`main`) | Remote rejects push (`non-fast-forward`); teammates get diverged trees | Hash changed on shared branch | Revert or create forward fix commit (`git revert` or new commit) |
| Blind `git commit --amend` without staging | Unintended message prompt; no file updates | Forgot `git add <file>` prior to amend | Run `git add <file>` first, then `git commit --amend --no-edit` |
| Accidental staging leakage | Unrelated files permanently folded into commit | `git add .` was run beforehand without checking `git status` | Review `git diff --staged` before running amend |
| Force pushing with `-f` instead of `--force-with-lease` | Teammate's commits overwritten on remote | Blind `--force` overrides any remote changes | Always use `git push --force-with-lease` |
| Assuming amend can modify older commits | Modifies only `HEAD` | `--amend` strictly targets the immediate latest commit | Use interactive rebase (`git rebase -i HEAD~N`) |
| Panicking after a bad amend | Developer believes original work is destroyed | Unawareness of Git reflog | Recover original commit hash via `git reflog` |

---

## 8. Performance, Security & Cost

- **Storage Footprint**: An amended commit leaves the orphaned commit object in `.git/objects/`. While negligible for code diffs, amending large binary files leaves dangling blobs until garbage collection triggers (`git gc --prune=now`).
- **Cryptographic Signatures**: Amending a cryptographically signed commit (GPG/SSH) invalidates the original signature. You must pass `-S` or ensure `commit.gpgSign = true` to sign the replacement commit.
- **CI/CD Triggers**: Pushing an amended commit with `--force-with-lease` cancels or invalidates ongoing CI pipeline runs for the previous commit SHA and launches a fresh build for the new SHA.

---

## 9. Comparison Table

| Feature | `git commit --amend` | `git rebase -i` | `git reset --soft HEAD~1` | `git revert HEAD` |
|:---|:---|:---|:---|:---|
| **Target Commit** | Only the most recent (`HEAD`) | Any commit in history (`HEAD~N`) | Only the most recent (`HEAD`) | Any specific commit |
| **Commit Hash** | Creates new SHA | Creates new SHAs for target & children | Deletes HEAD commit; leaves index intact | Creates a new forward commit |
| **History Mutated** | Yes (rewrites HEAD) | Yes (rewrites multiple commits) | Yes (destroys commit object reference) | No (strictly append-only) |
| **Safe for Public Branches** | No (requires force push) | No (requires force push) | No (requires force push) | **Yes** (clean forward history) |
| **Interactive Prompts** | Optional (message editor) | Yes (todo script editor) | No (silent pointer reset) | Optional (revert message editor) |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What happens to the original commit object when you execute `git commit --amend`?</strong></summary>

The original commit object is not deleted or modified. Because Git objects are immutable, the original commit remains intact in `.git/objects/`. However, the branch reference pointer is updated to point to the newly generated commit object. The original commit becomes an "unreachable" or "dangling" commit, though its SHA remains accessible through the reflog (`.git/logs/HEAD`) until garbage collection (`git gc`) purges it after the reflog expiration window (default 90 days).
</details>

<details>
<summary><strong>Q2: Why does `git commit --amend` produce a completely different commit hash even if you only change the message?</strong></summary>

A commit object's SHA hash is calculated over its entire header and content:
```
commit <size>\0
tree <tree_sha>
parent <parent_sha>
author <name> <email> <author_timestamp>
committer <name> <email> <committer_timestamp>

<commit message>
```
Changing the commit message alters the byte content of the object. Furthermore, even if you run `--no-edit` and change only files, the `tree` SHA and `committer_timestamp` change. Any byte change produces a completely distinct cryptographic hash.
</details>

<details>
<summary><strong>Q3: How does Git distinguish between the Author and the Committer when amending a commit?</strong></summary>

Git tracks two distinct identities and timestamps per commit:
- `Author`: The original creator of the patch and the date it was initially written (`GIT_AUTHOR_NAME`, `GIT_AUTHOR_DATE`). `--amend` preserves this by default unless `--reset-author` or `--date` is explicitly passed.
- `Committer`: The person who last applied, amended, or rebased the commit (`GIT_COMMITTER_NAME`, `GIT_COMMITTER_DATE`). `--amend` always updates the committer to the current active Git user and sets the committer timestamp to the current system time.
</details>

<details>
<summary><strong>Q4: A developer amended a commit and pushed it to remote with `--force-with-lease`. Another developer had already based a feature branch off the original commit. What problem occurs and how is it resolved?</strong></summary>

The second developer will experience a diverged history. Their branch still references the old, orphaned commit parent which no longer exists on the upstream tracking branch. When they pull or rebase, Git may attempt to re-apply duplicate commits or report merge conflicts.
**Resolution**: The second developer must run:
```bash
git fetch origin
git rebase --onto origin/<branch> <old-commit-sha> <their-feature-branch>
```
This transplants their commits onto the new amended commit tip.
</details>

<details>
<summary><strong>Q5: Can you use `git commit --amend` to unstage a file that was mistakenly committed in the last commit? How?</strong></summary>

Yes. First, remove the file from the index while keeping it in the working tree:
```bash
git rm --cached <path/to/file>
```
Then run:
```bash
git commit --amend --no-edit
```
The newly created commit will reference an index tree that omits the removed file, effectively removing it from git tracking while leaving the local file untouched on disk.
</details>

<details>
<summary><strong>Q6: How do you recover a commit that was accidentally overwritten by `git commit --amend`?</strong></summary>

Run `git reflog` to view recent state changes of `HEAD`. Identify the entry immediately preceding the amend operation (typically `HEAD@{1}` with description `commit: <original message>`). Run:
```bash
git reset --hard HEAD@{1}
```
This points the branch ref back to the original commit SHA.
</details>

<details>
<summary><strong>Q7: What is the difference between `git commit --amend --no-edit` and `git commit --fixup <commit>`?</strong></summary>

`git commit --amend --no-edit` immediately rewrites `HEAD` locally in place. `git commit --fixup <commit>` creates a new, separate commit whose commit message is formatted as `fixup! <subject>`. This commit can target any historical commit in the branch. When you later run `git rebase -i --autosquash`, Git automatically rearranges and squashes all fixup commits into their respective parent targets.
</details>

<details>
<summary><strong>Q8: Why is `git commit --amend` forbidden on protected main branches in CI/CD environments?</strong></summary>

Protected branches enforce append-only linear or merge histories. Because amending changes the commit hash, pushing it requires a non-fast-forward force push (`push -f`). Force pushes break downstream pull requests, invalidate ongoing deployment artifacts tied to specific commit SHAs, and can permanently erase teammate contributions.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Fix a Missed File via Amend</strong></summary>

**Scenario**: Create a test repo, commit `file1.txt`, forget `file2.txt`, and amend the commit without opening an editor.
```bash
mkdir amend-lab && cd amend-lab && git init
echo "version 1" > file1.txt
git add file1.txt && git commit -m "feat: initial commit"

# Missed file:
echo "helper" > file2.txt
git add file2.txt
git commit --amend --no-edit

# Verify single commit contains both files:
git log --oneline
git show --stat
```
</details>

<details>
<summary><strong>Exercise 2: Reword the Latest Commit Message</strong></summary>

**Scenario**: Change the commit message of `HEAD` to follow Conventional Commits standard.
```bash
git commit --amend -m "feat(core): initialize base project files"
git log -1 --pretty=format:"%s%n"
```
</details>

<details>
<summary><strong>Exercise 3: Rescue an Amended Commit from Reflog</strong></summary>

**Scenario**: Make a bad amend that drops a file, then restore the pre-amend commit using `git reflog`.
```bash
# Add file3, commit, then amend with bad message
echo "important" > file3.txt
git add file3.txt && git commit -m "feat: add important logic"
git commit --amend -m "bad commit"

# Find pre-amend hash
ORIGINAL_HASH=$(git reflog -n 2 --format="%h" | sed -n '2p')
git reset --hard $ORIGINAL_HASH
git log -1 --oneline
```
</details>

<details>
<summary><strong>Exercise 4: Reset Author Identity on Amend</strong></summary>

**Scenario**: Amend a commit while explicitly updating the author name and email.
```bash
git commit --amend --author="Lead Dev <lead@enterprise.org>" --no-edit
git log -1 --format="Author: %an <%ae> | Committer: %cn <%ce>"
```
</details>

<details>
<summary><strong>Exercise 5: Exclude a Secret File via Amend</strong></summary>

**Scenario**: Commit a file `api.key`, realize the mistake, and amend the commit so `api.key` is untracked.
```bash
echo "SECRET_KEY_12345" > api.key
git add api.key && git commit -m "feat: configure secrets"

# Untrack and amend
git rm --cached api.key
echo "api.key" >> .gitignore
git add .gitignore
git commit --amend -m "feat: configure secrets with gitignore"

# Verify api.key is not in commit tree
git ls-tree --name-only HEAD
```
</details>

<details>
<summary><strong>Exercise 6: Inspect Object Store Differences Before and After Amend</strong></summary>

**Scenario**: Compare the SHA identifiers and tree hashes before and after an amend.
```bash
OLD_COMMIT=$(git rev-parse HEAD)
OLD_TREE=$(git rev-parse HEAD^{tree})

echo "change" >> file1.txt
git add file1.txt
git commit --amend --no-edit

NEW_COMMIT=$(git rev-parse HEAD)
NEW_TREE=$(git rev-parse HEAD^{tree})

echo "Old Commit: $OLD_COMMIT | New Commit: $NEW_COMMIT"
echo "Old Tree:   $OLD_TREE   | New Tree:   $NEW_TREE"
# Both commit and tree SHAs will differ!
```
</details>

---

## 12. Summary & Cheat Sheet

```bash
# Amend commit message
git commit --amend -m "new message"

# Add staged changes to last commit without changing message
git add <files>
git commit --amend --no-edit

# Change author details
git commit --amend --author="Name <email@domain.com>" --no-edit

# Reset author timestamp to current time
git commit --amend --date="now" --no-edit

# Recover previous version if amend was accidental
git reflog
git reset --hard HEAD@{1}

# Safe push after amending an already-pushed branch
git push --force-with-lease
```

---

## 13. Further Reading
- [Official Git Documentation - git-commit](https://git-scm.com/docs/git-commit)
- [Pro Git Book - Changing the Last Commit](https://git-scm.com/book/en/v2/Git-Tools-Rewriting-History#_git_amend)
- [Git Reflog Documentation](https://git-scm.com/docs/git-reflog)
- [03-history-rewriting/02-rebase.md](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/02-rebase.md)
