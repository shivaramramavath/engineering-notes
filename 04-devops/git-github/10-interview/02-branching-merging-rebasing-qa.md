<!--
================================================================================
Metadata:
  Title: Technical Interview Preparation - Branching, Merging & Rebasing Q&A
  Track: Git & GitHub / DevOps
  Difficulty: Senior to Staff
  Estimated Time: 55 minutes
  Key Terminology: Fast-Forward, 3-Way Merge, Merge Commits, Rebase, Squash, Cherry-Pick, ort Merge Strategy, git rebase --onto
================================================================================
-->

# Branching, Merging & Rebasing: 20 In-Depth Interview Questions & Answers

[<- Back to Interview Overview](./README.md) | [Next Note: 03-internals-qa.md ->](./03-internals-qa.md)

---

## Overview

This guide covers 20 advanced, architectural interview questions focusing on Git's branching mechanics, merge strategies, rebase operations, history linearisation tradeoffs, and conflict resolution internals.

---

## Questions Index

1. [Q01: What is a Git branch physically at the filesystem level?](#q01-what-is-a-git-branch-physically-at-the-filesystem-level)
2. [Q02: Compare and contrast `git merge` vs `git rebase`. When should you use which?](#q02-compare-and-contrast-git-merge-vs-git-rebase-when-should-you-use-which)
3. [Q03: What is the Golden Rule of Rebasing, and what happens when it is violated?](#q03-what-is-the-golden-rule-of-rebasing-and-what-happens-when-it-is-violated)
4. [Q04: How does a Squash Merge work, and what are its architectural advantages and drawbacks?](#q04-how-does-a-squash-merge-work-and-what-are-its-architectural-advantages-and-drawbacks)
5. [Q05: Explain the difference between `git merge --no-ff` and a standard merge.](#q05-explain-the-difference-between-git-merge---no-ff-and-a-standard-merge)
6. [Q06: How does the new default `ort` merge strategy differ from the legacy `recursive` strategy?](#q06-how-does-the-new-default-ort-merge-strategy-differ-from-the-legacy-recursive-strategy)
7. [Q07: Explain how `git cherry-pick` operates under the hood.](#q07-explain-how-git-cherry-pick-operates-under-the-hood)
8. [Q08: What is `git rebase --onto`, and provide a production scenario where it is indispensable.](#q08-what-is-git-rebase---onto-and-provide-a-production-scenario-where-it-is-indispensable)
9. [Q09: How does Git compute the "common ancestor" during a 3-way merge?](#q09-how-does-git-compute-the-common-ancestor-during-a-3-way-merge)
10. [Q10: What is a "criss-cross merge", and why does it cause problems for older merge strategies?](#q10-what-is-a-criss-cross-merge-and-why-does-it-cause-problems-for-older-merge-strategies)
11. [Q11: Explain how `git rerere` works and how it accelerates long-lived branch rebasing.](#q11-explain-how-git-rerere-works-and-how-it-accelerates-long-lived-branch-rebasing)
12. [Q12: Why does `git pull` often create unintended merge commits, and how do you configure Git to prevent it?](#q12-why-does-git-pull-often-create-unintended-merge-commits-and-how-do-you-configure-git-to-prevent-it)
13. [Q13: What is the difference between `git cherry-pick A..B` and `git cherry-pick A...B`?](#q13-what-is-the-difference-between-git-cherry-pick-ab-and-git-cherry-pick-ab)
14. [Q14: What happens to commit metadata (author, committer, timestamps) during a rebase?](#q14-what-happens-to-commit-metadata-author-committer-timestamps-during-a-rebase)
15. [Q15: How do you abort a merge or rebase that has encountered overwhelming conflicts?](#q15-how-do-you-abort-a-merge-or-rebase-that-has-encountered-overwhelming-conflicts)
16. [Q16: What is a fast-forward-only merge policy, and why do CI systems like GitHub enforce it?](#q16-what-is-a-fast-forward-only-merge-policy-and-why-do-ci-systems-like-github-enforce-it)
17. [Q17: What does the `--autostash` flag do during a `git rebase` or `git pull`?](#q17-what-does-the---autostash-flag-do-during-a-git-rebase-or-git-pull)
18. [Q18: How do you identify which branch a specific commit originated from in a complex DAG?](#q18-how-do-you-identify-which-branch-a-specific-commit-originated-from-in-a-complex-dag)
19. [Q19: Explain the difference between `ours` and `theirs` merge strategies vs merge options (`-X ours`).](#q19-explain-the-difference-between-ours-and-theirs-merge-strategies-vs-merge-options--x-ours)
20. [Q20: Why can repeatedly cherry-picking commits between release branches cause future merge conflicts?](#q20-why-can-repeatedly-cherry-picking-commits-between-release-branches-cause-future-merge-conflicts)

---

## Questions & Answers

### Q01: What is a Git branch physically at the filesystem level?
**Answer:**  
In Git, a branch is not a copy of directories or files; it is simply a 41-byte text file located inside `.git/refs/heads/<branch-name>` containing a 40-character SHA hash of a commit object followed by a newline. Creating a new branch (`git branch feature`) merely writes the current commit SHA into a new text file. This is why branching in Git is instantaneous ($O(1)$ time complexity) regardless of repository size.

---

### Q02: Compare and contrast `git merge` vs `git rebase`. When should you use which?
**Answer:**  
- **`git merge`**: Creates a non-destructive merge commit with two parents, preserving the exact historical chronology, context, and commit timestamps of both branches. It leaves individual branch commits unaltered.
  - *Best For*: Preserving historical audit trails, large shared feature branches, and integrating features into `main`.
- **`git rebase`**: Takes the commits from the current branch and replays them sequentially onto the tip of the target branch, rewriting commit SHAs and producing a perfectly linear, sequential history without merge bubble clutter.
  - *Best For*: Cleaning up local private feature branches before opening a PR, synchronizing a feature branch with latest `main`.

---

### Q03: What is the Golden Rule of Rebasing, and what happens when it is violated?
**Answer:**  
> **"Never rebase commits that have been pushed to a public/shared remote repository."**

When you rebase, Git creates *new* commit objects with *new* SHAs, abandoning the old commits. If other engineers have already pulled and based their work on the original commits, force-pushing a rebase invalidates their local git trees. When they run `git pull`, Git will attempt to merge the old commits with the new rebased duplicates, resulting in severe merge conflicts, duplicated commits in history, and massive confusion.

---

### Q04: How does a Squash Merge work, and what are its architectural advantages and drawbacks?
**Answer:**  
A Squash Merge takes all commits on a feature branch, combines their combined diff into a single new commit object, and appends it to the target branch.
- **Advantages**: Keeps trunk history (`main`) exceptionally clean and readable; eliminates messy intermediate commits (`"wip"`, `"fix typo"`, `"forgot file"`); makes `git bisect` and `git revert` simple because every commit represents a complete, functional feature.
- **Drawbacks**: Destroys fine-grained development history; makes backporting cherry-picks difficult; the feature branch commits are abandoned, meaning subsequent merges from that branch will cause conflicts.

---

### Q05: Explain the difference between `git merge --no-ff` and a standard merge.
**Answer:**  
A standard `git merge` will perform a fast-forward if possible (moving the target pointer forward without creating a commit).
Passing `--no-ff` (no fast-forward) forces Git to construct a dedicated merge commit with two parent pointers even if a fast-forward was technically possible.
**Why Use It?**: It creates an explicit visual envelope in the Git log marking the group of commits that constituted that specific feature, preserving the metadata indicating *when* and *how* the feature was merged.

---

### Q06: How does the new default `ort` merge strategy differ from the legacy `recursive` strategy?
**Answer:**  
Introduced in Git 2.33, `ort` stands for **"Ostensibly Recursive's Twin"**. It completely rewrote Git's merge algorithm from the ground up:
- **Performance**: Up to 100x faster on large repositories and complex merges.
- **Rename Detection**: Detects file renames globally across directories and handles rename/rename conflicts cleanly.
- **Conflict Handling**: Memorizes rename resolutions and avoids re-traversing directory trees multiple times during criss-cross merges.

---

### Q07: Explain how `git cherry-pick` operates under the hood.
**Answer:**  
`git cherry-pick <commit>` applies the changes introduced by a specific existing commit onto your current `HEAD` as a new commit.
Under the hood, Git executes a 3-way merge:
1. **Base**: The parent of the cherry-picked commit.
2. **Theirs**: The cherry-picked commit itself.
3. **Ours**: The current `HEAD` of your working tree.
Git calculates the diff between the commit and its parent and applies that diff patch onto `HEAD`, creating a new commit with a new SHA and new timestamp, but retaining the original author and commit message.

---

### Q08: What is `git rebase --onto`, and provide a production scenario where it is indispensable.
**Answer:**  
Syntax: `git rebase --onto <new-base> <old-base> <branch>`
It extracts commits from `<old-base>` to `<branch>` and grafts them directly onto `<new-base>`.

**Production Scenario (Branch Chaining)**:  
You created `feature-auth` from `main`. Then, before `feature-auth` merged, you branched `feature-billing` off `feature-auth`.
Now, `feature-auth` is rejected or rewritten, but `feature-billing` is ready to merge into `main`.
To move `feature-billing` onto `main` while stripping out `feature-auth`:
```bash
git rebase --onto main feature-auth feature-billing
```
This replays only the unique commits of `feature-billing` directly onto `main`.

---

### Q09: How does Git compute the "common ancestor" during a 3-way merge?
**Answer:**  
Git analyzes the Directed Acyclic Graph (DAG) using the Lowest Common Ancestor (LCA) algorithm to find the most recent commit shared by both branch tips (`git merge-base branchA branchB`). The snapshot at this commit serves as the baseline to determine which lines were modified by branch A vs branch B.

---

### Q10: What is a "criss-cross merge", and why does it cause problems for older merge strategies?
**Answer:**  
A criss-cross merge occurs when two branches have merged with each other multiple times, creating a situation where there is **more than one common ancestor** in the DAG.
Older merge strategies struggled because selecting either ancestor resulted in spurious conflicts. The `recursive` and `ort` strategies solve this by constructing a **virtual common ancestor commit** in memory that merges the multiple ancestors first, and then using that virtual commit as the baseline for the final 3-way merge.

---

### Q11: Explain how `git rerere` works and how it accelerates long-lived branch rebasing.
**Answer:**  
`rerere` stands for **Reuse Recorded Resolution**. When enabled (`git config rerere.enabled true`), Git takes a cryptographic preimage hash of any conflict markers when a merge halts. Once the developer resolves the conflict and commits, Git records the postimage resolution in `.git/rr-cache/`.
If the developer later encounters the exact same conflict (e.g., during a rebase across 10 commits or cherry-picking to an LTS branch), `rerere` automatically identifies the matching preimage and applies the recorded resolution instantly without manual developer intervention.

---

### Q12: Why does `git pull` often create unintended merge commits, and how do you configure Git to prevent it?
**Answer:**  
By default in older Git versions, if your local branch had 1 commit and the remote branch had 1 commit, `git pull` ran `git merge`, creating an unsightly "Merge branch 'main' of github.com..." merge commit bubble.
To enforce clean pulling, configure:
```bash
# Option A: Always rebase local commits on top of remote
git config --global pull.rebase true

# Option B: Fast-forward only; fail if branches diverged
git config --global pull.ff only
```

---

### Q13: What is the difference between `git cherry-pick A..B` and `git cherry-pick A...B`?
**Answer:**  
- `git cherry-pick A..B`: Applies all commits reachable from `B` that are NOT reachable from `A` (exclusive of `A`, inclusive of `B`). Replays commits in chronological order from `A+1` up to `B`.
- `git cherry-pick A...B`: Symmetric difference. Not valid syntax for standard cherry-pick; `cherry-pick` expects a linear range. In `git log`, `A...B` shows commits reachable from either `A` or `B` but not both.

---

### Q14: What happens to commit metadata (author, committer, timestamps) during a rebase?
**Answer:**  
- **Author & Author Date**: Preserved intact (the original author and the timestamp when the code was originally written remain unchanged).
- **Committer & Commit Date**: Updated (the person performing the rebase becomes the committer, and the commit timestamp is updated to the current time).
- **Commit SHA**: Always recalculated and changed.

---

### Q15: How do you abort a merge or rebase that has encountered overwhelming conflicts?
**Answer:**  
- Merge: `git merge --abort`  
- Rebase: `git rebase --abort`  
- Cherry-Pick: `git cherry-pick --abort`  
These commands restore the working directory, index, and branch pointers back to the exact state prior to initiating the operation.

---

### Q16: What is a fast-forward-only merge policy, and why do CI systems like GitHub enforce it?
**Answer:**  
A fast-forward-only policy mandates that pull requests can only merge if the target branch has not moved ahead of the PR branch. If another developer merges ahead of you, GitHub disables the merge button until you rebase your PR on latest `main`.
**Why Enforce It?**: It guarantees that the test suite executed on the PR branch was tested against the exact state of `main`, eliminating hidden semantic regression bugs that occur when two non-conflicting PRs merge without testing their combination.

---

### Q17: What does the `--autostash` flag do during a `git rebase` or `git pull`?
**Answer:**  
If you have dirty unstaged changes in your working tree, Git normally refuses to start a rebase. With `git rebase --autostash`, Git automatically stashes your local changes, executes the rebase operation, and pops the stash back onto your working tree once the rebase completes successfully.
Configure globally: `git config --global rebase.autoStash true`.

---

### Q18: How do you identify which branch a specific commit originated from in a complex DAG?
**Answer:**  
Commits in Git do not retain branch labels because branches are transient pointers. However, you can identify which branches currently contain the commit:
```bash
git branch --contains <commit-sha>      # Local branches
git branch -r --contains <commit-sha>   # Remote branches
```
If merge commits were used, `git log --graph --oneline` visually displays the merge topology where the commit entered trunk.

---

### Q19: Explain the difference between `ours` and `theirs` merge strategies vs merge options (`-X ours`).
**Answer:**  
- **Merge Strategy (`-s ours`)**: Completely ignores all changes introduced by the incoming branch. It synthesizes a merge commit where the tree matches `HEAD` 100%, without even inspecting diffs.
- **Merge Option / Recursive Flag (`-X ours`)**: Performs a normal 3-way merge, but if and only if a line conflict occurs, it automatically favors the version from `HEAD` without stopping for manual conflict resolution.

---

### Q20: Why can repeatedly cherry-picking commits between release branches cause future merge conflicts?
**Answer:**  
Cherry-picking duplicates code changes under completely different commit SHAs. Git's 3-way merge algorithm tracks history via commit lineage, not content diffs. If you cherry-pick commit `C` from `main` to `v1-release`, and later attempt to merge `v1-release` into `main`, Git will see two different commits touching the exact same lines without realizing one was derived from the other, frequently triggering false merge conflicts.
**Best Practice**: Avoid cyclical merges between branches that exchange cherry-picks.

---

[<- Back to Interview Overview](./README.md) | [Next Note: 03-internals-qa.md ->](./03-internals-qa.md)
