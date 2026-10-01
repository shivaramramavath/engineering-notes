# 05 — History Rewriting

Everything in `02_git_commands` treats history as append-only — you add commits, undo them, or add new ones that reverse them. This section covers tools that actually **change existing commits**: their content, their order, or which branch they appear to belong to.

## In this section

| File                | Covers                                                                                                            |
| ------------------- | ----------------------------------------------------------------------------------------------------------------- |
| `01_amend.md`       | Fixing the most recent commit in place — message, forgotten files, or both                                        |
| `02_rebase.md`      | Replaying commits onto a new base, including interactive rebase for editing/reordering/squashing multiple commits |
| `03_cherry-pick.md` | Copying a specific commit from one branch onto another, without merging everything else                           |
| `04_reflog.md`      | Git's safety net — a log of every place `HEAD` has pointed, useful for recovering from a rewrite gone wrong       |

## The one rule that matters more than any command here

> **Never rewrite history that's already been pushed and might be shared with others.**

Amending, rebasing, and cherry-picking all create commits with **new hashes**, even when the content looks identical. If someone else has already pulled the original commits, rewriting them causes their local history to diverge from yours — leading to duplicated commits, confusing conflicts, or force-push headaches. Everything in this section is safe on commits that exist **only locally**; get much more cautious once something's been pushed and shared.

## Why learn this at all, then?

Because cleaning up your own local, unpushed history — before it becomes permanent and shared — makes for a much more readable project history. Squashing 15 "wip" commits into one clear commit, or fixing a typo in a message before anyone sees it, costs nothing and helps everyone reading `git log` later.

## What you should be able to do after this section

- Fix the last commit without creating a messy "fix typo" follow-up commit
- Clean up a string of local commits into a small number of clear ones (interactive rebase)
- Pull a single useful commit from another branch without merging everything else on it
- Recover from a rebase or reset that went wrong, using `git reflog`

## This is the last section

At this point you've covered setup, fundamentals, day-to-day commands, and history rewriting — the practical core of using Git. From here, the best next step is using Git on a real project, and coming back to earlier sections as reference when something doesn't behave the way you expect.
