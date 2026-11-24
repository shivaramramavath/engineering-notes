# Module 06: Cloud Operations, Security & IaC

Welcome to the **AWS Cloud Operations, Security & Infrastructure as Code (IaC)** module. This curriculum provides comprehensive, production-grade coverage of declarative and programmable IaC, automated deployment pipelines, enterprise secrets management, continuous threat detection, full-stack observability, reliability engineering, FinOps cost management, multi-account landing zones, and automated compliance auditing.

---

## Module Overview

Day-2 cloud operations determine whether an enterprise thrives or suffers from configuration drift, security breaches, uncontrolled spending, and cascading outages. This module equips DevOps engineers, platform architects, site reliability engineers (SREs), and cloud security professionals with the automation tools and governance frameworks required to operate resilient, secure, and cost-effective multi-account cloud environments.

---

## Detailed Curriculum Roadmap

| # | Note | Description | Key Focus Areas |
|:---|:---|:---|:---|
| **01** | [Infrastructure as Code (IaC) Fundamentals](01-infrastructure-as-code.md) | Declarative vs imperative paradigms, drift detection, state management, and GitOps | Idempotency, Remote state locking, Immutable infrastructure, Policy-as-code scanning |
| **02** | [AWS CloudFormation & Multi-Stack Governance](02-cloudformation.md) | Native declarative modeling, intrinsic functions, nested stacks, and StackSets | YAML/JSON templates, Intrinsic functions (!Sub, !Ref, !GetAtt), StackSets, DeletionPolicy |
| **03** | [AWS Cloud Development Kit (CDK v2)](03-cdk.md) | Programmable cloud infrastructure in TypeScript/Python, construct hierarchy, and security | L1/L2/L3 constructs, cdk synth/deploy, Bootstrapping, cdk-nag compliance, Jest unit testing |
| **04** | [Terraform & OpenTofu on AWS](04-terraform-on-aws.md) | Multi-cloud HCL engineering, S3/DynamoDB remote state, and modular architecture | AWS provider aliases, S3 backend with DynamoDB locking, moved blocks, state import |
| **05** | [CI/CD Deployment Pipelines to AWS](05-ci-cd-to-aws.md) | Automated software releases, GitHub Actions OIDC federation, and CodeDeploy strategies | GitHub Actions OIDC (zero static keys), Blue/Green & Canary deployments, AppSpec hooks |
| **06** | [Secrets Management & Cryptography (KMS/SSM)](06-security-and-secrets.md) | Hardware-backed key management, automated database rotation, and parameter stores | AWS KMS Customer Managed Keys, Envelope encryption, Secrets Manager 4-step rotation, SSM |
| **07** | [AWS Threat Detection & CSPM Security Services](07-security-services.md) | Machine learning anomaly detection, posture management, CVE scanning, and PII discovery | Amazon GuardDuty, AWS Security Hub (CIS/FSBP), Amazon Inspector, Amazon Macie |
| **08** | [Full-Stack Observability: CloudWatch & X-Ray](08-observability.md) | Telemetry metrics, structured log analytics, distributed tracing, and synthetic canaries | CloudWatch Logs Insights SQL, Metric Math, Composite Alarms, AWS X-Ray subsegments |
| **09** | [Reliability Engineering & Operational Health](09-reliability-and-cost.md) | Architectural governance, health notifications, service quotas, and chaos testing | Well-Architected Framework HRIs, Personal Health Dashboard, Service Quotas, AWS FIS |
| **10** | [Cost Optimization & FinOps Management](10-cost-optimisation-and-finops.md) | Cloud financial management, cost allocation tagging, rightsizing, and pricing models | Cost Allocation Tags, Compute Savings Plans, Compute Optimizer, S3 Intelligent-Tiering |
| **11** | [Enterprise Multi-Account Strategy & Landing Zones](11-multi-account-strategy.md) | AWS Organizations, Control Tower, Service Control Policies (SCPs), and shared services | Multi-account isolation, SCP preventive guardrails, Log Archive, Network Hub, AWS RAM |
| **12** | [Compliance, Governance & Continuous Auditing](12-compliance-and-auditing-basics.md) | Immutable audit trails, continuous configuration tracking, and compliance evidence | AWS CloudTrail multi-region trails, Log file validation, AWS Config rules, Audit Manager |

---

## Architectural Principles Enforced

1. **Zero Human Access in Production (GitOps)**:
   - Strip engineers of manual write permissions in production AWS consoles. All infrastructure and application changes must originate from version-controlled Git pull requests validated by CI/CD pipelines.
2. **Elimination of Static Long-Lived Credentials**:
   - Prohibit static AWS Access Keys in CI/CD runners. Authenticate third-party systems (GitHub Actions, GitLab) exclusively through **OIDC Federation** and temporary STS session tokens.
3. **Air-Gapped Immutable Telemetry**:
   - Stream all CloudTrail logs and audit records into an isolated, air-gapped **Log Archive Account** protected by **S3 Object Lock in Compliance Mode** (WORM), rendering logs mathematically tamper-proof.
4. **Preventive Guardrails via Service Control Policies (SCPs)**:
   - Enforce hard boundaries at the AWS hypervisor API layer (blocking unapproved regions, protecting security log deletion, enforcing encryption) that cannot be bypassed even by local account root users.

---

## Recommended Learning Flow

```
[01. IaC Fundamentals] -----------> [02. CloudFormation] -----------> [03. AWS CDK v2]
                                            |                                 |
                                            v                                 v
                                    [04. Terraform on AWS] ---------> [05. CI/CD to AWS]
                                                                              |
        +---------------------------------------------------------------------+
        |
        v
[06. Secrets & KMS] --------------> [07. Threat Detection (GuardDuty/Hub)]
        |                                       |
        v                                       v
[08. Observability (CW/X-Ray)] ---> [09. Reliability (Health/FIS)]
        |                                       |
        v                                       v
[10. FinOps Cost Optimization] ---> [11. Multi-Account Strategy & Control Tower]
                                                |
                                                v
                                    [12. Compliance & Auditing (CloudTrail/Config)]
```

---

## Module Pre-requisites & Cross-References

- **Foundations**: See [01-foundations](../01-foundations/) for AWS IAM fundamentals and Organizations basics.
- **Networking**: See [04-networking](../04-networking/) for VPC Transit Gateway and Network Firewall hub configurations.
- **Architecture Patterns**: See [07-architecture-patterns](../07-architecture-patterns/) for enterprise landing zone reference designs.
