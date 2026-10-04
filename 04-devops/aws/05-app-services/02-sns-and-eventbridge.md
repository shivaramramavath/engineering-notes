# SNS and EventBridge: Pub/Sub and Event Routing

Both services let one part of a system announce that "something happened" without knowing who cares. Producers **publish**, and zero or many consumers **react**. That's the opposite of a [queue](./01-sqs.md), where each message goes to one worker.

- **SNS (Simple Notification Service)**: simple, high-throughput **pub/sub fan-out**. A message published to a topic is pushed to every subscriber (SQS queues, Lambda, HTTP endpoints, email, SMS, mobile push).
- **EventBridge**: a serverless **event bus** with **content-based routing**. Rules inspect event JSON and send matching events to targets. It natively receives events from AWS services and SaaS partners, and has schedulers, pipes, archive/replay.

Rule of thumb: **SNS for "broadcast this to my queues/functions"**, **EventBridge for "route events between services by their content, and react to things happening in AWS"**.

Prerequisites: [SQS](./01-sqs.md), [Lambda](../02-compute/02-lambda.md), [IAM](../01-foundations/03-iam.md).

---

# Part 1: SNS

## Core concepts

```
Publisher ──publish──► Topic ──► Subscription ──► SQS queue
                                 Subscription ──► Lambda function
                                 Subscription ──► HTTPS endpoint
                                 Subscription ──► Email / SMS / mobile push
```

| Concept | Meaning |
|---|---|
| **Topic** | Named channel you publish to. **Standard** (high throughput, at-least-once, best-effort order) or **FIFO** (ordered, deduplicated; FIFO topics deliver to FIFO SQS queues). |
| **Subscription** | A destination + protocol attached to a topic. |
| **Message attributes** | Key/value metadata used by filter policies. |
| **Filter policy** | Per-subscription rule so a subscriber only receives messages it cares about. |

```ts
import { SNSClient, PublishCommand } from "@aws-sdk/client-sns";
const sns = new SNSClient({});

await sns.send(new PublishCommand({
  TopicArn: process.env.TOPIC_ARN,
  Message: JSON.stringify({ orderId: "o-123", total: 40 }),
  MessageAttributes: {
    eventType: { DataType: "String", StringValue: "order.created" },
    region:    { DataType: "String", StringValue: "ap-south-1" },
  },
}));
```

## The fan-out pattern (SNS → SQS)

The canonical SNS use: publish once, and let each consumer own its own queue.

```
                       ┌──► SQS: billing-queue   ──► billing workers
order-events topic ────┼──► SQS: shipping-queue  ──► shipping workers
                       └──► SQS: analytics-queue ──► analytics
```

Why queues behind the topic rather than Lambda directly? Each consumer gets **independent retry, buffering, and a DLQ**, and a slow or failing consumer doesn't affect the others.

Two details that bite:

1. The **queue's access policy must allow `sns.amazonaws.com`** to send to it (conditioned on the topic ARN). Without it, the subscription silently delivers nothing.
2. Enable **raw message delivery** on the SQS subscription if you want the original body. By default SNS wraps it in a JSON envelope (`Type`, `MessageId`, `Message`, …), and your consumer would need to unwrap it.

## Filter policies

```json
{
  "eventType": ["order.created", "order.cancelled"],
  "total": [{ "numeric": [">=", 100] }]
}
```

Attached to a subscription, this delivers only matching messages, so filtering happens **at SNS**, not in your consumer code, which saves invocations and cost. By default filters match on **message attributes**; filter on the message **body** by setting the subscription's filter-policy scope accordingly.

## Delivery, retries and failure handling

- SNS retries failed deliveries according to the **delivery policy** per protocol (SQS and Lambda are retried for a long period; HTTP endpoints follow a configurable policy).
- Attach a **dead-letter queue to the subscription** (an SQS queue) to catch messages that couldn't be delivered. It's the **subscription's** DLQ, not the topic's.
- Delivery is **at-least-once** on Standard topics, so subscribers must be idempotent.
- Message size is limited (256 KB). Put big payloads in [S3](../03-storage-and-databases/01-s3.md) and send a reference.
- Topics are encrypted at rest with SSE if you enable it (needs KMS permissions for publishers/subscribers when using a customer key).

## Other uses

SNS also sends **email, SMS and mobile push** notifications, handy for operational alerts (CloudWatch alarms publish to SNS topics) and simple user notifications. For transactional or marketing **email at scale**, use SES instead ([SES](./03-ses-email.md)). SMS has regional rules, registration and spending limits, so check them early.

---

# Part 2: EventBridge

## Core concepts

```
Event sources                    Event bus                  Rules                    Targets
 AWS services (S3, EC2, ...)  ─►  default bus  ─┐
 Your app (PutEvents)         ─►  custom bus   ─┼─► rule: pattern match ─► Lambda, SQS, SNS,
 SaaS partners                ─►  partner bus  ─┘        (JSON pattern)    Step Functions, API dest, ...
```

| Concept | Meaning |
|---|---|
| **Event** | A JSON document with envelope fields (`source`, `detail-type`, `detail`, `time`, `account`, `region`…) |
| **Event bus** | Receives events. The **default bus** gets events from AWS services; create **custom buses** for your own domains; partner buses for SaaS. |
| **Rule** | A pattern (JSON) + one or more **targets**. Matching events are delivered to each target. |
| **Target** | Lambda, SQS, SNS, Step Functions, ECS task, API destination (HTTP), another bus (incl. cross-account/region), and many more. |
| **Archive & replay** | Store events and replay them later (debugging, recovery). |
| **Schema registry** | Discover/generate code bindings for event shapes. |

### Sending events

```ts
import { EventBridgeClient, PutEventsCommand } from "@aws-sdk/client-eventbridge";
const eb = new EventBridgeClient({});

const res = await eb.send(new PutEventsCommand({
  Entries: [{
    EventBusName: "orders",
    Source: "com.myapp.orders",
    DetailType: "OrderCreated",
    Detail: JSON.stringify({ orderId: "o-123", total: 40, country: "IN" }),
  }],
}));
if (res.FailedEntryCount) { /* inspect res.Entries for ErrorCode/ErrorMessage and retry */ }
```

`PutEvents` can **partially fail**: always check `FailedEntryCount` (the call itself returns 200). Each entry is limited to 256 KB.

### Event patterns: content-based routing

```json
{
  "source": ["com.myapp.orders"],
  "detail-type": ["OrderCreated"],
  "detail": {
    "total": [{ "numeric": [">=", 100] }],
    "country": ["IN", "SG"]
  }
}
```

Patterns match on the event's fields (exact values, prefixes, numeric ranges, `exists`, `anything-but`, and more). A pattern with several fields requires **all** of them to match, while a list of values means **any** of those values.

### Reacting to AWS itself

The default bus receives events from AWS services, which is a major use case, since you can react to infrastructure and service changes without polling:

```json
{
  "source": ["aws.ecs"],
  "detail-type": ["ECS Task State Change"],
  "detail": { "lastStatus": ["STOPPED"], "stopCode": ["TaskFailedToStart"] }
}
```

Similar patterns react to S3 object events (once EventBridge notifications are enabled on the bucket), EC2 state changes, CodePipeline status, GuardDuty findings, and so on.

### Scheduling

- **EventBridge Scheduler** is the dedicated scheduling service: one-off or recurring schedules (rate, cron, or a specific time) with time-zone support, flexible time windows, retries, and a vast target list. Prefer it over older "scheduled rules" for new work, as it scales to very large numbers of schedules.
- Scheduled rules on a bus (`rate(5 minutes)`, `cron(...)`) still exist.

```bash
aws scheduler create-schedule --name nightly-report \
  --schedule-expression "cron(0 2 * * ? *)" --schedule-expression-timezone "Asia/Kolkata" \
  --flexible-time-window Mode=OFF \
  --target '{"Arn":"arn:aws:lambda:ap-south-1:111122223333:function:report","RoleArn":"arn:aws:iam::111122223333:role/scheduler-invoke"}'
```

(Note the AWS cron format has **six** fields and needs `?` in either day-of-month or day-of-week.)

### Pipes

**EventBridge Pipes** connects a source (SQS, DynamoDB Streams, Kinesis, Kafka…) to a target with optional **filtering, enrichment** (Lambda, API) and transformation, which replaces glue Lambdas that do nothing but move data.

### Delivery guarantees and failure handling

- Delivery to targets is **at-least-once**, and EventBridge retries failed deliveries for a configurable period (up to 24 hours by default) with backoff.
- Configure a **dead-letter queue on each target** (SQS) so undeliverable events aren't silently lost after retries are exhausted.
- Ordering is **not guaranteed**.
- Each target needs permission: EventBridge assumes a role (or a **resource-based policy** on the target, such as a Lambda permission or an SQS queue policy) to invoke it. A missing permission is the usual cause of "rule matches but target never runs".
- Add an **input transformer** to reshape the event into just what the target needs.

---

# SNS vs EventBridge vs SQS

| | **SNS** | **EventBridge** | **SQS** |
|---|---|---|---|
| Model | Pub/sub push | Event bus + routing rules | Queue, consumer pulls |
| Routing | Topic + attribute/body filter per subscription | Rich pattern matching on any event field | None (a queue is a destination) |
| Sources | Your publishers (and some AWS services like CloudWatch alarms/S3) | **AWS services, SaaS partners**, your apps | Your producers |
| Targets | SQS, Lambda, HTTP, email, SMS, push | 20+ AWS services, API destinations, other buses | A consumer polling |
| Throughput/latency | Very high, low latency | High, typically somewhat higher latency than SNS | Very high |
| Extras | FIFO topics, SMS/email/push | Schedules, Pipes, archive/replay, schema registry, cross-account buses | Buffering, DLQ, backpressure |
| Choose for | Fan-out to many queues/functions | Event-driven app integration, reacting to AWS events, decoupled domains | Work distribution and buffering |

These are **complementary**: a common shape is *EventBridge rule → SQS queue → Lambda* (buffered, retryable consumer) or *SNS topic → many SQS queues*.

---

## Common mistakes

- **Queue policy missing** for SNS/EventBridge → nothing arrives.
- Forgetting **raw message delivery**, then parsing SNS's envelope by accident (or not).
- No **DLQ** on subscriptions/targets, so failed deliveries vanish silently.
- Assuming **ordering** on Standard topics/EventBridge.
- Treating `PutEvents` success as full success (**ignoring `FailedEntryCount`**).
- Non-idempotent consumers with at-least-once delivery.
- Over-broad rules (`source` only) that fan out far more than intended, or patterns so strict they never match (wrong field case or nesting).
- Filter policy on **attributes** while publishing data only in the **body** (or vice versa).
- Using SNS for **bulk marketing email** (use SES).
- **Event loops**: a rule reacts to an event that its own target generates (e.g. S3 → Lambda → writes S3).
- Using `rate()`/`cron()` rules for huge numbers of per-user schedules instead of **EventBridge Scheduler**.
- Putting giant payloads in events.

---

## Debugging

| Symptom | Check |
|---|---|
| Published, but subscriber gets nothing | Subscription confirmed? Filter policy excludes it? Queue/Lambda **resource policy** allows SNS? |
| Email subscription inactive | Recipient must click the confirmation link |
| Rule never fires | Test the pattern against a sample event (console **Test event pattern** / `aws events test-event-pattern`); check event bus (default vs custom); field names/case |
| Rule matches, target not invoked | Target permission/role; target-level errors in the DLQ; check the **`FailedInvocations`** metric |
| Event "lost" | Retry window expired with no DLQ; archive/replay if enabled |
| Duplicates | Expected at-least-once; make consumers idempotent |
| Schedule didn't run | Wrong time zone/cron fields, the scheduler's **role** can't invoke the target, flexible window, target error |
| `AccessDenied` on publish | IAM `sns:Publish` / `events:PutEvents` on the right ARN; KMS permissions if encrypted |

```bash
aws events test-event-pattern --event-pattern file://pattern.json --event file://sample-event.json
```

CloudWatch metrics to watch: SNS `NumberOfNotificationsFailed`, EventBridge `MatchedEvents`, `Invocations`, `FailedInvocations`, `DeadLetterInvocations`. See [Observability](../06-operations/04-observability.md).

---

## Quick Summary

- **Pub/sub** means publishers don't know subscribers; both SNS and EventBridge do this, in different styles.
- **SNS**: topics → subscriptions (SQS/Lambda/HTTP/email/SMS/push). Best for **fan-out**, usually SNS → multiple SQS queues. Use **filter policies**, **raw delivery**, and subscription **DLQs**; allow SNS in the queue policy.
- **EventBridge**: buses + **pattern-matching rules** + targets. Best for event-driven integration and reacting to **AWS service events**. Also schedules (**Scheduler**), **Pipes**, archive/replay.
- Both are **at-least-once and unordered** (except FIFO topics): idempotent consumers, DLQs, and alarms.
- Check `PutEvents` **`FailedEntryCount`**; test patterns before blaming the target.
- Combine with SQS for buffering: *event → queue → worker*.

**Next:** [SES](./03-ses-email.md)
