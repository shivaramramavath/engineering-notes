# AWS Curriculum Architecture & Completion Plan

| Metadata | Details |
|---|---|
| **Track** | AWS Cloud Architecture & Operations (`04-devops/aws/`) |
| **Status** | Active Execution Plan |
| **Total Modules** | 10 Modules (`01-foundations` through `10-cheatsheets`) |
| **Total Notes Planned** | **101 Notes** + 10 Module `README.md` files + 1 Track Root `README.md` |
| **Template Baseline** | 100% Strict Compliance with `04-devops/_templates/` |

---

## 1. Master Rename & Migration Map

| Original File Path | New Target Path | Status / Action |
|---|---|---|
| `aws/reference/cheatsheet.md` | `aws/10-cheatsheets/` (split into 10 focused cards) | Migrate & expand into dedicated cheatsheet module |
| `aws/reference/interview.md` | `aws/09-interview/` (split into 9 categorized Q&A notes) | Migrate & expand into dedicated interview module |
| `aws/01-foundations/01-what-is-aws.md` | `aws/01-foundations/01-what-is-aws.md` | Rewrite to 13 sections with metadata & diagrams |
| `aws/01-foundations/02-accounts-regions-and-billing.md` | `aws/01-foundations/03-accounts-regions-and-billing.md` | Renumber & rewrite (inserts `02-shared-responsibility`) |
| `aws/01-foundations/03-iam.md` | `aws/01-foundations/05-iam-fundamentals.md` | Renumber & rewrite with deep IAM model |
| `aws/01-foundations/04-aws-cli.md` | `aws/01-foundations/08-aws-cli.md` | Renumber & rewrite with v2 SSO & JMESPath |
| `aws/02-compute/01-ec2.md` | `aws/02-compute/01-ec2.md` | Rewrite with IMDSv2, Nitro, Graviton4 |
| `aws/02-compute/02-lambda.md` | `aws/02-compute/03-lambda.md` | Renumber & rewrite with lifecycle & runtimes |
| `aws/02-compute/03-ecs-fargate.md` | `aws/02-compute/05-ecs-fargate.md` | Renumber & rewrite with modern task definitions |
| `aws/03-storage-and-databases/01-s3.md` | `aws/03-storage-and-databases/01-s3.md` | Rewrite with modern security defaults |
| `aws/03-storage-and-databases/02-dynamodb.md` | `aws/03-storage-and-databases/04-dynamodb.md` | Renumber & rewrite with core schema concepts |
| `aws/03-storage-and-databases/03-rds-and-aurora.md` | `aws/03-storage-and-databases/06-rds-and-aurora.md` | Renumber & rewrite with Aurora architecture |
| `aws/04-networking/01-vpc.md` | `aws/04-networking/01-vpc.md` | Rewrite with 3-tier layout & CIDR math |
| `aws/04-networking/02-route53-and-cloudfront.md` | `aws/04-networking/05-route53-and-dns-patterns.md` & `06-cloudfront.md` | Split into dedicated DNS and CDN notes |
| `aws/04-networking/03-load-balancing-and-api-gateway.md` | `aws/04-networking/07-load-balancing.md` & `08-api-gateway.md` | Split into dedicated ALB/NLB and Gateway notes |
| `aws/05-app-services/01-sqs.md` | `aws/05-app-services/01-sqs.md` | Rewrite with FIFO & DLQ redrive patterns |
| `aws/05-app-services/02-sns-and-eventbridge.md` | `aws/05-app-services/02-sns-and-eventbridge.md` | Rewrite with EventBridge bus architecture |
| `aws/05-app-services/03-ses-email.md` | `aws/05-app-services/05-ses-email.md` | Renumber & rewrite with DKIM/SPF hardening |
| `aws/05-app-services/04-cognito.md` | `aws/05-app-services/06-cognito.md` | Renumber & rewrite with User/Identity pools |
| `aws/05-app-services/05-bedrock.md` | `aws/05-app-services/08-bedrock.md` | Renumber & rewrite with 2026 model catalog |
| `aws/06-operations/01-infrastructure-as-code.md` | `aws/06-operations/01-infrastructure-as-code.md` | Rewrite with declarative IaC paradigms |
| `aws/06-operations/02-ci-cd-to-aws.md` | `aws/06-operations/05-ci-cd-to-aws.md` | Renumber & rewrite with GitHub Actions OIDC |
| `aws/06-operations/03-security-and-secrets.md` | `aws/06-operations/06-security-and-secrets.md` | Renumber & rewrite with KMS & Secrets Manager |
| `aws/06-operations/04-observability.md` | `aws/06-operations/08-observability.md` | Renumber & rewrite with CloudWatch & X-Ray |
| `aws/06-operations/05-reliability-and-cost.md` | `aws/06-operations/09-reliability-and-cost.md` | Renumber & rewrite with Well-Architected DR |

---

## 2. Final Numbered Module & Note Layout (101 Notes)

### Module 01: Cloud Foundations & Governance (12 Notes)
- `README.md`: Module 01 Overview
- `01-what-is-aws.md`: Cloud computing models, API-driven infrastructure, enterprise mental model.
- `02-shared-responsibility-and-well-architected.md`: Security OF vs IN the cloud, 6 Well-Architected pillars.
- `03-accounts-regions-and-billing.md`: Root hygiene, Regions, Availability Zones, billing alarms, public IPv4 costs.
- `04-organizations-scp-and-control-tower.md`: Multi-account management, OU hierarchies, Service Control Policies (SCPs).
- `05-iam-fundamentals.md`: Users, Groups, Roles, Policies, STS temporary credentials.
- `06-iam-policy-evaluation-and-least-privilege.md`: Evaluation logic, explicit Deny, Permission Boundaries, Condition keys.
- `07-iam-identity-center-and-sso.md`: Workforce access, SAML/OIDC IdP integration, permission sets.
- `08-aws-cli.md`: AWS CLI v2 installation, SSO profiles, JMESPath `--query`, pagination.
- `09-sdks-and-credential-provider-chain.md`: Default credential provider chain, environment variables, STS role assumption.
- `10-console-cloudshell-and-tagging.md`: AWS CloudShell, Management Console, Tagging strategies, Cost Allocation Tags.
- `11-global-infrastructure.md`: Edge locations, Regional edge caches, Local Zones, Wavelength, GovCloud, Partitions.
- `12-pricing-calculators-and-budgets.md`: AWS Pricing Calculator, Cost Explorer, AWS Budgets, anomaly detection.

### Module 02: Compute & Containers (10 Notes)
- `README.md`: Module 02 Overview
- `01-ec2.md`: Instance families, Graviton4, EBS boot disks, User Data scripts, IMDSv2 enforcement.
- `02-ec2-autoscaling-and-launch-templates.md`: Launch Templates, Auto Scaling Groups, dynamic & predictive scaling.
- `03-lambda.md`: Serverless execution environment, handler signatures, event payloads, active runtimes.
- `04-lambda-deep-dive.md`: Cold starts, SnapStart, Provisioned Concurrency, VPC networking, Lambda Layers.
- `05-ecs-fargate.md`: Elastic Container Service, Fargate serverless compute, Task Definitions, clusters.
- `06-ecs-deep-dive.md`: ECS Service Connect, rolling deployments, circuit breakers, container healthchecks.
- `07-eks-fundamentals.md`: Elastic Kubernetes Service, control plane, managed node groups, EKS Pod Identity.
- `08-elastic-beanstalk-and-app-runner.md`: AWS App Runner container deployments vs Elastic Beanstalk PaaS.
- `09-batch-and-step-functions.md`: AWS Batch job queues and compute environments for asynchronous workloads.
- `10-graviton-and-spot-and-savings-plans.md`: ARM64 Graviton migration, EC2 Spot interruption handling, Savings Plans.

### Module 03: Storage & Databases (11 Notes)
- `README.md`: Module 03 Overview
- `01-s3.md`: Bucket creation, Block Public Access, SSE-S3 default encryption, S3 URIs, CLI sync.
- `02-s3-deep-dive.md`: S3 Express One Zone, Object Ownership, Lifecycle rules, Cross-Region Replication, Presigned URLs.
- `03-ebs-efs-and-fsx.md`: Elastic Block Store (gp3/io2), Elastic File System (NFS), Amazon FSx for Lustre/Windows.
- `04-dynamodb.md`: NoSQL core concepts, tables, partition keys, sort keys, items, attributes, read consistency.
- `05-dynamodb-data-modelling.md`: Single-table design, GSI / LSI indexes, DynamoDB Streams, transactional writes.
- `06-rds-and-aurora.md`: Relational Database Service, Amazon Aurora distributed storage architecture.
- `07-rds-operations.md`: Automated snapshots, Multi-AZ failover, Read Replicas, RDS Proxy connection pooling.
- `08-elasticache-redis-and-memcached.md`: In-memory caching, Redis replication groups, cluster mode, caching patterns.
- `09-opensearch-service.md`: Managed search and analytics cluster, indexing, vector search capabilities.
- `10-analytics-overview.md`: Amazon Athena serverless SQL, AWS Glue data catalog, Amazon Redshift data warehousing.
- `11-backup-and-disaster-recovery.md`: AWS Backup centralized policies, vault lock, cross-region snapshot replication.

### Module 04: Networking & Content Delivery (10 Notes)
- `README.md`: Module 04 Overview
- `01-vpc.md`: Virtual Private Cloud, CIDR math, 3-tier subnets (public, private, data), Internet Gateways.
- `02-vpc-deep-dive.md`: NAT Gateway high-availability, VPC Route Tables, VPC Gateway & Interface Endpoints (PrivateLink).
- `03-security-groups-and-nacls.md`: Stateful Security Groups vs Stateless Network Access Control Lists (NACLs).
- `04-vpc-peering-transit-gateway-and-privatelink.md`: Inter-VPC peering, AWS Transit Gateway hub-and-spoke, PrivateLink.
- `05-route53-and-dns-patterns.md`: Hosted zones, Alias records, Simple, Weighted, Latency, Failover, Geolocation routing.
- `06-cloudfront.md`: Global CDN, origin request policies, Origin Access Control (OAC), CloudFront Functions.
- `07-load-balancing.md`: Application Load Balancer (ALB), Network Load Balancer (NLB), target groups, TLS termination.
- `08-api-gateway.md`: HTTP APIs vs REST APIs, WebSocket APIs, request validation, throttling, Cognito authorizers.
- `09-vpn-and-direct-connect-overview.md`: AWS Site-to-Site VPN, Client VPN, and AWS Direct Connect dedicated links.
- `10-waf-shield-and-network-security.md`: AWS WAF rate-limiting rules, SQLi/XSS inspection, AWS Shield DDoS protection.

### Module 05: Application Integration & Messaging (9 Notes)
- `README.md`: Module 05 Overview
- `01-sqs.md`: Simple Queue Service, Standard vs FIFO queues, deduplication, visibility timeouts, Dead-Letter Queues.
- `02-sns-and-eventbridge.md`: Simple Notification Service pub/sub fan-out, Amazon EventBridge default & custom buses.
- `03-kinesis-and-msk-overview.md`: Kinesis Data Streams sharding vs Amazon Managed Streaming for Apache Kafka (MSK).
- `04-step-functions.md`: Visual state machines, task states, parallel execution, distributed map, error retry catch.
- `05-ses-email.md`: Simple Email Service, domain verification, DKIM, SPF, reputation dashboard, bounce handling.
- `06-cognito.md`: User Pools (authentication, JWT tokens, MFA) vs Identity Pools (federated AWS STS temporary access).
- `07-appsync-overview.md`: Managed GraphQL API service, real-time subscriptions, DynamoDB resolvers, caching.
- `08-bedrock.md`: Generative AI foundation models (Claude 3.5, Nova, Llama 3), Guardrails, Knowledge Bases RAG.
- `09-event-driven-patterns-on-aws.md`: Event choreography vs orchestration, transactional outbox pattern, idempotent consumers.

### Module 06: Cloud Operations, Security & IaC (12 Notes)
- `README.md`: Module 06 Overview
- `01-infrastructure-as-code.md`: Declarative IaC principles, state management, drift detection, GitOps pipelines.
- `02-cloudformation.md`: CloudFormation JSON/YAML templates, intrinsic functions, stacks, change sets, StackSets.
- `03-cdk.md`: AWS Cloud Development Kit (CDK v2), TypeScript/Python constructs, synthesizer, bootstrap.
- `04-terraform-on-aws.md`: HashiCorp Terraform / OpenTofu AWS provider, remote S3 state backend with DynamoDB locking.
- `05-ci-cd-to-aws.md`: Continuous integration and deployment via GitHub Actions with OIDC and AWS CodePipeline.
- `06-security-and-secrets.md`: AWS KMS symmetric/asymmetric keys, Secrets Manager automated rotation, SSM Parameter Store.
- `07-security-services.md`: AWS CloudTrail audit logs, AWS Config compliance, Amazon GuardDuty, AWS Security Hub.
- `08-observability.md`: CloudWatch Metrics, Alarms, Logs Insights queries, AWS X-Ray distributed tracing, OpenTelemetry.
- `09-reliability-and-cost.md`: Multi-AZ vs Multi-Region, RTO / RPO recovery objectives, fault injection testing.
- `10-cost-optimisation-and-finops.md`: Cost allocation tags, Compute Savings Plans, AWS Cost Anomaly Detection, FinOps culture.
- `11-multi-account-strategy.md`: AWS Landing Zones, AWS Control Tower, centralized networking, log archive accounts.
- `12-compliance-and-auditing-basics.md`: SOC 1/2/3, ISO 27001, HIPAA, PCI-DSS compliance on AWS with AWS Artifact.

### Module 07: Enterprise Architecture Patterns (10 Notes)
- `README.md`: Module 07 Overview
- `01-well-architected-pillars-in-practice.md`: Operational Excellence, Security, Reliability, Performance, Cost, Sustainability.
- `02-three-tier-web-app.md`: Standard production architecture: ALB in public subnets, ECS/EC2 in private, RDS in isolated.
- `03-serverless-api-pattern.md`: API Gateway HTTP API + Lambda Node/Python + DynamoDB on-demand + Cognito auth.
- `04-event-driven-architecture.md`: EventBridge event router, decoupled microservices, SQS buffering, fanout topologies.
- `05-containers-on-aws-decision-guide.md`: Architectural decision framework: App Runner vs ECS Fargate vs EKS.
- `06-multi-region-and-dr-strategies.md`: Backup & Restore, Pilot Light, Warm Standby, and Multi-Site Active-Active designs.
- `07-data-lake-pattern.md`: S3 raw/curated zones, AWS Glue crawlers, Athena query engine, Lake Formation security.
- `08-zero-trust-and-landing-zone.md`: Zero-trust network segmentation, IAM Identity Center, VPC endpoints, mTLS.
- `09-migration-strategies.md`: The 6 Rs (Rehost, Replatform, Repurchase, Refactor, Retire, Retain) with AWS Migration Hub.
- `10-choosing-the-right-service.md`: Definitive decision trees: compute, database, caching, messaging, and storage services.

### Module 08: Production Reference Projects (8 Projects)
- `README.md`: Module 08 Overview
- `01-static-site-on-s3-cloudfront.md`: Static single-page application hosted on S3 with CloudFront CDN, custom domain, and OAC.
- `02-serverless-rest-api.md`: Serverless CRUD REST API using API Gateway HTTP API, Lambda (Node.js/Python), and DynamoDB.
- `03-containerised-app-on-ecs-fargate-with-alb.md`: Containerized backend running on ECS Fargate behind an Application Load Balancer.
- `04-three-tier-vpc-with-rds.md`: Complete 3-tier VPC architecture with NAT Gateways, bastion host, and Aurora PostgreSQL.
- `05-event-driven-order-pipeline.md`: E-commerce event processing pipeline using EventBridge, SQS FIFO queues, and Lambda.
- `06-secure-ci-cd-with-github-oidc.md`: Zero-secret CI/CD pipeline using GitHub Actions OIDC to assume an IAM role and deploy to ECS.
- `07-terraform-landing-zone-basics.md`: Baseline Terraform/OpenTofu project provisioning VPC, KMS keys, and S3 secure state.
- `08-observability-stack-on-cloudwatch.md`: Production observability dashboard with CloudWatch Metric Math, Alarms, and Logs Insights.

### Module 09: Technical Interview Preparation (9 Notes)
- `README.md`: Module 09 Overview
- `01-aws-fundamentals-qa.md`: Cloud computing models, global infrastructure, shared responsibility, billing mechanics.
- `02-iam-and-security-qa.md`: IAM policy evaluation logic, cross-account access, permission boundaries, KMS key policies.
- `03-networking-qa.md`: VPC subnets, route tables, NAT Gateways, Security Groups vs NACLs, Route 53 routing policies.
- `04-compute-and-containers-qa.md`: EC2 instance lifecycle, Lambda execution environment, cold starts, ECS vs EKS tradeoffs.
- `05-storage-and-databases-qa.md`: S3 consistency and storage classes, DynamoDB partition math and modeling, Aurora replication.
- `06-serverless-and-messaging-qa.md`: SQS vs SNS vs EventBridge, FIFO deduplication, Step Functions state machines.
- `07-architecture-and-scenario-questions.md`: Scenario walkthroughs: scaling for traffic spikes, multi-region failover, cost crisis.
- `08-cost-and-reliability-qa.md`: FinOps calculations, surprise bill diagnosis, high availability vs disaster recovery trade-offs.
- `09-behavioural-and-project-discussion.md`: Explaining AWS architectures to interviewers, trade-off storytelling, production war stories.

### Module 10: Quick Reference Cheatsheets (10 Notes)
- `README.md`: Module 10 Overview
- `01-cli-commands.md`: High-density AWS CLI v2 command reference across core services.
- `02-iam-policy-snippets.md`: Copy-pasteable production IAM policy statements (S3, DynamoDB, SQS, KMS, OIDC).
- `03-vpc-cidr-and-routing.md`: Subnet CIDR calculation matrix, route table configurations, and port references.
- `04-s3-and-dynamodb.md`: S3 CLI commands and DynamoDB single-table key schema reference card.
- `05-lambda-and-api-gateway.md`: Lambda handler templates, environment variables, API Gateway routing directives.
- `06-ecs-eks-quick-ref.md`: Task definition JSON templates, kubectl commands, and Fargate sizing charts.
- `07-cloudwatch-queries.md`: CloudWatch Logs Insights query recipes and Metric Math expressions.
- `08-terraform-and-cdk-snippets.md`: Essential Terraform AWS provider blocks and AWS CDK construct recipes.
- `09-service-chooser.md`: Rapid lookup decision tables: "I need X, which service should I choose?".
- `10-limits-and-defaults.md`: Verified, dated reference of default quotas, timeouts, payload limits, and costs.

---

## 3. Execution Batches Strategy

- **Batch 1**: Module 01 Foundations (Notes 01–06)
- **Batch 2**: Module 01 Foundations (Notes 07–12) + Module 01 README
- **Batch 3**: Module 02 Compute (Notes 01–05)
- **Batch 4**: Module 02 Compute (Notes 06–10) + Module 02 README
- **Batch 5**: Module 03 Storage & Databases (Notes 01–06)
- **Batch 6**: Module 03 Storage & Databases (Notes 07–11) + Module 03 README
- **Batch 7**: Module 04 Networking (Notes 01–05)
- **Batch 8**: Module 04 Networking (Notes 06–10) + Module 04 README
- **Batch 9**: Module 05 App Services (Notes 01–05)
- **Batch 10**: Module 05 App Services (Notes 06–09) + Module 05 README
- **Batch 11**: Module 06 Operations (Notes 01–06)
- **Batch 12**: Module 06 Operations (Notes 07–12) + Module 06 README
- **Batch 13**: Module 07 Architecture Patterns (Notes 01–05)
- **Batch 14**: Module 07 Architecture Patterns (Notes 06–10) + Module 07 README
- **Batch 15**: Module 08 Reference Projects (Projects 01–04)
- **Batch 16**: Module 08 Reference Projects (Projects 05–08) + Module 08 README
- **Batch 17**: Module 09 Interview Preparation (Notes 01–09) + Module 09 README
- **Batch 18**: Module 10 Cheatsheets (Notes 01–10) + Module 10 README
- **Batch 19**: Track Root Integration (`README.md`, `AWS_PROGRESS.md`, `AWS_CHANGELOG.md`, `AWS_DECISIONS.md`, `tree.txt`, `AWS_FINAL_REPORT.md`)
