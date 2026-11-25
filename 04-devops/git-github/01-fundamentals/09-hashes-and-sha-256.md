<!--
================================================================================
Metadata:
  Title: Cryptographic Hashing & The SHA-256 Transition
  Track: Git & GitHub / DevOps
  Difficulty: Advanced
  Estimated Time: 50 minutes
  Prerequisites: 08-the-index-in-depth.md
  Key Terminology: Cryptographic Hashing, SHA-1, SHA-256, SHAttered Attack, Collision Resistance, sha1dc, --object-format=sha256, Supply Chain Integrity
================================================================================
-->

# Cryptographic Hashing & The SHA-256 Transition

[<- 08-the-index-in-depth.md](./08-the-index-in-depth.md) | [Back to Module Overview](./README.md) | [10-git-internals-walkthrough.md ->](./10-git-internals-walkthrough.md)

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

In Git, cryptographic hash functions serve as the cornerstone of the entire architecture. A hash function takes an arbitrary stream of input bytes and transforms it into a fixed-length hexadecimal digest.

Git relies on cryptographic hashing for three foundational requirements:
1. **Content Addressing**: The hash acts as the primary key in the `.git/objects/` key-value store.
2. **Data Integrity**: Any silent bit rot, network transmission packet loss, or storage hardware corruption alters the hash, causing immediate verification failure.
3. **Cryptographic Tamper-Evidence**: Because each commit incorporates the hash of its parent commit, the entire repository history forms a Merkle tree. Modifying past history requires recalculating every downstream hash.

For over a decade, Git utilized **SHA-1** (producing 160-bit, 40-character hexadecimal digests). However, cryptographic advances and the successful demonstration of practical SHA-1 collisions in 2017 catalyzed Git's architectural transition to **SHA-256** (256-bit, 64-character hexadecimal digests). Understanding this transition is crucial for software security engineers and enterprise architects.

---

## 2. Core Concepts

### Properties of Cryptographic Hash Functions
A secure hash function must guarantee:
- **Pre-image Resistance (One-Way)**: Given a hash $H$, it must be computationally infeasible to determine the original input $M$ such that $hash(M) = H$.
- **Second Pre-image Resistance**: Given an input $M_1$, it must be computationally infeasible to find a different input $M_2$ such that $hash(M_1) = hash(M_2)$.
- **Collision Resistance**: It must be computationally infeasible to find *any* two arbitrary, different inputs $M_1$ and $M_2$ that produce the identical hash $hash(M_1) = hash(M_2)$.

### The 2017 "SHAttered" Attack
In February 2017, researchers from CWI Amsterdam and Google announced the **SHAttered** attack: the first practical collision generation for SHA-1. They produced two distinct PDF documents that generated the exact same SHA-1 hash (`38762cf7f55934b34d179ae6a4c80cadccbb7f0a`), requiring approximately 9 quintillion SHA-1 computations.

```
File A (Good PDF) ───┐
                     ├─── SHA-1 Calculation ───► 38762cf7f55934b34d179ae6a4c80cadccbb7f0a
File B (Evil PDF) ───┘
```

**Impact on Git**: If an attacker could engineer two colliding blobs, they could submit a benign pull request containing File A, and later swap it on the server with malicious File B without changing the commit hash!

### Git's Defense: `sha1dc` and SHA-256
Git responded with a two-phase defense:
1. **Immediate Defense (`sha1dc`)**: Integrated Marc Stevens' **SHA-1 Collision Detection** library directly into Git core. It detects mathematical signatures of known collision attacks during hashing and aborts the operation.
2. **Permanent Solution (SHA-256)**: Added native support for SHA-256 repositories via `--object-format=sha256`.

---

## 3. How It Works Internally

### SHA-1 vs. SHA-256 Object Representation
In a SHA-1 repository, objects are identified by 20 bytes (40 hexadecimal characters). In a SHA-256 repository, objects are identified by 32 bytes (64 hexadecimal characters).

```
SHA-1 Object ID (160 bits / 40 hex digits):
4b825dc642cb6eb9a060e54bf8d69288fbee4904

SHA-256 Object ID (256 bits / 64 hex digits):
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
```

### The Transition Plan: Object Format Interoperability
Git core maintains an interoperability roadmap allowing developers to clone, push, and fetch between SHA-1 and SHA-256 repositories:
- **Loose Mapping Table**: Git stores an internal translation table mapping SHA-1 hashes to SHA-256 hashes for all objects.
- **Protocol Negotiation**: Git's wire protocol negotiates the hash algorithm between client and server during connection initiation.

```
Client (SHA-256 Repo)                          Server (SHA-1 Legacy Forge)
      │                                                     │
      │─── 1. Connect & Advertise Capabilities ────────────>│
      │<── 2. Server declares SHA-1 support only ───────────│
      │                                                     │
      │─── 3. Client translates SHA-256 commits to SHA-1 ──>│
      │       (Uses internal Loose Object Hash Map)         │
      │─── 4. Stream SHA-1 Packfile ───────────────────────>│
```

---

## 4. Syntax & Usage

### Initializing and Working with SHA-256 Repositories

```bash
# Initialize a new repository using the SHA-256 object format
git init --object-format=sha256 my-secure-repo

# Verify the object format configured for the repository
git rev-parse --show-object-format

# Inspect the 64-character hash of HEAD
git rev-parse HEAD

# Query SHA-256 object details
git cat-file -p <64-char-hash>
```

---

## 5. Practical Examples

### Example 1: Creating a Real SHA-256 Repository
Walk through initializing and committing in a SHA-256 repository:

```bash
mkdir /tmp/sha256-lab && cd /tmp/sha256-lab

# Initialize SHA-256 repo
git init --object-format=sha256 -b main

# Verify object format
git rev-parse --show-object-format
# Output: sha256

# Create first commit
echo "Supply Chain Hardened" > audit.log
git add audit.log && git commit -m "feat: initial hardened commit"

# Inspect commit hash length (should be 64 characters)
COMMIT_HASH=$(git rev-parse HEAD)
echo "Commit Hash: $COMMIT_HASH"
echo "Hash Length: ${#COMMIT_HASH} characters"
# Output: Hash Length: 64 characters!
```

### Example 2: Inspecting `.git/config` for Object Format
Examine how Git records the hash format on disk:

```bash
cat .git/config
```
Output:
```ini
[core]
    repositoryformatversion = 1
    filemode = true
    bare = false
    logallrefupdates = true
[extensions]
    objectFormat = sha256
```
*Notice that `repositoryformatversion` is set to `1` and `extensions.objectFormat` is declared.*

### Example 3: Comparing Hash Output for Identical Content
Demonstrate the mathematical divergence between SHA-1 and SHA-256 for the same file:

```bash
CONTENT="Production Artifact Payload"$'\n'

# SHA-1 hash computation (Format: "blob 28\0...")
printf "blob 28\0$CONTENT" | sha1sum | awk '{print $1}'
# Output: 40-character hash (e.g. 5d8e7a...)

# SHA-256 hash computation
printf "blob 28\0$CONTENT" | sha256sum | awk '{print $1}'
# Output: 64-character hash (e.g. b2f1e4...)
```

### Example 4: Testing Git's Collision Detection Mechanism
Verify that modern Git has collision detection enabled:

```bash
# Check if sha1dc is compiled into your Git binary
git version --build-options | grep "sha1"
# Output shows collision detection library active
```

---

## 6. Best Practices

1. **Adopt SHA-256 for New Internal Infrastructure Repositories**: For internal security-critical systems (firmware, cryptography tools, defense software), initialize repositories with `git init --object-format=sha256`.
2. **Verify Forge Support Before Migrating Shared Repos**: While Git CLI natively supports SHA-256, verify whether your cloud forge (GitHub, GitLab, internal Gitea) fully supports pushing and hosting SHA-256 repositories before migrating production team codebases.
3. **Combine SHA-256 with Cryptographic Commit Signing**: A secure hash algorithm guarantees data integrity, but **not** author authenticity. Always sign commits (`commit.gpgsign = true`) using SSH or GPG.
4. **Never Rely on Truncated Hashes in Security Policies**: When referencing commits in security advisories or audit records, always record the full 40-char (SHA-1) or 64-char (SHA-256) hash, never the 7-character short hash.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `fatal: unknown object format 'sha256'` | Installed Git version is older than Git 2.29. | Upgrade Git to version 2.40 or higher. |
| GitHub rejects push of SHA-256 repository | Cloud forge does not currently support SHA-256 pushes for this repository tier. | Check GitHub's current SHA-256 public beta status or host on an enterprise server supporting SHA-256. |
| Cannot fetch from SHA-256 remote into SHA-1 local repo | Mismatched object formats without cross-format translation configured. | Clone as a fresh SHA-256 repository or configure interoperability mapping. |
| `error: object ... contains collision` | `sha1dc` detected an attempted mathematical collision attack in incoming object. | Reject the commit payload immediately; do not bypass collision detection. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Hash Computation Speed**: On modern 64-bit CPUs with hardware SHA extensions (Intel SHA-NI or ARMv8 Crypto Extensions), SHA-256 executes at **several gigabytes per second**, resulting in virtually identical performance to SHA-1 during daily commits and checkouts.
- **Index and Packfile Size**: A SHA-256 hash requires 32 bytes of storage per object compared to 20 bytes for SHA-1. In a repository with 1,000,000 objects, the raw hashes consume ~32MB versus ~20MB—an imperceptible difference on modern storage media.

### Security
- **Collision Immunity**: SHA-256 has a 256-bit keyspace ($2^{256} \approx 10^{77}$ possible combinations), providing complete mathematical immunity against brute-force collision attacks for the foreseeable future.

---

## 9. Comparison Table

| Feature / Metric | SHA-1 (Legacy Default) | SHA-256 (Modern Standard) |
| :--- | :--- | :--- |
| **Digest Bit Length** | 160 bits | 256 bits |
| **Hexadecimal Length** | 40 characters | 64 characters |
| **Collision Status** | Practically broken (SHAttered, 2017)| Cryptographically secure |
| **Git Flag** | `--object-format=sha1` (default)| `--object-format=sha256` |
| **Git Support Since** | Git 1.0 (2005) | Experimental 2.29+, Stable 2.45+ |
| **Hardware Acceleration**| ARM / x86 AES-NI / SHA-NI | Intel SHA Extensions / ARMv8 Crypto |
| **Recommended For** | Legacy open-source compatibility | New security-critical & enterprise repos|

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Why did Git historically use SHA-1 and what vulnerability forced the transition to SHA-256?</strong></summary>

Git adopted SHA-1 in 2005 primarily as a fast, reliable integrity checksum to detect bit rot and provide content-addressable indexing. Linus Torvalds noted that Git did not originally rely on SHA-1 for security against malicious cryptographic adversaries. However, in 2017, the SHAttered attack demonstrated the first practical collision for SHA-1, creating two different PDF files with identical SHA-1 hashes. This proved that an attacker could generate colliding Git objects, potentially swapping benign code with malicious backdoors without changing the commit hash, forcing Git to transition to SHA-256.
</details>

<details>
<summary><strong>2. How does Git protect existing SHA-1 repositories today without requiring immediate conversion to SHA-256?</strong></summary>

Git integrated `sha1dc` (SHA-1 Collision Detection library by Marc Stevens) into core Git. Whenever Git hashes an object or receives an object during push/fetch, `sha1dc` analyzes the computation for mathematical signatures and differential cryptanalysis patterns indicative of a collision attack. If a collision attempt is detected, Git halts execution and throws an error, preventing malicious collisions from entering the repository.
</details>

<details>
<summary><strong>3. How do you initialize a repository using SHA-256 in modern Git?</strong></summary>

By passing the `--object-format` flag during repository initialization:
```bash
git init --object-format=sha256 <repo-name>
```
To verify the active object format:
```bash
git rev-parse --show-object-format
```
</details>

<details>
<summary><strong>4. How long is a SHA-256 commit hash in hexadecimal compared to a SHA-1 hash?</strong></summary>

- **SHA-1**: 160 bits, represented as a **40-character** hexadecimal string.
- **SHA-256**: 256 bits, represented as a **64-character** hexadecimal string.
</details>

<details>
<summary><strong>5. Does using SHA-256 eliminate the need for GPG or SSH commit signing?</strong></summary>

No. Cryptographic hashing guarantees **integrity** (that the content has not changed) and **tamper-evidence**. However, it does not guarantee **authenticity** (who actually authored the commit). Because anyone can generate a valid SHA-256 hash for any commit containing forged author headers, cryptographic signatures (GPG or SSH) remain strictly necessary to prove identity and non-repudiation.
</details>

<details>
<summary><strong>6. What is the role of `extensions.objectFormat` in `.git/config`?</strong></summary>

When a repository is initialized with a non-default object format (like SHA-256), Git sets `core.repositoryformatversion = 1` and adds `[extensions] objectFormat = sha256` to `.git/config`. Older versions of Git that do not recognize repository format version 1 or the `objectFormat` extension safely halt with an error, preventing repository corruption by legacy tooling.
</details>

<details>
<summary><strong>7. How does the Git transition plan handle interoperability between SHA-1 and SHA-256 repositories?</strong></summary>

Git's transition design specifies an internal loose mapping table that records pairwise equivalents (`<sha1-hash> <sha256-hash>`) for all objects in the repository. When interacting with a SHA-1 remote, a SHA-256 client converts commit, tree, and blob references to their SHA-1 equivalents on-the-fly during network negotiation, enabling seamless inter-forge collaboration.
</details>

<details>
<summary><strong>8. What is the impact of SHA-256 on Git's content-addressable storage structure?</strong></summary>

The underlying storage mechanism in `.git/objects/` remains functionally identical. Objects are stored under 2-character subdirectories (`.git/objects/xx/`), followed by the remaining 62 characters of the hash as the filename. The only difference is that object keys are 64 characters instead of 40 characters.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Initializing and Verifying a SHA-256 Repository</strong></summary>

**Task**: Initialize a SHA-256 repository, create a commit, and inspect the length of the commit hash using `git rev-parse`.

**Solution**:
```bash
mkdir /tmp/sha256-exercise && cd /tmp/sha256-exercise
git init --object-format=sha256 -b main
echo "Security Baseline" > README.md
git add README.md && git commit -m "Initial commit"

HASH=$(git rev-parse HEAD)
echo "Hash: $HASH"
echo "Length: ${#HASH}"
# Confirms 64 characters
```
</details>

<details>
<summary><strong>Exercise 2: Querying Object Format via Plumbing</strong></summary>

**Task**: Use `git rev-parse --show-object-format` to programmatically verify whether a repository is using `sha1` or `sha256`.

**Solution**:
```bash
FORMAT=$(git rev-parse --show-object-format)
echo "Active format: $FORMAT"
# Returns sha256
```
</details>

<details>
<summary><strong>Exercise 3: Inspecting `.git/config` Extensions</strong></summary>

**Task**: Inspect `.git/config` in your test repository and verify that `extensions.objectFormat` is present.

**Solution**:
```bash
git config --local --get extensions.objectFormat
# Returns: sha256
```
</details>

<details>
<summary><strong>Exercise 4: Computing SHA-256 Blob Hash Manually</strong></summary>

**Task**: Use `printf` and `sha256sum` to compute the expected SHA-256 hash of a file manually, and verify that it matches `git hash-object`.

**Solution**:
```bash
echo -n "Test" > test.txt
GIT_HASH=$(git hash-object test.txt)

# Manual calculation (Format: "blob 4\0Test")
MANUAL_HASH=$(printf "blob 4\0Test" | sha256sum | awk '{print $1}')

echo "Git:    $GIT_HASH"
echo "Manual: $MANUAL_HASH"
# Both match exactly!
```
</details>

<details>
<summary><strong>Exercise 5: Comparing Object Directory Structures</strong></summary>

**Task**: Inspect `.git/objects/` to confirm that the first 2 characters form the directory name and the remaining 62 form the filename.

**Solution**:
```bash
PREFIX=${GIT_HASH:0:2}
POSTFIX=${GIT_HASH:2}

ls -l .git/objects/$PREFIX/$POSTFIX
# Confirms object exists at the 62-character filename!
```
</details>

<details>
<summary><strong>Exercise 6: Testing Format Rejection on Legacy Clones</strong></summary>

**Task**: Inspect what happens when you query repository format version in a SHA-256 repository.

**Solution**:
```bash
git config --local --get core.repositoryformatversion
# Returns: 1
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Command / Concept | Usage / Syntax | Role |
| :--- | :--- | :--- |
| **Init SHA-256 Repo** | `git init --object-format=sha256` | Initializes repository using 256-bit hashes |
| **Check Object Format** | `git rev-parse --show-object-format` | Returns `sha1` or `sha256` |
| **Full Commit Hash** | `git rev-parse HEAD` | Returns 64-char hash in SHA-256 repo |
| **Collision Defense** | `sha1dc` | Embedded library detecting collision attacks in SHA-1 |
| **Format Version** | `core.repositoryformatversion = 1` | Signals enhanced repo capabilities to Git core |
| **Extension Key** | `extensions.objectFormat = sha256` | Declares SHA-256 in `.git/config` |

---

## 13. Further Reading & Official Documentation

- [Git Documentation - Git Object Format Transition Plan](https://git-scm.com/docs/git-object-format)
- [SHAttered: First Practical Collision for Single-Block SHA-1](https://shattered.io/)
- [GitHub Blog - Support for SHA-256 Repositories](https://github.blog/2024-04-15-work-with-sha-256-repositories-in-github/)
- [Marc Stevens - SHA-1 Collision Detection (sha1dc)](https://github.com/cr-marcstevens/sha1collisiondetection)

---

[<- 08-the-index-in-depth.md](./08-the-index-in-depth.md) | [Back to Module Overview](./README.md) | [10-git-internals-walkthrough.md ->](./10-git-internals-walkthrough.md)
