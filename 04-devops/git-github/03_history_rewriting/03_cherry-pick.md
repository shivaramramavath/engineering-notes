# `git cherry-pick`

Copies a single, specific commit from one branch onto another — without merging or rebasing everything else on that branch.

```bash
git cherry-pick <commit-hash>
```

## When to use it

The situation cherry-pick is built for: a commit exists somewhere you need it, but you don't want (or aren't allowed) to bring in everything else that branch contains.

- A critical bug fix was committed on a feature branch that isn't ready to merge yet, but `main` needs the fix now
- You accidentally committed to the wrong branch and want to move that one commit to the right one
- Backporting a fix to an older release branch, without bringing that release up to date with everything else on `main`

---

## Basic usage

```bash
git switch main
git cherry-pick a1b2c3d
```

```
[main f9e8d7c] Fix critical login bug
 1 file changed, 3 insertions(+), 1 deletion(-)
```

Git creates a **new commit** on your current branch, with the same changes and message as the original — but a different hash, since it now has a different parent.

### Finding the commit hash to pick

```bash
git log --oneline feature/login
```

```
a1b2c3d Fix critical login bug
d4e5f6a Add login form
```

---

## Cherry-picking multiple commits

```bash
git cherry-pick a1b2c3d d4e5f6a
```

Applies them in the order listed, each as its own new commit.

### A range of commits

```bash
git cherry-pick a1b2c3d^..g7h8i9j
```

Picks every commit from `a1b2c3d` through `g7h8i9j` inclusive. (The `^` after the first hash includes that commit itself — without it, the range would start _after_ `a1b2c3d`.)

---

## Handling conflicts

Cherry-pick conflicts work the same way merge/rebase conflicts do — resolved one commit at a time:

```bash
git cherry-pick a1b2c3d
```

```
CONFLICT (content): Merge conflict in app.js
```

```bash
# resolve the conflict in app.js
git add app.js
git cherry-pick --continue
```

To skip a problematic commit:

```bash
git cherry-pick --skip
```

To abandon the whole operation and return to how things were:

```bash
git cherry-pick --abort
```

---

## Useful options

### Stage the change without committing

```bash
git cherry-pick -n a1b2c3d
```

(`-n` / `--no-commit`) — applies the changes and stages them, but lets you review, combine with other changes, or edit before committing yourself.

### Cherry-picking a merge commit

```bash
git cherry-pick -m 1 <merge-commit-hash>
```

Merge commits have two parents, so `-m` specifies which parent's side of the merge to use as the basis for the pick — same idea as `git revert -m`.

---

## Cherry-pick vs merge vs rebase

|                   | Brings in                                    |
| ----------------- | -------------------------------------------- |
| `git merge`       | An entire branch's history                   |
| `git rebase`      | Your entire branch, replayed on a new base   |
| `git cherry-pick` | One (or a few) specific, hand-picked commits |

Cherry-pick is the narrowest of the three — reach for it when you need _some_, not _all_, of another branch's changes.

## A caution

Because cherry-picking creates a new commit with a new hash, if that same original commit is later merged normally, Git may not recognize them as "the same change" and you can end up with duplicate-looking commits in history. This is usually harmless (same content, applied twice is a no-op) but can look confusing in `git log`. It's most often used deliberately for exactly this trade-off — e.g. hotfixes that need to exist on both a release branch and `main`.

## Quick summary

- `git cherry-pick <hash>` copies one commit's changes onto your current branch as a new commit
- Use it to grab a specific fix without merging/rebasing an entire branch
- Conflicts resolve the same way as merge/rebase: `add` + `cherry-pick --continue`
- `-n` stages without committing; `-m` picks which parent to use for a merge commit

## Next

**`04_reflog.md`** covers Git's safety net for when a rebase, reset, or cherry-pick goes wrong.
