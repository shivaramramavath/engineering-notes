# SQS: Message Queues

SQS (Simple Queue Service) is a fully managed message queue. A **producer** puts messages in, a **consumer** pulls them out and processes them, and the queue holds everything in between. It **decouples** the two sides: the producer doesn't wait for (or even know about) the consumer, spikes get absorbed instead of crashing downstream systems, and a failing consumer doesn't lose work.

Reach for SQS when work can happen *later* or *elsewhere*: sending emails, resizing images, calling a slow third-party API, smoothing a traffic burst before a database, or fanning jobs out to workers.

Prerequisites: [IAM](../01-foundations/03-iam.md); [Lambda](../02-compute/02-lambda.md) helps for the consumer examples.

---

## The model

```
Producer ──send──►  [ queue: m1 m2 m3 m4 ... ]  ──receive──► Consumer(s)
                         ▲                                       │
                         └──── delete after success ◄────────────┘
```

SQS is a **pull** system with a crucial twist: receiving a message does **not** remove it. The message becomes temporarily **invisible**, and you must explicitly **delete** it once processed. If you don't delete it in time, it reappears and is delivered again.

### The visibility timeout

```
t=0   consumer receives m1  ──► m1 invisible for 30s (visibility timeout)
t=12  processing succeeds   ──► consumer deletes m1 → gone for good
        — or —
t=30  no delete yet         ──► m1 becomes visible again → another receive gets it
```

This is the heart of SQS reliability *and* the source of most SQS bugs:

- Set the visibility timeout **longer than your processing time** (default is 30 seconds, max 12 hours). Too short → duplicates while the first attempt is still running.
- For Lambda consumers, set the queue's visibility timeout to **at least 6× the function timeout** (AWS's guidance), so retries don't overlap with running invocations.
- Need more time mid-flight? Extend it with `ChangeMessageVisibility`.

---

## Standard vs FIFO

| | **Standard** | **FIFO** |
|---|---|---|
| Throughput | Very high, effectively unlimited | Limited per queue/message group (batching and high-throughput mode raise it) |
| Delivery | **At-least-once**: duplicates can occur | **Exactly-once processing** within the deduplication window |
| Ordering | **Best-effort**: can arrive out of order | **Strict, per message group** |
| Name | Any | Must end in `.fifo` |
| Use when | Most workloads | Order matters (per entity) or duplicates are unacceptable |

**Default to Standard** and make your consumers **idempotent**. Use FIFO when you really need ordering, such as account-ledger updates for one customer, and pick a **`MessageGroupId`** per ordering domain (e.g. `customerId`). Messages in the same group are processed in order and one at a time, while different groups run in parallel. A single hot group is therefore a throughput bottleneck. FIFO also needs a **deduplication ID** (or content-based deduplication enabled), and a failing message at the head of a group **blocks the rest of that group** until it succeeds or moves to a DLQ.

Common misconception: "FIFO means I don't need idempotency." Deduplication covers a limited window, and your consumer can still crash after doing the work but before deleting the message. Design for retries regardless.

---

## Sending and receiving

```ts
import {
  SQSClient, SendMessageCommand, ReceiveMessageCommand, DeleteMessageCommand,
} from "@aws-sdk/client-sqs";

const sqs = new SQSClient({});
const QueueUrl = process.env.QUEUE_URL!;

// Produce
await sqs.send(new SendMessageCommand({
  QueueUrl,
  MessageBody: JSON.stringify({ orderId: "o-123", action: "send-receipt" }),
  MessageAttributes: { type: { DataType: "String", StringValue: "receipt" } },
}));

// Consume (a worker loop)
while (true) {
  const { Messages } = await sqs.send(new ReceiveMessageCommand({
    QueueUrl,
    MaxNumberOfMessages: 10,   // up to 10 per call
    WaitTimeSeconds: 20,       // long polling
  }));
  for (const m of Messages ?? []) {
    await handle(JSON.parse(m.Body!));          // your logic, must be idempotent
    await sqs.send(new DeleteMessageCommand({ QueueUrl, ReceiptHandle: m.ReceiptHandle! }));
  }
}
```

Essentials:

- **Always use long polling** (`WaitTimeSeconds` up to 20). Short polling returns immediately (often empty) and wastes requests and money.
- Delete with the **receipt handle** from *that* receive. It changes on each receive.
- Use the **batch** APIs (`SendMessageBatch`, `DeleteMessageBatch`, up to 10 entries) to cut request costs.
- Message size is limited (1 MiB at the time of writing, raised from the older 256 KiB limit; check the current quota). For large payloads, store the data in [S3](../03-storage-and-databases/01-s3.md) and send a pointer in the message.
- Retention is configurable from 1 minute to 14 days (default 4 days). Messages not processed by then are dropped.
- **Delay queues / per-message delay** (up to 15 minutes) postpone visibility.

---

## Dead-letter queues (DLQ): non-negotiable for production

Without a DLQ, a message that always fails (a "poison message") is retried **forever**, burning compute and hiding behind healthy traffic. A DLQ catches it after N attempts.

```
main queue ──(receive count > maxReceiveCount)──► dead-letter queue
```

```bash
aws sqs create-queue --queue-name orders-dlq --attributes MessageRetentionPeriod=1209600  # 14 days

aws sqs create-queue --queue-name orders --attributes '{
  "VisibilityTimeout": "180",
  "ReceiveMessageWaitTimeSeconds": "20",
  "RedrivePolicy": "{\"deadLetterTargetArn\":\"arn:aws:sqs:ap-south-1:111122223333:orders-dlq\",\"maxReceiveCount\":\"5\"}"
}'
```

Operational rules:

- A FIFO queue needs a FIFO DLQ; Standard needs Standard.
- Give the DLQ **long retention** so you have time to investigate.
- **Alarm on `ApproximateNumberOfMessagesVisible` > 0 on the DLQ.** Otherwise failures rot silently.
- After fixing the bug, use **DLQ redrive** (console/API: `StartMessageMoveTask`) to move messages back to the source queue.

---

## SQS + Lambda (the most common pairing)

Lambda polls the queue for you (an **event source mapping**), invokes your function with a **batch**, and deletes messages when the function succeeds.

```ts
export const handler = async (event) => {
  const batchItemFailures = [];
  for (const record of event.Records) {
    try {
      await process(JSON.parse(record.body));
    } catch (err) {
      batchItemFailures.push({ itemIdentifier: record.messageId });
    }
  }
  return { batchItemFailures };   // only these return to the queue
};
```

- Enable **`ReportBatchItemFailures`** on the mapping so successes in a batch aren't retried because one message failed. Otherwise, if the function throws, the **whole batch** returns.
- Tune **batch size** and **batching window** (collect more messages before invoking) for throughput/cost.
- **Concurrency control:** Lambda scales consumers up with queue depth. To protect a downstream database, set the mapping's **maximum concurrency** (or the function's reserved concurrency). Beware of reserved concurrency set *too low*, which causes throttled invocations whose messages count as failed receives and can end up in the DLQ.
- Set the queue's visibility timeout relative to the function timeout (above).

---

## Security and encryption

- **IAM** controls who can send/receive. For cross-service delivery (SNS → SQS, S3 → SQS, EventBridge → SQS), the **queue's resource policy** must allow the sender, and the most common reason a subscription "does nothing".
- Messages are **encrypted at rest** by default (SQS-managed keys); you can use a KMS key instead (then senders/receivers/services also need KMS permissions).
- Use a **VPC interface endpoint** to reach SQS from private subnets without NAT ([VPC](../04-networking/01-vpc.md)).

---

## Patterns

| Pattern | Shape |
|---|---|
| **Work queue** | API enqueues job → workers (Lambda/ECS) process → results to DB/S3 |
| **Load leveling** | Queue absorbs bursts in front of a slow dependency |
| **Fan-out** | SNS topic → multiple SQS queues, one per consumer ([SNS and EventBridge](./02-sns-and-eventbridge.md)) |
| **Async API response** | API returns `202 Accepted` + job id; worker processes; client polls or gets notified (sidesteps API Gateway's timeout) |
| **Retry buffer** | Failed calls to flaky APIs re-driven with delays |

### SQS vs the neighbours

| | Model | Choose when |
|---|---|---|
| **SQS** | Queue; each message processed by **one** consumer; consumer pulls | Work distribution, buffering |
| **SNS** | Pub/sub; one message pushed to **many** subscribers | Broadcasting, fan-out |
| **EventBridge** | Event bus with content-based routing and 3rd-party/AWS sources | Event-driven integration between services |
| **Kinesis / MSK** | Ordered, replayable streams | High-volume streaming, multiple readers re-reading history |

---

## Costs

Billed **per request** (a "request" is up to 64 KB of payload, so larger messages count as multiple), with a monthly free allowance. Empty receives still cost, which is why long polling and batching matter. Cost is rarely the issue; unnoticed DLQs and runaway loops are.

---

## Common mistakes

- **No DLQ**, so poison messages loop forever.
- **Visibility timeout shorter than processing time**, causing duplicate processing.
- **Not idempotent** consumers on Standard queues (duplicates *will* happen).
- Forgetting to **delete** after success (message reappears), or deleting **before** finishing the work (message lost if you crash).
- **Short polling** loops.
- Lambda consumer **without partial-batch-failure** handling.
- **Reserved concurrency too low** on the consumer, causing throttles and false DLQ moves.
- Assuming **ordering** on Standard queues.
- Single `MessageGroupId` on FIFO, serialising everything.
- Putting **big payloads** in messages instead of S3 pointers.
- Queue **resource policy** missing for SNS/S3/EventBridge senders.
- Not alarming on **queue age** (`ApproximateAgeOfOldestMessage`) or DLQ depth.

---

## Debugging

| Symptom | Check |
|---|---|
| Messages processed twice | Expected with at-least-once: make handler idempotent. Also visibility timeout < processing time |
| Messages stuck / "in flight" | Consumer crashed or is slow; they'll return after the visibility timeout. Check `ApproximateNumberOfMessagesNotVisible` |
| Messages vanish | Retention expired; DLQ redrive threshold hit; consumer deleted before success |
| Queue grows | Consumers too slow/failing/throttled: check consumer errors, concurrency, `ApproximateAgeOfOldestMessage` |
| SNS/S3/EventBridge → queue delivers nothing | Queue access policy doesn't allow the source (and KMS permissions if using a CMK) |
| `AccessDenied` | IAM policy / queue policy / KMS key policy |
| Lambda not invoked | Event source mapping disabled, wrong ARN, execution role lacks `sqs:ReceiveMessage`/`DeleteMessage`/`GetQueueAttributes` |
| FIFO queue stuck | A failing message blocks its group; check DLQ and group IDs |

Peek without consuming: `aws sqs receive-message --queue-url … --visibility-timeout 0` (the message stays available). Check depth with `aws sqs get-queue-attributes --attribute-names All`.

---

## Quick Summary

- SQS = durable queue that decouples producers from consumers. Receive → process → **delete**; otherwise it reappears after the **visibility timeout**.
- **Standard** (default): high throughput, at-least-once, best-effort ordering, so make consumers **idempotent**. **FIFO**: ordered per `MessageGroupId`, deduplicated, lower throughput.
- Always: **long polling**, **DLQ with an alarm**, visibility timeout > processing time, batch APIs.
- With Lambda: enable **partial batch responses**, set concurrency limits to protect downstreams, and size timeouts sensibly.
- Large payloads → S3 pointer. Cross-service senders need the **queue policy** to allow them.
- Use SNS for fan-out, EventBridge for event routing, SQS for work distribution.

**Next:** [SNS and EventBridge](./02-sns-and-eventbridge.md)
