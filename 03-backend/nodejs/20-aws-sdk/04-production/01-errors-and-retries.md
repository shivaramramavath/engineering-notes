# Errors and Retries

Calls to AWS fail for ordinary reasons: a network blip, a throttled request, a server that hiccups. The SDK already retries many of these for you. The production skill is knowing **what it retries, what it doesn't, how your own retry layers multiply it, and how to bound the time all of that can take**. Get this wrong and a small outage becomes a retry storm, or a hung socket quietly eats your Lambda's entire timeout.

Prerequisites: [SDK v3 and credentials](../01-setup/01-sdk-v3-and-credentials.md). Several earlier notes defer to this one for throttling and backoff.

---

## What the SDK retries on its own

Every v3 client has a retry strategy. By default it uses **standard** mode: up to **3 attempts** (the first try plus 2 retries) with **exponential backoff and jitter**.

It retries failures that are plausibly transient:

- network errors and connection resets
- timeouts
- **5xx** responses from the service
- **throttling** errors (`ThrottlingException`, `TooManyRequestsException`, `ProvisionedThroughputExceededException`, S3's `SlowDown`, and similar)
- clock-skew errors, after correcting for the skew

It does **not** retry client errors that repeating can't fix: `AccessDenied`, `ValidationException`, `NoSuchKey`, `ConditionalCheckFailedException`, bad input. Those fail immediately, which is what you want.

Standard mode also has a **retry quota**: a budget of retries that drains as retries happen and refills on successes. During a widespread outage the quota runs out and the client stops retrying, instead of piling more load on a struggling service.

### Configuring it

```ts
import { S3Client } from "@aws-sdk/client-s3";
import { SQSClient } from "@aws-sdk/client-sqs";
import { ConfiguredRetryStrategy } from "@smithy/util-retry";

// simplest: more attempts
const s3 = new S3Client({ region: "ap-south-1", maxAttempts: 5 });

// custom backoff: attempt number in, delay (ms) out
const sqs = new SQSClient({
  retryStrategy: new ConfiguredRetryStrategy(5, (attempt) => 100 * 2 ** attempt),
});
```

The same settings can come from environment variables (`AWS_MAX_ATTEMPTS`, `AWS_RETRY_MODE`) or the shared config file, which is handy for tuning without a code change.

There's also an **adaptive** mode that adds client-side rate limiting based on observed throttling. It can help a single client hammering a throttled resource, but treat it as experimental and measure before adopting it.

### See what happened

Every SDK response and error carries retry metadata:

```ts
try {
  await ddb.send(command);
} catch (err: any) {
  console.error({
    name: err.name,
    fault: err.$fault,                       // "client" | "server"
    status: err.$metadata?.httpStatusCode,
    attempts: err.$metadata?.attempts,       // how many tries the SDK made
    totalRetryDelay: err.$metadata?.totalRetryDelay,
    requestId: err.$metadata?.requestId,     // quote this to AWS Support
  });
  throw err;
}
```

Logging `attempts` on errors tells you whether you're seeing a one-off or a throttling pattern. Successful responses have the same `$metadata`, so you can log it to detect "succeeding, but only after retries" before it becomes an outage.

---

## Retries stack up (the part that bites)

The SDK is rarely the only thing retrying. A single logical message can be retried at several layers, and the attempts **multiply**:

```text
SQS delivers message (receive count up to maxReceiveCount = 5)
 └─ Lambda invoked → your handler
     └─ your own retry helper (3 tries)
         └─ SDK call (3 attempts)

worst case for one message: 5 × 3 × 3 = 45 calls to the downstream service
```

And when the downstream service is throttling *because* of load, 45 calls per message is the worst thing you could send it.

Guidance:

- **Retry at one level where you can.** If the queue already retries, don't also add a loop in the handler. Let the message fail and come back.
- Inside a context that is itself retried (queue consumers, async Lambda), keep SDK attempts modest.
- Every retried operation must be **idempotent**. A retried `SendMessage` can deliver twice if the first request succeeded but the response was lost; a retried `PutItem` is safe. See [event-driven Lambda](../03-lambda/03-event-driven-lambda.md#at-least-once-means-idempotent).
- Retry only **transient** failures. Retrying an `AccessDenied` or a validation error just wastes time.

---

## Timeouts: bound the waiting

A retry strategy is useless if one attempt can hang. By default the SDK's Node HTTP handler doesn't impose a short request timeout (check the defaults for the version you use), so a dead connection can wait until the OS gives up or your Lambda times out. Set explicit timeouts.

```ts
import { NodeHttpHandler } from "@smithy/node-http-handler";

const dynamo = new DynamoDBClient({
  requestHandler: new NodeHttpHandler({
    connectionTimeout: 2_000,   // time to establish the TCP/TLS connection
    requestTimeout: 5_000,      // time to wait for the response
  }),
  maxAttempts: 3,
});
```

For a **per-call deadline**, pass an abort signal:

```ts
await s3.send(new GetObjectCommand({ Bucket, Key }), {
  abortSignal: AbortSignal.timeout(3_000),
});
// aborts the request, and the SDK will not keep retrying past it
```

### Budget the whole thing

The worst-case time of one SDK call is roughly `attempts × requestTimeout + total backoff`. That must fit **inside** the caller's own limit:

```text
Lambda timeout        30 s
 ├─ other work         ~5 s
 └─ SDK call budget   ≤ 3 attempts × 5 s timeout + backoff ≈ 16 s   ✔ fits
```

In Lambda, derive the deadline from the time actually left:

```ts
export const handler = async (event: unknown, context: Context) => {
  const remaining = context.getRemainingTimeInMillis();
  await s3.send(cmd, { abortSignal: AbortSignal.timeout(Math.max(remaining - 1_000, 0)) });
};
```

If your Lambda is invoked by SQS, also keep the **visibility timeout** comfortably above the function timeout ([SQS](../02-services/03-sqs.md), [event-driven Lambda](../03-lambda/03-event-driven-lambda.md)). Otherwise a slow-but-healthy call gets redelivered mid-flight.

---

## Throttling

Throttling means "slow down", not "broken". Typical sources:

| Service | What you see | Usual cause |
|---|---|---|
| DynamoDB | `ProvisionedThroughputExceededException`, `ThrottlingException` | Provisioned capacity exceeded, or a **hot partition** (one key getting all the traffic) |
| S3 | 503 `SlowDown` | Very high request rate on one key prefix (S3 documents per-prefix request rates; spread keys across prefixes) |
| SQS | Rarely throttled; limits mostly on in-flight messages | Too many in-flight messages |
| Lambda | `TooManyRequestsException` (429) | Concurrency limit reached |
| Bedrock | `ThrottlingException` | Per-model requests or tokens per minute quota |
| SES | `TooManyRequestsException` | Send rate exceeded |
| API Gateway | 429 | Throttling limits on the stage/route/account |

The SDK retries throttling errors with backoff, but if you keep generating more load than the resource allows, retries just queue up. The real fixes:

1. **Reduce concurrency** at the source: SQS event source `maxConcurrency`, Lambda reserved concurrency, `p-limit` around a bulk loop in a script.
2. **Smooth the load**: put a queue in front, spread keys (avoid hot partitions), add jitter to scheduled jobs.
3. **Raise the limit** with provisioned capacity, on-demand mode, or a quota increase request, when the load is legitimate.
4. Let failed work go back to a queue and retry later, rather than blocking in a tight loop.

---

## Backoff and jitter, and your own retry helper

For work the SDK doesn't retry (a third-party API, a business-level conflict, an `UnprocessedItems` loop), write one small helper with **exponential backoff and full jitter**. Jitter (randomizing the wait) matters because without it, every failed client retries at the same instants and re-creates the spike.

```ts
export async function retry<T>(
  fn: (attempt: number) => Promise<T>,
  { retries = 4, baseMs = 100, capMs = 5_000, isRetryable = (_e: unknown) => true } = {}
): Promise<T> {
  for (let attempt = 0; ; attempt++) {
    try {
      return await fn(attempt);
    } catch (err) {
      if (attempt >= retries || !isRetryable(err)) throw err;
      const delay = Math.random() * Math.min(capMs, baseMs * 2 ** attempt); // full jitter
      await new Promise((r) => setTimeout(r, delay));
    }
  }
}
```

`isRetryable` is where you encode the transient-vs-permanent decision. Never retry blindly.

---

## Partial failures: a 200 that contains errors

Batch APIs report per-item failures **inside a successful response**. The SDK sees a 200 and retries nothing. If you don't check, you silently lose data.

| API | Where failures appear |
|---|---|
| DynamoDB `BatchWriteItem` | `UnprocessedItems` |
| DynamoDB `BatchGetItem` | `UnprocessedKeys` |
| SQS `SendMessageBatch` / `DeleteMessageBatch` | `Failed` |
| SNS `PublishBatch` | `Failed` |
| EventBridge `PutEvents` | `FailedEntryCount` and per-entry `ErrorCode` |
| S3 `DeleteObjects` | `Errors` |
| Kinesis `PutRecords` | `FailedRecordCount` |

Retry **only the failed items**, with backoff:

```ts
import { BatchWriteCommand } from "@aws-sdk/lib-dynamodb";

export async function batchPut(table: string, items: Record<string, unknown>[]) {
  for (let i = 0; i < items.length; i += 25) {                 // 25 items per request max
    let pending: Record<string, any> | undefined = {
      [table]: items.slice(i, i + 25).map((Item) => ({ PutRequest: { Item } })),
    };
    for (let attempt = 0; pending && Object.keys(pending).length; attempt++) {
      if (attempt >= 6) throw new Error("batch write did not drain");
      const res = await ddb.send(new BatchWriteCommand({ RequestItems: pending }));
      pending = res.UnprocessedItems;
      if (pending && Object.keys(pending).length) {
        await new Promise((r) => setTimeout(r, Math.random() * 100 * 2 ** attempt));
      }
    }
  }
}
```

---

## Classifying errors in your code

Split failures into three buckets and treat them differently:

| Kind | Examples | Do |
|---|---|---|
| **Transient** | Throttling, timeouts, 5xx, connection reset | Retry with backoff (the SDK may already have) |
| **Permanent** | `AccessDenied`, `ValidationException`, malformed message, `NoSuchKey` you didn't expect | Fail fast; send to a DLQ; don't retry |
| **Expected conflict** | `ConditionalCheckFailedException`, `NotFound` on a lookup | Handle as normal control flow (duplicate request, missing record) |

Detect them by class or name, not by message text:

```ts
import { NoSuchKey, S3ServiceException } from "@aws-sdk/client-s3";

try {
  await s3.send(new GetObjectCommand({ Bucket, Key }));
} catch (err) {
  if (err instanceof NoSuchKey) return undefined;                   // expected
  if (err instanceof S3ServiceException && err.$fault === "server") throw err; // transient: let a retry layer handle it
  throw err;
}
```

Service exception classes are exported from each client package. For errors without a dedicated class, `err.name` works. Use `err.$retryable` and `err.$fault` when you want to know what the SDK itself considers retryable.

Never swallow errors. A `catch` that logs and returns normally tells Lambda and queues "success", so nothing retries and the message is deleted.

---

## Where failed work ends up

Retries are finite. Decide where work goes when they run out, **before** you ship:

| Trigger/path | Safety net |
|---|---|
| SQS queue | DLQ via redrive policy, plus an alarm on its depth |
| Async Lambda (S3, SNS, EventBridge) | On-failure destination or DLQ on the function |
| DynamoDB Streams | Retry limits, bisect-on-error, on-failure destination |
| SNS subscription | Subscription DLQ |
| EventBridge target | Target DLQ |
| Your own HTTP/API handler | A clear 4xx/5xx, and a queue if the work can be deferred |

A safety net nobody watches is just slow data loss. Alarm on them ([logging and tracing](./02-logging-and-tracing.md)), and have a replay procedure (redrive from the DLQ after fixing the bug).

---

## Debugging

| Symptom | Likely cause | Check |
|---|---|---|
| Intermittent `ECONNRESET` / "socket hang up" | Stale pooled connection, or a flaky network | The SDK retries it; if frequent, check keep-alive and timeouts |
| Calls hang until the Lambda times out | No request timeout set | `NodeHttpHandler` timeouts, `abortSignal` |
| `ThrottlingException` bursts | Load exceeds capacity or a hot key | Concurrency limits, key distribution, capacity mode |
| Data missing after a batch write | Ignored `UnprocessedItems`/`Failed` | Check batch response fields |
| Same side effect happens twice | Retry of a non-idempotent operation | Idempotency key or dedupe record |
| `RequestTimeTooSkewed` / `SignatureDoesNotMatch` | System clock skew | Fix NTP/time sync |
| Retries seem to do nothing | The error is a permanent client error | Look at `$fault` and the error name |
| Latency doubles under load | Backoff delays from throttling | Log `$metadata.attempts` and `totalRetryDelay` |
| Downstream overwhelmed during an incident | Stacked retries (queue × handler × SDK) | Retry at one layer; lower `maxAttempts` |

To see each attempt as it happens, give the client a logger (`new S3Client({ logger: console })`), as in the [SDK note](../01-setup/01-sdk-v3-and-credentials.md).

---

## Quick summary

- The SDK retries **transient** errors (network, timeout, 5xx, throttling) by default: 3 attempts, exponential backoff with jitter, plus a retry quota. It never retries permanent client errors.
- Tune with `maxAttempts` or a retry strategy; read `$metadata.attempts` to see what really happened.
- Retry layers **multiply** (queue × handler × SDK). Retry at one level, and keep everything idempotent.
- Set connection and request **timeouts** and a per-call `abortSignal`; make `attempts × timeout + backoff` fit inside the Lambda timeout and below the queue visibility timeout.
- Throttling is solved by reducing or smoothing load, not by retrying harder.
- Batch APIs hide failures in a 200: retry `UnprocessedItems`/`Failed`/`FailedEntryCount` yourself.
- Classify errors (transient / permanent / expected), never swallow them, and give failures somewhere to go, with an alarm.

## Next

[Logging and tracing](./02-logging-and-tracing.md): seeing all of the above while it's happening.