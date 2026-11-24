# AWS Backup & Multi-Region Disaster Recovery (DR) Strategies

> **Module**: `03-storage-and-databases`  
> **Topic**: Centralized Backup Policies, Vault Lock, RTO/RPO Metrics, and DR Topologies  
> **Target Audience**: Disaster Recovery Coordinators, Cloud Security Architects, SRE Leads  
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

Unplanned outages, localized data center catastrophic failures, human operational error, and sophisticated ransomware attacks represent existential threats to modern digital enterprises. Relying on ad-hoc scripts, uncoordinated database snapshots, and un-replicated storage volumes leaves organizations vulnerable to permanent data loss and extended outages.

Establishing robust enterprise resilience requires two coordinated disciplines:
1. **Centralized Data Protection (AWS Backup)**: A unified, policy-based service automating data protection across 15+ AWS services (EBS, RDS, Aurora, DynamoDB, EFS, FSx, S3, EC2, VMware). Features **AWS Backup Vault Lock** to enforce immutable WORM protection against malicious deletion.
2. **Disaster Recovery (DR) Architecture**: Structuring application topologies to meet strict **Recovery Point Objective (RPO)** and **Recovery Time Objective (RTO)** SLAs. Strategies range from low-cost **Backup and Restore** to real-time **Multi-Region Active-Active** deployments.

---

## 2. Core Concepts

### 2.1 Disaster Recovery Metrics: RPO and RTO
- **Recovery Point Objective (RPO)**: The maximum acceptable volume of data loss measured backward in time from the moment of disruption.
  - *Example*: An RPO of 15 minutes means the organization can tolerate losing at most 15 minutes of transactional data.
- **Recovery Time Objective (RTO)**: The maximum acceptable duration of infrastructure downtime before the system is restored to operation.
  - *Example*: An RTO of 1 hour means the service must be fully operational within 60 minutes of disaster declaration.

### 2.2 The Four AWS Disaster Recovery Strategies
1. **Backup & Restore (RPO: Hours | RTO: 24+ Hours | Lowest Cost)**:
   - Data is backed up to S3 or AWS Backup and replicated cross-region.
   - Zero compute runs in the secondary region until a disaster occurs.
   - Slowest recovery; compute must be provisioned from scratch.
2. **Pilot Light (RPO: Minutes | RTO: Tens of Minutes | Low Cost)**:
   - Core data is continuously replicated to the secondary region (e.g., Aurora Global Database or DynamoDB Global Tables).
   - Core supporting infrastructure (VPCs, subnets, routing, IAM) is pre-configured.
   - Compute instances (EC2, ECS, EKS) run at **0 capacity** until failover, when Launch Templates and ASGs scale up rapidly.
3. **Warm Standby (RPO: Seconds/Minutes | RTO: Minutes | Moderate Cost)**:
   - A scaled-down, functional copy of the production environment runs 24/7 in the secondary region.
   - Capable of serving minimal background or synthetic traffic.
   - Upon disaster, the Auto Scaling Groups scale out to full production capacity in minutes.
4. **Multi-Region Active-Active (RPO: ~0 | RTO: ~0 | Highest Cost & Complexity)**:
   - Production traffic is served simultaneously from two or more AWS regions using Route 53 or Global Accelerator.
   - If one region experiences a complete failure, traffic is instantly shifted to the surviving regions with zero downtime.

### 2.3 AWS Backup Architecture
- **Backup Plan**: Policy declaring *when* (cron schedule), *how* (lifecycle retention, cold storage transitions), and *where* (target vault, cross-region copy) backups occur.
- **Backup Vault**: Encrypted container holding recovery points.
- **AWS Backup Vault Lock**: Enforces WORM (Write Once, Read Many) immutability on backups. In **Compliance Mode**, no user—including the root account—can delete recovery points or shorten retention periods.
- **Cross-Account Backup**: Copies recovery points to a physically separate AWS account (e.g., an isolated Security Vault account) to protect against rogue internal administrators or compromised production credentials.

### 2.4 AWS Elastic Disaster Recovery (AWS DRS)
Specialized continuous block-level server replication:
- Replicates on-premises physical servers, VMware VMs, or EC2 instances into a staging area in AWS.
- Uses low-cost EBS storage and minimal staging instances until failover, launching full-sized recovery instances within minutes during a disaster.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        AWS Disaster Recovery Spectrum Comparison                  |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ Cost & Complexity Increases ] -----------------------------------------------> |
|                                                                                   |
|  1. Backup & Restore      2. Pilot Light           3. Warm Standby      4. Active |
|  +------------------+     +------------------+     +------------------+  Active   |
|  | S3 Cross-Region  |     | Data Replicated  |     | Scaled-Down Live |  Multi-   |
|  | Zero Compute     |     | Zero Live Compute|     | Small Fleets Live|  Region   |
|  +------------------+     +------------------+     +------------------+  Zero     |
|  RPO: Hours               RPO: Minutes             RPO: Seconds          RPO: ~0  |
|  RTO: Days                RTO: Tens of Mins        RTO: Minutes          RTO: ~0  |
|                                                                                   |
|  -------------------------------------------------------------------------------  |
|  [ AWS Backup Centralized Governance & Immutable Vault Lock ]                     |
|                                                                                   |
|  [ Workload Services: EBS / RDS / DynamoDB / EFS / S3 / EC2 ]                     |
|              |                                                                    |
|              v (Policy-Driven Daily Snapshot Schedule)                            |
|  +-----------------------------------------------------------------------------+  |
|  | Primary Account Backup Vault (us-east-1)                                    |  |
|  | - Encrypted with AWS KMS                                                     |  |
|  +-----------------------------------------------------------------------------+  |
|              |                                                                    |
|              v (Automated Cross-Account & Cross-Region Copy)                      |
|  +-----------------------------------------------------------------------------+  |
|  | Isolated Security Account: Immutable Backup Vault (us-west-2)               |  |
|  | - AWS Backup Vault Lock: COMPLIANCE MODE                                    |  |
|  | - Rejects all DeleteRecoveryPoint API calls (Anti-Ransomware Protection)    |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Creating an Immutable Backup Vault via AWS CLI
```bash
# 1. Create a dedicated backup vault
aws backup create-backup-vault \
  --backup-vault-name "ImmutableRansomwareVault" \
  --encryption-key-arn "arn:aws:kms:us-east-1:111122223333:key/abc-123"

# 2. Lock the vault in Compliance Mode (requires minimum and maximum retention days)
aws backup put-backup-vault-lock-configuration \
  --backup-vault-name "ImmutableRansomwareVault" \
  --min-retention-days 30 \
  --max-retention-days 365 \
  --changeable-for-days 3 # 3-day grace period before lock becomes permanent
```

### 4.2 Creating an AWS Backup Plan via JSON Definition
```json
{
  "BackupPlanName": "DailyEnterprisePlan",
  "Rules": [
    {
      "RuleName": "DailySnapshotsWithCrossRegionCopy",
      "TargetBackupVaultName": "ImmutableRansomwareVault",
      "ScheduleExpression": "cron(0 2 * * ? *)",
      "StartWindowMinutes": 60,
      "CompletionWindowMinutes": 180,
      "Lifecycle": {
        "MoveToColdStorageAfterDays": 30,
        "DeleteAfterDays": 365
      },
      "CopyActions": [
        {
          "DestinationBackupVaultArn": "arn:aws:backup:us-west-2:111122223333:backup-vault:DRVault",
          "Lifecycle": {
            "DeleteAfterDays": 365
          }
        }
      ]
    }
  ]
}
```

Apply via CLI:
```bash
aws backup create-backup-plan --backup-plan file://backup-plan.json
```

---

## 5. Practical Examples

### Example 1: Declarative Terraform Configuration for Tag-Based AWS Backup
Automatically protects any resource across EBS, RDS, and DynamoDB tagged with `Backup = True`.

```hcl
resource "aws_backup_vault" "primary_vault" {
  name        = "production-central-backup-vault"
  kms_key_arn = var.kms_key_arn
}

resource "aws_backup_plan" "production_plan" {
  name = "production-compliance-backup-plan"

  rule {
    rule_name         = "daily-backup-rule"
    target_vault_name = aws_backup_vault.primary_vault.name
    schedule          = "cron(0 3 * * ? *)"

    lifecycle {
      cold_storage_after = 30
      delete_after       = 180
    }

    copy_action {
      destination_vault_arn = var.dr_region_vault_arn
      lifecycle {
        delete_after = 180
      }
    }
  }
}

resource "aws_iam_role" "backup_role" {
  name = "aws-backup-service-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Action    = "sts:AssumeRole"
      Effect    = "Allow"
      Principal = { Service = "backup.amazonaws.com" }
    }]
  })
}

resource "aws_iam_role_policy_attachment" "backup_policy" {
  role       = aws_iam_role.backup_role.name
  policy_arn = "arn:aws:iam::aws:policy/service-role/AWSBackupServiceRolePolicyForBackup"
}

resource "aws_backup_selection" "tagged_resources" {
  name         = "tagged-production-resources"
  plan_id      = aws_backup_plan.production_plan.id
  iam_role_arn = aws_iam_role.backup_role.arn

  selection_tag {
    type  = "STRINGEQUALS"
    key   = "Backup"
    value = "True"
  }
}
```

### Example 2: Restoring an EBS Volume from AWS Backup via CLI
Locate the latest recovery point and initiate a restore job.

```bash
# 1. Identify recovery point ARN
RECOVERY_POINT_ARN=$(aws backup list-recovery-points-by-backup-vault \
  --backup-vault-name "production-central-backup-vault" \
  --query 'RecoveryPoints[0].RecoveryPointArn' \
  --output text)

# 2. Start restore job
aws backup start-restore-job \
  --recovery-point-arn "$RECOVERY_POINT_ARN" \
  --metadata '{"availabilityZone": "us-east-1a", "volumeType": "gp3"}' \
  --iam-role-arn "arn:aws:iam::111122223333:role/aws-backup-service-role"
```

### Example 3: Route 53 Multi-Region Failover Routing Policy
Configure DNS health check failover directing users to secondary region if primary fails.

```json
{
  "Comment": "Failover DNS routing for disaster recovery",
  "Changes": [
    {
      "Action": "UPSERT",
      "ResourceRecordSet": {
        "Name": "api.example.com",
        "Type": "A",
        "SetIdentifier": "Primary-Region-US-East-1",
        "Failover": "PRIMARY",
        "TTL": 60,
        "HealthCheckId": "abc12345-6789-0123-4567-890123456789",
        "ResourceRecords": [{"Value": "52.0.1.10"}]
      }
    },
    {
      "Action": "UPSERT",
      "ResourceRecordSet": {
        "Name": "api.example.com",
        "Type": "A",
        "SetIdentifier": "Secondary-Region-US-West-2",
        "Failover": "SECONDARY",
        "TTL": 60,
        "ResourceRecords": [{"Value": "54.0.2.20"}]
      }
    }
  ]
}
```

### Example 4: Testing Ransomware Protection on Locked Vaults
Simulate an attacker attempting to delete a recovery point protected by AWS Backup Vault Lock.

```bash
# Attempt to delete recovery point
aws backup delete-recovery-point \
  --backup-vault-name "ImmutableRansomwareVault" \
  --recovery-point-arn "$RECOVERY_POINT_ARN"

# Result: Fails with AccessDeniedException
# "Client error: Access Denied. The recovery point cannot be deleted because the vault is locked in compliance mode."
```

### Example 5: Cross-Account Backup Copy for Air-Gapped Ransomware Isolation
Enforcing organizational security by copying recovery points to an isolated AWS account.

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "AllowCrossAccountBackupCopy",
      "Effect": "Allow",
      "Principal": {
        "AWS": "arn:aws:iam::222233334444:role/CrossAccountBackupWorker"
      },
      "Action": "backup:CopyIntoBackupVault",
      "Resource": "arn:aws:backup:us-east-1:111122223333:backup-vault:AirGappedVault"
    }
  ]
}
```

---

## 6. Best Practices

1. **Enforce AWS Backup Vault Lock in Compliance Mode**: Protect your organization against catastrophic ransomware and rogue insider attacks. Locking vaults in Compliance Mode guarantees backups cannot be modified, encrypted by external ransomware, or deleted prior to expiration.
2. **Implement Cross-Account Air-Gapped Backups**: Never store backups exclusively within the primary production AWS account. Use AWS Organizations to copy recovery points to an isolated, dedicated **Backup / Security Account** where production IAM credentials have zero access.
3. **Conduct Regular Automated DR Failover Drills**: A disaster recovery plan that has never been tested in production will fail during a real emergency. Use **AWS Fault Injection Simulator (FIS)** to simulate regional network cuts, test Route 53 health check failovers, and verify that RTO/RPO targets are achieved.
4. **Use Pilot Light Architecture for Cost-Effective Low-RPO Systems**: For mission-critical systems requiring sub-minute RPOs without paying for 100% duplicate compute 24/7, deploy Pilot Light: continuously replicate databases (Aurora Global DB) while keeping EC2/EKS compute fleets at 0 instances, scaling up via Terraform/ASGs during emergencies.
5. **Tag Resources Systematically for Policy Automation**: Use tag-based backup assignments (`Backup = True`, `Tier = Gold`) rather than manually adding resource ARNs. This guarantees that newly deployed databases or volumes are automatically enrolled in daily backup plans upon creation.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Storing Backups in the Same Account Only** | Compromised administrator credentials allow attackers to delete both production resources and their backups. | Replicate backups across accounts to an isolated, air-gapped **Security Account**. |
| **Assuming Backup & Restore Achieves 15-Min RTO** | Restoring multi-terabyte databases and rebuilding compute from scratch takes 12–24 hours. | Use **Pilot Light** or **Warm Standby** if required RTO is under 1 hour. |
| **Testing DR Without Production Traffic Volumes** | Secondary region crashes immediately upon failover due to regional service quotas or capacity limits. | Regularly pre-scale and test failover under simulated production load using chaos engineering. |
| **Locking Vaults in Compliance Mode During Testing** | Test recovery points cannot be deleted, accumulating storage bills for months with no ability to cancel. | Use **Governance Mode** during sandbox testing; lock in Compliance Mode only for verified production vaults. |
| **Forgetting KMS Key Grants in Cross-Account Copies** | Cross-account copy jobs fail with access denied errors because the destination cannot decrypt the source KMS key. | Ensure the source KMS key policy explicitly permits the destination account role to call `kms:Decrypt` and `kms:CreateGrant`. |
| **Ignoring DNS TTL During Failover** | Client operating systems cache outdated IP addresses for hours, failing to route to the secondary region. | Set Route 53 record TTLs to **60 seconds or lower** on all failover endpoints. |

---

## 8. Performance, Security & Cost Notes

### RPO / RTO Trade-Off Curve
- **Backup & Restore**: Cost: $, RPO: Hours, RTO: Days.
- **Pilot Light**: Cost: $$, RPO: Minutes, RTO: 10–30 Minutes.
- **Warm Standby**: Cost: $$$, RPO: Seconds, RTO: Minutes.
- **Active-Active**: Cost: $$$$, RPO: Real-Time, RTO: Sub-second.

### Security Hardening
- **AWS Backup Audit Manager**: Continuously monitors and evaluates backup compliance against enterprise frameworks (e.g., verifying that all databases have daily backups and 30-day retention), alerting on non-compliant resources.
- **Zero-Trust Recovery**: Implement multi-party approval via AWS Organizations SCPs to prevent tampering with backup policies.

### Cost Breakdown
- **AWS Backup Service**: 100% Free management service.
- **Backup Storage**: Billed at standard EBS snapshot ($0.05/GB/mo) or cold storage ($0.01/GB/mo) rates.
- **Cross-Region Replication Transfer**: Standard AWS inter-region data transfer fees ($0.02/GB).

---

## 9. Comparison Table

| Disaster Recovery Strategy | RPO (Data Loss) | RTO (Downtime) | Active Secondary Compute | Relative Cost |
|:---|:---|:---|:---|:---|
| **Backup and Restore** | Hours | 24+ Hours | None | $ (Lowest) |
| **Pilot Light** | < 15 Minutes | 10–30 Minutes | None (Data replicated only) | $$ (Low) |
| **Warm Standby** | Seconds | 2–5 Minutes | Scaled-down fleet running | $$$ (Medium) |
| **Multi-Region Active-Active**| Real-Time (~0) | Zero (~0) | 100% Full production fleet | $$$$ (Highest) |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the architectural difference between RPO and RTO?</strong></summary>

**Answer:**
- **Recovery Point Objective (RPO)**: The maximum acceptable volume of data loss caused by a disaster, measured backward in time from the disruption event. It answers: *"How many minutes or hours of transactional data can the business afford to lose?"* An RPO of 5 minutes requires continuous replication technologies (like Aurora Global Database or S3 RTC).
- **Recovery Time Objective (RTO)**: The maximum acceptable duration of infrastructure downtime before business operations must be restored. It answers: *"How quickly must the application be online and processing requests after disaster strikes?"* An RTO of 10 minutes requires automated DNS failover and pre-warmed compute infrastructure (Warm Standby or Pilot Light).
</details>

<details>
<summary><strong>Q2: How does AWS Backup Vault Lock in Compliance Mode protect against ransomware?</strong></summary>

**Answer:**
Ransomware attacks frequently attempt to compromise cloud administrator credentials and delete all snapshots and backups before encrypting primary databases, rendering traditional backups useless.

AWS Backup Vault Lock enforces Write Once, Read Many (WORM) immutability:
- In **Compliance Mode**, once the configurable grace period (e.g., 3 days) expires, the vault configuration is locked permanently.
- **No one—including the AWS account root user, IAM administrators, and AWS Support personnel—can delete recovery points** before their retention period expires.
- Any API call attempting `DeleteRecoveryPoint` or shortening the retention period is rejected by the AWS service core, guaranteeing that uncorrupted recovery points remain available for restoration.
</details>

<details>
<summary><strong>Q3: Explain the "Pilot Light" disaster recovery strategy.</strong></summary>

**Answer:**
In a Pilot Light architecture:
1. **Continuous Data Replication**: The core database tier is continuously replicated to the secondary AWS region (e.g., Amazon Aurora Global Database, DynamoDB Global Tables, or S3 Cross-Region Replication). The data is always live and current (low RPO).
2. **Minimal/Zero Compute**: No application servers (EC2, ECS, EKS) run in the secondary region during normal operations, saving significant compute expenses. Supporting networking (VPCs, subnets, route tables, security groups) is pre-provisioned via Infrastructure-as-Code.
3. **Failover Execution**: When a disaster occurs in the primary region, automated orchestration promotes the secondary database to primary writer and triggers Auto Scaling Groups or Launch Templates to provision the application compute fleet. Once booted, Route 53 shifts traffic. RTO is typically **10 to 30 minutes**.
</details>

<details>
<summary><strong>Q4: Why should enterprise backups be copied to a separate AWS account?</strong></summary>

**Answer:**
Storing backups in the same AWS account as production infrastructure violates the principle of separation of duties and introduces a single point of compromise:
- If a bad actor or compromised CI/CD pipeline obtains administrator privileges (`AdministratorAccess`) in the production account, they can delete both the active databases and all local backups.
- Copying backups to an **isolated Security/Backup account** (cross-account copy) ensures that credentials compromised in the production account possess zero access to the secondary vault. The secondary account can enforce strict Vault Lock and multi-factor authentication (MFA) delete policies.
</details>

<details>
<summary><strong>Q5: How does AWS Elastic Disaster Recovery (AWS DRS) work?</strong></summary>

**Answer:**
AWS DRS provides continuous, non-disruptive block-level replication of operating system volumes:
1. A lightweight agent is installed on source machines (on-premises physical servers, VMware VMs, or EC2 instances).
2. The agent replicates all storage block changes asynchronously over an encrypted TLS connection into a low-cost **Staging Area** in the target AWS region.
3. In the staging area, data is written directly to low-cost EBS volumes managed by minimal lightweight EC2 replication instances (consuming very little compute).
4. When a disaster is declared or during drills, AWS DRS automatically executes failover: it converts the staged EBS blocks into fully provisioned production EC2 instances within minutes.
</details>

<details>
<summary><strong>Q6: What is the purpose of Route 53 DNS Failover Routing?</strong></summary>

**Answer:**
Route 53 Failover Routing enables active-passive disaster recovery by monitoring the health of primary endpoints:
- Route 53 creates two records for a single domain name: a **Primary** record and a **Secondary** record.
- A **Route 53 Health Check** continuously probes the primary application endpoint (e.g., `GET /healthz` every 10 seconds across global probe nodes).
- While the primary health check passes, Route 53 responds to DNS queries with the primary region's IP/ALB.
- If consecutive health checks fail, Route 53 automatically stops serving the primary record and responds exclusively with the secondary region's IP/ALB, shifting global user traffic in under 60 seconds.
</details>

<details>
<summary><strong>Q7: What happens if an organization attempts to cancel or remove an AWS Backup Vault Lock in Compliance Mode?</strong></summary>

**Answer:**
- During the initial **cooling-off / grace period** (configured between 1 and 365 days, e.g., 3 days), administrators can delete or modify the lock configuration.
- Once the cooling-off period expires, the lock status transitions to **`LOCKED`**.
- At that point, the lock is **permanently immutable**. AWS APIs will reject any call to remove the lock or reduce retention times. Even contacting AWS Support or the AWS Chief Information Security Officer will not unlock the vault. The data can only be deleted once its retention period expires naturally.
</details>

<details>
<summary><strong>Q8: How does Multi-Region Active-Active compare to Warm Standby in terms of operational complexity?</strong></summary>

**Answer:**
- **Warm Standby**: Operational complexity is moderate. A single region actively accepts writes; the secondary region only runs scaled-down compute and receives replicated data. Database failover is a simple promotion event, avoiding distributed split-brain scenarios.
- **Multi-Region Active-Active**: Operational complexity is immense. Both regions process live read and write transactions simultaneously. This requires **multi-master distributed databases** (DynamoDB Global Tables or Aurora Multi-Master), distributed conflict-resolution algorithms (Last-Writer-Wins or CRDTs), bidirectional data sync, and strict idempotency in application code to prevent race conditions across geographies.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Centralized Backup Vault via AWS CLI</strong></summary>

**Objective**: Provision an encrypted backup vault.

**Command**:
```bash
aws backup create-backup-vault \
  --backup-vault-name "EnterpriseComplianceVault" \
  --tags Key=Environment,Value=Production
```
</details>

<details>
<summary><strong>Exercise 2: Create a Tag-Based Backup Selection</strong></summary>

**Objective**: Automatically assign resources tagged `Environment=Production` to a backup plan.

**Command**:
```bash
aws backup create-backup-selection \
  --backup-plan-id "<BACKUP_PLAN_ID>" \
  --backup-selection '{
    "SelectionName": "ProductionTagSelection",
    "IamRoleArn": "arn:aws:iam::111122223333:role/service-role/AWSBackupDefaultServiceRole",
    "ListOfTags": [{"ConditionType": "STRINGEQUALS", "ConditionKey": "Environment", "ConditionValue": "Production"}]
  }'
```
</details>

<details>
<summary><strong>Exercise 3: Configure AWS Backup Vault Lock in Governance Mode</strong></summary>

**Objective**: Test vault locking with bypass capabilities.

**Command**:
```bash
aws backup put-backup-vault-lock-configuration \
  --backup-vault-name "EnterpriseComplianceVault" \
  --min-retention-days 7 \
  --max-retention-days 90
```
</details>

<details>
<summary><strong>Exercise 4: Initiate an On-Demand Backup Job via CLI</strong></summary>

**Objective**: Capture an immediate recovery point of an active DynamoDB table.

**Command**:
```bash
aws backup start-backup-job \
  --backup-vault-name "EnterpriseComplianceVault" \
  --resource-arn "arn:aws:dynamodb:us-east-1:111122223333:table/Orders" \
  --iam-role-arn "arn:aws:iam::111122223333:role/service-role/AWSBackupDefaultServiceRole"
```
</details>

<details>
<summary><strong>Exercise 5: Configure Route 53 Health Check for Failover</strong></summary>

**Objective**: Create an HTTP health check probing endpoint availability.

**Command**:
```bash
aws route53 create-health-check \
  --caller-reference "health-check-$(date +%s)" \
  --health-check-config '{
    "IPAddress": "52.0.1.10",
    "Port": 80,
    "Type": "HTTP",
    "ResourcePath": "/healthz",
    "RequestInterval": 10,
    "FailureThreshold": 3
  }'
```
</details>

<details>
<summary><strong>Exercise 6: Test Backup Restoration to an Alternate Availability Zone</strong></summary>

**Objective**: Practice restoring an EBS snapshot into a secondary AZ.

**Steps**:
1. List recovery points: `aws backup list-recovery-points-by-backup-vault --backup-vault-name EnterpriseComplianceVault`.
2. Execute `start-restore-job` with `--metadata '{"availabilityZone": "us-east-1b"}'`.
3. Verify that the newly created EBS volume appears in `us-east-1b`.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                            AWS Disaster Recovery Decision Matrix                      |
+---------------------------------------------------------------------------------------+
|  Strategy              | RPO               | RTO               | Primary Trade-off    |
|------------------------+-------------------+-------------------+----------------------|
|  Backup & Restore      | Hours             | 24+ Hours         | Lowest cost, slow    |
|  Pilot Light           | < 15 Minutes      | 10-30 Minutes     | Ideal cost/RPO ratio |
|  Warm Standby          | Seconds           | 2-5 Minutes       | Small fleets running |
|  Active-Active         | Real-time (~0)    | Zero (~0)         | Highest cost & complex|
|  AWS Backup Vault Lock | WORM compliance   | Non-deletable     | Anti-ransomware gold |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [AWS Backup Developer Guide](https://docs.aws.amazon.com/aws-backup/latest/devguide/whatisbackup.html)
- [AWS Backup Vault Lock Documentation](https://docs.aws.amazon.com/aws-backup/latest/devguide/vault-lock.html)
- [Disaster Recovery of Workloads on AWS: Whitepaper](https://docs.aws.amazon.com/whitepapers/latest/disaster-recovery-workloads-on-aws/disaster-recovery-workloads-on-aws.html)
- [AWS Elastic Disaster Recovery (AWS DRS) Documentation](https://docs.aws.amazon.com/drs/latest/userguide/what-is-drs.html)
- [Configuring Route 53 DNS Failover](https://docs.aws.amazon.com/Route53/latest/DeveloperGuide/dns-failover.html)
