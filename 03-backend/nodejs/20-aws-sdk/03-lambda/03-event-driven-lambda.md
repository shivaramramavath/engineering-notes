# Event-Driven Lambda

Most Lambda functions in production aren't behind an API. They react: a file lands in S3, a message arrives on a queue, a row changes in DynamoDB. These triggers look similar but have **very different retry and failure behavior**, and every one of them can deliver the same event more than once. Getting partial batch failure and idempotency right is what separates a function that works in the demo from one that survives production.

Prerequisites: [Lambda handlers](./01-lambda-handlers.md), plus the services involved: [S3](../02-services/01-s3.md), [SQS](../02-services/03-sqs.md), [DynamoDB](../02-services/02-dynamodb.md).

```bash
npm install -D @types/aws-lambda
npm install @aws-sdk/util-dynamodb
```

---

## Three ways Lambda gets invoked

```text
Synchronous      caller waits for the result      API Gateway, SDK Invoke
Asynchronous     Lambda queues the event,         S3, SNS, EventBridge
                 caller gets "accepted"
Polling          Lambda POLLS the source and      SQS, DynamoDB Streams,
(event source    invokes you with a BATCH         Kinesis, Kafka
 mapping)
```

This decides everything about failure handling:

| Trigger | Who retries | Where failures go |
|---|---|---|
| S3, SNS, EventBridge (async) | Lambda: by default **2 retries** (3 attempts), within a max event age | Failure **destination** or DLQ you configure; else dropped |
| SQS (polling) | The **queue**: message reappears after the visibility timeout | The **queue's** dead-letter queue (redrive policy) |
| DynamoDB Streams (polling) | Lambda: retries the batch until success or the record expires | Configurable retry limits/age, bisecting, on-failure destination |

Be careful about "DLQ" ambiguity: for **async** invocations you configure it on the *function*; for **SQS** triggers it lives on the *source queue*, not the function.

---

## S3 trigger

S3 invokes your function asynchronously when objects are created or removed (configured with event types and optional prefix/suffix filters).

```ts
import type { S3Event } from "aws-lambda";
import { S3Client, GetObjectCommand, PutObjectCommand } from "@aws-sdk/client-s3";

const s3 = new S3Client({});

export const handler = async (event: S3Event) => {
  for (const record of event.Records) {
    const bucket = record.s3.bucket.name;
    // keys arrive URL-encoded, with spaces as "+"
    const key = decodeURIComponent(record.s3.object.key.replace(/\+/g, " "));

    const obj = await s3.send(new GetObjectCommand({ Bucket: bucket, Key: key }));
    const body = await obj.Body!.transformToString();

    await s3.send(new PutObjectCommand({
      Bucket: bucket,
      Key: `processed/${key.replace(/^uploads\//, "")}.json`,   // DIFFERENT prefix than the trigger
      Body: JSON.stringify({ length: body.length }),
      ContentType: "application/json",
    }));
  }
};
```

Rules:

- **Decode the key.** Skipping this breaks on any key containing spaces or special characters.
- **Never write back to the prefix that triggers you**, or you create an infinite loop (and a large bill). Use separate prefixes with filters, or separate buckets.
- Events can be **delivered more than once**, and are not guaranteed in order.
- The `Records` array normally has one record, but handle many.
- For control over concurrency and retries, route events **S3 → SQS → Lambda** instead of S3 → Lambda directly. The queue absorbs bursts and gives you a DLQ.
- Failed async invocations are retried, then dropped unless a **failure destination** (SQS, SNS, EventBridge or another Lambda) or DLQ is set. Prefer destinations: they carry the error context.

---

## SQS trigger

Lambda polls the queue for you, scales pollers up and down, and invokes your function with a **batch** of messages.

```ts
import type { SQSEvent } from "aws-lambda";

export const handler = async (event: SQSEvent) => {
  for (const record of event.Records) {
    const body = JSON.parse(record.body);
    console.log(record.messageId, record.attributes.ApproximateReceiveCount, body);
  }
};
```

### The batch problem

If your handler **throws**, Lambda treats the **entire batch as failed**, and *every* message in it returns to the queue, including the ones you already processed successfully. With a batch of 10 and one poison message, the other nine get reprocessed on each retry, and eventually all ten land in the DLQ.

### The fix: partial batch responses

Enable **`ReportBatchItemFailures`** on the event source mapping (`FunctionResponseTypes: ["ReportBatchItemFailures"]`) and return the IDs of the messages that failed. Only those go back to the queue.

```ts
import type { SQSEvent, SQSBatchResponse } from "aws-lambda";

export const handler = async (event: SQSEvent): Promise<SQSBatchResponse> => {
  const results = await Promise.allSettled(
    event.Records.map((r) => processMessage(JSON.parse(r.body)))
  );

  const batchItemFailures = results.flatMap((res, i) =>
    res.status === "rejected"
      ? [{ itemIdentifier: event.Records[i].messageId }]
      : []
  );

  return { batchItemFailures };
};
```

Rules:

- Return `{ batchItemFailures: [] }` when everything succeeded.
- **If you throw, or return a malformed response, the whole batch fails**, so catch per message.
- **FIFO queues:** order matters, so process sequentially, and on the first failure report **that message and all later ones** in the batch; continuing past a failure would break ordering.
- Powertools for AWS Lambda (TypeScript) has a batch utility (`@aws-lambda-powertools/batch`, `processPartialResponse`) that implements this pattern, including FIFO handling, if you'd rather not hand-roll it.

### Settings that cause real incidents

- **Visibility timeout ≥ function timeout**, and AWS recommends about **6×** your function timeout so in-flight batches aren't redelivered mid-processing (the mapping can retry on throttle and needs headroom).
- **Batch size and window**: batch size up to 10 by default for standard queues; set a *maximum batching window* to gather bigger batches (larger batch sizes are possible with a window). Bigger batches mean fewer invocations but more work per failure.
- **Throttling and the DLQ:** if Lambda is throttled (concurrency limit reached), messages go back to the queue with their **receive count incremented**. Set the DLQ `maxReceiveCount` comfortably above what throttling can cause (AWS suggests at least 5), or healthy messages get dead-lettered just because you were busy.
- **`maxConcurrency` on the mapping** limits how many concurrent invocations the queue can drive, protecting a downstream database without reserved-concurrency throttling side effects.
- The **DLQ belongs to the queue**, not the function. Put a CloudWatch alarm on it ([logging and tracing](../04-production/02-logging-and-tracing.md)).

---

## DynamoDB Streams trigger

Streams deliver an ordered log of item changes per partition key. Lambda polls the stream's shards and invokes you with batches of change records.

```ts
import type { DynamoDBStreamEvent } from "aws-lambda";
import { unmarshall } from "@aws-sdk/util-dynamodb";

export const handler = async (event: DynamoDBStreamEvent) => {
  for (const r of event.Records) {
    // r.eventName: "INSERT" | "MODIFY" | "REMOVE"
    const newImage = r.dynamodb?.NewImage ? unmarshall(r.dynamodb.NewImage as any) : undefined;
    const oldImage = r.dynamodb?.OldImage ? unmarshall(r.dynamodb.OldImage as any) : undefined;
    console.log(r.eventName, newImage, oldImage);
  }
};
```

- Images are in **DynamoDB's typed JSON** (`{ S: "x" }`). Convert with `unmarshall`. Which images exist depends on the stream's *view type* (`NEW_IMAGE`, `OLD_IMAGE`, `NEW_AND_OLD_IMAGES`, `KEYS_ONLY`).
- **Failures block the shard.** Ordering is per shard, so a record that keeps failing stalls everything behind it until it succeeds or expires. That's the biggest operational difference from SQS. Configure `MaximumRetryAttempts`, `MaximumRecordAgeInSeconds`, **`BisectBatchOnFunctionError`** (split the batch to isolate the bad record) and an **on-failure destination** so a poison record eventually gets skipped and saved for inspection.
- Partial batch responses work here too: return `{ batchItemFailures: [{ itemIdentifier: <SequenceNumber> }] }` using the record's `dynamodb.SequenceNumber`.
- Use **event filtering** on the mapping to invoke only for relevant changes (say, `INSERT` of a certain type) instead of filtering in code and paying for empty invocations.
- Stream records are retained for a limited time (24 hours), so a long outage can lose events.
- Common uses: denormalization and search indexing, publishing domain events, audit trails.

---

## At-least-once means idempotent

Every trigger above can invoke you **more than once for the same logical event**: S3 notification duplicates, async retries, SQS redelivery after a visibility timeout or partial failure, stream retries. Exactly-once is not on offer, so make processing safe to repeat.

Approaches, from best to most work:

1. **Naturally idempotent operations**: `PUT` the same object to the same key, set a status to `PAID`, upsert by ID.
2. **Idempotency keys on downstream calls**: pass a stable key to payment/email APIs that support one.
3. **A dedupe record** written *before* the side effect with a conditional write ([DynamoDB conditions](../02-services/02-dynamodb.md#conditions-dynamodbs-concurrency-tool)):

```ts
import { DynamoDBClient } from "@aws-sdk/client-dynamodb";
import { DynamoDBDocumentClient, PutCommand, UpdateCommand } from "@aws-sdk/lib-dynamodb";

const ddb = DynamoDBDocumentClient.from(new DynamoDBClient({}));
const Table = process.env.IDEMPOTENCY_TABLE!;      // enable TTL on the `ttl` attribute

export async function runOnce(id: string, work: () => Promise<void>) {
  const now = Date.now();
  try {
    await ddb.send(new PutCommand({
      TableName: Table,
      Item: { id, status: "IN_PROGRESS", lockUntil: now + 60_000, ttl: Math.floor(now / 1000) + 86_400 },
      // first time, or a previous attempt crashed and its lock expired
      ConditionExpression: "attribute_not_exists(id) OR (#s = :inprog AND lockUntil < :now)",
      ExpressionAttributeNames: { "#s": "status" },
      ExpressionAttributeValues: { ":inprog": "IN_PROGRESS", ":now": now },
    }));
  } catch (err: any) {
    if (err.name === "ConditionalCheckFailedException") return; // already done, or running elsewhere
    throw err;
  }

  await work();                                                  // if this throws, the lock simply expires

  await ddb.send(new UpdateCommand({
    TableName: Table, Key: { id },
    UpdateExpression: "SET #s = :done",
    ExpressionAttributeNames: { "#s": "status" },
    ExpressionAttributeValues: { ":done": "COMPLETE" },
  }));
}
```

Design notes on that pattern:

- **Key on a business ID** (`orderId`, an upload's S3 key + version), not the transport's ID. An SQS `messageId` is stable for *redelivery* of one message, but a producer that sends the same logical request twice creates two different messages.
- The `IN_PROGRESS` + `lockUntil` lease handles crashes: without it, a failure between "record written" and "work done" would block every retry forever. Make `lockUntil` longer than your function timeout.
- A dedupe record narrows the window but can't make the side effect itself atomic. If `work()` sends an email and then crashes before the status update, a retry may send it again after the lock expires. Where that matters, make the downstream call idempotent too.
- Expire records with DynamoDB **TTL** (epoch seconds) so the table doesn't grow forever.

4. **Powertools Idempotency** (`@aws-lambda-powertools/idempotency`) wraps a handler or function with DynamoDB-backed idempotency, including in-progress handling, so you don't write the above yourself.

---

## Time, batching and backpressure

- Use `context.getRemainingTimeInMillis()` in batch handlers: if time is nearly up, stop and **report the remaining items as failures** rather than being killed mid-item.
- Process batch items concurrently only if the downstream can take it. `Promise.all` over 10 messages is 10 simultaneous calls per invocation, times your concurrency.
- Protect databases and third-party APIs with `maxConcurrency` (SQS) or reserved concurrency, and let the queue buffer the rest. This is the main reason to put SQS in front of a function.
- A message that can never succeed (malformed JSON) should fail fast and reach the DLQ, not be retried by clever code. Distinguish *permanent* from *transient* errors in your handler, and log which.

---

## Testing and debugging

- Unit-test handlers with JSON **fixture events** and mocked clients, including a batch with one failing record: assert the returned `batchItemFailures` ([testing](../01-setup/02-local-development-and-testing.md)).
- Capture a real event once with `console.log(JSON.stringify(event))`, then save it as a fixture.
- Logs: `/aws/lambda/<function>`. Log the `messageId`/business ID and attempt count (`ApproximateReceiveCount`) to see retries ([logging and tracing](../04-production/02-logging-and-tracing.md)).

| Symptom | Likely cause | Check |
|---|---|---|
| Same message processed many times | Handler throws for the whole batch, or visibility timeout shorter than processing | `ReportBatchItemFailures`; visibility timeout ≥ 6× function timeout |
| Healthy messages in the DLQ | Throttling inflated receive counts | Raise `maxReceiveCount`; set `maxConcurrency` |
| Messages stuck, nothing processing | Event source mapping disabled, or function role lacks `sqs:ReceiveMessage`/`DeleteMessage`/`GetQueueAttributes` | Check mapping state and role |
| Stream processing stalled | Poison record blocking a shard | Retry/age limits, bisect-on-error, on-failure destination |
| S3 handler loops forever | Output written under the trigger prefix | Separate prefixes/buckets |
| `NoSuchKey` right after an S3 event | Key not URL-decoded | Decode the key |
| `ReportBatchItemFailures` set but whole batch retried | Handler threw, or response shape wrong | Catch per item; return `{ batchItemFailures }` |
| Async failures vanish | No destination/DLQ configured | Set an on-failure destination |
| Duplicate side effects | No idempotency | Add a dedupe record or idempotent downstream call |

---

## Quick summary

- Three invocation models: synchronous, asynchronous (S3/SNS/EventBridge), polling (SQS, streams). **Failure handling differs for each.**
- Async: 2 retries by default, then a destination/DLQ if configured. SQS: the queue retries, and the DLQ is on the queue. Streams: retried in order, and a bad record can block a shard.
- For SQS, turn on **`ReportBatchItemFailures`** and return failed `messageId`s; catch per message; FIFO stops at the first failure.
- Tune visibility timeout (≥ 6× function timeout), `maxReceiveCount` (≥ 5), batch size/window and `maxConcurrency`.
- S3: decode keys, never write to the triggering prefix. Streams: `unmarshall` images; bisect and set limits.
- **Everything is at-least-once**: key idempotency on a business ID with a conditional write and a lease, and keep downstream calls idempotent.

## Next

[Errors and retries](../04-production/01-errors-and-retries.md): how SDK retries, timeouts and backoff interact with the retry layers above.