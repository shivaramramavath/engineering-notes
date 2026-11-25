# `git rebase` & Interactive Rebase

Rebasing replays your commits onto a different base commit — as if you'd started your work from there in the first place. It's the main alternative to merging, and interactive rebase is the tool for cleaning up a series of local commits.

```
git rebase              →  replay commits onto a new base (an alternative to merge)
git rebase -i           →  interactively edit, reorder, squash, or drop commits
```

---

## Basic rebase: an alternative to merge

Say `main` has moved forward since you branched off:

```
main:            a1b2c3 → x9y8z7
feature/login:   a1b2c3 → d4e5f6 → g7h8i9
```

A **merge** would combine both histories with a merge commit. A **rebase** instead moves your branch's commits so they start from `main`'s latest commit:

```bash
git switch feature/login
git rebase main
```

```
Before:
main:            a1b2c3 → x9y8z7
feature/login:   a1b2c3 → d4e5f6 → g7h8i9

After:
main:            a1b2c3 → x9y8z7
feature/login:   a1b2c3 → x9y8z7 → d4e5f6' → g7h8i9'
```

Your commits (`d4e5f6'`, `g7h8i9'`) are re-created with new hashes on top of `x9y8z7` — the content is the same, but they're technically new commits. History ends up **linear**, with no merge commit at all.

## Merge vs rebase

|                                 | Merge                                 | Rebase                                                   |
| ------------------------------- | ------------------------------------- | -------------------------------------------------------- |
| History shape                   | Branching, preserves exact chronology | Linear, rewrites your branch's commits                   |
| Creates a new commit?           | Yes (merge commit)                    | No merge commit; existing commits get new hashes         |
| Safe on pushed/shared branches? | Yes                                   | Only your own commits, and only if not yet pushed/shared |
| `git log --graph` readability   | Shows the branch/merge structure      | Clean straight line                                      |

A common convention: rebase your own local feature branch to keep it up to date with `main`, but merge (never rebase) when integrating a finished feature branch into `main`.

## Handling conflicts during a rebase

Conflicts during rebase work like merge conflicts (see `02_git_commands/07_merging.md`), but resolved one commit at a time:

```bash
git rebase main
```

```
CONFLICT (content): Merge conflict in app.js
```

```bash
# fix the conflict in app.js
git add app.js
git rebase --continue
```

If a commit's conflict isn't worth resolving:

```bash
git rebase --skip
```

To back out of the whole rebase and return to how things were before it started:

```bash
git rebase --abort
```

---

## Interactive rebase: cleaning up local history

```bash
git rebase -i HEAD~4
```

Opens an editor listing your last 4 commits, oldest first:

```
pick a1b2c3 Add login form
pick d4e5f6 Fix typo
pick g7h8i9 WIP
pick j1k2l3 Actually finish login feature
```

Change the word at the start of each line to control what happens to that commit:

| Command  | Effect                                                                   |
| -------- | ------------------------------------------------------------------------ |
| `pick`   | Keep the commit as-is                                                    |
| `reword` | Keep the commit, but edit its message                                    |
| `edit`   | Pause here so you can amend the commit's content                         |
| `squash` | Combine this commit into the one above it, keeping both messages to edit |
| `fixup`  | Like `squash`, but discards this commit's message entirely               |
| `drop`   | Remove the commit entirely                                               |

### Example: squashing messy work into one clean commit

```
pick a1b2c3 Add login form
fixup d4e5f6 Fix typo
fixup g7h8i9 WIP
fixup j1k2l3 Actually finish login feature
```

Result: a single commit, `Add login form`, containing all four commits' combined changes.

### Example: reordering commits

Just reorder the lines — Git replays them top to bottom in whatever order you leave them:

```
pick g7h8i9 Add tests
pick a1b2c3 Add login form
```

### Example: editing a specific commit's content

```
pick a1b2c3 Add login form
edit d4e5f6 Fix typo
pick g7h8i9 WIP
```

Rebase pauses right after `d4e5f6`:

```bash
# make your changes
git add .
git commit --amend
git rebase --continue
```

---

## Rewriting more of history: `--root`

```bash
git rebase -i --root
```

Interactively rebase your entire history, all the way back to the very first commit. Rarely needed, but occasionally useful for cleaning up an old project's messy early history.

---

## The golden rule (repeated on purpose)

```
✅ Rebase your own local, unpushed commits freely.
❌ Never rebase commits that have been pushed and others may have pulled.
```

Since rebasing creates new commit hashes, doing it on shared history means everyone else's copy of those commits is now "different" from yours — leading to duplicated commits and painful merges when they eventually sync up.

## Quick summary

- `git rebase main` replays your branch's commits on top of `main`'s latest commit — a linear alternative to merging
- Conflicts during rebase are resolved one commit at a time with `add` + `rebase --continue`
- `git rebase -i` lets you reword, squash, fixup, reorder, edit, or drop commits before they become permanent
- Rebasing rewrites hashes — only do it to commits that are still entirely local

## Next

**`03_cherry-pick.md`** covers copying a single specific commit onto another branch, without rebasing or merging everything else.
