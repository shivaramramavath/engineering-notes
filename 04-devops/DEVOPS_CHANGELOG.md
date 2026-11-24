# 04-DevOps Changelog & Audit Record

All notable structural changes, conventions, templates, and foundation files for `04-devops/` are documented in this file.

---

## [0.1.0] - 2026-11-25 - Initial Shared Foundation Release

### Added
- **Curriculum Conventions Standard (`CONVENTIONS.md`)**:
  - Established universal `NN-kebab-case` naming for modules and concept notes.
  - Mandated `README.md` in every folder.
  - Mandated numbered modules for projects, interviews, and cheatsheets.
  - Codified the strict markdown-only invariant (no raw code or config files).
- **Authoritative Templates (`_templates/`)**:
  - Adapted `_templates/concept-template.md` (13 sections) with DevOps metadata (`OS / Cloud Scope`, `Tool Version`, `Tested With`) and mandatory Security & Cost section.
  - Adapted `_templates/project-template.md` (5 sections) with topology diagrams, multi-stage Dockerfiles, compose manifests, verification scripts, and cloud cost models.
- **Root Navigation & Governance**:
  - `README.md`: Central portal cataloging the 5 tracks, tech baselines, and cross-track integration relative links.
  - `INDEX.md`: Comprehensive topic-to-note index mapping Linux, Git, Docker, Nginx, and AWS.
  - `GLOSSARY.md`: Seeded master lexicon defining 50+ core DevOps, Linux, container, and cloud terms.
  - `ROADMAP.md`: Role-based learning sequences with estimated timeframes (Beginner DevOps, Backend Developer Ops, Container Focused, AWS Cloud, Interview Prep).
  - `CONTRIBUTING.md`: Strict authoring, verification, and code encapsulation rules.
- **Planning & Audit Suite**:
  - `DEVOPS_FACTS.md`: Phase 0 verified technical specifications for modern Linux, Docker, Git, Nginx, and AWS.
  - `DEVOPS_AUDIT.md`: Phase 1 comprehensive baseline audit of all 108 existing files across 5 tracks.
  - `DEVOPS_COMPLETION_PLAN.md`: Phase 2 complete rename map (old $\rightarrow$ new path) and expert topics.
  - `DEVOPS_PROGRESS.md`: Master checklist tracking module and note progress across all tracks.
  - `DEVOPS_DECISIONS.md`: Architecture Decision Records (ADR-001 through ADR-005).
  - `DEVOPS_FINAL_REPORT.md`: Final delivery and architectural handover summary.

### Identified for Remediation (Track Prompts)
- Inconsistent snake_case folders in `docker/` (`00_setup` to `06_production`) and `git-github/` (`00_setup` to `04_github_actions`).
- Unnumbered note names in `docker/`, `git-github/`, and `nginx/`.
- 5 raw `.yml` files in `git-github/04_github_actions/examples/` to be absorbed into markdown.
- Unnumbered `reference/` folders in `aws/` and `linux/` to be restructured into numbered modules.
- 0-byte stub `README.md` files in `aws/` and `linux/` to be replaced with full track portals.
