# `git status`, `git diff` & `git log`

Three read-only commands for answering "what's going on in this repo right now?" — the ones you'll run more than anything else.

```
git status  →  what's changed, and where (staged/unstaged/untracked)
git diff    →  exactly what changed, line by line
git log     →  what's already been committed
```

---

## `git status`

The single most-used Git command. Shows your current branch and the state of every file relative to the last commit.

```bash
git status
```

```
On branch main
Changes to be committed:
  modified:   app.js

Changes not staged for commit:
  modified:   utils.js

Untracked files:
  notes.txt
```

- **Staged** — will be included in the next commit
- **Not staged** — modified, but not yet added
- **Untracked** — new files Git isn't tracking yet

### Shorter output

```bash
git status -s
```

```
M  app.js      # staged
 M utils.js    # modified, not staged
?? notes.txt   # untracked
```

The first column is staged status, the second is working-tree status — handy once you're used to reading it at a glance.

---

## `git diff`

Shows the actual line-by-line changes, not just which files changed.

```bash
git diff
```

Shows changes in the working tree that **aren't staged yet** — the difference between your files on disk and what's in the staging area.

### Diff of staged changes

```bash
git diff --staged
```

(also written as `git diff --cached`) — shows what _will_ go into your next commit.

### Diff between commits

```bash
git diff <commit1> <commit2>
git diff HEAD~1 HEAD          # last commit vs the one before it
```

### Diff of a specific file

```bash
git diff app.js
```

### Reading diff output

```diff
- const port = 3000;
+ const port = process.env.PORT;
```

`-` lines were removed, `+` lines were added. A line that changed shows as a removal of the old version plus an addition of the new one.

---

## `git log`

Shows the commit history.

```bash
git log
```

```
commit g7h8i9j0k1l2 (HEAD -> main)
Author: Your Name <you@example.com>
Date:   Mon Jan 1 10:00:00 2026 +0000

    Add login feature
```

### Compact one-line format

```bash
git log --oneline
```

```
g7h8i9j Add login feature
d4e5f6a Add README
a1b2c3d Initial commit
```

### Graph view (very useful once branches are involved)

```bash
git log --oneline --graph --all
```

```
* g7h8i9j (HEAD -> main) Add login feature
| * a9f2c31 (feature/signup) Add signup form
|/
* d4e5f6a Add README
* a1b2c3d Initial commit
```

### Limiting output

```bash
git log -5                    # last 5 commits
git log --since="2 weeks ago"
git log --author="Jane"
git log --oneline -- app.js   # history of one file
```

### Showing the actual changes in each commit

```bash
git log -p          # full diff per commit
git log --stat       # just a summary of files changed
```

---

## Putting them together

A typical inspection sequence while working:

```bash
git status              # what's changed?
git diff                # what exactly changed, unstaged?
git add app.js
git diff --staged       # confirm what's about to be committed
git commit -m "..."
git log --oneline        # confirm the commit landed
```

## Quick summary

| Command                     | Answers                                     |
| --------------------------- | ------------------------------------------- |
| `git status`                | What state is everything in right now?      |
| `git diff`                  | What exactly changed (unstaged by default)? |
| `git diff --staged`         | What exactly will be committed?             |
| `git log`                   | What has already been committed?            |
| `git log --oneline --graph` | How do branches relate to each other?       |

## Next

**`03_add-commit.md`** covers turning what `status`/`diff` show you into an actual commit.
