# 02 — Git Commands

With the core concepts from `01_git_fundamentals` in place, this section covers the actual commands you'll use day to day — grouped by workflow rather than listed alphabetically, so related commands are learned together.

## In this section

| File                      | Covers                                                                                                  |
| ------------------------- | ------------------------------------------------------------------------------------------------------- |
| `01_init-clone.md`        | Starting a repository — from scratch (`init`) or from an existing one (`clone`)                         |
| `02_status-diff-log.md`   | Inspecting state — what's changed (`status`, `diff`) and what's happened (`log`)                        |
| `03_add-commit.md`        | The core save workflow — staging changes and committing them                                            |
| `04_undoing-changes.md`   | The three ways to undo something — `restore`, `reset`, and `revert` — and when to use which             |
| `05_clean-rm-mv.md`       | File housekeeping — removing untracked files (`clean`), and removing/renaming tracked ones (`rm`, `mv`) |
| `06_branching.md`         | Creating and switching branches (`branch`, `switch`/`checkout`), plus common branching strategies       |
| `07_merging.md`           | Combining branches with `merge`, and resolving merge conflicts when they happen                         |
| `08_remotes.md`           | Working with remote repositories — `remote`, `fetch`, `pull`, `push`                                    |
| `09_tracking-upstream.md` | How a local branch tracks a remote one, and what "upstream" means                                       |

## How to use this section

Each file is written to be read in order the first time — `04_undoing-changes.md`, for example, assumes you already understand staging from `03_add-commit.md`. After that, treat these as reference: jump straight to the file for whatever command you need a refresher on.

## What you should be able to do after this section

- Start a repository either way (`init` or `clone`)
- Check what's changed and why, at any point (`status`, `diff`, `log`)
- Stage and commit changes deliberately
- Undo a mistake with the right tool, depending on whether it's staged, committed, or already pushed
- Create, switch, and merge branches, and resolve a conflict when one comes up
- Push and pull from a remote, and understand what "tracking" a remote branch means

## Next

**`05_history_rewriting`** covers changing history you've already committed — amending, rebasing, and cherry-picking — for when the commands here aren't enough.
