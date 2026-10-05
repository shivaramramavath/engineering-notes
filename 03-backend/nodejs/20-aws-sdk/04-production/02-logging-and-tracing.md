# Logging and Tracing

You can't attach a debugger to a Lambda or a Fargate task at 3 a.m. What you have is what the code emitted: **logs** (what happened), **metrics** (how much, how often) and **traces** (where the time went across services). On AWS these land in CloudWatch Logs, CloudWatch Metrics and X-Ray. The goal of this note is a small, consistent setup that lets you go from "an alarm fired" to "this exact request failed here, in this call" in a few minutes.

Prerequisites: [Lambda handlers](../03-lambda/01-lambda-handlers.md) and the failure paths in [errors and retries](./01-errors-and-retries.md).

---

## Where output goes

- **Lambda**: anything written to stdout/stderr (`console.log`, `console.error`) goes to the log group `/aws/lambda/<function-name>`. The function role needs permission to write logs (the basic execution role provides it).
- **ECS/Fargate**: container stdout/stderr goes to a log group via the task's `awslogs` log driver.
- **API Gateway** access logs and other services write to log groups you configure.

```text
code ──console.log(JSON)──▶ CloudWatch Logs ──▶ Logs Insights (query)
                                  │
                                  ├──▶ metric filters / EMF ──▶ CloudWatch Metrics ──▶ Alarms
                                  └──▶ subscriptions (to Lambda/Firehose/other tools)
```

**Set a retention policy.** Log groups keep logs *forever* by default, and you pay for storage. Choose a retention period per log group (days to weeks for noisy debug logs, longer for audit logs).

---

## Structured logging

Log **one JSON object per line**. CloudWatch Logs Insights discovers JSON fields automatically, so you can filter and aggregate on them. Free-text logs force you to grep.

```ts
console.log(JSON.stringify({
  level: "info",
  message: "order processed",
  orderId: "01J9A",
  durationMs: 84,
  requestId: context.awsRequestId,
}));
```

Guidelines:

- **One line per event.** Multi-line output (pretty-printed objects, stack traces split across lines) becomes many separate log events.
- Always include a **request/correlation ID** so you can find everything about one request.
- Log **identifiers and outcomes**, not whole payloads: `orderId`, `status`, `durationMs`, retry `attempts`.
- **Don't log secrets or sensitive data**: tokens, passwords, API keys, full presigned URLs, personal data, raw user prompts or model outputs unless you've decided that's acceptable. Redaction is much easier up front than after a leak.
- **Log an error once, at the boundary** where you handle it, with context. Logging and rethrowing at every layer produces the same error five times.
- Use levels (`debug`, `info`, `warn`, `error`) and make the level configurable by environment variable, so you can turn on debug for an incident without a deploy.
- Lambda's **advanced logging controls** can emit logs in JSON format and filter by level at the platform; check the Lambda docs if you want that without a library.

### Powertools Logger (Lambda)

Powertools for AWS Lambda (TypeScript) gives you structured JSON logs with Lambda context already attached.

```bash
npm install @aws-lambda-powertools/logger
```

```ts
import { Logger } from "@aws-lambda-powertools/logger";
import type { Context, SQSEvent } from "aws-lambda";

const logger = new Logger({ serviceName: "orders" });   // service name can also come from POWERTOOLS_SERVICE_NAME

export const handler = async (event: SQSEvent, context: Context) => {
  logger.addContext(context);   // adds function name, request id, cold start flag, trace id

  for (const record of event.Records) {
    const { orderId } = JSON.parse(record.body);
    logger.appendKeys({ orderId });                    // included in every log line from now on
    try {
      logger.info("processing order");
      await process(orderId);
    } catch (err) {
      logger.error("order failed", err as Error);      // serializes error name, message and stack
      throw err;
    } finally {
      logger.removeKeys(["orderId"]);                  // don't leak into the next record
    }
  }
};
```

Output is one JSON line per call, with fields such as `level`, `message`, `service`, `timestamp`, `function_request_id`, `cold_start` and `xray_trace_id`. The minimum level is configurable through an environment variable (see the Powertools docs for the current name, such as `POWERTOOLS_LOG_LEVEL`).

Remember the module-scope rule from [Lambda handlers](../03-lambda/01-lambda-handlers.md): the logger instance is shared across invocations, so keys you append **persist** until you remove them. That's why the example cleans up.

### Outside Lambda (Express on ECS)

Use a fast JSON logger such as `pino` writing to stdout; the container's log driver ships it to CloudWatch.

```ts
import pino from "pino";
export const log = pino({ level: process.env.LOG_LEVEL ?? "info" });
// per request: const reqLog = log.child({ requestId });
```

---

## Querying with Logs Insights

Logs Insights is where structured logs pay off.

```text
# recent errors for one service
fields @timestamp, message, orderId, function_request_id
| filter level = "ERROR"
| sort @timestamp desc
| limit 50
```

```text
# everything for one request
fields @timestamp, level, message
| filter function_request_id = "8b1b..."
| sort @timestamp asc
```

Lambda's own `REPORT` lines are queryable too, using the discovered `@duration`, `@initDuration`, `@maxMemoryUsed` and `@memorySize` fields:

```text
# latency percentiles per 5 minutes
filter @type = "REPORT"
| stats avg(@duration), pct(@duration, 95), pct(@duration, 99), max(@duration), count(*) by bin(5m)
```

```text
# how often are cold starts happening, and how slow?
filter @type = "REPORT" and ispresent(@initDuration)
| stats count() as coldStarts, avg(@initDuration) as avgInitMs by bin(1h)
```

```text
# are we over-provisioned on memory?
filter @type = "REPORT"
| stats max(@maxMemoryUsed / 1024 / 1024) as peakMB, max(@memorySize / 1024 / 1024) as configuredMB
```

Queries run over the log groups you select and are billed by data scanned, so narrow the time range and groups.

---

## Metrics

### What you get for free

| Source | Metrics worth watching |
|---|---|
| Lambda | `Errors`, `Throttles`, `Duration`, `ConcurrentExecutions`, `IteratorAge` (stream triggers) |
| SQS | `ApproximateAgeOfOldestMessage`, `ApproximateNumberOfMessagesVisible` (including on the **DLQ**) |
| API Gateway | `4xx`/`5xx` counts, `Latency` |
| DynamoDB | `ThrottledRequests`, `SystemErrors`, consumed capacity |
| SES | Bounce and complaint rates (via reputation metrics) |
| Bedrock | Invocation counts, latency, throttles, token usage |

### Custom metrics with EMF

For business metrics ("orders placed", "payment failures"), don't call the metrics API on every request. Write a log line in **Embedded Metric Format (EMF)** and CloudWatch extracts the metric from your logs asynchronously: no extra network call, no added latency. Powertools Metrics produces EMF for you:

```ts
import { Metrics, MetricUnit } from "@aws-lambda-powertools/metrics";

const metrics = new Metrics({ namespace: "Orders", serviceName: "orders" });

export const handler = async () => {
  metrics.addMetric("OrderPlaced", MetricUnit.Count, 1);
  metrics.addMetadata("orderId", "01J9A");     // searchable in logs, NOT a metric dimension
  metrics.publishStoredMetrics();              // flush as EMF log lines
};
```

**Mind dimensions.** Each unique combination of metric name + dimension values is a separate billable custom metric. Dimensions like `service` or `environment` are fine; **never use high-cardinality values** (`userId`, `orderId`, request ID) as dimensions. Put those in logs or metadata instead. Check current CloudWatch pricing.

---

## Alarms: alert on symptoms

An alarm should answer "do I need to act?", not "did something odd happen?". A small, useful baseline:

| Alarm | Why |
|---|---|
| **DLQ depth > 0** (`ApproximateNumberOfMessagesVisible`) | Messages are failing permanently. Silent data loss otherwise |
| Lambda `Errors` or error rate | Handlers are throwing |
| Lambda `Throttles` > 0 | Concurrency limit hit, work is delayed or dropped |
| SQS `ApproximateAgeOfOldestMessage` high | Consumers are falling behind |
| Stream `IteratorAge` high | Stream processing is stuck or lagging (often a poison record) |
| API Gateway `5xx` rate, p99 `Latency` | Users are affected |
| SES bounce/complaint rates | Approaching account review thresholds ([SES](../02-services/05-ses-email.md)) |

Route alarm actions to an SNS topic that notifies a channel or on-call tool ([SNS](../02-services/04-sns-and-eventbridge.md)). Decide how each alarm treats *missing data* (no invocations may be fine for a nightly job, and not for a payment webhook).

---

## Tracing

A **trace** follows one request across services, as a tree of timed **segments** and **subsegments**. It answers "which downstream call made this request slow?" and "what called what?", which logs alone make tedious. AWS X-Ray collects these and draws a **service map**.

### Turning it on

- **Lambda:** enable **active tracing** on the function. The runtime then creates a segment per invocation and the function role needs permission to send trace data (IaC and the console add it when you enable tracing).
- **Propagation:** the trace ID travels in the `X-Amzn-Trace-Id` header and, for SQS, in the `AWSTraceHeader` message system attribute. A trace can therefore continue from API Gateway → Lambda → SQS → Lambda when each hop has tracing enabled.
- Default sampling records only a fraction of requests, so don't expect every request to have a trace.

### Instrumenting SDK calls

To see each AWS call (S3, DynamoDB, SQS) as a subsegment with its own timing and errors, wrap the client. Powertools Tracer does this on top of the X-Ray SDK:

```bash
npm install @aws-lambda-powertools/tracer
```

```ts
import { Tracer } from "@aws-lambda-powertools/tracer";
import { S3Client, GetObjectCommand } from "@aws-sdk/client-s3";

const tracer = new Tracer({ serviceName: "orders" });
const s3 = tracer.captureAWSv3Client(new S3Client({}));          // every call becomes a subsegment

export const handler = async (event: { orderId: string }) => {
  tracer.putAnnotation("orderId", event.orderId);                // indexed: you can search traces by it
  tracer.putMetadata("input", { size: 1 });                      // not indexed: extra detail

  const parent = tracer.getSegment();
  const sub = parent?.addNewSubsegment("calculate-totals");      // time a section of your own code
  if (sub) tracer.setSegment(sub);
  try {
    await s3.send(new GetObjectCommand({ Bucket: "b", Key: "k" }));
  } catch (err) {
    tracer.addErrorAsMetadata(err as Error);
    throw err;
  } finally {
    sub?.close();
    if (parent) tracer.setSegment(parent);
  }
};
```

- **Annotations** are key/value pairs that are **indexed** (filterable, e.g. find all traces for one `orderId`); **metadata** is not indexed and carries bulk detail.
- Create clients once at module level (as always) and wrap them there.

> **Status note:** AWS has been steering new instrumentation toward **OpenTelemetry** (the AWS Distro for OpenTelemetry, and CloudWatch Application Signals) rather than the original X-Ray SDKs, which Powertools Tracer builds on. X-Ray as a backend and the service map remain, but check the current status of the X-Ray SDK and Powertools Tracer before investing heavily, and consider OpenTelemetry if you're starting a long-lived system.

On ECS you typically run an OpenTelemetry/ADOT collector as a sidecar container to ship traces.

---

## Putting it together: debugging one failure

```text
1. Alarm: DLQ depth > 0 / Lambda errors up
2. Metrics: which function, when did it start, deploy at that time?
3. Logs Insights: filter level="ERROR" in that window → get orderId + request ID
4. Logs for that request ID: the full story, including retry attempts
5. Trace (by annotation orderId): which downstream call failed or was slow
6. Fix → redrive the DLQ ([SQS](../02-services/03-sqs.md))
```

That flow only works if the earlier pieces exist: structured logs with IDs, alarms on the safety nets, annotations on traces.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Unstructured `console.log("got here")` | One JSON object per line with IDs and outcomes |
| Pretty-printed or multi-line logs | Single-line JSON |
| No retention policy | Set retention on every log group |
| Logging secrets, tokens, presigned URLs, personal data | Never log them; redact |
| Same error logged at every layer | Log once where handled |
| Logger keys leaking between invocations | Remove per-request keys (module-scope logger is shared) |
| High-cardinality metric dimensions (`userId`) | Keep IDs in logs/metadata; dimensions low-cardinality |
| Alarms on every error, none on DLQ depth | Alarm on symptoms and safety nets |
| Calling `PutMetricData` per request | Use EMF / Powertools Metrics |
| Expecting every request to be traced | Default sampling records a fraction; sample deliberately |
| Debug logs left on in production | Level from env var; raise only during incidents |
| No correlation ID passed to queues/events | Put it in message attributes or event `detail` |

---

## Quick summary

- stdout → CloudWatch Logs. Log **single-line JSON** with request/correlation IDs; never log secrets; set retention.
- Powertools Logger gives you context-rich JSON in Lambda (mind that its keys persist across invocations); `pino` fills the same role in containers.
- **Logs Insights** queries over JSON fields and the `REPORT` line (`@duration`, `@initDuration`, `@maxMemoryUsed`) answer most questions.
- Use built-in metrics, add business metrics through **EMF**, and keep dimensions low-cardinality.
- Alarm on **DLQ depth, errors, throttles, queue age, iterator age, 5xx and latency**.
- Tracing (X-Ray, or OpenTelemetry) shows where time goes across services; use annotations for searchable IDs, and check the current instrumentation status.

## Next

[Projects](../05-projects/README.md): putting the SDK, Lambda and production practices together in three real builds.