<!--
================================================================================
Metadata:
  Title: Tags, Releases & Semantic Versioning (SemVer 2.0.0)
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 11-stash.md
  Key Terminology: Lightweight Tags, Annotated Tags, Cryptographic Tag Signing, Semantic Versioning (SemVer), push.followTags, GitHub Releases, version:refname
================================================================================
-->

# Tags, Releases & Semantic Versioning (SemVer 2.0.0)

[<- 11-stash.md](./11-stash.md) | [Back to Module Overview](./README.md) | [13-diff-patch-and-format-patch.md ->](./13-diff-patch-and-format-patch.md)

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

In software release engineering, commit hashes like `8a1e2f7b824319451996518176510e4a775f0a02` are precise but unintuitive for customers, stakeholders, and deployment automation. Production deployments, package registries (npm, PyPI, Maven), and customer release notes require fixed, immutable, human-meaningful milestone labels.

In Git, this milestone mechanism is the **Tag**.
- Unlike a branch (which moves forward with every new commit), **a tag is permanent and stationary**. Once created, a tag permanently marks an exact commit snapshot in the Directed Acyclic Graph (DAG).
- On platforms like GitHub, Git tags anchor **GitHub Releases**, triggering automated CI/CD deployment pipelines, generating release notes, and attaching compiled distribution assets (binaries, tarballs, container images).

Understanding the fundamental distinctions between **Lightweight Tags**, **Annotated Tags**, and **Cryptographically Signed Tags**, coupled with **Semantic Versioning (SemVer 2.0.0)**, is a foundational requirement for production release engineering.

---

## 2. Core Concepts

### Lightweight vs. Annotated Tags

1. **Lightweight Tag**:
   - A pointer stored in `.git/refs/tags/<tagname>`.
   - Literally just a 41-byte text file containing a commit hash.
   - Contains **NO author**, **NO creation date**, and **NO message**.
   - Essentially a branch that does not move. Used primarily for temporary local milestones.
2. **Annotated Tag**:
   - A complete, immutable object created in `.git/objects/`.
   - Contains the tagger name, email address, creation timestamp, a dedicated release message, and a pointer to the commit object.
   - **Mandatory standard for public production releases and package distribution.**
3. **Signed Tag (`git tag -s`)**:
   - An annotated tag containing an embedded cryptographic signature (GPG or SSH).
   - Mathematically proves that the release was authorized by a specific key holder, preventing supply-chain release spoofing.

```
+-----------------------------------------------------------------------------------+
|                        Lightweight vs. Annotated Tag Storage                      |
|                                                                                   |
|  LIGHTWEIGHT TAG:                                                                 |
|  .git/refs/tags/v1.0-light ──────────────────────────────────► [ Commit Object ]  |
|  (41-byte text file pointing directly to commit)                                  |
|                                                                                   |
|  ANNOTATED TAG:                                                                   |
|  .git/refs/tags/v1.0-annotated ──► [ Tag Object in .git/objects/ ]                 |
|                                    ├── Tagger: Alex Morgan <alex@corp.io>         |
|                                    ├── Date: Mon Nov 25 10:00:00 2024             |
|                                    ├── Message: "Production Release 1.0.0"        |
|                                    ├── GPG/SSH Signature (Optional)               |
|                                    └── Points to: ────────────► [ Commit Object ] |
+-----------------------------------------------------------------------------------+
```

### Semantic Versioning (SemVer 2.0.0)
Modern software engineering follows the SemVer standard format: `MAJOR.MINOR.PATCH`:
- **`MAJOR` (e.g. `2.0.0`)**: Incompatible API breaking changes.
- **`MINOR` (e.g. `1.1.0`)**: Backward-compatible new features.
- **`PATCH` (e.g. `1.0.1`)**: Backward-compatible bug fixes and security patches.
- **Prerelease Tags**: Appended via hyphen: `1.0.0-alpha.1`, `1.0.0-rc.2`.

---

## 3. How It Works Internally

### Why `git push` Does NOT Push Tags by Default
A frequent surprise to developers is running `git tag v1.0.0 && git push origin main`, only to discover that GitHub shows zero tags!
- Git treats tags and branches as separate reference namespaces (`refs/tags/` vs `refs/heads/`).
- By design, Git's default push behavior only transfers branch references matching your active refspec.
- To push tags, you must either push them explicitly (`git push origin v1.0.0`), push all tags (`git push origin --tags`), or enable the **`push.followTags`** configuration setting.

### How `push.followTags` Works
Enabling `git config --global push.followTags true`:
- When you run `git push`, Git scans the commit history of the branch being pushed.
- It finds any **annotated tags** that point to commits reachable by that branch that are not yet on the remote server.
- It automatically pushes those annotated tags alongside the branch, while leaving irrelevant or lightweight tags untouched!

---

## 4. Syntax & Usage

### 1. Creating Tags
```bash
# Create an annotated tag with message (Standard for releases!)
git tag -a v1.0.0 -m "Release version 1.0.0: initial public production release"

# Create a cryptographically signed tag using configured GPG/SSH key (-s)
git tag -s v2.1.0 -m "Release version 2.1.0: hardened OAuth endpoints"

# Create a lightweight tag (for local temporary bookmarks)
git tag v1.0.0-temp

# Tag an older historical commit by supplying its commit hash
git tag -a v0.9.0 8a1e2f7 -m "Retrospective tag for beta launch"
```

---

### 2. Inspecting & Listing Tags
```bash
# List all tags in repository
git tag

# List tags matching a wildcard pattern
git tag -l "v1.2.*"

# Sort tags naturally by semantic version rather than alphabetical order!
git tag --sort=version:refname

# Inspect an annotated tag's metadata, tagger, and associated commit diff
git show v1.0.0

# Verify the cryptographic signature on a signed tag
git tag -v v2.1.0
```

---

### 3. Sharing & Deleting Tags
```bash
# Push a single specific tag to the remote server
git push origin v1.0.0

# Push ALL local tags to remote (USE WITH CAUTION - pushes local temporary tags!)
git push origin --tags

# Delete a tag locally
git tag -d v1.0.0

# Delete a tag on the remote server (GitHub)
git push origin --delete v1.0.0
```

---

## 5. Practical Examples

### Example 1: Full Release Tagging Workflow with Semantic Sorting
Configure natural version sorting so `v1.10.0` correctly sorts *after* `v1.2.0`:

```bash
mkdir /tmp/tags-lab && cd /tmp/tags-lab
git init -b main
echo "v1" > app.txt && git add app.txt && git commit -m "init"

# Create tags in various semantic orders
git tag -a v1.1.0 -m "Release 1.1.0"
git tag -a v1.2.0 -m "Release 1.2.0"
git tag -a v1.10.0 -m "Release 1.10.0"

# Lexicographical sort (alphabetical - INCORRECT! v1.10.0 appears before v1.2.0):
git tag
# Output: v1.1.0, v1.10.0, v1.2.0

# Semantic version sort (CORRECT!):
git tag --sort=version:refname
# Output: v1.1.0, v1.2.0, v1.10.0
```
Make this the global default:
```bash
git config --global tag.sort version:refname
```

### Example 2: Configuring Automated Tag Pushing (`push.followTags`)
Never forget to push release tags again:

```bash
# Enable followTags globally
git config --global push.followTags true

# Now, creating an annotated tag and pushing the branch pushes BOTH!
git tag -a v1.3.0 -m "Release version 1.3.0"
git push origin main
# Output confirms:
#  * [new tag]         v1.3.0 -> v1.3.0
```

### Example 3: Inspecting Raw Tag Object Architecture
Examine how Git serializes the annotated tag in `.git/objects/`:

```bash
# Get the hash of the tag object
TAG_OBJ_HASH=$(cat .git/refs/tags/v1.3.0)

# Check object type
git cat-file -t $TAG_OBJ_HASH
# Output: tag

# Inspect raw payload
git cat-file -p $TAG_OBJ_HASH
```
Output:
```text
object 4b825dc642cb6eb9a060e54bf8d69288fbee4904
type commit
tag v1.3.0
tagger Engineering Lead <lead@company.com> 1700000000 +0000

Release version 1.3.0
```

### Example 4: Deleting a Tag Locally and Remotely
Completely revoke an erroneous release tag:

```bash
# 1. Delete locally
git tag -d v1.3.0

# 2. Delete on remote server
git push origin --delete v1.3.0
# Output: - [deleted]         v1.3.0
```

### Example 5: Retrospective Tagging of Past Releases
Tag a commit from two weeks ago:

```bash
# Find commit hash from two weeks ago
HISTORICAL_HASH=$(git log --before="2 weeks ago" -n 1 --format="%H")

# Tag that commit retrospectively
git tag -a v0.8.0 $HISTORICAL_HASH -m "Release 0.8.0: legacy checkpoint"
git push origin v0.8.0
```

---

## 6. Best Practices

1. **Always Use Annotated Tags for Releases (`-a`)**: Never use lightweight tags for official releases or package versioning. Annotated tags contain immutable audit metadata (author, date, message).
2. **Standardize on SemVer 2.0.0**: Prefix tags with `v` (e.g. `v1.2.3`). Follow strict SemVer rules to enable automated dependency managers to calculate upgrade safety.
3. **Configure `push.followTags = true`**: Eliminates the common mistake of pushing branch commits while forgetting to push the associated release tags.
4. **Sign Release Tags in Production (`git tag -s`)**: In enterprise CI/CD and open-source distribution, sign tags using verified SSH or GPG keys to protect supply chain provenance.
5. **Never Move an Existing Published Tag**: Moving or re-pointing a tag that colleagues or customers have already fetched causes checksum discrepancies and broken builds. If a release is flawed, release a new patch version (`v1.0.1`).

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| Ran `git push` but tags did not appear on GitHub | `git push` pushes branches only by default. | Push tags explicitly: `git push origin <tag>` or enable `push.followTags = true`. |
| Tag sorting shows `v1.10.0` before `v1.2.0` | Default alphabetical string sort evaluates `1` before `2`. | Run `git tag --sort=version:refname`. |
| Attempted to commit while on a tag and commits disappeared | Checking out a tag (`git switch v1.0.0`) enters **Detached HEAD** state. | Create a branch to make changes: `git switch -c fix-v1 v1.0.0`. |
| `fatal: tag '...' already exists` | Attempted to create a tag with a name that is already in use. | Use a new version number, or delete the old tag first if unpushed. |
| Customer downloaded wrong code for a release | Developer re-pointed an existing tag to a new commit. | Never mutate published tags. Release a new SemVer version (`v1.0.1`). |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Zero Build-Time Overhead**: Resolving tags is an instant $O(1)$ reference lookup in `.git/refs/tags/` or `.git/packed-refs`.
- **Packfile Retention**: Tags act as reference roots. Any commit pointed to by a tag (and all its historical trees and blobs) is permanently preserved from garbage collection (`git gc`).

### Security
- **Supply-Chain Integrity**: Signed tags (`git tag -s`) guarantee that release tarballs and Docker container builds originate from verified developer keys, preventing man-in-the-middle code injection in CI pipelines.

---

## 9. Comparison Table

| Attribute | Lightweight Tag | Annotated Tag (`-a`) | Signed Tag (`-s`) |
| :--- | :--- | :--- | :--- |
| **Object Type** | Ref pointer only (`refs/tags/`) | Independent `tag` object | Independent `tag` object |
| **Contains Message?** | No | Yes | Yes |
| **Contains Author/Date?**| No | Yes | Yes |
| **Cryptographic Signature**| No | No | Yes (GPG / SSH) |
| **Pushed by `followTags`?**| No | Yes | Yes |
| **Recommended For** | Local temporary bookmarks | Production releases & SemVer | Enterprise compliance & security |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Explain the difference between a lightweight tag and an annotated tag in Git.</strong></summary>

- **Lightweight Tag**: Not an object in `.git/objects/`. It is simply a 41-byte reference pointer file in `.git/refs/tags/<name>` containing a commit hash. It contains no metadata, author, timestamp, or message. It is essentially a branch pointer that does not move.
- **Annotated Tag**: Creates an actual immutable `tag` object in the Git object store (`.git/objects/`) with its own unique SHA hash. It records the tagger name, email address, timestamp, tag message, and a pointer to the target commit object. It can also be cryptographically signed. Annotated tags are standard for public releases and production deployments.
</details>

<details>
<summary><strong>2. Why doesn't `git push` push tags by default, and how can you configure Git to push annotated tags automatically?</strong></summary>

Git treats branches (`refs/heads/*`) and tags (`refs/tags/*`) as separate reference namespaces. The default push refspec only pushes the currently checked-out branch to avoid accidentally publishing private local milestone tags to shared production remotes. To push annotated tags automatically whenever their associated commits are pushed, configure:
```bash
git config --global push.followTags true
```
This automatically pushes any annotated tags that point to commits reachable by the branch being pushed, while leaving lightweight and unmerged tags private.
</details>

<details>
<summary><strong>3. What is Semantic Versioning (SemVer 2.0.0) and how does it map to software releases?</strong></summary>

Semantic Versioning is a formal specification for release numbering using the format `MAJOR.MINOR.PATCH`:
- **MAJOR**: Incremented when introducing incompatible, breaking API changes.
- **MINOR**: Incremented when adding functionality in a backward-compatible manner.
- **PATCH**: Incremented when making backward-compatible bug fixes.
Pre-release identifiers (e.g. `-beta.1`, `-rc.2`) can be appended. SemVer allows automated dependency resolution tools (like npm or Bundler) to determine whether an update is safe to install automatically.
</details>

<details>
<summary><strong>4. How does sorting tags by `version:refname` solve alphabetical ordering issues?</strong></summary>

Standard alphabetical sorting evaluates characters lexicographically, causing `v1.10.0` to sort *before* `v1.2.0` because `'1'` is compared before `'2'`. The `version:refname` sort key parses embedded numeric components as natural integer versions, ensuring that `v1.2.0` sorts before `v1.10.0`.
</details>

<details>
<summary><strong>5. What happens to your working directory when you run `git switch <tagname>` or `git checkout <tagname>`?</strong></summary>

Because a tag points to a fixed commit rather than a named branch reference, checking out a tag places Git into a **Detached HEAD** state. `.git/HEAD` contains the raw commit hash directly. Any new commits created while in this state will become dangling unreferenced objects once you switch away to another branch unless saved into a new branch via `git switch -c <new-branch>`.
</details>

<details>
<summary><strong>6. How do you delete a tag that has already been pushed to GitHub?</strong></summary>

Deleting a remote tag requires two steps:
1. Delete the tag locally: `git tag -d <tagname>`
2. Delete the tag on the remote server: `git push origin --delete <tagname>` (or legacy syntax `git push origin :refs/tags/<tagname>`).
</details>

<details>
<summary><strong>7. Why is mutating or re-pointing an existing public tag considered an anti-pattern?</strong></summary>

Git clients cache tags permanently once fetched. If a maintainer changes where tag `v1.0.0` points on the remote server, existing developer clones and CI caches that already fetched `v1.0.0` will not update the tag by default, creating divergent codebases under the same version label. Furthermore, cryptographic hashes and package lockfiles (like `package-lock.json` or `go.sum`) will fail with integrity mismatch errors.
</details>

<details>
<summary><strong>8. What is the difference between a Git tag and a GitHub Release?</strong></summary>

A Git tag is a core, protocol-level Git reference pointing to a specific commit. A **GitHub Release** is a platform-level feature built on top of a Git tag. A GitHub Release adds markdown release notes, automated changelogs generated from pull requests, binary asset attachments (e.g. `.exe`, `.tar.gz`, `.deb`), and pre-release flags.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Creating and Inspecting an Annotated Tag</strong></summary>

**Task**: Initialize a repository, create a commit, create an annotated tag `v1.0.0`, and inspect the tag object using `git show`.

**Solution**:
```bash
mkdir /tmp/tag-lab && cd /tmp/tag-lab
git init -b main
echo "v1" > app.txt && git add app.txt && git commit -m "init"

# Create annotated tag
git tag -a v1.0.0 -m "Production release 1.0.0"

# Inspect tag
git show v1.0.0
# Shows tagger, date, message, commit hash, and diff
```
</details>

<details>
<summary><strong>Exercise 2: Comparing Lightweight vs. Annotated Tag Types</strong></summary>

**Task**: Create a lightweight tag `v1.0.0-light` on the same commit. Use `git cat-file -t` to prove that one is a `tag` object while the other is a `commit`.

**Solution**:
```bash
git tag v1.0.0-light

# Inspect object types
git cat-file -t v1.0.0
# Output: tag

git cat-file -t v1.0.0-light
# Output: commit (lightweight tag resolves directly to commit!)
```
</details>

<details>
<summary><strong>Exercise 3: Semantic Version Sorting</strong></summary>

**Task**: Create tags `v1.1.0`, `v1.2.0`, and `v1.10.0`. Run `git tag --sort=version:refname` to verify natural numerical ordering.

**Solution**:
```bash
git tag -a v1.1.0 -m "1.1.0"
git tag -a v1.2.0 -m "1.2.0"
git tag -a v1.10.0 -m "1.10.0"

git tag --sort=version:refname
# Correct order:
# v1.0.0
# v1.0.0-light
# v1.1.0
# v1.2.0
# v1.10.0
```
</details>

<details>
<summary><strong>Exercise 4: Retrospective Tagging of Past Commits</strong></summary>

**Task**: Create 3 commits. Tag the first commit as `v0.1.0` without checking it out.

**Solution**:
```bash
echo "c2" >> app.txt && git commit -am "c2"
echo "c3" >> app.txt && git commit -am "c3"

FIRST_COMMIT=$(git rev-list --max-parents=0 HEAD)
git tag -a v0.1.0 $FIRST_COMMIT -m "Initial alpha release"

git show v0.1.0 --oneline
# Confirms tag points to the root commit!
```
</details>

<details>
<summary><strong>Exercise 5: Deleting Tags Locally and Remotely</strong></summary>

**Task**: Create a bare remote hub, push a tag, delete it locally, and delete it from the remote hub.

**Solution**:
```bash
git init --bare /tmp/tag-hub.git
git remote add origin /tmp/tag-hub.git
git push origin main
git push origin v1.0.0

# Delete locally
git tag -d v1.0.0

# Delete on remote
git push origin --delete v1.0.0
```
</details>

<details>
<summary><strong>Exercise 6: Configuring Auto-Push Tags</strong></summary>

**Task**: Enable `push.followTags` in your repository config and verify the setting.

**Solution**:
```bash
git config --local push.followTags true

git config --local --get push.followTags
# Returns: true
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Command | Usage | Result |
| :--- | :--- | :--- |
| `git tag -a <tag> -m "msg"` | CLI | Creates an annotated tag object with metadata |
| `git tag -s <tag> -m "msg"` | CLI | Creates cryptographically signed annotated tag |
| `git tag <tag>` | CLI | Creates lightweight reference pointer |
| `git tag -a <tag> <hash>` | CLI | Tags a specific historical commit |
| `git tag --sort=version:refname`| CLI | Displays tags in natural semantic version order |
| `git show <tag>` | CLI | Displays tagger metadata, date, and commit diff |
| `git push origin <tag>` | Network | Pushes single tag to remote server |
| `git push origin --delete <tag>`| Network | Deletes tag on remote server |
| `git tag -d <tag>` | CLI | Deletes tag on local workstation |
| `git config --global push.followTags true`| Config | Auto-pushes reachable annotated tags on branch push |

---

## 13. Further Reading & Official Documentation

- [Official Git Documentation - git-tag](https://git-scm.com/docs/git-tag)
- [Semantic Versioning 2.0.0 Specification](https://semver.org/)
- [Pro Git Book - Tagging](https://git-scm.com/book/en/v2/Git-Basics-Tagging)
- [GitHub Docs - About Releases](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases)

---

[<- 11-stash.md](./11-stash.md) | [Back to Module Overview](./README.md) | [13-diff-patch-and-format-patch.md ->](./13-diff-patch-and-format-patch.md)
