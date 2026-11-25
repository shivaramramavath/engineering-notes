<!--
================================================================================
Metadata:
  Title: Technical Interview Preparation - Scenario-Based Troubleshooting & Recovery
  Track: Git & GitHub / DevOps
  Difficulty: Senior to Staff
  Estimated Time: 55 minutes
  Key Terminology: git reflog, git bisect, git-filter-repo, Submodules, Detached HEAD, Non-Fast-Forward, Unrelated Histories
================================================================================
-->

# Scenario-Based Troubleshooting & Recovery: 15 Production Scenarios

[<- Back to Interview Overview](./README.md) | [Next Note: 05-github-collaboration-qa.md ->](./05-github-collaboration-qa.md)

---

## Overview

In senior DevOps and platform engineering interviews, candidates are evaluated on their ability to diagnose and remediate real-world Git failure modes under pressure. This guide presents 15 detailed production scenarios covering data loss recovery, corrupted submodule references, automated bisect debugging, large file purging, and remote synchronization blockages.

---

## Scenarios Index

1. [Scenario 01: Junior Engineer Ran `git reset --hard` and Lost 3 Days of Work](#scenario-01-junior-engineer-ran-git-reset---hard-and-lost-3-days-of-work)
2. [Scenario 02: Accidental Commit of a 2GB SQL Dump Bloating Repository Size](#scenario-02-accidental-commit-of-a-2gb-sql-dump-bloating-repository-size)
3. [Scenario 03: Automating Root-Cause Bug Hunting with `git bisect run`](#scenario-03-automating-root-cause-bug-hunting-with-git-bisect-run)
4. [Scenario 04: Resolving "Fatal: Refusing to merge unrelated histories"](#scenario-04-resolving-fatal-refusing-to-merge-unrelated-histories)
5. [Scenario 05: Developer Committed and Made 5 Commits in "Detached HEAD" State](#scenario-05-developer-committed-and-made-5-commits-in-detached-head-state)
6. [Scenario 06: Git Push Fails with "Updates were rejected because the remote contains work"](#scenario-06-git-push-fails-with-updates-were-rejected-because-the-remote-contains-work)
7. [Scenario 07: Submodule Pointer Out of Sync or Pointing to Inaccessible Commit](#scenario-07-submodule-pointer-out-of-sync-or-pointing-to-inaccessible-commit)
8. [Scenario 08: Accidental Commit to `main` Instead of a Feature Branch](#scenario-08-accidental-commit-to-main-instead-of-a-feature-branch)
9. [Scenario 09: Reverting an Already-Merged Pull Request that Contained Bugs](#scenario-09-reverting-an-already-merged-pull-request-that-contained-bugs)
10. [Scenario 10: Cherry-Picking a Bugfix onto 3 Different LTS Maintenance Branches](#scenario-10-cherry-picking-a-bugfix-onto-3-different-lts-maintenance-branches)
11. [Scenario 11: Stash Conflict on Apply: "Your local changes would be overwritten by merge"](#scenario-11-stash-conflict-on-apply-your-local-changes-would-be-overwritten-by-merge)
12. [Scenario 12: Developer Force-Pushed and Overwrote a Colleague's Active Branch](#scenario-12-developer-force-pushed-and-overwrote-a-colleagues-active-branch)
13. [Scenario 13: Git Operations Are Sluggish on a Large Monorepo](#scenario-13-git-operations-are-sluggish-on-a-large-monorepo)
14. [Scenario 14: Recovering a Deleted Stash Entry](#scenario-14-recovering-a-deleted-stash-entry)
15. [Scenario 15: Commit Shows "Unverified" Despite Being Signed with Valid GPG Key](#scenario-15-commit-shows-unverified-despite-being-signed-with-valid-gpg-key)

---

## Scenarios & Solutions

### Scenario 01: Junior Engineer Ran `git reset --hard` and Lost 3 Days of Work
**Problem**: An engineer meant to undo an uncommitted file edit but executed `git reset --hard HEAD~3`, destroying 3 days of committed local work.
**Diagnosis**: In Git, committed data is almost never deleted immediately. The commit objects still exist in `.git/objects` and their historical positions are tracked in `.git/logs/HEAD`.
**Solution**:
1. Run `git reflog` on the engineer's workstation.
2. Identify the commit hash immediately before the reset (e.g., `HEAD@{1}: commit: feat(auth): add jwt`).
3. Restore the branch pointer:
   ```bash
   git reset --hard HEAD@{1}
   # Or create a recovery branch:
   git branch recovered-work HEAD@{1}
   ```
4. All commits, files, and tree states are 100% restored.

---

### Scenario 02: Accidental Commit of a 2GB SQL Dump Bloating Repository Size
**Problem**: A developer accidentally committed and pushed `dump.sql` (2GB) 10 commits ago. Deleting it in a new commit (`git rm dump.sql`) did not shrink repo clone size because the blob remains in Git history.
**Solution**:
1. Do NOT use deprecated `git filter-branch`. Use modern `git-filter-repo`.
2. Create a clean bare mirror clone:
   ```bash
   git clone --mirror git@github.com:org/repo.git repo-surgery.git
   cd repo-surgery.git
   ```
3. Purge the path completely across all branches and tags:
   ```bash
   git filter-repo --invert-paths --path dump.sql
   ```
4. Re-add remote and force-push:
   ```bash
   git remote add origin git@github.com:org/repo.git
   git push origin --force --mirror
   ```
5. Instruct team members to back up uncommitted work and re-clone fresh copies.

---

### Scenario 03: Automating Root-Cause Bug Hunting with `git bisect run`
**Problem**: A critical regression exists in production (`HEAD`), but tests passed 200 commits ago at tag `v2.4.0`. Manually checking out 200 commits is unacceptable.
**Solution**:
1. Author an automated test script (`test.sh`) that exits with code `0` if the code is good, and non-zero (`1`) if broken:
   ```bash
   #!/usr/bin/env bash
   npm test -- --grep "CheckoutFlow"
   ```
2. Initialize `git bisect`:
   ```bash
   git bisect start
   git bisect bad HEAD
   git bisect good v2.4.0
   ```
3. Run the automated binary search harness:
   ```bash
   git bisect run ./test.sh
   ```
4. Git executes binary search in $O(\log N)$ iterations (~8 steps for 200 commits), automatically checking out commits, running `test.sh`, and pinpointing the exact commit SHA that introduced the bug.

---

### Scenario 04: Resolving "Fatal: Refusing to merge unrelated histories"
**Problem**: A team created a repository on GitHub with an initial `README.md`, while another engineer ran `git init` locally and created initial code. Running `git pull origin main` aborts with `fatal: refusing to merge unrelated histories`.
**Diagnosis**: The two histories share zero common ancestor commits. Git halts to prevent accidental merging of two unrelated codebases.
**Solution**:
Instruct Git to merge independent commit roots:
```bash
git pull origin main --allow-unrelated-histories
```
Resolve any file conflicts (e.g., conflicting `README.md`), commit, and push.

---

### Scenario 05: Developer Committed and Made 5 Commits in "Detached HEAD" State
**Problem**: An engineer checked out a tag (`git checkout v1.2.0`), made 5 valuable commits, and realized they are in detached HEAD state. Switching branches now will orphan the commits.
**Solution**:
Simply attach a new branch to the current `HEAD` before switching:
```bash
git checkout -b feature-from-tag
```
Git creates the branch pointing to the latest commit of the detached sequence, safely preserving all 5 commits.

---

### Scenario 06: Git Push Fails with "Updates were rejected because the remote contains work"
**Problem**: Running `git push origin main` fails with non-fast-forward rejection. A coworker pushed commits to `main` while you were working.
**Solution**:
Do NOT use `git push --force`!
1. Fetch remote updates:
   ```bash
   git fetch origin main
   ```
2. Rebase your local commits on top of incoming remote work:
   ```bash
   git rebase origin/main
   ```
3. Push cleanly via fast-forward:
   ```bash
   git push origin main
   ```

---

### Scenario 07: Submodule Pointer Out of Sync or Pointing to Inaccessible Commit
**Problem**: Running `git submodule update` fails with `fatal: reference is not a tree: <sha>`. A developer updated the submodule pointer in the parent repo to a commit that only existed on their local workstation and was never pushed to the submodule remote.
**Solution**:
1. Check who committed the pointer update: `git log -S "<sha>" -- <submodule_path>`.
2. Contact the author to push their submodule branch to its remote.
3. If unreachable, revert the submodule pointer to the last known good commit SHA:
   ```bash
   cd <submodule_path>
   git checkout <last_good_sha>
   cd ..
   git add <submodule_path>
   git commit -m "fix(submodule): revert pointer to accessible upstream commit"
   ```

---

### Scenario 08: Accidental Commit to `main` Instead of a Feature Branch
**Problem**: A developer authored 3 commits directly on local `main` instead of creating `feature-billing`. The changes have not yet been pushed to remote.
**Solution**:
1. Create and switch to the new feature branch from current position:
   ```bash
   git branch feature-billing
   ```
2. Reset local `main` back to match the remote:
   ```bash
   git reset --hard origin/main
   ```
3. Checkout your feature branch:
   ```bash
   git checkout feature-billing
   ```
`main` is restored to clean remote state, and all 3 commits reside safely on `feature-billing`.

---

### Scenario 09: Reverting an Already-Merged Pull Request that Contained Bugs
**Problem**: Pull Request #88 was merged into `main` using a merge commit (`M`). Production is broken, and you must revert the entire PR without rewriting shared history.
**Solution**:
Use `git revert` with the `-m` (parent number) flag:
```bash
# Find merge commit hash (e.g., 4a1b2c)
# Parent 1 is the main branch; Parent 2 is the incoming feature branch
git revert -m 1 4a1b2c -m "revert: rollback PR #88 due to billing bug"
git push origin main
```
This applies an inverse diff of the entire pull request as a single new commit on `main`.

---

### Scenario 10: Cherry-Picking a Bugfix onto 3 Different LTS Maintenance Branches
**Problem**: A critical security vulnerability was patched on `main` in commit `9c8b7a`. It must be backported to `v1.2-lts`, `v1.3-lts`, and `v2.0-lts`.
**Solution**:
```bash
for branch in v1.2-lts v1.3-lts v2.0-lts; do
  git checkout "$branch"
  git pull origin "$branch"
  git cherry-pick -x 9c8b7a
  git push origin "$branch"
done
```
The `-x` flag appends `(cherry picked from commit 9c8b7a...)` to the commit message, maintaining audit traceability across release streams.

---

### Scenario 11: Stash Conflict on Apply: "Your local changes would be overwritten by merge"
**Problem**: An engineer stashed WIP edits, pulled latest changes, and ran `git stash pop`, triggering merge conflicts inside the stash application.
**Solution**:
1. The stash remains on the stash list because conflicts prevent automatic dropping.
2. Resolve the conflict markers (`<<<<<<< Updated upstream ... >>>>>>> Stashed changes`) in the files.
3. Stage the resolved files: `git add <resolved_files>`.
4. Manually drop the stash entry once verified:
   ```bash
   git stash drop stash@{0}
   ```

---

### Scenario 12: Developer Force-Pushed and Overwrote a Colleague's Active Branch
**Problem**: Developer Alice ran `git push --force` on a shared branch `feature-auth`, accidentally wiping out 10 commits pushed by Bob an hour earlier.
**Solution**:
1. On Bob's machine, the original commits still exist! Bob simply runs:
   ```bash
   git push --force-with-lease origin feature-auth
   ```
2. If Bob is offline, check GitHub's web interface or audit logs. GitHub retains unreferenced commits for days:
   Query GitHub API or inspect the PR timeline to find the overwritten commit SHA, then:
   ```bash
   git fetch origin <overwritten_sha>
   git checkout -b recovered-branch <overwritten_sha>
   git push origin recovered-branch
   ```
**Prevention**: Always mandate `git push --force-with-lease` rather than raw `--force`.

---

### Scenario 13: Git Operations Are Sluggish on a Large Monorepo
**Problem**: `git status` takes 15 seconds, and `git fetch` takes minutes on a 50GB monorepo.
**Solution**:
1. Enable filesystem monitor daemon (Watchman / built-in FSMonitor):
   ```bash
   git config core.fsmonitor true
   git config core.untrackedCache true
   ```
2. Run Git maintenance to optimize packfiles and commit graphs:
   ```bash
   git maintenance start
   git maintenance run --task=commit-graph --task=gc
   ```
3. Use sparse checkout to only populate the microservice directories you actively edit:
   ```bash
   git sparse-checkout set services/my-service packages/core
   ```

---

### Scenario 14: Recovering a Deleted Stash Entry
**Problem**: An engineer accidentally ran `git stash drop` on a stash containing critical uncommitted code.
**Solution**:
Stashes are simply commit objects with multiple parents. Dropping a stash deletes the ref pointer (`refs/stash`), but leaves the commit object dangling in `.git/objects`.
1. Find dangling commits:
   ```bash
   git fsck --no-reflogs | awk '/dangling commit/ {print $3}'
   ```
2. Inspect dangling commits to find the stash:
   ```bash
   git log --graph --oneline <commit_sha>
   git show <commit_sha>
   ```
3. Restore the stash as a new branch:
   ```bash
   git checkout -b recovered-stash <commit_sha>
   ```

---

### Scenario 15: Commit Shows "Unverified" Despite Being Signed with Valid GPG Key
**Problem**: An engineer signed their commit with a valid GPG key, but GitHub displays a yellow "Unverified" badge.
**Diagnosis & Solution**:
1. Check commit author email: `git log -1 --format="%ae"`.
2. Check GPG key email: `gpg --list-keys --keyid-format=long`.
3. Verify email on GitHub account: The email in the Git commit (`user.email`) MUST match both the email on the GPG key AND a verified email address added to the user's GitHub account settings.
4. Ensure the GPG public key has not expired.

---

[<- Back to Interview Overview](./README.md) | [Next Note: 05-github-collaboration-qa.md ->](./05-github-collaboration-qa.md)
