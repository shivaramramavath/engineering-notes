---
topic: "Containers on AWS Decision Guide"
module: "07-architecture-patterns"
track: "aws"
last-tested: "2026-11"
prerequisites:
  - "02-compute/03-ecs-fargate.md"
  - "02-compute/04-eks-overview.md"
  - "02-compute/05-app-runner.md"
  - "02-compute/06-compute-decision-matrix.md"
complexity: "advanced"
tags:
  - "containers"
  - "ecs"
  - "fargate"
  - "eks"
  - "app-runner"
  - "lambda-containers"
  - "architecture"
---

# Containers on AWS Decision Guide

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Notes](#8-performance-security--cost-notes)
- [9. Comparison Table](#9-comparison-table)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

Running containerized workloads in production on AWS presents teams with multiple distinct compute abstractions: **AWS App Runner**, **AWS Lambda Container Images**, **Amazon Elastic Container Service (ECS) with Fargate**, **Amazon ECS with EC2**, and **Amazon Elastic Kubernetes Service (EKS)**. Each option trades off operational complexity, infrastructure control, cold start latency, cost predictability, and ecosystem compatibility. Choosing the wrong container runtime can lead to excessive operational overhead, high idle infrastructure costs, or rigid architectures that cannot scale.

### Why a Structured Decision Framework Matters
- **Avoid Over-Engineering**: Deploying EKS for a standard monolithic web service introduces unnecessary Kubernetes control plane costs (\$73/month/cluster minimum) and steep operational burdens (CRDs, ingress controllers, CNI plugins).
- **Avoid Under-Engineering**: Selecting AWS Lambda container images for long-running, streaming, or stateful TCP workloads leads to execution timeout failures (15-minute cap) and connection drops.
- **Optimize Total Cost of Ownership (TCO)**: Compute instance pricing alone does not reflect engineering hours spent managing operating systems, patching AMIs, and upgrading orchestrator versions.

---

## 2. Core Concepts

### The 5 AWS Container Execution Environments

1. **AWS App Runner**:
   - Fully managed container application service designed for web applications and APIs.
   - Automatically builds from source or container image, provisions load balancers, manages TLS certificates, and scales instances up or down based on concurrent request thresholds.
   - Zero infrastructure management; limited to HTTP/HTTPS web endpoints.

2. **AWS Lambda (Container Image Packaging)**:
   - Run containers up to 10 GB in size using Lambda's serverless pay-per-use execution model.
   - Ideal for event-driven tasks, asynchronous processing, batch ETL, and bursty APIs.
   - Subject to Lambda constraints: 15-minute maximum execution timeout, stateless execution, and no direct incoming TCP socket access.

3. **Amazon ECS with AWS Fargate (Serverless Containers)**:
   - Serverless compute engine for containers managed by Amazon ECS.
   - Eliminates EC2 server management, OS patching, and cluster capacity planning while retaining full VPC networking control (`awsvpc` network mode), task-level IAM roles, and multi-container task definitions.
   - The default AWS recommendation for most microservices, scheduled batch jobs, and internal APIs.

4. **Amazon ECS with EC2 Instances**:
   - Container orchestration where you own and manage the underlying EC2 instance fleet.
   - Required for specialized hardware (GPU/Inf2 machine learning inference, Graviton custom sizing), persistent local NVMe storage, Windows containers, or specialized network configurations.
   - Lower unit cost per vCPU at high sustained utilization via EC2 Savings Plans or Spot Instances, but requires cluster AMI patching and capacity management.

5. **Amazon Elastic Kubernetes Service (Amazon EKS)**:
   - Certified Kubernetes conformant control plane managed by AWS.
   - Required for organizations with established Kubernetes tooling (Helm, ArgoCD, Istio, Prometheus Operator), multi-cloud portability requirements, complex multi-tenant pod isolation, or intricate microservice meshes.
   - Highest operational overhead and steepest learning curve.

---

## 3. How It Works Internally

```
+----------------------------------------------------------------------------------------------------+
|                               CONTAINERS ON AWS ARCHITECTURAL SPECTRUM                             |
+----------------------------------------------------------------------------------------------------+

  Lower Operational Burden -----------------------------------------> Higher Architectural Control
  Higher Opinionated Abstraction -----------------------------------> Lower-Level Infrastructure Control

   +---------------+     +--------------------+     +---------------+     +---------------+     +---------------+
   |  App Runner   |     | Lambda Containers  |     |  ECS Fargate  |     |    ECS EC2    |     |  Amazon EKS   |
   +---------------+     +--------------------+     +---------------+     +---------------+     +---------------+
   | Full Managed  |     | Serverless Event   |     | Serverless    |     | Managed Fleet |     | Kubernetes    |
   | Web & API     |     | Driven Invocations |     | Microservices |     | Full OS/GPU   |     | Cloud-Native  |
   | HTTPS Ingress |     | Max 15 min runtime |     | awsvpc native |     | Host Control  |     | Ecosystem     |
   +---------------+     +--------------------+     +---------------+     +---------------+     +---------------+
          |                        |                        |                     |                     |
          v                        v                        v                     v                     v
   Managed ALB/TLS          MicroVM (Firecracker)     Fargate Worker        EC2 Instances         EKS Worker Nodes
   Direct Public Ingress    Event Source Trigger      VPC Dedicated ENI     ECS Agent / Docker    Kubelet / containerd
```

### Decision Flowchart

```
Start: Need to run a Container?
   |
   +--> Is it a simple public web app / REST API with minimal infrastructure configuration?
   |      |-- YES --> Use **AWS App Runner**
   |      |-- NO
   |
   +--> Is it an event-driven task, SQS consumer, or batch job running < 15 minutes?
   |      |-- YES --> Use **AWS Lambda (Container Packaging)**
   |      |-- NO
   |
   +--> Does your team have strict requirements for Kubernetes APIs, Helm, ArgoCD, or multi-cloud?
   |      |-- YES --> Use **Amazon EKS** (Fargate or Managed Node Groups)
   |      |-- NO
   |
   +--> Do you need direct GPU access, persistent host storage, custom daemonsets, or ultra-low unit cost?
          |-- YES --> Use **Amazon ECS with EC2**
          |-- NO  --> Use **Amazon ECS with AWS Fargate** (Default Production Standard)
```

---

## 4. Syntax & Usage

### 4.1 Terraform: Deploying a Scalable ECS Fargate Service

```hcl
terraform {
  required_version = ">= 1.5.0"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.40"
    }
  }
}

provider "aws" {
  region = "us-east-1"
}

# ECS Cluster
resource "aws_ecs_cluster" "main" {
  name = "production-ecs-cluster"

  setting {
    name  = "containerInsights"
    value = "enabled"
  }
}

# ECS Task Definition (Fargate)
resource "aws_ecs_task_definition" "app" {
  family                   = "production-api-task"
  network_mode             = "awsvpc"
  requires_compatibilities = ["FARGATE"]
  cpu                      = "512"  # 0.5 vCPU
  memory                   = "1024" # 1 GB
  execution_role_arn       = aws_iam_role.ecs_execution.arn
  task_role_arn            = aws_iam_role.ecs_task.arn

  container_definitions = jsonencode([
    {
      name      = "api-container"
      image     = "123456789012.dkr.ecr.us-east-1.amazonaws.com/api:v1.2.0"
      essential = true
      portMappings = [
        {
          containerPort = 8080
          hostPort      = 8080
          protocol      = "tcp"
        }
      ]
      logConfiguration = {
        logDriver = "awslogs"
        options = {
          "awslogs-group"         = "/ecs/production-api"
          "awslogs-region"        = "us-east-1"
          "awslogs-stream-prefix" = "api"
        }
      }
      healthCheck = {
        command     = ["CMD-SHELL", "curl -f http://localhost:8080/health || exit 1"]
        interval    = 30
        timeout     = 5
        retries     = 3
        startPeriod = 60
      }
    }
  ])
}

# ECS Service with Fargate Capacity Provider
resource "aws_ecs_service" "app" {
  name            = "production-api-service"
  cluster         = aws_ecs_cluster.main.id
  task_definition = aws_ecs_task_definition.app.arn
  desired_count   = 3

  capacity_provider_strategy {
    capacity_provider = "FARGATE"
    weight            = 1
    base              = 1
  }

  capacity_provider_strategy {
    capacity_provider = "FARGATE_SPOT"
    weight            = 2 # 66% on Spot for cost optimization
  }

  network_configuration {
    subnets          = ["subnet-0123456789abcdef0", "subnet-0fedcba9876543210"]
    security_groups  = [aws_security_group.ecs_tasks.id]
    assign_public_ip = false
  }

  load_balancer {
    target_group_arn = aws_lb_target_group.api.arn
    container_name   = "api-container"
    container_port   = 8080
  }
}
```

---

## 5. Practical Examples

### Example 1: Building and Packaging Docker Containers for AWS Lambda

```dockerfile
# Dockerfile for AWS Lambda Container Image
FROM public.ecr.aws/lambda/python:3.11

# Copy requirements file
COPY requirements.txt ${LAMBDA_TASK_ROOT}

# Install dependencies into task root
RUN pip install --no-cache-dir -r requirements.txt

# Copy application handler code
COPY app.py ${LAMBDA_TASK_ROOT}

# Set the CMD to your handler (file.function)
CMD [ "app.handler" ]
```

```bash
# Authenticate Docker to Amazon ECR
aws ecr get-login-password --region us-east-1 | docker login --username AWS --password-stdin 123456789012.dkr.ecr.us-east-1.amazonaws.com

# Build and Push Container Image
docker build -t 123456789012.dkr.ecr.us-east-1.amazonaws.com/image-processor:latest .
docker push 123456789012.dkr.ecr.us-east-1.amazonaws.com/image-processor:latest

# Create or Update Lambda Function
aws lambda create-function \
    --function-name ImageProcessingFunction \
    --package-type Image \
    --code ImageUri=123456789012.dkr.ecr.us-east-1.amazonaws.com/image-processor:latest \
    --role arn:aws:iam::123456789012:role/lambda-execution-role \
    --timeout 300 \
    --memory-size 2048
```

### Example 2: AWS App Runner Service Deployment via AWS CLI

```bash
# Deploy a container to App Runner without configuring ALBs or VPCs
aws apprunner create-service \
    --service-name my-web-api \
    --source-configuration '{
        "ImageRepository": {
            "ImageIdentifier": "123456789012.dkr.ecr.us-east-1.amazonaws.com/web-app:v1.0",
            "ImageConfiguration": {
                "Port": "8080",
                "RuntimeEnvironmentVariables": {
                    "ENV": "production",
                    "PORT": "8080"
                }
            },
            "ImageRepositoryType": "ECR"
        },
        "AutoDeploymentsEnabled": true,
        "AuthenticationConfiguration": {
            "AccessRoleArn": "arn:aws:iam::123456789012:role/AppRunnerECRAccessRole"
        }
    }' \
    --instance-configuration '{
        "Cpu": "1 vCPU",
        "Memory": "2 GB"
    }'
```

### Example 3: Kubernetes Deployment for Amazon EKS with AWS Load Balancer Controller

```yaml
# deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: order-service
  namespace: production
  labels:
    app: order-service
spec:
  replicas: 3
  selector:
    matchLabels:
      app: order-service
  template:
    metadata:
      labels:
        app: order-service
    spec:
      serviceAccountName: order-service-sa # IAM Roles for Service Accounts (IRSA)
      containers:
      - name: order-service
        image: 123456789012.dkr.ecr.us-east-1.amazonaws.com/order-service:v2.1
        ports:
        - containerPort: 8080
        resources:
          limits:
            cpu: "500m"
            memory: "512Mi"
          requests:
            cpu: "250m"
            memory: "256Mi"
        livenessProbe:
          httpGet:
            path: /healthz
            port: 8080
          initialDelaySeconds: 15
          periodSeconds: 20
---
# ingress.yaml (Automates ALB creation via AWS Load Balancer Controller)
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: order-service-ingress
  namespace: production
  annotations:
    kubernetes.io/ingress.class: alb
    alb.ingress.kubernetes.io/scheme: internet-facing
    alb.ingress.kubernetes.io/target-type: ip
spec:
  rules:
  - host: orders.example.com
    http:
      paths:
      - path: /
        pathType: Prefix
        backend:
          service:
            name: order-service
            port:
              number: 8080
```

### Example 4: Mixed Fargate On-Demand and Fargate Spot Auto Scaling

```hcl
# Auto Scaling Target for ECS Service
resource "aws_appautoscaling_target" "ecs_target" {
  max_capacity       = 20
  min_capacity       = 2
  resource_id        = "service/${aws_ecs_cluster.main.name}/${aws_ecs_service.app.name}"
  scalable_dimension = "ecs:service:DesiredCount"
  service_namespace  = "ecs"
}

# Target Tracking Policy on CPU Utilization
resource "aws_appautoscaling_policy" "ecs_cpu_policy" {
  name               = "ecs-target-tracking-cpu70"
  policy_type        = "TargetTrackingScaling"
  resource_id        = aws_appautoscaling_target.ecs_target.resource_id
  scalable_dimension = aws_appautoscaling_target.ecs_target.scalable_dimension
  service_namespace  = aws_appautoscaling_target.ecs_target.service_namespace

  target_tracking_scaling_policy_configuration {
    target_value       = 70.0
    scale_in_cooldown  = 300
    scale_out_cooldown = 60
    predefined_metric_specification {
      predefined_metric_type = "ECSServiceAverageCPUUtilization"
    }
  }
}
```

### Example 5: ECS Task Role vs ECS Task Execution Role Separation

```hcl
# 1. Execution Role: Used by ECS Agent to pull images and write logs
resource "aws_iam_role" "ecs_execution" {
  name = "ecs-task-execution-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Action    = "sts:AssumeRole"
      Effect    = "Allow"
      Principal = { Service = "ecs-tasks.amazonaws.com" }
    }]
  })
}

resource "aws_iam_role_policy_attachment" "execution_attach" {
  role       = aws_iam_role.ecs_execution.name
  policy_arn = "arn:aws:iam::aws:policy/service-role/AmazonECSTaskExecutionRolePolicy"
}

# 2. Task Role: Used by container application code to call AWS APIs (DynamoDB, S3)
resource "aws_iam_role" "ecs_task" {
  name = "ecs-order-app-task-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Action    = "sts:AssumeRole"
      Effect    = "Allow"
      Principal = { Service = "ecs-tasks.amazonaws.com" }
    }]
  })
}

resource "aws_iam_policy" "task_ddb_policy" {
  name = "ecs-app-dynamodb-access"
  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Effect   = "Allow"
      Action   = ["dynamodb:GetItem", "dynamodb:PutItem"]
      Resource = "arn:aws:dynamodb:us-east-1:123456789012:table/Orders"
    }]
  })
}

resource "aws_iam_role_policy_attachment" "task_attach" {
  role       = aws_iam_role.ecs_task.name
  policy_arn = aws_iam_policy.task_ddb_policy.arn
}
```

---

## 6. Best Practices

1. **Default to ECS with Fargate for New AWS Services**: Fargate delivers the ideal balance of zero server maintenance, enterprise VPC networking, task IAM isolation, and sub-minute scaling.
2. **Strictly Separate Task Execution Role from Task Application Role**: Use the execution role strictly for infrastructure bootstrap (ECR pull, Secrets Manager decryption), and the task role for business application permissions.
3. **Leverage Fargate Spot for Stateless, Fault-Tolerant Microservices**: Combine Fargate On-Demand (base=1) with Fargate Spot (weight=2) to reduce compute costs by up to 70% with automatic fallback.
4. **Tune StopTimeout for Graceful Shutdown**: Configure `stopTimeout` (up to 120 seconds in ECS) to give SIGTERM handlers adequate time to drain active HTTP connections before receiving SIGKILL.
5. **Use Multi-Stage Docker Builds and Minimal Base Images**: Build production containers using distroless, Alpine, or minimal Chainguard images to minimize vulnerability attack surfaces and optimize image pull speed.
6. **Do Not Default to EKS Without Dedicated Platform Engineers**: Kubernetes requires ongoing cluster version upgrades (every 14 months per version), ingress controller lifecycle management, and node group rotation.

---

## 7. Common Mistakes

| Mistake | Why It Hurts | Correct Approach |
|:---|:---|:---|
| **Choosing EKS for simple microservices** | High idle cost (\$73/mo control plane + nodes) and massive operational complexity without utilizing K8s specific features. | Use Amazon ECS with AWS Fargate. |
| **Using Lambda Containers for long-running streaming** | Hard 15-minute execution limit causes sudden process termination mid-stream. | Deploy long-running services to ECS Fargate or App Runner. |
| **Confusing Task Execution Role with Task Role** | Putting application database IAM permissions on the Execution Role prevents containers from accessing AWS services. | Put ECR/logging on `execution_role_arn`; put app DynamoDB/S3 permissions on `task_role_arn`. |
| **Running ECS tasks in public subnets with public IPs** | Exposes container ENIs directly to internet port scans and violates security compliance. | Place tasks in private subnets behind an Application Load Balancer in public subnets. |
| **Ignoring Docker image size (>2GB)** | Massive container images cause slow Fargate task startup times (2-5 minutes) during horizontal scale-out. | Use multi-stage Docker builds; keep production runtime images below 250 MB. |
| **Using hardcoded EC2 node sizes without Karpenter on EKS** | Static node groups result in low bin-packing efficiency, wasted memory/CPU, and slow node scaling. | Deploy **Karpenter** for rapid, right-sized, just-in-time EC2 node provisioning on EKS. |

---

## 8. Performance, Security & Cost Notes

### Performance Characteristics
- **Startup Latency**:
  - AWS Lambda: Sub-second (50ms - 800ms)
  - AWS App Runner: ~15-30 seconds
  - ECS Fargate: 30-90 seconds (ENI allocation + container image pull)
  - ECS EC2 (pre-pulled cached image): 5-15 seconds
  - EKS with Karpenter: 45-90 seconds (EC2 launch + join + pod schedule)

### Security Baseline
- **awsvpc Mode**: Every ECS Fargate task receives its own Elastic Network Interface (ENI) with dedicated private IP and dedicated Security Group rules, ensuring strict pod/task network isolation.
- **Vulnerability Scanning**: Enable Amazon ECR Enhanced Scanning (powered by Amazon Inspector) for automated CVE detection in container base layers.

### Cost Breakdown
- **AWS App Runner**: \$0.064/vCPU-hour + \$0.007/GB-hour (Memory remains active during idle at lower rate).
- **AWS Fargate (us-east-1)**: \$0.04048/vCPU-hour + \$0.004445/GB-hour.
- **Fargate Spot**: Up to 70% discount (\$0.012144/vCPU-hour).
- **Amazon EKS**: \$0.10/hour (\$73/month) per cluster control plane + underlying compute node costs.

---

## 9. Comparison Table

| Attribute | AWS App Runner | AWS Lambda Containers | Amazon ECS Fargate | Amazon ECS EC2 | Amazon EKS |
|:---|:---|:---|:---|:---|:---|
| **Operational Overhead** | Near Zero | Near Zero | Low | Medium | High |
| **Scaling Metric** | Concurrent Requests | Requests / Invocations | CPU / Memory / Custom | Cluster Autoscaler | HPA / KEDA / Karpenter |
| **Max Runtime** | Unlimited | 15 Minutes | Unlimited | Unlimited | Unlimited |
| **VPC Integration** | Optional (Egress) | Native (ENI) | Native (`awsvpc`) | Bridge / Host / awsvpc | AWS VPC CNI Plugin |
| **Hardware Control** | Standard CPU | Standard / Graviton | Standard / Graviton | GPU, Inf2, Bare Metal | GPU, Custom Nodes |
| **Pricing Model** | Provisioned + Request | Per-Millisecond | Per-Second | Per-EC2-Instance | \$73/mo + Compute |
| **Ecosystem Portability** | Low (AWS Proprietary) | Low (Lambda API) | Medium (Docker) | Medium (Docker) | High (Open K8s) |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between the ECS Task Execution Role and the ECS Task Role?</strong></summary>

- **ECS Task Execution Role (`execution_role_arn`)**: Used by the AWS infrastructure layer (the ECS container agent and Fargate infrastructure). It grants permissions required to launch the container: pulling images from Amazon ECR, fetching decrypted environment variables from AWS Secrets Manager/SSM Parameter Store, and streaming container logs to CloudWatch Logs.
- **ECS Task Role (`task_role_arn`)**: Used by the actual application process running inside the Docker container. It grants permissions needed by your code: querying Amazon DynamoDB, reading from Amazon S3, publishing to SNS, or calling external AWS APIs.
</details>

<details>
<summary><strong>2. When should an engineering organization choose Amazon EKS over Amazon ECS?</strong></summary>

Choose **Amazon EKS** when:
1. The organization already has deep Kubernetes expertise, standard Helm charts, GitOps workflows (ArgoCD, Flux), or complex service meshes (Istio, Linkerd).
2. The workload requires multi-cloud or hybrid-cloud portability with identical orchestrator semantics across environments.
3. Advanced Kubernetes ecosystem operators (KEDA, Prometheus Operator, cert-manager) are central to the application lifecycle.
Choose **Amazon ECS** when the team wants simpler, deeply integrated, robust container orchestration without the ongoing overhead of managing Kubernetes versions, etcd backups, and complex CNI networking.
</details>

<details>
<summary><strong>3. How does Fargate Spot work in ECS, and how do you protect against sudden terminations?</strong></summary>

Fargate Spot runs container tasks on spare AWS compute capacity at up to a 70% discount. When AWS needs the capacity back, Fargate sends an interruption notice via Amazon EventBridge and a SIGTERM signal to the container 2 minutes prior to termination. To protect application reliability:
1. Use a mixed capacity provider strategy: keep a minimum baseline of tasks on `FARGATE` On-Demand (e.g., `base: 1, weight: 1`), and scale burst traffic on `FARGATE_SPOT` (`weight: 2`).
2. Implement graceful shutdown handling in application code: intercept `SIGTERM`, stop accepting new HTTP requests, drain in-flight transactions, and exit cleanly within the 2-minute window.
</details>

<details>
<summary><strong>4. Why does ECS Fargate require the `awsvpc` network mode?</strong></summary>

In `awsvpc` network mode, every ECS task is allocated its own dedicated Elastic Network Interface (ENI) and private IPv4/IPv6 address from the VPC subnet. This provides the exact same networking properties and security capabilities as an EC2 instance: tasks can be assigned specific Security Groups, network traffic can be monitored via VPC Flow Logs, and tasks can communicate securely across subnets without host port mapping collisions. Fargate requires `awsvpc` because there are no underlying customer-accessible EC2 hosts to support bridge or host networking modes.
</details>

<details>
<summary><strong>5. How does Karpenter improve on the standard Kubernetes Cluster Autoscaler in Amazon EKS?</strong></summary>

The standard Cluster Autoscaler works by adjusting the desired capacity of pre-configured EC2 Auto Scaling Groups (ASGs), which is constrained to specific instance sizes and takes several minutes to evaluate and launch. **Karpenter** is an open-source, flexible, high-performance node autoscaler that bypasses ASGs entirely:
1. It listens directly to unschedulable pod events and evaluates pod resource requests, labels, and taints.
2. It calls AWS EC2 fleet APIs directly to launch the exact right-sized, lowest-cost EC2 instance (choosing dynamically across hundreds of instance types and Spot capacity pools).
3. It spins up new worker nodes in under 45 seconds and automatically consolidates or terminates under-utilized nodes to minimize compute waste.
</details>

<details>
<summary><strong>6. What are the primary trade-offs of AWS App Runner compared to ECS Fargate?</strong></summary>

- **AWS App Runner Advantages**: Extreme simplicity. Automatically configures TLS certificates, custom domains, load balancers, and automatic scale-out/scale-in to zero based on HTTP concurrency without writing complex VPC or ALB Terraform code.
- **App Runner Limitations**: Limited strictly to HTTP/HTTPS workloads (no raw TCP/UDP, no background queue workers without HTTP listeners); limited advanced VPC ingress customization; cannot run daemon sidecars or complex multi-container pod architectures; higher unit cost per active compute hour than pure EC2.
</details>

<details>
<summary><strong>7. How do you pass secrets securely into an ECS Fargate container?</strong></summary>

Never hardcode secrets in Dockerfiles or pass plaintext secrets in task definition `environment` blocks. Instead, store the secret in **AWS Secrets Manager** or **AWS Systems Manager Parameter Store**, and reference the secret ARN in the task definition `secrets` block. The ECS container agent fetches and decrypts the secret at container launch using permissions from the **Task Execution Role**, injecting the plaintext secret directly into the container process memory as an environment variable.
</details>

<details>
<summary><strong>8. Why might container startup times on AWS Fargate be slower than on EC2, and how can this be optimized?</strong></summary>

On EC2, if multiple tasks run on the same instance, Docker reuses cached container image layers already present on the host disk. On Fargate, every task runs in a fresh, isolated MicroVM sandbox with no local layer cache, meaning the entire container image must be downloaded across the network from ECR on every launch. To optimize:
1. Minimize image size using Alpine or Distroless base images and multi-stage Docker builds.
2. Host container images in an Amazon ECR repository in the same AWS Region.
3. Enable **AWS Fargate Seekable OCI (SOCI)**: SOCI generates an image index that allows Fargate to start the container lazily while downloading image data in the background, cutting launch times from minutes to seconds.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Challenge 1: Deploy a Multi-Container ECS Task Definition with Fargate</strong></summary>

**Goal**: Write and deploy an ECS task definition containing an application container (Node.js or Python) and an Envoy/Nginx reverse-proxy sidecar in `awsvpc` network mode.
**Verification**: Verify via `curl` that requests to the sidecar port proxy correctly to the application container via `localhost`.
</details>

<details>
<summary><strong>Challenge 2: Implement Graceful Shutdown in a Container</strong></summary>

**Goal**: Build a Docker container that listens for `SIGTERM`, delays for 10 seconds while finishing in-flight requests, and logs a clean shutdown message before exiting.
**Verification**: Deploy to ECS Fargate, set `stopTimeout: 30`, issue a task stop command via AWS CLI, and inspect CloudWatch Logs to confirm graceful termination.
</details>

<details>
<summary><strong>Challenge 3: Deploy an AWS App Runner Service from ECR</strong></summary>

**Goal**: Push a container to Amazon ECR and create an AWS App Runner service with auto-deployments enabled on image push.
**Verification**: Push a new tag to ECR and verify that App Runner automatically initiates a zero-downtime rolling deployment.
</details>

<details>
<summary><strong>Challenge 4: Configure Fargate Capacity Provider Strategy (On-Demand + Spot)</strong></summary>

**Goal**: Configure an ECS service using Terraform with a capacity provider strategy specifying `FARGATE: base 1, weight 1` and `FARGATE_SPOT: weight 3`.
**Verification**: Scale the service to 4 tasks; verify in the AWS console that 1 task runs on Fargate and 3 run on Fargate Spot.
</details>

<details>
<summary><strong>Challenge 5: Deploy an EKS Pod with IAM Roles for Service Accounts (IRSA)</strong></summary>

**Goal**: Create an IAM OIDC identity provider for an EKS cluster, create a Kubernetes ServiceAccount annotated with an IAM role ARN, and deploy a pod reading from an S3 bucket.
**Verification**: Execute `kubectl exec` into the pod and run `aws s3 ls` without configuring any static AWS access keys.
</details>

<details>
<summary><strong>Challenge 6: Measure Container Cold Starts with Seekable OCI (SOCI)</strong></summary>

**Goal**: Build a 1 GB container image. Deploy it to ECS Fargate normally, then generate a SOCI index using the `soci` CLI and redeploy.
**Verification**: Compare the task launch time metrics in CloudWatch Logs to quantify the startup acceleration achieved by SOCI.
</details>

---

## 12. Summary / Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
|                               CONTAINER COMPUTE DECISION MATRIX                                    |
+----------------------------------------------------------------------------------------------------+
| Use Case                          | Recommended Service | Key Justification                        |
+-----------------------------------+---------------------+------------------------------------------+
| Public Web API / Simple Service   | AWS App Runner      | Zero infrastructure; automatic TLS & ALB |
| Event-driven / Short ETL (<15min) | Lambda Containers   | Pay-per-millisecond; scales to zero      |
| Production Microservices (Default)| Amazon ECS Fargate  | Enterprise VPC networking, no OS ops     |
| GPU / Custom Hardware / Windows   | Amazon ECS on EC2   | Full host control, specialized hardware  |
| Kubernetes / Multi-Cloud Strategy | Amazon EKS          | Industry standard K8s API, Helm, GitOps  |
+-----------------------------------+---------------------+------------------------------------------+
```

---

## 13. Further Reading

- [AWS Prescriptive Guidance: Choosing an AWS Container Service](https://docs.aws.amazon.com/prescriptive-guidance/latest/strategy-evaluating-container-services/welcome.html)
- [Amazon ECS Best Practices Guide](https://docs.aws.amazon.com/AmazonECS/latest/bestpracticesguide/)
- [Amazon EKS Best Practices Guide](https://aws.github.io/aws-eks-best-practices/)
- [Karpenter: Kubernetes Node Autoscaler Documentation](https://karpenter.sh/)
- [Under the Hood: AWS Fargate Architecture](https://aws.amazon.com/blogs/compute/under-the-hood-aws-fargate-data-plane/)
