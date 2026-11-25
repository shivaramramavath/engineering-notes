# Quick Reference Cheatsheet: Git Undo & Disaster Recovery Matrix

[<- Back to Cheatsheets Overview](./README.md) | [Previous Cheatsheet: Git Essential Commands <-](./01-git-commands.md) | [Next Cheatsheet: Branching Strategies ->](./03-branching-strategies.md)

---

## 1. Quick Decision Matrix

| What happened? | State / Scope | Exact Remediation Command | Risk Level |
|:---|:---|:---|:---|
| Modified file locally, want to discard edits | Working Tree (Unstaged) | `git restore <file>` | Medium (Edits lost) |
| Staged a file accidentally with `git add` | Index (Staged) | `git restore --staged <file>` | Zero (Keeps edits) |
| Committed too early, want to add more files | Local Commit | `git add <files> && git commit --amend --no-edit` | Zero (Safe) |
| Bad commit message on latest commit | Local Commit | `git commit --amend -m "new message"` | Zero (Safe) |
| Undo last local commit, keep all file edits staged | Local Commit | `git reset --soft HEAD~1` | Zero (100% safe) |
| Undo last local commit, keep edits unstaged in working tree | Local Commit | `git reset HEAD~1` (or `--mixed`) | Zero (100% safe) |
| Completely destroy last local commit & all edits | Local Commit | `git reset --hard HEAD~1` | High (Data destroyed) |
| Accidental `git reset --hard` destroyed work | Local Reflog | `git reset --hard HEAD@{1}` (from `git reflog`) | Zero (Restores work) |
| Accidental branch deletion (`git branch -D`) | Local Reflog | `git checkout -b <name> <SHA_from_reflog>` | Zero (Restores branch) |
| Committed directly to `main` by mistake | Local Commits | `git branch feat && git reset --hard origin/main && git checkout feat` | Zero (Safe) |
| Merge has horrifying conflicts, want to cancel | Mid-Merge | `git merge --abort` | Zero (Restores state) |
| Rebase went off the rails with conflicts | Mid-Rebase | `git rebase --abort` | Zero (Restores state) |
| Pushed a bad commit to shared branch (`main`) | Remote Pushed | `git revert <commit-sha> && git push origin main` | Zero (Safe for teams) |
| Pushed a bad merge commit to `main` | Remote Pushed | `git revert -m 1 <merge-commit-sha>` | Zero (Safe for teams) |
| Accidentally dropped a stash | Dangling Commit | `git fsck --no-reflogs` -> `git checkout -b recovered <sha>` | Low |

---

## 2. Reset vs Revert vs Restore Deep Dive

### `git reset` (Local History Rewriting)
Moves the branch pointer backward. **Never use on commits already pushed to shared branches!**

```bash
# --soft: Moves branch ref. Leaves Index and Working Tree untouched.
git reset --soft HEAD~1

# --mixed (default): Moves branch ref and resets Index. Leaves Working Tree untouched.
git reset HEAD~1

# --hard: Moves branch ref, resets Index, and WIPES Working Tree to match commit.
git reset --hard HEAD~1
```

### `git revert` (Safe Remote Rollback)
Applies an inverse diff as a brand new commit. **Always use for public/shared branches!**

```bash
# Revert a single commit
git revert 4a3b1c

# Revert a merge commit (preserve parent 1)
git revert -m 1 8f2a1b

# Revert multiple commits without creating individual commits
git revert -n HEAD~3..HEAD
git commit -m "revert: rollback last 3 commits"
```

### `git restore` (Modern File-Level Undoing)
Replaces ambiguous `git checkout -- <file>` commands.

```bash
# Discard uncommitted changes in working tree
git restore src/app.js

# Unstage a file from the Index
git restore --staged src/app.js

# Restore file state from a specific commit
git restore --source=HEAD~2 src/app.js
```

---

## 3. The `git reflog` Emergency Playbook

When all else fails and you believe your code is lost forever, consult the reflog:

```bash
# 1. View local reference movements
git reflog

# Sample Output:
# 4a1b2c3 HEAD@{0}: reset: moving to HEAD~3
# 9f8e7d6 HEAD@{1}: commit: feat(billing): add paypal express
# 1a2b3c4 HEAD@{2}: checkout: moving from feature-auth to main

# 2. Inspect the commit before the disaster
git show HEAD@{1}

# 3. Create a rescue branch at that exact commit
git checkout -b disaster-recovery HEAD@{1}
```

---

## 4. Purging Leaked Secrets from History

When an API key or password is committed and pushed, use `git-filter-repo`:

```bash
# 1. Clone bare mirror
git clone --mirror git@github.com:org/repo.git
cd repo.git

# 2. Scrub literal text string across all files and commit messages
echo "my_secret_token_12345==>REDACTED" > /tmp/replace.txt
git filter-repo --replace-text /tmp/replace.txt

# 3. Force-push rewritten refs
git remote add origin git@github.com:org/repo.git
git push origin --force --mirror
```

---

[<- Back to Cheatsheets Overview](./README.md) | [Previous Cheatsheet: Git Essential Commands <-](./01-git-commands.md) | [Next Cheatsheet: Branching Strategies ->](./03-branching-strategies.md)
