# 01 — Git Fundamentals

Before learning more commands, it helps to understand what Git is actually doing under the hood. This section covers the core concepts — not the commands themselves, but the ideas the commands operate on.

## In this section

| File                              | Covers                                                                                                   |
| --------------------------------- | -------------------------------------------------------------------------------------------------------- |
| `01_git-vs-github.md`             | The difference between Git (the version control tool) and GitHub (a hosting service built around it)     |
| `02_repositories.md`              | What a repository is, and what actually lives inside the `.git` folder                                   |
| `03_working-tree-staging-area.md` | The three areas every change moves through: working tree → staging area → repository                     |
| `04_commits.md`                   | What a commit really is — a snapshot, not a diff — and what makes one up (author, message, parent, tree) |
| `05_git-objects.md`               | The blob/tree/commit objects Git stores internally, and how they connect to form history                 |

## Why this comes before commands

Commands like `add`, `commit`, `branch`, and `merge` are just operations on these underlying concepts. Learning the concepts first means the commands make sense the first time you see them, instead of feeling like memorized incantations.

## What you should understand after this section

- Git and GitHub are not the same thing, and Git works fine with no GitHub account at all
- A commit is a full snapshot of your project at a point in time, not just "what changed"
- Why `git add` exists as a separate step from `git commit` (the staging area)
- That a repository's entire history is really just a graph of objects, connected by hashes

## Next

Move on to **`02_git_commands`** to start using these concepts through the actual Git commands.
