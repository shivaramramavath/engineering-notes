# AWS Cost Optimization & FinOps: Cloud Financial Management

> **Module**: `06-operations`  
> **Topic**: Cost Allocation Tagging, Cost Explorer, CUR with Athena, Compute Optimizer & Savings Plans  
> **Target Audience**: FinOps Practitioners, Cloud Architects, Engineering Managers, DevOps Leads  
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

In traditional corporate data centers, infrastructure procurement requires months of capital expenditure (CapEx) approvals, financial depreciation schedules, and hardware sizing for peak capacity years in advance. In the cloud, the paradigm shifts to operating expenditure (OpEx): anyone with IAM permissions can provision thousands of dollars of computing capacity in seconds. Without strict financial governance, idle instances, oversized databases, unattached EBS volumes, and forgotten test environments cause cloud spending to spiral out of control.

**FinOps (Cloud Financial Management)** is an evolving cloud financial management discipline and cultural practice that enables organizations to get maximum business value by helping engineering, finance, technology, and business teams to collaborate on data-driven spending decisions. 

On AWS, FinOps combines automated cost governance tools to:
1. **Drive Cost Accountability**: Attribute 100% of cloud spending to specific business units, applications, and owners using **Cost Allocation Tags**.
2. **Eliminate Waste**: Use machine learning via **AWS Compute Optimizer** to rightsize over-provisioned instances and storage volumes.
3. **Maximize Commitment Discounts**: Leverage **Savings Plans** and **Reserved Instances (RIs)** to achieve up to 72% discounts over On-Demand rates.
4. **Detect Anomalies in Real-Time**: Use **AWS Cost Anomaly Detection** to catch runaway spending within hours rather than waiting for the end-of-month invoice.

---

## 2. Core Concepts

### 2.1 Cost Allocation Tags
- Key-value pairs attached to AWS resources for cost attribution and billing breakdown.
- **Two Categories**:
  - *AWS-Generated Tags*: Automatically created and tracked by AWS (e.g., `aws:createdBy`).
  - *User-Defined Tags*: Custom tags created by engineers (e.g., `Environment: Production`, `CostCenter: CC-9012`, `Owner: TeamAlpha`, `Application: CheckoutService`).
- **Activation Required**: Adding tags to an EC2 instance or S3 bucket does *not* automatically display them in billing reports. Tags must be explicitly **activated** in the AWS Billing & Cost Management console.

### 2.2 Cost Analysis Engines
- **AWS Cost Explorer**: A web UI tool for visualizing, understanding, and managing AWS costs and usage over a rolling 14-month historical period with predictive 3-month forecasting.
- **AWS Cost and Usage Report (CUR)**: The single most comprehensive, granular dataset of AWS cost and usage data available. Delivered to an S3 bucket hourly or daily as CSV or Apache Parquet files, ready for SQL querying via **Amazon Athena** and dashboarding in **Amazon QuickSight**.
- **AWS Cost Anomaly Detection**: An ML-powered service that continuously monitors spending patterns, establishes baseline behavior, and flags statistically abnormal spending spikes with root-cause analysis.

### 2.3 Pricing Commitment Models
- **On-Demand**: Maximum flexibility, zero commitment, highest per-hour cost.
- **Savings Plans**: Flexible commitment to a consistent amount of usage (measured in \$/hour, e.g., \$10.00/hr) for a 1-year or 3-year term:
  - *Compute Savings Plans*: Highest flexibility (up to 66% discount). Applies across EC2 instances (any family, size, region, or OS), AWS Fargate, and AWS Lambda.
  - *EC2 Instance Savings Plans*: Lower flexibility, higher savings (up to 72% discount). Applies to a specific instance family in a specific region (e.g., `c6g` in `us-east-1`).
- **Reserved Instances (RIs)**: Older commitment model for EC2, RDS, ElastiCache, OpenSearch, and Redshift.
- **Spot Instances**: Spare unused AWS compute capacity available at up to **90% discount** compared to On-Demand. AWS can reclaim instances with a 2-minute interruption notice; ideal for stateless, fault-tolerant batch workloads, CI/CD runners, and containerized microservices.

### 2.4 AWS Compute Optimizer
- Analyzes CloudWatch utilization metrics (CPU, memory, storage, network) over 14 to 93 days using machine learning.
- Recommends optimal AWS resources across four categories: EC2 instances, EBS volumes, Lambda functions, and ECS tasks. Identifies whether resources are **Under-provisioned**, **Over-provisioned**, or **Optimized**.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                        AWS FINOPS BILLING PIPELINE & COST ALLOCATION                              |
|                                                                                                   |
|  [All AWS Resources: EC2, S3, RDS, ECS, Lambda]                                                   |
|  - Tagged with: `CostCenter=CC-100`, `Environment=Prod`, `Owner=DataTeam`                         |
|         |                                                                                         |
|         v (Continuous Hourly Usage Telemetry)                                                     |
|  +---------------------------------------------------------------------------------------------+  |
|  | AWS BILLING & COST MANAGEMENT ENGINE                                                        |  |
|  | 1. Evaluates Active Cost Allocation Tags                                                    |  |
|  | 2. Applies Commitment Discounts (Compute Savings Plans applied first to highest % saving)   |  |
|  | 3. Evaluates Machine Learning: Cost Anomaly Detection                                       |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                              |                                          |
|         | (Hourly/Daily Parquet Export)                | (Real-Time Spike Detected!)              |
|         v                                              v                                          |
|  +-----------------------------+               +-----------------------------------------------+  |
|  | S3 CUR BUCKET (Parquet)     |               | COST ANOMALY DETECTION ALERT                  |  |
|  | s3://billing-cur-reports/   |               | Dispatches SNS Alert to FinOps Slack Channel: |  |
|  +--------------+--------------+               | "Spike: SageMaker endpoint spending jumped    |  |
|                 |                              |  from $10/day to $850/day (Root: ml.p4de)!"   |  |
|                 v                              +-----------------------------------------------+  |
|  +-----------------------------+                                                                  |
|  | AMAZON ATHENA (Serverless)  |                                                                  |
|  | SQL Query:                  |                                                                  |
|  | `SELECT cost_center, sum()  |                                                                  |
|  |  FROM cur_table GROUP BY 1` |                                                                  |
|  +--------------+--------------+                                                                  |
|                 |                                                                                 |
|                 v                                                                                 |
|  [AMAZON QUICKSIGHT / FINOPS EXECUTIVE DASHBOARD]                                                 |
|  Showback & Chargeback Visualizations per Team, Unit, and Microservice                            |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 CLI Commands for Budgets, Cost Explorer & Compute Optimizer

```bash
# 1. Query Cost and Usage via Cost Explorer CLI for last 30 days
aws ce get-cost-and-usage \
    --time-period Start=2026-10-01,End=2026-10-31 \
    --granularity MONTHLY \
    --metrics "UnblendedCost" \
    --group-by Type=DIMENSION,Key=SERVICE

# 2. Retrieve Compute Optimizer recommendations for over-provisioned EC2 instances
aws compute-optimizer get-ec2-instance-recommendations \
    --filters name=Finding,values=Overprovisioned

# 3. Create an AWS Budget with 80% threshold email notification
aws budgets create-budget \
    --account-id "123456789012" \
    --budget file://monthly-budget.json \
    --notifications-with-subscribers file://budget-notifications.json
```

---

## 5. Practical Examples

### 5.1 AWS Budget with SNS & Email Alerting (Terraform)

```hcl
# Monthly Cost Budget: $5,000 threshold
resource "aws_budgets_budget" "monthly_spend" {
  name              = "monthly-total-production-budget"
  budget_type       = "COST"
  limit_amount      = "5000"
  limit_unit        = "USD"
  time_unit         = "MONTHLY"
  time_period_start = "2026-01-01_00:00"

  cost_filter {
    name   = "TagKeyValue"
    values = ["user:Environment$Production"]
  }

  # Alert 1: Actual spend reaches 80% of budget
  notification {
    comparison_operator        = "GREATER_THAN"
    threshold                  = 80
    threshold_type             = "PERCENTAGE"
    notification_type          = "ACTUAL"
    subscriber_email_addresses = ["finops-alerts@company.com"]
    subscriber_sns_topic_arns  = [var.budget_alerts_sns_topic_arn]
  }

  # Alert 2: Forecasted spend is projected to breach 100% of budget
  notification {
    comparison_operator        = "GREATER_THAN"
    threshold                  = 100
    threshold_type             = "PERCENTAGE"
    notification_type          = "FORECASTED"
    subscriber_email_addresses = ["finops-lead@company.com"]
  }
}
```

### 5.2 AWS Cost Anomaly Detection Monitor and Subscription (Terraform)

```hcl
# Anomaly Monitor tracking all AWS services
resource "aws_ce_anomaly_monitor" "service_monitor" {
  name              = "AllServicesCostAnomalyMonitor"
  monitor_type      = "DIMENSIONAL"
  monitor_dimension = "SERVICE"
}

# Anomaly Subscription alerting on anomalies exceeding $100
resource "aws_ce_anomaly_subscription" "slack_subscription" {
  name      = "FinOpsTeamSpikeAlert"
  frequency = "IMMEDIATE"
  monitor_arn_list = [
    aws_ce_anomaly_monitor.service_monitor.arn
  ]

  subscriber {
    type    = "SNS"
    address = var.finops_sns_topic_arn
  }

  threshold_expression {
    dimension {
      key           = "ANOMALY_TOTAL_IMPACT_ABSOLUTE"
      values        = ["100"] # Alert on anomalies exceeding $100
      match_options = ["GREATER_THAN_OR_EQUAL"]
    }
  }
}
```

### 5.3 Athena SQL Query for Cost and Usage Report (CUR) Chargeback

```sql
-- Query CUR table to calculate total monthly cost per CostCenter tag
SELECT 
    resource_tags_user_cost_center AS cost_center,
    line_item_product_code AS aws_service,
    SUM(line_item_unblended_cost) AS total_spend_usd
FROM 
    "finops_cur_database"."customer_cur_report"
WHERE 
    line_item_usage_start_date >= DATE '2026-11-01'
    AND line_item_usage_start_date < DATE '2026-12-01'
GROUP BY 
    resource_tags_user_cost_center,
    line_item_product_code
ORDER BY 
    total_spend_usd DESC;
```

### 5.4 S3 Intelligent-Tiering Bucket Configuration (Terraform)

```hcl
resource "aws_s3_bucket" "data_lake" {
  bucket = "enterprise-finops-optimized-lake-2026"
}

# Automatically moves data across Frequent, Infrequent, Archive Instant, and Deep Archive
resource "aws_s3_bucket_intelligent_tiering_configuration" "entire_bucket" {
  bucket = aws_s3_bucket.data_lake.id
  name   = "EntireBucketIntelligentTiering"

  tiering {
    access_tier = "ARCHIVE_ACCESS"
    days        = 90
  }

  tiering {
    access_tier = "DEEP_ARCHIVE_ACCESS"
    days        = 180
  }
}
```

---

## 6. Best Practices

1. **Enforce Mandatory Cost Allocation Tags via Service Control Policies (SCPs)**: Use AWS Organizations SCPs to block developers from launching EC2 instances, RDS databases, or S3 buckets unless required tags (`CostCenter`, `Environment`, `Owner`) are supplied.
2. **Review Compute Optimizer Recommendations Monthly**: Never let over-provisioned instances sit idle. Regularly downgrade instances (e.g., from `m6g.2xlarge` to `m6g.large`) based on observed CPU and memory metrics, saving up to 50% instantly.
3. **Commit to Compute Savings Plans, Not EC2 Instance Savings Plans**: Compute Savings Plans automatically apply to EC2, Fargate, and Lambda across any instance family and region. If your team migrates workloads from EC2 to Fargate or switches from x86 to Graviton, Compute Savings Plans continue to apply discounts seamlessly.
4. **Deploy S3 Intelligent-Tiering as the Default Storage Class**: S3 Intelligent-Tiering automatically moves objects between frequent, infrequent, and archive access tiers based on real access patterns with **zero retrieval fees**, slashing S3 costs by up to 60%.
5. **Use Graviton Processors (ARM64)**: Transition compute workloads (EC2, ECS, EKS, RDS, Lambda) to AWS Graviton3/Graviton4. Delivers up to **40% better price-performance** compared to comparable x86 (Intel/AMD) instances.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Forgetting to Activate Tags in Billing | Adding tags in code but not activating them in Billing console. | Tags never appear in Cost Explorer or CUR; cannot attribute costs. | Manually activate User-Defined Cost Allocation Tags in Billing console. |
| Oversizing Compute Instances for Peak | Sizing EC2 instances for peak Black Friday load 24/7/365. | Massive wasted spend during off-peak hours (instances run at 5% CPU). | Use EC2 Auto Scaling groups and rightsize with Compute Optimizer. |
| Buying 3-Year All-Upfront RIs on Unstable Tech | Committing to 3 years of legacy x86 EC2 instances. | Locked into obsolete hardware; unable to migrate to Graviton or Fargate. | Commit to 1-year or 3-year flexible **Compute Savings Plans**. |
| Orphaned Elastic IP and Unattached EBS Waste | Terminating EC2 instances without deleting attached EBS volumes. | Idle EBS volumes and unattached EIPs continue billing month after month. | Use Trusted Advisor to identify and delete unattached volumes and EIPs. |
| Waiting for the Monthly Invoice | Checking costs only when the monthly credit card statement arrives. | A rogue runaway recursive Lambda function costs \$15,000 before noticed. | Set up **AWS Cost Anomaly Detection** with immediate SNS alerts. |
| Neglecting Inter-AZ Data Transfer Costs | Multi-tier application sending terabytes of uncompressed traffic across AZs. | Data Transfer Out charges (\$0.01/GB each way) exceed compute costs. | Keep chattiest services in the same AZ, or compress network payloads. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance vs Cost Trade-offs
- **Graviton Adoption**: Transitioning to Graviton requires compiling container images for `linux/arm64`. Modern runtimes (Python, Node.js, Java, Go) run natively with zero code changes, providing immediate 20% cost reductions and 20% latency improvements.

### 8.2 Security & FinOps Governance
- **Budget Actions**: AWS Budgets can automatically execute IAM policies or apply SCPs if a spending limit is breached (e.g., automatically revoking permissions to launch expensive GPU instances).

### 8.3 Cost Breakdown
- **Cost Explorer**: \$0.01 per Paginated API Request (UI usage is 100% Free).
- **AWS Budgets**: First 2 budgets are free; \$0.02 per budget per day thereafter (~$0.60/month).
- **Cost Anomaly Detection**: 100% Free.
- **Compute Optimizer**: Free for standard 14-day analysis; \$0.00036 per resource-hour for 93-day enhanced metrics.

---

## 9. Comparison Table

| Dimension | Compute Savings Plans | EC2 Instance Savings Plans | Standard Reserved Instances (RIs) | Spot Instances |
|---|---|---|---|---|
| **Max Discount** | Up to 66% | Up to 72% | Up to 72% | Up to 90% |
| **Commitment** | 1 or 3 Years (\$/hour)| 1 or 3 Years (\$/hour) | 1 or 3 Years (Instance count)| Zero (Hourly market) |
| **Flexibility** | Any Region, Family, OS, ECS/Lambda| Fixed Region & Family | Fixed Region, Family & OS | Can be interrupted with 2-min notice|
| **Fargate & Lambda**| Yes | No (EC2 only) | No (EC2 only) | Fargate Spot available |
| **Marketplace Resell**| No | No | Yes (EC2 RI Marketplace) | No |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the architectural difference between Compute Savings Plans and EC2 Instance Savings Plans?</strong></summary>

- **Compute Savings Plans**: Provide the highest flexibility. You commit to a specific hourly dollar spend (e.g., \$10/hr). The discount automatically applies to any EC2 instance regardless of instance family (e.g., switching from `m5` to `c6g`), instance size, operating system (Linux or Windows), or AWS region. It also applies automatically to **AWS Fargate** container tasks and **AWS Lambda** invocations.
- **EC2 Instance Savings Plans**: Provide slightly higher discounts (up to 72% vs 66%), but require committing to a specific instance family within a specific AWS region (e.g., `m6g` in `us-east-1`). They do not apply across regions, to other instance families, or to Fargate/Lambda.
</details>

<details>
<summary><strong>Q2: Why are Cost Allocation Tags essential for enterprise FinOps, and why don't they show up immediately in reports?</strong></summary>

Cost Allocation Tags are the foundation of **Showback** (reporting costs per department) and **Chargeback** (billing departments internally). 
They do not show up immediately in Cost Explorer because AWS decouples resource tagging from the billing engine:
1. Adding a tag to an EC2 instance writes metadata to the EC2 control plane.
2. The tag must be explicitly **activated** in the AWS Billing & Cost Management console by an administrator.
3. Once activated, the billing engine begins recording tag associations into the cost dataset. Note: Cost allocation tags are **forward-looking only**; they do not apply retroactively to historical usage prior to activation.
</details>

<details>
<summary><strong>Q3: How does AWS Compute Optimizer determine rightsizing recommendations?</strong></summary>

AWS Compute Optimizer analyzes historical CloudWatch utilization metrics (CPU utilization, memory usage, network in/out, and disk I/O) over a default 14-day evaluation window (or 93 days with enhanced infrastructure metrics).
Using machine learning models trained on thousands of workload patterns:
- If peak CPU utilization consistently stays below 20%, it flags the instance as **Over-provisioned** and recommends a smaller instance size within the same or modern family.
- If CPU or memory reaches 100%, it flags it as **Under-provisioned** to prevent performance degradation.
- It also detects when migrating to AWS Graviton processors would yield higher price-performance.
</details>

<details>
<summary><strong>Q4: What is the AWS Cost and Usage Report (CUR) and how is it queried using Amazon Athena?</strong></summary>

The CUR is the most detailed billing report provided by AWS, containing individual line items for every resource, hour, tag, and rate dimension. 
To query it:
1. Configure CUR to export daily/hourly reports in **Apache Parquet format** to an Amazon S3 bucket.
2. Use AWS CloudFormation or an AWS Glue Crawler to register the schema in the **AWS Glue Data Catalog**.
3. Use **Amazon Athena** to run serverless ANSI SQL queries directly against the Parquet files in S3 (e.g., calculating unblended costs grouped by team tags or identifying unattached EBS volumes), powering dashboards in Amazon QuickSight or Tableau.
</details>

<details>
<summary><strong>Q5: How do Spot Instances work and what architectures are best suited for them?</strong></summary>

Spot Instances allow you to purchase spare, unused EC2 compute capacity at up to 90% discount compared to On-Demand rates. 
- **The Trade-Off**: AWS can reclaim Spot instances whenever capacity is needed elsewhere, providing a **2-minute interruption notice** via CloudWatch Events and Instance Metadata (IMDS).
- **Best Suited For**: Fault-tolerant, stateless, horizontally scaled workloads that can handle sudden node termination without data loss:
  1. Containerized worker microservices running on ECS/EKS.
  2. Asynchronous batch jobs consuming from Amazon SQS.
  3. Big data processing clusters (Amazon EMR / Apache Spark worker nodes).
  4. CI/CD test execution runners.
</details>

<details>
<summary><strong>Q6: What is AWS Cost Anomaly Detection and how does it differ from AWS Budgets?</strong></summary>

- **AWS Budgets**: Static and rule-based. You set an explicit monthly spending limit (e.g., \$5,000), and Budgets alerts you when actual or forecasted spending exceeds a static percentage (e.g., 80%). It cannot detect if a specific service spikes unexpectedly while total account spending remains under the threshold.
- **AWS Cost Anomaly Detection**: Dynamic and ML-driven. It continuously learns the unique daily and seasonal spending patterns of each individual AWS service. It detects anomalous spikes (e.g., DynamoDB spending jumping from \$5/day to \$300/day due to an unindexed query loop), isolates the exact root cause, and alerts engineers within hours.
</details>

<details>
<summary><strong>Q7: How does S3 Intelligent-Tiering optimize storage costs without retrieval fees?</strong></summary>

Traditional lifecycle transitions (moving data to S3 Standard-IA or Glacier) charge significant per-gigabyte **data retrieval fees** when objects are read, making them risky for unpredictable access patterns.
**S3 Intelligent-Tiering** monitors access patterns at the object level:
- If an object is not accessed for 30 consecutive days, it moves it to the *Infrequent Access Tier* (saving 40%).
- If not accessed for 90 days, it moves to *Archive Instant Access* (saving 68%).
- As soon as the object is accessed again, it immediately moves back to the *Frequent Access Tier* with **zero retrieval fees**.
</details>

<details>
<summary><strong>Q8: What causes high inter-AZ data transfer charges in AWS?</strong></summary>

Traffic within the same Availability Zone is free. However, traffic passing between two distinct Availability Zones inside the same region incurs a charge of **\$0.01 per GB in each direction** (\$0.02/GB round-trip). 
Common causes:
1. Microservices in AZ-A communicating with databases in AZ-B over private IPs.
2. Distributed clusters (Elasticsearch, Kafka, Cassandra) replicating uncompressed data across AZs.
3. NAT Gateways deployed in only one AZ routing egress traffic for instances in other AZs.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Activate Cost Allocation Tags in AWS Billing</strong></summary>

**Goal**: Make user-defined tags visible in cost reporting.
1. Apply tags `Environment: Production` and `CostCenter: Core` to a test resource.
2. Navigate to the AWS Billing Console -> Cost Allocation Tags.
3. Locate `Environment` and `CostCenter` under User-Defined Cost Allocation Tags and click **Activate**.
4. Confirm their active status.
</details>

<details>
<summary><strong>Exercise 2: Create a Forecasted Cost Budget with Alerting</strong></summary>

**Goal**: Catch spending overages before they happen.
1. Open AWS Budgets -> Create Budget.
2. Select Cost Budget with a \$100 monthly limit.
3. Configure an alert for **Forecasted Spend > 100%**.
4. Enter your email address and verify budget creation.
</details>

<details>
<summary><strong>Exercise 3: Review Compute Optimizer Rightsizing Recommendations</strong></summary>

**Goal**: Identify over-provisioned EC2 instances.
1. Enable AWS Compute Optimizer in the console.
2. Allow several days for metrics to collect (or view sample recommendations).
3. Review instances flagged as `Overprovisioned` and evaluate recommended smaller instance types.
</details>

<details>
<summary><strong>Exercise 4: Setup an AWS Cost Anomaly Monitor</strong></summary>

**Goal**: Automate ML spike detection.
1. In Cost Management, navigate to Cost Anomaly Detection.
2. Create an Anomaly Monitor for "All AWS Services".
3. Configure an Anomaly Subscription alerting on total impact $> \$50$.
4. Attach an SNS topic or email recipient.
</details>

<details>
<summary><strong>Exercise 5: Configure S3 Intelligent-Tiering on an Existing Bucket</strong></summary>

**Goal**: Automate storage tiering with zero retrieval fees.
1. Select an S3 bucket in the console.
2. Under Properties, open Intelligent-Tiering Archive Configurations.
3. Enable Archive Access Tier (90 days) and Deep Archive Access Tier (180 days).
4. Save configuration.
</details>

<details>
<summary><strong>Exercise 6: Query Cost Explorer via AWS CLI</strong></summary>

**Goal**: Analyze monthly spending breakdown programmatically.
1. Execute `aws ce get-cost-and-usage --time-period Start=2026-10-01,End=2026-11-01 --granularity MONTHLY --metrics UnblendedCost`.
2. Review the returned JSON output and inspect unblended cost metrics.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| AWS FINOPS COST OPTIMIZATION PILLARS                                                              |
+--------------------------+-----------------------+------------------------------------------------+
| FinOps Area              | Primary AWS Tool      | Best Practice Implementation                   |
+--------------------------+-----------------------+------------------------------------------------+
| Visibility & Attribution | Cost Allocation Tags  | Enforce Environment & CostCenter via SCPs      |
| Anomaly Detection        | Cost Anomaly Detection| ML alerts on unexpected dollar spikes          |
| Budget Thresholds        | AWS Budgets           | Set 80% Actual & 100% Forecasted alerts        |
| Rightsizing Waste        | Compute Optimizer     | Monthly review of over-provisioned EC2/EBS     |
| Commitment Discounts     | Compute Savings Plans | 1-year or 3-year commitments for base load     |
| Spot Capacity            | EC2 Spot Instances    | 70-90% discount for stateless batch & workers  |
| Storage Tiering          | S3 Intelligent-Tiering| Zero retrieval fees for unpredictable access   |
+--------------------------+-----------------------+------------------------------------------------+
```

- **Cost Allocation Tags**: Essential for attribution; must be activated in the Billing console.
- **Compute Savings Plans**: Preferred over rigid RIs for multi-service flexibility (EC2, Fargate, Lambda).
- **Compute Optimizer**: Data-driven rightsizing prevents over-provisioning waste.

---

## 13. Further Reading

- [AWS Cloud Financial Management Documentation](https://aws.amazon.com/financial-management/)
- [FinOps Foundation Official Framework](https://www.finops.org/framework/)
- [AWS Cost and Usage Report (CUR) Guide](https://docs.aws.amazon.com/cur/latest/userguide/what-is-cur.html)
- [AWS Compute Optimizer User Guide](https://docs.aws.amazon.com/compute-optimizer/latest/ug/what-is-compute-optimizer.html)
- [06-operations/09-reliability-and-cost.md](file:///04-devops/aws/06-operations/09-reliability-and-cost.md)
