# Tracing & OpenTelemetry

Following a single request through every function, database query, and service it touches — with timings — so you can see exactly *where* the time and the errors come from.

## The question metrics can't answer

Your dashboard says p99 latency for `POST /orders` jumped from 300 ms to 4 seconds. Metrics (`03`) tell you **that** it happened and **how widely**. They can't tell you **why**:

- Was it the database? Which query?
- The payment provider? The email service?
- A cache miss storm? A slow downstream microservice?
- One specific customer's giant order?

A **distributed trace** records the journey of *one* request as a tree of timed steps, so you can look at a slow request and see the answer:

```
POST /api/v1/orders                                         ████████████████████████████████████ 4,012 ms
├─ middleware: authenticate                                 █ 8 ms
├─ orders.service.placeOrder                                ████████████████████████████████████ 3,990 ms
│  ├─ SELECT users WHERE id = $1                            █ 4 ms
│  ├─ SELECT products WHERE id = ANY($1)                    █ 6 ms
│  ├─ payments-service  POST /charges                       ██████████████████████████████████ 3,820 ms   ← the culprit
│  │  ├─ (payments-service) validate card                   █ 5 ms
│  │  └─ HTTP POST api.stripe.com/v1/charges                █████████████████████████████████ 3,790 ms     ← the third party is slow
│  ├─ INSERT orders ...                                     █ 11 ms
│  └─ queue.add email                                       █ 2 ms
└─ response                                                 
```

You didn't guess. You *saw* that 95% of the time was an outbound call to Stripe, and you know what to do next (timeouts, retries, circuit breaker, alert the provider).

---

## Vocabulary

| Term | Meaning |
|---|---|
| **Trace** | The whole journey of one request across all services: identified by a **trace ID** |
| **Span** | One timed operation within a trace (an HTTP handler, a DB query, an outbound call): has a name, start/end time, **span ID**, **parent span ID**, attributes, status |
| **Root span** | The first span in a trace (usually the incoming request) |
| **Attributes** | Key–value details on a span: `http.method`, `db.statement`, `user.id`, `order.id` |
| **Events** | Timestamped notes inside a span ("cache miss", exception recorded) |
| **Status** | `OK`, `ERROR`, or `UNSET` |
| **Context propagation** | Passing the trace ID + parent span ID between services (HTTP headers, message headers) so their spans join the same trace |
| **Instrumentation** | The code that creates spans: **automatic** (libraries patched for you) or **manual** (you write it) |
| **Sampling** | Deciding which traces to keep, since keeping all of them at scale is expensive |
| **Exporter / Collector** | Ships spans to a backend (Jaeger, Tempo, Datadog, Honeycomb, ...) |

---

## What is OpenTelemetry?

**OpenTelemetry (OTel)** is the vendor-neutral, open standard (a CNCF project) for generating and exporting **traces, metrics, and logs**.

- **APIs and SDKs** in every major language (including Node.js)
- **Auto-instrumentation** for popular libraries (Express, `http`, `pg`, `mysql2`, MongoDB, Redis/ioredis, `fetch`/undici, gRPC, Kafka (KafkaJS), AWS SDK, and many more)
- A wire protocol, **OTLP**, that every major observability vendor accepts
- The **OpenTelemetry Collector**: a standalone process that receives, processes (batch, filter, sample), and exports telemetry

Why it matters: **instrument once, send anywhere.** You write instrumentation against OTel and choose or change the backend (Jaeger, Grafana Tempo, Datadog, New Relic, Honeycomb, AWS X-Ray) with configuration, not code. It has largely displaced vendor-specific agents and the older OpenTracing/OpenCensus projects.

```
Your app (OTel SDK) ──OTLP──▶ OTel Collector ──▶ Tempo / Jaeger / Datadog / ...
                                  │ (batching, sampling, redaction, routing)
                                  └──▶ (also metrics → Prometheus, logs → Loki)
```

---

## Setup in Node.js

The SDK must start **before** your application code loads any instrumented library (`express`, `pg`, `http`), because auto-instrumentation works by patching modules as they're imported.

```bash
npm install @opentelemetry/sdk-node \
            @opentelemetry/api \
            @opentelemetry/auto-instrumentations-node \
            @opentelemetry/exporter-trace-otlp-http \
            @opentelemetry/resources \
            @opentelemetry/semantic-conventions
```

```js
// instrumentation.js: loaded BEFORE the app
import { NodeSDK } from "@opentelemetry/sdk-node";
import { getNodeAutoInstrumentations } from "@opentelemetry/auto-instrumentations-node";
import { OTLPTraceExporter } from "@opentelemetry/exporter-trace-otlp-http";
import { resourceFromAttributes } from "@opentelemetry/resources";
import { ATTR_SERVICE_NAME, ATTR_SERVICE_VERSION } from "@opentelemetry/semantic-conventions";

const sdk = new NodeSDK({
  resource: resourceFromAttributes({
    [ATTR_SERVICE_NAME]: "orders-api",                         // how this service appears in the trace UI
    [ATTR_SERVICE_VERSION]: process.env.APP_VERSION ?? "dev",
    "deployment.environment.name": process.env.NODE_ENV ?? "development",
  }),
  traceExporter: new OTLPTraceExporter({
    url: process.env.OTEL_EXPORTER_OTLP_ENDPOINT
      ? `${process.env.OTEL_EXPORTER_OTLP_ENDPOINT}/v1/traces`
      : "http://localhost:4318/v1/traces",                      // the Collector's OTLP/HTTP port
  }),
  instrumentations: [
    getNodeAutoInstrumentations({
      "@opentelemetry/instrumentation-fs": { enabled: false },   // very noisy: usually disable
      "@opentelemetry/instrumentation-http": {
        ignoreIncomingRequestHook: (req) => req.url === "/health" || req.url === "/metrics",   // don't trace probes
      },
    }),
  ],
});

sdk.start();

process.on("SIGTERM", () => {
  sdk.shutdown().finally(() => process.exit(0));                // flush pending spans before exiting
});
```

> **Version note:** this uses the OpenTelemetry JS **2.x** SDK API (`resourceFromAttributes`, `spanProcessors` constructor option). On 1.x, build the resource with `new Resource({...})` (from `@opentelemetry/resources`) and register processors with `provider.addSpanProcessor(...)`. The OTel JS packages move quickly, so check the docs for the versions you install.

Start the app with the instrumentation preloaded:

```json
{
  "scripts": {
    "start": "node --import ./instrumentation.js src/server.js"
  }
}
```

(CommonJS apps use `node --require ./instrumentation.js src/server.js`.)

### ES modules need an extra step

OpenTelemetry's patching relies on hooking `require`. For **native ES modules** (`import`), you also need the ESM loader hook, otherwise auto-instrumentation silently produces nothing. The flags and package names have changed across Node and OTel versions, so check the current *"ESM support"* section of the OpenTelemetry JS docs. Historically it looked like:

```bash
node --experimental-loader=@opentelemetry/instrumentation/hook.mjs --import ./instrumentation.js src/server.js
```

If you run into "no spans appear" with ESM, this is the first thing to check. Bundling to CommonJS, or testing with a tiny CJS entry point, can isolate the problem.

### Configuration through environment variables

OTel SDKs read standard environment variables, which is how you configure them per environment without code changes:

```bash
OTEL_SERVICE_NAME=orders-api
OTEL_EXPORTER_OTLP_ENDPOINT=http://otel-collector:4318
OTEL_EXPORTER_OTLP_PROTOCOL=http/protobuf
OTEL_TRACES_SAMPLER=parentbased_traceidratio
OTEL_TRACES_SAMPLER_ARG=0.1                       # keep 10% of traces
OTEL_RESOURCE_ATTRIBUTES=deployment.environment.name=production,service.version=1.8.2
OTEL_LOG_LEVEL=info                               # set to debug when troubleshooting the SDK itself
```

### What you get for free

With only the setup above, with **zero changes to your handlers**, you get spans for:

| Library | Span examples |
|---|---|
| Incoming HTTP / Express | `GET /api/v1/orders/:id`, plus a span per middleware and router layer |
| `pg` / `mysql2` / MongoDB / Redis | `SELECT orders`, `redis-GET`, with the statement (parameters are not captured by default) |
| Outgoing `http` / `fetch` / undici | `POST api.stripe.com`, with status code and timing |
| gRPC, Kafka, AWS SDK, etc. | Calls to those systems |

Open the traces and you immediately see the shape of your requests and where the time goes.

---

## Context propagation: how a trace crosses service boundaries

When service A calls service B, B's spans must attach to A's trace. OTel does this by passing the trace context in a header. The default is the W3C **Trace Context** standard:

```
traceparent: 00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01
             │  └──────── trace ID ──────────────┘ └─ parent span ID ─┘ └ flags (sampled)
             version
```

- The **outgoing HTTP instrumentation** automatically **injects** `traceparent` into requests.
- The **incoming HTTP instrumentation** automatically **extracts** it and makes the new spans children of the caller's span.
- Both services must run OTel (or any W3C Trace Context-compatible tracer).

The result: one trace ID spans the browser (if instrumented), the API gateway, your API, the payments service, and the database calls, which is the "distributed" in distributed tracing.

### Propagation over queues and brokers

HTTP is handled automatically. **Messages and jobs are not** unless a library instruments them. You inject and extract the context yourself, through message headers/metadata (`11-async-processing/`):

```js
import { context, propagation, trace, SpanKind, SpanStatusCode } from "@opentelemetry/api";

// PRODUCER: capture the current trace context into the job
await emailQueue.add("welcome", {
  userId: user.id,
  _otel: (() => { const carrier = {}; propagation.inject(context.active(), carrier); return carrier; })(),
});

// CONSUMER: restore it, so the job's span becomes part of the original trace
new Worker("email", async (job) => {
  const parentContext = propagation.extract(context.active(), job.data._otel ?? {});
  const tracer = trace.getTracer("email-worker");

  return context.with(parentContext, () =>
    tracer.startActiveSpan("process email job", { kind: SpanKind.CONSUMER, attributes: { "messaging.system": "bullmq", "messaging.destination.name": "email", "job.id": job.id } }, async (span) => {
      try {
        await sendWelcomeEmail(job.data);
      } catch (err) {
        span.recordException(err);
        span.setStatus({ code: SpanStatusCode.ERROR, message: err.message });
        throw err;
      } finally {
        span.end();
      }
    })
  );
}, { connection });
```

For asynchronous work whose duration is unrelated to the request (a job that runs an hour later), many teams use a **span link** to the producing span rather than making it a child, so the original request's trace doesn't appear to take an hour.

For Kafka, OTel's KafkaJS instrumentation propagates through message headers automatically. Verify for your version.

---

## Manual instrumentation: your own spans

Auto-instrumentation covers libraries. It doesn't know about **your business operations**. Add spans around meaningful units of work.

```js
import { trace, SpanStatusCode } from "@opentelemetry/api";

const tracer = trace.getTracer("orders-service");           // name it after the module/library

export async function placeOrder(input) {
  return tracer.startActiveSpan("placeOrder", async (span) => {
    try {
      span.setAttribute("user.id", input.userId);            // IDs are fine on spans (unlike metric labels)
      span.setAttribute("order.item_count", input.items.length);

      const products = await loadProducts(input.items);       // auto-instrumented DB spans nest under this one
      span.addEvent("products loaded", { count: products.length });

      const total = calculateTotal(products, input);
      span.setAttribute("order.total_cents", total);

      const order = await saveOrder(input, total);
      span.setAttribute("order.id", order.id);
      return order;
    } catch (err) {
      span.recordException(err);                               // attaches the error (type, message, stack) as a span event
      span.setStatus({ code: SpanStatusCode.ERROR, message: err.message });
      throw err;
    } finally {
      span.end();                                              // ALWAYS end the span, or it's never exported
    }
  });
}
```

`startActiveSpan` makes the new span the **active** one, so any span created inside (including auto-instrumented DB/HTTP calls, through `AsyncLocalStorage`, the same mechanism as `02-correlation-id.md`) becomes its **child**. That's what builds the tree.

### A reusable helper to avoid boilerplate

```js
// src/telemetry/withSpan.js
import { trace, SpanStatusCode } from "@opentelemetry/api";

export function withSpan(tracer, name, fn, attributes = {}) {
  return tracer.startActiveSpan(name, { attributes }, async (span) => {
    try {
      return await fn(span);
    } catch (err) {
      span.recordException(err);
      span.setStatus({ code: SpanStatusCode.ERROR, message: err.message });
      throw err;
    } finally {
      span.end();
    }
  });
}

// usage
const order = await withSpan(tracer, "orders.place", async (span) => {
  span.setAttribute("order.item_count", items.length);
  return orderService.place(input);
}, { "user.id": userId });
```

### What deserves a span

| ✅ Instrument | ❌ Don't |
|---|---|
| Business operations (`placeOrder`, `generateReport`) | Every tiny function (spans have overhead, and clutter drowns signal) |
| Calls to things auto-instrumentation misses (an internal SDK, a legacy client) | Tight loops: one span per iteration of a 10,000-item loop |
| Expensive or important phases (parse, validate, compute, persist) | Pure, fast helpers (formatting, mapping) |
| Background job processing and scheduled tasks | Anything already covered by an auto-instrumented span |

### Span attributes and naming

- **Name spans by the operation, not the instance:** `GET /orders/:id`, `placeOrder`, `SELECT orders`, never `GET /orders/42` (the same low-cardinality principle as metric labels, since backends group by span name).
- **Put detail in attributes**, which *can* be high-cardinality (user IDs, order IDs). That's the key difference from metrics.
- Follow **semantic conventions** (`http.request.method`, `db.system`, `messaging.system`, `error.type`) where they exist, so backends and dashboards understand your data. Custom attributes get a namespace: `app.order.id`.
- **Never put secrets or sensitive personal data in attributes** (tokens, passwords, card numbers, full emails). Traces are stored and viewed widely, so the same rules as logs (`01-pino-and-structured-logging.md`). Be especially careful with `db.statement` if queries embed literal values, and with HTTP headers/bodies.

### Recording errors correctly

```js
span.recordException(err);                                       // adds an "exception" event with type/message/stacktrace
span.setStatus({ code: SpanStatusCode.ERROR, message: err.message });   // marks the span as failed (backends highlight it)
```

Both calls matter: `setStatus` makes the span show as an error; `recordException` supplies the details. Don't mark **expected** outcomes as errors: a `404` or validation failure returned to a client is normal; only mark spans that represent genuine failures (the OTel HTTP instrumentation marks server spans as errors for `5xx` and leaves `4xx` as unset).

---

## Linking logs and traces

The most useful practical step: **put `trace_id` and `span_id` into every log line.** In Grafana (`05-grafana.md`), you can then click from a slow trace to its logs, or from an error log straight to its trace.

### Automatic: the Pino instrumentation

```bash
npm install @opentelemetry/instrumentation-pino
```

`getNodeAutoInstrumentations()` includes the Pino instrumentation, which **injects `trace_id`, `span_id`, and `trace_flags` into every Pino log record** made inside an active span (no code changes). Just make sure it isn't disabled.

### Manual: through the `mixin` you already have

```js
import { trace } from "@opentelemetry/api";

export const logger = pino({
  mixin() {
    const ctx = requestContext.get();                              // 02-correlation-id.md
    const span = trace.getActiveSpan()?.spanContext();
    return {
      requestId: ctx?.requestId,
      ...(span && { trace_id: span.traceId, span_id: span.spanId }),
    };
  },
});
```

```json
{"level":"error","trace_id":"4bf92f3577b34da6a3ce929d0e0e4736","span_id":"00f067aa0ba902b7","requestId":"req_8f3a2c1d","err":{...},"msg":"Payment failed"}
```

### Return the trace ID to the caller

Like the request ID, expose the trace ID in error responses so support can find the trace directly:

```js
const spanContext = trace.getActiveSpan()?.spanContext();
res.status(500).json({ error: { code: "internal_error", requestId, traceId: spanContext?.traceId } });
```

Once tracing is on, many teams use the **trace ID as the correlation ID everywhere** and drop the separate request ID (`02-correlation-id.md`, "Correlation ID vs trace ID").

---

## Sampling: you can't keep everything

At scale, tracing every request is expensive in CPU, network, and storage. **Sampling** decides which traces to keep.

| Strategy | How | Pros | Cons |
|---|---|---|---|
| **Always on** | Keep 100% | Complete data | Costly at volume; fine for development and low-traffic services |
| **Head sampling** (probabilistic) | Decide at the *start* of the trace: keep 10% | Cheap and simple; consistent across services via the parent's decision | Might discard the one interesting trace (the slow or failing one) |
| **Tail sampling** | Collect everything, decide at the *end* in the **Collector**: keep all errors, all slow traces, and 5% of the rest | Keeps what matters | Needs a Collector and memory (it buffers whole traces); more infrastructure |

```bash
# head sampling: parent-based means "follow the caller's decision if there is one"; otherwise sample 10%
OTEL_TRACES_SAMPLER=parentbased_traceidratio
OTEL_TRACES_SAMPLER_ARG=0.1
```

`parentbased_*` samplers are important: if the upstream service decided to keep a trace, every downstream service keeps its part, so traces are **complete or absent**, never half-recorded.

A common production setup: **head sample a low percentage in the SDK for baseline volume, and use tail sampling in the Collector to additionally keep all errors and slow requests.** Example Collector tail-sampling policy:

```yaml
processors:
  tail_sampling:
    decision_wait: 10s
    policies:
      - { name: errors, type: status_code, status_code: { status_codes: [ERROR] } }
      - { name: slow, type: latency, latency: { threshold_ms: 1000 } }
      - { name: baseline, type: probabilistic, probabilistic: { sampling_percentage: 5 } }
```

---

## The OpenTelemetry Collector

You can export straight from the SDK to a backend, but a **Collector** between them is the recommended production setup:

| Benefit | Detail |
|---|---|
| **Decouples** apps from backends | Change or add a backend without redeploying services |
| **Offloads** work from your app | Batching, retries, compression, and sampling happen outside Node |
| **Central policy** | Redact attributes, drop noisy spans, add environment labels, tail sample |
| **Fan-out** | Send traces to Tempo *and* a vendor; metrics to Prometheus; logs to Loki |

```yaml
# otel-collector-config.yaml
receivers:
  otlp:
    protocols:
      grpc: { endpoint: 0.0.0.0:4317 }
      http: { endpoint: 0.0.0.0:4318 }

processors:
  memory_limiter: { check_interval: 1s, limit_mib: 512 }
  batch: {}                                          # batch spans before sending: far more efficient
  attributes/redact:
    actions:
      - { key: http.request.header.authorization, action: delete }
      - { key: db.statement, action: delete }        # if your queries could contain sensitive literals

exporters:
  otlp/tempo:
    endpoint: tempo:4317
    tls: { insecure: true }                          # inside a private network only
  debug: { verbosity: basic }                        # print to stdout while testing

service:
  pipelines:
    traces:
      receivers: [otlp]
      processors: [memory_limiter, attributes/redact, batch]
      exporters: [otlp/tempo]
```

Run it as a sidecar, a per-node agent (DaemonSet), or a central gateway service. `05-grafana.md` includes a complete local stack with the Collector, Tempo, Prometheus, Loki, and Grafana. Config keys and component names change between Collector versions, so check the docs for your version.

---

## Tracing backends

| Backend | Notes |
|---|---|
| **Jaeger** | Mature open-source UI and storage; easy to run locally (all-in-one image) |
| **Grafana Tempo** | Cheap object-storage-based traces, integrates tightly with Grafana, Loki, and Prometheus (`05-grafana.md`) |
| **Zipkin** | The older open-source option |
| **SigNoz, Uptrace, OpenObserve** | Open-source all-in-one platforms (traces + metrics + logs) |
| **Datadog, New Relic, Honeycomb, Dynatrace, Elastic APM, Splunk, Lightstep** | Commercial platforms with their own analysis features (Honeycomb is especially strong at exploratory querying) |
| **AWS X-Ray / Google Cloud Trace / Azure Monitor** | Cloud-native, with OTel exporters/collectors |

Because you instrument with OTel, switching between these is a Collector/exporter configuration change.

---

## Quick local try-out with Jaeger

```bash
docker run --rm -p 16686:16686 -p 4318:4318 jaegertracing/all-in-one:latest
```

(Recent Jaeger versions accept OTLP directly; check the image's docs for ports and flags.)

```bash
OTEL_SERVICE_NAME=orders-api node --import ./instrumentation.js src/server.js
curl localhost:3000/api/v1/orders/42
open http://localhost:16686        # pick "orders-api", find the trace, expand the spans
```

Seeing your first trace of a real request is usually the moment tracing "clicks."

---

## Performance and cost

- Auto-instrumentation adds a **small overhead** (typically low single-digit percent, more with very chatty spans). Measure it in your own service (`15-performance/`).
- **Sample.** Cost scales with span count × traffic.
- **Disable noisy instrumentations** (`fs`, `dns`, `net`) and **ignore health checks and metrics endpoints**.
- Use the **batch** span processor (the default with `NodeSDK`), never the simple/synchronous one in production.
- **Limit attribute sizes** and span counts: a loop that creates 10,000 spans per request is a bug.
- **Shut down cleanly** (`sdk.shutdown()`), or the last spans before a deploy are lost.
- Keep the **Collector** close to the app (same node or cluster) to reduce network cost and latency.

---

## Testing and debugging tracing

```js
// 1. Debug locally: print spans to the console
import { ConsoleSpanExporter, SimpleSpanProcessor } from "@opentelemetry/sdk-trace-base";
// in dev only: add a SimpleSpanProcessor(new ConsoleSpanExporter()) to see spans in your terminal

// 2. Unit-test your manual spans with the in-memory exporter
import { InMemorySpanExporter, SimpleSpanProcessor, BasicTracerProvider } from "@opentelemetry/sdk-trace-base";

const exporter = new InMemorySpanExporter();
const provider = new BasicTracerProvider({ spanProcessors: [new SimpleSpanProcessor(exporter)] });
trace.setGlobalTracerProvider(provider);

test("placeOrder records a span with the order id", async () => {
  const order = await placeOrder(input);
  const spans = exporter.getFinishedSpans();
  const span = spans.find((s) => s.name === "placeOrder");
  expect(span.attributes["order.id"]).toBe(order.id);
  expect(span.status.code).not.toBe(SpanStatusCode.ERROR);
});
```

Troubleshooting checklist when no traces appear:

1. **Is the SDK loaded before the app?** (`--import`/`--require` first.) The most common problem.
2. **ESM?** Add the loader hook (see above).
3. **Is the exporter reaching the Collector/backend?** Set `OTEL_LOG_LEVEL=debug`; check ports (`4317` gRPC, `4318` HTTP), URLs (`/v1/traces`), and network/DNS.
4. **Sampling too low?** Try `OTEL_TRACES_SAMPLER=always_on` while testing.
5. **Is the process exiting before spans flush?** Short scripts need `await sdk.shutdown()`.
6. **Are the libraries supported and imported after the SDK started?**
7. **Context lost?** If child spans appear as separate traces, context isn't propagating (see the `AsyncLocalStorage` notes in `02-correlation-id.md`).

---

## Common mistakes

```js
// ❌ starting the SDK after importing express/pg → no auto-instrumentation
import express from "express";
import "./instrumentation.js";                         // too late: must be loaded FIRST (--import)

// ❌ forgetting span.end() (the span is never exported and leaks)
// ❌ spans named with IDs or URLs ("GET /orders/42") instead of patterns
// ❌ secrets or personal data in attributes, or capturing full request/response bodies and headers
// ❌ span per loop iteration / per tiny function → huge overhead and unreadable traces
// ❌ marking expected 4xx outcomes as errors → error rates and alerts get noisy
// ❌ not recording the exception (only logging it) → the trace shows an error with no details
// ❌ tracing health checks and metrics scrapes (flood of useless traces)
// ❌ 100% sampling in production at scale, without budgeting for cost
// ❌ head-sampling away the errors: use tail sampling or a higher error rate
// ❌ no propagation through queues → background work appears as unrelated traces
// ❌ mixing propagation formats between services (W3C vs B3) without configuring propagators
// ❌ never calling sdk.shutdown() → lost spans on deploys
// ❌ treating traces as a substitute for logs and metrics instead of linking all three
```

## Checklist

- [ ] `instrumentation.js` loaded **first** (`--import`/`--require`; ESM loader hook if using ESM)
- [ ] `service.name`, version, and environment set as resource attributes
- [ ] Auto-instrumentation on; noisy modules (`fs`) off; health/metrics endpoints ignored
- [ ] Exporting OTLP to an OpenTelemetry Collector (batching, redaction, sampling there)
- [ ] Manual spans around key business operations; names are patterns, details are attributes; every span ended; errors recorded with status
- [ ] Context propagated across HTTP (automatic) and queues/brokers (manual inject/extract)
- [ ] `trace_id` and `span_id` in every log line; trace ID returned in error responses
- [ ] Sampling configured deliberately (parent-based head sampling; tail sampling for errors and slow traces at scale)
- [ ] No secrets or sensitive personal data in attributes; redaction processor in the Collector
- [ ] `sdk.shutdown()` on termination; overhead measured

## Next

**`05-grafana.md`** brings the three pillars together: dashboards for your metrics, Loki for logs, Tempo for traces, and the links between them, plus a complete local stack you can run with Docker Compose.
