# Tracking Branches & Upstream

This is the concept that makes `git push` and `git pull` work with no arguments once you've set it up — and the source of a lot of beginner confusion about what "upstream" even means.

## What is a tracking branch?

A **tracking branch** (fully: "remote-tracking branch") is a local branch that has a remembered link to a specific branch on a remote. That link is what lets Git know: when you `push`/`pull` on this branch with no arguments, _this_ is the remote branch to sync with.

```bash
git branch -vv
```

```
* main            g7h8i9j [origin/main] Add login feature
  feature/signup  a9f2c31 [origin/feature/signup: ahead 2] Add signup form
```

- `[origin/main]` — `main` is tracking `origin/main`, and they're in sync
- `[origin/feature/signup: ahead 2]` — this branch is tracking `origin/feature/signup`, and has 2 local commits not yet pushed

## What "upstream" means

"Upstream" is just the term for the remote branch a local branch tracks. Setting the upstream is what creates the tracking relationship in the first place.

```
local branch  ──tracks──►  upstream (remote branch)

feature/login  ──tracks──►  origin/feature/login
```

Once that link exists, `git push` and `git pull` know where to send/fetch from without you typing the remote and branch name every time.

## Setting the upstream

### When cloning

If you `git clone` a repo, your local default branch (e.g. `main`) is automatically set to track the matching remote branch — no setup needed.

### When pushing a new branch for the first time

```bash
git push -u origin feature/login
```

`-u` (short for `--set-upstream`) does two things in one command: pushes the branch, **and** links it to `origin/feature/login` for every future push/pull.

After that first push, you can just run:

```bash
git push
git pull
```

with no arguments — Git already knows where.

### Setting/changing the upstream without pushing

```bash
git branch --set-upstream-to=origin/main main
```

Useful if the tracking link got lost, or you want a local branch to track a _different_ remote branch than its own name would suggest.

### Checking the current upstream

```bash
git status
```

```
On branch feature/login
Your branch is up to date with 'origin/feature/login'.
```

or:

```bash
git rev-parse --abbrev-ref --symbolic-full-name @{u}
```

---

## "Ahead" and "behind"

Once tracking is set up, `git status` (and `branch -vv`) tell you how your branch compares to its upstream:

```
Your branch is ahead of 'origin/main' by 2 commits.
```

You have local commits not yet pushed.

```
Your branch is behind 'origin/main' by 3 commits.
```

The remote has commits you haven't pulled yet.

```
Your branch and 'origin/main' have diverged,
and have 2 and 3 different commits each, respectively.
```

Both sides have unique commits — this is the situation that requires a merge or rebase to reconcile (see `07_merging.md` and `05_history_rewriting/02_rebase.md`).

---

## Upstream vs. `upstream` remote (the naming collision)

One genuinely confusing wrinkle: "upstream" as a _concept_ (the remote branch you track) is different from `upstream` as a common _remote name_ used specifically in fork-based workflows:

```bash
# origin = your fork
# "upstream" (the remote's name here) = the original repo you forked from
git remote add upstream https://github.com/original-owner/repo.git
git fetch upstream
git merge upstream/main
```

Here, `upstream` is just a chosen name for a second remote — it has nothing to do with the tracking relationship above, other than sharing the word. Most people encounter both meanings around the same time, which is exactly why it's confusing at first.

## Quick summary

- A tracking branch is a local branch linked to a specific remote branch — its "upstream"
- `git push -u origin <branch>` sets that link on first push; after that, `push`/`pull` need no arguments
- `git branch -vv` and `git status` both show the tracking relationship and ahead/behind counts
- "Upstream" (the tracking concept) and `upstream` (a conventional remote name in fork workflows) are two different things that happen to share a word

## Section complete

You've now covered the full day-to-day command set. **`05_history_rewriting`** picks up from here with tools for changing history you've already committed — amending, rebasing, and cherry-picking.
