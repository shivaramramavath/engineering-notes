# SQS

SQS is a managed message queue. A producer sends messages; a consumer polls, processes and then **deletes** them. It decouples "accept the work" from "do the work", absorbs spikes, and retries failures for you. In Node it's the AWS counterpart of BullMQ, with less power (no priorities, rate limiting or job UI) and no Redis to run.

Prerequisites: [SDK v3 and credentials](../01-setup/01-sdk-v3-and-credentials.md).

```bash
npm install @aws-sdk/client-sqs
```

---

## How a message lives

```text
Producer ──send──▶ [ Queue ] ──receive──▶ Consumer
                       ▲   (message becomes invisible        │
                       │    for the visibility timeout)      │
                       │                                     ▼
                       └── reappears if NOT deleted ◀── process → DELETE
                                   │
                          after N failed receives
                                   ▼
                          Dead-letter queue (DLQ)
```

The key idea: **receiving a message doesn't remove it**. It becomes invisible to other consumers for the *visibility timeout* (default 30 seconds). If you finish and call `DeleteMessage`, it's gone. If you crash or take too long, it reappears and is delivered again. That's the whole retry mechanism.

Consequences you must design for:

- **Standard queues deliver at-least-once**, occasionally more than once, and not strictly ordered. Your processing must be **idempotent**.
- **FIFO queues** (name ends in `.fifo`) give ordering per `MessageGroupId` and deduplication within a window, with lower throughput.

---

## Sending

```ts
import { SQSClient, SendMessageCommand, SendMessageBatchCommand } from "@aws-sdk/client-sqs";

const sqs = new SQSClient({ region: process.env.AWS_REGION });
const QueueUrl = process.env.ORDERS_QUEUE_URL!; // a URL, not an ARN

await sqs.send(new SendMessageCommand({
  QueueUrl,
  MessageBody: JSON.stringify({ orderId: "01J9A", action: "charge" }),
  MessageAttributes: {
    type: { DataType: "String", StringValue: "order.charge" },
  },
  DelaySeconds: 10, // optional; max 900
}));

// up to 10 messages per call; check for partial failure
const res = await sqs.send(new SendMessageBatchCommand({
  QueueUrl,
  Entries: orders.slice(0, 10).map((o, i) => ({ Id: String(i), MessageBody: JSON.stringify(o) })),
}));
if (res.Failed?.length) { /* retry just those entries */ }
```

- `MessageBody` is a **string**: `JSON.stringify` it and parse on receipt.
- Message size is limited (256 KiB historically; AWS has raised the quota since, so check the current limit). For big payloads, put the data in [S3](./01-s3.md) and send the key (the "claim check" pattern).
- Batch calls can **partly fail with a 200 response**: always inspect `Failed`.
- **FIFO** sends need `MessageGroupId`, and either `MessageDeduplicationId` or content-based deduplication enabled on the queue.
- Queue URL vs ARN: the SDK wants the **URL** (`https://sqs.<region>.amazonaws.com/<account>/<name>`); IAM policies and triggers use the **ARN**.

---

## Receiving: long polling and a worker loop

```ts
import { ReceiveMessageCommand, DeleteMessageCommand } from "@aws-sdk/client-sqs";

let running = true;
process.on("SIGTERM", () => { running = false; }); // finish the current batch, then exit

while (running) {
  const { Messages = [] } = await sqs.send(new ReceiveMessageCommand({
    QueueUrl,
    MaxNumberOfMessages: 10,   // 1-10
    WaitTimeSeconds: 20,       // long polling: wait up to 20s for messages
    VisibilityTimeout: 60,     // optional override for this receive
    MessageAttributeNames: ["All"],
    MessageSystemAttributeNames: ["ApproximateReceiveCount"],
  }));

  await Promise.all(Messages.map(async (m) => {
    try {
      await handle(JSON.parse(m.Body!));
      await sqs.send(new DeleteMessageCommand({ QueueUrl, ReceiptHandle: m.ReceiptHandle! }));
    } catch (err) {
      console.error({ messageId: m.MessageId, err });
      // do NOT delete: it reappears after the visibility timeout and is retried
    }
  }));
}
```

(Older SDK versions name the system-attribute option `AttributeNames`.)

Why these choices:

- **Long polling (`WaitTimeSeconds: 20`)** holds the request open until a message arrives. Short polling (the default at 0) returns empty responses constantly: more cost, more latency. Always set it.
- **Delete only after success**, per message. Use the `ReceiptHandle` from *that receive*; it changes on every receive.
- **Set the visibility timeout above your worst-case processing time.** Too short and the same message gets processed twice concurrently; too long and failed messages wait forever to retry. For long jobs, extend it mid-flight with `ChangeMessageVisibilityCommand`.
- `ApproximateReceiveCount` tells you which attempt this is; handy for logging and backoff.
- An empty `Messages` array is normal (the long poll timed out). Don't treat it as an error.

In Lambda you don't write this loop at all; the event source mapping polls for you and invokes your handler with a batch ([event-driven Lambda](../03-lambda/03-event-driven-lambda.md)). A loop like the above is for containers (ECS, EC2) and scripts.

---

## Dead-letter queues

A DLQ is just another queue that catches messages that keep failing. You attach it to the source queue with a **redrive policy**:

```json
{ "deadLetterTargetArn": "arn:aws:sqs:ap-south-1:111122223333:orders-dlq", "maxReceiveCount": 5 }
```

After a message is received `maxReceiveCount` times without being deleted, SQS moves it to the DLQ. Rules:

- The DLQ must be the **same type** (standard with standard, FIFO with FIFO).
- Give the DLQ a **longer retention** than the source queue (retention counts from original send time; default 4 days, max 14), otherwise messages can expire before you look.
- **Put an alarm on DLQ depth.** A DLQ nobody watches is just a slower way to lose messages. See [logging and tracing](../04-production/02-logging-and-tracing.md).
- After fixing the bug, move messages back with the console's redrive, or the `StartMessageMoveTask` API.
- Pick `maxReceiveCount` deliberately: 1 means no retries; very high means a poison message loops for ages. 3-5 is typical.

Poison messages (invalid JSON, bad data that will never succeed) should fail fast and land in the DLQ rather than be retried forever.

---

## Idempotency

Because delivery is at-least-once, handle duplicates deliberately. Two common approaches:

- **Natural idempotency**: the operation is safe to repeat (set a status to `PAID`).
- **Dedupe key**: record the message/business ID with a conditional write (`attribute_not_exists`) in [DynamoDB](./02-dynamodb.md) before doing the side effect. If the write fails, you've already handled it.

FIFO's deduplication only covers a short window and only duplicate *sends*, not redelivery after a visibility timeout, so you still need idempotent handlers.

---

## SQS vs BullMQ

| | SQS | BullMQ (Redis) |
|---|---|---|
| Infra | Fully managed, no servers | You run and size Redis |
| Delivery | At-least-once (standard) | At-least-once-ish, per job state machine |
| Delays | Up to 15 minutes per message | Arbitrary delayed jobs, repeatable/cron jobs |
| Priorities, rate limits, job UI | No | Yes |
| Retries/backoff | Visibility timeout + redrive to DLQ | Per-job attempts and backoff |
| Scale/durability | Very high, multi-AZ | Bounded by your Redis |
| Fits | AWS-native fan-out, Lambda consumers, spiky load | Rich job semantics inside a Node app |

Not either/or: many systems use SQS between services and BullMQ for in-app job features.

---

## Permissions

```json
{
  "Effect": "Allow",
  "Action": ["sqs:SendMessage", "sqs:ReceiveMessage", "sqs:DeleteMessage", "sqs:ChangeMessageVisibility", "sqs:GetQueueAttributes"],
  "Resource": "arn:aws:sqs:ap-south-1:111122223333:orders-queue"
}
```

When SNS, S3 or EventBridge send *into* the queue, the permission goes on the **queue's resource policy**, scoped with `aws:SourceArn` ([SNS and EventBridge](./04-sns-and-eventbridge.md)).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Messages processed twice | Expected; make handlers idempotent, and size the visibility timeout |
| Never deleting messages | `DeleteMessage` after success, or they loop until retention expires |
| Short polling | `WaitTimeSeconds: 20` |
| Visibility timeout shorter than processing time | Raise it, or extend with `ChangeMessageVisibility` |
| No DLQ, or DLQ unmonitored | Add `maxReceiveCount` and an alarm on DLQ depth |
| Using the queue ARN where a URL is needed | `QueueUrl` for the SDK |
| Ignoring `Failed` in batch responses | Retry failed entries |
| Large payloads in the body | Put data in S3, send the key |
| Assuming ordering on a standard queue | Use FIFO and a sensible `MessageGroupId` |
| DLQ retention shorter than the source | Make the DLQ retention longer |

---

## Quick summary

- Send → receive (message hides) → process → **delete**. Not deleted means it comes back.
- Standard = at-least-once, unordered; FIFO = ordered per group, dedup window. Idempotent handlers either way.
- Use long polling, per-message delete, a visibility timeout above your processing time.
- DLQ via redrive policy, same type, longer retention, with an alarm.
- Check `Failed` on batch sends; keep bodies small; the SDK takes queue **URLs**.
- Lambda polls for you; loops are for long-running workers.

## Next

[SNS and EventBridge](./04-sns-and-eventbridge.md): fanning one event out to many queues and services.
