# Amazon ECS & AWS Fargate Fundamentals

> **Module**: `02-compute`  
> **Topic**: Container Orchestration & Serverless Container Infrastructure  
> **Target Audience**: DevOps Engineers, Platform Engineers, Cloud Architects  
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

Running Docker containers directly on bare virtual machines introduces severe fleet-management overhead: deploying container updates requires manual host orchestration, port collisions prevent co-locating multiple services on a single server, and host hardware failures require manual container restarts.

**Amazon Elastic Container Service (Amazon ECS)** is a fully managed container orchestration service providing high-density container scheduling, service self-healing, integration with AWS Application Load Balancers, and centralized IAM access control.

With ECS, teams can choose between two fundamental compute backends:
1. **EC2 Launch Type**: You manage a fleet of EC2 instances registered to an ECS cluster. Offers maximum control over instance types, local NVMe storage, and custom AMI operating system kernels.
2. **AWS Fargate Launch Type**: The **serverless** container engine for ECS. AWS completely manages the underlying virtual machine infrastructure. You declare the container's required CPU and memory, and Fargate launches the container in an isolated single-tenant sandbox within seconds, billing purely for the vCPU and memory consumed while running.

---

## 2. Core Concepts

### 2.1 ECS Architectural Building Blocks
- **Cluster**: A logical grouping of tasks or services. Clusters can combine both EC2 capacity providers and Fargate capacity providers.
- **Task Definition**: The immutable blueprint (declared in JSON/YAML) defining one or more containers that run together. Specifies container images, CPU/RAM limits, port mappings, environment variables, logging drivers, and storage mounts.
- **Task**: A running instance of a Task Definition. Ephemeral; once a task exits, it is not automatically restarted unless governed by a Service.
- **Service**: Maintains a specified number of healthy task instances (`desired_count`) across multiple Availability Zones. Handles Application Load Balancer target registration, rolling blue/green deployments, auto-scaling, and task self-healing.

### 2.2 Task Role vs. Task Execution Role
A crucial security boundary in Amazon ECS:
- **Task Execution Role (`executionRoleArn`)**: Used by the **ECS container agent and infrastructure** *before* the container starts. Authorizes the agent to:
  - Pull private container images from Amazon ECR.
  - Decrypt and inject secrets from AWS Secrets Manager or SSM Parameter Store.
  - Stream `stdout`/`stderr` logs to Amazon CloudWatch Logs.
- **Task Role (`taskRoleArn`)**: Used by the **application running inside the container** *after* it boots. Authorizes application code to call AWS APIs (e.g., querying Amazon DynamoDB, reading from S3, publishing to SNS).

### 2.3 Capacity Providers
Capacity Providers decouple task scheduling from underlying infrastructure provisioning:
- **`FARGATE`**: Allocates on-demand serverless container capacity.
- **`FARGATE_SPOT`**: Allocates spare AWS compute capacity at up to 70% discount (subject to 2-minute interruption notices).
- **Auto Scaling Group (ASG) Capacity Provider**: Manages EC2 instance fleets dynamically with **ECS Cluster Auto Scaling (CAS)**, automatically launching EC2 instances when pending tasks cannot be placed due to insufficient CPU/memory.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Amazon ECS Architectural Topology                          |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ AWS Application Load Balancer (ALB) ]                                          |
|            |                                                                      |
|            +-------------------------------+------------------------------+       |
|            | Routes to Port 8080           | Routes to Port 8080          |       |
|            v                               v                              v       |
|  +-----------------------------------------------------------------------------+  |
|  |                           Amazon ECS Cluster                                |  |
|  |                                                                             |  |
|  |  +-------------------------------------+  +------------------------------+  |  |
|  |  |       ECS Service (Desired: 2)      |  | Capacity Provider: FARGATE   |  |  |
|  |  |       - Target Group Health Checked |  | Base: 1, Weight: 100         |  |  |
|  |  +-------------------------------------+  +------------------------------+  |  |
|  |                     |                                   |                   |  |
|  |          +----------+----------+                        v                   |  |
|  |          |                     |             +---------------------+        |  |
|  |          v                     v             | FARGATE_SPOT (Base 0|        |  |
|  |   +-------------+       +-------------+      | Weight: 200)        |        |  |
|  |   | ECS Task A  |       | ECS Task B  |      +---------------------+        |  |
|  |   | (Subnet 1a) |       | (Subnet 1b) |                                     |  |
|  |   +-------------+       +-------------+                                     |  |
|  |          |                     |                                            |  |
|  +----------|---------------------|--------------------------------------------+  |
|             v                     v                                               |
|  +-----------------------------------------------------------------------------+  |
|  |                    Underlying AWS Fargate Infrastructure                    |  |
|  |                                                                             |  |
|  |  +------------------------------+     +----------------------------------+  |  |
|  |  | Single-Tenant MicroVM (1a)   |     | Single-Tenant MicroVM (1b)       |  |  |
|  |  | - Dedicated awsvpc ENI       |     | - Dedicated awsvpc ENI           |  |  |
|  |  | - App Container (Port 8080)   |     | - App Container (Port 8080)       |  |  |
|  |  | - Log Driver to CloudWatch   |     | - Log Driver to CloudWatch       |  |  |
|  |  +------------------------------+     +----------------------------------+  |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

### ECS Task Launch Progression
```
[Task Scheduled]
       |
       v
[ECS Agent / Fargate Host] ---> (Assumes Task Execution Role)
       |
       +---> Pulls Docker Image from Amazon ECR
       +---> Resolves and Decrypts Secrets from Secrets Manager
       +---> Creates dedicated awsvpc Elastic Network Interface (ENI)
       |
       v
[Container Boots] ------------> (Assumes Task Role)
       |
       +---> Application starts listening on configured port (e.g. 8080)
       +---> AWS ALB registers ENI private IP as target
       +---> Health checks pass -> Task enters RUNNING state
```

---

## 4. Syntax & Usage

### 4.1 Production ECS Task Definition (JSON)
```json
{
  "family": "production-api-task",
  "networkMode": "awsvpc",
  "requiresCompatibilities": ["FARGATE"],
  "cpu": "1024",
  "memory": "2048",
  "executionRoleArn": "arn:aws:iam::111122223333:role/ecsTaskExecutionRole",
  "taskRoleArn": "arn:aws:iam::111122223333:role/apiApplicationTaskRole",
  "containerDefinitions": [
    {
      "name": "api-container",
      "image": "111122223333.dkr.ecr.us-east-1.amazonaws.com/api-service:v1.2.0",
      "essential": true,
      "portMappings": [
        {
          "containerPort": 8080,
          "hostPort": 8080,
          "protocol": "tcp"
        }
      ],
      "environment": [
        {"name": "NODE_ENV", "value": "production"},
        {"name": "PORT", "value": "8080"}
      ],
      "secrets": [
        {
          "name": "DATABASE_URL",
          "valueFrom": "arn:aws:secretsmanager:us-east-1:111122223333:secret:prod/db/url-abc123"
        }
      ],
      "logConfiguration": {
        "logDriver": "awslogs",
        "options": {
          "awslogs-group": "/ecs/production-api",
          "awslogs-region": "us-east-1",
          "awslogs-stream-prefix": "api"
        }
      },
      "healthCheck": {
        "command": ["CMD-SHELL", "curl -f http://localhost:8080/healthz || exit 1"],
        "interval": 30,
        "timeout": 5,
        "retries": 3,
        "startPeriod": 60
      }
    }
  ]
}
```

### 4.2 Registering Task Definition & Creating Service via CLI
```bash
# 1. Register Task Definition
TASK_DEF_ARN=$(aws ecs register-task-definition \
  --cli-input-json file://task-def.json \
  --query 'taskDefinition.taskDefinitionArn' \
  --output text)

echo "Registered: $TASK_DEF_ARN"

# 2. Create Fargate Service behind ALB
aws ecs create-service \
  --cluster production-cluster \
  --service-name api-service \
  --task-definition "$TASK_DEF_ARN" \
  --desired-count 2 \
  --launch-type FARGATE \
  --platform-version LATEST \
  --network-configuration '{
    "awsvpcConfiguration": {
      "subnets": ["subnet-01a", "subnet-01b"],
      "securityGroups": ["sg-0123456789abcdef0"],
      "assignPublicIp": "DISABLED"
    }
  }' \
  --load-balancers '{
    "targetGroupArn": "arn:aws:elasticloadbalancing:us-east-1:111122223333:targetgroup/api-tg/12345",
    "containerName": "api-container",
    "containerPort": 8080
  }' \
  --health-check-grace-period-seconds 120
```

---

## 5. Practical Examples

### Example 1: Terraform Declarative ECS Fargate Service with Load Balancer
Complete production setup with automated CloudWatch log groups, security groups, and ALB target group integration.

```hcl
resource "aws_ecs_cluster" "main" {
  name = "production-ecs-cluster"

  setting {
    name  = "containerInsights"
    value = "enabled"
  }
}

resource "aws_cloudwatch_log_group" "ecs_logs" {
  name              = "/ecs/prod-backend-api"
  retention_in_days = 30
}

resource "aws_ecs_task_definition" "app" {
  family                   = "backend-api"
  network_mode             = "awsvpc"
  requires_compatibilities = ["FARGATE"]
  cpu                      = "512"
  memory                   = "1024"
  execution_role_arn       = var.ecs_execution_role_arn
  task_role_arn            = var.ecs_task_role_arn

  container_definitions = jsonencode([
    {
      name      = "backend-container"
      image     = "${var.ecr_repository_url}:latest"
      essential = true
      portMappings = [
        {
          containerPort = 8000
          hostPort      = 8000
        }
      ]
      logConfiguration = {
        logDriver = "awslogs"
        options = {
          "awslogs-group"         = aws_cloudwatch_log_group.ecs_logs.name
          "awslogs-region"        = "us-east-1"
          "awslogs-stream-prefix" = "backend"
        }
      }
    }
  ])
}

resource "aws_ecs_service" "app_service" {
  name            = "backend-api-service"
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
    weight            = 2
  }

  network_configuration {
    subnets          = var.private_subnet_ids
    security_groups  = [var.ecs_tasks_security_group_id]
    assign_public_ip = false
  }

  load_balancer {
    target_group_arn = var.alb_target_group_arn
    container_name   = "backend-container"
    containerPort    = 8000
  }

  deployment_circuit_breaker {
    enable   = true
    rollback = true
  }
}
```

### Example 2: Interactive Troubleshooting with ECS Exec
Connect an interactive bash shell directly into a running Fargate container without running SSH daemons.

```bash
# 1. Enable ECS Exec on the service (requires SSM permissions on Task Role)
aws ecs update-service \
  --cluster production-ecs-cluster \
  --service backend-api-service \
  --enable-execute-command

# 2. Identify a running task ID
TASK_ID=$(aws ecs list-tasks \
  --cluster production-ecs-cluster \
  --service-name backend-api-service \
  --query 'taskArns[0]' \
  --output text)

# 3. Start interactive shell session inside the container
aws ecs execute-command \
  --cluster production-ecs-cluster \
  --task "$TASK_ID" \
  --container backend-container \
  --interactive \
  --command "/bin/sh"
```

### Example 3: Running a Standalone One-Off Database Migration Task
Run an isolated database migration task that exits immediately upon completion.

```bash
aws ecs run-task \
  --cluster production-ecs-cluster \
  --task-definition backend-api:5 \
  --launch-type FARGATE \
  --network-configuration '{
    "awsvpcConfiguration": {
      "subnets": ["subnet-01a"],
      "securityGroups": ["sg-0123456789abcdef0"],
      "assignPublicIp": "DISABLED"
    }
  }' \
  --overrides '{
    "containerOverrides": [
      {
        "name": "backend-container",
        "command": ["npm", "run", "db:migrate"]
      }
    ]
  }'
```

### Example 4: Configuring Service Auto Scaling on CPU Utilization
Configure target tracking auto-scaling to keep average service CPU utilization at 70%.

```bash
# 1. Register ECS Service as a scalable target
aws application-autoscaling register-scalable-target \
  --service-namespace ecs \
  --resource-id service/production-ecs-cluster/backend-api-service \
  --scalable-dimension ecs:service:DesiredCount \
  --min-capacity 2 \
  --max-capacity 10

# 2. Attach target tracking policy
aws application-autoscaling put-scaling-policy \
  --service-namespace ecs \
  --resource-id service/production-ecs-cluster/backend-api-service \
  --scalable-dimension ecs:service:DesiredCount \
  --policy-name ecs-cpu-target-tracking \
  --policy-type TargetTrackingScaling \
  --target-tracking-scaling-policy-configuration '{
    "TargetValue": 70.0,
    "PredefinedMetricSpecification": {
      "PredefinedMetricType": "ECSServiceAverageCPUUtilization"
    },
    "ScaleInCooldown": 300,
    "ScaleOutCooldown": 60
  }'
```

### Example 5: Configuring Deployment Circuit Breakers with Automated Rollback
Protect production deployments from stalling indefinitely when a bad container fails to boot.

```bash
aws ecs update-service \
  --cluster production-ecs-cluster \
  --service backend-api-service \
  --deployment-configuration '{
    "deploymentCircuitBreaker": {
      "enable": true,
      "rollback": true
    },
    "maximumPercent": 200,
    "minimumHealthyPercent": 100
  }'
```

---

## 6. Best Practices

1. **Always Use `awsvpc` Network Mode**: `awsvpc` assigns every task its own dedicated Elastic Network Interface (ENI) and private IP. This allows tasks to have independent Security Groups and eliminates host port collisions.
2. **Enable Deployment Circuit Breaker with Rollback**: Without a circuit breaker, if a bad image or database configuration causes container crashes, ECS retries task launches indefinitely and leaves the deployment hung. The circuit breaker detects failing tasks and automatically rolls back to the previous stable Task Definition.
3. **Combine Fargate and Fargate Spot**: In your Capacity Provider Strategy, configure `FARGATE` with `base: 1` or `base: 2` (to ensure core instances never get interrupted), and route remaining elastic scaling traffic to `FARGATE_SPOT` (`weight: 2` or `3`) to cut compute bills by up to 70%.
4. **Use ECS Exec Instead of SSH Daemons**: Never install `sshd` or bake SSH keys inside Docker images. Enable `execute-command` and use the AWS Systems Manager Session Manager tunnel to inspect running containers securely.
5. **Decouple Task Role and Execution Role**: Never share a single IAM role for both roles. Task Execution Role requires infrastructure privileges (`ecr:GetAuthorizationToken`, `secretsmanager:GetSecretValue`), while Task Role should strictly contain application-level business permissions (`dynamodb:PutItem`, `s3:GetObject`).

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Confusing Task Role with Execution Role** | Image pull errors or permission denied errors accessing Secrets Manager during task provisioning. | Use Execution Role for ECR/Secrets/CloudWatch; use Task Role for internal application AWS SDK calls. |
| **Omitting CloudWatch Log Driver** | Container crashes silently on startup with no logs retained anywhere for debugging. | Always configure `awslogs` log driver in container definitions pointing to an active Log Group. |
| **Missing `assignPublicIp: DISABLED` in Private Subnets** | Tasks in private subnets cannot pull images or fetch secrets if routed through misconfigured public NAT paths. | Set `assignPublicIp = DISABLED` in private subnets and verify routing table leads to NAT Gateway or VPC Endpoints. |
| **Setting Health Check Grace Period Too Low** | Tasks are killed by the ALB target group as unhealthy before the application finish booting. | Configure `healthCheckGracePeriodSeconds` (e.g., 60–120s) to allow app initialization before ALB health checks take effect. |
| **Using Docker Host Mode on Shared EC2** | Port collisions occur when launching multiple tasks listening on the same port on a single EC2 host. | Use `awsvpc` or dynamic host port mapping (`hostPort: 0`) behind an ALB. |
| **Hardcoding Plaintext Passwords in Environment** | Sensitive credentials visible to anyone with `ecs:DescribeTaskDefinition` console access. | Inject credentials securely using the `secrets` attribute referencing Secrets Manager or SSM Parameter Store. |

---

## 8. Performance, Security & Cost Notes

### Fargate Resource Allocation Matrix
Fargate enforces strict, predefined combinations of vCPU and RAM:
- 0.25 vCPU: 512 MB, 1 GB, 2 GB
- 0.5 vCPU: 1 GB, 2 GB, 3 GB, 4 GB
- 1 vCPU: 2 GB to 8 GB (1 GB increments)
- 2 vCPU: 4 GB to 16 GB (1 GB increments)
- 4 vCPU: 8 GB to 30 GB (1 GB increments)
- 8 vCPU: 16 GB to 60 GB (4 GB increments)
- 16 vCPU: 32 GB to 120 GB (8 GB increments)

### Security Boundaries
- **Hypervisor-Level Isolation**: AWS Fargate runs each task inside an isolated, dedicated single-tenant virtual machine. Unlike multi-tenant Kubernetes or Docker daemons where multiple containers share a single Linux kernel, Fargate tasks cannot execute kernel privilege escalation attacks against neighboring tasks.

### Cost Breakdown
- **Fargate Pricing**: Billed per second (1-minute minimum).
  - ~$0.04048 per vCPU-hour
  - ~$0.004445 per GB-hour
- **Fargate Spot**: Up to 70% discount over standard Fargate rates.

---

## 9. Comparison Table

| Dimension | ECS on EC2 | ECS on AWS Fargate | Amazon EKS (Kubernetes) |
|:---|:---|:---|:---|
| **Server Management** | Customer manages VMs / OS patching | AWS manages 100% of infrastructure | Customer or managed node groups |
| **Container Density** | High (bin-packing multiple tasks) | 1 task per dedicated microVM | High (Kubernetes bin-packing) |
| **Startup Latency** | 5–15 seconds (host pre-warmed) | 20–45 seconds (cold task launch) | 5–15 seconds on pre-warmed nodes |
| **Pricing Model** | EC2 hourly rate (regardless of load)| Per vCPU / memory per second | Node cost + $0.10/hr control plane |
| **Ecosystem & API** | AWS Proprietary / Simple API | AWS Proprietary / Simple API | CNCF Standard Kubernetes API / Helm |
| **Local Disk Storage** | Large EBS volumes / NVMe instance store| Ephemeral disk (20GB free, up to 200GB)| Persistent Volumes (EBS, EFS, CSI) |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the exact difference between an ECS Task Execution Role and an ECS Task Role?</strong></summary>

**Answer:**
- **Task Execution Role (`executionRoleArn`)**: Grants permissions to the **AWS ECS infrastructure and container agent** to execute actions *prior to* and *during* container bootstrap. These actions include calling `ecr:GetAuthorizationToken` and `ecr:BatchGetImage` to pull private images, calling `secretsmanager:GetSecretValue` to decrypt environment secrets, and calling `logs:CreateLogStream`/`logs:PutLogEvents` to push container logs to CloudWatch.
- **Task Role (`taskRoleArn`)**: Grants permissions to the **application process running inside the container**. Once the container is running, any AWS SDK call executed by the application code (e.g., querying DynamoDB, writing to S3) assumes this role via the ECS task metadata endpoint.
</details>

<details>
<summary><strong>Q2: How does the ECS Deployment Circuit Breaker prevent broken deployments?</strong></summary>

**Answer:**
In a standard ECS rolling deployment, when a new Task Definition version is deployed, ECS launches replacement tasks and waits for them to reach a healthy status. If the new containers contain a crash loop, missing environment variable, or failed database migration, they repeatedly crash. Without a circuit breaker, ECS continues re-launching failed tasks indefinitely, leaving the deployment in an incomplete state.

With `deploymentCircuitBreaker` enabled:
1. ECS monitors task launch failure thresholds.
2. If consecutive tasks repeatedly fail to enter the `RUNNING` state or fail health checks, the circuit breaker trips and marks the deployment as **FAILED**.
3. If `rollback: true` is configured, ECS automatically terminates the failing tasks and transitions the service back to the last known healthy Task Definition version without human intervention.
</details>

<details>
<summary><strong>Q3: Why is the `awsvpc` network mode mandatory for AWS Fargate?</strong></summary>

**Answer:**
AWS Fargate enforces `awsvpc` because it isolates every task into its own single-tenant virtualized sandbox. Under `awsvpc`, AWS attaches a dedicated Elastic Network Interface (ENI) directly to each task.
This design guarantees:
- Every task receives a unique private IPv4 address within the VPC subnet.
- Security Groups are applied at the individual task level rather than at the underlying shared host level.
- Network traffic between tasks traverses standard VPC routing tables and VPC Flow Logs.
- Port collisions are eliminated; multiple tasks on the same host can listen on identical ports (e.g., port 80 or 8080) because each task possesses an independent network namespace and IP.
</details>

<details>
<summary><strong>Q4: How do Capacity Providers manage mixed Fargate and Fargate Spot workloads?</strong></summary>

**Answer:**
An ECS cluster uses a **Capacity Provider Strategy** composed of `base` and `weight` attributes across multiple capacity providers:
- **`base`**: Specifies the absolute minimum number of tasks that must run on that capacity provider before other providers receive traffic. For example, setting `FARGATE` with `base: 2` ensures at least 2 tasks run on non-interruptible on-demand infrastructure.
- **`weight`**: Determines the relative ratio of tasks allocated once the base capacity is met. If `FARGATE` has `weight: 1` and `FARGATE_SPOT` has `weight: 3`, then for every 4 tasks scaled above the base, 1 task is placed on Fargate and 3 tasks are placed on Fargate Spot.
</details>

<details>
<summary><strong>Q5: How does AWS ECS handle dynamic port mapping behind an Application Load Balancer?</strong></summary>

**Answer:**
When using the EC2 launch type with `bridge` network mode:
- In the task definition, you set `containerPort: 8080` and `hostPort: 0` (ephemeral port).
- The Docker daemon assigns a random high-order port (e.g., 32768–65535) on the EC2 host when the container launches.
- The ECS agent automatically registers this dynamic host port and the EC2 instance's IP with the ALB Target Group.
- The ALB distributes traffic directly to the correct ephemeral port on the host, allowing multiple containers of the same service to run on a single EC2 instance without port conflicts.
*(Note: In `awsvpc` mode, dynamic port mapping is unnecessary because every task has its own IP).*
</details>

<details>
<summary><strong>Q6: What happens when an AWS Fargate Spot task receives an interruption notice?</strong></summary>

**Answer:**
When AWS reclaims capacity, Fargate Spot sends a **2-minute warning notification** via Amazon EventBridge (`ECS Task State Change` event with `stopCode: SpotInterruption`).
Inside the container:
1. The container receives a `SIGTERM` signal.
2. The application has 120 seconds to finish processing in-flight HTTP requests, drain database transactions, and flush logs.
3. If the container does not exit before the deadline, Fargate sends a `SIGKILL`.
4. The ECS Service scheduler automatically detects the lost capacity and attempts to provision a replacement task.
</details>

<details>
<summary><strong>Q7: How do you safely inject database credentials into an ECS container?</strong></summary>

**Answer:**
Never embed plaintext credentials in Task Definition `environment` variables or Dockerfiles.
Instead:
1. Store credentials in **AWS Secrets Manager** or **AWS Systems Manager Parameter Store (SecureString)**.
2. In the container definition, use the `secrets` array specifying the target environment variable name and the secret ARN.
3. Grant the ECS **Task Execution Role** permissions to call `secretsmanager:GetSecretValue` and `kms:Decrypt`.
4. During container launch, the ECS container agent securely resolves the secret and injects it directly into the container's environment variables in memory.
</details>

<details>
<summary><strong>Q8: What is ECS Exec and how does it securely replace SSH?</strong></summary>

**Answer:**
ECS Exec allows engineers to execute interactive commands or open a shell inside a running container without opening inbound ports or running an SSH daemon.
Under the hood:
- The ECS container agent launches the **AWS Systems Manager (SSM) agent** binary inside the container namespace.
- The SSM agent establishes an outbound encrypted TLS connection to the AWS SSM service endpoint.
- Clients connect via the `aws ecs execute-command` CLI tool.
- Access is controlled via IAM permissions (`ecs:ExecuteCommand` and `ssm:StartSession`), and all interactive session keystrokes can be audited and logged to CloudWatch Logs or Amazon S3.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Register an ECS Task Definition with CloudWatch Logs via CLI</strong></summary>

**Objective**: Create a JSON task definition and register it with the ECS API.

**Steps**:
```bash
cat <<'EOF' > task-def.json
{
  "family": "nginx-fargate",
  "networkMode": "awsvpc",
  "requiresCompatibilities": ["FARGATE"],
  "cpu": "256",
  "memory": "512",
  "executionRoleArn": "arn:aws:iam::111122223333:role/ecsTaskExecutionRole",
  "containerDefinitions": [{
    "name": "web",
    "image": "public.ecr.aws/nginx/nginx:latest",
    "essential": true,
    "portMappings": [{"containerPort": 80, "hostPort": 80}],
    "logConfiguration": {
      "logDriver": "awslogs",
      "options": {
        "awslogs-group": "/ecs/nginx-demo",
        "awslogs-region": "us-east-1",
        "awslogs-stream-prefix": "web"
      }
    }
  }]
}
EOF

aws ecs register-task-definition --cli-input-json file://task-def.json
```
</details>

<details>
<summary><strong>Exercise 2: Launch a Fargate Task into a Private Subnet</strong></summary>

**Objective**: Run an ad-hoc container task without creating an ECS service.

**Command**:
```bash
aws ecs run-task \
  --cluster test-cluster \
  --task-definition nginx-fargate:1 \
  --launch-type FARGATE \
  --network-configuration '{
    "awsvpcConfiguration": {
      "subnets": ["subnet-0123456789abcdef0"],
      "securityGroups": ["sg-0123456789abcdef0"],
      "assignPublicIp": "DISABLED"
    }
  }'
```
</details>

<details>
<summary><strong>Exercise 3: Test ECS Exec Interactive Shell Connection</strong></summary>

**Objective**: Connect an interactive terminal session into a running container.

**Command**:
```bash
aws ecs execute-command \
  --cluster test-cluster \
  --task <TASK_ARN> \
  --container web \
  --interactive \
  --command "/bin/sh"
```
</details>

<details>
<summary><strong>Exercise 4: Configure an ECS Service with Deployment Circuit Breaker</strong></summary>

**Objective**: Enable automated rollback protection on an existing ECS service.

**Command**:
```bash
aws ecs update-service \
  --cluster test-cluster \
  --service web-service \
  --deployment-configuration '{"deploymentCircuitBreaker": {"enable": true, "rollback": true}}'
```
</details>

<details>
<summary><strong>Exercise 5: Configure Fargate Spot Capacity Provider Strategy</strong></summary>

**Objective**: Shift 75% of non-base workload to Fargate Spot.

**Command**:
```bash
aws ecs update-service \
  --cluster test-cluster \
  --service web-service \
  --capacity-provider-strategy \
    capacityProvider=FARGATE,weight=1,base=1 \
    capacityProvider=FARGATE_SPOT,weight=3
```
</details>

<details>
<summary><strong>Exercise 6: Inspect Container Lifecycle Events via EventBridge</strong></summary>

**Objective**: Stream task state change events (`RUNNING`, `STOPPED`) to CloudWatch Logs to audit container termination reasons.

**Steps**:
1. Create EventBridge rule matching `source: ["aws.ecs"]` and `detail-type: ["ECS Task State Change"]`.
2. Set target to CloudWatch Log Group `/aws/events/ecs-lifecycle`.
3. Stop an active task and verify the JSON record capturing `stopCode` and `exitCode`.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                                ECS & Fargate Quick Reference                          |
+---------------------------------------------------------------------------------------+
|  Role / Component      | Purpose                                                      |
|------------------------+--------------------------------------------------------------|
|  Task Execution Role   | Infrastructure permissions (ECR pull, CloudWatch, Secrets)  |
|  Task Role             | Application code AWS permissions (S3, DynamoDB, SNS)         |
|  Network Mode (awsvpc) | Dedicated ENI & IP per task; task-level Security Groups      |
|  Deployment Circuit Br | Detects crash loops and triggers auto-rollback               |
|  ECS Exec              | Interactive container shell via SSM without SSH or open ports|
|  Fargate Spot          | Up to 70% discount with 2-minute interruption warning        |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Amazon Elastic Container Service Developer Guide](https://docs.aws.amazon.com/AmazonECS/latest/developerguide/Welcome.html)
- [AWS Fargate User Guide for Amazon ECS](https://docs.aws.amazon.com/AmazonECS/latest/userguide/what-is-fargate.html)
- [Using Amazon ECS Exec for Debugging](https://docs.aws.amazon.com/AmazonECS/latest/developerguide/ecs-exec.html)
- [Amazon ECS Deployment Circuit Breaker Documentation](https://docs.aws.amazon.com/AmazonECS/latest/developerguide/deployment-circuit-breaker.html)
- [Architecting Containers on AWS Best Practices Guide](https://aws.amazon.com/architecture/well-architected/)
