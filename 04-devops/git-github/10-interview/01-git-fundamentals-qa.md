<!--
================================================================================
Metadata:
  Title: Technical Interview Preparation - Git Fundamentals & Core Concepts Q&A
  Track: Git & GitHub / DevOps
  Difficulty: Junior to Senior
  Estimated Time: 50 minutes
  Key Terminology: Distributed VCS, Staging Area, Working Tree, Index, Commits, Remotes, Refspecs, Fast-Forward
================================================================================
-->

# Git Fundamentals & Core Concepts: 20 In-Depth Interview Questions & Answers

[<- Back to Interview Overview](./README.md) | [Next Note: 02-branching-merging-rebasing-qa.md ->](./02-branching-merging-rebasing-qa.md)

---

## Overview

This guide provides 20 rigorous, production-focused interview questions covering Git fundamentals, core architecture, the three-tree state machine, staging mechanics, and remote tracking concepts. Questions range from operational command mechanics to senior-level architectural design.

---

## Questions Index

1. [Q01: What makes Git a "distributed" version control system, and how does it contrast with centralized systems like SVN?](#q01-what-makes-git-a-distributed-version-control-system-and-how-does-it-contrast-with-centralized-systems-like-svn)
2. [Q02: Explain the three states of Git and the "Three Trees" mental model.](#q02-explain-the-three-states-of-git-and-the-three-trees-mental-model)
3. [Q03: Why does Git have a Staging Area (the Index) instead of committing directly from the working directory?](#q03-why-does-git-have-a-staging-area-the-index-instead-of-committing-directly-from-the-working-directory)
4. [Q04: What is the physical structure of a `.git` directory?](#q04-what-is-the-physical-structure-of-a-git-directory)
5. [Q05: What is the difference between `git fetch` and `git pull`?](#q05-what-is-the-difference-between-git-fetch-and-git-pull)
6. [Q06: How does `git clone` differ from `git init` paired with `git remote add`?](#q06-how-does-git-clone-differ-from-git-init-paired-with-git-remote-add)
7. [Q07: What is a Git refspec, and how does `+refs/heads/*:refs/remotes/origin/*` work?](#q07-what-is-a-git-refspec-and-how-does-refsheadrefsremotesorigin-work)
8. [Q08: What is `HEAD` in Git, and what does it mean to be in a "detached HEAD" state?](#q08-what-is-head-in-git-and-what-does-it-mean-to-be-in-a-detached-head-state)
9. [Q09: Explain the difference between `git checkout`, `git switch`, and `git restore`.](#q09-explain-the-difference-between-git-checkout-git-switch-and-git-restore)
10. [Q10: What does `git status -s` output, and what do the two columns represent?](#q10-what-does-git-status--s-output-and-what-do-the-two-columns-represent)
11. [Q11: How does `.gitignore` differ from `.git/info/exclude`?](#q11-how-does-gitignore-differ-from-gitinfoexclude)
12. [Q12: If a file is already tracked by Git, why does adding it to `.gitignore` have no effect?](#q12-if-a-file-is-already-tracked-by-git-why-does-adding-it-to-gitignore-have-no-effect)
13. [Q13: What does `git diff` show vs `git diff --staged` vs `git diff HEAD`?](#q13-what-does-git-diff-show-vs-git-diff---staged-vs-git-diff-head)
14. [Q14: Explain the difference between an annotated tag and a lightweight tag.](#q14-explain-the-difference-between-an-annotated-tag-and-a-lightweight-tag)
15. [Q15: What is the purpose of `git stash`, and how does `git stash pop` differ from `git stash apply`?](#q15-what-is-the-purpose-of-git-stash-and-how-does-git-stash-pop-differ-from-git-stash-apply)
16. [Q16: How does `git clean -fd` work, and how can you dry-run it safely?](#q16-how-does-git-clean--fd-work-and-how-can-you-dry-run-it-safely)
17. [Q17: What is a fast-forward merge, and when is it impossible to perform?](#q17-what-is-a-fast-forward-merge-and-when-is-it-impossible-to-perform)
18. [Q18: What is upstream tracking (`--set-upstream` or `-u`), and what does it configure in `.git/config`?](#q18-what-is-upstream-tracking---set-upstream-or--u-and-what-does-it-configure-in-gitconfig)
19. [Q19: Explain the difference between HTTPS and SSH transport protocols in Git.](#q19-explain-the-difference-between-https-and-ssh-transport-protocols-in-git)
20. [Q20: How does Git handle line endings across operating systems (`core.autocrlf` vs `.gitattributes`)?](#q20-how-does-git-handle-line-endings-across-operating-systems-coreautocrlf-vs-gitattributes)

---

## Questions & Answers

### Q01: What makes Git a "distributed" version control system, and how does it contrast with centralized systems like SVN?
**Answer:**  
In a centralized system (CVCS like Subversion or Perforce), there is a single central server containing the repository database. Clients check out only a working snapshot of a specific revision. Operations like viewing history (`svn log`), diffing between versions, or committing require an active network connection to the central server. If the server goes down, development stalls.

In a distributed system (DVCS like Git), every cloned repository is a complete, full-fidelity mirror of the repository database, including all files, commit history, branches, and tags. Developers can commit, branch, inspect logs, and diff entirely offline. Remote servers (like GitHub) simply act as collaboration hubs for exchanging commits via SSH or HTTPS.

---

### Q02: Explain the three states of Git and the "Three Trees" mental model.
**Answer:**  
Git manages files across three distinct logical trees:
1. **Working Tree**: The actual physical directory on your local filesystem containing files you can edit.
2. **Index (Staging Area)**: A binary cache file (`.git/index`) containing a list of pathnames, permissions, and SHA-1/SHA-256 hashes representing the exact snapshot destined for the next commit.
3. **Commit History (`HEAD`)**: The immutable Directed Acyclic Graph (DAG) of commit objects stored permanently in the repository.

```
+--------------------+        git add         +--------------------+        git commit       +--------------------+
|    Working Tree    | ---------------------> |   Index (Staging)  | ----------------------> |    HEAD (Commit)   |
| (Local Filesystem) | <--------------------- |    (.git/index)    | <---------------------- |  (Immutable DAG)   |
+--------------------+      git restore       +--------------------+    git restore --staged +--------------------+
```

---

### Q03: Why does Git have a Staging Area (the Index) instead of committing directly from the working directory?
**Answer:**  
The Index decouples *editing code* from *curating commit history*. In production development, an engineer might touch 10 files across 3 distinct tasks (e.g., fixing a bug, refactoring a helper, and adding a test). The Index allows developers to craft atomic, logical commits by selectively staging specific files (`git add <file>`) or even specific line hunks within a file (`git add -p`). This ensures commit logs remain clean and reviewable.

---

### Q04: What is the physical structure of a `.git` directory?
**Answer:**  
- `HEAD`: Text file containing a symbolic ref to the currently checked-out branch (e.g., `ref: refs/heads/main`).
- `config`: Repository-specific configuration settings (remotes, user details, branch tracking).
- `index`: Binary cache representing the staging area.
- `objects/`: Content-addressable object store holding compressed blobs, trees, commits, and tags (`objects/pack/` stores packfiles).
- `refs/`: Pointers to commit objects: `refs/heads/` (local branches), `refs/remotes/` (remote tracking branches), `refs/tags/` (tags).
- `logs/`: Append-only reflogs tracking historical movements of `HEAD` and branch refs.
- `hooks/`: Client-side executable scripts triggered on git events (`pre-commit`, `commit-msg`).

---

### Q05: What is the difference between `git fetch` and `git pull`?
**Answer:**  
- `git fetch`: Downloads all new commits, refs, and objects from the remote repository into your local `.git` database and updates remote-tracking branches (e.g., `origin/main`). It **never modifies** your local working directory or current branch.
- `git pull`: A compound command that runs `git fetch` followed immediately by `git merge FETCH_HEAD` (or `git rebase` if configured with `pull.rebase=true`). It updates your local working tree.
**Best Practice**: In professional environments, prefer `git fetch` to review incoming changes via `git log HEAD..origin/main` before integrating them.

---

### Q06: How does `git clone` differ from `git init` paired with `git remote add`?
**Answer:**  
`git clone <url>` performs four automated operations in one command:
1. Creates the target directory and executes `git init`.
2. Adds a remote named `origin` pointing to the provided URL.
3. Fetches all objects, branches, and tags from the remote (`git fetch`).
4. Checks out the remote repository's default branch into your working tree.
`git init` followed by `git remote add origin <url>` merely configures the remote pointer; it does not download objects or check out files until you explicitly execute `git fetch` and `git checkout`.

---

### Q07: What is a Git refspec, and how does `+refs/heads/*:refs/remotes/origin/*` work?
**Answer:**  
A refspec is a mapping pattern that defines how remote references map to local references:
`[+]<src>:<dst>`
- `+`: Allows non-fast-forward updates (force updates).
- `refs/heads/*`: The source ref on the remote (all local branches on the remote server).
- `refs/remotes/origin/*`: The destination ref on your local machine (your remote tracking branches).
This pattern instructs Git: "Take all branches from the remote server's `refs/heads/` and store them locally under `refs/remotes/origin/`."

---

### Q08: What is `HEAD` in Git, and what does it mean to be in a "detached HEAD" state?
**Answer:**  
`HEAD` is a reference pointer indicating the current commit and context your working directory is based on. Normally, `HEAD` is a *symbolic ref* pointing to a branch name (e.g., `ref: refs/heads/main`).
A **detached HEAD** occurs when you check out a specific commit hash, tag, or remote branch directly (`git checkout 4a3b1c`) instead of a local branch. `HEAD` points directly to the commit SHA rather than a branch ref. Any new commits made in this state are not attached to any branch; if you switch away, they become orphaned ("dangling") and will eventually be garbage collected unless tagged or assigned to a branch.

---

### Q09: Explain the difference between `git checkout`, `git switch`, and `git restore`.
**Answer:**  
Historically, `git checkout` was overloaded: it was used both for branch switching and for discarding file changes, leading to frequent accidental data loss. In Git 2.23, Git split these responsibilities into two distinct, intention-revealing commands:
- `git switch`: Exclusively handles branches (e.g., `git switch main`, `git switch -c feature`).
- `git restore`: Exclusively handles undoing and reverting file states:
  - `git restore <file>`: Discards uncommitted changes in the working tree.
  - `git restore --staged <file>`: Unstages changes from the Index back to the working tree.
- `git checkout`: Retained for backward compatibility.

---

### Q10: What does `git status -s` output, and what do the two columns represent?
**Answer:**  
`git status -s` produces short-format status output with two character columns:
- **First Column (X)**: State of the **Staging Area (Index)** relative to `HEAD`.
- **Second Column (Y)**: State of the **Working Tree** relative to the Index.

Examples:
- `M ` (M in col 1, space in col 2): Staged modification.
- ` M` (space in col 1, M in col 2): Unstaged modification in working tree.
- `MM`: File was modified, staged, and then modified *again* in the working tree.
- `A `: Newly added file, staged.
- `??`: Untracked file.

---

### Q11: How does `.gitignore` differ from `.git/info/exclude`?
**Answer:**  
- `.gitignore`: Tracked in version control and committed to the repository. Its ignore patterns apply to every developer and CI runner who clones the repository.
- `.git/info/exclude`: Local to your specific clone only. It is not tracked in Git. It is used for personal tooling artifacts (e.g., custom local scratchpads or IDE configs) that you do not want to impose on the rest of the team.

---

### Q12: If a file is already tracked by Git, why does adding it to `.gitignore` have no effect?
**Answer:**  
`.gitignore` only tells Git to ignore **untracked** files when discovering new files during `git add .` or `git status`. If a file is already stored in the Git Index, Git continues tracking changes to it.
To ignore an already-tracked file, you must untrack it first:
```bash
git rm --cached <file>
git commit -m "chore: untrack file and delegate to .gitignore"
```

---

### Q13: What does `git diff` show vs `git diff --staged` vs `git diff HEAD`?
**Answer:**  
- `git diff`: Shows differences between the **Working Tree** and the **Index (Staging Area)** (unstaged changes).
- `git diff --staged` (or `--cached`): Shows differences between the **Index** and the latest **Commit (`HEAD`)** (staged changes ready to commit).
- `git diff HEAD`: Shows all changes between the **Working Tree** and the latest **Commit (`HEAD`)**, combining both staged and unstaged edits.

---

### Q14: Explain the difference between an annotated tag and a lightweight tag.
**Answer:**  
- **Lightweight Tag**: Simply a named pointer (a text file in `.git/refs/tags/<name>`) containing a commit SHA. It carries no author metadata, message, or cryptographic signature. Equivalent to a branch that never moves.
- **Annotated Tag**: A full, first-class Git object stored in `.git/objects`. It contains a tagger name, email, date, an explicit tag message, and can be cryptographically signed with GPG or SSH (`git tag -s`).
**Best Practice**: Always use annotated tags (`git tag -a v1.0.0 -m "Release v1.0.0"`) for official software releases.

---

### Q15: What is the purpose of `git stash`, and how does `git stash pop` differ from `git stash apply`?
**Answer:**  
`git stash` takes uncommitted changes (both staged and unstaged) in your working tree and saves them on an internal stack of commit-like objects (`refs/stash`), reverting your working tree to clean `HEAD` state.
- `git stash pop`: Reapplies the most recent stash onto the current working tree and **deletes** it from the stash stack.
- `git stash apply`: Reapplies the stash onto the working tree but **retains** it on the stash stack, allowing you to re-apply the same diff across multiple branches.

---

### Q16: How does `git clean -fd` work, and how can you dry-run it safely?
**Answer:**  
`git clean` forcefully removes untracked files from the working directory:
- `-f` (force): Mandated by Git safety guards.
- `-d`: Removes untracked directories as well as files.
Because `git clean` deletes files permanently outside Git's object store (they cannot be recovered via reflog), always execute a dry run first:
```bash
git clean -nd   # -n / --dry-run reports what would be removed without deleting
```

---

### Q17: What is a fast-forward merge, and when is it impossible to perform?
**Answer:**  
A **fast-forward merge** occurs when the target branch has not diverged from the branch being merged. The tip of the target branch is a direct ancestor of the incoming branch. Git does not need to compute a 3-way merge or create a merge commit; it simply moves the branch pointer forward to point to the incoming commit.
A fast-forward is **impossible** when divergent commits exist on both branches (the target branch has advanced with new commits since the feature branch split). Git must execute a true 3-way merge (`ort` or `recursive` strategy), creating a merge commit with two parents.

---

### Q18: What is upstream tracking (`--set-upstream` or `-u`), and what does it configure in `.git/config`?
**Answer:**  
When you push with `git push -u origin feature`, Git establishes a tracking relationship between your local branch and a remote branch. It appends configuration to `.git/config`:
```ini
[branch "feature"]
    remote = origin
    merge = refs/heads/feature
```
This enables shorthand commands: `git push` and `git pull` automatically know which remote and branch to sync with, and `git status` reports how many commits your branch is ahead/behind origin.

---

### Q19: Explain the difference between HTTPS and SSH transport protocols in Git.
**Answer:**  
- **HTTPS (`https://github.com/org/repo.git`)**: Communicates over port 443. Works universally through corporate firewalls and proxies. Requires authentication via Personal Access Tokens (PATs) or credential helpers (passwords are deprecated).
- **SSH (`git@github.com:org/repo.git`)**: Communicates over port 22 (or 443 via SSH-over-HTTPS). Uses asymmetric public/private key pairs (`ed25519` / `rsa`). Highly preferred for automated CI/CD and developer workstations because keys are password-protected locally and do not require periodic token rotations.

---

### Q20: How does Git handle line endings across operating systems (`core.autocrlf` vs `.gitattributes`)?
**Answer:**  
Windows uses CRLF (`\r\n`) while Linux/macOS use LF (`\n`). If unmanaged, cross-platform teams suffer from massive "ghost diffs" where entire files appear modified due to line ending conversions.
- `core.autocrlf`: A client-side workstation setting (`true` on Windows, `input` on macOS/Linux). It relies on individual developer compliance.
- `.gitattributes`: The enterprise standard. A version-controlled repository configuration that enforces uniform line ending normalization for everyone:
```ini
* text=auto eol=lf
*.bat text eol=crlf
*.png binary
```

---

[<- Back to Interview Overview](./README.md) | [Next Note: 02-branching-merging-rebasing-qa.md ->](./02-branching-merging-rebasing-qa.md)
