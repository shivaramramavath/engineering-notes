# SNS and EventBridge

Both let a producer announce "something happened" without knowing who cares. Consumers subscribe; the producer stays unchanged when you add a fourth one. They solve the same shape of problem at different levels:

- **SNS** is pub/sub: publish to a **topic**, every subscriber gets a copy. Simple, fast, minimal routing.
- **EventBridge** is an event **bus** with **rules** that match on the *content* of events and route them to targets. More routing power, plus events emitted by AWS services themselves.

Prerequisites: [SDK v3 and credentials](../01-setup/01-sdk-v3-and-credentials.md). The usual consumer is a queue ([SQS](./03-sqs.md)) or a Lambda ([event-driven Lambda](../03-lambda/03-event-driven-lambda.md)).

```bash
npm install @aws-sdk/client-sns @aws-sdk/client-eventbridge
```

---

## SNS

### The fan-out pattern

```text
                         ┌─▶ SQS: billing-queue ──▶ billing worker
Producer ──▶ SNS topic ──┼─▶ SQS: email-queue   ──▶ email worker
 "order.paid"            └─▶ Lambda: analytics
```

Subscribing a **queue** to the topic (rather than a Lambda or HTTP endpoint directly) is the robust default: each consumer gets its own buffer, retries and DLQ, and one slow consumer doesn't affect the others.

### Publishing

```ts
import { SNSClient, PublishCommand, PublishBatchCommand } from "@aws-sdk/client-sns";

const sns = new SNSClient({ region: process.env.AWS_REGION });
const TopicArn = process.env.ORDERS_TOPIC_ARN!;

await sns.send(new PublishCommand({
  TopicArn,
  Message: JSON.stringify({ orderId: "01J9A", total: 1200 }),
  MessageAttributes: {
    type: { DataType: "String", StringValue: "order.paid" },
    region: { DataType: "String", StringValue: "in" },
  },
}));
```

- `Message` is a **string**; `JSON.stringify` it.
- `PublishBatchCommand` sends up to 10 messages per call. Like SQS batches, it can partly fail while returning success, so check the `Failed` array.
- Subscriptions are normally defined in infrastructure code (the CDK stacks in [projects](../05-projects/README.md)); `SubscribeCommand` exists but you rarely call it at runtime.

### Filter policies: subscribers choose what they receive

A **filter policy** on a *subscription* makes SNS deliver only matching messages. By default it matches **message attributes**:

```json
{ "type": ["order.paid", "order.refunded"], "region": ["in"] }
```

Fields are ANDed; values within a field are ORed. Set the subscription's `FilterPolicyScope` to `MessageBody` to match on the JSON body instead (then your message must be valid JSON). Filtering happens in SNS, so unwanted messages never reach (or cost) the subscriber.

### What the consumer actually receives

By default SNS **wraps** your message in an envelope. An SQS subscriber's `Body` is JSON like:

```json
{ "Type": "Notification", "MessageId": "…", "TopicArn": "…",
  "Message": "{\"orderId\":\"01J9A\",\"total\":1200}",
  "MessageAttributes": { "type": { "Type": "String", "Value": "order.paid" } } }
```

So the consumer must parse **twice**: the SQS body, then its `Message` string. Enable **raw message delivery** on the subscription to get your original message as the body, and `MessageAttributes` as SQS message attributes.

```ts
// without raw delivery
const envelope = JSON.parse(sqsMessage.body);
const order = JSON.parse(envelope.Message);
```

A Lambda subscribed directly receives `event.Records[0].Sns.Message` (still a string).

### Delivery notes

- At-least-once: consumers must be idempotent.
- **FIFO topics** (`.fifo`) preserve order per `MessageGroupId` with deduplication, and are meant for FIFO queue subscribers.
- Message size is capped (256 KB). Put big payloads in [S3](./01-s3.md) and publish a reference.
- Failed deliveries to a subscriber can be retried per the delivery policy and sent to a **subscription DLQ** (an SQS queue configured through the subscription's redrive policy). Without one, undeliverable messages are dropped.
- Email and SMS subscriptions exist, but for app email use [SES](./05-ses-email.md); SNS email is for operator alerts.

### The permission people forget

For SNS to write into your queue, the **queue's resource policy** must allow it. Scope it to your topic:

```json
{
  "Effect": "Allow",
  "Principal": { "Service": "sns.amazonaws.com" },
  "Action": "sqs:SendMessage",
  "Resource": "arn:aws:sqs:ap-south-1:111122223333:billing-queue",
  "Condition": { "ArnEquals": { "aws:SourceArn": "arn:aws:sns:ap-south-1:111122223333:orders-topic" } }
}
```

IaC helpers add this for you. If you wire it by hand and messages never arrive, this is the first thing to check. Your *publisher* needs `sns:Publish` on the topic ARN.

---

## EventBridge

### Concepts

- **Event**: a JSON envelope with `source`, `detail-type` and a free-form `detail`.
- **Event bus**: the channel events are sent to. The **default bus** receives events from AWS services; create **custom buses** for your own application events.
- **Rule**: an *event pattern* attached to a bus, plus one or more **targets** (Lambda, SQS, SNS, Step Functions, another bus, an HTTP endpoint through an API destination, and more).

```text
Producer ──PutEvents──▶ Event bus ──▶ Rule A (pattern) ──▶ Lambda
                                  ├─▶ Rule B (pattern) ──▶ SQS queue
                                  └─▶ (no rule matches)  ──▶ dropped silently
```

### Sending events

```ts
import { EventBridgeClient, PutEventsCommand } from "@aws-sdk/client-eventbridge";

const eb = new EventBridgeClient({ region: process.env.AWS_REGION });

const res = await eb.send(new PutEventsCommand({
  Entries: [{
    EventBusName: "app-bus",
    Source: "app.orders",                  // your naming; "aws.*" is reserved
    DetailType: "OrderPaid",
    Detail: JSON.stringify({ orderId: "01J9A", total: 1200, country: "IN" }),
  }],
}));

if (res.FailedEntryCount) {
  const failed = res.Entries!.filter((e) => e.ErrorCode);
  // retry these; the call itself returned 200
}
```

- **`Detail` must be a JSON string.** Passing an object throws a validation error.
- `PutEvents` takes up to 10 entries and each entry is size-limited (about 256 KB). Always check `FailedEntryCount`: failures come back inside a successful response.
- Omit `EventBusName` and the event goes to the **default** bus; a rule on your custom bus won't see it. That's a very common "my rule never fires".

### Event patterns

Patterns match on the event's content. Every listed field must match (AND); the array of values for a field is OR.

```json
{
  "source": ["app.orders"],
  "detail-type": ["OrderPaid"],
  "detail": {
    "total": [{ "numeric": [">", 1000] }],
    "country": ["IN", "SG"]
  }
}
```

Other operators include `prefix`, `anything-but`, `exists` and others; see the pattern syntax docs. You can test a pattern against a sample event in the console or with `TestEventPattern` before deploying.

### What the consumer receives

A Lambda target gets the whole envelope, with `detail` **already parsed** (unlike SNS's string):

```ts
export const handler = async (event: {
  source: string; "detail-type": string; detail: { orderId: string; total: number };
}) => {
  console.log(event["detail-type"], event.detail.orderId);
};
```

### Things EventBridge gives you beyond SNS

- **AWS service events** on the default bus (for example ECS state changes, or S3 object events once EventBridge notifications are enabled on the bucket).
- **Content-based routing** without touching the producer.
- **Archive and replay**: store events and replay them to a bus later, useful for recovery and testing new consumers.
- **Per-target retry policy and DLQ**: configure an SQS DLQ on a rule's target so undeliverable events aren't lost.
- **Input transformers** to reshape the event before it hits a target.
- **Scheduling**: use **EventBridge Scheduler** (a separate service and client, `@aws-sdk/client-scheduler`) for cron and one-off schedules.

---

## SNS or EventBridge?

| | SNS | EventBridge |
|---|---|---|
| Model | Topic, subscribers | Bus, rules, targets |
| Routing | Attribute/body filter per subscription | Rich content patterns per rule |
| Reacts to AWS service events | No | Yes |
| Typical throughput/latency | Very high, low latency | High, slightly more latency |
| Replay / archive | No | Yes |
| Targets | SQS, Lambda, HTTP, email, SMS, … | Many AWS services, other buses, API destinations |
| Reach for it when | Simple fan-out to queues, mobile/email alerts | Decoupling services by event type, AWS events, scheduling-style integration |

A solid default for service-to-service events is **EventBridge → SQS** per consumer. Use **SNS → SQS** when you just need fan-out with high volume and simple filtering.

---

## Debugging "nothing arrived"

1. **Publish succeeded?** Check `FailedEntryCount` (EventBridge) and `Failed` (SNS batch).
2. **Right bus/topic and region?** Wrong bus name or a different region is the commonest cause.
3. **Does the rule/filter match?** Test the pattern against the exact event. For SNS, remember filters match attributes by default, not the body.
4. **Target permissions.** Queue policy for SNS/EventBridge as a principal; Lambda resource policy allowing `events.amazonaws.com` or `sns.amazonaws.com`.
5. **Look at the consumer's DLQ** and the service metrics (SNS `NumberOfNotificationsFailed`, EventBridge `FailedInvocations`) in CloudWatch ([logging and tracing](../04-production/02-logging-and-tracing.md)).
6. **Double-parsing**: a consumer failing on SNS-wrapped bodies (`Message` is a string, envelope around it).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Parsing the SNS body once | The payload is in the envelope's `Message` string, or enable raw delivery |
| Missing queue policy for SNS/EventBridge | Allow the service principal with `aws:SourceArn` |
| `Detail` passed as an object | `JSON.stringify` it |
| Events sent to default bus, rule on custom bus | Set `EventBusName` |
| Ignoring `FailedEntryCount` / `Failed` | Inspect and retry failed entries |
| Subscribing Lambda directly with no DLQ | Put a queue in between, or configure a DLQ |
| Expecting ordering or exactly-once | Neither is guaranteed (standard); make consumers idempotent |
| Using SNS/EventBridge as a work queue | They push; use SQS for buffered, retryable work |
| Rule with no match is "silent" | Add a catch-all test rule to a log target while developing |

---

## Quick summary

- SNS = topic with fan-out and simple filters; EventBridge = bus with content-based rules and AWS-native events.
- Best pattern: publish an event, deliver to **per-consumer SQS queues** with DLQs.
- SNS wraps messages (parse `Message`) unless raw delivery is on; EventBridge delivers the envelope with `detail` parsed.
- Both batch APIs can partly fail inside a 200; check the failure fields.
- Grant the service principal permission on the target, scoped with `aws:SourceArn`.
- At-least-once everywhere: idempotent consumers.

## Next

[SES](./05-ses-email.md): sending real email from your app, and handling bounces through the events you just learned about.
