# Amazon ECS Advanced Networking & Deployment Patterns

> **Module**: `02-compute`  
> **Topic**: Service Connect, Advanced Networking, and Blue/Green Deployments  
> **Target Audience**: Senior Platform Engineers, Cloud Architects, Site Reliability Engineers  
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

Deploying basic container services behind an Application Load Balancer is effective for ingress web traffic, but large-scale microservice architectures require sophisticated service-to-service communication, robust traffic routing, and zero-downtime deployment strategies:
- Internal microservices calling each other through internal load balancers introduce unnecessary latency, complex network topologies, and high hourly ALB provisioning costs.
- Managing private DNS via AWS Cloud Map provides discovery, but lacks client-side load balancing, connection pooling, and granular latency metrics.
- Standard rolling deployments can expose live traffic to runtime regressions if canary testing or validation gates are missing.

This deep dive covers the advanced capabilities of Amazon ECS: **ECS Service Connect** (managed service mesh powered by Envoy), **Task Networking Modes**, **Cluster Auto Scaling (CAS) with Managed Termination Protection**, and **Canary Blue/Green Deployments with AWS CodeDeploy**.

---

## 2. Core Concepts

### 2.1 ECS Service Connect
ECS Service Connect provides native, service-to-service communication within an ECS cluster without requiring internal Application Load Balancers:
- Powered by a fully managed **Envoy sidecar proxy** injected automatically into the task.
- Uses **AWS Cloud Map** under the hood for discovery, while routing traffic point-to-point between task IPs.
- Implements intelligent client-side load balancing, connection pooling, retries, and outlier detection.
- Emits granular HTTP request count, error code, and latency telemetry directly to Amazon CloudWatch without external agents.

### 2.2 Task Network Modes
- **`awsvpc` (Recommended / Mandatory on Fargate)**:
  - Every task receives its own dedicated Elastic Network Interface (ENI) and private IPv4 address.
  - Security Groups are enforced at the individual container task level.
- **`bridge` (EC2 only)**:
  - Uses the Docker host's internal bridge network.
  - Requires dynamic host port mapping (`hostPort: 0`) behind an ALB.
- **`host` (EC2 only)**:
  - Containers bind directly to the EC2 host network namespace. Highest raw throughput, but prevents multiple containers from listening on the same port.
- **`none`**:
  - Container has no external network access (isolated batch computation).

### 2.3 ENI Trunking (High-Density EC2 Task Hosting)
By default, EC2 instances have a strict hardware limit on the number of ENIs that can be attached (e.g., an `m5.large` supports at most 3 ENIs). Because `awsvpc` consumes 1 ENI per task, an `m5.large` could only run 2 tasks.
- **ENI Trunking**: Allows an EC2 instance to attach a "trunk" network interface, increasing ENI capacity by 3x–4x (e.g., running up to 10–30 `awsvpc` tasks on a single instance).

### 2.4 Blue/Green Deployments with AWS CodeDeploy
Replaces simple in-place rolling deployments with canary or linear traffic shifting:
- Provisions a new "Green" task set alongside the existing "Blue" task set.
- Routes test traffic to Green using a dedicated **Test Listener** (e.g., port 8443) for automated health verification.
- Gradually shifts production traffic (e.g., `Canary10Percent5Minutes` or `Linear10PercentEvery1Minute`).
- Automatically rolls back if Amazon CloudWatch alarms trigger during the evaluation window.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        ECS Service Connect Mesh Architecture                      |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ Client Service: Order-Processor ]                                              |
|  +-----------------------------------------------------------------------------+  |
|  | Task Definition (awsvpc)                                                    |  |
|  |                                                                             |  |
|  |  +---------------------------+          +--------------------------------+  |  |
|  |  | Application Container     | -------> | Envoy Proxy Sidecar            |  |  |
|  |  | (Calls http://inventory:80| (Local)  | (Managed by ECS Agent)         |  |  |
|  |  +---------------------------+          +--------------------------------+  |  |
|  +---------------------------------------------------------|-------------------+  |
|                                                            |                      |
|                                                            | mTLS / Point-to-Point|
|                                                            | Dynamic Load Balance |
|                                                            v                      |
|  [ Server Service: Inventory-Service ]                                            |
|  +-----------------------------------------------------------------------------+  |
|  | Task Definition (awsvpc)                                                    |  |
|  |                                                                             |  |
|  |  +---------------------------+          +--------------------------------+  |  |
|  |  | Application Container     | <------- | Envoy Proxy Sidecar            |  |  |
|  |  | (Listening on port 8080)  | (Local)  | (Receives on ingress port)     |  |  |
|  |  +---------------------------+          +--------------------------------+  |  |
|  +-----------------------------------------------------------------------------+  |
|                                                                                   |
|  Cloud Map Private DNS Namespace: `production.internal`                           |
|  Telemetry: Envoy emits EnvoyLatency, HTTPCode_Target_2XX/5XX to CloudWatch       |
+-----------------------------------------------------------------------------------+
```

### CodeDeploy Blue/Green Traffic Shifting
```
[Step 1: Deploy Green]   ALB Prod Listener (80)  ----> [Blue Task Set (100%)]
                         ALB Test Listener (8443) ---> [Green Task Set (New Code)]
                                                               |
                                                 (Run Integration Tests)
                                                               v
[Step 2: Canary Shift]   ALB Prod Listener (80)  ----> [Blue: 90%]  [Green: 10%]
                                                               |
                                                 (Wait 5 mins; Monitor Alarms)
                                                               v
[Step 3: Complete Shift] ALB Prod Listener (80)  ----> [Green Task Set (100%)]
                         Blue Task Set Terminated after termination wait timer.
```

---

## 4. Syntax & Usage

### 4.1 Configuring ECS Service Connect via AWS CLI
```bash
# 1. Create Cloud Map Private DNS Namespace
NAMESPACE_ID=$(aws servicediscovery create-private-dns-namespace \
  --name "production.local" \
  --vpc "vpc-0123456789abcdef0" \
  --query 'OperationId' \
  --output text)

# 2. Deploy Server Service with Service Connect configuration
aws ecs create-service \
  --cluster production-cluster \
  --service-name inventory-service \
  --task-definition inventory-task:1 \
  --desired-count 3 \
  --launch-type FARGATE \
  --network-configuration '{"awsvpcConfiguration": {"subnets": ["subnet-01a", "subnet-01b"], "securityGroups": ["sg-01234"]}}' \
  --service-connect-configuration '{
    "enabled": true,
    "namespace": "production.local",
    "services": [
      {
        "portName": "http-api",
        "discoveryName": "inventory",
        "clientAliases": [
          {"port": 80, "dnsName": "inventory.production.local"}
        ]
      }
    ]
  }'
```

### 4.2 CodeDeploy `appspec.yaml` for ECS Blue/Green Deployments
```yaml
version: 0.0
Resources:
  - TargetService:
      Type: AWS::ECS::Service
      Properties:
        TaskDefinition: "arn:aws:ecs:us-east-1:111122223333:task-definition/api-service:4"
        LoadBalancerInfo:
          ContainerName: "api-container"
          ContainerPort: 8080
        CapacityProviderStrategy:
          - CapacityProvider: "FARGATE"
            Weight: 1
            Base: 1
          - CapacityProvider: "FARGATE_SPOT"
            Weight: 2
Hooks:
  - AfterAllowTestTraffic: "arn:aws:lambda:us-east-1:111122223333:function:ValidateGreenDeployment"
```

---

## 5. Practical Examples

### Example 1: Terraform Declarative ECS Service Connect Setup
Configure two microservices communicating privately via Service Connect with Envoy sidecars.

```hcl
resource "aws_service_discovery_private_dns_namespace" "internal" {
  name        = "corp.internal"
  vpc         = var.vpc_id
  description = "Private namespace for ECS Service Connect"
}

resource "aws_ecs_task_definition" "inventory" {
  family                   = "inventory-api"
  network_mode             = "awsvpc"
  requires_compatibilities = ["FARGATE"]
  cpu                      = "512"
  memory                   = "1024"
  execution_role_arn       = var.execution_role_arn
  task_role_arn            = var.task_role_arn

  container_definitions = jsonencode([
    {
      name      = "inventory-app"
      image     = "${var.ecr_repo_url}:latest"
      essential = true
      portMappings = [
        {
          name          = "inventory-http"
          containerPort = 8080
          hostPort      = 8080
          protocol      = "tcp"
        }
      ]
    }
  ])
}

resource "aws_ecs_service" "inventory" {
  name            = "inventory-service"
  cluster         = var.cluster_id
  task_definition = aws_ecs_task_definition.inventory.arn
  desired_count   = 2

  network_configuration {
    subnets         = var.private_subnet_ids
    security_groups = [var.inventory_sg_id]
  }

  service_connect_configuration {
    enabled   = true
    namespace = aws_service_discovery_private_dns_namespace.internal.arn

    service {
      port_name      = "inventory-http"
      discovery_name = "inventory"

      client_alias {
        port     = 80
        dns_name = "inventory.corp.internal"
      }
    }
  }
}
```

### Example 2: Complete CodeDeploy Canary Deployment Configuration
Create a CodeDeploy deployment group shifting traffic to Green over 5 minutes with automated rollback.

```bash
aws deploy create-deployment-group \
  --application-name AppECS-production-cluster-api-service \
  --deployment-group-name DgpECS-production-cluster-api-service \
  --deployment-config-name CodeDeployDefault.ECSCanary10Percent5Minutes \
  --service-role-arn arn:aws:iam::111122223333:role/CodeDeployServiceRole \
  --blue-green-deployment-configuration '{
    "terminateBlueInstancesOnDeploymentSuccess": {
      "action": "TERMINATE",
      "terminationWaitTimeInMinutes": 10
    },
    "deploymentReadyOption": {
      "actionOnTimeout": "CONTINUE"
    }
  }' \
  --ecs-services '[{"serviceName":"api-service","clusterName":"production-cluster"}]' \
  --load-balancer-info '{
    "targetGroupPairInfoList": [{
      "targetGroups": [
        {"name": "prod-api-tg-blue"},
        {"name": "prod-api-tg-green"}
      ],
      "prodTrafficRoute": {
        "listenerArns": ["arn:aws:elasticloadbalancing:us-east-1:111122223333:listener/app/prod-alb/123/prod-listener"]
      },
      "testTrafficRoute": {
        "listenerArns": ["arn:aws:elasticloadbalancing:us-east-1:111122223333:listener/app/prod-alb/123/test-listener"]
      }
    }]
  }' \
  --auto-rollback-configuration '{
    "enabled": true,
    "events": ["DEPLOYMENT_FAILURE", "DEPLOYMENT_STOP_ON_ALARM"]
  }' \
  --alarm-configuration '{
    "enabled": true,
    "alarms": [{"name": "High5XXErrorsOnGreenTargetGroup"}]
  }'
```

### Example 3: ECS Cluster Auto Scaling (CAS) with Managed Termination Protection
Configure an EC2 Capacity Provider that automatically provisions EC2 instances when pending tasks exceed cluster capacity.

```hcl
resource "aws_ecs_capacity_provider" "cas_provider" {
  name = "ec2-cas-capacity-provider"

  auto_scaling_group_provider {
    auto_scaling_group_arn         = aws_autoscaling_group.ecs_fleet.arn
    managed_termination_protection = "ENABLED"

    managed_scaling {
      maximum_scaling_step_size = 5
      minimum_scaling_step_size = 1
      status                    = "ENABLED"
      target_capacity           = 100 # Maintain 100% utilization of allocated EC2 instances
    }
  }
}

resource "aws_ecs_cluster_capacity_providers" "main" {
  cluster_name = aws_ecs_cluster.main.name

  capacity_providers = [aws_ecs_capacity_provider.cas_provider.name]

  default_capacity_provider_strategy {
    capacity_provider = aws_ecs_capacity_provider.cas_provider.name
    weight            = 1
  }
}
```

### Example 4: Testing Canary Validation via Post-Traffic Shift Lambda Hook
Lambda function executed by CodeDeploy to validate Green target group health before completing traffic cutover.

```python
import boto3
import json

codedeploy = boto3.client('codedeploy')
elbv2 = boto3.client('elbv2')

def lambda_handler(event, context):
    deployment_id = event['DeploymentId']
    lifecycle_event_hook_execution_id = event['LifecycleEventHookExecutionId']
    
    print(f"Executing validation for deployment {deployment_id}...")
    
    # Target group synthetic health probe
    try:
        # Perform HTTP GET request or verify error metric rate < 0.1%
        test_success = True 
        
        status = 'Succeeded' if test_success else 'Failed'
    except Exception as e:
        print(f"Validation failed: {str(e)}")
        status = 'Failed'
        
    codedeploy.put_lifecycle_event_hook_execution_status(
        deploymentId=deployment_id,
        lifecycleEventHookExecutionId=lifecycle_event_hook_execution_id,
        status=status
    )
    return {"status": status}
```

### Example 5: Inspecting Envoy Sidecar Metrics in CloudWatch
Query ECS Service Connect connection metrics via AWS CLI.

```bash
aws cloudwatch get-metric-data \
  --metric-data-queries '[
    {
      "Id": "m1",
      "MetricStat": {
        "Metric": {
          "Namespace": "AWS/ECS",
          "MetricName": "EnvoyLatency",
          "Dimensions": [
            {"Name": "ClusterName", "Value": "production-cluster"},
            {"Name": "DiscoveryName", "Value": "inventory"}
          ]
        },
        "Period": 60,
        "Stat": "p99"
      }
    }
  ]' \
  --start-time $(date -u -d "1 hour ago" +%s) \
  --end-time $(date -u +%s)
```

---

## 6. Best Practices

1. **Replace Internal ALBs with Service Connect**: For east-west (microservice-to-microservice) communication, eliminate internal ALBs. Service Connect avoids per-hour load balancer costs, eliminates TLS termination hops, and provides client-side load balancing.
2. **Always Use Managed Termination Protection on ASGs**: When using EC2 Capacity Providers with ECS, always set `managed_termination_protection = "ENABLED"`. This prevents the EC2 Auto Scaling group from terminating an instance that is actively hosting running ECS tasks during scale-in events.
3. **Use CodeDeploy Canary Traffic Shifts for Mission-Critical APIs**: Avoid all-at-once deployments for core revenue services. Use `ECSCanary10Percent5Minutes` with an automated CloudWatch alarm trigger. If error rates increase on the green target group during the 5-minute window, traffic rolls back instantly with zero user impact.
4. **Tune Grace Periods for CodeDeploy Health Checks**: Ensure `healthCheckGracePeriodSeconds` covers slow application startups. Otherwise, Green tasks will be marked unhealthy, causing CodeDeploy to trip and abort the deployment prematurely.
5. **Separate Task Security Groups**: Assign distinct security groups to tasks in `awsvpc` mode. Configure the inventory service security group to only permit ingress on port 8080 from the order service security group ID (`sg-orders`).

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Disabling Managed Termination Protection** | ASG terminates EC2 instances hosting critical running tasks during scale-in, dropping active requests. | Enable `managed_termination_protection = "ENABLED"` on the Capacity Provider and enable instance protection on ASG. |
| **Omitting Test Listener in CodeDeploy** | Inability to run synthetic integration tests against the Green task set before live users hit the code. | Provision a dedicated test listener (e.g. port 8443) on the ALB pointing to the Green target group. |
| **Running Service Connect with Mismatched Namespaces** | Microservices cannot resolve service aliases and connection requests fail with DNS NXDOMAIN. | Ensure all communicating services are registered to the exact same Cloud Map private DNS namespace. |
| **Forgetting to Update Task Execution Role for Sidecars** | ECS fails to pull the Envoy container image or stream sidecar logs, causing task launch loops. | Ensure `ecsTaskExecutionRole` has permissions to pull public ECR images and create CloudWatch log streams. |
| **Setting `target_capacity` to 80% on CAS** | Over-provisions 20% spare EC2 instances continuously, increasing infrastructure bills. | Set `target_capacity = 100` unless spare standby capacity is explicitly required for burst scaling. |
| **Using `bridge` Mode on Fargate** | Fargate task creation fails immediately with an unsupported network mode validation error. | Fargate strictly supports and requires `awsvpc` network mode. |

---

## 8. Performance, Security & Cost Notes

### Envoy Sidecar Overhead
- ECS Service Connect automatically allocates a small CPU and RAM overhead for the Envoy sidecar (typically ~64 MB RAM and 32–64 CPU units per task).
- P99 latency overhead added by the Envoy sidecar is negligible (< 1.5 milliseconds).

### Security Isolation
- **mTLS Encryption**: ECS Service Connect natively supports automatic mutual TLS (mTLS) traffic encryption using AWS Private Certificate Authority (PCA) without modifying application code.
- **Microservice Segmentation**: Because each task possesses an independent `awsvpc` ENI, security group rules can enforce zero-trust network policies between internal tiers.

### Cost Savings
- **ALB Elimination**: Replacing 10 internal Application Load Balancers with ECS Service Connect saves ~$225/month in fixed ALB hourly fees plus data processing charges.

---

## 9. Comparison Table

| Feature | Internal ALB | AWS Cloud Map (Raw DNS) | ECS Service Connect |
|:---|:---|:---|:---|
| **Load Balancing** | Server-side (ALB intermediary) | Client-side (DNS round-robin) | Client-side (Envoy proxy) |
| **Latency Overhead** | 2 hops (Client -> ALB -> Server) | 1 hop (Direct IP connection) | 1 hop (Direct IP via Envoy) |
| **Connection Pooling** | Yes (managed by ALB) | No (application dependent) | Yes (Envoy connection reuse) |
| **Health Probing** | Active HTTP probes | Route 53 / Custom checks | Automatic outlier detection |
| **Telemetry** | ALB CloudWatch metrics | None | Granular Envoy metrics |
| **Hourly Cost** | ~$22.50/month per ALB | $0.10/service/month | Free (small sidecar RAM/CPU) |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What problem does ECS Service Connect solve compared to traditional internal ALBs?</strong></summary>

**Answer:**
In traditional microservice architectures, when Service A calls Service B, traffic routes through an internal Application Load Balancer. This introduces multiple drawbacks:
1. **Cost**: Each internal ALB costs ~$22.50/month base plus LCU data processing fees.
2. **Latency**: Requests make two network hops (Client -> ALB, then ALB -> Target).
3. **Infrastructure Burden**: Requires configuring subnets, listeners, and target groups for every internal service.

ECS Service Connect solves this by deploying a lightweight Envoy sidecar proxy inside each task. Communication occurs point-to-point directly between tasks (1 network hop), with Envoy managing client-side load balancing, connection pooling, retries, and metric emission, completely eliminating internal ALBs and their associated costs.
</details>

<details>
<summary><strong>Q2: How does ECS Cluster Auto Scaling (CAS) prevent task placement failures on EC2 clusters?</strong></summary>

**Answer:**
Standard EC2 Auto Scaling evaluates instance-level CloudWatch metrics (e.g., average CPU utilization). If 5 tasks need to be placed but all EC2 hosts are at 99% memory, the ASG will not scale if average CPU is low, resulting in task placement failures.

ECS Cluster Auto Scaling introduces the **Capacity Provider** with a metric called **`CapacityProviderReservation`**:
$$\text{CapacityProviderReservation} = \left( \frac{\text{Instances Needed by Tasks}}{\text{Active Instances Available}} \right) \times 100$$
If tasks enter a `PENDING` state because no existing EC2 instance has sufficient CPU or memory to fit them, the reservation metric spikes above 100%. The Capacity Provider automatically signals the ASG to launch the exact number of new EC2 instances required to satisfy the pending tasks.
</details>

<details>
<summary><strong>Q3: What is Managed Termination Protection and why is it mandatory for ECS Capacity Providers?</strong></summary>

**Answer:**
When an EC2 Auto Scaling Group scales in, its default termination policy selects instances based on Availability Zone balance and age—it has no awareness of whether an instance is currently executing healthy ECS tasks.

With **Managed Termination Protection** enabled:
1. ECS applies instance scale-in protection to any EC2 instance hosting active tasks.
2. When the ASG receives a scale-in signal, it cannot terminate protected instances.
3. ECS safely drains tasks off an instance, redistributes them to remaining nodes, and only removes termination protection once the instance is completely empty, ensuring zero dropped requests during cluster scale-in.
</details>

<details>
<summary><strong>Q4: How does AWS CodeDeploy execute a blue/green canary deployment for an ECS service?</strong></summary>

**Answer:**
1. CodeDeploy reads the new Task Definition from `appspec.yaml`.
2. It launches the new "Green" task set on the ECS cluster.
3. The Green tasks register with the ALB's **Test Listener** (e.g., port 8443) on an alternate target group.
4. Pre-traffic validation hooks (e.g., a test Lambda function) run synthetic queries against port 8443 to verify system health.
5. If tests pass, CodeDeploy begins shifting production traffic on port 80 according to the deployment configuration (e.g., `Canary10Percent5Minutes` sends 10% to Green for 5 minutes).
6. If CloudWatch error alarms trigger during this window, traffic rolls back instantly to Blue. If healthy, 100% of traffic shifts to Green, and Blue tasks are terminated after the termination wait timer.
</details>

<details>
<summary><strong>Q5: What is the difference between `awsvpc` network mode and `bridge` network mode in ECS?</strong></summary>

**Answer:**
- **`bridge` Mode**: Uses the Docker host's virtual bridge interface. Containers share the host's physical network adapter and IP address. To avoid port collisions, containers must use dynamic host port mapping (`hostPort: 0`). Security groups are applied at the EC2 host level, meaning all containers on that host share identical network firewall rules.
- **`awsvpc` Mode**: Attaches a dedicated Elastic Network Interface (ENI) directly to each container task. Every task receives its own unique private IP address from the VPC subnet. Tasks can have independent Security Groups, traffic is logged in VPC Flow Logs, and multiple containers can bind to port 80 or 8080 without port collisions.
</details>

<details>
<summary><strong>Q6: How does ENI Trunking enable higher container density on EC2 instances?</strong></summary>

**Answer:**
In `awsvpc` mode, each task requires 1 ENI. Standard EC2 instances have hard limits on the number of network interfaces they can attach (e.g., an `m5.large` has a limit of 3 ENIs, leaving only 2 ENIs for tasks after the primary host ENI).

**ENI Trunking** allows compatible Nitro instances to attach a specialized "Trunk ENI". The ECS agent then attaches secondary "Branch ENIs" into the trunk interface via VLAN encapsulation. This increases the available ENIs per instance by 3x–4x (e.g., allowing an `m5.large` to host up to 10–12 `awsvpc` tasks), drastically improving container bin-packing efficiency.
</details>

<details>
<summary><strong>Q7: What happens if an ECS task fails its Docker health check versus its ALB health check?</strong></summary>

**Answer:**
- **Docker Health Check (`CMD-SHELL`)**: Configured inside the Task Definition. If the container fails this check consecutively, ECS marks the container as `UNHEALTHY`. If the container is declared `essential`, ECS stops the entire task and provisions a replacement task.
- **ALB Health Check**: Configured on the load balancer target group. If the task fails HTTP probes on the ALB, the load balancer stops routing new requests to that specific target. If the ECS Service is configured with `healthCheckGracePeriodSeconds`, ECS will eventually replace the task if it remains unhealthy on the ALB.
</details>

<details>
<summary><strong>Q8: What are the security advantages of using mTLS with ECS Service Connect?</strong></summary>

**Answer:**
Without mTLS, east-west microservice traffic traversing VPC subnets is transmitted in unencrypted plaintext, vulnerable to inspection if network boundaries are compromised.
With ECS Service Connect mTLS enabled:
- The Envoy sidecars mutually authenticate each other using X.509 certificates issued automatically by **AWS Private CA**.
- All inter-service payload data in transit is encrypted using TLS 1.3.
- Application code requires zero modifications or TLS certificate bundle management.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Private Cloud Map Namespace via AWS CLI</strong></summary>

**Objective**: Provision a private DNS namespace for Service Connect.

**Command**:
```bash
aws servicediscovery create-private-dns-namespace \
  --name "apps.local" \
  --vpc "vpc-0123456789abcdef0"
```
</details>

<details>
<summary><strong>Exercise 2: Deploy an ECS Service with Service Connect Enabled</strong></summary>

**Objective**: Configure a service client alias exposing port 80 mapped to container port 3000.

**Command**:
```bash
aws ecs create-service \
  --cluster test-cluster \
  --service-name auth-service \
  --task-definition auth-task:1 \
  --desired-count 2 \
  --launch-type FARGATE \
  --network-configuration '{"awsvpcConfiguration": {"subnets": ["subnet-01a"], "securityGroups": ["sg-012"]}}' \
  --service-connect-configuration '{
    "enabled": true,
    "namespace": "apps.local",
    "services": [{
      "portName": "auth-http",
      "discoveryName": "auth",
      "clientAliases": [{"port": 80, "dnsName": "auth.apps.local"}]
    }]
  }'
```
</details>

<details>
<summary><strong>Exercise 3: Trigger a CodeDeploy ECS Canary Deployment</strong></summary>

**Objective**: Execute a 10% canary shift for 5 minutes using a new Task Definition revision.

**Command**:
```bash
aws deploy create-deployment \
  --application-name AppECS-test-cluster-web-service \
  --deployment-group-name DgpECS-test-cluster-web-service \
  --deployment-config-name CodeDeployDefault.ECSCanary10Percent5Minutes \
  --revision '{"revisionType": "AppSpecContent", "appSpecContent": {"content": "version: 0.0\nResources:\n  - TargetService:\n      Type: AWS::ECS::Service\n      Properties:\n        TaskDefinition: \"arn:aws:ecs:us-east-1:111122223333:task-definition/web-task:2\"\n        LoadBalancerInfo:\n          ContainerName: \"web-container\"\n          ContainerPort: 80"}}'
```
</details>

<details>
<summary><strong>Exercise 4: Verify ENI Trunking Capacity on an EC2 Cluster Node</strong></summary>

**Objective**: Check whether an EC2 instance registered to ECS has ENI trunking enabled.

**Command**:
```bash
aws ecs describe-container-instances \
  --cluster test-cluster \
  --container-instances <CONTAINER_INSTANCE_ID> \
  --query 'containerInstances[0].attributes[?name==`ecs.capability.branch-cni-plugin`]'
```
</details>

<details>
<summary><strong>Exercise 5: Inspect Envoy Sidecar Telemetry in CloudWatch</strong></summary>

**Objective**: Monitor active client connections handled by the Service Connect proxy.

**Command**:
```bash
aws cloudwatch list-metrics \
  --namespace "AWS/ECS" \
  --metric-name "EnvoyLatency"
```
</details>

<details>
<summary><strong>Exercise 6: Test CodeDeploy Automated Rollback via Alarm</strong></summary>

**Objective**: Simulate a broken deployment by introducing a failing health check and verifying that CodeDeploy automatically rolls back to the Blue target group.

**Steps**:
1. Configure CloudWatch alarm on ALB Target Group 5XX errors.
2. Deploy a broken Task Definition revision returning 500 errors.
3. Observe CodeDeploy canary shift 10% of traffic.
4. Watch alarm trip and verify CodeDeploy status changes to `RolledBack`.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                         Advanced ECS Networking & Deployments                         |
+---------------------------------------------------------------------------------------+
|  Technology            | Primary Benefit                   | Key Metric / Requirement |
|------------------------+-----------------------------------+--------------------------|
|  ECS Service Connect   | Eliminates internal ALBs          | AWS Cloud Map Namespace  |
|  Managed Termination   | Prevents task killing on scale-in | ASG Instance Protection  |
|  Cluster Auto Scaling  | Scales EC2 based on pending tasks | CapacityProviderReservtn |
|  CodeDeploy Blue/Green | Canary zero-downtime cutover      | Test Listener (Port 8443)|
|  ENI Trunking          | 3x-4x container density on EC2    | Nitro instances (awsvpc) |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Amazon ECS Service Connect User Guide](https://docs.aws.amazon.com/AmazonECS/latest/developerguide/service-connect.html)
- [Managing Blue/Green Deployments with AWS CodeDeploy](https://docs.aws.amazon.com/AmazonECS/latest/developerguide/deployment-type-bluegreen.html)
- [Deep Dive on Amazon ECS Cluster Auto Scaling](https://aws.amazon.com/blogs/compute/deep-dive-on-amazon-ecs-cluster-auto-scaling/)
- [Under the Hood: AWS Fargate and awsvpc Networking](https://aws.amazon.com/blogs/compute/under-the-hood-aws-fargate-data-plane/)
- [AWS Cloud Map Service Discovery Documentation](https://docs.aws.amazon.com/cloud-map/latest/dg/what-is-cloud-map.html)
