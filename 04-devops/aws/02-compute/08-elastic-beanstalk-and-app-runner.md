# AWS Elastic Beanstalk & AWS App Runner (PaaS Compute)

> **Module**: `02-compute`  
> **Topic**: Platform-as-a-Service (PaaS) and Fully Managed Container Compute  
> **Target Audience**: Full-Stack Developers, Cloud Engineers, DevOps Architects  
> **Last Updated**: November 2026

---

## Navigation & Contents

- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Notes](#8-performance-security--cost-notes)
- [9. Comparison Table](#9-comparison-table)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference](#12-summary--quick-reference)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

Deploying web applications directly onto raw infrastructure (EC2, VPCs, Auto Scaling Groups, ALBs) requires significant operational setup, pipeline orchestration, and continuous maintenance. When rapid time-to-market is the primary business requirement, engineering teams often prefer **Platform-as-a-Service (PaaS)** abstractions that handle deployment, SSL provisioning, and load balancing automatically.

AWS provides two distinct PaaS paradigms:
1. **AWS Elastic Beanstalk (Legacy / Configurable PaaS)**: Provisions and orchestrates underlying AWS infrastructure resources (EC2 instances, ASGs, ALBs, Security Groups) inside the customer's account. Provides a high level of abstraction while retaining direct root-level access to the underlying virtual servers and networking components.
2. **AWS App Runner (Modern / Fully Managed Container PaaS)**: The modern, serverless container application platform. You point App Runner to a GitHub repository or a container image in Amazon ECR, and it automatically builds, deploys, load-balances, and scales the application. App Runner abstracts away servers, VPC networking, and load balancers entirely—delivering a streamlined "Heroku-like" developer experience directly on top of AWS infrastructure.

---

## 2. Core Concepts

### 2.1 AWS Elastic Beanstalk Architecture
- **Application**: The logical container for the project (e.g., `PaymentService`).
- **Application Version**: A specific, labeled iteration of deployable code packaged in an Amazon S3 zip bundle.
- **Environment**: A provisioned deployment running a specific Application Version. Environments are categorized into two tiers:
  - **Web Server Environment**: Provisions an Application Load Balancer, an Auto Scaling Group of EC2 instances, and Route 53 DNS records to serve HTTP/HTTPS traffic.
  - **Worker Environment**: Provisions an EC2 Auto Scaling fleet paired with an Amazon SQS queue and an **SQS Daemon (`sqsd`)** that pulls queue messages and forwards them as local HTTP POST requests to the web server running on `localhost`.
- **Configuration Hooks**:
  - `.ebextensions/`: Legacy YAML/JSON configuration files executed during environment provisioning.
  - `.platform/`: Modern platform hooks executed at specific lifecycle phases (`hooks/prebuild`, `hooks/predeploy`, `hooks/postdeploy`).

### 2.2 Elastic Beanstalk Deployment Strategies
- **All at Once**: Deploys to all instances simultaneously. Fastest, but incurs customer-facing downtime.
- **Rolling**: Deploys in batches (e.g., 2 instances at a time). Avoids total downtime but reduces available capacity during deployment.
- **Rolling with Additional Batch**: Launches a spare batch of new instances first, preserving 100% capacity throughout the rollout.
- **Immutable**: Provisions an entirely new temporary Auto Scaling Group with new instances. If deployment succeeds, instances are merged into the main ASG and old instances are terminated. Guarantees clean rollback on failure.
- **Traffic Splitting**: Directs a small percentage of production traffic to the new version (canary) for a evaluation period before completing the rollout.

### 2.3 AWS App Runner Architecture
- **Source Code vs. Container Image**:
  - **Code Repository Provider**: Connects to GitHub, detects runtime (Node.js, Python, Java, Go, Ruby, PHP, .NET), builds the image via an App Runner managed build pipeline, and deploys.
  - **Image Repository Provider**: Pulls pre-built container images directly from Amazon ECR (Public or Private).
- **Automatic Scaling**: Scales container instances up or down based on concurrent HTTP requests (e.g., scale out when concurrent requests exceed 100 per instance).
- **Fully Managed Ingress & TLS**: Automatically assigns a default domain with managed public SSL/TLS certificates; custom domains are mapped with automated certificate renewal.
- **VPC Connector**: Allows the serverless App Runner instances to reach private resources (RDS Aurora, Redis, internal APIs) inside a customer VPC.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        PaaS Architectural Models Compared                         |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ AWS Elastic Beanstalk (Customer-Owned IaaS Automation) ]                       |
|  +-----------------------------------------------------------------------------+  |
|  | Customer VPC                                                                |  |
|  |                                                                             |  |
|  |  +-----------------------------------------------------------------------+  |  |
|  |  | Application Load Balancer (Customer Account)                          |  |  |
|  |  +-----------------------------------------------------------------------+  |  |
|  |                                     |                                       |  |
|  |                                     v                                       |  |
|  |  +-----------------------------------------------------------------------+  |  |
|  |  | Auto Scaling Group of EC2 Instances (Customer Manages OS & SSH)       |  |  |
|  |  | - Web Server (Nginx / Gunicorn / Puma)                                |  |  |
|  |  | - Elastic Beanstalk Health Daemon                                     |  |  |
|  |  +-----------------------------------------------------------------------+  |  |
|  +-----------------------------------------------------------------------------+  |
|                                                                                   |
|  [ AWS App Runner (AWS-Managed Serverless Container Platform) ]                   |
|  +-----------------------------------------------------------------------------+  |
|  | AWS Managed Service Boundary (Zero Customer Server Infrastructure)          |  |
|  |                                                                             |  |
|  |  [ Managed HTTPS Edge Ingress & TLS Termination ]                           |  |
|  |                                |                                            |  |
|  |                                v                                            |  |
|  |  [ Managed Request Router & Concurrency Autoscaler ]                        |  |
|  |                                |                                            |  |
|  |                 +--------------+--------------+                             |  |
|  |                 |                             |                             |  |
|  |                 v                             v                             |  |
|  |      +---------------------+       +---------------------+                  |  |
|  |      | App Runner Instance |       | App Runner Instance |                  |  |
|  |      | (Isolated MicroVM)  |       | (Isolated MicroVM)  |                  |  |
|  |      +---------------------+       +---------------------+                  |  |
|  |                 |                             |                             |  |
|  +-----------------|-----------------------------|-----------------------------+  |
|                    v                             v                                |
|             [ AWS App Runner VPC Connector (Hyperplane Network Tunnel) ]          |
|                                                  |                                |
|                                                  v                                |
|             [ Customer Private VPC: Amazon Aurora RDS / ElastiCache ]             |
+-----------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 AWS App Runner Service Creation via CLI
Deploy a containerized application directly from Amazon ECR with VPC access.

```bash
aws apprunner create-service \
  --service-name catalog-api \
  --source-configuration '{
    "ImageRepository": {
      "ImageIdentifier": "111122223333.dkr.ecr.us-east-1.amazonaws.com/catalog:v1.0.0",
      "ImageConfiguration": {
        "Port": "8080",
        "RuntimeEnvironmentVariables": {
          "ENVIRONMENT": "production",
          "PORT": "8080"
        }
      },
      "ImageRepositoryType": "ECR"
    },
    "AutoDeploymentsEnabled": true,
    "AuthenticationConfiguration": {
      "AccessRoleArn": "arn:aws:iam::111122223333:role/AppRunnerECRAccessRole"
    }
  }' \
  --instance-configuration '{
    "Cpu": "1 vCPU",
    "Memory": "2 GB",
    "InstanceRoleArn": "arn:aws:iam::111122223333:role/CatalogAppInstanceRole"
  }' \
  --network-configuration '{
    "EgressConfiguration": {
      "EgressType": "VPC",
      "VpcConnectorArn": "arn:aws:apprunner:us-east-1:111122223333:vpcconnector/prod-vpc-connector/1"
    }
  }'
```

### 4.2 Elastic Beanstalk `.platform` Lifecycle Hook
Configuring an automated database migration hook prior to application start.

**Directory Structure:**
```
my-app/
├── .platform/
│   └── confighooks/
│       └── predeploy/
│           └── 01_run_migrations.sh
├── app.py
└── requirements.txt
```

**File: `.platform/confighooks/predeploy/01_run_migrations.sh`**
```bash
#!/bin/bash
set -xeuo pipefail

echo "Running pre-deploy database migrations..."
source /var/app/venv/staging-LQM1lest/bin/activate
cd /var/app/staging
python manage.py migrate --noinput
echo "Migrations completed successfully."
```

---

## 5. Practical Examples

### Example 1: Declarative Terraform Definition for AWS App Runner Service
Complete deployment of an App Runner service connected to private subnets via a VPC Connector.

```hcl
resource "aws_iam_role" "apprunner_instance_role" {
  name = "apprunner-catalog-instance-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Action    = "sts:AssumeRole"
      Effect    = "Allow"
      Principal = { Service = "tasks.apprunner.amazonaws.com" }
    }]
  })
}

resource "aws_iam_role" "apprunner_ecr_access" {
  name = "apprunner-ecr-access-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Action    = "sts:AssumeRole"
      Effect    = "Allow"
      Principal = { Service = "build.apprunner.amazonaws.com" }
    }]
  })
}

resource "aws_iam_role_policy_attachment" "ecr_access" {
  role       = aws_iam_role.apprunner_ecr_access.name
  policy_arn = "arn:aws:iam::aws:policy/service-role/AWSAppRunnerServicePolicyForECRAccess"
}

resource "aws_apprunner_vpc_connector" "connector" {
  vpc_connector_name = "apprunner-private-vpc"
  subnets            = var.private_subnet_ids
  security_groups    = [var.apprunner_security_group_id]
}

resource "aws_apprunner_service" "api" {
  service_name = "production-catalog-api"

  source_configuration {
    auto_deployments_enabled = true
    authentication_configuration {
      access_role_arn = aws_iam_role.apprunner_ecr_access.arn
    }
    image_repository {
      image_identifier      = "${var.ecr_repo_url}:v1.0"
      image_repository_type = "ECR"
      image_configuration {
        port = "8080"
      }
    }
  }

  instance_configuration {
    cpu               = "1024"
    memory            = "2048"
    instance_role_arn = aws_iam_role.apprunner_instance_role.arn
  }

  network_configuration {
    egress_configuration {
      egress_type       = "VPC"
      vpc_connector_arn = aws_apprunner_vpc_connector.connector.arn
    }
  }
}
```

### Example 2: Elastic Beanstalk Immutable Deployment via `.ebextensions`
Declare strict immutable deployment settings to guarantee zero downtime and clean rollback.

**File: `.ebextensions/01_deploy_policy.config`**
```yaml
option_settings:
  aws:elasticbeanstalk:command:
    DeploymentPolicy: Immutable
    Timeout: '600'
  aws:autoscaling:asg:
    MinSize: '2'
    MaxSize: '8'
  aws:elasticbeanstalk:environment:process:default:
    HealthCheckPath: '/healthz'
    DeregistrationDelay: '30'
```

### Example 3: App Runner Custom Concurrency Auto Scaling Configuration
Configure App Runner to scale out when an instance experiences more than 50 concurrent requests.

```bash
# 1. Create custom auto scaling configuration
AUTO_SCALING_ARN=$(aws apprunner create-auto-scaling-configuration \
  --auto-scaling-configuration-name "high-throughput-scaling" \
  --max-concurrency 50 \
  --min-size 2 \
  --max-size 20 \
  --query 'AutoScalingConfiguration.AutoScalingConfigurationArn' \
  --output text)

# 2. Bind to service
aws apprunner update-service \
  --service-arn "arn:aws:apprunner:us-east-1:111122223333:service/production-catalog-api/12345" \
  --auto-scaling-configuration-arn "$AUTO_SCALING_ARN"
```

### Example 4: Elastic Beanstalk Worker Tier Processing Pattern
In a worker tier, Elastic Beanstalk runs a local SQS daemon (`sqsd`) that reads messages from an SQS queue and posts them locally:

```python
# Worker app listening on port 80 (Flask / Express)
from flask import Flask, request, jsonify

app = Flask(__name__)

@app.route('/', methods=['POST'])
def process_sqs_message():
    """
    Called by local sqsd daemon forwarding an SQS message.
    HTTP 200 deletes message from SQS.
    HTTP 4xx/5xx forces sqsd to leave message in SQS for retry.
    """
    message_body = request.get_json(force=True)
    task_id = message_body.get('taskId')
    
    # Process computation
    print(f"Executing batch task: {task_id}")
    
    return jsonify({"status": "SUCCESS"}), 200

if __name__ == '__main__':
    app.run(host='127.0.0.1', port=5000)
```

### Example 5: Custom Domain Association on AWS App Runner
Bind a branded domain to App Runner with automated SSL verification.

```bash
aws apprunner associate-custom-domain \
  --service-arn "arn:aws:apprunner:us-east-1:111122223333:service/production-catalog-api/12345" \
  --domain-name "api.company.com" \
  --enable-www-subdomain false
```

---

## 6. Best Practices

1. **Standardize on AWS App Runner for New Containerized Web Apps**: For standard web APIs, microservices, and webhooks that don't need custom EC2 kernel modules or specialized daemonsets, choose **App Runner over Elastic Beanstalk**. It eliminates server patching, VPC configuration, and load balancer management.
2. **Use Immutable Deployments for Elastic Beanstalk**: Avoid "All at Once" or "Rolling" in production. Immutable deployments spin up a fresh ASG, guaranteeing that if code fails health checks, production traffic is untouched and broken instances are discarded automatically.
3. **Use App Runner VPC Connectors for Database Access**: When connecting App Runner to Amazon Aurora or Amazon ElastiCache, always deploy an **App Runner VPC Connector** into private subnets. Never expose databases publicly.
4. **Tune Max Concurrency on App Runner**: App Runner's default concurrency threshold is 100 concurrent requests per instance. For memory-heavy or CPU-intensive applications (e.g., image resizing or complex machine learning inference), lower `MaxConcurrency` to 20–30 to trigger earlier scale-out and maintain low latency.
5. **Decouple Secrets from Application Bundles**: In both Beanstalk and App Runner, inject sensitive secrets via AWS Secrets Manager or Parameter Store rather than checking `.env` files into source repositories.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Using Elastic Beanstalk for New Green-Field Projects** | Unnecessary operational debt managing underlying EC2 AMIs, SSH keys, and legacy `.ebextensions`. | Adopt AWS App Runner or Amazon ECS Fargate for modern container workflows. |
| **All-at-Once Deployment in Production Beanstalk** | Immediate customer-facing HTTP 502 Bad Gateway downtime while all servers restart simultaneously. | Standardize on `Immutable` or `RollingWithAdditionalBatch` deployment policies. |
| **Omitting App Runner ECR Access Role** | App Runner fails to pull the container image from Amazon ECR, failing deployment with an authentication error. | Attach an IAM role with `AWSAppRunnerServicePolicyForECRAccess` to `authentication_configuration`. |
| **Exposing Worker Tier Beanstalk to the Internet** | SQS worker instances are deployed with public IPs and an open ALB, wasting budget and creating an attack surface. | Worker tier environments should be deployed without an ALB, pulling messages from SQS via the internal `sqsd`. |
| **Ignoring App Runner Instance Pause Billing** | Paused App Runner services retain provisioned memory allocations, continuing to incur memory baseline costs. | Fully delete test services or scale `MinSize` down to 1. |
| **Hardcoding DB Migrations in Container Entrypoint** | In multi-instance auto-scaling setups, 5 instances boot concurrently and run migrations in race conditions. | Run migrations as pre-deploy hooks (`.platform/confighooks/predeploy`) or via one-off ECS migration tasks. |

---

## 8. Performance, Security & Cost Notes

### Scaling Characteristics
- **App Runner Scaling Metric**: Based purely on **concurrent in-flight HTTP requests**. It measures active requests per instance in real time and provisions additional instances in 15–30 seconds.
- **Elastic Beanstalk Scaling Metric**: Relies on standard EC2 Auto Scaling (e.g., average CPU utilization or ALB request count). Reaction time is slower (1–3 minutes).

### Security Architecture
- **App Runner Isolation**: Each App Runner instance runs in an isolated, dedicated single-tenant microVM managed by AWS.
- **TLS Offloading**: App Runner handles TLS termination at the AWS managed edge, enforcing TLS 1.2+ with automatic certificate issuance and rotation via ACM.

### Cost Breakdown
- **Elastic Beanstalk**: Free service; you pay standard rates for underlying resources (EC2 instances, ALBs, S3 storage, CloudWatch logs).
- **AWS App Runner**:
  - **Active Instances**: Billed per vCPU ($0.064/vCPU-hour) and memory ($0.007/GB-hour).
  - **Provisioned (Idle) Instances**: Only billed for memory ($0.007/GB-hour); CPU is throttled and billed at **$0.00**.

---

## 9. Comparison Table

| Dimension | Elastic Beanstalk | AWS App Runner | Amazon ECS on Fargate |
|:---|:---|:---|:---|
| **Paradigm** | IaaS Automation / Legacy PaaS | Serverless Container PaaS | Managed Container Orchestration |
| **Infrastructure Visibility** | Full (EC2, ASG, ALB visible in account)| Completely Abstracted / Managed | Semi-visible (Tasks, Services, VPC) |
| **Direct Server Access** | Yes (SSH / SSM Session Manager) | No | Yes (via ECS Exec) |
| **Build & CI/CD Pipeline** | CodeBuild / Beanstalk CLI | Built-in GitHub auto-deploy | External (GitHub Actions, CodePipeline)|
| **Custom Domains & TLS** | Manual Route 53 & ACM on ALB | Built-in 1-click custom domain | Manual Route 53 & ACM on ALB |
| **Pricing Model** | Underlying EC2 & ALB charges | Provisioned Memory + Active Compute | Per-second vCPU & Memory |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the architectural difference between Elastic Beanstalk and AWS App Runner?</strong></summary>

**Answer:**
- **Elastic Beanstalk**: An **infrastructure orchestrator**. When you deploy a Beanstalk environment, AWS creates real EC2 instances, Auto Scaling Groups, Application Load Balancers, and Security Groups directly inside your AWS account. You retain full administrative visibility and can SSH into the servers or modify the underlying resources.
- **AWS App Runner**: A true **managed serverless container platform**. App Runner provisions containers inside an AWS-managed service boundary. There are no EC2 instances, ALBs, or target groups in your account to configure or patch. You interact exclusively with the high-level service API, and App Runner handles routing, scaling, TLS termination, and deployments automatically.
</details>

<details>
<summary><strong>Q2: How does the Elastic Beanstalk Worker Tier process asynchronous background tasks?</strong></summary>

**Answer:**
The Worker Tier environment couples an EC2 Auto Scaling fleet with an Amazon SQS queue:
1. Producer applications publish messages to the SQS queue.
2. Inside each worker EC2 instance, Elastic Beanstalk runs a background daemon called **`sqsd`**.
3. `sqsd` polls the SQS queue for messages. When a message is retrieved, `sqsd` makes a local HTTP POST request to `http://localhost:80/` containing the message payload.
4. If the local application returns an HTTP `200 OK`, `sqsd` acknowledges and deletes the message from the SQS queue.
5. If the application returns a `4xx` or `5xx` error, `sqsd` allows the message to remain in the queue for retry according to the queue's visibility timeout and dead-letter policy.
</details>

<details>
<summary><strong>Q3: Why is the "Immutable" deployment policy considered the safest option for Elastic Beanstalk?</strong></summary>

**Answer:**
In an Immutable deployment:
1. Beanstalk creates an entirely new temporary Auto Scaling Group containing 1 instance running the new version.
2. It tests this instance behind the existing load balancer.
3. If health checks pass, Beanstalk scales up the new ASG to match the desired capacity of the production environment.
4. Once all new instances are healthy, Beanstalk shifts traffic, merges the instances into the primary ASG, and terminates the old ASG and its instances.
5. If anything fails at any point during testing, Beanstalk terminates the temporary ASG immediately, ensuring zero customer downtime and 100% clean rollback.
</details>

<details>
<summary><strong>Q4: How does AWS App Runner scale instances compared to Amazon ECS?</strong></summary>

**Answer:**
- **AWS App Runner**: Scales based on **in-flight concurrent HTTP requests**. You configure a threshold (e.g., `MaxConcurrency = 100`). If an active instance reaches 100 concurrent requests, App Runner provisions a new instance automatically. Scaling is request-driven and native to web traffic.
- **Amazon ECS**: Scales using CloudWatch metric alarms (e.g., average CPU utilization, memory utilization, or ALB `RequestCountPerTarget` across target groups). ECS scaling requires Application Auto Scaling policies, CloudWatch alarm evaluation periods, and cooldown windows.
</details>

<details>
<summary><strong>Q5: What is an App Runner VPC Connector and why is it needed?</strong></summary>

**Answer:**
By default, App Runner instances run inside an AWS-managed network environment with outbound egress directly to the public internet. If your application must query a private Amazon Aurora database, ElastiCache Redis cluster, or internal microservice located in a customer VPC, public internet routing is impossible.

An **App Runner VPC Connector** establishes an AWS Hyperplane network tunnel between the managed App Runner containers and private subnets inside your VPC. This allows App Runner instances to communicate privately with internal VPC resources using standard private IP addresses.
</details>

<details>
<summary><strong>Q6: How do `.ebextensions` differ from `.platform` hooks in Elastic Beanstalk?</strong></summary>

**Answer:**
- **`.ebextensions`**: The legacy configuration system. Uses YAML or JSON files located in `.ebextensions/` to configure AWS resources (via CloudFormation snippets), install Linux packages, manage users, and write arbitrary files during initial server provisioning.
- **`.platform`**: The modern platform hook architecture for Amazon Linux 2 and Amazon Linux 2023 platforms. It organizes executable shell scripts into structured lifecycle folders (`confighooks/prebuild`, `confighooks/predeploy`, `confighooks/postdeploy`), giving engineers explicit control over exact execution timing during deployments without dealing with CloudFormation syntax.
</details>

<details>
<summary><strong>Q7: How does App Runner pricing handle idle instances?</strong></summary>

**Answer:**
App Runner separates billing for active compute versus idle provisioned instances:
- When an instance is actively serving traffic, you are billed for both **vCPU** ($0.064/vCPU-hour) and **memory** ($0.007/GB-hour).
- When traffic drops to zero and an instance is idle (maintaining pre-warmed capacity to avoid cold starts), the CPU is throttled and billed at **$0.00**. You only pay for the provisioned **memory** ($0.007/GB-hour), making idle standby capacity extremely affordable (~$5/month for 1 GB RAM).
</details>

<details>
<summary><strong>Q8: When should an enterprise migrate from Elastic Beanstalk to Amazon ECS or EKS?</strong></summary>

**Answer:**
Migrate from Elastic Beanstalk to ECS/EKS when:
1. The architecture transitions from monolithic web apps to multi-service container meshes requiring complex service discovery (ECS Service Connect or Istio).
2. The team requires container bin-packing to maximize host utilization across hundreds of microservices.
3. Advanced deployment topologies (canaries, traffic shadowing, blue/green with AWS CodeDeploy) are required.
4. Kubernetes open-source tooling, CRDs, and multi-cloud portability become organizational requirements.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Deploy a Public App Runner Service via AWS CLI</strong></summary>

**Objective**: Deploy a sample containerized Python service from AWS Public ECR.

**Command**:
```bash
aws apprunner create-service \
  --service-name demo-web-app \
  --source-configuration '{
    "ImageRepository": {
      "ImageIdentifier": "public.ecr.aws/aws-containers/hello-app-runner:latest",
      "ImageConfiguration": {"Port": "8000"},
      "ImageRepositoryType": "ECR_PUBLIC"
    }
  }'
```
</details>

<details>
<summary><strong>Exercise 2: Create an App Runner VPC Connector</strong></summary>

**Objective**: Provision a private network connector to reach VPC databases.

**Command**:
```bash
aws apprunner create-vpc-connector \
  --vpc-connector-name "prod-db-connector" \
  --subnets "subnet-01a" "subnet-01b" \
  --security-groups "sg-0123456789abcdef0"
```
</details>

<details>
<summary><strong>Exercise 3: Configure Elastic Beanstalk Rolling with Additional Batch Deployment</strong></summary>

**Objective**: Configure deployment settings via the AWS CLI.

**Command**:
```bash
aws elasticbeanstalk update-environment \
  --environment-name "prod-web-env" \
  --option-settings \
    Namespace=aws:elasticbeanstalk:command,OptionName=DeploymentPolicy,Value=RollingWithAdditionalBatch \
    Namespace=aws:elasticbeanstalk:command,OptionName=BatchSizeType,Value=Percentage \
    Namespace=aws:elasticbeanstalk:command,OptionName=BatchSize,Value=50
```
</details>

<details>
<summary><strong>Exercise 4: Associate a Custom Domain with App Runner</strong></summary>

**Objective**: Map `api.example.com` to your App Runner service.

**Command**:
```bash
aws apprunner associate-custom-domain \
  --service-arn "<SERVICE_ARN>" \
  --domain-name "api.example.com"
```
</details>

<details>
<summary><strong>Exercise 5: Inspect Elastic Beanstalk Health via EB CLI</strong></summary>

**Objective**: Retrieve real-time instance health and request rates.

**Command**:
```bash
eb health --refresh
```
</details>

<details>
<summary><strong>Exercise 6: Test App Runner Concurrency Autoscaling Under Load</strong></summary>

**Objective**: Run ApacheBench against an App Runner service and observe automated scale-out.

**Steps**:
1. Run load test: `ab -n 10000 -c 150 https://<app-runner-url>/`.
2. Inspect CloudWatch metrics: `ActiveInstances` and `Concurrency`.
3. Verify that App Runner provisions additional instances when concurrent requests exceed the target threshold.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                               PaaS Platform Decision Guide                            |
+---------------------------------------------------------------------------------------+
|  Criteria               | Elastic Beanstalk             | AWS App Runner              |
|-------------------------+-------------------------------+-----------------------------|
|  Target Workload        | Legacy monoliths / Worker tier| Modern containerized APIs   |
|  Infrastructure Control | Full OS & EC2 access          | Zero server access (Managed)|
|  Deployment Policy      | Immutable, Rolling, Splitting | Rolling container swap      |
|  Scaling Trigger        | CPU / ALB Request count       | In-flight HTTP Concurrency  |
|  VPC Integration        | Native (runs inside your VPC) | Via VPC Connector (Tunnel)  |
|  Operational Burden     | Medium (OS updates, ALBs)     | Near Zero (Platform handled)|
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [AWS App Runner Developer Guide](https://docs.aws.amazon.com/apprunner/latest/dg/what-is-apprunner.html)
- [AWS Elastic Beanstalk Developer Guide](https://docs.aws.amazon.com/elasticbeanstalk/latest/dg/Welcome.html)
- [Deep Dive on AWS App Runner VPC Networking](https://aws.amazon.com/blogs/containers/deep-dive-on-aws-app-runner-vpc-networking/)
- [Elastic Beanstalk Deployment Policies and Best Practices](https://docs.aws.amazon.com/elasticbeanstalk/latest/dg/using-features.rolling-version-deploy.html)
- [App Runner Auto Scaling Configuration](https://docs.aws.amazon.com/apprunner/latest/dg/manage-autoscaling.html)
