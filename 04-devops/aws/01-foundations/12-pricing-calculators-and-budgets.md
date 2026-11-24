# Pricing Calculators, Budgets, and Cost Anomaly Detection

> **Module**: `01-foundations`  
> **Topic**: Cost Governance, Estimation & Alerting  
> **Target Audience**: Cloud Engineers, DevOps Architects, FinOps Practitioners  
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

Cost transparency is a primary operational requirement in modern cloud infrastructure. The flexibility of public cloud infrastructure allows engineering teams to deploy resources in seconds, but without proactive governance, unmonitored compute instances, abandoned disks, and unoptimized network transfers can lead to compounding operational expenses.

AWS provides a cohesive toolset to predict, monitor, and react to infrastructure spend:
- **AWS Pricing Calculator**: Web-based pre-deployment sizing and architectural cost modeling tool.
- **AWS Budgets**: Proactive tracking mechanism against planned monthly, quarterly, or annual spending limits with automated alerts.
- **AWS Cost Explorer**: Historical trend analysis engine providing up to 14 months of historical and 12 months of forecasted billing telemetry.
- **AWS Cost Anomaly Detection**: Machine learning (ML) powered engine that isolates anomalous spikes in spending and identifies root causes down to individual accounts, services, or tags.

Implementing proactive cost monitoring protects organizations from unexpected billing spikes caused by infinite loops, distributed denial-of-service (DDoS) reflection attacks, unthrottled serverless invocations, or rogue worker scripts.

---

## 2. Core Concepts

### 2.1 Pre-Deployment vs. Post-Deployment Sizing
- **AWS Pricing Calculator (`calculator.aws`)**: Replaced the legacy Simple Monthly Calculator. Models complex multi-service architectures across multiple regions. Calculates volume discounts, Savings Plans deductions, and data transfer egress costs before deploying any infrastructure.
- **AWS Cost Explorer**: Analyzes actual historical cost and usage metrics via multidimensional queries grouped by Service, Region, Linked Account, Tag, or Usage Type.

### 2.2 AWS Budgets Architecture
AWS Budgets evaluates actual or forecasted usage against a defined monetary target or resource limit.
- **Cost Budget**: Monitors actual or forecasted dollar spend (e.g., alert when forecasted monthly spend exceeds $5,000).
- **Usage Budget**: Monitors consumed resource units (e.g., S3 storage gigabytes, EC2 runtime hours, data transfer out gigabytes).
- **Savings Plans / Reservation Budgets**: Monitors utilization percentage (how much of a committed reservation is consumed) and coverage percentage (how much of eligible compute is covered by commitments).
- **Budget Actions**: Automated responses triggered when a threshold is breached, including IAM policy detachment, service SCP attachment, or stopping EC2/RDS instances.

### 2.3 Machine Learning Anomaly Detection
AWS Cost Anomaly Detection uses machine learning models trained on an account's unique historical spend patterns.
- Accounts for organic daily, weekly, or seasonal consumption variations.
- Minimizes false alarms associated with static threshold alerting.
- Segments anomalies by:
  - AWS Services (Evaluates each AWS service independently).
  - Cost Allocation Tags (Evaluates specific cost centers or environment tags).
  - Cost Categories (Evaluates defined business groupings).
  - Member / Linked Accounts.

---

## 3. How It Works Internally

```
+-------------------------------------------------------------------------------+
|                       AWS Cost Telemetry & Alerting Pipeline                  |
+-------------------------------------------------------------------------------+
|                                                                               |
|  [AWS Infrastructure Resources]                                               |
|  (EC2, S3, RDS, Lambda, DynamoDB, NAT Gateway, CloudFront)                   |
|              |                                                                |
|              | Hourly Telemetry Logs & Metric Generation                      |
|              v                                                                |
|  +------------------------------------+                                       |
|  | AWS Cost and Usage Report (CUR)   |                                        |
|  | (Parquet / S3 Data Lake)          |                                        |
|  +------------------------------------+                                       |
|              |                                                                |
|              +--------------------------+-------------------------+           |
|              |                          |                         |           |
|              v                          v                         v           |
|  +-----------------------+  +-----------------------+  +-------------------+  |
|  |   AWS Cost Explorer   |  |     AWS Budgets       |  |   Cost Anomaly    |  |
|  |   Analytics Engine    |  |  (Rules & Thresholds) |  |   Detection (ML)  |  |
|  +-----------------------+  +-----------------------+  +-------------------+  |
|              |                          |                         |           |
|              | Ad-hoc                   | Alert Threshold         | Anomalous |
|              | Reporting                | Breached                | Deviation |
|              v                          v                         v           |
|     [FinOps Dashboards]        [Amazon SNS Topic]        [Amazon SNS / Slack] |
|                                         |                                     |
|                        +----------------+----------------+                    |
|                        |                                 |                    |
|                        v                                 v                    |
|             [Email / Slack Webhook]             [Budget Action Target]        |
|                                                 - Apply Deny-All IAM Policy   |
|                                                 - Stop Dev EC2/RDS Instances  |
+-------------------------------------------------------------------------------+
```

### Telemetry Pipeline Breakdown
1. **Telemetry Capture**: All AWS service usage generates line-item billing telemetry, logged continuously into billing backend databases and exposed via Cost and Usage Reports (CUR).
2. **Aggregation Interval**: AWS updates billing metrics up to three times daily (approximately every 8 to 24 hours). Real-time per-second billing metrics are not emitted directly by Cost Explorer or Budgets.
3. **Threshold Evaluation**: AWS Budgets queries aggregated billing datasets every 8 to 12 hours. If accumulated spend or forecasted spend crosses the notification threshold (e.g., 80% or 100%), an SNS message is published.
4. **Automated Remediations**: If a Budget Action is bound to an IAM Role, the Budget engine executes API calls (`ec2:StopInstances`, `iam:AttachUserPolicy`) directly without requiring custom external Lambda scripts.

---

## 4. Syntax & Usage

### 4.1 Creating a Cost Budget via AWS CLI
```bash
aws budgets create-budget \
  --account-id 111122223333 \
  --budget file://monthly-cost-budget.json \
  --notifications-with-subscribers file://notifications.json
```

**File: `monthly-cost-budget.json`**
```json
{
  "BudgetName": "Monthly-Total-Production-Limit",
  "BudgetLimit": {
    "Amount": "5000",
    "Unit": "USD"
  },
  "CostFilters": {
    "TagKeyValue": [
      "Environment$production"
    ]
  },
  "CostTypes": {
    "IncludeTax": true,
    "IncludeSubscription": true,
    "UseBlended": false,
    "IncludeRefund": false,
    "IncludeCredit": false,
    "IncludeUpfront": true,
    "IncludeRecurring": true,
    "IncludeOtherSubscription": true,
    "IncludeSupport": true,
    "IncludeDiscount": true,
    "UseAmortized": false
  },
  "TimeUnit": "MONTHLY",
  "BudgetType": "COST"
}
```

**File: `notifications.json`**
```json
[
  {
    "Notification": {
      "NotificationType": "ACTUAL",
      "ComparisonOperator": "GREATER_THAN",
      "Threshold": 80.0,
      "ThresholdType": "PERCENTAGE",
      "NotificationState": "ALARM"
    },
    "Subscribers": [
      {
        "SubscriptionType": "EMAIL",
        "Address": "finops-alerts@example.com"
      },
      {
        "SubscriptionType": "SNS",
        "Address": "arn:aws:sns:us-east-1:111122223333:cost-alerts-topic"
      }
    ]
  },
  {
    "Notification": {
      "NotificationType": "FORECASTED",
      "ComparisonOperator": "GREATER_THAN",
      "Threshold": 100.0,
      "ThresholdType": "PERCENTAGE",
      "NotificationState": "ALARM"
    },
    "Subscribers": [
      {
        "SubscriptionType": "EMAIL",
        "Address": "finops-alerts@example.com"
      }
    ]
  }
]
```

### 4.2 Querying Historical Spend via Cost Explorer CLI
Retrieve unblended cost grouped by service for the previous month:
```bash
aws ce get-cost-and-usage \
  --time-period Start=2026-10-01,End=2026-10-31 \
  --granularity MONTHLY \
  --metrics "UnblendedCost" \
  --group-by Type=DIMENSION,Key=SERVICE \
  --query 'ResultsByTime[0].Groups[?Metrics.UnblendedCost.Amount > `10.0`].[Keys[0], Metrics.UnblendedCost.Amount]' \
  --output table
```

---

## 5. Practical Examples

### Example 1: Creating an AWS Cost Anomaly Detection Monitor and Subscription
Configure automated ML anomaly monitoring across all services with alert delivery to SNS and email.

```bash
# 1. Create the Anomaly Monitor evaluating all AWS services
MONITOR_ARN=$(aws ce create-anomaly-monitor \
  --anomaly-monitor '{
    "MonitorName": "All-AWS-Services-Anomaly-Monitor",
    "MonitorType": "DIMENSIONAL",
    "MonitorDimension": "SERVICE"
  }' \
  --query 'MonitorArn' \
  --output text)

echo "Created Monitor: $MONITOR_ARN"

# 2. Create the Anomaly Subscription alerting on any anomaly exceeding $100
aws ce create-anomaly-subscription \
  --anomaly-subscription '{
    "SubscriptionName": "High-Impact-Anomaly-Subscription",
    "Threshold": 100.0,
    "Frequency": "IMMEDIATE",
    "MonitorArnList": ["'"$MONITOR_ARN"'"],
    "Subscribers": [
      {
        "Type": "EMAIL",
        "Address": "devops-leads@example.com"
      }
    ]
  }'
```

### Example 2: Terraform Definition for AWS Budget with Automated SNS Alert
Declarative infrastructure provisioning for a team-level monthly development budget.

```hcl
resource "aws_sns_topic" "dev_cost_alerts" {
  name = "dev-cost-alerts-topic"
}

resource "aws_sns_topic_policy" "allow_budgets" {
  arn = aws_sns_topic.dev_cost_alerts.arn
  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Sid       = "AllowAWSBudgetsToPublish"
        Effect    = "Allow"
        Principal = { Service = "budgets.amazonaws.com" }
        Action    = "SNS:Publish"
        Resource  = aws_sns_topic.dev_cost_alerts.arn
      }
    ]
  })
}

resource "aws_budgets_budget" "dev_environment_budget" {
  name              = "dev-environment-monthly-limit"
  budget_type       = "COST"
  limit_amount      = "1500"
  limit_unit        = "USD"
  time_unit         = "MONTHLY"
  time_period_start = "2026-01-01_00:00"

  cost_filter {
    name = "TagKeyValue"
    values = [
      "user:Environment$development"
    ]
  }

  notification {
    comparison_operator        = "GREATER_THAN"
    threshold                  = 85
    threshold_type             = "PERCENTAGE"
    notification_type          = "ACTUAL"
    subscriber_email_addresses = ["dev-oncall@example.com"]
    subscriber_sns_topic_arns  = [aws_sns_topic.dev_cost_alerts.arn]
  }

  notification {
    comparison_operator        = "GREATER_THAN"
    threshold                  = 100
    threshold_type             = "PERCENTAGE"
    notification_type          = "FORECASTED"
    subscriber_email_addresses = ["finops-leads@example.com"]
  }
}
```

### Example 3: Automated Remediation via AWS Budget Actions
When an unmanaged sandbox account exceeds $500, AWS Budgets executes a Budget Action attaching a strict `Deny-All-EC2-Run` policy.

```bash
# Register budget action using IAM execution role
aws budgets create-budget-action \
  --account-id 111122223333 \
  --budget-name "Sandbox-Hard-Cap" \
  --notification-type ACTUAL \
  --action-type APPLY_IAM_POLICY \
  --action-threshold '{"ActionThresholdValue": 100, "ActionThresholdType": "PERCENTAGE"}' \
  --definition '{
    "IamActionDefinition": {
      "PolicyArn": "arn:aws:iam::111122223333:policy/DenyComputeProvisioning",
      "Roles": ["DeveloperSandboxRole"]
    }
  }' \
  --execution-role-arn "arn:aws:iam::111122223333:role/AWSBudgetsActionExecutionRole" \
  --approval-model AUTOMATIC
```

### Example 4: Python Boto3 Script for Tag-Based Cost Breakdown
Generate an automated summary of costs broken down by Kubernetes namespace tag.

```python
import boto3
from datetime import datetime, timedelta

def get_namespace_cost_report():
    client = boto3.client('ce', region_name='us-east-1')
    
    end_date = datetime.utcnow().date()
    start_date = end_date - timedelta(days=7)
    
    response = client.get_cost_and_usage(
        TimePeriod={
            'Start': start_date.strftime('%Y-%m-%d'),
            'End': end_date.strftime('%Y-%m-%d')
        },
        Granularity='DAILY',
        Metrics=['UnblendedCost'],
        GroupBy=[
            {'Type': 'TAG', 'Key': 'k8s_namespace'}
        ]
    )
    
    print(f"Cost report from {start_date} to {end_date}:")
    for day in response['ResultsByTime']:
        print(f"\nDate: {day['TimePeriod']['Start']}")
        for group in day['Groups']:
            tag_value = group['Keys'][0].replace('k8s_namespace$', '')
            amount = float(group['Metrics']['UnblendedCost']['Amount'])
            if amount > 0.05:
                print(f"  Namespace [{tag_value}]: ${amount:.2f}")

if __name__ == '__main__':
    get_namespace_cost_report()
```

### Example 5: Architectural Estimation with Pricing Calculator API
Generate structured programmatic cost models for an Elastic Container Service (ECS) Fargate workload.

```bash
# Pricing Calculator supports sharing URLs and programmatic CSV exports
# For programmatic pricing lookups of specific rates, use AWS Price List Service API
aws pricing get-products \
  --service-code AmazonECS \
  --filters "Type=TERM_MATCH,Field=operatingSystem,Value=Linux" \
            "Type=TERM_MATCH,Field=location,Value=US East (N. Virginia)" \
  --region us-east-1 \
  --max-items 2 \
  --query 'PriceList[0]' \
  --output json
```

---

## 6. Best Practices

1. **Activate Cost Allocation Tags Early**: AWS tags are not retroactive. You must activate User-Defined Cost Allocation Tags (e.g., `Environment`, `Owner`, `CostCenter`, `Project`) in the Billing Console before they appear in Cost Explorer, Budgets, and CUR data.
2. **Combine Actual and Forecasted Budget Alerts**: Always configure two notifications on critical budgets: one at **80% Actual** to catch steady run-rate overruns, and one at **100% Forecasted** to catch mid-cycle spikes before the billing cycle completes.
3. **Use Cost Anomaly Detection Over Low Static Thresholds**: Static threshold budgets produce alert fatigue when seasonal workloads scale up naturally. Anomaly Detection uses machine learning to adapt to cyclic business traffic.
4. **Enforce Organization-Level Consolidated Budgets**: Define master spending limits in the AWS Organizations Management account while delegating member-account budgets for development sandboxes.
5. **Differentiate Amortized vs. Unblended Costs**: When analyzing accounts using Savings Plans or Reserved Instances, always use **Amortized Cost** in Cost Explorer. Unblended cost shows massive upfront spikes on day 1 of the month, skewing daily trend lines.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Expecting Real-Time Billing Telemetry** | Assuming Budgets will alert within 5 minutes of a runaway script launching. | AWS billing telemetry refreshes every 8–24 hours. For real-time infrastructure caps, use CloudWatch instance count alarms or AWS Service Quotas. |
| **Missing User-Defined Tag Activation** | Spending hours tagging Terraform resources only to find Cost Explorer groups everything under `No Tag`. | Navigate to **Billing Console -> Cost Allocation Tags**, select user tags, and click **Activate**. Wait 24 hours for indexing. |
| **Using Static Budgets for Variable Traffic** | Continuous alert spam whenever valid marketing campaigns or flash sales occur. | Pair fixed quarterly baseline budgets with AWS Cost Anomaly Detection configured for dynamic anomaly scoring. |
| **Omitting CloudWatch/SNS Permissions** | AWS Budgets fails to send alert notifications due to missing resource policy on the target SNS topic. | Explicitly attach an SNS Topic Policy granting `Principal: { Service: "budgets.amazonaws.com" }` permission to call `SNS:Publish`. |
| **Ignoring Free Tier Expiration** | Year-two bill shocks when initial 12-month free tier credits and services (EC2 `t2.micro`, S3 5GB) expire. | Configure an AWS Free Tier Usage Budget in the billing console to alert immediately upon exceeding 85% of monthly free quotas. |
| **Relying on Unblended Cost for Reservations** | False impression that compute cost dropped to $0 on day 2 while day 1 had a massive spending spike. | Utilize **Amortized Cost** in Cost Explorer to distribute upfront Reservation and Savings Plan fees evenly across the commitment term. |

---

## 8. Performance, Security & Cost Notes

### Telemetry Latency & Consistency
- AWS Billing data is eventually consistent across global regions.
- Telemetry feeds process CloudWatch usage logs, VPC flow metrics, and API invocation counts asynchronously.
- Changes in consumption typically appear in Cost Explorer and Budgets between 8 and 24 hours later.

### Security & IAM Governance
- Restrict access to billing APIs using IAM service prefixes:
  - `aws-portal:*` (Legacy console permissions)
  - `ce:*` (AWS Cost Explorer API)
  - `budgets:*` (AWS Budgets API)
  - `cur:*` (Cost and Usage Report definitions)
- Ensure execution roles for Budget Actions implement strict least-privilege scoping so automated actions cannot impact mission-critical production databases or management accounts.

### Tool Pricing Matrix
- **AWS Pricing Calculator**: Free web utility.
- **AWS Cost Explorer**: First 1,000 API requests per month are free ($0.01 per request thereafter). Console UI access is completely free.
- **AWS Budgets**: First 2 action-enabled budgets per month are free; each additional active budget costs $0.02 per day (~$0.60/month).
- **AWS Cost Anomaly Detection**: 100% Free of charge across all accounts.

---

## 9. Comparison Table

| Dimension | AWS Pricing Calculator | AWS Budgets | AWS Cost Explorer | AWS Cost Anomaly Detection |
|:---|:---|:---|:---|:---|
| **Primary Lifecycle Phase** | Pre-Deployment / Architectural Design | Operational Tracking & Enforcement | Post-Deployment Historical Analysis | Continuous Real-Time ML Auditing |
| **Input Data Source** | User manual entry / configuration | Aggregated AWS billing records | Historical CUR billing telemetry | Machine learning model on historical CUR |
| **Granularity** | Estimate / Sizing models | Monthly, Quarterly, Annual, Daily | Hourly (opt-in), Daily, Monthly | Service-level, Tag-level, Account-level |
| **Automated Actions** | None (export to CSV / shareable link) | Yes (Stop EC2/RDS, apply SCP/IAM policy) | None (Data querying & reporting) | Alert notification (SNS / Email / Slack) |
| **False Positive Mitigation** | N/A | Requires careful threshold tuning | N/A | High (ML accounts for seasonal trends) |
| **Direct Cost** | Free | First 2 free; $0.02/day per budget | Free UI; $0.01/API call after 1k | Free |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: How does AWS Cost Explorer calculate Amortized Cost versus Unblended Cost?</strong></summary>

**Answer:**
- **Unblended Cost**: Represents the cash-accounting figure charged on the specific day an invoice item occurs. If an organization purchases a 1-year All-Upfront Reserved Instance or Savings Plan for $12,000 on October 1st, the unblended cost shows a $12,000 spike on October 1st and $0 for compute usage on October 2nd through 31st.
- **Amortized Cost**: Represents accrual-based accounting. It takes the upfront and recurring commitment fees and divides them evenly across every hour of the commitment period. In the $12,000 example, amortized cost reports ~$32.87 per day for the entire year, providing an accurate, smoothed picture of actual daily workload operational cost.
</details>

<details>
<summary><strong>Q2: Why did an engineering team not receive an AWS Budget alert despite exceeding their monthly limit?</strong></summary>

**Answer:**
Common root causes include:
1. **SNS Topic Policy**: The SNS Topic lacked a resource policy permitting `budgets.amazonaws.com` to invoke `sns:Publish`.
2. **Billing Telemetry Lag**: AWS billing data updates every 8 to 24 hours; if the spending spike occurred rapidly within hours, the threshold calculation had not yet processed the aggregated line items.
3. **Threshold Metric Mismatch**: The budget was configured for `ACTUAL` spend at 100%, and the notification did not trigger mid-month because current spend was 95% even though forecasted spend was 250%. (Fix: add a `FORECASTED` trigger).
4. **Cost Filter Exclusions**: The budget cost filter excluded Taxes, Upfront Fees, or specific tags that absorbed the majority of the spend.
</details>

<details>
<summary><strong>Q3: How can an organization prevent runaway EC2 costs in sandbox accounts automatically?</strong></summary>

**Answer:**
Implement **AWS Budget Actions**:
1. Create a monthly cost budget in the sandbox account (e.g., target limit $250).
2. Configure a Budget Action with a 100% actual threshold.
3. Define the action to attach an IAM Deny Policy (`DenyAllComputeProvisioning`) to the sandbox engineering role or execute an SSM Automation runbook to stop all tagged `Environment=sandbox` EC2 instances.
4. Set the execution approval mode to `AUTOMATIC`.
</details>

<details>
<summary><strong>Q4: What is the operational difference between AWS Budgets and AWS Cost Anomaly Detection?</strong></summary>

**Answer:**
AWS Budgets relies on static, rule-based limits defined by administrators (e.g., alert if monthly spend > $10,000). Budgets work well for planned fiscal boundaries but struggle with fluctuating seasonal workloads where normal traffic spikes trigger false alarms.

AWS Cost Anomaly Detection uses machine learning to establish a baseline of normal spending patterns per service or account. It accounts for organic weekly or daily usage cycles and only alerts when spend deviates statistically from expected variance (e.g., a service suddenly incurring 4x its normal hourly rate due to an unclosed network socket or rogue loop).
</details>

<details>
<summary><strong>Q5: Can AWS Budgets enforce hard spending limits that shut down an entire AWS account?</strong></summary>

**Answer:**
AWS Budgets cannot natively terminate or freeze an entire account natively in a single atomic toggle. However, you can achieve near-equivalent hard-capping by pairing Budget Actions with AWS Organizations:
- The Budget Action can trigger a Lambda function or an AWS Systems Manager document that moves the target AWS account into a quarantine Organizational Unit (OU) with an attached Service Control Policy (SCP) denying all non-administrative API calls (`"Action": "*", "Resource": "*", "Effect": "Deny"`).
</details>

<details>
<summary><strong>Q6: Why do cost allocation tags not show up immediately in Cost Explorer after tagging resources in Terraform?</strong></summary>

**Answer:**
Resource tags do not automatically become cost allocation tags. 
1. Tags must be explicitly activated in the AWS Billing & Cost Management Console under **Cost Allocation Tags**.
2. Once activated, AWS takes up to 24 hours to begin indexing those tags into the billing data pipeline.
3. Crucially, cost allocation tags **are not retroactive**; historical spend before tag activation remains permanently categorized as untagged.
</details>

<details>
<summary><strong>Q7: What is the AWS Cost and Usage Report (CUR) and how does it relate to Cost Explorer?</strong></summary>

**Answer:**
The Cost and Usage Report (CUR) is the most granular, raw dataset AWS provides for billing data. It writes detailed line-item CSV or Apache Parquet files directly to an Amazon S3 bucket at hourly or daily granularity, detailing every resource ARN, usage type, pricing tier, and tag.

Cost Explorer is a managed querying and visualization engine that ingests processed subsets of this underlying data. For advanced FinOps analytics beyond Cost Explorer's 14-month retention or UI filtering limits, organizations ingest CUR data into Amazon Athena and build dashboards in Amazon QuickSight.
</details>

<details>
<summary><strong>Q8: How does AWS Pricing Calculator account for regional price differences?</strong></summary>

**Answer:**
AWS Pricing Calculator requires each added service module to be pinned to a specific AWS Region (e.g., `us-east-1`, `eu-central-1`, `ap-southeast-1`). Because infrastructure, power, tax, and local regulatory costs vary by geography, AWS hardware and bandwidth rates differ across regions. The calculator applies the exact localized pricing tables, local data transfer rates, and region-specific discounts.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Forecasted Cost Budget using the AWS CLI</strong></summary>

**Objective**: Configure a budget that notifies your email when current monthly run-rate is forecasted to exceed $50.

**Step-by-step**:
1. Retrieve your current 12-digit AWS Account ID:
   ```bash
   ACCOUNT_ID=$(aws sts get-caller-identity --query "Account" --output text)
   ```
2. Create `forecast-budget.json`:
   ```json
   {
     "BudgetName": "Forecasted-Spend-Cap",
     "BudgetLimit": { "Amount": "50", "Unit": "USD" },
     "TimeUnit": "MONTHLY",
     "BudgetType": "COST"
   }
   ```
3. Create `forecast-notification.json`:
   ```json
   [
     {
       "Notification": {
         "NotificationType": "FORECASTED",
         "ComparisonOperator": "GREATER_THAN",
         "Threshold": 100.0,
         "ThresholdType": "PERCENTAGE"
       },
       "Subscribers": [
         { "SubscriptionType": "EMAIL", "Address": "your-email@example.com" }
       ]
     }
   ]
   ```
4. Execute creation:
   ```bash
   aws budgets create-budget \
     --account-id $ACCOUNT_ID \
     --budget file://forecast-budget.json \
     --notifications-with-subscribers file://forecast-notification.json
   ```
5. Confirm creation via `aws budgets describe-budgets --account-id $ACCOUNT_ID`.
</details>

<details>
<summary><strong>Exercise 2: Setup an AWS Cost Anomaly Detection Monitor and SNS Alert</strong></summary>

**Objective**: Provision an automated ML service-level anomaly detector.

**Step-by-step**:
1. Create a dedicated SNS topic for anomalies:
   ```bash
   TOPIC_ARN=$(aws sns create-topic --name cost-anomalies-topic --query 'TopicArn' --output text)
   ```
2. Subscribe your email:
   ```bash
   aws sns subscribe --topic-arn $TOPIC_ARN --protocol email --notification-endpoint your-email@example.com
   ```
3. Create the dimensional anomaly monitor:
   ```bash
   MONITOR_ARN=$(aws ce create-anomaly-monitor \
     --anomaly-monitor '{"MonitorName":"CoreServicesAnomaly","MonitorType":"DIMENSIONAL","MonitorDimension":"SERVICE"}' \
     --query 'MonitorArn' --output text)
   ```
4. Bind subscription with immediate notification:
   ```bash
   aws ce create-anomaly-subscription \
     --anomaly-subscription '{
       "SubscriptionName": "CoreAnomalyAlert",
       "Threshold": 25.0,
       "Frequency": "IMMEDIATE",
       "MonitorArnList": ["'"$MONITOR_ARN"'"],
       "Subscribers": [{"Type":"SNS","Address":"'"$TOPIC_ARN"'"}]
     }'
   ```
</details>

<details>
<summary><strong>Exercise 3: Query Top 5 Most Expensive Services in the Last 30 Days</strong></summary>

**Objective**: Use the Cost Explorer CLI to identify top spend categories.

**Command**:
```bash
START_DATE=$(date -u -d "30 days ago" +%Y-%m-%d 2>/dev/null || date -v-30d +%Y-%m-%d)
END_DATE=$(date -u +%Y-%m-%d)

aws ce get-cost-and-usage \
  --time-period Start=$START_DATE,End=$END_DATE \
  --granularity MONTHLY \
  --metrics "UnblendedCost" \
  --group-by Type=DIMENSION,Key=SERVICE \
  --query 'ResultsByTime[0].Groups | sort_by(@, &Metrics.UnblendedCost.Amount) | reverse(@) | [0:5].[Keys[0], Metrics.UnblendedCost.Amount]' \
  --output table
```
</details>

<details>
<summary><strong>Exercise 4: Construct a Multi-Tier Architecture Estimate in AWS Pricing Calculator</strong></summary>

**Objective**: Model the annual expense of a resilient web architecture.

**Steps**:
1. Open [calculator.aws](https://calculator.aws) in your browser.
2. Add an **Application Load Balancer** in `us-east-1` (estimate 15 LCUs, 50 GB processed/month).
3. Add **Amazon EC2**: 4 instances of `t4g.medium` running 720 hours/month with 100 GB gp3 EBS per instance.
4. Add **Amazon Aurora PostgreSQL**: 1 Primary + 1 Read Replica (`db.r6g.large`), 250 GB storage.
5. Add **Amazon S3**: 1 TB Standard Storage, 100,000 PUT requests, 2,000,000 GET requests.
6. Compare **On-Demand** pricing vs **3-Year Compute Savings Plans** upfront. Note total cost savings percentage.
</details>

<details>
<summary><strong>Exercise 5: Activate and Test Cost Allocation Tags</strong></summary>

**Objective**: Enable tag-based cost attribution.

**Steps**:
1. Deploy an S3 bucket tagged with `Environment=staging` and `Project=Phoenix`.
2. Navigate to AWS Billing Console -> **Cost Allocation Tags**.
3. Under **User-defined cost allocation tags**, search for `Environment` and `Project`.
4. Select both tags and click **Activate**.
5. Wait 24 hours, then run a `ce get-cost-and-usage` query grouped by `TAG:Environment` to confirm data ingestion.
</details>

<details>
<summary><strong>Exercise 6: Configure an Automated Stop-Instance Action on Budget Overrun</strong></summary>

**Objective**: Protect a dev environment by stopping compute when spend exceeds budget.

**Steps**:
1. Create a dedicated IAM Role `BudgetAutoStopRole` with trust policy for `budgets.amazonaws.com`.
2. Attach policy granting `ec2:StopInstances` and `ec2:DescribeInstances`.
3. Create a test budget with limit $10.
4. Add a Budget Action targeting an existing test EC2 instance ID with an execution threshold of 100% Actual.
5. Manually invoke or test the action in the console to verify execution logs in AWS CloudTrail.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                             AWS FinOps Governance Cheat Sheet                         |
+---------------------------------------------------------------------------------------+
|  Tool                  | Use Case                   | Pricing    | Update Frequency   |
|------------------------+----------------------------+------------+--------------------|
|  Pricing Calculator    | Pre-deployment modeling    | Free       | Static estimates   |
|  AWS Budgets           | Policy alerts & actions    | 2 free     | Every 8-12 hours   |
|  Cost Explorer         | Visual historical trends   | Free UI    | Every 24 hours     |
|  Anomaly Detection     | ML deviation alerting      | Free       | Continuous (daily) |
|  Cost & Usage Report   | Granular raw Parquet data  | S3 storage | Hourly / Daily     |
+---------------------------------------------------------------------------------------+
```

### Essential CLI Snippets
```bash
# Check all active budgets
aws budgets describe-budgets --account-id <ACCOUNT_ID> --output table

# Check cost anomaly alerts
aws ce get-anomalies --date-interval StartDate=2026-10-01,EndDate=2026-10-31

# Daily spend over last 7 days
aws ce get-cost-and-usage \
  --time-period Start=2026-11-15,End=2026-11-22 \
  --granularity DAILY \
  --metrics "UnblendedCost"
```

---

## 13. Further Reading

- [AWS Pricing Calculator Documentation](https://docs.aws.amazon.com/pricing-calculator/latest/userguide/)
- [AWS Budgets User Guide & Budget Actions](https://docs.aws.amazon.com/cost-management/latest/userguide/budgets-managing-costs.html)
- [AWS Cost Anomaly Detection Guide](https://docs.aws.amazon.com/cost-management/latest/userguide/manage-cost-anomalies.html)
- [AWS Well-Architected Framework: Cost Optimization Pillar](https://docs.aws.amazon.com/wellarchitected/latest/cost-optimization-pillar/welcome.html)
- [FinOps Foundation: Cloud Cost Governance Framework](https://www.finops.org/)
