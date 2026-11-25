# Quick Reference Cheatsheet: Git Essential Commands

[<- Back to Cheatsheets Overview](./README.md) | [Next Cheatsheet: Undo & Recovery Matrix ->](./02-undo-and-recovery-matrix.md)

---

## 1. Setup & Configuration

```bash
# Global user identity
git config --global user.name "Your Name"
git config --global user.email "you@example.com"

# Default branch & editor
git config --global init.defaultBranch main
git config --global core.editor "vim"

# Line endings
git config --global core.autocrlf input     # macOS / Linux
git config --global core.autocrlf true      # Windows

# Conflict style & pull behavior
git config --global merge.conflictStyle diff3
git config --global pull.rebase true

# List all effective configurations with origins
git config --list --show-origin
```

---

## 2. Creating & Initializing Repositories

```bash
# Initialize a new local repository
git init

# Initialize with custom initial branch
git init -b main

# Clone remote repository
git clone https://github.com/org/repo.git

# Clone only the latest commit (shallow clone)
git clone --depth 1 https://github.com/org/repo.git

# Partial clone (blobs on-demand for large repos)
git clone --filter=blob:none https://github.com/org/repo.git
```

---

## 3. Staging & Committing

```bash
# Check status in short format
git status -s

# Stage all changes (new, modified, deleted)
git add .
git add -A

# Interactively stage hunks
git add -p <file>

# Commit with inline message
git commit -m "feat(auth): add oauth2 token exchange"

# Amend the most recent commit (without changing message)
git commit --amend --no-edit

# Amend message and files of latest commit
git commit --amend -m "feat(auth): add oauth2 exchange with token refresh"

# Commit bypassing client pre-commit hooks (Emergency only)
git commit -n -m "hotfix: bypass local hook"
```

---

## 4. Inspecting & Diffing

```bash
# View unstaged changes (working tree vs index)
git diff

# View staged changes (index vs HEAD)
git diff --staged

# View combined changes (working tree vs HEAD)
git diff HEAD

# Visual compact commit log
git log --oneline --graph --decorate -n 20

# Search commit history for code addition/deletion (Pickaxe)
git log -S "StripeAPIKey" -p

# Blame line-by-line attribution ignoring whitespace
git blame -w -L 10,25 path/to/file.py
```

---

## 5. Branching & Merging

```bash
# List local and remote branches
git branch -a

# Create and switch to new branch
git switch -c feature-branch
git checkout -b feature-branch

# Switch back to previous branch
git switch -
git checkout -

# Delete merged branch
git branch -d feature-branch

# Force delete unmerged branch
git branch -D feature-branch

# Merge branch into current branch
git merge feature-branch

# Force non-fast-forward merge commit
git merge --no-ff feature-branch

# Abort merge with conflicts
git merge --abort
```

---

## 6. Rebasing & Cherry-Picking

```bash
# Rebase current branch on top of main
git rebase main

# Interactive rebase of last 5 commits
git rebase -i HEAD~5

# Move feature branch from old-base to new-base
git rebase --onto new-base old-base feature-branch

# Abort rebase
git rebase --abort

# Continue rebase after resolving conflict
git rebase --continue

# Apply a single commit to current branch
git cherry-pick <commit-sha>

# Apply commit without automatically committing (staged only)
git cherry-pick -n <commit-sha>

# Abort cherry-pick
git cherry-pick --abort
```

---

## 7. Remote Synchronization

```bash
# List remotes with URLs
git remote -v

# Add new remote
git remote add upstream https://github.com/upstream-org/repo.git

# Fetch all branches and tags from all remotes
git fetch --all --prune

# Push branch and set upstream tracking
git push -u origin feature-branch

# Safe force-push (never overwrites teammate commits)
git push --force-with-lease

# Delete remote branch
git push origin --delete feature-branch
```

---

## 8. Stashing Work

```bash
# Stash tracked working tree changes
git stash

# Stash including untracked files with custom message
git stash push -u -m "WIP: billing checkout refactor"

# List stashes
git stash list

# Reapply latest stash and remove from stack
git stash pop

# Reapply latest stash and keep on stack
git stash apply

# View diff of stash@{0}
git stash show -p stash@{0}

# Clear all stashes
git stash clear
```

---

## 9. Tags & Releases

```bash
# Create lightweight tag
git tag v1.0.0

# Create annotated, message-bearing tag
git tag -a v1.0.0 -m "Release version 1.0.0"

# Cryptographically sign annotated tag
git tag -s v1.0.0 -m "Release version 1.0.0 - verified"

# Push single tag to remote
git push origin v1.0.0

# Push all local tags
git push origin --tags

# Delete local and remote tag
git tag -d v1.0.0
git push origin --delete v1.0.0
```

---

[<- Back to Cheatsheets Overview](./README.md) | [Next Cheatsheet: Undo & Recovery Matrix ->](./02-undo-and-recovery-matrix.md)
