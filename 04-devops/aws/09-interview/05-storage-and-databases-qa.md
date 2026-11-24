---
topic: "AWS Storage & Databases Technical Interview Q&A"
module: "09-interview"
track: "aws"
difficulty: "advanced"
last-tested: "2026-11"
tags:
  - "interview"
  - "storage"
  - "database"
  - "s3"
  - "ebs"
  - "dynamodb"
  - "rds"
  - "aurora"
---

# AWS Storage & Databases Technical Interview Q&A

## Table of Contents
- [1. Amazon S3 Architecture & Storage Classes](#1-amazon-s3-architecture--storage-classes)
- [2. Block & File Storage: EBS and EFS](#2-block--file-storage-ebs-and-efs)
- [3. Amazon DynamoDB & NoSQL Architecture](#3-amazon-dynamodb--nosql-architecture)
- [4. Relational Databases: RDS vs Amazon Aurora](#4-relational-databases-rds-vs-amazon-aurora)
- [5. Deep-Dive Storage & Database Scenarios](#5-deep-dive-storage--database-scenarios)

---

## 1. Amazon S3 Architecture & Storage Classes

### Q1: Explain the Amazon S3 consistency model. Is S3 eventually consistent or strongly consistent?
**Answer**:
Since December 2020, Amazon S3 provides **strong read-after-write consistency** for `PUT` and `DELETE` requests of objects in all AWS regions at zero extra cost.
- **Immediate Read Consistency**: Immediately after an application executes a successful `PUT` of a new object or an overwrite of an existing object, any subsequent `GET` or `HEAD` request will immediately return the latest version of the object.
- **Immediate Listing Consistency**: Object listings (`ListObjectsV2`) are strongly consistent. Immediately after writing or deleting an object, a list API call will accurately reflect the changes.
- **Concurrent Writes**: S3 uses atomic commits. If two concurrent `PUT` requests write to the exact same key, the write with the latest timestamp wins (Last-Write-Wins).

### Q2: Compare Amazon S3 storage classes across durability, retrieval speed, and cost. When should you use S3 Intelligent-Tiering?
**Answer**:
All S3 storage classes (except One Zone-IA) provide **99.999999999% (11 9s) of data durability** by replicating objects across a minimum of 3 physically separated Availability Zones.

| Storage Class | Availability | Retrieval Latency | Storage Cost / GB | Retrieval Fee | Best For |
|:---|:---|:---|:---|:---|:---|
| **S3 Standard** | 99.99% | Milliseconds | ~\$0.023 | None | Active, frequently accessed data |
| **S3 Intelligent-Tiering** | 99.9% | Milliseconds | Varies | None (Small monitoring fee) | Unknown, unpredictable, or changing access patterns |
| **S3 Standard-IA** | 99.9% | Milliseconds | ~\$0.0125 | \$0.01 / GB | Long-lived, infrequently accessed (backups, DR) |
| **S3 One Zone-IA** | 99.5% | Milliseconds | ~\$0.010 | \$0.01 / GB | Non-critical, recreatable data stored in 1 AZ |
| **S3 Glacier Instant** | 99.9% | Milliseconds | ~\$0.004 | \$0.03 / GB | Rare access requiring millisecond retrieval |
| **S3 Glacier Flexible** | 99.9% | Minutes to Hours | ~\$0.0036 | \$0.03 / GB | Archive data (retrieval within 1-5 mins or 3-5 hours)|
| **S3 Glacier Deep Archive** | 99.9% | 12 to 48 Hours | ~\$0.00099 | \$0.02 / GB | Regulatory 7-10 year compliance archives |

**When to use S3 Intelligent-Tiering**:
Use Intelligent-Tiering whenever access patterns are unpredictable or unknown. S3 automatically monitors access frequency and moves objects between Frequent, Infrequent (no access for 30 days), and Archive Instant (no access for 90 days) tiers with **zero operational overhead and zero retrieval fees**.

### Q3: What is the difference between S3 Object Lock in Governance Mode versus Compliance Mode?
**Answer**:
S3 Object Lock enforces a **WORM (Write Once, Read Many)** data protection model to prevent objects from being deleted or overwritten for a fixed retention period.
- **Governance Mode**:
  - Protects objects from deletion by standard users.
  - *Override permitted*: Users with special IAM permissions (`s3:BypassGovernanceRetention`) can override or delete the retention lock and delete the object.
  - Ideal for testing retention rules or internal corporate data retention policies.
- **Compliance Mode**:
  - Inviolable data lock.
  - **No user—including the AWS account root user—can overwrite or delete the object during the retention period.**
  - The retention period cannot be shortened.
  - Required for strict regulatory compliance mandates (e.g., SEC Rule 17a-4, FINRA, HIPAA).

---

## 2. Block & File Storage: EBS and EFS

### Q4: Compare Amazon EBS gp3 and io2 Block Express volumes. How does gp3 decouple IOPS from storage capacity?
**Answer**:
- **Amazon EBS gp3 (General Purpose SSD)**:
  - Baseline: Delivers a baseline performance of **3,000 IOPS and 125 MB/s throughput** included for free with any volume size (even a 20 GB volume).
  - **Decoupled Scaling**: In legacy `gp2`, IOPS were tied strictly to volume size (3 IOPS per GB; you had to over-provision disk size to get performance). With `gp3`, you can independently provision up to **16,000 IOPS and 1,000 MB/s throughput** without increasing disk gigabytes.
  - Cost: 20% cheaper than `gp2` (\$0.08/GB-month).
- **Amazon EBS io2 Block Express (Provisioned IOPS SSD)**:
  - Designed for Tier-0, mission-critical relational database workloads (SAP HANA, Oracle, Microsoft SQL Server).
  - Delivers up to **256,000 IOPS and 4,000 MB/s throughput** with sub-millisecond average latency.
  - Provides 99.999% volume durability (100x more durable than commodity SSDs).
  - Cost: High (\$0.125/GB-month + \$0.065 per provisioned IOPS).

### Q5: Amazon EBS vs Amazon EFS: When must you use EFS instead of EBS?
**Answer**:
- **Amazon EBS (Elastic Block Store)**:
  - Block-level storage attached to **a single EC2 instance** within the **same Availability Zone** (with minor exceptions for Multi-Attach on io2 within one AZ).
  - High performance, low latency, raw filesystem block device.
- **Amazon EFS (Elastic File System)**:
  - Managed serverless **POSIX file system (NFSv4)**.
  - **Shared Storage**: Can be mounted concurrently by **thousands of EC2 instances, ECS Fargate tasks, and Lambda functions across multiple Availability Zones simultaneously**.
  - Automatically scales storage up and down elastically without pre-provisioning capacity.
  - *Decision Rule*: Use EBS for high-IOPS local databases and OS root drives. Use EFS when multiple compute instances require concurrent read/write access to a shared directory (e.g., content management systems like WordPress, CI/CD build caches, shared machine learning training datasets).

---

## 3. Amazon DynamoDB & NoSQL Architecture

### Q6: Explain how Amazon DynamoDB partitions data internally, and what causes a "Hot Partition".
**Answer**:
DynamoDB achieves horizontal scale by partitioning data across internal storage servers:
1. **Partition Key Hashing**: When an item is written, DynamoDB applies an internal cryptographic hash function (MD5) to the **Partition Key (PK)** value to determine the exact storage partition on which to store the item.
2. **Partition Limits**: Each physical partition is limited to:
   - Maximum storage: **10 GB of data**.
   - Maximum throughput: **1,000 Write Capacity Units (WCU)** and **3,000 Read Capacity Units (RCU)** per second.
3. **What causes a "Hot Partition"**:
   - A hot partition occurs when access patterns are heavily skewed toward a small set of partition keys (e.g., choosing `status` or `creation_date` as a partition key, where millions of requests hit the key `status = "PENDING"`).
   - If requests to a single partition exceed 1,000 WCU or 3,000 RCU, DynamoDB throttles requests (`ProvisionedThroughputExceededException`), even if total provisioned account capacity has not been reached.
- *Mitigation*: Choose high-cardinality partition keys (e.g., `user_id`, `order_id`, `uuid`) or append random synthetic suffix numbers (e.g., `order_id#1`, `order_id#2`) to distribute traffic evenly across physical partitions.

### Q7: Global Secondary Index (GSI) vs Local Secondary Index (LSI) in DynamoDB: Compare design rules and consequences.
**Answer**:

| Attribute | Local Secondary Index (LSI) | Global Secondary Index (GSI) |
|:---|:---|:---|
| **Partition Key** | Must be the **same** as the base table. | Can have a **different** partition key. |
| **Sort Key** | Must be **different** from the base table. | Can be any attribute (or omitted). |
| **Creation Timing** | **Must be created at table creation time**. Cannot be added or deleted later. | **Can be created or deleted anytime** on an existing table. |
| **Storage Limit** | Item collection size cannot exceed **10 GB** per partition key! | **No storage limit**; scales infinitely. |
| **Consistency** | Supports **Strongly Consistent** and Eventual Consistency. | Supports **Eventual Consistency only**. |
| **Capacity Sizing** | Consumes base table's provisioned RCU/WCU. | Has its **own dedicated RCU/WCU** allocation. |

*Architectural Recommendation*: In modern single-table DynamoDB design, **almost always prefer GSIs over LSIs**. LSIs introduce a dangerous 10 GB partition collection hard cap that can cause writes to fail once an entity's data grows.

---

## 4. Relational Databases: RDS vs Amazon Aurora

### Q8: Compare RDS Multi-AZ deployments with RDS Read Replicas across replication mechanism, failover capability, and performance.
**Answer**:

| Dimension | RDS Multi-AZ (Deployment) | RDS Read Replicas |
|:---|:---|:---|
| **Primary Purpose** | **High Availability & Disaster Recovery** | **Read Performance Scalability** |
| **Replication Type** | **Synchronous physical block-level replication** | **Asynchronous engine-level replication** (binlog/WAL) |
| **Active Nodes** | 1 Active Primary + 1 Passive Standby (Standby cannot be queried) | 1 Primary + Up to 15 Read Replicas (All serve read traffic) |
| **Failover** | **Automated failover in 60-120 seconds**; flips DNS CNAME. Zero data loss. | Manual promotion required; risk of data loss due to replication lag. |
| **Geographic Scope** | Within the **same AWS Region** across 2 AZs | Can be created across AZs or **Cross-Region** |

### Q9: How does Amazon Aurora's distributed storage architecture differ fundamentally from traditional Amazon RDS?
**Answer**:
Traditional Amazon RDS runs standard database engines on an EC2 instance attached to a dedicated Amazon EBS volume.
**Amazon Aurora** decouples the SQL database compute layer from the underlying storage layer:
1. **Log-Structured Distributed Storage Engine**:
   - Aurora eliminates traditional write-ahead logging to local disk.
   - When a transaction commits, the compute node sends only redo log records over the network to the storage fleet.
2. **6-Way Replication Across 3 AZs**:
   - Every Aurora database volume is automatically partitioned into 10 GB segments replicated **6 times across 3 Availability Zones** (2 copies per AZ).
3. **Quorum Model for Writes and Reads**:
   - **Write Quorum**: A write is committed the moment **4 of 6 storage nodes** acknowledge the redo log write (tolerates loss of an entire AZ plus an additional storage node without losing write availability).
   - **Read Quorum**: Requires acknowledgment from **3 of 6 nodes**.
4. **Near-Zero Replica Lag**:
   - Up to 15 read replicas share the **exact same underlying storage volume** as the primary writer. Replicas do not replay write logs to local disk; typical replica lag is under 10 milliseconds.
5. **Self-Healing Storage**: Aurora continuously scans and repairs corrupted disk blocks in the background without compute downtime.

---

## 5. Deep-Dive Storage & Database Scenarios

### Scenario 1: Debugging an Aurora CPU Spike and Connection Exhaustion Outage
**Interviewer**: *"During a marketing flash sale, your Aurora PostgreSQL database CPU spikes to 100%, and application containers begin crashing with `FATAL: remaining connection slots are reserved for non-replication superuser connections`. How do you triage and resolve this?"*

**Structured Candidate Response**:
1. **Immediate Stabilization**:
   - Identify whether read traffic is overwhelming the primary writer. If application read queries are hitting the cluster read/write endpoint, immediately point read queries to the **Aurora Reader Endpoint** (`cluster.reader.xxxx`) to distribute traffic across read replicas.
   - Scale out read replicas using Aurora Auto Scaling.
   - If connection count has exhausted `max_connections` (due to hundreds of horizontal Lambda functions opening direct connections): Deploy **AWS RDS Proxy**. RDS Proxy pools and multiplexes connections, reducing database connection memory overhead by up to 90%.
2. **Identify Slow Queries with Performance Insights**:
   - Open **Amazon RDS Performance Insights**.
   - Inspect the **Average Active Sessions (AAS)** metric against the Max vCPU line.
   - Look at the top SQL statements contributing to `CPU` and `Lock:transaction` wait states.
   - Identify un-indexed table scans (`Seq Scan`) caused by new query access patterns.
3. **Long-Term Architectural Fix**:
   - Add missing composite indexes to eliminate full table scans.
   - Cache high-frequency read results in **Amazon ElastiCache for Redis** with a 60-second TTL to eliminate database load.

### Scenario 2: Selecting the Correct Database for a Global Social Graph
**Interviewer**: *"You are designing a fraud detection platform that needs to detect relationship loops (e.g., User A shares an address with User B, who shares a credit card with User C). How do you choose between Amazon DynamoDB, Amazon Aurora, and Amazon Neptune?"*

**Structured Candidate Response**:
- **Why DynamoDB is Unsuitable**: DynamoDB is optimized for known key-value and single-table lookups. Traversing 4-5 degrees of dynamic relationships requires multiple iterative network roundtrips (`Query` calls), resulting in high latency and complex client-side joins.
- **Why Aurora is Suboptimal**: While Aurora can model relationships using relational foreign keys and recursive Common Table Expressions (CTEs), multi-hop recursive SQL joins on billions of relationship rows cause severe CPU degradation and slow queries.
- **The Correct Choice: Amazon Neptune**:
  - Amazon Neptune is a purpose-built, high-performance **Graph Database** engine.
  - Supports open graph standards: **Apache TinkerPop Gremlin** and **openCypher**.
  - Internal storage is optimized for graph structures (nodes, edges, and properties). Traversing millions of relationships across multiple hops takes milliseconds.
  - *Recommendation*: Use **Amazon Neptune** as the fraud relationship graph engine, backed by **Amazon DynamoDB** for raw user profile documents.
