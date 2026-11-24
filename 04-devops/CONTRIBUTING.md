# Contributing to the DevOps Learning Track

Thank you for contributing to the `04-devops/` learning area. This repository is maintained as an authoritative, production-grade technical curriculum. All contributions must adhere to the engineering standards outlined below.

---

## 1. Core Engineering Invariants

### 1.1 Strict Markdown-Only Output
- **No Raw Configuration Files**: Never commit `.sh`, `.yml`, `.conf`, `.tf`, `.json`, or Dockerfiles directly to the repository tree.
- **Labeled Fenced Code Blocks**: All source code, orchestration manifests, and configuration snippets must reside inside labeled markdown code blocks in `.md` files.

### 1.2 Zero Invented Output Policy
- **Real Execution Output Only**: Paste actual, verified terminal output produced by command executions in test sandboxes.
- **Labeling Unexecutable Items**: Where a command requires expensive paid cloud resources or custom domain DNS, explicitly mark the output block as `illustrative / not executed here` and set metadata status to `Draft (unverified: reason)`.

### 1.3 Security as Mandatory Content
Every concept note and project must explicitly address:
- Principle of least privilege (IAM, Linux users, file permissions).
- Non-root container execution (`USER appuser`).
- Secure secrets handling (no hardcoded passwords, tokens, or private keys).
- Attack surface minimization and blast radius containment.

---

## 2. Note Authoring Standards

### 2.1 Concept Notes
- Must adhere strictly to [`_templates/concept-template.md`](file:///04-devops/_templates/concept-template.md).
- Must contain all **13 standard numbered sections** in exact sequence.
- Target length: 400–700 lines. Split if exceeding 900 lines; expand if below 150 lines.
- Section 5 must include exactly **5 practical, verified examples**:
  1. Basic Idiomatic Usage
  2. Production Real-World Scenario
  3. Edge Case & Boundary Handling
  4. Failure & Anti-Pattern Case with Fix
  5. Production-Grade Hardened Implementation
- Section 7 must feature a common mistakes table with $\ge 6$ detailed rows.
- Section 10 must provide 8 progressive interview Q&As in `<details>`.
- Section 11 must provide 6 practice exercises (2 Easy, 2 Medium, 2 Hard) with verified solutions in `<details>`.

### 2.2 Project Architectures
- Must adhere strictly to [`_templates/project-template.md`](file:///04-devops/_templates/project-template.md).
- Must implement all **5 standard project sections**:
  1. Architectural Overview & Component Topology (with ASCII/Mermaid diagram).
  2. Complete Production Implementation (complete compilable configs and Dockerfiles).
  3. Automated Verification & Test Harness (verification script and output log).
  4. Production Hardening, Security & Cost Notes (security checklist and monthly cloud cost table).
  5. Operational Runbook & Troubleshooting (routine commands and failure diagnostic table).

---

## 3. Directory & File Naming Conventions

- **Folders**: Strictly `NN-kebab-case` (`00-setup`, `01-fundamentals`).
- **Files**: Strictly `NN-kebab-case.md` starting at `01-` within each module.
- **Mandatory Folder README**: Every directory must contain a `README.md` cataloging its contents.
- **Reference Modules**: Numbered modules (`NN-projects`, `NN-interview`, `NN-cheatsheets`) rather than unnumbered reference folders.

---

## 4. Verification & Testing Workflow

Before submitting changes or committing notes:
1. **Lint Markdown**: Ensure valid syntax, heading hierarchy, and table formatting.
2. **Verify Commands**: Execute CLI commands in a local Docker/Linux container to ensure flags and syntax match verified tool versions.
3. **Verify Links**: Ensure all relative markdown links (`file:///...`) resolve to existing files.
4. **Preserve Git History**: Always use `git mv` when renaming existing files or folders.
