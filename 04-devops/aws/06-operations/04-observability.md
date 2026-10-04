# Observability: CloudWatch and Tracing

Observability is your ability to answer "what is my system doing, and why is it broken?" from the outside, without attaching a debugger to production. On AWS it rests on three signals:

| Signal | Question it answers | AWS home |
|---|---|---|
| **Logs** | What happened? (events, errors, details) | CloudWatch Logs |
| **Metrics** | How is it behaving? (rates, latencies, saturation over time) | CloudWatch Metrics, Alarms, Dashboards |
| **Traces** | Where did this request spend its time, across services? | AWS X-Ray / OpenTelemetry, CloudWatch Application Signals |

A fourth tool is often confused with these: **CloudTrail** records **who called which AWS API** (an audit trail), not what your application is doing. It's for security and "who changed this?" ([Security and Secrets](./03-security-and-secrets.md)), not for debugging your code.

Most AWS services emit logs and metrics automatically. Your job is to **choose what to alert on, make your own telemetry useful, and control the cost.**

Prerequisites: [Lambda](../02-compute/02-lambda.md) and [ECS](../02-compute/03-ecs-fargate.md) (the sources of most logs you'll read).

---

## CloudWatch Logs

```
Log group  /aws/lambda/create-order        (one per application/function; holds retention + access settings)
 └── Log stream  2026/10/04/[$LATEST]abc…  (one per execution environment / container / instance)
      └── Log events (timestamped lines)
```

- **Lambda** writes to `/aws/lambda/<function>` automatically (its execution role needs permission to create log groups/streams and put events, which the basic execution policy provides).
- **ECS** sends container stdout/stderr with the `awslogs` driver (or FireLens for routing) ([ECS](../02-compute/03-ecs-fargate.md)). **EC2** needs the CloudWatch agent to ship logs.
- **API Gateway, ALB, VPC Flow Logs, RDS, CloudTrail** can deliver logs to CloudWatch Logs or S3 when enabled.

### Set retention (the cheapest win)

New log groups **never expire** by default, so you pay for storage forever.

```bash
aws logs put-retention-policy --log-group-name /aws/lambda/create-order --retention-in-days 30
```

Set retention in your IaC for every log group you own (and for Lambda, create the log group explicitly so you can control it). Archive to S3 if you need long-term retention cheaply. CloudWatch also offers an **Infrequent Access** log class for logs you rarely query.

### Write logs that can be queried: structured JSON

```ts
// Unstructured: hard to filter or aggregate
console.log(`Order ${id} failed for ${customer}: ${err.message}`);

// Structured: every field is queryable
console.log(JSON.stringify({
  level: "error",
  msg: "order failed",
  orderId: id,
  customerId: customer,
  err: err.message,
  requestId: ctx.awsRequestId,
}));
```

Include a **correlation/request ID** in every log line and pass it across service calls (headers, message attributes), so you can follow one request through many services. **Powertools for AWS Lambda** (TypeScript/Python/Java/.NET) provides a Logger that does structured JSON, correlation IDs and cold-start flags for you, plus Metrics utilities. Check its docs for the current tracing integration.

**Never log secrets, tokens, or unnecessary PII.** Logs are widely readable and long-lived.

### Logs Insights: querying logs

```sql
-- Recent errors
fields @timestamp, level, msg, orderId, requestId
| filter level = "error"
| sort @timestamp desc
| limit 50

-- Lambda performance and memory headroom (from the REPORT line)
filter @type = "REPORT"
| stats avg(@duration), pct(@duration, 99), max(@maxMemoryUsed / 1024 / 1024) as maxMB by bin(5m)

-- Error count by message
filter level = "error"
| stats count() as n by msg
| sort n desc
```

Logs Insights automatically discovers JSON fields, which is the payoff for structured logging. Queries are billed by **data scanned**, so narrow the time range and log groups.

### From logs to metrics and streams

- **Metric filters** turn matching log lines into CloudWatch metrics (e.g. count of `"level":"error"`) that you can alarm on.
- **Subscription filters** stream logs in near-real time to Lambda, Kinesis/Firehose, or OpenSearch/S3 for processing or archival.

---

## CloudWatch Metrics

A **metric** is a time series identified by **namespace + name + dimensions**: e.g. `AWS/Lambda` / `Errors` / `FunctionName=create-order`.

- AWS services publish **default metrics** for free or at no extra cost (Lambda: `Invocations`, `Errors`, `Duration`, `Throttles`, `ConcurrentExecutions`; ALB: `HTTPCode_Target_5XX_Count`, `TargetResponseTime`; SQS: `ApproximateAgeOfOldestMessage`; DynamoDB: throttled requests; and so on).
- **EC2 doesn't publish memory or disk-usage metrics** by default. Install the CloudWatch agent.
- Basic metrics have **coarse resolution** (often 1–5 minutes); some services/options give finer granularity. Choose the **statistic** carefully: averages hide tail latency, so look at **p95/p99** and `Maximum`.

### Custom metrics

Publish business and application metrics (orders placed, queue depth in your own terms). The efficient way from Lambda/containers is the **Embedded Metric Format (EMF)**: write a specially shaped JSON log line, and CloudWatch extracts metrics from it, with no API call in your hot path:

```ts
console.log(JSON.stringify({
  _aws: {
    Timestamp: Date.now(),
    CloudWatchMetrics: [{
      Namespace: "MyApp/Orders",
      Dimensions: [["Service"]],
      Metrics: [{ Name: "OrdersPlaced", Unit: "Count" }],
    }],
  },
  Service: "checkout",
  OrdersPlaced: 1,
  orderId: "o-123",          // extra fields stay searchable in the log, not as metric dimensions
}));
```

**Cost warning:** each unique combination of metric name and dimension values is a **separate billed metric**. Putting `userId` or `orderId` in dimensions creates unbounded cardinality and a surprise bill. Keep dimensions low-cardinality (service, endpoint, status class) and leave high-cardinality values in log fields.

---

## Alarms: alert on symptoms, not noise

An alarm watches a metric against a threshold and changes state: `OK`, `ALARM`, or `INSUFFICIENT_DATA`. Wire alarms to an **SNS topic** (email, Slack via Chatbot, PagerDuty) or automated actions ([SNS](../05-app-services/02-sns-and-eventbridge.md)).

```bash
aws cloudwatch put-metric-alarm \
  --alarm-name create-order-errors \
  --namespace AWS/Lambda --metric-name Errors \
  --dimensions Name=FunctionName,Value=create-order \
  --statistic Sum --period 60 --evaluation-periods 5 --datapoints-to-alarm 3 \
  --threshold 5 --comparison-operator GreaterThanOrEqualToThreshold \
  --treat-missing-data notBreaching \
  --alarm-actions arn:aws:sns:ap-south-1:111122223333:ops-alerts
```

What to alarm on, per tier. Pick **user-visible symptoms** first:

| Layer | Alarm on |
|---|---|
| Any service | **Error rate** and **latency (p99)** at the edge (ALB `5XX`, API Gateway `5XXError`/`Latency`) |
| Lambda | `Errors`, `Throttles`, `Duration` near timeout, `ConcurrentExecutions` near limit |
| Queues | **DLQ depth > 0**, `ApproximateAgeOfOldestMessage` rising ([SQS](../05-app-services/01-sqs.md)) |
| Containers | Running task count below desired, memory near limit ([ECS](../02-compute/03-ecs-fargate.md)) |
| Databases | `FreeStorageSpace`, connections near max, CPU/credit balance, replica lag ([RDS](../03-storage-and-databases/03-rds-and-aurora.md)) |
| DynamoDB | Throttled requests, system errors ([DynamoDB](../03-storage-and-databases/02-dynamodb.md)) |
| Cost/Security | Budget alerts ([Accounts](../01-foundations/02-accounts-regions-and-billing.md)), root login, GuardDuty findings |

Practical rules:

- **Choose `treat-missing-data` deliberately.** A function that stops being invoked emits *no* error datapoints, so "no data" can mean "all good" or "completely down". Alarm on `Invocations` dropping too if silence is a failure.
- Use **multiple datapoints** (`3 of 5`) to avoid flapping.
- **Anomaly detection** alarms learn a metric's normal band for seasonal traffic. **Composite alarms** combine alarms to cut noise.
- Every alarm should have a **clear action or runbook**. If nobody would act on it at 3 a.m., make it a dashboard line or a ticket, not a page. Alert fatigue is how real incidents get ignored.
- Define alarms in **IaC**, not by hand ([IaC](./01-infrastructure-as-code.md)). In CDK, `metric.createAlarm(...)` is concise.

### Dashboards and synthetic checks

- **Dashboards** show the golden signals per service (traffic, errors, latency, saturation) on one screen. Keep one per service, not one giant wall.
- **CloudWatch Synthetics canaries** run scripted checks (HTTP/browser flows) on a schedule from outside, which catch outages and broken flows even when there's no real traffic.
- Define **SLOs** (e.g. "99.9% of requests succeed under 500 ms") and alert on how fast you're burning the error budget. CloudWatch **Application Signals** supports SLOs.

---

## Tracing: following a request across services

Logs tell you what one component did. A **trace** shows one request's whole journey: API Gateway → Lambda → DynamoDB → SQS → another Lambda, with timings for each hop.

### X-Ray, OpenTelemetry and Application Signals (what changed)

- **AWS X-Ray** is the trace backend and service map. The **X-Ray service remains supported and is gaining features**.
- The **X-Ray SDKs and Daemon entered maintenance mode on 25 February 2026**: security fixes only, no new features. AWS recommends **OpenTelemetry (OTel)** for instrumentation, using the **AWS Distro for OpenTelemetry (ADOT)** or the **CloudWatch Agent** (which can collect traces via OTLP) to send spans to X-Ray.
- **CloudWatch Application Signals** is an APM layer on top of OTel traces: auto-discovered services, RED metrics (rate/errors/duration) and **SLOs**. **Transaction Search** lets you search and analyse spans across your traces.
- So for new work: **instrument with OpenTelemetry**, not the X-Ray SDK. Existing X-Ray SDK code keeps working, but don't invest further in it.

### Lambda tracing

For Lambda, **Active tracing** is a function setting (one toggle in the console, or `tracing: lambda.Tracing.ACTIVE` in CDK). Lambda then creates trace segments for each invocation and links upstream sampled requests (e.g. from API Gateway) and downstream SQS-triggered invocations into one trace. Add OTel/ADOT instrumentation (e.g. the ADOT Lambda layer) for detail on outgoing calls and your own spans. Check current Lambda docs for the supported instrumentation options and layers.

### ECS/EC2/containers

Run the **ADOT Collector or CloudWatch Agent** as a sidecar/daemon, instrument the app with the OTel SDK (or zero-code auto-instrumentation where available), and export to it via OTLP.

### Sampling and cost

Tracing every request is expensive and unnecessary. Use **sampling** (a percentage of requests, plus always-sample errors/slow requests). Trace data and Application Signals/Transaction Search have their own pricing, so check the current pricing page before enabling everything everywhere.

---

## A practical minimum for a new service

1. **Structured JSON logs** with a request ID, at INFO level by default; **retention set** on every log group.
2. **Alarms**: edge error rate and latency, queue DLQ depth, key resource saturation, a "service is silent" check.
3. **One dashboard** with the four golden signals.
4. **Tracing** on (Lambda Active tracing and/or OTel), sampled.
5. **Runbook links** in alarm descriptions.
6. Alerts routed to a place a human actually looks, and tested.

---

## Common mistakes

- **Log groups with infinite retention.**
- **Unstructured logs**, so you can't filter, count or correlate.
- **No correlation ID** across services.
- **Logging secrets/PII**, or logging huge payloads on every request (a surprisingly big bill).
- **Debug-level logging in production** without a switch.
- **Alerting on CPU/averages** instead of user-visible symptoms; ignoring p99.
- **Alarms with no action**, or sent to an inbox nobody reads, causing alert fatigue.
- **Wrong `treat-missing-data`**, so a dead service looks healthy.
- **High-cardinality dimensions** on custom metrics.
- **No memory/disk metrics on EC2**, because the agent was never installed.
- **Tracing 100% of traffic** with no sampling plan.
- **Starting new work on the X-Ray SDK** instead of OpenTelemetry.
- **Confusing CloudTrail with application logging.**
- Building alarms and dashboards **by hand** instead of in IaC.

---

## Debugging your observability

| Symptom | Check |
|---|---|
| **No logs from Lambda** | Execution role lacks `logs:CreateLogGroup/CreateLogStream/PutLogEvents`; function never invoked; looking in the wrong **region** or log group name; a log-group KMS key the role can't use |
| **No logs from ECS tasks** | Log group doesn't exist; **execution role** can't write logs; wrong `awslogs-region`; task failed before the app started ([ECS](../02-compute/03-ecs-fargate.md)) |
| Logs delayed or missing the last lines | Ingestion delay; process exited before flushing stdout; check stream ordering/time range |
| **Metric not appearing** | Wrong namespace/dimension values (they must match exactly); not enough datapoints in the period; custom metric not yet published; wrong region |
| Alarm stuck in **`INSUFFICIENT_DATA`** | Metric has no datapoints in the period (idle service, wrong dimensions); fix dimensions or `treat-missing-data` |
| Alarm never fires | Period/evaluation too lenient, wrong statistic, threshold unreachable, actions not attached |
| Alarm fires but nobody notified | SNS topic has no confirmed subscription, or its access policy/KMS blocks CloudWatch |
| **Missing traces** | Active tracing not enabled; role lacks X-Ray/OTel export permission; sampling dropped it; the collector/sidecar isn't reachable; **trace headers not propagated** across a hop |
| Logs Insights returns nothing | Time range or log group selection wrong; field name mismatch (JSON key case) |
| Surprise CloudWatch bill | Log ingestion volume, no retention, high-cardinality custom metrics, detailed monitoring everywhere, queries scanning huge ranges, excess tracing |

Use Cost Explorer grouped by **usage type** to see whether logs ingestion, storage, metrics, or traces dominate.

---

## Quick Summary

- Three signals: **logs, metrics, traces**; **CloudTrail** is for auditing API calls, not for debugging your app.
- **Set log retention**, write **structured JSON** logs with a **correlation ID**, query with **Logs Insights**, never log secrets.
- Metrics: know the defaults per service, use **EMF** for custom metrics, keep dimensions **low-cardinality**, and watch **p99**, not averages.
- **Alarm on user-visible symptoms** (error rate, latency, DLQ depth, silence), choose `treat-missing-data` carefully, route to humans, and define alarms in IaC.
- Tracing: **X-Ray service lives on**, but the **X-Ray SDK/Daemon are in maintenance mode (since Feb 2026)**. Use **OpenTelemetry (ADOT/CloudWatch Agent)**, with **Application Signals** for SLOs and **Lambda Active tracing** for quick wins.
- Sample traces, and watch observability costs.

**Next:** [Reliability and Cost](./05-reliability-and-cost.md)
