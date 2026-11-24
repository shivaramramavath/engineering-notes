---
topic: "Choosing the Right AWS Service: The Architectural Decision Matrix"
module: "07-architecture-patterns"
track: "aws"
last-tested: "2026-11"
prerequisites:
  - "02-compute/06-compute-decision-matrix.md"
  - "03-storage-and-databases/08-database-selection-guide.md"
  - "05-app-services/09-event-driven-patterns-on-aws.md"
complexity: "advanced"
tags:
  - "decision-guide"
  - "architecture"
  - "trade-offs"
  - "compute"
  - "database"
  - "storage"
  - "messaging"
---

# Choosing the Right AWS Service: The Architectural Decision Matrix

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

With over 200 fully featured services spanning compute, storage, databases, networking, messaging, and analytics, the primary challenge of an AWS Solutions Architect is not simply knowing how individual services work, but **choosing the right service for a given technical and business context**. Selecting the wrong service introduces technical debt, operational friction, excessive latency, or catastrophic cost overruns.

This guide provides a rigorous, battle-tested decision framework across the primary foundational AWS architectural dimensions:
1. **Compute** (Lambda vs Fargate vs ECS EC2 vs EKS vs App Runner vs EC2)
2. **Storage** (S3 vs EBS vs EFS vs FSx)
3. **Database** (Aurora vs RDS vs DynamoDB vs DocumentDB vs ElastiCache vs Neptune vs Timestream)
4. **Messaging & Asynchronous Integration** (SQS vs SNS vs EventBridge vs Kinesis vs MSK vs Step Functions)
5. **API & Network Ingress** (ALB vs NLB vs API Gateway HTTP vs API Gateway REST vs CloudFront)

---

## 2. Core Concepts

### Architectural Decision Dimensions
Every technology selection on AWS must be evaluated against five core trade-off axes:
1. **Operational Burden (NoOps vs SysOps)**: Managed serverless services (DynamoDB, Lambda) eliminate patching, capacity planning, and OS maintenance. Self-managed services (EC2, self-hosted Kafka) require full lifecycle engineering.
2. **Access Patterns & Latency**: Does the workload require sub-millisecond memory caching (ElastiCache Redis), single-digit millisecond key-value lookups (DynamoDB), or complex relational SQL joins (Aurora)?
3. **Cost Model (Pay-per-use vs Provisioned Capacity)**: Serverless pay-per-request pricing is superior for unpredictable or bursty traffic; provisioned reserved instances or Savings Plans are vastly more cost-effective for 24/7 sustained baseline loads.
4. **Data Durability & Consistency Requirements**: Eventual consistency vs strict ACID transactional consistency; ephemeral scratch storage vs 11 9s durability.
5. **Ecosystem & Portability**: Native AWS proprietary services (DynamoDB, Step Functions) provide rapid velocity; open standards (PostgreSQL, Kafka, Kubernetes) preserve multi-cloud portability.

---

## 3. How It Works Internally

### 3.1 Compute Selection Flowchart

```
Start: What kind of workload are you running?
   |
   +--> Event-driven, bursty, short-lived (< 15 min), or microservice API?
   |      |-- YES --> Do you need custom kernel/OS, persistent local disk, or > 15 min runtime?
   |      |             |-- NO  --> **AWS Lambda** (Fastest scaling, pay-per-millisecond)
   |      |             |-- YES --> Continue below
   |      |-- NO
   |
   +--> Simple public containerized web service or REST API with minimal networking?
   |      |-- YES --> **AWS App Runner** (Zero infra ops, managed TLS & ALB)
   |      |-- NO
   |
   +--> Standard containerized microservice requiring VPC integration and task IAM isolation?
   |      |-- YES --> Do you have deep Kubernetes platform investment (Helm, ArgoCD, K8s ecosystem)?
   |      |             |-- YES --> **Amazon EKS** (Managed Kubernetes control plane)
   |      |             |-- NO  --> **Amazon ECS with AWS Fargate** (Default Production Microservices)
   |      |-- NO
   |
   +--> Need direct GPU access, Inf2 chips, host-level kernel tuning, or legacy OS?
          |-- YES --> **Amazon EC2** or **Amazon ECS with EC2 capacity**
```

### 3.2 Storage Selection Flowchart

```
Start: What type of storage access pattern is needed?
   |
   +--> Unstructured objects, files, backups, media, static assets, or big data lake?
   |      |-- YES --> **Amazon S3** (11 9s durability, HTTP REST API, multi-tier lifecycle)
   |      |-- NO
   |
   +--> Block storage attached to a single EC2 instance (OS root drive, high IOPS DB)?
   |      |-- YES --> **Amazon EBS** (gp3 for standard workloads, io2 Block Express for mission-critical DB)
   |      |-- NO
   |
   +--> Shared POSIX file system accessible simultaneously by hundreds of Linux EC2/Fargate tasks?
   |      |-- YES --> **Amazon EFS** (Elastic serverless NFSv4 file system)
   |      |-- NO
   |
   +--> High-performance Windows file share (SMB) or ultra-fast Lustre for HPC / ML training?
          |-- YES --> **Amazon FSx** (FSx for Windows File Server or FSx for Lustre)
```

### 3.3 Database Selection Flowchart

```
Start: What is the nature of your data and query patterns?
   |
   +--> Relational data requiring ACID transactions, complex multi-table JOINs, and SQL?
   |      |-- High-scale enterprise, autoscaling storage, global replication?
   |      |     |-- YES --> **Amazon Aurora** (PostgreSQL / MySQL compatible)
   |      |     |-- NO  --> **Amazon RDS** (PostgreSQL, MySQL, MariaDB, SQL Server, Oracle)
   |
   +--> Semi-structured or key-value data with known query access patterns, needing massive scale?
   |      |-- YES --> **Amazon DynamoDB** (Sub-10ms latency at any scale, Single-Table NoSQL)
   |      |-- NO
   |
   +--> Ultra-low latency (< 1ms) in-memory cache, session store, or pub/sub?
   |      |-- YES --> **Amazon ElastiCache** (Redis / Memcached) or **Amazon MemoryDB**
   |      |-- NO
   |
   +--> Graph data with complex relationship traversals (social networks, fraud rings)?
   |      |-- YES --> **Amazon Neptune** (Apache TinkerPop Gremlin / SPARQL / openCypher)
   |      |-- NO
   |
   +--> Time-series IoT telemetry, metrics, or sensor streams?
          |-- YES --> **Amazon Timestream**
```

### 3.4 Messaging & Asynchronous Integration Flowchart

```
Start: How should services communicate asynchronously?
   |
   +--> Point-to-point worker task decoupling, buffering, rate-leveling, with DLQ?
   |      |-- YES --> **Amazon SQS** (Pull-based queue, Standard or FIFO)
   |      |-- NO
   |
   +--> High-throughput 1-to-many broadcast fan-out to email, SMS, mobile push, or SQS?
   |      |-- YES --> **Amazon SNS** (Push-based pub/sub topic)
   |      |-- NO
   |
   +--> Event routing based on JSON payload inspection, SaaS events, cross-account events?
   |      |-- YES --> **Amazon EventBridge** (Schema-aware event bus, declarative rules)
   |      |-- NO
   |
   +--> Real-time sequential time-series stream ingestion, video streams, or multiple readers?
   |      |-- YES --> **Amazon Kinesis Data Streams** or **Amazon Managed Streaming for Apache Kafka (MSK)**
   |      |-- NO
   |
   +--> Complex multi-step business transactions, sagas, branching logic, human approvals?
          |-- YES --> **AWS Step Functions** (Orchestrated state machine)
```

---

## 4. Syntax & Usage

### 4.1 Comparative Architectural Pattern Definition (IaC)

Below is an architectural configuration illustrating how complementary services are combined in a real-world enterprise pattern (API Gateway + Lambda + DynamoDB + SQS + SNS):

```hcl
# Ingress Layer: API Gateway HTTP API
resource "aws_apigatewayv2_api" "ingress" {
  name          = "order-ingestion-api"
  protocol_type = "HTTP"
}

# Synchronous Processing: AWS Lambda
resource "aws_lambda_function" "order_handler" {
  function_name = "order-ingestion-handler"
  runtime       = "nodejs20.x"
  architectures = ["arm64"]
  handler       = "index.handler"
  role          = aws_iam_role.lambda_exec.arn
  filename      = "handler.zip"
}

# Persistence: Amazon DynamoDB
resource "aws_dynamodb_table" "orders" {
  name         = "EnterpriseOrders"
  billing_mode = "PAY_PER_REQUEST"
  hash_key     = "PK"
  range_key    = "SK"

  attribute {
    name = "PK"
    type = "S"
  }
  attribute {
    name = "SK"
    type = "S"
  }
}

# Asynchronous Decoupling: Amazon SQS Queue with DLQ
resource "aws_sqs_queue" "order_dlq" {
  name = "order-processing-dlq"
}

resource "aws_sqs_queue" "order_worker_queue" {
  name                      = "order-processing-worker-queue"
  visibility_timeout_seconds = 60
  redrive_policy = jsonencode({
    deadLetterTargetArn = aws_sqs_queue.order_dlq.arn
    maxReceiveCount     = 3
  })
}
```

---

## 5. Practical Examples

### Scenario 1: E-Commerce Checkout Pipeline (Right Services Selected)
- **Problem**: Handle 10,000 orders/minute during Black Friday sales. Prevent database locking, ensure payments are processed once without duplication, and send email confirmations.
- **Architectural Solution**:
  - **Ingress**: Amazon CloudFront + AWS WAF + API Gateway HTTP API.
  - **Compute**: AWS Lambda (Node.js ARM64) to validate order request and check inventory in **Amazon DynamoDB** with conditional writes.
  - **Asynchronous Queue**: Write order message to **Amazon SQS FIFO Queue** to guarantee exactly-once processing order.
  - **Fulfillment Worker**: **Amazon ECS Fargate** workers consume SQS messages in batches, invoke payment gateway, and update order status.
  - **Notification**: Publish event to **Amazon SNS Topic** fanned out to **Amazon SES** (email customer) and analytics S3 data lake.

### Scenario 2: Legacy Monolithic Enterprise ERP Migration
- **Problem**: 15-year-old Java EE application backed by a 4TB Microsoft SQL Server database. The business must exit the on-premise data center within 3 months.
- **Architectural Solution**:
  - **Strategy**: Rehost / Replatform.
  - **Compute**: **Amazon EC2** running Windows Server (right-sized based on AWS Application Discovery Service metrics).
  - **Storage**: **Amazon EBS gp3** volumes for high-throughput OS disks, attached to an **Amazon FSx for Windows File Server** multi-AZ SMB share for shared legacy file attachments.
  - **Database**: **Amazon RDS for SQL Server (Multi-AZ)** to eliminate manual database mirroring and patching while preserving 100% T-SQL compatibility.

### Scenario 3: Real-Time Connected Vehicle IoT Telemetry
- **Problem**: 500,000 IoT vehicle sensors reporting GPS and engine diagnostic packets every 5 seconds (100,000 packets/sec). Needs sub-minute anomaly alerts and historical aggregation.
- **Architectural Solution**:
  - **Ingestion**: **AWS IoT Core** broker ingesting MQTT messages authenticated by X.509 device certificates.
  - **Streaming Pipeline**: IoT Core Basic Ingest rules forward raw streams directly to **Amazon Kinesis Data Streams**.
  - **Real-Time Anomaly Detection**: **Amazon Managed Service for Apache Flink** reads Kinesis streams and calculates sliding window temperature averages.
  - **Long-Term Storage**: **Amazon Kinesis Data Firehose** converts streams into Apache Parquet format and writes to **Amazon S3** (Bronze Data Lake).
  - **Ad-Hoc Querying**: **Amazon Athena** queries historical vehicle telemetry directly from S3.

### Scenario 4: Global Mobile Game Leaderboard
- **Problem**: 2 million active mobile players competing globally. Leaderboards must update instantly in real-time, with global sub-50ms read/write latencies.
- **Architectural Solution**:
  - **Ingress**: **AWS Global Accelerator** terminating TCP/UDP traffic at edge locations to eliminate internet routing jitter.
  - **Caching / Hot Leaderboard**: **Amazon ElastiCache for Redis** utilizing Redis Sorted Sets (`ZADD`, `ZRANK`) for real-time sub-millisecond ranking queries.
  - **Persistent Backing Store**: **Amazon DynamoDB Global Tables** replicating user profiles and historical score archives across `us-east-1` and `eu-west-1`.

### Scenario 5: Financial Fraud Detection Pipeline
- **Problem**: Evaluate credit card transactions for fraud within a 200ms latency budget. Requires evaluating identity, complex graph relationships (shared phone numbers or addresses across accounts), and historical velocity.
- **Architectural Solution**:
  - **Edge Ingress**: Application Load Balancer with mTLS.
  - **Compute**: Amazon ECS Fargate container running Go microservice.
  - **Graph Evaluation**: **Amazon Neptune** querying identity clusters via openCypher within 15ms.
  - **Decision Engine**: **Amazon SageMaker Real-Time Endpoint** scoring fraud risk using a boosted tree model.

---

## 6. Best Practices

1. **Default to Managed & Serverless Before Choosing Infrastructure**: Start with Lambda, Fargate, DynamoDB, and S3. Only move down the abstraction ladder to EC2 or self-hosted databases when specific technical constraints (custom kernels, licensing, extreme sustained throughput) demand it.
2. **Align Service Selection with Team Operational Capacity**: A 5-person startup should rarely operate a raw Kubernetes cluster (EKS) or self-hosted Kafka (EC2); the operational maintenance will starve product feature delivery.
3. **Decouple Synchronous APIs with Asynchronous Buffers**: Never let a frontend web service write directly to a slow third-party API or relational database inside a synchronous request thread; place SQS or EventBridge in the middle.
4. **Use Multi-AZ for Production High Availability**: Always select Multi-AZ for RDS, Aurora, and ECS/EC2 deployments to survive data center-level hardware failures.
5. **Right-Size Pricing Models to Traffic Profiles**: Combine Savings Plans for steady baseline loads (e.g., 24/7 web fleets) with on-demand or Spot compute for variable bursts.
6. **Enforce Single-Table Design for DynamoDB**: Avoid treating DynamoDB like a relational SQL database; model access patterns upfront to leverage single-table design with composite keys and GSIs.

---

## 7. Common Mistakes

| Mistake | Why It Hurts | Correct Approach |
|:---|:---|:---|
| **Choosing RDS/Postgres for millions of simple KV lookups** | Relational overhead, connection limits, and row locking cause scaling bottlenecks and high costs. | Use **Amazon DynamoDB** for simple key-value and single-entity queries at scale. |
| **Using SQS instead of Kinesis for ordered streams with multiple consumers** | SQS FIFO queues cannot fan out independently to 5 different analytical consumers reading at different speeds. | Use **Amazon Kinesis Data Streams** or **Amazon MSK** for multi-reader streaming. |
| **Selecting EKS without a dedicated platform engineering team** | Constant Kubernetes control plane upgrades, CNI patching, and ingress management drain product engineers. | Use **Amazon ECS with AWS Fargate** for zero-maintenance container orchestration. |
| **Using Amazon EFS for high-IOPS transactional databases** | NFS network latency and distributed locking make EFS poorly suited for low-latency relational database storage. | Use **Amazon EBS (io2 Block Express or gp3)** mounted directly to EC2. |
| **Placing Lambda inside VPC to talk to DynamoDB or S3** | Adds unnecessary ENI allocation and complexity when communicating with public AWS endpoints. | Keep Lambda outside VPC unless it needs to access private resources (RDS, internal ALB). |
| **Using API Gateway REST API instead of HTTP API for basic APIs** | REST APIs cost \$3.50/M requests compared to \$1.00/M for HTTP APIs and introduce 15ms higher latency overhead. | Default to **API Gateway HTTP APIs** unless REST-specific features (WAF on APIGW, usage plans) are required. |

---

## 8. Performance, Security & Cost Notes

### Performance Matrix
- **In-Memory Caching (ElastiCache / MemoryDB)**: Sub-millisecond latency (< 1 ms).
- **NoSQL KV (DynamoDB)**: Single-digit millisecond latency (2 - 8 ms).
- **Relational DB (Amazon Aurora)**: 5 - 25 ms depending on query complexity.
- **Object Storage (Amazon S3)**: 50 - 150 ms first-byte latency.

### Security Baseline
- **Least-Privilege IAM Execution Roles**: Every compute service (Lambda, ECS Task, EC2) must run under its own dedicated IAM role without wildcard permissions.
- **Zero Static Credentials**: Enforce IAM SigV4 and AWS Secrets Manager integration.

### Cost Optimization Hierarchy
- **Zero Idle Cost**: AWS Lambda, DynamoDB On-Demand, S3, API Gateway HTTP APIs.
- **Predictable Low-Cost Baseline**: ECS Fargate with Fargate Spot (up to 70% off) + Compute Savings Plans.
- **High-Volume Sustained Cost**: EC2 Reserved Instances / Savings Plans with Graviton processors.

---

## 9. Comparison Table

| Category | Option A | Option B | Selection Criteria (Choose A when...) |
|:---|:---|:---|:---|
| **Compute** | AWS Lambda | Amazon ECS Fargate | Workload is event-driven, short-lived (<15m), and scales to zero. |
| **Containers** | Amazon ECS | Amazon EKS | You want simple container orchestration without Kubernetes operational overhead. |
| **Storage** | Amazon EBS | Amazon EFS | You need high-IOPS local block storage for a single instance (OS disk, database). |
| **Database** | Amazon DynamoDB | Amazon Aurora | Access patterns are predictable, scaling requirements are massive, and joins are unneeded. |
| **Ingress** | Application Load Balancer | API Gateway | You are routing HTTP traffic to persistent ECS containers/EC2 with high sustained volume. |
| **Messaging** | Amazon SQS | Amazon EventBridge | You need a durable pull-based queue for load leveling and worker fleet consumption. |
| **Pub/Sub** | Amazon SNS | Amazon Kinesis | You need simple topic broadcast fan-out rather than sequential sharded stream storage. |

---

## 10. Interview Questions

<details>
<summary><strong>1. How do you decide between Amazon DynamoDB and Amazon Aurora PostgreSQL for a new project?</strong></summary>

Evaluate data relationships, query patterns, and scale:
- Choose **Amazon Aurora PostgreSQL** if: The domain model requires complex relational joins, arbitrary ad-hoc filtering across multiple dimensions, strict multi-record ACID transactions, foreign keys, or the schema is rapidly evolving without known fixed query patterns.
- Choose **Amazon DynamoDB** if: The query access patterns are well-defined upfront (e.g., fetch user by ID, get orders for customer), scale exceeds thousands of writes per second, horizontal auto-scaling without connection pool limits is required, and single-digit millisecond latency is mandatory at any scale.
</details>

<details>
<summary><strong>2. When should an architect select Amazon Kinesis Data Streams over Amazon SQS FIFO?</strong></summary>

Choose **Amazon Kinesis Data Streams** when:
1. Multiple independent consumer applications need to read the identical stream of data simultaneously at their own independent rates (e.g., billing, fraud detection, and analytics all tailing the same transaction stream).
2. Data must be replayed from historical offsets (Kinesis retains data for up to 365 days).
3. Ultra-high sequential throughput (megabytes per second per shard) is required.
Choose **Amazon SQS FIFO** when:
1. Each message must be processed by only one worker and deleted upon success.
2. The processing rate is discrete, and messages should automatically retry or land in a DLQ upon failure.
</details>

<details>
<summary><strong>3. What are the key architectural differences between an Application Load Balancer (ALB) and a Network Load Balancer (NLB)?</strong></summary>

- **Application Load Balancer (ALB)**: Operates at Layer 7 (Application). Inspects HTTP/HTTPS headers, paths, query strings, and host headers to make routing decisions. Supports TLS termination, redirects, mTLS, Cognito/OIDC authentication, and AWS WAF integration. Latency is typically 2-5 ms.
- **Network Load Balancer (NLB)**: Operates at Layer 4 (Transport). Routes raw TCP, UDP, and TLS connections at ultra-high throughput (millions of requests per second) with sub-millisecond latency. Provides static Anycast IP addresses and client IP preservation by default, but cannot inspect HTTP application payloads or route by URL path.
</details>

<details>
<summary><strong>4. Under what conditions is AWS App Runner a better choice than ECS Fargate?</strong></summary>

AWS App Runner is superior when:
1. The workload is a standard web application, API, or frontend service listening on HTTP/HTTPS.
2. The engineering team wants zero infrastructure code (no custom ALBs, Target Groups, Route 53 aliases, or VPC subnet configuration required).
3. Automatic TLS provisioning, custom domain mapping, and automatic scaling based on concurrent HTTP requests are desired out of the box.
ECS Fargate is required if the service needs background queue processing without HTTP listeners, complex multi-container tasks, persistent EFS mounts, or advanced VPC ingress security controls.
</details>

<details>
<summary><strong>5. How do you choose between Amazon S3 and Amazon EFS for storing application files?</strong></summary>

- Choose **Amazon S3** when: Storing media, documents, logs, backups, or unstructured data accessed via standard HTTP REST APIs (`GET`, `PUT`). S3 offers 11 9s durability, massive parallel throughput, global CDN edge caching, and cost-effective tiered storage.
- Choose **Amazon EFS** when: Multiple EC2 instances or container tasks require a shared, POSIX-compliant file system with standard file system directory semantics (`open`, `read`, `write`, `seek`, file locking) mounted directly to the operating system over NFSv4.
</details>

<details>
<summary><strong>6. What factors dictate using Amazon EventBridge instead of Amazon SNS?</strong></summary>

Use **Amazon EventBridge** when:
1. Routing decisions depend on deep inspection of the JSON event content (e.g., route only if `amount > 500` and `status == "APPROVED"`).
2. Integrating natively with external SaaS applications (Salesforce, Zendesk, Datadog) or AWS service state change events.
3. Event schema discovery and contract validation via Schema Registry are required.
Use **Amazon SNS** when:
1. High-throughput, low-latency topic broadcast fan-out is needed directly to hundreds of thousands of SQS queues, mobile push devices, SMS, or webhook endpoints.
</details>

<details>
<summary><strong>7. When would you choose Amazon FSx over Amazon EFS?</strong></summary>

Choose **Amazon FSx** when:
1. You require native Windows Server compatibility (NTFS, Active Directory integration, SMB protocol) via **FSx for Windows File Server**.
2. You require ultra-high-performance compute file storage with millions of IOPS and hundreds of GB/s throughput for machine learning training, rendering, or HPC via **FSx for Lustre**.
3. You are migrating existing NetApp ONTAP enterprise storage arrays via **FSx for NetApp ONTAP**.
EFS is the default choice for standard Linux POSIX shared file systems.
</details>

<details>
<summary><strong>8. How does the pricing model impact the choice between AWS Lambda and Amazon ECS Fargate?</strong></summary>

- **AWS Lambda** charges per invocation and execution duration (GB-seconds). It scales to absolute zero, meaning costs are \$0.00 when there is no traffic. However, for a service handling sustained, constant high traffic (e.g., 500 requests per second 24/7), paying per millisecond becomes significantly more expensive than running persistent containers.
- **Amazon ECS Fargate** charges per vCPU-hour and GB-hour for provisioned container capacity. It incurs a baseline cost even when idle, but provides a vastly lower unit cost per request under high, continuous, predictable workloads—especially when combined with Compute Savings Plans and Fargate Spot.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Challenge 1: Map Business Requirements to AWS Services</strong></summary>

**Goal**: Given a scenario requiring real-time sub-10ms user profile lookup, a 30-day IoT telemetry archive, and fan-out notifications to mobile users, produce a formal service selection architecture document.
**Verification**: Verify that the proposed architecture utilizes DynamoDB, S3 with lifecycle policies, and SNS Mobile Push.
</details>

<details>
<summary><strong>Challenge 2: Deploy and Benchmark an HTTP API Gateway vs REST API Gateway</strong></summary>

**Goal**: Deploy both an HTTP API and a REST API in front of the identical Lambda function using Terraform.
**Verification**: Use `curl` or Apache Bench to measure response latency and observe the 10-15ms latency improvement on the HTTP API.
</details>

<details>
<summary><strong>Challenge 3: Compare Cost of Lambda vs ECS Fargate for Sustained Traffic</strong></summary>

**Goal**: Calculate using AWS Pricing Calculator the monthly cost of 100 million requests/month (average 100ms duration, 512MB RAM) on Lambda versus 2 persistent Fargate tasks (0.5 vCPU, 1 GB RAM).
**Verification**: Document the break-even point in requests per second where Fargate becomes cheaper than Lambda.
</details>

<details>
<summary><strong>Challenge 4: Implement SQS FIFO vs Standard Queue Message Ordering Test</strong></summary>

**Goal**: Create one Standard SQS queue and one FIFO SQS queue. Publish 20 sequentially numbered messages concurrently across multiple threads.
**Verification**: Receive messages and demonstrate that the FIFO queue guarantees exact sequential delivery, while the standard queue exhibits out-of-order delivery.
</details>

<details>
<summary><strong>Challenge 5: Build a Multi-Consumer Stream using Amazon Kinesis</strong></summary>

**Goal**: Deploy an Amazon Kinesis Data Stream with two distinct consumer Lambda functions: one logging audit events and one calculating running transaction sums.
**Verification**: Put records onto the stream and verify that both Lambda consumers process the exact same sequence independently.
</details>

<details>
<summary><strong>Challenge 6: Configure an ALB with Path-Based Routing to Heterogeneous Targets</strong></summary>

**Goal**: Provision an ALB that routes `/api/*` to an ECS Fargate service and `/static/*` to an S3 bucket or Lambda function.
**Verification**: Send HTTP requests to both paths and verify the distinct backend responses.
</details>

---

## 12. Summary / Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
|                                AWS SERVICE SELECTION CHEAT SHEET                                   |
+----------------------------------------------------------------------------------------------------+
| Architecture Domain | Default Enterprise Choice | Select Alternate When...                         |
+---------------------+---------------------------+--------------------------------------------------+
| Compute             | ECS with AWS Fargate      | Lambda (Event/Bursty); EKS (K8s platform orgs)   |
| Block Storage       | Amazon EBS gp3            | io2 Block Express (Extreme mission-critical IOPS)|
| Shared File Storage | Amazon EFS (Linux NFS)    | FSx (Windows SMB / Lustre HPC / NetApp ONTAP)    |
| Object Storage      | Amazon S3 Standard        | S3 Glacier Deep Archive (Long-term compliance)   |
| Relational Database | Amazon Aurora             | Amazon RDS (Standard engines, custom Oracle/MSSQL|
| NoSQL Database      | Amazon DynamoDB           | DocumentDB (MongoDB API); Neptune (Graph data)   |
| In-Memory Cache     | Amazon ElastiCache Redis  | Amazon MemoryDB (Durable in-memory transactional)|
| Worker Queue        | Amazon SQS                | Amazon Kinesis / MSK (Multi-reader streams)      |
| Event Router        | Amazon EventBridge        | Amazon SNS (High-speed notification broadcast)   |
| Ingress Load Balance| Application Load Balancer | Network Load Balancer (Raw TCP/UDP, static IPs)  |
+----------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [AWS Well-Architected Framework: Reliability, Performance & Cost Pillars](https://aws.amazon.com/architecture/well-architected/)
- [AWS Prescriptive Guidance: Application Architecture Patterns](https://docs.aws.amazon.com/prescriptive-guidance/latest/patterns/architecture.html)
- [Amazon DynamoDB: Single-Table Design and Best Practices](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/best-practices.html)
- [AWS Modern Application Design Patterns](https://aws.amazon.com/modern-apps/)
- [AWS Architecture Center & Reference Architectures](https://aws.amazon.com/architecture/)
