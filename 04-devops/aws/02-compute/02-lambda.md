# Lambda: Functions as a Service

Lambda runs your code in response to events without you managing servers. You upload a function, tell AWS what should trigger it, and AWS provisions execution environments, scales them with demand, and bills you only for requests and execution time. If nothing calls it, you pay (almost) nothing.

It is the default choice for event-driven glue, APIs with variable traffic, scheduled jobs, and file/queue/stream processing. It is a poor fit for very long-running work, sustained high-throughput services where per-request billing outgrows containers, or anything needing a persistent connection to the function itself.

Prerequisite: [IAM](../01-foundations/03-iam.md) (every function runs with an execution role).

---

## The model

```
 Event source ──► Lambda service ──► execution environment ──► your handler(event, context)
 (API GW, S3,                        (micro-VM, created on demand,
  SQS, schedule…)                     reused for later invocations)
```

You write a **handler**: a function that receives an `event` (JSON whose shape depends on the trigger) and returns a result.

```ts
// index.mjs / index.ts compiled to JS: Node.js runtime
export const handler = async (event) => {
  console.log("event:", JSON.stringify(event));
  return {
    statusCode: 200,
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ message: "hello" }),
  };
};
```

Notes on the Node.js runtimes:

- Use **`async` handlers**. Callback-style handlers are not supported on newer runtimes (removed as of Node.js 24).
- The shape `{ statusCode, headers, body }` is the response format for **API Gateway / Function URL** triggers specifically. A function triggered by S3 or SQS returns whatever is useful (or nothing).
- Lambda-supported runtimes are on a published schedule and old ones get deprecated (patching stops, then creating and updating functions is blocked). Check the [runtimes page](https://docs.aws.amazon.com/lambda/latest/dg/lambda-runtimes.html) when choosing, for example `nodejs22.x` or `nodejs24.x` rather than an old runtime from a tutorial.

---

## Invoking: three modes

| Mode | Who waits? | Retries | Examples |
|---|---|---|---|
| **Synchronous** | Caller waits for the result | **Caller** decides | API Gateway, Function URL, ALB, direct `invoke` |
| **Asynchronous** | Lambda queues the event and returns immediately | Lambda retries failures (twice by default), then DLQ/destination | S3 events, SNS, EventBridge |
| **Poll-based (event source mapping)** | Lambda polls the source and invokes you with batches | Depends on source | SQS, Kinesis, DynamoDB Streams |

This distinction explains most behaviour: who handles errors, who retries, and what "failure" even means differs per mode.

```bash
aws lambda invoke --function-name my-fn \
  --payload '{"hello":"world"}' --cli-binary-format raw-in-base64-out out.json
cat out.json
```

---

## The execution environment lifecycle (cold starts)

```
First request:   [ create env ][ download code ][ init: code outside handler ][ handler ]   ← cold start
Next requests:                                                              [ handler ]    ← warm
```

- A **cold start** happens when Lambda must create a new environment: download code, start the runtime, run your **initialisation code** (everything outside the handler).
- Environments are **reused** for subsequent invocations while warm, then discarded after idle time you don't control.
- Concurrency = environments running at once. Two simultaneous requests need two environments. Each handles **one request at a time**.

Consequences for how you write code:

```ts
import { DynamoDBClient } from "@aws-sdk/client-dynamodb";

// Runs once per environment: reused across warm invocations
const client = new DynamoDBClient({});

export const handler = async (event) => {
  // Runs every invocation
  ...
};
```

- **Create SDK clients and DB connections outside the handler** so warm invocations reuse them.
- **Don't rely on in-memory state.** It may or may not persist, and it's never shared across concurrent environments. Persist to DynamoDB/S3.
- `/tmp` offers scratch space (512 MB by default, configurable up), which may persist between warm invocations but never count on it.
- Reduce cold start impact by keeping the package small, avoiding heavy init work, and choosing faster-starting runtimes. Mitigations if it matters: **provisioned concurrency** (pre-warmed environments, billed for being kept ready) and **SnapStart** (snapshotted init, available for selected runtimes).

---

## Configuration that matters

| Setting | Notes |
|---|---|
| **Memory** (128 MB to 10 GB) | Also scales **CPU** proportionally. More memory often *reduces total cost* if the function finishes faster. Measure rather than guess (the open-source "Lambda Power Tuning" tool automates this). |
| **Timeout** | Max **15 minutes**. Set it just above your realistic duration. The default is only a few seconds. |
| **Execution role** | IAM role the function runs as. Give it only what it needs. |
| **Environment variables** | Plain config. For secrets use Secrets Manager / Parameter Store ([Security and Secrets](../06-operations/03-security-and-secrets.md)). |
| **Architecture** | `arm64` (Graviton) is typically cheaper; `x86_64` for compatibility. |
| **Reserved concurrency** | Caps (and guarantees) a function's concurrency, which is useful to protect a downstream database. |
| **Package** | Zip (size limits apply) or a **container image** (up to 10 GB). Layers share dependencies. Check current quotas in Service Quotas. |

Pricing in one line: **requests × duration × memory** (GB-seconds), with a free monthly allowance. Check the pricing page for current numbers.

---

## Practical example: S3 upload → process → write to DynamoDB

```ts
import { S3Client, GetObjectCommand } from "@aws-sdk/client-s3";
import { DynamoDBClient, PutItemCommand } from "@aws-sdk/client-dynamodb";

const s3 = new S3Client({});
const ddb = new DynamoDBClient({});

export const handler = async (event) => {
  for (const record of event.Records) {
    const bucket = record.s3.bucket.name;
    // S3 keys in events are URL-encoded ('+' for spaces)
    const key = decodeURIComponent(record.s3.object.key.replace(/\+/g, " "));

    const obj = await s3.send(new GetObjectCommand({ Bucket: bucket, Key: key }));
    const size = obj.ContentLength ?? 0;

    await ddb.send(new PutItemCommand({
      TableName: process.env.TABLE_NAME,
      Item: { pk: { S: key }, size: { N: String(size) } },
    }));
  }
};
```

The execution role needs `s3:GetObject` on the bucket and `dynamodb:PutItem` on the table, and nothing more. Beware of **recursive triggers**: if a function writes back into the same bucket that triggers it, you can create an infinite (and expensive) loop. Use separate prefixes/buckets and filter the trigger.

---

## Working with queues (SQS) and partial failures

With SQS as a poll-based source, Lambda receives a **batch**. If your handler throws, the **whole batch** returns to the queue and is retried, including messages that already succeeded. Report only the failures:

```ts
export const handler = async (event) => {
  const batchItemFailures = [];
  for (const record of event.Records) {
    try {
      await process(JSON.parse(record.body));
    } catch {
      batchItemFailures.push({ itemIdentifier: record.messageId });
    }
  }
  return { batchItemFailures };   // requires "ReportBatchItemFailures" enabled on the mapping
};
```

Set a **dead-letter queue** on the SQS queue so poison messages don't loop forever. See [SQS](../05-app-services/01-sqs.md).

---

## Reliability rules for event-driven code

- **Make handlers idempotent.** Lambda guarantees *at-least-once* delivery for most event sources, so the same event can arrive twice. Use idempotency keys or conditional writes.
- **Configure failure handling:** for async invocations, set retry attempts and a **DLQ or on-failure destination**; for streams, set a bisect-on-error / on-failure destination.
- **Mind the timeout vs source visibility timeout.** For SQS, the queue's visibility timeout should comfortably exceed the function timeout.
- **Protect downstream systems.** A burst can scale Lambda far faster than a relational database can accept connections. Use reserved concurrency, a queue in between, or RDS Proxy ([RDS and Aurora](../03-storage-and-databases/03-rds-and-aurora.md)).

---

## VPC, networking and the internet

By default a function runs in an AWS-managed network with internet access. If you attach it to **your VPC** (to reach a private RDS instance, say):

- It loses default internet access. Reaching the internet or AWS APIs needs a **NAT Gateway** or **VPC endpoints** (see [VPC](../04-networking/01-vpc.md)).
- Only attach to a VPC when you need private resources, because it adds configuration and possible NAT cost.

---

## Exposing a function over HTTP

| Option | Use when |
|---|---|
| **Function URL** | Simple dedicated HTTPS endpoint, no extras |
| **API Gateway** | Auth, throttling, usage plans, routing, request validation ([API Gateway](../04-networking/03-load-balancing-and-api-gateway.md)) |
| **ALB target** | You already have an ALB in front of other targets |

---

## Common mistakes

- **Initialising clients inside the handler**, which pays setup cost on every call.
- **Assuming warm state** or sharing it across concurrent requests.
- **Not handling duplicate events**, which causes double charges or double writes.
- **Recursive invocation** (function writes to the bucket/queue that triggers it).
- **Timeout too high/low**: a hung call burns money until the timeout; a too-short one kills good work.
- **Giving the function a `*:*` role.**
- **Putting it in a VPC "just because"** and then losing internet access.
- **Treating Lambda as a long-running server**: the 15-minute cap and per-request scaling model don't match it. Use [Fargate](./03-ecs-fargate.md) or EC2.
- **Using an old runtime** copied from a tutorial. Check the runtime support schedule.
- **Unbounded log verbosity**: CloudWatch Logs ingestion can cost more than the function itself.

---

## Debugging

1. **Logs:** anything printed (`console.log`) goes to a CloudWatch log group named `/aws/lambda/<function>`.
   ```bash
   aws logs tail /aws/lambda/my-fn --follow
   ```
   Each invocation ends with a `REPORT` line showing **duration, billed duration, memory size and max memory used**: this is your tuning data.
2. **Reproduce locally** with a saved event: `aws lambda invoke`, or `sam local invoke` if you use SAM ([Infrastructure as Code](../06-operations/01-infrastructure-as-code.md)).
3. **`Task timed out after X seconds`**: raise the timeout, or find what's slow (a downstream call, a cold dependency, or a VPC function with no NAT).
4. **Out of memory** (`Runtime exited with error: signal: killed`): compare max memory used to configured memory.
5. **`AccessDeniedException`**: execution role missing a permission (see [IAM](../01-foundations/03-iam.md)). Separately, a *trigger* that can't invoke the function is a missing **resource-based permission** on the function.
6. **Throttling (`429` / `Rate Exceeded`)**: you've hit the account/regional concurrency limit or the function's reserved concurrency. Check the `Throttles` metric.
7. **Distributed tracing:** enable X-Ray / use CloudWatch metrics (`Errors`, `Duration`, `ConcurrentExecutions`). See [Observability](../06-operations/04-observability.md).

---

## Lambda vs the alternatives

| | Lambda | Fargate | EC2 |
|---|---|---|---|
| Unit | Function (per request) | Container (per task) | VM |
| Scaling | Automatic, very fast, to zero | Service autoscaling, slower | Auto Scaling group |
| Max run time | 15 min | Unlimited | Unlimited |
| Idle cost | ~0 | Pay while tasks run | Pay while instances run |
| Ops burden | Lowest | Low | Highest |
| Best for | Spiky/event-driven | Steady services, long jobs | Full control |

---

## Quick Summary

- Lambda = event-triggered functions, billed per request × duration × memory, scaling automatically.
- Three invocation models (sync, async, poll-based), and **retries/error handling depend on which**.
- Init code runs per environment; **create clients outside the handler**, keep nothing important in memory, expect cold starts.
- Memory also sets CPU: tune it by measuring.
- Be **idempotent**, configure DLQs/destinations, report partial batch failures for SQS, and protect downstream databases with concurrency limits.
- Debug via CloudWatch logs (`REPORT` line), the execution role, and the throttle/timeout metrics.

**Next:** [ECS and Fargate](./03-ecs-fargate.md)