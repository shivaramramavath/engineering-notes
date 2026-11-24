# Module 02: Compute, Containers & Serverless

Welcome to the **AWS Compute, Containers & Serverless** module. This curriculum provides comprehensive, production-grade coverage of compute infrastructure across the AWS cloud: virtual servers, container orchestration platforms, serverless microVMs, distributed batch engines, and advanced cost optimization frameworks.

---

## Module Overview

This module equips platform engineers, DevOps practitioners, and cloud architects with the practical knowledge required to deploy, scale, and optimize enterprise workloads. From bare-metal Nitro virtualization to serverless container meshes and automated spot reclamation, every note emphasizes modern production standards: mandatory **IMDSv2**, keyless management via **SSM Session Manager**, **Graviton ARM64 architecture**, and **FinOps efficiency**.

---

## Detailed Curriculum Roadmap

| # | Note | Description | Key Focus Areas |
|:---|:---|:---|:---|
| **01** | [Amazon EC2 Deep Dive](01-ec2.md) | Virtual computing, Nitro System, virtualization types, storage, and IMDSv2 security | Nitro architecture, CPU families, GP3 vs GP2, IMDSv2 enforcement, SSM Session Manager |
| **02** | [EC2 Auto Scaling & Launch Templates](02-ec2-autoscaling-and-launch-templates.md) | Elastic fleet scaling, versioned templates, mixed instances, and rolling upgrades | Target tracking, lifecycle hooks, instance refresh, ELB health checks, spot policies |
| **03** | [AWS Lambda Serverless Fundamentals](03-lambda.md) | Serverless event-driven architecture, execution lifecycle, and invocation models | Firecracker MicroVM, cold vs warm starts, 1,769 MB vCPU scaling, SQS batch failure reporting |
| **04** | [AWS Lambda Advanced Patterns](04-lambda-deep-dive.md) | Layers, extensions, provisioned concurrency, SnapStart, and container images | Hyperplane ENI VPC networking, external sidecar extensions, Java SnapStart, 10 GB OCI containers |
| **05** | [Amazon ECS & AWS Fargate Fundamentals](05-ecs-fargate.md) | Container orchestration, Task Definitions, ECS Services, and serverless containers | Task Execution Role vs Task Role, Fargate Spot, deployment circuit breaker, ECS Exec |
| **06** | [Amazon ECS Advanced Networking & Deployments](06-ecs-deep-dive.md) | Microservice mesh with Service Connect, task networking, and canary blue/green deployments | Envoy sidecars, Cloud Map private DNS, Cluster Auto Scaling (CAS), CodeDeploy canary shifting |
| **07** | [Amazon EKS Fundamentals](07-eks-fundamentals.md) | Managed Kubernetes control plane, worker node fleets, and native VPC networking | AWS VPC CNI, Prefix Delegation (/28), EKS Pod Identity, Karpenter sub-minute autoscaler |
| **08** | [AWS Elastic Beanstalk & App Runner](08-elastic-beanstalk-and-app-runner.md) | Platform-as-a-Service (PaaS) and fully managed serverless web container hosting | Customer IaaS vs managed PaaS, App Runner VPC Connector, concurrency scaling, worker tier |
| **09** | [AWS Batch & Step Functions Orchestration](09-batch-and-step-functions.md) | High-throughput batch computing and finite state machine workflow orchestration | Managed compute environments, Spot fleets, Standard vs Express workflows, Distributed Map |
| **10** | [AWS Graviton, Spot & Savings Plans](10-graviton-and-spot-and-savings-plans.md) | Compute cost reduction, ARM64 silicon migration, Spot interruption handling, and FinOps | Graviton3/4 price-performance, 2-minute Spot warning, Compute vs EC2 Savings Plans |

---

## Architectural Principles Enforced

1. **Defense-in-Depth & Keyless Remote Access**:
   - Zero open SSH port 22 ingress rules. All remote access to EC2 and ECS containers is governed via **AWS Systems Manager Session Manager** and **ECS Exec**.
   - Mandatory **IMDSv2** (`HttpTokens=required`, `HttpPutResponseHopLimit=1`) to neutralize Server-Side Request Forgery (SSRF) token exfiltration.
2. **Modern Silicon & Microarchitecture**:
   - Default to **AWS Graviton3/Graviton4 (ARM64)** across EC2, ECS, EKS, and Lambda for ~40% better price-to-performance and 60% lower energy consumption.
3. **Resilient Container & MicroVM Lifecycle**:
   - Enforce **deployment circuit breakers** with automated rollback in ECS.
   - Decouple **Task Execution Roles** (infrastructure bootstrap) from **Task Roles** (application business permissions).
4. **Structural FinOps Engineering**:
   - Stack **Compute Savings Plans** (covering 80–85% of steady-state baseline trough) with diversified **EC2 Spot Fleets** (using `price-capacity-optimized`) to achieve aggregate compute savings up to 70–80%.

---

## Recommended Learning Flow

```
[01. EC2 Deep Dive] ---------> [02. EC2 Auto Scaling & Launch Templates]
       |                                          |
       +--------------------+                     v
                            |          [10. Graviton, Spot & Savings Plans]
                            v                     ^
                 [03. Lambda Fundamentals]        |
                            |                     |
                            v                     |
                 [04. Lambda Advanced Patterns]   |
                            |                     |
                            v                     |
                 [05. ECS & Fargate] -------------+
                            |                     |
                            v                     |
                 [06. ECS Advanced Networking]    |
                            |                     |
                            v                     |
                 [07. EKS Fundamentals] ----------+
                            |                     |
                            v                     |
        [08. Elastic Beanstalk & App Runner]      |
                            |                     |
                            v                     |
        [09. AWS Batch & Step Functions] ---------+
```

Proceed to **[Module 03: Storage & Databases](../03-storage-and-databases/README.md)** once you have mastered compute architectures!
