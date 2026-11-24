# AWS Curriculum Architecture Decision Records (ADRs)

This document records the foundational architectural decisions, conventions, and design trade-offs governing the `04-devops/aws/` track.

---

## ADR-001: Expansion to 10 Modules with Dedicated Reference Tracks

### Context
The original track contained 6 modules with thin coverage (~3 notes per module) and an unnumbered `reference/` directory containing two large dumps (`cheatsheet.md` and `interview.md`). This prevented in-depth study of enterprise topics like EKS, Terraform, CDK, CloudWatch Insights, DynamoDB single-table design, and modern Bedrock generative AI.

### Decision
Expand the curriculum to 10 structured modules:
1. `01-foundations` (12 notes)
2. `02-compute` (10 notes)
3. `03-storage-and-databases` (11 notes)
4. `04-networking` (10 notes)
5. `05-app-services` (9 notes)
6. `06-operations` (12 notes)
7. `07-architecture-patterns` (10 notes)
8. `08-projects` (8 reference projects)
9. `09-interview` (9 categorized technical interview notes)
10. `10-cheatsheets` (10 high-density reference cards)

### Consequences
Provides comprehensive end-to-end coverage totaling 101 production notes while cleanly organizing interview prep and cheatsheets as numbered track modules.

---

## ADR-002: Reorganization and Splitting of Reference Notes

### Context
`reference/interview.md` and `reference/cheatsheet.md` were unnumbered monolithic files. `CONVENTIONS.md` explicitly forbids unnumbered reference folders and requires numbered modules.

### Decision
- Split `reference/interview.md` into 9 focused interview notes in `09-interview/` covering fundamentals, IAM, networking, compute, storage/DB, serverless, scenario design, FinOps, and behavioral questions.
- Split `reference/cheatsheet.md` into 10 focused cards in `10-cheatsheets/` covering CLI, IAM policies, VPC CIDR, S3/DynamoDB, Lambda/API Gateway, ECS/EKS, CloudWatch, Terraform/CDK, Service Chooser, and Limits/Defaults.
- Delete the old `reference/` directory once all content is migrated and expanded.

### Consequences
Eliminates unnumbered directories while keeping every piece of valuable content from the old files, expanded into the standard 13-section format.

---

## ADR-003: Strict Enforcement of IMDSv2 and IAM Zero-Static-Key Standard

### Context
Legacy AWS tutorials frequently demonstrate assigning long-lived IAM access keys (`AKIA...`) or querying EC2 instance metadata via unauthenticated IMDSv1 (`http://169.254.169.254/latest/meta-data/`), exposing systems to credential theft via Server-Side Request Forgery (SSRF).

### Decision
- Mandate IMDSv2 (`HttpTokens=required`, `HttpPutResponseHopLimit=1`) in all EC2 configurations.
- Require IAM Roles with short-lived STS credentials (instance profiles, ECS task roles, EKS Pod Identity, Lambda execution roles) and GitHub Actions OIDC across all examples.
- Forbid hardcoded access keys in code, configuration, or CI scripts.

### Consequences
Guarantees modern enterprise security compliance across all code samples.
