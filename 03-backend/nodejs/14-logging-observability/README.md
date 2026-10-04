# Logging & Observability

How to see what your application is actually doing in production, so that when something breaks at 3 a.m. you can find out *what*, *where*, and *why* in minutes instead of hours.

## The problem: production is a black box

Tests (`13-testing/`) tell you the code works *before* release. Once it's running for real users, new questions appear that no test can answer:

- Why did checkout get slow **this afternoon**?
- Which of our 12 services is responsible for the `500`s a customer just reported?
- Is this error new, or has it been happening quietly for a week?
- Is the queue backing up? Is memory leaking? Is the database the bottleneck?
- Did last night's deploy make things better or worse?

Without instrumentation, you're left with `console.log` archaeology and guesses. With it, you answer these questions from a dashboard.

---

## Monitoring vs observability

| | Monitoring | Observability |
|---|---|---|
| Question | "Is it broken?" | "**Why** is it broken?" |
| Approach | Watch known indicators (CPU, error rate) and alert on thresholds | Collect rich data so you can explore **unknown** problems |
| Handles | Failures you predicted | Failures you didn't predict |
| Example | Alert: "error rate > 5%" | Drill from that alert to the one slow query in one service behind one customer's requests |

You need both. Monitoring wakes you up; observability lets you fix the problem once you're awake.

---

## The three pillars (and how they work together)

```
                        ┌─────────────────────────────────────────────┐
                        │              One request fails              │
                        └─────────────────────────────────────────────┘
                                          │
        ┌─────────────────────────────────┼─────────────────────────────────┐
        ▼                                 ▼                                 ▼
┌───────────────┐                ┌────────────────┐                ┌────────────────┐
│    METRICS    │                │     TRACES     │                │      LOGS      │
│ "What's the   │   zoom in ───▶ │ "Where did the │   zoom in ───▶ │ "What exactly  │
│  overall      │                │  time go?"     │                │  happened      │
│  picture?"    │                │                │                │  here?"        │
│               │                │ API → DB → 3rd │                │                │
│ error rate ↑  │                │ party (slow)   │                │ {"err":"timeout│
│ p99 latency ↑ │                │                │                │  calling X"}   │
└───────────────┘                └────────────────┘                └────────────────┘
   aggregated numbers              one request's journey              discrete events
   cheap, long retention           per-request detail                 most detail, most expensive
```

| Pillar | What it is | Best at | Weakness | In this section |
|---|---|---|---|---|
| **Logs** | Timestamped records of discrete events | Explaining *what happened* in one specific case | Expensive at volume; hard to see trends | `01`, `02` |
| **Metrics** | Numbers aggregated over time (counters, gauges, histograms) | Dashboards, alerting, trends, capacity planning | No per-request detail | `03` |
| **Traces** | The path of a single request across functions and services, with timings | Finding *where* latency or errors come from | Needs sampling at scale | `04` |

The pillars are most powerful when **linked**: an alert (metric) leads to a slow trace, which leads to the exact log lines for that request, and a shared **correlation ID / trace ID** is the thread that ties them together (`02`, `04`).

---

## What's in this section

| File | What you learn |
|---|---|
| `01-pino-and-structured-logging.md` | Why logs should be JSON, using Pino, levels, child loggers, redacting secrets, HTTP request logging, log shipping |
| `02-correlation-id.md` | Tracking one request across middleware, services, queues, and other systems with `AsyncLocalStorage` |
| `03-metrics-and-prometheus.md` | Counters, gauges, histograms with `prom-client`, the RED and USE methods, PromQL basics, alert rules |
| `04-tracing-and-opentelemetry.md` | Distributed tracing with OpenTelemetry: auto and manual spans, context propagation, sampling |
| `05-grafana.md` | Dashboards, data sources (Prometheus, Loki, Tempo), linking signals, alerting, and a local observability stack |

Read in order. Each builds on the previous: structured logs → a request ID that threads them → metrics for the big picture → traces for the journey → Grafana to see it all in one place.

---

## Prerequisites

- `02-core-modules/07-process.md` and `02-core-modules/04-events.md`: process signals, `stdout`, events
- `06-express/02-middleware.md`: logging, IDs, and timing live in middleware
- `09-api-development/05-error-responses.md`: the `requestId` in error bodies ties support tickets to logs
- `15-performance/` (next section) uses everything here to find real bottlenecks
- `16-production/` for deployment: containers, log collection, and health checks

---

## The architecture of a typical stack

```
   Your Node.js services
   ┌────────────────────────────────────────────────────────────────┐
   │  logs (JSON to stdout)   metrics (/metrics)   traces (OTLP)    │
   └───────┬────────────────────────┬──────────────────┬────────────┘
           │                        │                  │
           ▼                        ▼                  ▼
   ┌──────────────┐        ┌──────────────┐    ┌──────────────────┐
   │ Log collector│        │  Prometheus  │    │ OpenTelemetry    │
   │ (Promtail /  │        │  (scrapes    │    │ Collector        │
   │  Fluent Bit /│        │   /metrics)  │    │ (receives, batches│
   │  platform)   │        │              │    │  forwards)       │
   └──────┬───────┘        └──────┬───────┘    └────────┬─────────┘
          ▼                       ▼                     ▼
   ┌──────────────┐        ┌──────────────┐    ┌──────────────────┐
   │  Loki / ELK /│        │  Prometheus  │    │ Tempo / Jaeger   │
   │  CloudWatch  │        │  TSDB        │    │                  │
   └──────┬───────┘        └──────┬───────┘    └────────┬─────────┘
          └──────────────┬────────┴──────────────────────┘
                         ▼
                 ┌──────────────┐        ┌───────────────┐
                 │   Grafana    │ ─────▶ │ Alerts → Slack│
                 │ (dashboards) │        │ / PagerDuty   │
                 └──────────────┘        └───────────────┘
```

Many teams replace the self-hosted boxes with a vendor (Datadog, New Relic, Honeycomb, Grafana Cloud, Elastic Cloud, AWS CloudWatch/X-Ray). The concepts and your instrumentation code are the same either way, and **OpenTelemetry** is the vendor-neutral standard that keeps you from being locked in.

---

## Where to start (in priority order)

You don't need everything on day one. A sensible order for a new service:

1. **Structured JSON logs to stdout**, with a request ID on every line (`01`, `02`). Enormous value for little effort.
2. **A health endpoint and basic alerts:** is it up? (`16-production/02-graceful-shutdown-and-health-checks.md`)
3. **RED metrics for HTTP:** request **R**ate, **E**rrors, **D**uration, plus Node runtime metrics (`03`).
4. **Alerts on symptoms users feel:** error rate, latency (`03`, `05`).
5. **Tracing** once you have more than one service or a mysterious latency problem (`04`).
6. **Business metrics:** signups, orders, payments, queue depth: the numbers that tell you the *product* works, not just the servers.

---

## Principles

1. **Instrument before you need it.** You can't add the log line after the incident.
2. **Structure beats prose.** Machines (and you at 3 a.m., with a query language) read JSON better than sentences.
3. **Log events, not noise.** Every line should help answer a question; volume costs money and hides signal.
4. **One request, one ID** across every log, trace, job, and service it touches.
5. **Alert on symptoms, not causes.** "Checkout error rate is 8%" is actionable; "CPU is 80%" often isn't.
6. **Mind cardinality.** Labels like `userId` on metrics can bring down Prometheus. Put high-cardinality detail in logs and traces instead.
7. **Never log secrets or unnecessary personal data** (passwords, tokens, card numbers, full request bodies). Telemetry is retained widely and read by many people.
8. **Observability is a feature with a cost** (CPU, network, storage, money). Sample, set retention, and aggregate deliberately.
9. **Make it easy to find the data.** Consistent field names (`requestId`, `userId`, `orderId`) across all services matter more than any tool choice.
10. **Practice using it.** Dashboards nobody has looked at before an incident are hard to read during one.

## Next

**`01-pino-and-structured-logging.md`** starts with the foundation: why logs should be structured JSON, and how to set up Pino so every log line is fast, safe, and searchable.
