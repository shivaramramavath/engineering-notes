# Module 09: Technical Interview Preparation

Welcome to the **AWS Technical Interview Preparation** module. This curriculum synthesizes enterprise cloud engineering concepts, operational battle-scars, distributed system design scenarios, and behavioral leadership frameworks into high-signal preparation material for cloud architect, DevOps, and SRE technical interviews.

---

## Module Overview

Modern technical interviews at top cloud-native companies (Amazon, Netflix, Stripe, Datadog) do not test rote trivia; they evaluate **trade-offs, failure modes, diagnostic instincts, and operational maturity**. A candidate who simply states "SQS is a queue" will fail; a candidate who explains:
> *"Standard SQS guarantees at-least-once delivery with best-effort ordering, meaning consumers must be designed to be strictly idempotent, and the SQS visibility timeout must be configured to at least 6x the consumer processing time to prevent duplicate concurrent processing during cold starts."*

will stand out immediately. This module equips you to answer with depth, precision, and architectural reasoning.

---

## Detailed Interview Curriculum

| # | Note | Topic & Core Focus |
|:---|:---|:---|
| **01** | [AWS Fundamentals & Cloud Concepts](01-aws-fundamentals-qa.md) | Regions vs AZs vs Edge locations, Shared Responsibility Model, Account governance, Billing surprises |
| **02** | [IAM, Cryptography & Security Engineering](02-iam-and-security-qa.md) | IAM evaluation algorithm, SCPs vs Boundaries, Envelope Encryption, KMS, GuardDuty, WAF, IMDSv2 |
| **03** | [Networking, VPC & Content Delivery](03-networking-qa.md) | Subnet CIDR calculation, Security Groups vs NACLs, Transit Gateway, PrivateLink, Route 53, CDN |
| **04** | [Compute, Containers & Serverless](04-compute-and-containers-qa.md) | EC2 lifecycle, Spot interruption handling, SSM vs SSH, Lambda cold starts, ECS Fargate vs EKS, Karpenter |
| **05** | [Storage & Databases](05-storage-and-databases-qa.md) | S3 strong consistency, storage classes, Object Lock, EBS gp3 vs io2, DynamoDB GSIs/hot partitions, Aurora |
| **06** | [Serverless, Messaging & Event-Driven](06-serverless-and-messaging-qa.md) | SQS Standard vs FIFO, Long polling, EventBridge vs SNS, Step Functions Sagas, API Gateway HTTP vs REST |
| **07** | [System Design & Architecture Scenarios](07-architecture-and-scenario-questions.md) | 100k req/s API, Multi-region active-active DR, 10TB/day lakehouse ingestion, Ticketing flash-sales |
| **08** | [FinOps, Cost Optimization & Reliability](08-cost-and-reliability-qa.md) | Cost allocation tags, Compute Savings Plans, Compute Optimizer, Well-Architected HRIs, RTO/RPO math |
| **09** | [Behavioral, Leadership & Post-Mortems](09-behavioural-and-project-discussion.md) | Amazon Leadership Principles, STAR method, blameless post-mortems, prioritizing technical debt |

---

## The Candidate's 4-Step Answering Framework

When answering technical interview questions, use this proven communication framework:

1. **Lead with the Direct, One-Sentence Answer**:
   - Answer the question directly without preamble. Establish immediate technical credibility.
2. **State the Trade-Offs & Edge Cases ("X, unless Y, because Z")**:
   - Explain under what conditions the default choice breaks down.
3. **Name the Failure Modes & What You'd Check First**:
   - Show production empathy: mention exit codes, CloudWatch metrics, or packet drops.
4. **Quantify When Possible**:
   - Cite latency figures (< 10ms), cost ratios (\$1.00/M vs \$3.50/M), or SLAs (99.99%).
