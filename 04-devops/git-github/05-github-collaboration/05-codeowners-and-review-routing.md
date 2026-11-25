---
title: "GitHub CODEOWNERS & Review Routing Architecture"
category: "04-devops"
subcategory: "git-github / 05-github-collaboration"
tags: ["github", "codeowners", "code-review", "review-routing", "governance", "security"]
difficulty: "Intermediate-Advanced"
estimated_read_time: "22 min"
prerequisites:
  - "05-github-collaboration/01-pull-requests-and-code-review"
  - "05-github-collaboration/04-branch-protection-and-rulesets"
last_updated: "2026-11-25"
---

# GitHub CODEOWNERS & Review Routing Architecture

> **Navigation**: [← Previous: 04-branch-protection-and-rulesets.md](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/04-branch-protection-and-rulesets.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/README.md) | [Next: 06-issue-and-pr-templates.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/06-issue-and-pr-templates.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [File Locations & Discovery Precedence](#2-file-locations--discovery-precedence)
3. [Syntax & Pattern Matching Rules](#3-syntax--pattern-matching-rules)
4. [Owner Identification (Users vs Teams vs Emails)](#4-owner-identification-users-vs-teams-vs-emails)
5. [The "Last Matching Rule Wins" Principle](#5-the-last-matching-rule-wins-principle)
6. [Enforcing Code Owner Approvals in Branch Protection](#6-enforcing-code-owner-approvals-in-branch-protection)
7. [Practical Examples](#7-practical-examples)
8. [Best Practices](#8-best-practices)
9. [Common Mistakes](#9-common-mistakes)
10. [Comparison Table: Manual Review Requests vs CODEOWNERS](#10-comparison-table-manual-review-requests-vs-codeowners)
11. [Interview Questions & Answers](#11-interview-questions--answers)
12. [Practice Exercises](#12-practice-exercises)
13. [Summary & Cheat Sheet](#13-summary--cheat-sheet)
14. [Further Reading](#14-further-reading)

---

## 1. What & Why

### What is CODEOWNERS?
A `CODEOWNERS` file is a special configuration file residing in your Git repository that defines which individuals or engineering teams are responsible for specific files, directories, or subsystems within the codebase.

### Why Do We Use It?
1. **Automated Review Requests**: When a pull request modifies files matching a pattern in `CODEOWNERS`, GitHub automatically assigns and requests reviews from the designated owners, eliminating manual reviewer pinging.
2. **Hard Regulatory Approval Gates**: Paired with branch protection ("Require review from Code Owners"), PRs touching sensitive files (e.g. security configs, database migrations, billing logic) **cannot be merged** until an authorized owner formally approves.
3. **Domain Ownership Clarification**: Provides clear organizational ownership in large monorepos, signaling to engineers who to contact regarding architectural questions.

---

## 2. File Locations & Discovery Precedence

GitHub searches for the `CODEOWNERS` file in the following order of precedence. The first file found is used; all others are ignored:
1. `.github/CODEOWNERS` (Recommended standard)
2. `CODEOWNERS` (Repository root)
3. `docs/CODEOWNERS`

> **Note**: The file name must be uppercase `CODEOWNERS` with no file extension.

---

## 3. Syntax & Pattern Matching Rules

The pattern syntax closely mirrors `.gitignore` rules:

```
[ Pattern ]                                  [ Owner(s) ]
# Default owners for everything:
*                                            @my-org/core-engineers

# Specific directory (leading slash pins to repository root):
/services/auth/                              @my-org/security-team

# Match file extension anywhere in repo:
*.sql                                        @my-org/dba-team

# Match all files in a folder, but NOT subfolders:
/docs/*                                      @docs-lead

# Match all files and recursive subfolders:
/packages/ui/**                              @my-org/design-systems
```

---

## 4. Owner Identification (Users vs Teams vs Emails)

Owners can be declared using three formats:
1. **GitHub Username**: `@octocat`
2. **Organization Team**: `@my-org/backend-leads` *(Recommended for resilience against employee turnover)*
3. **Verified Email**: `security@enterprise.com` *(Must match the user's verified GitHub email)*

> **Important**: For a team to act as a Code Owner, the team must have **explicit write permission** on the repository, and the repository must belong to the organization that owns the team!

---

## 5. The "Last Matching Rule Wins" Principle

```
Rule Evaluation Order:

Line 1: *                                    @default-team
Line 2: /services/billing/                   @billing-team
Line 3: /services/billing/stripe.ts          @compliance-lead

Scenario A: PR modifies `/src/app.ts`
  • Matches Line 1 (*).
  • Result: Assigned to @default-team.

Scenario B: PR modifies `/services/billing/invoice.ts`
  • Matches Line 1 (*), then overwritten by Line 2.
  • Result: Assigned to @billing-team.

Scenario C: PR modifies `/services/billing/stripe.ts`
  • Matches Line 1, overwritten by Line 2, overwritten by Line 3.
  • Result: Assigned to @compliance-lead.
```

Unlike `.gitignore` where specific rules override general rules regardless of position, in `CODEOWNERS` **the last matching pattern takes complete ownership**. Earlier matching lines are discarded!

---

## 6. Enforcing Code Owner Approvals in Branch Protection

By default, `CODEOWNERS` only *requests* reviews. To make their approval mandatory:
1. Navigate to **Settings -> Branches -> Branch protection rules** (or Rulesets).
2. Check: **Require a pull request before merging**.
3. Check: **Require review from Code Owners**.

Now, if a PR touches `/services/auth/`, at least one member of `@my-org/security-team` **must approve the pull request**, even if other general reviewers have already approved!

---

## 7. Practical Examples

### Example 1: Comprehensive Production `CODEOWNERS`
```ini
# .github/CODEOWNERS

# Global catch-all fallback
*                                    @my-org/engineering

# Infrastructure & CI/CD
/.github/workflows/                  @my-org/devops @devops-lead
/terraform/                          @my-org/devops
Dockerfile                           @my-org/devops

# Security & Compliance
/services/auth/                      @my-org/security-team
/certs/                              @my-org/security-team
*.pem                                @my-org/security-team

# Database Schemas & Migrations
/prisma/migrations/                  @my-org/database-admins
*.sql                                @my-org/database-admins

# Frontend & Design System
/apps/web/                           @my-org/frontend-team
/packages/ui/                        @my-org/design-system-team

# Documentation & Legal
/docs/                               @tech-writer
LICENSE                              @legal-team
```

### Example 2: Team Code Review Routing via GitHub Team Settings
To prevent fatigue where all 15 team members are tagged for every PR:
1. Go to Organization -> **Teams** -> Select Team -> **Settings**.
2. Click **Code review assignment**.
3. Enable:
   - **Routing Algorithm**: `Round robin` or `Load balance` (assigns to person with fewest reviews).
   - **Count**: 1 or 2 reviewers assigned automatically.
   - **Never assign author**: Ensures team members never review their own PR.

---

## 8. Best Practices

1. **Always Use GitHub Teams Rather Than Individual Users**: If an employee leaves or is on vacation, individual user rules create blocked PRs. Teams allow flexible delegation.
2. **Order Rules from General to Specific**: Put `*` at the top, followed by subdirectories, followed by critical single files at the bottom.
3. **Keep `CODEOWNERS` in `.github/CODEOWNERS`**: Consistent convention standard across all repositories.
4. **Grant Teams Explicit Repository Write Access**: If a team is listed in `CODEOWNERS` without write access to the repository, GitHub silently ignores the rule.
5. **Verify Syntax via GitHub Web UI**: In GitHub's web interface, opening `.github/CODEOWNERS` displays syntax validation banners highlighting invalid usernames or missing teams.

---

## 9. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Inverted rule order | Specific rules overwritten by general rule | Putting `*` at the bottom of the file | Place general catch-alls at top; specific at bottom |
| Team lacks write access | Reviewers are not assigned; rule ignored | Insufficient team permissions | Grant team Write access to repository |
| Missing leading slash | Pattern matches anywhere in the tree unexpectedly | `/auth/` vs `auth/` | Use leading `/` to anchor to repository root |
| File named `codeowners.txt` | GitHub fails to discover file | Lowercase or wrong extension | Use strictly `.github/CODEOWNERS` |
| Multiple owners on separate lines | Only the last line takes effect | Incorrect syntax | Put multiple owners on same line space-separated |
| Assigning external users | Rule silently ignored | User is not a collaborator | Add user as repository collaborator first |

---

## 10. Comparison Table: Manual Review Requests vs CODEOWNERS

| Dimension | Manual Review Requests | CODEOWNERS (Standard) | CODEOWNERS + Branch Protection |
|:---|:---|:---|:---|
| **Assignment Effort** | Manual tagging by author | **100% Automated** | **100% Automated** |
| **Enforcement** | Voluntary | Voluntary (requests review) | **Strictly Enforced Gate** |
| **Domain Ownership** | Implicit / Tribal knowledge | **Explicitly Documented** | **Explicitly Documented** |
| **Audit Compliance** | Fails SOC2 strict review gates | Weak compliance | **Full SOC2 / ISO27001 Compliance** |

---

## 11. Interview Questions & Answers

<details>
<summary><strong>Q1: What determines which rule wins when multiple patterns in a `CODEOWNERS` file match a modified file?</strong></summary>

In `CODEOWNERS`, **the last matching pattern takes complete precedence**. Unlike `.gitignore` (which selects the most specific path regardless of position), `CODEOWNERS` parses rules top-to-bottom. If line 1 matches `*` and line 5 matches `/services/auth/login.ts`, the owner defined on line 5 owns the file. If an author places `*` at the very bottom of the file, it will overwrite every specific rule above it.
</details>

<details>
<summary><strong>Q2: Why must a GitHub Team have explicit write access to a repository for `CODEOWNERS` to function?</strong></summary>

GitHub security rules dictate that only users and teams with Write access (or higher) to a repository can approve pull requests and trigger merges. If a team listed in `CODEOWNERS` only has Read access (or no access), GitHub silently ignores the rule, and reviews will not be automatically requested from that team.
</details>

<details>
<summary><strong>Q3: How does the "Require review from Code Owners" setting in Branch Protection interact with the standard "Required approving reviews" count?</strong></summary>

Both rules must be satisfied independently:
- If required approvals is set to 2, and Code Owner review is required:
  - If 2 general engineers approve, but neither is a Code Owner of the modified files, the PR is **still blocked**.
  - If 1 Code Owner approves, the PR still needs 1 more approval (from anyone) to satisfy the 2-approval requirement.
  - If 2 Code Owners approve, both requirements are satisfied simultaneously.
</details>

<details>
<summary><strong>Q4: Where can a `CODEOWNERS` file be placed within a Git repository?</strong></summary>

GitHub looks for the file in three possible locations in order of precedence:
1. `.github/CODEOWNERS`
2. `CODEOWNERS` (in repository root)
3. `docs/CODEOWNERS`
The first file found is loaded.
</details>

<details>
<summary><strong>Q5: What is Code Review Assignment in GitHub Teams, and how does it prevent review bottlenecks?</strong></summary>

When a large team (e.g. 20 engineers) is specified as a Code Owner, GitHub would normally notify all 20 members for every PR. Code Review Assignment allows team leads to configure automated routing:
- **Round Robin**: Distributes review requests cyclically across members.
- **Load Balancing**: Assigns review requests to the team member with the fewest open reviews.
- **Reviewer Count**: Specifies how many individuals from the team are requested (e.g. exactly 1 or 2).
</details>

<details>
<summary><strong>Q6: How can you specify multiple co-owners for a single path in `CODEOWNERS`?</strong></summary>

List the usernames or team slugs separated by spaces on the same line:
```ini
/security/ @my-org/sec-team @external-auditor @ciso
```
All listed owners will be requested for review.
</details>

<details>
<summary><strong>Q7: Can a user or team own an entire directory while delegating a specific subfolder to another team?</strong></summary>

Yes, by leveraging rule order:
```ini
/packages/ @my-org/frontend-platform
/packages/ui/ @my-org/design-system-team
```
Any changes in `/packages/` are owned by the platform team, except files under `/packages/ui/`, which are owned exclusively by the design system team.
</details>

<details>
<summary><strong>Q8: What happens if a pull request modifies multiple files owned by different teams in `CODEOWNERS`?</strong></summary>

GitHub automatically requests reviews from **every distinct owner** corresponding to all modified files. If branch protection requires Code Owner reviews, **at least one representative from EACH affected owner team must submit an approval** before the PR can be merged.
</details>

---

## 12. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Standard `.github/CODEOWNERS` File</strong></summary>

**Scenario**: Define global fallback, devops owner for workflows, and security owner for auth.
```ini
# .github/CODEOWNERS
*                   @my-org/core-devs
/.github/workflows/ @my-org/devops
/services/auth/     @my-org/security-team
```
</details>

<details>
<summary><strong>Exercise 2: Anchor Pattern with Leading Slash</strong></summary>

**Scenario**: Ensure only root `config.json` is owned, not nested configs.
```ini
/config.json        @lead-architect
```
</details>

<details>
<summary><strong>Exercise 3: Match File Extension Globally</strong></summary>

**Scenario**: Assign all SQL migration files across any folder to DBA team.
```ini
*.sql               @my-org/dba-leads
```
</details>

<details>
<summary><strong>Exercise 4: Verify CODEOWNERS via GitHub CLI</strong></summary>

**Scenario**: Validate file syntax using GitHub API.
```bash
gh api /repos/:owner/:repo/codeowners/errors
```
</details>

<details>
<summary><strong>Exercise 5: Configure Round-Robin Review in Team Settings</strong></summary>

**Scenario**: Navigate to team settings and configure 1 round-robin reviewer.
*(Configured via GitHub Org Settings -> Teams -> Team Settings -> Code review assignment)*.
</details>

<details>
<summary><strong>Exercise 6: Test PR Review Routing</strong></summary>

**Scenario**: Open a PR modifying `/services/auth/` and verify that GitHub automatically assigns `@my-org/security-team`.
```bash
gh pr create --title "feat: update login" --body "Testing review routing"
```
</details>

---

## 13. Summary & Cheat Sheet

```ini
# .github/CODEOWNERS Quick Reference

# Rule 1: Put general rules at TOP, specific rules at BOTTOM
*                        @my-org/all-engineers

# Rule 2: Leading slash anchors to repo root
/services/billing/       @my-org/billing-team

# Rule 3: File extension match everywhere
*.tf                     @my-org/platform-engineers

# Rule 4: Multiple co-owners on same line
/api/schema.graphql      @my-org/frontend-leads @my-org/backend-leads

# Enforcement Checklist:
# [x] Team must have explicit WRITE permission on repository
# [x] Enable "Require review from Code Owners" in Branch Protection
```

---

## 14. Further Reading
- [GitHub Docs - About Code Owners](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-code-owners)
- [Managing Code Review Assignment for Your Team](https://docs.github.com/en/organizations/organizing-members-into-teams/managing-code-review-assignment-for-your-team)
- [05-github-collaboration/06-issue-and-pr-templates.md](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/06-issue-and-pr-templates.md)
