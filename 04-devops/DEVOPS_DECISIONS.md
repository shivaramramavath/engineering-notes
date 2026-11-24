# 04-DevOps Architecture Decision Records (ADRs)

This file documents all foundational design decisions, structural trade-offs, and governance policies established for the `04-devops/` learning area.

---

## ADR-001: Standardization of `NN-kebab-case` Across All Tracks

### Context
Prior to this foundation pass, directory and file naming across `04-devops/` was fragmented:
- `docker/` and `git-github/` used snake_case directory prefixes (`00_setup`, `01_docker_fundamentals`).
- `docker/` and `git-github/` used mixed snake-kebab note names (`01_docker-vs-vm.md`) and unnumbered notes (`getting-started.md`).
- `nginx/` used unnumbered note names (`config-structure.md`, `installation.md`).
- `aws/` and `linux/` used kebab-case (`01-foundations`).

### Decision
Enforce universal `NN-kebab-case` for all folders and files starting at `00-setup` or `01-foundations`, and `01-` for notes within each folder. All renames will be executed via `git mv` in the respective track prompts.

### Consequences
- Eliminates cross-track naming friction.
- Guarantees deterministic alphabetic and sequential sorting in IDE file trees and GitHub web views.
- Aligns `04-devops/` with the Java and Spring Boot curriculum tracks.

---

## ADR-002: Reorganization of Reference Materials into Numbered Modules

### Context
`aws/` and `linux/` sequestered reference notes into an unnumbered `reference/` directory (`cheatsheet.md`, `interview.md`), while `nginx/` used `08-reference/` and `git-github/` nested `CHEATSHEET.md` inside a submodule. This prevented a uniform approach to interview preparation, cheatsheets, and projects.

### Decision
Abolish unnumbered `reference/` directories. Standardize reference materials into dedicated, top-level numbered modules at the track root:
- `NN-projects/`: End-to-end runnable architectural projects adhering to `project-template.md`.
- `NN-interview/`: Technical interview Q&As, scenario analysis, and system troubleshooting challenges.
- `NN-cheatsheets/`: High-density CLI command lookups, configuration recipes, and quick syntax reference cards.

### Consequences
- Treats reference materials as first-class curriculum modules rather than miscellaneous appendices.
- Provides a predictable location for interview candidates and engineers needing quick syntax lookups.

---

## ADR-003: Strict Markdown-Only Code Encapsulation Invariant

### Context
`git-github/04_github_actions/examples/` contained 5 raw `.yml` workflow files. The repository rule mandates that ONLY markdown files (`.md`) exist inside the learning tree, with all runnable code, manifests, and configurations encapsulated within labeled fenced code blocks.

### Decision
Migrate the full text of all 5 `.yml` files into corresponding markdown notes within `04-github-actions/` under explicit code headings, then permanently delete the `examples/` subfolder.

### Consequences
- Prevents unformatted code sprawl and ensures all code examples are accompanied by architectural rationale and expected outputs.
- Keeps repository linters focused on markdown consistency without tripping over standalone YAML files.

---

## ADR-004: Adaptation of Java Templates to DevOps Specifications

### Context
The repository's authoritative templates originated in `01-languages/java/_templates/`, which emphasized JVM bytecode, memory allocation, and compilation flags. DevOps infrastructure topics require distinct operational metadata and sections.

### Decision
Adapt `concept-template.md` and `project-template.md` for DevOps by introducing:
- Metadata fields: `OS / Cloud Scope`, `Tool Version`, and `Tested With`.
- Mandatory Section 8: `Performance, Security & Cost Notes` covering blast radius, IAM least privilege, container hardening, and cloud expenditure.
- Real verification output requirements: all command examples must feature verified output from actual execution or be explicitly labeled.

### Consequences
- Preserves the 13-section concept schema and 5-section project schema while tailoring the content to infrastructure, containers, and cloud operations.

---

## ADR-005: Modern Tool Version Baseline Alignment (Late 2026)

### Context
Infrastructure tools evolve rapidly; documenting obsolete defaults (e.g. `docker-compose` v1 Python syntax, Ubuntu 18.04, cgroup v1, legacy `git master` branch, Node 16 in GitHub Actions) produces non-functional code for modern developers.

### Decision
Lock all tracks to contemporary enterprise LTS and stable releases (as verified in `DEVOPS_FACTS.md`):
- **Linux**: Ubuntu 24.04 LTS (Noble Numbat), Linux Kernel 6.12 LTS, systemd 255+, cgroup v2 standard.
- **Docker**: Docker Engine v28+, Compose v2.30+ (`compose.yaml` specification standard), Buildx v0.19+.
- **Git & GitHub**: Git 2.56+, default branch `main`, GitHub Actions Node 20 runtime baseline.
- **Nginx**: Nginx Stable 1.30+ / Mainline 1.31+, native HTTP/3 QUIC, TLS 1.3 standard.
- **AWS**: AWS CLI v2.22+, Amazon Linux 2023 (AL2023), AWS Graviton4 ARM architecture.

### Consequences
- Ensures all CLI commands, configurations, and scripts work out of the box on modern systems without deprecation warnings.
