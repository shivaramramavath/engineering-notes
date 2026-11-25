---
title: "Rewriting Shared History Safely: Force-with-Lease and Team Protocols"
category: "04-devops"
subcategory: "git-github / 03-history-rewriting"
tags: ["git", "force-push", "force-with-lease", "force-if-includes", "team-coordination", "history-rewriting", "branch-protection"]
difficulty: "Advanced"
estimated_read_time: "24 min"
prerequisites:
  - "02-commands/09-remotes"
  - "03-history-rewriting/02-rebase"
  - "03-history-rewriting/04-reflog-and-recovery"
last_updated: "2026-11-25"
---

# Rewriting Shared History Safely: Force-with-Lease and Team Protocols

> **Navigation**: [← Previous: 05-filter-repo-and-removing-secrets.md](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/05-filter-repo-and-removing-secrets.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/README.md) | [Next: 07-reset-vs-revert-vs-restore.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/07-reset-vs-revert-vs-restore.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices & Team Coordination Protocols](#6-best-practices--team-coordination-protocols)
7. [Common Mistakes](#7-common-mistakes)
8. [Performance, Security & Branch Governance](#8-performance-security--branch-governance)
9. [Comparison Table](#9-comparison-table)
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is Safe History Rewriting?
Whenever you rebase, amend, squash, or filter commits on a branch that has already been pushed to a remote server, your local branch diverges from the remote branch. Pushing your updated branch requires a "force push" because Git's default behavior refuses non-fast-forward updates. **Safe history rewriting** encompasses the tools (`--force-with-lease`, `--force-if-includes`) and protocols used to update remote history without obliterating work pushed by collaborators.

### Why Does It Matter?
1. **Preventing Catastrophic Code Erasure**: Running a blunt `git push --force` (`-f`) unconditionally overwrites the remote ref. If a teammate pushed commits to that branch five minutes ago, their work is instantly severed from the branch and orphaned on the server.
2. **Conditional Atomic Updates**: `--force-with-lease` turns a dangerous force push into a conditional "compare-and-swap" operation. It only updates the remote if the remote branch is in the exact state you expected.
3. **Addressing the Background Fetch Vulnerability**: Git 2.30 introduced `--force-if-includes` to address a vulnerability where automated background fetches (e.g. in IDEs) could update your remote-tracking ref and inadvertently satisfy `--force-with-lease` even when you hadn't reviewed the teammate's commits.

---

## 2. Core Concepts

### 1. Fast-Forward vs Non-Fast-Forward
- **Fast-Forward**: The remote branch tip is a direct ancestor of the commit being pushed. Git simply slides the pointer forward.
- **Non-Fast-Forward**: The local branch and remote branch have diverged (different commit hashes). Git rejects the push with:
  ```text
  ! [rejected]        feature -> feature (non-fast-forward)
  error: failed to push some refs to 'git@github.com:org/repo.git'
  hint: Updates were rejected because the tip of your current branch is behind
  hint: its remote counterpart.
  ```

### 2. Compare-and-Swap Mechanics (`--force-with-lease`)
`--force-with-lease` operates as an atomic compare-and-swap (CAS) check:
- It checks the value of the remote-tracking ref (e.g. `refs/remotes/origin/feature`).
- It contacts the remote repository and inspects the remote ref's actual current SHA.
- **Match**: If `remote == remote-tracking ref`, Git safely updates the remote to your new commit.
- **Mismatch**: If `remote != remote-tracking ref` (meaning someone else pushed commits to the remote), Git aborts the push immediately.

### 3. The Auto-Fetch Race Condition & `--force-if-includes`
If an IDE (such as VS Code or IntelliJ) runs `git fetch` periodically in the background, your local `refs/remotes/origin/feature` ref is updated to the teammate's commit automatically! If you then run `git push --force-with-lease`, the lease check passes because your local tracking ref matches the remote, yet you still overwrite the teammate's work because you never incorporated their commit into your rebase!
`--force-if-includes` verifies that the commit pointed to by the remote-tracking ref is actually an ancestor of your local commit before pushing.

---

## 3. How It Works Internally

```
Compare-and-Swap Flow with --force-with-lease:

[ Local Machine ]                              [ Remote Server (origin) ]
Local 'feature': Commit Z                      Actual 'feature': Commit Y
Local tracking 'origin/feature': Commit X
              │
              │  1. Check: Does Remote SHA == Local Tracking SHA?
              │     (Is Commit Y == Commit X?)
              ├─────────────────────────────────────────►
              │
              │  2. Mismatch Detected! (Commit Y != Commit X)
              │     A teammate pushed Commit Y!
              │◄─────────────────────────────────────────
              ▼
   [ Push Aborted Safely ]
   stale info: remote-tracking ref is behind
   error: failed to push some refs to 'origin'
```

### Protocol Interaction
During the Git send-pack negotiation:
```text
push-option: force-with-lease
ref-check: refs/heads/feature = <Expected-Old-SHA>
new-val:   refs/heads/feature = <New-Local-SHA>
```
If the server's ref does not equal `<Expected-Old-SHA>`, the push transaction fails atomically on the server side.

---

## 4. Syntax & Usage

```bash
# Standard safe force push
git push --force-with-lease

# Safest enterprise push (verifies tracking ref AND commit inclusion)
git push --force-with-lease --force-if-includes

# Explicit lease check specifying expected commit hash
git push --force-with-lease=refs/heads/feature:3a7b942 origin feature

# Recommended Git configuration alias
git config --global alias.pf "push --force-with-lease --force-if-includes"

# Enable force-if-includes globally by default (Git >= 2.30)
git config --global push.useForceIfIncludes true
```

---

## 5. Practical Examples

### Example 1: Standard Rebase and Safe Push Workflow
```bash
# 1. Developer finishes feature branch work
git log --oneline -n 3

# 2. Rebase on latest main
git fetch origin main
git rebase origin/main

# 3. Attempt standard push (fails because rebase rewrote SHAs)
git push origin feature-login
# ! [rejected] (non-fast-forward)

# 4. Push safely with lease
git push --force-with-lease origin feature-login
# Successfully updated refs/heads/feature-login
```

### Example 2: How `--force-with-lease` Protects Teammate Commits
```bash
# Scenario:
# You and Alice both work on 'feature/checkout'.
# Alice pushed commit 'C3' while you were squashing your commits locally.

# You attempt to push:
git push --force-with-lease origin feature/checkout

# Git output:
# ! [rejected]        feature/checkout -> feature/checkout (stale info)
# error: failed to push some refs to 'origin'

# 1. Fetch Alice's commit:
git fetch origin feature/checkout

# 2. Inspect what Alice added:
git log ..origin/feature/checkout

# 3. Rebase your work on top of Alice's new commit:
git rebase origin/feature/checkout

# 4. Now push safely:
git push --force-with-lease origin feature/checkout
```

### Example 3: Rescuing Work Overwritten by an Unchecked Force Push
```bash
# Scenario: A collaborator accidentally ran 'git push --force' and erased your commits.
# You need to recover the lost commits on the remote branch.

# 1. If you still have the commits locally:
git log -1 --format="%H" feature/my-work
# Output: e4f5a6b...

# 2. Re-push your work back onto the branch:
git push --force-with-lease origin feature/my-work

# 3. If nobody has the commits locally, inspect the GitHub/GitLab audit log or Events API:
# GET /repos/org/repo/events
# Locate the 'PushEvent' payload showing the pre-force 'before' SHA.
# Create a new branch pointing to that SHA:
git fetch origin e4f5a6b:restored-work
```

---

## 6. Best Practices & Team Coordination Protocols

1. **Never Ever Force Push to Default Branches**: Protect `main`, `master`, and release branches in GitHub repository settings by disabling force pushes entirely.
2. **Always Use `--force-with-lease --force-if-includes`**: Configure this as your default habit. Alias `git pf` or set `push.useForceIfIncludes = true`.
3. **Communicate on Shared Feature Branches**: If multiple engineers must collaborate on a single feature branch, establish a communication channel (Slack/Discord) before rebasing or prefer merge commits for multi-author feature branches.
4. **Use Ephemeral User-Specific Branches for Rebasing**: Follow the pattern `feature/username/description`. When a branch has a single owner, history rewriting is isolated and risk-free.
5. **Inspect Diff Before Pushing**: Run `git diff origin/<branch>` before force pushing to review exactly what the remote branch will lose or gain.
6. **Prefer Squash-and-Merge at the PR Boundary**: Maintain granular commits during development, and let GitHub's PR merge button perform the final squash onto `main`, avoiding local force pushes.

---

## 7. Common Mistakes

| Mistake | Consequence | Root Cause | Correct Procedure |
|:---|:---|:---|:---|
| Running `git push -f` | Permanently destroys teammates' commits on remote | Blind force push | Always use `--force-with-lease` |
| Auto-fetch in IDE overriding lease check | Accidental overwrite despite `--force-with-lease` | Remote-tracking ref updated in background | Enable `--force-if-includes` |
| Force pushing to `main` | Breaks repository history for entire engineering organization | Incorrect branch target or missing branch protection | Enable GitHub Branch Protection Rules |
| Collaborator rebasing shared release branch | Cascading conflicts across dozens of active PRs | Violating Golden Rule of Rebasing | Rebase private feature branches only |
| Forgetting to fetch before re-evaluating rejected push | Inability to see what teammate committed | Blindly retrying force push | Run `git fetch` and inspect incoming log |

---

## 8. Performance, Security & Branch Governance

- **Branch Protection Rules (GitHub)**: In GitHub/GitLab, configure Branch Protection Rules:
  - Check: **Require a pull request before merging**
  - Check: **Do not allow bypassing the above settings**
  - Check: **Block force pushes** (or ensure "Allow force pushes" is strictly restricted to repository admins with "Specify who can force push").
- **Audit Trails**: GitHub records force pushes in the repository timeline and Webhook audit streams with an explicit event notice: `user force-pushed the feature branch from <old-sha> to <new-sha>`.
- **Reflog on GitHub Servers**: GitHub maintains internal reflogs for remote repositories, enabling enterprise support to restore accidentally overwritten branches if contacted within the retention window.

---

## 9. Comparison Table

| Push Mechanism | Checks Remote Ref? | Checks Commit Ancestry? | Risk of Overwriting Work | Recommended Use Case |
|:---|:---:|:---:|:---:|:---|
| `git push` | Yes | Yes (Fast-forward only) | None | Normal daily commits |
| `git push --force` (`-f`) | **No** | **No** | **Extreme** | Never in collaborative environments |
| `git push --force-with-lease` | **Yes** (Matches tracking ref) | No | Moderate (vulnerable to auto-fetch) | Rebased solo feature branches |
| `git push --force-with-lease --force-if-includes` | **Yes** | **Yes** (Ensures ref integrated) | **Minimal** | **Gold Standard for all rebased branches** |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: Why is `git push --force-with-lease` safer than `git push --force`?</strong></summary>

`git push --force` unconditionally updates the remote branch pointer to the commit specified by the pusher, regardless of what commits currently exist on the remote. If another developer pushed commits to that branch in the meantime, their work is instantly overwritten.
`git push --force-with-lease` checks that the remote branch still matches the local repository's remote-tracking ref (`refs/remotes/origin/<branch>`). If someone else pushed to the remote branch, the remote SHA will differ from your local tracking SHA, and Git will reject the push with a `stale info` error.
</details>

<details>
<summary><strong>Q2: Explain the race condition vulnerability in `--force-with-lease` that prompted the creation of `--force-if-includes`.</strong></summary>

The vulnerability occurs when an external process—such as an IDE (VS Code, IntelliJ) or a cron script—executes `git fetch` in the background. When `git fetch` runs, it updates the local remote-tracking ref (`origin/<branch>`) to the teammate's new commit. When the developer later executes `git push --force-with-lease`, Git compares the remote ref to the local tracking ref. Because the tracking ref was just updated by the background fetch, the lease check succeeds, even though the developer never saw, reviewed, or incorporated the teammate's commits into their local rebase. `--force-if-includes` adds a second check: it verifies that the commit pointed to by the remote-tracking ref is actually present in the ancestry chain of the commit being pushed.
</details>

<details>
<summary><strong>Q3: How do you configure Git so that `--force-if-includes` is applied automatically whenever `--force-with-lease` is invoked?</strong></summary>

Run:
```bash
git config --global push.useForceIfIncludes true
```
With this setting enabled, passing `--force-with-lease` automatically enables `--force-if-includes`.
</details>

<details>
<summary><strong>Q4: If a teammate runs `git push --force` and erases your recent commits on a feature branch, how can you recover the lost code?</strong></summary>

1. If you still have the commits in your local working tree or local reflog, inspect your branch with `git log` or `git reflog`. Find the latest commit hash and force-push it back up using `--force-with-lease`.
2. If nobody has the branch locally, navigate to GitHub/GitLab's activity log or use the GitHub REST API (`GET /repos/:owner/:repo/events`) to find the `PushEvent`. The payload contains the `before` SHA (the branch tip immediately prior to the force push). You can then fetch that SHA directly from GitHub (`git fetch origin <sha>:recovered-branch`) and restore it.
</details>

<details>
<summary><strong>Q5: What is the recommended strategy when multiple developers must collaborate on the same feature branch?</strong></summary>

1. Avoid rebasing or amending on shared branches. Use standard merge commits (`git merge`) to integrate changes without rewriting history.
2. If history must be cleaned before merging into `main`, do not rebase on the shared branch during active development; instead, use GitHub's "Squash and Merge" feature when approving the pull request.
3. If rebasing is unavoidable, the author must coordinate with teammates via chat, ensure all teammates have pushed and committed, execute the rebase and push with `--force-with-lease`, and immediately notify the team so they can reset their local branches via `git fetch && git reset --hard origin/<branch>`.
</details>

<details>
<summary><strong>Q6: How do GitHub Branch Protection Rules enforce safe history practices?</strong></summary>

Branch protection rules can completely disable force pushes on critical branches (e.g., `main`, `production`). By enabling "Do not allow force pushes", GitHub's pre-receive hooks reject any push command that is non-fast-forward, regardless of whether `-f` or `--force-with-lease` is passed. This guarantees that history on protected branches is strictly append-only.
</details>

<details>
<summary><strong>Q7: Can `--force-with-lease` be used to check an explicit, specific commit hash rather than trusting the tracking ref?</strong></summary>

Yes. You can pass the specific expected commit hash directly in the lease option:
```bash
git push --force-with-lease=refs/heads/feature:<expected-commit-sha> origin feature
```
This guarantees that the remote branch will only be updated if its current tip exactly equals `<expected-commit-sha>`.
</details>

<details>
<summary><strong>Q8: What error does Git output when `--force-with-lease` detects that a remote branch has changed?</strong></summary>

Git prints:
```text
To git@github.com:org/repo.git
 ! [rejected]        feature -> feature (stale info)
error: failed to push some refs to 'git@github.com:org/repo.git'
```
The key indicator is `(stale info)`, signifying that the local tracking ref is out of sync with the true remote ref.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Configure Safe Force-Pushing Globally</strong></summary>

**Scenario**: Set up global Git configuration to enable `push.useForceIfIncludes` and create a global alias `pf`.
```bash
git config --global push.useForceIfIncludes true
git config --global alias.pf "push --force-with-lease"

# Verify settings
git config --global --get push.useForceIfIncludes
git config --global --get alias.pf
```
</details>

<details>
<summary><strong>Exercise 2: Simulate Compare-and-Swap Rejection</strong></summary>

**Scenario**: Create a local repo and a simulated remote (bare repo), push a branch, simulate an external commit, and observe `--force-with-lease` rejection.
```bash
mkdir safe-push-lab && cd safe-push-lab
git init --bare remote.git
git clone remote.git client1
git clone remote.git client2

# Client 1 creates initial commit
cd client1
echo "base" > file.txt && git add file.txt && git commit -m "feat: init"
git push origin master

# Client 2 pulls
cd ../client2
git pull origin master

# Client 1 pushes an update
echo "client1 update" >> file.txt && git commit -am "feat: update from client 1"
git push origin master

# Client 2 amends its initial commit locally (diverging history)
cd ../client2
git commit --amend -m "feat: init modified by client 2"

# Client 2 tries to force-with-lease WITHOUT fetching Client 1's update
git push --force-with-lease origin master
# Notice: Push is REJECTED with (stale info)! Client 1's work is protected!
```
</details>

<details>
<summary><strong>Exercise 3: Safe Recovery and Integration After Lease Rejection</strong></summary>

**Scenario**: Continuing from Exercise 2, fetch Client 1's changes, inspect, rebase Client 2's work, and push safely.
```bash
cd ../client2
git fetch origin master
git log --oneline ..origin/master
git rebase origin/master
git push --force-with-lease origin master
# Push now succeeds!
```
</details>

<details>
<summary><strong>Exercise 4: Verify Tracking Ref Ancestry with `--force-if-includes`</strong></summary>

**Scenario**: Test the behavior of `--force-if-includes` flag.
```bash
git push --force-with-lease --force-if-includes origin master
```
</details>

<details>
<summary><strong>Exercise 5: Inspect Remote Ref Differences Prior to Force Push</strong></summary>

**Scenario**: Run a diff against the remote-tracking ref to audit all incoming and outgoing changes.
```bash
git diff origin/master HEAD
git log --oneline --left-right origin/master...HEAD
```
</details>

<details>
<summary><strong>Exercise 6: Inspect Server Audit Logs</strong></summary>

**Scenario**: View ref updates and verify reflog entries for remote tracking branches.
```bash
git reflog show origin/master
```
</details>

---

## 12. Summary & Cheat Sheet

```bash
# Recommended Daily Safe Force Push Command
git push --force-with-lease --force-if-includes

# Specify explicit commit lease
git push --force-with-lease=refs/heads/<branch>:<expected-sha> origin <branch>

# Inspect incoming changes when push is rejected with "stale info"
git fetch origin <branch>
git log ..origin/<branch>

# Rebase onto the newly fetched commits
git rebase origin/<branch>

# Re-attempt safe push
git push --force-with-lease

# Global Configuration for Safety
git config --global push.useForceIfIncludes true
git config --global alias.pf "push --force-with-lease"
```

---

## 13. Further Reading
- [Official Git Documentation - git-push options](https://git-scm.com/docs/git-push#Documentation/git-push.txt---force-with-leaseltrefnamegt)
- [Git 2.30 Release Notes - force-if-includes](https://lore.kernel.org/git/xmqq35ye5ovs.fsf@gitster.c.googlers.com/)
- [GitHub Docs - Managing Protected Branches](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches)
- [03-history-rewriting/07-reset-vs-revert-vs-restore.md](file:///e:/Learning/engineering-notes/04-devops/git-github/03-history-rewriting/07-reset-vs-revert-vs-restore.md)
