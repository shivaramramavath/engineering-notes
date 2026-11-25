---
title: "GitHub Code Scanning & CodeQL Static Analysis"
category: "04-devops"
subcategory: "git-github / 07-github-security"
tags: ["git", "github", "codeql", "code-scanning", "sast", "security", "devsecops", "sarif", "ghas"]
---

# GitHub Code Scanning & CodeQL Static Analysis

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Considerations](#8-performance-security--cost-considerations)
- [9. Comparison Table](#9-comparison-table)
- [10. Real-World Interview Questions](#10-real-world-interview-questions)
- [11. Hands-On Exercises](#11-hands-on-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

Securing application source code requires analyzing proprietary implementation logic for security vulnerabilities before binaries are built and deployed. While linters check for style and surface syntax issues, they fail to track how untrusted user input propagates through complex distributed calls into dangerous sinks.

**GitHub Code Scanning** is a developer-first Static Application Security Testing (SAST) platform integrated directly into GitHub pull requests. Powered primarily by **CodeQL**, it treats code as data. CodeQL compiles or interprets source code into a relational database capturing Abstract Syntax Trees (AST), control flow graphs, and dataflow edges. It then executes specialized declarative queries written in the QL language to find vulnerabilities like SQL injection, cross-site scripting (XSS), command injection, and deserialization flaws.

Third-party scanners (such as SonarQube, Snyk, or Checkmarx) can also upload results into GitHub Code Scanning using the industry-standard **SARIF** (Static Analysis Results Interchange Format) specification, providing a single consolidated pane of glass for all security findings.

---

## 2. Core Concepts

### Code as Data & The CodeQL Database
CodeQL does not run simple regular expressions against text files. Instead, it parses source code into an intermediate relational database schema representing:
- **Abstract Syntax Tree (AST)**: Syntactic structure of classes, functions, statements, and expressions.
- **Control Flow Graph (CFG)**: Execution ordering and branch conditions.
- **Data Flow Graph (DFG)**: Variable assignments, value propagation, and memory allocations.

### Sources, Sinks, and Taint Tracking
Security vulnerabilities frequently stem from tainted data:
- **Source**: Untrusted user input (HTTP request params, headers, websocket payloads, file uploads).
- **Sink**: A critical function that must not receive untrusted data (e.g., `db.query()`, `eval()`, `exec()`, `res.send()`).
- **Sanitizer / Guard**: Logic that validates, escapes, or transforms data, cleansing its tainted status.
- **Taint Analysis**: CodeQL computes whether an unbroken path exists between an untrusted source and a vulnerable sink without passing through an effective sanitizer.

### Default Setup vs Advanced Setup
- **Default Setup**: Configured via the GitHub web UI with zero YAML configuration. GitHub automatically infers repository languages and schedules weekly and PR-level scans.
- **Advanced Setup**: Configured via `.github/workflows/codeql.yml`. Allows custom build steps for compiled languages (C++, Java, C#, Go), matrix builds, query suites (`security-extended`, `security-and-quality`), and custom `.ql` rule execution.

---

## 3. How It Works Internally

```
+-------------------------------------------------------------------------+
| Source Code Repository (Java, TypeScript, Go, Python, C++, etc.)       |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| 1. Database Extraction (codeql database create)                         |
|    - Compiled languages: Traps compiler calls during build              |
|    - Interpreted languages: Extracts AST & files directly               |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| CodeQL Relational Database (AST, CFG, DFG, Call Graph)                 |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| 2. Query Execution (codeql database analyze)                            |
|    - Runs QL queries (e.g., SQLInjection.ql, XssTaint.ql)               |
|    - Performs inter-procedural taint propagation analysis               |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| 3. SARIF File Generation (.sarif JSON)                                  |
|    - Contains file paths, line ranges, CWE IDs, and remediation advice  |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| 4. Upload to GitHub Code Scanning API                                   |
|    - Displays annotations in PR diffs                                   |
|    - Enforces PR check pass/fail blocks on Security Rulesets            |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

A standard CodeQL Advanced Setup workflow defined in `.github/workflows/codeql.yml`:

```yaml
name: "CodeQL Analysis"

on:
  push:
    branches: ["main"]
  pull_request:
    branches: ["main"]
  schedule:
    - cron: "0 4 * * 1" # Weekly every Monday at 04:00 UTC

permissions:
  actions: read
  contents: read
  security-events: write

jobs:
  analyze:
    name: Analyze (${{ matrix.language }})
    runs-on: ubuntu-latest
    timeout-minutes: 120
    strategy:
      fail-fast: false
      matrix:
        include:
          - language: javascript-typescript
            build-mode: none
          - language: java-kotlin
            build-mode: autobuild

    steps:
      - name: Checkout repository
        uses: actions/checkout@v4

      - name: Initialize CodeQL
        uses: github/codeql-action/init@v3
        with:
          languages: ${{ matrix.language }}
          queries: security-extended

      - name: Perform CodeQL Analysis
        uses: github/codeql-action/analyze@v3
        with:
          category: "/language:${{ matrix.language }}"
```

---

## 5. Practical Examples

### Example 1: CodeQL Custom Build for Java Spring Boot
When `autobuild` fails on complex multi-module Maven/Gradle repos, provide manual build steps:
```yaml
      - name: Initialize CodeQL
        uses: github/codeql-action/init@v3
        with:
          languages: java-kotlin

      - name: Set up JDK 21
        uses: actions/setup-java@v4
        with:
          java-version: "21"
          distribution: "temurin"

      - name: Build Java Project
        run: |
          ./mvnw clean compile -DskipTests=true

      - name: Perform CodeQL Analysis
        uses: github/codeql-action/analyze@v3
```

### Example 2: Uploading Third-Party SARIF Reports (e.g., Trivy or Snyk)
Upload external vulnerability scans to the GitHub Security tab:
```yaml
name: "Trivy Vulnerability Scan"
on: [pull_request]

permissions:
  security-events: write
  contents: read

jobs:
  trivy-scan:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Run Trivy vulnerability scanner
        uses: aquasecurity/trivy-action@master
        with:
          scan-type: "fs"
          format: "sarif"
          output: "trivy-results.sarif"

      - name: Upload SARIF to GitHub Code Scanning
        uses: github/codeql-action/upload-sarif@v3
        with:
          sarif_file: "trivy-results.sarif"
```

### Example 3: Defining Custom CodeQL Query Suite Configuration
In `.github/codeql/codeql-config.yml`:
```yaml
name: "Custom Corporate CodeQL Suite"

queries:
  - uses: security-and-quality
  - uses: ./custom-queries/company-rules

paths-ignore:
  - "**/test/**"
  - "**/mock/**"
  - "**/*.spec.ts"
```

Reference in workflow:
```yaml
      - name: Initialize CodeQL
        uses: github/codeql-action/init@v3
        with:
          languages: javascript-typescript
          config-file: ./.github/codeql/codeql-config.yml
```

### Example 4: Querying Code Scanning Alerts via GitHub CLI
```bash
# Fetch all open High and Critical code scanning alerts
gh api -H "Accept: application/vnd.github+json" \
  /repos/my-org/payment-service/code-scanning/alerts \
  --jq '.[] | select(.state == "open" and (.rule.security_severity_level == "high" or .rule.security_severity_level == "critical")) | {id: .number, tool: .tool.name, rule: .rule.id, severity: .rule.security_severity_level, path: .most_recent_instance.location.path}'
```

### Example 5: Resolving False Positives Programmatically
```bash
# Resolve alert #87 as false positive
gh api --method PATCH -H "Accept: application/vnd.github+json" \
  /repos/my-org/payment-service/code-scanning/alerts/87 \
  -f state="dismissed" \
  -f dismissed_reason="false positive" \
  -f dismissed_comment="Input is validated by proprietary HMAC signature upstream in API gateway."
```

---

## 6. Best Practices

- **Run Scans on Default Branch and PRs**: Scan PRs to block vulnerabilities from merging, and scan the default branch weekly via cron to detect new vulnerabilities discovered in existing code.
- **Fail Fast on PR Thresholds**: Configure branch protection rulesets to require `CodeQL / Analyze` to pass before merge.
- **Exclude Test and Generated Files**: Use `paths-ignore` in `codeql-config.yml` to omit unit test fixtures and generated code (`dist/`, `build/`) to accelerate scan times.
- **Use `security-extended` Suite**: Default queries only flag the most blatant vulnerabilities. The `security-extended` suite flags lower-severity security flaws, weak cryptographic primitives, and insecure configurations.
- **Document Dismissals with Rigor**: Never dismiss an alert without documented architectural justification in the `dismissed_comment`.

---

## 7. Common Mistakes

| Mistake | Impact | Correct Approach |
|:---|:---|:---|
| Skipping clean compile for compiled languages | CodeQL database missing trapped compiler invocations, leading to 0 findings | Ensure build script executes a clean compile (`mvn clean compile`, `dotnet build --no-incremental`) |
| Running full unit tests inside CodeQL step | Dramatically increases scan duration and Actions billing minutes | Pass `-DskipTests=true` or skip test execution during CodeQL compilation |
| Scanning without `security-events: write` permission | Upload step fails with HTTP 403 Forbidden | Ensure `permissions: security-events: write` is declared at job or workflow level |
| Using Default Setup on complex multi-repo builds | Auto-detection fails to build custom submodules | Switch to Advanced Setup with explicit build commands in `.github/workflows/codeql.yml` |
| Failing to schedule weekly cron scans | Newly discovered zero-day vulnerabilities in existing code remain undetected | Include schedule cron trigger in the workflow |
| Dismissing alerts without security review | Vulnerabilities reach production environments unchecked | Restrict alert dismissal permissions to security leads or designated reviewers |

---

## 8. Performance, Security & Cost Considerations

- **Performance**: CodeQL compilation and deep taint analysis are computationally heavy. Use larger GitHub-hosted runners (e.g., `runs-on: ubuntu-latest-8-cores`) or caching for faster execution.
- **Security**: The CodeQL engine executes locally on the Actions runner; source code never leaves your runner environment. Only the resulting SARIF metadata is transmitted to GitHub.
- **Cost**: Code scanning is free for all public repositories. For private repositories, CodeQL requires a GitHub Advanced Security (GHAS) license. Uploading third-party SARIF files (e.g., from open-source tools) also requires GHAS in private repositories.

---

## 9. Comparison Table

| Attribute | GitHub CodeQL | SonarQube | Snyk Code |
|:---|:---|:---|:---|
| **Analysis Paradigm**| Semantic Relational Queries (QL) | Pattern matching & Quality Gates | AI-assisted Semantic Engine |
| **Taint Tracking** | Deep inter-procedural dataflow | Moderate dataflow rules | Fast dataflow analysis |
| **PR Integration** | Native GitHub inline PR annotations | Comments via Webhooks / Bot | Native PR checks |
| **Extensibility** | Write custom declarative `.ql` queries| Custom Java/Regex plugins | Custom rules engine |
| **Output Standard** | Native SARIF 2.1.0 | Proprietary UI + SARIF plugin | Proprietary UI + SARIF export |

---

## 10. Real-World Interview Questions

<details>
<summary>1. How does CodeQL differ from a regular regex-based static analysis tool like ESLint?</summary>
Regex-based linters inspect token streams and superficial syntax within a single file, unable to understand execution flow across functions. CodeQL compiles the entire project into a relational database capturing full Abstract Syntax Trees, control flow, and data flow. It can trace a string from an HTTP controller parameter, through multiple services, utility classes, and helpers, into a SQL query sink, detecting vulnerabilities across file boundaries.
</details>

<details>
<summary>2. Why does CodeQL require a build step for compiled languages (Java, C++, C#)?</summary>
For compiled languages, CodeQL acts as a compiler wrapper (tracer). It intercepts compiler calls (`javac`, `gcc`, `cl.exe`) during the build process to observe exact compilation options, classpath bindings, and source transformations, ensuring the extracted CodeQL database mirrors the true compiled binary.
</details>

<details>
<summary>3. What is SARIF and why is it important in DevSecOps?</summary>
SARIF (Static Analysis Results Interchange Format) is an OASIS standard JSON schema for sharing static analysis results across different security tools. GitHub Code Scanning uses SARIF as its universal ingestion format, allowing teams to consolidate alerts from CodeQL, Trivy, Checkmarx, and Snyk into a single unified GitHub dashboard.
</details>

<details>
<summary>4. What are the three core components of a CodeQL taint tracking query?</summary>
1. **Source**: Defines the entry point of untrusted input (e.g., parameter of a `@GetMapping` controller method).  
2. **Sink**: Defines the dangerous execution destination (e.g., the argument of `Statement.executeQuery`).  
3. **Sanitizer / Sanitizing Barrier**: Defines methods or conditions that neutralize taint (e.g., input validation, parameter binding, or HTML escaping).
</details>

<details>
<summary>5. How do you resolve a false positive in CodeQL without modifying source code?</summary>
In the GitHub Security tab under Code Scanning alerts, locate the finding and select "Dismiss alert". You must provide a formal reason (`False positive`, `Used in tests`, `Won't fix`) and an explanatory comment. Alternatively, you can add query filters or suppression annotations in source code if supported.
</details>

<details>
<summary>6. How can you optimize CodeQL build times in a large monorepo?</summary>
- Use matrix strategies to scan languages in parallel.  
- Run CodeQL only on modified paths using GitHub Actions path filtering for pull requests.  
- Skip test compilation and artifact packaging (`-DskipTests`, `skipClean`).  
- Utilize high-performance compute runners with fast NVMe disks and 8+ vCPUs.
</details>

<details>
<summary>7. What GitHub permission is required for an Actions workflow to upload SARIF results?</summary>
The workflow token requires `security-events: write` permission to POST findings to `/repos/{owner}/{repo}/code-scanning/sarifs`.
</details>

<details>
<summary>8. What is the difference between the `security-extended` and `security-and-quality` query suites?</summary>
`security-extended` includes high, medium, and low security vulnerability queries, as well as CWE and OWASP Top 10 indicators. `security-and-quality` includes all security queries plus maintainability, cyclomatic complexity, dead code, and code smell rules.
</details>

---

## 11. Hands-On Exercises

<details>
<summary>Exercise 1: Inspect Code Scanning alerts via GitHub CLI</summary>

**Goal:** List open code scanning findings in a repository.

```bash
gh api /repos/:owner/:repo/code-scanning/alerts \
  --jq '.[] | {id: .number, rule: .rule.id, severity: .rule.severity}'
```
</details>

<details>
<summary>Exercise 2: Create a minimal CodeQL workflow for Python and JavaScript</summary>

**Goal:** Write `.github/workflows/codeql.yml` supporting interpreted languages.

```yaml
name: "CodeQL Analysis"
on: [push, pull_request]
permissions:
  security-events: write
jobs:
  analyze:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        language: ['javascript-typescript', 'python']
    steps:
      - uses: actions/checkout@v4
      - uses: github/codeql-action/init@v3
        with:
          languages: ${{ matrix.language }}
      - uses: github/codeql-action/analyze@v3
```
</details>

<details>
<summary>Exercise 3: Inspect a SARIF output file</summary>

**Goal:** Parse an exported SARIF file using `jq` to display finding messages and file paths.

```bash
cat results.sarif | jq '.runs[].results[] | {ruleId: .ruleId, message: .message.text, file: .locations[0].physicalLocation.artifactLocation.uri}'
```
</details>

<details>
<summary>Exercise 4: Configure paths-ignore for CodeQL</summary>

**Goal:** Exclude vendor directories and test suites in `.github/codeql/codeql-config.yml`.

```yaml
paths-ignore:
  - "**/test/**"
  - "**/vendor/**"
  - "**/*.min.js"
```
</details>

<details>
<summary>Exercise 5: Add third-party SARIF upload to a pipeline</summary>

**Goal:** Upload external tool outputs using `upload-sarif`.

```yaml
- name: Upload Scan Results
  uses: github/codeql-action/upload-sarif@v3
  with:
    sarif_file: scan-results.sarif
    category: external-linter
```
</details>

<details>
<summary>Exercise 6: Query alert details by alert number</summary>

**Goal:** Retrieve the specific code snippet and message for alert #12.

```bash
gh api /repos/:owner/:repo/code-scanning/alerts/12 \
  --jq '{rule: .rule.id, desc: .rule.description, location: .most_recent_instance.location}'
```
</details>

---

## 12. Summary / Cheat Sheet

```text
================================================================================
                    CODE SCANNING & CODEQL STATIC ANALYSIS
================================================================================
Architecture    : Extracts AST/CFG/DFG into relational database -> Queries via QL
Interpreted Lang: Zero-compilation extraction (JS, Python, Ruby)
Compiled Lang   : Intercepts compiler commands (Java, C++, C#, Go) -> autobuild or custom
Suites          : default | security-extended | security-and-quality
Output Format   : SARIF 2.1.0 standard JSON
Permissions     : security-events: write
Taint Analysis  : Tracks data propagation: Source -> Sanitizer -> Sink
CLI Command     : gh api /repos/:owner/:repo/code-scanning/alerts
================================================================================
```

---

## 13. Further Reading
- [GitHub Docs: About code scanning with CodeQL](https://docs.github.com/en/code-security/code-scanning/introduction-to-code-scanning/about-code-scanning-with-codeql)
- [CodeQL Documentation & QL Language Guide](https://codeql.github.com/docs/)
- [OASIS SARIF Specification](https://docs.oasis-open.org/sarif/sarif/v2.1.0/sarif-v2.1.0.html)
- [OWASP Top 10 Web Application Security Risks](https://owasp.org/www-project-top-ten/)

---

[<- Previous: Dependabot & Dependency Review](./02-dependabot-and-dependency-review.md) | [Module 07 Overview](./README.md) | [Next: Signed Commits & Vigilant Mode ->](./04-signed-commits-and-vigilant-mode.md)
