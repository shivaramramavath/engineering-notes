---
topic: "AWS Limits, Quotas & Default Configurations Cheatsheet"
module: "10-cheatsheets"
track: "aws"
difficulty: "reference"
last-tested: "2026-11"
tags:
  - "cheatsheet"
  - "limits"
  - "quotas"
  - "defaults"
  - "service-quotas"
---

# AWS Limits, Quotas & Default Configurations Cheatsheet

A high-density reference card of essential AWS default configurations, hard limits, and soft quotas across compute, storage, networking, database, and messaging services.

---

## 1. Compute & Serverless Limits

| Service / Resource | Limit / Quota | Type | Notes & Guidance |
|:---|:---|:---|:---|
| **Lambda Execution Timeout** | **15 minutes (900 seconds)** | **Hard** | Jobs exceeding 15m must use ECS Fargate or AWS Batch |
| **Lambda Memory Allocation** | **128 MB to 10,240 MB (10 GB)** | **Hard** | Configurable in 1 MB increments; CPU scales linearly |
| **Lambda Ephemeral Disk (`/tmp`)** | **512 MB to 10,240 MB (10 GB)**| **Hard** | Configurable scratch disk storage |
| **Lambda Container Image Size** | **10 GB** | **Hard** | Packaged and pulled from Amazon ECR |
| **Lambda Zip Deployment Package** | **50 MB (zipped), 250 MB (unzipped)**| **Hard** | Use container packaging or Lambda Layers if larger |
| **Lambda Regional Concurrency** | **1,000 concurrent executions**| **Soft** | Account baseline; adjustable via Service Quotas |
| **ECS Task Definition Resources** | Min: 0.25 vCPU / 0.5 GB; Max: 16 vCPU / 120 GB | **Hard** | Fixed valid combinations on AWS Fargate |
| **EC2 Spot Interruption Notice** | **2 minutes (120 seconds)** | **Hard** | Delivered via EventBridge and instance metadata |

---

## 2. Storage & Database Limits

| Service / Resource | Limit / Quota | Type | Notes & Guidance |
|:---|:---|:---|:---|
| **S3 Maximum Object Size** | **5 TB** | **Hard** | Individual object upper ceiling |
| **S3 Single `PUT` Upload Limit** | **5 GB** | **Hard** | Objects > 5 GB **must** use Multipart Upload (recommended > 100 MB)|
| **S3 Minimum Part Size (Multipart)**| **5 MB** (Except last part) | **Hard** | Minimum individual split size |
| **DynamoDB Maximum Item Size** | **400 KB** | **Hard** | Includes attribute names and binary values |
| **DynamoDB Physical Partition Size**| **10 GB** | **Hard** | DynamoDB auto-splits partitions when exceeded |
| **DynamoDB Partition Throughput** | **1,000 WCU / 3,000 RCU** | **Hard** | Per physical partition limit |
| **DynamoDB Batch Operations** | Max **100 items (16 MB)** for reads; Max **25 items (16 MB)** for writes | **Hard** | `BatchGetItem` / `BatchWriteItem` limits |
| **RDS Stopped Instance Auto-Restart**| **7 Days** | **Hard** | A stopped RDS instance auto-starts after 7 days |
| **EBS gp3 Volume Size** | **1 GB to 16 TB** | **Hard** | Maximum single volume size |
| **EBS gp3 Max IOPS / Throughput** | **16,000 IOPS / 1,000 MB/s** | **Hard** | Baseline is 3,000 IOPS / 125 MB/s |
| **EBS io2 Block Express Max IOPS** | **256,000 IOPS / 4,000 MB/s**| **Hard** | Ultra-high performance database tier |

---

## 3. Networking & Traffic Ingress Limits

| Service / Resource | Limit / Quota | Type | Notes & Guidance |
|:---|:---|:---|:---|
| **VPC CIDR Prefix Range** | `/16` (65,536 IPs) to `/28` (16 IPs) | **Hard** | Standard IPv4 VPC bounds |
| **Reserved IPs per Subnet** | **5 IP Addresses** (`.0, .1, .2, .3, .255`)| **Hard** | Dedicated to AWS VPC router, DNS, broadcast |
| **VPCs per Region** | **5** | **Soft** | Adjustable via Service Quotas |
| **Subnets per VPC** | **200** | **Soft** | Adjustable |
| **Security Groups per Network Interface**| **5** | **Soft** | Can be increased to 16 (lowering rules per SG) |
| **Inbound/Outbound Rules per SG**| **60** | **Soft** | Max 1,000 across all SGs per ENI |
| **ALB Idle Timeout** | **60 seconds** | Default | Adjustable from 1 to 4,000 seconds |
| **API Gateway Integration Timeout** | **29 seconds** (REST & HTTP APIs)| **Hard** | Backend Lambda/HTTP must respond within 29s |
| **API Gateway Payload Size** | **10 MB** | **Hard** | Maximum request/response body size |
| **CloudFront ACM Certificate Region**| **`us-east-1` (N. Virginia)** | **Hard** | Edge distributions require certificates in us-east-1 |

---

## 4. Messaging & Application Integration Limits

| Service / Resource | Limit / Quota | Type | Notes & Guidance |
|:---|:---|:---|:---|
| **SQS Standard Message Size** | **256 KB** | **Hard** | Up to 2 GB using Amazon S3 Extended Client |
| **SQS Visibility Timeout** | Default: **30 seconds**; Max: **12 hours**| **Hard** | Configurable per queue or per message |
| **SQS Message Retention Period** | Min: **1 minute**; Default: **4 days**; Max: **14 days**| **Hard** | Maximum time messages persist in queue |
| **SQS Standard FIFO Throughput** | **300 msg/sec** (3,000 with batching) | Default | Up to 70,000 msg/s with High Throughput Mode |
| **SNS Message Payload Size** | **256 KB** | **Hard** | Standard topic payload limit |
| **EventBridge Custom Bus Payload** | **256 KB** | **Hard** | Maximum size per `PutEvents` entry |
| **EventBridge Default Throughput** | **10,000 events/second** per bus | **Soft** | Adjustable via quota increase |
| **Step Functions Standard Duration**| **Up to 1 year** | **Hard** | Longest running serverless workflow |
| **Step Functions Express Duration** | **Up to 5 minutes** | **Hard** | High-throughput streaming state machine |

---

## 5. Security & Operations Quotas

| Service / Resource | Limit / Quota | Type | Notes & Guidance |
|:---|:---|:---|:---|
| **AWS KMS Key Deletion Waiting Period**| **7 to 30 Days** | **Hard** | KMS keys cannot be deleted immediately |
| **Secrets Manager Secret Size** | **64 KB** | **Hard** | Maximum secret string size |
| **SSM Parameter Store (Standard)** | **4 KB** | **Hard** | Free tier; Advanced parameters allow up to 8 KB |
| **CloudFormation Resources per Stack**| **500 Resources** | **Hard** | Split larger architectures using Nested Stacks |
| **CloudFormation Template Body Size** | **51,200 bytes** (direct); **1 MB** (via S3)| **Hard** | Upload template to S3 if exceeding 51.2 KB |
| **CloudTrail Default Event History** | **90 Days** | Default | Does not replace a Trail; create an S3 trail for compliance |
