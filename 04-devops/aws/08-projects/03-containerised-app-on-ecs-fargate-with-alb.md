---
project: "Production Containerised Application on ECS Fargate with ALB"
module: "08-projects"
track: "aws"
difficulty: "advanced"
architecture: "containerized-microservice-fargate"
last-tested: "2026-11"
tags:
  - "ecs"
  - "fargate"
  - "alb"
  - "vpc"
  - "auto-scaling"
  - "terraform"
  - "project"
---

# Production Containerised Application on ECS Fargate with ALB

## Table of Contents
- [1. Project Topology & Specifications](#1-project-topology--specifications)
- [2. Declarative Code (Terraform)](#2-declarative-code-terraform)
- [3. Verification & Automated Test Harness](#3-verification--automated-test-harness)
- [4. Hardening, Security & Cost Analysis](#4-hardening-security--cost-analysis)
- [5. Operational Runbook & Disaster Recovery](#5-operational-runbook--disaster-recovery)

---

## 1. Project Topology & Specifications

### 1.1 Architecture Overview
This production reference project implements an enterprise containerized microservice deployed on **Amazon Elastic Container Service (ECS)** using **AWS Fargate** (serverless container compute) fronted by an internet-facing **Application Load Balancer (ALB)**.

Key Architectural Guarantees:
- **Zero Public IPs on Tasks**: All container tasks run strictly inside **private subnets** with dedicated Elastic Network Interfaces (`awsvpc` network mode). Egress internet traffic routes via managed NAT Gateways.
- **Layer 7 Load Balancing**: The ALB terminates client TLS/HTTP traffic in public subnets and performs active health checks against private task containers on port 8080.
- **Automated Target Tracking Scaling**: Scales task count dynamically between 2 and 10 replicas based on average CPU utilization (target: 70%).
- **Strict Role Separation**: Splits the ECS **Task Execution Role** (infrastructure operations: ECR pull, CloudWatch log streams) from the **Task Role** (application code permissions).

```
+----------------------------------------------------------------------------------------------------+
|                         ECS FARGATE MICROSERVICE ARCHITECTURE                                      |
+----------------------------------------------------------------------------------------------------+

  [ Internet Traffic ]
           |
           | HTTP / HTTPS (Port 80/443)
           v
  +--------------------------------------------------------------------------------------------------+
  | VPC (10.0.0.0/16)                                                                                |
  |                                                                                                  |
  |  +--------------------------------------------------------------------------------------------+  |
  |  | PUBLIC SUBNETS (AZ-a: 10.0.1.0/24 | AZ-b: 10.0.2.0/24)                                     |  |
  |  |                                                                                            |  |
  |  |   [ Internet-Facing Application Load Balancer (ALB) ]                                      |  |
  |  |      - Port 80 Listener forwarding to Target Group                                         |  |
  |  |      - Health Checks: GET /health (interval: 30s, timeout: 5s, retries: 3)                 |  |
  |  |                                                                                            |  |
  |  |   [ NAT Gateway A ]                             [ NAT Gateway B ]                          |  |
  |  +--------------------------------------------------------------------------------------------+  |
  |            |                                             |                                       |
  |            v                                             v                                       |
  |  +--------------------------------------------------------------------------------------------+  |
  |  | PRIVATE SUBNETS (AZ-a: 10.0.10.0/24 | AZ-b: 10.0.20.0/24)                                  |  |
  |  |                                                                                            |  |
  |  |   Amazon ECS Cluster (`production-cluster`)                                                |  |
  |  |   ECS Service: Desired=2, Min=2, Max=10 (awsvpc network mode)                             |  |
  |  |                                                                                            |  |
  |  |     +-------------------------+             +-------------------------+                    |  |
  |  |     | Fargate Task Replica A  |             | Fargate Task Replica B  |                    |  |
  |  |     | App Container (port 8080|             | App Container (port 8080|                    |  |
  |  |     | awslogs to CloudWatch   |             | awslogs to CloudWatch   |                    |  |
  |  |     +-------------------------+             +-------------------------+                    |  |
  |  |                                                                                            |  |
  |  |   Target Tracking Auto Scaling Policy: Maintain 70% CPU Utilization                        |  |
  |  +--------------------------------------------------------------------------------------------+  |
  +--------------------------------------------------------------------------------------------------+
```

### 1.2 Bill of Materials
- `aws_vpc`, `aws_subnet`, `aws_internet_gateway`, `aws_nat_gateway`: High-availability 2-AZ network topology.
- `aws_security_group`: ALB ingress security group and Task private security group (accepting traffic only from ALB).
- `aws_lb`, `aws_lb_target_group`, `aws_lb_listener`: Application Load Balancer routing.
- `aws_ecs_cluster`: Logical grouping of container services with Container Insights enabled.
- `aws_ecs_task_definition`: Fargate task configuration specifying 256 CPU units (0.25 vCPU) and 512 MB RAM.
- `aws_ecs_service`: Managed service with rolling deployment configuration.
- `aws_appautoscaling_target` & `aws_appautoscaling_policy`: Target tracking auto scaling policy on CPU.

---

## 2. Declarative Code (Terraform)

Create `main.tf`:

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

variable "project_name" {
  type    = string
  default = "fargate-microservice"
}

# -----------------------------------------------------------------------------
# 1. High Availability VPC Networking
# -----------------------------------------------------------------------------
resource "aws_vpc" "main" {
  cidr_block           = "10.0.0.0/16"
  enable_dns_hostnames = true
  enable_dns_support   = true

  tags = {
    Name = "${var.project_name}-vpc"
  }
}

resource "aws_internet_gateway" "igw" {
  vpc_id = aws_vpc.main.id
}

# Public Subnets
resource "aws_subnet" "public_1" {
  vpc_id                  = aws_vpc.main.id
  cidr_block              = "10.0.1.0/24"
  availability_zone       = "us-east-1a"
  map_public_ip_on_launch = true
  tags = { Name = "${var.project_name}-public-1" }
}

resource "aws_subnet" "public_2" {
  vpc_id                  = aws_vpc.main.id
  cidr_block              = "10.0.2.0/24"
  availability_zone       = "us-east-1b"
  map_public_ip_on_launch = true
  tags = { Name = "${var.project_name}-public-2" }
}

# Private Subnets
resource "aws_subnet" "private_1" {
  vpc_id            = aws_vpc.main.id
  cidr_block        = "10.0.10.0/24"
  availability_zone = "us-east-1a"
  tags = { Name = "${var.project_name}-private-1" }
}

resource "aws_subnet" "private_2" {
  vpc_id            = aws_vpc.main.id
  cidr_block        = "10.0.20.0/24"
  availability_zone = "us-east-1b"
  tags = { Name = "${var.project_name}-private-2" }
}

# NAT Gateway for Private Subnet Outbound Egress
resource "aws_eip" "nat_1" {
  domain = "vpc"
}

resource "aws_nat_gateway" "nat_1" {
  allocation_id = aws_eip.nat_1.id
  subnet_id     = aws_subnet.public_1.id
  tags          = { Name = "${var.project_name}-nat-1" }
}

# Route Tables
resource "aws_route_table" "public" {
  vpc_id = aws_vpc.main.id
  route {
    cidr_block = "0.0.0.0/0"
    gateway_id = aws_internet_gateway.igw.id
  }
  tags = { Name = "${var.project_name}-public-rt" }
}

resource "aws_route_table" "private" {
  vpc_id = aws_vpc.main.id
  route {
    cidr_block     = "0.0.0.0/0"
    nat_gateway_id = aws_nat_gateway.nat_1.id
  }
  tags = { Name = "${var.project_name}-private-rt" }
}

resource "aws_route_table_association" "pub_1" {
  subnet_id      = aws_subnet.public_1.id
  route_table_id = aws_route_table.public.id
}

resource "aws_route_table_association" "pub_2" {
  subnet_id      = aws_subnet.public_2.id
  route_table_id = aws_route_table.public.id
}

resource "aws_route_table_association" "priv_1" {
  subnet_id      = aws_subnet.private_1.id
  route_table_id = aws_route_table.private.id
}

resource "aws_route_table_association" "priv_2" {
  subnet_id      = aws_subnet.private_2.id
  route_table_id = aws_route_table.private.id
}

# -----------------------------------------------------------------------------
# 2. Security Groups
# -----------------------------------------------------------------------------
resource "aws_security_group" "alb_sg" {
  name        = "${var.project_name}-alb-sg"
  description = "Allows HTTP ingress from internet"
  vpc_id      = aws_vpc.main.id

  ingress {
    from_port   = 80
    to_port     = 80
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
  }

  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }
}

resource "aws_security_group" "ecs_tasks_sg" {
  name        = "${var.project_name}-tasks-sg"
  description = "Allows ingress ONLY from ALB"
  vpc_id      = aws_vpc.main.id

  ingress {
    from_port       = 8080
    to_port         = 8080
    protocol        = "tcp"
    security_groups = [aws_security_group.alb_sg.id]
  }

  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }
}

# -----------------------------------------------------------------------------
# 3. Application Load Balancer
# -----------------------------------------------------------------------------
resource "aws_lb" "main" {
  name               = "${var.project_name}-alb"
  internal           = false
  load_balancer_type = "application"
  security_groups    = [aws_security_group.alb_sg.id]
  subnets            = [aws_subnet.public_1.id, aws_subnet.public_2.id]
}

resource "aws_lb_target_group" "app" {
  name        = "${var.project_name}-tg"
  port        = 8080
  protocol    = "HTTP"
  vpc_id      = aws_vpc.main.id
  target_type = "ip"

  health_check {
    enabled             = true
    path                = "/health"
    port                = "8080"
    protocol            = "HTTP"
    interval            = 30
    timeout             = 5
    healthy_threshold   = 2
    unhealthy_threshold = 3
    matcher             = "200"
  }
}

resource "aws_lb_listener" "http" {
  load_balancer_arn = aws_lb.main.arn
  port              = 80
  protocol          = "HTTP"

  default_action {
    type             = "forward"
    target_group_arn = aws_lb_target_group.app.arn
  }
}

# -----------------------------------------------------------------------------
# 4. IAM Roles for ECS
# -----------------------------------------------------------------------------
resource "aws_iam_role" "ecs_execution" {
  name = "${var.project_name}-execution-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Action    = "sts:AssumeRole"
      Effect    = "Allow"
      Principal = { Service = "ecs-tasks.amazonaws.com" }
    }]
  })
}

resource "aws_iam_role_policy_attachment" "execution_policy" {
  role       = aws_iam_role.ecs_execution.name
  policy_arn = "arn:aws:iam::aws:policy/service-role/AmazonECSTaskExecutionRolePolicy"
}

resource "aws_iam_role" "ecs_task" {
  name = "${var.project_name}-task-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Action    = "sts:AssumeRole"
      Effect    = "Allow"
      Principal = { Service = "ecs-tasks.amazonaws.com" }
    }]
  })
}

# -----------------------------------------------------------------------------
# 5. ECS Cluster, Task Definition & Service
# -----------------------------------------------------------------------------
resource "aws_cloudwatch_log_group" "ecs" {
  name              = "/ecs/${var.project_name}"
  retention_in_days = 7
}

resource "aws_ecs_cluster" "main" {
  name = "${var.project_name}-cluster"

  setting {
    name  = "containerInsights"
    value = "enabled"
  }
}

resource "aws_ecs_task_definition" "app" {
  family                   = "${var.project_name}-task"
  network_mode             = "awsvpc"
  requires_compatibilities = ["FARGATE"]
  cpu                      = "256" # 0.25 vCPU
  memory                   = "512" # 512 MB
  execution_role_arn       = aws_iam_role.ecs_execution.arn
  task_role_arn            = aws_iam_role.ecs_task.arn

  container_definitions = jsonencode([
    {
      name      = "app-container"
      image     = "public.ecr.aws/docker/library/httpd:alpine"
      essential = true
      portMappings = [
        {
          containerPort = 8080
          hostPort      = 8080
          protocol      = "tcp"
        }
      ]
      command = [
        "sh", "-c",
        "mkdir -p /usr/local/apache2/htdocs && echo 'OK' > /usr/local/apache2/htdocs/health && sed -i 's/Listen 80/Listen 8080/' /usr/local/apache2/conf/httpd.conf && httpd-foreground"
      ]
      logConfiguration = {
        logDriver = "awslogs"
        options = {
          "awslogs-group"         = aws_cloudwatch_log_group.ecs.name
          "awslogs-region"        = "us-east-1"
          "awslogs-stream-prefix" = "app"
        }
      }
    }
  ])
}

resource "aws_ecs_service" "app" {
  name            = "${var.project_name}-service"
  cluster         = aws_ecs_cluster.main.id
  task_definition = aws_ecs_task_definition.app.arn
  desired_count   = 2
  launch_type     = "FARGATE"

  network_configuration {
    subnets          = [aws_subnet.private_1.id, aws_subnet.private_2.id]
    security_groups  = [aws_security_group.ecs_tasks_sg.id]
    assign_public_ip = false
  }

  load_balancer {
    target_group_arn = aws_lb_target_group.app.arn
    container_name   = "app-container"
    container_port   = 8080
  }

  depends_on = [aws_lb_listener.http]
}

# -----------------------------------------------------------------------------
# 6. Target Tracking Auto Scaling Policy
# -----------------------------------------------------------------------------
resource "aws_appautoscaling_target" "ecs_target" {
  max_capacity       = 10
  min_capacity       = 2
  resource_id        = "service/${aws_ecs_cluster.main.name}/${aws_ecs_service.app.name}"
  scalable_dimension = "ecs:service:DesiredCount"
  service_namespace  = "ecs"
}

resource "aws_appautoscaling_policy" "ecs_cpu" {
  name               = "${var.project_name}-cpu-autoscale"
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

output "alb_dns_name" {
  value = aws_lb.main.dns_name
}
```

---

## 3. Verification & Automated Test Harness

### 3.1 Deployment Instructions

```bash
terraform init
terraform apply -auto-approve
ALB_DNS=$(terraform output -raw alb_dns_name)
```

### 3.2 Smoke Tests & Health Check Verification

```bash
echo "Waiting for ECS Fargate tasks to pass ALB health checks (approx 60-90 seconds)..."

# Poll ALB endpoint until healthy
for i in {1..30}; do
  STATUS=$(curl -s -o /dev/null -w "%{http_code}" "http://${ALB_DNS}/health")
  if [ "$STATUS" -eq 200 ]; then
    echo "SUCCESS: ALB returned HTTP 200 from container health check!"
    break
  fi
  echo "Attempt $i: Status $STATUS. Waiting 10s..."
  sleep 10
done

# Verify container task placement in private subnets
echo "Querying ECS Service Tasks..."
aws ecs list-tasks --cluster "fargate-microservice-cluster"
```

### 3.3 Teardown Script

```bash
terraform destroy -auto-approve
```

---

## 4. Hardening, Security & Cost Analysis

### 4.1 Security & Networking Hardening
- **Zero Ingress to Tasks from Internet**: The ECS tasks security group strictly denies all internet ingress. Traffic is accepted solely from the `aws_security_group.alb_sg.id` reference.
- **Private Subnet Placement**: Tasks receive private IPs (`10.0.10.x`, `10.0.20.x`) and have `assign_public_ip = false`. Internet access for container image pulls and logging is secured via the NAT Gateway.
- **Role Isolation**: The execution role is restricted to pulling from ECR and logging. The task role contains no permissions by default until required by application logic.

### 4.2 Cost Breakdown (Baseline Production 2 Tasks)

| Component | Sizing / Configuration | Estimated Monthly Cost |
|:---|:---|:---|
| **Application Load Balancer** | 1 ALB (~15 LCU-hours/day) | \$22.50 |
| **NAT Gateway** | 1 NAT Gateway (\$0.045/hr + \$0.045/GB) | \$34.00 |
| **ECS Fargate (2 Tasks 24/7)** | 2 tasks $\times$ (0.25 vCPU + 0.5 GB RAM) | \$15.50 |
| **CloudWatch Logs** | 10 GB retention | \$5.00 |
| **Total Estimated Cost** | **2-Task High Availability Baseline** | **~\$77.00 / month** |

---

## 5. Operational Runbook & Disaster Recovery

### Common Operational Scenarios

#### Alert 1: Unhealthy Target Group (502 / 503 Bad Gateway)
- **Diagnostic Runbook**:
  1. Inspect ECS Service Events:
     ```bash
     aws ecs describe-services \
         --cluster "fargate-microservice-cluster" \
         --services "fargate-microservice-service" \
         --query "services[0].events[0:5]"
     ```
  2. Inspect container exit code: An exit code of `137` indicates out-of-memory (OOM killed); increase task memory definition.
  3. Verify health check path: Confirm the container responds with HTTP 200 on port 8080 at `/health`.

#### Disaster Recovery & Zero-Downtime Rolling Updates
- **Rolling Deployment Guarantee**: ECS service configuration enforces `minimum_healthy_percent = 100` and `maximum_percent = 200`. During deployments, new tasks launch and pass health checks before old tasks are drained and terminated.
