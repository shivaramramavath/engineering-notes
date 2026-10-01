# `git commit --amend`

The simplest history-rewriting tool: fix the **most recent** commit in place instead of creating a new one on top of it.

```bash
git commit --amend
```

## What it actually does

`--amend` doesn't edit the old commit — nothing in Git is ever edited in place. It creates a brand-new commit with your changes, and moves your branch pointer to it, replacing the old one. The old commit still technically exists in Git's object store for a while (recoverable via `git reflog` if needed), but it's no longer part of your branch's history.

Because the content changes, **the commit hash changes too** — this is the detail that makes amending unsafe once a commit has been pushed and shared (see the golden rule in `00_README.md`).

---

## Fixing the commit message

```bash
git commit --amend -m "Corrected commit message"
```

Or, to open your configured editor with the old message pre-filled:

```bash
git commit --amend
```

---

## Adding a forgotten file

```bash
git add forgotten-file.js
git commit --amend --no-edit
```

`--no-edit` keeps the existing commit message unchanged — use this when you're only adding/fixing content, not the message.

---

## Fixing the last commit's content

```bash
# edit the file(s)
git add fixed-file.js
git commit --amend --no-edit
```

This is the common "oops, typo in the code I just committed" fix — rather than making a separate "fix typo" commit, fold the fix into the commit it belongs to.

---

## Changing the author

```bash
git commit --amend --author="New Name <new@example.com>"
```

Useful if a commit was accidentally made under the wrong Git identity.

---

## What amend does NOT do

- It only touches the **most recent** commit — for anything further back in history, you need interactive rebase (`02_rebase.md`).
- It doesn't change anything about the commit's parent or position in history — only its content, message, and/or author.

---

## The safety rule

```
✅ Safe:    amending a commit that only exists locally, never pushed
❌ Unsafe:  amending a commit that's already been pushed/shared
```

If you've already pushed and then amend, your local and remote histories diverge at that commit. Pushing again requires:

```bash
git push --force-with-lease
```

which will fail (safely) if someone else has pushed to that branch in the meantime — but even when it succeeds, anyone who already pulled the old commit now has a conflicting version. Only do this on branches you're certain nobody else is building on, and ideally only ever on your own local, unpushed work.

## Quick summary

- `git commit --amend` replaces your last commit with a new one — new hash, same or updated content
- Use `--no-edit` to keep the message, or `-m "..."` to set a new one
- Add a forgotten file: `git add file && git commit --amend --no-edit`
- Never amend a commit that's already been pushed and might be shared

## Next

**`02_rebase.md`** covers changing more than just the last commit — reordering, editing, or squashing a whole series of commits.
