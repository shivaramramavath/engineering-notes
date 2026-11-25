---
title: "Testing GitHub Actions Workflows Locally with act"
category: "04-devops"
subcategory: "git-github / 04-github-actions"
tags: ["github-actions", "act", "local-testing", "docker", "debugging", "ci-testing"]
difficulty: "Intermediate-Advanced"
estimated_read_time: "24 min"
prerequisites:
  - "04-github-actions/02-workflow-syntax"
  - "04-github-actions/04-jobs-and-steps"
  - "04-devops/docker/01-fundamentals"
last_updated: "2026-11-25"
---

# Testing GitHub Actions Workflows Locally with `act`

> **Navigation**: [← Previous: 22-workflow-performance-and-cost.md](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/22-workflow-performance-and-cost.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/README.md) | [Next: 24-monorepo-and-path-filters.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/24-monorepo-and-path-filters.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How `act` Works Internally](#3-how-act-works-internally)
4. [Installation & Setup](#4-installation--setup)
5. [Syntax & Commands](#5-syntax--commands)
6. [Practical Examples](#6-practical-examples)
7. [Best Practices](#7-best-practices)
8. [Common Mistakes](#8-common-mistakes)
9. [Limitations: Local vs Real GitHub Runners](#9-limitations-local-vs-real-github-runners)
10. [Comparison: Fast Feedback Loop](#10-comparison-fast-feedback-loop)
11. [Interview Questions & Answers](#11-interview-questions--answers)
12. [Practice Exercises](#12-practice-exercises)
13. [Summary & Cheat Sheet](#13-summary--cheat-sheet)
14. [Further Reading](#14-further-reading)

---

## 1. What & Why

### What is `act`?
`act` is an open-source Go-based command-line tool (created by Nektos) that parses your local `.github/workflows/` files and executes the workflow jobs locally on your development machine using Docker containers.

### Why Do We Use It?
1. **Eliminate "Git Commit Push Ping-Pong"**: Without local testing, debugging a failing CI workflow requires repeatedly creating trivial commits ("fix yaml typo", "test CI again", "debug step 4"), cluttering git history and waiting minutes for GitHub queue scheduling.
2. **Instant Feedback**: Run workflows in seconds directly from your local terminal.
3. **Offline Development**: Test build scripts and automation logic without internet access or when GitHub is experiencing outages.
4. **Secret Safety**: Test deployment and build workflows with local development secrets without uploading test keys to GitHub.

---

## 2. Core Concepts

### 1. Docker-Based Runner Emulation
`act` reads your workflow YAML, determines the `runs-on` label (e.g. `ubuntu-latest`), pulls a Docker image mimicking GitHub's runner environment, mounts your local repository code into the container, and runs each step sequentially.

### 2. Runner Image Sizes
`act` provides three levels of runner images:
- **Micro Image (~200MB)**: Node.js and basic tools only. Extremely fast to download, but lacks many pre-installed tools.
- **Medium Image (~500MB)**: Includes Git, Node, Python, and common build utilities. Recommended default.
- **Large Image (~18GB)**: Catthehacker image that closely mirrors the complete GitHub `ubuntu-latest` virtual environment (Docker, Java, Go, Rust, .NET, Ruby, Android SDK).

### 3. Event Emulation
You can tell `act` which event to trigger:
```bash
act push
act pull_request
act workflow_dispatch
```

### 4. Mocking Secrets and Environment Variables
- Secrets are provided via `.secrets` file or `-s SECRET_NAME=value`.
- Environment variables are provided via `.env` file or `--env VAR=value`.

---

## 3. How `act` Works Internally

```
                      act Execution Architecture:

 1. Developer runs: act push -j test
             │
             ▼
 2. Parse YAML from .github/workflows/*.yml
    Validates syntax and resolves job dependency graph
             │
             ▼
 3. Local Docker Engine API
    - Pulls runner image: ghcr.io/catthehacker/ubuntu:act-latest
    - Creates container with Docker socket mount (/var/run/docker.sock)
    - Mounts local repository into /workspace/<repo>
             │
             ▼
 4. Step Execution inside Container
    - Downloads referenced GitHub actions (cloned into container cache)
    - Injects mocked ${{ github.* }} context and secrets
    - Runs shell commands (run:) capturing stdout/stderr
             │
             ▼
 5. Output Streams to Local Terminal (Colorized by Job ID)
```

---

## 4. Installation & Setup

### Installation
```bash
# macOS (Homebrew)
brew install act

# Windows (Chocolatey or Scoop or winget)
winget install nektos.act
# or: choco install act-cli

# Linux (curl / bash)
curl https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash
```

### Initial Configuration (`~/.actrc`)
Create `~/.actrc` to specify default runner images:
```text
-P ubuntu-latest=catthehacker/ubuntu:act-latest
-P ubuntu-22.04=catthehacker/ubuntu:act-22.04
-P ubuntu-20.04=catthehacker/ubuntu:act-20.04
```

---

## 5. Syntax & Commands

```bash
# List all workflows and jobs in current repo without running
act -l

# Dry-run: parse workflow and print execution plan without executing
act -n

# Run all jobs triggered by 'push' event
act push

# Run only a specific job by name
act -j test

# Run a specific workflow file
act -W .github/workflows/ci.yml

# Pass secrets from file
act --secret-file .secrets

# Pass secrets directly via CLI
act -s NPM_TOKEN=npm_123456789 -s GITHUB_TOKEN=$(gh auth token)

# Pass event payload JSON (e.g. simulate a pull request)
act pull_request -e event.json

# Reuse containers between steps (faster debugging)
act -r
```

---

## 6. Practical Examples

### Example 1: Dry-Running a Complex Workflow
```bash
# Verify that YAML syntax is valid and inspect job dependencies
act -n -j deploy

# Output:
# Stage  Job ID   Job name  Workflow name  Workflow file  Events
# 0      build    Build     Deploy Pipeline deploy.yml    push
# 1      test     Test      Deploy Pipeline deploy.yml    push
# 2      deploy   Deploy    Deploy Pipeline deploy.yml    push
```

### Example 2: Testing with Local Secret Files
```bash
# 1. Create .secrets file (Add to .gitignore!)
echo "DATABASE_URL=postgres://localhost:5432/testdb" > .secrets
echo "AWS_REGION=us-east-1" >> .secrets

# 2. Run test job with local secrets
act -j test --secret-file .secrets
```

### Example 3: Simulating a Pull Request with a Payload File
```json
// pr-event.json
{
  "pull_request": {
    "number": 42,
    "head": {
      "ref": "feature/user-auth"
    },
    "base": {
      "ref": "main"
    }
  }
}
```
Run `act` with the event payload:
```bash
act pull_request -e pr-event.json
```

### Example 4: Testing an Action inside a Specific Working Directory
```bash
# Run jobs within a monorepo subdirectory
act -j test --directory services/auth
```

### Example 5: Interactive Debugging on Container Failure
```bash
# If a step fails, act keeps the container alive for inspection
act -j test --reuse
```

---

## 7. Best Practices

1. **Always Add `.secrets` and `.env` to `.gitignore`**: Never commit local secret files used for `act` testing.
2. **Use `catthehacker/ubuntu:act-latest` for High Fidelity**: Micro images miss basic dependencies like `curl`, `jq`, or Python, causing false negatives.
3. **Use `act -l` Before Executing**: Always list jobs first to confirm event mappings and job IDs.
4. **Use `-n` (Dry Run) in CI Linters**: Use `act -n` or `actionlint` in pre-commit hooks to catch syntax errors before code review.
5. **Pass Real GitHub Token via `gh auth token`**: For workflows calling the GitHub API, pass your local CLI token:
   ```bash
   act -s GITHUB_TOKEN="$(gh auth token)"
   ```
6. **Remember Architecture Differences**: If testing on an Apple Silicon (M1/M2/M3) Mac, Docker runs ARM64 containers. If actions expect x86_64 binaries, use `--container-architecture linux/amd64`.

---

## 8. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Committing `.secrets` file | Leaked credentials in Git | Missing `.gitignore` entry | Add `.secrets` to `.gitignore` immediately |
| Missing tools on micro image | Steps fail with `command not found` | Default micro image lacks packages | Use `catthehacker` image in `~/.actrc` |
| Expecting OIDC to work in `act` | Cloud authentication fails | `act` cannot mint signed GitHub OIDC tokens | Mock credentials via `.secrets` locally |
| Architecture mismatch on Apple Silicon | `exec format error` in x86 binaries | Docker runs ARM64 on Apple Silicon | Pass `--container-architecture linux/amd64` |
| Attempting to run Windows/macOS runners | Fails with unsupported OS | Docker cannot run native macOS or Windows | Map `runs-on` to Linux image or test Linux jobs |
| Forgetting Docker socket mount for Docker steps | `docker: Cannot connect to Docker daemon` | Docker-in-Docker needs host socket | Ensure Docker Desktop is running |

---

## 9. Limitations: Local vs Real GitHub Runners

| Capability | Real GitHub Runner | Local `act` Runner |
|:---|:---:|:---:|
| **Linux (`ubuntu-latest`)** | Native Hyper-V VM | Emulated Docker Container |
| **Windows Runner (`windows-latest`)** | Native Windows Server VM | **Not supported** (runs as Linux or fails) |
| **macOS Runner (`macos-latest`)** | Bare-metal Apple Mac | **Not supported** (Apple EULA prohibits) |
| **OIDC Token Minting** (`id-token: write`) | **Supported** (GitHub IdP) | **Not supported** (No GitHub IdP) |
| **GitHub API Contexts** | Full live context | Partially mocked |
| **Repository Caching** (`actions/cache`) | Global Azure Blob storage | Local disk cache (or disabled) |
| **Artifact Downloads across Jobs** | Cloud Blob Storage | Local directory mount |

---

## 10. Comparison: Fast Feedback Loop

```
Traditional CI Debugging (Commit Loop):
[Edit YAML] ──► [git commit] ──► [git push] ──► [Queue 2m] ──► [Run 5m] ──► [Fail: 7m total]
(Repeat 5 times = 35 minutes wasted!)

Local act Debugging:
[Edit YAML] ──► [act -j test] ──► [Run 15s] ──► [Pass/Fail: 15s total!]
(Saved 34+ minutes of developer time!)
```

---

## 11. Interview Questions & Answers

<details>
<summary><strong>Q1: What is `act` and how does it execute GitHub Actions workflows locally?</strong></summary>

`act` is an open-source CLI tool written in Go that reads workflow YAML files from `.github/workflows/`, parses the jobs and steps, and uses the local Docker daemon to spin up containers that mimic GitHub Actions runners (e.g. using images published by Catthehacker). It clones any external actions (`uses:`) into a local cache directory, maps the repository into the container workspace, injects mocked context and secrets, and executes the steps locally.
</details>

<details>
<summary><strong>Q2: Why cannot `act` accurately test workflows targeting `macos-latest` or `windows-latest`?</strong></summary>

`act` relies strictly on Docker containerization. Docker on Linux and macOS only supports Linux-based containers. Due to Apple's macOS licensing agreements and OS architecture, macOS cannot be containerized inside standard Docker. Windows containers require native Windows Server hosts and cannot run on Linux or macOS. Therefore, `act` maps non-Linux runners to Linux containers or fails.
</details>

<details>
<summary><strong>Q3: How do you pass encrypted secrets to `act` without putting sensitive data on the command line?</strong></summary>

Create a `.secrets` file containing key-value pairs:
```text
AWS_SECRET_ACCESS_KEY=my_key
DEPLOY_TOKEN=ghp_exampletoken
```
Ensure `.secrets` is listed in `.gitignore`, and execute:
```bash
act --secret-file .secrets
```
</details>

<details>
<summary><strong>Q4: Why does OIDC authentication (`id-token: write`) fail when executed inside `act`?</strong></summary>

OIDC in GitHub Actions relies on a live token brokering service (`ACTIONS_ID_TOKEN_REQUEST_URL` and `ACTIONS_ID_TOKEN_REQUEST_TOKEN`) that communicates with GitHub's central OIDC Identity Provider (`token.actions.githubusercontent.com`). Because `act` runs locally and has no authority to mint tokens signed by GitHub's private cryptographic keys, any step attempting to exchange a GitHub OIDC token with AWS or GCP will fail. Workflows tested locally must be provided with mock static credentials instead.
</details>

<details>
<summary><strong>Q5: What is `actionlint` and how does it complement `act` in workflow testing?</strong></summary>

`actionlint` is a static analysis linter for GitHub Actions workflow files. While `act` actually executes the steps in Docker containers, `actionlint` inspects the YAML syntax, verifies that referenced action inputs and outputs match action definitions, checks bash script syntax inside `run:` steps with ShellCheck, and flags unknown context variables—without needing Docker or running containers.
</details>

<details>
<summary><strong>Q6: How do you emulate a pull request event originating from a specific branch in `act`?</strong></summary>

By passing an event JSON payload:
```bash
act pull_request -e event.json
```
Where `event.json` contains a mocked GitHub webhook payload defining `pull_request.head.ref` and `pull_request.base.ref`.
</details>

<details>
<summary><strong>Q7: What is the purpose of the `--reuse` (or `-r`) flag in `act`?</strong></summary>

By default, `act` deletes the runner Docker container after each run. Passing `-r` instructs `act` to preserve the container between runs. This dramatically speeds up repeated test cycles because previously installed dependencies, tools, and apt packages remain cached inside the container.
</details>

<details>
<summary><strong>Q8: How do you resolve `exec format error` when running `act` on an Apple Silicon (M1/M2/M3) Mac?</strong></summary>

This occurs when a step or pre-compiled binary inside the action was compiled for x86_64 (amd64) architecture, but Docker on Apple Silicon defaults to arm64 containers.
Pass the `--container-architecture` flag:
```bash
act --container-architecture linux/amd64
```
Docker Desktop will use Rosetta 2 emulation to execute the x86_64 container.
</details>

---

## 12. Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Local Workflow Inventory with `act -l`</strong></summary>

**Scenario**: List all available workflows and jobs in your repository.
```bash
act -l
```
</details>

<details>
<summary><strong>Exercise 2: Dry-Run a Workflow</strong></summary>

**Scenario**: Verify workflow execution plan without launching Docker containers.
```bash
act -n -j test
```
</details>

<details>
<summary><strong>Exercise 3: Test a Single Job with Inline Secret</strong></summary>

**Scenario**: Run job `lint` passing a custom API key.
```bash
act -j lint -s API_KEY="mock_key_12345"
```
</details>

<details>
<summary><strong>Exercise 4: Configure Default Runner Images in `~/.actrc`</strong></summary>

**Scenario**: Create `~/.actrc` configuring medium catthehacker image.
```bash
cat << 'EOF' > ~/.actrc
-P ubuntu-latest=catthehacker/ubuntu:act-latest
-P ubuntu-22.04=catthehacker/ubuntu:act-22.04
EOF
```
</details>

<details>
<summary><strong>Exercise 5: Lint Workflow Syntax with `actionlint`</strong></summary>

**Scenario**: Run static analysis across `.github/workflows/`.
```bash
# Install via go or brew: brew install actionlint
actionlint
```
</details>

<details>
<summary><strong>Exercise 6: Test Local Monorepo Subdirectory Build</strong></summary>

**Scenario**: Execute a job targeting a monorepo subfolder.
```bash
act -j test -W .github/workflows/service.yml
```
</details>

---

## 13. Summary & Cheat Sheet

```bash
# act CLI Essential Commands
act -l                               # List all jobs
act -n                               # Dry run (verify YAML and print plan)
act push                             # Run push event workflows
act pull_request                     # Run pull_request event workflows
act -j <job-id>                      # Run specific job by ID
act -W <path-to-workflow.yml>        # Run specific workflow file
act --secret-file .secrets           # Inject secrets from file
act -s GITHUB_TOKEN="$(gh auth token)" # Use current GitHub CLI session token
act --reuse                          # Do not destroy container on exit (faster)
act --container-architecture linux/amd64 # Emulate x86 on Apple Silicon
```

---

## 14. Further Reading
- [nektos/act Official Repository](https://github.com/nektos/act)
- [catthehacker/docker-images for act](https://github.com/catthehacker/docker-images)
- [actionlint - Static Checker for GitHub Workflows](https://github.com/rhysd/actionlint)
- [04-github-actions/24-monorepo-and-path-filters.md](file:///e:/Learning/engineering-notes/04-devops/git-github/04-github-actions/24-monorepo-and-path-filters.md)
