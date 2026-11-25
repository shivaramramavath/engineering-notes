# Module 09: Production Reference Projects

## Overview
This module contains 8 comprehensive, production-grade reference projects demonstrating end-to-end Git and GitHub workflows. Each project provides complete working code, shell scripts, workflow YAML definitions, architectural diagrams, verification steps, and production troubleshooting checklists.

---

## Project Roadmap

| # | Project | File | Summary |
|---|---|---|---|
| 01 | Merge Conflict Resolution Lab | [`01-conflict-resolution-lab.md`](./01-conflict-resolution-lab.md) | Multi-file 3-way conflict simulation, `diff3` inspection, and `git rerere` automated resolution. |
| 02 | Rebase & Recovery Lab | [`02-rebase-and-recovery-lab.md`](./02-rebase-and-recovery-lab.md) | Interactive rebase surgery, reflog forensic analysis, and recovering deleted branches and commits. |
| 03 | Full Node.js CI Pipeline | [`03-ci-pipeline-for-a-node-app.md`](./03-ci-pipeline-for-a-node-app.md) | Matrix testing (Node 18/20/22), linting, TypeScript typechecking, caching, and concurrency cancellation. |
| 04 | Multi-Arch Docker Build to GHCR | [`04-docker-build-and-push-to-ghcr.md`](./04-docker-build-and-push-to-ghcr.md) | Docker Buildx, QEMU emulation (amd64/arm64), GHA layer caching, GHCR publishing, and Trivy scan. |
| 05 | Automated Release Automation | [`05-release-automation-with-conventional-commits.md`](./05-release-automation-with-conventional-commits.md) | Conventional Commits, Commitlint enforcement, SemVer calculation, and Google `release-please`. |
| 06 | Selective Monorepo CI | [`06-monorepo-ci-with-path-filters.md`](./06-monorepo-ci-with-path-filters.md) | `dorny/paths-filter`, selective test execution across services, and gateway required status check. |
| 07 | Reusable Workflow Library | [`07-reusable-workflow-library.md`](./07-reusable-workflow-library.md) | Centralized enterprise template repo, `workflow_call`, composite actions, and input/secret contracts. |
| 08 | Zero-Secret OIDC Deployment | [`08-secure-deploy-with-oidc-and-approvals.md`](./08-secure-deploy-with-oidc-and-approvals.md) | Keyless AWS OIDC IAM authentication, GitHub Environment approval gates, and automated rollback. |

---

## Pedagogical Design
Every reference project adheres to the standard 5-section enterprise format:
1. **Overview & Architecture**: Problem statement, learning outcomes, and ASCII system topology.
2. **Prerequisites & Setup**: Version dependencies and repository directory structure.
3. **Step-by-Step Implementation Walkthrough**: Complete, reproducible commands and source files.
4. **Verification & Testing**: Commands to test success, expected terminal outputs, and error handling.
5. **Production Readiness & Troubleshooting**: Production checklist and root-cause troubleshooting matrices.
