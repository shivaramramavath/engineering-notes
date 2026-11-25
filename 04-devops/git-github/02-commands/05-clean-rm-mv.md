# File Housekeeping: `clean`, `rm` & `mv`

Three commands for removing or renaming files, grouped together because they're all about tidying up rather than the core edit/commit workflow.

```
git clean  →  delete untracked files (never committed)
git rm     →  remove a tracked file (from disk AND from Git)
git mv     →  rename/move a tracked file (and record it as Git-aware)
```

---

## `git clean` — remove untracked files

Deletes files that Git isn't tracking — build output, stray temp files, anything never `git add`ed. This does **not** touch tracked files at all, no matter how they've been modified.

### Always preview first

```bash
git clean -n
```

```
Would remove dist/bundle.js
Would remove notes.txt
```

`-n` (dry run) shows what _would_ be deleted without deleting anything. Get in the habit of running this before the real thing.

### Actually delete

```bash
git clean -f
```

Git refuses to run `clean` without `-f` (force) by default, precisely because it's a permanent, non-recoverable deletion.

### Include untracked directories

```bash
git clean -fd
```

By default `clean` only removes files, not directories. Add `-d` to remove untracked directories too.

### Include ignored files too

```bash
git clean -fx     # ignored files only
git clean -fdx    # ignored files AND directories
```

Useful for wiping build artifacts that are in `.gitignore` (e.g. `node_modules`, `dist`) to get back to a truly clean checkout — but double-check with `-n` first, since this removes things `.gitignore` was specifically hiding from Git for a reason.

### ⚠️ There is no undo

Unlike `reset --hard` (sometimes rescuable via `reflog`), files deleted by `git clean` were never tracked by Git in the first place — there's no history to recover them from. Always dry-run first.

---

## `git rm` — remove a tracked file

Removes a file from both your working directory **and** stages that removal for the next commit.

```bash
git rm old-file.js
git commit -m "Remove old-file.js"
```

### Remove from Git but keep the file locally

```bash
git rm --cached secrets.env
```

Stops tracking the file going forward, without deleting it from disk. Common when you accidentally committed something that should've been in `.gitignore`:

```bash
git rm --cached .env
echo ".env" >> .gitignore
git commit -m "Stop tracking .env"
```

(Note: this doesn't remove `.env` from _past_ commits — see `05_history_rewriting` if you need to purge it from history entirely, e.g. for a leaked secret.)

### Remove a whole folder

```bash
git rm -r old-folder/
```

### Force-remove a modified/staged file

```bash
git rm -f config.js
```

Git normally refuses to `rm` a file with uncommitted changes, to avoid silently discarding them — `-f` overrides that safety check.

---

## `git mv` — rename or move a tracked file

```bash
git mv old-name.js new-name.js
```

This is a convenience shortcut for:

```bash
mv old-name.js new-name.js
git rm old-name.js
git add new-name.js
```

Functionally, plain `mv` + `git add` achieves the same result — Git detects renames based on content similarity regardless of which method you use. `git mv` is just fewer keystrokes for one file at a time.

### Moving into a folder

```bash
git mv config.js src/config.js
```

### After moving, commit as usual

```bash
git commit -m "Move config.js into src/"
```

---

## Quick summary

| Command           | Removes from disk?         | Removes from Git tracking? | Recoverable?                              |
| ----------------- | -------------------------- | -------------------------- | ----------------------------------------- |
| `git clean -f`    | Yes (untracked files only) | N/A — was never tracked    | No                                        |
| `git rm`          | Yes                        | Yes                        | Via `git checkout`/history, before commit |
| `git rm --cached` | No                         | Yes                        | N/A — file stays on disk                  |
| `git mv`          | Renames, doesn't delete    | Renames the tracked entry  | N/A                                       |

## Golden rule

Always `git clean -n` (or `git status`) before any destructive cleanup — `clean -f` and `rm -f` don't ask for confirmation, and untracked-file deletions in particular have no history to fall back on.

## Next

**`06_branching.md`** moves from single-line history into working with multiple branches.
