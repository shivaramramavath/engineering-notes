---
title: "GitHub Actions Workflow Performance, Caching & Cost Optimization"
category: "04-devops"
subcategory: "git-github / 04-github-actions"
tags: ["github-actions", "performance", "caching", "cost-optimization", "concurrency", "billing"]
difficulty: "Advanced"
estimated_read_time: "26 min"
prerequisites:
  - "04-github-actions/04-jobs-and-steps"
  - "04-github-actions/09-artifacts-caching-dependencies"
  - "04-github-actions/10-matrix-and-concurrency"
last_updated: "2026-11-25"
---

# GitHub Actions Workflow Performance, Caching & Cost Optimization

> **Navigation**: [← Previous: 21-self-hosted-runners-and-scaling.md](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/21-self-hosted-runners-and-scaling.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/README.md) | [Next: 23-testing-workflows-locally.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/23-testing-workflows-locally.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How Caching Operates Internally](#3-how-caching-operates-internally)
4. [Syntax & Implementation](#4-syntax--implementation)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices for Speed and Cost](#6-best-practices-for-speed-and-cost)
7. [Common Mistakes](#7-common-mistakes)
8. [GitHub Actions Billing Architecture & Minute Multipliers](#8-github-actions-billing-architecture--minute-multipliers)
9. [Optimization Comparison Matrix](#9-optimization-comparison-matrix)
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is CI Performance & Cost Optimization?
Workflow performance and cost optimization is the systematic engineering effort to minimize pipeline execution duration (feedback loop latency) and eliminate wasted compute minutes on GitHub Actions runners through caching, intelligent job pruning, concurrency cancellation, and resource rightsizing.

### Why Does It Matter?
1. **Developer Velocity**: A build pipeline taking 25 minutes interrupts developer flow. Shrinking that pipeline to 3 minutes allows engineers to review, iterate, and deploy multiple times per hour without context switching.
2. **Direct Financial Impact**: GitHub Actions charges per-minute billing for private repositories once included monthly allowances are exhausted. Inefficient workflows with macOS runners (10x multiplier) or hanging jobs can generate thousands of dollars in surprise cloud bills.
3. **Queue Health**: Faster jobs clear the concurrent runner pool quickly, preventing pull request queues from backing up during peak office hours.

---

## 2. Core Concepts

### 1. The GitHub Cache Backend (`actions/cache@v4`)
GitHub provides up to **10 GB of cache storage per repository**. Caches are immutable snapshots keyed by an exact cryptographic hash (e.g. `npm-cache-${{ hashFiles('**/package-lock.json') }}`). Unused cache entries are automatically evicted after 7 days, or when total repository cache exceeds 10 GB (Least Recently Used eviction).

### 2. Built-in Setup Caching vs Raw `actions/cache`
Modern setup actions (`actions/setup-node`, `actions/setup-python`, `actions/setup-go`, `actions/setup-java`) include native caching flags (`cache: 'npm'`, `cache: 'pip'`, `cache: 'gradle'`). They handle path resolution, cache key hashing, and post-step save hooks automatically and are significantly more reliable than manual `actions/cache` blocks.

### 3. Concurrency Groups (`cancel-in-progress: true`)
When an engineer pushes three commits in 60 seconds to a pull request, three complete workflow runs are launched. Concurrency groups automatically abort previous in-flight runs, running only the latest commit.

### 4. Fail-Fast Matrix Strategy
When a matrix job runs across 10 configurations (e.g. 5 Node versions $\times$ 2 OSs), `fail-fast: true` (the default) automatically cancels all remaining 9 matrix jobs the instant any single job fails, saving up to 90% of wasted matrix compute.

---

## 3. How Caching Operates Internally

```
                   The Cache Key & Eviction Flow:

                   [ Workflow Starts: Runner VM ]
                                 │
                                 ▼
                     1. Calculate Primary Key:
             hashFiles('**/package-lock.json') = a1b2c3d
                                 │
                                 ▼
                 2. Search GitHub Actions Cache DB:
                Does key "npm-linux-a1b2c3d" exist?
                 ├── YES (CACHE HIT)
                 │     ├── Download compressed tarball from Azure Blob
                 │     └── Extract to ~/.npm or node_modules/
                 │     └── Skip download step!
                 └── NO (CACHE MISS)
                       ├── Check restore-keys: "npm-linux-"
                       │     ├── Partial match found? Extract older cache
                       │     └── No match? Start clean
                       ├── Run package manager download
                       └── (Post-Job Hook): Upload new tarball to Cache DB
```

---

## 4. Syntax & Implementation

### 1. Native Language Setup Caching (Recommended)
```yaml
      # Node.js
      - uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: 'npm' # or 'pnpm', 'yarn'

      # Python
      - uses: actions/setup-python@v5
        with:
          python-version: '3.12'
          cache: 'pip' # or 'poetry', 'pipenv'

      # Go
      - uses: actions/setup-go@v5
        with:
          go-version: '1.22'
          cache: true
```

### 2. Concurrency Group Configuration
```yaml
concurrency:
  group: ${{ github.workflow }}-${{ github.head_ref || github.run_id }}
  cancel-in-progress: true
```

### 3. Enforcing Job Timeouts
```yaml
jobs:
  test:
    runs-on: ubuntu-latest
    timeout-minutes: 10 # Kills job if it exceeds 10 minutes (prevents 6-hour hangs!)
    steps:
      - uses: actions/checkout@v4
      - run: npm test
```

---

## 5. Practical Examples

### Example 1: Multi-Stage Docker Build with GitHub Cache Backend (`type=gha`)
```yaml
name: Build Docker Image with GHA Cache
on:
  push:
    branches: [main]

jobs:
  docker:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: docker/setup-buildx-action@v3
      - uses: docker/login-action@v3
        with:
          registry: ghcr.io
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}

      - uses: docker/build-push-action@v5
        with:
          context: .
          push: true
          tags: ghcr.io/${{ github.repository }}:latest
          # Use GitHub Actions cache backend for Docker layer caching:
          cache-from: type=gha
          cache-to: type=gha,mode=max
```

### Example 2: Optimizing Git Checkout for Large Repositories
```yaml
      # Shallow clone with single branch and no tags (Default in actions/checkout@v4)
      - uses: actions/checkout@v4
        with:
          fetch-depth: 1 # Only latest commit
          submodules: false # Don't fetch unless needed
```

### Example 3: Selective Matrix Testing with `fail-fast`
```yaml
jobs:
  matrix-test:
    runs-on: ubuntu-latest
    strategy:
      fail-fast: true # Instantly cancels remaining jobs on first error
      matrix:
        node: [18, 20, 22]
        shard: [1/3, 2/3, 3/3]
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: ${{ matrix.node }}
      - run: npm ci
      - run: npx jest --shard=${{ matrix.shard }}
```

### Example 4: Caching Custom Directory via `actions/cache@v4`
```yaml
      - name: Cache Next.js Build Output
        uses: actions/cache@v4
        with:
          path: |
            ~/.npm
            ${{ github.workspace }}/.next/cache
          # Generate new cache key on lockfile or source changes
          key: ${{ runner.os }}-nextjs-${{ hashFiles('**/package-lock.json') }}-${{ hashFiles('**/*.js', '**/*.jsx', '**/*.ts', '**/*.tsx') }}
          # Fallback to previous Next.js caches on miss
          restore-keys: |
            ${{ runner.os }}-nextjs-${{ hashFiles('**/package-lock.json') }}-
```

---

## 6. Best Practices for Speed and Cost

1. **Always Set `timeout-minutes` on Every Job**: Default is 360 minutes (6 hours). A single hung test can burn 360 billable minutes in one run.
2. **Use Linux Runners Over Windows or macOS**:
   - Linux: 1x minute cost.
   - Windows: 2x minute cost.
   - macOS: **10x minute cost** (\$0.08/min vs \$0.008/min).
   - *Never run non-Apple specific workloads on macOS runners!*
3. **Avoid Caching Things Slower to Download Than Generate**: Caching small folders (<5MB) often takes longer to compress, upload, download, and extract than simply running the generator.
4. **Leverage Monorepo Path Filters (`on.push.paths`)**: Avoid running backend test suites when a developer only edited documentation or frontend CSS.
5. **Shard Long-Running Test Suites**: Split a 30-minute unit test suite into 3 parallel jobs running in 10 minutes using Jest or Playwright test sharding (`--shard=1/3`).
6. **Use `cancel-in-progress: true` on PRs**: Automatically kills obsolete runs when new commits arrive.

---

## 7. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Running tests on macOS runners | 10x billing rate consumption | Unnecessary OS selection | Use `ubuntu-latest` unless compiling Xcode |
| Omitting `timeout-minutes` | Job hangs for 6 hours; massive bill | Default timeout is 360 min | Add `timeout-minutes: 10` to all jobs |
| Missing concurrency cancellation | Multiple obsolete PR runs execute | No `concurrency` block defined | Set `concurrency: cancel-in-progress: true` |
| Caching `node_modules` directly | Misses native C++ compilation bindings | Incompatible binaries between environments | Cache `~/.npm` package cache, not `node_modules` |
| Overwriting cache keys with static names | Cache becomes stale forever | Static key without hash | Include `hashFiles('**/lockfile')` in key |
| Unsharded 45-minute monolithic test run | Long PR feedback cycles | Sequential test execution | Implement test sharding across matrix |

---

## 8. GitHub Actions Billing Architecture & Minute Multipliers

### Multiplier Table (GitHub-Hosted Runners):
| OS Runner Type | vCPU / RAM | Minute Multiplier | Cost per Minute (Private Repos) |
|:---|:---|:---:|:---:|
| **Linux (`ubuntu-latest`)** | 2 vCPU / 7 GB RAM | **1x** | \$0.008 |
| **Windows (`windows-latest`)** | 2 vCPU / 7 GB RAM | **2x** | \$0.016 |
| **macOS (`macos-latest`)** | 3–4 vCPU / 14 GB RAM | **10x** | \$0.080 |
| **macOS M1 / M2 (`macos-14`)** | 4 vCPU / 14 GB RAM | **10x** | \$0.080 |

### Billing Formula:
$$\text{Total Billable Minutes} = \sum (\text{Actual Job Runtime in Minutes} \times \text{OS Multiplier})$$
*Note: GitHub rounds every individual job runtime up to the nearest whole minute!* (A 61-second Linux job is billed as 2 minutes).

---

## 9. Optimization Comparison Matrix

| Strategy | Implementation Effort | Latency Reduction | Cost Reduction |
|:---|:---:|:---:|:---:|
| **Language Setup Caching** (`cache: 'npm'`) | Minimal (1 line) | 30% – 50% | Medium |
| **Concurrency Cancellation** (`cancel-in-progress`) | Minimal (3 lines) | N/A (Frees queue) | **High (20% – 40%)** |
| **Fail-Fast Matrix** (`fail-fast: true`) | Zero (Default) | Fast failure signal | **High (up to 90% on fail)** |
| **Enforcing Timeouts** (`timeout-minutes: 10`) | Minimal | N/A (Safety net) | Prevents catastrophic spikes |
| **Docker BuildKit Cache (`type=gha`)** | Medium | 50% – 70% | High |
| **Test Suite Sharding** (`--shard=1/N`) | Medium | **60% – 80% (wall-clock)** | Neutral (same compute time) |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: Why should you avoid caching `node_modules` directly and cache the global package cache (`~/.npm`) instead?</strong></summary>

Caching `node_modules` directly causes subtle bugs and cross-platform failures:
1. Many npm packages have native C/C++ bindings compiled during post-install hooks. If the runner OS or libc minor version shifts, the cached compiled binary fails.
2. If `package.json` changes slightly, an existing `node_modules` directory can leave orphaned, incompatible dependencies.
3. Caching the global package tarball cache (`~/.npm` or `~/.cache/yarn`) preserves compressed packages without native binding issues, allowing `npm ci` to unpack dependencies cleanly in seconds.
</details>

<details>
<summary><strong>Q2: Explain how GitHub Actions calculates billable minutes for private repositories, including the rounding rule.</strong></summary>

GitHub Actions calculates billable minutes by taking the duration of each job, rounding it up to the nearest whole minute, and multiplying by the OS multiplier:
- Linux: 1x
- Windows: 2x
- macOS: 10x
For example, if a job on `windows-latest` runs for 1 minute and 2 seconds:
1. Rounded up: 2 minutes.
2. Multiplied by 2 (Windows multiplier): **4 billable minutes**.
If you run 10 sequential 10-second jobs, each is rounded up to 1 minute, totaling 10 billable minutes instead of 2.
</details>

<details>
<summary><strong>Q3: What is the maximum cache size per repository in GitHub Actions, and what eviction policy is used when the limit is exceeded?</strong></summary>

The total cache limit is **10 GB per repository**. When the repository cache exceeds 10 GB, GitHub's cache service evicts older cache entries based on a **Least Recently Used (LRU)** algorithm. Additionally, any cache entry that has not been accessed for 7 days is automatically deleted.
</details>

<details>
<summary><strong>Q4: How does `concurrency: cancel-in-progress: true` work, and why should it be scoped to `github.head_ref || github.run_id`?</strong></summary>

When `concurrency` is defined with `cancel-in-progress: true`, GitHub permits only one workflow run per concurrency group at a time. If a new run starts within the same group, GitHub terminates any running or queued workflows in that group.
Using `group: ${{ github.workflow }}-${{ github.head_ref || github.run_id }}` ensures:
- For pull requests (`github.head_ref` is populated), multiple pushes to the same PR share the same group, cancelling older PR runs.
- For pushes to `main` (`github.head_ref` is empty, fallback to `github.run_id`), every run gets a unique ID, ensuring sequential release/deployment runs on `main` are **not** cancelled!
</details>

<details>
<summary><strong>Q5: What is test sharding, and how does it reduce pipeline wall-clock time without changing total compute?</strong></summary>

Test sharding divides a test suite into $N$ equal slices and runs each slice in parallel across separate runner machines using a matrix (e.g. `shard: [1/4, 2/4, 3/4, 4/4]`).
If a test suite takes 40 minutes sequentially, sharding it into 4 runners allows all 4 slices to complete in 10 minutes. The total compute remains 40 minutes, but developer wait time is cut from 40 minutes to 10 minutes.
</details>

<details>
<summary><strong>Q6: How does Docker Buildx caching with `type=gha` optimize container image builds in GitHub Actions?</strong></summary>

By default, Docker in GitHub Actions builds images from scratch because each runner starts with an empty Docker daemon. Using `cache-from: type=gha` and `cache-to: type=gha,mode=max` exports BuildKit layer cache entries directly into the GitHub Actions cache API. On subsequent runs, BuildKit downloads and reuses cached layers (such as installed apt packages and dependencies), cutting Docker build times from minutes to seconds.
</details>

<details>
<summary><strong>Q7: What happens when `fail-fast: false` is configured on a matrix strategy?</strong></summary>

By default (`fail-fast: true`), if Job 1 of a 20-job matrix fails, GitHub instantly cancels the other 19 running jobs.
Setting `fail-fast: false` instructs GitHub to let all 20 matrix jobs run to completion regardless of failures. This is necessary when you want to collect full test reports across all operating systems and versions even if one environment failed.
</details>

<details>
<summary><strong>Q8: How does `actions/checkout@v4` optimize network transfer by default?</strong></summary>

`actions/checkout@v4` performs a shallow clone of depth 1 (`git clone --depth=1`) for only the single commit that triggered the workflow, skipping the rest of the Git history and unneeded tags. It also disables submodules by default. For large repositories with gigabytes of Git history, this reduces checkout time from minutes to a fraction of a second.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Implement PR Concurrency Cancellation</strong></summary>

**Scenario**: Add concurrency control that cancels obsolete PR runs but protects `main`.
```yaml
concurrency:
  group: ${{ github.workflow }}-${{ github.head_ref || github.run_id }}
  cancel-in-progress: true
```
</details>

<details>
<summary><strong>Exercise 2: Configure Built-in npm Caching</strong></summary>

**Scenario**: Use `actions/setup-node@v4` with native `cache: 'npm'`.
```yaml
- uses: actions/setup-node@v4
  with:
    node-version: 20
    cache: 'npm'
- run: npm ci
```
</details>

<details>
<summary><strong>Exercise 3: Add Explicit Job Timeouts</strong></summary>

**Scenario**: Enforce a 12-minute maximum timeout on a test job.
```yaml
jobs:
  test:
    runs-on: ubuntu-latest
    timeout-minutes: 12
    steps:
      - run: npm test
```
</details>

<details>
<summary><strong>Exercise 4: Implement Test Sharding with Jest</strong></summary>

**Scenario**: Split test execution across two parallel matrix runners.
```yaml
strategy:
  matrix:
    shard: [1/2, 2/2]
steps:
  - uses: actions/checkout@v4
  - run: npx jest --shard=${{ matrix.shard }}
```
</details>

<details>
<summary><strong>Exercise 5: Inspect Repository Cache via GitHub CLI</strong></summary>

**Scenario**: List all active caches and their size using `gh cache`.
```bash
gh cache list --limit 20
```
</details>

<details>
<summary><strong>Exercise 6: Delete a Specific Stale Cache via GitHub CLI</strong></summary>

**Scenario**: Delete an outdated cache entry by key.
```bash
gh cache delete "Linux-npm-a1b2c3d"
```
</details>

---

## 12. Summary & Cheat Sheet

```yaml
# Performance & Cost Optimization Checklist

# 1. Concurrency (Cancel obsolete PR runs)
concurrency:
  group: ${{ github.workflow }}-${{ github.head_ref || github.run_id }}
  cancel-in-progress: true

jobs:
  fast-build:
    runs-on: ubuntu-latest      # 2. Use Linux (1x rate) instead of macOS (10x rate)
    timeout-minutes: 10         # 3. Prevent hanging jobs from consuming hours

    strategy:
      fail-fast: true           # 4. Abort matrix on first error

    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 1        # 5. Shallow clone

      - uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: 'npm'          # 6. Built-in setup caching

      - run: npm ci             # 7. Fast, deterministic dependencies
```

---

## 13. Further Reading
- [GitHub Docs - Managing Workflow Run Concurrency](https://docs.github.com/en/actions/using-workflows/workflow-syntax-for-github-actions#concurrency)
- [GitHub Docs - Caching Dependencies to Speed Up Workflows](https://docs.github.com/en/actions/using-workflows/caching-dependencies-to-speed-up-workflows)
- [GitHub Actions Billing and Runner Pricing](https://docs.github.com/en/billing/managing-billing-for-github-actions/about-billing-for-github-actions)
- [04-github-actions/23-testing-workflows-locally.md](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/23-testing-workflows-locally.md)
