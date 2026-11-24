# Module 03: Storage & Databases

Welcome to the **AWS Storage & Databases** module. This curriculum provides comprehensive, production-grade coverage of cloud storage architectures, relational database engines, distributed NoSQL datastores, in-memory caching systems, and disaster recovery strategies across Amazon Web Services.

---

## Module Overview

This module equips data architects, cloud engineers, and platform administrators with the deep operational knowledge required to design, secure, scale, and recover enterprise data tiers. Topics range from foundational object storage to cloud-native Aurora distributed quorum architectures, single table NoSQL modeling in DynamoDB, and automated disaster recovery.

---

## Detailed Curriculum Roadmap

| # | Note | Description | Key Focus Areas |
|:---|:---|:---|:---|
| **01** | [Amazon S3 Fundamentals](01-s3.md) | Object storage architecture, storage classes, lifecycle policies, and security governance | 11 9's durability, S3 Intelligent-Tiering, Block Public Access, Bucket Policies, Presigned URLs |
| **02** | [Amazon S3 Advanced Architecture](02-s3-deep-dive.md) | Replication, WORM compliance, ultra-low latency directory buckets, and global access | Cross-Region Replication (CRR/RTC), S3 Object Lock, S3 Express One Zone, Multi-Region Access Points |
| **03** | [Amazon EBS, EFS, and FSx](03-ebs-efs-and-fsx.md) | Block, shared POSIX NFS, and parallel high-performance file systems | EBS gp3/io2, Elastic Volumes, Multi-Attach, EFS Elastic Throughput, FSx for Lustre S3 integration |
| **04** | [Amazon DynamoDB Fundamentals](04-dynamodb.md) | Serverless NoSQL key-value/document store, capacity planning, and partition mechanics | Primary keys (PK/SK), WCU/RCU calculations, On-Demand vs Provisioned, Eventual vs Strong Consistency |
| **05** | [DynamoDB Advanced Data Modeling](05-dynamodb-data-modelling.md) | Single Table Design, secondary indexes, event-driven streams, and in-memory acceleration | Single table pre-joins, GSIs vs LSIs, sparse indexes, DynamoDB Streams to Lambda, DAX caching |
| **06** | [Amazon RDS & Amazon Aurora Architecture](06-rds-and-aurora.md) | Managed relational databases, multi-AZ high availability, and cloud-native Aurora | Multi-AZ failover, Read Replicas, Aurora 6-way distributed quorum, Serverless v2, Global Database |
| **07** | [Amazon RDS Day-2 Operations & Tuning](07-rds-operations.md) | Maintenance windows, major version upgrades, parameter tuning, and query profiling | PITR backups, Parameter/Option Groups, RDS Blue/Green Deployments, Performance Insights (AAS) |
| **08** | [Amazon ElastiCache (Redis & Memcached)](08-elasticache-redis-and-memcached.md) | Sub-millisecond in-memory caching, cluster topologies, and caching design patterns | Redis vs Memcached, Cluster Mode Enabled (hash slots), Serverless cache, Cache-Aside, Write-Through |
| **09** | [Amazon OpenSearch Service & Serverless](09-opensearch-service.md) | Distributed search, log analytics, vector search (k-NN), and multi-tier storage | Inverted index, Dedicated Cluster Managers, Hot/UltraWarm/Cold storage, OpenSearch Serverless |
| **10** | [AWS Analytics & Lakehouse Overview](10-analytics-overview.md) | Modern Data Lakehouse architecture, serverless SQL, and cloud data warehousing | Amazon Athena, AWS Glue Data Catalog & Crawlers, Amazon Redshift Serverless & Spectrum |
| **11** | [AWS Backup & Disaster Recovery Strategies](11-backup-and-disaster-recovery.md) | Centralized backup governance, immutable vault lock, RTO/RPO metrics, and DR topologies | RTO/RPO tiering, Pilot Light, Warm Standby, Active-Active, AWS Backup Vault Lock (WORM) |

---

## Architectural Principles Enforced

1. **Decoupled Compute and Storage**:
   - Modern architectures (Amazon Aurora, S3 Express One Zone, Redshift Serverless) decouple compute nodes from underlying storage fabrics, eliminating replication bottlenecks and enabling independent auto-scaling.
2. **Access-Pattern-Driven Data Modeling**:
   - Move beyond rigid relational normalization when designing internet-scale NoSQL systems: embrace DynamoDB Single Table Design to satisfy read patterns in a single sub-5ms round trip.
3. **Immutable Data Protection & Air-Gapping**:
   - Neutralize ransomware threats by enforcing **WORM** (Write Once, Read Many) immutability using **S3 Object Lock** and **AWS Backup Vault Lock in Compliance Mode**.
   - Copy backups cross-account to isolated, air-gapped security vaults.
4. **Predictive FinOps Tiering**:
   - Eliminate idle infrastructure costs using automated lifecycle policies: S3 Intelligent-Tiering, EFS Lifecycle Management, OpenSearch UltraWarm, and Aurora Serverless v2.

---

## Recommended Learning Flow

```
[01. S3 Fundamentals] --------> [02. S3 Advanced Architecture & Express]
       |                                          |
       +--------------------+                     v
                            |          [03. EBS, EFS & FSx Storage]
                            v                     |
               [04. DynamoDB Fundamentals]        |
                            |                     v
               [05. DynamoDB Single Table]     [06. RDS & Aurora Architecture]
                            |                     |
                            v                     v
            [08. ElastiCache Redis/Memcached]  [07. RDS Operations & Blue/Green]
                            |                     |
                            +----------+----------+
                                       |
                                       v
                        [09. OpenSearch & Vector Search]
                                       |
                                       v
                        [10. Analytics & Data Lakehouse]
                                       |
                                       v
                        [11. AWS Backup & DR Strategies]
```

Proceed to **[Module 04: Networking & Content Delivery](../04-networking/README.md)** once you have mastered storage and database architectures!
