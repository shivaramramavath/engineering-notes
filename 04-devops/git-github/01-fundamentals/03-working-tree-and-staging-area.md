# Working Tree & Staging Area

Every change you make in Git passes through three distinct areas before it becomes permanent history. Understanding these is the key to understanding why commands like `git add` and `git commit` are separate steps.

```
Working Tree  ──git add──►  Staging Area  ──git commit──►  Repository
 (your files)                 (index)                     (.git history)
```

## 1. The working tree

This is just your project folder as it exists on disk — the files you open in your editor and actually edit. When you save a change, Git notices it, but does nothing with it automatically.

```bash
echo "console.log('hi')" > app.js
git status
```

```
Untracked files:
  app.js
```

At this point, `app.js` exists only in the working tree. Git knows it's there but isn't tracking its history yet.

## 2. The staging area (the "index")

The staging area is a holding zone — a list of changes you're preparing to include in your _next_ commit. You put things here with `git add`:

```bash
git add app.js
git status
```

```
Changes to be committed:
  new file:   app.js
```

The staging area is stored in a file called `index` inside `.git`. It's what makes Git different from most other tools: you don't commit "everything that changed," you commit exactly what you've staged.

### Why does staging exist as a separate step?

It lets you build a commit deliberately, rather than being forced to commit every change in a file at once. For example, if you edited two unrelated things in the same file, you can stage and commit them separately:

```bash
git add -p app.js   # interactively choose which changes to stage
```

Or stage some files but not others:

```bash
git add src/feature.js     # ready to commit
# leave notes.md unstaged — not ready yet
```

This means a commit can be a clean, intentional snapshot instead of "whatever happened to be in the folder at the time."

## 3. The repository (committed history)

Running `git commit` takes everything currently staged and permanently saves it as a new snapshot in the repository's history:

```bash
git commit -m "Add app.js"
```

Once committed, that snapshot is part of your project's permanent history (unless you deliberately rewrite history later — see `05_history_rewriting`).

## Seeing all three states at once

```bash
git status
```

This single command shows you the state of all three areas:

```
On branch main

Changes to be committed:      ← staged, ready for the next commit
  modified:   app.js

Changes not staged for commit: ← modified in working tree, not staged
  modified:   utils.js

Untracked files:                ← working tree only, Git isn't tracking these yet
  notes.txt
```

## Moving between the areas

| Action                           | Command                       | Direction              |
| -------------------------------- | ----------------------------- | ---------------------- |
| Stage a change                   | `git add <file>`              | Working tree → Staging |
| Unstage a change (keep the edit) | `git restore --staged <file>` | Staging → Working tree |
| Commit staged changes            | `git commit`                  | Staging → Repository   |
| Discard a working-tree edit      | `git restore <file>`          | Reverts to last commit |

(These are covered in depth in `02_git_commands/04_undoing-changes.md`.)

## A file's possible states

A tracked file in your working tree is always in one of these states:

- **Unmodified** — matches the last commit exactly
- **Modified** — changed since the last commit, not yet staged
- **Staged** — changed and added, ready for the next commit

An **untracked** file is one Git has never been told to follow at all (new files, before their first `git add`).

## Quick summary

- Working tree → what's on disk right now
- Staging area → what you've deliberately chosen to include in the next commit
- Repository → permanent, committed snapshots
- `git add` moves changes forward one step; `git commit` moves them the final step
- `git status` is how you check where everything currently sits

## Next

**`04_commits.md`** covers what actually gets created when you run `git commit` — what a commit _is_, structurally.
