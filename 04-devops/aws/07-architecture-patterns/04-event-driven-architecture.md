---
topic: "Event-Driven Architecture on AWS"
module: "07-architecture-patterns"
track: "aws"
last-tested: "2026-11"
prerequisites:
  - "05-app-services/01-sqs.md"
  - "05-app-services/02-sns-and-eventbridge.md"
  - "05-app-services/04-step-functions.md"
complexity: "advanced"
tags:
  - "event-driven"
  - "eventbridge"
  - "sns"
  - "sqs"
  - "step-functions"
  - "architecture"
---

# Event-Driven Architecture on AWS

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

Event-Driven Architecture (EDA) is an architectural paradigm where decoupled software components communicate asynchronously by producing, routing, and reacting to state changes known as **events**. Unlike synchronous request-response patterns (such as REST or gRPC) where services are tightly coupled in time, availability, and network topologies, an event-driven system allows producers and consumers to evolve, scale, and fail independently.

### Why Event-Driven Architecture on AWS?
- **Loose Coupling & Temporal Decoupling**: A producer emits an event (`OrderPlaced`) without knowing or caring how many consumers exist or whether consumers are currently online.
- **Resilience Against Downstream Outages**: If a payment processor or inventory service degrades, messages queue safely in SQS or EventBridge archives without cascading failures back to the end user.
- **High Concurrency & Load Smoothing**: Buffers such as Amazon SQS and Kinesis absorb unexpected traffic spikes, allowing worker fleets to consume messages at a controlled, sustainable rate.
- **Extensibility**: New features (such as fraud detection, audit logging, or push notifications) can be added as passive subscribers without touching existing upstream producer services.

---

## 2. Core Concepts

### Core Primitives of AWS Event-Driven Systems
1. **Events**: Immutable JSON payloads representing a factual domain occurrence (`detail-type: "OrderCreated"`). Contains metadata (source, timestamp, trace ID) and domain state.
2. **Event Router (Amazon EventBridge)**:
   - Serverless event bus that inspects JSON event payloads against declarative pattern rules and routes events to targets.
   - Built-in Schema Registry, archive and replay capabilities, and cross-account event forwarding.
3. **Point-to-Point Queueing (Amazon SQS)**:
   - Provides pull-based delivery, message persistence, backpressure buffering, and Dead-Letter Queues (DLQ).
   - Standard Queues (nearly unlimited throughput, at-least-once delivery) vs FIFO Queues (strictly ordered, deduplicated, 300-3000 msg/s).
4. **Publish-Subscribe Fan-out (Amazon SNS)**:
   - Push-based messaging service broadcasting a single notification simultaneously to hundreds of thousands of HTTP endpoints, SQS queues, SMS, or email subscribers.
5. **Choreography vs Orchestration**:
   - **Choreography (EventBridge / SNS)**: Services react independently to domain events. Best for decentralized cross-domain workflows with low coupling.
   - **Orchestration (AWS Step Functions)**: A centralized state machine coordinates multi-step business transactions, retries, and compensations (Saga pattern).
6. **Transactional Outbox Pattern**:
   - Ensures atomicity between database state changes and event emission. Changes are committed to a local DB outbox table in the same database transaction, then published via DynamoDB Streams or Debezium.

---

## 3. How It Works Internally

```
+----------------------------------------------------------------------------------------------------+
|                         EVENT-DRIVEN ARCHITECTURE ROUTING TOPOLOGY                                 |
+----------------------------------------------------------------------------------------------------+

  [ Order Microservice ]
            |
            | 1. DB Commit + Event Emission
            v
  +--------------------------------------------------------------------------------------------------+
  | Amazon EventBridge (Custom Event Bus: "ecommerce-bus")                                           |
  |                                                                                                  |
  |  Rule 1: detail-type = "OrderPlaced" && amount > 1000                                            |
  |  Rule 2: detail-type = "OrderPlaced"                                                             |
  |  Rule 3: detail-type = "PaymentFailed"                                                           |
  +--------------------------------------------------------------------------------------------------+
        |                                       |                                    |
        | Match Rule 1                          | Match Rule 2                       | Match Rule 3
        v                                       v                                    v
  +---------------------------+   +---------------------------+   +----------------------------------+
  | Amazon SQS (Fraud Queue)  |   | Amazon SQS (Inventory)    |   | AWS Step Functions (Order Saga)  |
  +---------------------------+   +---------------------------+   +----------------------------------+
        |                                       |                                    |
        v                                       v                                    v
  +---------------------------+   +---------------------------+   +----------------------------------+
  | Lambda (Fraud Analyzer)   |   | Lambda (Reserve Stock)    |   | Compensation Workflow            |
  +---------------------------+   +---------------------------+   |  - Refund Customer               |
        |                                       |                 |  - Release Hold                  |
        v (Failure / Poison Pill)               v                 |  - Send Notification             |
  +---------------------------+   +---------------------------+   +----------------------------------+
  | SQS Dead-Letter Queue     |   | Amazon DynamoDB           |
  | (MaxReceiveCount = 3)     |   | (Inventory Table)         |
  +---------------------------+   +---------------------------+
```

### Event Processing Lifecycle
1. **Event Ingestion**: A producer calls `PutEvents` to an EventBridge bus. The payload contains JSON schema validation fields (`Source`, `DetailType`, `Detail`).
2. **Rule Evaluation**: EventBridge evaluates all active rules in parallel using sub-millisecond JSON matching algorithms. Rules evaluate equality, prefix, numeric range, and existence filters.
3. **Target Fan-out**: For each matching rule, EventBridge delivers the event to up to 5 targets. In production, targets are almost always **SQS queues** to guarantee persistent buffering and retry isolation.
4. **Consumer Worker Execution**: Lambda polls the SQS queue using event source mapping (ESM). Lambda processes messages in configurable batches (e.g., 10 messages).
5. **Dead-Letter Handling**: If a message fails processing after `maxReceiveCount` retries, SQS routes the poison message to a Dead-Letter Queue (DLQ) for alarm triggering and manual remediation.

---

## 4. Syntax & Usage

### 4.1 Terraform: EventBridge Bus, Rule, SQS Target, and DLQ

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

# Custom EventBridge Bus
resource "aws_cloudwatch_event_bus" "order_bus" {
  name = "enterprise-order-bus"
}

# Dead Letter Queue for Failed Ingestion/Processing
resource "aws_sqs_queue" "order_dlq" {
  name                      = "order-processing-dlq"
  message_retention_seconds = 1209600 # 14 days
}

# Main SQS Queue for Inventory Service
resource "aws_sqs_queue" "inventory_queue" {
  name                      = "inventory-order-queue"
  visibility_timeout_seconds = 60
  redrive_policy = jsonencode({
    deadLetterTargetArn = aws_sqs_queue.order_dlq.arn
    maxReceiveCount     = 3
  })
}

# SQS Queue Policy allowing EventBridge to send messages
resource "aws_sqs_queue_policy" "inventory_queue_policy" {
  queue_url = aws_sqs_queue.inventory_queue.id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Sid       = "AllowEventBridgeSend"
        Effect    = "Allow"
        Principal = { Service = "events.amazonaws.com" }
        Action    = "sqs:SendMessage"
        Resource  = aws_sqs_queue.inventory_queue.arn
        Condition = {
          ArnEquals = {
            "aws:SourceArn" = aws_cloudwatch_event_rule.order_placed_rule.arn
          }
        }
      }
    ]
  })
}

# EventBridge Rule matching OrderPlaced events
resource "aws_cloudwatch_event_rule" "order_placed_rule" {
  name           = "capture-order-placed"
  event_bus_name = aws_cloudwatch_event_bus.order_bus.name
  description    = "Routes all high-value OrderPlaced events to inventory"

  event_pattern = jsonencode({
    source      = ["ecommerce.orders"]
    detail-type = ["OrderPlaced"]
    detail = {
      status = ["CONFIRMED"]
      amount = [{ numeric = [">=", 50] }]
    }
  })
}

# Target: Send to SQS
resource "aws_cloudwatch_event_target" "sqs_target" {
  rule           = aws_cloudwatch_event_rule.order_placed_rule.name
  event_bus_name = aws_cloudwatch_event_bus.order_bus.name
  target_id      = "SendToInventoryQueue"
  arn            = aws_sqs_queue.inventory_queue.arn

  dead_letter_config {
    arn = aws_sqs_queue.order_dlq.arn
  }
}
```

---

## 5. Practical Examples

### Example 1: Publishing Events to EventBridge via Python AWS SDK (Boto3)

```python
import json
import uuid
from datetime import datetime
import boto3

eventbridge_client = boto3.client("events", region_name="us-east-1")

def publish_order_placed_event(order_id: str, customer_id: str, amount: float):
    event_payload = {
        "orderId": order_id,
        "customerId": customer_id,
        "amount": amount,
        "status": "CONFIRMED",
        "timestamp": datetime.utcnow().isoformat() + "Z"
    }

    response = eventbridge_client.put_events(
        Entries=[
            {
                "EventBusName": "enterprise-order-bus",
                "Source": "ecommerce.orders",
                "DetailType": "OrderPlaced",
                "Detail": json.dumps(event_payload),
                "Time": datetime.utcnow()
            }
        ]
    )

    # Check for partial entry failures
    if response["FailedEntryCount"] > 0:
        error_info = response["Entries"][0]
        raise RuntimeError(f"Event emission failed: {error_info.get('ErrorMessage')}")

    print(f"Event successfully published. EventId: {response['Entries'][0]['EventId']}")
    return response["Entries"][0]["EventId"]

if __name__ == "__main__":
    publish_order_placed_event(
        order_id=str(uuid.uuid4()),
        customer_id="cust-88219",
        amount=149.99
    )
```

### Example 2: SQS Batch Consumer Lambda with Partial Batch Failure Reporting

```javascript
// handler.mjs
// Using Partial Batch Failure reporting prevents reprocessing already-successful messages in the batch
export const handler = async (event) => {
  const batchItemFailures = [];

  for (const record of event.Records) {
    try {
      const messageBody = JSON.parse(record.body);
      const eventDetail = typeof messageBody.detail === "string" 
        ? JSON.parse(messageBody.detail) 
        : messageBody.detail;

      console.log(`Processing Order ID: ${eventDetail.orderId}`);
      await processOrderInventory(eventDetail);

    } catch (error) {
      console.error(`Failed to process message ID ${record.messageId}:`, error);
      // Push only the failed messageId so SQS retries only this item
      batchItemFailures.push({ itemIdentifier: record.messageId });
    }
  }

  return { batchItemFailures };
};

async function processOrderInventory(order) {
  if (order.amount < 0) {
    throw new Error("Invalid negative order amount detected.");
  }
  // Simulate stock reservation logic...
}
```

### Example 3: Transactional Outbox Pattern via DynamoDB Streams

```python
# outbox_stream_processor.py
# Reads committed events from DynamoDB Streams and publishes to EventBridge
import json
import boto3

eventbridge = boto3.client("events")

def handler(event, context):
    events_to_publish = []

    for record in event["Records"]:
        # Only process newly committed outbox rows
        if record["eventName"] == "INSERT":
            new_image = record["dynamodb"]["NewImage"]
            
            # Extract outbox metadata
            event_type = new_image["EventType"]["S"]
            source = new_image["EventSource"]["S"]
            payload_str = new_image["Payload"]["S"]

            events_to_publish.append({
                "EventBusName": "enterprise-order-bus",
                "Source": source,
                "DetailType": event_type,
                "Detail": payload_str
            })

    if events_to_publish:
        # Publish in chunks of 10 (EventBridge PutEvents limit)
        for i in range(0, len(events_to_publish), 10):
            chunk = events_to_publish[i:i+10]
            resp = eventbridge.put_events(Entries=chunk)
            if resp["FailedEntryCount"] > 0:
                print(f"Warning: {resp['FailedEntryCount']} events failed to publish.")
```

### Example 4: Step Functions Saga Pattern Definition (ASL) for Distributed Transactions

```json
{
  "Comment": "Order Fulfillment Saga with Automated Compensation",
  "StartAt": "ReserveInventory",
  "States": {
    "ReserveInventory": {
      "Type": "Task",
      "Resource": "arn:aws:states:::lambda:invoke",
      "Parameters": {
        "FunctionName": "arn:aws:lambda:us-east-1:123456789012:function:reserve-inventory",
        "Payload.$": "$"
      },
      "ResultPath": "$.inventoryResult",
      "Catch": [
        {
          "ErrorEquals": ["States.ALL"],
          "Next": "HandleInventoryFailure"
        }
      ],
      "Next": "ProcessPayment"
    },
    "ProcessPayment": {
      "Type": "Task",
      "Resource": "arn:aws:states:::lambda:invoke",
      "Parameters": {
        "FunctionName": "arn:aws:lambda:us-east-1:123456789012:function:process-payment",
        "Payload.$": "$"
      },
      "ResultPath": "$.paymentResult",
      "Catch": [
        {
          "ErrorEquals": ["States.ALL"],
          "Next": "CompensateInventory"
        }
      ],
      "Next": "OrderSuccess"
    },
    "CompensateInventory": {
      "Type": "Task",
      "Resource": "arn:aws:states:::lambda:invoke",
      "Parameters": {
        "FunctionName": "arn:aws:lambda:us-east-1:123456789012:function:release-inventory",
        "Payload.$": "$"
      },
      "Next": "OrderFailed"
    },
    "HandleInventoryFailure": {
      "Type": "Fail",
      "Cause": "Inventory reservation failed",
      "Error": "InventoryUnavailable"
    },
    "OrderFailed": {
      "Type": "Fail",
      "Cause": "Payment failed, inventory released",
      "Error": "PaymentDeclined"
    },
    "OrderSuccess": {
      "Type": "Succeed"
    }
  }
}
```

### Example 5: SNS to SQS Fan-Out Pattern with Message Filtering

```bash
# 1. Create SNS Topic
TOPIC_ARN=$(aws sns create-topic --name enterprise-customer-events --query "TopicArn" --output text)

# 2. Create SQS Queue
QUEUE_URL=$(aws sqs create-queue --name audit-compliance-queue --query "QueueUrl" --output text)
QUEUE_ARN=$(aws sqs get-queue-attributes --queue-url "$QUEUE_URL" --attribute-names QueueArn --query "Attributes.QueueArn" --output text)

# 3. Subscribe SQS to SNS Topic with Attribute Filtering
aws sns subscribe \
    --topic-arn "$TOPIC_ARN" \
    --protocol sqs \
    --notification-endpoint "$QUEUE_ARN" \
    --attributes '{"FilterPolicy": "{\"event_category\": [\"SECURITY\", \"COMPLIANCE\"]}"}'
```

---

## 6. Best Practices

1. **Always Place SQS Between EventBridge and Lambda**: Avoid targeting Lambda directly from EventBridge. Directly invoking Lambda lacks backpressure control, concurrency smoothing, and native message durability during Lambda concurrency throttling.
2. **Implement ReportBatchItemFailures in SQS Consumers**: Never fail the entire Lambda invocation when only 1 out of 10 messages fails; return `batchItemFailures` so SQS only retries the specific failed message.
3. **Design All Event Handlers for Idempotency**: Due to at-least-once delivery guarantees across distributed networks, handlers will eventually receive duplicate messages. Use DynamoDB conditional writes or unique deduplication tokens.
4. **Enforce JSON Schemas via EventBridge Schema Registry**: Track schema versions and prevent breaking downstream consumers by publishing undeclared schema mutations.
5. **Configure DLQs with CloudWatch Alarms**: Always attach a DLQ with `ApproximateNumberOfMessagesVisible > 0` alarms to catch poisoned messages before data loss occurs.
6. **Use Orchestration (Step Functions) for Multi-Step Business Transactions**: Do not build complex state machines out of chained asynchronous events; use Step Functions to maintain deterministic visibility and compensation execution.

---

## 7. Common Mistakes

| Mistake | Why It Hurts | Correct Approach |
|:---|:---|:---|
| **Direct EventBridge to Lambda invocation** | No backpressure; sudden bursts spike Lambda concurrency and can cause dropped events or account throttling. | Route EventBridge to an SQS queue, then trigger Lambda via Event Source Mapping. |
| **Assuming exactly-once message delivery** | Network retries and distributed broker failovers result in duplicate messages, causing double-charges or double-inventory drops. | Build idempotent consumers using idempotency keys stored in DynamoDB or Redis. |
| **Throwing error for entire SQS batch** | If 1 message out of 10 fails, throwing an exception causes all 10 messages to be retried, duplicating 9 successful operations. | Enable `ReportBatchItemFailures` on the Lambda ESM and return failed `messageId`s. |
| **Dual-writing to DB and messaging bus** | If the DB write succeeds but network fails before EventBridge `PutEvents`, downstream services never receive the event (inconsistent state). | Use the **Transactional Outbox Pattern** with DynamoDB Streams or Debezium. |
| **Omitting Visibility Timeout buffer on SQS** | If SQS Visibility Timeout is $\le$ Lambda timeout, a running Lambda execution may have its message redelivered to a second worker concurrently. | Set SQS `visibility_timeout_seconds` to at least **6x the Lambda timeout**. |
| **Choreographing complex multi-step workflows** | "Event spaghetti": debugging distributed cascades of 15 asynchronous events across 10 teams is nearly impossible. | Use AWS Step Functions to explicitly orchestrate transactions requiring saga compensations. |

---

## 8. Performance, Security & Cost Notes

### Performance
- **EventBridge Ingestion Latency**: Typical end-to-end event routing latency is under 50 milliseconds.
- **Throughput Limits**: EventBridge supports up to 10,000 events/sec per bus by default (adjustable via quota increase). Standard SQS queues offer near-infinite throughput.

### Security
- **Least-Privilege Resource Policies**: Secure SQS queues using `aws:SourceArn` conditions matching specific EventBridge rules.
- **Payload Encryption**: Enable AWS KMS Customer Managed Keys (CMK) on SQS queues, SNS topics, and EventBridge buses to encrypt event payloads at rest.

### Cost Analysis
- **Amazon EventBridge**: \$1.00 per 1 million events published (custom bus). Free for AWS service events.
- **Amazon SQS**: First 1 million requests/month free; \$0.40 per 1 million requests thereafter.
- **Amazon SNS**: First 1 million publishes free; \$0.50 per 1 million publishes thereafter.
- **AWS Step Functions**: \$1.00 per 1,000 state transitions (Standard) or \$1.00 per 1M executions (Express Workflows).

---

## 9. Comparison Table

| Service / Pattern | Delivery Mechanism | Ordering | Deduplication | Filtering | Best For |
|:---|:---|:---|:---|:---|:---|
| **Amazon EventBridge** | Event Routing (Push to targets) | Unordered | Target-level | Advanced JSON content matching | Cross-service routing, SaaS integrations, system state events |
| **Amazon SNS** | Pub/Sub Fan-out (Push) | Standard: Unordered / FIFO: Ordered | FIFO only | Message Attributes | High-throughput broadcast fan-out to SQS, mobile push, SMS |
| **Amazon SQS** | Point-to-Point Queueing (Pull) | Standard: Best-effort / FIFO: Strict | FIFO only | None (Queue consumer filters) | Buffering, load leveling, decoupling worker fleets |
| **AWS Step Functions** | State Machine Orchestration | Deterministic workflow | Built-in execution ID | Step branching logic | Sagas, long-running workflows, automated human approval |

---

## 10. Interview Questions

<details>
<summary><strong>1. How does the Transactional Outbox Pattern solve the dual-write problem in distributed systems?</strong></summary>

In a microservice, updating a local database and publishing an event to a message broker are two distinct network operations. If the application crashes after committing to the database but before publishing the event (or vice-versa), data becomes inconsistent. The Transactional Outbox Pattern solves this by writing the domain entity and an event record into an `Outbox` table within the **same local database transaction**. A separate process (such as DynamoDB Streams or Debezium tailing the WAL) asynchronously and reliably tails the outbox changes and publishes them to EventBridge, guaranteeing at-least-once event publication.
</details>

<details>
<summary><strong>2. Why must the SQS Visibility Timeout be configured to at least 6 times the Lambda function timeout?</strong></summary>

When a Lambda worker pulls a message from SQS, the message remains hidden from other consumers for the duration of the `VisibilityTimeout`. If the visibility timeout is set too close to (or below) the Lambda timeout, temporary execution delays (such as network latency or cold starts) may cause the visibility timeout to expire while the first Lambda is still processing. SQS will then redeliver the message to a second Lambda worker, causing duplicate simultaneous processing. AWS recommends setting the visibility timeout to at least 6x the Lambda function timeout to provide safety against retries and container restarts.
</details>

<details>
<summary><strong>3. What is the difference between Orchestration and Choreography, and when do you use each on AWS?</strong></summary>

- **Choreography**: Services publish events, and other services react independently without centralized direction (e.g., EventBridge routing to multiple independent subscriber queues). Best for loosely coupled, cross-domain notifications where publishers do not need to track overall completion.
- **Orchestration**: A centralized coordinator (e.g., AWS Step Functions) explicitly directs each step of the business process, handling errors, timeouts, and compensations. Best for multi-step workflows with strict SLAs, transactional integrity, or complex recovery sequences (Saga pattern).
</details>

<details>
<summary><strong>4. How do you implement the Saga Pattern for distributed transactions on AWS?</strong></summary>

The Saga pattern coordinates transactions across multiple microservices without distributed two-phase locking. On AWS, this is typically implemented using **AWS Step Functions**. The state machine invokes Lambda functions for each forward step (e.g., Reserve Inventory, Charge Card, Create Shipment). If any step fails, the Step Functions state machine triggers a predefined `Catch` block that invokes corresponding compensating actions in reverse order (e.g., Release Inventory, Refund Card) to return the system to a consistent state.
</details>

<details>
<summary><strong>5. What is the SNS-to-SQS fan-out pattern and why is it preferred over direct SNS-to-Lambda?</strong></summary>

In the SNS-to-SQS fan-out pattern, a publisher sends a message to an SNS topic, which fans out copies to multiple SQS queues subscribed to that topic. Each subscribing microservice owns its private SQS queue. This is superior to direct SNS-to-Lambda because:
1. It provides persistent storage and backpressure buffering if the consumer fleet is overwhelmed.
2. It supports DLQs and automated retry policies per subscriber.
3. Subscribers can pause message processing during deployments without dropping events.
</details>

<details>
<summary><strong>6. How does SQS FIFO message deduplication work?</strong></summary>

SQS FIFO queues enforce deduplication within a 5-minute sliding window using two methods:
1. **Content-Based Deduplication**: SQS generates an SHA-256 hash of the message body. If another message with an identical body arrives within 5 minutes, it is accepted but not delivered.
2. **Explicit Message Deduplication ID**: The producer passes a unique token (`MessageDeduplicationId`). Any duplicate message submitted with the same ID within 5 minutes is silently acknowledged and ignored.
</details>

<details>
<summary><strong>7. How does AWS Lambda handle partial batch failures when reading from SQS?</strong></summary>

By default, if any message in a batch throws an unhandled exception, Lambda fails the entire batch, causing all messages to become visible again in SQS. To prevent reprocessing already-successful items, configure `ReportBatchItemFailures` in the event source mapping. The Lambda handler catches errors per record and returns a JSON payload containing `batchItemFailures: [{ itemIdentifier: "messageId" }]`. SQS will then delete successful messages and only retry the failed IDs.
</details>

<details>
<summary><strong>8. What are the key differences between Amazon EventBridge and Amazon Kinesis Data Streams?</strong></summary>

EventBridge is an **event router** designed for discrete, asynchronous domain events (push-based, payload up to 256 KB, evaluated against JSON rules, routing to heterogeneous AWS targets). Kinesis is a **streaming data pipeline** designed for high-throughput, sequential time-series ingestion (pull-based, sharded partitions, sub-second latency, gigabytes/sec throughput) optimized for real-time analytics, clickstreams, and telemetry metrics.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Challenge 1: Create an EventBridge Rule and Route to SQS</strong></summary>

**Goal**: Provision a custom EventBridge bus, an SQS queue with an attached SQS resource policy, and a rule filtering for `source: "payment.service"` and `amount >= 100`.
**Verification**: Use the AWS CLI `aws events put-events` to publish a matching and non-matching event; verify only the matching event lands in the SQS queue via `aws sqs receive-message`.
</details>

<details>
<summary><strong>Challenge 2: Implement SQS Partial Batch Failure Handler</strong></summary>

**Goal**: Write a Node.js Lambda function configured with `ReportBatchItemFailures`. Simulate processing 5 messages where the 3rd message fails.
**Verification**: Verify in CloudWatch Logs that only 1 message ID is returned in `batchItemFailures`, and verify in SQS that the other 4 messages are successfully deleted.
</details>

<details>
<summary><strong>Challenge 3: Build an Idempotent Consumer with DynamoDB</strong></summary>

**Goal**: Create a Lambda handler that stores a unique `idempotencyKey` in a DynamoDB table with a conditional expression `attribute_not_exists(idempotencyKey)`.
**Verification**: Send 10 identical messages; verify that business logic runs exactly once and 9 messages are safely ignored.
</details>

<details>
<summary><strong>Challenge 4: Deploy an SNS-to-SQS Fan-out Architecture with Terraform</strong></summary>

**Goal**: Provision an SNS topic subscribed to two separate SQS queues (Billing Queue and Shipping Queue) with attribute filter policies.
**Verification**: Publish messages with different attributes to the topic and verify that messages are routed selectively to the appropriate queues.
</details>

<details>
<summary><strong>Challenge 5: Build a Step Functions Order Fulfillment Saga</strong></summary>

**Goal**: Create an AWS Step Functions state machine with a Task step that fails intentionally and catches the error to execute a compensating cleanup Lambda.
**Verification**: Execute the state machine via AWS CLI; inspect the execution graph to confirm the compensation step ran and the workflow finished gracefully.
</details>

<details>
<summary><strong>Challenge 6: Set Up EventBridge Archive and Replay</strong></summary>

**Goal**: Enable an EventBridge Archive on a custom event bus with a 7-day retention period. Publish test events, then initiate an Event Replay via AWS CLI.
**Verification**: Verify that replayed events are processed again by downstream target queues with the `replay-name` attribute populated.
</details>

---

## 12. Summary / Cheat Sheet

```
+----------------------------------------------------------------------------------------------------+
|                             EVENT-DRIVEN PATTERNS QUICK REFERENCE                                  |
+----------------------------------------------------------------------------------------------------+
| Pattern               | Core Services                      | Primary Purpose                       |
+-----------------------+------------------------------------+---------------------------------------+
| Event Routing         | EventBridge Bus + Rules            | Schema-aware JSON routing & filtering |
| Pub/Sub Broadcast     | Amazon SNS -> Multiple SQS Queues  | High-throughput 1-to-many fan-out     |
| Buffering & Leveling  | Amazon SQS -> Lambda ESM           | Protect downstream backends from load |
| Transactional Outbox  | DynamoDB + Streams -> Lambda       | Atomic DB write + event publishing    |
| Saga Orchestration    | AWS Step Functions                 | Deterministic transactions & rollback |
| Poison Pill Defense   | SQS Dead-Letter Queue (DLQ)        | Isolate corrupted messages & alarm    |
+-----------------------+------------------------------------+---------------------------------------+
```

---

## 13. Further Reading

- [AWS Prescriptive Guidance: Building Event-Driven Architectures on AWS](https://docs.aws.amazon.com/prescriptive-guidance/latest/patterns/event-driven-architecture.html)
- [Amazon EventBridge User Guide](https://docs.aws.amazon.com/eventbridge/latest/userguide/eb-what-is-how-it-works.html)
- [AWS Lambda with Amazon SQS Event Source Mapping Best Practices](https://docs.aws.amazon.com/lambda/latest/dg/with-sqs.html)
- [Enterprise Integration Patterns - Gregor Hohpe](https://www.enterpriseintegrationpatterns.com/)
- [Saga Distributed Transactions Pattern](https://microservices.io/patterns/data/saga.html)
