<!--
================================================================================
Metadata:
  Title: Technical Interview Preparation - GitHub Collaboration & Enterprise Governance Q&A
  Track: Git & GitHub / DevOps
  Difficulty: Senior to Staff
  Estimated Time: 50 minutes
  Key Terminology: Pull Requests, Rulesets, Branch Protection, CODEOWNERS, Merge Queue, Issue Forms, RBAC, SCIM Sync
================================================================================
-->

# GitHub Collaboration & Enterprise Governance: 20 In-Depth Interview Questions & Answers

[<- Back to Interview Overview](./README.md) | [Next Note: 06-github-actions-qa.md ->](./06-github-actions-qa.md)

---

## Overview

This guide covers 20 advanced interview questions focused on GitHub platform architecture, collaborative workflows, enterprise security policies, branch protection rulesets, CODEOWNERS routing, and organizational RBAC.

---

## Questions Index

1. [Q01: What is the architectural difference between GitHub Rulesets and Classic Branch Protection?](#q01-what-is-the-architectural-difference-between-github-rulesets-and-classic-branch-protection)
2. [Q02: How does GitHub's "Evaluate Mode" in Rulesets prevent disruption during security policy rollouts?](#q02-how-does-githubs-evaluate-mode-in-rulesets-prevent-disruption-during-security-policy-rollouts)
3. [Q03: How does the `CODEOWNERS` path evaluation algorithm work, and what is the "last matching rule wins" principle?](#q03-how-does-the-codeowners-path-evaluation-algorithm-work-and-what-is-the-last-matching-rule-wins-principle)
4. [Q04: What is a GitHub Merge Queue, and what problem does it solve in high-velocity teams?](#q04-what-is-a-github-merge-queue-and-what-problem-does-it-solve-in-high-velocity-teams)
5. [Q05: Explain the difference between Squash, Rebase, and Merge Commit merge types on GitHub.](#q05-explain-the-difference-between-squash-rebase-and-merge-commit-merge-types-on-github)
6. [Q06: How do strict status checks differ from non-strict status checks in branch protection?](#q06-how-do-strict-status-checks-differ-from-non-strict-status-checks-in-branch-protection)
7. [Q07: How do GitHub Issue Forms (`.github/ISSUE_TEMPLATE/*.yml`) prevent bad bug reports?](#q07-how-do-github-issue-forms-githubissue_templateyml-prevent-bad-bug-reports)
8. [Q08: What are Bypass Actors in GitHub Rulesets, and how should they be governed?](#q08-what-are-bypass-actors-in-github-rulesets-and-how-should-they-be-governed)
9. [Q09: How does the triangular forking model work in open source, and why is it preferred over branch collaboration?](#q09-how-does-the-triangular-forking-model-work-in-open-source-and-why-is-it-preferred-over-branch-collaboration)
10. [Q10: What is SCIM synchronization in GitHub Enterprise, and how does it manage organization membership?](#q10-what-is-scim-synchronization-in-github-enterprise-and-how-does-it-manage-organization-membership)
11. [Q11: How do custom repository roles differ from base roles (Read, Triage, Write, Maintain, Admin)?](#q11-how-do-custom-repository-roles-differ-from-base-roles-read-triage-write-maintain-admin)
12. [Q12: What is the special `.github` repository, and what files can be defined globally within it?](#q12-what-is-the-special-github-repository-and-what-files-can-be-defined-globally-within-it)
13. [Q13: How does GitHub prevent unauthorized workflow runs on pull requests submitted from public forks?](#q13-how-does-github-prevent-unauthorized-workflow-runs-on-pull-requests-submitted-from-public-forks)
14. [Q14: Explain the difference between GitHub Discussions and GitHub Issues.](#q14-explain-the-difference-between-github-discussions-and-github-issues)
15. [Q15: How does GitHub Container Registry (`ghcr.io`) manage permissions independently from repository code?](#q15-how-does-github-container-registry-ghcrio-manage-permissions-independently-from-repository-code)
16. [Q16: What is a draft pull request, and how does it interact with automated CI triggers?](#q16-what-is-a-draft-pull-request-and-how-does-it-interact-with-automated-ci-triggers)
17. [Q17: How does GitHub handle suggested changes in code reviews, and what commit is created?](#q17-how-does-github-handle-suggested-changes-in-code-reviews-and-what-commit-is-created)
18. [Q18: What is the difference between closing an issue with "completed" vs "not planned"?](#q18-what-is-the-difference-between-closing-an-issue-with-completed-vs-not-planned)
19. [Q19: How can enterprise teams audit who bypassed a branch protection rule?](#q19-how-can-enterprise-teams-audit-who-bypassed-a-branch-protection-rule)
20. [Q20: What are GitHub Projects (v2) and how do they integrate with GraphQL automations?](#q20-what-are-github-projects-v2-and-how-do-they-integrate-with-graphql-automations)

---

## Questions & Answers

### Q01: What is the architectural difference between GitHub Rulesets and Classic Branch Protection?
**Answer:**  
- **Classic Branch Protection**: Configured per repository, one branch pattern at a time. Lacks inheritance, cannot be defined at the organization level, and cannot be tested before enforcement.
- **GitHub Rulesets**: Modern, enterprise-grade policy engine:
  - **Hierarchical Governance**: Defined at both the organization and repository levels; org-level rulesets cannot be overridden by repo admins.
  - **Multi-Targeting**: Targets branches and tags using fnmatch patterns or dynamic criteria (e.g., "Default branch", "All branches matching `release/*`").
  - **Enforcement Modes**: Supports **Active**, **Disabled**, and **Evaluate Mode** (logs violations without blocking developers).
  - **Bypass Governance**: Granular bypass actor lists (specific roles, teams, or deploy keys) with audit logging.

---

### Q02: How does GitHub's "Evaluate Mode" in Rulesets prevent disruption during security policy rollouts?
**Answer:**  
In enterprise environments with hundreds of developers, rolling out a strict new rule (such as requiring signed commits or a new security linter check) in "Active" mode risks breaking dozens of in-flight developer branches and deployment pipelines.
In **Evaluate Mode**, the ruleset does not block pushes or merges. Instead, it silently records every violation that *would* have failed into GitHub Insights and audit logs. Security teams can analyze violation metrics, identify which teams need training or configuration updates, and safely transition to Active enforcement once violations drop to zero.

---

### Q03: How does the `CODEOWNERS` path evaluation algorithm work, and what is the "last matching rule wins" principle?
**Answer:**  
`CODEOWNERS` files reside in `.github/`, root, or `docs/`. When a PR modifies files, GitHub evaluates the paths against the patterns in `CODEOWNERS` from top to bottom.
**Crucial Rule**: The **last matching pattern** in the file takes precedence.
Example:
```ini
# Rule 1: Catch-all team
*                   @my-org/core-devs

# Rule 2: Billing team owns billing directory
/services/billing/   @my-org/billing-team

# Rule 3: Security team owns all auth files, even inside billing
**/auth/**          @my-org/security-team
```
If a file `services/billing/auth/token.ts` is modified, Rule 1 matches, Rule 2 matches, but Rule 3 matches last. GitHub automatically routes required reviews to `@my-org/security-team`.

---

### Q04: What is a GitHub Merge Queue, and what problem does it solve in high-velocity teams?
**Answer:**  
In high-velocity teams merging dozens of PRs per day, multiple PRs pass CI independently against `main`. However, merging them sequentially can introduce semantic incompatibilities where PR A and PR B both pass tests individually, but break when combined.
A **Merge Queue** automates speculative execution:
1. When a PR is approved, the developer clicks "Merge when ready".
2. GitHub queues the PR and creates a temporary merge commit combining the PR with preceding queued PRs.
3. CI runs against this speculative state.
4. If tests pass, the PR merges automatically. If tests fail, GitHub ejects the failing PR from the queue without breaking `main`, and re-tests remaining PRs.

---

### Q05: Explain the difference between Squash, Rebase, and Merge Commit merge types on GitHub.
**Answer:**  
- **Create a Merge Commit (`--no-ff`)**: Combines all feature commits into `main` with a 2-parent merge commit. Preserves fine-grained history, author timestamps, and branch shape.
- **Squash and Merge**: Combines all commits from the PR into a single commit on `main`. Keeps trunk history linear and clean; ideal for feature branches with messy WIP commits.
- **Rebase and Merge**: Replays every individual commit from the PR onto the tip of `main` without creating a merge commit. Produces a strictly linear history while preserving individual commit objects.

---

### Q06: How do strict status checks differ from non-strict status checks in branch protection?
**Answer:**  
- **Non-Strict**: The status check must pass on the feature branch. However, the branch does not need to be up to date with the target branch (`main`). If `main` has moved ahead, the PR can still be merged without re-testing.
- **Strict ("Require branches to be up to date before merging")**: Mandates that the feature branch has been updated with the latest commit on `main` and all CI tests have passed on that combined state. Eliminates semantic regression bugs.

---

### Q07: How do GitHub Issue Forms (`.github/ISSUE_TEMPLATE/*.yml`) prevent bad bug reports?
**Answer:**  
Legacy markdown templates (`.md`) are freeform text that developers frequently delete or leave blank. **YAML Issue Forms** provide an interactive, structured form UI with:
- Required input fields with regex pattern validation.
- Dropdown selection menus (e.g., Component: Billing, Auth, Database).
- Checkboxes for reproduction confirmation.
- Form field data is automatically rendered into formatted Markdown when submitted, guaranteeing that bug reports include reproduction steps, logs, and environment versions.

---

### Q08: What are Bypass Actors in GitHub Rulesets, and how should they be governed?
**Answer:**  
Bypass Actors are designated roles, teams, or integrations exempt from ruleset restrictions (e.g., emergency break-glass admins or automated release bots).
**Governance Standards**:
- Limit bypass privileges to dedicated Security Break-Glass teams or specific GitHub App identities.
- Never grant bypass privileges to individual personal developer accounts.
- Configure audit log streaming (Splunk, Datadog) to alert security operations whenever a bypass action occurs (`ruleset.bypass`).

---

### Q09: How does the triangular forking model work in open source, and why is it preferred over branch collaboration?
**Answer:**  
In the triangular model:
1. Contributor forks the upstream repository to their personal namespace (`origin`).
2. Contributor clones their fork locally, configuring `upstream` pointing to the canonical project.
3. Contributor fetches updates from `upstream/main` and pushes features to `origin/feature`.
4. Contributor opens a cross-repository PR from `origin/feature` into `upstream/main`.
**Why Preferred?**: Upstream maintainers do not need to grant write permissions to external contributors. It isolates untrusted code and protects CI runner secrets from malicious execution.

---

### Q10: What is SCIM synchronization in GitHub Enterprise, and how does it manage organization membership?
**Answer:**  
System for Cross-domain Identity Management (SCIM) is an open standard that allows identity providers (IdPs like Okta, Azure AD, PingFederate) to automatically provision and deprovision users in GitHub Enterprise.
- When an employee joins, the IdP provisions their GitHub account and assigns them to IdP-synced teams.
- When an employee departs, deprovisioning in the IdP immediately invalidates their GitHub sessions, revokes access tokens, and removes repository permissions in real time.

---

### Q11: How do custom repository roles differ from base roles (Read, Triage, Write, Maintain, Admin)?
**Answer:**  
Base roles offer fixed bundles of permissions. Often, an organization needs fine-grained delegation (e.g., allowing contractors to close issues and trigger workflows without allowing them to push code or manage branch protection).
**Custom Repository Roles** allow organization administrators to create tailored roles combining over 40 granular permissions (e.g., "Workflow Operator" with only `actions: run` and `issues: write`).

---

### Q12: What is the special `.github` repository, and what files can be defined globally within it?
**Answer:**  
If an organization creates a repository named `.github` (`my-org/.github`), files placed inside it serve as organization-wide defaults for all repositories that do not define their own:
- `SECURITY.md`: Default security disclosure policy.
- `CODE_OF_CONDUCT.md`: Universal community code of conduct.
- `CONTRIBUTING.md`: Default contribution guidelines.
- `.github/ISSUE_TEMPLATE/`: Standard organization issue templates and forms.
- `.github/pull_request_template.md`: Default pull request template.

---

### Q13: How does GitHub prevent unauthorized workflow runs on pull requests submitted from public forks?
**Answer:**  
By default on public repositories:
1. Workflows triggered by `pull_request` from first-time external contributors require manual maintainer approval ("Approve and run") before GitHub Actions runners execute.
2. The workflow executes with a read-only `GITHUB_TOKEN`.
3. Repository secrets are completely masked and unavailable to fork PR runs, preventing attackers from submitting malicious code to print secrets.

---

### Q14: Explain the difference between GitHub Discussions and GitHub Issues.
**Answer:**  
- **GitHub Issues**: Actionable, trackable units of work with clear completion states (Open/Closed). Intended for bug reports, feature implementations, and sprint backlog items.
- **GitHub Discussions**: Collaborative, open-ended forums for brainstorming, community Q&A, RFCs, and announcements. Supports threaded replies, upvoting, and marking answers as "Accepted". Issues can be converted to Discussions and vice versa.

---

### Q15: How does GitHub Container Registry (`ghcr.io`) manage permissions independently from repository code?
**Answer:**  
Unlike legacy Docker registries tied rigidly to repository permissions, packages on `ghcr.io` have independent access control lists. A container image can be published as **Public** while the source code repository remains **Private**, or vice versa. Organizations can grant specific teams write access to container tags without giving them write access to repository source code.

---

### Q16: What is a draft pull request, and how does it interact with automated CI triggers?
**Answer:**  
A Draft PR signals that work is in-progress and not ready for formal review.
- The "Merge" button is disabled.
- `CODEOWNERS` are not automatically assigned or requested for review.
- Automated CI workflows *still trigger* on draft PRs, allowing developers to verify test suites and build outputs before requesting peer review.

---

### Q17: How does GitHub handle suggested changes in code reviews, and what commit is created?
**Answer:**  
Reviewers can embed suggested code diffs using Markdown syntax:
````markdown
```suggestion
const timeout = 45;
```
````
The author can click **"Commit suggestion"** directly in the GitHub web UI. GitHub generates a new commit on the PR branch with a standard message (`Update <file>`), setting the reviewer as co-author.

---

### Q18: What is the difference between closing an issue with "completed" vs "not planned"?
**Answer:**  
- **Completed (Purple icon)**: The work was implemented, tested, and resolved.
- **Not Planned (Grey icon)**: The issue was dismissed as a duplicate, won't fix, obsolete, or invalid.
This distinction provides accurate metrics for engineering velocity and burndown charts in GitHub Projects.

---

### Q19: How can enterprise teams audit who bypassed a branch protection rule?
**Answer:**  
Query the GitHub Enterprise Audit Log:
```bash
gh api "/orgs/my-org/audit-log?phrase=action:protected_branch.policy_override"
```
Or for modern rulesets:
```bash
gh api "/orgs/my-org/audit-log?phrase=action:ruleset.bypass"
```
The audit log records the exact actor, IP address, timestamp, repository, target ref, and the specific rule that was bypassed.

---

### Q20: What are GitHub Projects (v2) and how do they integrate with GraphQL automations?
**Answer:**  
GitHub Projects (v2) is a customizable spreadsheet/Kanban project management engine built directly on GitHub. It supports custom fields (Single-select, Date, Number, Iterations for sprints), multi-repo cross-org tracking, and automated workflows.
Every project action (moving cards, updating iteration sprint cycles, changing priority) is accessible via GitHub's GraphQL API (`addProjectV2ItemById`, `updateProjectV2ItemFieldValue`), enabling automated sprint management from CI pipelines.

---

[<- Back to Interview Overview](./README.md) | [Next Note: 06-github-actions-qa.md ->](./06-github-actions-qa.md)
