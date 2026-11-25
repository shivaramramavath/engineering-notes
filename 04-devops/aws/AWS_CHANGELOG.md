# AWS Track Changelog & Audit Record

All notable changes, refactorings, renames, and batch completions for `04-devops/aws/` are documented in this file.

---

## [1.0.0] - 2026-11-25 - Complete 100% Curriculum Delivery

### Added
- **Module 01: Foundations & Governance** (12 notes + `README.md`): Comprehensive coverage of global infrastructure, IAM evaluation logic, billing alarms, least privilege auditing, CloudTrail, and pricing.
- **Module 02: Compute & Containers** (10 notes + `README.md`): Nitro system, EC2 lifecycle, Spot interruption handling, ARM64 Graviton, Lambda cold starts, ECS Fargate, EKS, and App Runner.
- **Module 03: Storage & Databases** (11 notes + `README.md`): S3 strong consistency, storage classes, Object Lock, EBS gp3/io2, DynamoDB Single-Table, Aurora distributed storage, ElastiCache, OpenSearch, Redshift, and AWS Backup.
- **Module 04: Networking & Content Delivery** (10 notes + `README.md`): VPC CIDR math, public/private/isolated subnets, Transit Gateway, PrivateLink, Route 53 routing policies, CloudFront OAC, ALB/NLB, and WAF/Shield.
- **Module 05: Application Integration & Messaging** (9 notes + `README.md`): SQS Standard & FIFO, SNS pub/sub, EventBridge event routing, Step Functions sagas, Kinesis & MSK streaming, Cognito JWTs, AppSync, and Bedrock.
- **Module 06: Cloud Operations, Security & IaC** (12 notes + `README.md`): Declarative IaC, CloudFormation, AWS CDK v2, Terraform & OpenTofu, GitHub Actions OIDC, Secrets Manager, Threat detection (GuardDuty), FinOps, and Landing Zones.
- **Module 07: Enterprise Architecture Patterns** (10 notes + `README.md`): Well-Architected Framework, Three-Tier Web App, Serverless REST API, Event-Driven Architecture, Container Runtimes, Multi-Region Active-Active DR, Modern Data Lakehouse, Zero Trust, and Cloud Migration (7 Rs).
- **Module 08: Production Reference Projects** (8 complete projects + `README.md`):
  1. Static Site on S3 + CloudFront with OAC & WAF
  2. Serverless REST API with HTTP API, Lambda ARM64, and DynamoDB
  3. Microservice on ECS Fargate with ALB and Auto Scaling
  4. Enterprise Three-Tier VPC with Aurora PostgreSQL
  5. Event-Driven Order Pipeline with EventBridge, SQS, and DLQ
  6. Secure CI/CD with GitHub Actions and OIDC (Zero Static Keys)
  7. Terraform Landing Zone with AWS Organizations and SCPs
  8. Enterprise Observability Stack on CloudWatch with Composite Alarms
- **Module 09: Technical Interview Preparation** (9 notes + `README.md`): Comprehensive technical interview preparation covering Fundamentals, Security, Networking, Compute, Storage, Serverless, System Design Scenarios, FinOps/Reliability, and Behavioral Leadership.
- **Module 10: Quick Reference Cheatsheets** (10 notes + `README.md`): High-density cards for CLI commands, IAM policy snippets, VPC CIDRs, S3/DynamoDB math, Lambda/API Gateway, ECS/EKS, CloudWatch queries, Terraform/CDK, Service Chooser, and Limits/Quotas.
- **Track Root Portal**: `04-devops/aws/README.md` unifying all 10 modules, learning roadmaps, and cross-track references.

### Removed
- Deprecated legacy `04-devops/aws/reference/` directory (`cheatsheet.md` and `interview.md`), whose contents were fully absorbed and expanded into Module 09 and Module 10.

---

## [0.1.0] - 2026-11-25 - Track Architecture & Governance Inception

### Added
- Phase 0 Technical Facts Sheet (`AWS_FACTS.md`).
- Phase 1 Baseline Audit Report (`AWS_AUDIT.md`).
- Phase 2 Curriculum Architecture & Completion Plan (`AWS_COMPLETION_PLAN.md`).
- Resumable Progress Tracker (`AWS_PROGRESS.md`).
- Architecture Decision Records (`AWS_DECISIONS.md`).
- Baseline Changelog (`AWS_CHANGELOG.md`).
