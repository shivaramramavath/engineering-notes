# Amazon SNS & Amazon EventBridge: Enterprise Pub/Sub & Event Buses

> **Module**: `05-app-services`  
> **Topic**: Publish/Subscribe Messaging, Fan-Out Patterns, EventBridge Buses, Schema Registry & Pipes  
> **Target Audience**: Cloud Architects, Backend Engineers, Event-Driven Systems Designers  
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

In traditional enterprise software, services notify each other by registering webhooks or executing direct point-to-point HTTP calls. As microservices multiply, this leads to an unmaintainable "spaghetti architecture" where a change in an order state requires the ordering service to know about, authenticate with, and send requests to the inventory service, billing service, notification service, analytics pipeline, and shipping partners.

AWS solves this architectural anti-pattern through decoupled publish/subscribe and event routing services:
1. **Amazon SNS (Simple Notification Service)**: A high-throughput, push-based publish/subscribe messaging service that delivers messages from publishers to millions of subscribers (SQS queues, Lambda functions, HTTP endpoints, SMS, email, mobile push).
2. **Amazon EventBridge**: A serverless event bus service that routes events using sophisticated content-based pattern matching, connects SaaS applications (Datadog, Zendesk, Salesforce) directly to AWS, generates typed event schemas, and integrates point-to-point event streaming pipelines via **EventBridge Pipes**.

Together, SNS and EventBridge form the reactive backbone of modern serverless and event-driven architectures (EDA).

---

## 2. Core Concepts

### 2.1 Amazon SNS (Simple Notification Service)
- **Topics**: Named channels where publishers send messages. Publishers have zero awareness of who or how many consumers are subscribed.
- **Subscribers**: Endpoints that receive pushed copies of every published message: SQS queues, AWS Lambda, HTTP/HTTPS webhooks, SMS, Email, Application Push Notifications.
- **The Fan-Out Pattern**: A single publisher publishes to an SNS topic, which instantaneously broadcasts the message to multiple SQS queues. Each consuming microservice owns its own SQS queue, allowing workers to process events independently and at their own pace.
- **Message Filtering Policies**: JSON filters applied at the subscription level. Subscribers receive only messages whose attributes match their criteria, eliminating unnecessary invocations and processing.
- **SNS FIFO Topics**: Delivers strict ordering and deduplication across multiple FIFO SQS subscribers.

### 2.2 Amazon EventBridge (Formerly CloudWatch Events)
- **Event Buses**:
  - *Default Event Bus*: Receives events from all native AWS services (e.g., EC2 state change, S3 object creation via CloudTrail, GuardDuty findings).
  - *Custom Event Bus*: Created by developers to receive custom domain events from applications (`OrderCreated`, `UserRegistered`).
  - *Partner Event Bus*: Receives events directly from integrated third-party SaaS providers without custom webhook polling.
- **Rules & Event Patterns**: JSON-based pattern matching rules that evaluate incoming event envelopes and dispatch matches to up to 5 targets.
- **EventBridge Schema Registry**: Discovers, generates, and manages schemas for events on your buses. Can generate typed code bindings (TypeScript, Python, Java) for developer IDEs.
- **EventBridge Pipes**: Point-to-point integrations connecting event sources (SQS, DynamoDB Streams, Kinesis) to targets with optional filtering and enrichment steps (Lambda, Step Functions) without glue code.
- **EventBridge API Destinations**: Directly routes events to external HTTP APIs with managed connection authentication (OAuth, API keys).

---

## 3. How It Works Internally

```
+---------------------------------------------------------------------------------------------------+
|                        SNS FAN-OUT VS EVENTBRIDGE ROUTING ARCHITECTURES                           |
|                                                                                                   |
|  PATTERN 1: SNS FAN-OUT PATTERN (HIGH-THROUGHPUT BROADCAST)                                       |
|                                                                                                   |
|  [Checkout Service]                                                                               |
|         |                                                                                         |
|         v (Publish: OrderCreated)                                                                 |
|  +---------------------------------------------------------------------------------------------+  |
|  | AMAZON SNS TOPIC (orders-topic)                                                             |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                 |                                 |                     |
|         v (Filter: amount > 1000)         v (Filter: none)                  v (Filter: tier=vip)  |
|  +---------------+               +---------------+               +---------------+                |
|  | SQS Queue:    |               | SQS Queue:    |               | SQS Queue:    |                |
|  | Fraud Check   |               | Fulfillment   |               | Loyalty VIP   |                |
|  +---------------+               +---------------+               +---------------+                |
|                                                                                                   |
|  -----------------------------------------------------------------------------------------------  |
|                                                                                                   |
|  PATTERN 2: EVENTBRIDGE CONTENT-BASED ENTERPRISE ROUTING                                          |
|                                                                                                   |
|  [Custom App]         [Salesforce SaaS]       [AWS S3 / GuardDuty]                                |
|         |                     |                       |                                           |
|         +---------------------+-----------------------+                                           |
|                               |                                                                   |
|                               v                                                                   |
|  +---------------------------------------------------------------------------------------------+  |
|  | AMAZON EVENTBRIDGE BUS (app-event-bus)                                                      |  |
|  +---------------------------------------------------------------------------------------------+  |
|         |                                                   |                                     |
|         v (Rule 1: detail-type == "PaymentFailed")          v (Rule 2: detail.amount > 50000)     |
|  +--------------------------------+               +-----------------------------------+           |
|  | Target: Step Functions Saga    |               | Target: API Destination (Slack)   |           |
|  +--------------------------------+               +-----------------------------------+           |
+---------------------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 4.1 Creating SNS Topic and Subscription via AWS CLI

```bash
# 1. Create an SNS Topic
aws sns create-topic \
    --name "order-events" \
    --attributes DisplayName="OrderNotifications"

# 2. Subscribe an SQS Queue to the SNS Topic
aws sns subscribe \
    --topic-arn "arn:aws:sns:us-east-1:123456789012:order-events" \
    --protocol "sqs" \
    --notification-endpoint "arn:aws:sqs:us-east-1:123456789012:billing-orders-queue" \
    --return-subscription-arn

# 3. Publish an Event to the SNS Topic
aws sns publish \
    --topic-arn "arn:aws:sns:us-east-1:123456789012:order-events" \
    --message '{"orderId": "ord-1001", "total": 250.00}' \
    --message-attributes '{
      "event_type": {
        "DataType": "String",
        "StringValue": "order_placed"
      }
    }'
```

### 4.2 Putting Custom Events to EventBridge via AWS CLI

```bash
aws events put-events \
    --entries '[
      {
        "Source": "ecommerce.orders",
        "DetailType": "OrderPlaced",
        "Detail": "{\"orderId\": \"ord-9021\", \"amount\": 1250.00, \"region\": \"EU\"}",
        "EventBusName": "production-event-bus"
      }
    ]'
```

---

## 5. Practical Examples

### 5.1 SNS Fan-Out to Multiple SQS Queues with Subscription Filter Policies (Terraform)

```hcl
# SNS Topic
resource "aws_sns_topic" "orders" {
  name = "enterprise-orders-topic"
}

# Queue A: Fraud Detection (Subscribes only to high-value orders)
resource "aws_sqs_queue" "fraud_queue" {
  name = "fraud-detection-queue"
}

# Queue B: Fulfillment (Subscribes to all orders)
resource "aws_sqs_queue" "fulfillment_queue" {
  name = "order-fulfillment-queue"
}

# Allow SNS to send messages to Fraud Queue
resource "aws_sqs_queue_policy" "fraud_queue_policy" {
  queue_url = aws_sqs_queue.fraud_queue.id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [{
      Effect    = "Allow"
      Principal = { Service = "sns.amazonaws.com" }
      Action    = "sqs:SendMessage"
      Resource  = aws_sqs_queue.fraud_queue.arn
      Condition = {
        ArnEquals = { "aws:SourceArn" = aws_sns_topic.orders.arn }
      }
    }]
  })
}

# Subscription with Filter Policy: only if high_risk == true OR total >= 500
resource "aws_sns_topic_subscription" "fraud_sub" {
  topic_arn = aws_sns_topic.orders.arn
  protocol  = "sqs"
  endpoint  = aws_sqs_queue.fraud_queue.arn

  filter_policy = jsonencode({
    high_risk = ["true"]
    total     = [{ "numeric" = [">=", 500] }]
  })
}

# Subscription for Fulfillment (All orders)
resource "aws_sns_topic_subscription" "fulfillment_sub" {
  topic_arn = aws_sns_topic.orders.arn
  protocol  = "sqs"
  endpoint  = aws_sqs_queue.fulfillment_queue.arn
}
```

### 5.2 EventBridge Custom Event Bus with Content-Based Rule (Terraform)

```hcl
# Custom Event Bus
resource "aws_cloudwatch_event_bus" "app_bus" {
  name = "corp-application-bus"
}

# Rule matching Order Placed events where amount > 1000
resource "aws_cloudwatch_event_rule" "high_value_orders" {
  name           = "high-value-order-rule"
  description    = "Capture orders exceeding $1000"
  event_bus_name = aws_cloudwatch_event_bus.app_bus.name

  event_pattern = jsonencode({
    "source"      : ["ecommerce.orders"],
    "detail-type" : ["OrderPlaced"],
    "detail" : {
      "amount" : [{ "numeric" : [">", 1000] }]
    }
  })
}

# Target: Step Functions State Machine
resource "aws_cloudwatch_event_target" "step_functions_target" {
  rule           = aws_cloudwatch_event_rule.high_value_orders.name
  event_bus_name = aws_cloudwatch_event_bus.app_bus.name
  arn            = var.step_functions_arn
  role_arn       = var.eventbridge_execution_role_arn
}
```

### 5.3 EventBridge Pipe: SQS to Step Functions with Lambda Enrichment

```hcl
resource "aws_pipes_pipe" "order_pipeline" {
  name     = "orders-enrichment-pipe"
  role_arn = var.pipe_role_arn
  source   = aws_sqs_queue.orders_queue.arn
  target   = var.target_step_functions_arn

  source_parameters {
    sqs_queue_parameters {
      batch_size                         = 10
      maximum_batching_window_in_seconds = 5
    }
  }

  enrichment = var.enrichment_lambda_arn

  target_parameters {
    step_function_state_machine_parameters {
      invocation_type = "FIRE_AND_FORGET"
    }
  }
}
```

### 5.4 EventBridge API Destination Routing to External Webhook (Slack / Datadog)

```hcl
# 1. Connection (Stores API Key / OAuth)
resource "aws_cloudwatch_event_connection" "slack_conn" {
  name               = "slack-webhook-connection"
  authorization_type = "API_KEY"

  auth_parameters {
    api_key {
      key   = "Authorization"
      value = "Bearer xoxb-slack-token-sample"
    }
  }
}

# 2. API Destination Endpoint
resource "aws_cloudwatch_event_api_destination" "slack_destination" {
  name                             = "slack-alerts-destination"
  connection_arn                   = aws_cloudwatch_event_connection.slack_conn.arn
  http_method                      = "POST"
  invocation_endpoint              = "https://hooks.slack.com/services/T00/B00/XXXX"
  invocation_rate_limit_per_second = 10
}
```

### 5.5 SNS Delivery Status Logging and Dead-Letter Queue

```hcl
# SNS Dead-Letter Queue for failed HTTP / SQS deliveries
resource "aws_sqs_queue" "sns_delivery_dlq" {
  name = "sns-undelivered-dlq"
}

resource "aws_sns_topic_subscription" "webhook_sub" {
  topic_arn     = aws_sns_topic.orders.arn
  protocol      = "https"
  endpoint      = "https://partner.enterprise.com/webhooks/orders"
  redrive_policy = jsonencode({
    deadLetterTargetArn = aws_sqs_queue.sns_delivery_dlq.arn
  })
}
```

---

## 6. Best Practices

1. **Use Fan-Out with SQS to Protect Downstream Systems**: Never subscribe HTTP webhooks or relational databases directly to high-volume SNS topics. Fan out into an SQS queue so the consumer can process messages at a controlled rate without being overwhelmed.
2. **Filter at the Subscription Level**: Apply SNS Filter Policies or EventBridge Rules at the source. This prevents sending millions of irrelevant messages to worker functions, reducing execution costs and queue bloat.
3. **Use EventBridge for Complex Content Routing, SNS for High-Scale Fan-Out**: SNS is built for ultra-high throughput ($>100,000$ msg/sec) and simple pub/sub broadcasting. EventBridge is built for intelligent, JSON payload routing with schema validation and third-party SaaS integrations.
4. **Attach DLQs to SNS Subscriptions**: When SNS delivers to HTTP endpoints or Lambda functions, downstream timeouts can cause drops. Configure subscription redrive policies with an SQS DLQ to preserve failed deliveries.
5. **Standardize on CloudEvents Specification**: When publishing custom events to EventBridge, follow the open [CloudEvents](https://cloudevents.io/) standard to ensure payload interoperability across polyglot microservices.

---

## 7. Common Mistakes

| Mistake | Root Cause | Impact | Fix |
|---|---|---|---|
| Forgetting SQS Policy for SNS Fan-Out | SQS queue policy does not grant `sqs:SendMessage` to the SNS topic. | SNS cannot deliver messages; deliveries fail silently. | Add `aws_sqs_queue_policy` permitting the SNS topic ARN as principal. |
| Over-Broad Subscription Filters | Filter policy checks attributes, but producer sends them in payload body. | Filter fails; subscriber receives zero messages. | Filter on Message Attributes for SNS, or use EventBridge for body filtering. |
| Using SNS for SaaS Integration | Attempting to connect Zendesk or Salesforce directly into SNS. | Requires custom webhook servers and auth logic. | Use Amazon EventBridge Partner Event Sources for direct native ingestion. |
| EventBridge Latency Expectations | Expecting sub-5ms routing latency for financial HFT. | EventBridge has a typical latency of ~20ms - 50ms. | Use Amazon SNS or Kinesis for sub-10ms pub/sub performance. |
| EventBridge Rule Target Limit Reached | Creating more than 5 targets on a single rule. | Rule deployment fails validation. | Deploy multiple rules with the same pattern or target an SNS topic. |
| Missing Schema Registry Validation | Breaking payload changes released without notice. | Downstream consumer deserialization crashes. | Enforce Schema Registry validation in CI/CD pipeline. |

---

## 8. Performance, Security & Cost Notes

### 8.1 Performance & Throughput
- **SNS Throughput**: Virtually unlimited throughput for standard topics. Millions of messages per second with global fan-out latency under 30 milliseconds.
- **EventBridge Throughput**: Default soft quota of 10,000 events/second per region (adjustable via AWS Support). Typical end-to-end event routing latency is 20ms - 40ms.

### 8.2 Security
- **Cross-Account Delivery**: Both SNS topics and EventBridge event buses support cross-account resource policies, enabling secure event ingestion across multi-account landing zones.
- **KMS Encryption**: SNS topics support AWS KMS Customer Managed Keys (CMK) for encryption at rest. Note: Services publishing to the topic (e.g., S3 or CloudWatch) must have `kms:GenerateDataKey` permissions.

### 8.3 Cost Breakdown
- **Amazon SNS**:
  - First 1 million publishes/month free; \$0.50 per million thereafter.
  - SQS/Lambda delivery: Free.
  - HTTP/HTTPS delivery: \$0.60 per million.
  - SMS delivery: Billed per destination country carrier rates.
- **Amazon EventBridge**:
  - First 1 million custom events/month free; \$1.00 per million events thereafter.
  - AWS service events (EC2 state changes, S3 events): Free.
  - Schema Discovery: \$0.10 per million ingested events.

---

## 9. Comparison Table

| Dimension | Amazon SNS | Amazon EventBridge |
|---|---|---|
| **Primary Focus** | High-throughput pub/sub broadcast | Intelligent content-based event routing |
| **Throughput** | Unlimited (Millions of msg/sec) | 10,000+ events/sec (Adjustable) |
| **Filtering Capabilities** | Message Attributes only | Deep JSON payload content matching |
| **Targets per Topic/Rule** | Up to 12,500,000 subscriptions | Up to 5 targets per rule |
| **SaaS Integrations** | No (Custom webhooks required) | Yes (Native Partner Event Sources) |
| **Schema Management** | None | Native EventBridge Schema Registry |
| **Pricing** | \$0.50 per million requests | \$1.00 per million custom events |
| **Typical Latency** | Ultra-low (< 30ms) | Low (~20ms - 50ms) |

---

## 10. Interview Questions & Answers

<details>
<summary><strong>Q1: When would you choose Amazon SNS over Amazon EventBridge, and vice-versa?</strong></summary>

- **Choose Amazon SNS** when you need:
  1. High-throughput fan-out broadcasting to hundreds or millions of subscribers (e.g., mobile push notifications, SMS, or thousands of microservice SQS queues).
  2. Sub-millisecond to low-millisecond messaging latency.
  3. Simple topic-based pub/sub with filtering based on message header attributes.
- **Choose Amazon EventBridge** when you need:
  1. Content-based routing inspecting deep JSON payload values (e.g., routing based on `detail.order.total > 500`).
  2. Integration with third-party SaaS applications (Salesforce, Datadog, Zendesk).
  3. Direct connection to native AWS service events (EC2, S3, CodePipeline).
  4. Typed schema discovery and IDE code generation.
  5. Direct routing to external HTTP endpoints via API Destinations without custom code.
</details>

<details>
<summary><strong>Q2: Explain the SNS Fan-Out pattern and why it is superior to a single shared queue.</strong></summary>

In the Fan-Out pattern, a publisher posts a single message to an SNS topic. That topic is subscribed to by multiple independent SQS queues (e.g., Billing Queue, Shipping Queue, Analytics Queue). 
- **Advantage 1: Isolation**: Each downstream team independently scales, monitors, and consumes messages without interfering with others. If the Shipping service crashes, the Billing service continues processing without delay.
- **Advantage 2: Flexible Retry & DLQs**: Each consumer has its own visibility timeout, retry settings, and dead-letter queue tailored to its operational requirements.
- **Advantage 3: Workload Evolution**: New microservices can subscribe to the topic at any time without modifying or redeploying the publisher.
</details>

<details>
<summary><strong>Q3: How does EventBridge content-based filtering evaluate numeric and prefix conditions?</strong></summary>

EventBridge rules use declarative JSON patterns. For example:
```json
{
  "source": ["ecommerce.checkout"],
  "detail": {
    "price": [{ "numeric": [">=", 100, "<", 500] }],
    "customer": {
      "region": [{ "prefix": "EU-" }],
      "tier": ["gold", "platinum"]
    }
  }
}
```
EventBridge matches any event from source `ecommerce.checkout` where `price` is between 100 and 500, `region` starts with `EU-`, and `tier` is either `gold` or `platinum`.
</details>

<details>
<summary><strong>Q4: What is EventBridge Pipes and what architectural complexity does it eliminate?</strong></summary>

Prior to EventBridge Pipes, connecting an event stream (like Amazon DynamoDB Streams or Amazon Kinesis) to an SQS queue or Step Functions workflow required writing and maintaining a custom AWS Lambda function to poll the stream, filter records, enrich payloads, and push them to the target. 
**EventBridge Pipes** provides a managed, serverless point-to-point integration that handles polling, batching, filtering, payload enrichment (via Lambda, API Destinations, or Step Functions), and delivery natively without writing any boilerplate glue code.
</details>

<details>
<summary><strong>Q5: What happens when an SNS subscription endpoint fails to receive a message?</strong></summary>

SNS implements a multi-phase retry policy:
- For **HTTP/HTTPS endpoints**, SNS executes 4-phase exponential backoff retries (over 50 attempts across 20+ days by default) before dropping the message.
- If a **Subscription Redrive Policy** is configured, failed messages that exceed the retry limit are sent to an SQS Dead-Letter Queue (DLQ).
- For **SQS queues**, deliveries are virtually guaranteed unless the SQS queue policy denies permissions or the queue is deleted.
</details>

<details>
<summary><strong>Q6: How do EventBridge API Destinations work and how do they handle rate limiting?</strong></summary>

EventBridge API Destinations allow rules to route events directly to any external HTTP/REST endpoint (e.g., Slack, webhook receivers) without writing Lambda functions. 
- API Destinations use an associated **Connection** resource that securely manages authentication (OAuth client credentials, Basic auth, or API Keys) stored in AWS Secrets Manager.
- To prevent overwhelming the third-party endpoint, API Destinations allow you to set an **Invocation Rate Limit per Second** (e.g., max 10 requests/sec). Events exceeding this rate are automatically queued and retried by EventBridge for up to 24 hours.
</details>

<details>
<summary><strong>Q7: Can SNS deliver messages in strict order?</strong></summary>

Yes, using **Amazon SNS FIFO Topics**. SNS FIFO topics work in conjunction with **Amazon SQS FIFO Queues**. When a publisher sends messages to an SNS FIFO topic with a `MessageGroupId`, SNS preserves the exact publishing order and delivers messages sequentially to all subscribed SQS FIFO queues without duplication.
</details>

<details>
<summary><strong>Q8: How does EventBridge Schema Discovery work?</strong></summary>

When Schema Discovery is enabled on an event bus, EventBridge automatically inspects incoming events and infers their JSON structural schemas. It adds these schemas to the **EventBridge Schema Registry**, versioning them automatically as payloads evolve. Developers can download code bindings directly into VS Code or IntelliJ, providing strongly typed autocomplete models in Java, Python, or TypeScript.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Implement the SNS Fan-Out Pattern with AWS CLI</strong></summary>

**Goal**: Broadcast a message to two distinct SQS queues.
1. Create an SNS Topic named `fanout-topic`.
2. Create two SQS Queues: `queue-alpha` and `queue-beta`.
3. Apply SQS policies permitting `sns.amazonaws.com` to send messages.
4. Subscribe both queues to `fanout-topic`.
5. Publish a message to `fanout-topic` and verify it arrives in both queues.
</details>

<details>
<summary><strong>Exercise 2: Filter Messages Using SNS Subscription Filter Policies</strong></summary>

**Goal**: Deliver messages only to specific queues based on attributes.
1. Add a filter policy to `queue-alpha`: `{"category": ["urgent"]}`.
2. Publish message 1 with attribute `category = "normal"`.
3. Publish message 2 with attribute `category = "urgent"`.
4. Verify `queue-alpha` receives only message 2.
</details>

<details>
<summary><strong>Exercise 3: Create an EventBridge Rule for EC2 State Changes</strong></summary>

**Goal**: Capture an AWS native event and trigger a target.
1. On the default event bus, create a rule with pattern:
   `{"source": ["aws.ec2"], "detail-type": ["EC2 Instance State-change Notification"], "detail": {"state": ["stopped"]}}`.
2. Attach a CloudWatch Log Group as the target.
3. Stop a test EC2 instance and inspect the captured event in CloudWatch logs.
</details>

<details>
<summary><strong>Exercise 4: Test EventBridge Content Filtering on Numeric Ranges</strong></summary>

**Goal**: Filter custom events by numeric threshold.
1. Create a custom event bus `orders-bus`.
2. Create a rule matching `detail.orderTotal >= 100`.
3. Target an SQS queue.
4. Put two events via CLI ($total = 50$ and $total = 200$) and confirm only the \$200 event is routed.
</details>

<details>
<summary><strong>Exercise 5: Build an EventBridge Pipe from SQS to CloudWatch Logs</strong></summary>

**Goal**: Build a serverless stream processor with zero code.
1. Create an SQS source queue.
2. Create an EventBridge Pipe connecting the queue to a CloudWatch Log Group target.
3. Send a message to SQS and verify it appears in CloudWatch logs within seconds.
</details>

<details>
<summary><strong>Exercise 6: Configure an EventBridge API Destination for Webhooks</strong></summary>

**Goal**: Forward events to a public webhook testing service (e.g., webhook.site).
1. Create an EventBridge Connection with an API Key header.
2. Create an API Destination pointing to your test webhook URL.
3. Configure an EventBridge rule that routes events to this API Destination.
4. Emit a matching event and observe the incoming HTTP POST in your webhook inspector.
</details>

---

## 12. Summary & Quick Reference

```
+---------------------------------------------------------------------------------------------------+
| EVENT SERVICE SELECTION MATRIX                                                                    |
+--------------------------+-----------------------+------------------------------------------------+
| Architectural Need       | Recommended Service   | Primary Reason                                 |
+--------------------------+-----------------------+------------------------------------------------+
| High-volume Pub/Sub (1M+)| Amazon SNS            | Low latency, massive subscriber fan-out        |
| SMS & Mobile Push Alerts | Amazon SNS            | Native mobile platform delivery                |
| AWS Service Event Hooks  | Amazon EventBridge    | Default bus captures all AWS infrastructure    |
| SaaS Application Events  | Amazon EventBridge    | Partner event buses (Salesforce, Datadog)      |
| Deep JSON Body Filtering | Amazon EventBridge    | Advanced rule pattern matching engine          |
| Point-to-Point Pipelines | EventBridge Pipes     | Replaces custom Lambda polling glue code       |
+--------------------------+-----------------------+------------------------------------------------+
```

- **SNS**: Pushes to subscribers; best for fan-out and high-speed broadcast.
- **EventBridge**: Evaluates and routes events; best for schema-driven microservice orchestration.
- **Fan-Out + SQS**: Core resilience pattern isolating producers from consumer failures.

---

## 13. Further Reading

- [Amazon SNS Developer Guide](https://docs.aws.amazon.com/sns/latest/dg/)
- [Amazon EventBridge User Guide](https://docs.aws.amazon.com/eventbridge/latest/userguide/)
- [Amazon EventBridge Pipes Overview](https://docs.aws.amazon.com/eventbridge/latest/userguide/eb-pipes.html)
- [Building Event-Driven Architectures on AWS](https://aws.amazon.com/event-driven-architecture/)
- [05-app-services/01-sqs.md](file:///04-devops/aws/05-app-services/01-sqs.md)
