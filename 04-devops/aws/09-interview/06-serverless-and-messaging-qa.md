---
topic: "AWS Serverless, Messaging & Event-Driven Architecture Technical Interview Q&A"
module: "09-interview"
track: "aws"
difficulty: "advanced"
last-tested: "2026-11"
tags:
  - "interview"
  - "sqs"
  - "sns"
  - "eventbridge"
  - "step-functions"
  - "api-gateway"
  - "messaging"
---

# AWS Serverless, Messaging & Event-Driven Architecture Technical Interview Q&A

## Table of Contents
- [1. Queueing: Amazon SQS Internals & Reliability](#1-queueing-amazon-sqs-internals--reliability)
- [2. Pub/Sub & Event Routing: SNS vs EventBridge](#2-pubsub--event-routing-sns-vs-eventbridge)
- [3. Workflow Orchestration: AWS Step Functions](#3-workflow-orchestration-aws-step-functions)
- [4. API Management: Amazon API Gateway](#4-api-management-amazon-api-gateway)
- [5. Deep-Dive Messaging Scenarios & Distributed Failure Triage](#5-deep-dive-messaging-scenarios--distributed-failure-triage)

---

## 1. Queueing: Amazon SQS Internals & Reliability

### Q1: Compare Amazon SQS Standard Queues with SQS FIFO Queues across ordering, delivery guarantees, throughput, and deduplication.
**Answer**:

| Feature / Dimension | SQS Standard Queue | SQS FIFO (First-In-First-Out) Queue |
|:---|:---|:---|
| **Message Ordering** | **Best-effort ordering**. (Messages can arrive out of order). | **Strictly preserved order** within a Message Group ID. |
| **Delivery Guarantee** | **At-least-once delivery**. (Occasional duplicate messages delivered). | **Exactly-once processing** (Within 5-minute deduplication window). |
| **Throughput Capacity** | **Nearly unlimited transactions per second**. | Standard FIFO: 300 msg/s (3,000 with batching). High Throughput FIFO: up to 70,000 msg/s. |
| **Deduplication** | None (Consumer must implement idempotency). | **Automated** via Content-Based Deduplication or explicit Deduplication ID. |
| **Queue Naming** | Any alphanumeric name. | Name **must end with the `.fifo` suffix**. |
| **Pricing** | \$0.40 per 1 million requests. | \$0.50 per 1 million requests. |

### Q2: What is SQS Long Polling and why should it always be enabled?
**Answer**:
When an application polls SQS using standard **Short Polling** (`WaitTimeSeconds = 0`), SQS samples a subset of its distributed storage servers and returns immediately—even if no messages were found in that specific sample. This results in empty responses, high CPU loops, and massive API costs (paying for empty `ReceiveMessage` calls).

In **Long Polling** (`WaitTimeSeconds = 1 to 20` seconds):
- When the consumer requests messages, SQS waits up to 20 seconds for messages to arrive across all servers before returning an empty response.
- As soon as a message arrives, it is delivered to the consumer immediately.
- **Benefits**:
  1. **Slashing Costs**: Eliminates empty responses, reducing SQS billing costs by up to 90%.
  2. **Lower Latency**: Delivers messages to consumers the instant they arrive in the queue.
  3. **Zero False Empties**: Ensures all distributed storage nodes are queried.

### Q3: What is the SQS Visibility Timeout, and what happens if a consumer takes longer to process a message than the configured timeout?
**Answer**:
When a consumer pulls a message from SQS, the message is not deleted; instead, it enters the **Visibility Timeout period**, during which it is rendered invisible to all other consumers.
- **The Intended Flow**: The consumer finishes processing the message and issues an explicit `DeleteMessage` API call using the receipt handle before the timeout expires.
- **The Failure Mode (Timeout Expiration)**: If the consumer takes 45 seconds to process a message, but the SQS Visibility Timeout is configured to 30 seconds:
  1. At second 30, SQS assumes the original consumer crashed or died.
  2. The message **becomes visible again in the queue**.
  3. A second worker pulls the identical message and begins processing it concurrently.
  4. The first worker finishes at second 45 and calls `DeleteMessage`, but the second worker is now executing a duplicate operation.
- *Best Practice*: Set the SQS Visibility Timeout to **at least 6 times the consumer processing timeout** (or use the `ChangeMessageVisibility` heartbeat API during long-running tasks).

---

## 2. Pub/Sub & Event Routing: SNS vs EventBridge

### Q4: When should an architect choose Amazon EventBridge over Amazon SNS?
**Answer**:
Both services provide asynchronous decoupled messaging, but serve distinct architectural patterns:
- **Choose Amazon EventBridge when**:
  1. **Content-Based Routing**: You need to inspect deep nested JSON event payloads to make routing decisions (e.g., route only if `detail.transaction.amount > 500` AND `detail.billing.country == "US"`).
  2. **Native SaaS Integrations**: You are ingesting events directly from external SaaS partners (Salesforce, Datadog, Zendesk, Auth0, Shopify).
  3. **AWS Service Events**: You need to react to native AWS operational state changes (e.g., an EC2 instance state change, an S3 object created, a GuardDuty finding).
  4. **Schema Discovery**: You want automated schema registration, versioning, and code bindings via the EventBridge Schema Registry.
- **Choose Amazon SNS when**:
  1. **Massive Fan-out Throughput**: You need to broadcast a notification simultaneously to millions of mobile push endpoints (APNs, FCM), SMS messages, or HTTP webhooks.
  2. **Lowest Latency**: Typical SNS propagation latency is sub-30ms, compared to ~50-100ms for EventBridge.
  3. **Simpler Message Attribute Filtering**: Basic key-value attribute matching is sufficient.

---

## 3. Workflow Orchestration: AWS Step Functions

### Q5: Compare AWS Step Functions Standard Workflows with Express Workflows.
**Answer**:

| Feature / Dimension | Standard Workflows | Express Workflows |
|:---|:---|:---|
| **Maximum Duration** | **Up to 1 year** | **Up to 5 minutes** |
| **Execution Rate** | Up to 2,000 executions / second | **Over 100,000 executions / second** |
| **Execution Model** | **Exactly-once execution** guarantee | **At-least-once execution** guarantee |
| **Pricing Model** | **Per State Transition** (\$0.025 per 1,000 transitions) | **Per duration & memory** (Similar to Lambda: GB-seconds) |
| **Execution History** | Full visual execution history in console (90 days) | Streamed to CloudWatch Logs (No visual console replay) |
| **Best For** | Long-running business processes, human approvals, complex sagas | High-throughput streaming, IoT event processing, microservice orchestration |

### Q6: How do you implement the Saga Pattern in AWS Step Functions to maintain data consistency without distributed two-phase commit?
**Answer**:
In a distributed microservice architecture, a single business transaction (e.g., Book Vacation) spans multiple independent services: (1) Reserve Flight, (2) Book Hotel, (3) Charge Credit Card. If Step 3 fails, the system cannot use distributed ACID locks.

**Step Functions Saga Implementation**:
1. **Forward Steps**: The state machine executes tasks sequentially:
   - `ReserveFlight` $\rightarrow$ `BookHotel` $\rightarrow$ `ChargeCard`.
2. **Catch Blocks**: Every state defines a `Catch` block specifying compensating actions:
   ```json
   "BookHotel": {
     "Type": "Task",
     "Resource": "arn:aws:lambda:...:function:book-hotel",
     "Catch": [
       {
         "ErrorEquals": ["States.ALL"],
         "Next": "CancelFlightCompensation"
       }
     ],
     "Next": "ChargeCard"
   }
   ```
3. **Compensating Actions**: If `BookHotel` or `ChargeCard` fails, the state machine branches to compensating Lambda tasks (`CancelHotelReservation`, `RefundCard`, `CancelFlightReservation`) in reverse order, returning all distributed systems to a semantically consistent state.

---

## 4. API Management: Amazon API Gateway

### Q7: Detailed comparison: Amazon API Gateway HTTP APIs vs REST APIs.
**Answer**:

| Feature / Capability | HTTP APIs (v2) | REST APIs (v1) |
|:---|:---|:---|
| **Latency Overhead** | **Ultra-low (< 10 ms overhead)** | Moderate (~25-40 ms overhead) |
| **Cost / 1 Million Requests** | **\$1.00 / million** | **\$3.50 / million** (Over 3x more expensive!) |
| **Native JWT Authorizers** | **Built-in** (Direct Cognito / OIDC token validation) | Requires custom Lambda Authorizer |
| **CORS Configuration** | One-click native CORS | Complex mock response & option methods |
| **Request / Response Validation**| Basic schema validation | **Full JSON Schema request body validation** |
| **Request Transformation (VTL)**| Not supported | **Full Velocity Template Language (VTL)** mapping |
| **API Keys & Usage Plans** | Not supported | **Supported natively** |
| **AWS WAF Integration** | Via CloudFront distribution | **Direct association** with API Gateway stage |
| **Private VPC APIs** | Supported via PrivateLink | Supported via Interface Endpoints |

*Decision Rule*: Use **HTTP APIs** for 90% of modern microservices and web APIs to save 60-70% on cost and achieve lower latency. Reserve **REST APIs** only when you specifically require API Keys with Usage Plans, VTL request transformations, or direct WAF association on the stage.

---

## 5. Deep-Dive Messaging Scenarios & Distributed Failure Triage

### Scenario 1: Debugging an SQS Dead-Letter Queue Flooding Outage
**Interviewer**: *"You receive a high-severity pager alert: Your SQS Dead-Letter Queue (DLQ) is receiving 500 failed messages per minute. Downstream inventory updates are halted. How do you triage this incident without losing customer orders?"*

**Structured Candidate Response**:
1. **Immediate Assessment (Prevent Data Loss)**:
   - SQS DLQs retain messages for up to 14 days by default. Customers' orders are not lost; they are safely isolated.
   - Do **NOT** purge the DLQ.
2. **Inspect the Poison Pill Payloads**:
   - Pull sample messages from the DLQ via AWS CLI:
     ```bash
     aws sqs receive-message --queue-url <dlq-url> --max-number-of-messages 5 --attribute-names All
     ```
   - Inspect the message attributes and JSON body.
   - Check the `ApproximateReceiveCount`: If it equals `maxReceiveCount` (e.g., 3), the worker attempted and failed processing 3 times.
3. **Inspect Consumer Worker CloudWatch Logs**:
   - Search the consumer Lambda or ECS worker logs for unhandled exceptions matching the message timestamps.
   - Look for common causes:
     - *Third-party API outage* (e.g., Payment Gateway returning HTTP 503).
     - *Schema mismatch* (e.g., an upstream service deployed a breaking schema change with missing fields).
     - *Database lock timeouts*.
4. **Fix and Redrive**:
   - If caused by downstream API downtime: Wait for the downstream system to recover.
   - If caused by a code bug: Deploy a software patch to the consumer Lambda function.
   - Once workers are healthy, execute an automated **SQS Dead-Letter Queue Redrive**:
     ```bash
     aws sqs start-message-move-task \
         --source-arn <dlq-arn> \
         --destination-arn <main-queue-arn>
     ```
   - SQS automatically transfers the failed messages back to the primary queue for successful reprocessing.
