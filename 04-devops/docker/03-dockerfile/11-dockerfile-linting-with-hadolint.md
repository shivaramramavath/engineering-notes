# Dockerfile Static Analysis: Linting and Policy Enforcement with Hadolint

```
Track: DevOps & Containerization
Module: 03-dockerfile
Prerequisites: 03-dockerfile/01-dockerfile-basics.md, 03-dockerfile/02-instructions-reference.md
Difficulty: Intermediate
Estimated Duration: 40 minutes
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Configuration Reference](#4-syntax--configuration-reference)
5. [Essential Hadolint Rules Catalog](#5-essential-hadolint-rules-catalog)
6. [Practical Implementation & Examples](#6-practical-implementation--examples)
7. [Production Best Practices & Antipatterns](#7-production-best-practices--antipatterns)
8. [Common Pitfalls & Troubleshooting](#8-common-pitfalls--troubleshooting)
9. [Performance Tuning & Resource Optimization](#9-performance-tuning--resource-optimization)
10. [Security Hardening & Threat Model](#10-security-hardening--threat-model)
11. [Architectural Comparison Table](#11-architectural-comparison-table)
12. [Real-World Scenario Walkthrough](#12-real-world-scenario-walkthrough)
13. [Self-Assessment & Interview Questions](#13-self-assessment--interview-questions)
14. [Hands-on Practice Exercises](#14-hands-on-practice-exercises)
15. [Summary & Reference Cheat Sheet](#summary--reference-cheat-sheet)

---

## 1. What & Why

Dockerfiles are code. Without automated static analysis and linting, bad practices inevitably slip into production images:
- Unpinned package manager commands causing broken builds months later.
- Leaving package manager caches (`/var/lib/apt/lists/*`) in image layers, inflating image size by hundreds of megabytes.
- Forgetting to declare a non-root `USER`.
- Unintended shell interpretation bugs and improper signal trapping in `RUN` instructions.

**Hadolint** (Haskell Dockerfile Linter) is the de facto industry standard static analysis tool for Dockerfiles. It parses the Dockerfile into an Abstract Syntax Tree (AST) and leverages **ShellCheck** to lint the Bash/POSIX shell commands executed within `RUN` directives.

---

## 2. Core Concepts

### Rule Architecture: DL vs SC
Hadolint evaluates two distinct families of static analysis rules:
1. **`DLxxxx` Rules (Dockerfile Linter)**: Enforces Dockerfile structure, instruction ordering, best practices, and security guidelines (e.g., `DL3007: Using latest is prone to errors`).
2. **`SCxxxx` Rules (ShellCheck)**: Analyzes the bash/sh code inside `RUN` directives for quoting bugs, unhandled exit codes, useless cats, and bashisms (e.g., `SC2086: Double quote to prevent globbing and word splitting`).

### Severity Levels
Every Hadolint violation is categorized by severity:
- **`error`**: Security vulnerabilities or catastrophic syntax flaws.
- **`warning`**: Strong anti-patterns, cache bloat, or reproducibility failures.
- **`info`**: Style recommendations and idiomatic adjustments.
- **`style`**: Formatting suggestions.

---

## 3. How It Works Internally

### Parsing Engine Architecture

```
+-------------------------------------------------------------+
|                       Dockerfile                            |
+------------------------------+------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|                  Language.Docker Parser                     |
|           (Parses instructions into typed AST)              |
+------------------------------+------------------------------+
                               |
              +----------------+----------------+
              |                                 |
              v                                 v
+-----------------------------+   +---------------------------+
|     Hadolint Rule Engine    |   |     ShellCheck Engine     |
|  - Validates FROM, COPY,    |   |  - Extracts RUN commands  |
|    USER, EXPOSE directives  |   |  - Parses Bash/POSIX AST  |
|  - Checks DL rules          |   |  - Checks SC rules        |
+-----------------------------+   +---------------------------+
              |                                 |
              +----------------+----------------+
                               |
                               v
+-------------------------------------------------------------+
|                      Aggregated Report                      |
|       (CLI stdout, JSON, SARIF, or exit code status)        |
+-------------------------------------------------------------+
```

Hadolint does not execute your container; it analyzes source text statically in milliseconds, making it ideal for pre-commit hooks and PR gate checks.

---

## 4. Syntax & Configuration Reference

### CLI Usage

```bash
# Lint a single Dockerfile
hadolint Dockerfile

# Lint via Docker container (no installation required)
docker run --rm -i hadolint/hadolint < Dockerfile

# Specify configuration file
hadolint --config .hadolint.yaml Dockerfile

# Output SARIF format for GitHub Security Code Scanning
hadolint --format sarif Dockerfile > results.sarif

# Enforce failure on warnings or higher
hadolint --failure-threshold warning Dockerfile
```

### Configuration File (`.hadolint.yaml`)

```yaml
# Strict configuration example
failure-threshold: warning
no-fail: false

# Override rule severities
override:
  error:
    - DL3002 # Do not use root user
    - DL3007 # Using latest tag
  warning:
    - DL3008 # Pin versions in apt get install
    - DL3018 # Pin versions in apk add

# Globally ignored rules
ignored:
  - DL3059 # Multiple consecutive RUN instructions (often needed for readability)
  - SC2015 # Note that A && B || C is not if-then-else

# Trusted registries for FROM instructions
trustedRegistries:
  - docker.io
  - gcr.io
  - ghcr.io
  - "*.mycompany.internal"
```

### Inline Rule Suppression

```dockerfile
# hadolint ignore=DL3008,DL3009
RUN apt-get update && apt-get install -y curl

# Suppress on stage base image
# hadolint ignore=DL3007
FROM node:latest AS builder
```

---

## 5. Essential Hadolint Rules Catalog

| Rule Code | Severity | Description | Fix Requirement |
|:---|:---|:---|:---|
| **`DL3002`** | Error | `Last USER should not be root` | Add `USER <non-root-uid>` before the end of the Dockerfile. |
| **`DL3003`** | Warning | `Use WORKDIR to switch to a directory` | Replace `RUN cd /app && ...` with `WORKDIR /app`. |
| **`DL3006`** | Warning | `Always tag the version of an image explicitly` | Replace `FROM ubuntu` with `FROM ubuntu:24.04`. |
| **`DL3007`** | Warning | `Using latest is prone to errors` | Pin an explicit version tag or digest instead of `:latest`. |
| **`DL3008`** | Warning | `Pin versions in apt get install` | Specify package versions: `apt-get install -y package=1.2.3*`. |
| **`DL3009`** | Info | `Delete the apt-get lists after installing something`| Append `&& rm -rf /var/lib/apt/lists/*`. |
| **`DL3015`** | Info | `Avoid additional packages by specifying --no-install-recommends`| Always include `--no-install-recommends` in `apt-get install`. |
| **`DL3018`** | Warning | `Pin versions in apk add` | Use `apk add --no-cache package=1.2.3`. |
| **`DL3019`** | Info | `Use the --no-cache switch with apk add` | Use `apk add --no-cache` instead of updating apk index manually. |
| **`DL3020`** | Error | `Use COPY instead of ADD for files and folders` | Replace `ADD` with `COPY` unless extracting archives. |
| **`DL3025`** | Warning | `Use arguments JSON notation for CMD and ENTRYPOINT` | Replace shell form `CMD node server.js` with `CMD ["node", "server.js"]`. |
| **`SC2086`** | Warning | `Double quote to prevent globbing and word splitting` | Quote shell variables: `"$MY_VAR"` instead of `$MY_VAR`. |

---

## 6. Practical Implementation & Examples

### Example 1: Refactoring a Broken Dockerfile Against Hadolint

**Defective Dockerfile:**
```dockerfile
FROM ubuntu:latest
RUN apt-get update
RUN apt-get install -y python3 curl
RUN cd /tmp && curl -O https://example.com/tool.tar.gz && tar -xzf tool.tar.gz
ADD app.py /app/app.py
CMD python3 /app/app.py
```

**Hadolint Findings:**
```
Dockerfile:1 DL3007 warning: Using latest is prone to errors
Dockerfile:2 DL3009 info: Delete the apt-get lists after installing something
Dockerfile:2 DL3059 info: Multiple consecutive `RUN` instructions
Dockerfile:3 DL3008 warning: Pin versions in apt get install
Dockerfile:3 DL3015 info: Avoid additional packages by specifying --no-install-recommends
Dockerfile:4 DL3003 warning: Use WORKDIR to switch to a directory
Dockerfile:5 DL3020 error: Use COPY instead of ADD for files and folders
Dockerfile:6 DL3025 warning: Use arguments JSON notation for CMD and ENTRYPOINT
Dockerfile:6 DL3002 error: Last USER should not be root
```

**Compliant, Refactored Dockerfile:**
```dockerfile
FROM ubuntu:24.04

# Consolidate RUN layers, specify --no-install-recommends, clean lists
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3=3.12.* \
    curl=8.5.* \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp
RUN curl -fsSL -O https://example.com/tool.tar.gz \
 && tar -xzf tool.tar.gz \
 && rm tool.tar.gz

WORKDIR /app
COPY app.py ./

# Create non-root user and drop root privileges
RUN useradd -u 10001 -s /sbin/nologin appuser
USER 10001

CMD ["python3", "/app/app.py"]
```

### Example 2: GitHub Actions CI Workflow with SARIF Upload

```yaml
name: Dockerfile Security & Linting

on:
  pull_request:
    paths:
      - '**/Dockerfile*'
      - '.hadolint.yaml'

jobs:
  hadolint:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout Code
        uses: actions/checkout@v4

      - name: Run Hadolint
        uses: hadolint/hadolint-action@v3.1.0
        with:
          dockerfile: Dockerfile
          config: .hadolint.yaml
          format: sarif
          output-file: hadolint-results.sarif
          failure-threshold: warning

      - name: Upload SARIF report to GitHub Security
        uses: github/codeql-action/upload-sarif@v3
        if: always()
        with:
          sarif_file: hadolint-results.sarif
```

### Example 3: Pre-commit Hook Integration

In `.pre-commit-config.yaml`:
```yaml
repos:
  - repo: https://github.com/hadolint/hadolint
    rev: v2.12.0
    hooks:
      - id: hadolint
        args: ["--config", ".hadolint.yaml"]
```

---

## 7. Production Best Practices & Antipatterns

### Best Practices
- **Gate Merges with Hadolint in CI**: Never permit merging a PR that introduces new `error` or `warning` level Hadolint violations.
- **Whitelist Registries**: Configure `trustedRegistries` in `.hadolint.yaml` to prevent developers from pulling untrusted or personal Docker Hub images into enterprise codebases.
- **Consolidate Package Installation**: Chain `update`, `install`, and cleanup into a single atomic `RUN` instruction.
- **Document Necessary Suppressions**: If a rule must be ignored (e.g., pinning minor system packages that frequently rotate out of Debian mirrors), document the inline comment explaining why.

### Anti-Patterns
- **Globally Disabling DL3008 and DL3018**: Ignoring package pinning wholesale leads to non-reproducible builds where containers fail to build six months later when upstream repos delete versions.
- **Using `# hadolint ignore` to Mask Real Bugs**: Silencing ShellCheck warnings (e.g., ignoring missing quotes) leads to obscure runtime failures during shell variable expansion.

---

## 8. Common Pitfalls & Troubleshooting

| Symptom | Root Cause | Diagnosis Command | Permanent Fix |
|:---|:---|:---|:---|
| Hadolint fails on BuildKit heredocs (`RUN <<EOF`) | Older Hadolint version (< 2.12) lacks syntax support for heredocs | `hadolint --version` | Upgrade Hadolint to >= 2.12.0. |
| `apt-get install` fails with `Version not found` after strict DL3008 pinning | Debian/Ubuntu package archives remove old patch versions when new security patches release | Inspect `apt-cache madison <package>` | Pin with wildcards: `curl=8.5.*` or maintain an internal deb package mirror. |
| Hadolint flags `DL3002` on intermediate build stages | Final stage has `USER nonroot`, but builder stage ran as root | Inspect line number of reported DL3002 | Ensure `USER` is set in the **final** runtime stage (Hadolint only requires the last stage to drop root). |
| Pre-commit hook takes 10+ seconds per commit | Running Hadolint via `docker run` inside pre-commit spawns a full Docker container on every commit | Check `time pre-commit run hadolint` | Install native Hadolint binary locally (`brew install hadolint` or direct binary download). |
| ShellCheck flags valid Bash syntax in `RUN` | Default shell in Hadolint is POSIX `/bin/sh` which doesn't support Bash arrays | ShellCheck flags SC3030 (In POSIX sh, arrays are undefined) | Declare `SHELL ["/bin/bash", "-o", "pipefail", "-c"]` before the `RUN` command. |
| Hadolint rejects custom internal registry in `FROM` | Image originates from `internal-artifactory.corp` which is not in the whitelist | Check `.hadolint.yaml` | Add internal registry domain to `trustedRegistries` in `.hadolint.yaml`. |

---

## 9. Performance Tuning & Resource Optimization

- **Native Binary Execution**: Running Hadolint as a compiled static Haskell binary executes in ~15 milliseconds. Running via `docker run hadolint/hadolint` incurs 1-2 seconds of container startup overhead. In high-frequency developer inner loops, always use the native binary.
- **Pipefail Enforcement**: Enforcing `SHELL ["/bin/bash", "-o", "pipefail", "-c"]` prevents silent command pipeline failures (e.g., `curl | tar` where curl fails but tar exits 0), avoiding corrupted image layers.

---

## 10. Security Hardening & Threat Model

Hadolint acts as the first line of defense against supply chain poisoning and privilege escalation:
1. **Prevents Root Execution (`DL3002`)**: Enforces dropped privileges, preventing container breakout attacks.
2. **Mitigates Man-in-the-Middle Attacks (`DL3020`)**: Restricts `ADD` from fetching unverified remote URLs over unauthenticated HTTP.
3. **Ensures Reproducibility (`DL3006`, `DL3007`, `DL3008`)**: Eliminates floating tags, preventing surprise breaking changes or malicious upstream tag updates.

---

## 11. Architectural Comparison Table

| Tool | Focus | Analysis Mode | Speed | Rule Extensibility |
|:---|:---|:---|:---|:---|
| **Hadolint** | Dockerfile structure & Shell commands | Static AST parsing | Ultra-fast (<50ms)| YAML config / Rego (via Conftest) |
| **Dockle** | Built container image security | Runtime image inspection | Medium (~2s) | Limited |
| **Trivy / Grype**| CVEs in installed packages & OS | Binary/SBOM vulnerability scan | Medium (~5s) | Custom CVE policies |
| **Checkov** | Infrastructure as Code (IaC) & Docker | AST + Static Analysis | Fast (~500ms) | Python / YAML custom policies |

---

## 12. Self-Assessment & Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between `DL` rules and `SC` rules in Hadolint?</strong></summary>

**Answer:**
`DL` rules evaluate Dockerfile-specific instructions (`FROM`, `COPY`, `USER`, `EXPOSE`, `HEALTHCHECK`) according to Docker best practices. `SC` rules are generated by ShellCheck, which Hadolint embeds to analyze the shell script code inside `RUN` instructions (detecting quoting issues, unhandled errors, and syntax bugs).
</details>

<details>
<summary><strong>2. Why does Hadolint flag `DL3020: Use COPY instead of ADD`? When is `ADD` actually justified?</strong></summary>

**Answer:**
`ADD` has unexpected side effects: it can fetch files from remote URLs (without certificate pinning or hash verification) and automatically unpack tar archives. `COPY` is transparent and strictly copies local files into the container. `ADD` is justified **only** when automatically unpacking local compressed archives directly into an image layer (e.g., `ADD rootfs.tar.gz /`).
</details>

<details>
<summary><strong>3. How does Hadolint evaluate `DL3002: Last USER should not be root` in a multi-stage Dockerfile?</strong></summary>

**Answer:**
Hadolint specifically checks the final runtime stage. Intermediate build stages are permitted to execute as `root` (e.g., to install compilers, run package managers). However, the final stage that ships to production must declare a non-root `USER` directive before the end of the file.
</details>

<details>
<summary><strong>4. Why is `apt-get update` forbidden from standing alone in an isolated `RUN` instruction?</strong></summary>

**Answer:**
If `RUN apt-get update` is in its own layer, Docker caches that layer. If you subsequently modify the package list in a downstream `RUN apt-get install -y package`, Docker reuses the stale cached update layer. This leads to cache miss failures or installing outdated packages. `apt-get update` and `apt-get install` must always be chained together in the same `RUN` command.
</details>

<details>
<summary><strong>5. How do you suppress a specific Hadolint warning on a single line without disabling the rule globally?</strong></summary>

**Answer:**
Use the inline comment `# hadolint ignore=RULE_ID` directly above the line where the violation occurs. For example:
```dockerfile
# hadolint ignore=DL3008
RUN apt-get update && apt-get install -y --no-install-recommends curl
```
</details>

<details>
<summary><strong>6. What is the significance of `SHELL ["/bin/bash", "-o", "pipefail", "-c"]` in Dockerfiles, and why does Hadolint recommend it?</strong></summary>

**Answer:**
By default, Docker uses `/bin/sh -c`. In shell pipelines (e.g., `curl -fsSL https://... | tar -xz`), if `curl` fails with a 404 or connection error, but `tar` receives an empty stream and exits with 0 (or another code), the overall step may exit 0. `-o pipefail` ensures that the pipeline return code is the exit status of the last command to exit with a non-zero status, preventing broken downloads from passing the build.
</details>

<details>
<summary><strong>7. How can an organization enforce that developers only pull base images from approved corporate registries using Hadolint?</strong></summary>

**Answer:**
By configuring the `trustedRegistries` list in `.hadolint.yaml`. Hadolint will flag any `FROM` instruction referencing an unlisted domain (such as unapproved public Docker Hub or random third-party registries) with an error.
</details>

<details>
<summary><strong>8. How does Hadolint integrate into a modern GitOps or Pull Request workflow?</strong></summary>

**Answer:**
Hadolint runs in CI (e.g., GitHub Actions, GitLab CI) upon every pull request touching a Dockerfile. It can output results in SARIF format, which integrates into GitHub Code Scanning to display inline annotations on code review diffs. Builds failing a configured threshold (e.g., `warning`) block PR merge.
</details>

---

## 13. Hands-on Practice Exercises

<details>
<summary><strong>Exercise 1: Installing and Running Hadolint on a Sample File</strong></summary>

**Goal:** Run Hadolint via Docker and inspect the output.

**Step-by-step Solution:**
1. Create a flawed Dockerfile:
   ```dockerfile
   FROM node:latest
   RUN npm install express
   CMD node app.js
   ```
2. Lint using Hadolint container:
   ```bash
   docker run --rm -i hadolint/hadolint < Dockerfile
   ```
3. Observe violations reported for `DL3007`, `DL3025`, and `DL3002`.
</details>

<details>
<summary><strong>Exercise 2: Writing a Custom `.hadolint.yaml` Configuration</strong></summary>

**Goal:** Create a strict policy file and verify it enforces failure on warnings.

**Step-by-step Solution:**
1. Create `.hadolint.yaml`:
   ```yaml
   failure-threshold: warning
   trustedRegistries:
     - "docker.io"
     - "gcr.io"
   ignored:
     - DL3059
   ```
2. Run Hadolint pointing to the config:
   ```bash
   hadolint --config .hadolint.yaml Dockerfile
   ```
</details>

<details>
<summary><strong>Exercise 3: Fixing ShellCheck Pipefail Issues</strong></summary>

**Goal:** Resolve ShellCheck warnings when piping curl to tar.

**Step-by-step Solution:**
1. Add pipefail shell declaration before pipelined RUN commands:
   ```dockerfile
   FROM alpine:3.21
   SHELL ["/bin/sh", "-o", "pipefail", "-c"]
   RUN wget -qO- https://example.com/archive.tar.gz | tar -xz
   ```
2. Verify Hadolint passes cleanly without pipe warnings.
</details>

<details>
<summary><strong>Exercise 4: Generating SARIF Output for Security Dashboards</strong></summary>

**Goal:** Export linting results to standard JSON SARIF format.

**Step-by-step Solution:**
1. Run Hadolint with SARIF formatter:
   ```bash
   hadolint --format sarif Dockerfile > hadolint.sarif
   ```
2. Inspect the SARIF schema structure using `jq`:
   ```bash
   jq '.runs[0].results' hadolint.sarif
   ```
</details>

<details>
<summary><strong>Exercise 5: Selective Rule Suppression in Complex Stages</strong></summary>

**Goal:** Suppress an unpinned package warning cleanly in a legacy maintenance Dockerfile.

**Step-by-step Solution:**
1. Apply targeted suppression:
   ```dockerfile
   FROM debian:bookworm-slim
   # hadolint ignore=DL3008,DL3015
   RUN apt-get update && apt-get install -y curl && rm -rf /var/lib/apt/lists/*
   USER 10001
   ```
2. Verify exit code is 0.
</details>

<details>
<summary><strong>Exercise 6: Multi-Stage Dockerfile User Validation</strong></summary>

**Goal:** Ensure Hadolint correctly recognizes `USER` declaration in the final stage while allowing root in the builder.

**Step-by-step Solution:**
1. Test multi-stage structure:
   ```dockerfile
   FROM golang:1.24 AS builder
   WORKDIR /app
   COPY . .
   RUN go build -o server

   FROM alpine:3.21 AS runtime
   RUN adduser -D -u 10001 appuser
   COPY --from=builder /app/server /server
   USER 10001
   ENTRYPOINT ["/server"]
   ```
2. Run Hadolint; verify no `DL3002` error is thrown.
</details>

---

## 15. Summary & Reference Cheat Sheet

### Top 5 Hadolint Rules to Memorize
1. **`DL3002`**: Never leave the last `USER` as root.
2. **`DL3007`**: Never use `:latest`.
3. **`DL3008` / `DL3018`**: Pin package versions in `apt-get` / `apk`.
4. **`DL3009`**: Clean package lists (`rm -rf /var/lib/apt/lists/*`).
5. **`DL3025`**: Use JSON array notation for `CMD` and `ENTRYPOINT`.

---

[← Previous: 10-language-recipes.md](10-language-recipes.md) | [Next: 12-reproducible-builds-and-pinning.md →](12-reproducible-builds-and-pinning.md)
