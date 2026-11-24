# EC2 Auto Scaling and Launch Templates

> **Module**: `02-compute`  
> **Topic**: Elastic Fleet Management, Launch Templates, and Scaling Policies  
> **Target Audience**: Cloud Engineers, Site Reliability Engineers, DevOps Architects  
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

Running isolated virtual machines introduces operational failure modes: if an underlying hardware host crashes, the application experiences sustained downtime until an engineer intervenes. Furthermore, traffic fluctuates dynamically throughout the day, rendering static server counts either wastefully over-provisioned or overwhelmed by demand.

**Amazon EC2 Auto Scaling** and **EC2 Launch Templates** solve these availability and elasticity challenges:
- **High Availability**: Automatically detects unhealthy instances and replaces them across multiple Availability Zones without human intervention.
- **Cost Elasticity**: Scales instance fleets dynamically up or down in response to real-time traffic demand (e.g., CPU, request counts, queue depths).
- **Immutable Infrastructure**: Uses versioned Launch Templates to declare golden AMI configurations, IMDSv2 metadata requirements, and bootstrap scripts, decoupling server specification from fleet orchestration.
- **Zero-Downtime Deployments**: Features **Instance Refresh** to orchestrate canary or rolling upgrades of fleet instances when updating operating system patches or application code.

---

## 2. Core Concepts

### 2.1 Launch Templates vs. Legacy Launch Configurations
- **Launch Configurations (Deprecated)**: Immutable, unversioned templates. Modifying any parameter required creating an entirely new configuration and manually updating the Auto Scaling Group (ASG).
- **Launch Templates (Current Standard)**: Versioned declarations of instance parameters. Supports:
  - Version inheritance and default version tagging.
  - Parameter subsets (e.g., defining storage or network parameters independently).
  - Mixed Instances Policies (combining On-Demand and Spot instances across multiple instance families).
  - Advanced features: T4g/T3 unlimited credits, Elastic Inference, and strict IMDSv2 metadata enforcement.

### 2.2 Auto Scaling Group (ASG) Dimensions
An ASG maintains fleet sizing based on three core capacity parameters:
- **Minimum Size (`Min`)**: The hard floor. The ASG will never terminate instances below this count, even when completely idle.
- **Maximum Size (`Max`)**: The ceiling protecting against runaway scaling expenses during DDoS or looping traffic spikes.
- **Desired Capacity (`Desired`)**: The current target count of active instances. Scaling policies increment or decrement this figure.

### 2.3 Scaling Policy Types
1. **Target Tracking Scaling**: Functions like a thermostat. You specify an aggregate metric target (e.g., keep average CPU utilization at 60%, or maintain 1,000 ALB requests per instance), and the ASG mathematically calculates the necessary scale-in/scale-out increments.
2. **Step Scaling**: Increases or decreases capacity in discrete steps based on CloudWatch alarm thresholds (e.g., if CPU > 70% add 1 instance; if CPU > 85% add 3 instances).
3. **Simple Scaling**: Legacy policy; executes a single adjustment and enters a static cooldown period.
4. **Scheduled Scaling**: Pre-scales instances based on predictable cron schedules (e.g., scale up at 08:00 AM Monday–Friday).
5. **Predictive Scaling**: Analyzes historical CloudWatch patterns using machine learning models to forecast future capacity needs and launch instances in advance of predicted load surges.

### 2.4 Lifecycle Hooks
Lifecycle hooks pause an instance's state transition during scale-out or scale-in:
- `autoscaling:EC2_INSTANCE_LAUNCHING`: Pauses the instance in a `Pending:Wait` state after booting but before joining the Application Load Balancer target group, allowing bootstrap scripts, software installation, or warm-up checks to complete.
- `autoscaling:EC2_INSTANCE_TERMINATING`: Pauses the instance in a `Terminating:Wait` state, allowing the application to drain database connections, flush memory buffers to S3, or gracefully disconnect websocket clients before the hypervisor destroys the instance.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        EC2 Auto Scaling Architecture & Flow                       |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ CloudWatch Metric Stream ] (CPU, Memory, ALB RequestCountPerTarget, SQS Depth) |
|              |                                                                    |
|              v                                                                    |
|  +-----------------------+                                                        |
|  | Target Tracking Alarm | (e.g., Maintain 60% CPU Across Fleet)                  |
|  +-----------------------+                                                        |
|              |                                                                    |
|              | Threshold Exceeded -> Trigger Scale-Out                            |
|              v                                                                    |
|  +-------------------------------------------------------------+                  |
|  |                 EC2 Auto Scaling Group (ASG)                |                  |
|  | - Desired: 2 -> 4                                           |                  |
|  | - Availability Zones: us-east-1a, us-east-1b, us-east-1c    |                  |
|  +-------------------------------------------------------------+                  |
|              |                                                                    |
|              +--------------------------+-------------------------+               |
|              | Launches Instance A      | Launches Instance B     |               |
|              v                          v                         v               |
|     +------------------+       +------------------+                               |
|     |  Pending:Wait    |       |  Pending:Wait    |  (Lifecycle Hook: Launching)  |
|     |  - Run bootstrap |       |  - Run bootstrap |  (SSM / Script Completes)     |
|     +------------------+       +------------------+                               |
|              |                          |                                         |
|              | CompleteLifecycleAction  | CompleteLifecycleAction                 |
|              v                          v                                         |
|     +------------------+       +------------------+                               |
|     |     InService    |       |     InService    |  (Target Group Registration)  |
|     +------------------+       +------------------+  (Passes ALB Health Checks)   |
|              ^                          ^                                         |
|              |                          |                                         |
|     +---------------------------------------------+                               |
|     |       Application Load Balancer (ALB)       |                               |
|     +---------------------------------------------+                               |
+-----------------------------------------------------------------------------------+
```

### Lifecycle Hook State Progression
```
[Pending] ---> [Pending:Wait] ---> [Pending:Proceed] ---> [InService]
                     ^
                     | (Execute Bootstrap, Warmup, SSM RunCommand)
                     +-- CompleteLifecycleAction (CONTINUE / ABANDON)

[InService] ---> [Terminating:Wait] ---> [Terminating:Proceed] ---> [Terminated]
                        ^
                        | (Drain connections, Flush logs, Push state to S3)
                        +-- CompleteLifecycleAction (CONTINUE)
```

---

## 4. Syntax & Usage

### 4.1 Creating a Launch Template with IMDSv2 via AWS CLI
```bash
aws ec2 create-launch-template \
  --launch-template-name "prod-web-template" \
  --version-description "v1-graviton-imds2" \
  --launch-template-data '{
    "ImageId": "ami-0c7217cdde317cfec",
    "InstanceType": "c7g.large",
    "IamInstanceProfile": {
      "Arn": "arn:aws:iam::111122223333:instance-profile/AppServerEC2Profile"
    },
    "SecurityGroupIds": ["sg-0123456789abcdef0"],
    "MetadataOptions": {
      "HttpEndpoint": "enabled",
      "HttpTokens": "required",
      "HttpPutResponseHopLimit": 1
    },
    "BlockDeviceMappings": [
      {
        "DeviceName": "/dev/xvda",
        "Ebs": {
          "VolumeSize": 40,
          "VolumeType": "gp3",
          "Iops": 3000,
          "Throughput": 125,
          "Encrypted": true,
          "DeleteOnTermination": true
        }
      }
    ],
    "UserData": "IyEvYmluL2Jhc2gKZG5mIHVwZGF0ZSAteQo=",
    "TagSpecifications": [
      {
        "ResourceType": "instance",
        "Tags": [
          {"Key": "Environment", "Value": "production"},
          {"Key": "Role", "Value": "api-worker"}
        ]
      }
    ]
  }'
```

### 4.2 Creating an ASG with Mixed Instances Policy
Combine On-Demand and Spot instances across multiple Graviton instance families for cost optimization and high availability:

```bash
aws autoscaling create-auto-scaling-group \
  --auto-scaling-group-name "prod-api-asg" \
  --min-size 2 \
  --max-size 10 \
  --desired-capacity 2 \
  --vpc-zone-identifier "subnet-01a,subnet-01b,subnet-01c" \
  --target-group-arns "arn:aws:elasticloadbalancing:us-east-1:111122223333:targetgroup/prod-api-tg/12345" \
  --health-check-type "ELB" \
  --health-check-grace-period 300 \
  --mixed-instances-policy '{
    "LaunchTemplate": {
      "LaunchTemplateSpecification": {
        "LaunchTemplateName": "prod-web-template",
        "Version": "$Latest"
      },
      "Overrides": [
        {"InstanceType": "c7g.large"},
        {"InstanceType": "m7g.large"},
        {"InstanceType": "c6g.large"}
      ]
    },
    "InstancesDistribution": {
      "OnDemandBaseCapacity": 2,
      "OnDemandPercentageAboveBaseCapacity": 20,
      "SpotAllocationStrategy": "price-capacity-optimized"
    }
  }'
```

---

## 5. Practical Examples

### Example 1: Target Tracking Scaling Policy on ALB Request Count
Ensure the ASG scales dynamically so each instance processes approximately 750 requests per minute.

```bash
aws autoscaling put-scaling-policy \
  --auto-scaling-group-name "prod-api-asg" \
  --policy-name "alb-target-tracking-policy" \
  --policy-type "TargetTrackingScaling" \
  --target-tracking-configuration '{
    "PredefinedMetricSpecification": {
      "PredefinedMetricType": "ALBRequestCountPerTarget",
      "ResourceLabel": "app/prod-alb/12345/targetgroup/prod-api-tg/67890"
    },
    "TargetValue": 750.0,
    "DisableScaleIn": false
  }'
```

### Example 2: Terraform Definition for ASG with Lifecycle Hook & Instance Refresh
Declarative production setup orchestrating canary rolling updates and scale-in connection draining.

```hcl
resource "aws_launch_template" "api_template" {
  name_prefix   = "api-worker-"
  image_id      = var.golden_ami_id
  instance_type = "c7g.large"

  iam_instance_profile {
    arn = var.ec2_instance_profile_arn
  }

  network_interfaces {
    associate_public_ip_address = false
    security_groups             = [var.app_security_group_id]
  }

  metadata_options {
    http_endpoint               = "enabled"
    http_tokens                 = "required"
    http_put_response_hop_limit = 1
  }

  user_data = base64encode(file("${path.module}/bootstrap.sh"))
}

resource "aws_autoscaling_group" "api_asg" {
  name_prefix         = "api-asg-"
  vpc_zone_identifier = var.private_subnet_ids
  min_size            = 2
  max_size            = 8
  desired_capacity    = 2
  health_check_type   = "ELB"
  health_check_grace_period = 300

  target_group_arns = [var.alb_target_group_arn]

  launch_template {
    id      = aws_launch_template.api_template.id
    version = "$Latest"
  }

  # Zero-downtime rolling update configuration
  instance_refresh {
    strategy = "Rolling"
    preferences {
      min_healthy_percentage = 50
      instance_warmup        = 300
    }
    triggers = ["tag"]
  }

  tag {
    key                 = "Name"
    value               = "prod-api-instance"
    propagate_at_launch = true
  }
}

resource "aws_autoscaling_lifecycle_hook" "terminating_hook" {
  name                   = "graceful-shutdown-hook"
  autoscaling_group_name = aws_autoscaling_group.api_asg.name
  default_result         = "CONTINUE"
  heartbeat_timeout      = 120
  lifecycle_transition   = "autoscaling:EC2_INSTANCE_TERMINATING"
}
```

### Example 3: Graceful Termination Script Executing Inside EC2
Catches the termination lifecycle event, drains local application state, and signals completion back to Auto Scaling.

```bash
#!/bin/bash
set -euo pipefail

# 1. Fetch instance ID and region via IMDSv2
TOKEN=$(curl -s -S -X PUT "http://169.254.169.254/latest/api/token" -H "X-aws-ec2-metadata-token-ttl-seconds: 60")
INSTANCE_ID=$(curl -s -S -H "X-aws-ec2-metadata-token: $TOKEN" http://169.254.169.254/latest/meta-data/instance-id)
AZ=$(curl -s -S -H "X-aws-ec2-metadata-token: $TOKEN" http://169.254.169.254/latest/meta-data/placement/availability-zone)
REGION="${AZ::-1}"

ASG_NAME="prod-api-asg"
HOOK_NAME="graceful-shutdown-hook"

echo "Executing graceful drain for $INSTANCE_ID..."

# 2. Stop application gracefully to prevent new connections
systemctl stop api-worker.service

# 3. Flush local telemetry/logs to CloudWatch/S3
sync

# 4. Notify Auto Scaling Group that cleanup is finished
aws autoscaling complete-lifecycle-action \
  --lifecycle-action-result CONTINUE \
  --instance-id "$INSTANCE_ID" \
  --lifecycle-hook-name "$HOOK_NAME" \
  --auto-scaling-group-name "$ASG_NAME" \
  --region "$REGION"

echo "Termination hook completed successfully."
```

### Example 4: Orchestrating an ASG Instance Refresh via CLI
Roll out a newly published AMI version across all running instances with 100% minimum health guarantees.

```bash
aws autoscaling start-instance-refresh \
  --auto-scaling-group-name "prod-api-asg" \
  --preferences '{
    "MinHealthyPercentage": 100,
    "InstanceWarmup": 180,
    "CheckpointDelay": 600,
    "ScaleInProtectedInstances": "Ignore",
    "StandbyInstances": "Ignore"
  }' \
  --desired-configuration '{
    "LaunchTemplate": {
      "LaunchTemplateName": "prod-web-template",
      "Version": "$Latest"
    }
  }'
```

### Example 5: SQS Queue Depth Backlog-Per-Instance Scaling
Scale EC2 worker fleets based on an SQS queue's pending message depth per instance.

```bash
# 1. Custom CloudWatch metric math: BacklogPerInstance = ApproximateNumberOfMessagesVisible / ASG_Capacity
# 2. Configure target tracking with customized metric specification
aws autoscaling put-scaling-policy \
  --auto-scaling-group-name "sqs-worker-asg" \
  --policy-name "sqs-backlog-scaling" \
  --policy-type "TargetTrackingScaling" \
  --target-tracking-configuration '{
    "CustomizedMetricSpecification": {
      "MetricDataQueries": [
        {
          "Id": "m1",
          "MetricStat": {
            "Metric": {
              "Namespace": "AWS/SQS",
              "MetricName": "ApproximateNumberOfMessagesVisible",
              "Dimensions": [{"Name": "QueueName", "Value": "OrderProcessingQueue"}]
            },
            "Stat": "Sum"
          },
          "ReturnData": false
        },
        {
          "Id": "m2",
          "MetricStat": {
            "Metric": {
              "Namespace": "AWS/AutoScaling",
              "MetricName": "GroupInServiceInstances",
              "Dimensions": [{"Name": "AutoScalingGroupName", "Value": "sqs-worker-asg"}]
            },
            "Stat": "Average"
          },
          "ReturnData": false
        },
        {
          "Id": "e1",
          "Expression": "m1 / m2",
          "Label": "BacklogPerInstance",
          "ReturnData": true
        }
      ]
    },
    "TargetValue": 100.0
  }'
```

---

## 6. Best Practices

1. **Always Set `health-check-type` to `ELB`**: The default `EC2` health check only monitors hypervisor hardware status. If your web process crashes or enters a 500-error deadlock while the VM stays booted, the ASG will never replace it unless configured with `ELB` health checks.
2. **Tune Health Check Grace Periods**: Set `health-check-grace-period` (e.g., 300 seconds) long enough to accommodate OS boot, package installs, and warm-up. If set too short, an instance will be killed as unhealthy before it finishes booting, triggering a continuous terminate-and-launch death loop.
3. **Use Spot with `price-capacity-optimized`**: In mixed instances policies, always select `price-capacity-optimized`. This strategy evaluates historical Spot pools and allocates from pools with the highest capacity depth, minimizing the risk of Spot interruptions.
4. **Decouple Dynamic Scaling Using Target Tracking**: Avoid writing complicated multi-threshold Step Scaling policies whenever a standard metric (CPU, ALB Request Count, Network In) exists. Target Tracking continuously adjusts the scale-out curve dynamically.
5. **Protect Against Scale-In During Heavy Batch Jobs**: For batch worker nodes processing hours-long calculations, enable **Scale-In Protection** (`SetInstanceProtection`) while a task runs, and remove protection once the unit of work completes.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Using `EC2` Health Checks for Web Services** | Deadlocked application servers that return HTTP 500 errors remain in service indefinitely. | Change `health_check_type` to `ELB` so failing HTTP target group probes trigger instance replacement. |
| **Short Grace Period Leading to Death Loops** | Instances killed after 30 seconds while bootstrap needs 90 seconds, causing continuous cycling and billing churn. | Benchmark bootstrap time and set `health_check_grace_period` to bootstrap time + 120 seconds buffer. |
| **Hardcoding Single Instance Family in Spot Policies** | When a specific instance family runs out of Spot capacity in an AZ, scale-out requests fail entirely. | Provide at least 3–5 alternative instance families in `Overrides` (e.g., `c7g.large`, `c6g.large`, `m7g.large`). |
| **Omitting Scale-In Cooldowns** | Fleet rapidly scales out on a quick 30-second traffic burst, then terminates immediately, flapping continuously. | Set scale-in cooldowns or enable `DisableScaleIn: true` on target tracking policies during unpredictable spikes. |
| **Forgetting to Update Default Template Version** | Deploying a new Launch Template version but instances continue booting old AMIs because the ASG points to `$Default`. | Configure ASG `LaunchTemplate.Version = "$Latest"` or explicitly advance the default version in CI/CD pipelines. |
| **Ignoring Default Lifecycle Hook Timeout** | The lifecycle hook times out after 3600 seconds, leaving instances stuck in `Pending:Wait` or failing silently. | Always deliver `CompleteLifecycleAction` explicitly in scripts or handle timeout signals gracefully. |

---

## 8. Performance, Security & Cost Notes

### Elasticity & Latency Characteristics
- **Instance Warmup**: Modern Linux VMs take 45–90 seconds to boot, configure networking, and pass load balancer target group checks. If traffic surges within seconds, consider maintaining a higher baseline `Min` capacity or using serverless containers (Lambda/Fargate).
- **Scale-In Termination Policy**: Default ASG termination policy balances instances across Availability Zones first. It finds the AZ with the most instances, selects instances with the oldest launch template, and terminates the closest to the next billing hour.

### Security Enforcements
- **Enforce IMDSv2 via Launch Template**: Always declare `"HttpTokens": "required"` and `"HttpPutResponseHopLimit": 1` in the Launch Template metadata options.
- **Keyless Management**: Never bake SSH private keys or deploy instances with key pairs. Rely on IAM Instance Profiles with AWS Systems Manager Session Manager.

### Cost Optimization Mechanics
- **Mixed Instances Allocation**: Set `OnDemandBaseCapacity = 2` to guarantee quorum/stability, and satisfy remaining capacity with 100% Spot instances for an aggregate 70–80% compute cost reduction.

---

## 9. Comparison Table

| Scaling Metric / Type | Target Tracking | Step Scaling | Scheduled Scaling | Predictive Scaling |
|:---|:---|:---|:---|:---|
| **Trigger Mechanism** | Continuous feedback loop | CloudWatch Alarm steps | Cron expression | ML forecast model |
| **Complexity** | Minimal (set 1 target value) | High (configure steps & alarms) | Low (fixed time windows) | Low (enable and train) |
| **Ideal Workload** | Smooth web traffic / ALB requests | Batch queue spikes (SQS) | Regular business hours | Cyclic seasonal traffic |
| **Lead Time** | Reactive (after metric rises) | Reactive (after alarm triggers) | Proactive (exact schedule) | Proactive (launches ahead of curve) |
| **Cooldown Handling** | Automatic mathematical damping | Manual cooldown per step | N/A | Continuously self-tuning |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the sequence of events when an ASG scales in an instance behind an Application Load Balancer?</strong></summary>

**Answer:**
1. A scale-in policy or instance refresh triggers termination.
2. The ASG deregisters the target instance from the ALB Target Group.
3. The Target Group enters the **Deregistration Delay** (connection draining) period (default 300 seconds). In-flight requests are allowed to complete, while new requests are routed to surviving instances.
4. If a termination lifecycle hook (`autoscaling:EC2_INSTANCE_TERMINATING`) is attached, the instance transitions into `Terminating:Wait`.
5. Application shutdown scripts execute (drain background threads, flush logs).
6. Script calls `CompleteLifecycleAction` with `CONTINUE`.
7. The instance transitions to `Terminating:Proceed` and is destroyed by the hypervisor.
</details>

<details>
<summary><strong>Q2: Why does setting `health-check-type` to `ELB` prevent customer-facing downtime during application crashes?</strong></summary>

**Answer:**
The default `EC2` health check only verifies hardware hypervisor status (1/2 or 2/2 system checks). If the application server process (e.g., Node.js or Gunicorn) crashes, throws unhandled exceptions, or runs out of database connections, the operating system remains booted and the `EC2` check reports healthy.

When set to `ELB`, the ASG monitors the results of the Load Balancer's target group HTTP health checks (e.g., `GET /healthz == 200`). If consecutive HTTP probes fail, the target group marks the target `unhealthy`, and the ASG initiates an immediate instance replacement.
</details>

<details>
<summary><strong>Q3: How do Mixed Instances Policies combine Spot and On-Demand capacity safely?</strong></summary>

**Answer:**
A Mixed Instances Policy allows an ASG to launch multiple instance types (e.g., `c7g.large`, `c6g.large`, `m7g.large`) across both On-Demand and Spot markets.
- **On-Demand Base**: Guarantees a minimum number of non-interruptible instances for core baseline load.
- **Percentage Split**: Defines the split for instances launched above the base (e.g., 20% On-Demand, 80% Spot).
- **Allocation Strategy**: Setting `price-capacity-optimized` ensures Spot instances are provisioned from the pools with the deepest available capacity, minimizing the probability of receiving a 2-minute Spot termination notification.
</details>

<details>
<summary><strong>Q4: What is an ASG Instance Refresh and what does `MinHealthyPercentage` guarantee?</strong></summary>

**Answer:**
Instance Refresh orchestrates a rolling update of an existing ASG when a new AMI or Launch Template version is released.
`MinHealthyPercentage` (e.g., 100% or 50%) specifies the minimum percentage of the group's desired capacity that must remain in the `InService` and healthy state throughout the refresh process. Setting it to 100% forces the ASG to scale out new instances first, verify health, and only then terminate old instances, guaranteeing zero capacity reduction.
</details>

<details>
<summary><strong>Q5: What happens if an instance launch lifecycle hook reaches its heartbeat timeout before receiving a completion signal?</strong></summary>

**Answer:**
The ASG acts according to the hook's `DefaultResult` parameter:
- If `DefaultResult` is **`ABANDON`**: The ASG terminates the stalled instance, increments the failure counter, and launches a replacement instance to attempt the bootstrap process again.
- If `DefaultResult` is **`CONTINUE`**: The ASG ignores the lack of response, moves the instance directly into the `InService` state, and registers it with the load balancer target group regardless of bootstrap completion. In production, `ABANDON` is standard to avoid routing traffic to partially-configured instances.
</details>

<details>
<summary><strong>Q6: How does Predictive Scaling differ from standard Target Tracking?</strong></summary>

**Answer:**
Target Tracking is **reactive**: traffic must first arrive, causing CPU or request metrics to increase and cross a threshold before the ASG launches additional instances. Because EC2 instances require 1–3 minutes to boot, users may experience latency during sudden spikes.

Predictive Scaling is **proactive**: it runs machine learning models over at least 24 hours (ideally 14 days) of historical CloudWatch data to forecast daily and weekly traffic curves. It automatically schedules capacity increases 15–30 minutes before expected surges, ensuring instances are booted and healthy before the spike hits.
</details>

<details>
<summary><strong>Q7: What is the purpose of Scale-In Protection and when should it be applied?</strong></summary>

**Answer:**
Scale-in protection prevents an individual instance within an ASG from being terminated during automated scale-in events.
It is typically applied programmatically by long-running background worker processes (e.g., video rendering or financial data batch processing) upon picking up a job from a queue. Once the task finishes, the worker script removes scale-in protection so the ASG can terminate the instance if capacity drops.
</details>

<details>
<summary><strong>Q8: What is the Cooldown Period in EC2 Auto Scaling and why does Target Tracking not strictly require it?</strong></summary>

**Answer:**
A cooldown period is a static timer (e.g., 300 seconds) during which simple scaling policies suspend all further scaling activities, allowing newly launched instances time to stabilize and absorb traffic before re-evaluating alarms.

Target Tracking replaces static cooldowns with continuous mathematical rate-of-change algorithms. It calculates the exact instance delta needed and uses built-in scale-in stabilization windows (default 300s) to dampen scale-in actions while permitting immediate scale-out responses if traffic continues to surge.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Versioned Launch Template via CLI</strong></summary>

**Objective**: Declare a Launch Template enforcing IMDSv2 and GP3 storage.

**Command**:
```bash
aws ec2 create-launch-template \
  --launch-template-name "lab-web-template" \
  --version-description "v1" \
  --launch-template-data '{
    "ImageId": "ami-0c7217cdde317cfec",
    "InstanceType": "t4g.micro",
    "MetadataOptions": {
      "HttpTokens": "required",
      "HttpPutResponseHopLimit": 1
    }
  }'
```
</details>

<details>
<summary><strong>Exercise 2: Attach a Target Tracking Scaling Policy</strong></summary>

**Objective**: Automatically maintain 50% CPU across a running ASG.

**Command**:
```bash
aws autoscaling put-scaling-policy \
  --auto-scaling-group-name "lab-asg" \
  --policy-name "cpu-50-target-tracking" \
  --policy-type "TargetTrackingScaling" \
  --target-tracking-configuration '{
    "PredefinedMetricSpecification": {
      "PredefinedMetricType": "ASGAverageCPUUtilization"
    },
    "TargetValue": 50.0
  }'
```
</details>

<details>
<summary><strong>Exercise 3: Configure a Termination Lifecycle Hook</strong></summary>

**Objective**: Grant 180 seconds for instances to drain connections before hypervisor termination.

**Command**:
```bash
aws autoscaling put-lifecycle-hook \
  --lifecycle-hook-name "drain-connections-hook" \
  --auto-scaling-group-name "lab-asg" \
  --lifecycle-transition "autoscaling:EC2_INSTANCE_TERMINATING" \
  --heartbeat-timeout 180 \
  --default-result "CONTINUE"
```
</details>

<details>
<summary><strong>Exercise 4: Trigger an ASG Instance Refresh</strong></summary>

**Objective**: Execute a rolling upgrade with 100% minimum health.

**Command**:
```bash
aws autoscaling start-instance-refresh \
  --auto-scaling-group-name "lab-asg" \
  --preferences '{"MinHealthyPercentage": 100, "InstanceWarmup": 120}'
```
</details>

<details>
<summary><strong>Exercise 5: Simulate CPU Spike and Monitor Fleet Elasticity</strong></summary>

**Objective**: Induce 100% CPU on an instance and observe ASG scale-out.

**Steps**:
1. Connect via SSM: `aws ssm start-session --target <INSTANCE_ID>`.
2. Run stress: `stress-ng --cpu 2 --timeout 600s`.
3. Monitor ASG activity: `aws autoscaling describe-scaling-activities --auto-scaling-group-name lab-asg`.
4. Observe CloudWatch alarm transition to `ALARM` state and new instances launching.
</details>

<details>
<summary><strong>Exercise 6: Test Spot Interruption Handling via AWS FIS</strong></summary>

**Objective**: Inject Spot termination notices using AWS Fault Injection Simulator (FIS) and verify that the ASG immediately provisions replacements.

**Steps**:
1. Create an AWS FIS experiment targeting your ASG's Spot instances with `aws:ec2:send-spot-instance-interruptions`.
2. Run experiment.
3. Observe termination lifecycle hooks and replacement instance provisioning in CloudTrail and Auto Scaling logs.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                             Auto Scaling Component Checklist                          |
+---------------------------------------------------------------------------------------+
|  Component             | Production Standard               | Anti-Pattern             |
|------------------------+-----------------------------------+--------------------------|
|  Instance Definition   | Launch Template (Versioned)       | Launch Configuration     |
|  Health Check Type     | ELB (Monitors HTTP endpoints)     | EC2 (Hardware only)      |
|  Fleet Architecture    | Multi-AZ (Minimum 3 Subnets)      | Single AZ deployment     |
|  Cost Optimization     | Mixed Instances Policy (Spot/OD)  | 100% On-Demand Static    |
|  Scale-out Strategy    | Target Tracking (Dynamic)         | Fixed manual capacity    |
|  Upgrades              | Instance Refresh (Rolling)        | Terminate-all recreation |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [AWS Auto Scaling User Guide](https://docs.aws.amazon.com/autoscaling/ec2/userguide/what-is-amazon-ec2-auto-scaling.html)
- [Amazon EC2 Launch Templates Documentation](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/ec2-launch-templates.html)
- [Target Tracking Scaling Policies for Amazon EC2 Auto Scaling](https://docs.aws.amazon.com/autoscaling/ec2/userguide/as-scaling-target-tracking.html)
- [Using AWS Auto Scaling Lifecycle Hooks](https://docs.aws.amazon.com/autoscaling/ec2/userguide/lifecycle-hooks.html)
- [Rolling Updates with Auto Scaling Instance Refresh](https://docs.aws.amazon.com/autoscaling/ec2/userguide/asg-instance-refresh.html)
