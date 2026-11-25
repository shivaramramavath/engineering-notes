<!--
================================================================================
Metadata:
  Title: Technical Interview Preparation - Git Internals & Data Structures Q&A
  Track: Git & GitHub / DevOps
  Difficulty: Staff to Principal
  Estimated Time: 60 minutes
  Key Terminology: Content-Addressable Database, DAG, Blobs, Trees, Commits, Packfiles, Loose Objects, Plumbing Commands, SHA-256
================================================================================
-->

# Git Internals & Data Structures: 20 In-Depth Interview Questions & Answers

[<- Back to Interview Overview](./README.md) | [Next Note: 04-troubleshooting-scenarios.md ->](./04-troubleshooting-scenarios.md)

---

## Overview

This guide provides 20 senior- and staff-level interview questions exploring Git's underlying data structures, object storage model, cryptographic content-addressing mechanisms, packfile indexing, and low-level plumbing commands.

---

## Questions Index

1. [Q01: What data structure does Git use under the hood to model repository history?](#q01-what-data-structure-does-git-use-under-the-hood-to-model-repository-history)
2. [Q02: What are the four fundamental object types stored in `.git/objects`?](#q02-what-are-the-four-fundamental-object-types-stored-in-gitobjects)
3. [Q03: How does Git compute an object's SHA hash, and what is the exact header format?](#q03-how-does-git-compute-an-objects-sha-hash-and-what-is-the-exact-header-format)
4. [Q04: Why does a blob object in Git NOT store the file's name, timestamp, or permissions?](#q04-why-does-a-blob-object-in-git-not-store-the-files-name-timestamp-or-permissions)
5. [Q05: What is a tree object and how does it represent directories and file metadata?](#q05-what-is-a-tree-object-and-how-does-it-represent-directories-and-file-metadata)
6. [Q06: Explain the anatomy of a raw commit object.](#q06-explain-the-anatomy-of-a-raw-commit-object)
7. [Q07: What is the difference between porcelain commands and plumbing commands? Give examples.](#q07-what-is-the-difference-between-porcelain-commands-and-plumbing-commands-give-examples)
8. [Q08: How can you create a commit from scratch using ONLY plumbing commands?](#q08-how-can-you-create-a-commit-from-scratch-using-only-plumbing-commands)
9. [Q09: What is the binary structure of the `.git/index` file?](#q09-what-is-the-binary-structure-of-the-gitindex-file)
10. [Q10: What are loose objects, and why does Git organize them in 2-character subdirectories?](#q10-what-are-loose-objects-and-why-does-git-organize-them-in-2-character-subdirectories)
11. [Q11: What is a packfile (`.pack`) and its index file (`.idx`), and why are they necessary?](#q11-what-is-a-packfile-pack-and-its-index-file-idx-and-why-are-they-necessary)
12. [Q12: How does delta compression work in packfiles, and why is it reverse-chronological?](#q12-how-does-delta-compression-work-in-packfiles-and-why-is-it-reverse-chronological)
13. [Q13: What triggers `git gc` (garbage collection), and what does it physically do?](#q13-what-triggers-git-gc-garbage-collection-and-what-does-it-physically-do)
14. [Q14: Explain the difference between unreachable objects and dangling objects.](#q14-explain-the-difference-between-unreachable-objects-and-dangling-objects)
15. [Q15: What is the transition from SHA-1 to SHA-256 in Git, and why was it necessary?](#q15-what-is-the-transition-from-sha-1-to-sha-256-in-git-and-why-was-it-necessary)
16. [Q16: How does Git handle cryptographic commit verification internally (`gpgsig` header)?](#q16-how-does-git-handle-cryptographic-commit-verification-internally-gpgsig-header)
17. [Q17: What is a commit graph file (`commit-graph`) and how does it speed up traversal?](#q17-what-is-a-commit-graph-file-commit-graph-and-how-does-it-speed-up-traversal)
18. [Q18: What is Git shallow clone (`--depth 1`) vs partial clone (`--filter=blob:none`)?](#q18-what-is-git-shallow-clone---depth-1-vs-partial-clone---filterblobnone)
19. [Q19: How does the Git transport protocol v2 improve fetch negotiation performance?](#q19-how-does-the-git-transport-protocol-v2-improve-fetch-negotiation-performance)
20. [Q20: What is the `bitmap` index in packfiles, and how does it accelerate clone operations?](#q20-what-is-the-bitmap-index-in-packfiles-and-how-does-it-accelerate-clone-operations)

---

## Questions & Answers

### Q01: What data structure does Git use under the hood to model repository history?
**Answer:**  
Git models repository revision history as an immutable **Directed Acyclic Graph (DAG)** of commit objects, stored inside a **Content-Addressable Object Database**. Each node in the graph is a commit object pointing back to zero (root commit), one (regular commit), or multiple (merge commit) parent commit nodes. Edges are directed backward in time towards ancestor commits. Because edges only point to existing prior commits, cycles are mathematically impossible (acyclic).

---

### Q02: What are the four fundamental object types stored in `.git/objects`?
**Answer:**  
1. **Blob**: Stores raw uncompressed file contents (text, binary). Contains zero metadata (no filename, mode, or dates).
2. **Tree**: Represents a directory. Maps names, permissions (file modes like `100644` or `040000`), and types to the SHA hashes of child blobs and subtree objects.
3. **Commit**: Represents an immutable snapshot. Points to a top-level root tree object, lists parent commit SHAs, author, committer, timestamps, and the commit message.
4. **Tag (Annotated)**: Points to a commit or other object, carrying a tagger name, date, message, and optional cryptographic signature.

---

### Q03: How does Git compute an object's SHA hash, and what is the exact header format?
**Answer:**  
Git prepends a standardized header to the raw payload before computing the hash:
`"<type> <size>\0<payload>"`
Where:
- `<type>`: `blob`, `tree`, `commit`, or `tag`
- `<size>`: Content byte length in ASCII decimal
- `\0`: A null byte delimiter
- `<payload>`: The exact raw bytes of the content

The resulting byte sequence is hashed using SHA-1 (160 bits / 40 hex characters) or SHA-256 (256 bits / 64 hex characters), and compressed using **zlib Deflate** before being written to disk.

---

### Q04: Why does a blob object in Git NOT store the file's name, timestamp, or permissions?
**Answer:**  
Decoupling file contents from metadata enables massive deduplication:
- If a 10MB file is renamed, copied to 10 different subdirectories, or modified across multiple branches without changing content, Git stores the blob **exactly once**.
- File metadata (filename, directory path, executable bit) changes frequently even when content remains identical. Git stores this variable metadata inside **tree objects**, keeping blobs pure and maximizing content reusability.

---

### Q05: What is a tree object and how does it represent directories and file metadata?
**Answer:**  
A tree object corresponds to a Unix directory. In raw format, it contains a binary list of entries:
`[mode] [path]\0[binary SHA]`
- `100644`: Normal non-executable file (blob)
- `100755`: Executable script/binary file (blob)
- `120000`: Symbolic link (blob)
- `040000`: Subdirectory (tree object)
- `160000`: Git submodule (commit SHA)

Inspecting with plumbing: `git cat-file -p <tree-sha>` formats this binary into a readable tabular tree listing.

---

### Q06: Explain the anatomy of a raw commit object.
**Answer:**  
A raw commit object (inspected via `git cat-file -p <commit-sha>`) is structured as plain text:
```text
tree 8a3f2b1c4e9d0a...
parent 4c1a2b3d4e5f...
author Shiva <shiva@corp.com> 1700000000 +0530
committer Shiva <shiva@corp.com> 1700000000 +0530
gpgsig -----BEGIN SSH SIGNATURE-----
 ...
 -----END SSH SIGNATURE-----

feat(core): implement payment validation pipeline
```
It points directly to one root tree hash, zero or more parent commit hashes, followed by author/committer identities, optional signature blocks, a blank line, and the commit message.

---

### Q07: What is the difference between porcelain commands and plumbing commands? Give examples.
**Answer:**  
- **Porcelain Commands**: High-level, user-friendly commands intended for human interactive use. They manage multiple states and provide friendly terminal interfaces (e.g., `git add`, `git commit`, `git checkout`, `git status`, `git branch`).
- **Plumbing Commands**: Low-level, deterministic, scriptable primitives designed to manipulate internal Git objects and references directly. They produce machine-readable outputs and have stable interfaces that never change between Git releases (e.g., `git hash-object`, `git cat-file`, `git write-tree`, `git commit-tree`, `git update-ref`).

---

### Q08: How can you create a commit from scratch using ONLY plumbing commands?
**Answer:**  
```bash
# 1. Write file content to a blob object
BLOB_SHA=$(echo "Hello Git Internals" | git hash-object -w --stdin)

# 2. Stage blob into the index
git update-index --add --cacheinfo 100644 "$BLOB_SHA" hello.txt

# 3. Write index to a tree object
TREE_SHA=$(git write-tree)

# 4. Create commit object pointing to tree
COMMIT_SHA=$(echo "feat: initial plumbing commit" | git commit-tree "$TREE_SHA")

# 5. Point master branch to the new commit
git update-ref refs/heads/main "$COMMIT_SHA"
```

---

### Q09: What is the binary structure of the `.git/index` file?
**Answer:**  
The `.git/index` (staging area) is a binary file starting with a 12-byte header:
- 4 bytes: File signature `DIRC` ("Directory Cache")
- 4 bytes: Index version number (typically 2, 3, or 4)
- 4 bytes: Number of index entries
Following the header is a sorted array of entries. Each entry contains POSIX `stat` metadata (ctime, mtime, device, inode, file mode, UID, GID, file size), the 20-byte SHA-1 of the blob, merge conflict stage flags (0=normal, 1=ancestor, 2=ours, 3=theirs), and the relative file pathname. A trailing 20-byte SHA-1 checksum validates index integrity.

---

### Q10: What are loose objects, and why does Git organize them in 2-character subdirectories?
**Answer:**  
When an object is created, it is saved as a "loose object": a single zlib-compressed file stored under `.git/objects/XX/YYYY...`, where `XX` is the first 2 characters of the 40-character SHA hash, and `YYYY...` is the remaining 38 characters.
**Why Subdirectories?**: Most filesystems (like ext3 or older FAT32) suffer severe performance degradation when tens of thousands of files are placed inside a single flat directory. Splitting by the first 2 hex digits distributes files across 256 subdirectories, ensuring directories remain small and fast to seek.

---

### Q11: What is a packfile (`.pack`) and its index file (`.idx`), and why are they necessary?
**Answer:**  
Storing every file version as an individual loose object wastes disk space and consumes filesystem inodes.
- **Packfile (`.pack`)**: A single concatenated binary file holding thousands of compressed Git objects together. Inside, Git applies sliding-window delta compression between similar files across revisions.
- **Pack Index (`.idx`)**: A binary file containing a 256-entry first-level fan-out table and a sorted list of object SHAs with their exact byte offsets inside the corresponding `.pack` file. This allows Git to locate any packed object in $O(1)$ time without scanning the multi-gigabyte packfile.

---

### Q12: How does delta compression work in packfiles, and why is it reverse-chronological?
**Answer:**  
In a packfile, Git does not store full file copies for every revision. It sorts objects by filename, size, and type, and calculates byte-level sliding window diffs (deltas).
**Reverse-Chronological Storage**: Counter-intuitively, Git stores the **newest (latest) revision as a complete, un-delta'd base object**, while storing older historical revisions as reverse deltas.
**Why?**: Developers and CI systems frequently read the latest `HEAD` code. Making the latest revision a full object ensures checkout and diff operations on `HEAD` require zero delta decompression cycles, maximizing runtime performance.

---

### Q13: What triggers `git gc` (garbage collection), and what does it physically do?
**Answer:**  
`git gc` is triggered manually or automatically when Git notices too many loose objects (>6,700 by default) or during operations like `git merge` or `git push`.
It executes four core cleanup tasks:
1. Compresses all loose objects into an optimized packfile (`git pack-objects`).
2. Consolidates loose references into `.git/packed-refs` (`git pack-refs`).
3. Prunes dangling objects older than the expiry window (`core.pruneExpire`, default 14 days).
4. Updates the commit graph and bitmap indices.

---

### Q14: Explain the difference between unreachable objects and dangling objects.
**Answer:**  
- **Unreachable Object**: Any object that cannot be reached by traversing the DAG backward from any current ref (branch, tag, remote, notes).
- **Dangling Object**: A specific subset of unreachable objects: an unreachable object that has **no incoming references** from any other object (e.g., a commit abandoned by `git reset --hard` that is not referenced by any other commit or reflog).

---

### Q15: What is the transition from SHA-1 to SHA-256 in Git, and why was it necessary?
**Answer:**  
In 2017, the SHAttered research project demonstrated the first real-world SHA-1 hash collision (producing identical SHA-1 hashes for two distinct PDF files). While Git's object headers mitigate collision attacks, SHA-1's cryptographic strength is theoretically compromised.
Git developed **Object Format SHA-256** (enabled via `git init --object-format=sha256`). SHA-256 provides 256-bit collision resistance. Modern Git supports both formats and includes a bidirectional loose object translation map to allow repositories to interoperate between SHA-1 and SHA-256 remotes.

---

### Q16: How does Git handle cryptographic commit verification internally (`gpgsig` header)?
**Answer:**  
When `commit.gpgsign=true` is set, Git generates the commit payload text (excluding the signature line itself). It invokes the signing agent (`gpg` or `ssh-keygen`) to produce an ASCII-armored digital signature. Git embeds this signature directly inside the commit object header under `gpgsig`.
To verify (`git verify-commit`), Git strips the `gpgsig` header, reconstructs the original raw commit payload bytes, hashes them, and checks the signature against the public key store.

---

### Q17: What is a commit graph file (`commit-graph`) and how does it speed up traversal?
**Answer:**  
In large repositories (like Linux or Windows with 1,000,000+ commits), traversing commit parent pointers by unpacking commit objects from packfiles takes seconds.
The **`commit-graph`** file (`.git/objects/info/commit-graph`) stores a linearized, fixed-width binary table of all commit SHAs, parent offsets, and generation numbers. Operations like `git log --graph`, merge-base lookups, and branch reachability checks become orders of magnitude faster because Git queries this binary table without decompressing commit objects.

---

### Q18: What is Git shallow clone (`--depth 1`) vs partial clone (`--filter=blob:none`)?
**Answer:**  
- **Shallow Clone (`--depth 1`)**: Truncates commit history. Downloads only the latest commit and tree, breaking DAG history. Cannot push changes cleanly or perform accurate `git merge-base` operations.
- **Partial Clone (`--filter=blob:none`)**: Downloads the **entire commit DAG and all tree objects**, but completely skips downloading file blobs. Git lazily downloads individual file blobs on-demand only when you check out or diff a file. This is the modern standard for scaling massive monorepos.

---

### Q19: How does the Git transport protocol v2 improve fetch negotiation performance?
**Answer:**  
In Protocol v1, the remote server advertised *every single ref and tag* in the repository (sometimes millions of lines) before the client could even state what it wanted.
In **Protocol v2** (`git config --global protocol.version 2`), the client issues specific capability commands (e.g., `command=ls-refs`), allowing the client to request filtered ref subsets, and enabling server-side object filtering (partial clones) dynamically.

---

### Q20: What is the `bitmap` index in packfiles, and how does it accelerate clone operations?
**Answer:**  
When a client runs `git clone`, the server historically had to traverse the entire commit DAG to determine the exact set of objects to transmit.
A **Packfile Bitmap** (`.bitmap`) stores pre-computed bit arrays for reachability: each bit corresponds to an object in the packfile. Determining which objects belong to a branch requires simple bitwise OR/AND operations across bitmaps, reducing server CPU time from minutes to milliseconds during large clone requests.

---

[<- Back to Interview Overview](./README.md) | [Next Note: 04-troubleshooting-scenarios.md ->](./04-troubleshooting-scenarios.md)
