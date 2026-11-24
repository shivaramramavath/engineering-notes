# AWS Threat Detection & Security Governance: GuardDuty, Security Hub, Inspector & Macie

> **Module**: `06-operations`  
> **Topic**: Threat Intelligence, Cloud Security Posture Management (CSPM), CVE Scanning & Data Discovery  
> **Target Audience**: Cloud Security Architects, DevSecOps Engineers, Compliance Officers  
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

Securing enterprise cloud infrastructure manually is impossible given the scale and velocity of modern deployments. An organization with hundreds of AWS accounts generates billions of events daily across VPC Flow Logs, DNS queries, and API calls. Threat actors continuously scan for compromised IAM access keys, vulnerable open-source software libraries, open S3 buckets containing personally identifiable information (PII), and cryptocurrency mining malware.

AWS provides an intelligent, automated suite of security and compliance services:
1. **Amazon GuardDuty**: An intelligent threat detection service that continuously monitors AWS accounts and workloads for malicious activity and unauthorized behavior using machine learning and threat intelligence feeds.
2. **AWS Security Hub**: A Cloud Security Posture Management (CSPM) service that aggregates, organizes, and prioritizes security alerts from multiple AWS services and third-party tools, evaluating resources against industry compliance standards (CIS AWS Foundations, PCI-DSS).
3. **Amazon Inspector**: An automated vulnerability management service that continuously scans Amazon EC2 instances, container images in Amazon ECR, and AWS Lambda functions for software vulnerabilities (CVEs) and unintended network exposure.
4. **Amazon Macie**: A data security service that uses machine learning and pattern matching to discover, classify, and protect sensitive data (PII, financial records, API keys) stored in Amazon S3.

---

## 2. Core Concepts

### 2.1 Amazon GuardDuty (Threat Detection)
- Operates completely out-of-band: analyzes foundational AWS telemetry streams without installing agents or impacting workload performance.
- **Data Sources Analyzed**:
  - AWS CloudTrail management and S3 data events.
  - VPC Flow Logs (network traffic metadata).
  - Amazon Route 53 Resolver DNS query logs.
  - Optional Protection Plans: **EKS Audit & Runtime Monitoring**, **RDS Login Activity**, **Lambda Network Activity**, and **EBS Malware Protection** (agentless scanning of EBS volume snapshots).
- **Threat Types Detected**: Cryptocurrency mining (`CryptoCurrency:EC2/BitcoinTool.B!DNS`), compromised credentials (`UnauthorizedAccess:IAMUser/InstanceCredentialExfiltration`), TOR exit node communication, brute-force SSH/RDP attacks.

### 2.2 AWS Security Hub (CSPM & Aggregation)
- Central security pane-of-glass for the enterprise.
- Ingests findings from GuardDuty, Inspector, Macie, IAM Access Analyzer, AWS Firewall Manager, and partner tools.
- Evaluates resources against formal **Security Standards**:
  - *AWS Foundational Security Best Practices (FSBP)*.
  - *CIS AWS Foundations Benchmark v1.4.0 / v3.0.0*.
  - *PCI-DSS v3.2.1*.
  - *NIST SP 800-53*.
- Generates an overall **Security Score** (0-100%) and supports **Automated Security Response** via EventBridge.

### 2.3 Amazon Inspector (Vulnerability Management)
- Automatically detects software vulnerabilities (Common Vulnerabilities and Exposures - CVEs) and unintended open ports.
- **Coverage**:
  - *EC2 Scanning*: Agent-based (via AWS Systems Manager SSM Agent) or agentless hybrid scanning.
  - *ECR Container Image Scanning*: Scans container layers upon push and re-scans continuously when new CVEs are published.
  - *Lambda Code & Dependency Scanning*: Scans function application code and third-party dependencies (npm, pip).

### 2.4 Amazon Macie (Sensitive Data Discovery)
- Uses machine learning and managed regex data identifiers to locate sensitive data across S3 buckets.
- Detects PII: Social Security Numbers, Passport numbers, credit card CVVs, medical records, and AWS secret keys.
- Evaluates S3 bucket security posture: public accessibility, unencrypted buckets, and buckets shared outside your AWS Organization.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                        AWS ENTERPRISE SECURITY & THREAT MITIGATION PIPELINE                       |
|                                                                                                   |
|  [Underlying Workloads: EC2, EKS, Lambda, S3, IAM, Network Traffic]                              |
|         |                                 |                                 |                     |
|         v (Telemetry Streams)             v (Vulnerability Scan)            v (Object Scanning)   |
|  +-----------------------------+   +-----------------------------+   +--------------------------+ |
|  | AMAZON GUARDDUTY            |   | AMAZON INSPECTOR            |   | AMAZON MACIE             | |
|  | Out-of-band ML Analysis:    |   | Continuous CVE Detection:   |   | S3 Data Discovery:       | |
|  | - CloudTrail API Events     |   | - EC2 Package Vulnerabilities|  | - ML PII Classification  | |
|  | - VPC Flow Logs & DNS Logs  |   | - ECR Docker Container CVEs |   | - Credit Card / Passport | |
|  | - EKS Runtime & Malware     |   | - Lambda Dependency Scans   |   | - Public S3 Bucket Alert | |
|  +--------------+--------------+   +--------------+--------------+   +--------------+-----------+ |
|                 |                                 |                                 |             |
|                 +------------------------+--------+---------------------------------+             |
|                                          |                                                        |
|                                          v (Normalized AWS Security Finding Format - ASFF)        |
|  +---------------------------------------------------------------------------------------------+  |
|  | AWS SECURITY HUB (Centralized CSPM & Compliance Dashboard)                                  |  |
|  | - Aggregates, correlates, and scores findings (CIS / FSBP standards)                         |  |
|  | - Computes Enterprise Security Score: e.g., 91% Compliant                                   |  |
|  +---------------------------------------------------------------------------------------------+  |
|                                          |                                                        |
|                                          v (EventBridge Rule on High/Critical Severity)           |
|  +---------------------------------------------------------------------------------------------+  |
|  | AUTOMATED REMEDIATION & NOTIFICATION ENGINE                                                 |  |
|  |                                                                                             |  |
|  |   [Action 1: GuardDuty Isolator] ---------> Lambda attaches Quarantine SG to EC2 instance   |  |
|  |                                                                                             |  |
|  |   [Action 2: Macie S3 Enforcer] ----------> Lambda enables S3 Block Public Access instantly |  |
|  |                                                                                             |  |
|  |   [Action 3: Security Team Alert] --------> EventBridge API Destination -> PagerDuty / Slack|  |
|  +---------------------------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Enabling GuardDuty, Security Hub & Inspector via AWS CLI

```bash
# 1. Enable Amazon GuardDuty with S3 Protection
aws guardduty create-detector \
    --enable \
    --data-sources '{"S3Logs": {"Enable": true}, "Kubernetes": {"AuditLogs": {"Enable": true}}}'

# 2. Enable AWS Security Hub and attach the AWS Foundational Security standard
aws securityhub enable-security-hub \
    --enable-default-standards

# 3. Enable Amazon Inspector across EC2, ECR, and Lambda
aws inspector2 enable \
    --resource-types "EC2" "ECR" "LAMBDA"

# 4. Enable Amazon Macie
aws macie2 enable-macie \
    --status "ENABLED"
```

---

## 5. Practical Examples

### 5.1 Multi-Account Security Suite Setup via Terraform

```hcl
# 1. GuardDuty Detector
resource "aws_guardduty_detector" "primary" {
  enable = true

  datasources {
    s3_logs {
      enable = true
    }
    kubernetes {
      audit_logs {
        enable = true
      }
    }
    malware_protection {
      scan_ec2_instance_with_findings {
        ebs_volumes {
          enable = true
        }
      }
    }
  }
}

# 2. AWS Security Hub
resource "aws_securityhub_account" "main" {
  enable_default_standards = true
  control_finding_generator = "SECURITY_CONTROL"
}

# 3. Amazon Inspector
resource "aws_inspector2_enabler" "all_resources" {
  account_ids    = [var.account_id]
  resource_types = ["EC2", "ECR", "LAMBDA", "LAMBDA_CODE"]
}

# 4. Amazon Macie
resource "aws_macie2_account" "main" {
  finding_publishing_frequency = "FIFTEEN_MINUTES"
  status                       = "ENABLED"
}
```

### 5.2 Automated Incident Remediation with EventBridge & Lambda

```hcl
# EventBridge Rule triggering on GuardDuty High Severity (> 7.0) findings
resource "aws_cloudwatch_event_rule" "guardduty_critical" {
  name        = "guardduty-critical-remediation"
  description = "Trigger immediate automated quarantine on critical GuardDuty findings"

  event_pattern = jsonencode({
    source      = ["aws.guardduty"],
    detail-type = ["GuardDuty Finding"],
    detail = {
      severity = [{ "numeric" : [">=", 7.0] }]
    }
  })
}

# Target: Quarantine Lambda Function
resource "aws_cloudwatch_event_target" "quarantine_target" {
  rule      = aws_cloudwatch_event_rule.guardduty_critical.name
  target_id = "TriggerQuarantineLambda"
  arn       = aws_lambda_function.quarantine.arn
}
```

### 5.3 Automated EC2 Isolation Lambda Function (Python)

```python
"""
Isolates a compromised EC2 instance by swapping its security group to an isolation SG.
"""
import boto3
import json

ec2 = boto3.client("ec2")
ISOLATION_SG_ID = "sg-0quarantine987654321"

def lambda_handler(event, context):
    detail = event["detail"]
    finding_type = detail.get("type", "")
    resource = detail.get("resource", {})
    instance_details = resource.get("instanceDetails", {})
    instance_id = instance_details.get("instanceId")

    if not instance_id:
        print("No EC2 instance identified in finding. Skipping.")
        return

    print(f"CRITICAL THREAT: {finding_type} on instance {instance_id}!")
    print(f"Applying Isolation Security Group {ISOLATION_SG_ID}...")

    # Overwrite instance security groups with the isolation SG (blocks all ingress/egress)
    ec2.modify_instance_attribute(
        InstanceId=instance_id,
        Groups=[ISOLATION_SG_ID]
    )

    print(f"Successfully isolated instance {instance_id}. Forensic preservation ready.")
```

### 5.4 Macie Sensitive Data Discovery Job (Terraform)

```hcl
resource "aws_macie2_classification_job" "audit_customer_uploads" {
  job_type = "SCHEDULED"
  name     = "weekly-pii-customer-uploads-audit"

  schedule_frequency {
    weekly_schedule {
      day_of_week = "SUNDAY"
    }
  }

  s3_job_definition {
    bucket_definitions {
      account_id = var.account_id
      buckets    = [var.customer_uploads_bucket_name]
    }
  }

  # Scan only objects modified in the last week
  sampling_percentage = 100
}
```

---

## 6. Best Practices

1. **Designate a Delegated Administrator Account**: In AWS Organizations, never enable security services inside the Root Management account. Designate a dedicated **Security / Audit Account** as the Delegated Administrator for GuardDuty, Security Hub, Inspector, and Macie to isolate security operations.
2. **Auto-Enable Services in New Member Accounts**: Configure Security Hub and GuardDuty auto-enablement at the Organization level so that whenever a new AWS account is created or joined, security monitoring is provisioned automatically.
3. **Turn On GuardDuty Malware Protection**: Enable EBS Volume Scanning in GuardDuty. If an instance exhibits command-and-control behavior, GuardDuty automatically takes an agentless snapshot of the EBS volume and scans it for trojans and rootkits without impacting the running instance.
4. **Implement Automated Response for High-Confidence Findings**: Do not wait for humans to review critical findings like leaked IAM access keys or cryptocurrency miners. Use EventBridge to automatically revoke compromised IAM session tokens and isolate EC2 instances within seconds.
5. **Tune Macie Discovery with Classification Scopes**: Macie charges per GB of S3 data inspected. Exclude known binary buckets (video files, compiled tarballs, raw images) using S3 exclusion prefixes to avoid unnecessary inspection costs.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Alert Fatigue from Low Severity Alerts | Paging on-call engineers for Security Hub Informational findings. | Engineers ignore critical alerts due to sheer noise. | Filter notifications in EventBridge: route Informational to Jira, and Critical/High ($>7.0$) to PagerDuty. |
| Neglecting GuardDuty Findings | Leaving GuardDuty active without downstream EventBridge alerts. | Breaches persist for months undetected in the console. | Route findings to SNS/Slack or SIEM (Splunk, Datadog) immediately. |
| Running Unconstrained Macie Scans | Running Macie across a 50 TB video streaming bucket. | Massive unexpected AWS bill (\$1.00/GB scanned = \$50,000!). | Configure Macie job filters to inspect only `.csv`, `.json`, `.pdf`, `.docx` files. |
| Missing SSM Agent on EC2 for Inspector | EC2 instances launched from custom AMIs lack Systems Manager SSM agent. | Inspector cannot perform deep package CVE scanning. | Ensure SSM agent is installed, running, and has `AmazonSSMManagedInstanceCore` IAM policy. |
| Enabling Standards Blindly in Security Hub | Enabling all 5 standards simultaneously on day one. | Thousands of failed controls overwhelm engineering teams. | Enable **AWS Foundational Security Best Practices (FSBP)** first; address findings incrementally. |
| Forgetting Regional Scope | Enabling GuardDuty in `us-east-1` but leaving `eu-west-1` disabled. | Attackers spin up unauthorized miners in unmonitored regions. | GuardDuty and Security Hub must be enabled across **all active regions** via Organizations. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance Impact
- **Zero Workload Overhead (GuardDuty & Macie)**: GuardDuty inspects raw AWS hypervisor and network telemetry streams out-of-band; Macie reads S3 objects independently. Neither service consumes EC2 CPU, memory, or network bandwidth.
- **Inspector Lightweight Scans**: Inspector uses the lightweight SSM agent to inspect package manifests; no long-running port scans or CPU spikes occur on target instances.

### 8.2 Security & Compliance
- **ASFF Standard Format**: Security Hub normalizes all findings into the **AWS Security Finding Format (ASFF)**, providing standardized JSON structures (ID, Title, Severity, Resources, Remediation URL) for easy parsing by automation scripts.

### 8.3 Cost Breakdown
- **Amazon GuardDuty**: Billed based on CloudTrail event volume, VPC Flow Log gigabytes analyzed, and DNS query counts (first 30 days free trial per account).
- **AWS Security Hub**: \$0.0010 per security check evaluated (first 10,000 checks/month free per account).
- **Amazon Inspector**:
  - EC2 instance: \$1.25 per instance per month.
  - ECR container image: \$0.09 per initial scan + \$0.01 per rescan.
  - Lambda function: \$0.30 per function per month.
- **Amazon Macie**: \$1.00 per GB of S3 data analyzed (first 1 GB/month free).

---

## 9. Comparison Table

| Dimension | Amazon GuardDuty | AWS Security Hub | Amazon Inspector | Amazon Macie |
|---|---|---|---|---|
| **Primary Domain** | Threat Detection & Anomalies | CSPM & Compliance Benchmarks| Vulnerability Scanning (CVEs)| Data Privacy & PII Discovery|
| **Inspection Target** | Network logs, API calls, DNS | Resource configurations | OS packages, Docker, Lambda | S3 bucket contents |
| **Detection Method** | ML anomaly + Threat Intel | Rules matching CIS/FSBP | CVE database comparison | ML & Regex pattern matching|
| **Agent Required** | None (Out-of-band) | None (API based) | SSM Agent (EC2) / Serverless| None (S3 API based) |
| **Primary Output** | Threat Findings (Compromised)| Compliance Score & Controls | Vulnerabilities & CVSS Scores| Sensitive Data Findings |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: How does Amazon GuardDuty detect threats without requiring agents on EC2 instances?</strong></summary>

Amazon GuardDuty operates entirely **out-of-band** at the AWS infrastructure fabric layer. It does not inspect host memory or install OS agents. Instead, it ingests and analyzes AWS foundational telemetry streams:
1. **AWS CloudTrail Management & S3 Data Events**: Analyzes API call patterns (identifying anomalous API calls from unexpected countries or unusual times).
2. **VPC Flow Logs**: Analyzes network flow metadata (detecting connections to known malicious botnet command-and-control IPs or cryptocurrency mining pools).
3. **Route 53 Resolver DNS Logs**: Detects DNS queries resolving to malicious domains or DNS data exfiltration attempts.
AWS machine learning models process these logs independently, meaning zero CPU, memory, or network performance impact on customer instances.
</details>

<details>
<summary><strong>Q2: What is AWS Security Hub and how does it calculate compliance scores?</strong></summary>

AWS Security Hub is a Cloud Security Posture Management (CSPM) service. It automatically checks resources against industry compliance standards such as the **AWS Foundational Security Best Practices (FSBP)** and **CIS AWS Foundations Benchmark**. 
Each standard consists of a collection of specific security controls (e.g., "Ensure MFA is enabled for root", "Ensure no S3 buckets allow public write"). Security Hub runs continuous automated checks; the **Security Score** represents the percentage of passed security controls across all evaluated resources.
</details>

<details>
<summary><strong>Q3: What is the AWS Security Finding Format (ASFF) and why is it important for DevSecOps?</strong></summary>

Prior to Security Hub, every security tool (GuardDuty, Inspector, Macie, third-party SIEMs) emitted alerts in completely different JSON structures, requiring custom parsing code for each tool. 
**ASFF** is a standardized, comprehensive JSON specification used by Security Hub to normalize all security findings across all tools. It standardizes key fields—such as `Severity.Label`, `Resources[].Id`, `Types`, and `Remediation.Recommendation.Url`—allowing engineering teams to write a single EventBridge automation rule that parses and remediates alerts from any security tool consistently.
</details>

<details>
<summary><strong>Q4: How does Amazon Inspector continuously monitor container images in Amazon ECR?</strong></summary>

When an image is pushed to an ECR repository, Amazon Inspector automatically extracts the image layers and compares the installed operating system packages and language libraries against known vulnerability databases (such as the National Vulnerability Database - NVD). 
Unlike legacy scanners that run once at build time, Inspector maintains an active inventory of deployed images. When a **new CVE is published** weeks or months later, Inspector automatically re-evaluates the stored package inventory and generates a finding without requiring the container image to be rebuilt or re-pushed.
</details>

<details>
<summary><strong>Q5: What are GuardDuty Malware Protection and Runtime Monitoring?</strong></summary>

- **GuardDuty Malware Protection**: When GuardDuty detects suspicious activity on an EC2 instance, it automatically creates a snapshot of the instance's EBS volumes, duplicates it to an isolated service account, and performs an agentless antivirus/malware scan for trojans, worms, and rootkits before deleting the snapshot.
- **EKS Runtime Monitoring**: Uses a lightweight, secure eBPF container agent on Amazon EKS worker nodes to monitor operating system-level events (file access, process executions, network connections) in real time to detect container escapes and unauthorized execution.
</details>

<details>
<summary><strong>Q6: How do you optimize costs when running Amazon Macie across petabyte-scale S3 data lakes?</strong></summary>

Macie charges \$1.00 per GB of S3 data inspected. Scanning petabytes blindly will result in massive costs. 
To optimize:
1. **Enable Automated Sensitive Data Discovery**: Samples a tiny, intelligent percentage of objects across all buckets to identify high-risk buckets for a fraction of the cost.
2. **Exclude Non-Text Buckets**: Configure Classification Scopes to exclude file extensions that cannot contain PII (e.g., `.mp4`, `.png`, `.jpg`, `.tar.gz`, `.parquet`).
3. **Scan Incremental Objects Only**: Configure targeted, scheduled discovery jobs that inspect only objects modified within the last 7 days.
</details>

<details>
<summary><strong>Q7: What is the Delegated Administrator model in AWS Organizations for security services?</strong></summary>

Best practices dictate that the AWS Organizations Management (Root) account should be used exclusively for organization governance and billing, with zero day-to-day operational workloads. 
The **Delegated Administrator** feature allows the Management account to delegate complete administrative control of GuardDuty, Security Hub, Inspector, and Macie to a dedicated **Security / Tooling Account**. The security team operates out of this dedicated account, managing organization-wide policies and viewing aggregated findings across all member accounts without having Root access.
</details>

<details>
<summary><strong>Q8: What is the difference between an Inspector CVE finding and a GuardDuty finding?</strong></summary>

- **Amazon Inspector** detects **Vulnerabilities / Weaknesses** (State of Risk): It identifies software that *could* be exploited (e.g., "Apache Log4j CVE-2021-44228 is installed on EC2 instance A"). It does not indicate that an attack is actively occurring.
- **Amazon GuardDuty** detects **Active Threats / Behaviors** (State of Breach): It identifies unauthorized activity actively happening right now (e.g., "EC2 instance A is communicating with a known cryptocurrency mining pool or Tor exit node").
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Enable GuardDuty and Generate Mock Sample Findings</strong></summary>

**Goal**: Inspect finding details and severity ratings.
1. Enable GuardDuty via CLI or Console.
2. In GuardDuty Settings, click "Generate sample findings".
3. Inspect finding `Recon:IAMUser/TorIPCaller` and `CryptoCurrency:EC2/BitcoinTool.B!DNS`.
4. Review the raw JSON payload and note the ASFF structure.
</details>

<details>
<summary><strong>Exercise 2: Review Security Hub Security Standards and Controls</strong></summary>

**Goal**: Evaluate account security posture against FSBP.
1. Enable AWS Security Hub.
2. Navigate to Security Standards -> AWS Foundational Security Best Practices.
3. Review failed controls (e.g., "S3.1 S3 Block Public Access should be enabled").
4. Click on a failed control and inspect the remediation guidance URL.
</details>

<details>
<summary><strong>Exercise 3: Trigger Automated Isolation via EventBridge and Lambda</strong></summary>

**Goal**: Test automated threat containment.
1. Create an EventBridge rule capturing GuardDuty sample findings.
2. Target a Lambda function that prints "Quarantining instance...".
3. Trigger a sample finding in GuardDuty and verify the Lambda execution log in CloudWatch.
</details>

<details>
<summary><strong>Exercise 4: Scan an ECR Container Image with Amazon Inspector</strong></summary>

**Goal**: Identify vulnerabilities in a container image.
1. Push an older Docker image with known vulnerabilities (e.g., `alpine:3.10` or an old Node image) to Amazon ECR.
2. Open Amazon Inspector -> ECR Container Scans.
3. Review the identified CVE list, CVSS severity scores, and package fix versions.
</details>

<details>
<summary><strong>Exercise 5: Run an Amazon Macie PII Discovery Job</strong></summary>

**Goal**: Discover sensitive data in an S3 bucket.
1. Create an S3 bucket and upload a sample text file containing a simulated credit card number and SSN.
2. In Amazon Macie, create a One-Time Classification Job targeting that bucket.
3. Run the job and review the generated Macie finding detailing the exact PII type identified.
</details>

<details>
<summary><strong>Exercise 6: Enable EBS Malware Protection in GuardDuty</strong></summary>

**Goal**: Protect workloads against trojans and rootkits.
1. In GuardDuty settings, enable Malware Protection for EC2.
2. Verify IAM role permissions are automatically provisioned.
3. Review CloudWatch metrics for `MalwareProtectionScansCompleted`.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| AWS SECURITY DETECTION & GOVERNANCE STACK                                                         |
+--------------------------+-----------------------+------------------------------------------------+
| Service                  | Core Function         | Key Trigger / Mechanism                        |
+--------------------------+-----------------------+------------------------------------------------+
| Amazon GuardDuty         | Active Threat Detection| ML analysis of CloudTrail, VPC Flow & DNS logs |
| AWS Security Hub         | CSPM & Compliance Hub | Evaluates CIS & FSBP rules; aggregates ASFF    |
| Amazon Inspector         | Vulnerability (CVE)   | Package manifest scanning on EC2, ECR & Lambda |
| Amazon Macie             | PII & Data Privacy    | S3 text & metadata ML classification           |
| EventBridge Auto-Response| Automated Remediation | Listens for High/Critical ASFF finding events  |
+--------------------------+-----------------------+------------------------------------------------+
```

- **GuardDuty**: Threat detection (Active attacks, compromised credentials, crypto-miners).
- **Security Hub**: Posture management (Compliance scoring against CIS/FSBP).
- **Inspector**: Vulnerability management (Continuous CVE scanning).
- **Macie**: Sensitive data discovery (PII and secret detection in S3).

---

## 13. Further Reading

- [Amazon GuardDuty User Guide](https://docs.aws.amazon.com/guardduty/latest/ug/what-is-guardduty.html)
- [AWS Security Hub User Guide](https://docs.aws.amazon.com/securityhub/latest/userguide/what-is-securityhub.html)
- [Amazon Inspector User Guide](https://docs.aws.amazon.com/inspector/latest/user/what-is-inspector.html)
- [Amazon Macie User Guide](https://docs.aws.amazon.com/macie/latest/user/what-is-macie.html)
- [06-operations/06-security-and-secrets.md](file:///04-devops/aws/06-operations/06-security-and-secrets.md)
