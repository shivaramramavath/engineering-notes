---
topic: "Cloud Migration Strategies and the 7 Rs"
module: "07-architecture-patterns"
track: "aws"
last-tested: "2026-11"
prerequisites:
  - "02-compute/01-ec2.md"
  - "03-storage-and-databases/03-rds-and-aurora.md"
  - "04-networking/01-vpc.md"
  - "06-operations/10-cost-optimisation-and-finops.md"
complexity: "advanced"
tags:
  - "migration"
  - "the-7-rs"
  - "mgn"
  - "dms"
  - "sct"
  - "architecture"
---

# Cloud Migration Strategies and the 7 Rs

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Notes](#8-performance-security--cost-notes)
- [9. Comparison Table](#9-comparison-table)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

Enterprise cloud migration is the strategic process of transitioning on-premises data centers, legacy virtual machines, physical servers, and databases into AWS. Successfully migrating complex enterprise estates requires a structured framework that classifies workloads according to business value, technical debt, modernization potential, and migration velocity.

AWS codifies this methodology into **The 7 Rs of Cloud Migration**: Rehost, Replatform, Repurchase, Refactor/Re-architect, Retain, Retire, and Relocate. Utilizing automated migration tooling such as **AWS Application Migration Service (MGN)** and **AWS Database Migration Service (DMS)**, organizations can minimize business disruption, eliminate multi-hour downtime cutovers, and transition smoothly from capital-intensive physical data centers to agile cloud infrastructure.

### Why a Structured Migration Strategy Matters
- **Avoid "Lift-and-Dump" Cost Pitfalls**: Moving un-optimized on-premises architectures directly to EC2 without right-sizing or modernizing licenses can cause cloud operating costs to exceed original on-premises budgets.
- **Minimize Cutover Downtime**: By using continuous, asynchronous block-level replication (AWS MGN) and Change Data Capture (AWS DMS), production cutover windows can be compressed from days down to minutes.
- **Strategic Portfolio Rationalization**: Discovering and inventorying an estate reveals that 10% to 20% of legacy applications are redundant and can be retired immediately, freeing up capital and developer bandwidth.

---

## 2. Core Concepts

### The 7 Rs Framework

```
+----------------------------------------------------------------------------------------------------+
|                                THE 7 Rs OF CLOUD MIGRATION                                         |
+----------------------------------------------------------------------------------------------------+

  Least Effort / Fastest Migration ----------------------------------> Highest Effort / Highest Value
  No Code Changes ---------------------------------------------------> Full Cloud-Native Redesign

   +-------------+    +---------------+    +---------------+    +----------------+    +-------------+
   | 1. Rehost   |    | 2. Replatform |    | 3. Repurchase |    | 4. Refactor    |    | 5. Relocate |
   | (Lift/Shift)|    | (Lift, Tinker)|    | (Drop/Shop)   |    | (Re-architect) |    | (Hypervisor)|
   +-------------+    +---------------+    +---------------+    +----------------+    +-------------+
   | AWS MGN     |    | Move DB to    |    | Migrate to    |    | Redesign to    |    | VMware Cloud|
   | Block-level |    | RDS/Aurora;   |    | SaaS (e.g.    |    | Serverless,    |    | on AWS (VMC)|
   | EC2 copy    |    | OS to ECS     |    | Salesforce)   |    | Microservices  |    | No VM change|
   +-------------+    +---------------+    +---------------+    +----------------+    +-------------+

               +-----------------------+              +-----------------------+
               | 6. Retain (Revisit)   |              | 7. Retire (Decom)     |
               +-----------------------+              +-----------------------+
               | Keep on-prem due to   |              | Turn off completely;  |
               | compliance/dependency |              | redundant workloads   |
               +-----------------------+              +-----------------------+
```

1. **Rehost ("Lift and Shift")**:
   - Move applications from on-premises to EC2 instances with zero code or architectural modifications.
   - Powered by **AWS Application Migration Service (MGN)** via continuous block-level storage replication.
   - Fastest migration path; ideal for data center lease expirations.

2. **Replatform ("Lift, Tinker, and Shift")**:
   - Make targeted optimizations to reduce operational overhead without altering core application code.
   - Examples: Moving self-hosted Oracle/SQL Server to Amazon RDS; moving containerized Linux apps to Amazon ECS or Elastic Beanstalk.

3. **Repurchase ("Drop and Shop")**:
   - Abandon custom legacy systems in favor of purpose-built SaaS solutions (e.g., replacing custom CRM with Salesforce; migrating self-hosted exchange to Microsoft 365 or WorkMail).

4. **Refactor / Re-architect**:
   - Deconstruct monolithic applications into cloud-native microservices, serverless components (Lambda, DynamoDB), or event-driven topologies.
   - Highest engineering effort and initial investment, but unlocks maximum long-term elasticity, agility, and cost-efficiency.

5. **Relocate**:
   - Move virtual machines at the hypervisor level to **VMware Cloud on AWS** without modifying operating systems or VM configs.

6. **Retain**:
   - Keep workloads on-premises due to severe legacy dependencies, high-bandwidth local industrial hardware interfaces, or regulatory residency constraints. Revisit in future phases.

7. **Retire**:
   - Identify obsolete, non-value-adding applications during the discovery phase and shut them down entirely.

---

## 3. How It Works Internally

```
+----------------------------------------------------------------------------------------------------+
|                         CONTINUOUS BLOCK & DATA MIGRATION ARCHITECTURE                             |
+----------------------------------------------------------------------------------------------------+

  [ On-Premises Data Center ]                                  [ Target AWS Production Environment ]
                                                                             (VPC)
  +--------------------------+                                 +----------------------------------+
  | Application VM / Server  |                                 | Staging Subnet (Replication Area)|
  |                          |                                 |                                  |
  |  [ AWS MGN Agent ]       | -- TLS 1.3 Block Replication -> |  [ Low-Cost MGN Replication EC2] |
  |   (Continuous storage    |    (Over Direct Connect / VPN)  |         | (Writes to EBS)        |
  |    block capture)        |                                 |         v                        |
  +--------------------------+                                 |  [ Staging EBS Volume Mirrors ]  |
                                                               +----------------------------------+
                                                                                 |
                                                                                 | Automated Cutover Trigger
                                                                                 v
                                                               +----------------------------------+
  +--------------------------+                                 | Target Production Subnet         |
  | On-Premises DB Server    |                                 |                                  |
  | (Oracle / MS SQL Server) |                                 |  [ Converted Production EC2 ]    |
  |                          |                                 |    (Target Server launched)      |
  |  [ Source Data Logs ]    |                                 +----------------------------------+
  +--------------------------+                                                   ^
               |                                                                 |
               | Continuous CDC Changes                                          |
               v                                                                 |
  +------------------------------------------------------------+                 |
  | AWS Database Migration Service (DMS Replication Instance)  | ----------------+
  |  - Full Load: Extracts tables                              | (Continuous CDC to Aurora RDS)
  |  - CDC: Replicates ongoing WAL changes                     |
  +------------------------------------------------------------+
```

### The 3 Migration Phases
1. **Discovery & Assessment**: Use **AWS Application Discovery Service** and **AWS Migration Hub** to inventory server CPU/RAM usage, network dependencies, and software catalogs. Map out dependencies to group applications into migration waves.
2. **Mobilization & Pilot**: Establish the AWS Landing Zone foundation (Multi-Account, Transit Gateway, Direct Connect). Run a pilot migration of non-critical workloads to validate operational runbooks and team readiness.
3. **Migration & Modernization**: Deploy automated replication agents (AWS MGN, AWS DMS). Perform continuous replication until data is fully synced. Execute cutover during a scheduled maintenance window.

---

## 4. Syntax & Usage

### 4.1 Terraform: AWS Database Migration Service (DMS) Replication Setup

```hcl
terraform {
  required_version = ">= 1.5.0"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.40"
    }
  }
}

provider "aws" {
  region = "us-east-1"
}

# DMS Subnet Group
resource "aws_dms_replication_subnet_group" "dms_subnets" {
  replication_subnet_group_id          = "dms-replication-subnet-group"
  replication_subnet_group_description = "Subnet group for DMS replication instances"
  subnet_ids                           = ["subnet-0123456789abcdef1", "subnet-0123456789abcdef2"]
}

# DMS Replication Instance
resource "aws_dms_replication_instance" "dms_instance" {
  replication_instance_id    = "production-migration-dms-instance"
  replication_instance_class = "dms.c5.xlarge"
  allocated_storage          = 100
  multi_az                   = true
  publicly_accessible       = false
  replication_subnet_group_id = aws_dms_replication_subnet_group.dms_subnets.id
  vpc_security_group_ids     = ["sg-dms-security-group"]

  tags = {
    Environment = "Migration"
  }
}

# Source Endpoint (On-Premises PostgreSQL)
resource "aws_dms_endpoint" "source_onprem_pg" {
  endpoint_id   = "onprem-source-postgres"
  endpoint_type = "source"
  engine_name   = "postgres"
  server_name   = "db.onprem.enterprise.internal"
  port          = 5432
  database_name = "production_db"
  username      = "migration_user"
  password      = "SecureMigrationPassword123!"
  ssl_mode      = "require"
}

# Target Endpoint (AWS Amazon Aurora PostgreSQL)
resource "aws_dms_endpoint" "target_aurora_pg" {
  endpoint_id   = "target-aurora-postgres"
  endpoint_type = "target"
  engine_name   = "aurora-postgresql"
  server_name   = "aurora-cluster.cluster-xyz.us-east-1.rds.amazonaws.com"
  port          = 5432
  database_name = "production_db"
  username      = "dbadmin"
  password      = "ProductionAuroraPassword123!"
  ssl_mode      = "require"
}

# Migration Task: Full Load + Continuous CDC (Change Data Capture)
resource "aws_dms_replication_task" "migrate_orders_task" {
  replication_task_id      = "migrate-orders-full-load-and-cdc"
  migration_type           = "full-load-and-cdc"
  replication_instance_arn = aws_dms_replication_instance.dms_instance.replication_instance_arn
  source_endpoint_arn      = aws_dms_endpoint.source_onprem_pg.endpoint_arn
  target_endpoint_arn      = aws_dms_endpoint.target_aurora_pg.endpoint_arn

  table_mappings = jsonencode({
    rules = [
      {
        "rule-type" = "selection"
        "rule-id"   = "1"
        "rule-name" = "include-all-public-tables"
        "object-locator" = {
          "schema-name" = "public"
          "table-name"  = "%"
        }
        "rule-action" = "include"
      }
    ]
  })

  replication_task_settings = jsonencode({
    TargetMetadata = {
      TargetSchema = ""
      SupportLobs  = true
      FullLobMode  = false
      LobChunkSize = 64
    }
    Logging = {
      EnableLogging = true
    }
  })
}
```

---

## 5. Practical Examples

### Example 1: Installing the AWS Application Migration Service (MGN) Replication Agent

```bash
# 1. Download the AWS MGN Replication Agent installer on the source Linux server
wget -O ./aws-replication-installer-init.py https://aws-application-migration-service-us-east-1.s3.amazonaws.com/latest/linux/aws-replication-installer-init.py

# 2. Run the installer using temporary AWS IAM migration credentials
sudo python3 aws-replication-installer-init.py \
    --region us-east-1 \
    --aws-access-key-id AKIAIOSFODNN7EXAMPLE \
    --aws-secret-access-key wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY

# 3. Verify replication status on the server
# The agent automatically discovers block devices and initiates continuous synchronization to staging EBS
```

### Example 2: Launching a Cutover Instance via AWS MGN CLI

```bash
# 1. Check replication status of source server
aws mgn describe-source-servers \
    --source-server-ids "s-1234567890abcdef0" \
    --query "items[0].dataReplicationInfo.dataReplicationState"

# 2. Trigger test launch (verifies target EC2 boots and application functions)
aws mgn launch-test-instances \
    --source-server-ids "s-1234567890abcdef0"

# 3. Final Cutover Launch (stops on-prem writes, syncs final delta, launches production EC2)
aws mgn launch-cutover-instances \
    --source-server-ids "s-1234567890abcdef0"

# 4. Finalize cutover (cleans up replication staging resources in AWS)
aws mgn finalize-cutover \
    --source-server-ids "s-1234567890abcdef0"
```

### Example 3: AWS Schema Conversion Tool (SCT) Database Assessment

```bash
# Automate heterogeneous database schema evaluation via AWS SCT CLI
# Evaluates Oracle DB compatibility against Amazon Aurora PostgreSQL
aws-sct --generate-assessment-report \
    --source-type ORACLE \
    --source-connection-string "jdbc:oracle:thin:@db.onprem:1521/ORCL" \
    --target-type AURORA_POSTGRES \
    --output-directory "/reports/migration_assessment/"

# Inspect the resulting PDF/CSV report:
# - Identifies 88% automatic schema conversion
# - Flags complex PL/SQL stored procedures requiring manual refactoring
```

### Example 4: Automating DNS Cutover with Route 53 Weighted Records

```bash
# 1. Route 90% of traffic to on-prem and 10% to newly migrated AWS EC2 target
aws route53 change-resource-record-sets \
    --hosted-zone-id Z1234567890ABC \
    --change-batch '{
      "Changes": [
        {
          "Action": "UPSERT",
          "ResourceRecordSet": {
            "Name": "app.enterprise.com",
            "Type": "A",
            "SetIdentifier": "OnPremiseTarget",
            "Weight": 90,
            "TTL": 60,
            "ResourceRecords": [{ "Value": "192.168.10.50" }]
          }
        },
        {
          "Action": "UPSERT",
          "ResourceRecordSet": {
            "Name": "app.enterprise.com",
            "Type": "A",
            "SetIdentifier": "AWSTarget",
            "Weight": 10,
            "TTL": 60,
            "ResourceRecords": [{ "Value": "10.0.1.100" }]
          }
        }
      ]
    }'
```

### Example 5: Verifying DMS Replication Lag via Amazon CloudWatch Metrics

```bash
# Query the CDC Replication Lag in seconds for the DMS task
aws cloudwatch get-metric-statistics \
    --namespace "AWS/DMS" \
    --metric-name "CDCLatencyTarget" \
    --dimensions Name=ReplicationInstanceIdentifier,Value=production-migration-dms-instance \
                 Name=ReplicationTaskIdentifier,Value=migrate-orders-full-load-and-cdc \
    --start-time $(date -u -d '1 hour ago' +%Y-%m-%dT%H:%M:%SZ) \
    --end-time $(date -u +%Y-%m-%dT%H:%M:%SZ) \
    --period 300 \
    --statistics Maximum Average
```

---

## 6. Best Practices

1. **Perform Right-Sizing Before and After Migration**: On-premises servers are historically over-provisioned by 50% to 70%. Use AWS Application Discovery Service metrics to allocate right-sized EC2 instances and Savings Plans.
2. **Execute Full End-to-End Test Cuts**: Always launch test cutover instances in an isolated VPC staging subnet to verify operating system boot, license validation, and database connectivity prior to actual cutover night.
3. **Keep DNS TTLs Low (60 seconds) Weeks Prior to Migration**: Reduce DNS TTLs across public and internal hosted zones well in advance of cutover to allow immediate traffic shifting and rapid rollback if needed.
4. **Use Continuous Replication Tools (MGN & DMS)**: Never attempt offline file-based copies (e.g., rsync or manual pg_dump) for production workloads; continuous CDC minimizes downtime to minutes.
5. **Establish a Deterministic Rollback Plan**: Define the exact failure criteria (e.g., error rate > 2% for 10 minutes) and specific rollback steps (shifting DNS back to on-premise IP) before initiating cutover.
6. **Migrate in Discrete Application Waves**: Group applications by dependency clusters; migrate non-critical internal apps in early waves, followed by core transactional engines.

---

## 7. Common Mistakes

| Mistake | Why It Hurts | Correct Approach |
|:---|:---|:---|
| **Lifting and shifting without right-sizing** | Migrating an idle 64-core on-premises server to an expensive `m5.16xlarge` doubles hosting costs unnecessarily. | Right-size based on 95th percentile CPU/RAM utilization metrics from discovery tools. |
| **Neglecting database collation and data type differences in DMS** | Heterogeneous migrations (e.g., Oracle to PostgreSQL) can corrupt timestamps, CLOBs, or case-sensitive strings. | Run **AWS Schema Conversion Tool (SCT)** assessment to identify data type mismatches before data sync. |
| **Executing cutover with high DNS TTLs (e.g., 86400s)** | Client DNS caches continue resolving to on-premises servers for 24 hours, splitting writes across environments. | Lower DNS TTLs to 60 seconds at least 7 days before cutover. |
| **Attempting to Refactor everything in Phase 1** | Rewriting entire architectures while simultaneously migrating causes multi-year project stalls. | Rehost or Replatform first to exit the data center, then modernize iteratively in the cloud. |
| **Ignoring egress network bandwidth constraints** | Replicating 50 terabytes of data over a saturated 100 Mbps internet pipe takes weeks and disrupts office traffic. | Provision dedicated **AWS Direct Connect** or use **AWS Snowball Edge** for initial data seeding. |
| **Omitting cutover rollback criteria** | When a deployment issue arises at 3:00 AM, teams panic and debate rollback decisions ad-hoc while services stay down. | Agree upon unambiguous rollback thresholds and runbook steps in advance. |

---

## 8. Performance, Security & Cost Notes

### Performance
- **AWS MGN Replication Impact**: Operates at the OS kernel block driver level, consuming less than 3% CPU and minimal I/O on source production servers.
- **DMS CDC Latency**: Under normal network conditions, Change Data Capture replication latency is sub-second (< 1 second).

### Security
- **In-Transit Encryption**: All block data replicated by AWS MGN and AWS DMS is encrypted in transit using TLS 1.3.
- **Dedicated Private Transit**: Route replication traffic through AWS Direct Connect or Site-to-Site VPN to prevent exposing database ports over the public internet.

### Cost Analysis
- **AWS MGN**: Free for the first 90 days per server migrated. Standard EC2 and EBS charges apply only for staging replication instances and target servers.
- **AWS DMS**: Compute and storage pricing applies to the DMS replication instance (e.g., `dms.c5.xlarge` $\approx$ \$0.27/hour). Free if migrating to Amazon Aurora, DynamoDB, or Redshift for 6 months.

---

## 9. Comparison Table

| Migration Strategy (R) | Engineering Effort | Migration Speed | Business Disruption | Long-Term Cost Savings | Best For |
|:---|:---|:---|:---|:---|:---|
| **Rehost (Lift & Shift)** | Very Low | Fast (Days/Weeks) | Minimal | Low (Initial) | Urgent data center exits, legacy COTS apps |
| **Replatform (Lift & Tinker)** | Low - Medium | Moderate (Weeks) | Low | Medium | Replacing self-hosted DBs with RDS; containerizing |
| **Repurchase (SaaS)** | Low - Medium | Variable | Medium (User change) | High (Zero infra ops) | Standard commodity tools (CRM, HR, Mail) |
| **Refactor (Cloud-Native)** | High | Slow (Months/Years)| High (Re-architecting)| Maximum | Core competitive differentiators, scaling bottlenecks |
| **Relocate (VMware on AWS)** | Minimal | Fastest | Near Zero | Low | Existing VMware vSphere enterprises |

---

## 10. Interview Questions

<details>
<summary><strong>1. Explain the difference between homogeneous and heterogeneous database migration with AWS DMS.</strong></summary>

- **Homogeneous Migration**: The source and target databases share the same engine (e.g., on-premises PostgreSQL to Amazon RDS PostgreSQL, or on-premises Oracle to RDS Oracle). Schema, stored procedures, and data types are 100% compatible. AWS DMS can handle both schema and data replication directly without translation.
- **Heterogeneous Migration**: The source and target database engines differ (e.g., on-premises Oracle to Amazon Aurora PostgreSQL). Because SQL syntax, PL/SQL stored procedures, and data types differ fundamentally, **AWS Schema Conversion Tool (SCT)** must first analyze and convert the database schema and code. Then, AWS DMS is used to perform the data extraction and continuous Change Data Capture (CDC).
</details>

<details>
<summary><strong>2. How does AWS Application Migration Service (MGN) achieve near-zero downtime cutovers?</strong></summary>

AWS MGN installs a lightweight agent on the source machine that captures storage changes at the block level beneath the operating system file system. It continuously compresses, encrypts, and streams modified disk blocks to low-cost staging EBS volumes in AWS. During cutover:
1. The application on-premise is temporarily stopped to cease new writes.
2. MGN replicates the final pending delta blocks (usually taking 1-2 minutes).
3. MGN automatically launches a production EC2 instance configured from the staging EBS volumes.
4. DNS is shifted to the new EC2 instance. Total downtime is restricted to the final delta sync and DNS propagation window.
</details>

<details>
<summary><strong>3. Why should enterprises avoid the "Refactor Everything" approach when migrating large legacy estates?</strong></summary>

Attempting to completely rewrite and decompose decades of legacy code into microservices and serverless architectures prior to cloud migration dramatically increases project risk. It leads to scope creep, multi-year delays, shifting requirements, and budget exhaustion while the organization continues paying expensive on-premises data center renewal fees. The proven enterprise approach is **Rehost or Replatform first** to gain cloud agility, right-size infrastructure, and exit the physical facility, followed by iterative **post-migration refactoring** of high-value services directly in the cloud.
</details>

<details>
<summary><strong>4. How does Change Data Capture (CDC) work in AWS Database Migration Service (DMS)?</strong></summary>

DMS CDC replicates ongoing database transactions in near-real-time without querying table data or locking tables. It reads directly from the source database engine's transaction logs or write-ahead logs (e.g., Oracle Redo Logs, PostgreSQL WAL, MySQL Binary Logs, SQL Server Transaction Logs). DMS decodes transaction log records, transforms data types if migrating heterogeneously, and applies the equivalent SQL operations sequentially to the target database with minimal source CPU overhead.
</details>

<details>
<summary><strong>5. What is the role of AWS Snowball Edge in a large-scale data migration?</strong></summary>

When an enterprise needs to migrate hundreds of terabytes or petabytes of data to AWS, copying across public internet or even standard Direct Connect links can take months or saturate available bandwidth. **AWS Snowball Edge** is a ruggedized physical data transfer device with up to 100 TB of usable storage. The enterprise connects the appliance locally to their internal network over 10/25/40 GbE links, copies data at multi-gigabit speeds, and ships the device back to AWS, where data is ingested directly into Amazon S3. Ongoing changes can then be caught up using AWS DMS or MGN over the network.
</details>

<details>
<summary><strong>6. What criteria determine whether a workload should be "Retired" or "Retained"?</strong></summary>

- **Retire Criteria**: Workloads that have no active users, redundant internal dashboards, legacy systems superseded by newer SaaS platforms, or services whose annual business value is less than the cost of hosting and securing them. Discovery data typically reveals 10-20% of servers can be safely turned off.
- **Retain Criteria**: Applications that run on non-x86 legacy hardware (e.g., mainframes not yet ready for modernization), systems tied to local physical hardware (e.g., factory floor robotics with strict sub-millisecond physical latency requirements), or workloads bound by legal/sovereignty constraints requiring on-premise residency.
</details>

<details>
<summary><strong>7. How do you design an effective database cutover runbook with zero data loss?</strong></summary>

1. Ensure AWS DMS CDC replication lag is sub-second (`CDCLatencyTarget < 1s`).
2. Put on-premises web applications into maintenance mode / read-only mode to prevent new transactions.
3. Wait for DMS replication lag to drop to exactly zero seconds (confirming all in-flight WAL transactions are committed to target RDS).
4. Stop the DMS replication task.
5. Point AWS web servers to the target RDS database.
6. Verify data integrity via automated smoke tests and row-count reconciliation scripts.
7. Update DNS or load balancer routing to direct client traffic to the AWS application.
8. Disable maintenance mode.
</details>

<details>
<summary><strong>8. What is the AWS Application Discovery Service and how does it assist portfolio analysis?</strong></summary>

AWS Application Discovery Service collects technical data from on-premises environments. It operates in two modes:
- **Agentless Discovery (VMware vCenter)**: Collects VM configuration, host specs, and resource utilization (CPU, memory, disk).
- **Agent-based Discovery (OS-level)**: Installs an agent on physical or virtual servers to inspect inbound and outbound TCP network connections, running processes, and exact performance profiles.
This data is ingested into **AWS Migration Hub** to map inter-service network dependencies, identify application clusters, and group systems into logical migration waves.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Challenge 1: Configure an AWS DMS Replication Task via Terraform</strong></summary>

**Goal**: Provision an AWS DMS replication instance, source endpoint, target endpoint, and replication task for PostgreSQL using Terraform.
**Verification**: Check in the AWS console that endpoints successfully test connection and the replication task enters the `Ready` state.
</details>

<details>
<summary><strong>Challenge 2: Run an AWS SCT Schema Assessment Report</strong></summary>

**Goal**: Install the AWS Schema Conversion Tool (SCT), connect to a local MySQL/PostgreSQL database, and generate an assessment report for target Aurora.
**Verification**: Open the generated executive summary report and review the action items and estimated conversion complexity percentage.
</details>

<details>
<summary><strong>Challenge 3: Simulate Traffic Shifting via Route 53 Weighted Records</strong></summary>

**Goal**: Create two weighted Route 53 A records pointing to two different mock IPs (90% to Old Environment, 10% to New AWS Environment).
**Verification**: Execute 50 `dig` or `nslookup` queries and calculate the empirical distribution of returned IP addresses.
</details>

<details>
<summary><strong>Challenge 4: Deploy and Test an AWS MGN Replication Environment</strong></summary>

**Goal**: Initialize AWS Application Migration Service in a sandbox AWS region, configure the replication template with subnet and EBS volume types.
**Verification**: Confirm via `aws mgn get-replication-configuration-template` that the default template is active.
</details>

<details>
<summary><strong>Challenge 5: Monitor DMS CDC Latency Alarms</strong></summary>

**Goal**: Create a CloudWatch Alarm on the `CDCLatencyTarget` metric triggering when latency exceeds 60 seconds for 2 consecutive evaluation periods.
**Verification**: Inspect the alarm state in CloudWatch Alarms and verify the SNS topic notification configuration.
</details>

<details>
<summary><strong>Challenge 6: Build a Rollback Runbook for DNS-Based Cutover</strong></summary>

**Goal**: Write a markdown runbook documenting explicit abort criteria, CLI rollback commands, and communication templates for a database migration cutover.
**Verification**: Peer review the runbook against real-world cutover scenarios to ensure all rollback steps are executable in under 5 minutes.
</details>

---

## 12. Summary / Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
|                                 CLOUD MIGRATION STRATEGY CHEAT SHEET                               |
+----------------------------------------------------------------------------------------------------+
| Strategy (7 Rs)  | AWS Automation Tool           | Primary Objective                               |
+------------------+-------------------------------+-------------------------------------------------+
| Rehost           | AWS Application Migration(MGN)| Rapid block-level lift & shift to EC2           |
| Replatform       | AWS DMS / Elastic Beanstalk   | Reduce DB/OS ops by moving to RDS/ECS           |
| Repurchase       | AWS Marketplace / SaaS        | Replace legacy custom software with SaaS        |
| Refactor         | Lambda / DynamoDB / Containers| Complete cloud-native architectural redesign    |
| Relocate         | VMware Cloud on AWS (VMC)     | Hypervisor-level VM relocation without re-image |
| Retain           | AWS Outposts / On-Premise     | Keep workloads on-prem due to strict constraints|
| Retire           | Manual / Portfolio discovery  | Decommission unused or non-value-adding servers |
+------------------+-------------------------------+-------------------------------------------------+
| Tooling          | AWS Migration Hub, Application Discovery Service, AWS Schema Conversion Tool    |
| Cutover Rule     | Lower DNS TTL to 60s; verify CDC latency is zero before taking DB out of sync    |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [AWS Prescriptive Guidance: Migration Strategy for Expanding to the Cloud](https://docs.aws.amazon.com/prescriptive-guidance/latest/migration-strategy/welcome.html)
- [AWS Application Migration Service (MGN) User Guide](https://docs.aws.amazon.com/mgn/latest/ug/what-is-application-migration-service.html)
- [AWS Database Migration Service (DMS) User Guide](https://docs.aws.amazon.com/dms/latest/userguide/Welcome.html)
- [AWS Schema Conversion Tool (SCT) Documentation](https://docs.aws.amazon.com/SchemaConversionTool/latest/userguide/Welcome.html)
- [Cloud Migration Checklist and Framework - Martin Fowler](https://martinfowler.com/articles/cloud-migration.html)
