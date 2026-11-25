# Commits

## What is a commit, really?

A commit is a **complete, permanent snapshot** of your entire project at a point in time — not a "diff" or a list of changes. This is one of the most common misconceptions for people coming from other tools.

```bash
git commit -m "Add login feature"
```

Internally, Git stores what every tracked file looked like at that moment. When you see a "diff" in `git log -p` or on GitHub, that's Git _computing_ the difference between two snapshots for your convenience — the diff isn't what's actually stored.

## What makes up a commit

Every commit records:

| Field                | Meaning                                               |
| -------------------- | ----------------------------------------------------- |
| **Snapshot**         | The full state of the project's files at that moment  |
| **Author**           | Name/email from `git config user.name` / `user.email` |
| **Timestamp**        | When the commit was made                              |
| **Message**          | The `-m "..."` text describing the change             |
| **Parent commit(s)** | The commit(s) that came immediately before it         |
| **Hash (SHA-1)**     | A unique ID identifying this exact commit             |

You can see all of this with:

```bash
git show <commit-hash>
```

or a compact history with:

```bash
git log
```

## The commit hash

Every commit gets a unique identifier — a 40-character SHA-1 hash, e.g. `a3f5c9d2e1b8...`. It's calculated from the commit's content (the snapshot, parent, author, message, timestamp), which is why:

- The same content always produces the same hash
- Changing anything about a commit — even the message — produces an entirely different hash
- Hashes let Git detect corruption, since content and ID are tied together

You'll usually refer to commits by a shortened version of the hash (the first 7 characters are typically enough): `a3f5c9d`.

## Parent commits: how history connects

Each commit (except the very first) points to the commit that came before it. This chain of parent pointers is what forms your project's history:

```
a1b2c3 (Initial commit)
   ↑
d4e5f6 (Add README)
   ↑
g7h8i9 (Fix typo)      ← HEAD, main
```

A **merge commit** is the one exception — it has _two_ parents, one from each branch being merged (see `02_git_commands/07_merging.md`).

## Writing a good commit message

```bash
git commit -m "Fix login redirect bug"
```

Conventions vary by team, but a widely used structure:

```
<type>: <short summary, imperative mood>

<optional longer description of why, not just what>
```

```
fix: correct redirect after failed login

Previously redirected to /dashboard even on failure,
exposing a blank page. Now redirects back to /login
with an error message.
```

Common `type` prefixes: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`.

Write commit messages in the imperative ("Add feature," not "Added feature" or "Adds feature") — this matches the tone Git itself uses (e.g. "Merge branch...").

## Committing: the full flow

```bash
git add file.js          # stage the change
git commit -m "message"  # create the snapshot
```

Skip staging for tracked files you've simply modified (not new files):

```bash
git commit -am "message"   # stages all *modified* tracked files, then commits
```

`-a` won't pick up brand-new untracked files — those still need an explicit `git add`.

## Amending the last commit

If you committed too early (forgot a file, typo'd the message):

```bash
git add forgotten-file.js
git commit --amend
```

This replaces the last commit with a new one — it doesn't add a second commit. Because the content changes, **the hash changes too**. Only amend commits you haven't shared/pushed yet (covered further in `05_history_rewriting/01_amend.md`).

## `HEAD`: where you currently are

`HEAD` is a pointer to whatever commit you currently have checked out — almost always the tip of your current branch. As you commit, `HEAD` moves forward automatically.

```bash
git log
```

```
commit g7h8i9 (HEAD -> main)
```

## Quick summary

- A commit is a full snapshot, not a diff — diffs are computed on demand for display
- Every commit has an author, timestamp, message, parent(s), and a unique hash
- Parent pointers are what link commits into a history
- `HEAD` tracks your current position in that history
- Write clear, imperative commit messages — you'll read them again later

## Next

**`05_git-objects.md`** goes one level deeper: the actual blob/tree/commit objects Git uses internally to store all of this.
