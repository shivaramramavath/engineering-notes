# Git Objects

Everything in Git — every file, every folder, every commit — is stored internally as one of four object types inside `.git/objects`. Understanding these demystifies almost everything else Git does.

## The four object types

| Object     | Stores                                                                                   |
| ---------- | ---------------------------------------------------------------------------------------- |
| **Blob**   | The contents of a single file (just the data, no filename)                               |
| **Tree**   | A folder listing — filenames, permissions, and pointers to blobs/other trees             |
| **Commit** | A pointer to one tree (the project snapshot), plus author, message, and parent commit(s) |
| **Tag**    | A named, permanent pointer to a specific commit (used for releases)                      |

Every object is identified by a **SHA-1 hash** of its own content, and every object is immutable — you never edit an object, you create a new one.

## How they fit together

```
commit
  │
  ├── author, message, timestamp
  ├── parent commit(s)
  └── tree ──────────────┐
                          ▼
                        tree (project root)
                          ├── blob (README.md contents)
                          ├── blob (package.json contents)
                          └── tree (src/)
                                ├── blob (index.js contents)
                                └── blob (utils.js contents)
```

A commit doesn't store files directly — it points to a **tree**, which represents the entire folder structure at that moment. The tree points to **blobs** for file contents and to other trees for subfolders.

## Blobs store content, not filenames

This is a key, often-surprising detail: a blob is identified purely by its content's hash. Two files with identical content — even in completely different locations, or with different names — are stored as the **same blob**. The filename lives in the tree object, not the blob.

This is also why renaming a file with identical content is cheap for Git to detect: the blob hash doesn't change, only the tree entry pointing to it does.

## Seeing objects yourself

You can inspect Git's internals directly with the plumbing command `git cat-file`:

```bash
# find a commit's hash
git log --oneline

# see what type an object is
git cat-file -t <hash>

# see a commit's contents
git cat-file -p <commit-hash>
```

```
tree a1b2c3d4...
parent e5f6a7b8...
author Your Name <you@example.com> 1700000000 +0000
committer Your Name <you@example.com> 1700000000 +0000

Add login feature
```

```bash
# see the tree it points to
git cat-file -p a1b2c3d4
```

```
100644 blob 9daeafb...  README.md
100644 blob 3d2f8a1...  package.json
040000 tree 7b9c1e0...  src
```

## Why content-addressed storage matters

Because every object's ID is derived from its content:

- **Deduplication is automatic** — identical file content anywhere in your history is stored once
- **Integrity is built in** — if any byte of an object changes, its hash no longer matches, so corruption is detectable
- **History is tamper-evident** — since each commit's hash depends on its parent's hash, changing an old commit changes every hash after it (this is exactly what `git rebase`/`amend` do deliberately)

## Branches and tags are just pointers

A branch is nothing more than a small file containing a commit hash:

```bash
cat .git/refs/heads/main
```

```
g7h8i9j0k1l2...
```

That's the entirety of what a branch "is" — a movable pointer to a commit. Creating a branch is cheap (just writing a new small file); it doesn't copy any project files. This is why Git branching is so fast compared to some other version control systems.

## Quick summary

- Git stores four object types: blob (file content), tree (directory listing), commit (snapshot metadata), tag (named pointer)
- A commit points to a tree; a tree points to blobs and other trees
- Objects are identified by the SHA-1 hash of their own content and are never modified in place
- Identical content is automatically stored only once
- Branches are just files holding a commit hash — lightweight and fast to create

## Section complete

You now have the conceptual foundation — repositories, the staging area, commits, and the objects underneath them. Move on to **`02_git_commands`** to start applying this through the actual Git commands.
