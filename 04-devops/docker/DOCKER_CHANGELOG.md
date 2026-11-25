# Docker Track Changelog

All notable changes, restructuring, file movements, and additions to the Docker curriculum track are documented in this file.

---

## [Unreleased] - 2026-11-25

### Restructuring & Migrations
- Renamed all 7 legacy snake_case directories to `NN-kebab-case`:
  - `00_setup` $\rightarrow$ `00-setup`
  - `01_docker_fundamentals` $\rightarrow$ `01-fundamentals`
  - `02_docker_commands` $\rightarrow$ `02-commands`
  - `03_dockerfile` $\rightarrow$ `03-dockerfile`
  - `04_volumes_networking` $\rightarrow$ `04-volumes-and-networking`
  - `05_docker_compose` $\rightarrow$ `05-compose`
  - `06_production` $\rightarrow$ `06-production`
- Added new curriculum modules:
  - `07-projects` (8 production reference projects)
  - `08-interview` (8 interview prep topic deep dives)
  - `09-cheatsheets` (8 quick reference guides)

### Governance & Baseline Documentation
- Created `DOCKER_FACTS.md`: Official tool version matrix, CLI/Engine baselines, deprecation notices, and environment configuration.
- Created `DOCKER_AUDIT.md`: Line-by-line audit of legacy 20 notes, gap analysis, and section compliance status.
- Created `DOCKER_COMPLETION_PLAN.md`: Full module architecture, 94 planned notes, and rename mapping.
- Created `DOCKER_DECISIONS.md`: Architectural rationale for conventions, syntax, and formatting choices.
- Created `DOCKER_PROGRESS.md`: Real-time tracking checklist for all 94 notes and 10 module READMEs.

### Module 00: Setup & Installation
- Authoring in progress (6 notes + README).
