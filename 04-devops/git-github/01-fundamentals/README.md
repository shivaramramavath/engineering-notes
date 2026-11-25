# Module 01: Core Git Fundamentals & Architecture

[<- Back to Setup](../00-setup/README.md) | [Back to Track Overview](../README.md) | [Next Module: 02-commands ->](../02-commands/README.md)

---

## Overview

Welcome to **Module 01: Core Git Fundamentals & Architecture** of the Git & GitHub engineering curriculum. To use Git effectively in high-throughput enterprise environments, engineers cannot treat Git as a black box of magical commands.

This module provides a rigorous, deep-dive examination of Git's internal architecture: from understanding the clear separation of concerns between the local Git engine and cloud forges like GitHub, to deconstructing the Directed Acyclic Graph (DAG), the Content-Addressable Object Database (`blobs`, `trees`, `commits`, `tags`), reference topology (`refs/heads/`, `refs/tags/`, `HEAD`), packfile delta compression, the binary anatomy of `.git/index`, and the ongoing transition to SHA-256. The module concludes with a capstone walkthrough where you build a full repository using only low-level plumbing commands.

---

## Module Roadmap & Notes

| Note | Title | Difficulty | Estimated Time | Key Topics |
| :--- | :--- | :--- | :--- | :--- |
| **[01-git-vs-github.md](./01-git-vs-github.md)** | Git vs. GitHub Architectural Breakdown | Beginner | 45 min | DVCS engine vs SaaS forge, wire protocol, upload-pack/receive-pack |
| **[02-repositories.md](./02-repositories.md)** | Anatomy of a Git Repository (.git Directory & Bare Repos) | Intermediate | 50 min | Working tree vs `.git`, bare repos, HEAD, refs, shallow/mirror clones |
| **[03-working-tree-and-staging-area.md](./03-working-tree-and-staging-area.md)** | The Three Trees: Working Tree, Staging Area & HEAD | Intermediate | 50 min | Three Trees, file states, `git restore`, diff across trees, stage slots |
| **[04-commits.md](./04-commits.md)** | Commits & The Directed Acyclic Graph (DAG) | Intermediate | 50 min | Snapshots vs deltas, commit DAG, Author vs Committer, Conventional Commits |
| **[05-git-objects.md](./05-git-objects.md)** | The Git Object Model (Blobs, Trees, Commits & Tags) | Advanced | 55 min | Content-addressable store, 4 object types, zlib, plumbing inspection |
| **[06-refs-branches-and-head.md](./06-refs-branches-and-head.md)** | References, Branches & HEAD Internals | Advanced | 50 min | Refs topology, 41-byte branches, symrefs, `ORIG_HEAD`, packed-refs, Reftable |
| **[07-packfiles-and-garbage-collection.md](./07-packfiles-and-garbage-collection.md)** | Packfiles, Delta Compression & Garbage Collection (git gc) | Advanced | 55 min | Loose vs packed, reverse delta compression, `git gc`, pruning, maintenance |
| **[08-the-index-in-depth.md](./08-the-index-in-depth.md)** | The Git Index In-Depth (.git/index Binary Anatomy & Extensions) | Advanced | 55 min | DIRC header, stat cache, TREE/UNTR extensions, `--skip-worktree` |
| **[09-hashes-and-sha-256.md](./09-hashes-and-sha-256.md)** | Cryptographic Hashing & The SHA-256 Transition | Advanced | 50 min | Cryptographic integrity, SHAttered attack, sha1dc, `--object-format=sha256` |
| **[10-git-internals-walkthrough.md](./10-git-internals-walkthrough.md)** | End-to-End Git Internals Walkthrough (Plumbing from Scratch) | Advanced | 60 min | Full repository built from scratch using only low-level plumbing commands |

---

## Architectural Summary: The Git Engine

```
+-----------------------------------------------------------------------------------------+
|                                    Git Architecture Map                                 |
|                                                                                         |
|   WORKING TREE                     STAGING AREA                       OBJECT DATABASE   |
|   (Disk Filesystem)                (.git/index)                       (.git/objects/)   |
|                                                                                         |
|   src/index.ts ────── git add ────► [Stat Cache] ─── git commit ────► [ Commit Object ] |
|   package.json                      ├── ctime / mtime                        │          |
|   README.md                         ├── file_size                            ▼          |
|                                     ├── SHA hashes ────────────────►  [ Root Tree ]     |
|                                     └── Stage slots (0-3)                    ├── Blob   |
|                                                                              ├── Blob   |
|                                                                              └── Subtree|
|                                                                                         |
|   POINTERS & REFERENCES (.git/refs/):                                                   |
|   .git/HEAD ──(symref)──► refs/heads/main ──(hash)──► [Latest Commit in DAG]            |
+-----------------------------------------------------------------------------------------+
```

---

## Learning Objectives

By completing this module, you will be able to:
1. Articulate the technical boundaries between Git (CLI engine) and GitHub (cloud collaboration platform).
2. Explain the internal structure and responsibilities of the `.git/` directory and distinguish bare from non-bare repositories.
3. Master the Three Trees data flow and inspect diffs precisely using modern `git restore` and `git diff` commands.
4. Diagram the Directed Acyclic Graph (DAG) and explain why Git stores complete snapshots rather than diff deltas.
5. Deconstruct the 4 core Git objects (blobs, trees, commits, annotated tags) and compute object hashes manually.
6. Manipulate references, symbolic refs (`HEAD`), and pseudo-refs (`ORIG_HEAD`) using atomic plumbing commands.
7. Explain reverse delta compression in packfiles and configure background automated repository maintenance.
8. Parse the binary layout of `.git/index` and leverage `--skip-worktree` and `core.untrackedCache` for performance.
9. Evaluate the cryptographic implications of the SHA-1 to SHA-256 migration.
10. Construct an entire functional Git repository using exclusively low-level plumbing commands.

---

[Proceed to Module 02: Essential Commands ->](../02-commands/README.md)
