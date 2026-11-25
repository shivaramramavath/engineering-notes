# AWS Enterprise Cloud Engineering Curriculum & Reference Guide

> A comprehensive, production-grade learning track and engineering reference manual for designing, provisioning, securing, and operating resilient enterprise systems on Amazon Web Services (AWS).

---

## Curriculum Overview

This track is an exhaustive, production-tested curriculum for cloud architects, DevOps engineers, site reliability engineers (SREs), and systems engineers. Spanning 10 core modules, 101 detailed concept and reference notes, 8 complete production projects, 9 categorized interview preparation notes, and 10 high-density cheatsheets, this repository serves as both a progressive mastery pathway and an on-call diagnostic reference.

Every note adheres to strict production invariants:
- **Zero Hallucinations & Zero Placeholders**: Verified AWS configurations, valid Terraform HCL (v1.5+), and AWS SDK v3 code.
- **Zero Static Credentials**: Enforcement of IAM Roles, AWS STS temporary sessions, and GitHub Actions OIDC federation.
- **Standardized Depth**: Concept notes implement all 13 standard architectural sections (including ASCII topologies, comparative matrices, failure analysis tables, interview Q&As, and progressive exercises).
- **Hardened Security & Modern Standards**: Default enforcement of IMDSv2, S3 Block Public Access, CloudFront OAC, KMS encryption, and multi-account landing zone governance.

---

## Master Module Roadmap

```
01-foundations ──────────> 02-compute ──────────> 03-storage-and-databases
                                                          │
┌─────────────────────────────────────────────────────────┘
▼
04-networking ───────────> 05-app-services ─────> 06-operations
                                                          │
┌─────────────────────────────────────────────────────────┘
▼
07-architecture-patterns ─> 08-projects ─────────> 09-interview ──> 10-cheatsheets
```

---

## Module Directory & Curriculum Matrix

| Module | Notes | Level | Core Focus Areas | Module Link |
|:---|:---:|:---:|:---|:---|
| **01-foundations** | 12 | Foundational | Global Infrastructure, Shared Responsibility, Account Setup, Billing & Budgets, IAM Fundamentals, Policies, Roles, CLI, CloudShell, CloudTrail, Pricing, Cost Explorer | [Explore Module 01](01-foundations/README.md) |
| **02-compute** | 10 | Intermediate | EC2 Deep Dive, AMIs, Instance Types, Auto Scaling, Systems Manager (SSM), Lambda, ECS & Fargate, EKS, App Runner, Compute Decision Matrix | [Explore Module 02](02-compute/README.md) |
| **03-storage-and-databases**| 11 | Intermediate | S3 Storage & Security, DynamoDB Single-Table, RDS & Aurora, ElastiCache (Redis), EBS Volumes, EFS Shared Storage, OpenSearch, Redshift, Backup, Migration (DMS), DB Chooser | [Explore Module 03](03-storage-and-databases/README.md) |
| **04-networking** | 10 | Advanced | VPC Deep Dive, Subnetting & CIDR Math, Security Groups & NACLs, Peering, Transit Gateway, PrivateLink, Route 53, CloudFront, ALB/NLB, API Gateway, Direct Connect, WAF & Shield | [Explore Module 04](04-networking/README.md) |
| **05-app-services** | 9 | Advanced | SQS Standard & FIFO, SNS Pub/Sub, EventBridge Event Buses, Kinesis & MSK Streaming, Step Functions Sagas, SES Email, Cognito User Pools, AppSync GraphQL, Bedrock GenAI | [Explore Module 05](05-app-services/README.md) |
| **06-operations** | 12 | Senior | Infrastructure as Code (IaC), CloudFormation, AWS CDK v2, Terraform & OpenTofu, CI/CD to AWS (OIDC), Secrets Manager & KMS, Security Services, Observability, FinOps, Landing Zones | [Explore Module 06](06-operations/README.md) |
| **07-architecture-patterns**| 10 | Staff | Well-Architected Pillars, Three-Tier Web Apps, Serverless REST APIs, Event-Driven Architecture, Container Runtimes, Multi-Region & DR, Modern Data Lake, Zero Trust, Cloud Migration (7 Rs) | [Explore Module 07](07-architecture-patterns/README.md) |
| **08-projects** | 8 | Production | Complete reference projects: Static S3/CloudFront with OAC, Serverless REST API, ECS Fargate with ALB, Three-Tier VPC with Aurora, Order Pipeline with DLQ, GitHub OIDC, Landing Zone, Observability | [Explore Module 08](08-projects/README.md) |
| **09-interview** | 9 | Career | Categorized technical Q&A: Fundamentals, IAM & Security, Networking, Compute, Storage & Databases, Serverless, System Design Scenarios, Cost & Reliability, Behavioral & Post-Mortems | [Explore Module 09](09-interview/README.md) |
| **10-cheatsheets** | 10 | Reference | High-density reference cards: CLI commands, IAM policy snippets, VPC CIDRs & routing, S3 & DynamoDB math, Lambda & API Gateway, ECS/EKS, CloudWatch queries, Terraform/CDK, Chooser, Limits | [Explore Module 10](10-cheatsheets/README.md) |

**Total Curriculum Scope**: **101 Comprehensive Technical Notes + 10 Module Overview Portals**.

---

## Production Reference Projects (Module 08)

The curriculum includes 8 production-grade reference projects complete with declarative Terraform HCL, verification test harnesses, and disaster recovery runbooks:

1. [01. Production Static Site on S3 + CloudFront with OAC](08-projects/01-static-site-on-s3-cloudfront.md): Private S3 origin, OAC SigV4, AWS WAF rate-limiting, and security response headers.
2. [02. Production Serverless REST API](08-projects/02-serverless-rest-api.md): API Gateway HTTP API v2, Node.js 20 ARM64 Lambda, DynamoDB Single-Table with PITR.
3. [03. Containerised App on ECS Fargate with ALB](08-projects/03-containerised-app-on-ecs-fargate-with-alb.md): Zero public IP container tasks, Target Tracking Auto Scaling, and rolling deployments.
4. [04. Enterprise Three-Tier VPC with Aurora PostgreSQL](08-projects/04-three-tier-vpc-with-rds.md): Multi-AZ public, private, and isolated database subnets with Secrets Manager rotation.
5. [05. Event-Driven Order Processing Pipeline](08-projects/05-event-driven-order-pipeline.md): EventBridge routing, SQS backpressure buffer, Dead-Letter Queue (DLQ), and CloudWatch alarms.
6. [06. Secure CI/CD with GitHub Actions and OIDC](08-projects/06-secure-ci-cd-with-github-oidc.md): Zero-static-key deployment pipeline federated with GitHub's OIDC provider.
7. [07. Terraform Landing Zone Baseline](08-projects/07-terraform-landing-zone-basics.md): AWS Organizations hierarchy, Region-Deny SCPs, and immutable S3 Object Lock compliance logs.
8. [08. Enterprise Observability Stack on CloudWatch](08-projects/08-observability-stack-on-cloudwatch.md): Golden Signals dashboard, composite alarms, and saved CloudWatch Logs Insights queries.

---

## Cross-Track Knowledge Boundaries

To maintain focus and avoid redundant duplication across the engineering repository:
- **AWS SDK in Application Code (Node.js)**: See [03-backend/nodejs/20-aws-sdk](../../03-backend/nodejs/README.md) for SDK v3 clients, pagination, middleware, and mocking.
- **Docker & Container Packaging**: See [04-devops/docker](../docker/README.md) for Dockerfile optimization and multi-stage builds.
- **Linux Operating System Internals**: See [04-devops/linux](../linux/README.md) for kernel tuning, systemd, and disk I/O profiling.
- **Reverse Proxy & Web Serving**: See [04-devops/nginx](../nginx/README.md) for Nginx proxy pass, TLS termination, and caching.
- **System Design & Distributed Patterns**: See [07-system-design](../../07-system-design/README.md) for vendor-agnostic distributed algorithms (CAP theorem, consensus, consistent hashing).

---

## Core Architectural Invariants

All architectures documented in this track enforce four non-negotiable principles:

1. **Principle of Least Privilege (Zero Static Credentials)**:
   - Eliminate hardcoded AWS Access Keys. Use IAM Roles for internal compute, IAM Identity Center (SSO) for human operators, and OIDC federation for external CI/CD pipelines.
2. **Defense in Depth & Micro-Segmentation**:
   - Secure workloads across multiple concentric rings: Route 53 DNSSEC $\rightarrow$ CloudFront Edge TLS $\rightarrow$ AWS WAF $\rightarrow$ VPC Subnet isolation $\rightarrow$ Security Groups referencing SGs (not CIDRs) $\rightarrow$ IAM Policy evaluation.
3. **Design for Continuous Failure**:
   - Systems must tolerate server, data center, and regional failures. Spread compute across 2+ Availability Zones, implement circuit breakers and exponential backoff, and automate disaster recovery failover.
4. **Automate Everything in Declarative Code (IaC)**:
   - Prohibit manual changes in production AWS consoles ("ClickOps"). All resources must be declared in version-controlled Terraform, OpenTofu, or AWS CDK code.
