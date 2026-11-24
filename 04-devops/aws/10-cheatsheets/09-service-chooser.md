---
topic: "AWS Service Chooser & Diagnostic Decision Matrix Cheatsheet"
module: "10-cheatsheets"
track: "aws"
difficulty: "reference"
last-tested: "2026-11"
tags:
  - "cheatsheet"
  - "service-chooser"
  - "symptoms"
  - "troubleshooting"
  - "decision-matrix"
---

# AWS Service Chooser & Diagnostic Decision Matrix Cheatsheet

A high-density reference card mapping business problems to the right AWS services, diagnostic symptoms to root causes, and fundamental architectural one-liners.

---

## 1. Which Service for What (Quick Lookup)

| I Need To... | Use This AWS Service | Primary Justification |
|:---|:---|:---|
| Run a VM with full OS/kernel control | **Amazon EC2** | Complete host access, custom drivers, legacy applications |
| Run code on events, pay strictly per use | **AWS Lambda** | Scales to zero, zero server management, sub-second auto-scaling |
| Run containerized microservices without servers | **Amazon ECS on AWS Fargate** | Native VPC networking (`awsvpc`), task IAM roles, zero host patching |
| Run an enterprise Kubernetes cluster | **Amazon EKS** | K8s API conformance, GitOps (ArgoCD), multi-cloud portability |
| Store files, backups, or build a data lake | **Amazon S3** | 11 9s durability, HTTP REST access, automated lifecycle tiering |
| Sub-10ms Key-Value or document data at huge scale | **Amazon DynamoDB** | Single-table NoSQL, on-demand capacity, single-digit ms latency |
| Relational SQL database with joins & transactions | **Amazon Aurora** / **RDS** | Multi-AZ high availability, managed backups, PostgreSQL/MySQL |
| Ultra-low latency (<1ms) in-memory cache | **Amazon ElastiCache (Redis)** | High cache hit ratios, session storage, real-time leaderboards |
| Shared POSIX file system mounted to 100+ tasks | **Amazon EFS** | Elastic serverless NFSv4 shared storage across AZs |
| Isolated cloud network | **Amazon VPC** | Micro-segmentation, public/private subnets, Security Groups |
| Global DNS routing | **Amazon Route 53** | Low-latency DNS, health checks, failover routing, apex Alias |
| Edge CDN, TLS termination, and WAF protection | **Amazon CloudFront** | 450+ edge PoPs, origin shield, DDoS protection |
| Distribute HTTP/HTTPS traffic to containers/EC2 | **Application Load Balancer (ALB)**| Layer 7 path/host routing, mTLS, target tracking integration |
| Extreme throughput TCP/UDP or static Anycast IPs | **Network Load Balancer (NLB)**| Layer 4 routing, millions of req/sec with sub-ms latency |
| Managed front door for serverless REST APIs | **API Gateway (HTTP API)** | Low cost (\$1.00/M), native JWT authorizers, auto-scaling |
| Decouple workers with persistent point-to-point buffer | **Amazon SQS** | At-least-once (or FIFO), backpressure protection, DLQ |
| 1-to-many broadcast pub/sub fan-out | **Amazon SNS** | Push notifications to mobile, SMS, email, and multiple SQS queues |
| Route events by JSON content / react to AWS events | **Amazon EventBridge** | Declarative schema matching, SaaS integrations, Event Replay |
| Orchestrate multi-step distributed sagas | **AWS Step Functions** | State machines, automatic retries, visual execution history |
| User sign-up, sign-in, and MFA for web/mobile apps | **Amazon Cognito User Pools** | Issues standard OIDC/OAuth2 JWT access tokens |
| Centralized workforce human access to AWS accounts | **IAM Identity Center (SSO)** | Federated Okta/Azure AD login, temporary STS credentials |
| Deploy from GitHub without stored keys | **GitHub OIDC + IAM Role** | Zero static access keys; short-lived STS tokens |
| Store database credentials with automated rotation | **AWS Secrets Manager** | Native 4-step Lambda rotation for RDS/Aurora |
| Centralized immutable compliance audit logging | **AWS CloudTrail + S3 Object Lock**| WORM compliance mode; tamper-proof security auditing |

---

## 2. Decision Shortcuts

```text
Compute?     Event-driven & short-lived (<15m) ....... AWS Lambda
             Containerized microservices ............. Amazon ECS with AWS Fargate
             Need direct host/GPU control ............ Amazon EC2
             Existing Kubernetes platform ............ Amazon EKS

Database?    Known key access, extreme scale ......... Amazon DynamoDB
             Complex joins, SQL, transactions ........ Amazon Aurora / RDS
             Large files / media / data lake ......... Amazon S3 (Store metadata pointer in DB)
             Graph relations (fraud rings, social) ... Amazon Neptune

Async?       One worker per message, buffer load ..... Amazon SQS
             Many subscribers (Fan-out) .............. Amazon SNS -> Multiple SQS Queues
             Route by JSON payload, SaaS integrations  Amazon EventBridge
             Multi-step transaction saga ............. AWS Step Functions

Front Door?  Edge CDN + Static S3 + WAF .............. Amazon CloudFront (via OAC)
             Persistent container services ........... Application Load Balancer (ALB)
             Serverless API endpoints ................ API Gateway HTTP API
             Ultra-high throughput raw TCP/UDP ....... Network Load Balancer (NLB)

Auth?        End-users of your mobile/web app ........ Amazon Cognito User Pools
             Engineers accessing AWS console/CLI ..... AWS IAM Identity Center (SSO)
             Third-party CI/CD runners (GitHub) ...... IAM Role via OIDC Federation
             Internal AWS compute services ........... IAM Execution Role
```

---

## 3. Symptom to Likely Root Cause Diagnostic Table

| Symptom | What You Check First | Root Cause & Remediation |
|:---|:---|:---|
| **Connection Timed Out** | Security Group / Route Table / NACL | Target SG missing inbound rule; private subnet missing route to NAT Gateway. |
| **Connection Refused** | Target host listening interface / port | Process listening on `127.0.0.1` instead of `0.0.0.0`; process crashed. |
| **`AccessDenied` anywhere** | `aws sts get-caller-identity` | Wrong account/profile; explicit `Deny` in SCP; missing KMS key decrypt permissions. |
| **S3 `AccessDenied` with valid policy** | `Resource` ARN in policy | PutObject requires `bucket/*` ARN, NOT `bucket` ARN; check S3 Block Public Access. |
| **Resources Disappeared** | AWS Console navigation bar | **Wrong AWS Region** selected in the top-right console dropdown. |
| **Lambda `Task timed out`** | Function timeout / VPC routing | Timeout set too low; Lambda placed in private VPC without NAT Gateway or VPC endpoints. |
| **Lambda processes message twice** | SQS Visibility Timeout | Visibility timeout expired before handler finished. **Set visibility to 6x Lambda timeout**. |
| **SQS Queue Depth growing** | Consumer worker status | Consumer containers crashing; check worker logs and inspect Dead-Letter Queue (DLQ). |
| **SNS/EventBridge delivers nothing** | Target Resource Policy | SQS queue policy does NOT grant `sqs:SendMessage` to the EventBridge rule ARN. |
| **ALB returns HTTP 502 / 503** | Target Group health check status | Containers failing health checks; target group contains zero healthy targets. |
| **API Gateway: `Missing Authentication Token`** | Request URL / HTTP method | **Wrong route or HTTP method** requested! (Does not match route definitions). |
| **ECS `CannotPullContainerError`** | ECR route / Execution role | Private subnet lacks route to ECR (no NAT/endpoint); task execution role missing ECR policy. |
| **ECS service flapping / crash looping** | `aws ecs describe-tasks` | Exit code `137` (OOM killed); health check grace period too short for container boot. |
| **CloudFront serves stale content** | Cache behavior TTL | Assets lack cache-busting hashes; execute `aws cloudfront create-invalidation --paths "/*"`. |
| **ACM Certificate not showing in CloudFront** | Certificate Region | CloudFront SSL certificates **MUST reside in `us-east-1` (N. Virginia)**. |
| **Database `too many connections`** | Active connection count | Horizontal Lambda surge exhausted connection pool; deploy **AWS RDS Proxy**. |

---

## 4. Fundamental Architectural One-Liners

1. **Default Deny; Explicit Deny Wins**: IAM permissions boundaries and SCPs filter and restrict; they never grant.
2. **Multi-AZ $\ne$ Backup $\ne$ Multi-Region**: Multi-AZ provides local high availability; backups protect against corruption; multi-region protects against regional disasters.
3. **At-Least-Once Delivery Mandates Idempotent Consumers**: Distributed queues will occasionally deliver duplicate messages; design handlers to handle retries cleanly.
4. **Always Attach a Dead-Letter Queue (DLQ)**: Never leave un-processable poison pill messages to cycle infinitely in an active queue.
5. **Initialize SDK and DB Clients Outside the Lambda Handler**: Leverage persistent container memory across warm invocations.
6. **Deploy Private Subnets for Data Tiers; Reference Security Groups, Not CIDRs**: Enforce micro-segmentation by allowing ingress only from specific security group IDs.
7. **Budgets Alert; They Do Not Cap**: Protect cloud accounts using real-time anomaly detection, active budget notifications, and restrictive SCPs.
8. **Disaster Recovery Backups Do Not Count Until Restored**: Regularly test backup restoration runbooks in automated quarterly GameDay simulations.
