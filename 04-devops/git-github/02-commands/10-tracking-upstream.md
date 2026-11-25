<!--
================================================================================
Metadata:
  Title: Upstream Tracking Branches & Ahead/Behind Dynamics
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 09-remotes.md
  Key Terminology: Upstream Tracking, Ahead/Behind Calculation, @{upstream} / @{u}, git branch --set-upstream-to, push.autoSetupRemote, Refspec Mapping
================================================================================
-->

# Upstream Tracking Branches & Ahead/Behind Dynamics

[<- 09-remotes.md](./09-remotes.md) | [Back to Module Overview](./README.md) | [11-stash.md ->](./11-stash.md)

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

When you run `git status` on a cloned repository, you often see informational messages like:
```text
Your branch is ahead of 'origin/main' by 2 commits.
```
or
```text
Your branch and 'origin/main' have diverged,
and have 1 and 3 different commits each, respectively.
```

How does Git know which remote branch corresponds to your local branch? How does it compute the "ahead" and "behind" metrics without querying the internet on every command?

The answer lies in **Upstream Tracking Branches**.
An upstream tracking branch is a persistent, declarative link configured in `.git/config` that binds a local branch (`refs/heads/feature`) to a remote-tracking branch (`refs/remotes/origin/feature`).

Establishing proper tracking is critical because:
- It eliminates repetitive CLI boilerplate, enabling simple parameterless `git push` and `git pull`.
- It gives instant, offline visibility into whether your branch has diverged from your team's code.
- It unlocks powerful revision shortcuts like `@{upstream}` (`@{u}`) for rapid diffing and rebasing.

---

## 2. Core Concepts

### The Three Branches in Tracking Dynamics
When tracking is configured, three distinct entities participate in synchronization:
1. **Local Branch (`refs/heads/main`)**: The active branch on your workstation that moves forward as you make local commits.
2. **Remote-Tracking Branch (`refs/remotes/origin/main`)**: A local read-only bookmark that records the state of the remote branch as of the last `git fetch`.
3. **Remote Server Branch (`main` on GitHub)**: The actual branch reference stored on the remote forge.

```
+-----------------------------------------------------------------------------------+
|                           The Three Tracking Entities                             |
|                                                                                   |
|  LOCAL WORKSTATION DISK                                                           |
|                                                                                   |
|  [ Local Branch ] ────────── git push ──────────► [ Remote Branch on GitHub ]     |
|   refs/heads/main                                     (e.g. github.com/org/repo)  |
|          │                                                        │               |
|          │ Tracks via .git/config                                 │               |
|          ▼                                                        │ git fetch     |
|  [ Remote-Tracking Branch ] <─────────────────────────────────────┘               |
|   refs/remotes/origin/main (Cached Mirror)                                        |
+-----------------------------------------------------------------------------------+
```

### The Ahead / Behind Calculation
Ahead and behind counts are computed strictly against your **local remote-tracking cache** (`refs/remotes/origin/*`), not the live remote server!
- **Ahead $N$**: Your local branch has $N$ commits that do not exist on the remote-tracking branch.
- **Behind $M$**: The remote-tracking branch has $M$ commits that do not exist on your local branch.
- **Diverged (Ahead $N$, Behind $M$)**: Both branches have new commits since they diverged from their Lowest Common Ancestor (`git merge-base`).

---

## 3. How It Works Internally

### Tracking Configuration in `.git/config`
When you run `git push -u origin feature-auth` or `git branch --set-upstream-to=origin/feature-auth`, Git writes an explicit section into `.git/config`:

```ini
[branch "feature-auth"]
    remote = origin
    merge = refs/heads/feature-auth
```
- **`branch.<name>.remote`**: The remote name to push to and fetch from (e.g. `origin`).
- **`branch.<name>.merge`**: The full reference name on the remote that corresponds to this local branch.

### How Git Calculates Ahead / Behind Counts
To determine ahead and behind metrics, Git executes a two-endpoint revision graph walk:
```bash
# Ahead count: Commits reachable from local branch but NOT from remote-tracking branch
git rev-list --count refs/remotes/origin/main..refs/heads/main

# Behind count: Commits reachable from remote-tracking branch but NOT from local branch
git rev-list --count refs/heads/main..refs/remotes/origin/main
```

### The `@{upstream}` / `@{u}` Revision Selector
Once tracking is configured, Git provides the special shortcut `@{u}` (or `@{upstream}`):
- `git diff @{u}`: Diffs your local branch against its configured upstream tracking branch.
- `git log @{u}..HEAD`: Lists all commits you have created locally that have not yet been pushed.
- `git log HEAD..@{u}`: Lists all commits that have been fetched from the remote but not yet integrated locally.

---

## 4. Syntax & Usage

### 1. Setting and Updating Upstream Tracking
```bash
# Set upstream on initial push (-u / --set-upstream)
git push -u origin feature-login

# Set or update upstream tracking on an existing local branch
git branch --set-upstream-to=origin/feature-login feature-login

# Shortcut syntax while currently checked out on the branch
git branch -u origin/feature-login

# Remove upstream tracking configuration without deleting branch
git branch --unset-upstream

# Automatically set upstream on first push globally (Git 2.37+)
git config --global push.autoSetupRemote true
```

---

### 2. Inspecting Tracking Status & Diffing
```bash
# Display all local branches with their upstream tracking targets and ahead/behind counts
git branch -vv

# Short status showing branch and tracking status
git status -sb

# Print the full symbolic ref name of the upstream tracking branch
git rev-parse --symbolic-full-name @{u}

# View commits you are ahead by (ready to push)
git log @{u}..HEAD --oneline

# View commits you are behind by (ready to pull)
git log HEAD..@{u} --oneline

# Diff working tree directly against upstream tracking branch
git diff @{u}
```

---

## 5. Practical Examples

### Example 1: Resolving Divergent Upstream History
Suppose your feature branch is `ahead 1, behind 2` (both you and a colleague pushed changes to the same branch):

```bash
git status -sb
# Output: ## feature-auth...origin/feature-auth [ahead 1, behind 2]

# 1. Inspect what you have locally that remote lacks:
git log @{u}..HEAD --oneline
# Shows your 1 unpushed commit

# 2. Inspect what remote has that you lack:
git log HEAD..@{u} --oneline
# Shows colleague's 2 incoming commits

# 3. Rebase your local commit cleanly on top of incoming commits:
git pull --rebase

# Verify status after rebase:
git status -sb
# Output: ## feature-auth...origin/feature-auth [ahead 1] (Cleanly rebased, ready to push!)
```

### Example 2: Binding a Local Branch to a Differently-Named Remote Branch
A local branch does not have to share the exact same name as the remote branch:

```bash
# Bind local branch 'my-local-feature' to track 'origin/jira-1042-auth'
git branch -u origin/jira-1042-auth my-local-feature

# Inspect .git/config
git config --get branch.my-local-feature.merge
# Returns: refs/heads/jira-1042-auth
```

### Example 3: Auditing All Ahead Commits Across All Branches
Write a quick script to see what work across your entire repository has not been pushed:

```bash
git for-each-ref --format='%(refname:short) %(upstream:short) %(upstream:track)' refs/heads | grep -v '\[\]'
```
Example Output:
```text
feature-auth origin/feature-auth [ahead 2]
main origin/main [behind 1]
hotfix [gone]
```
*Note: `[gone]` indicates the remote branch was deleted on the server!*

### Example 4: Handling `[gone]` Upstream Branches
When GitHub deletes a branch after PR merge, `git branch -vv` reports `[gone]`:

```bash
git branch -vv | grep ': gone]'
# Output: feature-old  8a1e2f7 [origin/feature-old: gone] feat: old feature

# Cleanly delete local branches whose upstreams are gone:
git branch -vv | grep ': gone]' | awk '{print $1}' | xargs -r git branch -d
```

---

## 6. Best Practices

1. **Enable `push.autoSetupRemote = true` Globally**: Eliminates the annoying `fatal: The current branch has no upstream branch` error on initial branch pushes.
2. **Always Run `git fetch` Before Trusting Ahead/Behind Counts**: Remember that ahead/behind metrics in `git status` are calculated against your local cache. If you haven't run `git fetch` in 3 days, your behind count is 3 days out of date!
3. **Use `@{u}` in Aliases and Scripts**: Leverage `@{u}` to write portable scripts that don't need hardcoded branch or remote names (e.g. `git diff @{u}`).
4. **Clean Up `[gone]` Branches Regularly**: Prune dead tracking branches via `git fetch --prune` and delete obsolete local branches.
5. **Configure `pull.rebase = true`**: Ensures `git pull` maintains a clean, linear commit history when resolving behind counts.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `fatal: no upstream configured for branch '...'` | Local branch has no upstream tracking entry in `.git/config`. | Run `git push -u origin <branch>` or `git branch -u origin/<branch>`. |
| `git status` says "Your branch is up to date" but remote has new commits | You haven't fetched from the remote; status checks the local cache. | Run `git fetch origin` to update remote tracking refs. |
| `fatal: Cannot setup tracking: 'origin/...' does not exist` | Remote branch has not been fetched or does not exist on remote. | Run `git fetch origin` first, then set tracking. |
| Local branch name differs from remote branch name and confuses teammate | Renamed local branch without remapping tracking. | Reconfigure with `git branch -u origin/<remote-branch-name>`. |
| `git branch -vv` shows `[gone]` next to branch | The remote branch was merged and deleted on GitHub. | Delete local branch if work is done: `git branch -d <branch>`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Local Evaluation**: Computing ahead/behind counts executes in **<5 milliseconds** because Git traverses local commit-graph files in `.git/objects/` without making any network roundtrips.
- **Network Independence**: You can check upstream drift while disconnected on an airplane based on your last fetched state.

### Security
- **Branch Hijacking Prevention**: Upstream tracking mappings ensure `git push` updates only the explicitly configured remote branch, preventing accidental pushes to production or collateral branch overwrites.

---

## 9. Comparison Table

| Attribute | Local Branch | Remote-Tracking Branch | Upstream Binding |
| :--- | :--- | :--- | :--- |
| **Path / Storage** | `.git/refs/heads/<name>` | `.git/refs/remotes/<remote>/<name>` | `.git/config` (`[branch "..."]`) |
| **Mutability** | Directly writable by commits | Read-only local mirror | Configuration metadata |
| **Updated By** | `git commit`, `git merge` | `git fetch`, `git pull`, `git push` | `git branch -u`, `git push -u` |
| **Can Check Out?** | Yes (`git switch <name>`) | No (triggers detached HEAD) | N/A (relationship mapping) |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What is an upstream tracking branch in Git and how is it configured in `.git/config`?</strong></summary>

An upstream tracking branch is a configured link between a local branch (e.g. `refs/heads/feature`) and a remote-tracking branch (e.g. `refs/remotes/origin/feature`). It is stored in `.git/config` under the `[branch "<name>"]` section with two keys:
- `remote = <remote-name>` (specifies the remote repository, e.g. `origin`).
- `merge = refs/heads/<branch-name>` (specifies the target reference on the remote).
This configuration enables parameterless `git push` and `git pull`, and powers ahead/behind calculation in `git status`.
</details>

<details>
<summary><strong>2. How does Git calculate whether a branch is "ahead" or "behind" and why is it sometimes inaccurate?</strong></summary>

Git calculates ahead/behind counts by finding the Lowest Common Ancestor (merge base) between your local branch and the corresponding local remote-tracking branch (`refs/remotes/origin/*`).
- **Ahead**: Count of commits reachable from local branch but not remote-tracking branch.
- **Behind**: Count of commits reachable from remote-tracking branch but not local branch.
It can be "inaccurate" (out of date) if the developer has not executed `git fetch` recently, because Git computes these metrics strictly against the local cached snapshot, without making live network calls to the remote server.
</details>

<details>
<summary><strong>3. What does the `@{upstream}` or `@{u}` revision selector represent?</strong></summary>

`@{u}` (shorthand for `@{upstream}`) resolves to the exact commit hash currently pointed to by the remote-tracking branch that the active local branch is configured to track. For example, if you are on `feature` tracking `origin/feature`, `git diff @{u}` diffs your working tree against `origin/feature`, and `git log @{u}..HEAD` lists all unpushed commits.
</details>

<details>
<summary><strong>4. What does `[gone]` mean in the output of `git branch -vv`?</strong></summary>

`[gone]` indicates that the local branch has an upstream tracking configuration in `.git/config`, but the corresponding remote-tracking branch no longer exists in `refs/remotes/origin/` (usually because it was deleted on GitHub following a pull request merge and subsequently pruned locally via `git fetch --prune`). It signals to the developer that the feature branch has landed and is safe to delete.
</details>

<details>
<summary><strong>5. How does `push.autoSetupRemote = true` improve developer experience?</strong></summary>

Prior to Git 2.37, pushing a newly created local branch for the first time without `-u` resulted in `fatal: The current branch has no upstream branch`. Developers had to copy-paste `git push --set-upstream origin <branch>`. Setting `push.autoSetupRemote = true` instructs Git to automatically create the matching remote branch and configure tracking during the initial `git push` without requiring explicit CLI flags.
</details>

<details>
<summary><strong>6. Can a local branch track a remote branch with a different name?</strong></summary>

Yes. The local branch name and the remote branch name do not need to match. For example, running `git branch -u origin/jira-1042-billing my-billing-fix` configures the local branch `my-billing-fix` to push to and fetch from `jira-1042-billing` on remote `origin`.
</details>

<details>
<summary><strong>7. How do you remove an upstream tracking relationship without deleting either branch?</strong></summary>

By executing:
```bash
git branch --unset-upstream [<branch-name>]
```
This removes the `[branch "<name>"]` configuration section from `.git/config`, converting the branch into a standalone local branch that no longer tracks any remote.
</details>

<details>
<summary><strong>8. What is the difference between `@{upstream}` and `@{push}`?</strong></summary>

- `@{upstream}`: Resolves to the branch you pull/fetch from (defined by `branch.<name>.remote` and `branch.<name>.merge`).
- `@{push}`: Resolves to the branch you push to (which can differ from the fetch branch if asymmetric push refspecs or `push.default = upstream/current` are configured). In standard symmetric setups, both resolve to the same reference.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Setting Upstream on Push</strong></summary>

**Task**: In a test repository with a mock remote, create a feature branch, push it with `-u`, and verify the tracking entry in `.git/config`.

**Solution**:
```bash
mkdir /tmp/upstream-lab && cd /tmp/upstream-lab
git init --bare /tmp/hub.git
git clone /tmp/hub.git app && cd app
git switch -c main
echo "init" > app.txt && git add app.txt && git commit -m "init"
git push -u origin main

# Create feature and push with -u
git switch -c feat-auth
echo "auth" > auth.txt && git add auth.txt && git commit -m "auth commit"
git push -u origin feat-auth

# Inspect .git/config
git config --get branch.feat-auth.remote
# Output: origin
git config --get branch.feat-auth.merge
# Output: refs/heads/feat-auth
```
</details>

<details>
<summary><strong>Exercise 2: Inspecting Ahead/Behind Counts with `git branch -vv`</strong></summary>

**Task**: Create 2 local commits without pushing. Run `git branch -vv` and `git status -sb` to observe `[ahead 2]`.

**Solution**:
```bash
echo "c1" >> auth.txt && git commit -am "commit 1"
echo "c2" >> auth.txt && git commit -am "commit 2"

git status -sb
# Shows: ## feat-auth...origin/feat-auth [ahead 2]

git branch -vv
# Shows: * feat-auth <hash> [origin/feat-auth: ahead 2]
```
</details>

<details>
<summary><strong>Exercise 3: Querying the `@{u}` Revision Selector</strong></summary>

**Task**: Use `@{u}` to view only unpushed commits and diff your local branch against upstream.

**Solution**:
```bash
# View the 2 unpushed commits
git log @{u}..HEAD --oneline

# View diff of unpushed changes
git diff @{u}
```
</details>

<details>
<summary><strong>Exercise 4: Unsetting and Re-setting Upstream Tracking</strong></summary>

**Task**: Remove upstream tracking for `feat-auth`, verify that `git status` no longer reports ahead/behind, and restore tracking using `git branch -u`.

**Solution**:
```bash
# Unset tracking
git branch --unset-upstream

git status -sb
# Shows only: ## feat-auth (no tracking info!)

# Restore tracking
git branch -u origin/feat-auth

git status -sb
# Shows: ## feat-auth...origin/feat-auth [ahead 2]
```
</details>

<details>
<summary><strong>Exercise 5: Diagnosing `[gone]` Branches</strong></summary>

**Task**: Delete `feat-auth` on the remote hub, prune remotes, and observe `[gone]` in `git branch -vv`.

**Solution**:
```bash
# Delete remote branch
git push origin --delete feat-auth

# Prune tracking refs
git fetch --prune

# Check verbose branches
git branch -vv | grep feat-auth
# Shows: [origin/feat-auth: gone]
```
</details>

<details>
<summary><strong>Exercise 6: Configuring Auto-Setup Remote</strong></summary>

**Task**: Verify global auto-setup configuration and create a new branch to test automatic tracking on initial push.

**Solution**:
```bash
git config --global push.autoSetupRemote true
git switch -c feat-billing
echo "billing" > bill.txt && git add bill.txt && git commit -m "billing"

# Push without -u!
git push

# Tracking is automatically configured!
git branch -vv | grep feat-billing
# Displays [origin/feat-billing]!
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Command | Result |
| :--- | :--- | :--- |
| **Initial Push & Track** | `git push -u origin <branch>` | Pushes branch and creates upstream mapping |
| **Bind Tracking Locally** | `git branch -u origin/<branch>` | Binds active branch to remote tracking branch |
| **Unbind Tracking** | `git branch --unset-upstream` | Removes tracking link from `.git/config` |
| **Inspect Ahead/Behind** | `git branch -vv` | Displays tracking branch and ahead/behind metrics |
| **Short Tracking Status**| `git status -sb` | Concise branch tracking overview |
| **View Unpushed Commits**| `git log @{u}..HEAD --oneline` | Lists commits created locally not on remote |
| **View Unpulled Commits**| `git log HEAD..@{u} --oneline` | Lists commits fetched from remote not local |
| **Diff vs Upstream** | `git diff @{u}` | Diffs working tree against upstream branch |
| **Auto-Track on Push** | `git config --global push.autoSetupRemote true` | Auto-configures upstream without `-u` flag |

---

## 13. Further Reading & Official Documentation

- [Official Git Documentation - git-branch tracking options](https://git-scm.com/docs/git-branch#_options)
- [Pro Git Book - Tracking Branches](https://git-scm.com/book/en/v2/Git-Branching-Remote-Branches#_tracking_branches)
- [Git Revision Selection (@{u} syntax)](https://git-scm.com/docs/gitrevisions#_specifying_revisions)
- [GitHub Blog - Highlights from Git 2.37 (push.autoSetupRemote)](https://github.blog/2022-06-27-highlights-from-git-2-37/#push-autosetupremote)

---

[<- 09-remotes.md](./09-remotes.md) | [Back to Module Overview](./README.md) | [11-stash.md ->](./11-stash.md)
