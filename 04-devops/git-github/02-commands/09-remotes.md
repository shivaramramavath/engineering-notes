# Remotes: `remote`, `fetch`, `pull` & `push`

A **remote** is a version of your repository hosted elsewhere — usually on GitHub, GitLab, or a similar service — that you sync with. These four commands cover setting one up and moving history between it and your local repo.

```
git remote  →  manage the list of remotes a repo knows about
git fetch   →  download commits from a remote, without touching your work
git pull    →  fetch + merge (or rebase) in one step
git push    →  upload your commits to a remote
```

---

## `git remote` — managing remotes

When you `git clone` something, a remote called `origin` is added automatically. You can also add one manually to an existing repo:

```bash
git remote add origin https://github.com/user/repo.git
```

### List configured remotes

```bash
git remote -v
```

```
origin  https://github.com/user/repo.git (fetch)
origin  https://github.com/user/repo.git (push)
```

### Change a remote's URL

```bash
git remote set-url origin git@github.com:user/repo.git
```

### Add a second remote

Common when working with a fork:

```bash
git remote add upstream https://github.com/original-owner/repo.git
```

### Remove a remote

```bash
git remote remove upstream
```

---

## `git fetch` — download without merging

```bash
git fetch origin
```

Downloads any new commits, branches, and tags from `origin`, updating your **remote-tracking branches** (e.g. `origin/main`) — but doesn't touch your own local branches or working files at all.

This makes `fetch` the safe way to "see what's changed" without affecting your current work:

```bash
git fetch origin
git log origin/main --oneline     # look at what's new, before deciding to merge it
git diff main origin/main          # see exactly what differs
```

---

## `git pull` — fetch and integrate

```bash
git pull origin main
```

Equivalent to:

```bash
git fetch origin
git merge origin/main
```

Since it merges automatically, `pull` can create a merge commit or trigger a conflict — same rules as `07_merging.md`.

### Pulling with rebase instead of merge

```bash
git pull --rebase
```

Replays your local commits on top of the fetched changes instead of creating a merge commit — keeps history linear. Useful on a personal feature branch; see `05_history_rewriting/02_rebase.md` for the trade-offs of rebasing.

### Setting rebase as the default for pulls

```bash
git config --global pull.rebase true
```

---

## `git push` — upload your commits

```bash
git push origin main
```

Uploads commits from your local `main` to `origin`'s `main`.

### First push of a new branch

```bash
git push -u origin feature/login
```

`-u` (`--set-upstream`) links your local branch to the remote one, so future pushes/pulls on this branch can just be `git push` / `git pull` with no arguments (see `09_tracking-upstream.md`).

### Push rejected? (someone else pushed first)

```
! [rejected]  main -> main (fetch first)
```

This means the remote has commits you don't have locally yet. Fix it by integrating them first:

```bash
git pull
git push
```

### Force-pushing (use with real caution)

```bash
git push --force
```

Overwrites the remote branch with your local history, discarding whatever was there instead of merging. This can destroy teammates' work if they've already pulled and built on what you're overwriting — **never force-push a shared branch like `main`** without team agreement.

A safer variant:

```bash
git push --force-with-lease
```

Fails if the remote has commits you haven't seen yet (i.e. it refuses to blindly overwrite someone else's work), unlike a plain `--force`.

---

## Putting it together: a typical sync

```bash
git fetch origin                 # see what's new, no changes yet
git status                       # "your branch is behind by N commits"
git pull                         # bring those changes in
# ... work, commit ...
git push                         # send your commits up
```

## `fetch` vs `pull` at a glance

|             | Downloads new commits? | Merges into your branch?        |
| ----------- | ---------------------- | ------------------------------- |
| `git fetch` | Yes                    | No — you decide what to do next |
| `git pull`  | Yes                    | Yes, automatically              |

## Quick summary

- `git remote` manages which remote repositories your local repo knows about
- `git fetch` downloads updates safely, without touching your current branch
- `git pull` = fetch + merge (or `--rebase`) in one step
- `git push` uploads local commits; `-u` sets up tracking for future pushes/pulls
- Avoid `--force`; prefer `--force-with-lease` if you must overwrite remote history, and never do either on a shared branch without agreement

## Next

**`09_tracking-upstream.md`** covers what "tracking" a remote branch actually means, and how `-u` and "upstream" fit together.
