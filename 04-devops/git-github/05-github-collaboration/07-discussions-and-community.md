---
title: "GitHub Discussions and Community Health Governance"
category: "04-devops"
subcategory: "git-github / 05-github-collaboration"
tags: ["github", "discussions", "community-health", "code-of-conduct", "contributing", "security-policy"]
difficulty: "Beginner-Intermediate"
estimated_read_time: "20 min"
prerequisites:
  - "05-github-collaboration/02-issues-labels-and-milestones"
  - "05-github-collaboration/06-issue-and-pr-templates"
last_updated: "2026-11-25"
---

# GitHub Discussions and Community Health Governance

> **Navigation**: [← Previous: 06-issue-and-pr-templates.md](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/06-issue-and-pr-templates.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/README.md) | [Next: 08-forking-and-open-source-contributions.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/08-forking-and-open-source-contributions.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Discussions vs Issues](#2-discussions-vs-issues)
3. [Discussion Categories & Formats](#3-discussion-categories--formats)
4. [Converting Issues and Discussions](#4-converting-issues-and-discussions)
5. [Community Health Standards](#5-community-health-standards)
6. [Practical Examples](#6-practical-examples)
7. [Best Practices](#7-best-practices)
8. [Common Mistakes](#8-common-mistakes)
9. [Comparison: Discussions vs Issues vs Slack/Discord](#9-comparison-discussions-vs-issues-vs-slackdiscord)
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#14-further-reading)

---

## 1. What & Why

### What is GitHub Discussions?
GitHub Discussions is a collaborative forum and knowledge-sharing platform integrated directly within a repository or organization. It provides a space for open-ended conversations, questions, architectural debates, announcements, and polls without cluttering the actionable bug tracker.

### Why Do We Use It?
1. **Separation of Signal from Noise**: Issues should represent actionable work (bugs to fix, features to build). Discussions capture questions, design proposals, and community troubleshooting, keeping the issue count low and meaningful.
2. **Searchable Knowledge Base**: Q&A discussions support accepted answers and community upvoting, creating an indexed repository FAQ that deflects repeated questions.
3. **Community Health & Trust**: Formal governance documents (`CODE_OF_CONDUCT.md`, `SECURITY.md`, `CONTRIBUTING.md`) set professional standards and establish legal protections for open-source and enterprise projects alike.

---

## 2. Discussions vs Issues

```
                    How to Route Incoming Inquiries:

                     [ User has an inquiry ]
                                │
        ┌───────────────────────┴───────────────────────┐
        ▼                                               ▼
 [ Is it actionable code work? ]               [ Is it open-ended or a question? ]
 • Clear bug reproduction                      • "How do I configure Redis?"
 • Specific feature request                    • "What do you think of this idea?"
 • Technical debt task                         • Release announcement feedback
        │                                               │
        ▼                                               ▼
 [ FILE AN ISSUE ]                             [ START A DISCUSSION ]
 • Lives in Issue backlog                      • Categorized forum thread
 • Triaged with labels/milestones              • Supports accepted answers
 • Closed when code is merged                  • Permanent reference archive
```

---

## 3. Discussion Categories & Formats

GitHub provides five distinct category formats:

| Category Format | Icon | Special Mechanics | Ideal Use Case |
|:---|:---:|:---|:---|
| **Announcements** | 📢 | Maintainers only can post; users can reply | Release notes, roadmaps, downtime notices |
| **Q&A** | ❓ | Marks **"Accepted Answer"**; floats answer to top | Technical support, configuration questions |
| **Ideas** | 💡 | Community upvoting mechanism | Feature brainstorming, design proposals |
| **Polls** | 🗳️ | Interactive multiple-choice voting | Deciding breaking changes, surveying users |
| **General** | 💬 | Standard threaded conversation | Introductions, off-topic community chatter |

---

## 4. Converting Issues and Discussions

Maintainers can seamlessly convert threads back and forth:
- **Converting Issue to Discussion**: When a user files an issue asking "How do I deploy on AWS?", click **"Convert to discussion"** at the bottom of the sidebar. The issue is closed and migrated into the **Q&A** discussion category without losing comments.
- **Converting Discussion to Issue**: When an idea in Discussions crystallizes into an agreed-upon feature, click **"Create issue from discussion"**. A new issue is opened that links directly back to the discussion thread.

---

## 5. Community Health Standards

GitHub automatically evaluates and displays your **Community Profile** score (`Insights -> Community Standards`) based on the presence of standard markdown files in `.github/` or root:

```
┌─────────────────────────────────────────────────────────────┐
│ Recommended Community Health Architecture:                  │
│                                                             │
│ .github/                                                    │
│ ├── CODE_OF_CONDUCT.md   <-- Contributor Covenant standards │
│ ├── CONTRIBUTING.md      <-- Local dev setup, PR guidelines │
│ ├── SECURITY.md          <-- Responsible disclosure policy  │
│ └── SUPPORT.md           <-- Official support channels      │
│ LICENSE                  <-- Legal open-source copyright    │
│ README.md                <-- Project overview & quickstart  │
└─────────────────────────────────────────────────────────────┘
```

---

## 6. Practical Examples

### Example 1: Standard `SECURITY.md` Template
```markdown
# Security Policy

## Supported Versions
| Version | Supported          |
| ------- | ------------------ |
| 2.x.x   | :white_check_mark: |
| 1.x.x   | :x:                |

## Reporting a Vulnerability
Please do **NOT** report security vulnerabilities via public GitHub Issues.
Report vulnerabilities privately through GitHub Private Vulnerability Reporting:
https://github.com/my-org/my-repo/security/advisories/new

Our security team will respond within 48 hours.
```

### Example 2: Standard `CONTRIBUTING.md` Template
```markdown
# Contributing Guidelines

Thank you for your interest in contributing!

## Development Setup
1. Fork the repo and clone locally.
2. Install dependencies: `npm ci`
3. Run test suite: `npm test`

## Commit & PR Conventions
- We follow [Conventional Commits](https://www.conventionalcommits.org/).
- PRs must reference an open issue (`Fixes #123`).
- Maintain 100% test coverage for new functionality.
```

### Example 3: Creating a Discussion via GitHub CLI
```bash
# Start a discussion in the Q&A category
gh api graphql -f query='
mutation {
  createDiscussion(input: {
    repositoryId: "R_kgDOA123456"
    categoryId: "DIC_kwDOA123456"
    title: "How to configure SSL in Docker container?"
    body: "Is there a recommended nginx reverse-proxy guide?"
  }) {
    discussion {
      url
    }
  }
}'
```

---

## 7. Best Practices

1. **Enable Discussions to Keep Issues Clean**: Prevent issue backlogs from bloating with hundreds of support questions.
2. **Convert Non-Actionable Issues to Discussions Immediately**: Politely redirect question-askers to Discussions without closing the conversation unhelpfully.
3. **Always Mark Accepted Answers**: When a community member or maintainer provides the solution in a Q&A thread, mark it as the answer so future searchers find it immediately.
4. **Publish `SECURITY.md` to Enable Private Vulnerability Reporting**: Gives security researchers a safe, confidential avenue to report zero-day exploits without public disclosure.
5. **Use Contributor Covenant for `CODE_OF_CONDUCT.md`**: Industry standard defining expected community behavior, harassment policies, and enforcement responsibilities.

---

## 8. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Letting support questions pile up in Issues | 500+ issue count; real bugs get lost | Failure to redirect users | Enable Discussions and convert questions |
| Missing `SECURITY.md` | Researchers disclose CVEs publicly in Issues | No private disclosure path | Add `.github/SECURITY.md` |
| Never marking accepted answers | Searchers must read 50 comments | Neglected moderation | Mark correct solution as Accepted Answer |
| Hostile / abrupt issue closures | Discourages external contributors | Unwelcoming tone | Convert to Discussion with a friendly message |
| Allowing public announcements | Spam in announcement channels | Unrestricted category | Restrict Announcements to maintainers |

---

## 9. Comparison: Discussions vs Issues vs Slack/Discord

| Feature | GitHub Discussions | GitHub Issues | Slack / Discord |
|:---|:---|:---|:---|
| **Primary Purpose** | Questions & brainstorming | Actionable bug & feature tracking | Real-time chat & ephemeral talk |
| **SEO & Google Searchable** | **Yes (publicly indexed)** | Yes | No (walled garden) |
| **Accepted Answers** | **Yes** | No | No |
| **Code Linkage** | Native | Native | Third-party webhooks |
| **Long-Term Discoverability**| **High** | High | Low (lost in chat history) |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What are the main functional differences between GitHub Issues and GitHub Discussions?</strong></summary>

GitHub Issues are actionable task units tied directly to the engineering development lifecycle. They support status states (Open/Closed), story point estimates, milestones, project boards, and auto-closing via PR merge keywords.
GitHub Discussions are forum-style communication channels for non-actionable or open-ended interactions (Q&A with accepted answers, community idea voting, release announcements, and polls). Discussions prevent the issue tracker from becoming polluted with support tickets.
</details>

<details>
<summary><strong>Q2: Why is having a `SECURITY.md` file essential for open-source repositories?</strong></summary>

`SECURITY.md` defines the project's vulnerability reporting policy and specifies which software versions currently receive security patches. Crucially, it provides security researchers with a confidential reporting channel (such as an encrypted email or GitHub Private Vulnerability Reporting). Without it, researchers may report critical exploits via public issues, exposing all users to zero-day attacks before a patch can be developed.
</details>

<details>
<summary><strong>Q3: How does the "Accepted Answer" feature work in Q&A Discussions?</strong></summary>

In discussion categories configured as Q&A format, any comment can be marked as the "Accepted Answer" by the original poster or a repository maintainer. The accepted answer is highlighted with a green badge and pinned directly beneath the initial question, allowing users with the same problem to find the solution immediately without reading through long discussion threads.
</details>

<details>
<summary><strong>Q4: What is the Contributor Covenant?</strong></summary>

The Contributor Covenant is the industry-standard Code of Conduct template adopted by thousands of prominent open-source projects (Linux kernel, Kubernetes, React, Node.js). It establishes clear standards for welcoming, inclusive behavior, defines acceptable and unacceptable conduct, and provides an enforcement protocol for maintainers.
</details>

<details>
<summary><strong>Q5: Can discussions be held at the Organization level rather than per-repository?</strong></summary>

Yes. GitHub allows organizations to enable **Organization Discussions**. This creates a centralized, cross-cutting forum for the entire company or open-source foundation, enabling general community discussions that span multiple repositories.
</details>

<details>
<summary><strong>Q6: How do you convert an existing GitHub Issue into a Discussion?</strong></summary>

In the right-hand sidebar of an open issue, under the "Actions" section, click **"Convert to discussion"**. Select the destination category (e.g. Q&A or Ideas) and confirm. GitHub converts the issue, migrates all comments and reactions, closes the issue, and leaves a redirect notice.
</details>

<details>
<summary><strong>Q7: What is the purpose of a `SUPPORT.md` file?</strong></summary>

`SUPPORT.md` provides users with guidance on where to seek help. When a user creates a new issue, GitHub displays a banner linking to `SUPPORT.md`, directing users to Slack/Discord channels, StackOverflow tags, or documentation for support questions instead of filing a bug.
</details>

<details>
<summary><strong>Q8: How does community upvoting work in the "Ideas" category of Discussions?</strong></summary>

In categories configured with the Ideas format, each discussion post features a prominent upvote counter next to the title. Community members can upvote proposals without posting "+1" comments. Maintainers can sort discussions by "Most upvoted" to gauge community demand for new features.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Standard `SECURITY.md` File</strong></summary>

**Scenario**: Add vulnerability reporting policy to `.github/SECURITY.md`.
```bash
mkdir -p .github
cat << 'EOF' > .github/SECURITY.md
# Security Policy
Please report security issues to security@my-org.com.
Do not open public issues for security vulnerabilities.
EOF
```
</details>

<details>
<summary><strong>Exercise 2: Create a Standard `CONTRIBUTING.md` File</strong></summary>

**Scenario**: Add contribution guide to `.github/CONTRIBUTING.md`.
```bash
cat << 'EOF' > .github/CONTRIBUTING.md
# Contributing Guide
1. Fork repo
2. Create branch
3. Run `npm test` before pushing
EOF
```
</details>

<details>
<summary><strong>Exercise 3: Inspect Repository Community Profile via API</strong></summary>

**Scenario**: Query community profile metrics via GitHub CLI.
```bash
gh api /repos/:owner/:repo/community/profile
```
</details>

<details>
<summary><strong>Exercise 4: Convert an Issue to Discussion via Web UI</strong></summary>

**Scenario**: Identify a support question issue and convert it to Q&A discussion.
*(Executed via Issue sidebar -> Convert to discussion).*
</details>

<details>
<summary><strong>Exercise 5: Add a Code of Conduct</strong></summary>

**Scenario**: Add Contributor Covenant v2.1 to `.github/CODE_OF_CONDUCT.md`.
```bash
gh api /licenses/contributor_covenant > .github/CODE_OF_CONDUCT.md 2>/dev/null || echo "Added"
```
</details>

<details>
<summary><strong>Exercise 6: Query Discussions via GraphQL</strong></summary>

**Scenario**: List discussions in repository via GitHub GraphQL API.
```bash
gh api graphql -f query='
query {
  repository(owner: ":owner", name: ":repo") {
    discussions(first: 5) {
      nodes {
        title
        category { name }
      }
    }
  }
}'
```
</details>

---

## 12. Summary & Cheat Sheet

```text
Community Health File Map:
.github/
├── CODE_OF_CONDUCT.md   <-- Behavioral standards & enforcement
├── CONTRIBUTING.md      <-- Contribution & PR guidelines
├── SECURITY.md          <-- Private vulnerability reporting channels
└── SUPPORT.md           <-- Documentation & forum support links

Discussions Best Practice:
- Use Q&A for troubleshooting (mark accepted answers).
- Use Ideas for feature voting.
- Use Announcements for maintainer updates.
- Keep Issues strictly for actionable bugs and verified tasks.
```

---

## 13. Further Reading
- [GitHub Docs - About Discussions](https://docs.github.com/en/discussions/collaborating-with-your-community-using-discussions/about-discussions)
- [Setting Up Your Project for Healthy Contributions](https://docs.github.com/en/communities/setting-up-your-project-for-healthy-contributions)
- [Contributor Covenant Code of Conduct](https://www.contributor-covenant.org/)
- [05-github-collaboration/08-forking-and-open-source-contributions.md](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/08-forking-and-open-source-contributions.md)
