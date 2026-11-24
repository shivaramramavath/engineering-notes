# AWS Reliability & Operational Governance: Well-Architected, Health Dashboard, Trusted Advisor & FIS

> **Module**: `06-operations`  
> **Topic**: Resilience Architecture, Well-Architected Reviews, Health Events, Service Quotas & Chaos Engineering  
> **Target Audience**: Cloud Architects, SREs, Systems Administrators, Technical Leads  
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

Building applications in the cloud does not automatically make them resilient. Infrastructure failures are inevitable: underlying hardware degenerates, networking cables get severed, fiber lines experience packet degradation, and regional service quotas get exhausted during flash sales. Assuming that the cloud is inherently 100% available without active architectural design leads to catastrophic system downtime.

AWS provides an interconnected suite of reliability, architectural governance, and health monitoring services:
1. **AWS Well-Architected Tool**: Conducts structured architectural reviews against battle-tested AWS architectural pillars, identifying High-Risk Issues (HRIs) and Medium-Risk Issues (MRIs).
2. **AWS Health Dashboard**: Delivers personalized alerts and operational guidance when AWS experiences events, service disruptions, or scheduled maintenance that may impact your specific resources.
3. **AWS Trusted Advisor**: A real-time cloud optimization engine providing automated checks across Cost, Security, Fault Tolerance, Performance, and Service Quotas.
4. **Service Quotas**: Centrally views, manages, and requests automated quota increases for AWS resource limits.
5. **AWS Fault Injection Service (FIS)**: A managed chaos engineering service that safely injects real-world faults (e.g., stopping EC2 instances, throttling DynamoDB, terminating AZ network links) into workloads to test and validate resilience.

---

## 2. Core Concepts

### 2.1 The AWS Well-Architected Framework
A structured methodology for evaluating cloud architectures across six fundamental pillars:
1. **Operational Excellence**: Running and monitoring systems to deliver business value and continually improving processes.
2. **Security**: Protecting data, systems, and assets through privilege management and defense-in-depth.
3. **Reliability**: Recovering from disruptions, dynamically acquiring computing resources to meet demand, and mitigating disruptions.
4. **Performance Efficiency**: Using computing resources efficiently to meet system requirements.
5. **Cost Optimization**: Eliminating unneeded expense and optimizing workloads.
6. **Sustainability**: Minimizing the environmental impacts of running cloud workloads.

### 2.2 AWS Health Dashboard
- **AWS Service Health**: Displays the general status of all AWS services across all global regions (public status board).
- **AWS Personal Health Dashboard (PHD)**: Tailored specifically to your AWS account. Alerts you only to events that directly affect *your* active resources (e.g., "EC2 instance `i-12345` is scheduled for hardware retirement on Dec 10").
- **Event Categories**: Open Issues, Scheduled Changes, Other Notifications. Integrates natively with Amazon EventBridge for automated self-healing.

### 2.3 AWS Trusted Advisor
Provides real-time recommendations categorized into five pillars:
1. **Cost Optimization**: Identifies idle RDS databases, unattached Elastic IPs, and underutilized EC2 instances.
2. **Security**: Flags public S3 buckets, missing MFA on the root account, and open security group ports.
3. **Fault Tolerance**: Detects single-AZ RDS deployments, missing EBS snapshots, and non-redundant Route 53 DNS records.
4. **Performance**: Flags high EC2 CPU utilization and rules nearing throughput limits.
5. **Service Limits (Quotas)**: Warns when account usage reaches 80% or more of an AWS service limit.

### 2.4 Service Quotas
- Every AWS account has default service limits (e.g., maximum 5 VPCs per region, maximum 10,000 req/sec on API Gateway).
- **Service Quotas Console & API**: Enables programmatic tracking of usage vs limits and submitting automated quota increase requests directly from CI/CD pipelines or Terraform.

### 2.5 AWS Fault Injection Service (FIS) - Chaos Engineering
- Controlled chaos engineering platform.
- Safely injects synthetic faults into live or staging systems to validate that auto-recovery mechanisms (Auto Scaling, Multi-AZ failovers, circuit breakers) operate as expected.
- **Guardrails**: Automated Stop Conditions monitor CloudWatch alarms; if user-facing error rates spike uncontrollably during an experiment, FIS automatically halts the experiment and rolls back the injected faults.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                        AWS RELIABILITY & AUTOMATED HEALTH REMEDIATION                             |
|                                                                                                   |
|  [Underlying Physical AWS Hardware]                                                               |
|         |                                                                                         |
|         v (Hardware Degradation Detected on Host Hosting EC2 Instance i-9912)                     |
|  +---------------------------------------------------------------------------------------------+  |
|  | AWS HEALTH DASHBOARD (Personal Health Event)                                                |  |
|  | Event: `AWS_EC2_INSTANCE_RETIREMENT_SCHEDULED`                                              |  |
|  | Target: Instance i-9912 scheduled for termination in 7 days                                 |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (Emits Event to EventBridge Default Bus)                                                |
|  +---------------------------------------------------------------------------------------------+  |
|  | AMAZON EVENTBRIDGE RULE                                                                     |  |
|  | Pattern: `{"source": ["aws.health"], "detail": {"service": ["EC2"]}}`                       |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v                                                                                         |
|  +---------------------------------------------------------------------------------------------+  |
|  | AUTOMATED REMEDIATION WORKFLOW (AWS Systems Manager Automation / Lambda)                    |  |
|  | 1. Deregisters instance i-9912 from Application Load Balancer Target Group                  |  |
|  | 2. Waits for active connection draining (60 seconds)                                        |  |
|  | 3. Stops instance i-9912 and restarts it (Launches on a fresh, healthy physical host!)     |  |
|  | 4. Re-registers instance with ALB -> Verifies Health Check -> Marks Incident Resolved        |  |
|  +---------------------------------------------------------------------------------------------+  |
|                                                                                                   |
|  -----------------------------------------------------------------------------------------------  |
|                                                                                                   |
|  [CHAOS ENGINEERING VALIDATION: AWS FAULT INJECTION SERVICE (FIS)]                                |
|  - Injects synthetic network latency (200ms) between App and RDS Database                        |
|  - Validates that application Circuit Breaker trips gracefully and serves cached read data         |
|  - If CloudWatch Alarm `CriticalErrors > 5%` fires -> FIS Stop Condition aborts experiment!        |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 CLI Commands for AWS Health, Quotas & Trusted Advisor

```bash
# 1. Query active AWS Personal Health events impacting your account
aws health describe-events \
    --filter eventStatusCodes=open \
    --region us-east-1

# 2. Check current usage against service quota (e.g., VPCs per region)
aws service-quotas get-service-quota \
    --service-code vpc \
    --quota-code L-F678F13C

# 3. Request an automated service quota increase
aws service-quotas request-service-quota-increase \
    --service-code vpc \
    --quota-code L-F678F13C \
    --desired-value 10

# 4. Describe Trusted Advisor checks and status
aws support describe-trusted-advisor-checks \
    --language en
```

---

## 5. Practical Examples

### 5.1 Automated EC2 Retirement Remediation with EventBridge & SSM (Terraform)

```hcl
# EventBridge Rule capturing AWS Health EC2 Retirement notifications
resource "aws_cloudwatch_event_rule" "ec2_retirement" {
  name        = "capture-ec2-scheduled-retirement"
  description = "Trigger automated restart when an EC2 instance is scheduled for retirement"

  event_pattern = jsonencode({
    source      = ["aws.health"],
    detail-type = ["AWS Health Event"],
    detail = {
      service           = ["EC2"],
      eventTypeCode     = ["AWS_EC2_INSTANCE_RETIREMENT_SCHEDULED"]
    }
  })
}

# Target: Systems Manager Automation Document
resource "aws_cloudwatch_event_target" "ssm_remediation" {
  rule      = aws_cloudwatch_event_rule.ec2_retirement.name
  target_id = "TriggerSSMRestart"
  arn       = "arn:aws:ssm:${var.aws_region}:${var.account_id}:automation-definition/AWS-StopAndStartEC2Instance"
  role_arn  = var.ssm_automation_execution_role_arn

  input_transformer {
    input_paths = {
      instanceId = "$.detail.affectedEntities[0].entityValue"
    }
    input_template = "{\"InstanceId\": [<instanceId>]}"
  }
}
```

### 5.2 AWS Fault Injection Service (FIS) Experiment: Terminate Random Cluster Node

```json
{
  "description": "Terminate random worker node to test Auto Scaling recovery",
  "targets": {
    "TargetInstances": {
      "resourceType": "aws:ec2:instance",
      "resourceTags": {
        "Environment": "Staging",
        "Role": "WorkerNode"
      },
      "selectionMode": "PERCENT(20)"
    }
  },
  "actions": {
    "TerminateInstances": {
      "actionId": "aws:ec2:terminate-instances",
      "parameters": {},
      "targets": {
        "Instances": "TargetInstances"
      }
    }
  },
  "stopConditions": [
    {
      "source": "aws:cloudwatch:alarm",
      "value": "arn:aws:cloudwatch:us-east-1:123456789012:alarm:Staging5xxErrorRateHigh"
    }
  ],
  "roleArn": "arn:aws:iam::123456789012:role/FISExperimentRole"
}
```

### 5.3 Requesting Service Quota Increase Programmatically (Python)

```python
"""
Monitors service quota utilization and requests automated increase if threshold exceeds 80%.
"""
import boto3

quotas_client = boto3.client("service-quotas", region_name="us-east-1")
cw_client = boto3.client("cloudwatch", region_name="us-east-1")

def check_and_request_quota_increase(service_code: str, quota_code: str, current_usage: int):
    # 1. Fetch current quota limit
    response = quotas_client.get_service_quota(
        ServiceCode=service_code,
        QuotaCode=quota_code
    )
    quota = response["Quota"]
    limit_value = quota["Value"]
    quota_name = quota["QuotaName"]

    print(f"Quota: {quota_name} | Limit: {limit_value} | Current Usage: {current_usage}")

    # 2. Check if usage exceeds 80% threshold
    if current_usage >= (limit_value * 0.8):
        new_desired_value = int(limit_value * 1.5)
        print(f"Usage at {current_usage}/{limit_value} (>=80%). Requesting increase to {new_desired_value}...")
        
        request_response = quotas_client.request_service_quota_increase(
            ServiceCode=service_code,
            QuotaCode=quota_code,
            DesiredValue=new_desired_value
        )
        print(f"Request submitted! Status: {request_response['RequestedQuota']['Status']}")
```

---

## 6. Best Practices

1. **Conduct Well-Architected Reviews Prior to Production Go-Live**: Use the AWS Well-Architected Tool before every major production launch. Zero High-Risk Issues (HRIs) should be permitted into production without formal business exception approvals.
2. **Automate AWS Health Dashboard Event Responses**: Never rely on engineers reading email alerts for scheduled hardware maintenance. Route AWS Health events via EventBridge to automated SSM Automation runbooks to reboot instances during off-peak windows.
3. **Set Up Automated Alarms on Service Quota Usage**: Do not wait for a flash sale to discover your account has reached its 10,000 RPS API Gateway limit. Monitor quota utilization metrics in CloudWatch and request increases weeks in advance.
4. **Conduct Chaos Engineering in Staging with Hard Guardrails**: Run AWS FIS experiments regularly to test automated multi-AZ database failover, instance termination, and network latency. Always configure CloudWatch stop condition alarms to automatically abort experiments if blast radiuses escape.
5. **Regularly Audit Trusted Advisor Recommendations**: Review the Trusted Advisor dashboard weekly. Automate cost cleanup (terminating unattached Elastic IPs, deleting unattached EBS volumes, rightsizing instances).

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Hitting Service Quota in Production | Relying on default soft limits during sudden business growth. | AWS API rejects calls (`LimitExceededException`); downtime. | Monitor Service Quotas and request increases in advance. |
| Ignoring Scheduled Retirement Emails | AWS emails notification that an EC2 hardware host is failing. | Instance is forcibly terminated on scheduled deadline; downtime. | Migrate instance to a new host immediately by stopping and starting it. |
| Running Chaos Experiments Without Guardrails | Running FIS tests without automated stop conditions. | Experiment runs out of control, causing cascading system outages. | Always define CloudWatch Alarms in FIS `stopConditions`. |
| Deploying Single-AZ Stateful Services | Deploying an RDS database in a single Availability Zone. | Outage in that specific AZ takes the entire company down for hours. | Always enable `Multi-AZ: true` on production databases. |
| Neglecting Well-Architected HRIs | Treating Well-Architected reviews as a check-the-box exercise. | Architecture has single points of failure that cause massive outages. | Prioritize and track HRI remediation in sprint backlogs. |
| Missing Enterprise Support for Trusted Advisor | Free basic support tier only includes 7 core checks. | Misses dozens of critical fault tolerance and security checks. | Upgrade to Business or Enterprise Support for full 100+ check access. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance & High Availability
- **Multi-AZ Resilience**: Distribute all compute tiers across at least three Availability Zones. If one AZ experiences a flood or power cut, surviving AZs absorb the load without customer disruption.

### 8.2 Security
- **FIS IAM Safeguards**: AWS FIS requires dedicated IAM roles with granular action permissions. Service control policies (SCPs) can enforce that FIS experiments cannot run in production accounts without explicit executive break-glass approvals.

### 8.3 Cost Breakdown
- **AWS Well-Architected Tool**: **100% Free**.
- **AWS Health Dashboard**: Free (Personal Health events are included for all accounts).
- **Service Quotas API**: Free.
- **AWS Trusted Advisor**: 7 core checks are free; 100+ full checks require Business or Enterprise Support plans.
- **AWS FIS**: \$0.10 per action-minute.

---

## 9. Comparison Table

| Tool / Service | Primary Focus | Proactive vs Reactive | Action Mechanism |
|---|---|---|---|
| **Well-Architected Tool** | Architectural Governance & HRIs | Proactive (Design Phase) | Self-assessment questionnaire & plan |
| **AWS Health Dashboard** | Infrastructure event notifications| Reactive & Predictive | EventBridge integration to Lambda/SSM |
| **AWS Trusted Advisor** | Cost, Security & Quota checks | Continuous Audit | Automated account-wide check reports |
| **Service Quotas** | Tracking & increasing soft limits| Proactive | Automated increase requests |
| **AWS FIS** | Chaos Engineering & Resilience | Proactive (Validation Phase) | Synthetic fault injection with guardrails |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What are High-Risk Issues (HRIs) in the AWS Well-Architected Framework and how are they managed?</strong></summary>

In the AWS Well-Architected Tool, an **HRI (High-Risk Issue)** represents an architectural decision that violates fundamental AWS best practices and introduces severe risk of outage, security breach, data loss, or uncontrollable cost. Examples include storing database backups in a single region without immutability, deploying production databases in a single AZ, or using root credentials for applications. 
Enterprises manage HRIs by generating Well-Architected Milestone reports and tracking HRI remediation tickets in Jira/GitLab sprint backlogs as mandatory technical debt items.
</details>

<details>
<summary><strong>Q2: What is the difference between the AWS Service Health Dashboard and the AWS Personal Health Dashboard?</strong></summary>

- **AWS Service Health Dashboard**: The public global status board (`status.aws.amazon.com`). It reports broad, macro-level outages affecting entire AWS regions (e.g., "S3 is experiencing elevated error rates in us-east-1").
- **AWS Personal Health Dashboard (PHD)**: A personalized, authenticated view tailored exclusively to *your* AWS account. It reports events directly impacting *your* specific resources (e.g., specific EC2 instances scheduled for hardware retirement, SSL certificate expiration alerts, or billing threshold breaches) and emits events to your default EventBridge bus.
</details>

<details>
<summary><strong>Q3: How do you handle an AWS_EC2_INSTANCE_RETIREMENT_SCHEDULED notification?</strong></summary>

This notification indicates that the underlying physical hardware host running your EC2 instance has degraded or is scheduled for maintenance. 
To resolve:
1. Schedule a maintenance window (or use an automated EventBridge Lambda rule).
2. Simply **Stop and Start the instance** via the console or CLI:
   `aws ec2 stop-instances --instance-ids <i>` followed by `aws ec2 start-instances --instance-ids <i>`.
3. When an EBS-backed instance stops and restarts, AWS automatically relocates the virtual machine to a completely different, healthy physical host in the same Availability Zone.
4. Verify application health and close the maintenance ticket.
</details>

<details>
<summary><strong>Q4: What is Chaos Engineering and why is it essential for cloud applications?</strong></summary>

Chaos Engineering is the discipline of experimenting on a system to build confidence in its capability to withstand turbulent conditions in production. 
Rather than assuming that multi-AZ failovers, autoscaling groups, and circuit breakers will work during an unexpected 3:00 AM emergency, teams use **AWS Fault Injection Service (FIS)** to proactively introduce controlled disruptions during business hours (e.g., terminating instances, cutting network connections to RDS, introducing 500ms API latency). This reveals hidden failure modes, incorrect timeouts, and missing alarms before real customer outages occur.
</details>

<details>
<summary><strong>Q5: What are AWS FIS Stop Conditions and why are they critical?</strong></summary>

A Stop Condition is an emergency brake mechanism in AWS Fault Injection Service. It binds an active chaos experiment to one or more **Amazon CloudWatch Alarms** (e.g., `Production5xxRate > 2%` or `TargetResponseTime > 1s`). 
If the injected fault cascades beyond the intended blast radius and causes the alarm to fire, FIS immediately aborts the experiment, rolls back the injected faults, and restores the system to normal operation automatically.
</details>

<details>
<summary><strong>Q6: How do you monitor AWS Service Quotas to prevent production disruptions?</strong></summary>

1. Use the **Service Quotas console** to identify default limits for critical services (e.g., VPC Elastic IPs, NLBs, API Gateway throttle rates).
2. CloudWatch publishes metrics under the `AWS/Usage` namespace for services supporting quota tracking.
3. Set CloudWatch Alarms to fire when usage reaches **80% of the quota limit**.
4. Use the Service Quotas API or Terraform to automatically request quota increases weeks prior to expected traffic surges or marketing campaigns.
</details>

<details>
<summary><strong>Q7: What is the difference between a Hard Limit and a Soft Limit in AWS?</strong></summary>

- **Soft Limit (Service Quota)**: A default baseline limit assigned by AWS to protect accounts from runaway billing and protect multi-tenant infrastructure. Soft limits **can be increased** by submitting a request via the Service Quotas console or API (e.g., increasing VPCs per region from 5 to 20).
- **Hard Limit**: A fixed architectural or physical limitation that **cannot be increased under any circumstances** (e.g., maximum S3 object size is 5 TB; maximum SQS message size is 256 KB; maximum IAM policy size is 6,144 characters).
</details>

<details>
<summary><strong>Q8: What are the five pillars evaluated by AWS Trusted Advisor?</strong></summary>

1. **Cost Optimization**: Eliminating unused and idle resources.
2. **Security**: Auditing permissions, open ports, and encryption settings.
3. **Fault Tolerance**: Evaluating multi-AZ redundancy, backup intervals, and autoscaling.
4. **Performance**: Identifying throughput bottlenecks and over-utilized resources.
5. **Service Limits**: Monitoring account usage against soft quota limits.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Conduct a Well-Architected Review in the Console</strong></summary>

**Goal**: Evaluate a mock workload.
1. Open the AWS Well-Architected Tool.
2. Define a workload named `EcommerceCore`.
3. Answer questions for the **Reliability Pillar** (focusing on Multi-AZ and automated recovery).
4. Generate the PDF Milestone report and review the identified High-Risk Issues.
</details>

<details>
<summary><strong>Exercise 2: Query Active Personal Health Events via AWS CLI</strong></summary>

**Goal**: Inspect health status programmatically.
1. Run `aws health describe-events --filter eventStatusCodes=open`.
2. Inspect any open notifications or scheduled maintenance events.
3. Check affected entities using `describe-affected-entities`.
</details>

<details>
<summary><strong>Exercise 3: Request a Service Quota Increase via AWS CLI</strong></summary>

**Goal**: Programmatically request additional capacity.
1. Find the quota code for EC2 Elastic IPs: `aws service-quotas list-service-quotas --service-code ec2`.
2. Request an increase: `aws service-quotas request-service-quota-increase --service-code ec2 --quota-code L-0263D0A3 --desired-value 10`.
3. Check the request status using `get-requested-service-quota-change`.
</details>

<details>
<summary><strong>Exercise 4: Review Trusted Advisor Fault Tolerance Checks</strong></summary>

**Goal**: Identify single points of failure.
1. Open AWS Trusted Advisor -> Fault Tolerance.
2. Review the status of "Amazon RDS Multi-AZ" and "Amazon S3 Bucket Logging".
3. Export the recommendations to CSV.
</details>

<details>
<summary><strong>Exercise 5: Build a Basic AWS FIS Experiment Template</strong></summary>

**Goal**: Design a safe chaos experiment.
1. In AWS FIS, create an experiment template targeting test EC2 instances tagged `Environment=Test`.
2. Select action `aws:ec2:stop-instances` with duration 5 minutes.
3. Attach a CloudWatch Alarm stop condition.
4. Review the template JSON.
</details>

<details>
<summary><strong>Exercise 6: Simulate Instance Reboot Remediation with EventBridge</strong></summary>

**Goal**: Test automated health reaction.
1. Create an EventBridge rule capturing AWS Health events.
2. Target an SNS topic that sends an email alert.
3. Trigger a test event via AWS EventBridge Sandbox and verify email receipt.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| AWS RELIABILITY & RESILIENCE TOOLKIT                                                              |
+--------------------------+-----------------------+------------------------------------------------+
| Tool / Service           | Target Area           | Key Metric / Trigger                           |
+--------------------------+-----------------------+------------------------------------------------+
| Well-Architected Tool    | Architectural Review  | High-Risk Issues (HRIs) / Milestone Reports    |
| Personal Health Dashboard| Infrastructure Health | EventBridge notifications for host retirements |
| Service Quotas           | Capacity Boundaries   | Alarms on 80% usage; automated increase requests|
| Trusted Advisor          | Account Optimization  | 5 Pillars: Cost, Security, Reliability, Limits |
| Fault Injection Service  | Chaos Engineering     | Synthetic fault injection + Stop Condition Ctl |
+--------------------------+-----------------------+------------------------------------------------+
```

- **Zero HRIs**: Remediate High-Risk Issues before deploying to production.
- **Stop and Start**: Relocates compromised or scheduled-retirement EC2 hosts cleanly.
- **Chaos Testing**: Test failover automation proactively with AWS FIS.

---

## 13. Further Reading

- [AWS Well-Architected Framework Whitepaper](https://docs.aws.amazon.com/wellarchitected/latest/framework/)
- [AWS Health User Guide](https://docs.aws.amazon.com/health/latest/ug/what-is-aws-health.html)
- [AWS Service Quotas User Guide](https://docs.aws.amazon.com/servicequotas/latest/userguide/intro.html)
- [AWS Fault Injection Service (FIS) User Guide](https://docs.aws.amazon.com/fis/latest/userguide/)
- [06-operations/08-observability.md](file:///04-devops/aws/06-operations/08-observability.md)
