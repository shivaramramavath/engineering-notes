# Amazon Simple Queue Service (SQS) & Distributed Message Queuing

> **Module**: `05-app-services`  
> **Topic**: Asynchronous Decoupling, Standard vs FIFO, Visibility Timeout, DLQs & Idempotency  
> **Target Audience**: Cloud Engineers, Backend Architects, Distributed Systems Engineers  
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

In tightly coupled synchronous architectures, Service A makes direct HTTP/RPC calls to Service B. If Service B experiences a traffic spike, slows down due to database contention, or crashes during a deployment, Service A's threads block, resource pools exhaust, and cascading failures ripple across the entire system.

**Amazon Simple Queue Service (Amazon SQS)** is a fully managed, distributed message queuing service that decouples and scales microservices, distributed systems, and serverless applications. As the first service ever launched by AWS in 2004, SQS:
1. **Eliminates Cascading Outages**: Acts as a resilient buffer absorbing spiky traffic workloads, allowing consumers to process messages at their own sustainable rate.
2. **Provides Elastic Infinite Scale**: Scales dynamically to handle virtually unlimited throughput without provisioning hardware, clusters, or storage partitions.
3. **Ensures Reliability**: Replicates messages redundantly across multiple Availability Zones, guaranteeing message durability even during data center disruptions.

---

## 2. Core Concepts

### 2.1 Standard Queues vs FIFO Queues
1. **Standard Queues**:
   - **Throughput**: Unlimited transactions per second (TPS).
   - **Ordering**: Best-effort ordering (messages may occasionally be delivered in a different order than sent).
   - **Delivery Guarantees**: **At-least-once delivery** (occasional duplicate messages can occur due to distributed acknowledgment delays).
2. **FIFO Queues (First-In-First-Out)**:
   - **Throughput**: Up to 300 transactions/sec (or 3,000 TPS with batching); up to 70,000 TPS in high-throughput FIFO mode.
   - **Ordering**: Strict FIFO ordering preserved per `MessageGroupId`.
   - **Delivery Guarantees**: **Exactly-once processing** within a 5-minute deduplication window using `MessageDeduplicationId` or SHA-256 content-based deduplication.
   - **Naming Convention**: Queue name must end with the `.fifo` suffix.

### 2.2 Visibility Timeout & Message Lifecycle
- **Visibility Timeout**: The duration (default 30 seconds, range 0 seconds to 12 hours) during which a message is hidden from other consumers after being received by a worker.
- **Explicit Deletion Required**: SQS does *not* automatically delete a message when consumed. The consumer must explicitly invoke `DeleteMessage` once processing succeeds. If the worker crashes or fails before deletion, the message becomes visible again for another worker to process.
- **Heartbeat Extension (`ChangeMessageVisibility`)**: Long-running workers can periodically extend the visibility timeout while actively processing a message to prevent premature redelivery.

### 2.3 Short Polling vs Long Polling
- **Short Polling (`WaitTimeSeconds = 0`)**: SQS samples a subset of distributed storage servers and returns immediately, even if no messages are found. This leads to empty responses and higher API costs.
- **Long Polling (`WaitTimeSeconds = 1 to 20`)**: SQS holds the consumer connection open until messages arrive or the timeout elapses. Eliminates false empty responses and slashes API request charges by up to 90%.

### 2.4 Dead-Letter Queues (DLQs) & Redrive Policies
- **Dead-Letter Queue (DLQ)**: A secondary queue dedicated to holding "poison pill" messages that fail processing repeatedly.
- **`maxReceiveCount`**: Threshold (typically 3 to 5) defining how many times a message can be received and returned to the queue before SQS automatically routes it to the DLQ.
- **Dead-Letter Redrive**: AWS Console and CLI capability to inspect failed messages and redrive them back to the source queue once bugs are patched.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                              AMAZON SQS MESSAGE LIFECYCLE & WORKFLOW                              |
|                                                                                                   |
|  [Producer Service]                                                                               |
|         |                                                                                         |
|         v (SendMessage: Payload + MessageAttributes)                                              |
|  +---------------------------------------------------------------------------------------------+  |
|  | AMAZON SQS DISTRIBUTED STORAGE LAYER (Multi-AZ Replicated Quorum)                           |  |
|  |                                                                                             |  |
|  |   [Message 1: State = Visible] ------> [Message 2: State = Visible]                          |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                                         |
|         v (ReceiveMessage: Long Polling WaitTime=20s)                                             |
|  [Consumer Worker A]                                                                              |
|         |                                                                                         |
|         +---> Message 1 enters INVISIBLE state (Clock starts: 30s Visibility Timeout)             |
|         |                                                                                         |
|         +--- Path 1: Worker succeeds within 12 seconds ----------------------------------------+  |
|         |            Executes DeleteMessage(ReceiptHandle)                                     |  |
|         |            Message 1 permanently purged from queue                                   |  |
|         |                                                                                      |  |
|         +--- Path 2: Worker crashes or exceeds 30 seconds -------------------------------------+  |
|                      Visibility Timeout expires!                                               |  |
|                      Message 1 transitions back to VISIBLE                                     |  |
|                      ReceiveCount increments to 2                                              |  |
|                                                                                                |  |
|         +--- Path 3: ReceiveCount exceeds maxReceiveCount (e.g., 3) ---------------------------+  |
|                      RedrivePolicy moves message to DEAD-LETTER QUEUE (DLQ)                    |  |
|                      CloudWatch Alarm fires on ApproximateNumberOfMessagesVisible in DLQ        |  |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Creating Standard and FIFO Queues via AWS CLI

```bash
# 1. Create a Standard SQS Queue with Long Polling (20s) and 60s Visibility Timeout
aws sqs create-queue \
    --queue-name "order-processing-queue" \
    --attributes '{
      "ReceiveMessageWaitTimeSeconds": "20",
      "VisibilityTimeout": "60",
      "MessageRetentionPeriod": "86400"
    }'

# 2. Create a FIFO SQS Queue with Content-Based Deduplication
aws sqs create-queue \
    --queue-name "financial-ledger.fifo" \
    --attributes '{
      "FifoQueue": "true",
      "ContentBasedDeduplication": "true",
      "ReceiveMessageWaitTimeSeconds": "20",
      "VisibilityTimeout": "30"
    }'
```

### 4.2 Sending and Receiving Messages with Receipt Handles

```bash
# Send message to Standard Queue
aws sqs send-message \
    --queue-url "https://sqs.us-east-1.amazonaws.com/123456789012/order-processing-queue" \
    --message-body '{"orderId": "ord-9921", "amount": 149.99, "currency": "USD"}'

# Receive message using Long Polling (returns ReceiptHandle)
aws sqs receive-message \
    --queue-url "https://sqs.us-east-1.amazonaws.com/123456789012/order-processing-queue" \
    --wait-time-seconds 20 \
    --max-number-of-messages 1

# Delete message using the ReceiptHandle returned from receive-message
aws sqs delete-message \
    --queue-url "https://sqs.us-east-1.amazonaws.com/123456789012/order-processing-queue" \
    --receipt-handle "AQEB...sample_receipt_handle...=="
```

---

## 5. Practical Examples

### 5.1 Complete SQS Queue with DLQ and Redrive Policy (Terraform)

```hcl
# Dead-Letter Queue (DLQ)
resource "aws_sqs_queue" "orders_dlq" {
  name                      = "orders-dlq"
  message_retention_seconds = 1209600 # Retain for 14 days
}

# Main Workload Queue
resource "aws_sqs_queue" "orders_queue" {
  name                       = "orders-processing-queue"
  visibility_timeout_seconds = 180 # 3 minutes (must exceed consumer processing time)
  message_retention_seconds  = 345600 # 4 days
  receive_wait_time_seconds  = 20 # Enable Long Polling by default

  redrive_policy = jsonencode({
    deadLetterTargetArn = aws_sqs_queue.orders_dlq.arn
    maxReceiveCount     = 3
  })

  tags = {
    Environment = "production"
    Service     = "order-pipeline"
  }
}

# Allow main queue to send messages to DLQ
resource "aws_sqs_queue_redrive_allow_policy" "dlq_allow" {
  queue_url = aws_sqs_queue.orders_dlq.id

  redrive_allow_policy = jsonencode({
    redrivePermission = "byQueue"
    sourceQueueArns   = [aws_sqs_queue.orders_queue.arn]
  })
}
```

### 5.2 Resilient Python Consumer with Idempotency & Visibility Heartbeat

```python
"""
SQS Consumer demonstrating long polling, visibility heartbeat, idempotency, and deletion.
"""
import boto3
import json
import time
import threading

sqs = boto3.client("sqs", region_name="us-east-1")
dynamodb = boto3.resource("dynamodb", region_name="us-east-1")
idempotency_table = dynamodb.Table("ProcessedMessages")
QUEUE_URL = "https://sqs.us-east-1.amazonaws.com/123456789012/orders-processing-queue"

def extend_visibility(receipt_handle: str, stop_event: threading.Event):
    """Heartbeat thread extending visibility timeout every 20 seconds during long tasks."""
    while not stop_event.wait(20):
        try:
            sqs.change_message_visibility(
                QueueUrl=QUEUE_URL,
                ReceiptHandle=receipt_handle,
                VisibilityTimeout=30
            )
        except Exception as e:
            print(f"Failed to extend visibility: {e}")
            break

def process_message(message: dict):
    receipt_handle = message["ReceiptHandle"]
    body = json.loads(message["Body"])
    order_id = body["orderId"]

    # 1. Idempotency Check using DynamoDB conditional write
    try:
        idempotency_table.put_item(
            Item={"orderId": order_id, "processedAt": int(time.time())},
            ConditionExpression="attribute_not_exists(orderId)"
        )
    except Exception:
        print(f"Order {order_id} already processed. Skipping business logic.")
        sqs.delete_message(QueueUrl=QUEUE_URL, ReceiptHandle=receipt_handle)
        return

    # 2. Start heartbeat thread for long-running workload
    stop_heartbeat = threading.Event()
    heartbeat_thread = threading.Thread(target=extend_visibility, args=(receipt_handle, stop_heartbeat))
    heartbeat_thread.start()

    try:
        # Simulate business logic processing
        time.sleep(25)
        print(f"Successfully processed order {order_id}")

        # 3. Explicitly delete message on success
        sqs.delete_message(QueueUrl=QUEUE_URL, ReceiptHandle=receipt_handle)
    finally:
        stop_heartbeat.set()
        heartbeat_thread.join()

def poll_queue():
    while True:
        response = sqs.receive_message(
            QueueUrl=QUEUE_URL,
            MaxNumberOfMessages=5,
            WaitTimeSeconds=20,
            AttributeNames=["All"]
        )
        messages = response.get("Messages", [])
        for msg in messages:
            process_message(msg)
```

### 5.3 FIFO Queue with Deduplication and Message Grouping

```hcl
resource "aws_sqs_queue" "ledger_fifo" {
  name                        = "ledger-transactions.fifo"
  fifo_queue                  = true
  content_based_deduplication = false # We supply explicit MessageDeduplicationId
  visibility_timeout_seconds  = 30
  receive_wait_time_seconds   = 20
}
```

```python
# Producer sending to FIFO queue
sqs.send_message(
    QueueUrl="https://sqs.us-east-1.amazonaws.com/123456789012/ledger-transactions.fifo",
    MessageBody=json.dumps({"accountId": "acc-100", "txn": "debit", "amount": 50.00}),
    MessageGroupId="acc-100",           # Strict ordering maintained per accountId
    MessageDeduplicationId="txn-987654" # Prevents double debit within 5 minutes
)
```

### 5.4 Auto-Scaling EC2 Workers Based on SQS Queue Backlog

```hcl
# CloudWatch Metric Alarm for Backlog Depth
# Calculates Backlog per Instance = ApproximateNumberOfMessagesVisible / RunningCapacity
resource "aws_cloudwatch_metric_alarm" "sqs_backlog_high" {
  alarm_name          = "sqs-backlog-per-instance-high"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = 2
  metric_name         = "ApproximateNumberOfMessagesVisible"
  namespace           = "AWS/SQS"
  period              = 60
  statistic           = "Average"
  threshold           = 100

  dimensions = {
    QueueName = aws_sqs_queue.orders_queue.name
  }

  alarm_actions = [aws_autoscaling_policy.scale_out.arn]
}
```

### 5.5 SQS Large Payload Storage with Amazon S3 Extended Client

```python
# SQS native message size limit is 256 KB.
# For payloads exceeding 256 KB (up to 2 GB), offload the payload to S3 
# and pass the S3 pointer inside the SQS message body.
import boto3
import uuid

s3 = boto3.client("s3")
sqs = boto3.client("sqs")

def send_large_message(large_data: bytes, queue_url: str, bucket_name: str):
    s3_key = f"sqs-payloads/{uuid.uuid4()}.json"
    s3.put_object(Bucket=bucket_name, Key=s3_key, Body=large_data)

    payload_pointer = {
        "s3Bucket": bucket_name,
        "s3Key": s3_key,
        "isPayloadPointer": True
    }
    sqs.send_message(QueueUrl=queue_url, MessageBody=json.dumps(payload_pointer))
```

---

## 6. Best Practices

1. **Always Enable Long Polling (`WaitTimeSeconds = 20`)**: Slashes empty receive API calls, reducing monthly AWS billing by up to 90% while reducing message delivery latency.
2. **Design Consumers for Idempotency**: Standard SQS delivers messages with **at-least-once** semantics. Network hiccups or consumer timeouts can result in duplicate deliveries. Use unique transaction tokens in DynamoDB or Redis to prevent duplicate processing.
3. **Queue Visibility Timeout >= 6x Lambda Timeout**: When using AWS Lambda as an SQS consumer, configure the queue's visibility timeout to at least 6 times the Lambda function timeout to prevent retries from overlapping running executions.
4. **Always Attach a Dead-Letter Queue (DLQ)**: Without a DLQ, malformed "poison pill" messages fail repeatedly, consuming worker capacity in an infinite retry loop until the message retention period expires.
5. **Scale Consumers on Backlog per Instance, Not CPU**: Scaling EC2 or ECS worker fleets using CPU utilization is ineffective for queuing systems. Scale based on `ApproximateNumberOfMessagesVisible` divided by active worker instance count.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Visibility Timeout Shorter than Processing Time | Long-running task takes 45s; visibility timeout is 30s. | Message reappears; second worker picks it up; duplicate execution. | Increase visibility timeout or use `ChangeMessageVisibility` heartbeat. |
| Forgetting to Call `DeleteMessage` | Developer assumes receiving a message automatically removes it. | Message reappears every 30 seconds indefinitely. | Always invoke `DeleteMessage(ReceiptHandle)` upon successful processing. |
| Single Hot `MessageGroupId` in FIFO Queue | Sending all messages with `MessageGroupId = "default"`. | Throughput bottlenecks to single-worker processing (300 TPS max). | Partition `MessageGroupId` by customer ID, tenant, or entity ID. |
| Relying on Short Polling in Production | `ReceiveWaitTimeSeconds` left at default 0. | High API costs due to millions of empty polling requests. | Set `ReceiveMessageWaitTimeSeconds: 20` at queue level. |
| SQS to Lambda Timeout Race Condition | Queue visibility timeout (30s) equals Lambda timeout (30s). | Lambda retry triggers before original invocation times out. | Set SQS visibility timeout to $\ge 6 \times \text{Lambda timeout}$. |
| Missing DLQ Alarm Monitoring | DLQ captures failed messages, but nobody is notified. | Silent business failures; unprocessed orders accumulate unnoticed. | Set CloudWatch Alarm on DLQ `ApproximateNumberOfMessagesVisible > 0`. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance & Batching
- **Batch APIs**: Use `SendMessageBatch` and `DeleteMessageBatch` to group up to 10 messages (max 256 KB combined) in a single API call, reducing API costs and increasing throughput 10x.
- **High-Throughput FIFO Mode**: Enable high-throughput mode on FIFO queues to scale up to 70,000 transactions/second with batching.

### 8.2 Security & Compliance
- **KMS Server-Side Encryption (SSE-SQS / SSE-KMS)**: Protects message bodies at rest. `SSE-SQS` encrypts queues using AWS-owned keys at zero extra KMS cost.
- **VPC Endpoints (AWS PrivateLink)**: Interface endpoints allow EC2/ECS workloads in private subnets to send and receive SQS messages without traversing the public Internet.

### 8.3 Cost Breakdown
- **Standard Queue**: First 1 million requests/month free; \$0.40 per million requests thereafter.
- **FIFO Queue**: \$0.50 per million requests.
- **Payload Billing**: Each 64 KB chunk of payload counts as one request (e.g., a 256 KB message counts as 4 billable requests).

---

## 9. Comparison Table

| Dimension | SQS Standard Queue | SQS FIFO Queue |
|---|---|---|
| **Ordering** | Best-effort | Strict First-In-First-Out |
| **Delivery Guarantee**| At-least-once | Exactly-once processing |
| **Throughput** | Unlimited TPS | 300 - 3,000 TPS (up to 70k in high-throughput) |
| **Deduplication** | Manual (Consumer Idempotency) | Native 5-minute deduplication window |
| **Message Grouping** | Not applicable | Required (`MessageGroupId`) |
| **Queue Naming** | Any valid string | Must end in `.fifo` |
| **Pricing** | \$0.40 per million requests | \$0.50 per million requests |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: Why is idempotency mandatory when consuming from an SQS Standard Queue?</strong></summary>

SQS Standard Queues operate across a distributed architecture of thousands of storage servers across multiple Availability Zones. When a consumer deletes a message, the deletion acknowledgment must propagate across all servers. If a network blip occurs during this propagation, or if a consumer's processing time exceeds the visibility timeout, the message becomes visible again and another consumer receives it. Because SQS Standard guarantees **at-least-once delivery**, duplicates are inevitable. Consumers must be idempotent (e.g., tracking processed message IDs in DynamoDB or SQL unique constraints) so duplicate executions produce identical, safe outcomes.
</details>

<details>
<summary><strong>Q2: How does the SQS Visibility Timeout work, and what happens if a consumer crashes?</strong></summary>

When a consumer receives a message, SQS marks it as invisible for the configured Visibility Timeout duration (e.g., 30 seconds). During this window, other workers polling the queue cannot see or receive that message.
- If the worker processes the message and calls `DeleteMessage`, the message is permanently destroyed.
- If the worker crashes, loses power, or takes 31 seconds, SQS assumes the worker died. Once the 30-second timer expires, the message transitions back to visible status, and another worker receives it on its next poll.
</details>

<details>
<summary><strong>Q3: How does message grouping work in SQS FIFO queues and how does it affect parallelism?</strong></summary>

In an SQS FIFO queue, every message must contain a `MessageGroupId`. 
- Messages sharing the same `MessageGroupId` are strictly processed in sequential order, one message at a time. If message 1 in Group A is in-flight, SQS will not deliver message 2 in Group A to any worker until message 1 is deleted or its visibility timeout expires.
- However, messages with *different* `MessageGroupIds` (e.g., Group A, Group B, Group C) are processed in parallel concurrently. To achieve high throughput in a FIFO queue, you must ensure a high cardinality of distinct message group IDs (e.g., using `userId` or `orderId`).
</details>

<details>
<summary><strong>Q4: Why does AWS recommend setting the SQS Visibility Timeout to 6x the Lambda function timeout?</strong></summary>

When an SQS queue triggers an AWS Lambda function via an Event Source Mapping (ESM), the ESM polls the queue and invokes Lambda. If the visibility timeout is too close to the Lambda timeout (e.g., both 30s), a minor execution delay or retried function invocation could cause the visibility timeout to expire while the Lambda function is still running. SQS would then redeliver the message to a second Lambda invocation, leading to duplicate processing. A 6x buffer ensures Lambda's internal retries and execution lifecycle complete cleanly before SQS considers redelivering the message.
</details>

<details>
<summary><strong>Q5: What is the difference between SQS Long Polling and Short Polling?</strong></summary>

- **Short Polling (`WaitTimeSeconds = 0`)**: SQS queries a small random sample of its distributed storage servers and returns immediately. If the sampled servers hold no messages (even if messages exist on other servers in the cluster), SQS returns an empty response. This creates high API call volume and costs.
- **Long Polling (`WaitTimeSeconds = 1 to 20`)**: SQS queries all storage servers and holds the HTTP request open until at least one message is available (up to 20 seconds). This eliminates empty responses, reduces API charges, and minimizes message latency.
</details>

<details>
<summary><strong>Q6: How do you handle messages exceeding the 256 KB SQS message size limit?</strong></summary>

Use the **Claim Check Pattern** via the Amazon S3 Extended Client Library:
1. The producer stores the large payload (up to 2 GB) in an Amazon S3 bucket.
2. The producer puts a lightweight JSON message in SQS containing the S3 bucket name and object key (the "claim check").
3. The consumer reads the claim check from SQS, downloads the payload from S3, processes it, deletes the SQS message, and purges the S3 object.
</details>

<details>
<summary><strong>Q7: What is an SQS Dead-Letter Queue Redrive?</strong></summary>

A Dead-Letter Queue Redrive is an automated AWS feature that moves messages accumulated in a Dead-Letter Queue back to their original source queue (or a specified custom queue). Once developers identify and fix the underlying bug that caused messages to fail (such as a database schema mismatch or third-party API outage), they initiate a redrive from the AWS Console or CLI, processing the queued messages without writing custom migration scripts.
</details>

<details>
<summary><strong>Q8: What metric should you use to scale an Auto Scaling group of EC2 worker instances consuming from SQS?</strong></summary>

Do NOT scale on CPU Utilization, as idle polling workers consume minimal CPU even with millions of messages pending. 
Instead, scale on the **Backlog per Instance** custom metric:
$$\text{Backlog Per Instance} = \frac{\text{ApproximateNumberOfMessagesVisible}}{\text{Running Instance Count}}$$
If a single worker can process 10 messages per minute and acceptable latency is 3 minutes, target a backlog of 30 messages per instance.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create an SQS Queue with Dead-Letter Redrive</strong></summary>

**Goal**: Verify automatic message movement to DLQ on repeated failures.
1. Create a DLQ named `test-dlq`.
2. Create a source queue named `test-work-queue` with `maxReceiveCount = 2` pointing to `test-dlq`.
3. Send a message to `test-work-queue`.
4. Poll the message twice using `aws sqs receive-message`, without deleting it.
5. Check `test-dlq` and verify the message has moved automatically.
</details>

<details>
<summary><strong>Exercise 2: Implement Long Polling and Measure Cost Reduction</strong></summary>

**Goal**: Compare API invocation counts between short polling and long polling.
1. Write a script that short-polls an empty queue 100 times (`WaitTimeSeconds = 0`). Note the completion time.
2. Update the script to long-poll (`WaitTimeSeconds = 5`).
3. Observe how long polling dramatically reduces the number of billable HTTP requests.
</details>

<details>
<summary><strong>Exercise 3: Test FIFO Deduplication Behavior</strong></summary>

**Goal**: Verify that identical `MessageDeduplicationId`s are rejected within the 5-minute window.
1. Create an SQS FIFO queue.
2. Send a message with `MessageGroupId = "grp1"` and `MessageDeduplicationId = "dedup-1"`.
3. Send a second message with the exact same deduplication ID within 10 seconds.
4. Poll the queue and verify that only one message is returned.
</details>

<details>
<summary><strong>Exercise 4: Visibility Timeout Heartbeat Implementation</strong></summary>

**Goal**: Dynamically extend visibility for a slow task.
1. Set queue visibility timeout to 10 seconds.
2. Send a message to the queue.
3. Write a consumer that sleeps for 25 seconds while spawning a background thread that calls `change_message_visibility` every 8 seconds.
4. Verify no other worker receives the message during the 25-second processing period.
</details>

<details>
<summary><strong>Exercise 5: Build a Dead-Letter Queue CloudWatch Alarm</strong></summary>

**Goal**: Trigger an alert when failed messages land in a DLQ.
1. Create a CloudWatch Alarm monitoring `ApproximateNumberOfMessagesVisible` on your DLQ.
2. Set threshold $> 0$ for 1 evaluation period.
3. Inject a poison pill message and verify the alarm transitions to `ALARM` state.
</details>

<details>
<summary><strong>Exercise 6: Redrive DLQ Messages Back to Source</strong></summary>

**Goal**: Execute an automated redrive operation.
1. Ensure your DLQ has 3 failed messages.
2. In the AWS Console or via AWS CLI, initiate a dead-letter redrive task:
   `aws sqs start-message-move-task --source-arn <dlq-arn>`.
3. Monitor task status and verify all 3 messages return to the source queue.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| SQS CORE TIMEOUTS & QUOTAS                                                                        |
+--------------------------+-----------------------+------------------------------------------------+
| Setting / Parameter      | Default Value         | Allowed Range / Limit                          |
+--------------------------+-----------------------+------------------------------------------------+
| Visibility Timeout       | 30 seconds            | 0 seconds to 12 hours                          |
| Message Retention Period | 4 days (345,600s)     | 60 seconds to 14 days (1,209,600s)             |
| Delivery Delay           | 0 seconds             | 0 seconds to 15 minutes                        |
| Receive Wait Time        | 0 seconds (Short)     | 0 seconds to 20 seconds (Long Polling)         |
| Message Size Limit       | 256 KB                | Up to 2 GB via S3 Extended Client              |
| Batch API Limit          | 10 messages           | Maximum 10 messages / 256 KB per batch         |
| FIFO Deduplication       | 5 minutes             | Fixed 5-minute deduplication window            |
+--------------------------+-----------------------+------------------------------------------------+
```

- **Visibility Timeout**: Prevents duplicate consumption; must exceed worker processing duration.
- **Long Polling (`WaitTimeSeconds = 20`)**: Mandatory for cost reduction and low latency.
- **DLQ**: Essential safety valve; pair with CloudWatch alarms to detect poison messages.

---

## 13. Further Reading

- [Amazon SQS Developer Guide](https://docs.aws.amazon.com/AWSSimpleQueueService/latest/SQSDeveloperGuide/)
- [Using Amazon SQS with AWS Lambda](https://docs.aws.amazon.com/lambda/latest/dg/with-sqs.html)
- [Amazon SQS FIFO Queues & High-Throughput Mode](https://docs.aws.amazon.com/AWSSimpleQueueService/latest/SQSDeveloperGuide/high-throughput-fifo.html)
- [Backlog per Instance Scaling Architecture](https://docs.aws.amazon.com/autoscaling/ec2/userguide/as-using-sqs-queue.html)
- [04-networking/08-api-gateway.md](file:///04-devops/aws/04-networking/08-api-gateway.md)
