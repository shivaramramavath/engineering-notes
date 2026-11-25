# Module 06: Workflows, Branching & Release Strategies

## Overview
This module covers professional software delivery workflows, branching models, versioning taxonomies, and team operational cadences. It covers the shift from heavy, merge-hell prone branching paradigms to high-velocity Trunk-Based Development enabled by feature flags; contrasts GitFlow against GitHub Flow; covers release maintenance and backport hotfixing; specifies Conventional Commits and automated Semantic Versioning; breaks down monorepo vs polyrepo scaling tradeoffs; analyzes why environment-specific branches are an anti-pattern; and delivers an actionable operational playbook for engineering teams.

---

## Learning Objectives
By completing this module, you will:
- Understand and implement Trunk-Based Development with short-lived branch lifecycles (< 24 hours) to eliminate merge debt.
- Contrast GitFlow, GitHub Flow, and GitLab Flow to choose the appropriate workflow for web apps, mobile apps, and distributed libraries.
- Architect release branching cadences, LTS maintenance pipelines, and cherry-pick hotfix backporting procedures.
- Standardize team commit syntax using Conventional Commits (`feat:`, `fix:`, `chore:`, `BREAKING CHANGE:`) and enforce them with automated linters.
- Decouple code deployment from feature release using Feature Flags, kill switches, percentage rollouts, and dark launching.
- Evaluate Monorepo vs Polyrepo architectures, identifying when tooling like Nx/Turborepo, Git sparse-checkout, and Scalar are necessary.
- Diagnose and remediate the "Environment Branching" anti-pattern using modern 12-factor immutable artifacts and GitOps promotion.
- Implement an end-to-end team operational cadence: PR size limits, review SLAs, rebase vs merge policies, and automated changelogs.

---

## Curriculum Roadmap

| # | Topic | File | Summary |
|---|---|---|---|
| 01 | Trunk-Based Development | [`01-trunk-based-development.md`](./01-trunk-based-development.md) | Short-lived branches (<24h), CI prerequisites, DORA metrics correlation, and migration steps. |
| 02 | GitFlow vs GitHub Flow | [`02-gitflow-and-github-flow.md`](./02-gitflow-and-github-flow.md) | Architectural comparison, release cadence tradeoffs, ceremony reduction, and SaaS workflows. |
| 03 | Release Branching & Hotfixes | [`03-release-branching-and-hotfixes.md`](./03-release-branching-and-hotfixes.md) | Release cutoffs, cherry-pick hotfixes, LTS patch management, and backport automation. |
| 04 | Conventional Commits & SemVer | [`04-conventional-commits-and-semantic-versioning.md`](./04-conventional-commits-and-semantic-versioning.md) | Commitlint specification, MAJOR.MINOR.PATCH rules, release-please, and automated changelogs. |
| 05 | Feature Flags & Dark Launching | [`05-feature-flags-and-dark-launching.md`](./05-feature-flags-and-dark-launching.md) | Decoupling deploy from release, OpenFeature, percentage rollouts, circuit breakers, and flag debt. |
| 06 | Monorepo vs Polyrepo Git Strategies | [`06-monorepo-vs-polyrepo-git-strategies.md`](./06-monorepo-vs-polyrepo-git-strategies.md) | Git scale boundaries, sparse-checkout, shallow clones, Scalar, change detection, and package boundary management. |
| 07 | Environment Branching Anti-Patterns | [`07-environment-branching-anti-patterns.md`](./07-environment-branching-anti-patterns.md) | Drift analysis, snowflake releases, 12-Factor immutable builds, and GitOps image promotion. |
| 08 | Git for Teams Playbook | [`08-git-for-teams-playbook.md`](./08-git-for-teams-playbook.md) | PR size limits (<400 lines), review SLAs (4-hour rule), commit hygiene, and onboarding checklists. |

---

## Key Workflow Comparison Matrix

| Workflow Model | Branch Lifetime | Release Cadence | Complexity | Best Fit For |
|:---|:---|:---|:---|:---|
| **Trunk-Based Development** | < 24 Hours | Continuous (multiple/day) | Low ceremony, high CI maturity | SaaS web services, cloud-native apps |
| **GitHub Flow** | 1 – 3 Days | Continuous / Daily | Very Low | Startups, continuous deployment web apps |
| **Release Branching** | Sprint / Cadence (1-2 weeks) | Scheduled (bi-weekly/monthly) | Moderate | Mobile apps, enterprise on-premise SaaS |
| **GitFlow** | Weeks – Months | Infrequent / Milestones | Very High | Legacy boxed software, multi-version products |
| **Environment Branching** | Permanent (*Anti-Pattern*) | Unpredictable (Merge-drift) | High risk | *Avoid* — Replace with GitOps promotion |
