# Module 05: GitHub Collaboration & Governance

## Overview
This module explores GitHub's platform-level collaboration, project management, and enterprise governance architecture. It spans code review mechanics, issue triage taxonomies, GitHub Projects (v2), branch protection rules and modern Rulesets, automated review routing via `CODEOWNERS`, structured YAML issue forms, community forums (`Discussions`), open-source forking workflows, documentation hosting on GitHub Pages, container and package management (`ghcr.io`), and organization-wide RBAC access controls.

---

## Learning Objectives
By completing this module, you will:
- Master the complete Pull Request lifecycle, suggested changes syntax, review states, and merge strategies (Squash, Rebase, Merge Commit).
- Build a structured issue and label taxonomy (`type:`, `priority:`, `area:`) and link PRs using closing keywords.
- Plan sprints and manage multi-repository portfolios using GitHub Projects (v2), Kanban boards, iteration cycles, and GraphQL automations.
- Enforce strict compliance guardrails using Branch Protection Rules and modern GitHub Rulesets (strict status checks, signed commits, linear history).
- Automate domain review routing across monorepos and microservices using `CODEOWNERS` and team round-robin review assignments.
- Design interactive, required YAML Issue Forms (`.github/ISSUE_TEMPLATE/*.yml`) and comprehensive PR templates.
- Cultivate a healthy open-source or enterprise community using GitHub Discussions, `SECURITY.md`, and `CODE_OF_CONDUCT.md`.
- Execute open-source contributions cleanly using the triangular forking workflow and safe upstream synchronization.
- Deploy automated technical documentation sites to GitHub Pages with custom domains and Let's Encrypt TLS via GitHub Actions.
- Publish, version, and consume Docker/OCI container images on GitHub Container Registry (`ghcr.io`) and scoped npm packages.
- Architect enterprise identity hierarchies, nested teams, IdP team synchronization (Okta/Azure AD SCIM), and custom repository roles.

---

## Curriculum Roadmap

| # | Topic | File | Summary |
|---|---|---|---|
| 01 | Pull Requests & Code Review | [`01-pull-requests-and-code-review.md`](./01-pull-requests-and-code-review.md) | PR lifecycle, review comments, suggested changes, draft PRs, and merge strategies. |
| 02 | Issues, Labels & Milestones | [`02-issues-labels-and-milestones.md`](./02-issues-labels-and-milestones.md) | Scoped label taxonomies, milestone progress bars, closing keywords, and issue pinning. |
| 03 | GitHub Projects (v2) | [`03-github-projects-and-automation.md`](./03-github-projects-and-automation.md) | Spreadsheet/Kanban boards, iteration sprint cycles, custom fields, and GraphQL automations. |
| 04 | Branch Protection & Rulesets | [`04-branch-protection-and-rulesets.md`](./04-branch-protection-and-rulesets.md) | Strict status checks, evaluate mode, signed commits, bypass actors, and org rulesets. |
| 05 | CODEOWNERS & Review Routing | [`05-codeowners-and-review-routing.md`](./05-codeowners-and-review-routing.md) | Path matching syntax, team ownership, "last matching rule wins", and round-robin review routing. |
| 06 | Issue Forms & PR Templates | [`06-issue-and-pr-templates.md`](./06-issue-and-pr-templates.md) | Interactive YAML forms, required validations, `config.yml`, and org-wide `.github` defaults. |
| 07 | Discussions & Community | [`07-discussions-and-community.md`](./07-discussions-and-community.md) | Q&A accepted answers, idea upvoting, converting issues, `SECURITY.md`, and Code of Conduct. |
| 08 | Forking & Open Source | [`08-forking-and-open-source-contributions.md`](./08-forking-and-open-source-contributions.md) | Triangular forking topology, syncing upstream remotes, maintainer edits, and fork CI security. |
| 09 | GitHub Pages & Docs | [`09-github-pages-and-documentation.md`](./09-github-pages-and-documentation.md) | User vs project sites, Actions deployment (`deploy-pages@v4`), custom domains, and SSL. |
| 10 | Releases & Packages (GHCR) | [`10-github-releases-and-packages.md`](./10-github-releases-and-packages.md) | Release assets, checksums, GitHub Container Registry (`ghcr.io`), and scoped npm registries. |
| 11 | Organizations, Teams & RBAC | [`11-organisations-teams-and-permissions.md`](./11-organisations-teams-and-permissions.md) | Enterprise hierarchy, base permissions, nested teams, SCIM IdP sync, and audit log streaming. |

---

## Enterprise Governance Baseline
- **Access Model**: Set organization base permission to `None`. Manage all permissions via IdP-synced Teams.
- **Merge Security**: Enforce Branch Protection Rulesets requiring 2 reviews, strict status checks, conversation resolution, and signed commits.
- **Automated Routing**: Mandate `CODEOWNERS` for critical modules (`/auth/`, `/terraform/`, `*.sql`).
- **Defect Prevention**: Use YAML Issue Forms with mandatory validations to eliminate incomplete tickets.
