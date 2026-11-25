# `git add` & `git commit`

The core save workflow: stage what you want to keep, then commit it. These two commands are almost always used as a pair.

```
git add     →  move changes into the staging area
git commit  →  save the staged changes as a permanent snapshot
```

---

## `git add`

Stages a change — marks it to be included in the next commit.

### Stage a specific file

```bash
git add app.js
```

### Stage multiple files

```bash
git add app.js utils.js
```

### Stage everything

```bash
git add .
```

Stages all new and modified files in the current directory and below. Be deliberate with this — it's easy to accidentally stage files you didn't mean to (build output, `.env` files, etc.). A good `.gitignore` prevents most of that.

### Stage by pattern

```bash
git add "*.js"
```

### Stage only part of a file

```bash
git add -p app.js
```

Walks through each changed "hunk" in the file and asks whether to stage it — useful when a single file has multiple unrelated changes and you want separate, focused commits.

```
Stage this hunk [y,n,q,a,d,s,e,?]?
```

- `y` — yes, stage this hunk
- `n` — no, skip it
- `s` — split into smaller hunks
- `q` — quit, stop reviewing

### Checking what's staged

```bash
git status
git diff --staged
```

---

## `git commit`

Takes everything currently staged and permanently records it as a new commit.

### Basic commit

```bash
git commit -m "Add login feature"
```

### Multi-line commit message

```bash
git commit -m "Fix login redirect" -m "Redirects to /login on failure instead of a blank page."
```

Or open your configured editor for a longer message:

```bash
git commit
```

### Skip staging for tracked files

```bash
git commit -am "Update styles"
```

`-a` automatically stages any file Git already tracks that's been modified or deleted — but it will **not** pick up brand-new untracked files. Those still need an explicit `git add`.

### Amending the last commit

```bash
git commit --amend
```

Replaces the previous commit with a new one — useful for fixing a typo in the message or adding a forgotten file. This changes the commit's hash, so only amend commits that haven't been pushed/shared yet (see `05_history_rewriting/01_amend.md`).

```bash
git add forgotten-file.js
git commit --amend --no-edit   # keep the same message, just add the file
```

---

## A typical commit cycle

```bash
git status                    # see what's changed
git add src/feature.js        # stage the parts you're ready to commit
git diff --staged             # double-check what will be committed
git commit -m "Add feature X" # save the snapshot
git log --oneline             # confirm it landed
```

## Writing good commit messages

Keep the summary short and in the imperative mood:

```
✅ Add password reset flow
❌ Added password reset flow
❌ Adding password reset flow
```

For anything non-obvious, add a body explaining _why_, not just _what_ (the diff already shows what):

```
fix: prevent duplicate email on signup

Race condition allowed two signups with the same email
when submitted within the same second. Added a unique
constraint at the database level as the source of truth.
```

## Common mistakes

- **Committing without checking `git status`/`git diff --staged` first** — easy to accidentally include unrelated or unfinished changes.
- **Using `git add .` carelessly** — stages everything, including files you may not want tracked. Use a `.gitignore` for build artifacts, `node_modules`, `.env`, etc.
- **Vague messages** — `"fix stuff"` or `"update"` tell your future self nothing when scanning `git log` later.
- **One giant commit for unrelated changes** — harder to review, harder to revert just one part later. Prefer smaller, focused commits.

## Quick summary

- `git add <file>` stages a change; `git add .` stages everything; `git add -p` stages selectively
- `git commit -m "..."` creates a permanent snapshot of what's staged
- `git commit -am "..."` skips staging for already-tracked, modified files only
- `git commit --amend` fixes the most recent commit instead of creating a new one

## Next

**`04_undoing-changes.md`** covers what to do when you've staged or committed something you didn't mean to.
