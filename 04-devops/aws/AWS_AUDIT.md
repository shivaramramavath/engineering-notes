# AWS Track Baseline Audit Report

| Audit Metric | Current Track Details |
|---|---|
| **Track Root** | `04-devops/aws/` |
| **Audit Date** | November 25, 2026 |
| **Existing Notes Count** | 25 concept notes + 2 reference files (`reference/cheatsheet.md`, `reference/interview.md`) |
| **Root README Status** | Empty stub (0 bytes) |
| **Module READMEs Status** | 0 module `README.md` files present |
| **Template Conformance** | 0% (Existing notes follow informal 4–6 section layouts; 0 notes comply with 13-section schema) |
| **Target Curriculum Gap** | 76 notes missing (Target: 101 notes across 10 modules + 11 READMEs) |

---

## 1. Inventory of Existing Files & Conformance Analysis

| File Path | Lines | Current Sections | Conformance Status | Critical Deficiencies Identified |
|---|:---:|---|:---:|---|
| `README.md` | 0 | None (Empty) | Stub | 0 bytes; missing track navigation, architecture map, prerequisites |
| `01-foundations/01-what-is-aws.md` | 164 | 5 informal | Partial | Missing metadata, internals diagram, 5 practical examples, interview Q&A |
| `01-foundations/02-accounts-regions-and-billing.md` | 170 | 6 informal | Partial | Missing metadata, Organizations/SCP depth, FinOps formulas, exercises |
| `01-foundations/03-iam.md` | 185 | 6 informal | Partial | Lacks IAM Identity Center, policy evaluation engine flowchart, 13 sections |
| `01-foundations/04-aws-cli.md` | 180 | 5 informal | Partial | Missing AWS CLI v2 SSO login, JMESPath query recipes, error handling |
| `02-compute/01-ec2.md` | 215 | 6 informal | Partial | Missing IMDSv2 enforcement, Nitro hypervisor internals, Graviton comparison |
| `02-compute/02-lambda.md` | 230 | 6 informal | Partial | Lacks SnapStart, execution environment lifecycle diagram, provisioned concurrency |
| `02-compute/03-ecs-fargate.md` | 225 | 6 informal | Partial | Missing Service Connect, task role vs execution role architecture, 5 examples |
| `03-storage-and-databases/01-s3.md` | 240 | 6 informal | Partial | Missing S3 Express One Zone, Block Public Access defaults, bucket policies |
| `03-storage-and-databases/02-dynamodb.md` | 235 | 6 informal | Partial | Missing single-table design modeling, partition limits, streams integration |
| `03-storage-and-databases/03-rds-and-aurora.md` | 245 | 6 informal | Partial | Missing Aurora Serverless v2 internals, storage replication diagram, proxying |
| `04-networking/01-vpc.md` | 220 | 5 informal | Partial | Missing 3-tier multi-AZ subnet layout, CIDR allocation math, NAT Gateway traps |
| `04-networking/02-route53-and-cloudfront.md` | 235 | 6 informal | Partial | Missing DNS routing policies diagram, OAC vs OAI, CloudFront Functions |
| `04-networking/03-load-balancing-and-api-gateway.md` | 250 | 6 informal | Partial | Lacks ALB vs NLB deep dive, HTTP vs REST API Gateway performance metrics |
| `05-app-services/01-sqs.md` | 210 | 5 informal | Partial | Missing FIFO deduplication IDs, long polling sockets, DLQ redrive policies |
| `05-app-services/02-sns-and-eventbridge.md` | 225 | 6 informal | Partial | Lacks EventBridge Pipes, content filtering rules, bus-to-bus routing |
| `05-app-services/03-ses-email.md` | 190 | 5 informal | Partial | Missing DKIM/SPF DNS records, sandbox exit procedures, bounce webhooks |
| `05-app-services/04-cognito.md` | 215 | 5 informal | Partial | Missing User Pool vs Identity Pool token exchange diagram, Hosted UI |
| `05-app-services/05-bedrock.md` | 260 | 6 informal | Partial | Outdated model references; missing Claude 3.5 Sonnet, Nova, Guardrails, RAG |
| `06-operations/01-infrastructure-as-code.md` | 240 | 6 informal | Partial | High-level overview; missing concrete Terraform vs CDK vs CloudFormation |
| `06-operations/02-ci-cd-to-aws.md` | 230 | 5 informal | Partial | Missing GitHub Actions OIDC trust policy JSON, ECS deployment actions |
| `06-operations/03-security-and-secrets.md` | 255 | 6 informal | Partial | Lacks KMS key policies vs IAM policies comparison, Secrets Manager rotation |
| `06-operations/04-observability.md` | 245 | 6 informal | Partial | Missing CloudWatch Metric Math, Insights query syntax, X-Ray OTel daemon |
| `06-operations/05-reliability-and-cost.md` | 260 | 6 informal | Partial | Missing RTO/RPO disaster recovery matrix, Compute Savings Plans calculations |
| `reference/cheatsheet.md` | 338 | Tables | Needs Rewrite | Unnumbered directory; needs splitting into dedicated `10-cheatsheets/` notes |
| `reference/interview.md` | 372 | Q&A list | Needs Rewrite | Unnumbered directory; needs splitting into structured `09-interview/` notes |

---

## 2. Accuracy & Architectural Findings

1. **Security Vulnerability - IMDSv1**: Existing EC2 notes do not mandate IMDSv2. Modern AWS standards require `HttpTokens=required` to prevent SSRF-based metadata and IAM credential exfiltration.
2. **Outdated Bedrock Model Catalog**: `05-bedrock.md` lists legacy model generations and lacks coverage of current Anthropic Claude 3.5 Sonnet, Amazon Nova (Micro, Lite, Pro), Bedrock Guardrails, and Knowledge Bases.
3. **Public IPv4 Charges Omission**: Networking notes do not account for the $0.005/hour ($3.60/month per IP) public IPv4 surcharge instituted in Feb 2024, leading to unexpected architectural costs.
4. **Static IAM Key Pitfalls**: CI/CD and CLI documentation needs explicit emphasis on eliminating long-lived `AWS_ACCESS_KEY_ID` in favor of IAM Roles Anywhere, IAM Identity Center SSO, and GitHub Actions OIDC.
5. **Missing S3 Security Invariants**: S3 notes do not highlight that S3 Block Public Access is enabled by default, default encryption is mandatory, and ACLs are disabled (`BucketOwnerEnforced`) on all new buckets.

---

## 3. Curriculum Gap Analysis vs Production Requirements

The current 25 notes offer only surface-level coverage of AWS services. To match the depth of the Java and Spring Boot tracks, the following major areas must be added:
1. **Foundations**: Organizations, SCPs, Control Tower, IAM Identity Center (SSO), SDK credential provider chain, Global Infrastructure partitions, and FinOps budgeting.
2. **Compute**: EC2 Auto Scaling Groups, Launch Templates, Lambda SnapStart & cold start optimization, ECS deep dive with Service Connect, EKS Kubernetes fundamentals, App Runner, Batch, and Graviton4.
3. **Storage & Databases**: S3 Deep Dive (security, replication, lifecycle, S3 Express One Zone), EBS/EFS/FSx, DynamoDB single-table design, RDS Proxy & Aurora Serverless v2, ElastiCache Redis, OpenSearch, and Disaster Recovery.
4. **Networking**: VPC Deep Dive (CIDR math, private endpoints, NAT traps), Security Groups vs NACLs, Transit Gateway, Route 53 advanced routing policies, WAF & Shield.
5. **App Services**: MSK Kafka, Kinesis Data Streams, Step Functions state machines, AppSync GraphQL, and Event-Driven Architecture patterns.
6. **Operations & IaC**: Dedicated deep-dive notes for CloudFormation, AWS CDK, and Terraform on AWS; AWS Security Services (GuardDuty, Security Hub, Inspector); FinOps optimization.
7. **Architecture Patterns**: 10 enterprise system design patterns (Three-tier web app, Serverless API, Event-Driven, Multi-Region DR, Data Lake, Zero-Trust Landing Zone, 6 Rs Migration).
8. **Reference Projects**: 8 complete production projects implemented via markdown (Static site on S3/CloudFront, Serverless REST API, ECS Fargate Stack, Three-Tier VPC, Event-driven pipeline, GitHub Actions OIDC CI/CD, Terraform Landing Zone, Observability stack).
9. **Interview Modules**: 9 categorized interview preparation notes replacing the raw reference file.
10. **Cheatsheets**: 10 high-density syntax and command cards replacing the raw reference file.
