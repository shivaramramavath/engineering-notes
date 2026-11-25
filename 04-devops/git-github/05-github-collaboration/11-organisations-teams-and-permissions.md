---
title: "GitHub Organizations, Teams & Enterprise Access Control"
category: "04-devops"
subcategory: "git-github / 05-github-collaboration"
tags: ["github", "organizations", "teams", "rbac", "permissions", "enterprise", "scim", "sso"]
difficulty: "Advanced"
estimated_read_time: "26 min"
prerequisites:
  - "05-github-collaboration/04-branch-protection-and-rulesets"
  - "05-github-collaboration/05-codeowners-and-review-routing"
last_updated: "2026-11-25"
---

# GitHub Organizations, Teams & Enterprise Access Control

> **Navigation**: [← Previous: 10-github-releases-and-packages.md](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/10-github-releases-and-packages.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/README.md) | [Next Module: 06-workflows-and-strategies →](file:///e:/Learning/engineering-notes/04-devops/git-github/06-workflows-and-strategies/README.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Identity Hierarchy: Enterprise vs Organization vs User](#2-identity-hierarchy-enterprise-vs-organization-vs-user)
3. [Base Repository Permissions & Role Taxonomy](#3-base-repository-permissions--role-taxonomy)
4. [Team Architecture & Nested Hierarchies](#4-team-architecture--nested-hierarchies)
5. [IdP Team Sync & SAML SSO / SCIM Provisioning](#5-idp-team-sync--saml-sso--scim-provisioning)
6. [Custom Repository Roles](#6-custom-repository-roles)
7. [Audit Log Streaming & Enterprise Governance](#7-audit-log-streaming--enterprise-governance)
8. [Practical Examples](#8-practical-examples)
9. [Best Practices](#9-best-practices)
10. [Common Mistakes](#10-common-mistakes)
11. [Role Comparison Matrix](#11-role-comparison-matrix)
12. [Interview Questions & Answers](#12-interview-questions--answers)
13. [Practice Exercises](#13-practice-exercises)
14. [Summary & Cheat Sheet](#14-summary--cheat-sheet)
15. [Further Reading](#15-further-reading)

---

## 1. What & Why

### What is GitHub Enterprise Access Control?
GitHub access control is the role-based security framework governing authentication, authorization, team collaboration, and repository permissions across an enterprise's software portfolio. It scales from individual developers up through nested engineering teams, organization boundaries, and multi-tenant enterprise accounts.

### Why Does It Matter?
1. **Principle of Least Privilege**: Ensure developers have read or write access only to the repositories they actively maintain, blocking unauthorized access to proprietary intellectual property.
2. **Automated Onboarding & Offboarding**: Connect identity providers (Okta, Azure AD, PingIdentity) via SCIM to instantly provision new hires and de-provision departing employees across all repositories automatically.
3. **Auditing & Compliance**: Strict SOC2, FedRAMP, and HIPAA compliance mandates require centralized audit logs recording every repository creation, permission escalation, and SSH key addition.
4. **Resilient Team Ownership**: Assign repository permissions and CODEOWNERS to dynamic teams rather than individual user accounts.

---

## 2. Identity Hierarchy: Enterprise vs Organization vs User

```
┌─────────────────────────────────────────────────────────────┐
│ GitHub Enterprise Account (e.g. acme-corp)                  │
│ • Central billing, license pool, and security policies      │
│ • SAML SSO / SCIM IdP integration                           │
│ • Organization creation policies & audit log streaming      │
└──────────────────────────────┬──────────────────────────────┘
                               │
            ┌──────────────────┴──────────────────┐
            ▼                                     ▼
┌──────────────────────────┐         ┌──────────────────────────┐
│ Organization: Core-Apps  │         │ Organization: Data-Eng   │
│ • Base Permission: None  │         │ • Base Permission: None  │
│ • Managed Teams & IdP    │         │ • Managed Teams & IdP    │
└───────────┬──────────────┘         └───────────┬──────────────┘
            │                                    │
    ┌───────┴───────┐                    ┌───────┴───────┐
    ▼               ▼                    ▼               ▼
┌────────┐     ┌────────┐            ┌────────┐     ┌────────┐
│ Repo A │     │ Repo B │            │ Repo C │     │ Repo D │
└────────┘     └────────┘            └────────┘     └────────┘
```

---

## 3. Base Repository Permissions & Role Taxonomy

### Organization Base Permission
When a new repository is created, the organization's **Base Permission** determines what access general members receive automatically:
- **No permission (Recommended for Enterprise)**: Members have zero access until explicitly granted via team membership.
- **Read**: Members can view and clone all internal repositories.
- **Write**: Members can push to non-protected branches.

### The 5 Standard Repository Roles:
1. **Read**: View, clone, open issues, and submit PRs.
2. **Triage**: Read + assign labels, milestones, and close/reopen issues.
3. **Write**: Triage + push directly to unprotected branches, merge PRs.
4. **Maintain**: Write + manage repository settings, labels, and milestones (cannot delete repository or manage access).
5. **Admin**: Maintain + delete repository, manage collaborators, secrets, and branch protection.

---

## 4. Team Architecture & Nested Hierarchies

Teams reflect your company's organizational chart. Teams can be nested to inherit permissions automatically:

```
[ Engineering (Parent Team) ] ──► (Access: All Engineering General Repos)
      │
      ├── [ Backend Team ] ────► (Inherits Engineering + Access to Backend Repos)
      │         │
      │         └── [ Auth Sub-team ] ──► (Access to Security & Auth Repos)
      │
      └── [ Frontend Team ] ───► (Inherits Engineering + Access to Web/Mobile Repos)
```

Child teams inherit:
- Members of child teams are automatically members of parent teams.
- Permissions granted to parent teams cascade downwards to child teams.

---

## 5. IdP Team Sync & SAML SSO / SCIM Provisioning

### 1. SAML Single Sign-On (SSO)
Users authenticate against corporate Okta / Azure Active Directory credentials before accessing enterprise organization assets.

### 2. SCIM (System for Cross-domain Identity Management)
Automatically creates, updates, and deletes GitHub user accounts when HR systems add or terminate employees.

### 3. IdP Team Sync
Connects GitHub Teams directly to IdP Security Groups:
- Add employee to `okta-group-backend` in Okta.
- Employee is automatically added to `@my-org/backend-team` in GitHub within minutes!

---

## 6. Custom Repository Roles

For enterprise organizations needing granular permissions beyond the 5 standard roles, **Custom Roles** allow administrators to assemble specific permission sets:
- **Example: "Release Engineer"**: Read + Write + manage releases and tags + bypass branch protection (without admin rights).
- **Example: "Security Reviewer"**: Read + view CodeQL alerts + view secret scanning alerts (without push access).

---

## 7. Audit Log Streaming & Enterprise Governance

Enterprise Cloud accounts can stream real-time JSON audit logs directly to SIEM systems:
- **Supported Sinks**: Amazon S3, Azure Blob, Google Cloud Storage, Splunk, Datadog.
- **Logged Events**: SSH key creation, member invitations, repository transfers, team deletions, branch protection changes, and PAT creation.

---

## 8. Practical Examples

### Example 1: Creating an Organization Team via GitHub CLI
```bash
# Create a team with closed privacy (visible within org)
gh api --method POST /orgs/my-org/teams \
  -f name="Billing Engineers" \
  -f description="Engineers maintaining Stripe integrations" \
  -f privacy="closed"
```

### Example 2: Granting Team Access to a Repository
```bash
# Grant "Billing Engineers" team 'push' (Write) access to repo
gh api --method PUT /orgs/my-org/teams/billing-engineers/repos/my-org/billing-service \
  -f permission="push"
```

### Example 3: Auditing Repository Collaborators via CLI
```bash
# List all teams and users with access to a specific repo
gh api /repos/my-org/billing-service/collaborators \
  --jq '.[] | {user: .login, role: .role_name}'
```

---

## 9. Best Practices

1. **Set Organization Base Permission to "None"**: Never grant default read/write access to all repositories. Enforce explicit team-based grants.
2. **Use IdP Team Sync for All Access Grants**: Never manually invite individual users to teams; map GitHub teams to corporate Okta/Azure AD groups.
3. **Enforce Two-Factor Authentication (2FA) Organization-Wide**: Mandatory requirement across all enterprise accounts.
4. **Use Nested Teams Sparingly**: Keep nesting depth to 2–3 levels maximum to avoid confusing permission inheritance webs.
5. **Restrict Repository Creation to Organization Admins**: Prevent developers from spawning unmonitored private or public repositories without approval.

---

## 10. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Base permission set to "Write" | Any contractor can edit any internal repo | Default permissive setting | Set organization base permission to "None" |
| Granting permissions to individual users | Security debt when roles change | Bypassing team structures | Assign permissions strictly to Teams |
| 20 Organization Owners | Too many users can delete org or wipe policies | Over-granting Admin access | Limit Org Owners to 3–5 trusted administrators |
| Orphaned ex-employee accounts | Former staff retain repo access | Manual offboarding failures | Enable SCIM automated provisioning |
| Allowing members to make repos public | Accidental intellectual property leaks | Permissive member privileges | Restrict public repo creation to Org Admins |

---

## 11. Role Comparison Matrix

| Permission Capability | Read | Triage | Write | Maintain | Admin |
|:---|:---:|:---:|:---:|:---:|:---:|
| Clone and view code | Yes | Yes | Yes | Yes | Yes |
| Manage labels & milestones | No | **Yes** | Yes | Yes | Yes |
| Push to unprotected branches | No | No | **Yes** | Yes | Yes |
| Manage repo topics & descriptions | No | No | No | **Yes** | Yes |
| Manage webhooks & deploy keys | No | No | No | No | **Yes** |
| Manage collaborators & permissions| No | No | No | No | **Yes** |
| Delete repository | No | No | No | No | **Yes** |

---

## 12. Interview Questions & Answers

<details>
<summary><strong>Q1: What are the differences between an Organization Owner, an Organization Member, and a Billing Manager?</strong></summary>

- **Organization Owner**: Full administrative power over the organization. Can add/remove members, manage billing, create/delete teams, adjust security settings, configure SAML/SSO, and delete the organization.
- **Organization Member**: Standard employee account. Can view organization members and teams, and access repositories strictly according to assigned team permissions.
- **Billing Manager**: Specialized account that can view and modify payment methods, invoices, and subscription plans without having any access to repository source code.
</details>

<details>
<summary><strong>Q2: Why is setting the Organization Base Permission to "None" recommended for enterprise security?</strong></summary>

If base permission is set to "Read" or "Write", every newly invited member (including external contractors, interns, or guest auditors) automatically receives access to every internal repository in the company. Setting base permission to "None" enforces the Principle of Least Privilege: users see and access **only** the specific repositories explicitly assigned to their department's team.
</details>

<details>
<summary><strong>Q3: How does SCIM provisioning improve enterprise security in GitHub?</strong></summary>

SCIM (System for Cross-domain Identity Management) automates the identity lifecycle. When an employee is terminated and deactivated in corporate HR software (e.g. Workday), the change propagates to the identity provider (Okta/Azure AD), which immediately instructs GitHub's SCIM API to de-provision the user. This removes their organization access, revokes active sessions, and deletes assigned tokens in real time, preventing unauthorized access by former employees.
</details>

<details>
<summary><strong>Q4: What is the difference between the "Write" role and the "Maintain" role on a repository?</strong></summary>

Both roles allow pushing code and merging pull requests. However, the **Maintain** role grants additional repository management capabilities without giving full Admin rights:
- Maintainers can edit repository topics, descriptions, and default branches.
- Maintainers can configure issue templates and discussions.
- Crucially, Maintainers **cannot** delete the repository, modify branch protection rules, manage secrets, or grant collaborator access (which remains restricted to Admins).
</details>

<details>
<summary><strong>Q5: What are Custom Repository Roles and when are they necessary?</strong></summary>

Custom Repository Roles allow enterprise administrators to assemble specific combinations of 40+ granular permissions into a tailor-made role. They are used when standard roles are either too restrictive or too permissive (e.g. creating a "Release Manager" role that can manage tags and releases without granting full Admin privileges, or a "Security Auditor" role that can view CodeQL alerts without push access).
</details>

<details>
<summary><strong>Q6: How does Team permission inheritance work in nested teams?</strong></summary>

Team hierarchies function hierarchically. A child team inherits all repository access granted to its parent team. Furthermore, every member of a child team is automatically recognized as an indirect member of the parent team. However, permissions granted explicitly to a child team do not travel upwards to the parent team.
</details>

<details>
<summary><strong>Q7: What is Audit Log Streaming and why is it preferred over manual audit log exports?</strong></summary>

Audit Log Streaming automatically exports enterprise security events in real-time JSON format directly into an enterprise SIEM (Splunk, Datadog) or cloud storage (AWS S3). Unlike manual CSV exports (which are delayed and labor-intensive), streaming enables real-time threat detection, automated alerting on suspicious privilege escalations, and immutable long-term compliance archiving.
</details>

<details>
<summary><strong>Q8: What happens to a user's fork of a private repository when the user is removed from the organization?</strong></summary>

When a user is removed from a private organization, GitHub automatically deletes all private forks of that organization's repositories created by that user, ensuring proprietary corporate source code cannot be retained on personal developer accounts after departure.
</details>

---

## 13. Practice Exercises

<details>
<summary><strong>Exercise 1: Create an Organization Team via CLI</strong></summary>

**Scenario**: Create a closed team named "DevOps Leads".
```bash
gh api --method POST /orgs/my-org/teams -f name="DevOps Leads" -f privacy="closed"
```
</details>

<details>
<summary><strong>Exercise 2: Assign Team to Repository with Maintain Role</strong></summary>

**Scenario**: Grant "DevOps Leads" the maintain role on `infra-repo`.
```bash
gh api --method PUT /orgs/my-org/teams/devops-leads/repos/my-org/infra-repo -f permission="maintain"
```
</details>

<details>
<summary><strong>Exercise 3: Query Organization Audit Log via CLI</strong></summary>

**Scenario**: Query the 5 most recent security events in your organization.
```bash
gh api /orgs/my-org/audit-log?per_page=5
```
</details>

<details>
<summary><strong>Exercise 4: Add Team Member</strong></summary>

**Scenario**: Add user to team via GitHub API.
```bash
gh api --method PUT /orgs/my-org/teams/devops-leads/memberships/octocat
```
</details>

<details>
<summary><strong>Exercise 5: Inspect Repository Permission Levels</strong></summary>

**Scenario**: Check permission level of a specific collaborator.
```bash
gh api /repos/my-org/infra-repo/collaborators/octocat/permission
```
</details>

<details>
<summary><strong>Exercise 6: Restrict Repository Creation Permissions</strong></summary>

**Scenario**: Ensure only organization admins can create repositories.
```bash
gh api --method PATCH /orgs/my-org -F members_can_create_repositories=false
```
</details>

---

## 14. Summary & Cheat Sheet

```bash
# Organization & Team CLI Management
gh api /orgs/<org>/teams                          # List all teams
gh api --method POST /orgs/<org>/teams \          # Create team
  -f name="Backend" -f privacy="closed"
gh api --method PUT /orgs/<org>/teams/<team>/repos/<org>/<repo> \
  -f permission="push"                            # Grant Write access

# Enterprise Security Baseline:
# [x] Base permission = None
# [x] Mandatory 2FA enabled
# [x] SAML SSO & SCIM automated provisioning
# [x] Permissions managed via Teams (never individual users)
# [x] Real-time audit log streaming enabled to SIEM
```

---

## 15. Further Reading
- [GitHub Docs - About Organizations](https://docs.github.com/en/organizations/collaborating-with-groups-in-organizations/about-organizations)
- [Managing Team Access to an Organization Repository](https://docs.github.com/en/organizations/managing-user-access-to-your-organizations-repositories/managing-team-access-to-an-organization-repository)
- [Enterprise Audit Log Streaming](https://docs.github.com/en/enterprise-cloud@latest/admin/monitoring-activity-in-your-enterprise/reviewing-audit-logs-for-your-enterprise/streaming-the-audit-log-for-your-enterprise)
- [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/README.md)
