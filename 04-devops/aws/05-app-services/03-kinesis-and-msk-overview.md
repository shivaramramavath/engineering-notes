# Real-Time Streaming: Amazon Kinesis & Amazon Managed Streaming for Apache Kafka (MSK)

> **Module**: `05-app-services`  
> **Topic**: Real-Time Streaming Ingestion, Shard Mechanics, Firehose Delivery & Managed Apache Kafka  
> **Target Audience**: Data Engineers, Cloud Architects, Distributed Streaming Developers  
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

Traditional message queues like Amazon SQS are designed for asynchronous transactional job processing: when a worker processes a message, that message is permanently deleted from the queue. However, modern big data analytics, IoT sensor telemetry, clickstream tracking, and financial fraud detection systems require **event streaming**: continuous, high-throughput, ordered sequences of events that can be replayed, analyzed concurrently by multiple independent processing engines, and retained for long-term historical analysis.

AWS provides two primary real-time streaming platforms:
1. **Amazon Kinesis**: A suite of fully managed, cloud-native serverless streaming services:
   - **Kinesis Data Streams (KDS)**: Massively scalable, durable real-time data streaming service with sub-second processing latency.
   - **Kinesis Data Firehose (Amazon Data Firehose)**: Zero-code extraction, transformation, and loading (ETL) service that buffers and loads streams directly into Amazon S3, Amazon Redshift, OpenSearch, and third-party tools like Splunk.
2. **Amazon Managed Streaming for Apache Kafka (Amazon MSK)**: A fully managed service that runs open-source Apache Kafka clusters, allowing enterprises to migrate existing Kafka pipelines to AWS with zero code changes while eliminating the operational overhead of managing ZooKeeper/KRaft, brokers, and storage volumes.

---

## 2. Core Concepts

### 2.1 Amazon Kinesis Data Streams (KDS)
- **Shards**: The base throughput unit of a stream.
  - Ingest capacity: **1 MB/second** or **1,000 records/second** write.
  - Egress capacity: **2 MB/second** read shared across all standard consumers.
- **Partition Key**: Used to route records to specific shards via MD5 hashing. Records with the same partition key land on the same shard and are strictly ordered.
- **Capacity Modes**:
  - *Provisioned Mode*: Explicitly specify the number of shards; manual or auto-scaled shard splitting/merging.
  - *On-Demand Mode*: Automatically scales shard capacity up and down to accommodate unpredictable traffic spikes without capacity planning.
- **Data Retention**: Default 24 hours; can be extended up to **365 days** (replaying data at any historical offset).
- **Enhanced Fan-Out (EFO)**: Dedicated 2 MB/sec read pipe per consumer per shard using HTTP/2 server push, lowering read latency to ~70ms and eliminating consumer throughput contention.

### 2.2 Amazon Data Firehose (Kinesis Data Firehose)
- **Zero-Code Delivery**: Captures and automatically buffers streaming records, writing them into S3, Redshift, OpenSearch, or HTTP endpoints.
- **Buffering Hints**: Flushes data based on buffer size (1 MB to 128 MB) or buffer interval (60 to 900 seconds), whichever condition is met first.
- **Inline Data Transformation**: Invokes an AWS Lambda function synchronously to transform, enrich, or decrypt incoming records before writing to destinations.
- **Data Format Conversion**: Automatically converts JSON records into columnar formats (**Apache Parquet** or **Apache ORC**) to optimize Athena and analytics query performance.

### 2.3 Amazon Managed Streaming for Apache Kafka (Amazon MSK)
- **Brokers**: EC2 compute instances running the Kafka broker process in multi-AZ private subnets.
- **Topics & Partitions**: Kafka topics are divided into partitions distributed across brokers for parallel consumption.
- **Consumer Groups**: Independent fleets of workers coordinating via Kafka consumer group protocols to consume partition offsets.
- **MSK Serverless**: Automatically provisions and scales Kafka compute and storage without broker management, billed per cluster hour and data throughput.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                        KINESIS DATA STREAMS ARCHITECTURE & SHARD MECHANICS                        |
|                                                                                                   |
|  [Producers: IoT Devices, Web Servers, Clickstream Trackers]                                      |
|         |                                                                                         |
|         v (PutRecords: Payload + PartitionKey)                                                    |
|  +---------------------------------------------------------------------------------------------+  |
|  | AMAZON KINESIS DATA STREAM                                                                  |  |
|  |                                                                                             |  |
|  |   MD5(PartitionKey) mapped across 128-bit hash ring:                                        |  |
|  |                                                                                             |  |
|  |   Shard 1 [Hash: 0x0000... to 0x5555...] -> 1 MB/s Ingest | 2 MB/s Egress                   |  |
|  |   Shard 2 [Hash: 0x5556... to 0xAAAA...] -> 1 MB/s Ingest | 2 MB/s Egress                   |  |
|  |   Shard 3 [Hash: 0xAAAB... to 0xFFFF...] -> 1 MB/s Ingest | 2 MB/s Egress                   |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                              |                                          |
|         | (Standard Read: Polling ~200ms)              | (Enhanced Fan-Out: HTTP/2 Push ~70ms)    |
|         v                                              v                                          |
|  +-----------------------------+               +-----------------------------------------------+  |
|  | CONSUMER 1: Real-time Fraud |               | CONSUMER 2: Apache Flink Stream Analytics     |  |
|  | AWS Lambda (Batch size 100) |               | (Kinesis Data Analytics dedicated 2 MB/s pipe)|  |
|  +-----------------------------+               +-----------------------------------------------+  |
|                                                                                                   |
|         +---> Stream Branch to Kinesis Data Firehose                                              |
|               | (Inline Lambda JSON to Parquet conversion)                                        |
|               v                                                                                   |
|         +--------------------------------------------------------------------------------------+  |
|         | AMAZON S3 ANALYTICS DATA LAKE (s3://company-lake/year=2026/month=11/)                |  |
|         | Queryable immediately via Amazon Athena Serverless SQL                               |  |
|         +--------------------------------------------------------------------------------------+  |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Creating a Kinesis Data Stream via AWS CLI

```bash
# 1. Create Kinesis Stream in On-Demand capacity mode
aws kinesis create-stream \
    --stream-name "clickstream-events" \
    --stream-mode-details StreamMode=ON_DEMAND

# 2. Put record into the stream with partition key
aws kinesis put-record \
    --stream-name "clickstream-events" \
    --partition-key "user-10294" \
    --data "eyJldmVudCI6ICJjbGljayIsICJwYWdlIjogIi9jaGVja291dCJ9"

# 3. Read record using Shard Iterator
SHARD_ITERATOR=$(aws kinesis get-shard-iterator \
    --stream-name "clickstream-events" \
    --shard-id "shardId-000000000000" \
    --shard-iterator-type TRIM_HORIZON \
    --query "ShardIterator" --output text)

aws kinesis get-records --shard-iterator "$SHARD_ITERATOR"
```

---

## 5. Practical Examples

### 5.1 Kinesis Data Stream with Enhanced Fan-Out and Lambda Consumer (Terraform)

```hcl
# Kinesis Data Stream
resource "aws_kinesis_stream" "telemetry" {
  name             = "iot-telemetry-stream"
  retention_period = 48 # 48 hours retention
  stream_mode_details {
    stream_mode = "ON_DEMAND"
  }

  encryption_type = "KMS"
  kms_key_id      = "alias/aws/kinesis"

  tags = {
    Environment = "production"
  }
}

# Enhanced Fan-Out Dedicated Consumer Pipe
resource "aws_kinesis_stream_consumer" "analytics_efo" {
  name       = "realtime-analytics-consumer"
  stream_arn = aws_kinesis_stream.telemetry.arn
}

# Lambda Event Source Mapping using EFO
resource "aws_lambda_event_source_mapping" "kinesis_efo_mapping" {
  event_source_arn  = aws_kinesis_stream_consumer.analytics_efo.arn
  function_name     = var.consumer_lambda_arn
  starting_position = "LATEST"
  batch_size        = 100

  # Parallelization factor: process up to 5 concurrent batches per shard
  parallelization_factor = 5

  # Bisect batch on error: isolate poison pill records automatically
  bisect_batch_on_function_error = true

  destination_config_on_failure {
    destination_arn = var.dlq_sqs_arn
  }
}
```

### 5.2 Kinesis Data Firehose Delivering Parquet to S3 (Terraform)

```hcl
resource "aws_kinesis_firehose_delivery_stream" "s3_lake" {
  name        = "firehose-to-s3-lake"
  destination = "extended_s3"

  extended_s3_configuration {
    role_arn   = var.firehose_role_arn
    bucket_arn = var.s3_lake_bucket_arn
    prefix     = "telemetry/year=!{timestamp:yyyy}/month=!{timestamp:MM}/day=!{timestamp:dd}/"
    error_output_prefix = "telemetry-errors/!{firehose:error-output-type}/"

    buffer_size     = 64 # 64 MB buffer
    buffer_interval = 300 # 5 minutes

    # Enable JSON to Parquet columnar conversion
    data_format_conversion_configuration {
      input_format_configuration {
        deserializer {
          open_x_json_ser_de {}
        }
      }

      output_format_configuration {
        serializer {
          parquet_ser_de {}
        }
      }

      schema_configuration {
        database_name = var.glue_database_name
        table_name    = var.glue_table_name
        role_arn      = var.firehose_role_arn
      }
    }
  }
}
```

### 5.3 High-Throughput Python Producer with Partition Key Hashing

```python
"""
Kinesis Producer demonstrating batch ingestion and partition key distribution.
"""
import boto3
import json
import uuid

kinesis = boto3.client("kinesis", region_name="us-east-1")
STREAM_NAME = "iot-telemetry-stream"

def send_batch_records(sensor_events: list):
    records = []
    for event in sensor_events:
        # Use high-cardinality deviceId as partition key for uniform shard distribution
        device_id = event["deviceId"]
        payload_bytes = json.dumps(event).encode("utf-8")

        records.append({
            "Data": payload_bytes,
            "PartitionKey": device_id
        })

    # PutRecords accepts up to 500 records or 5 MB per batch
    response = kinesis.put_records(
        StreamName=STREAM_NAME,
        Records=records
    )

    failed_record_count = response.get("FailedRecordCount", 0)
    if failed_record_count > 0:
        print(f"Warning: {failed_record_count} records failed due to throttling. Retrying...")
```

### 5.4 Amazon MSK Serverless Cluster Definition (Terraform)

```hcl
resource "aws_msk_serverless_cluster" "kafka" {
  cluster_name = "corp-msk-serverless"

  vpc_config {
    subnet_ids         = var.private_subnet_ids
    security_group_ids = [aws_security_group.msk_sg.id]
  }

  client_authentication {
    sasl {
      iam {
        enabled = true # Native AWS IAM authentication
      }
    }
  }
}
```

---

## 6. Best Practices

1. **Use High-Cardinality Partition Keys**: Always use unique, uniformly distributed partition keys (e.g., `uuid`, `userId`, `deviceId`). Low-cardinality keys (e.g., `country` or `status`) create "hot shards" that saturate 1 MB/s limits while other shards sit idle.
2. **Enable On-Demand Mode for Unpredictable Workloads**: Use On-Demand capacity mode to eliminate manual capacity monitoring and avoid `ProvisionedThroughputExceededException` errors during sudden traffic spikes.
3. **Use Enhanced Fan-Out (EFO) for Multiple Consumers**: If more than two independent consumer applications read from the same stream, standard 2 MB/sec shared read limits cause throttling. Use EFO to dedicate 2 MB/sec per consumer.
4. **Bisect Batch on Error in Lambda Event Source Mappings**: If a batch of 100 records fails processing due to a bad record, Lambda automatically splits the batch in half and retries until the poison record is isolated and forwarded to the failure destination.
5. **Convert to Parquet in Firehose**: Always enable Apache Parquet conversion in Firehose before storing in S3. Columnar Parquet data queries up to 10x faster in Athena and reduces S3 storage/scan costs by up to 80%.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Hot Shard Throttling | Partition key hardcoded to a static string or low-cardinality field. | `ProvisionedThroughputExceededException` even under low overall stream traffic. | Use high-cardinality keys (e.g., hash of user ID + timestamp). |
| Standard Consumer Contention | 5 Lambda consumers all polling the same standard shard. | Consumers exceed 2 MB/s read limit; latency spikes. | Register consumers as Enhanced Fan-Out (EFO) consumers. |
| Forgetting Firehose Minimum Flush Interval | Firehose buffer interval set to 900s for real-time dashboard. | 15-minute data lag in destination S3 bucket. | Lower buffer interval to 60s and buffer size to 1 MB. |
| Missing Lambda Error Handling in Stream Consumer | Consumer throws uncaught exception; shard processing halts. | Stream processing blocks; records reach retention expiry. | Enable `bisect_batch_on_function_error` and configure DLQ destination. |
| Re-inventing Offsets instead of using KCL | Writing custom consumer logic to track shard offset pointers. | Complexity, race conditions, and duplicated data on failover. | Use AWS Kinesis Client Library (KCL) with DynamoDB checkpointing. |
| Provisioning Too Few MSK Partitions | Creating a topic with only 1 partition in MSK. | Only one consumer in a consumer group can read at a time. | Partition topics based on consumer concurrency requirements. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance Comparison
- **Kinesis Data Streams Latency**: ~200ms with standard polling; ~70ms with Enhanced Fan-Out HTTP/2 push.
- **Apache Kafka / Amazon MSK Latency**: Sub-10ms (broker-level memory caching).

### 8.2 Security
- **Data Protection**: Both Kinesis and MSK support AWS KMS encryption at rest and TLS encryption in transit.
- **IAM Authentication for MSK**: MSK supports SASL/SCRAM, mutual TLS (mTLS), and native **AWS IAM Authentication**, eliminating the need to manage static Kafka usernames and passwords.

### 8.3 Cost Breakdown
- **Kinesis On-Demand**: \$0.04 per stream-hour + \$0.08 per GB ingested.
- **Kinesis Provisioned**: \$0.015 per shard-hour (~$11/month per shard) + \$0.014 per million PUT payload units.
- **Data Firehose**: \$0.029 per GB ingested (first 500 TB/month).
- **Amazon MSK Serverless**: \$0.75 per cluster-hour + \$0.10 per GB written + \$0.05 per GB read.

---

## 9. Comparison Table

| Dimension | Amazon Kinesis Data Streams | Amazon MSK (Managed Kafka) | Amazon SQS (Queuing) |
|---|---|---|---|
| **Paradigm** | Distributed Replay Log | Distributed Replay Log | Message Queue |
| **Message Deletion** | No (Retained by time window) | No (Retained by time/size) | Yes (Deleted upon consumption) |
| **Ordering** | Strict per Shard / Key | Strict per Partition / Key | Strict in FIFO queues only |
| **Multiple Consumers** | Yes (Concurrent independent reads)| Yes (Independent Consumer Groups)| No (1 worker consumes 1 msg) |
| **Max Retention** | 365 Days | Indefinite (Tiered storage) | 14 Days |
| **Management** | Serverless / Cloud-native | Open-source Kafka on AWS | Serverless |
| **Typical Latency** | 70ms - 200ms | < 10ms | Variable (pull based) |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the difference between an event streaming log (Kinesis/Kafka) and a message queue (SQS)?</strong></summary>

- **Message Queue (SQS)**: Implements transient, point-to-point job passing. Messages are processed once by a single consumer and explicitly deleted from the queue. Queues do not support data replay or multiple independent subscriber groups reading the exact same historical data stream.
- **Event Streaming Log (Kinesis/Kafka)**: Implements an immutable, append-only, ordered log. Multiple independent consumers read the stream concurrently at their own independent offset pointers without deleting data. Records persist in the log for the full retention period (e.g., 7 days or 365 days), allowing consumers to rewind and replay historical data from any point in time.
</details>

<details>
<summary><strong>Q2: What is the cause of a ProvisionedThroughputExceededException in Kinesis and how do you resolve it?</strong></summary>

This exception occurs when a producer or consumer exceeds shard throughput quotas:
- Producer exceeds **1 MB/s or 1,000 records/s** write per shard.
- Consumer exceeds **2 MB/s** read per shard.
**Resolution**:
1. **Fix Hot Shards**: Ensure partition keys are uniformly distributed (high cardinality).
2. **Batching**: Use `put_records` to optimize payload units.
3. **Scale Capacity**: In Provisioned mode, split congested shards or switch to **On-Demand Mode**.
4. **Enhanced Fan-Out**: Use EFO to provide dedicated 2 MB/s read bandwidth for consumers.
</details>

<details>
<summary><strong>Q3: How does the Kinesis Client Library (KCL) coordinate shard processing across multiple instances?</strong></summary>

The Kinesis Client Library (KCL) uses an **Amazon DynamoDB lease table** to coordinate workers:
1. Each shard is tracked as a lease item in DynamoDB.
2. As worker instances join or leave the consumer fleet, the KCL automatically rebalances shard leases across workers so each shard is assigned to exactly one worker thread.
3. The KCL continuously writes **checkpoints** (the latest processed sequence number) to DynamoDB. If a worker crashes, a surviving worker claims the lease and resumes processing from the last checkpointed offset.
</details>

<details>
<summary><strong>Q4: What is the difference between Kinesis Data Streams and Amazon Data Firehose?</strong></summary>

- **Kinesis Data Streams (KDS)**: Real-time, custom stream ingestion. Requires custom consumer applications (Lambda, KCL, Flink) to process data with sub-second latency. Retention is configurable (up to 365 days).
- **Amazon Data Firehose**: Fully managed, zero-code delivery service. Buffers incoming streaming data (by size or time) and loads it directly into destinations like S3, Redshift, and OpenSearch. It does not provide streaming replay capabilities—once data is delivered to destination storage, Firehose retains no data.
</details>

<details>
<summary><strong>Q5: When should an enterprise choose Amazon MSK over Amazon Kinesis?</strong></summary>

Choose **Amazon MSK** when:
1. Migrating existing on-premises Apache Kafka applications without refactoring producer/consumer code.
2. The architecture requires multi-cloud or hybrid compatibility.
3. Ultra-low, single-digit millisecond latency is mandatory.
4. Payload sizes exceed Kinesis's 1 MB per record limit (Kafka supports configurable multi-megabyte payloads).
5. Complex ecosystem tools (Kafka Connect, Kafka Streams, Schema Registry) are heavily utilized.
Choose **Amazon Kinesis** for pure AWS cloud-native serverless architectures where zero cluster management and seamless IAM/CloudWatch integration are prioritized.
</details>

<details>
<summary><strong>Q6: How does Enhanced Fan-Out (EFO) lower consumer latency in Kinesis?</strong></summary>

Standard Kinesis consumers poll shards using HTTP `GetRecords` calls over a shared 2 MB/sec bandwidth pool, resulting in read latencies around 200ms. 
Enhanced Fan-Out establishes a persistent **HTTP/2 server-push connection** between Kinesis and registered consumers. Kinesis pushes records to the consumer as soon as they are written to the shard, providing a dedicated 2 MB/sec throughput pipe per consumer and dropping latency to approximately 70ms.
</details>

<details>
<summary><strong>Q7: What is the function of the Partition Key in Amazon Kinesis?</strong></summary>

The Partition Key determines which shard in the stream stores a particular record. Kinesis applies an MD5 hash function to the partition key string, mapping the resulting 128-bit integer to the corresponding hash key range allocated to a specific shard. Records with the same partition key are guaranteed to land on the same shard and are read in strict sequential order.
</details>

<details>
<summary><strong>Q8: What is MSK Tiered Storage and what benefit does it provide?</strong></summary>

Historically, retaining large volumes of historical Kafka data required provisioning expensive high-performance EBS volumes attached to every broker. **MSK Tiered Storage** automatically offloads older, infrequently accessed Kafka log segments to cost-effective, practically infinite Amazon S3 storage, while keeping recent active data in local broker storage. This allows enterprises to retain petabytes of streaming data in Kafka for months or years at up to 70% lower storage costs.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create an On-Demand Kinesis Stream and Ingest Records</strong></summary>

**Goal**: Ingest and verify streaming records.
1. Create a Kinesis stream named `test-stream` in On-Demand mode.
2. Use Python `boto3` to send 10 records with random partition keys.
3. Obtain a shard iterator with type `TRIM_HORIZON`.
4. Call `get_records` and verify all 10 records are retrieved.
</details>

<details>
<summary><strong>Exercise 2: Setup Kinesis Firehose Delivering JSON to S3</strong></summary>

**Goal**: Create an automated ingestion pipeline to S3.
1. Create an S3 destination bucket.
2. Create a Firehose delivery stream with a 60-second buffer interval.
3. Send 5 test records to Firehose via CLI: `aws firehose put-record`.
4. Wait 60 seconds and inspect the generated S3 prefix path (`YYYY/MM/DD`).
</details>

<details>
<summary><strong>Exercise 3: Simulate Shard Throttling with a Hot Partition Key</strong></summary>

**Goal**: Observe `ProvisionedThroughputExceededException`.
1. Create a provisioned stream with exactly 1 shard.
2. Run a loop sending 2,000 records per second using the static partition key `"hot-key"`.
3. Observe write throttling in CloudWatch metrics (`WriteProvisionedThroughputExceeded`).
4. Update partition key to a random UUID and observe throttling disappear.
</details>

<details>
<summary><strong>Exercise 4: Enable Enhanced Fan-Out on a Kinesis Consumer</strong></summary>

**Goal**: Register a dedicated consumer.
1. Register a consumer using `aws kinesis register-stream-consumer`.
2. Wait for consumer status to become `ACTIVE`.
3. Note the unique Consumer ARN and inspect its dedicated 2 MB/s read pipe.
</details>

<details>
<summary><strong>Exercise 5: Inline Lambda Data Transformation in Firehose</strong></summary>

**Goal**: Enrich or clean records in transit.
1. Write a Lambda function that decodes base64 Firehose records, appends a timestamp, and marks status as `Ok`.
2. Configure Firehose to use this Lambda function as a data transformation step.
3. Ingest raw records and verify the enriched timestamp appears in S3.
</details>

<details>
<summary><strong>Exercise 6: Query Streaming Data using Amazon Athena</strong></summary>

**Goal**: Run serverless SQL on Parquet data written by Firehose.
1. In AWS Glue Data Catalog, define a table pointing to the S3 bucket written by Firehose.
2. Open Amazon Athena.
3. Execute `SELECT count(*), max(amount) FROM streaming_db.orders WHERE year='2026';`.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| STREAMING SERVICES CAPABILITY COMPARISON                                                          |
+--------------------------+-----------------------+-----------------------+------------------------+
| Dimension                | Kinesis Data Streams  | Kinesis Data Firehose | Amazon MSK             |
+--------------------------+-----------------------+-----------------------+------------------------+
| Architecture             | Serverless Shard Log  | Serverless Buffer ETL | Managed Apache Kafka   |
| Ingestion Latency        | ~70ms - 200ms         | 60s - 900s (Buffered) | < 10ms                 |
| Custom Processing Code   | Required (Lambda/KCL) | None (Destination ETL)| Required (Kafka Client)|
| Storage Retention        | 24 hours to 365 days  | None (Pass-through)   | Days to Indefinite     |
| Destinations             | Any custom application| S3, Redshift, OpenSrch| Any Kafka consumer     |
| Partition Key Control    | Yes (MD5 Hashing)     | N/A (Auto-buffered)   | Yes (Kafka Partitioner)|
+--------------------------+-----------------------+-----------------------+------------------------+
```

- **Kinesis Data Streams**: Real-time serverless log for sub-second reactive analytics.
- **Firehose**: Fully managed zero-code data loader into S3 data lakes and search engines.
- **MSK**: Managed enterprise Apache Kafka for hybrid, multi-cloud, and sub-10ms event backbones.

---

## 13. Further Reading

- [Amazon Kinesis Data Streams Developer Guide](https://docs.aws.amazon.com/streams/latest/dev/introduction.html)
- [Amazon Data Firehose Developer Guide](https://docs.aws.amazon.com/firehose/latest/dev/what-is-this-service.html)
- [Amazon Managed Streaming for Apache Kafka (MSK) Developer Guide](https://docs.aws.amazon.com/msk/latest/developerguide/what-is-msk.html)
- [Developing Consumers Using KCL](https://docs.aws.amazon.com/streams/latest/dev/shared-throughput-kcl-users.html)
- [05-app-services/02-sns-and-eventbridge.md](file:///04-devops/aws/05-app-services/02-sns-and-eventbridge.md)
