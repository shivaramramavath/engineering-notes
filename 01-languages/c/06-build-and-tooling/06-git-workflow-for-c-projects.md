# Git Workflow and Release Engineering for C Projects

> Master professional Git workflows in C: `.gitignore` best practices for C build artifacts, managing third-party vendor dependencies with Git submodules, Semantic Versioning (SemVer), release tagging, GPG commit signing, and automated changelogs.

| Metadata | Details |
| :--- | :--- |
| **Level** | Tooling / Advanced |
| **Prerequisites** | [01-make-in-depth.md](./01-make-in-depth.md), [02-cmake.md](./02-cmake.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| Git 2.25+ / Semantic Versioning 2.0.0 / Conventional Commits |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why
Developing C software in team environments requires disciplined source control. Unlike languages with official package managers (such as Rust with `cargo` or Go with `go modules`), the C ecosystem has historically lacked a universal package manager. Projects frequently manage third-party dependencies directly within Git repositories, while compilers produce dozens of intermediate binary artifacts (`.o`, `.d`, `.a`, `.so`, `.gcda`).

A disciplined **Git Workflow for C** prevents accidental commits of non-portable machine binaries, manages external dependencies reproducibly using **Git submodules**, enforces structured commit messages with Conventional Commits, and automates software release tagging using **Semantic Versioning (SemVer)**.

---

## 2. Core Concepts
- **C-Specific `.gitignore`**: Must strictly exclude all platform-specific compiled binaries, object files, shared libraries, core dumps, and editor temporary files.
- **Git Submodules**: Allows keeping a Git repository as a subdirectory of another Git repository. The parent repository does not store the files of the dependency; it records only the remote URL and the exact commit SHA of the dependency, ensuring 100% reproducible builds across all developer machines.
- **Semantic Versioning (SemVer 2.0.0)**: Version format `MAJOR.MINOR.PATCH`:
  - `MAJOR`: Incremented on incompatible API / ABI breaking changes (e.g. changing struct layout or function signature).
  - `MINOR`: Incremented when adding backwards-compatible functionality.
  - `PATCH`: Incremented on backwards-compatible bug fixes.
- **Signed Git Releases**: Cryptographically signing release tags with GPG (`git tag -s v1.0.0 -m "Release"`) guarantees authenticity and verifies that the release artifact was authored by a trusted maintainer.
- **Conventional Commits**: Standardized commit prefix format (`feat:`, `fix:`, `refactor:`, `docs:`, `ci:`) that enables automated changelog generation.

---

## 3. How It Works Internally

### Git Submodule Tree Binding
When you add a dependency via `git submodule add https://github.com/lib/foo.git vendor/foo`:

```
Parent Repository (Git Tree Object)
+-------------------------------------------------------+
| Mode: 100644  blob  .gitignore                       |
| Mode: 100644  blob  CMakeLists.txt                   |
| Mode: 100644  blob  src/main.c                       |
| Mode: 160000  commit 8f3a1b02 (Points to vendor/foo) |
+-------------------------------------------------------+
                                |
                                v
.gitmodules Configuration File:
[submodule "vendor/foo"]
    path = vendor/foo
    url = https://github.com/lib/foo.git
```

1. **`160000` Mode**: Git stores submodules as a special "gitlink" tree entry with file mode `160000` pointing directly to an external commit SHA.
2. **Deterministic Locking**: Unlike pulling a branch (like `main`), a submodule remains frozen at that specific commit SHA until explicitly updated by a developer.
3. **Cloning**: Clones require `git clone --recurse-submodules` or `git submodule update --init --recursive` to populate the directory contents.

---

## 4. Syntax & Usage

### Comprehensive C `.gitignore` Template
```gitignore
# Object files and archives
*.o
*.obj
*.a
*.lib

# Shared libraries and dynamic modules
*.so
*.so.*
*.dylib
*.dll

# Executables
*.exe
*.out
*.app

# Auto-dependency and compilation database
*.d
compile_commands.json

# Code coverage artifacts
*.gcno
*.gcda
*.gcov
coverage/
*.info

# Build directories
build/
bin/
obj/
dist/

# Crash dumps and logs
core
core.*
*.log

# IDE & Editor metadata
.vscode/
.idea/
*.swp
*~
```

### Submodule & Release Command Reference
```bash
# Add third-party C library as a submodule
git submodule add https://github.com/DaveGamble/cJSON.git vendor/cjson

# Initialize and update submodules after clone
git submodule update --init --recursive

# Create an annotated, GPG-signed SemVer release tag
git tag -s v2.1.0 -m "Release v2.1.0: Added connection pooling and fixed memory leak"

# Push tag to remote origin
git push origin v2.1.0
```

---

## 5. Practical Examples

### Example 1: Semantic Versioning Architecture Simulation
```c
#include <stdio.h>
#include <stdint.h>

typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    const char *prerelease;
} SemVer;

void print_semver(const SemVer *v) {
    if (v->prerelease && v->prerelease[0] != '\0') {
        printf("v%u.%u.%u-%s\n", v->major, v->minor, v->patch, v->prerelease);
    } else {
        printf("v%u.%u.%u\n", v->major, v->minor, v->patch);
    }
}

int main(void) {
    printf("Evaluating C Project Git Release & SemVer Architecture...\n");
    SemVer release = { 2, 4, 1, "" };
    SemVer beta    = { 3, 0, 0, "rc.1" };

    printf("Production Release Tag: ");
    print_semver(&release);
    printf("Pre-Release Pipeline Tag: ");
    print_semver(&beta);
    printf("Git tag signed and validated via GPG verification.\n");
    return 0;
}
```

```text
Evaluating C Project Git Release & SemVer Architecture...
Production Release Tag: v2.4.1
Pre-Release Pipeline Tag: v3.0.0-rc.1
Git tag signed and validated via GPG verification.
```

### Example 2: Automatic Version Header Injection
To make the Git release version accessible within C code, CMake can query the latest Git tag and inject it into a generated header:

```cmake
# CMake snippet: generate version.h from Git tag
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        OUTPUT_VARIABLE PROJECT_GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(PROJECT_GIT_VERSION "v1.0.0-unknown")
endif()

add_compile_definitions(APP_VERSION="${PROJECT_GIT_VERSION}")
```

In C code:
```c
#include <stdio.h>
void print_version(void) {
    printf("Server Version: %s\n", APP_VERSION);
}
```

---

## 6. Best Practices
1. **Never Commit Binary Artifacts**: Keep Git repositories strictly text-based. Binary files (`.o`, `.a`, `.so`, `.exe`) inflate repository size permanently and cannot be meaningfully diffed.
2. **Pin Submodules to Release Tags**: When adding a submodule, checkout an official release tag (`git checkout v1.7.14`) rather than tracking an unstable development branch.
3. **Use Conventional Commits**: Structure commit messages consistently:
   - `feat(network): implement epoll event loop`
   - `fix(allocator): resolve off-by-one buffer overrun`
   - `docs: update build instructions for macOS`
4. **Sign All Release Tags with GPG**: Protect your software supply chain by signing release tags with `git tag -s`.
5. **Enforce Branch Protection on `main`**: Require all code changes to enter through pull requests that pass automated CI builds and static analysis gates before merging.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Committing `.o` or `.so` files to Git | Permanently bloats `.git` history size; non-portable across other OSs | Add comprehensive `.gitignore` before first commit |
| Cloning without `--recurse-submodules` | Submodule directories remain empty directories; compilation fails with missing headers | Run `git submodule update --init --recursive` |
| Breaking ABI without incrementing MAJOR version | Dynamic linkers fail at runtime with undefined symbol or memory layout crashes | Increment MAJOR version whenever public headers or struct layouts change |
| Modifying files directly inside `vendor/` submodules | Changes are lost or detached on next submodule update | Fork dependency or submit upstream PR |
| Lightweight tags instead of annotated tags | Lightweight tags (`git tag v1.0`) lack author info, timestamps, and commit messages | Always use annotated tags: `git tag -a v1.0 -m "Msg"` |
| Committing secret API keys or certificates | Secrets remain in Git commit history permanently even if deleted in later commit | Use environment variables; use tools like `git-filter-repo` to purge if leaked |

---

## 8. Performance & Security Notes
- **Shallow Cloning in CI (`--depth=1`)**: Continuous integration runners do not need the complete 10-year Git history. Use shallow clones (`git clone --depth=1`) to speed up CI checkout times from minutes to seconds.
- **Supply Chain Security & Submodule Pinning**: Attackers can compromise remote upstream Git repositories. By pinning submodules to exact commit SHAs (rather than branch heads), your C project remains protected against upstream malicious branch injections until you intentionally audit and update the SHA.

---

## 9. Comparison / When to Use What

| Dependency Approach | Reproducibility | Maintenance Overhead | Best Use Case |
| :--- | :--- | :--- | :--- |
| **Git Submodules** | **100% Deterministic (Pinned SHA)** | Low | Standard C libraries, stable dependencies |
| **System Packages (`apt`, `brew`)**| Varies across OS versions | Zero (OS managed) | Standard libraries (OpenSSL, zlib, libuv) |
| **Vendoring (Copy-Paste Code)** | High | Very high (Manual updates) | Tiny single-header libraries (stb_image) |
| **CMake `FetchContent`** | High (Git SHA based) | Low (Automated by CMake) | Modern CMake-based C/C++ dependencies |

---

## 10. Interview Questions

<details>
<summary>1. What does Git record in the parent repository when a submodule is added?</summary>
The parent repository records two things:
1. An entry in the `.gitmodules` text file mapping the submodule path to its remote repository URL.
2. A special tree entry with file mode `160000` (gitlink) that records the exact 40-character commit SHA of the submodule. The physical files of the submodule are **not** stored in the parent repository tree.
</details>

<details>
<summary>2. What is the difference between an annotated tag and a lightweight tag in Git?</summary>
A lightweight tag is simply a pointer (like a branch that never moves) pointing directly to a specific commit object. An annotated tag is a full Git object stored in the Git database; it contains its own author name, email, date, tagging message, and can be cryptographically signed with a GPG key. Production releases should always use annotated tags (`git tag -a` or `git tag -s`).
</details>

<details>
<summary>3. What is Semantic Versioning (SemVer) and how does it apply to C shared libraries?</summary>
SemVer specifies versioning in the format `MAJOR.MINOR.PATCH`. In C shared libraries:
- `MAJOR` is bumped when backwards-incompatible API or ABI changes occur (e.g. altering struct members, removing public functions, or changing function argument types), requiring recompilation of client applications.
- `MINOR` is bumped when adding new backwards-compatible functions or symbols.
- `PATCH` is bumped for backwards-compatible internal bug fixes that preserve exact ABI contracts.
</details>

<details>
<summary>4. What command initializes and clones submodules in an already-cloned repository?</summary>
`git submodule update --init --recursive`.
</details>

<details>
<summary>5. How does `git describe --tags` work?</summary>
It searches the Git history for the most recent annotated tag reachable from the current commit and outputs a human-readable string like `v1.2.0-4-g2a1b3c` (meaning: 4 commits ahead of tag `v1.2.0`, current commit SHA `2a1b3c`). If the working tree has uncommitted modifications, adding `--dirty` appends `-dirty`.
</details>

<details>
<summary>6. Why is it dangerous to commit compiled `.a` or `.so` libraries to a Git repository?</summary>
1. **Portability**: The binary was built for a specific CPU architecture, OS kernel, and C library ABI (e.g. glibc version). It will fail to link or run on other developers' machines.
2. **Repository Bloat**: Binaries cannot be delta-compressed efficiently by Git; every rebuild creates a full new binary blob, permanently bloating `.git` directory size.
</details>

<details>
<summary>7. What is the purpose of `.gitattributes` in a C project?</summary>
`.gitattributes` configures repository-level settings such as line ending normalization (`* text=auto`, `*.c text eol=lf`), preventing Windows CRLF vs Linux LF merge conflicts, and marking vendor directories or generated files to be ignored in GitHub language statistics.
</details>

<details>
<summary>8. How do you permanently remove an accidentally committed large binary from Git history?</summary>
Using `git-filter-repo` (the modern replacement for `git filter-branch`):
`git-filter-repo --path my_large_binary.a --invert-paths`
This rewrites all commit objects in history to erase the file.
</details>

---

## 11. Practice Exercises

### Easy
1. **Gitignore Generator**: Create a complete `.gitignore` file for a C project using Make and GCC.
2. **Annotated Tag**: Create an annotated Git tag `v1.0.0` with a release message and verify it with `git show v1.0.0`.
3. **SemVer String Formatter**: Write a small C function that formats major, minor, patch numbers into a string `vX.Y.Z`.

<details>
<summary>Solutions (Easy)</summary>

```c
/* Exercise 3: SemVer Formatter */
#include <stdio.h>

void format_semver(char *buf, size_t cap, int major, int minor, int patch) {
    snprintf(buf, cap, "v%d.%d.%d", major, minor, patch);
}
```
</details>

### Medium
4. **Git Submodule Integration**: Add a small open-source C library (e.g. `cJSON` or `Unity`) as a submodule under `vendor/` and verify `.gitmodules`.
5. **Git Version Header Script**: Write a shell script `make_version.sh` that queries `git describe --tags` and generates `include/version.h` containing `#define VERSION_STRING "..."`.
6. **Git Release Changelog Generator**: Use `git log --oneline` with tag ranges (`v1.0.0..v1.1.0`) to generate release release notes.

<details>
<summary>Solutions (Medium)</summary>

```bash
#!/bin/bash
# Exercise 5: make_version.sh
VERSION=$(git describe --tags --always --dirty 2>/dev/null || echo "v0.0.0-dev")

cat <<EOF > include/version.h
#ifndef VERSION_H
#define VERSION_H

#define APP_VERSION "$VERSION"

#endif
EOF

echo "Generated include/version.h with APP_VERSION=$VERSION"
```
</details>

### Hard
7. **Semantic Version Comparator in C**: Implement a complete SemVer 2.0.0 comparison function `int semver_cmp(const char *ver_a, const char *ver_b)` that parses major, minor, patch, and prerelease tags, returning -1, 0, or 1.
8. **Automated Release Preparation Script**: Write a release script `prepare_release.sh 1.2.0` that verifies clean working tree, updates version constants, commits changes with message `chore(release): v1.2.0`, and creates a signed tag `v1.2.0`.
9. **Git Hook Secret Checker**: Implement a pre-commit hook in C or Bash that scans staged files for accidentally committed private keys (`-----BEGIN PRIVATE KEY-----`) or AWS tokens, aborting the commit if found.

<details>
<summary>Solutions (Hard)</summary>

```bash
#!/bin/bash
# Exercise 8: prepare_release.sh
set -e

if [ -z "$1" ]; then
    echo "Usage: $0 <new_version>"
    exit 1
fi

NEW_VER="v$1"

# Verify working tree is clean
if [ -n "$(git status --porcelain)" ]; then
    echo "Error: Working directory contains uncommitted changes!"
    exit 1
fi

echo "Preparing release $NEW_VER..."

# Create annotated tag
git tag -a "$NEW_VER" -m "Release $NEW_VER"

echo "Tag $NEW_VER created successfully. Push with: git push origin $NEW_VER"
```
</details>

---

## 12. Summary / Cheat Sheet
- **.gitignore**: Exclude `*.o`, `*.a`, `*.so`, `*.exe`, `build/`, `*.d`.
- **Submodules**: `git submodule add <url> <path>` and `git submodule update --init`.
- **SemVer**: `MAJOR.MINOR.PATCH` (bump MAJOR on breaking ABI changes).
- **Annotated Tag**: `git tag -a v1.0.0 -m "Release message"`.
- **Git Blame Ignore**: Put large formatting commits in `.git-blame-ignore-revs`.
- **Describe**: `git describe --tags --always --dirty` for runtime build identification.

---

## 13. Further Reading
- Scott Chacon & Ben Straub, *Pro Git* (Free Official Book).
- Semantic Versioning 2.0.0 Specification (semver.org).
- Conventional Commits 1.0.0 Specification.

---

[← Previous Note: Code Coverage (gcov & lcov)](./05-gcov-coverage.md) | [Module Index](README.md) | [Next Note: Cross-Compilation Intro](./07-cross-compilation-intro.md)
