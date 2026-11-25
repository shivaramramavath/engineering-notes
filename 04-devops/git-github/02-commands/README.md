# Module 02: Essential Git Commands

## Overview
This module covers the core command-line surface of Git, transitioning from daily workspace operations (inspecting status, staging, committing, undoing changes) to branch workflows, conflict resolution, remote synchronization, stashing, patch mechanics, debugging tools (`bisect`, `blame`), enterprise scaling (`worktrees`, `submodules`, `sparse-checkout`, `LFS`), automation hooks, and custom aliases.

---

## Learning Objectives
By completing this module, you will:
- Master day-to-day repository operations: creating commits, viewing status, and generating structured diffs.
- Safely navigate and undo mistakes using modern porcelain commands (`git restore`, `git switch`, `git reset`).
- Understand branch topologies, three-way merge mechanics, fast-forward semantics, and complex conflict resolution.
- Configure and synchronize distributed remotes (`origin`, `upstream`) and track branches reliably.
- Use temporary state storage (`git stash`) and immutable release tagging (`git tag`).
- Debug defects rapidly using algorithmic binary search (`git bisect`) and line-level attribution (`git blame`).
- Manage multi-repository dependencies and large-scale monorepos using `worktrees`, `submodules`, `sparse-checkout`, and `Git LFS`.
- Automate local developer workflows through client-side lifecycle hooks and customized productivity aliases.

---

## Curriculum Roadmap

| # | Topic | File | Summary |
|---|---|---|---|
| 01 | Initialization & Cloning | [`01-init-clone.md`](./01-init-clone.md) | `init` vs `clone`, depth, shallow clones, bare repos, and protocol variants (SSH vs HTTPS). |
| 02 | Inspection & Logs | [`02-status-diff-log.md`](./02-status-diff-log.md) | `status -sb`, three-tree diffing, graph formatting, and relative date filtering. |
| 03 | Staging & Committing | [`03-add-commit.md`](./03-add-commit.md) | Interactive staging (`-p`), patch selection, semantic commit conventions, and atomic commits. |
| 04 | Undoing Changes | [`04-undoing-changes.md`](./04-undoing-changes.md) | `restore`, `reset` (--soft, --mixed, --hard), and `revert` safety boundaries. |
| 05 | Cleaning & Renaming | [`05-clean-rm-mv.md`](./05-clean-rm-mv.md) | `git clean` dry-runs, removing tracked files (`rm --cached`), and safe file renames (`mv`). |
| 06 | Branching Mechanics | [`06-branching.md`](./06-branching.md) | Pointer manipulation, `switch` vs `checkout`, tracking references, and deletion safeguards. |
| 07 | Merging Strategies | [`07-merging.md`](./07-merging.md) | Fast-forward, recursive, ort, octopus merges, merge commits, and `--squash` workflows. |
| 08 | Merge Conflict Resolution | [`08-merge-conflicts-and-resolution.md`](./08-merge-conflicts-and-resolution.md) | Conflict markers, 3-way diffs, `diff3`/`zdiff3`, checkout strategies, and `git rerere`. |
| 09 | Remote Repositories | [`09-remotes.md`](./09-remotes.md) | Adding remotes, URL rewriting, remote tracking branches, refspecs, and pruning dead refs. |
| 10 | Upstream Tracking | [`10-tracking-upstream.md`](./10-tracking-upstream.md) | Forking workflows, configuring upstream remotes, syncing branches, and rebase strategies. |
| 11 | Stashing Work in Progress | [`11-stash.md`](./11-stash.md) | Stash stack internals, `--include-untracked`, named stashes, and conflict-free popping. |
| 12 | Tags & Releases | [`12-tags-and-releases.md`](./12-tags-and-releases.md) | Lightweight vs annotated tags, cryptographic signing, pushing tags, and SemVer release tagging. |
| 13 | Diffs, Patches & Format-Patch | [`13-diff-patch-and-format-patch.md`](./13-diff-patch-and-format-patch.md) | Unified diff syntax, email patch workflows (`format-patch`, `am`), and binary diffs. |
| 14 | Blame & History Search | [`14-blame-and-log-search.md`](./14-blame-and-log-search.md) | Line attribution (`blame`), ignoring bulk reformatting (`--ignore-rev`), and pickaxe searches (`-S`, `-G`). |
| 15 | Bisect Debugging | [`15-bisect.md`](./15-bisect.md) | Algorithmic bug hunting, binary search automation with test scripts (`git bisect run`). |
| 16 | Submodules & Subtrees | [`16-submodules-and-subtrees.md`](./16-submodules-and-subtrees.md) | Nested Git repositories, `.gitmodules`, submodule pinning, subtree merging, and alternatives. |
| 17 | Git Worktrees | [`17-worktrees.md`](./17-worktrees.md) | Concurrent branch checkouts without re-cloning, shared object store, and production debugging. |
| 18 | Sparse Checkout & Partial Clone | [`18-sparse-checkout-and-partial-clone.md`](./18-sparse-checkout-and-partial-clone.md) | Monorepo optimization, blobless/treeless clones (`--filter=blob:none`), and cone mode. |
| 19 | Git LFS (Large File Storage) | [`19-git-lfs.md`](./19-git-lfs.md) | Pointer files, smudge/clean filters, tracking binary assets, migration, and storage optimization. |
| 20 | Git Hooks | [`20-hooks.md`](./20-hooks.md) | Client-side hooks (`pre-commit`, `commit-msg`), server hooks, husky, and security vetting. |
| 21 | Aliases & Productivity | [`21-aliases-and-productivity.md`](./21-aliases-and-productivity.md) | Custom shortcuts, shell function parameterization (`!f()`), fuzzy finding (`fzf`), and CLI efficiency. |

---

## Verification & Key Takeaways
- All commands in this module operate directly on standard Git versions $\ge 2.40.0$.
- Destructive commands (`reset --hard`, `clean -fdx`) should always be preceded by inspection (`status`, `clean -n`, `diff`).
- For collaborative branches, prefer rebase workflows with `--force-with-lease` over unchecked force pushes.
