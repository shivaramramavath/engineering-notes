---
title: "GitHub Issue Forms and Pull Request Templates"
category: "04-devops"
subcategory: "git-github / 05-github-collaboration"
tags: ["github", "issue-templates", "pr-templates", "issue-forms", "community-health", "automation"]
difficulty: "Beginner-Intermediate"
estimated_read_time: "20 min"
prerequisites:
  - "05-github-collaboration/01-pull-requests-and-code-review"
  - "05-github-collaboration/02-issues-labels-and-milestones"
last_updated: "2026-11-25"
---

# GitHub Issue Forms and Pull Request Templates

> **Navigation**: [← Previous: 05-codeowners-and-review-routing.md](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/05-codeowners-and-review-routing.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/README.md) | [Next: 07-discussions-and-community.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/07-discussions-and-community.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Legacy Markdown vs Modern YAML Issue Forms](#2-legacy-markdown-vs-modern-yaml-issue-forms)
3. [YAML Issue Forms Architecture & Schema](#3-yaml-issue-forms-architecture--schema)
4. [Configuring the Template Chooser (`config.yml`)](#4-configuring-the-template-chooser-configyml)
5. [Pull Request Templates](#5-pull-request-templates)
6. [Organization-Wide Default Health Files (`.github` Repo)](#6-organization-wide-default-health-files-github-repo)
7. [Practical Examples](#7-practical-examples)
8. [Best Practices](#8-best-practices)
9. [Common Mistakes](#9-common-mistakes)
10. [Comparison Matrix](#10-comparison-matrix)
11. [Interview Questions & Answers](#11-interview-questions--answers)
12. [Practice Exercises](#12-practice-exercises)
13. [Summary & Cheat Sheet](#13-summary--cheat-sheet)
14. [Further Reading](#14-further-reading)

---

## 1. What & Why

### What are Issue & PR Templates?
Issue and Pull Request Templates are pre-formatted scaffolding documents and structured web forms that populate automatically whenever a contributor opens a new issue or creates a pull request.

### Why Do We Use Them?
1. **Eliminate Incomplete Bug Reports**: Force reporters to provide essential diagnostics—reproduction steps, browser/OS versions, log snippets, and minimal reproduction repositories—before submitting.
2. **Standardize PR Verification**: Enforce consistent author checklists (unit tests passed, documentation updated, breaking changes noted) on every proposed diff.
3. **Automated Triage**: Automatically attach labels (`type:bug`), assign teams, and set milestones based on the selected template.
4. **Community Guidance**: Route users to GitHub Discussions or documentation for general questions, keeping the issue tracker clean.

---

## 2. Legacy Markdown vs Modern YAML Issue Forms

```
┌─────────────────────────────────────────────────────────────────────────┐
│ Comparison: Markdown Templates vs YAML Issue Forms                      │
├──────────────────────────┬──────────────────────┬───────────────────────┤
│ Feature                  │ Markdown (.md)       │ YAML Issue Forms      │
├──────────────────────────┼──────────────────────┼───────────────────────┤
│ File Format              │ Free-text Markdown   │ Structured YAML Schema│
│ Input Types              │ Text only            │ Text, Dropdown, Check │
│ Required Validations     │ None (Users delete!) │ Native HTML5 Required │
│ Mobile Friendly          │ Poor                 │ Excellent Web UI      │
│ Target                   │ Issues and PRs       │ **Issues Only**       │
└──────────────────────────┴──────────────────────┴───────────────────────┘
```

> **Important**: Modern YAML Forms are supported for **Issues only**. Pull requests continue to use Markdown templates (`pull_request_template.md`).

---

## 3. YAML Issue Forms Architecture & Schema

Files are placed in `.github/ISSUE_TEMPLATE/<name>.yml`.

### Top-Level Attributes:
- `name`: Display title in template selector.
- `description`: Explanatory subtitle.
- `title`: Default issue subject prefix (e.g. `[Bug]: `).
- `labels`: Auto-applied labels (e.g. `["type:bug", "triage-needed"]`).
- `assignees`: Auto-assigned users.
- `body`: List of interactive form fields.

### Supported Field Types:
1. `markdown`: Informational text and headers for the user.
2. `input`: Single-line text input (can enforce `required: true`).
3. `textarea`: Multi-line text box with code formatting hints.
4. `dropdown`: Selectable choices (single or multi-select).
5. `checkboxes`: Mandatory confirmation checkmarks.

---

## 4. Configuring the Template Chooser (`config.yml`)

Create `.github/ISSUE_TEMPLATE/config.yml` to customize the template selection screen:
```yaml
blank_issues_enabled: false # Disables the "Open a blank issue" button!
contact_links:
  - name: Ask a Question
    url: https://github.com/my-org/my-repo/discussions
    about: Please ask usage and setup questions in Discussions!
  - name: Report Security Vulnerability
    url: https://github.com/my-org/my-repo/security/advisories/new
    about: Privately disclose security vulnerabilities to our team.
```

---

## 5. Pull Request Templates

### Locations Supported by GitHub (in order of discovery):
1. `.github/pull_request_template.md` (Recommended)
2. `pull_request_template.md` (Root)
3. `docs/pull_request_template.md`

### Multiple PR Templates
You can offer multiple PR templates by storing them in `.github/PULL_REQUEST_TEMPLATE/`:
- `.github/PULL_REQUEST_TEMPLATE/feature.md`
- `.github/PULL_REQUEST_TEMPLATE/bugfix.md`
- `.github/PULL_REQUEST_TEMPLATE/release.md`
*Contributors select templates via URL query parameter:* `?template=bugfix.md`.

---

## 6. Organization-Wide Default Health Files (`.github` Repo)

If your organization has 100 repositories, you do **not** need to copy templates into every single repository!
Create a special public repository named strictly `.github`:
```text
https://github.com/my-org/.github
```
Place your default templates inside:
```
my-org/.github/
└── .github/
    ├── ISSUE_TEMPLATE/
    │   ├── bug_report.yml
    │   └── feature_request.yml
    ├── pull_request_template.md
    └── CODEOWNERS
```
Any repository in the organization that lacks its own local template automatically inherits the default templates from `my-org/.github`!

---

## 7. Practical Examples

### Example 1: Production Bug Report Form (`.github/ISSUE_TEMPLATE/bug_report.yml`)
```yaml
name: Bug Report
description: File an actionable bug report to help us improve
title: "[Bug]: "
labels: ["type:bug", "triage-needed"]

body:
  - type: markdown
    attributes:
      value: |
        Thanks for taking the time to report this issue! Please search existing issues before submitting.

  - type: input
    id: version
    attributes:
      label: Software Version
      description: What version of our software are you running? (Run `mycli --version`)
      placeholder: e.g. v2.4.1
    validations:
      required: true

  - type: dropdown
    id: os
    attributes:
      label: Operating System
      description: Which OS did this defect occur on?
      options:
        - Ubuntu / Debian Linux
        - Alpine Linux
        - macOS (Apple Silicon)
        - macOS (Intel)
        - Windows 11
    validations:
      required: true

  - type: textarea
    id: reproduction
    attributes:
      label: Steps to Reproduce
      description: Detailed, minimal step-by-step instructions to reproduce the bug
      placeholder: |
        1. Run `mycli login`
        2. Execute `mycli sync --remote`
        3. See error: Connection timeout
    validations:
      required: true

  - type: textarea
    id: logs
    attributes:
      label: Relevant Log Output
      description: Please copy and paste any relevant terminal output or crash dumps
      render: shell

  - type: checkboxes
    id: terms
    attributes:
      label: Code of Conduct & Prerequisites
      options:
        - label: I have verified this is not a duplicate issue
          required: true
        - label: I agree to follow this project's Code of Conduct
          required: true
```

### Example 2: Production PR Template (`.github/pull_request_template.md`)
```markdown
## Description
<!-- Provide a clear summary of what this PR introduces or fixes. -->

Fixes # <!-- Issue Number -->

## Type of Change
- [ ] 🐛 Bug fix (non-breaking change fixing an issue)
- [ ] ✨ New feature (non-breaking change adding functionality)
- [ ] 💥 Breaking change (fix or feature causing existing functionality to fail)
- [ ] 🧹 Refactoring / Code hygiene
- [ ] 📝 Documentation update

## Verification & Testing
<!-- Describe testing performed to verify this change. -->
- [ ] Unit tests added / updated
- [ ] Integration tests pass locally (`npm run test:e2e`)
- [ ] Tested manually on staging environment

## Checklist
- [ ] My code adheres to the style guidelines of this project
- [ ] I have self-reviewed my own code
- [ ] I have commented complex or non-obvious algorithms
- [ ] Documentation has been updated accordingly
- [ ] No secrets or sensitive credentials committed
```

---

## 8. Best Practices

1. **Disable Blank Issues via `config.yml`**: Setting `blank_issues_enabled: false` forces users through structured forms.
2. **Use Native Field Validations (`required: true`)**: Ensure reporters cannot submit empty bug reports missing version numbers or reproduction steps.
3. **Provide Links to Discussions in `config.yml`**: Redirect questions to Discussions so maintainers are not overwhelmed triaging non-bug questions.
4. **Keep PR Templates Focused and Concise**: A 50-item checklist gets blindly checked. Keep PR checklists under 8–10 high-value items.
5. **Use `.github` Special Repository for Organization Defaults**: Enforce company-wide template standards with zero repository-level duplication.

---

## 9. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Attempting YAML form for PRs | YAML text displayed as raw markdown in PR | GitHub does not support YAML forms for PRs | Use Markdown (`.md`) for PR templates |
| Missing `validations: required: true` | Empty bug reports submitted | Optional default fields | Mark critical inputs as required |
| Invalid YAML syntax in issue form | Template fails to render in UI | Syntax errors in YAML | Validate YAML using `yamllint` |
| Overwhelming 20-field bug forms | Users abandon filing bugs | Excessive form complexity | Ask for only essential diagnostics |
| Forgetting `render: shell` for logs | Logs render as messy unescaped text | Missing code rendering hint | Add `render: shell` to textarea |

---

## 10. Comparison Matrix

| Dimension | Legacy Markdown Issue Template | Modern YAML Issue Form | Pull Request Template |
|:---|:---|:---|:---|
| **Location** | `.github/ISSUE_TEMPLATE/*.md` | `.github/ISSUE_TEMPLATE/*.yml` | `.github/pull_request_template.md` |
| **Input Enforcement** | Voluntary (often erased) | **Mandatory HTML5 Validations** | Voluntary |
| **UI Experience** | Plain markdown editor | Interactive web form controls | Plain markdown editor |
| **Supported For** | Issues | **Issues Only** | **Pull Requests Only** |
| **Auto-Tagging** | Frontmatter `labels:` | Top-level `labels:` | N/A |

---

## 11. Interview Questions & Answers

<details>
<summary><strong>Q1: Why are YAML Issue Forms superior to traditional Markdown Issue Templates?</strong></summary>

Traditional Markdown templates load as raw text into the comment box. Users frequently delete the template instructions, skip critical questions, or fail to format logs.
YAML Issue Forms (`.github/ISSUE_TEMPLATE/*.yml`) render as structured web forms with interactive form widgets (dropdowns, single-line inputs, checkboxes, and textareas). They support native HTML5 validation (`required: true`), preventing users from submitting the issue until all required fields are filled out. They also automatically apply labels and assignees.
</details>

<details>
<summary><strong>Q2: Can you use YAML Forms for Pull Request templates in GitHub?</strong></summary>

**No.** GitHub currently supports YAML Forms strictly for **Issues**. Pull request templates must be written in standard Markdown (`pull_request_template.md`).
</details>

<details>
<summary><strong>Q3: How do you completely prevent contributors from submitting blank, un-templated issues?</strong></summary>

Create a file named `.github/ISSUE_TEMPLATE/config.yml` and set:
```yaml
blank_issues_enabled: false
```
This removes the "Open a regular blank issue" link from the template chooser, forcing all issue authors to select one of your defined templates.
</details>

<details>
<summary><strong>Q4: How does the special `.github` repository work for organization-wide templates?</strong></summary>

If an organization creates a public repository named exactly `.github`, any templates stored inside its `.github/ISSUE_TEMPLATE/` directory or `.github/pull_request_template.md` file serve as the default fallback templates for **all repositories** in that organization. If an individual repository defines its own templates, the local templates override the organizational defaults.
</details>

<details>
<summary><strong>Q5: How can a repository offer multiple different Pull Request templates?</strong></summary>

Store multiple markdown files inside `.github/PULL_REQUEST_TEMPLATE/`:
- `.github/PULL_REQUEST_TEMPLATE/bugfix.md`
- `.github/PULL_REQUEST_TEMPLATE/feature.md`
Contributors can load a specific template by appending a query parameter to the PR creation URL:
`https://github.com/org/repo/compare/main...branch?template=bugfix.md`
</details>

<details>
<summary><strong>Q6: What is the purpose of `contact_links` in `config.yml`?</strong></summary>

`contact_links` allows repository maintainers to display external links on the issue creation page. This is used to guide users toward GitHub Discussions, StackOverflow, Discord, or a dedicated security advisory reporting page before they submit an unnecessary bug report.
</details>

<details>
<summary><strong>Q7: How do you format a multi-line textarea in an Issue Form to automatically render code blocks?</strong></summary>

By adding the `render:` attribute:
```yaml
- type: textarea
  id: traceback
  attributes:
    label: Python Traceback
    render: python
```
When submitted, GitHub automatically fences the content with ````python ... ```` blocks.
</details>

<details>
<summary><strong>Q8: Can issue templates automatically apply labels and assignees? How?</strong></summary>

Yes. In the YAML header of the issue form:
```yaml
labels: ["type:bug", "area:ui"]
assignees: ["frontend-lead"]
```
When an issue is submitted via this template, GitHub automatically attaches the specified labels and assignees.
</details>

---

## 12. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Feature Request YAML Form</strong></summary>

**Scenario**: Create `.github/ISSUE_TEMPLATE/feature_request.yml`.
```yaml
name: Feature Request
description: Propose a new feature or improvement
title: "[Feature]: "
labels: ["type:feature"]
body:
  - type: textarea
    id: problem
    attributes:
      label: Is your feature request related to a problem?
      description: A clear description of what the problem is.
    validations:
      required: true
  - type: textarea
    id: solution
    attributes:
      label: Describe the solution you'd like
      description: Clear description of what you want to happen.
    validations:
      required: true
```
</details>

<details>
<summary><strong>Exercise 2: Create a PR Template with Checklist</strong></summary>

**Scenario**: Create `.github/pull_request_template.md`.
```markdown
## Summary
Fixes #

## Checklist
- [ ] Tests written
- [ ] Documentation updated
```
</details>

<details>
<summary><strong>Exercise 3: Disable Blank Issues via `config.yml`</strong></summary>

**Scenario**: Create `.github/ISSUE_TEMPLATE/config.yml` disabling blank issues.
```yaml
blank_issues_enabled: false
```
</details>

<details>
<summary><strong>Exercise 4: Add Dropdown Menu to Issue Form</strong></summary>

**Scenario**: Add component dropdown selector to issue template.
```yaml
- type: dropdown
  id: component
  attributes:
    label: Affected Component
    options:
      - Authentication
      - Billing API
      - Web Dashboard
  validations:
    required: true
```
</details>

<details>
<summary><strong>Exercise 5: Add Security Advisory Link to Template Chooser</strong></summary>

**Scenario**: Direct vulnerability reporters to private reporting.
```yaml
contact_links:
  - name: Report Security Vulnerability
    url: https://github.com/my-org/my-repo/security/advisories/new
    about: Please report security vulnerabilities privately.
```
</details>

<details>
<summary><strong>Exercise 6: Test Template Loading in GitHub Web UI</strong></summary>

**Scenario**: Commit templates and navigate to `/issues/new/choose` to inspect rendered forms.
</details>

---

## 13. Summary & Cheat Sheet

```text
Repository Template Layout:
.github/
├── ISSUE_TEMPLATE/
│   ├── config.yml           <-- Disables blank issues & defines external links
│   ├── bug_report.yml       <-- Interactive YAML form with required validations
│   └── feature_request.yml  <-- Feature proposal form
└── pull_request_template.md <-- Standardized Markdown PR checklist
```

---

## 14. Further Reading
- [GitHub Docs - Syntax for Issue Forms](https://docs.github.com/en/communities/using-templates-to-encourage-useful-issues-and-pull-requests/syntax-for-issue-forms)
- [Creating a Pull Request Template](https://docs.github.com/en/communities/using-templates-to-encourage-useful-issues-and-pull-requests/creating-a-pull-request-template-for-your-repository)
- [Creating Default Community Health Files](https://docs.github.com/en/communities/setting-up-your-project-for-healthy-contributions/creating-a-default-community-health-file)
- [05-github-collaboration/07-discussions-and-community.md](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/07-discussions-and-community.md)
