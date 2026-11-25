# Undoing Changes: `restore`, `reset` & `revert`

Git has three different "undo" commands because there are three different things you might want to undo: an uncommitted edit, a staged change, or an already-made commit. Using the wrong one is the most common source of "I think I just lost my work" panic — this file is about picking the right tool.

```
git restore  →  undo changes in the working tree / staging area (uncommitted)
git reset    →  move the current branch pointer, optionally undoing commits
git revert   →  undo a commit by creating a new, opposite commit (safe for shared history)
```

---

## `git restore` — undo uncommitted changes

For changes you haven't committed yet.

### Discard changes in the working tree

```bash
git restore app.js
```

Reverts `app.js` back to how it looked in the last commit. **This permanently discards the edit** — there's no staging step to save it from.

### Unstage a file (keep the edit)

```bash
git restore --staged app.js
```

Moves `app.js` out of the staging area, back to "modified but not staged." The actual edit in the file is untouched — this only undoes `git add`.

### Restore from a specific commit

```bash
git restore --source=HEAD~1 app.js
```

Brings `app.js` back to how it looked one commit ago, without touching anything else.

**Use `restore` when:** you haven't committed yet and just want to undo an edit or an accidental `git add`.

---

## `git reset` — move the branch pointer

Moves your current branch (and `HEAD`) to point at a different commit, optionally changing the staging area and working tree too. Three modes, each undoing progressively more:

```bash
git reset --soft HEAD~1    # undo the commit, keep changes staged
git reset --mixed HEAD~1   # undo the commit AND unstage (this is the default)
git reset --hard HEAD~1    # undo the commit AND discard the changes entirely
```

| Mode                | Commit | Staging area        | Working tree                 |
| ------------------- | ------ | ------------------- | ---------------------------- |
| `--soft`            | Undone | Kept (still staged) | Unchanged                    |
| `--mixed` (default) | Undone | Cleared (unstaged)  | Unchanged                    |
| `--hard`            | Undone | Cleared             | **Reset — changes are gone** |

### Common uses

```bash
# "I committed too early, let me redo the commit"
git reset --soft HEAD~1
git commit -m "Better commit message"

# "Undo my last commit but let me re-review the changes"
git reset --mixed HEAD~1

# "Throw away my last commit completely, I don't want any of it"
git reset --hard HEAD~1
```

### ⚠️ `--hard` is destructive

`git reset --hard` discards working-tree changes with **no confirmation and no easy recovery** through normal commands (see `git reflog` in `05_history_rewriting/06_reflog.md` for a possible rescue). Double-check `git status` and `git diff` before running it.

### Resetting a specific file (not the whole commit)

```bash
git reset HEAD app.js
```

Equivalent to `git restore --staged app.js` — unstages one file without moving the branch pointer.

**Use `reset` when:** you want to move your branch backward — undoing commits, typically ones that haven't been pushed/shared yet.

---

## `git revert` — undo a commit safely

Instead of moving the branch pointer backward, `revert` creates a **new commit** that undoes the changes from an earlier one. History moves forward; nothing is deleted or rewritten.

```bash
git revert <commit-hash>
```

```
a1b2c3d Revert "Add experimental feature"    ← new commit
g7h8i9j Add experimental feature             ← original, still in history
```

### Why this matters for shared branches

If a commit has already been pushed and others may have pulled it, `reset` would rewrite history they already have — causing conflicts and confusion for anyone who's based work on it. `revert` avoids that entirely, since it only _adds_ a commit rather than removing one.

### Reverting without committing immediately

```bash
git revert --no-commit <commit-hash>
```

Applies the revert to the staging area/working tree so you can review or combine it with other changes before committing.

### Reverting a merge commit

```bash
git revert -m 1 <merge-commit-hash>
```

Merge commits have two parents, so you need `-m` to specify which parent's history to keep as the "mainline."

**Use `revert` when:** the commit you're undoing has already been pushed or shared with others.

---

## Choosing the right command

| Situation                                          | Command                       |
| -------------------------------------------------- | ----------------------------- |
| Discard an uncommitted edit                        | `git restore <file>`          |
| Unstage a file, keep the edit                      | `git restore --staged <file>` |
| Undo a local, unpushed commit but keep the changes | `git reset --soft HEAD~1`     |
| Undo a local, unpushed commit and the changes      | `git reset --hard HEAD~1`     |
| Undo a commit that's already pushed/shared         | `git revert <commit-hash>`    |

## The golden rule

> **Never `reset` a commit that others may have already pulled.** Use `revert` instead once something is shared — it's the safe, collaborative-friendly way to undo history.

## Quick summary

- `restore` — undoes uncommitted changes (working tree and/or staging area)
- `reset` — moves your branch backward through history; `--soft`/`--mixed`/`--hard` control how much else gets undone
- `revert` — undoes a commit by adding a new one; safe for shared/pushed history
- When in doubt about a destructive command, run `git status`/`git diff` first, and remember `git reflog` can often rescue a `reset --hard` mistake

## Next

**`05_clean-rm-mv.md`** covers removing untracked files and removing/renaming tracked ones.
