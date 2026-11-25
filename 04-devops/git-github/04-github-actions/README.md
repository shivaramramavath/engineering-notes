# Module 04: GitHub Actions CI/CD

## Overview
This module provides a comprehensive, production-grade curriculum on GitHub Actions—from foundational workflow syntax, runners, triggers, and expressions to multi-stage CI/CD pipelines, reusable workflows, custom actions (Composite, JavaScript, Docker), cloud authentication via OpenID Connect (OIDC), Kubernetes auto-scaling runners (ARC), monorepo selective CI, performance and cost optimization, local testing (`act`), and supply chain hardening (SHA pinning, SLSA provenance, and artifact attestations).

---

## Learning Objectives
By completing this module, you will:
- Understand the core execution hierarchy: events, workflows, jobs, steps, actions, and ephemeral runners.
- Master YAML workflow syntax, triggers, paths filtering, cron expressions, and concurrency controls.
- Utilize contexts, expressions, environment variables, encrypted repository/organization secrets, and least-privilege token permissions.
- Implement dependency caching, artifact uploading/downloading, and matrix strategies with fail-fast options.
- Build multi-stage deployment pipelines with manual approval gates, environment protection rules, and rollback triggers.
- Create containerized CI/CD workflows utilizing Docker Buildx and GitHub Actions layer caching.
- Authenticate with AWS, GCP, and Azure without static credentials using OpenID Connect (OIDC) identity federation.
- Deploy and auto-scale self-hosted ephemeral runner pods on Kubernetes using Actions Runner Controller (ARC).
- Optimize pipeline wall-clock time and billing costs using test sharding, concurrency cancellation, and cache optimization.
- Test and debug workflow YAML files locally without commit ping-pong using `act`.
- Implement selective CI in monorepos using `dorny/paths-filter` and dynamic matrices without breaking branch protection rules.
- Author, bundle, and release custom Composite, JavaScript (`@actions/core`), and Docker Actions.
- Cryptographically secure CI/CD pipelines using immutable SHA pinning, Dependabot, StepSecurity egress firewalls, and SLSA artifact attestations.

---

## Curriculum Roadmap

| # | Topic | File | Summary |
|---|---|---|---|
| 01 | Core Concepts & Architecture | [`01-core-concepts.md`](./01-core-concepts.md) | Hierarchy of events, workflows, jobs, steps, runners, and basic smoke tests. |
| 02 | Workflow Syntax | [`02-workflow-syntax.md`](./02-workflow-syntax.md) | Structure of `.github/workflows/*.yml`, top-level keys, timeouts, and naming. |
| 03 | Events & Triggers | [`03-events-and-triggers.md`](./03-events-and-triggers.md) | Push, pull_request, schedule (cron), workflow_dispatch, and webhook activity types. |
| 04 | Jobs & Steps | [`04-jobs-and-steps.md`](./04-jobs-and-steps.md) | Sequential vs parallel jobs, `needs:` dependencies, working directories, and shells. |
| 05 | Actions & Marketplace | [`05-actions-and-marketplace.md`](./05-actions-and-marketplace.md) | Utilizing verified Marketplace actions, community ecosystem, and versioning. |
| 06 | Runners | [`06-runners.md`](./06-runners.md) | GitHub-hosted VMs (`ubuntu-latest`, Windows, macOS) vs Self-hosted environments. |
| 07 | Variables & Secrets | [`07-variables-and-secrets.md`](./07-variables-and-secrets.md) | Encrypted secrets, configuration variables, masking, and environment-level secrets. |
| 08 | Contexts & Expressions | [`08-contexts-expressions-conditions.md`](./08-contexts-expressions-conditions.md) | `${{ github.* }}`, `${{ secrets.* }}`, conditional evaluation (`if:`), and built-in functions. |
| 09 | Artifacts & Caching | [`09-artifacts-caching-dependencies.md`](./09-artifacts-caching-dependencies.md) | `actions/upload-artifact@v4` immutability, `actions/cache@v4`, and package manager caching. |
| 10 | Matrix & Concurrency | [`10-matrix-and-concurrency.md`](./10-matrix-and-concurrency.md) | Multi-OS/version combinatorial matrices, `fail-fast`, and `concurrency` cancellation. |
| 11 | Permissions & Environments | [`11-permissions-and-environments.md`](./11-permissions-and-environments.md) | Least-privilege `GITHUB_TOKEN` permissions, protected environments, and manual approvals. |
| 12 | Reusable Workflows | [`12-reusable-workflows.md`](./12-reusable-workflows.md) | `workflow_call` triggers, inputs, secrets inheritance, and enterprise standardization. |
| 13 | Custom Actions | [`13-custom-actions.md`](./13-custom-actions.md) | Building and packaging reusable actions across composite, node, and container formats. |
| 14 | CI Pipelines | [`14-ci-pipelines.md`](./14-ci-pipelines.md) | End-to-end continuous integration pipeline: linting, unit testing, and coverage reports. |
| 15 | CD & Deployment | [`15-cd-and-deployment.md`](./15-cd-and-deployment.md) | Staged deployment pipelines: staging, smoke testing, manual approval, and production deploy. |
| 16 | Docker CI/CD | [`16-docker-ci-cd.md`](./16-docker-ci-cd.md) | Container builds with Buildx, multi-platform compilation, and GitHub Container Registry. |
| 17 | Security Hardening | [`17-security-hardening.md`](./17-security-hardening.md) | Script injection prevention, fork pull request risks (`pull_request_target`), and secret safety. |
| 18 | Debugging & Logging | [`18-debugging-and-workflow-commands.md`](./18-debugging-and-workflow-commands.md) | Workflow commands (`::error::`, `::notice::`), step summary markdown, and runner debugging. |
| 19 | Release Automation | [`19-release-automation.md`](./19-release-automation.md) | Tag triggers, automated SemVer, changelog generation, checksums, and GitHub Releases. |
| 20 | OIDC & Cloud Auth | [`20-oidc-and-cloud-auth.md`](./20-oidc-and-cloud-auth.md) | Keyless authentication with AWS, GCP, and Azure using OpenID Connect (OIDC) JWTs. |
| 21 | Self-Hosted Runners & ARC | [`21-self-hosted-runners-and-scaling.md`](./21-self-hosted-runners-and-scaling.md) | Actions Runner Controller (ARC) on Kubernetes, auto-scaling, and ephemeral pod runners. |
| 22 | Performance & Cost | [`22-workflow-performance-and-cost.md`](./22-workflow-performance-and-cost.md) | Caching optimization, runner billing minute multipliers, timeouts, and test sharding. |
| 23 | Local Testing with `act` | [`23-testing-workflows-locally.md`](./23-testing-workflows-locally.md) | Running workflows on local Docker daemon, dry-runs, mocking secrets, and fast feedback. |
| 24 | Monorepos & Path Filters | [`24-monorepo-and-path-filters.md`](./24-monorepo-and-path-filters.md) | Selective CI with `dorny/paths-filter`, dynamic matrices, and CI status gates. |
| 25 | Action Types Deep Dive | [`25-composite-vs-javascript-vs-docker-actions.md`](./25-composite-vs-javascript-vs-docker-actions.md) | Deep architectural comparison: Composite vs Node 20 (`@actions/core`) vs Docker Actions. |
| 26 | Supply Chain & Provenance | [`26-supply-chain-pinning-and-provenance.md`](./26-supply-chain-pinning-and-provenance.md) | Immutable SHA pinning, Dependabot, StepSecurity Harden-Runner, and SLSA attestations. |

---

## Production Security Standards
- **Rule 1 (Least Privilege)**: Always configure `permissions: contents: read` globally at the top of every workflow.
- **Rule 2 (No Static Cloud Keys)**: Authenticate with AWS, GCP, and Azure using OpenID Connect (`permissions: id-token: write`).
- **Rule 3 (Immutable Pinning)**: Pin third-party actions to their 40-character commit SHA with Dependabot automated updates.
- **Rule 4 (Execution Boundaries)**: Never run self-hosted runners on public repositories, and enforce ephemeral single-use lifecycles.
- **Rule 5 (Fail-Safe Timeouts)**: Specify `timeout-minutes: 10` on every job to prevent billing runaway.
