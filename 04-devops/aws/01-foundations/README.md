# Module 01: Cloud Foundations & Governance

Welcome to the **AWS Cloud Foundations & Governance** module. This curriculum establishes the core operational baseline for architecting, securing, and operating production systems on Amazon Web Services.

---

## Module Overview

This module provides an in-depth, production-ready foundation covering AWS infrastructure topology, security boundaries, multi-account governance, identity modeling, the credential provider chain, and financial operations (FinOps).

Every topic adheres strictly to current AWS best practices: enforcing **least privilege**, mandatory **IMDSv2**, multi-account organization with **AWS Organizations & SCPs**, **IAM Identity Center (SSO)**, temporary short-lived credentials, and continuous cost anomaly detection.

---

## Detailed Curriculum Roadmap

| # | Note | Description | Key Focus Areas |
|:---|:---|:---|:---|
| **01** | [What is AWS?](01-what-is-aws.md) | AWS cloud computing model, evolution, service categories, and operational tenets | Cloud paradigms (IaaS/PaaS/SaaS), Capex vs Opex, global reach, elasticity |
| **02** | [Shared Responsibility & Well-Architected Framework](02-shared-responsibility-and-well-architected.md) | The AWS security boundary model and the Six Pillars of architectural excellence | Security of vs in the cloud, Operational Excellence, Security, Reliability, Performance, Cost, Sustainability |
| **03** | [Accounts, Regions, and Billing Setup](03-accounts-regions-and-billing.md) | Account boundaries, isolation models, consolidated billing, and root account hardening | Multi-account isolation, root user MFA/access key destruction, billing alarms |
| **04** | [AWS Organizations, SCPs, and Control Tower](04-organizations-scp-and-control-tower.md) | Enterprise multi-account hierarchy, Service Control Policies, and automated landing zones | Management vs Member accounts, OU inheritance, SCP deny policies, Control Tower guardrails |
| **05** | [IAM Fundamentals](05-iam-fundamentals.md) | Principals, users, groups, roles, and JSON identity policy structure | Principals, trust policies vs permission policies, role assumption via STS, permission boundaries |
| **06** | [IAM Policy Evaluation & Least Privilege](06-iam-policy-evaluation-and-least-privilege.md) | Deterministic policy decision engine, condition keys, and least privilege engineering | Explicit Deny > Explicit Allow > Default Deny, Condition blocks, IAM Access Analyzer |
| **07** | [IAM Identity Center & Single Sign-On](07-iam-identity-center-and-sso.md) | Centralized enterprise workforce authentication, permission sets, and IdP federation | SAML 2.0 / OIDC federation, Okta/Azure AD integration, short-lived session tokens |
| **08** | [AWS CLI v2 Installation & Configuration](08-aws-cli.md) | Modern CLI operations, named profiles, SSO integration, output filtering, and scripting | CLI v2 features, `aws sso login`, JMESPath querying (`--query`), `--output json/table` |
| **09** | [SDKs & The Credential Provider Chain](09-sdks-and-credential-provider-chain.md) | How language runtimes locate credentials, STS AssumeRole, and IMDSv2 vs IMDSv1 | Credential resolution order, STS tokens, token-gated IMDSv2 metadata security |
| **10** | [AWS Console, CloudShell, and Tagging](10-console-cloudshell-and-tagging.md) | Browser console workflows, ephemeral CloudShell environments, and tag enforcement | Tagging taxonomy, Cost Allocation Tags, Tag Policies, AWS Resource Groups |
| **11** | [Global Infrastructure Architecture](11-global-infrastructure.md) | Physical and logical layout of AWS Regions, Availability Zones, Local Zones, and Edge | Fault domains, AZ latency (<1-2ms), Direct Connect, CloudFront edge PoPs, Wavelength |
| **12** | [Pricing Calculators, Budgets, and Cost Anomaly Detection](12-pricing-calculators-and-budgets.md) | Pre-deployment sizing, post-deployment FinOps, budget actions, and ML anomaly detection | AWS Pricing Calculator, AWS Budgets, Cost Explorer, CUR, automated spending caps |

---

## Architectural Principles Enforced

1. **Identity as the Primary Perimeter**:
   - Never generate permanent static IAM access keys (`AKIA...`) for developers.
   - Enforce AWS IAM Identity Center with federated MFA and short-lived credentials (`aws sso login`).
2. **Deterministic Security Policies**:
   - Deny by default. Every action must be explicitly granted with resource ARNs and condition blocks (`aws:PrincipalOrgID`, `aws:SecureTransport`).
   - Use SCPs at the organization root to prevent disabling CloudTrail, disallow unapproved regions, and block root account usage.
3. **Defense in Depth**:
   - Enforce IMDSv2 across all EC2 workloads (`HttpTokens=required`, `HttpPutResponseHopLimit=1`) to eliminate SSRF token-theft vectors.
4. **Continuous FinOps Awareness**:
   - Always pair baseline static budgets with AWS Cost Anomaly Detection to catch unexpected spend without drowning in false alarms.
   - Enforce mandatory tag policies (`Environment`, `Owner`, `Project`) from day one.

---

## Recommended Learning Flow

```
[01. What is AWS] ---> [02. Shared Responsibility & Well-Architected]
                                  |
                                  v
[04. Organizations & SCPs] <--- [03. Accounts, Regions & Billing]
         |
         v
[05. IAM Fundamentals] -------> [06. Policy Evaluation Logic]
         |                                  |
         v                                  v
[07. IAM Identity Center]       [09. SDKs & Credential Chain]
         |                                  ^
         +-----------------+                |
                           v                |
                  [08. AWS CLI v2] ---------+
                           |
                           v
        [10. Console, CloudShell & Tagging]
                           |
                           v
        [11. Global Infrastructure Topology]
                           |
                           v
        [12. Pricing Calculators & Budgets]
```

Proceed to **[Module 02: Compute & Containers](../02-compute/README.md)** once you have mastered foundational governance!
