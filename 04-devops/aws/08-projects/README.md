# Module 08: Production Reference Projects

Welcome to the **AWS Production Reference Projects** module. This module contains 8 complete, production-grade, declarative reference projects implementing enterprise cloud architectures on AWS. Each project includes complete infrastructure-as-code (Terraform), architectural topology diagrams, automated verification test harnesses, security hardening analyses, and operational disaster recovery runbooks.

---

## Module Overview

Unlike fragmented code snippets or theoretical overviews, every project in this curriculum adheres to strict enterprise production standards:
- **Zero Static IAM Credentials**: Integration with OpenID Connect (OIDC) and IAM Roles.
- **Strict Network Isolation**: Chained Security Groups, private subnets, and isolated database layers without public IP exposure.
- **Declarative & Versioned**: Full, runnable Terraform code blocks that can be provisioned and destroyed deterministically.
- **Measurable Cost Models**: Realistic monthly cost breakdowns based on published AWS pricing.

---

## Detailed Project Catalog

| # | Project | Description | Architecture / Key Services | Complexity |
|:---|:---|:---|:---|:---|
| **01** | [Production Static Site Hosting with S3 and CloudFront OAC](01-static-site-on-s3-cloudfront.md) | Edge-accelerated, zero-server web hosting with private origin security and AWS WAF protection | S3, CloudFront OAC, AWS WAF, Security Headers | Intermediate |
| **02** | [Production Serverless REST API](02-serverless-rest-api.md) | Low-latency CRUD microservice with ARM64 compute, DynamoDB On-Demand, and X-Ray tracing | API Gateway HTTP API, Lambda ARM64, DynamoDB | Intermediate |
| **03** | [Containerised Application on ECS Fargate with ALB](03-containerised-app-on-ecs-fargate-with-alb.md) | Serverless microservice in private subnets with Target Tracking Auto Scaling and Layer 7 routing | Amazon ECS, AWS Fargate, ALB, Target Tracking | Advanced |
| **04** | [Enterprise Three-Tier VPC with Aurora PostgreSQL](04-three-tier-vpc-with-rds.md) | Multi-AZ network topology with public ingress, private app tier, and completely isolated database tier | VPC, Multi-AZ Subnets, Aurora PostgreSQL, Secrets Mgr | Advanced |
| **05** | [Event-Driven Order Processing Pipeline](05-event-driven-order-pipeline.md) | Asynchronous messaging pipeline with declarative JSON routing, SQS buffering, and DLQ alerting | EventBridge, SQS, Dead-Letter Queue, Lambda, Alarms | Intermediate |
| **06** | [Secure CI/CD with GitHub Actions and OIDC](06-secure-ci-cd-with-github-oidc.md) | Zero-static-key continuous deployment pipeline federated with GitHub's OIDC provider | IAM OIDC Provider, STS AssumeRoleWithWebIdentity | Intermediate |
| **07** | [Terraform Landing Zone Baseline](07-terraform-landing-zone-basics.md) | Multi-account governance foundation with Service Control Policies and immutable log archives | AWS Organizations, SCPs, S3 Object Lock (Compliance)| Advanced |
| **08** | [Enterprise Observability Stack on CloudWatch](08-observability-stack-on-cloudwatch.md) | Full-stack monitoring with Golden Signals dashboards, composite alarms, and Logs Insights | CloudWatch Dashboards, Metric Alarms, Composite Alarms | Intermediate |

---

## Standard Project Structure

Every project in this module implements the standardized 5-section enterprise reference template:

1. **Section 1: Project Topology & Specifications**:
   - Architectural summary, detailed ASCII topology diagram, bill of materials, and prerequisite services.
2. **Section 2: Declarative Code**:
   - Complete, verified, copy-pasteable declarative code blocks (Terraform HCL, Dockerfiles, Lambda handlers) with zero placeholders.
3. **Section 3: Verification & Automated Test Harness**:
   - Step-by-step deployment instructions, smoke testing commands (`curl`, AWS CLI), automated assertions, and teardown scripts.
4. **Section 4: Hardening, Security & Cost Analysis**:
   - Principle of Least Privilege verification, encryption mechanisms, network boundaries, and detailed monthly cost estimates.
5. **Section 5: Operational Runbook & Disaster Recovery**:
   - Common alerts and diagnostic triage runbooks, root cause remediation, and automated recovery procedures.

---

## Recommended Execution Order

```
[01. Static Site on S3/CloudFront] ----> [02. Serverless REST API]
                                                 |
                                                 v
[04. Three-Tier VPC with Aurora] <------ [03. ECS Fargate with ALB]
        |
        v
[05. Event-Driven Order Pipeline] -----> [06. Secure CI/CD with GitHub OIDC]
                                                 |
                                                 v
[08. Observability Stack on CloudWatch] < [07. Terraform Landing Zone Baseline]
```
