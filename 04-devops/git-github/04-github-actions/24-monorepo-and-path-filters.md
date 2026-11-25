---
title: "GitHub Actions in Monorepos: Path Filtering & Selective CI"
category: "04-devops"
subcategory: "git-github / 04-github-actions"
tags: ["github-actions", "monorepo", "path-filtering", "dorny-paths-filter", "turborepo", "nx", "selective-ci"]
difficulty: "Advanced"
estimated_read_time: "28 min"
prerequisites:
  - "04-github-actions/03-events-and-triggers"
  - "04-github-actions/10-matrix-and-concurrency"
  - "04-github-actions/22-workflow-performance-and-cost"
last_updated: "2026-11-25"
---

# GitHub Actions in Monorepos: Path Filtering & Selective CI

> **Navigation**: [← Previous: 23-testing-workflows-locally.md](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/23-testing-workflows-locally.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/README.md) | [Next: 25-composite-vs-javascript-vs-docker-actions.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/25-composite-vs-javascript-vs-docker-actions.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Native Path Filtering: Syntax & Limitations](#3-native-path-filtering-syntax--limitations)
4. [Advanced Selective CI with `dorny/paths-filter`](#4-advanced-selective-ci-with-dorny-paths-filter)
5. [Monorepo Dependency Graph Impact (Downstream Propagation)](#5-monorepo-dependency-graph-impact-downstream-propagation)
6. [Practical Examples](#6-practical-examples)
7. [Best Practices](#7-best-practices)
8. [Common Mistakes](#8-common-mistakes)
9. [Performance, Security & Cost](#9-performance-security--cost)
10. [Comparison: Native Paths vs paths-filter vs Turborepo/Nx](#10-comparison-native-paths-vs-paths-filter-vs-turborepo-nx)
11. [Interview Questions & Answers](#11-interview-questions--answers)
12. [Practice Exercises](#12-practice-exercises)
13. [Summary & Cheat Sheet](#13-summary--cheat-sheet)
14. [Further Reading](#14-further-reading)

---

## 1. What & Why

### What is Selective CI in Monorepos?
A **monorepo** stores multiple applications, services, and shared libraries within a single Git repository. Without selective CI, modifying a single CSS file in `apps/docs` triggers tests and builds across all 20 backend microservices, mobile apps, and shared libraries, taking hours and consuming thousands of runner minutes. **Selective CI** uses path filtering and dependency graph analysis to execute jobs strictly for the packages that changed or depend on changed code.

### Why Does It Matter?
1. **Preventing Monorepo Gridlock**: Running full regression suites on every pull request causes massive queuing delays. Selective CI cuts test times from 45 minutes to 3 minutes.
2. **Massive Cost Reduction**: Reduces billable runner minutes by 60% to 85% by skipping untouched microservices.
3. **Targeted Deployments**: Safely trigger independent CD pipelines (e.g. deploy `services/billing` to Kubernetes) only when its code or shared schema changes.

---

## 2. Core Concepts

### 1. Glob Patterns in Path Filtering
Git and GitHub Actions use standard POSIX globs:
- `apps/web/**`: Matches all files and nested subdirectories inside `apps/web/`.
- `*.md`: Matches markdown files in root.
- `!apps/web/**/*.test.ts`: Negative pattern (ignores test files).

### 2. The Native Path Filter (`on.push.paths`)
GitHub provides top-level event filtering based on changed paths. If no changed files match the patterns, the **entire workflow run is skipped**.

### 3. Step/Job-Level Filtering (`dorny/paths-filter`)
Top-level filtering cannot conditionally run individual jobs inside a unified workflow. Using `dorny/paths-filter`, an initial inspection job inspects Git diffs and exports boolean or matrix outputs (`outputs.web == 'true'`), allowing downstream jobs to use `if: needs.filter.outputs.web == 'true'`.

### 4. Dependency Graph Invalidation
If `packages/ui` changes, not only must `packages/ui` be tested, but all consumer applications (`apps/web`, `apps/mobile`) that import `packages/ui` must also be re-tested and re-built.

---

## 3. Native Path Filtering: Syntax & Limitations

### Native Workflow Syntax:
```yaml
name: Docs Service CI
on:
  push:
    branches: [main]
    paths:
      - 'apps/docs/**'
      - 'packages/shared-ui/**'
      - '!apps/docs/**/*.md' # Exclude documentation files
  pull_request:
    branches: [main]
    paths:
      - 'apps/docs/**'

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: echo "Running tests for docs app..."
```

### Critical Limitations of Native Path Filtering:
1. **Required Status Checks Dilemma**: If a branch protection rule requires `Docs Service CI / test` to pass before merging, and a PR touches only `apps/api`, the workflow **never runs**. GitHub marks the check as "Expected / Pending" forever, **blocking the pull request from merging!**
2. **All-or-Nothing Granularity**: Cannot trigger Job A for service 1 and Job B for service 2 within the same workflow based on diffs.
3. **No Dynamic Matrix**: Cannot dynamically build an array of changed packages to feed into `strategy.matrix`.

---

## 4. Advanced Selective CI with `dorny/paths-filter`

`dorny/paths-filter` solves the Required Status Checks dilemma by running a fast 5-second filter job on every PR that outputs status booleans and matrix JSON arrays.

```
                    Selective Monorepo CI Flow:

                     [ Pull Request Opened ]
                                │
                                ▼
         ┌──────────────────────────────────────────────┐
         │ Job 1: Filter (Always runs; ~5 seconds)      │
         │ Evaluates: git diff against base branch      │
         │ Outputs: web=true, api=false, matrix=['web'] │
         └──────────────────────┬───────────────────────┘
                                │
              ┌─────────────────┴─────────────────┐
              ▼                                   ▼
 ┌──────────────────────────┐       ┌──────────────────────────┐
 │ Job 2: Test Web App      │       │ Job 3: Test API Service  │
 │ if: needs.filter.outputs │       │ if: needs.filter.outputs │
 │     .web == 'true'       │       │     .api == 'true'       │
 │ [ EXECUTED ]             │       │ [ SKIPPED CLEANLY ]      │
 └────────────┬─────────────┘       └─────────────┬────────────┘
              │                                   │
              └─────────────────┬─────────────────┘
                                │
                                ▼
         ┌──────────────────────────────────────────────┐
         │ Job 4: CI Status Gate (Required Check)       │
         │ if: always()                                 │
         │ Verifies that no executed job failed         │
         │ [ STATUS GREEN -> PR ALLOWED TO MERGE! ]     │
         └──────────────────────────────────────────────┘
```

---

## 5. Monorepo Dependency Graph Impact (Downstream Propagation)

When configuring filters in a monorepo, always map shared libraries to consumer services:

```yaml
      - uses: dorny/paths-filter@v3
        id: filter
        with:
          filters: |
            # Web app changes if web code OR shared UI changes!
            web:
              - 'apps/web/**'
              - 'packages/ui/**'
              - 'packages/utils/**'

            # API changes if API code OR shared database models change!
            api:
              - 'services/api/**'
              - 'packages/db-schema/**'
              - 'packages/utils/**'

            # Shared packages
            ui:
              - 'packages/ui/**'
```

---

## 6. Practical Examples

### Example 1: Full Monorepo Workflow with Required Checks Pass
```yaml
# .github/workflows/monorepo-ci.yml
name: Monorepo CI

on:
  pull_request:
    branches: [main]

jobs:
  changes:
    name: Detect Changed Packages
    runs-on: ubuntu-latest
    permissions:
      pull-requests: read
    outputs:
      web: ${{ steps.filter.outputs.web }}
      api: ${{ steps.filter.outputs.api }}
    steps:
      - uses: actions/checkout@v4
      - uses: dorny/paths-filter@v3
        id: filter
        with:
          filters: |
            web:
              - 'apps/web/**'
              - 'packages/**'
            api:
              - 'services/api/**'
              - 'packages/**'

  test-web:
    needs: changes
    if: needs.changes.outputs.web == 'true'
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: 'npm'
      - run: npm ci
      - run: npm run test:web

  test-api:
    needs: changes
    if: needs.changes.outputs.api == 'true'
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: 'npm'
      - run: npm ci
      - run: npm run test:api

  # Required Status Check Gate for GitHub Branch Protection
  ci-status-gate:
    name: CI Status Gate
    needs: [changes, test-web, test-api]
    if: always()
    runs-on: ubuntu-latest
    steps:
      - name: Evaluate pipeline results
        run: |
          WEB_STATUS="${{ needs.test-web.result }}"
          API_STATUS="${{ needs.test-api.result }}"

          if [[ "$WEB_STATUS" == "failure" || "$API_STATUS" == "failure" ]]; then
            echo "::error::One or more test jobs failed!"
            exit 1
          fi
          echo "All targeted tests succeeded or were safely skipped."
```

### Example 2: Dynamic Matrix Generation for Changed Microservices
```yaml
jobs:
  filter:
    runs-on: ubuntu-latest
    outputs:
      packages: ${{ steps.filter.outputs.changes }}
    steps:
      - uses: actions/checkout@v4
      - uses: dorny/paths-filter@v3
        id: filter
        with:
          list-files: json
          filters: |
            auth: 'services/auth/**'
            billing: 'services/billing/**'
            orders: 'services/orders/**'

  build-dynamic-matrix:
    needs: filter
    if: needs.filter.outputs.packages != '[]'
    strategy:
      matrix:
        package: ${{ fromJSON(needs.filter.outputs.packages) }}
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build service
        run: |
          echo "Building changed microservice: ${{ matrix.package }}"
          make -C services/${{ matrix.package }} test
```

### Example 3: Turborepo Remote Caching Integration
```yaml
name: Turborepo Monorepo CI
on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    env:
      TURBO_TOKEN: ${{ secrets.TURBO_TOKEN }}
      TURBO_TEAM: ${{ vars.TURBO_TEAM }}
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: 'npm'
      - run: npm ci
      # Turborepo natively builds only changed packages and hashes dependencies
      - run: npx turbo run test --filter=...[origin/main]
```

---

## 7. Best Practices

1. **Always Implement a `ci-status-gate` Job**: Never mark skipped jobs as Required Checks in GitHub Branch Protection. Use a single consolidated gate job that evaluates `needs.*.result` with `if: always()`.
2. **Map Shared Core Packages to Consumers**: Any library imported by other packages must trigger downstream tests.
3. **Include Workflow YAML Files in Filters**: If `.github/workflows/ci.yml` or root lockfiles (`package-lock.json`) are modified, trigger the full test suite across all services.
4. **Use Shallow Clones Carefully**: When diffing branches in PRs, `paths-filter` needs access to the base branch. If diffing against `main`, ensure checkout has sufficient depth or fetch base ref:
   ```yaml
   - uses: actions/checkout@v4
     with:
       fetch-depth: 100
   ```
5. **Combine Path Filtering with Remote Caching**: Use Turborepo, Nx, or Gradle Enterprise remote caching to achieve sub-minute build times even on full runs.

---

## 8. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Native `paths:` on required checks | PR blocked forever: checks "Expected" | Workflow was skipped, so check never reported | Use `dorny/paths-filter` with gate job |
| Forgetting to map shared libraries | Broken build on main | Shared library modified without testing consumers | Add `packages/shared/**` to service filters |
| Omission of root config files | Config changes don't run tests | Modifying root `tsconfig.json` or `package-lock.json` | Add root files to all critical filters |
| Shallow clone breaking diff calculation | `paths-filter` fails to identify changes | `fetch-depth: 1` missing common merge base | Increase fetch-depth or fetch base ref |
| Overly complex filter duplication | Massive unmaintainable YAML | Redefining paths across 10 workflow files | Consolidate filters in root workflow or reusable workflow |

---

## 9. Performance, Security & Cost

- **Runner Time Savings**: In an enterprise repository with 15 microservices, selective CI reduces PR pipeline duration from 38 minutes to 4 minutes, saving **over 80% of compute minutes**.
- **Network Traffic**: Skipping untouched service jobs prevents unneeded Docker image pulls, artifact downloads, and container spins.
- **Security Scanners**: Security vulnerability scans (Dependabot, CodeQL, Snyk) should generally run on the **entire** repository on a nightly schedule, even if selective CI skips them on PRs.

---

## 10. Comparison: Native Paths vs paths-filter vs Turborepo/Nx

| Feature | Native `paths:` | `dorny/paths-filter` | Turborepo / Nx |
|:---|:---|:---|:---|
| **Scope** | Entire workflow only | Job-level & Step-level | Task-level (inside job) |
| **Required Checks Safe?** | **No** (Blocks PRs) | **Yes** (Runs gate job) | **Yes** (Single command) |
| **Dependency Graph Awareness**| Manual path listing | Manual path listing | **Automatic (AST / package.json)** |
| **Dynamic Matrix Output** | No | **Yes (JSON array)** | N/A (Internal runner) |
| **Setup Overhead** | None | 1 step in YAML | Requires monorepo tooling |

---

## 11. Interview Questions & Answers

<details>
<summary><strong>Q1: Why does using native GitHub Actions `paths:` triggers often break GitHub Branch Protection required checks?</strong></summary>

When a workflow uses `on.pull_request.paths: ['apps/web/**']` and a pull request modifies only `services/api`, GitHub detects that no changed files match the filter and does not run the workflow.
If the repository has a Branch Protection Rule requiring `web-test` to pass before merging, GitHub sees that `web-test` was never executed and keeps its status permanently at "Expected / Pending". The PR can never satisfy the merge requirements.
</details>

<details>
<summary><strong>Q2: How does the "CI Status Gate" pattern resolve the required status check problem?</strong></summary>

The workflow always runs on every PR. The first job (`filter`) determines which packages changed. Selective jobs run or skip based on `if: needs.filter.outputs.<pkg> == 'true'`.
A final job (`ci-status-gate`) runs with `if: always()`. It inspects the results of all upstream jobs (`needs.*.result`). If all jobs either succeeded or were skipped, the gate exits with code 0 (success). If any executed job failed, the gate exits with code 1.
The repository branch protection rule targets **only** `ci-status-gate`, ensuring PRs can always merge when untouched services are skipped.
</details>

<details>
<summary><strong>Q3: How do you handle shared packages in monorepo path filters?</strong></summary>

Any application that imports or depends on a shared package must include that shared package's directory in its filter definition:
```yaml
filters: |
  web:
    - 'apps/web/**'
    - 'packages/shared-ui/**'
    - 'packages/types/**'
```
This ensures that changes to `packages/shared-ui` trigger tests for both the library and all its consumer applications.
</details>

<details>
<summary><strong>Q4: How can `dorny/paths-filter` dynamically generate a matrix of jobs?</strong></summary>

By setting `list-files: json`, `dorny/paths-filter` outputs `steps.filter.outputs.changes`, which is a JSON array string containing the names of all filter keys that matched (e.g. `["service-a", "service-c"]`).
In downstream jobs, you supply this output to the strategy matrix using `fromJSON()`:
```yaml
strategy:
  matrix:
    service: ${{ fromJSON(needs.filter.outputs.changes) }}
```
GitHub Actions will dynamically launch one parallel runner per changed service.
</details>

<details>
<summary><strong>Q5: What is the difference between path filtering and build tools like Turborepo or Nx?</strong></summary>

Path filtering operates at the GitHub Actions orchestrator level (deciding which jobs or workflows to launch on virtual machines).
Monorepo build tools like Turborepo or Nx operate inside a single job on the runner machine. They analyze the abstract syntax tree and `package.json` dependency graphs to determine which tasks to execute, utilizing remote caching to skip compiling packages whose inputs have not changed. Combining both (running Turborepo inside selective CI jobs) provides maximum efficiency.
</details>

<details>
<summary><strong>Q6: Why must root configuration files and lockfiles be included in selective filters?</strong></summary>

Modifications to root files like `package-lock.json`, `.github/workflows/ci.yml`, `tsconfig.json`, or root linters (`.eslintrc.js`) affect the dependency resolution, compiler flags, and linting rules of every package in the monorepo. Omitting them from filters can result in broken packages on `main` when dependency versions shift.
</details>

<details>
<summary><strong>Q7: How do you exclude documentation files (e.g. `*.md`) from triggering builds in a subproject?</strong></summary>

Using negative glob patterns prefixed with `!`:
```yaml
filters: |
  backend:
    - 'services/backend/**'
    - '!services/backend/**/*.md'
    - '!services/backend/docs/**'
```
</details>

<details>
<summary><strong>Q8: What Git checkout issue can cause `paths-filter` to miscalculate diffs on pull requests?</strong></summary>

If `actions/checkout@v4` runs with default `fetch-depth: 1`, only the pull request merge commit is downloaded without the base commit history. `paths-filter` needs to compare `HEAD` against the merge base of the target branch (`origin/main`). If the commit history is truncated, diff calculations can fail or produce empty diffs. Setting `fetch-depth: 100` or fetching the target branch resolves the issue.
</details>

---

## 12. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Two-Service Path Filter Job</strong></summary>

**Scenario**: Write a filter job that outputs booleans for `frontend` and `backend`.
```yaml
jobs:
  filter:
    runs-on: ubuntu-latest
    outputs:
      frontend: ${{ steps.changes.outputs.frontend }}
      backend: ${{ steps.changes.outputs.backend }}
    steps:
      - uses: actions/checkout@v4
      - uses: dorny/paths-filter@v3
        id: changes
        with:
          filters: |
            frontend: 'frontend/**'
            backend: 'backend/**'
```
</details>

<details>
<summary><strong>Exercise 2: Write a Downstream Conditional Job</strong></summary>

**Scenario**: Execute backend tests only if `backend` changed.
```yaml
test-backend:
  needs: filter
  if: needs.filter.outputs.backend == 'true'
  runs-on: ubuntu-latest
  steps:
    - run: echo "Running backend tests..."
```
</details>

<details>
<summary><strong>Exercise 3: Build the Universal CI Status Gate</strong></summary>

**Scenario**: Implement the status check gate that satisfies branch protection.
```yaml
ci-gate:
  needs: [filter, test-frontend, test-backend]
  if: always()
  runs-on: ubuntu-latest
  steps:
    - run: |
        if [[ "${{ needs.test-frontend.result }}" == "failure" || "${{ needs.test-backend.result }}" == "failure" ]]; then
          exit 1
        fi
        echo "Pipeline successful."
```
</details>

<details>
<summary><strong>Exercise 4: Test Dynamic Matrix Output</strong></summary>

**Scenario**: Generate a matrix from `paths-filter` matches.
```yaml
deploy-matrix:
  needs: filter
  if: needs.filter.outputs.packages != '[]'
  strategy:
    matrix:
      pkg: ${{ fromJSON(needs.filter.outputs.packages) }}
  runs-on: ubuntu-latest
  steps:
    - run: echo "Deploying ${{ matrix.pkg }}"
```
</details>

<details>
<summary><strong>Exercise 5: Exclude Markdown and Asset Changes</strong></summary>

**Scenario**: Filter for app code while ignoring docs and images.
```yaml
filters: |
  app:
    - 'src/**'
    - '!src/**/*.md'
    - '!src/assets/images/**'
```
</details>

<details>
<summary><strong>Exercise 6: Test Turborepo Filter Invocations Locally</strong></summary>

**Scenario**: Use Turborepo CLI to run tests only for packages changed since origin/main.
```bash
npx turbo run test --filter=...[origin/main]
```
</details>

---

## 13. Summary & Cheat Sheet

```yaml
# Complete Monorepo Selective CI Pattern
name: Selective CI
on:
  pull_request:
    branches: [main]

jobs:
  filter:
    runs-on: ubuntu-latest
    outputs:
      web: ${{ steps.filter.outputs.web }}
      api: ${{ steps.filter.outputs.api }}
    steps:
      - uses: actions/checkout@v4
      - uses: dorny/paths-filter@v3
        id: filter
        with:
          filters: |
            web: ['apps/web/**', 'packages/**']
            api: ['services/api/**', 'packages/**']

  test-web:
    needs: filter
    if: needs.filter.outputs.web == 'true'
    runs-on: ubuntu-latest
    steps:
      - run: npm run test:web

  test-api:
    needs: filter
    if: needs.filter.outputs.api == 'true'
    runs-on: ubuntu-latest
    steps:
      - run: npm run test:api

  ci-status-gate:
    needs: [filter, test-web, test-api]
    if: always()
    runs-on: ubuntu-latest
    steps:
      - run: |
          [[ "${{ needs.test-web.result }}" != "failure" && "${{ needs.test-api.result }}" != "failure" ]]
```

---

## 14. Further Reading
- [dorny/paths-filter GitHub Repository](https://github.com/dorny/paths-filter)
- [Turborepo Documentation - Running Tasks on Changed Packages](https://turbo.build/repo/docs/core-concepts/filtering)
- [Nx Monorepo CI Guide](https://nx.dev/ci/intro/ci-with-github-actions)
- [04-github-actions/25-composite-vs-javascript-vs-docker-actions.md](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/25-composite-vs-javascript-vs-docker-actions.md)
