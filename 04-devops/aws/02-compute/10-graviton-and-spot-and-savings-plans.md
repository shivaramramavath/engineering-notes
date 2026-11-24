# AWS Graviton, Spot Instances, and Compute Savings Plans

> **Module**: `02-compute`  
> **Topic**: Advanced Compute Cost Optimization, Processor Architecture, and Capacity Commitments  
> **Target Audience**: FinOps Practitioners, Cloud Architects, Principal DevOps Engineers  
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

In enterprise cloud infrastructure, compute capacity typically accounts for **60% to 80% of total AWS expenditures**. Operating unoptimized compute fleets—relying entirely on default On-Demand pricing, legacy x86 hardware, and static instance sizing—results in massive budget waste that compounds as organizations scale.

Achieving structural compute cost optimization requires combining three complementary strategies:
1. **Architectural Efficiency (AWS Graviton)**: Migrating from traditional x86_64 processors (Intel/AMD) to custom AWS ARM-based Silicon (Graviton3 and Graviton4), delivering up to **40% better price-performance** and 60% lower energy consumption for identical workloads.
2. **Opportunistic Fleet Sizing (EC2 Spot Instances)**: Purchasing spare, unallocated AWS compute capacity at up to a **90% discount** compared to On-Demand rates for stateless, fault-tolerant, or batch workloads.
3. **Financial Commitment Modeling (Savings Plans)**: Committing to a consistent hourly compute spend ($/hour) over a 1- or 3-year term to lock in discounts up to **72%** across EC2, Fargate, and Lambda without sacrificing architectural flexibility.

---

## 2. Core Concepts

### 2.1 AWS Graviton Silicon Generations
AWS custom-designs ARM-based server processors powered by the AWS Nitro System:
- **Graviton2 (e.g., `c6g`, `m6g`, `r6g`, `t4g`)**: 64-bit Arm Neoverse N1 cores; 4x cache; 7nm process.
- **Graviton3 / 3E (e.g., `c7g`, `m7g`, `r7g`)**: Neoverse V1 cores; DDR5 memory support (50% more bandwidth); dedicated FP16/bfloat16 vector instructions. 25% faster compute performance over Graviton2.
- **Graviton4 (e.g., `r8g`, `c8g`)**: Neoverse V2 cores; up to 96 physical cores; 3x memory bandwidth; encrypted high-speed PCIe. Up to 30% faster compute than Graviton3.

### 2.2 EC2 Spot Architecture & Interruption Mechanics
AWS maintains vast pools of spare hardware across global Availability Zones to accommodate on-demand spikes:
- **Spot Price**: Determined by long-term supply and demand dynamics across independent **Spot Capacity Pools** (a unique combination of instance type, operating system, and Availability Zone).
- **Interruption Warning**: When AWS requires capacity back for On-Demand customers, it issues a **2-minute termination warning** via:
  - Instance Metadata Service (`http://169.254.169.254/latest/meta-data/spot/instance-action`).
  - Amazon EventBridge (`EC2 Spot Instance Interruption Warning` event).
- **Allocation Strategies**:
  - `price-capacity-optimized` (Recommended): Balances lowest price with deepest pool availability to minimize interruptions.
  - `capacity-optimized`: Allocates purely from the deepest pools to maximize workload stability.

### 2.3 Savings Plans Hierarchy
Savings Plans replaced legacy Reserved Instances (RIs) by decoupling discounts from static physical instances:
- **Compute Savings Plans**:
  - Maximum flexibility: automatically applies to **any EC2 instance** (regardless of family, size, region, OS, or tenancy), **AWS Fargate**, and **AWS Lambda**.
  - Up to **66% discount**.
- **EC2 Instance Savings Plans**:
  - Pinned to a specific **instance family in a specific region** (e.g., `c7g` in `us-east-1`).
  - Allows instance size flexibility (e.g., automatically shifts between `c7g.xlarge` and `c7g.2xlarge`) and OS flexibility.
  - Higher discount: up to **72%**.
- **Payment Options**: No Upfront, Partial Upfront, All Upfront (higher upfront payment yields deeper percentage discounts).

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        AWS Compute Cost Optimization Engine                       |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ Workload Ingestion & Classification ]                                          |
|            |                                                                      |
|            +-----------------------------------+------------------------------+   |
|            | Stateful / Non-Interruptible      | Stateless / Resilient Worker |   |
|            v                                   v                              v   |
|  +-----------------------------------+   +------------------------------------+   |
|  | Baseline On-Demand / Commitments  |   | EC2 Spot / Fargate Spot Fleets     |   |
|  +-----------------------------------+   +------------------------------------+   |
|            |                                   |                                  |
|            v                                   v                                  |
|  +-----------------------------------+   +------------------------------------+   |
|  | Modernize to AWS Graviton (ARM64) |   | Diversified Spot Pools             |   |
|  | (c7g / m7g / r7g / t4g)           |   | (Strategy: price-capacity-optimzd) |   |
|  | - 20% lower cost, 25% higher IPC  |   | - Up to 90% discount               |   |
|  +-----------------------------------+   +------------------------------------+   |
|            |                                   |                                  |
|            v                                   v                                  |
|  +-----------------------------------------------------------------------------+  |
|  |                   AWS Billing Engine Commitment Application                 |  |
|  |                                                                             |  |
|  |  1. Measure total steady-state hourly spend (e.g., $50/hour committed)      |  |
|  |  2. Apply EC2 Instance Savings Plans (highest discount, up to 72%)          |  |
|  |  3. Apply Compute Savings Plans (flexible discount across EC2/Fargate/Lmb)  |  |
|  |  4. Remaining non-committed burst compute billed at On-Demand rates         |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

### Spot Interruption Event Sequence
```
[T - 120s] AWS issues Spot Interruption Notice
           ├── Emits EventBridge Event: "EC2 Spot Instance Interruption Warning"
           └── IMDS returns action: "terminate" at /spot/instance-action
                │
                v
[T - 90s]  Application drains HTTP connections / signals graceful shutdown
           └── Auto Scaling Group detaches target from ALB
                │
                v
[T - 10s]  Local cache flushed, logs uploaded to CloudWatch/S3
                │
                v
[T = 0s]   AWS Hypervisor terminates EC2 Spot instance
```

---

## 4. Syntax & Usage

### 4.1 Building Multi-Architecture Docker Images for Graviton
Compile Docker containers for both AMD64 and ARM64 (Graviton) using Docker Buildx:
```bash
# 1. Initialize Docker Buildx multi-architecture builder
docker buildx create --name multiarch-builder --use
docker buildx inspect --bootstrap

# 2. Build and push image for both x86_64 and arm64 architectures
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  -t 111122223333.dkr.ecr.us-east-1.amazonaws.com/api-service:v2.0 \
  --push .
```

### 4.2 Handling Spot Interruption Signals via Python Daemon
Polling the Instance Metadata Service (IMDSv2) for the 2-minute interruption notice:

```python
import time
import requests
import subprocess
import logging

logging.basicConfig(level=logging.INFO)

def get_imds_token():
    url = "http://169.254.169.254/latest/api/token"
    headers = {"X-aws-ec2-metadata-token-ttl-seconds": "60"}
    res = requests.put(url, headers=headers, timeout=2)
    return res.text

def monitor_spot_interruption():
    token = get_imds_token()
    action_url = "http://169.254.169.254/latest/meta-data/spot/instance-action"
    
    while True:
        try:
            res = requests.get(
                action_url,
                headers={"X-aws-ec2-metadata-token": token},
                timeout=2
            )
            if res.status_code == 200:
                action_data = res.json()
                logging.warning(f"SPOT INTERRUPTION DETECTED! Action: {action_data}")
                execute_graceful_shutdown()
                break
        except requests.exceptions.RequestException:
            pass # 404 means no interruption scheduled yet
            
        time.sleep(5)

def execute_graceful_shutdown():
    logging.info("Draining connections and flushing application state...")
    subprocess.run(["systemctl", "stop", "api-worker.service"])
    subprocess.run(["sync"])
    logging.info("Shutdown completed.")

if __name__ == "__main__":
    monitor_spot_interruption()
```

---

## 5. Practical Examples

### Example 1: Mixed Instances Auto Scaling Group with Graviton & Spot
Terraform configuration provisioning a production-grade Auto Scaling Group leveraging Graviton Spot instances with On-Demand base capacity.

```hcl
resource "aws_launch_template" "graviton_template" {
  name_prefix   = "graviton-api-"
  image_id      = var.arm64_al2023_ami_id
  instance_type = "c7g.large"

  metadata_options {
    http_endpoint               = "enabled"
    http_tokens                 = "required"
    http_put_response_hop_limit = 1
  }

  tag_specifications {
    resource_type = "instance"
    tags = {
      Environment = "production"
      Arch        = "arm64"
    }
  }
}

resource "aws_autoscaling_group" "optimized_asg" {
  name                = "production-cost-optimized-asg"
  vpc_zone_identifier = var.private_subnet_ids
  min_size            = 2
  max_size            = 12
  desired_capacity    = 4

  mixed_instances_policy {
    launch_template {
      launch_template_specification {
        launch_template_id = aws_launch_template.graviton_template.id
        version            = "$Latest"
      }
      override { instance_type = "c7g.large" }
      override { instance_type = "m7g.large" }
      override { instance_type = "c6g.large" }
      override { instance_type = "m6g.large" }
    }

    instances_distribution {
      on_demand_base_capacity                  = 2
      on_demand_percentage_above_base_capacity = 25
      spot_allocation_strategy                 = "price-capacity-optimized"
    }
  }
}
```

### Example 2: EventBridge Rule Capturing Spot Interruption Notices
Capture Spot interruptions across all accounts and invoke a Lambda drain hook.

```hcl
resource "aws_cloudwatch_event_rule" "spot_interruption" {
  name        = "capture-spot-interruption"
  description = "Alerts and triggers draining when EC2 Spot instances receive termination notices"

  event_pattern = jsonencode({
    source      = ["aws.ec2"]
    detail-type = ["EC2 Spot Instance Interruption Warning"]
  })
}

resource "aws_cloudwatch_event_target" "sns_alert" {
  rule      = aws_cloudwatch_event_rule.spot_interruption.name
  target_id = "SendToSNS"
  arn       = var.ops_alerts_sns_arn
}
```

### Example 3: Purchasing a Compute Savings Plan via AWS CLI
Commit to $25.00/hour of compute usage for 3 years with No Upfront payment.

```bash
aws savingsplans purchase-savings-plan \
  --savings-plan-offering-id "offer-12345678-abcd-ef01-2345-6789abcdef01" \
  --commitment "25.00" \
  --upfront-payment-amount "0.00"
```

### Example 4: Evaluating Savings Plans Utilization & Coverage via CLI
FinOps audit commands querying current commitment efficiency.

```bash
# 1. Query Savings Plans Coverage (Percentage of eligible compute covered by plans)
aws ce get-savings-plans-coverage \
  --time-period Start=2026-10-01,End=2026-10-31 \
  --granularity MONTHLY \
  --metrics "CoveragePercentage"

# 2. Query Savings Plans Utilization (Percentage of purchased commitment consumed)
aws ce get-savings-plans-utilization \
  --time-period Start=2026-10-01,End=2026-10-31 \
  --granularity MONTHLY
```

### Example 5: Graviton Migration Compiler & Performance Optimization
Configuring GCC/Clang flags for C/C++/Go/Rust binaries targeting AWS Graviton3.

```bash
# Compile with Graviton3 specific Neoverse V1 tuning
gcc -O3 -march=armv8.4-a+crypto+fp16 -mcpu=neoverse-v1 -o benchmark benchmark.c

# For Go runtime (natively supports arm64 compilation):
GOOS=linux GOARCH=arm64 go build -ldflags="-s -w" -o api-worker main.go
```

---

## 6. Best Practices

1. **Migrate Stateless Runtimes to Graviton First**: Modern interpreted and managed runtimes (Python, Node.js, Java 17+, Go, .NET Core) run on ARM64 with zero code modifications. Migrating these runtimes to Graviton3/Graviton4 (`c7g`, `m7g`) yields an instant **20% direct cost reduction** and ~25% throughput increase.
2. **Always Use `price-capacity-optimized` for Spot**: Avoid legacy `lowest-price` Spot strategies. The `price-capacity-optimized` strategy evaluates pool depth in real time, drastically reducing the probability of receiving 2-minute interruption notices.
3. **Diversify Spot Pools Across At Least 4–6 Instance Types**: When configuring Auto Scaling Groups or EKS Karpenter NodePools with Spot, provide multiple instance generations and sizes (e.g., `c7g.large`, `c6g.large`, `m7g.large`, `c7a.large`). If one pool runs out of capacity, AWS launches instances from alternative pools seamlessly.
4. **Target 80–85% Savings Plans Coverage, Not 100%**: Never purchase Savings Plans to cover 100% of your peak compute workload. Aim for 80–85% coverage of your historical baseline trough to prevent over-commitment during traffic dips or infrastructure refactoring.
5. **Stack Compute Savings Plans on Top of Spot**: Use Savings Plans exclusively to cover non-interruptible baseline capacity (databases, stateful caches, core API instances). Satisfy all burst, batch, and variable scale-out capacity with Spot instances.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Buying 100% EC2 Instance Savings Plans** | Locked into a single instance family in one region; cannot adopt Graviton or shift to Fargate without paying for wasted commitments. | Purchase **Compute Savings Plans** for broader architectural flexibility across EC2, Fargate, and Lambda. |
| **Over-Committing on Savings Plans** | If infrastructure is optimized or decommissioned, unused commitment hours are billed at full rate. | Target 80% coverage of steady-state baseline; evaluate 30-day moving averages in AWS Cost Explorer. |
| **Using Spot for Monolithic Databases** | Databases crash ungracefully during Spot interruptions, causing data corruption and failover churn. | Restrict Spot to stateless web tiers, batch workers, and distributed microservices. Run databases on On-Demand/Savings Plans. |
| **Deploying Single Instance Family on Spot** | Scale-out fails completely during localized AZ capacity shortages. | Diversify launch template overrides across multiple instance families (`c7g`, `c6g`, `m7g`, `m6g`). |
| **Ignoring IMDSv2 Interruption Checks** | Workloads are hard-killed without draining in-flight transactions or flushing logs. | Implement a background daemon or EventBridge rule listening for the 2-minute interruption warning. |
| **Assuming x86 Docker Images Run on Graviton** | Containers crash with `exec format error` when deployed onto ARM64 instances. | Build multi-architecture container images (`linux/amd64` and `linux/arm64`) using Docker Buildx. |

---

## 8. Performance, Security & Cost Notes

### Architecture & Memory Bandwidth
- **DDR5 Memory on Graviton3/4**: Delivers 50% higher memory bandwidth than DDR4 used in 6th-gen Intel/AMD instances, drastically accelerating memory-intensive databases (Redis, PostgreSQL) and analytics pipelines.
- **Dedicated Crypto Acceleration**: Graviton processors feature dedicated crypto acceleration engines, doubling performance for TLS 1.3 handshakes, AES encryption, and SHA hashing.

### Security Isolation
- **Hardware Memory Encryption**: Graviton3 and Graviton4 enforce hardware-level AES-128 memory encryption directly in silicon, isolating CPU memory from hypervisor snooping.

### Cost Savings Matrix
- **Graviton Migration**: Flat 20% hourly savings per instance + ~20% performance boost = **~40% price-performance gain**.
- **Spot Instances**: Up to **90% discount** off standard On-Demand pricing.
- **Compute Savings Plans**: Up to **66% discount** on 3-year All Upfront commitments.

---

## 9. Comparison Table

| Dimension | On-Demand Instances | EC2 Spot Instances | Compute Savings Plans |
|:---|:---|:---|:---|
| **Discount Level** | 0% (Standard baseline) | Up to 90% | Up to 66% |
| **Commitment Term** | None (Pay per second) | None (Pay per second) | 1 Year or 3 Years |
| **Interruption Risk** | Zero | Yes (2-minute warning) | Zero |
| **Workload Suitability**| Unpredictable / Stateful | Stateless / Batch / Resilient | Stable baseline compute |
| **Service Scope** | EC2 only | EC2 / Fargate | EC2, AWS Fargate, AWS Lambda |
| **Cancellation Policy** | Terminate anytime | Terminate anytime | Non-cancelable commitment |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What are the primary architectural benefits of AWS Graviton processors compared to x86_64 alternatives?</strong></summary>

**Answer:**
1. **Price-Performance**: Graviton instances cost approximately 20% less per vCPU-hour than comparable Intel/AMD instances while delivering 20–25% higher performance (IPC), yielding a net ~40% price-performance improvement.
2. **Physical Cores vs. Hyper-Threading**: Standard x86 processors use Simultaneous Multi-Threading (hyper-threading), where 1 physical core presents as 2 logical vCPUs sharing execution units. Graviton allocates 1 vCPU per **dedicated physical core**, eliminating "noisy neighbor" core contention and delivering consistent, predictable latency.
3. **Memory Bandwidth**: Graviton3/4 leverage cutting-edge DDR5 memory, delivering up to 50% more bandwidth than legacy DDR4 systems.
4. **Energy Efficiency**: Custom ARM architecture consumes up to 60% less energy, reducing thermal throttling and aligning with corporate sustainability targets.
</details>

<details>
<summary><strong>Q2: How should an enterprise handle the 2-minute EC2 Spot interruption notice?</strong></summary>

**Answer:**
When AWS marks a Spot instance for reclamation, it issues a 2-minute notice via Amazon EventBridge and IMDSv2 (`/spot/instance-action`):
1. **Deregistration Delay**: An automated daemon or EventBridge rule notifies the Application Load Balancer to immediately put the instance into Target Group connection draining, stopping new HTTP traffic.
2. **Workload Draining**: In-flight HTTP transactions are allowed to finish; batch workers stop pulling new messages from SQS.
3. **State Flushing**: In-memory buffers, local caches, and telemetry logs are flushed to S3 or CloudWatch.
4. **Replacement Provisioning**: The Auto Scaling Group detects the capacity reduction and automatically launches a replacement Spot instance in an alternate AZ/pool before the old instance is destroyed.
</details>

<details>
<summary><strong>Q3: What is the difference between Compute Savings Plans and EC2 Instance Savings Plans?</strong></summary>

**Answer:**
- **Compute Savings Plans**: Offer maximum flexibility. They apply automatically to any compute usage across **EC2** (any family, size, region, OS, or tenancy), **AWS Fargate**, and **AWS Lambda**. If you migrate an application from EC2 instances in `us-east-1` to Fargate containers in `eu-west-1`, the discount seamlessly follows the workload. Provides up to **66% discount**.
- **EC2 Instance Savings Plans**: Pinned to a **specific instance family in a specific AWS Region** (e.g., `c7g` in `us-east-1`). You retain size and OS flexibility within that family, but the discount cannot apply to other families, other regions, Fargate, or Lambda. Because of the narrower scope, it offers a higher discount (up to **72%**).
</details>

<details>
<summary><strong>Q4: Why is the `price-capacity-optimized` Spot allocation strategy superior to `lowest-price`?</strong></summary>

**Answer:**
The legacy `lowest-price` allocation strategy selects instances strictly based on the cheapest hourly dollar rate. However, the cheapest Spot pool is often the smallest pool with the highest risk of running out of spare capacity, resulting in frequent, abrupt Spot interruptions.

The **`price-capacity-optimized`** strategy evaluates both real-time pricing and capacity depth across all candidate pools. It selects instances from pools that have significant spare capacity to absorb the workload while still offering deep discounts, reducing average Spot interruption rates by over 50%.
</details>

<details>
<summary><strong>Q5: Can you apply Savings Plans to EC2 Spot Instances?</strong></summary>

**Answer:**
No. Savings Plans apply exclusively to **On-Demand** usage.
Spot instances already receive an automatic market discount of up to 90% and cannot be stacked with Savings Plans. If your entire compute fleet runs on Spot instances, any purchased Savings Plans commitments will sit idle at **0% utilization**, wasting money. In a properly architected FinOps model, Savings Plans cover the 100% On-Demand baseline, while Spot satisfies the dynamic burst tier.
</details>

<details>
<summary><strong>Q6: How do you build and run Docker containers compatible with AWS Graviton?</strong></summary>

**Answer:**
Docker containers built on an x86 machine cannot run natively on ARM64 Graviton instances due to CPU instruction set mismatches (resulting in `exec format error`).
To resolve this:
1. Use **Docker Buildx** with QEMU emulation or native ARM64 CI/CD runners:
   `docker buildx build --platform linux/amd64,linux/arm64 -t repo:tag --push .`
2. Docker creates a multi-architecture manifest image in Amazon ECR.
3. When the container pulls onto an EC2 Graviton host, Docker automatically retrieves the `linux/arm64` image layer, and when pulled onto x86, it retrieves the `linux/amd64` layer.
</details>

<details>
<summary><strong>Q7: What is the optimal target for Savings Plans Coverage in an evolving cloud environment?</strong></summary>

**Answer:**
The recommended FinOps target is **80% to 85% coverage** of your minimum steady-state compute baseline.
Purchasing 100% coverage is an anti-pattern: if engineering teams refactor workloads (e.g., modernizing EC2 monoliths into serverless architecture, downsizing over-provisioned instances, or deleting test environments), compute usage drops below the committed hourly rate, and the organization continues paying for unused commitment hours.
</details>

<details>
<summary><strong>Q8: What happens to a 3-year All Upfront Savings Plan if an organization goes bankrupt or migrates to another cloud?</strong></summary>

**Answer:**
Savings Plans are **strictly non-cancelable and non-refundable** contracts. AWS does not permit canceling a plan or receiving a refund for upfront payments.
Unlike legacy EC2 Reserved Instances (which could be listed on the Reserved Instance Marketplace for resale to other AWS customers), Savings Plans **cannot be resold on the marketplace**. Therefore, 3-year commitments should only be applied to core, permanent enterprise workloads with multi-year organizational backing.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Inspect Spot Interruption Status via IMDSv2</strong></summary>

**Objective**: Query the metadata endpoint inside an EC2 instance to test interruption monitoring.

**Command**:
```bash
TOKEN=$(curl -s -S -X PUT "http://169.254.169.254/latest/api/token" -H "X-aws-ec2-metadata-token-ttl-seconds: 60")

# Query spot action (returns 404 if not scheduled for interruption)
curl -s -S -w "\nHTTP Status: %{http_code}\n" -H "X-aws-ec2-metadata-token: $TOKEN" \
  http://169.254.169.254/latest/meta-data/spot/instance-action
```
</details>

<details>
<summary><strong>Exercise 2: Create a Multi-Architecture Docker Image with Buildx</strong></summary>

**Objective**: Build a dual ARM64/AMD64 container image.

**Commands**:
```bash
docker buildx create --name test-builder --use
docker buildx build --platform linux/amd64,linux/arm64 -t test-app:latest .
```
</details>

<details>
<summary><strong>Exercise 3: Query Real-Time Spot Price History via CLI</strong></summary>

**Objective**: Compare Spot prices for Graviton (`c7g.large`) vs Intel (`c6i.large`) across Availability Zones.

**Command**:
```bash
aws ec2 describe-spot-price-history \
  --instance-types "c7g.large" "c6i.large" \
  --product-descriptions "Linux/UNIX" \
  --start-time $(date -u +%Y-%m-%dT%H:%M:%S) \
  --query 'SpotPriceHistory[*].[InstanceType, AvailabilityZone, SpotPrice]' \
  --output table
```
</details>

<details>
<summary><strong>Exercise 4: Query Savings Plans Recommendations via AWS Cost Explorer</strong></summary>

**Objective**: Retrieve AWS-recommended Savings Plans commitment levels based on past 30 days.

**Command**:
```bash
aws ce get-savings-plans-purchase-recommendation \
  --savings-plans-type COMPUTE_SP \
  --term-in-years THREE_YEARS \
  --payment-option NO_UPFRONT \
  --lookback-period-in-days THIRTY_DAYS
```
</details>

<details>
<summary><strong>Exercise 5: Configure an EventBridge Rule for Spot Termination Alerts</strong></summary>

**Objective**: Route Spot interruption events to an Amazon CloudWatch Log Group.

**Command**:
```bash
aws events put-rule \
  --name "SpotInterruptionRule" \
  --event-pattern '{"source": ["aws.ec2"], "detail-type": ["EC2 Spot Instance Interruption Warning"]}'
```
</details>

<details>
<summary><strong>Exercise 6: Benchmark Graviton3 vs x86 Performance</strong></summary>

**Objective**: Run standard matrix multiplication benchmark to measure Graviton IPC improvements.

**Steps**:
1. Launch one `c7g.large` and one `c6i.large`.
2. Run OpenSSL speed benchmark on both: `openssl speed -evp aes-256-gcm`.
3. Compare throughput in MB/s per core and calculate price-to-performance efficiency.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                          Compute Cost Optimization Triad                              |
+---------------------------------------------------------------------------------------+
|  Strategy              | Target Workload                   | Maximum Savings          |
|------------------------+-----------------------------------+--------------------------|
|  AWS Graviton (ARM64)  | All modern runtimes (Python/Node) | ~40% Price-Performance   |
|  EC2 Spot Instances    | Stateless / Fault-tolerant / Batch| Up to 90% vs On-Demand   |
|  Compute Savings Plans | Steady-state baseline (80% trough)| Up to 66% across EC2/F/L |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [AWS Graviton Processor Technical Specifications](https://aws.amazon.com/ec2/graviton/)
- [Amazon EC2 Spot Instances Best Practices Guide](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/spot-best-practices.html)
- [AWS Savings Plans User Guide](https://docs.aws.amazon.com/savingsplans/latest/userguide/what-is-savings-plans.html)
- [Porting Guide for AWS Graviton](https://github.com/aws/aws-graviton-getting-started)
- [FinOps Foundation: Rate Optimization Framework](https://www.finops.org/framework/capabilities/rate-optimization/)
