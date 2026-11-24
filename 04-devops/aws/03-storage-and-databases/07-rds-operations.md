# Amazon RDS & Aurora Day-2 Operations, Upgrades & Performance Tuning

> **Module**: `03-storage-and-databases`  
> **Topic**: Production Relational Database Operations, Blue/Green Upgrades, and Performance Insights  
> **Target Audience**: Database Administrators, Site Reliability Engineers, Platform Architects  
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

Deploying an initial database cluster is relatively simple; maintaining high performance, managing routine operating system patching, executing zero-downtime major version upgrades, and diagnosing complex lock contention under heavy production load constitutes the true challenge of database engineering.

Without standardized operational procedures:
- Manual database upgrades can lock critical tables or introduce breaking schema incompatibilities, causing hours of customer-facing downtime.
- Inadequate backup policies prevent point-in-time recovery during catastrophic data corruption.
- Undetected slow queries and un-indexed joins consume database CPU and memory, exhausting connection pools.

This operational guide covers the essential Day-2 management tools for Amazon RDS and Aurora: **Automated Backups vs. Manual Snapshots**, **Parameter & Option Groups**, **Blue/Green Deployments for Zero-Downtime Major Version Upgrades**, and deep-dive troubleshooting with **Performance Insights and Enhanced Monitoring**.

---

## 2. Core Concepts

### 2.1 Automated Backups & Point-in-Time Recovery (PITR)
- **Automated Backups**:
  - Continuous backups taken during the configured daily backup window.
  - Transaction logs (WAL / binlog) are uploaded to Amazon S3 every **5 minutes**.
  - Configurable retention period from **1 to 35 days**.
  - Deleting the database instance deletes all automated backups unless explicitly converted to manual snapshots.
- **Point-in-Time Recovery (PITR)**:
  - Restores a database to any specific second within the retention window (e.g., 5 minutes ago, or yesterday at 14:22:15 UTC).
  - Restores by applying transaction logs over the nearest daily storage snapshot, creating a **completely new database instance** with its own endpoint.
- **Manual DB Snapshots**:
  - Point-in-time user-initiated backups that persist indefinitely until explicitly deleted, even after the source database is destroyed.
  - Can be shared across AWS accounts or copied cross-region.

### 2.2 Parameter Groups vs. Option Groups
- **DB Parameter Groups**: Manage engine configuration variables (e.g., `max_connections`, `shared_buffers`, `autovacuum`, `work_mem`).
  - **Dynamic Parameters**: Applied immediately without rebooting the instance.
  - **Static Parameters**: Applied only after an explicit manual database reboot (`Pending-Reboot` status).
- **DB Option Groups**: Enable additional engine-specific enterprise features that require underlying OS or driver configuration (e.g., Oracle Transparent Data Encryption, SQL Server Native Backup, Memcached integration for MySQL).

### 2.3 RDS Blue/Green Deployments
Orchestrates fully managed, zero-downtime major version upgrades, parameter modifications, and schema migrations:
- Provisions an identical staging environment ("Green") running the target database version.
- Uses native replication to keep the Green environment continuously synchronized with the live production ("Blue") environment.
- Allows engineers to run staging validation and integration tests against Green without impacting Blue.
- **Switchover**: In under **1 minute**, RDS halts writes to Blue, verifies replication catch-up, and swaps the database DNS endpoints, promoting Green to production with zero data loss.

### 2.4 Monitoring: CloudWatch vs. Enhanced Monitoring vs. Performance Insights
- **Standard CloudWatch Metrics**: Captures hypervisor-level metrics (CPU Utilization, Freeable Memory, Read/Write IOPS, Network Throughput) at 1-minute intervals. Cannot inspect individual OS processes or SQL queries.
- **Enhanced Monitoring**: An agent running inside the database OS providing real-time metrics (CPU by process, memory consumption, context switches) at granular intervals from **1 to 60 seconds**.
- **Performance Insights**: Database performance analytics dashboard based on **Average Active Sessions (AAS)**:
  - Isolates query bottlenecks by slicing load by **Wait Events** (e.g., `CPU`, `IO:DataFileRead`, `Lock:transactionid`).
  - Identifies the exact SQL statements and client hosts consuming the majority of database resources.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        RDS Blue/Green Switchover Architecture                     |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ Client Application / API Layer ]                                               |
|             |                                                                     |
|             v (Connects via Production Endpoint: db.abc123.rds.amazonaws.com)     |
|  +-----------------------------------------------------------------------------+  |
|  | Blue Production Database (v15.4)                                            |  |
|  | - Active read/write transactional traffic                                    |  |
|  +---------------------------------------|-------------------------------------+  |
|                                          | Synchronous / Binary Replication       |
|                                          v (Continuous Log Streaming)             |
|  +-----------------------------------------------------------------------------+  |
|  | Green Staging Database (v16.1)                                               |  |
|  | - Running target engine version / upgraded schema                           |  |
|  | - Undergoing pre-production synthetic testing                               |  |
|  +-----------------------------------------------------------------------------+  |
|                                          |                                        |
|  [ SWITCHOVER PHASE: < 60 Seconds ]      |                                        |
|  1. Blocks writes on Blue instance       v                                        |
|  2. Replays remaining replication lag to 0                                        |
|  3. Swaps DNS CNAME endpoints atomicaly                                           |
|  4. Green is now LIVE Production! Blue is left isolated for rollback.            |
+-----------------------------------------------------------------------------------+
```

### Performance Insights Load Analysis
```
Average Active Sessions (AAS)
  ^
  |      [ High Database Load: AAS > Max vCPU Ceiling ]
  |      +-------------------------------------------+
  |      |   Wait Event: Lock:transactionid (Red)   |
  |      |   Wait Event: IO:DataFileRead (Blue)     |
  |      |   Wait Event: CPU (Green)                |
--+------+-------------------------------------------+-----------------> Time
  |      Max vCPUs (e.g. 8 vCPUs on db.r7g.2xlarge)
```
- If **AAS < Max vCPUs**: Database has spare capacity.
- If **AAS > Max vCPUs**: Database is bottlenecked; queries are queueing behind wait events.

---

## 4. Syntax & Usage

### 4.1 Creating and Applying a Custom Parameter Group via CLI
```bash
# 1. Create custom parameter group for PostgreSQL 16
aws rds create-db-parameter-group \
  --db-parameter-group-name "custom-pg16-params" \
  --db-parameter-group-family "postgres16" \
  --description "Tuned parameters for high-concurrency production"

# 2. Modify dynamic and static parameters
aws rds modify-db-parameter-group \
  --db-parameter-group-name "custom-pg16-params" \
  --parameters \
    "ParameterName=rds.force_ssl,ParameterValue=1,ApplyMethod=immediate" \
    "ParameterName=shared_buffers,ParameterValue='{DBInstanceClassMemory/4}',ApplyMethod=pending-reboot" \
    "ParameterName=work_mem,ParameterValue=16384,ApplyMethod=immediate" \
    "ParameterName=log_min_duration_statement,ParameterValue=500,ApplyMethod=immediate"

# 3. Associate parameter group with database instance
aws rds modify-db-instance \
  --db-instance-identifier "production-postgres-db" \
  --db-parameter-group-name "custom-pg16-params" \
  --apply-immediately
```

### 4.2 Executing Point-In-Time Recovery (PITR)
Restore a database to its exact state as of yesterday at 18:30:00 UTC:
```bash
aws rds restore-db-instance-to-point-in-time \
  --source-db-instance-identifier "production-postgres-db" \
  --target-db-instance-identifier "restored-postgres-db" \
  --restore-time "2026-11-24T18:30:00Z" \
  --db-instance-class "db.r7g.xlarge" \
  --db-subnet-group-name "production-db-subnet-group" \
  --publicly-accessible false
```

---

## 5. Practical Examples

### Example 1: Creating an RDS Blue/Green Deployment for Major Version Upgrade
Execute a major version upgrade from PostgreSQL 15 to 16 with under 1 minute cutover.

```bash
# 1. Create Blue/Green Deployment
DEPLOYMENT_ID=$(aws rds create-blue-green-deployment \
  --blue-green-deployment-name "pg15-to-pg16-upgrade" \
  --source "arn:aws:rds:us-east-1:111122223333:db:production-postgres-db" \
  --target-engine-version "16.1" \
  --query 'BlueGreenDeployment.BlueGreenDeploymentIdentifier' \
  --output text)

echo "Created Deployment: $DEPLOYMENT_ID"

# 2. Monitor status until Blue/Green deployment is 'AVAILABLE'
aws rds describe-blue-green-deployments \
  --blue-green-deployment-identifier "$DEPLOYMENT_ID" \
  --query 'BlueGreenDeployments[0].Status'

# 3. Run synthetic testing against Green database endpoint

# 4. Execute Switchover (Cutover traffic with 30-second timeout)
aws rds switchover-blue-green-deployment \
  --blue-green-deployment-identifier "$DEPLOYMENT_ID" \
  --switchover-timeout 30
```

### Example 2: Enabling Enhanced Monitoring and Performance Insights with Terraform
Declarative configuration enabling 1-second process metrics and 7-day Performance Insights retention.

```hcl
resource "aws_iam_role" "rds_enhanced_monitoring" {
  name = "rds-enhanced-monitoring-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Action    = "sts:AssumeRole"
      Effect    = "Allow"
      Principal = { Service = "monitoring.rds.amazonaws.com" }
    }]
  })
}

resource "aws_iam_role_policy_attachment" "rds_enhanced_monitoring" {
  role       = aws_iam_role.rds_enhanced_monitoring.name
  policy_arn = "arn:aws:iam::aws:policy/service-role/AmazonRDSEnhancedMonitoringRole"
}

resource "aws_db_instance" "production" {
  identifier           = "prod-core-db"
  allocated_storage    = 100
  storage_type         = "gp3"
  engine               = "postgres"
  engine_version       = "16.1"
  instance_class       = "db.r7g.xlarge"
  multi_az             = true

  # Performance Insights Configuration
  performance_insights_enabled          = true
  performance_insights_retention_period = 7 # 7 days included free
  performance_insights_kms_key_id       = var.kms_key_arn

  # Enhanced Monitoring Configuration (1-second granularity)
  monitoring_interval = 1
  monitoring_role_arn = aws_iam_role.rds_enhanced_monitoring.arn

  # Automated Backups & Maintenance
  backup_retention_period   = 30
  backup_window             = "03:00-04:00"
  maintenance_window        = "sun:04:30-sun:05:30"
  auto_minor_version_upgrade = true
  deletion_protection       = true
}
```

### Example 3: Creating and Copying an Encrypted Snapshot Cross-Region
Take an on-demand manual snapshot and copy it to another region for disaster recovery compliance.

```bash
# 1. Create manual snapshot
aws rds create-db-snapshot \
  --db-instance-identifier "prod-core-db" \
  --db-snapshot-identifier "prod-core-db-pre-release-snapshot"

# 2. Wait for snapshot to become 'available'
aws rds wait db-snapshot-available \
  --db-snapshot-identifier "prod-core-db-pre-release-snapshot"

# 3. Copy snapshot to secondary region (us-west-2) encrypted with KMS
aws rds copy-db-snapshot \
  --source-db-snapshot-identifier "arn:aws:rds:us-east-1:111122223333:snapshot:prod-core-db-pre-release-snapshot" \
  --target-db-snapshot-identifier "dr-copy-prod-core-db-snapshot" \
  --source-region "us-east-1" \
  --region "us-west-2" \
  --kms-key-id "arn:aws:kms:us-west-2:111122223333:key/abc-123"
```

### Example 4: Diagnosing Database Lock Contention via Performance Insights CLI
Retrieve the top wait events consuming database capacity over the last 15 minutes.

```bash
aws pi get-resource-metrics \
  --service-type RDS \
  --identifier "db-ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
  --start-time $(date -u -d "15 minutes ago" +%s) \
  --end-time $(date -u +%s) \
  --period 60 \
  --metric-queries '[
    {
      "Metric": "db.load.avg",
      "GroupBy": {"Group": "db.wait_event"}
    }
  ]'
```

### Example 5: Recovering from `Storage-Full` State
Dynamically increase storage capacity on an RDS instance halted due to disk exhaustion.

```bash
# When a database runs out of disk, it enters storage-full state and halts writes
# Immediately allocate additional storage (minimum 10% increase)
aws rds modify-db-instance \
  --db-instance-identifier "prod-core-db" \
  --allocated-storage 250 \
  --apply-immediately
```

---

## 6. Best Practices

1. **Always Use RDS Blue/Green Deployments for Major Upgrades**: Never perform in-place major version upgrades on live production databases. In-place upgrades take downtime and cannot be rolled back if regressions occur. Blue/Green deployments provide a safe staging environment with sub-minute cutover and zero-data-loss rollback.
2. **Enable 7-Day Performance Insights by Default**: Amazon RDS provides 7 days of Performance Insights history **100% free of charge**. Always enable it to continuously monitor Average Active Sessions (AAS) and pinpoint slow SQL queries.
3. **Set `apply_immediately = false` for Sensitive Parameter Changes in Production**: Applying modifications with `apply_immediately = true` can trigger unexpected database reboots during business hours. Use maintenance windows for non-urgent parameter and compute changes.
4. **Tune `autovacuum` and Memory Parameters for PostgreSQL**: In high-throughput PostgreSQL workloads, adjust default parameter groups: increase `work_mem` (to prevent disk spills during sorts), tune `maintenance_work_mem`, and lower `autovacuum_vacuum_scale_factor` to ensure tables are vacuumed frequently to prevent transaction ID wraparound.
5. **Enable Storage Auto-Scaling**: Configure `max_allocated_storage` on standard RDS instances. When free storage drops below 10%, RDS automatically scales storage capacity up to the configured limit, preventing catastrophic `storage-full` database deadlocks.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Using `apply-immediately` for Static Parameters** | Forces an immediate database reboot during live peak business hours. | Set `ApplyMethod=pending-reboot` and schedule a controlled maintenance window reboot. |
| **Relying Exclusively on Automated Backups** | If the RDS database instance is deleted, all automated backups are purged immediately. | Take a **final DB snapshot** prior to deletion or maintain scheduled manual snapshots via AWS Backup. |
| **Ignoring Performance Insights Wait Events** | Upgrading to larger EC2 instance sizes when the true bottleneck is an un-indexed lock contention. | Inspect wait events: if load is dominated by `Lock:transactionid`, tune application transactions, not hardware. |
| **In-Place Major Version Upgrades** | 30–60 minutes of unplanned downtime and impossible to roll back if application code fails. | Use **RDS Blue/Green Deployments** to validate on Green and cut over in <60 seconds. |
| **Setting Backup Retention to 0 Days** | Completely disables automated backups and Point-In-Time Recovery, and turns off Read Replication. | Maintain a minimum backup retention period of **7 to 35 days** on all production databases. |
| **Under-estimating PITR Restore Times** | Assuming a 5 TB database restores via PITR in 2 minutes; restoring large databases requires hours to copy storage. | Maintain active Read Replicas or Multi-AZ standbys for rapid recovery objectives. |

---

## 8. Performance, Security & Cost Notes

### Monitoring Metric Fidelity
- **CloudWatch**: Measures hypervisor-level metrics. Cannot see which queries or users are active.
- **Enhanced Monitoring**: Emits 1-second JSON logs directly to CloudWatch Logs, showing `top`-like metrics for `postgres`, `mysqld`, and OS processes.
- **Performance Insights**: Samples active database threads every second. Computes Average Active Sessions (AAS), the universal metric of database stress.

### Security Enforcements
- **Enforce SSL/TLS Connections**: Set `rds.force_ssl = 1` in the Parameter Group to reject unencrypted plaintext SQL queries.
- **Secrets Manager Automated Rotation**: Use AWS Secrets Manager to automatically rotate master passwords every 30–90 days using a managed Lambda rotation function.

### Cost Breakdown
- **Automated Backups**: Backups up to 100% of your total provisioned database storage are completely free. Backup storage exceeding 100% is billed at standard S3 rates ($0.095/GB/month).
- **Performance Insights**: 7 days of retention is **100% free**. Extended 2-year retention is billed per vCPU-month.
- **Enhanced Monitoring**: Free within CloudWatch Logs ingestion tier limits.

---

## 9. Comparison Table

| Operational Tool | Standard CloudWatch | Enhanced Monitoring | Performance Insights |
|:---|:---|:---|:---|
| **Data Source** | Hypervisor host | OS-level agent inside VM | Database engine internals (threads)|
| **Metric Frequency** | 1 minute | 1 to 60 seconds | Continuous 1-second sampling |
| **Granularity** | Total CPU, RAM, Disk, IOPS | Process-level CPU/memory table| Query-level Wait Events & AAS |
| **Identifies Slow Queries?** | No | No | Yes (Exact SQL digest & plans) |
| **Cost** | Free | Small CloudWatch log fee | 7 days free / 2 years paid |
| **Ideal Use Case** | Baseline alerting & alarms | OS starvation & process leaks | Application query optimization |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the difference between dynamic and static parameters in an RDS Parameter Group?</strong></summary>

**Answer:**
- **Dynamic Parameters**: Engine variables that can be modified on the fly without restarting the database engine (e.g., `work_mem`, `log_min_duration_statement`, `rds.force_ssl`). When modified with `ApplyMethod = immediate`, changes take effect immediately across all active connections.
- **Static Parameters**: Variables that allocate core operating system memory or modify engine startup initialization structures (e.g., `shared_buffers`, `max_connections`). Modifications are marked as `pending-reboot`. The database instance must be explicitly rebooted before the changes take effect.
</details>

<details>
<summary><strong>Q2: How does an RDS Blue/Green Deployment execute a switchover in under 1 minute?</strong></summary>

**Answer:**
1. **Staging Provisioning**: RDS provisions the Green environment running the upgraded engine version and establishes asynchronous replication from Blue to Green.
2. **Replication Catch-Up**: As changes occur on Blue, they stream continuously to Green.
3. **Switchover Trigger**: When `switchover-blue-green-deployment` is called, RDS sets Blue to read-only to stop incoming writes.
4. **Replication Drain**: It waits for Green to apply the final remaining replication log records so both databases are 100% synchronized.
5. **DNS Swap**: RDS re-points the production DNS CNAME endpoint to the Green instance and re-points the Green endpoint to the Blue instance.
6. **Promotion**: Green is promoted to read-write production. Total cutover downtime is typically **under 30 to 60 seconds**, and Blue remains available for immediate rollback if post-deployment errors occur.
</details>

<details>
<summary><strong>Q3: How does Point-in-Time Recovery (PITR) work under the hood in Amazon RDS?</strong></summary>

**Answer:**
Point-in-Time Recovery combines automated daily storage snapshots with continuous transaction log archives:
1. Amazon RDS takes daily snapshots of the database storage volume.
2. Every 5 minutes, transaction logs (WAL files in PostgreSQL, binary logs in MySQL) are uploaded from the database to an internal S3 bucket.
3. When a PITR restore is requested for a specific timestamp (e.g., 14:22:15 UTC):
   - RDS identifies and restores the latest snapshot taken *prior* to that timestamp.
   - It downloads the transaction logs covering the window between the snapshot and 14:22:15.
   - The database engine replays transactions up to the exact requested second.
   - A **completely new database instance** is launched with a fresh endpoint.
</details>

<details>
<summary><strong>Q4: What is Average Active Sessions (AAS) in Performance Insights and how is it interpreted?</strong></summary>

**Answer:**
Average Active Sessions (AAS) measures the average number of database threads that are actively working or waiting on a system resource at any given moment:
- The baseline reference is the **Max vCPU line** of the instance (e.g., 4 vCPUs on a `db.r7g.xlarge`).
- **AAS < Max vCPU**: The database is healthy; the CPU has spare processing cycles to handle additional queries.
- **AAS > Max vCPU**: The database is experiencing resource contention; active queries exceed available hardware cores and are queueing behind wait events (such as CPU contention, disk I/O, or lock acquisition).
</details>

<details>
<summary><strong>Q5: What are the primary Wait Event categories in RDS Performance Insights?</strong></summary>

**Answer:**
1. **`CPU`**: Queries are actively executing on the processor. If CPU wait dominates and AAS > Max vCPUs, queries are compute-heavy or lack indexes.
2. **`IO` (e.g., `IO:DataFileRead`, `IO:XactSync`)**: The database is waiting to read or write data blocks to disk. Common when memory (`shared_buffers`) is too small to cache working datasets, forcing disk page reads.
3. **`Lock` (e.g., `Lock:transactionid`, `Lock:tuple`)**: Queries are blocked waiting for other transactions to release row-level or table-level locks. Indicates long-running transactions or unoptimized application concurrency.
4. **`Network`**: The database is waiting for clients to read results over the network socket.
</details>

<details>
<summary><strong>Q6: What happens if an RDS instance runs completely out of free disk space (`storage-full`)?</strong></summary>

**Answer:**
When an RDS instance runs out of storage:
- The database status changes to **`storage-full`**.
- The database engine halts all write operations and transactions to prevent filesystem corruption, effectively taking the database offline for writes.
- The instance may fail to reboot or accept maintenance modifications.
- **Resolution**: An administrator must call `modify-db-instance` to increase `AllocatedStorage` by at least 10%. Amazon RDS expands the underlying EBS volume, transitions the database back to `available`, and resumes write traffic. To prevent this, **Storage Auto-Scaling** should always be enabled.
</details>

<details>
<summary><strong>Q7: What is the difference between Enhanced Monitoring and basic CloudWatch metrics?</strong></summary>

**Answer:**
- **Standard CloudWatch Metrics**: Gathered from the **hypervisor** level outside the virtual machine. Provides coarse instance-wide averages (e.g., total CPU utilization, network traffic) at 1-minute intervals. It cannot report on internal OS process states.
- **Enhanced Monitoring**: Gathers metrics via a lightweight agent running **inside the database guest operating system**. It provides 1-second to 60-second granularity, tracking per-process CPU and memory utilization, system threads, idle states, context switches, and file descriptor usage.
</details>

<details>
<summary><strong>Q8: Why should automated minor version upgrades be enabled on Amazon RDS?</strong></summary>

**Answer:**
Minor version upgrades contain critical security patches, bug fixes, and stability improvements without introducing breaking changes or backward-incompatible SQL syntax modifications.
Enabling **`auto_minor_version_upgrade = true`** ensures Amazon RDS automatically applies these security patches during the configured weekly maintenance window, protecting the database from known CVE vulnerabilities with minimal operational overhead.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Custom Parameter Group via AWS CLI</strong></summary>

**Objective**: Configure a parameter group forcing SSL connections.

**Command**:
```bash
aws rds create-db-parameter-group \
  --db-parameter-group-name "lab-pg16-secure" \
  --db-parameter-group-family "postgres16" \
  --description "Lab parameter group enforcing SSL"

aws rds modify-db-parameter-group \
  --db-parameter-group-name "lab-pg16-secure" \
  --parameters "ParameterName=rds.force_ssl,ParameterValue=1,ApplyMethod=immediate"
```
</details>

<details>
<summary><strong>Exercise 2: Create an On-Demand DB Snapshot</strong></summary>

**Objective**: Capture an immediate point-in-time manual snapshot.

**Command**:
```bash
aws rds create-db-snapshot \
  --db-instance-identifier "lab-postgres-db" \
  --db-snapshot-identifier "lab-snapshot-pre-patch-$(date +%s)"
```
</details>

<details>
<summary><strong>Exercise 3: Test Point-in-Time Recovery via CLI</strong></summary>

**Objective**: Restore a database to its state 15 minutes ago.

**Command**:
```bash
RESTORE_TIME=$(date -u -d "15 minutes ago" +%Y-%m-%dT%H:%M:%SZ 2>/dev/null || date -u -v-15M +%Y-%m-%dT%H:%M:%SZ)

aws rds restore-db-instance-to-point-in-time \
  --source-db-instance-identifier "lab-postgres-db" \
  --target-db-instance-identifier "lab-postgres-restored" \
  --restore-time "$RESTORE_TIME"
```
</details>

<details>
<summary><strong>Exercise 4: Enable Enhanced Monitoring on an Active Database</strong></summary>

**Objective**: Configure 1-second OS metrics reporting.

**Command**:
```bash
aws rds modify-db-instance \
  --db-instance-identifier "lab-postgres-db" \
  --monitoring-interval 1 \
  --monitoring-role-arn "arn:aws:iam::111122223333:role/rds-enhanced-monitoring-role" \
  --apply-immediately
```
</details>

<details>
<summary><strong>Exercise 5: Initiate an RDS Blue/Green Deployment</strong></summary>

**Objective**: Stage a major version upgrade.

**Command**:
```bash
aws rds create-blue-green-deployment \
  --blue-green-deployment-name "lab-bg-upgrade" \
  --source "arn:aws:rds:us-east-1:111122223333:db:lab-postgres-db" \
  --target-engine-version "16.1"
```
</details>

<details>
<summary><strong>Exercise 6: Inspect Average Active Sessions in Performance Insights</strong></summary>

**Objective**: Retrieve top SQL statements by execution load.

**Command**:
```bash
aws pi get-resource-metrics \
  --service-type RDS \
  --identifier "db-ABCDEF123456" \
  --start-time $(date -u -d "30 minutes ago" +%s) \
  --end-time $(date -u +%s) \
  --period 60 \
  --metric-queries '[{"Metric": "db.load.avg", "GroupBy": {"Group": "db.sql"}}]'
```
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                               RDS Operations Quick Reference                          |
+---------------------------------------------------------------------------------------+
|  Operational Task      | Recommended AWS Mechanism         | Key Characteristic       |
|------------------------+-----------------------------------+--------------------------|
|  Disaster Recovery     | Automated Backups + PITR (35 days)| Microsecond second-level |
|  Permanent Backup      | Manual DB Snapshot (S3 backed)    | Persists past deletion   |
|  Major Engine Upgrade  | RDS Blue/Green Deployments        | Cutover under 60 seconds |
|  Query Bottlenecks     | Performance Insights (AAS)        | Free 7-day wait event map|
|  OS Process Starvation | Enhanced Monitoring (1-60 sec)    | Agent inside VM          |
|  Disk Full Recovery    | Storage Auto-Scaling (10% grow)   | Prevents write lockouts  |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Amazon RDS Backup and Restore User Guide](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/CHAP_CommonTasks.BackupRestore.html)
- [Using Amazon RDS Blue/Green Deployments for Database Updates](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/blue-green-deployments.html)
- [Monitoring DB Load with Performance Insights](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/USER_PerfInsights.html)
- [Amazon RDS Enhanced Monitoring User Guide](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/USER_Monitoring.OS.html)
- [Best Practices for Amazon RDS Parameter Group Tuning](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/USER_WorkingWithParamGroups.html)
