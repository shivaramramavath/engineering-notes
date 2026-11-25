f# Branching: `branch`, `switch`/`checkout` & Strategies

Branches let you work on something — a feature, a fix, an experiment — without touching the main line of history until you're ready. Remember from `01_git_fundamentals/05_git-objects.md`: a branch is just a small file holding a commit hash, which is why creating one is instant and cheap.

```
git branch          →  create, list, or delete branches
git switch/checkout →  move between branches
```

---

## `git branch` — create, list, delete

### List branches

```bash
git branch
```

```
  feature/login
* main
```

The `*` marks your current branch.

### Create a branch

```bash
git branch feature/signup
```

This creates the branch but **doesn't move you onto it** — you're still on whatever branch you were on. (See `switch -c` below to create and move in one step.)

### Delete a branch

```bash
git branch -d feature/login       # safe delete — refuses if unmerged
git branch -D feature/login       # force delete, even if unmerged
```

### Rename a branch

```bash
git branch -m old-name new-name
```

### List remote-tracking branches too

```bash
git branch -a
```

---

## `git switch` (modern) & `git checkout` (older, broader)

Both move you between branches. `switch` was introduced to split out `checkout`'s branch-switching role from its other, unrelated jobs (like restoring files) — use `switch` for branches when you can; reach for `checkout` mainly in older codebases/tutorials or when you specifically need one of its extra abilities.

### Switch to an existing branch

```bash
git switch main
# older equivalent:
git checkout main
```

### Create and switch in one step

```bash
git switch -c feature/signup
# older equivalent:
git checkout -b feature/signup
```

### Switch back to the previous branch

```bash
git switch -
```

### `checkout`'s extra (non-branch) ability

```bash
git checkout -- app.js
```

Restores a file to its last-committed state — this is the older syntax for what `git restore app.js` does now (see `04_undoing-changes.md`). `switch` deliberately does **not** do this, which is exactly why it's less error-prone: it only ever changes branches.

---

## A typical feature-branch workflow

```bash
git switch main
git pull                          # make sure main is up to date
git switch -c feature/login       # branch off for the new work
# ... make changes, commit as you go ...
git push -u origin feature/login  # push and set upstream (see 09_tracking-upstream.md)
# open a pull request, get it reviewed, merge on GitHub/GitLab
git switch main
git pull                          # get the merged change back
git branch -d feature/login       # clean up the now-merged branch
```

---

## Branch naming conventions

Common patterns teams use:

```
feature/login-page
fix/redirect-bug
chore/update-deps
hotfix/critical-security-patch
```

A `type/short-description` prefix makes it easy to scan `git branch` output and understand intent at a glance.

---

## Common branching strategies

### Trunk-based development

Everyone commits small, frequent changes directly to `main` (or very short-lived branches merged within a day). Favors continuous integration and fast feedback; requires strong test coverage and feature flags for incomplete work.

### GitHub Flow

Simple and widely used for continuous deployment:

```
main (always deployable)
  └── feature/xyz → PR → review → merge → deploy
```

One long-lived branch (`main`), short-lived feature branches, merged via pull request.

### Git Flow

A heavier, structured model with dedicated long-lived branches:

```
main        → production-ready releases only
develop     → integration branch for in-progress work
feature/*   → branches off develop
release/*   → stabilizing a release before merging to main
hotfix/*    → urgent fixes branched directly from main
```

Suits projects with scheduled releases (e.g. versioned software), where multiple things are "in flight" and not everything ships immediately.

### Choosing one

| If your project...                                          | Consider                |
| ----------------------------------------------------------- | ----------------------- |
| Deploys continuously, every merge ships                     | GitHub Flow             |
| Has scheduled releases / needs to support multiple versions | Git Flow                |
| Has strong test automation and wants maximum simplicity     | Trunk-based development |

There's no universally "correct" strategy — pick the simplest one your team's release process actually needs, and don't adopt Git Flow's full ceremony if you deploy several times a day.

## Quick summary

- `git branch` creates/lists/deletes branches; it doesn't move you between them
- `git switch` is the modern way to change branches; `git checkout` is the older, broader command
- `git switch -c <name>` / `git checkout -b <name>` create and switch in one step
- Branches are cheap — created and deleted constantly as part of normal workflow
- Pick a branching strategy that matches your release cadence, not the most elaborate one available

## Next

**`07_merging.md`** covers bringing a branch's changes back together, including what to do when a merge conflict shows up.
