# Module 07: Enterprise Architecture Patterns

Welcome to the **AWS Enterprise Architecture Patterns** module. This curriculum synthesizes foundational cloud compute, networking, storage, database, and messaging building blocks into production-grade architectural blueprints. Covering the Well-Architected Framework, multi-tier web platforms, serverless APIs, asynchronous event-driven pipelines, container evaluation frameworks, multi-region disaster recovery, modern data lakes, zero-trust landing zones, large-scale cloud migrations, and service decision matrices, this module equips architects and senior engineers to design resilient, scalable, and cost-optimized distributed systems on AWS.

---

## Module Overview

Modern enterprise systems must satisfy stringent non-functional requirements: single-digit millisecond latency, five-nines availability (99.999%), RPO/RTO measured in seconds, least-privilege zero-trust security perimeters, and FinOps unit cost accountability. This module bridges theory and enterprise reality by presenting holistic reference architectures, declarative infrastructure code, decision flowcharts, failure mode analyses, and production battle-scars.

---

## Detailed Curriculum Roadmap

| # | Note | Description | Key Focus Areas |
|:---|:---|:---|:---|
| **01** | [AWS Well-Architected Framework in Practice](01-well-architected-pillars-in-practice.md) | The 6 pillars of cloud architecture, HRI risk assessments, and production governance | Operational Excellence, Security, Reliability, Performance, Cost, Sustainability, WA Tool |
| **02** | [Three-Tier Web Application Architecture](02-three-tier-web-app.md) | Enterprise decoupled web architectures across Multi-AZ VPC network topologies | Public ALB, Private ECS/EC2 app tier, Isolated Multi-AZ Aurora DB, NAT Gateways |
| **03** | [Serverless API Architecture Pattern](03-serverless-api-pattern.md) | High-speed serverless endpoints with zero server management and elastic scale | API Gateway HTTP/REST, Lambda ARM64, DynamoDB Single-Table, Cognito JWT validation |
| **04** | [Event-Driven Architecture on AWS](04-event-driven-architecture.md) | Asynchronous messaging, event routing, fan-out, and saga distributed transactions | EventBridge buses, SNS fan-out, SQS DLQ handling, Transactional Outbox, Step Functions |
| **05** | [Containers on AWS Decision Guide](05-containers-on-aws-decision-guide.md) | Technical evaluation matrix for container compute runtimes on AWS | App Runner vs Lambda Containers vs ECS Fargate vs ECS EC2 vs Amazon EKS (Karpenter) |
| **06** | [Multi-Region Architecture & Disaster Recovery](06-multi-region-and-dr-strategies.md) | RTO/RPO evaluation and the 4 DR tiers from backup to active-active | Backup/Restore, Pilot Light, Warm Standby, Active-Active, Aurora Global DB, Global Tables |
| **07** | [Data Lake & Modern Data Architecture](07-data-lake-pattern.md) | Lakehouse design, columnar storage, and fine-grained data lake governance | S3 Medallion layers (Bronze/Silver/Gold), Parquet, Glue Catalog, Athena, Lake Formation |
| **08** | [Zero Trust & Landing Zone Governance](08-zero-trust-and-landing-zone.md) | Identity-centric perimeters, VPN-less corporate ingress, and organizational guardrails | AWS Verified Access (Cedar), PrivateLink micro-segmentation, Control Tower, SCPs, mTLS |
| **09** | [Cloud Migration Strategies & The 7 Rs](09-migration-strategies.md) | Frameworks and continuous block/data replication for enterprise migration | Rehost, Replatform, Repurchase, Refactor, Relocate, Retain, Retire, AWS MGN, AWS DMS |
| **10** | [Choosing the Right AWS Service](10-choosing-the-right-service.md) | Architectural decision trees across compute, storage, databases, and messaging | Flowcharts, selection matrices, trade-offs, break-even cost models, and anti-patterns |

---

## Architectural Principles Enforced

1. **Design for Failure (Assume Everything Fails All the Time)**:
   - Eliminate single points of failure across every layer. Spread compute across multiple Availability Zones, implement circuit breakers and retries with exponential backoff and jitter, and automate disaster recovery failovers.
2. **Decouple Components Asynchronously**:
   - Never allow synchronous web requests to perform heavy background I/O or multi-step database mutations directly. Buffer workloads using Amazon SQS queues and EventBridge buses.
3. **Identity as the Security Perimeter (Zero Trust)**:
   - Abandon flat network trust assumptions. Enforce cryptographic identity verification (mTLS, SigV4, OIDC) on every internal API interaction, and govern cross-account environments using immutable Service Control Policies.
4. **Choose Managed Serverless Abstractions Before Infrastructure**:
   - Maximize developer velocity and eliminate undifferentiated heavy lifting by defaulting to managed services (Lambda, Fargate, DynamoDB, S3) before taking on the operational burden of managing EC2 hosts or self-hosted clusters.

---

## Recommended Learning Flow

```
[01. Well-Architected Pillars] -------> [02. Three-Tier Web App] -------> [03. Serverless API]
                                                |                                 |
                                                v                                 v
                                    [04. Event-Driven Architecture] <----+        |
                                                |                        |        |
        +---------------------------------------+                        |        |
        |                                                                |        |
        v                                                                |        |
[05. Containers Decision Guide] ---> [06. Multi-Region & DR] ------------+--------+
        |                                       |
        v                                       v
[07. Modern Data Lake Pattern] ----> [08. Zero Trust & Landing Zone]
                                                |
                                                v
                                    [09. Cloud Migration (7 Rs)]
                                                |
                                                v
                                    [10. Choosing the Right Service]
```

---

## Module Pre-requisites & Cross-References

- **Prerequisites**:
  - Foundational knowledge of AWS IAM, accounts, and regions from [01-foundations](../01-foundations/).
  - Compute primitives (EC2, Lambda, ECS, EKS) from [02-compute](../02-compute/).
  - Storage & database fundamentals (S3, DynamoDB, RDS/Aurora) from [03-storage-and-databases](../03-storage-and-databases/).
  - Enterprise networking (VPC, ALBs, Route 53, PrivateLink) from [04-networking](../04-networking/).
  - Messaging fundamentals (SQS, SNS, EventBridge, Step Functions) from [05-app-services](../05-app-services/).
  - Operations and IaC (Terraform, CloudFormation, Observability, Multi-Account) from [06-operations](../06-operations/).
- **Downstream Modules**:
  - Hands-on end-to-end reference projects implementing these patterns in [08-projects](../08-projects/).
  - Scenario and system design technical interview questions in [09-interview](../09-interview/).
  - High-density reference cheatsheets in [10-cheatsheets](../10-cheatsheets/).
