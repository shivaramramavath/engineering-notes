# AWS Accounts, Regions, Billing & Cost Mechanics

> The AWS Account is the ultimate security, administrative, and billing boundary, while Regions and Availability Zones define geographic resilience and low-latency network topology; mastering cost mechanics prevents unexpected cloud expenditure.

| Metadata | Details |
|---|---|
| **Level** | Beginner to Intermediate |
| **Prerequisites** | [01-what-is-aws.md](file:///04-devops/aws/01-foundations/01-what-is-aws.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **OS / Cloud Scope** | Multi-Account AWS Infrastructure & Financial Governance |
| **Tool Version** | AWS CLI v2.22+, AWS Cost Explorer API |
| **Tested With** | AWS Budgets API, AWS Pricing Calculator, Linux AL2023 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Security & Cost Notes](#8-performance-security--cost-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why
In cloud architecture, poor isolation leads to catastrophic blast radiuses: an intern testing a script can accidentally delete production databases, or a compromised API key can spin up 500 GPU instances for cryptocurrency mining. An AWS Account is not merely a login; it is an impenetrable boundary for identity, quotas, and billing.

Enterprise architecture mandates a multi-account topology where development, staging, production, security logging, and shared networking each reside in dedicated AWS accounts. Furthermore, selecting the correct geographic Regions and Availability Zones dictates legal compliance (data residency laws like GDPR/HIPAA), end-user network latency, and operational survival during natural disasters.

---

## 2. Core Concepts
- **The AWS Account**:
  - The absolute security boundary: IAM users and roles in Account A have zero visibility or permissions in Account B unless explicit cross-account trust is provisioned.
  - The billing boundary: Usage fees and cost allocation aggregate to the account's 12-digit account ID.
  - Service Quota isolation: Throttling limits in staging do not impact production quotas.
- **Root User vs Administrative Identity**:
  - The *Root User* is the email address used to create the account. It has irrevocable, unrestricted access, cannot be constrained by IAM policies, and must be secured with hardware MFA.
  - Daily tasks must use delegated administrator roles via IAM Identity Center (SSO).
- **Regions & Availability Zones**:
  - *Region*: Independent geographical territory (e.g. `ap-south-1` Mumbai, `eu-central-1` Frankfurt).
  - *Availability Zone*: Physically isolated clusters of data centers with independent power and networking.
- **Volatile & Hidden Cost Traps**:
  - *Public IPv4 addresses*: Billed at $0.005/hour ($3.60/month per IP) since Feb 2024.
  - *NAT Gateways*: Billed hourly (~$0.045/hr) plus ~$0.045/GB data processed.
  - *Cross-AZ Data Transfer*: $0.01/GB in and $0.01/GB out ($0.02/GB total).
  - *CloudWatch Logs*: Ingestion fees ($0.50/GB) with default infinite retention accumulating monthly storage debt.

---

## 3. How It Works Internally
Regional Isolation & Multi-Account Consolidation Architecture:

```
+-------------------------------------------------------------------------+
|                  AWS Organizations Management Account                   |
|               (Consolidated Billing & Central Invoicing)                |
+-------------------------------------------------------------------------+
        |                                                 |
        v                                                 v
+-------------------------------+         +-------------------------------+
|  Production Workload Account  |         |   Dev / Staging Test Account  |
|  - Account ID: 111122223333   |         |  - Account ID: 444455556666   |
|  - Independent IAM / Quotas   |         |  - Independent IAM / Quotas   |
|                               |         |                               |
|  [ Region: us-east-1 ]        |         |  [ Region: us-east-1 ]        |
|  +-------------+-------------+|         |  +-------------+-------------+|
|  | AZ: use1-az1| AZ: use1-az2||         |  | AZ: use1-az1| AZ: use1-az2||
|  | Subnet-Pub  | Subnet-Priv ||         |  | Test-VM     | Test-DB     ||
|  +-------------+-------------+|         |  +-------------+-------------+|
+-------------------------------+         +-------------------------------+
```

1. **Physical Redundancy**: AZs within a region are located within 100 km (60 miles) of each other to ensure sub-2 millisecond round-trip packet latency while being in separate flood plains and power grids.
2. **Consolidated Invoicing**: AWS Organizations groups accounts under a root management account. Compute volume discounts, Reserved Instance (RI) sharing, and Savings Plans pool automatically across accounts.

---

## 4. Syntax & Usage
CLI commands for account identity, cost tracking, and region inspection:

```bash
# Query active account ID and assumed role
aws sts get-caller-identity

# Query unblended monthly spend breakdown via Cost Explorer
aws ce get-cost-and-usage \
    --time-period Start=2026-11-01,End=2026-11-24 \
    --granularity MONTHLY \
    --metrics "UnblendedCost" \
    --group-by Type=DIMENSION,Key=SERVICE

# List all enabled AWS regions in the account
aws account list-regions --region-opt-status-contains "ENABLED" "ENABLED_BY_DEFAULT"

# Check active AWS Budgets
aws budgets describe-budgets --account-id <ACCOUNT_ID>
```

---

## 5. Practical Examples

### Example 1: Extracting Account ID and ARN
- **Goal:** Safely extract the current 12-digit AWS account ID in a bash automation script.
- **Commands:**
```bash
aws sts get-caller-identity --query "Account" --output text
```
- **Real Output:**
```text
123456789012
```
- **Explanation:** Essential one-liner for dynamically constructing resource ARNs in deployment scripts.

### Example 2: Querying Top Spend by Service
- **Goal:** Analyze which AWS services generated the highest charges during the current month.
- **Commands:**
```bash
aws ce get-cost-and-usage \
    --time-period Start=2026-11-01,End=2026-11-24 \
    --granularity MONTHLY \
    --metrics "UnblendedCost" \
    --group-by Type=DIMENSION,Key=SERVICE \
    --query "ResultsByTime[0].Groups[?Metrics.UnblendedCost.Amount > '1.0'].[Keys[0], Metrics.UnblendedCost.Amount]" \
    --output table
```
- **Real Output:**
```text
------------------------------------------------------
|                  GetCostAndUsage                   |
+------------------------------------+---------------+
|  Amazon Elastic Compute Cloud - ...|  142.50       |
|  Amazon Relational Database Service|  85.20        |
|  Amazon Virtual Private Cloud      |  48.10        |
|  Amazon Simple Storage Service     |  12.35        |
+------------------------------------+---------------+
```
- **Explanation:** Formats monthly service line-items, exposing high-cost contributors like EC2 and VPC NAT Gateways.

### Example 3: Edge Case - Opt-In Regions
- **Goal:** Identify newer AWS regions that require explicit account-level activation before resources can be launched.
- **Commands:**
```bash
aws account list-regions \
    --query "Regions[?RegionOptStatus=='DISABLED'].[RegionName, RegionOptStatus]" \
    --output table
```
- **Real Output:**
```text
----------------------------------------
|              ListRegions             |
+--------------------+-----------------+
|  af-south-1        |  DISABLED       |
|  ap-east-1         |  DISABLED       |
|  me-central-1      |  DISABLED       |
+--------------------+-----------------+
```
- **Explanation:** Regions launched after March 2019 (Hong Kong, Cape Town, UAE) are disabled by default to prevent accidental cross-border resource creation.

### Example 4: Failure & Anti-Pattern - Unchecked Public IPv4 Costs
- **Goal:** Demonstrate unexpected billing caused by unattached Elastic IPs and public IPv4 subnets.
- **Anti-Pattern Inspection:**
```bash
# Check Elastic IPs allocated but not attached to running instances
aws ec2 describe-addresses \
    --query "Addresses[?InstanceId==null].[PublicIp, AllocationId]" \
    --output table
```
- **Real Output:**
```text
------------------------------------------------
|               DescribeAddresses              |
+-----------------+----------------------------+
|  54.210.12.34   |  eipalloc-0123456789abcdef |
|  34.200.56.78   |  eipalloc-0fedcba987654321 |
+-----------------+----------------------------+
```
- **Explanation & Fix:** Unattached Elastic IPs and idle public IPs are billed $0.005/hr ($7.20/mo for 2 IPs). Release unattached Elastic IPs immediately using `aws ec2 release-address --allocation-id <ID>`.

### Example 5: Production Budget with Zero-Spend & Forecast Alarms
- **Goal:** Provision an automated budget alerting if total spend exceeds $100 or if forecasted spend exceeds $120.
- **Commands:**
```bash
ACCOUNT_ID=$(aws sts get-caller-identity --query Account --output text)

aws budgets create-budget \
    --account-id "$ACCOUNT_ID" \
    --budget '{
        "BudgetName": "Production-Monthly-Guardrail",
        "BudgetLimit": {"Amount": "100", "Unit": "USD"},
        "TimeUnit": "MONTHLY",
        "BudgetType": "COST"
    }' \
    --notifications-with-subscribers '[
        {
            "Notification": {
                "NotificationType": "ACTUAL",
                "ComparisonOperator": "GREATER_THAN",
                "Threshold": 100,
                "ThresholdType": "PERCENTAGE"
            },
            "Subscribers": [{"SubscriptionType": "EMAIL", "Address": "finops@example.com"}]
        },
        {
            "Notification": {
                "NotificationType": "FORECASTED",
                "ComparisonOperator": "GREATER_THAN",
                "Threshold": 120,
                "ThresholdType": "PERCENTAGE"
            },
            "Subscribers": [{"SubscriptionType": "EMAIL", "Address": "finops@example.com"}]
        }
    ]'
```
- **Explanation:** Provides proactive financial protection by notifying engineers *before* an invoice spikes at month-end.

---

## 6. Best Practices
1. **Never Share an Account Across Environments**: Maintain separate AWS accounts for `dev`, `stage`, `prod`, and `shared-services` orchestrated by AWS Organizations.
2. **Lock Down the Root User**: Delete root access keys, enforce FIDO2 hardware MFA, and configure CloudTrail alarms on any root login event.
3. **Select Regions Based on Latency, Compliance, and Service Availability**: Verify that target services (e.g. Bedrock models or specific instance types) exist in your chosen region.
4. **Enforce Log Retention Policies**: Always set explicit retention limits (e.g. 14–30 days) on CloudWatch log groups to avoid perpetual storage debt.
5. **Architect Around NAT Gateway Data Fees**: Deploy VPC Gateway Endpoints for S3 and DynamoDB (free) to prevent private subnet data from traversing paid NAT Gateways.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Retaining active access keys for the Root user | Root keys bypass all permission boundaries; leak results in catastrophic takeover | Delete root keys; lock root account with hardware security keys (YubiKey) |
| Running everything in `us-east-1` blindly | `us-east-1` is historically prone to control-plane congestion | Distribute enterprise workloads across stable regions like `us-west-2` or `eu-west-1` |
| Believing AWS Budgets caps or halts spending | Budgets only emit alert notifications; resources continue running and billing | Pair Budgets with AWS Systems Manager / Lambda automation to stop dev instances |
| Forgetting to release unattached Elastic IPs | AWS charges $0.005/hr for allocated but unattached public IPs | Script weekly cleanup of unattached Elastic IP allocations |
| Missing cross-AZ traffic charges in chatty clusters | Cassandra/Kafka clusters spanning AZs incur $0.01/GB ingress and egress | Group chatty nodes or architect partition keys to minimize cross-AZ chattiness |
| Infinite CloudWatch Logs retention | Default CloudWatch setting is "Never Expire", accumulating gigabytes monthly | Set default 14-day or 30-day retention via IaC templates |

---

## 8. Performance, Security & Cost Notes
- **Performance**: Choose regions close to the majority of your user base. Use Amazon CloudFront CDN edge caches to terminate TLS and cache static assets within 20ms of global users.
- **Security**: Monitor `RootAccountUsage` metrics in CloudWatch Logs derived from CloudTrail management events. Any root usage should trigger emergency P1 Slack/PagerDuty alerts.
- **Cost**: The AWS Pricing Calculator (`calculator.aws`) must be used during architecture design before provisioning. Factor in the public IPv4 surcharge ($3.60/month/IP) across NAT Gateways and EC2 nodes.

---

## 9. Comparison / When to Use What

| Concept | Region | Availability Zone | Local Zone |
|---|---|---|---|
| **Scope** | Large geographic area | Cluster of physical data centers | Edge compute near major metropolitan cities |
| **Latency** | 20–100ms global | < 2ms intra-region | < 10ms to local users |
| **Fault Independence** | Completely autonomous | Autonomous power/networking | Dependent on parent region control plane |
| **When to Use** | Disaster recovery, data residency | High availability (HA) multi-tier apps | Real-time gaming, video streaming, telco |

---

## 10. Interview Questions

<details>
<summary>1. A company hits their $1,000 monthly AWS Budget limit on day 10. What happens to running EC2 instances?</summary>
Nothing stops. AWS Budgets is primarily a monitoring and alerting mechanism. Unless the budget has explicit automated actions configured (such as triggering an IAM policy attachment or executing an SSM automation document to stop instances), running workloads continue operating and incurring charges.
</details>

<details>
<summary>2. Why does AWS mandate a multi-account strategy instead of using IAM groups inside one account?</summary>
Single accounts share service quotas (e.g. API rate limits, VPC peering limits), security blast radiuses, and complex IAM evaluation rules. Separate accounts provide hard security boundaries, distinct billing invoices, and isolated quota ceilings.
</details>

<details>
<summary>3. What is the financial impact of the public IPv4 pricing introduced in 2024?</summary>
AWS charges $0.005 per hour for all public IPv4 addresses, regardless of whether they are attached to running instances, stopped instances, or NAT Gateways. This equals ~$3.60 per IP per month, making it cost-prohibitive to assign public IPs to backend EC2 instances.
</details>

<details>
<summary>4. What are the top three hidden or surprise costs in AWS architectures?</summary>
1. NAT Gateway hourly fees ($0.045/hr) + data processing fees ($0.045/GB).
2. Inter-AZ data transfer fees ($0.02/GB round-trip).
3. CloudWatch Logs ingestion ($0.50/GB) paired with infinite default retention.
</details>

<details>
<summary>5. How does AWS define Data Residency, and how do Regions satisfy it?</summary>
Data residency mandates that customer data cannot physically leave designated legal boundaries (e.g., GDPR in the EU). AWS guarantees that data stored in a specific Region (such as `eu-central-1` Frankfurt) never leaves that Region unless the customer explicitly configures cross-region replication.
</details>

<details>
<summary>6. How does AWS Organizations consolidated billing benefit an enterprise?</summary>
It combines usage across all member accounts onto a single invoice, allowing accounts to collectively qualify for volume tiering discounts on S3 storage, and share unused Reserved Instances (RIs) and Savings Plans.
</details>

<details>
<summary>7. What should be done with the Root User credentials immediately after account creation?</summary>
Enable hardware MFA (FIDO2 / YubiKey), verify no root access keys exist (or delete them if generated), store the complex root password in an enterprise vault, and create an IAM Identity Center admin role for all ongoing administrative work.
</details>

<details>
<summary>8. What is the difference between Actual Spend alerts and Forecasted Spend alerts in AWS Budgets?</summary>
Actual alerts trigger only after spend has physically breached a threshold (reactive). Forecasted alerts evaluate daily run rates and trigger early in the month if AWS projects that spend will breach the threshold by month-end (proactive).
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Query current month-to-date total AWS cost</summary>
**Task:** Retrieve the total unblended spend for the current calendar month using the CLI.

**Solution:**
```bash
START_DATE=$(date +%Y-%m-01)
END_DATE=$(date +%Y-%m-%d)

aws ce get-cost-and-usage \
    --time-period Start="$START_DATE",End="$END_DATE" \
    --granularity MONTHLY \
    --metrics "UnblendedCost" \
    --query "ResultsByTime[0].Total.UnblendedCost.Amount" \
    --output text
```
</details>

<details>
<summary>Exercise 2 (Easy): Identify the default region in your AWS CLI configuration</summary>
**Task:** Inspect your local profile's default region without opening configuration files.

**Solution:**
```bash
aws configure get region
```
</details>

<details>
<summary>Exercise 3 (Medium): Audit CloudWatch log groups with never-expiring retention</summary>
**Task:** List all CloudWatch Log Groups that have infinite retention enabled.

**Solution:**
```bash
aws logs describe-log-groups \
    --query "logGroups[?retentionInDays==null].[logGroupName, storedBytes]" \
    --output table
```
</details>

<details>
<summary>Exercise 4 (Medium): Set 30-day retention on an unconfigured log group</summary>
**Task:** Update a specific log group to expire events after 30 days.

**Solution:**
```bash
aws logs put-retention-policy \
    --log-group-name "/aws/lambda/payment-service" \
    --retention-in-days 30
```
</details>

<details>
<summary>Exercise 5 (Hard): Identify idle NAT Gateways across all subnets</summary>
**Task:** Query NAT Gateways that have processed less than 1 MB of traffic in the last 24 hours.

**Solution:**
```bash
for nat in $(aws ec2 describe-nat-gateways --query "NatGateways[?State=='available'].NatGatewayId" --output text); do
    BYTES=$(aws cloudwatch get-metric-statistics \
        --namespace AWS/NATGateway \
        --metric-name BytesOutFromSource \
        --dimensions Name=NatGatewayId,Value="$nat" \
        --start-time $(date -u -d '24 hours ago' +%Y-%m-%dT%H:%M:%SZ) \
        --end-time $(date -u +%Y-%m-%dT%H:%M:%SZ) \
        --period 86400 \
        --statistics Sum \
        --query "Datapoints[0].Sum" --output text)
    echo "NAT Gateway $nat: ${BYTES:-0} bytes processed in last 24h."
done
```
</details>

<details>
<summary>Exercise 6 (Hard): Build an automated Cost Anomaly Monitor using CLI</summary>
**Task:** Create an AWS Cost Anomaly Monitor and Subscription sending alerts when an anomaly exceeding $20 occurs.

**Solution:**
```bash
MONITOR_ARN=$(aws ce create-anomaly-monitor \
    --anomaly-monitor '{
        "MonitorName": "Daily-Cost-Spike-Monitor",
        "MonitorType": "DIMENSIONAL",
        "MonitorDimension": "SERVICE"
    }' \
    --query "MonitorArn" --output text)

aws ce create-anomaly-subscription \
    --anomaly-subscription '{
        "SubscriptionName": "Daily-Alert-Over-20-Dollars",
        "Threshold": 20.0,
        "Frequency": "DAILY",
        "MonitorArnList": ["'"$MONITOR_ARN"'"],
        "Subscribers": [{"Type": "EMAIL", "Address": "admin@example.com"}]
    }'
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                       ACCOUNTS, REGIONS & BILLING CHEAT SHEET                   |
+---------------------------------------------------------------------------------+
| ACCOUNT ID     | 12-digit numeric identifier (Hard security & billing boundary) |
| ROOT HYGIENE   | FIDO2 hardware MFA, zero access keys, emergency usage only     |
| MULTI-ACCOUNT  | Separate dev, staging, prod, shared-services via Organizations |
| REGION CHOICE  | Latency (< 20ms), Data Residency (GDPR), Service Availability  |
| MULTI-AZ       | Sub-2ms private optical metro links across independent centers |
| PUBLIC IP COST | $0.005/hr ($3.60/month per public IPv4 address)                |
| NAT GATEWAY    | ~$0.045/hr + $0.045/GB (Use VPC Endpoints to eliminate fees)   |
| LOG RETENTION  | Change default "Never Expire" to 14 or 30 days                 |
| BUDGET ALERTS  | Configure Actual (>80%) and Forecasted (>100%) spend alerts    |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [AWS Organizations User Guide](https://docs.aws.amazon.com/organizations/latest/userguide/orgs_introduction.html)
- [AWS Cost Explorer API Reference](https://docs.aws.amazon.com/aws-cost-management/latest/APIReference/Welcome.html)
- [AWS New Public IPv4 Address Charge Details](https://aws.amazon.com/blogs/aws/new-aws-public-ipv4-address-charge-public-ip-insights/)
- [AWS Well-Architected Cost Optimization Pillar](https://docs.aws.amazon.com/wellarchitected/latest/cost-optimization-pillar/welcome.html)

---

[Previous: 02-shared-responsibility-and-well-architected.md](file:///04-devops/aws/01-foundations/02-shared-responsibility-and-well-architected.md) | [Section Index](file:///04-devops/aws/01-foundations/README.md) | [Next: 04-organizations-scp-and-control-tower.md](file:///04-devops/aws/01-foundations/04-organizations-scp-and-control-tower.md)
