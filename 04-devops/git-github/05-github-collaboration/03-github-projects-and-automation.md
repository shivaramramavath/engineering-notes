---
title: "GitHub Projects (v2) and Workflow Automation"
category: "04-devops"
subcategory: "git-github / 05-github-collaboration"
tags: ["github", "github-projects", "kanban", "automation", "project-management", "graphql-api"]
difficulty: "Intermediate-Advanced"
estimated_read_time: "25 min"
prerequisites:
  - "05-github-collaboration/01-pull-requests-and-code-review"
  - "05-github-collaboration/02-issues-labels-and-milestones"
last_updated: "2026-11-25"
---

# GitHub Projects (v2) and Workflow Automation

> **Navigation**: [← Previous: 02-issues-labels-and-milestones.md](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/02-issues-labels-and-milestones.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/README.md) | [Next: 04-branch-protection-and-rulesets.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/04-branch-protection-and-rulesets.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [Project Views: Table, Board & Roadmap](#3-project-views-table-board--roadmap)
4. [Custom Fields & Iteration Planning](#4-custom-fields--iteration-planning)
5. [Built-in Workflow Automations](#5-built-in-workflow-automations)
6. [Automating Projects via GitHub Actions](#6-automating-projects-via-github-actions)
7. [Practical Examples](#7-practical-examples)
8. [Best Practices](#8-best-practices)
9. [Common Mistakes](#9-common-mistakes)
10. [Performance, Governance & Enterprise Portfolio Planning](#10-performance-governance--enterprise-portfolio-planning)
11. [Interview Questions & Answers](#11-interview-questions--answers)
12. [Practice Exercises](#12-practice-exercises)
13. [Summary & Cheat Sheet](#13-summary--cheat-sheet)
14. [Further Reading](#14-further-reading)

---

## 1. What & Why

### What is GitHub Projects (v2)?
GitHub Projects (often referred to as Projects v2 or Memex) is an enterprise-grade, spreadsheet-fast project management tool natively integrated into GitHub. Unlike classic repository boards, Projects operate at both the user and organization level, spanning hundreds of repositories, tracking issues, pull requests, and standalone draft task cards across multiple views (Kanban, Table, and Roadmap).

### Why Do We Use It?
1. **Multi-Repository Visibility**: Unify engineering work across frontend, backend, infrastructure, and mobile repositories on a single cross-functional board.
2. **Spreadsheet Flexibility**: Filter, group, sort, and slice data instantly with keyboard-driven navigation and custom metadata columns.
3. **Native Automation**: Eliminate manual ticket dragging. When a PR is opened, merged, or reviewed, the board status updates automatically.
4. **Agile Sprint / Iteration Cycles**: First-class support for two-week sprint cadences, velocity metrics, and capacity estimation.

---

## 2. Core Concepts

### 1. Item Types
A project board can hold two types of items:
- **Repository Content**: Live GitHub Issues and Pull Requests linked from any repository in the organization.
- **Draft Issues**: Lightweight, markdown task cards created directly on the board without needing a repository. Draft issues can be converted into full GitHub Issues in any repository with one click.

### 2. Custom Fields
You can extend items with custom schema fields:
- `Text`: Short strings (e.g. "Customer Name").
- `Number`: Story points, story estimates, or complexity ratings.
- `Date`: Target completion deadlines.
- `Single Select`: Status (`Todo`, `In Progress`, `Review`, `Done`), Priority (`P0`, `P1`, `P2`), or Team (`Frontend`, `Platform`).
- `Iteration`: Sprint tracking with duration (e.g. 2 weeks) and start dates.

---

## 3. Project Views: Table, Board & Roadmap

```
1. TABLE VIEW (Spreadsheet-Style):
┌───────────┬──────────────┬──────────────┬─────────────┬───────────┐
│ Title     │ Repository   │ Status       │ Assignees   │ Estimate  │
├───────────┼──────────────┼──────────────┼─────────────┼───────────┤
│ Auth API  │ backend-api  │ In Progress  │ @octocat    │ 5 pts     │
│ UI Navbar │ frontend-web │ Todo         │ @mona       │ 3 pts     │
│ S3 Bucket │ terraform    │ Done         │ @hubot      │ 2 pts     │
└───────────┴──────────────┴──────────────┴─────────────┴───────────┘

2. BOARD VIEW (Kanban-Style):
┌──────────────┐   ┌──────────────┐   ┌──────────────┐   ┌──────────────┐
│     TODO     │   │ IN PROGRESS  │   │  IN REVIEW   │   │     DONE     │
├──────────────┤   ├──────────────┤   ├──────────────┤   ├──────────────┤
│ [Card 1]     │   │ [Card 2]     │   │ [Card 4]     │   │ [Card 5]     │
│ [Card 3]     │   │              │   │              │   │              │
└──────────────┘   └──────────────┘   └──────────────┘   └──────────────┘

3. ROADMAP VIEW (Gantt-Style Timeline):
   Task A: [==========] (Nov 1 - Nov 15)
   Task B:         [====================] (Nov 10 - Dec 5)
```

---

## 4. Custom Fields & Iteration Planning

### Iteration Fields
Iteration fields automatically generate rolling sprints:
- **Duration**: Configurable (e.g. 1 week, 2 weeks, 3 weeks).
- **Current Iteration**: Automatically rolls forward on Monday morning based on the current calendar date.
- **Velocity Tracking**: Projects aggregates custom `Number` fields (e.g. Story Points) per iteration, showing total capacity completed per sprint.

---

## 5. Built-in Workflow Automations

GitHub Projects includes a visual workflow rule engine requiring zero code:

```
                    Project Auto-Sync Automations:

[ Item Added to Project ] ──► Set 'Status' = 'Todo'
                                     │
[ Issue / PR Reopened ]   ──► Set 'Status' = 'Todo'
                                     │
[ Pull Request Opened ]   ──► Set 'Status' = 'In Progress'
                                     │
[ PR Review Requested ]   ──► Set 'Status' = 'In Review'
                                     │
[ PR Merged / Closed ]    ──► Set 'Status' = 'Done'
                                     │
[ Issue Closed ]          ──► Set 'Status' = 'Done'
```

### Auto-Add Workflows
Automatically ingest issues or pull requests matching specific criteria:
- *Condition*: When an issue is labeled `area:billing` in `my-org/backend`, automatically add it to the "Billing Team Project Board".

---

## 6. Automating Projects via GitHub Actions

For complex multi-stage automations, use GitHub Actions with the Projects GraphQL API or the official `actions/add-to-project` action.

### Action: Auto-Add New Issues to Organization Project
```yaml
# .github/workflows/project-sync.yml
name: Auto-Assign to Project
on:
  issues:
    types: [opened]

jobs:
  add-to-project:
    runs-on: ubuntu-latest
    steps:
      - name: Add issue to project
        uses: actions/add-to-project@v1.0.0
        with:
          # Personal Access Token (classic) or GitHub App token with 'project' scope
          project-url: https://github.com/orgs/my-org/projects/5
          github-token: ${{ secrets.PROJECT_PAT }}
```

---

## 7. Practical Examples

### Example 1: Querying Project Items via GitHub CLI
```bash
# List organization projects
gh project list --owner my-org

# View project items in tabular format
gh project item-list 5 --owner my-org --format json
```

### Example 2: Creating a Project Item via GitHub CLI
```bash
# Create a standalone Draft Issue on project board #5
gh project item-create 5 --owner my-org --title "Investigate Redis memory leak" --body "Monitor memory spikes on staging cluster."
```

### Example 3: Updating Custom Field Value via GraphQL API
```bash
# Update a custom single-select field (e.g. Status -> Done)
gh api graphql -f query='
mutation {
  updateProjectV2ItemFieldValue(
    input: {
      projectId: "PVT_kwDOA12345678"
      itemId: "PVTI_lADOA12345678"
      fieldId: "PVTSSF_lADOA12345678"
      value: {
        singleSelectOptionId: "982367a1"
      }
    }
  ) {
    projectV2Item {
      id
    }
  }
}'
```

---

## 8. Best Practices

1. **Create Projects at the Organization Level**: Avoid repository-specific projects when multiple repositories contribute to common business goals. Organization-level projects unify cross-functional workflows.
2. **Keep Status Columns Simple**: Limit status columns to 4–5 standard states: `Backlog`, `Todo`, `In Progress`, `Review`, `Done`. Over-complicated 10-state Kanban boards degrade developer adoption.
3. **Use Views for Different Audiences**:
   - Create a **"Sprint Board"** view for developers (grouped by Status).
   - Create a **"Roadmap"** view for product managers and executives (grouped by Milestones/Dates).
   - Create an **"Unassigned Triage"** table view for engineering leads.
4. **Use Number Fields for Story Points**: Enable capacity planning by assigning numerical complexity estimates to issues.
5. **Enforce Automated State Transitions**: Ensure PR merges automatically transition linked issues to `Done` to keep boards continuously accurate without manual drag-and-drop.

---

## 9. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Using Classic Projects | Missing modern features; classic boards deprecated | Selecting deprecated classic projects | Always create **Projects (v2)** |
| Missing Project PAT in Actions | Workflow fails: `Resource not accessible` | Default `GITHUB_TOKEN` lacks cross-org project write scope | Use GitHub App token or PAT with `project` scope |
| 50 custom fields on one board | Cluttered spreadsheet; slow loading | Over-engineering schema | Limit custom fields to essential 4–6 columns |
| Forgetting to link PR to Issue | Item remains in `In Progress` after merge | Developer forgot `Fixes #123` | Enforce PR template with closing keyword |
| Mixing sprint tracking with long-term backlog | Sprints contain 500 irrelevant backlog items | Inadequate view filtering | Filter sprint view by `Iteration: @current` |

---

## 10. Performance, Governance & Enterprise Portfolio Planning

- **High-Volume Capacity**: Projects v2 supports up to **1,200 items per project board** with client-side virtualized rendering, ensuring instant filtering across large teams.
- **Access Control & Permissions**:
  - **Admin**: Can create custom fields, delete projects, and manage automations.
  - **Writer**: Can add issues, edit fields, and move cards across columns.
  - **Reader**: Read-only view for stakeholders and auditors.
- **Enterprise Insights**: GitHub Enterprise accounts can aggregate multiple projects into portfolio-level charts displaying velocity, open vs closed burn-down curves, and cycle times.

---

## 11. Interview Questions & Answers

<details>
<summary><strong>Q1: What are the main differences between GitHub Classic Projects and modern GitHub Projects (v2)?</strong></summary>

Classic Projects were basic repository- or organization-level Kanban boards with fixed columns and limited automation.
Projects v2 (built on GitHub's Memex engine) is a full relational spreadsheet database featuring:
1. Multiple concurrent view types: Table, Kanban Board, and Gantt Roadmap.
2. Custom fields (Text, Number, Date, Single Select, and native Iterations).
3. Cross-repository aggregation across an entire organization.
4. Built-in reactive workflow automations.
5. High-performance GraphQL APIs for programmatic manipulation.
</details>

<details>
<summary><strong>Q2: How does an Iteration field work in GitHub Projects, and how does it support Scrum/Agile workflows?</strong></summary>

An Iteration field represents a recurring time-boxed sprint cycle (e.g. 2 weeks). It defines start dates and durations. Once configured, GitHub automatically calculates the `@current`, `@previous`, and `@next` iterations based on the active calendar date. Views can be filtered using `Iteration:@current` so developers only see active sprint tasks. Projects automatically aggregates story points per iteration, providing automated velocity metrics.
</details>

<details>
<summary><strong>Q3: Why can't the standard `GITHUB_TOKEN` easily update an Organization-level Project in a workflow?</strong></summary>

The default `GITHUB_TOKEN` provided by GitHub Actions is scoped strictly to the repository where the workflow is running. Organization Projects exist at the organization boundary, outside the repository's authorization scope. To modify or add items to an organization project from a workflow, you must provide a GitHub App token or a Fine-Grained Personal Access Token (PAT) with `read:project` and `write:project` organization permissions.
</details>

<details>
<summary><strong>Q4: What is a "Draft Issue" in GitHub Projects?</strong></summary>

A Draft Issue is a lightweight item created directly on a project board without needing a backing repository or creating an issue on disk. It allows product managers and developers to brainstorm, jot down quick ideas, or outline tasks. When a draft issue is ready for implementation, clicking **"Convert to issue"** prompts you to select a repository, converting the draft into a full GitHub issue while preserving all custom field values.
</details>

<details>
<summary><strong>Q5: How do you filter a Project view to show only issues assigned to the logged-in viewer?</strong></summary>

Use the `@me` macro in the view's search filter:
```text
assignee:@me
```
When team members open that view, it dynamically filters to show only items assigned to their specific GitHub username.
</details>

<details>
<summary><strong>Q6: How can you automatically add new issues with a specific label to a project board without writing GitHub Actions?</strong></summary>

Using the built-in **Auto-Add** workflow in GitHub Projects:
1. Navigate to Project Settings -> **Workflows** -> **Auto-add to project**.
2. Set query filter: `is:issue label:area:backend`.
3. Select the target repositories.
Whenever a matching issue is opened or labeled, GitHub automatically attaches it to the project board.
</details>

<details>
<summary><strong>Q7: What is the maximum number of items a single GitHub Project can track?</strong></summary>

A single GitHub Project (v2) can track up to **1,200 items**. If a project exceeds this threshold, teams should archive completed items or split boards by product domains or quarters.
</details>

<details>
<summary><strong>Q8: Can a Pull Request card on a project board automatically transition to 'Done' when merged? How?</strong></summary>

Yes. Under Project Settings -> **Workflows**, enable the **"Item closed"** built-in automation. Configure it to set the `Status` field to `Done` whenever a pull request is merged or closed.
</details>

---

## 12. Practice Exercises

<details>
<summary><strong>Exercise 1: List Organization Projects via CLI</strong></summary>

**Scenario**: List all project boards in your organization.
```bash
gh project list --owner my-org
```
</details>

<details>
<summary><strong>Exercise 2: Create a Draft Task Card on Project Board</strong></summary>

**Scenario**: Add a draft card titled "Optimize database queries" to project #3.
```bash
gh project item-create 3 --owner my-org --title "Optimize database queries" --body "Investigate slow queries on billing DB."
```
</details>

<details>
<summary><strong>Exercise 3: View Project Fields Schema via GraphQL</strong></summary>

**Scenario**: Query all custom fields and option IDs for a project.
```bash
gh api graphql -f query='
query {
  organization(login: "my-org") {
    projectV2(number: 3) {
      fields(first: 20) {
        nodes {
          ... on ProjectV2SingleSelectField {
            id
            name
            options {
              id
              name
            }
          }
        }
      }
    }
  }
}'
```
</details>

<details>
<summary><strong>Exercise 4: Filter Project View for Current Sprint</strong></summary>

**Scenario**: Write the search filter query for active sprint items that are in progress.
```text
iteration:@current status:"In Progress"
```
</details>

<details>
<summary><strong>Exercise 5: Add an Issue to Project via GitHub Actions</strong></summary>

**Scenario**: Configure workflow to automatically add bug reports to project board.
```yaml
- uses: actions/add-to-project@v1.0.0
  with:
    project-url: https://github.com/orgs/my-org/projects/3
    github-token: ${{ secrets.PROJECT_PAT }}
```
</details>

<details>
<summary><strong>Exercise 6: Archive Completed Items via CLI</strong></summary>

**Scenario**: Archive an item by ID using GitHub CLI.
```bash
gh project item-archive 3 --owner my-org --id "<item-id>"
```
</details>

---

## 13. Summary & Cheat Sheet

```bash
# GitHub CLI Project Commands
gh project list --owner <org>                     # List org projects
gh project view <number> --owner <org>            # View project overview
gh project item-list <number> --owner <org>       # View items on board
gh project item-create <number> --owner <org> \   # Create draft card
  --title "..." --body "..."
gh project item-archive <number> --owner <org> \  # Archive card
  --id "<item-id>"

# Search Filter Shortcuts
assignee:@me                                      # My assigned tasks
iteration:@current                                # Active sprint
status:Todo,Blocked                               # Unfinished tasks
no:milestone is:open                              # Unplanned backlog
```

---

## 14. Further Reading
- [GitHub Docs - About Projects](https://docs.github.com/en/issues/planning-and-tracking-with-projects/learning-about-projects/about-projects)
- [Automating Projects with Actions](https://github.com/actions/add-to-project)
- [GitHub Projects GraphQL API Reference](https://docs.github.com/en/graphql/reference/objects#projectv2)
- [05-github-collaboration/04-branch-protection-and-rulesets.md](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/04-branch-protection-and-rulesets.md)
