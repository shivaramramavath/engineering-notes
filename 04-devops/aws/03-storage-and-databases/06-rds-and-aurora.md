# Amazon RDS & Amazon Aurora Relational Database Architecture

> **Module**: `03-storage-and-databases`  
> **Topic**: Managed Relational Databases, Multi-AZ High Availability, and Cloud-Native Aurora  
> **Target Audience**: Database Administrators, Backend Architects, DevOps Engineers  
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

Running self-managed relational database management systems (RDBMS) on bare virtual machines introduces severe operational friction: engineers must manually configure operating system kernels, provision network storage, manage master-standby replication, script automated point-in-time recovery (PITR), execute minor/major version upgrades, and handle complex failover routing when hardware crashes.

AWS provides two distinct tiers of managed relational database services:
1. **Amazon Relational Database Service (Amazon RDS)**: Automates provisioning, patching, backup, recovery, and high availability for standard commercial and open-source database engines (PostgreSQL, MySQL, MariaDB, Oracle, and Microsoft SQL Server).
2. **Amazon Aurora**: A purpose-built, cloud-native relational database engine fully compatible with MySQL and PostgreSQL. Aurora reimagines the database engine for the cloud by completely decoupling the compute layer from the storage tier, delivering up to **5x the throughput of standard MySQL** and **3x the throughput of standard PostgreSQL** at a fraction of commercial database licensing costs.

---

## 2. Core Concepts

### 2.1 Amazon RDS High Availability: Multi-AZ vs. Read Replicas
- **Multi-AZ Deployment (High Availability / Disaster Recovery)**:
  - Provisions a synchronous, standby replica instance in a different Availability Zone.
  - In the event of primary host failure, OS maintenance, or AZ disruption, Amazon RDS executes an **automated failover in 60–120 seconds** by updating the database DNS CNAME endpoint to point to the standby instance.
  - Standby replicas **cannot serve read traffic**; they are strictly hot standbys.
- **Read Replicas (Horizontal Read Scaling)**:
  - Uses asynchronous replication to scale read-intensive workloads.
  - Can provision up to **15 read replicas** per database instance.
  - Can be deployed across Availability Zones or across AWS Regions (Cross-Region Read Replicas).
  - Can be manually promoted to an independent read-write standalone database.

### 2.2 Amazon Aurora Distributed Architecture
Unlike standard RDS where compute instances attach to dedicated Amazon EBS volumes, Amazon Aurora features a **custom distributed storage engine**:
- **Six-Way Replication Across 3 AZs**: Aurora writes **6 copies of data across 3 Availability Zones** (2 copies per AZ).
- **Quorum-Based Storage System**:
  - **Write Quorum (4 of 6)**: Writes are committed when 4 out of 6 storage nodes acknowledge receipt, surviving the loss of an entire AZ plus an additional storage node without losing write availability.
  - **Read Quorum (3 of 6)**: Ensures read consistency and enables automatic background self-healing.
- **Shared Storage Fleet**: All compute instances (Primary Writer and up to 15 Aurora Read Replicas) connect to the **exact same underlying distributed storage layer**. Replica lag is typically **under 10 milliseconds**.
- **Auto-Scaling Storage**: Storage automatically expands in 10 GB increments up to **128 TiB** without provisioning disks or performance impact.

### 2.3 Aurora Serverless v2
Instant auto-scaling relational compute:
- Scales database compute capacity up and down in fractions of a second based on real-time transaction throughput.
- Measured in **Aurora Capacity Units (ACUs)**: 1 ACU provides approximately 2 GB of memory, corresponding vCPU, and networking bandwidth.
- Scales from as low as **0.5 ACU up to 128 ACUs** (256 GB RAM) with zero connection drops.

### 2.4 Aurora Global Database
Global multi-region database deployment:
- Replicates storage blocks directly across AWS regions with typical replication latency of **under 1 second**.
- Supports up to 5 secondary regions with up to 16 read replicas per region.
- Delivers rapid cross-region disaster recovery with an RPO of under 1 second and an RTO of under 1 minute.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Amazon Aurora Distributed Architecture                     |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  [ Client Application / API Layer ]                                               |
|         |                                                        |                |
|         | Writes (Cluster Endpoint: db.cluster-...)              | Reads (Reader) |
|         v                                                        v                |
|  +---------------------------+                +--------------------------------+  |
|  | Aurora Primary Writer     |                | Aurora Read Replica (up to 15) |  |
|  | (Compute Node: AZ 1a)     |                | (Compute Node: AZ 1b / 1c)     |  |
|  +---------------------------+                +--------------------------------+  |
|         | (Streams Redo Logs Only via PCIe/Nitro)                |                |
|         +-----------------------+--------------------------------+                |
|                                 | (Shared Storage Layer Access)                   |
|                                 v                                                 |
|  +-----------------------------------------------------------------------------+  |
|  |             Amazon Aurora Distributed Storage Fleet (Multi-AZ)              |  |
|  |                                                                             |  |
|  |  +--------------------+   +---------------------+   +--------------------+  |  |
|  |  | AZ us-east-1a      |   | AZ us-east-1b       |   | AZ us-east-1c      |  |  |
|  |  | [Copy 1]  [Copy 2] |   | [Copy 3]   [Copy 4] |   | [Copy 5]  [Copy 6] |  |  |
|  |  +--------------------+   +---------------------+   +--------------------+  |  |
|  |                                                                             |  |
|  |  - Quorum Consensus: 4/6 Writes, 3/6 Reads                                  |  |
|  |  - Auto-grows dynamically up to 128 TiB in 10 GB protected segments         |  |
|  |  - Continuous background gossip repair & sector self-healing                |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

### The Aurora "The Log Is The Database" Innovation
In traditional databases (RDS PostgreSQL/MySQL):
- Writing a transaction requires writing to the Write-Ahead Log (WAL), writing double-write buffers, updating data blocks, and flushing dirty pages over the network to attached EBS storage.
- High network I/O bottlenecks overall transactions per second.

In Amazon Aurora:
- Compute nodes write **only Redo Log records** over the network to the distributed storage fleet.
- Storage nodes independently process the logs in parallel, materializing data blocks on disk asynchronously.
- Reduces network I/O traffic by **up to 80%**, unlocking massive transaction throughput.

---

## 4. Syntax & Usage

### 4.1 Creating an Aurora PostgreSQL Cluster via AWS CLI
```bash
# 1. Create DB Subnet Group spanning 3 AZs
aws rds create-db-subnet-group \
  --db-subnet-group-name "aurora-db-subnet-group" \
  --db-subnet-group-description "Private subnets for Aurora cluster" \
  --subnet-ids "subnet-01a" "subnet-01b" "subnet-01c"

# 2. Create Aurora PostgreSQL Cluster
aws rds create-db-cluster \
  --db-cluster-identifier "production-aurora-cluster" \
  --engine "aurora-postgresql" \
  --engine-version "16.1" \
  --master-username "clusteradmin" \
  --master-user-password "MasterPassword123!" \
  --db-subnet-group-name "aurora-db-subnet-group" \
  --vpc-security-group-ids "sg-0123456789abcdef0" \
  --backup-retention-period 14 \
  --storage-encrypted

# 3. Create Primary Writer Instance
aws rds create-db-instance \
  --db-instance-identifier "production-aurora-instance-1" \
  --db-cluster-identifier "production-aurora-cluster" \
  --engine "aurora-postgresql" \
  --db-instance-class "db.r7g.xlarge"

# 4. Create Read Replica Instance in a different AZ
aws rds create-db-instance \
  --db-instance-identifier "production-aurora-instance-2" \
  --db-cluster-identifier "production-aurora-cluster" \
  --engine "aurora-postgresql" \
  --db-instance-class "db.r7g.xlarge"
```

### 4.2 Connecting to Endpoints
- **Cluster Endpoint (Read/Write)**: Automatically resolves to the current primary writer node:  
  `production-aurora-cluster.cluster-abc123.us-east-1.rds.amazonaws.com:5432`
- **Reader Endpoint (Read-Only)**: Automatically load-balances read queries across all active read replicas via DNS round-robin:  
  `production-aurora-cluster.cluster-ro-abc123.us-east-1.rds.amazonaws.com:5432`

---

## 5. Practical Examples

### Example 1: Declarative Terraform Aurora Serverless v2 Cluster
Provision an Aurora Serverless v2 PostgreSQL cluster scaling dynamically from 0.5 to 16 ACUs.

```hcl
resource "aws_rds_cluster" "aurora_serverless" {
  cluster_identifier      = "production-serverless-aurora"
  engine                  = "aurora-postgresql"
  engine_version          = "16.1"
  database_name           = "appdb"
  master_username         = "appadmin"
  manage_master_user_password = true # Stores credentials in AWS Secrets Manager

  db_subnet_group_name    = var.db_subnet_group_name
  vpc_security_group_ids  = [var.db_security_group_id]
  storage_encrypted       = true
  kms_key_id              = var.kms_key_arn

  backup_retention_period = 30
  preferred_backup_window = "02:00-03:00"

  serverlessv2_scaling_configuration {
    min_capacity = 0.5
    max_capacity = 16.0
  }

  deletion_protection = true
}

resource "aws_rds_cluster_instance" "writer" {
  cluster_identifier = aws_rds_cluster.aurora_serverless.id
  instance_class     = "db.serverless"
  engine             = aws_rds_cluster.aurora_serverless.engine
  engine_version     = aws_rds_cluster.aurora_serverless.engine_version
  identifier         = "aurora-serverless-instance-1"
}

resource "aws_rds_cluster_instance" "reader" {
  cluster_identifier = aws_rds_cluster.aurora_serverless.id
  instance_class     = "db.serverless"
  engine             = aws_rds_cluster.aurora_serverless.engine
  engine_version     = aws_rds_cluster.aurora_serverless.engine_version
  identifier         = "aurora-serverless-instance-2"
}
```

### Example 2: Managing RDS Multi-AZ Failover via CLI
Trigger an intentional failover to test application reconnection resiliency.

```bash
# Force an immediate failover on the primary instance
aws rds failover-db-cluster \
  --db-cluster-identifier "production-aurora-cluster"

# Monitor failover transition status
aws rds describe-db-clusters \
  --db-cluster-identifier "production-aurora-cluster" \
  --query 'DBClusters[0].DBClusterMembers[*].[DBInstanceIdentifier, IsClusterWriter]' \
  --output table
```

### Example 3: Configuring IAM Database Authentication
Connect to PostgreSQL using temporary IAM tokens instead of static passwords.

```sql
-- 1. Inside PostgreSQL, grant rds_iam role to a database user
CREATE USER api_user;
GRANT rds_iam TO api_user;
GRANT SELECT, INSERT, UPDATE ON ALL TABLES IN SCHEMA public TO api_user;
```

**Python Boto3 Client Script:**
```python
import boto3
import psycopg2

rds_client = boto3.client('rds', region_name='us-east-1')

DB_HOST = "production-aurora-cluster.cluster-abc123.us-east-1.rds.amazonaws.com"
DB_PORT = 5432
DB_USER = "api_user"
DB_NAME = "appdb"

# Generate 15-minute authentication token
auth_token = rds_client.generate_db_auth_token(
    DBHostname=DB_HOST,
    Port=DB_PORT,
    DBUsername=DB_USER
)

# Connect to database using auth token as password
conn = psycopg2.connect(
    host=DB_HOST,
    port=DB_PORT,
    user=DB_USER,
    password=auth_token,
    dbname=DB_NAME,
    sslmode='require'
)

cursor = conn.cursor()
cursor.execute("SELECT version();")
print(f"Connected successfully: {cursor.fetchone()}")
conn.close()
```

### Example 4: Creating a Cross-Region Read Replica for Disaster Recovery
Provision an asynchronous read replica in `us-west-2` from an RDS PostgreSQL primary in `us-east-1`.

```bash
aws rds create-db-instance-read-replica \
  --db-instance-identifier "dr-replica-postgres-instance" \
  --source-db-instance-identifier "arn:aws:rds:us-east-1:111122223333:db:production-primary-instance" \
  --db-instance-class "db.m7g.large" \
  --region us-west-2 \
  --kms-key-id "arn:aws:kms:us-west-2:111122223333:key/abc-123" \
  --publicly-accessible false
```

### Example 5: Configuring Aurora Custom Endpoints for Workload Isolation
Isolate heavy reporting and analytical queries to specific read replicas without impacting web traffic.

```bash
aws rds create-db-cluster-endpoint \
  --db-cluster-identifier "production-aurora-cluster" \
  --db-cluster-endpoint-identifier "analytics-endpoint" \
  --endpoint-type "READER" \
  --static-members "production-aurora-instance-2"
```

---

## 6. Best Practices

1. **Always Use Graviton Instances (`db.r7g`, `db.m7g`)**: AWS Graviton instances provide up to 20% lower hourly costs and 25% higher query throughput for both PostgreSQL and MySQL compared to x86 alternatives.
2. **Standardize on Aurora for High-Concurrency Web Workloads**: Standard RDS MySQL/PostgreSQL bottlenecks on EBS I/O and replication lag when scaling read replicas. Aurora's shared storage engine eliminates replication delay and handles tens of thousands of concurrent connections easily.
3. **Use IAM Database Authentication with Secret Manager Rotation**: Eliminate hardcoded passwords in configuration files. Leverage AWS Secrets Manager with managed automatic rotation, or use IAM DB authentication.
4. **Implement AWS RDS Proxy for Serverless Workloads**: Serverless compute (AWS Lambda) can rapidly open thousands of short-lived database connections, exhausting connection pools. Place **Amazon RDS Proxy** in front of RDS/Aurora to pool and share database connections, cutting failover times by up to 66%.
5. **Always Place Databases in Private Subnets Across 3 AZs**: DB Subnet Groups must span at least two (ideally three) Availability Zones. Databases must have `PubliclyAccessible = false` and reject all public traffic.

---

## 7. Common Mistakes

| Mistake | Consequence | Correct Pattern |
|:---|:---|:---|
| **Using Read Replicas as DR Failover Targets on RDS** | Asynchronous replication lag causes data loss during uncoordinated promotions. | Use **RDS Multi-AZ** for synchronous zero-data-loss failover; use Read Replicas strictly for read scaling. |
| **Connecting Lambda Functions Directly to RDS** | Database crashes with `too many connections` as Lambda scales to hundreds of concurrent executions. | Deploy **Amazon RDS Proxy** between Lambda and the database cluster to pool connections. |
| **Hardcoding Writer Node IP Address** | When Multi-AZ failover or host replacement occurs, client traffic continues sending writes to the dead host. | Always connect via the **Cluster Endpoint** DNS CNAME, and tune application DNS TTL to under 5 seconds. |
| **Omitting Deletion Protection in Production** | Accidental `Destroy` calls in Terraform or console clicks instantly destroy the production database. | Enable `deletion_protection = true` across all production RDS and Aurora instances. |
| **Under-Sizing Aurora Serverless v2 Min Capacity** | Setting `min_capacity = 0.5` causes cold-start latency spikes when sudden heavy traffic bursts hit the database. | Set `min_capacity` (e.g. 2–4 ACUs) to provide an adequate pre-warmed memory baseline for production APIs. |
| **Running Analytical Queries Against Writer Node** | Long-running reporting queries consume CPU and buffer cache, causing timeouts on critical transactional writes. | Route reporting traffic to the **Reader Endpoint** or a dedicated **Custom Endpoint**. |

---

## 8. Performance, Security & Cost Notes

### Scaling & Throughput Dynamics
- **Standard RDS Throughput**: Bounded by EBS volume limits (maximum 16,000 IOPS for `gp3`, or up to 256,000 IOPS for `io2 Block Express`).
- **Amazon Aurora Throughput**: Delivers up to 500,000 read queries/sec and 100,000 write queries/sec per cluster.
- **Failover Speeds**: Standard RDS Multi-AZ takes **60–120 seconds** (requires DNS CNAME propagation and crash recovery). Aurora takes **under 30 seconds** (no crash recovery needed because storage is already consistent).

### Security Architecture
- **KMS Envelope Encryption**: All database storage, automated backups, snapshots, and read replicas are encrypted at rest using AWS KMS.
- **TLS Protocol Enforcement**: Enforce mandatory TLS connections by modifying parameter groups (`rds.force_ssl = 1`).

### Cost Comparison
- **Amazon RDS**: Billed for instance runtime (per second) + provisioned EBS storage + provisioned IOPS + backup storage.
- **Amazon Aurora**:
  - Aurora Standard: Billed for compute runtime + actual storage consumed ($0.10/GB/month) + I/O requests ($0.20 per million).
  - Aurora I/O-Optimized: 40% higher compute price, but **zero I/O request fees**, saving up to 50% for I/O-heavy applications.

---

## 9. Comparison Table

| Feature | Amazon RDS PostgreSQL | Amazon Aurora PostgreSQL |
|:---|:---|:---|
| **Storage Architecture** | Network-attached Amazon EBS | Shared 6-way distributed NVMe storage |
| **High Availability** | Multi-AZ Standby (Synchronous, 2 AZs) | Distributed Quorum across 3 AZs |
| **Failover Duration** | 60–120 seconds | Under 30 seconds |
| **Read Replicas** | Up to 15 (Independent EBS, Async lag)| Up to 15 (Shared storage, <10ms lag) |
| **Storage Auto-Scaling**| Storage Auto-Scaling (Requires EBS grow)| Completely automatic up to 128 TiB |
| **Serverless Option** | None (Static instances only) | Aurora Serverless v2 (0.5 to 128 ACUs) |
| **Global Footprint** | Cross-Region Read Replicas (Minutes lag)| Aurora Global Database (<1 sec latency)|

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: How does the underlying storage architecture of Amazon Aurora differ from Amazon RDS?</strong></summary>

**Answer:**
- **Amazon RDS**: Operates on a traditional monolithic architecture. The compute instance runs the database engine and attaches to a dedicated Amazon EBS network volume. Storage is tied to that specific instance, and writes require pushing entire dirty buffer pages and write-ahead logs across the network to EBS. Multi-AZ requires duplicating the entire compute and storage footprint in a second AZ.
- **Amazon Aurora**: Decouples compute from storage completely. The compute node streams **only lightweight Redo Log records** over the network to a purpose-built, distributed storage fleet. The storage layer writes 6 copies of the data across 3 Availability Zones using a quorum model (4/6 writes). All compute nodes (Primary Writer and up to 15 Read Replicas) share access to the exact same underlying storage tier, eliminating replication lag and double-buffering.
</details>

<details>
<summary><strong>Q2: What is the sequence of events during an Amazon RDS Multi-AZ automated failover?</strong></summary>

**Answer:**
1. A failure occurs on the primary instance (hardware crash, OS failure, or network disruption).
2. The RDS internal monitoring system detects consecutive heartbeat failures (typically within 30 seconds).
3. The standby replica in the secondary AZ is promoted to the primary role.
4. Amazon RDS updates the primary database **DNS CNAME record** (`mydb.abc123.rds.amazonaws.com`) to point to the IP address of the newly promoted standby instance.
5. Client applications reconnect to the DNS endpoint. Failover duration is typically **60 to 120 seconds**, dependent on the time required for OS boot, database crash recovery, and client DNS caching TTLs.
</details>

<details>
<summary><strong>Q3: Why does Amazon Aurora failover occur significantly faster than Amazon RDS?</strong></summary>

**Answer:**
Aurora failover completes in **under 30 seconds** because:
1. **Shared Storage**: Aurora Read Replicas share the exact same underlying distributed storage fleet as the Primary Writer. There is no independent disk volume to synchronize or mount.
2. **Zero Crash Recovery**: In standard RDS, the promoted standby must replay transaction logs from disk to rebuild dirty pages in memory before accepting connections. In Aurora, the distributed storage engine continuously processes redo logs in parallel across storage nodes, meaning the database storage is already consistent. When failover occurs, a promoted reader can accept write traffic immediately.
</details>

<details>
<summary><strong>Q4: What is Aurora Serverless v2 and what is an Aurora Capacity Unit (ACU)?</strong></summary>

**Answer:**
Aurora Serverless v2 is an on-demand, auto-scaling configuration for Amazon Aurora:
- It dynamically adjusts compute and memory capacity in fractions of a second based on application workload demand.
- Capacity is measured in **Aurora Capacity Units (ACUs)**: 1 ACU provides approximately **2 GB of RAM**, corresponding vCPU, and network bandwidth.
- It scales in increments as fine as 0.5 ACU (from 0.5 to 128 ACUs), scaling up or down seamlessly while preserving active client database connections, buffer pools, and open transactions.
</details>

<details>
<summary><strong>Q5: What problem does Amazon RDS Proxy solve for modern cloud applications?</strong></summary>

**Answer:**
In serverless architectures (like AWS Lambda), hundreds or thousands of function instances can spawn simultaneously in response to traffic spikes. If each function opens its own direct connection to a relational database, the database rapidly exceeds its maximum connection ceiling (`max_connections`), exhausting memory and rejecting requests.

**Amazon RDS Proxy**:
- Sits between applications and the database, establishing a shared pool of long-lived database connections.
- Allows thousands of concurrent serverless requests to share a small pool of database connections (connection multiplexing).
- Accelerates failover times by up to **66%** by preserving client connections during database failovers and swapping backend connections automatically.
</details>

<details>
<summary><strong>Q6: How does Amazon Aurora maintain data consistency if an entire Availability Zone crashes?</strong></summary>

**Answer:**
Aurora uses a **4 of 6 write quorum and 3 of 6 read quorum** across 6 storage copies distributed evenly across 3 Availability Zones (2 copies per AZ):
- If an entire Availability Zone fails completely, 2 out of the 6 storage copies become unavailable.
- However, **4 copies remain alive** in the surviving 2 AZs.
- Because 4 copies are sufficient to satisfy the 4/6 write quorum, Aurora **continues to process write transactions with zero data loss and zero downtime**.
- Even if a third copy fails (leaving 3 of 6), Aurora can still satisfy read quorums while self-healing the missing copies in the background.
</details>

<details>
<summary><strong>Q7: What is the purpose of Aurora Custom Endpoints?</strong></summary>

**Answer:**
By default, Aurora provides a Cluster Endpoint (Writer) and a Reader Endpoint (which load balances across all read replicas via DNS round-robin).
A **Custom Endpoint** allows architects to map specific client workloads to designated subsets of read replicas:
- Example: Mapping 2 high-memory instances (`db.r7g.2xlarge`) to an `analytics-endpoint` exclusively for data warehouse reporting.
- Normal customer web traffic connects to the standard Reader Endpoint on general-purpose instances, preventing heavy analytical queries from starving web API response times.
</details>

<details>
<summary><strong>Q8: When should you choose Amazon Aurora I/O-Optimized pricing over Aurora Standard?</strong></summary>

**Answer:**
- **Aurora Standard**: Billed for base compute + storage + **$0.20 per million I/O requests**. Best for light to moderate transactional workloads where I/O costs account for less than 25% of the database bill.
- **Aurora I/O-Optimized**: Compute costs are 40% higher and storage costs are 125% higher, but **all read and write I/O operations are 100% free**. Choose I/O-Optimized when I/O costs exceed 25% of your total Aurora bill (e.g., high-frequency trading, e-commerce payment ledgers, or real-time analytics), saving up to 50% on total database spend.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create an RDS DB Subnet Group via CLI</strong></summary>

**Objective**: Configure a multi-AZ private subnet group.

**Command**:
```bash
aws rds create-db-subnet-group \
  --db-subnet-group-name "lab-db-subnet-group" \
  --db-subnet-group-description "Subnet group for database lab" \
  --subnet-ids "subnet-01a" "subnet-01b" "subnet-01c"
```
</details>

<details>
<summary><strong>Exercise 2: Provision an Aurora Serverless v2 Cluster</strong></summary>

**Objective**: Deploy a cluster scaling from 0.5 to 4 ACUs.

**Commands**:
```bash
aws rds create-db-cluster \
  --db-cluster-identifier "lab-aurora-serverless" \
  --engine "aurora-postgresql" \
  --engine-version "16.1" \
  --master-username "labadmin" \
  --master-user-password "SecurePass123!" \
  --db-subnet-group-name "lab-db-subnet-group" \
  --serverlessv2-scaling-configuration MinCapacity=0.5,MaxCapacity=4.0

aws rds create-db-instance \
  --db-instance-identifier "lab-aurora-instance-1" \
  --db-cluster-identifier "lab-aurora-serverless" \
  --engine "aurora-postgresql" \
  --db-instance-class "db.serverless"
```
</details>

<details>
<summary><strong>Exercise 3: Test Manual Aurora Failover via CLI</strong></summary>

**Objective**: Trigger an intentional failover and monitor endpoint promotion.

**Command**:
```bash
aws rds failover-db-cluster --db-cluster-identifier "lab-aurora-serverless"
```
</details>

<details>
<summary><strong>Exercise 4: Enable Automated Password Management with Secrets Manager</strong></summary>

**Objective**: Provision a database with master credentials managed automatically by AWS Secrets Manager.

**Command**:
```bash
aws rds create-db-cluster \
  --db-cluster-identifier "vault-aurora-cluster" \
  --engine "aurora-mysql" \
  --master-username "admin" \
  --manage-master-user-password \
  --db-subnet-group-name "lab-db-subnet-group"
```
</details>

<details>
<summary><strong>Exercise 5: Create an Aurora Custom Endpoint for Analytics</strong></summary>

**Objective**: Route reporting queries to instance 2.

**Command**:
```bash
aws rds create-db-cluster-endpoint \
  --db-cluster-identifier "lab-aurora-serverless" \
  --db-cluster-endpoint-identifier "reporting" \
  --endpoint-type "READER" \
  --static-members "lab-aurora-instance-2"
```
</details>

<details>
<summary><strong>Exercise 6: Connect to PostgreSQL Using IAM Authentication Token</strong></summary>

**Objective**: Practice generating temporary IAM tokens and executing queries without static passwords.

**Steps**:
1. Grant `rds_iam` role to database user.
2. Generate token: `aws rds generate-db-auth-token --hostname <HOST> --port 5432 --username <USER>`.
3. Connect using `psql "host=<HOST> port=5432 sslmode=require dbname=appdb user=<USER>"`.
4. Paste the token when prompted for password.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------+
|                                Amazon RDS vs. Aurora Matrix                           |
+---------------------------------------------------------------------------------------+
|  Dimension             | Amazon RDS                        | Amazon Aurora            |
|------------------------+-----------------------------------+--------------------------|
|  Storage Layer         | Dedicated EBS volume per instance | Shared 6-way distributed |
|  Multi-AZ HA           | Synchronous Standby (Passive)     | Quorum (Active-Active)   |
|  Failover Latency      | 60 to 120 seconds                 | Under 30 seconds         |
|  Read Scaling          | Up to 15 (Async EBS lag)          | Up to 15 (<10ms lag)     |
|  Auto-Scaling Compute  | None                              | Serverless v2 (0.5-128)  |
|  Global Database       | Read Replica across regions       | Global DB (<1s latency)  |
|  Connection Pooling    | Amazon RDS Proxy                  | Amazon RDS Proxy         |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [Amazon Relational Database Service (RDS) User Guide](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/Welcome.html)
- [Amazon Aurora User Guide](https://docs.aws.amazon.com/AmazonRDS/latest/AuroraUserGuide/CHAP_AuroraOverview.html)
- [Amazon Aurora Architecture: How It Works](https://docs.aws.amazon.com/AmazonRDS/latest/AuroraUserGuide/Aurora.Overview.StorageReliability.html)
- [Managing Connections with Amazon RDS Proxy](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/rds-proxy.html)
- [Using IAM Database Authentication with PostgreSQL and MySQL](https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/UsingWithRDS.IAMDBAuth.html)
