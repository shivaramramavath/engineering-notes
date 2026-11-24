# DevOps Learning Track: Technical Conventions & Engineering Guidelines

## 1. Governance & Standards Heritage
This standard establishes the mandatory structural and technical rules governing all tracks within `04-devops/` (`aws/`, `docker/`, `git-github/`, `linux/`, `nginx/`). It mirrors the architectural rigor established in the Java (`01-languages/java/`) and Spring Boot (`03-backend/spring-boot/`) curriculum areas.

All subsequent track prompts and refactoring passes must strictly adhere to these conventions.

---

## 2. Directory & File Naming Conventions

### 2.1 Module Folder Naming
- **Format**: Strictly `NN-kebab-case` with zero-padded two-digit numbering.
- **Starting Index**:
  - `00-setup` for tracks that require local environment installation and verification (`docker`, `git-github`).
  - `01-foundations` or `01-fundamentals` for tracks where foundational theory precedes setup (`aws`, `linux`, `nginx`).
- **Forbidden Patterns**: No snake_case (`00_setup`), camelCase, spaces, or uppercase characters.

### 2.2 Concept Note Naming
- **Format**: Strictly `NN-kebab-case.md` starting at `01-` within each module folder.
- **Index Alignment**: Consecutive numbering without gaps (`01-foo.md`, `02-bar.md`, `03-baz.md`).
- **Forbidden Patterns**: Unnumbered files (`getting-started.md`), snake_case (`01_run-ps.md`), uppercase (`CHEATSHEET.md`).

### 2.3 Per-Folder Navigation
- **Mandatory README**: Every folder (including root, tracks, and submodules) must contain a `README.md`.
- **Contents**: The folder's scope, mental model, prerequisites, and a catalog table linking to every note in that folder.

### 2.4 Reference & Specialized Modules
- **No Unnumbered `reference/` Directories**: Reference materials must be structured as numbered modules at the track root:
  - `NN-projects/`: Complete runnable end-to-end architectures adhering to `project-template.md`.
  - `NN-interview/`: Technical interview question banks, scenario discussions, and debugging challenges.
  - `NN-cheatsheets/`: High-density syntax cards, command lookups, and troubleshooting matrices.

---

## 3. Rename & Migration Rules

When standardizing existing tracks, the following mechanical transformation rules apply:

1. **Underscore to Hyphen**:
   - `00_setup` $\rightarrow$ `00-setup`
   - `01_docker_fundamentals` $\rightarrow$ `01-fundamentals`
   - `01_docker-vs-vm.md` $\rightarrow$ `01-docker-vs-vm.md`
2. **Numeric Prefix Insertion**:
   - `getting-started.md` $\rightarrow$ `01-getting-started.md`
   - `config-structure.md` $\rightarrow$ `01-config-structure.md`
3. **Reference Module Transformation**:
   - `aws/reference/cheatsheet.md` $\rightarrow$ `aws/07-cheatsheets/01-aws-cheatsheet.md`
   - `linux/reference/interview.md` $\rightarrow$ `linux/08-interview/01-linux-interview-qa.md`
   - `git-github/04_github_actions/CHEATSHEET.md` $\rightarrow$ `git-github/06-cheatsheets/01-git-and-actions-cheatsheet.md`
4. **Git Preservation**: All renames must use `git mv` so history is preserved.
5. **Relative Link Integrity**: After renames, all relative markdown links must be updated immediately to target new file paths.

---

## 4. Markdown-Only Code Invariant

To maintain uniform repository cleanliness and prevent build artifact pollution:
- **No Raw Source / Config Files**: Absolutely no raw `.yml`, `.sh`, `.conf`, `.tf`, `.json`, or Dockerfiles in the repository.
- **Fenced Code Blocks**: All configurations and scripts must reside within fenced markdown code blocks in `.md` files.
- **Labeled Code Blocks**: Every code block must clearly indicate its target file path or purpose:
  ```markdown
  ### `compose.yaml`
  ```yaml
  services:
    app:
      image: myapp:latest
  ```
  ```
- **Example File Ingestion**: Existing raw example files (such as `04_github_actions/examples/*.yml`) must have their content migrated into fenced code blocks in corresponding concept notes before the raw file is removed.

---

## 5. Technical Metadata Schema

Every DevOps concept note begins with the standardized metadata block:

```markdown
| Metadata | Details |
|---|---|
| **Level** | [Beginner / Intermediate / Advanced / Expert] |
| **Prerequisites** | [Relative link to prerequisite note](file:///...) |
| **Time to Read** | [XX minutes] |
| **Status** | Complete (or Draft: reason) |
| **OS / Cloud Scope** | [Linux (Ubuntu 24.04) / AWS / Multi-Platform / macOS] |
| **Tool Version** | [e.g. Docker 28+, Git 2.47+, Nginx 1.30+, AWS CLI v2.22+] |
| **Tested With** | [Verified environment and runner details] |
```

---

## 6. Strict 13-Section Concept Schema

Every concept note MUST implement all 13 standard sections in exact numerical order:

1. **Title & Summary Blockquote** (`# Title` followed by `> [One-line summary]`)
2. **Metadata Table & Table of Contents**
3. **1. What & Why**: Operational problem statement, historical context, real-world DevOps role.
4. **2. Core Concepts**: Mental models, kernel/daemon/cloud invariants, state transitions.
5. **3. How It Works Internally**: Kernel primitives (namespaces, cgroups), daemon loops, networking packets, or AWS API control planes. Includes ASCII/Mermaid diagram.
6. **4. Syntax & Usage**: Canonical CLI commands, configuration directives, and flags.
7. **5. Practical Examples**: Exactly 5 verified, progressive examples:
   - *Example 1: Basic Idiomatic Usage*
   - *Example 2: Production Real-World Scenario*
   - *Example 3: Edge Case & Boundary Handling*
   - *Example 4: Failure & Anti-Pattern Case with Remediation*
   - *Example 5: Production-Grade Hardened / High-Scale Implementation*
8. **6. Best Practices**: Concrete operational rules with technical reasoning.
9. **7. Common Mistakes & Pitfalls**: Table with $\ge 6$ rows (`Mistake`, `Why It Fails`, `Practical Remedy`).
10. **8. Performance, Security & Cost Notes**: Blast radius, CPU/memory overhead, least privilege, financial impact.
11. **9. Comparison / When to Use What**: Trade-off decision matrix vs alternatives.
12. **10. Interview Questions**: 8 technical questions (Junior to Staff level) with comprehensive answers in `<details>`.
13. **11. Practice Exercises**: 6 progressive exercises (2 Easy, 2 Medium, 2 Hard) with verified solutions in `<details>`.
14. **12. Summary / Cheat Sheet**: High-density reference summary.
15. **13. Further Reading**: Verified official documentation URLs and RFC references.

---

## 7. Strict 5-Section Reference Project Schema

All architectural projects in `NN-projects/` implement the 5-section project schema:
1. **Metadata & Production Baseline**
2. **1. Architectural Overview & Component Topology** (with ASCII / Mermaid network & data flow diagram)
3. **2. Complete Production Implementation** (fenced code blocks for all configs, Dockerfiles, Compose, and scripts)
4. **3. Automated Verification & Test Harness** (testing scripts, health probes, and assertions)
5. **4. Production Hardening, Security & Deployment** (TLS setup, non-root users, secret management)
6. **5. Operational Runbook & Troubleshooting** (verification cURL calls, log inspection, failure recovery)
