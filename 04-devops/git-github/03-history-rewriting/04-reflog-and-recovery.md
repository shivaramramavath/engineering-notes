# `git reflog`

Git's safety net. Every time `HEAD` moves — a commit, checkout, merge, rebase, reset, amend, cherry-pick — Git quietly records it in the **reflog**. This is what lets you recover from most "I think I just destroyed my work" moments.

```bash
git reflog
```

## What it shows

```
g7h8i9j HEAD@{0}: commit: Add login feature
d4e5f6a HEAD@{1}: rebase (finish): returning to refs/heads/main
a3f5c9d HEAD@{2}: rebase (pick): Fix typo
x9y8z7a HEAD@{3}: reset: moving to HEAD~1
j1k2l3m HEAD@{4}: commit: WIP login form
```

Each line is a point `HEAD` was at, most recent first. `HEAD@{0}` is where you are right now; `HEAD@{1}` is one move ago, and so on.

**Important:** the reflog is local to your machine and isn't shared or pushed anywhere — it only helps recover from your own history-rewriting mistakes, not a teammate's.

---

## Why this matters after `05_history_rewriting`

Every command in this section — `amend`, `rebase`, `cherry-pick`, and `reset --hard` from `02_git_commands/04_undoing-changes.md` — can make a commit seem to "disappear" from your branch. It's not actually gone yet; the reflog still points to it, usually for about 90 days by default, until Git eventually garbage-collects genuinely unreachable objects.

---

## Recovering from common mistakes

### "I ran `reset --hard` and lost commits"

```bash
git reflog
```

```
x9y8z7a HEAD@{1}: commit: Add important feature   ← the one you lost
a3f5c9d HEAD@{0}: reset: moving to HEAD~1
```

```bash
git reset --hard HEAD@{1}
```

Moves you back to exactly where you were before the reset.

### "My rebase went wrong and I want to undo the whole thing"

```bash
git reflog
```

```
d4e5f6a HEAD@{5}: rebase (start): checkout main
```

Find the entry from just before the rebase started, then:

```bash
git reset --hard HEAD@{5}
```

(If the rebase is still in progress, `git rebase --abort` is simpler — see `02_rebase.md`. Reflog recovery is for when it's already finished and you want to undo it anyway.)

### "I amended a commit and want the original back"

```bash
git reflog
```

```
d4e5f6a HEAD@{1}: commit: Add login feature          ← original
a3f5c9d HEAD@{0}: commit (amend): Add login feature  ← the amend
```

```bash
git checkout HEAD@{1}          # look at it first, or:
git reset --hard HEAD@{1}      # restore it as your branch
```

### "I deleted a branch and want a commit that was on it"

Deleting a branch doesn't delete its commits — they just become harder to find. The reflog usually still has them:

```bash
git reflog
```

```
g7h8i9j HEAD@{3}: checkout: moving from feature/old to main
```

```bash
git branch recovered-branch g7h8i9j
```

Recreates a branch pointing at that commit.

---

## Referring to reflog entries

```bash
HEAD@{0}    # current position
HEAD@{1}    # one move before that
HEAD@{2.hours.ago}   # time-based reference
```

You can use these anywhere a commit reference is valid — `git show`, `git diff`, `git reset`, `git checkout`, etc.

---

## What the reflog does NOT protect against

- `git clean` — deletes untracked files, which were never in Git's object database in the first place, so there's nothing for the reflog to point back to (see `02_git_commands/05_clean-rm-mv.md`)
- Deleting the entire `.git` folder
- Anything that happened before Git's default retention window expires (unreachable commits are eventually garbage-collected, though usually not for 90 days)

## Quick summary

- `git reflog` shows every position `HEAD` has been at, on your machine, recently
- It's the recovery tool for a `reset --hard`, bad rebase, accidental amend, or deleted branch
- `git reset --hard HEAD@{n}` (or `git branch <name> <hash>`) restores from a reflog entry
- It does not protect untracked files removed by `git clean`, and it's local-only — not a substitute for backups or pushing important work

## Guide complete

This closes out `05_history_rewriting` — and the guide as a whole. Between fundamentals, day-to-day commands, and history rewriting (with reflog as the safety net), you have what's needed to use Git confidently on a real project.
