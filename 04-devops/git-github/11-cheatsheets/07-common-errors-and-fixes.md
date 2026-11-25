# Quick Reference Cheatsheet: Top 20 Git & GitHub Errors & Fixes

[<- Back to Cheatsheets Overview](./README.md) | [Previous Cheatsheet: Git Config & Aliases <-](./06-git-config-and-aliases.md)

---

## 1. Error Reference Table

| # | Error Message / Symptom | Root Cause | Immediate Fix |
|:---|:---|:---|:---|
| 01 | `fatal: refusing to merge unrelated histories` | Two branches share no common ancestor commit | `git pull origin main --allow-unrelated-histories` |
| 02 | `error: failed to push some refs to... Updates were rejected` | Remote contains newer commits not present locally | `git fetch origin && git rebase origin/main && git push` |
| 03 | `fatal: Not a git repository (or any of the parent directories)` | Command executed outside a directory containing `.git` | `cd` into repo root or run `git init` |
| 04 | `error: Your local changes would be overwritten by checkout/merge` | Uncommitted modifications conflict with target branch files | `git stash` -> run command -> `git stash pop` |
| 05 | `fatal: reference is not a tree: <sha>` (Submodules) | Submodule pointer points to commit not pushed to remote | In submodule: `git push origin <branch>`, then in parent commit pointer |
| 06 | `Permission denied (publickey)` | SSH key missing, not added to ssh-agent, or not registered on GitHub | `ssh-add ~/.ssh/id_ed25519` and verify via `ssh -T git@github.com` |
| 07 | `fatal: Authentication failed for 'https://github.com/...'` | Using account password instead of PAT or token expired | Generate new Fine-Grained PAT on GitHub and use as password |
| 08 | `fatal: A branch named 'feature' already exists` | Attempting to create duplicate branch | Use existing: `git checkout feature` or force: `git checkout -B feature` |
| 09 | `You are in 'detached HEAD' state` | Checked out a commit hash or tag directly rather than branch | Create branch from current position: `git checkout -b <new-branch>` |
| 10 | `fatal: corrupt patch at line...` | Patch file was manually edited with bad whitespace/headers | Use `git apply --ignore-whitespace <patch>` |
| 11 | `GH013: Repository rule violations found (Secret Scanning)` | GitHub Push Protection blocked commit containing hardcoded secret | `git reset HEAD~1`, remove secret, re-commit without secret |
| 12 | `error: cannot lock ref 'refs/remotes/origin/feature': is at X but expected Y` | Local remote-tracking branch cache is stale / desynchronized | `git remote prune origin` or `git fetch -p` |
| 13 | `error: could not apply <sha>... (during cherry-pick/rebase)` | Merge conflict encountered during patch application | Resolve markers -> `git add <files>` -> `git rebase --continue` |
| 14 | `LF will be replaced by CRLF the next time Git touches it` | Line ending normalization difference across OS | Add `.gitattributes` with `* text=auto eol=lf` |
| 15 | `fatal: pathspec '<file>' did not match any files` | Typo in filename or file is ignored / untracked | Check exact path using `git status` or `ls` |
| 16 | `File exceeds GitHub's file size limit of 100.00 MB` | Pushed large binary without Git LFS | Remove from history with `git-filter-repo` and configure Git LFS |
| 17 | `Unverified commit badge on GitHub` | Commit email does not match GPG/SSH key email or key expired | Align `git config user.email` with verified GitHub account email |
| 18 | `error: RPC failed; curl 56 GnuTLS recv error (-9)` | Network timeout on massive clone over HTTP | Increase buffer: `git config --global http.postBuffer 524288000` |
| 19 | `fatal: You have not concluded your merge (MERGE_HEAD exists)` | Merge halted by conflict and neither completed nor aborted | Run `git merge --abort` or finish: `git add . && git commit` |
| 20 | `branch 'feature' is not fully merged (when running git branch -d)` | Branch contains commits not merged into current branch | Safe delete check: verify in git log, then force: `git branch -D feature` |

---

## 2. Emergency Copy-Paste Fixes

### Fix 1: Discard Everything and Reset to Remote Clean State
```bash
# WARNING: Destroys all local uncommitted and un-pushed changes
git fetch origin
git reset --hard origin/main
git clean -fd
```

### Fix 2: Unstick a Broken Rebase Loop
```bash
# Aborts rebase and restores original branch state completely
git rebase --abort
```

### Fix 3: Remove Accidental `.DS_Store` or `node_modules` from Git
```bash
git rm -r --cached node_modules/ .DS_Store
echo "node_modules/" >> .gitignore
echo ".DS_Store" >> .gitignore
git commit -m "chore: untrack node_modules and .DS_Store"
```

### Fix 4: Force Remote Branch to Match Your Local Branch Safely
```bash
# Use --force-with-lease to protect coworkers' commits
git push --force-with-lease origin feature-branch
```

---

[<- Back to Cheatsheets Overview](./README.md) | [Previous Cheatsheet: Git Config & Aliases <-](./06-git-config-and-aliases.md)
