---
title: "Custom Actions: Composite vs JavaScript vs Docker Actions"
category: "04-devops"
subcategory: "git-github / 04-github-actions"
tags: ["github-actions", "custom-actions", "composite-actions", "javascript-actions", "docker-actions", "marketplace"]
difficulty: "Advanced"
estimated_read_time: "30 min"
prerequisites:
  - "04-github-actions/05-actions-and-marketplace"
  - "04-github-actions/13-custom-actions"
last_updated: "2026-11-25"
---

# Custom Actions: Composite vs JavaScript vs Docker Actions

> **Navigation**: [← Previous: 24-monorepo-and-path-filters.md](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/24-monorepo-and-path-filters.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/README.md) | [Next: 26-supply-chain-pinning-and-provenance.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/26-supply-chain-pinning-and-provenance.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Architectural Overview](#2-core-architectural-overview)
3. [Type 1: Composite Actions](#3-type-1-composite-actions)
4. [Type 2: JavaScript / TypeScript Actions](#4-type-2-javascript--typescript-actions)
5. [Type 3: Docker Container Actions](#5-type-3-docker-container-actions)
6. [Action Lifecycle Hooks: pre, main, and post](#6-action-lifecycle-hooks-pre-main-and-post)
7. [Practical Examples](#7-practical-examples)
8. [Best Practices & Marketplace Publishing](#8-best-practices--marketplace-publishing)
9. [Common Mistakes](#9-common-mistakes)
10. [Performance, Security & Cost Comparison](#10-performance-security--cost-comparison)
11. [The Definitive Architectural Matrix](#11-the-definitive-architectural-matrix)
12. [Interview Questions & Answers](#12-interview-questions--answers)
13. [Practice Exercises](#13-practice-exercises)
14. [Summary & Cheat Sheet](#14-summary--cheat-sheet)
15. [Further Reading](#15-further-reading)

---

## 1. What & Why

### What are Custom GitHub Actions?
Custom Actions are reusable, packaged automation components that encapsulate multi-step workflows, scripts, or containerized applications into a single portable entity callable via `uses: owner/repo@v1` or `uses: ./.github/actions/my-action`.

### Why Do We Build Custom Actions?
1. **DRY (Don't Repeat Yourself) Across Repositories**: Replace 80 lines of repetitive setup, authentication, and build scripts across 50 repositories with a single 3-line action invocation.
2. **Standardized Security Controls**: Enforce consistent security scanning, linting, and compliance logging across enterprise projects.
3. **Sharing with the Community**: Distribute open-source developer tooling via the GitHub Marketplace.
4. **Three Specialized Types**: GitHub provides three distinct implementation paradigms—**Composite**, **JavaScript**, and **Docker**—each tailored for specific operating systems, execution speed requirements, and complexity levels.

---

## 2. Core Architectural Overview

```
                      The 3 Action Architectures:

1. Composite Action (YAML + Shell):
   [ action.yml ] ──► Executes sequential shell steps directly on Runner OS
                      • Cross-platform (Linux, Windows, macOS)
                      • No compilation needed

2. JavaScript Action (Node.js runtime):
   [ action.yml + dist/index.js ] ──► Runner invokes built-in Node 20 runtime
                                      • Fastest execution (zero startup overhead)
                                      • Rich @actions/core and @actions/github SDK
                                      • Cross-platform

3. Docker Container Action (Dockerfile):
   [ action.yml + Dockerfile ] ──► Runner builds/pulls image and runs container
                                   • Fully isolated custom Linux environment
                                   • Any runtime/binary (Go, Rust, Python, Bash)
                                   • LINUX RUNNERS ONLY!
```

---

## 3. Type 1: Composite Actions

### What is a Composite Action?
A Composite Action bundles multiple workflow steps (shell commands and other actions) into a single `action.yml` file using `using: "composite"`.

### Key Characteristics:
- Pure YAML syntax.
- Every `run:` step must specify `shell:` explicitly (e.g. `shell: bash`).
- Supports inputs, outputs, and calling other actions (`uses:`).

### Syntax (`action.yml`):
```yaml
name: "Setup Node and Cache Dependencies"
description: "Standardized Node 20 setup with npm clean install"
inputs:
  node-version:
    description: "Target Node.js version"
    required: false
    default: "20"
outputs:
  cache-hit:
    description: "Whether dependencies were loaded from cache"
    value: ${{ steps.setup-node.outputs.cache-hit }}

runs:
  using: "composite"
  steps:
    - name: Set up Node.js
      id: setup-node
      uses: actions/setup-node@v4
      with:
        node-version: ${{ inputs.node-version }}
        cache: 'npm'

    - name: Install dependencies
      shell: bash
      run: npm ci
```

---

## 4. Type 2: JavaScript / TypeScript Actions

### What is a JavaScript Action?
A JavaScript Action executes directly within the runner's pre-installed Node.js runtime (Node 20 in modern runners).

### Key Characteristics:
- Direct access to `@actions/core` (inputs, outputs, secrets, logging) and `@actions/github` (Octokit REST/GraphQL API).
- Extremely fast execution with zero container launch overhead.
- Must bundle all `node_modules` into a single standalone file (e.g. `dist/index.js`) using `@vercel/ncc` or `esbuild` so runners don't need to run `npm install`.

### Syntax (`action.yml`):
```yaml
name: "Issue Labeler"
description: "Automatically labels issues based on keywords"
inputs:
  github-token:
    description: "GitHub Token for API access"
    required: true
  label-name:
    description: "Label to attach"
    required: true

runs:
  using: "node20"
  main: "dist/index.js"
  post: "dist/cleanup.js" # Optional post-job cleanup hook
```

### Implementation (`src/index.ts`):
```typescript
import * as core from '@actions/core';
import * as github from '@actions/github';

async function run(): Promise<void> {
  try {
    const token = core.getInput('github-token', { required: true });
    const label = core.getInput('label-name', { required: true });
    const octokit = github.getOctokit(token);
    const context = github.context;

    if (!context.payload.issue) {
      core.setFailed('This action only runs on issue events.');
      return;
    }

    await octokit.rest.issues.addLabels({
      owner: context.repo.owner,
      repo: context.repo.repo,
      issue_number: context.payload.issue.number,
      labels: [label],
    });

    core.info(`Successfully added label: ${label}`);
    core.setOutput('applied-label', label);
  } catch (error) {
    if (error instanceof Error) core.setFailed(error.message);
  }
}

run();
```

---

## 5. Type 3: Docker Container Actions

### What is a Docker Container Action?
A Docker Action packages the entire execution environment—including binaries, tools, OS libraries, and scripts written in any language (Go, Python, Rust, Bash)—inside a container image defined by a `Dockerfile`.

### Key Characteristics:
- **Strict Limitation**: **Linux runners only!** (`ubuntu-latest` or Linux self-hosted). Cannot run on macOS or Windows runners.
- True filesystem and dependency isolation.
- Slow startup if building the image from scratch; fast if pointing to a pre-built image on GitHub Packages / Docker Hub (`image: 'docker://ghcr.io/org/tool:v1'`).

### Syntax (`action.yml`):
```yaml
name: "Golang Linter & Scanner"
description: "Runs custom Golang vulnerability scanner"
inputs:
  rules-file:
    description: "Path to rules yaml"
    required: true
    default: ".rules.yml"

runs:
  using: "docker"
  image: "Dockerfile" # or "docker://ghcr.io/my-org/scanner:v1.2.0"
  args:
    - "--rules"
    - ${{ inputs.rules-file }}
```

### `Dockerfile`:
```dockerfile
FROM golang:1.22-alpine
RUN apk add --no-cache git ca-certificates
WORKDIR /app
COPY . .
RUN go build -o /bin/scanner main.go
ENTRYPOINT ["/bin/scanner"]
```

---

## 6. Action Lifecycle Hooks: pre, main, and post

Both JavaScript and Docker actions support three distinct lifecycle phases:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. pre: Runs BEFORE any workflow steps execute              │
│    (e.g., download binaries, start daemon, initialize auth) │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. main: Runs when the step is invoked in workflow          │
│    (e.g., execute main application logic)                   │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. post: Runs at the VERY END of the job (even on failure!) │
│    (e.g., stop daemon, upload cache tarball, clean tokens)  │
└─────────────────────────────────────────────────────────────┘
```

In `action.yml`:
```yaml
runs:
  using: "node20"
  pre: "dist/pre.js"
  main: "dist/index.js"
  post: "dist/post.js"
  post-if: "always()" # Runs even if earlier steps failed
```

---

## 7. Practical Examples

### Example 1: Local Composite Action in Repository (`.github/actions/`)
```yaml
# In workflow file: .github/workflows/ci.yml
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      # Call internal repository action directly via relative path:
      - name: Setup environment and lint
        uses: ./.github/actions/setup-and-lint
        with:
          node-version: '20'
```

### Example 2: Compiling & Bundling a JavaScript Action with `@vercel/ncc`
```bash
# 1. Initialize project
npm init -y
npm install @actions/core @actions/github
npm install -D typescript @vercel/ncc @types/node

# 2. Compile TypeScript and bundle all dependencies into single JS file
npx ncc build src/index.ts -o dist --minify

# 3. Commit dist/index.js to Git! (Required so runners don't run npm install)
git add dist/index.js action.yml
git commit -m "chore: build standalone distribution bundle"
```

### Example 3: Docker Action Using Pre-Built Image
```yaml
name: "AWS S3 Sync Action"
description: "Syncs directory to S3 bucket using pre-built AWS CLI image"
inputs:
  source:
    required: true
  bucket:
    required: true

runs:
  using: "docker"
  image: "docker://amazon/aws-cli:2.15.15"
  args:
    - "s3"
    - "sync"
    - ${{ inputs.source }}
    - ${{ inputs.bucket }}
```

---

## 8. Best Practices & Marketplace Publishing

1. **Commit the Compiled `dist/` Directory**: For JavaScript actions, always commit the bundled `dist/index.js` file generated by `@vercel/ncc`. GitHub runners will not run `npm install`.
2. **Support Semantic Version Tags (`v1`, `v1.2.0`)**: Maintain major version floating tags (`v1`, `v2`) pointing to the latest patch release so users can write `uses: my-org/my-action@v1`.
3. **Always Set `shell:` in Composite Actions**: In composite actions, omission of `shell: bash` results in syntax parsing errors.
4. **Use Pre-Built Docker Images for Docker Actions**: Avoid `image: Dockerfile` in production actions because every workflow run will spend 1-3 minutes rebuilding the image from scratch. Publish to GitHub Container Registry (`ghcr.io`) and reference `image: docker://ghcr.io/...`.
5. **Mask Sensitive Outputs**: If your action generates tokens or keys, always mask them using `core.setSecret(token)` to prevent accidental logging.
6. **Include Clear Input Defaults**: Provide sensible defaults for optional inputs in `action.yml`.

---

## 9. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Omission of `shell:` in composite action | `action.yml: run step missing shell` | Composite steps do not inherit job default shell | Explicitly add `shell: bash` to every `run:` step |
| Forgetting to commit `dist/` | `Cannot find module ...` error on runner | `node_modules` are gitignored and not installed by runner | Bundle with `@vercel/ncc` and commit `dist/` |
| Running Docker action on macOS/Windows | `Docker actions are only supported on Linux` | OS architectural limitation | Use Composite or JavaScript action |
| Using `node16` runtime | GitHub deprecation warnings; runner failures | Node 16 runtime end-of-life | Upgrade to `using: "node20"` |
| Building Dockerfile on every run | Wasted 2–5 minutes per build | Using `image: Dockerfile` | Publish image to GHCR and use `docker://` |
| Breaking changes on floating `@v1` tag | Breaks all downstream consumer workflows | Pushing breaking update to major tag | Release breaking updates under `@v2` |

---

## 10. Performance, Security & Cost Comparison

- **Startup Latency**:
  - **JavaScript Action**: **~0.1 seconds** (Runner executes compiled Node.js script in-process).
  - **Composite Action**: **~0.2 seconds** (Runner executes shell subshell processes).
  - **Docker Action (Pre-built)**: **~5 to 15 seconds** (Docker pull and container launch).
  - **Docker Action (Built on-the-fly)**: **~60 to 180 seconds** (`docker build` every single job!).
- **Security Scrutiny**: Custom actions run with the caller workflow's `GITHUB_TOKEN` permissions and environment secrets. Auditing JavaScript actions requires inspecting `dist/index.js` or using automated build provenance attestations.

---

## 11. The Definitive Architectural Matrix

| Feature | Composite Actions | JavaScript Actions | Docker Container Actions |
|:---|:---:|:---:|:---:|
| **Runtime Engine** | System Shell (`bash`, `pwsh`) | Built-in Node 20 | Docker Container Engine |
| **Supported OS** | Linux, macOS, Windows | Linux, macOS, Windows | **Linux Only** |
| **Execution Speed** | Fast (~0.2s startup) | **Fastest (~0.1s startup)** | Slower (5–120s container init) |
| **Dependencies** | Must exist on host OS | Bundled in `dist/index.js` | Built into container image |
| **GitHub SDK Access** | CLI only (`gh` or `curl`) | **Native Octokit (`@actions/github`)** | CLI / language SDKs |
| **Lifecycle Hooks (`post`)**| No | **Yes (`pre`, `main`, `post`)** | **Yes (`pre`, `main`, `post`)** |
| **Best Used For** | Multi-step shell scripts | Fast API tools & integrations | Heavy custom CLI/binary environments |

---

## 12. Interview Questions & Answers

<details>
<summary><strong>Q1: What are the primary differences between Composite Actions, JavaScript Actions, and Docker Container Actions?</strong></summary>

1. **Composite Actions**: Written in YAML. They combine shell commands and other actions. They run directly on the runner host across Linux, macOS, and Windows.
2. **JavaScript Actions**: Written in JavaScript/TypeScript and run directly on the runner's built-in Node.js runtime (Node 20). They are cross-platform, have zero container startup overhead, and provide direct access to official `@actions/core` and `@actions/github` libraries.
3. **Docker Actions**: Encapsulate tools inside a Docker image. They allow developers to use any programming language (Python, Go, Rust, Ruby) and custom OS packages. However, they are strictly limited to Linux runners and incur container startup overhead.
</details>

<details>
<summary><strong>Q2: Why must JavaScript Actions commit their compiled bundle (`dist/index.js`) to Git?</strong></summary>

When a workflow invokes an action (`uses: owner/repo@v1`), GitHub Actions clones the action repository onto the runner. However, GitHub Actions does **not** run `npm install` inside the action's folder. If dependencies (like `@actions/core`) reside in `node_modules` and are not committed, the action fails with `MODULE_NOT_FOUND`. Using a bundler like `@vercel/ncc` compiles all source code and transitive dependencies into a single, self-contained `dist/index.js` file that executes immediately without runtime package installation.
</details>

<details>
<summary><strong>Q3: Why do Docker Container Actions only run on Linux runners?</strong></summary>

GitHub-hosted macOS and Windows runners do not provide a native Linux container virtualization layer compatible with GitHub Actions Docker container jobs. macOS runs on bare-metal Apple hardware where Docker is not installed by default, and Windows runners use native Windows Server containers. Because Docker Actions default to Linux base images (like Alpine or Ubuntu), they can only execute on Linux runner hosts.
</details>

<details>
<summary><strong>Q4: What is the purpose of the `post:` lifecycle hook in a custom action?</strong></summary>

The `post:` hook defines a script that executes at the very end of the job, after all workflow steps have finished, even if subsequent steps failed. It is primarily used for resource teardown, cleaning up temporary auth tokens, logging out of container registries, or compressing and saving cache files (as done by `actions/cache`).
</details>

<details>
<summary><strong>Q5: Why is `shell:` mandatory on every `run:` step within a Composite Action?</strong></summary>

In regular workflows, jobs default to the host's platform default shell (e.g. `bash` on Linux/macOS, `cmd` or `pwsh` on Windows) or a workflow-level `defaults.run.shell`. In composite actions, steps are executed in isolation and do not inherit the calling workflow's shell defaults. To prevent ambiguous cross-platform execution failures, GitHub requires every `run:` step in a composite action to explicitly declare its shell (e.g. `shell: bash`).
</details>

<details>
<summary><strong>Q6: How should major and minor version tags be managed when releasing actions on the GitHub Marketplace?</strong></summary>

Follow the floating major tag pattern:
1. Release an exact semantic version tag: `v1.2.3`.
2. Move the major floating tag pointer (`v1`) to point to `v1.2.3`:
   ```bash
   git tag -fa v1 -m "Update v1 tag to v1.2.3"
   git push origin v1 --force
   ```
This allows users to specify `uses: org/action@v1` and automatically receive non-breaking bug fixes without breaking their workflows.
</details>

<details>
<summary><strong>Q7: How do you mask sensitive strings generated inside a custom JavaScript action?</strong></summary>

Using the `@actions/core` library:
```typescript
import * as core from '@actions/core';

const secretToken = generateToken();
core.setSecret(secretToken); // Masks token in all subsequent runner logs with '***'
core.setOutput('token', secretToken);
```
</details>

<details>
<summary><strong>Q8: What performance penalty occurs when a Docker Action references `image: 'Dockerfile'` instead of a published registry image?</strong></summary>

When `image: 'Dockerfile'` is specified, the GitHub Actions runner must execute `docker build` from scratch on **every single workflow run**, downloading base images and compiling dependencies. This adds 1 to 5 minutes of latency to every build. Publishing the pre-built image to GitHub Container Registry (`image: 'docker://ghcr.io/org/tool:v1'`) allows the runner to simply pull cached layers in a few seconds.
</details>

---

## 13. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Local Composite Action</strong></summary>

**Scenario**: Create `.github/actions/greeting/action.yml` that takes a name input and prints a greeting.
```yaml
name: "Greeting Action"
description: "Prints personalized greeting"
inputs:
  who:
    description: "Who to greet"
    required: true
    default: "World"
runs:
  using: "composite"
  steps:
    - shell: bash
      run: echo "Hello, ${{ inputs.who }}!"
```
Test in a workflow via `uses: ./.github/actions/greeting`.
</details>

<details>
<summary><strong>Exercise 2: Build a Standalone JS Action with `@vercel/ncc`</strong></summary>

**Scenario**: Setup and bundle a JavaScript action.
```bash
mkdir my-action && cd my-action && npm init -y
npm install @actions/core
cat << 'EOF' > index.js
const core = require('@actions/core');
core.info("Custom JS Action Executing!");
EOF

npx @vercel/ncc build index.js -o dist
ls -la dist/
```
</details>

<details>
<summary><strong>Exercise 3: Define Action Lifecycle Hooks in `action.yml`</strong></summary>

**Scenario**: Configure pre, main, and post execution hooks.
```yaml
name: "Lifecycle Demo"
runs:
  using: "node20"
  pre: "dist/pre.js"
  main: "dist/main.js"
  post: "dist/post.js"
  post-if: "always()"
```
</details>

<details>
<summary><strong>Exercise 4: Create a Docker Action with Pre-Built Image</strong></summary>

**Scenario**: Wrap `alpine/git` as a Docker action.
```yaml
name: "Git Status Docker Action"
runs:
  using: "docker"
  image: "docker://alpine/git:latest"
  args: ["status"]
```
</details>

<details>
<summary><strong>Exercise 5: Mask Secrets in JavaScript Action</strong></summary>

**Scenario**: Generate and mask a secret in Node.js.
```javascript
const core = require('@actions/core');
const password = "supersecretpassword123";
core.setSecret(password);
core.info(`Password is: ${password}`); // Logs: Password is: ***
```
</details>

<details>
<summary><strong>Exercise 6: Update Major Version Floating Tag</strong></summary>

**Scenario**: Point tag `v1` to latest release `v1.1.0`.
```bash
git tag -fa v1 -m "Update v1 to v1.1.0"
git push origin v1 --force
```
</details>

---

## 14. Summary & Cheat Sheet

```yaml
# 1. COMPOSITE ACTION (Multi-step shell scripts)
runs:
  using: "composite"
  steps:
    - shell: bash
      run: echo "Step 1"

# 2. JAVASCRIPT ACTION (Fastest, Cross-Platform)
runs:
  using: "node20"
  main: "dist/index.js"
  post: "dist/cleanup.js"

# 3. DOCKER ACTION (Linux only, Custom isolated environment)
runs:
  using: "docker"
  image: "docker://ghcr.io/org/scanner:v1"
  args: ["--scan", "."]
```

---

## 15. Further Reading
- [GitHub Docs - About Custom Actions](https://docs.github.com/en/actions/creating-actions/about-custom-actions)
- [Creating a Composite Action Guide](https://docs.github.com/en/actions/creating-actions/creating-a-composite-action)
- [Official GitHub Actions Toolkit](https://github.com/actions/toolkit)
- [04-github-actions/26-supply-chain-pinning-and-provenance.md](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/26-supply-chain-pinning-and-provenance.md)
