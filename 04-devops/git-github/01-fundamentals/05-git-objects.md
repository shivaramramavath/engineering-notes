<!--
================================================================================
Metadata:
  Title: The Git Object Model (Blobs, Trees, Commits & Annotated Tags)
  Track: Git & GitHub / DevOps
  Difficulty: Advanced
  Estimated Time: 55 minutes
  Prerequisites: 04-commits.md
  Key Terminology: Content-Addressable Storage, Blobs, Trees, Commits, Annotated Tags, git hash-object, git cat-file, zlib, Object Header
================================================================================
-->

# The Git Object Model (Blobs, Trees, Commits & Annotated Tags)

[<- 04-commits.md](./04-commits.md) | [Back to Module Overview](./README.md) | [06-refs-branches-and-head.md ->](./06-refs-branches-and-head.md)

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Troubleshooting](#7-common-mistakes--troubleshooting)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table](#9-comparison-table)
- [10. Real-World Interview Questions](#10-real-world-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference Cheat Sheet](#12-summary--quick-reference-cheat-sheet)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

At its core, Git is not primarily a version control system—it is a **content-addressable key-value data store** upon which a version control interface has been constructed.

- In a typical database or filesystem, you access data by providing a path, name, or memory address (`/home/user/document.txt`).
- In a **content-addressable store**, you retrieve data by providing the cryptographic hash of the content itself. If the content changes by even a single bit, its address changes completely. If two identical files exist with different names in different directories across the project, they share the exact same key and are stored only once in the object database.

Every historical version of every file, every folder hierarchy, every commit, and every release tag in a Git repository is modeled using only **four fundamental object types**: **blobs**, **trees**, **commits**, and **annotated tags**. Understanding these four primitives demystifies all Git operations.

---

## 2. Core Concepts

### The Four Fundamental Git Object Types

1. **Blob (Binary Large Object)**:
   - Stores raw file content bytes.
   - Contains **NO filename**, **NO directory path**, **NO file permissions**, and **NO timestamp**.
   - If two files in different folders have identical content, Git stores only one blob!
2. **Tree (Directory / Folder Structure)**:
   - Represents a directory in the filesystem.
   - Contains a list of entries mapping file mode permissions (`100644`, `100755`, `040000`), object types, and SHA hashes to human-readable filenames or child directory names.
   - Trees can reference blobs (files) or other trees (subdirectories).
3. **Commit (Revision Snapshot)**:
   - Points to a single top-level root tree object representing the entire repository filesystem.
   - Contains zero, one, or more parent commit hashes, author/committer identities, timestamps, and the commit message.
4. **Annotated Tag (Permanent Release Milestone)**:
   - An immutable object that points to a specific commit (or occasionally a tree or blob).
   - Contains the tagger name, email, timestamp, tagging message, and optional GPG/SSH cryptographic signature.

```
+---------------------------------------------------------------------------------+
|                             The Git Object Hierarchy                            |
|                                                                                 |
|   [ Tag Object ]                                                                |
|         │ (points to commit)                                                    |
|         ▼                                                                       |
|   [ Commit Object ] ──────────────► Parent: [ Previous Commit ]                 |
|         │ (points to root directory)                                            |
|         ▼                                                                       |
|   [ Tree Object: Root ]                                                         |
|         ├── 100644 blob a1b2c3...  README.md  ──────► [ Blob: README.md ]       |
|         ├── 100755 blob d4e5f6...  build.sh   ──────► [ Blob: build.sh ]        |
|         └── 040000 tree 7a8b9c...  src/                                         |
|                                     │                                           |
|                                     ▼                                           |
|                               [ Tree: src/ ]                                    |
|                                     └── 100644 blob f1e2d3... index.ts          |
|                                                                    │            |
|                                                                    ▼            |
|                                                            [ Blob: index.ts ]   |
+---------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### The Object Header and Storage Format
Every object written to `.git/objects/` follows an exact binary serialization formula:
```text
type <size-in-bytes>\0<payload-content>
```
1. **Type**: The ASCII string `blob`, `tree`, `commit`, or `tag`.
2. **Space**: A single ASCII space character (`0x20`).
3. **Size**: The ASCII representation of the uncompressed payload size in bytes.
4. **Null Byte**: A null byte delimiter (`0x00`).
5. **Payload**: The raw uncompressed bytes of the object.

### The Hash and Storage Engine:
- **Hashing**: Git calculates the SHA-1 (or SHA-256) hash over this entire combined header and payload.
- **Compression**: The combined byte stream is compressed using `zlib` (deflate).
- **Filesystem Placement**:
  - The first 2 hexadecimal characters of the 40-character hash form the subdirectory name in `.git/objects/`.
  - The remaining 38 characters form the filename.
  - *Example*: An object with hash `4b825dc642cb6eb9a060e54bf8d69288fbee4904` is written to `.git/objects/4b/825dc642cb6eb9a060e54bf8d69288fbee4904`.

```
Content: "Hello World\n" (12 bytes)
           │
           ▼ Prepend Header
"blob 12\0Hello World\n"
           │
           ├─── SHA-1 Hash ───► 557db03de997c86a4a028e1ebd3a1ceb225be238
           │
           └─── zlib Compress ─► Compressed Binary Data
                                        │
                                        ▼ Saved on Disk
                               .git/objects/55/7db03de997c86a4a028e1ebd3a1ceb225be238
```

---

## 4. Syntax & Usage

### Low-Level Plumbing Commands for Object Inspection

```bash
# Hash content from stdin and write it into .git/objects/ (-w)
echo "Hello World" | git hash-object -w --stdin

# Check the type of an object (blob, tree, commit, tag)
git cat-file -t <hash>

# Check the uncompressed size of an object in bytes
git cat-file -s <hash>

# Pretty-print the raw content of an object
git cat-file -p <hash>

# List all objects in the repository
git rev-list --objects --all
```

---

## 5. Practical Examples

### Example 1: Calculating a Blob Hash Manually
Verify that Git's hashing formula matches standard OS cryptography utilities:

```bash
# Content: "test content\n" (13 bytes)
CONTENT="test content"$'\n'

# 1. Ask Git to compute the hash
echo -n "$CONTENT" | git hash-object --stdin
# Output: d670460b4b4aece5915caf5c68d12f560a9fe3e4

# 2. Compute manually using standard printf and sha1sum:
# Format: "blob <size>\0<content>"
printf "blob 13\0test content\n" | sha1sum | awk '{print $1}'
# Output: d670460b4b4aece5915caf5c68d12f560a9fe3e4
```
*Exact mathematical match!*

### Example 2: Creating a Commit Completely by Hand (Plumbing Only!)
To truly understand how Git works, build a commit from scratch without touching `git add` or `git commit`:

```bash
mkdir /tmp/plumbing-lab && cd /tmp/plumbing-lab
git init

# 1. Create a blob object in .git/objects/
BLOB_HASH=$(echo "console.log('Built from scratch');" | git hash-object -w --stdin)
echo "Blob created: $BLOB_HASH"

# 2. Stage the blob into the index (.git/index) with path 'app.js'
git update-index --add --cacheinfo 100644 $BLOB_HASH app.js

# 3. Write the staging index into a Tree object
TREE_HASH=$(git write-tree)
echo "Tree created: $TREE_HASH"
git cat-file -p $TREE_HASH
# Shows: 100644 blob <blob-hash> app.js

# 4. Create a Commit object pointing to the Tree
COMMIT_HASH=$(echo "feat: manually crafted commit" | git commit-tree $TREE_HASH)
echo "Commit created: $COMMIT_HASH"
git cat-file -p $COMMIT_HASH

# 5. Point main branch to our new commit
git update-ref refs/heads/main $COMMIT_HASH

# 6. Check working tree status!
git status
# Clean!
git log -1
# Complete valid commit displayed!
```

### Example 3: Inspecting Tree Objects and Subdirectories
```bash
mkdir -p src/utils
echo "export const add = (a, b) => a + b;" > src/utils/math.ts
git add src/utils/math.ts && git commit -m "feat: math utility"

# View root tree of the latest commit
ROOT_TREE=$(git cat-file -p HEAD | grep tree | awk '{print $2}')
git cat-file -p $ROOT_TREE
# Shows: 040000 tree <hash>  src

# Descend into the src tree
SRC_TREE=$(git cat-file -p $ROOT_TREE | grep src | awk '{print $3}')
git cat-file -p $SRC_TREE
# Shows: 040000 tree <hash>  utils

# Descend into utils tree
UTILS_TREE=$(git cat-file -p $SRC_TREE | grep utils | awk '{print $3}')
git cat-file -p $UTILS_TREE
# Shows: 100644 blob <hash>  math.ts
```

### Example 4: Anatomy of an Annotated Tag Object
Create an annotated tag and inspect its raw object structure:

```bash
git tag -a v1.0.0 -m "Release version 1.0.0 for production"

# Find tag object hash
TAG_REF_HASH=$(cat .git/refs/tags/v1.0.0)

# Check type
git cat-file -t $TAG_REF_HASH
# Output: tag

# Inspect payload
git cat-file -p $TAG_REF_HASH
```
Output:
```text
object e3929009ff5a0c0e0cf476901844eef7eb1f07f8
type commit
tag v1.0.0
tagger Alex Morgan <alex@example.com> 1700000000 -0500

Release version 1.0.0 for production
```

### Example 5: Decompressing a Loose Object with Python / Shell
Inspect how objects are deflated on disk:

```bash
# Pick any loose object file in .git/objects/
OBJ_FILE=$(find .git/objects/ -type f | grep -v "pack" | head -1)

# Decompress using python zlib:
python3 -c "import zlib; print(repr(zlib.decompress(open('$OBJ_FILE', 'rb').read())))"
# Output shows raw bytes: b'blob 34\x00console.log(\'Built from scratch\');\n'
```

---

## 6. Best Practices

1. **Understand Deduplication Mechanics**: Remember that Git deduplicates identical file contents automatically. Ten copies of a 10MB video across different folders consume only 10MB of storage in `.git/objects/`, not 100MB.
2. **Never Store Monolithic Binary Updates**: If a 500MB binary file changes by 1 byte, Git generates a brand new 500MB blob object in `.git/objects/`. Use **Git LFS** for large media assets.
3. **Use Annotated Tags for Releases (`git tag -a`)**: Lightweight tags (`git tag <name>`) are just pointers in `.git/refs/tags/`. Annotated tags create permanent, auditable tag objects in `.git/objects/` containing tagger identity, date, and messages.
4. **Protect the Object Store Against Premature Pruning**: Avoid running `git prune` or `git gc --prune=now` while other team members or background jobs are actively staging files, as uncommitted staged blobs could be discarded.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `fatal: Not a valid object name` | Provided hash does not exist in `.git/objects/` or is truncated to fewer than 4 chars. | Verify hash using `git rev-parse` or `git log`. |
| Assumed blob stores filename and permissions | Blobs store only raw bytes; filenames and permissions reside in Tree objects. | Inspect Tree objects with `git cat-file -p <tree-hash>` to view metadata. |
| Lightweight tag lacks author and date metadata | Created with `git tag v1.0` instead of `git tag -a v1.0 -m "msg"`. | Recreate using `-a` flag for an annotated tag object. |
| Repository disk usage explodes after editing images | Every edit of a binary asset creates an uncompressed full-size loose blob. | Track binaries with Git LFS (Large File Storage). |
| `error: object file ... is empty / corrupted` | Hardware crash or ungraceful power loss caused truncated object write. | Recover missing blob from working tree or remote clone; see Module 03. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Content Deduplication**: Identical files across different directories, branches, or commits share the exact same blob hash and are stored once.
- **Tree Caching**: When committing a change in a deeply nested folder (e.g. `src/modules/billing/invoice.ts`), Git only recomputes trees along that specific folder path (`billing`, `modules`, `src`, root). All other subtree hashes remain identical and are reused without recalculation.

### Security
- **Collision Resistance (SHA-1 vs. SHA-256)**:
  In 2017, the SHAttered attack demonstrated practical SHA-1 collisions for PDF files. Git implemented **Sha1DC** (SHA-1 with collision detection) to detect and abort malicious colliding commits. Modern Git supports **SHA-256 repositories** (`git init --object-format=sha256`), providing military-grade cryptographic integrity.

---

## 9. Comparison Table

| Object Type | Stored Data | References Other Objects? | Holds Filenames? |
| :--- | :--- | :--- | :--- |
| **Blob** | Raw file contents | No | No (content bytes only) |
| **Tree** | Permissions + Names + Hashes | Yes (Blobs and child Trees) | Yes (Directory listing) |
| **Commit** | Metadata + Author + Message | Yes (Root Tree + Parent Commits) | No |
| **Tag (Annotated)**| Metadata + Tagger + Message | Yes (Commit, Tree, or Blob) | No |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What are the four core object types in Git's object model and what does each contain?</strong></summary>

1. **Blob**: Stores raw file content bytes without any metadata (no filename, mode, or timestamp).
2. **Tree**: Represents a directory. Contains a list of entries mapping file mode permissions (`100644`, `100755`, `040000`), object types, and SHA hashes to filenames or child tree names.
3. **Commit**: Contains a pointer to a top-level root Tree object, zero or more parent commit hashes, author/committer identities with timestamps, and the commit message.
4. **Annotated Tag**: Contains a pointer to a target object (usually a commit), the tagger identity, timestamp, tag message, and optional cryptographic signature.
</details>

<details>
<summary><strong>2. Why don't blobs store filenames or file permissions?</strong></summary>

Decoupling content (blobs) from directory metadata (trees) enables maximum storage deduplication. If a file is renamed, moved to another directory, or duplicated five times in different folders, the file content's hash remains unchanged. Git stores only **one** blob in `.git/objects/`. The filenames and file permissions reside inside Tree objects that point to that single shared blob.
</details>

<details>
<summary><strong>3. How does Git calculate the SHA hash of an object?</strong></summary>

Git prepends a standardized header to the raw payload:
`type <size-in-bytes>\0<payload>`
Where `type` is `blob`, `tree`, `commit`, or `tag`, followed by a space, the decimal length in bytes, and a null byte (`0x00`). Git then computes the SHA-1 or SHA-256 checksum over this combined byte stream.
</details>

<details>
<summary><strong>4. What is the difference between a lightweight tag and an annotated tag in the object database?</strong></summary>

- **Lightweight Tag**: Not an object in `.git/objects/`. It is simply a reference pointer file in `.git/refs/tags/<tagname>` containing a commit hash, identical to a branch ref that doesn't move.
- **Annotated Tag**: Creates an actual immutable `tag` object in `.git/objects/` with its own unique SHA hash. It records the tagger identity, timestamp, tag message, points to the target commit, and can be cryptographically signed with GPG or SSH.
</details>

<details>
<summary><strong>5. How does Git avoid re-hashing and re-writing unchanged directories in large codebases?</strong></summary>

Git's tree objects form a Merkle Tree. When a commit modifies a file, Git generates a new blob and updates the immediate parent directory tree. If sibling directories or files were not modified, their SHA hashes remain unchanged. Git simply reuses the existing hashes for those subtrees in the parent tree object, avoiding any file scanning or object writing for unmodified subtrees.
</details>

<details>
<summary><strong>6. What does the file mode `160000` represent in a Git tree object?</strong></summary>

`160000` is the special Git file mode for a **Git Submodule** (gitlink). Instead of pointing to a blob or tree inside the current repository, a `160000` entry points to an exact commit hash in a separate, independent external Git repository.
</details>

<details>
<summary><strong>7. How does Git organize loose objects on disk to prevent filesystem performance degradation?</strong></summary>

If all objects were stored in a single flat directory (`.git/objects/`), filesystems like ext4 or NTFS would suffer significant performance slowdowns when directory entry counts exceeded tens of thousands of files. Git shards objects by using the first 2 characters of the 40-character hex hash as a directory name (`.git/objects/xx/`), and the remaining 38 characters as the filename, creating 256 balanced subdirectories.
</details>

<details>
<summary><strong>8. What plumbing command allows you to view the type and contents of any object in `.git/objects/`?</strong></summary>

`git cat-file`:
- `git cat-file -t <hash>`: Prints the object type (`blob`, `tree`, `commit`, `tag`).
- `git cat-file -s <hash>`: Prints the object size in bytes.
- `git cat-file -p <hash>`: Pretty-prints the uncompressed contents of the object formatted according to its type.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Hashing a File and Storing It Manually with `git hash-object`</strong></summary>

**Task**: Create a text file, write it directly into `.git/objects/` using `git hash-object -w`, find the resulting file in `.git/objects/xx/`, and inspect its type with `git cat-file`.

**Solution**:
```bash
mkdir /tmp/objects-lab && cd /tmp/objects-lab
git init

echo "Direct Object Injection" > secret.txt
BLOB_HASH=$(git hash-object -w secret.txt)
echo "Hash: $BLOB_HASH"

# Inspect on disk
PREFIX=${BLOB_HASH:0:2}
POSTFIX=${BLOB_HASH:2}
ls -l .git/objects/$PREFIX/$POSTFIX

# Verify type and content
git cat-file -t $BLOB_HASH
git cat-file -p $BLOB_HASH
```
</details>

<details>
<summary><strong>Exercise 2: Verifying Object Deduplication</strong></summary>

**Task**: Create two identical files with different names in different subdirectories. Stage both. Verify that only a single blob object is created in `.git/objects/`.

**Solution**:
```bash
mkdir -p dir1 dir2
echo "Identical Payload" > dir1/a.txt
echo "Identical Payload" > dir2/b.txt

git add dir1/a.txt dir2/b.txt

# Inspect index
git ls-files --stage
# Notice that both dir1/a.txt and dir2/b.txt have the exact same SHA hash!
```
</details>

<details>
<summary><strong>Exercise 3: Inspecting a Tree Object's Binary Entries</strong></summary>

**Task**: Commit a couple of files and inspect the root tree object to see how permissions, object types, hashes, and filenames are formatted.

**Solution**:
```bash
git commit -m "feat: add identical files"

ROOT_TREE=$(git cat-file -p HEAD | grep tree | awk '{print $2}')
git cat-file -p $ROOT_TREE
# Outputs mode, type, hash, and name for dir1 and dir2
```
</details>

<details>
<summary><strong>Exercise 4: Comparing Lightweight vs. Annotated Tag Objects</strong></summary>

**Task**: Create one lightweight tag and one annotated tag on the same commit. Verify which one creates an entry in `.git/objects/`.

**Solution**:
```bash
# Lightweight tag
git tag v1-light

# Annotated tag
git tag -a v1-annotated -m "Annotated release"

# Check object database
git cat-file -t v1-light
# Output: commit (lightweight tag points straight to commit object!)

git cat-file -t v1-annotated
# Output: tag (annotated tag is its own object in .git/objects/!)
```
</details>

<details>
<summary><strong>Exercise 5: Building a Commit Manually via `git commit-tree`</strong></summary>

**Task**: Take the tree hash from Exercise 3 and create a new commit pointing to it with a custom message using `git commit-tree`.

**Solution**:
```bash
NEW_COMMIT=$(echo "Manual commit from tree" | git commit-tree $ROOT_TREE -p HEAD)
echo "New Commit Hash: $NEW_COMMIT"

git cat-file -p $NEW_COMMIT
# Shows tree, parent, author, committer, and message!
```
</details>

<details>
<summary><strong>Exercise 6: Listing All Objects in a Repository</strong></summary>

**Task**: Use `git rev-list` to output every blob, tree, and commit currently stored in your repository.

**Solution**:
```bash
git rev-list --objects --all
# Outputs list of SHA hashes and corresponding file paths
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Object Type | Header Prefix | Purpose | Key Attributes |
| :--- | :--- | :--- | :--- |
| **Blob** | `blob <size>\0` | File contents | Pure bytes; deduplicated by content hash |
| **Tree** | `tree <size>\0` | Directories | Maps permissions (`100644`, etc.) + names to hashes |
| **Commit** | `commit <size>\0` | Snapshot history | Points to root tree, parents, author, message |
| **Tag** | `tag <size>\0` | Release milestone | Points to commit, records tagger, message, signature |

| Command | Purpose |
| :--- | :--- |
| `git hash-object -w <file>` | Writes file into `.git/objects/` as a blob and outputs hash |
| `git cat-file -t <hash>` | Displays object type (`blob`, `tree`, `commit`, `tag`) |
| `git cat-file -s <hash>` | Displays uncompressed size in bytes |
| `git cat-file -p <hash>` | Pretty-prints the uncompressed object payload |
| `git write-tree` | Creates a tree object from current staging index |
| `git commit-tree <tree> -p <parent>` | Creates a commit object pointing to a tree |

---

## 13. Further Reading & Official Documentation

- [Git Internals - Git Objects (Pro Git Book)](https://git-scm.com/book/en/v2/Git-Internals-Git-Objects)
- [Official Git Documentation - git-cat-file](https://git-scm.com/docs/git-cat-file)
- [Official Git Documentation - git-hash-object](https://git-scm.com/docs/git-hash-object)
- [SHAttered - First Practical SHA-1 Collision](https://shattered.io/)

---

[<- 04-commits.md](./04-commits.md) | [Back to Module Overview](./README.md) | [06-refs-branches-and-head.md ->](./06-refs-branches-and-head.md)
