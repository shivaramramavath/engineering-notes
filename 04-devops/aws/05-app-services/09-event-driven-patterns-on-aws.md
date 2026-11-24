# Enterprise Event-Driven Architecture (EDA) Patterns on AWS

> **Module**: `05-app-services`  
> **Topic**: Choreography vs Orchestration, Transactional Outbox, Idempotency, Claim Check & Circuit Breakers  
> **Target Audience**: Principal Architects, Distributed Systems Engineers, Lead Developers  
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

In monolithic architectures, application modules communicate via in-memory function calls, and multi-table state modifications are wrapped in ACID database transactions. However, as systems decompose into microservices distributed across clouds and networks, maintaining synchronous request-response communication (REST or gRPC) introduces severe architectural friction:
- **Tight Coupling & Cascading Failures**: If Service D is slow, Services A, B, and C exhaust their connection pools waiting for responses.
- **Dual-Write Inconsistency**: If an application writes to a relational database and then sends an event to a message broker, a crash between those two operations causes permanent data drift.
- **Scaling Bottlenecks**: Synchronous HTTP calls force producers to wait for consumers, preventing the system from absorbing sudden bursts of traffic.

**Event-Driven Architecture (EDA)** is a distributed software design pattern where decoupled services asynchronously produce, detect, and react to state changes ("events"). By combining native AWS services—**EventBridge, SNS, SQS, Step Functions, DynamoDB Streams, and Kinesis**—architects can construct resilient, self-healing, horizontally scalable systems that handle millions of events per second with high fault tolerance.

---

## 2. Core Concepts

### 2.1 Choreography vs Orchestration
1. **Choreography (Decentralized / Reactive)**:
   - Services publish domain events to an event bus (Amazon EventBridge or SNS) without knowing who is listening.
   - Other microservices subscribe to events independently and react by executing their local logic.
   - **Pros**: Extreme decoupling, high scalability, services evolve independently.
   - **Cons**: Difficult to trace overall business process state; no central point of coordination.
2. **Orchestration (Centralized / Controlled)**:
   - A central coordinator (AWS Step Functions) explicitly directs participating services on what to execute and when.
   - The orchestrator manages retries, conditional branching, timeouts, and rollbacks.
   - **Pros**: Easy to monitor, debug, and enforce transactional consistency (Saga Pattern).
   - **Cons**: Orchestrator becomes a central coupling point if overused.
3. **The Hybrid Rule of Thumb**: Use **Orchestration *inside* the microservice boundary** (to manage complex multi-step workflows like order fulfillment), and use **Choreography *between* microservice boundaries** (emitting domain events to notify other bounded contexts).

### 2.2 The Transactional Outbox Pattern
- **Problem**: Writing to a database (e.g., PostgreSQL or DynamoDB) and publishing an event to a message broker (SNS/EventBridge) cannot be wrapped in a single distributed transaction without fragile two-phase commits.
- **Solution**:
  - The microservice saves the business entity *and* an "outbox" event record in the **same local database transaction**.
  - A separate background process or Change Data Capture (CDC) mechanism—such as **DynamoDB Streams + EventBridge Pipes** or **AWS DMS**—reads the outbox table and reliably publishes the event to the event bus.
  - Guarantees **at-least-once publishing** with zero dual-write race conditions.

### 2.3 The Idempotent Consumer Pattern
- Because distributed event buses guarantee **at-least-once delivery**, consumers will inevitably receive duplicate messages due to network retries.
- An **idempotent consumer** ensures that receiving the same event multiple times produces the exact same side effects as receiving it once.
- Implemented using unique event IDs stored in a fast key-value store (Amazon DynamoDB or ElastiCache Redis) with conditional writes and TTLs.

### 2.4 The Claim Check Pattern
- Message brokers and event buses enforce payload limits (SQS: 256 KB; EventBridge: 256 KB; SNS: 256 KB).
- When events contain large payloads (e.g., video files, high-resolution scans, large JSON datasets), the producer uploads the raw payload to **Amazon S3** and places an event containing only the S3 bucket name and key (the "claim check") onto the event bus.
- The consumer retrieves the claim check, fetches the payload from S3, processes it, and cleans up.

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                        TRANSACTIONAL OUTBOX & CHOREOGRAPHY EVENT ARCHITECTURE                     |
|                                                                                                   |
|  [Client Browser]                                                                                 |
|         |                                                                                         |
|         v (HTTP POST /orders)                                                                     |
|  +---------------------------------------------------------------------------------------------+  |
|  | ORDER MICROSERVICE (Bounded Context A)                                                      |  |
|  |                                                                                             |  |
|  |   BEGIN TRANSACTION (Single ACID Write)                                                     |  |
|  |   1. INSERT INTO Orders (id, total, status="PENDING")                                       |  |
|  |   2. INSERT INTO Outbox (id, event_type="OrderPlaced", payload={...})                       |  |
|  |   COMMIT TRANSACTION                                                                        |  |
|  |                                                                                             |  |
|  |   +---------------------------------------------------------------------------------------+ |  |
|  |   | DYNAMODB STREAMS / DEBEZIUM CDC                                                       | |  |
|  |   | Asynchronously reads Outbox commit log stream (zero dual-write risk!)                  | |  |
|  |   +-------------------------------------------+-------------------------------------------+ |  |
|  +-----------------------------------------------|---------------------------------------------+  |
|                                                  |                                                |
|                                                  v (EventBridge Pipe)                             |
|  +---------------------------------------------------------------------------------------------+  |
|  | ENTERPRISE EVENT BUS (Amazon EventBridge)                                                   |  |
|  | Event: `{"source": "order.service", "detail-type": "OrderPlaced"}`                          |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                                |                        |
|         | (Rule: Route to Billing)                                       | (Rule: Route to Stock) |
|         v                                                                v                        |
|  +-----------------------------+                                  +-----------------------------+ |
|  | SQS QUEUE: Billing Ingest   |                                  | SQS QUEUE: Inventory Ingest | |
|  +--------------+--------------+                                  +--------------+--------------+ |
|                 |                                                                |                |
|                 v                                                                v                |
|  [BILLING SERVICE WORKER]                                         [INVENTORY SERVICE WORKER]      |
|  1. Checks DynamoDB Idempotency Table                             1. Checks Idempotency Table     |
|  2. If already processed -> ACK & skip                            2. Decrements stock levels      |
|  3. Charges customer via Stripe                                   3. Emits `InventoryReserved`    |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 CloudEvents Standard JSON Event Envelope

```json
{
  "version": "0",
  "id": "c2b3e4f5-9876-4321-abcd-ef0123456789",
  "detail-type": "OrderPlaced",
  "source": "ecommerce.orders",
  "account": "123456789012",
  "time": "2026-11-25T12:00:00Z",
  "region": "us-east-1",
  "resources": ["arn:aws:dynamodb:us-east-1:123456789012:table/Orders/ord-9021"],
  "detail": {
    "orderId": "ord-9021",
    "customerId": "cust-8812",
    "amount": 299.99,
    "currency": "USD",
    "items": [
      {"sku": "SKU-PROD-1", "quantity": 1, "price": 299.99}
    ]
  }
}
```

---

## 5. Practical Examples

### 5.1 Transactional Outbox Pattern with DynamoDB Streams & EventBridge Pipes (Terraform)

```hcl
# 1. DynamoDB Table with Outbox Pattern (Streams Enabled)
resource "aws_dynamodb_table" "orders_with_outbox" {
  name             = "OrdersTable"
  billing_mode     = "PAY_PER_REQUEST"
  hash_key         = "PK"
  range_key        = "SK"
  stream_enabled   = true
  stream_view_type = "NEW_IMAGE"

  attribute {
    name = "PK"
    type = "S"
  }
  attribute {
    name = "SK"
    type = "S"
  }
}

# 2. EventBridge Bus
resource "aws_cloudwatch_event_bus" "domain_bus" {
  name = "enterprise-domain-bus"
}

# 3. EventBridge Pipe streaming Outbox records to Event Bus
resource "aws_pipes_pipe" "outbox_pipe" {
  name     = "orders-outbox-to-eventbridge"
  role_arn = var.pipe_iam_role_arn
  source   = aws_dynamodb_table.orders_with_outbox.stream_arn
  target   = aws_cloudwatch_event_bus.domain_bus.arn

  source_parameters {
    dynamodb_stream_parameters {
      starting_position = "LATEST"
      batch_size        = 10
    }
    # Filter: Only process records where SK starts with "OUTBOX#"
    filter_criteria {
      filter {
        pattern = jsonencode({
          dynamodb = {
            NewImage = {
              SK = {
                S = [{ "prefix": "OUTBOX#" }]
              }
            }
          }
        })
      }
    }
  }

  target_parameters {
    eventbridge_event_bus_parameters {
      detail_type = "DomainEventPublished"
      source      = "orders.outbox"
    }
  }
}
```

### 5.2 Python Robust Idempotent Consumer using DynamoDB

```python
"""
Idempotent Event Consumer preventing duplicate execution.
"""
import boto3
import json
import time
from botocore.exceptions import ClientError

dynamodb = boto3.resource("dynamodb", region_name="us-east-1")
idempotency_table = dynamodb.Table("IdempotencyRecords")

def process_event(event: dict):
    event_id = event["id"]
    detail = event["detail"]
    order_id = detail["orderId"]

    # 1. Acquire Idempotency Lock via Conditional Put
    # Set TTL to 7 days (604,800 seconds)
    now = int(time.time())
    ttl = now + 604800

    try:
        idempotency_table.put_item(
            Item={
                "eventId": event_id,
                "status": "IN_PROGRESS",
                "createdAt": now,
                "ttl": ttl
            },
            ConditionExpression="attribute_not_exists(eventId)"
        )
    except ClientError as e:
        if e.response["Error"]["Code"] == "ConditionalCheckFailedException":
            print(f"Duplicate event {event_id} detected. Skipping processing.")
            return {"status": "DUPLICATE_SKIPPED"}
        raise e

    try:
        # 2. Execute Business Logic (e.g., Charge Payment)
        print(f"Processing order payment for order {order_id}...")
        # Simulate payment processing
        time.sleep(1)

        # 3. Mark Idempotency Record as COMPLETED
        idempotency_table.update_item(
            Key={"eventId": event_id},
            UpdateExpression="SET #s = :completed, completedAt = :now",
            ExpressionAttributeNames={"#s": "status"},
            ExpressionAttributeValues={":completed": "COMPLETED", ":now": int(time.time())}
        )
        return {"status": "SUCCESS"}
    except Exception as err:
        # On fatal failure, release lock so message can be retried by SQS/DLQ
        idempotency_table.delete_item(Key={"eventId": event_id})
        raise err
```

### 5.3 Claim Check Pattern with S3 and EventBridge

```python
"""
Producer & Consumer implementing the Claim Check Pattern for payloads > 256 KB.
"""
import boto3
import json
import uuid

s3 = boto3.client("s3")
events = boto3.client("events")
BUCKET_NAME = "enterprise-large-event-payloads-2026"

def publish_large_event(large_payload_dict: dict):
    payload_bytes = json.dumps(large_payload_dict).encode("utf-8")
    claim_id = str(uuid.uuid4())
    s3_key = f"claims/{claim_id}.json"

    # 1. Store payload in S3
    s3.put_object(Bucket=BUCKET_NAME, Key=s3_key, Body=payload_bytes)

    # 2. Publish lightweight claim check to EventBridge (< 1 KB)
    events.put_events(
        Entries=[
            {
                "Source": "analytics.processor",
                "DetailType": "LargeBatchAvailable",
                "Detail": json.dumps({
                    "claimCheckId": claim_id,
                    "s3Bucket": BUCKET_NAME,
                    "s3Key": s3_key,
                    "recordCount": len(large_payload_dict.get("records", []))
                }),
                "EventBusName": "enterprise-domain-bus"
            }
        ]
    )
    print(f"Claim check published: {claim_id}")

def consume_large_event(event_detail: dict):
    s3_bucket = event_detail["s3Bucket"]
    s3_key = event_detail["s3Key"]

    # 3. Retrieve raw payload from S3
    obj = s3.get_object(Bucket=s3_bucket, Key=s3_key)
    full_payload = json.loads(obj["Body"].read().decode("utf-8"))

    print(f"Retrieved large payload containing {len(full_payload['records'])} records.")
```

### 5.4 Circuit Breaker Pattern with Step Functions & DynamoDB

```json
{
  "CheckCircuitBreakerState": {
    "Type": "Task",
    "Resource": "arn:aws:states:::dynamodb:getItem",
    "Parameters": {
      "TableName": "CircuitBreakerState",
      "Key": {"serviceName": {"S": "ThirdPartyPaymentGateway"}}
    },
    "ResultPath": "$.circuitState",
    "Next": "EvaluateCircuit"
  },
  "EvaluateCircuit": {
    "Type": "Choice",
    "Choices": [
      {
        "Variable": "$.circuitState.Item.status.S",
        "StringEquals": "OPEN",
        "Next": "FastFailFallback"
      }
    ],
    "Default": "ExecuteProtectedCall"
  },
  "ExecuteProtectedCall": {
    "Type": "Task",
    "Resource": "arn:aws:states:::lambda:invoke",
    "Parameters": {"FunctionName": "CallThirdPartyPaymentApi"},
    "Catch": [
      {
        "ErrorEquals": ["States.ALL"],
        "Next": "TripCircuitBreaker"
      }
    ],
    "End": true
  },
  "FastFailFallback": {
    "Type": "Fail",
    "Cause": "Circuit breaker is OPEN. Payment gateway is degraded.",
    "Error": "CircuitBreakerOpenException"
  },
  "TripCircuitBreaker": {
    "Type": "Task",
    "Resource": "arn:aws:states:::dynamodb:updateItem",
    "Parameters": {
      "TableName": "CircuitBreakerState",
      "Key": {"serviceName": {"S": "ThirdPartyPaymentGateway"}},
      "UpdateExpression": "SET #s = :open, failureCount = failureCount + :inc",
      "ExpressionAttributeNames": {"#s": "status"},
      "ExpressionAttributeValues": {":open": {"S": "OPEN"}, ":inc": {"N": "1"}}
    },
    "Next": "FastFailFallback"
  }
}
```

---

## 6. Best Practices

1. **Never Perform Dual-Writes**: Never write to a database and call an API/event bus sequentially in application code. Use the **Transactional Outbox Pattern** with CDC (DynamoDB Streams or Debezium) to ensure atomic persistence and guaranteed event delivery.
2. **Design All Event Consumers for Idempotency**: Assume duplicate messages *will* arrive due to at-least-once delivery guarantees. Use DynamoDB conditional writes with TTLs on unique event IDs.
3. **Use Orchestration Within Boundaries, Choreography Across Boundaries**: Orchestrate complex internal workflows using AWS Step Functions; choreograph cross-microservice communication using Amazon EventBridge.
4. **Enforce Semantic Schema Versioning**: Never mutate existing event fields in breaking ways. If a payload format changes, add optional fields or increment the event version (`OrderPlaced.v2`) to allow consumers to migrate gradually.
5. **Implement Dead-Letter Queues (DLQs) on All Event Subscriptions**: Every asynchronous consumer (EventBridge target, SQS queue, Lambda event source mapping) must have a dedicated DLQ monitored by CloudWatch alarms.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Dual-Write Inconsistency | App writes to DB, but crashes before calling `events:putEvents`. | Database state exists, but downstream services are never notified. | Implement Transactional Outbox with DynamoDB Streams / CDC. |
| Non-Idempotent Consumers | Consumer charges credit card without checking duplicate event ID. | Customer charged twice on network retry. | Enforce unique event ID checks in DynamoDB before processing. |
| Choreography Event Loop | Service A emits event -> triggers Service B -> emits event -> triggers Service A. | Runaway infinite loop costing thousands in AWS billing. | Use distinct event types and inspect event causation chains. |
| Exceeding 256 KB Event Limit | Publishing raw binary or 10 MB JSON directly to EventBridge. | Event rejected with `ValidationException`. | Use the Claim Check Pattern (store payload in S3; publish S3 URI). |
| Breaking Payload Schema Changes | Removing a field from an event payload relied upon by other teams. | Downstream microservices crash on deserialization. | Treat events as public immutable contracts; increment schema versions. |
| Missing DLQ on Event Targets | EventBridge target Lambda function times out repeatedly. | Undelivered events are dropped permanently after retries. | Attach an SQS Dead-Letter Queue to the EventBridge rule target. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance & Throughput
- **Event Latency**: Asynchronous event routing through EventBridge to SQS takes approximately 20ms - 50ms.
- **Backpressure Absorption**: SQS queues fronting worker fleets absorb sudden 100x traffic surges, buffering events and preventing database exhaustion.

### 8.2 Security
- **Cross-Account Event Ingestion**: EventBridge bus resource policies allow secure cross-account event routing without exposing public API endpoints or IAM credentials.
- **Least-Privilege Envelope Filtering**: EventBridge rules filter and pass only necessary event fields to targets using **Input Transformers**, preventing sensitive data leakage.

### 8.3 Cost Optimization
- **Event Filtering at the Bus**: Discard unwanted events at the EventBridge rule level before they trigger billable Lambda invocations or Step Functions state transitions.

---

## 9. Comparison Table

| Architecture Pattern | Primary Benefit | Primary Trade-Off | Key AWS Service |
|---|---|---|---|
| **Choreography (Pub/Sub)** | Maximum decoupling, autonomous teams | Hard to trace global process state| Amazon EventBridge, Amazon SNS |
| **Orchestration (Saga)** | Explicit state tracking, easy rollback| Centralized coordinator point | AWS Step Functions |
| **Transactional Outbox** | Eliminates dual-write data loss | Small eventual consistency lag | DynamoDB Streams, EventBridge Pipes|
| **Claim Check** | Bypasses 256 KB message size limits | Extra S3 read/write round trip | Amazon S3, SQS, EventBridge |
| **Idempotent Consumer** | Prevents duplicate processing | Requires fast state lookup store | Amazon DynamoDB (Conditional Put) |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the Dual-Write Problem in microservices, and how does the Transactional Outbox Pattern resolve it on AWS?</strong></summary>

- **The Problem**: A microservice updates its database and publishes an event to an event bus (e.g., EventBridge). If the database write succeeds but the application crashes before the event is published, downstream systems never learn of the change. If the order is reversed and the event is sent first, the database write could fail, broadcasting a phantom event.
- **The Outbox Solution**: The microservice writes the business entity and an event record into an `Outbox` table within a **single local ACID transaction**. An asynchronous Change Data Capture (CDC) process—such as **DynamoDB Streams connected to an EventBridge Pipe**—tails the commit log and reliably publishes the event to EventBridge. If the publisher crashes, the stream iterator resumes from the last checkpoint, guaranteeing at-least-once publishing with zero data loss.
</details>

<details>
<summary><strong>Q2: When should you use Event Choreography versus Event Orchestration?</strong></summary>

- **Orchestration** is preferred **within a single bounded context or microservice** where a complex, multi-step transaction requires strict control, timeouts, and automated compensating rollbacks (e.g., an Order Fulfillment workflow coordinating Payment, Inventory, and Shipping using AWS Step Functions).
- **Choreography** is preferred **between independent bounded contexts or separate microservices** where services should not be tightly coupled to each other's lifecycles. For example, when an order is fulfilled, the Order Service publishes an `OrderPlaced` event to EventBridge. The Marketing, Billing, and Analytics services independently consume the event without the Order Service knowing who they are.
</details>

<details>
<summary><strong>Q3: How do you design an Idempotent Consumer in an asynchronous serverless pipeline?</strong></summary>

1. Ensure every published event contains a globally unique identifier (e.g., `id: "uuid-v4"`).
2. When the consumer worker (Lambda/ECS) receives the event, it attempts a **Conditional Put** into a dedicated DynamoDB table:
   `attribute_not_exists(eventId)` with a record status of `IN_PROGRESS` and a TTL of 7 days.
3. If the conditional check fails, the event is recognized as a duplicate and the consumer immediately returns success (ACK) without executing business logic.
4. If the conditional check succeeds, the worker executes the business logic and updates the record status to `COMPLETED`.
5. If the worker encounters a retryable error, it deletes the lock record so the message can be safely retried.
</details>

<details>
<summary><strong>Q4: Explain the Claim Check Pattern and describe how it interacts with AWS service limits.</strong></summary>

AWS messaging services have strict payload size limits: Amazon SQS, Amazon SNS, and Amazon EventBridge all cap messages at **256 KB**. 
When an event payload exceeds this limit (e.g., a batch processing payload or medical imaging scan):
1. The producer writes the complete payload to an **Amazon S3 bucket**.
2. The producer publishes an event to EventBridge or SQS containing only the metadata reference (the "claim check"): `{"s3Bucket": "...", "s3Key": "..."}`.
3. The consumer intercepts the lightweight event, downloads the full payload from S3, executes processing, and cleans up the S3 object if appropriate.
</details>

<details>
<summary><strong>Q5: How do you prevent event loops in choreographed event-driven architectures?</strong></summary>

Event loops occur when Service A publishes Event 1, which triggers Service B to publish Event 2, which inadvertently triggers Service A to publish Event 1 again.
To prevent loops:
1. **Event Naming**: Distinguish between "Facts/Events" (`OrderPlaced`, `PaymentProcessed`) and "Commands" (`ProcessPayment`).
2. **Causation IDs**: Include a `causationId` and `correlationId` in the event metadata envelope. Consumers verify they are not reacting to an event they originally initiated.
3. **EventBridge Rule Precision**: Write strict EventBridge event patterns matching exact source and detail-type values rather than broad wildcard prefixes.
</details>

<details>
<summary><strong>Q6: What is the Circuit Breaker Pattern and how can it be implemented on AWS?</strong></summary>

The Circuit Breaker pattern prevents an application from repeatedly invoking a failing downstream third-party service or database, allowing the failing service time to recover.
- **Closed State**: Normal operation; calls pass through.
- **Open State**: If failure rate exceeds a threshold, the circuit trips "Open". Inbound calls immediately fail fast without hitting the downstream service.
- **Half-Open State**: After a timeout, a small fraction of requests are allowed through. If they succeed, the circuit resets to "Closed".
In AWS, this is typically implemented using **AWS Step Functions** or Lambda querying a centralized state flag in **Amazon ElastiCache Redis** or **Amazon DynamoDB**.
</details>

<details>
<summary><strong>Q7: How do you handle schema evolution and backward compatibility in event-driven systems?</strong></summary>

1. **Additive Changes Only**: Never rename or delete existing fields in an event payload. Always add new fields as optional.
2. **Schema Registry Validation**: Use **Amazon EventBridge Schema Registry** to discover and validate event schemas in CI/CD pipelines before deployment.
3. **Explicit Versioning**: If a breaking change is unavoidable, emit a new event type (e.g., `OrderPlaced.v2`). Maintain parallel processing for both versions until all consuming teams have migrated their subscriber rules.
</details>

<details>
<summary><strong>Q8: What is the role of a Dead-Letter Queue (DLQ) in an event-driven system?</strong></summary>

A DLQ acts as a safety buffer for events that cannot be processed after exhausting all configured retry attempts (due to malformed JSON payloads, schema mismatches, or unhandled exceptions). 
Without a DLQ, failed events are permanently discarded. By routing failed events to an SQS DLQ, developers can trigger CloudWatch alarms, investigate the root cause, fix the issue, and redrive the messages back to the primary processing queue.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Implement an Idempotency Check in Python with DynamoDB</strong></summary>

**Goal**: Block duplicate event processing.
1. Create a DynamoDB table `Idempotency` with partition key `eventId`.
2. Write a Python script that takes an event ID and executes a conditional put.
3. Run the script twice with the same event ID; confirm the second attempt catches `ConditionalCheckFailedException` and skips execution.
</details>

<details>
<summary><strong>Exercise 2: Build a Transactional Outbox Pipeline</strong></summary>

**Goal**: Stream database changes to EventBridge.
1. Create a DynamoDB table with Streams enabled.
2. Create an EventBridge custom event bus.
3. Create an EventBridge Pipe that triggers on stream inserts with prefix `OUTBOX#` and pushes events to the bus.
4. Insert a test outbox item into DynamoDB and verify the event appears on the EventBridge bus.
</details>

<details>
<summary><strong>Exercise 3: Simulate the Claim Check Pattern</strong></summary>

**Goal**: Pass payloads larger than 256 KB.
1. Generate a 1 MB random string in Python.
2. Upload the string to S3 and generate a claim check JSON envelope.
3. Publish the claim check to an SQS queue.
4. Consume the message from SQS, download the 1 MB payload from S3, and verify integrity.
</details>

<details>
<summary><strong>Exercise 4: Choreograph Multi-Service Event Routing</strong></summary>

**Goal**: Route an event to two distinct microservice SQS queues.
1. Create an EventBridge bus.
2. Create two SQS queues: `billing-queue` and `analytics-queue`.
3. Create two EventBridge rules routing `OrderPlaced` events to both queues.
4. Publish a test event and confirm both queues receive the message concurrently.
</details>

<details>
<summary><strong>Exercise 5: Configure Dead-Letter Queue for an EventBridge Rule</strong></summary>

**Goal**: Capture failed event deliveries.
1. Create a DLQ SQS queue.
2. Create an EventBridge rule targeting a Lambda function that has intentionally revoked invocation permissions.
3. Attach the DLQ to the target configuration.
4. Publish an event; verify delivery fails and the event is routed to the SQS DLQ.
</details>

<details>
<summary><strong>Exercise 6: Test CloudEvents Envelope Formatting</strong></summary>

**Goal**: Construct and validate an industry-standard CloudEvent.
1. Structure a JSON event following the CloudEvents v1.0 specification.
2. Put the event to an EventBridge bus using `aws events put-events`.
3. Verify that target consumers can parse standard attributes (`source`, `type`, `time`, `data`).
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| EVENT-DRIVEN DESIGN PATTERNS QUICK REFERENCE                                                      |
+--------------------------+-------------------------------------+----------------------------------+
| Pattern                  | Core Problem Solved                 | Primary AWS Mechanism            |
+--------------------------+-------------------------------------+----------------------------------+
| Transactional Outbox     | Dual-write inconsistency            | DynamoDB Streams + Pipes         |
| Idempotent Consumer      | Duplicate event delivery            | DynamoDB Conditional Put + TTL   |
| Claim Check              | 256 KB message payload limits       | Amazon S3 + EventBridge / SQS    |
| Saga Pattern             | Distributed multi-service rollbacks | AWS Step Functions (Catch/Retry) |
| Dead-Letter Queue (DLQ)  | Poison pill message loss            | Amazon SQS DLQ + CloudWatch Alarm|
| Circuit Breaker          | Cascading third-party API failures  | DynamoDB State + Step Functions  |
+--------------------------+-------------------------------------+----------------------------------+
```

- **Transactional Outbox**: Atomic database persistence + guaranteed CDC event emission.
- **Idempotency**: Mandatory protection against duplicate processing in distributed systems.
- **Choreography + Orchestration**: The gold standard for modern microservice decoupling.

---

## 13. Further Reading

- [AWS Prescriptive Guidance: Event-Driven Architecture](https://docs.aws.amazon.com/prescriptive-guidance/latest/patterns/implement-the-transactional-outbox-pattern.html)
- [CloudEvents Open Specification](https://cloudevents.io/)
- [Building Idempotent Lambda Functions with AWS Lambda Powertools](https://docs.powertools.aws.dev/lambda/python/latest/utilities/idempotency/)
- [Saga Pattern on AWS Reference Architecture](https://aws.amazon.com/blogs/compute/building-a-serverless-distributed-application-using-a-saga-pattern/)
- [05-app-services/02-sns-and-eventbridge.md](file:///04-devops/aws/05-app-services/02-sns-and-eventbridge.md)
