---
title: "GitHub Actions Core Concepts & Architecture"
category: "04-devops"
subcategory: "git-github / 04-github-actions"
tags: ["github-actions", "ci-cd", "automation", "workflows", "runners", "devops"]
difficulty: "Beginner-Intermediate"
estimated_read_time: "25 min"
prerequisites:
  - "00-setup/01-getting-started"
  - "01-fundamentals/01-git-vs-github"
last_updated: "2026-11-25"
---

# GitHub Actions Core Concepts & Architecture

> **Navigation**: [← Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/README.md) | [Next: 02-workflow-syntax.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/02-workflow-syntax.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts & Hierarchy](#2-core-concepts--hierarchy)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes](#7-common-mistakes)
8. [Performance, Security & Cost](#8-performance-security--cost)
9. [Comparison Table: Actions vs Jenkins vs GitLab CI](#9-comparison-table-actions-vs-jenkins-vs-gitlab-ci)
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is GitHub Actions?
GitHub Actions is a cloud-native continuous integration and continuous delivery (CI/CD) and workflow automation platform integrated directly into GitHub repositories. It allows software teams to define automated pipelines triggered by any GitHub platform event (pushes, pull requests, issue creation, releases, or cron schedules) that run on ephemeral virtual machines or containerized runners.

### Why Do We Use It?
1. **Tight GitHub Integration**: Eliminates external webhooks, separate authentication servers, and context switching. Checks, logs, annotations, and artifacts live directly inside pull requests.
2. **Event-Driven Versatility**: Beyond compilation and testing, Actions automates repository triage, stale issue management, deployment approvals, label automation, and release drafting.
3. **Composable Ecosystem**: Leverage thousands of verified, reusable open-source Actions from the GitHub Marketplace (`actions/checkout`, `actions/setup-node`, `docker/build-push-action`) without writing custom shell harnesses from scratch.
4. **Zero Infrastructure Maintenance**: GitHub provides hosted runners (`ubuntu-latest`, `windows-latest`, `macos-latest`) preconfigured with hundreds of development tools and runtimes.

---

## 2. Core Concepts & Hierarchy

The GitHub Actions execution hierarchy follows a strict parent-child relationship:

```
[ EVENT ] (Push, Pull Request, Schedule, Webhook)
    │
    ▼
[ WORKFLOW ] (.github/workflows/*.yml)
    │
    ├── [ JOB 1: Lint ] ─────► Runner VM A (Fresh isolated environment)
    │     ├── Step 1: actions/checkout@v4
    │     └── Step 2: npm run lint
    │
    └── [ JOB 2: Test ] ─────► Runner VM B (Runs in parallel with Job 1)
          ├── Step 1: actions/checkout@v4
          ├── Step 2: actions/setup-node@v4
          └── Step 3: npm test
```

### The Building Blocks:
| Concept | Description | Analogy |
|:---|:---|:---|
| **Event** | An activity that triggers a workflow (e.g. `push`, `pull_request`, `schedule`). | The doorbell |
| **Workflow** | A configurable automated process defined in a YAML file in `.github/workflows/`. | The instruction booklet |
| **Job** | A sequence of steps executing on the same runner instance. Jobs run in parallel by default. | A workstation |
| **Step** | An individual task within a job. Can be a command (`run`) or an action (`uses`). | A checklist item |
| **Action** | A reusable application or script designed for GitHub Actions. | A power tool |
| **Runner** | An ephemeral virtual machine or container that listens for and executes jobs. | The worker |

---

## 3. How It Works Internally

```
GitHub Actions Execution Lifecycle:

1. Webhook Event Triggered (Developer pushes code or opens PR)
             │
             ▼
2. Workflow Parser Engine
   Reads YAML files from .github/workflows/ in that specific commit ref
             │
             ▼
3. Job Dependency Graph (DAG) Resolution
   Determines parallel vs sequential jobs based on 'needs:'
             │
             ▼
4. Runner Allocation & Queue
   GitHub Orchestrator assigns job to a fresh Azure VM (GitHub-hosted)
   or dispatches to active Self-Hosted runner daemon
             │
             ▼
5. Step Execution on Runner
   - Initializes runner environment
   - Downloads referenced actions (Marketplace)
   - Executes step commands in subshells
   - Streams live stdout/stderr logs back to GitHub via WebSockets
             │
             ▼
6. Teardown & Post-Actions
   - Executes 'post' step hooks (e.g., node_modules caching, Docker cleanup)
   - Uploads artifacts to Azure Blob Storage
   - VM runner is destroyed immediately (ephemeral lifecycle)
```

---

## 4. Syntax & Usage

```yaml
# .github/workflows/ci.yml
name: Continuous Integration

# Event triggers
on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

# Security least-privilege default
permissions:
  contents: read

# Concurrency group to cancel obsolete PR runs
concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

jobs:
  # Job 1: Code formatting and linting
  lint:
    runs-on: ubuntu-latest
    timeout-minutes: 10
    steps:
      - name: Check out repository
        uses: actions/checkout@v4

      - name: Set up Node.js 20
        uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: 'npm'

      - name: Install dependencies
        run: npm ci

      - name: Run linter
        run: npm run lint

  # Job 2: Unit and integration tests (dependent on lint)
  test:
    needs: lint
    runs-on: ubuntu-latest
    timeout-minutes: 15
    steps:
      - name: Check out repository
        uses: actions/checkout@v4

      - name: Set up Node.js 20
        uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: 'npm'

      - name: Install dependencies
        run: npm ci

      - name: Run test suite
        run: npm test -- --coverage
```

---

## 5. Practical Examples

### Example 1: Basic Smoke Test on Push
```yaml
name: Smoke Test
on: [push]

jobs:
  smoke:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Verify Python runtime
        run: python3 --version
```

### Example 2: Multi-Job Pipeline with Dependency Ordering (`needs`)
```yaml
name: Staged Pipeline
on: [push]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: echo "Building application..."

  test:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - run: echo "Testing application after build succeeded..."

  deploy:
    needs: test
    runs-on: ubuntu-latest
    steps:
      - run: echo "Deploying application to staging..."
```

### Example 3: Matrix Build Across Multiple Node Versions
```yaml
name: Node Matrix Test
on: [pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        node-version: [18, 20, 22]
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: ${{ matrix.node-version }}
      - run: npm ci
      - run: npm test
```

### Example 4: Scheduled Nightly Health Check
```yaml
name: Nightly Security Audit
on:
  schedule:
    # Runs at 02:00 UTC every night
    - cron: '0 2 * * *'
  workflow_dispatch: # Allows manual trigger

jobs:
  audit:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: npm audit --audit-level=high
```

### Example 5: Path-Filtered Trigger for Monorepo Microservice
```yaml
name: Auth Service CI
on:
  push:
    paths:
      - 'services/auth/**'
      - '.github/workflows/auth-ci.yml'

jobs:
  test:
    runs-on: ubuntu-latest
    defaults:
      run:
        working-directory: services/auth
    steps:
      - uses: actions/checkout@v4
      - run: npm ci && npm test
```

---

## 6. Best Practices

1. **Pin Action Versions**: Pin external actions by SHA rather than mutable tags (`uses: actions/checkout@b4ffde65f46336ab88eb53be808477a3936bae11 # v4.1.1`).
2. **Enforce Least Privilege Permissions**: Explicitly declare `permissions: contents: read` at the top of every workflow to disable default write tokens.
3. **Always Set `timeout-minutes` on Every Job**: Prevent rogue hanging processes from consuming billing minutes indefinitely.
4. **Use `concurrency` with `cancel-in-progress: true`**: Automatically cancel superseded builds on feature branches when new commits are pushed.
5. **Use `npm ci` / `yarn install --frozen-lockfile`**: Never run `npm install` in CI; always use deterministic lockfile installations.
6. **Separate CI and CD Concerns**: Keep testing and deployment in distinct workflow files or explicit environment-gated jobs.

---

## 7. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Missing `actions/checkout` | Job fails with `file not found` | Runners start with empty workspaces | Add `uses: actions/checkout@v4` as first step |
| Assuming jobs share filesystem | Second job cannot find built artifacts from first job | Jobs execute on completely separate VMs | Pass data via `actions/upload-artifact` & `download-artifact` |
| Forgetting `timeout-minutes` | Run hangs for 6 hours (default max) burning runner minutes | Missing safeguard | Set `timeout-minutes: 15` on all jobs |
| Running `npm install` instead of `npm ci` | Inconsistent dependencies between local and CI | `npm install` updates lockfile non-deterministically | Use `npm ci` |
| Overly broad permissions | Attacker could tamper with repository via compromised action | Default `GITHUB_TOKEN` has read/write | Set `permissions: contents: read` globally |
| Editing workflow on feature branch and expecting `main` to run | Changes only apply to commits on that feature branch | Workflow files execute from the event ref | Test on branch, then merge to default branch |

---

## 8. Performance, Security & Cost

- **Runner Minute Multipliers**: GitHub charges different rates for OS runner types:
  - Linux (`ubuntu-latest`): 1x multiplier.
  - Windows (`windows-latest`): 2x multiplier.
  - macOS (`macos-latest`): 10x multiplier!
  - *Optimization*: Run builds and unit tests on Linux whenever possible; reserve macOS strictly for iOS/macOS compilation.
- **Supply Chain Security**: Third-party actions from the Marketplace execute arbitrary code inside your runner with access to your `GITHUB_TOKEN` and repository secrets. Always review source code and pin to commit SHAs.
- **Cache Optimization**: Use built-in setup action caching (`cache: 'npm'`, `cache: 'pip'`, `cache: 'gradle'`) rather than raw `actions/cache` for 30-50% faster setup times.

---

## 9. Comparison Table: Actions vs Jenkins vs GitLab CI

| Dimension | GitHub Actions | Jenkins | GitLab CI |
|:---|:---|:---|:---|
| **Hosting Model** | Fully managed cloud + self-hosted | Self-hosted master/agents | Managed cloud or self-hosted |
| **Configuration** | YAML (`.github/workflows/*.yml`) | Groovy Jenkinsfile / Web UI | YAML (`.gitlab-ci.yml`) |
| **Marketplace Ecosystem** | 20,000+ reusable actions | 1,800+ plugins (varying quality) | GitLab Components catalog |
| **Infrastructure Overhead**| Zero (GitHub manages VMs) | Heavy (controller patching, agent scaling) | Low to medium |
| **Event Triggers** | Every GitHub platform event | Git webhooks / polling | GitLab events & webhooks |
| **Secrets Management** | Native encrypted secrets & environments | Credentials Plugin | Masked CI/CD variables |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the relationship between a workflow, a job, a step, and a runner in GitHub Actions?</strong></summary>

A **workflow** is the top-level automated process defined in a YAML file in `.github/workflows/`. A workflow contains one or more **jobs**. By default, jobs run in parallel, and each job is executed on its own isolated, freshly provisioned **runner** (a VM or container). Within a job, execution proceeds sequentially through one or more **steps**. A step is either a shell command (`run`) or an action invocation (`uses`). Steps in the same job share the runner's filesystem and process memory.
</details>

<details>
<summary><strong>Q2: Do jobs within the same workflow share files by default? How do you pass data between jobs?</strong></summary>

**No.** Each job executes on a completely independent runner instance with its own isolated virtual machine and filesystem. Files generated in Job A (such as build artifacts or compiled binaries) are not accessible to Job B.
To pass data between jobs:
1. Job A uploads files using `actions/upload-artifact@v4`.
2. Job B specifies `needs: JobA` to enforce execution order.
3. Job B downloads the files using `actions/download-artifact@v4`.
Alternatively, small strings can be passed using Job Outputs (`outputs`).
</details>

<details>
<summary><strong>Q3: How do you enforce that Job B only runs after Job A completes successfully? What if Job A fails?</strong></summary>

Use the `needs:` keyword on Job B:
```yaml
jobs:
  build:
    ...
  test:
    needs: build
```
If `build` fails, `test` is automatically skipped by default. If you need Job B to run even when Job A fails (e.g. for cleanup or alerting), specify an `if:` condition:
```yaml
if: always()
# or
if: failure()
```
</details>

<details>
<summary><strong>Q4: Why is setting `permissions: contents: read` considered a foundational security best practice?</strong></summary>

By default, in newly created repositories or older organizations, GitHub provides the automatic `GITHUB_TOKEN` with broad read and write permissions to the repository (contents, pull requests, issues, packages). If a compromised or malicious third-party action runs in your pipeline, it can use that token to push malicious commits directly to your codebase. Explicitly defining minimal permissions (`permissions: contents: read`) adheres to the Principle of Least Privilege, neutralizing token abuse.
</details>

<details>
<summary><strong>Q5: What is the purpose of the `concurrency` block with `cancel-in-progress: true`?</strong></summary>

When developers push consecutive commits in rapid succession to a pull request, multiple workflow runs are queued simultaneously. Without concurrency controls, all runs execute to completion, wasting compute time and billing minutes. Configuring:
```yaml
concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true
```
ensures that whenever a new commit is pushed, GitHub instantly cancels any running or queued workflows for that branch, saving substantial resources.
</details>

<details>
<summary><strong>Q6: Why is `npm ci` strongly preferred over `npm install` in CI workflows?</strong></summary>

`npm install` reads `package.json`, resolves version ranges, updates the dependency tree, and can modify `package-lock.json` if minor version bumps are permitted. This introduces non-deterministic builds.
`npm ci` (Clean Install) strictly requires an exact `package-lock.json`, deletes existing `node_modules`, and installs the exact byte versions specified in the lockfile. If `package.json` and `package-lock.json` are out of sync, `npm ci` halts with an error. It is also significantly faster.
</details>

<details>
<summary><strong>Q7: What is the default maximum execution timeout for a GitHub Actions job if `timeout-minutes` is omitted?</strong></summary>

The default execution timeout for a job is **360 minutes (6 hours)**. If a test hangs or a build script waits indefinitely for input, the runner will remain active for 6 hours, consuming 360 billing minutes. Best practice is to set `timeout-minutes: 10` or `15`.
</details>

<details>
<summary><strong>Q8: Where must GitHub Actions workflow YAML files be stored in a repository, and can they be placed in subdirectories?</strong></summary>

Workflow files must be located strictly in `.github/workflows/` at the root of the repository. They must have a `.yml` or `.yaml` extension. Subdirectories inside `.github/workflows/` (e.g. `.github/workflows/ci/test.yml`) are ignored by the GitHub workflow parser and will never trigger.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Basic Two-Job Pipeline with Dependency</strong></summary>

**Scenario**: Create `.github/workflows/two-job.yml` with a `build` job and a dependent `test` job using `needs:`.
```yaml
name: Two Job Pipeline
on: push
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - run: echo "Compilation successful!"
  test:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - run: echo "All unit tests passed!"
```
</details>

<details>
<summary><strong>Exercise 2: Implement Fail-Safe Timeouts</strong></summary>

**Scenario**: Add `timeout-minutes: 5` to ensure a job never hangs.
```yaml
jobs:
  quick-check:
    runs-on: ubuntu-latest
    timeout-minutes: 5
    steps:
      - run: echo "Running within strict 5-minute timeout window"
```
</details>

<details>
<summary><strong>Exercise 3: Configure PR Concurrency Cancellation</strong></summary>

**Scenario**: Add concurrency grouping so older PR commits cancel automatically.
```yaml
concurrency:
  group: pr-${{ github.head_ref || github.run_id }}
  cancel-in-progress: true
```
</details>

<details>
<summary><strong>Exercise 4: Multi-OS Runner Test Matrix</strong></summary>

**Scenario**: Build a matrix that executes tests across Ubuntu and Windows.
```yaml
strategy:
  matrix:
    os: [ubuntu-latest, windows-latest]
runs-on: ${{ matrix.os }}
```
</details>

<details>
<summary><strong>Exercise 5: Restrict Token Permissions to Read-Only</strong></summary>

**Scenario**: Enforce least-privilege permissions across a workflow file.
```yaml
permissions:
  contents: read
  issues: none
  pull-requests: none
```
</details>

<details>
<summary><strong>Exercise 6: Manual Trigger via `workflow_dispatch`</strong></summary>

**Scenario**: Allow running a workflow on demand with a custom text input parameter.
```yaml
on:
  workflow_dispatch:
    inputs:
      environment:
        description: 'Target Deployment Environment'
        required: true
        default: 'staging'
```
</details>

---

## 12. Summary & Cheat Sheet

```yaml
# GitHub Actions Core Syntax Cheat Sheet
name: Workflow Title
on: [push, pull_request]

permissions:
  contents: read

concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

jobs:
  job-identifier:
    runs-on: ubuntu-latest
    timeout-minutes: 15
    needs: [prior-job-identifier]
    strategy:
      matrix:
        node: [18, 20]
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: ${{ matrix.node }}
          cache: 'npm'
      - run: npm ci
      - run: npm test
        env:
          NODE_ENV: test
```

---

## 13. Further Reading
- [Official GitHub Actions Documentation](https://docs.github.com/en/actions)
- [GitHub Actions Security Hardening Guide](https://docs.github.com/en/actions/security-guides/security-hardening-for-github-actions)
- [GitHub Marketplace for Actions](https://github.com/marketplace?type=actions)
- [04-github-actions/02-workflow-syntax.md](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/02-workflow-syntax.md)
