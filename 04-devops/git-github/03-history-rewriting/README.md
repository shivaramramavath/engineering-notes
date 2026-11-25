# Module 03: History Rewriting & Recovery

## Overview
This module explores Git's history-rewriting engine and disaster-recovery capabilities. It progresses from local commit editing (`amend`, `rebase -i`, `cherry-pick`) to repository-wide secret purging (`git-filter-repo`), safe collaborative force-push protocols (`--force-with-lease`, `--force-if-includes`), the architectural comparison of `reset` vs `revert` vs `restore`, and an emergency triage playbook for rescuing lost work.

---

## Learning Objectives
By completing this module, you will:
- Master single-commit and multi-commit history surgery using `git commit --amend` and interactive rebase (`git rebase -i`).
- Understand autosquash workflows (`git commit --fixup` and `git rebase -i --autosquash`) to eliminate manual rebase reorganization.
- Surgically port bug fixes and changes across branches using `git cherry-pick` and audit traceability (`-x`).
- Navigate Git's append-only transaction ledger (`reflog`) to recover lost commits, branches, and discarded stashes.
- Completely purge leaked passwords, private keys, and massive binaries across entire project histories using `git-filter-repo`.
- Safely push rewritten histories to remote repositories using compare-and-swap leases (`--force-with-lease --force-if-includes`).
- Understand the Three Trees architecture to choose precisely between `git reset`, `git revert`, and `git restore`.
- Execute emergency disaster recovery runbooks for hard resets, deleted branches, detached HEADs, and teammate force-pushes.

---

## Curriculum Roadmap

| # | Topic | File | Summary |
|---|---|---|---|
| 01 | Amending Commits | [`01-amend.md`](./01-amend.md) | Modifying recent commits, changing messages, updating staged trees, and avoiding public branch hazards. |
| 02 | Interactive Rebase & Linear History | [`02-rebase.md`](./02-rebase.md) | Moving commit bases, squash, fixup, drop, edit, exec test automation, autosquash, and the Golden Rule. |
| 03 | Selective Porting (Cherry-Pick) | [`03-cherry-pick.md`](./03-cherry-pick.md) | Backporting production hotfixes, `-x` provenance tracking, non-committing picks (`-n`), and merge commit picks (`-m`). |
| 04 | Reflog & Commit Rescue | [`04-reflog-and-recovery.md`](./04-reflog-and-recovery.md) | Git's internal flight recorder, relative time travel, reachability, dangling commits, and expiration rules. |
| 05 | Purging Secrets & Bloat | [`05-filter-repo-and-removing-secrets.md`](./05-filter-repo-and-removing-secrets.md) | `git-filter-repo` Python engine, credential revocation, stripping large blobs, text replacement, and BFG comparison. |
| 06 | Safe Force Pushes & Team Protocols | [`06-rewriting-shared-history-safely.md`](./06-rewriting-shared-history-safely.md) | Compare-and-swap pushing (`--force-with-lease`), `--force-if-includes`, auto-fetch protection, and branch governance. |
| 07 | Reset vs Revert vs Restore Matrix | [`07-reset-vs-revert-vs-restore.md`](./07-reset-vs-revert-vs-restore.md) | The Three Trees architecture (HEAD, Index, Working Directory), public vs private history, and command decision trees. |
| 08 | Lost Work Recovery Playbook | [`08-recovering-lost-work-playbook.md`](./08-recovering-lost-work-playbook.md) | Emergency triage runbook: accidental hard reset, deleted branch, ruined rebase, dropped stash, and fsck deep scans. |

---

## Safety Guidelines for History Rewriting
- **Rule 1 (The Golden Rule)**: Never rebase or rewrite commits that have been pushed to public, shared branches (`main`, `master`, `release/*`).
- **Rule 2 (Compare-and-Swap)**: When pushing rebased feature branches, always use `git push --force-with-lease --force-if-includes`.
- **Rule 3 (Credential Revocation)**: If credentials are leaked, revoke them in the cloud provider immediately before running history-scrubbing tools.
- **Rule 4 (Triage Before Panic)**: If commits appear lost, check `git reflog` before running any cleaning or pruning commands.
