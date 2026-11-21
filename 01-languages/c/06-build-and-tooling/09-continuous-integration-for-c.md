# Continuous Integration for C Projects

> Master automated quality assurance pipelines in C: GitHub Actions and GitLab CI, multi-compiler test matrices (GCC & Clang), build caching, automated sanitizer testing (ASan, UBSan, TSan), code coverage reporting, and automated release artifacts.

| Metadata | Details |
| :--- | :--- |
| **Level** | Tooling / Advanced |
| **Prerequisites** | [02-cmake.md](./02-cmake.md), [03-static-analysis-clang-tidy-cppcheck.md](./03-static-analysis-clang-tidy-cppcheck.md), [05-gcov-coverage.md](./05-gcov-coverage.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| GitHub Actions / GitLab CI / POSIX.1-2008 / Multi-Architecture Containers |

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
"It compiles on my machine" is the most dangerous phrase in C software engineering. Code that compiles cleanly with GCC on an Ubuntu laptop might crash on Clang on macOS, trigger undefined behavior on 32-bit ARM, or fail to build under MSVC on Windows due to subtle compiler extension differences and header variations.

**Continuous Integration (CI)** automates the verification process: on every single Git commit or pull request, cloud runner virtual machines or containers automatically clone the repository, compile the codebase across a matrix of compilers and operating systems, run unit tests under dynamic runtime sanitizers, execute static analysis, and verify formatting. Any regression is detected immediately before code merges into the main branch.

---

## 2. Core Concepts
- **The CI Matrix**: Testing multiple dimensions of variation simultaneously:
  - Compilers: GCC (versions 9, 11, 13), Clang (versions 12, 14, 16).
  - Operating Systems: Ubuntu Linux, macOS, Windows Server.
  - Build Types: Debug, Release, Sanitizer profiles.
- **Sanitizer Matrix**: Running the test suite under AddressSanitizer (ASan), UndefinedBehaviorSanitizer (UBSan), and ThreadSanitizer (TSan) on every commit.
- **Build Caching**: Storing package dependencies (`ccache`, third-party submodules) between CI runs to reduce build times from 10 minutes to 30 seconds.
- **Status Badges & Gating**: Visual badges on repository READMEs indicating build status; pull request protection rules blocking merges if any matrix job fails.
- **Artifact Publishing**: Automated compilation and upload of release binaries and tarballs when a new Git release tag is pushed.

---

## 3. How It Works Internally

### GitHub Actions Matrix Pipeline Architecture
```
Developer pushes Git Commit -> GitHub Webhook Triggers Actions Workflow
                                       |
                  +--------------------+--------------------+
                  | Matrix Job Dispatcher (Runs in Parallel)|
                  +--------------------+--------------------+
                                       |
     +---------------------------------+---------------------------------+
     |                                 |                                 |
[Job 1: Ubuntu + GCC 11]     [Job 2: Ubuntu + Clang + ASan]    [Job 3: macOS + Apple Clang]
  - Spin up VM runner          - Spin up VM runner               - Spin up VM runner
  - Install dependencies       - Compile with ASan flags         - Compile native build
  - cmake -B build             - Run CTest under ASan            - Run CTest
  - ctest                      - Check memory leaks              - Verify Darwin ABI
     |                                 |                                 |
     +---------------------------------+---------------------------------+
                                       |
                          Consolidated Status Report
                          (All jobs PASS -> Green Checkmark)
```

---

## 4. Syntax & Usage

### Production GitHub Actions Workflow (`.github/workflows/ci.yml`)
```yaml
name: Continuous Integration

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  build-and-test:
    name: ${{ matrix.os }} / ${{ matrix.compiler }} / ${{ matrix.build_type }}
    runs-on: ${{ matrix.os }}

    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest]
        compiler: [gcc-11, clang-14]
        build_type: [Debug, Release]
        include:
          # Add a dedicated AddressSanitizer job
          - os: ubuntu-latest
            compiler: clang-14
            build_type: Debug
            enable_asan: ON
          # Add macOS build
          - os: macos-latest
            compiler: default
            build_type: Release

    steps:
      - name: Checkout Repository
        uses: actions/checkout@v3
        with:
          submodules: recursive

      - name: Install Dependencies (Linux)
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y ninja-build valgrind

      - name: Configure CMake
        run: |
          cmake -B build -G Ninja             -DCMAKE_BUILD_TYPE=${{ matrix.build_type }}             -DENABLE_ASAN=${{ matrix.enable_asan || 'OFF' }}

      - name: Compile Project
        run: cmake --build build -j$(nproc)

      - name: Execute Test Suite
        run: ctest --test-dir build --output-on-failure
```

---

## 5. Practical Examples

### Example 1: CI Pipeline Matrix Evaluation
```c
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    const char *compiler;
    const char *sanitizer;
    const char *build_type;
    bool passed;
} CITestJob;

int main(void) {
    printf("Evaluating Multi-Compiler Continuous Integration Matrix...\n");
    CITestJob matrix[] = {
        { "gcc-11",   "none",    "Release", true },
        { "gcc-11",   "asan",    "Debug",   true },
        { "clang-14", "none",    "Release", true },
        { "clang-14", "ubsan",   "Debug",   true },
        { "clang-14", "tsan",    "Debug",   true }
    };

    int total = sizeof(matrix) / sizeof(matrix[0]);
    int passes = 0;
    for (int i = 0; i < total; i++) {
        printf("  Job [%s | %s | %s] -> %s\n",
               matrix[i].compiler, matrix[i].sanitizer, matrix[i].build_type,
               matrix[i].passed ? "SUCCESS" : "FAILED");
        if (matrix[i].passed) passes++;
    }
    printf("CI Pipeline Summary: %d/%d jobs passed. Zero regressions.\n", passes, total);
    return 0;
}
```

```text
Evaluating Multi-Compiler Continuous Integration Matrix...
  Job [gcc-11 | none | Release] -> SUCCESS
  Job [gcc-11 | asan | Debug] -> SUCCESS
  Job [clang-14 | none | Release] -> SUCCESS
  Job [clang-14 | ubsan | Debug] -> SUCCESS
  Job [clang-14 | tsan | Debug] -> SUCCESS
CI Pipeline Summary: 5/5 jobs passed. Zero regressions.
```

### Example 2: Compiler Cache (`ccache`) Acceleration in CI
Recompiling identical C files on every CI run wastes minutes of cloud runner compute time. Integrating `ccache` caches object files across runs:

```yaml
- name: Setup ccache
  uses: hendrikmuhs/ccache-action@v1.2
  with:
    key: ${{ matrix.os }}-${{ matrix.compiler }}

- name: Configure CMake with ccache
  run: |
    cmake -B build -G Ninja       -DCMAKE_C_COMPILER_LAUNCHER=ccache
```

---

## 6. Best Practices
1. **Always Set `fail-fast: false` on Matrix Builds**: By default, if one matrix job fails (e.g. GCC Debug), GitHub Actions cancels all other running jobs. Setting `fail-fast: false` ensures all jobs complete, revealing whether the failure is compiler-specific or universal.
2. **Always Run Sanitizers in CI**: Unit tests may pass natively while having memory corruptions that remain silent until production. Compiling with `-fsanitize=address,undefined` in CI catches leaks and out-of-bounds errors instantly.
3. **Use `--output-on-failure` with CTest**: By default, `ctest` prints only "Test #1: Failed". Adding `--output-on-failure` prints the exact standard output and failure assertion in the CI log.
4. **Cache Dependencies**: Use caching actions (`actions/cache`) for third-party build tools and `ccache` to keep CI run times under 2 minutes.
5. **Enforce Clean Git Working Tree After Build**: Add a step `git diff --exit-code` after running code generation or formatting scripts to ensure developers did not forget to commit generated files.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Testing only on developer's native compiler | Code passes GCC on Linux but fails on Clang on macOS due to non-standard extensions | Configure multi-compiler CI matrix |
| Forgetting `fail-fast: false` | Cancels other jobs on first failure, hiding whether other platforms also broke | Set `fail-fast: false` in matrix strategy |
| Silent CTest failures | CI log only shows "Test failed", requiring local reproduction to diagnose | Pass `ctest --output-on-failure` |
| Running ASan and TSan in the same build | AddressSanitizer and ThreadSanitizer are fundamentally incompatible in the same binary | Split ASan and TSan into separate matrix jobs |
| Missing submodule checkout in CI | Repositories using submodules fail to compile due to empty `vendor/` directories | Set `with: submodules: recursive` in checkout |
| Hardcoding `/nproc` on macOS runners | `nproc` command does not exist on macOS (uses `sysctl -n hw.ncpu`) | Use CMake's built-in parallel flag: `cmake --build build --parallel` |

---

## 8. Performance & Security Notes
- **CI Secret Hygiene**: Never print environment variables or secrets into CI terminal logs. Use GitHub Actions encrypted secrets (`${{ secrets.API_TOKEN }}`) and audit external third-party actions to prevent supply chain token exfiltration.
- **Reproducible Artifact Builds**: For production releases, build binaries inside standardized OCI/Docker container images with pinned compiler versions to ensure cryptographic bit-for-bit reproducibility.

---

## 9. Comparison / When to Use What

| CI Platform | Configuration Format | Free Tier Features | Best Use Case |
| :--- | :--- | :--- | :--- |
| **GitHub Actions** | YAML (`.github/workflows/`) | Generous public runner minutes, macOS/Windows | Open-source GitHub projects |
| **GitLab CI** | YAML (`.gitlab-ci.yml`) | Powerful self-hosted runners, native Docker | Self-hosted enterprise infrastructure |
| **SourceHut / Cirrus**| Minimal YAML | Native BSD (FreeBSD, OpenBSD) runners | Cross-platform Unix system daemons |

---

## 10. Interview Questions

<details>
<summary>1. Why should a C project test against both GCC and Clang in CI?</summary>
GCC and Clang implement different optimization algorithms, warning engines, and standard compliance checks. Code that compiles without warnings on GCC may trigger strict warnings on Clang (e.g. strict prototype checks or sign conversions). Testing both compilers ensures the codebase adheres strictly to ISO C standards rather than relying on compiler-specific extensions or quirks.
</details>

<details>
<summary>2. Why are AddressSanitizer (ASan) and ThreadSanitizer (TSan) not combined in a single build?</summary>
ASan and TSan use conflicting memory interception models: ASan replaces memory allocators and instruments every memory load/store with shadow memory redzone checks to catch buffer overflows. TSan instruments memory operations with shadow memory state tracking to detect concurrent data races. Combining both tools in a single executable causes compiler conflicts, excessive memory overhead, and unpredictable crashes. They must run in separate test jobs.
</details>

<details>
<summary>3. What is the benefit of `ccache` in a CI environment?</summary>
`ccache` computes a cryptographic hash of each C source file and its included preprocessed headers. If the file has not changed since the last build, `ccache` retrieves the compiled object file directly from cache rather than invoking the compiler. In CI pipelines with persistent caching, this reduces compilation times by up to $80\%$.
</details>

<details>
<summary>4. What does the `fail-fast: false` setting in GitHub Actions accomplish?</summary>
By default (`fail-fast: true`), if any single job in a multi-platform matrix fails (e.g. Ubuntu Clang Debug), GitHub Actions immediately cancels all other currently running matrix jobs (e.g. macOS and Windows). Setting `fail-fast: false` allows all matrix jobs to run to completion, providing developers with a complete diagnostic view across all platforms.
</details>

<details>
<summary>5. How can CI verify that a pull request does not introduce memory leaks?</summary>
By running the unit test suite under Valgrind Memcheck (`valgrind --error-exitcode=1 --leak-check=full ./tests`) or AddressSanitizer with LeakSanitizer enabled (`ASAN_OPTIONS=detect_leaks=1`). If any memory is leaked, the test runner exits with code 1, failing the CI build.
</details>

<details>
<summary>6. How do you trigger a CI job only when a new release tag is pushed?</summary>
In GitHub Actions:
```yaml
on:
  push:
    tags:
      - 'v*'
```
</details>

<details>
<summary>7. What is the purpose of `cmake --build build --parallel`?</summary>
It automatically queries the host operating system to determine the number of available CPU cores and spawns matching parallel compile jobs portably across Linux, macOS, and Windows, avoiding platform-specific flags like `$(nproc)`.
</details>

<details>
<summary>8. How do you upload compiled binaries as downloadable release artifacts in GitHub Actions?</summary>
Using the official `actions/upload-artifact` action in pull requests, or `softprops/action-gh-release` when publishing tagged releases.
</details>

---

## 11. Practice Exercises

### Easy
1. **Minimal Workflow Syntax**: Write a minimal GitHub Actions YAML file that runs `gcc main.c -o app` on `ubuntu-latest`.
2. **Matrix Declaration**: Write a GitHub Actions strategy block testing across `gcc` and `clang`.
3. **Status Check Command**: Write a shell command that checks if a Git repository has unstaged changes and exits with code 1 if dirty.

<details>
<summary>Solutions (Easy)</summary>

```yaml
# Exercise 1 & 2
name: Simple Build
on: [push]
jobs:
  build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        compiler: [gcc, clang]
    steps:
      - uses: actions/checkout@v3
      - name: Build
        run: ${{ matrix.compiler }} main.c -o app
```
</details>

### Medium
4. **Complete CMake CTest Workflow**: Author a GitHub Actions workflow that configures CMake, builds with Ninja, and runs CTest with `--output-on-failure`.
5. **ASan CI Job Configuration**: Write a dedicated CI job that compiles with `-DENABLE_ASAN=ON` and runs tests with `ASAN_OPTIONS=detect_leaks=1`.
6. **GitLab CI Equivalent**: Translate a GitHub Actions C build job into a `.gitlab-ci.yml` pipeline.

<details>
<summary>Solutions (Medium)</summary>

```yaml
# Exercise 6: .gitlab-ci.yml
stages:
  - test

test_c_project:
  stage: test
  image: gcc:11
  before_script:
    - apt-get update && apt-get install -y cmake ninja-build
  script:
    - cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
    - cmake --build build
    - ctest --test-dir build --output-on-failure
```
</details>

### Hard
7. **Comprehensive Multi-OS Multi-Compiler CI Matrix**: Author a complete `.github/workflows/ci.yml` matrix testing Linux (GCC 9, 11; Clang 12, 14), macOS (Clang), and Windows (MSVC), including ASan verification and ccache.
8. **Automated Release Tarball Generator**: Build a GitHub Actions release pipeline triggered by `v*` tags that compiles static release binaries, creates `.tar.gz` archives with SHA256 checksums, and attaches them to the GitHub Release.
9. **Coverage Badge Updater in CI**: Configure a CI pipeline that runs `lcov`, parses the line coverage percentage, and automatically updates a dynamic SVG coverage badge on the project README.

<details>
<summary>Solutions (Hard)</summary>

```yaml
# Exercise 8: Automated Release Packaging Workflow
name: Release
on:
  push:
    tags:
      - 'v*'

jobs:
  publish-release:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build Release Binary
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Release
          cmake --build build --config Release
      - name: Package Artifacts
        run: |
          mkdir -p package/bin
          cp build/my_app package/bin/
          tar -czf my_app-${{ github.ref_name }}-linux-x86_64.tar.gz -C package .
          sha256sum my_app-${{ github.ref_name }}-linux-x86_64.tar.gz > checksums.txt
      - name: Create GitHub Release
        uses: softprops/action-gh-release@v1
        with:
          files: |
            my_app-${{ github.ref_name }}-linux-x86_64.tar.gz
            checksums.txt
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Matrix**: Test GCC and Clang across Debug and Release profiles.
- **Fail-Fast**: Always set `fail-fast: false`.
- **CTest**: Run with `ctest --output-on-failure`.
- **Sanitizers**: Dedicate separate CI jobs for ASan/UBSan and TSan.
- **Submodules**: Use `with: submodules: recursive` in checkout.
- **Cache**: Use `ccache` to accelerate compilation times across runs.

---

## 13. Further Reading
- GitHub Actions Official Documentation: Building and Testing C/C++.
- GitLab CI/CD Documentation: Pipeline Architecture.
- Martin Fowler, *Continuous Integration* (Foundational Principles).

---

[← Previous Note: pkg-config and Dependencies](./08-pkg-config-and-dependencies.md) | [Module Index](README.md) | [Next Note: Doxygen Documentation](./10-doxygen-documentation.md)
