# Module 10: Quick Reference Cheatsheets

Welcome to the **AWS Quick Reference Cheatsheets** module. This curriculum provides a high-density, production-tested reference library designed for rapid lookup during on-call incidents, architecture reviews, day-to-day operations, and technical interviews.

---

## Module Overview

When a production system is degrading, engineers do not have time to read 40-page whitepapers. They need immediate access to exact AWS CLI commands, JMESPath queries, subnet calculation tables, IAM policy snippets, CloudWatch Insights queries, and hard platform limits. This module distills the entirety of the AWS cloud platform into 10 purpose-built reference cards.

---

## Detailed Cheatsheet Catalog

| # | Cheatsheet | Description & Key Focus Areas |
|:---|:---|:---|
| **01** | [AWS CLI Commands](01-cli-commands.md) | STS identity checks, JMESPath `--query` expressions, SSM shell, S3 sync, and Lambda/ECS triage |
| **02** | [IAM Policy Snippets](02-iam-policy-snippets.md) | Trust policies (Lambda, EC2, GitHub OIDC), S3 HTTPS/OAC policies, Organizations SCPs |
| **03** | [VPC CIDR Math & Routing](03-vpc-cidr-and-routing.md) | Subnet prefix math (/16 to /28), AWS 5 reserved IPs, 3-tier VPC blueprint, SG vs NACL matrix |
| **04** | [Amazon S3 & DynamoDB](04-s3-and-dynamodb.md) | S3 lifecycle syntax, DynamoDB RCU/WCU calculation formulas, single-table key naming, Boto3 queries |
| **05** | [AWS Lambda & API Gateway](05-lambda-and-api-gateway.md) | Memory-to-vCPU allocation table, REPORT line format, Payload format 2.0 JSON, error code mapping |
| **06** | [ECS & EKS Container Quick Ref](06-ecs-eks-quick-ref.md) | Fargate task definitions, container exit code guide (137 OOM), IRSA annotations, Karpenter NodePool |
| **07** | [CloudWatch Logs Insights Queries](07-cloudwatch-queries.md) | 10 production recipes: Top exceptions, cold start duration, VPC flow top talkers, CloudTrail audits |
| **08** | [Terraform & CDK Snippets](08-terraform-and-cdk-snippets.md) | S3 remote state locking, provider default tags, moved blocks, CDK L2 constructs, cdk-nag |
| **09** | [Service Chooser & Decision Matrix](09-service-chooser.md) | "Which service for what" master table, decision shortcuts, symptom-to-cause diagnostic table |
| **10** | [AWS Limits, Quotas & Defaults](10-limits-and-defaults.md) | Hard vs soft platform limits across compute, storage, networking, messaging, and databases |

---

## Key Operational Rules to Remember

1. **Default Deny; Explicit Deny Wins**: In IAM, an explicit deny in any policy overrides all allowances.
2. **Multi-AZ $\ne$ Multi-Region $\ne$ Backup**: Multi-AZ protects against local hardware failure; backups protect against corruption; multi-region protects against regional disasters.
3. **At-Least-Once Delivery Mandates Idempotent Consumers**: Build handlers to safely ignore or deduplicate duplicate messages.
4. **Always Attach a Dead-Letter Queue (DLQ)**: Prevent poison pills from looping infinitely in worker queues.
5. **Private Subnets for Data Tiers**: Never expose database instances to the public internet; reference Security Groups, not IP CIDRs.
6. **Budgets Alert; They Do Not Cap**: Protect cloud accounts with real-time budget alarms, cost anomaly detection, and restrictive SCPs.
