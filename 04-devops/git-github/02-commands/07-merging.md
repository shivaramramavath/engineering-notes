# Merging & Merge Conflicts

`git merge` brings the changes from one branch into another. Most of the time this happens automatically — this file covers both the happy path and what to do when Git can't figure it out on its own.

```
git merge  →  combine another branch's history into your current branch
```

---

## Basic merge

```bash
git switch main
git merge feature/login
```

This merges `feature/login` **into** whatever branch you're currently on (`main`, in this example). Order matters — always `switch` to the branch you want to receive the changes first.

## The two kinds of merge

### Fast-forward merge

If `main` hasn't moved since `feature/login` was branched off, Git just moves `main`'s pointer forward to `feature/login`'s latest commit — no new commit is created:

```
Before:
main:            a1b2c3
feature/login:   a1b2c3 → d4e5f6 → g7h8i9

After merge:
main:            a1b2c3 → d4e5f6 → g7h8i9   (feature/login too — pointer just moved)
```

### Three-way (true) merge

If `main` has moved forward too (other commits landed on it since the branch split), Git creates a new **merge commit** with two parents — one from each branch:

```
main:            a1b2c3 → x9y8z7 ─────────┐
                                            ├─ merge commit
feature/login:   a1b2c3 → d4e5f6 → g7h8i9 ─┘
```

```bash
git merge feature/login
```

```
Merge made by the 'recursive' strategy.
```

### Forcing a merge commit even when fast-forward is possible

```bash
git merge --no-ff feature/login
```

Some teams prefer this deliberately — it keeps a visible record in `git log --graph` that a feature branch existed, even when the history was linear enough to fast-forward.

---

## When merges fail: conflicts

A conflict happens when the **same lines** in the **same file** were changed differently on both branches, and Git can't automatically decide which version is correct.

```bash
git merge feature/login
```

```
Auto-merging app.js
CONFLICT (content): Merge conflict in app.js
Automatic merge failed; fix conflicts and then commit the result.
```

### What a conflict looks like in the file

Git inserts conflict markers directly into the file, showing both versions:

```
<<<<<<< HEAD
const PORT = 3000;
=======
const PORT = process.env.PORT;
>>>>>>> feature/login
```

- Everything between `<<<<<<< HEAD` and `=======` is **your current branch's** version
- Everything between `=======` and `>>>>>>> feature/login` is the **incoming branch's** version

### Resolving a conflict

1. Open the file and decide what the final version should be — this might be one side, the other, or a manual combination of both.
2. **Remove the conflict markers entirely** (`<<<<<<<`, `=======`, `>>>>>>>`).
3. Stage the resolved file:
   ```bash
   git add app.js
   ```
4. Once every conflicted file is resolved and staged, complete the merge:
   ```bash
   git commit
   ```
   Git pre-fills a merge commit message — usually fine to accept as-is.

### Seeing which files are conflicted

```bash
git status
```

```
Unmerged paths:
  both modified:   app.js
```

### Aborting a merge entirely

If it's gotten messy and you want to back out completely:

```bash
git merge --abort
```

Returns everything to exactly how it was before you ran `git merge` — as if it never happened.

### Using a merge tool

For conflicts spanning many files, a visual merge tool can help:

```bash
git mergetool
```

(Requires a configured tool — e.g. VS Code, `meld`, `kdiff3` — set via `git config merge.tool`.)

---

## Avoiding conflicts in the first place

- Merge/pull `main` into your feature branch **regularly**, rather than letting it drift for weeks — smaller, more frequent merges produce smaller, easier conflicts.
- Keep feature branches focused and short-lived.
- Coordinate with teammates when you know you're both touching the same file/area.

## Quick summary

- `git merge <branch>` brings another branch's history into your current branch
- Fast-forward merges just move a pointer; three-way merges create a real merge commit
- A conflict means Git needs a human decision — resolve by editing out the `<<<<<<<`/`=======`/`>>>>>>>` markers, then `add` and `commit`
- `git merge --abort` cleanly cancels an in-progress, conflicted merge
- Merging `main` into your branch often, rather than rarely, keeps conflicts small

## Next

**`08_remotes.md`** covers pushing and pulling this history to and from a remote repository.
