---
topic: "AWS System Design & Architecture Scenario Technical Interview Questions"
module: "09-interview"
track: "aws"
difficulty: "expert"
last-tested: "2026-11"
tags:
  - "interview"
  - "system-design"
  - "scenarios"
  - "high-throughput"
  - "multi-region"
  - "data-lake"
  - "architecture"
---

# AWS System Design & Architecture Scenario Technical Interview Questions

## Table of Contents
- [1. Scenario 1: Designing a 100,000 Requests/Sec Global REST API](#1-scenario-1-designing-a-100000-requestssec-global-rest-api)
- [2. Scenario 2: Multi-Region Active-Active Disaster Recovery Platform](#2-scenario-2-multi-region-active-active-disaster-recovery-platform)
- [3. Scenario 3: Real-Time Clickstream Ingestion & Fraud Detection (10 TB/Day)](#3-scenario-3-real-time-clickstream-ingestion--fraud-detection-10-tbday)
- [4. Scenario 4: Flash-Sale E-Commerce Ticketing System with Zero Overselling](#4-scenario-4-flash-sale-e-commerce-ticketing-system-with-zero-overselling)
- [5. System Design Interview Framework: The 5-Step Formula](#5-system-design-interview-framework-the-5-step-formula)

---

## 1. Scenario 1: Designing a 100,000 Requests/Sec Global REST API

### Problem Statement
*Design a global, resilient, low-latency REST API capable of serving 100,000 requests per second with a P99 latency SLA under 50 milliseconds, zero single points of failure, and strict rate-limiting per API key.*

### Architectural Solution & Component Breakdown

```
+----------------------------------------------------------------------------------------------------+
|                         100,000 REQ/SEC GLOBAL API ARCHITECTURAL TOPOLOGY                          |
+----------------------------------------------------------------------------------------------------+

  [ Global Client Applications ]
               |
               | HTTPS / Anycast IP Routing
               v
  +--------------------------------------------------------------------------------------------------+
  | AWS Global Accelerator (Static Anycast IPs, Bypasses Public Internet Congestion)                |
  +--------------------------------------------------------------------------------------------------+
               |
               v
  +--------------------------------------------------------------------------------------------------+
  | Amazon CloudFront Edge + AWS WAF (Rate Limiting, IP Reputation, Bot Mitigation)                  |
  +--------------------------------------------------------------------------------------------------+
               |
               | Regional Ingress (us-east-1, eu-west-1, ap-southeast-1)
               v
  +--------------------------------------------------------------------------------------------------+
  | Application Load Balancer (Cross-Zone Enabled, Pre-warmed / Auto-scaled)                         |
  +--------------------------------------------------------------------------------------------------+
               |
               | Target Group (Round Robin)
               v
  +--------------------------------------------------------------------------------------------------+
  | Amazon ECS on AWS Fargate Fleet (ARM64 Graviton3, Go / Rust / Node.js Microservices)             |
  |  - Auto-scaling based on ALB RequestCountPerTarget (target: 1,000 req/target)                    |
  +--------------------------------------------------------------------------------------------------+
         |                                                                      |
         | Sub-millisecond Cache Reads                                          | Single-digit ms Writes
         v                                                                      v
  +---------------------------------------+            +---------------------------------------------+
  | Amazon ElastiCache for Redis Cluster  |            | Amazon DynamoDB (Global Tables)             |
  | (Multi-AZ with Auto-Failover, Cluster |            | (On-Demand / Autoscaled Provisioned,        |
  |  Mode Enabled, 5 Shards)              |            |  Single-Table Design, DAX accelerator)      |
  +---------------------------------------+            +---------------------------------------------+
```

### Key Technical Decisions
1. **Edge Acceleration (Global Accelerator + CloudFront)**:
   - AWS Global Accelerator provides two static Anycast IPv4 addresses. Client TCP handshakes terminate at the nearest AWS edge PoP, routing traffic over the dedicated private AWS fiber backbone.
   - CloudFront caches static responses, and AWS WAF enforces rate limiting (e.g., max 2,000 requests per 5 minutes per client IP/API key).
2. **Compute Tier (ECS Fargate vs Lambda)**:
   - *Why NOT Lambda*: At a constant 100,000 requests/sec, AWS Lambda would cost \$50,000+/month in invocation execution fees and risk account concurrency throttling.
   - *Why ECS Fargate*: Sustained high traffic is significantly more cost-effective on persistent container fleets. A fleet of 100 Fargate tasks (each handling 1,000 req/sec) running Go or Node.js on Graviton3 ARM64 delivers consistent 10ms latencies with zero cold starts.
3. **Database & Caching Tier**:
   - **Amazon ElastiCache for Redis**: Read-heavy cache absorbing 85-90% of read volume with sub-millisecond latencies.
   - **Amazon DynamoDB**: Serves persistent writes with sub-10ms response times. Partition keys are designed with high-cardinality prefixes (`tenant_id#entity_id`) to distribute IOPS across thousands of internal physical storage partitions.

---

## 2. Scenario 2: Multi-Region Active-Active Disaster Recovery Platform

### Problem Statement
*Design a mission-critical financial ledger system spanning two AWS regions (us-east-1 and us-west-2). The system must operate in an Active-Active configuration where users in both regions read and write locally. RTO must be sub-second, and RPO must be under 1 second.*

### Architectural Solution & Component Breakdown

```
+----------------------------------------------------------------------------------------------------+
|                         MULTI-REGION ACTIVE-ACTIVE FINANCIAL ARCHITECTURE                          |
+----------------------------------------------------------------------------------------------------+

  [ US East Users ]                                                            [ US West Users ]
          |                                                                            |
          v                                                                            v
  +--------------------------------------------------------------------------------------------------+
  | Amazon Route 53 (Latency-Based Routing with Health Check Target Tracking)                        |
  +--------------------------------------------------------------------------------------------------+
          |                                                                            |
          v                                                                            v
  +---------------------------------------+                    +-------------------------------------+
  | PRIMARY REGION: us-east-1             |                    | SECONDARY REGION: us-west-2         |
  |                                       |                    |                                     |
  |  [ Application Load Balancer ]        |                    |  [ Application Load Balancer ]      |
  |                 |                     |                    |                 |                   |
  |                 v                     |                    |                 v                   |
  |  [ ECS Fargate Container Fleet ]      |                    |  [ ECS Fargate Container Fleet ]    |
  |                 |                     |                    |                 |                   |
  |                 v                     |                    |                 v                   |
  |  +---------------------------------+  |                    |  +--------------------------------+ |
  |  | Amazon DynamoDB Global Table    |  | <-- 1s Sync Lag -> |  | Amazon DynamoDB Global Table   | |
  |  | (Local Write: us-east-1)        |  |                    |  | (Local Write: us-west-2)       | |
  |  +---------------------------------+  |                    |  +--------------------------------+ |
  +---------------------------------------+                    +-------------------------------------+
```

### Key Technical Decisions & Conflict Resolution
1. **Database Selection (DynamoDB Global Tables vs Aurora Global Database)**:
   - *Why NOT Aurora Global Database*: Aurora Global Database supports only a **single active writer** region; secondary regions are read-only replicas. Routing writes across regions introduces 70ms+ cross-country network latency.
   - *Why DynamoDB Global Tables*: Supports true **multi-region active-active writes**. Applications write locally to the nearest region with 5ms latency; DynamoDB asynchronously replicates updates across regions within ~1 second.
2. **Conflict Resolution Strategy**:
   - DynamoDB Global Tables uses **Last-Write-Wins (LWW)** based on internal timestamps.
   - To prevent concurrent write conflicts on ledger balances, the application does not execute in-place balance mutations (`UPDATE balance = balance - 50`). Instead, it uses an **Append-Only Event Sourcing Pattern**: every transaction is written as an immutable new record (`PK: ACCOUNT#123, SK: TX#<UUID>`). Current balances are calculated via atomic folds or Materialized Views.
3. **Automated Regional Evacuation (RTO < 10s)**:
   - Route 53 continuously monitors regional health check endpoints.
   - If an entire region degrades, Route 53 shifts 100% of DNS traffic to the surviving region in under 10 seconds. Because data is already present in the local DynamoDB replica, the surviving region serves production traffic immediately.

---

## 3. Scenario 3: Real-Time Clickstream Ingestion & Fraud Detection (10 TB/Day)

### Problem Statement
*An e-commerce platform generates 10 TB of clickstream and telemetry data daily (averaging 50,000 events/sec, bursting to 200,000 events/sec). The business requires: (1) Real-time fraud anomaly scoring within 500ms of user action, and (2) Long-term storage in an open-format S3 data lake for interactive SQL querying.*

### Architectural Solution & Component Breakdown

```
+----------------------------------------------------------------------------------------------------+
|                         STREAMING INGESTION & FRAUD DETECTION PIPELINE                             |
+----------------------------------------------------------------------------------------------------+

  [ Web & Mobile Clickstreams ]
                |
                | HTTPS POST (JSON Telemetry)
                v
  +--------------------------------------------------------------------------------------------------+
  | Amazon API Gateway / Network Load Balancer                                                       |
  +--------------------------------------------------------------------------------------------------+
                |
                v
  +--------------------------------------------------------------------------------------------------+
  | Amazon Kinesis Data Streams (On-Demand Mode / 200 Shards)                                        |
  +--------------------------------------------------------------------------------------------------+
           |                                                                          |
           | Branch 1: Real-Time Path (<500ms)                                        | Branch 2: Analytical Data Lake
           v                                                                          v
  +---------------------------------------+                          +-------------------------------+
  | Amazon Managed Service for            |                          | Amazon Kinesis Data Firehose  |
  | Apache Flink                          |                          |  - Buffers: 128 MB / 5 mins   |
  |  - Sliding window aggregation         |                          |  - Converts JSON to Parquet   |
  |  - Evaluates fraud risk score         |                          +-------------------------------+
  +---------------------------------------+                                          |
           |                                                                          v
           v (High Risk Anomaly Detected)                            +-------------------------------+
  +---------------------------------------+                          | Amazon S3 (Bronze / Silver)   |
  | Amazon EventBridge -> Step Functions  |                          | Partitioned: year/month/day   |
  |  - Freeze user session                |                          +-------------------------------+
  |  - Trigger MFA challenge              |                                          |
  +---------------------------------------+                                          v
                                                                     +-------------------------------+
                                                                     | Amazon Athena / Glue Catalog  |
                                                                     | (Interactive SQL Analytics)   |
                                                                     +-------------------------------+
```

### Key Technical Decisions
1. **Ingestion Backbone (Amazon Kinesis Data Streams)**:
   - Kinesis decouples high-speed data ingestion from consumption. Operating in On-Demand mode, it elastically absorbs surges up to 200,000 events/sec without manual shard management.
2. **Real-Time Stream Processing (Apache Flink on AWS)**:
   - Ingests streaming records sub-second. Computes tumbling and sliding window metrics (e.g., number of checkout attempts per credit card within 60 seconds).
   - If an anomaly is detected, Flink publishes an event to EventBridge to trigger automated fraud mitigation workflows.
3. **Serverless Lakehouse Archiving (Kinesis Firehose + S3 + Athena)**:
   - Kinesis Data Firehose reads the stream, buffers records into optimal 128 MB blocks, automatically converts raw JSON into compressed **Apache Parquet**, and partitions files by date (`year=YYYY/month=MM/day=DD`) into S3.
   - Data is queryable immediately by data analysts using **Amazon Athena** with zero ETL server management.

---

## 4. Scenario 4: Flash-Sale E-Commerce Ticketing System with Zero Overselling

### Problem Statement
*Design an online ticketing platform selling 50,000 concert tickets in 2 minutes. 1,000,000 concurrent users will hit the "Buy Ticket" button at 10:00 AM. Requirements: Zero database crashes, zero ticket overselling, and fair queuing for end users.*

### Architectural Solution & Component Breakdown

```
+----------------------------------------------------------------------------------------------------+
|                         FLASH-SALE TICKETING ARCHITECTURAL TOPOLOGY                                |
+----------------------------------------------------------------------------------------------------+

  [ 1,000,000 Concurrent Buyers ]
                 |
                 v
  +--------------------------------------------------------------------------------------------------+
  | Amazon CloudFront + Virtual Waiting Room (CloudFront Functions / Lambda@Edge)                    |
  |  - Rate limits incoming traffic; meters buyers into checkout at 500 users/second                  |
  +--------------------------------------------------------------------------------------------------+
                 |
                 v
  +--------------------------------------------------------------------------------------------------+
  | Amazon API Gateway HTTP API                                                                      |
  +--------------------------------------------------------------------------------------------------+
                 |
                 v
  +--------------------------------------------------------------------------------------------------+
  | Amazon SQS FIFO Queue (`ticket-checkout.fifo`)                                                   |
  |  - MessageGroupId = CONCERT#441                                                                  |
  |  - Absorbs 1M requests safely; buffers transactions in strict arrival order                      |
  +--------------------------------------------------------------------------------------------------+
                 |
                 | Controlled Concurrency Batch Consumption
                 v
  +--------------------------------------------------------------------------------------------------+
  | AWS Lambda Checkout Workers (Reserved Concurrency = 20)                                          |
  |  - Performs atomic reservation in DynamoDB                                                       |
  +--------------------------------------------------------------------------------------------------+
                 |
                 v
  +--------------------------------------------------------------------------------------------------+
  | Amazon DynamoDB (Inventory Table)                                                                |
  |  - Item: { ConcertId: "441", AvailableTickets: 50000 }                                           |
  |  - Conditional Write: SET AvailableTickets = AvailableTickets - 1                                 |
  |                       WHERE AvailableTickets > 0                                                 |
  +--------------------------------------------------------------------------------------------------+
```

### Key Technical Decisions
1. **Virtual Waiting Room at CloudFront Edge**:
   - Rather than allowing 1,000,000 buyers to hammer backend APIs simultaneously, a **CloudFront Function** evaluates an encrypted signed admission token.
   - Surplus users are routed to a static waiting room page. Users are metered into the checkout funnel at a steady rate of 500 buyers/second.
2. **Buffer Traffic with SQS FIFO**:
   - Ticket purchase requests are accepted immediately by API Gateway and pushed to an SQS FIFO Queue. The client receives an HTTP 202 Accepted status with a polling job ID.
   - SQS absorbs the traffic spike, protecting downstream inventory databases from connection exhaustion.
3. **Eliminating Overselling with DynamoDB Conditional Writes**:
   - Workers consume from SQS and execute an atomic update against DynamoDB:
     ```python
     table.update_item(
         Key={"ConcertId": "441"},
         UpdateExpression="SET AvailableTickets = AvailableTickets - :qty",
         ConditionExpression="AvailableTickets >= :qty",
         ExpressionAttributeValues={":qty": 1}
     )
     ```
   - If `AvailableTickets` reaches 0, DynamoDB throws `ConditionalCheckFailedException`. It is mathematically impossible to sell more than 50,000 tickets.

---

## 5. System Design Interview Framework: The 5-Step Formula

When presented with an open-ended system design question in an AWS technical interview, follow this structured communication blueprint:

1. **Step 1: Clarify Scope & Functional / Non-Functional Requirements (3-5 mins)**:
   - Ask clarifying questions: What is the read vs write ratio? What is the expected peak QPS? What are the latency and availability SLAs (99.9% vs 99.999%)? What are the RPO/RTO constraints?
2. **Step 2: High-Level Sizing & Capacity Estimations (3 mins)**:
   - Perform quick back-of-the-envelope math: $\text{Requests/sec} \times \text{Payload Size} = \text{Network Bandwidth}$. Estimate daily/yearly storage footprint to determine database choices.
3. **Step 3: Define Core Data Model & API Contracts (5 mins)**:
   - Outline key API endpoints (`POST /orders`, `GET /products/{id}`) and core data models (relational vs document vs single-table NoSQL).
4. **Step 4: Draw the High-Level Architecture (Happy Path) (15 mins)**:
   - Walk through the request path: Edge DNS $\rightarrow$ CDN/WAF $\rightarrow$ Load Balancing $\rightarrow$ Compute $\rightarrow$ Database/Storage. Justify every AWS service choice over alternatives.
5. **Step 5: Deep-Dive into Failure Modes, Scaling & Security (15 mins)**:
   - Address edge cases: What happens if an entire AZ dies? How do you handle poison pill messages? How is data encrypted at rest and in transit? How does the system scale during a 10x traffic surge?
