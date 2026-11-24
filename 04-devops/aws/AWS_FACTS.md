# AWS Technical Facts & Architecture Baseline (As of 24 Nov 2026)

| Service / Domain | Parameter / Standard | Verified Value | Reference URL |
|---|---|---|---|
| **AWS CLI** | Version Baseline | AWS CLI v2 (v2.22+ / v2.24+) | https://docs.aws.amazon.com/cli/latest/ |
| **Compute - EC2** | Instance Architecture | Graviton3 (c7g, m7g, r7g) & Graviton4 (c8g, m8g, r8g); x86_64 (c7i, m7i) | https://aws.amazon.com/ec2/graviton/ |
| **Compute - EC2** | Metadata Service | IMDSv2 (Session Token required); IMDSv1 deprecated | https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/configuring-instance-metadata-service.html |
| **Compute - Lambda** | Active LTS Runtimes | Node.js 22, Node.js 20, Python 3.12, Python 3.11, Java 21, .NET 8, `provided.al2023` | https://docs.aws.amazon.com/lambda/latest/dg/lambda-runtimes.html |
| **Compute - Lambda** | Execution Limits | Max Memory: 10,240 MB (10 GB); Max `/tmp`: 10,240 MB; Max Duration: 900s (15 min) | https://docs.aws.amazon.com/lambda/latest/dg/gettingstarted-limits.html |
| **Compute - ECS** | Deployment & Fargate | ECS Service Connect (service mesh), Fargate compute platform 1.4.0+ | https://docs.aws.amazon.com/AmazonECS/latest/developerguide/ |
| **Compute - EKS** | Kubernetes Versions | Kubernetes 1.31, 1.30, 1.29 supported | https://docs.aws.amazon.com/eks/latest/userguide/kubernetes-versions.html |
| **Storage - S3** | Security Defaults | Block Public Access enabled by default; SSE-S3 default encryption; BucketOwnerEnforced (ACLs disabled) | https://docs.aws.amazon.com/AmazonS3/latest/userguide/create-bucket-overview.html |
| **Storage - S3** | Storage Classes | S3 Standard, S3 Intelligent-Tiering, S3 Standard-IA, S3 One Zone-IA, S3 Glacier Instant/Flexible/Deep Archive, S3 Express One Zone | https://aws.amazon.com/s3/storage-classes/ |
| **Database - DynamoDB** | Partition / Table Limits | Partition limit: 10 GB data, 1,000 WCU, 3,000 RCU; Max item size: 400 KB | https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/ServiceQuotas.html |
| **Database - RDS/Aurora**| Aurora Storage Limits | Auto-scaling storage up to 128 TiB; Aurora Serverless v2; Multi-AZ cluster with 2 readable standbys | https://docs.aws.amazon.com/AmazonRDS/latest/AuroraUserGuide/ |
| **Networking - VPC** | Core Quotas | Max 5 VPCs per region (default); 200 Route Tables per VPC; 5 Security Groups per ENI | https://docs.aws.amazon.com/vpc/latest/userguide/amazon-vpc-limits.html |
| **Networking - IPv4** | Public IPv4 Pricing | $0.005 per public IPv4 address per hour ($3.60/month per IP) | https://aws.amazon.com/blogs/aws/new-aws-public-ipv4-address-charge-public-ip-insights/ |
| **Security - IAM** | Identity Baseline | IAM Identity Center (AWS SSO) recommended for human users; short-lived STS credentials for workloads | https://docs.aws.amazon.com/singlesignon/latest/userguide/what-is.html |
| **AI - Bedrock** | Foundation Models | Anthropic Claude 3.5 Sonnet / Haiku / Opus, Amazon Nova (Micro, Lite, Pro), Meta Llama 3.3 / 4, Mistral Large | https://aws.amazon.com/bedrock/ |
| **Operations - IaC** | Modern Tooling | Terraform AWS Provider 5.x+, OpenTofu 1.8+, AWS CDK v2.160+ | https://registry.terraform.io/providers/hashicorp/aws/latest |

---

## 1. Security & Identity Invariants
- **Root Account Hygiene**: Root accounts must have hardware MFA enabled and zero active access keys. Root is used only for account closure, tax registration, and billing activation.
- **Human vs Workload IAM**:
  - Human users must access accounts via IAM Identity Center (SSO) integrated with an IdP (Okta, Google Workspace, Microsoft Entra ID).
  - Workload identities must use IAM Roles (EC2 Instance Profiles, ECS Task Roles, EKS Pod Identity / IRSA, Lambda Execution Roles) or GitHub Actions OIDC with zero static credentials.
- **IMDSv2 Enforcement**: Instances must enforce `HttpTokens=required` with `HttpPutResponseHopLimit=1` to prevent SSRF credential theft.

---

## 2. Compute Invariants
- **Graviton Adoption**: AWS Graviton processors (`arm64`) provide up to 40% better price-performance across EC2, Lambda, ECS, RDS, and OpenSearch.
- **Lambda Cold Start Optimization**: SnapStart for Java 11/17/21, Provisioned Concurrency for strict latency SLAs, and minimal initialization logic outside handlers.
- **Container Strategy**: Prefer ECS Fargate for lightweight microservices; choose EKS only when enterprise Kubernetes ecosystem compatibility is required.

---

## 3. Storage & Database Invariants
- **S3 Security Standard**: S3 buckets are private by default. Cross-account access requires explicit bucket policies and KMS key policy delegation.
- **DynamoDB Single-Table Design**: Optimize access patterns using composite partition and sort keys (`PK` / `SK`) with Global Secondary Indexes (`GSI`) rather than relational normalization.
- **RDS High Availability**: Multi-AZ deployments use synchronous physical replication across Availability Zones with automated DNS failover in 60–120 seconds.

---

## 4. Networking Invariants
- **VPC Subnet Architecture**: Standard 3-tier layout across 2–3 AZs:
  - Public Subnets: Internet Gateway (IGW), ALBs, NAT Gateways.
  - Private Subnets: Application workloads (EC2, ECS, Lambda with VPC ENIs).
  - Isolated Database Subnets: RDS, Aurora, ElastiCache (no route to NAT Gateway or IGW).
- **VPC Endpoints (PrivateLink)**: Interface endpoints for AWS services (S3, DynamoDB gateway endpoints; SQS, Secrets Manager interface endpoints) to avoid costly NAT Gateway data transfer fees.
- **Public IPv4 Cost Mitigation**: Avoid assigning public IPv4 addresses to backend EC2 instances; place workloads in private subnets behind ALBs.

---

## 5. Operations & FinOps Invariants
- **Infrastructure as Code (IaC)**: Never manually provision resources in production. Enforce GitOps via Terraform / OpenTofu or AWS CDK with automated pull request linting (`tflint`, `checkov`).
- **Observability**: CloudWatch Logs must have explicit retention periods (e.g. 14–30 days) to prevent runaway storage charges. Export long-term logs to S3 Glacier.
- **FinOps Best Practices**: Leverage AWS Compute Savings Plans (up to 66% discount) and EC2 Instance Savings Plans (up to 72% discount) alongside EC2 Spot instances for fault-tolerant workers.
