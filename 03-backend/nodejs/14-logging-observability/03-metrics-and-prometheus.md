# Metrics & Prometheus

Numbers aggregated over time — request rates, error rates, latency percentiles, queue depth — exposed from Node.js with `prom-client`, collected by Prometheus, and turned into dashboards and alerts.

## Why metrics

Logs answer *"what happened in this one case?"* Metrics answer *"how is the system doing overall, and is it getting better or worse?"*

| | Logs | Metrics |
|---|---|---|
| Data | One record per event | Numbers aggregated over time |
| Cost at scale | Grows with traffic (every request = a line) | **Constant** (a fixed set of series, however many requests) |
| Good for | Debugging a specific case | Dashboards, trends, alerting, capacity planning |
| Retention | Days to weeks (expensive) | Months to years (cheap) |
| Weakness | Hard to compute rates and percentiles | No per-request detail |

A metric is a **time series**: a name, a set of labels, and a stream of `(timestamp, value)` samples.

```
http_requests_total{method="GET", route="/orders/:id", status="200"}  →  1027 @10:15:00, 1049 @10:15:15, ...
```

---

## How Prometheus works

Prometheus **pulls** (scrapes) metrics from your services over HTTP, on an interval, and stores them in a time-series database.

```
   Node app (instance 1)  ──┐
   Node app (instance 2)  ──┼──◀── scrape GET /metrics every 15s ──── Prometheus ───▶ query (PromQL)
   Postgres exporter      ──┤                                           │
   Redis exporter         ──┘                                           ├──▶ Grafana dashboards (05)
                                                                         └──▶ Alertmanager ──▶ Slack / PagerDuty
```

Why pull instead of push?

- Prometheus knows **whether a target is up** (a failed scrape *is* a signal).
- Your app just exposes a page, with no client configuration, no backpressure problems.
- Easy to test: `curl localhost:3000/metrics`.

(Short-lived jobs that exit before a scrape can **push** to a *Pushgateway*; see below.)

The scrape format is plain text:

```
# HELP http_requests_total Total HTTP requests
# TYPE http_requests_total counter
http_requests_total{method="GET",route="/orders/:id",status="200"} 1027
http_requests_total{method="GET",route="/orders/:id",status="500"} 3
```

---

## The four metric types

| Type | What it is | Goes up/down? | Examples |
|---|---|---|---|
| **Counter** | A cumulative count that only increases (resets on restart) | Up only | Requests, errors, jobs processed, bytes sent |
| **Gauge** | A current value | Up and down | In-flight requests, queue depth, memory, active connections, temperature |
| **Histogram** | Counts observations in **buckets** (plus a sum and count) | n/a | Request duration, response size |
| **Summary** | Pre-computed quantiles on the client | n/a | Rarely preferred (can't be aggregated across instances) |

**Use a Histogram, not a Summary,** for latency: histograms can be **aggregated across all your instances** (so you can compute a fleet-wide p99); summaries' quantiles can't.

---

## Setting up `prom-client`

```bash
npm install prom-client
```

```js
// src/metrics/registry.js
import client from "prom-client";

export const register = new client.Registry();

register.setDefaultLabels({ service: "orders-api", env: process.env.NODE_ENV });   // added to every metric

// Node.js runtime metrics for free: event loop lag, heap, GC, CPU, open handles, ...
client.collectDefaultMetrics({ register });

export { client };
```

```js
// src/routes/metrics.js
import { register } from "../metrics/registry.js";

export async function metricsHandler(req, res) {
  res.set("Content-Type", register.contentType);
  res.end(await register.metrics());               // async in current prom-client versions
}
```

```js
// app.js
app.get("/metrics", metricsHandler);               // PROTECT this: see "Securing /metrics" below
```

Check it:

```bash
curl localhost:3000/metrics | head -30
```

You immediately get Node runtime metrics, including:

| Metric | Why it matters |
|---|---|
| `nodejs_eventloop_lag_seconds` (and `_p99_`, etc.) | **The key Node health signal:** a blocked event loop delays every request (`15-performance/01-event-loop-performance.md`) |
| `nodejs_heap_size_used_bytes`, `process_resident_memory_bytes` | Memory growth and leaks (`15-performance/02-profiling-and-memory-leaks.md`) |
| `nodejs_gc_duration_seconds` | Garbage collection pauses |
| `process_cpu_seconds_total` | CPU usage |
| `nodejs_active_handles_total` / `nodejs_active_requests_total` | Resource leaks |
| `process_open_fds` | Approaching file descriptor limits |

---

## Defining your own metrics

### Counter

```js
export const ordersPlaced = new client.Counter({
  name: "orders_placed_total",                    // convention: a counter name ends in _total
  help: "Total orders successfully placed",
  labelNames: ["payment_method"],
  registers: [register],
});

ordersPlaced.inc({ payment_method: "card" });     // +1
ordersPlaced.inc({ payment_method: "card" }, 3);  // +3
```

### Gauge

```js
export const queueDepth = new client.Gauge({
  name: "queue_jobs_waiting",
  help: "Jobs waiting in a queue",
  labelNames: ["queue"],
  registers: [register],
  // collect() runs at scrape time: ideal for "current value" metrics, no timers needed
  async collect() {
    for (const q of [emailQueue, reportsQueue]) {
      this.set({ queue: q.name }, await q.getWaitingCount());
    }
  },
});

// or manually
activeConnections.inc();       // on connect
activeConnections.dec();       // on disconnect
activeConnections.set(42);
```

### Histogram

```js
export const httpDuration = new client.Histogram({
  name: "http_request_duration_seconds",          // base unit: seconds (not milliseconds)
  help: "HTTP request duration in seconds",
  labelNames: ["method", "route", "status_class"],
  buckets: [0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10],   // choose around your SLO
  registers: [register],
});

const end = httpDuration.startTimer({ method: "GET", route: "/orders/:id" });
// ... work ...
end({ status_class: "2xx" });                     // observes the elapsed seconds, adds the final labels

httpDuration.observe({ method: "GET", route: "/x", status_class: "2xx" }, 0.123);
```

**Buckets** are cumulative upper bounds ("≤ 0.1 s", "≤ 0.25 s", ...). Prometheus estimates percentiles by interpolating *within* the buckets, so pick boundaries that bracket the numbers you care about. If your SLO is "95% under 300 ms," include buckets at `0.25` and `0.5`, or the p95 estimate will be coarse. The default buckets suit general web services but not everything.

### Naming conventions (follow them: they make queries predictable)

| Rule | Example |
|---|---|
| `snake_case`, with an application/domain prefix | `orders_placed_total` |
| **Base units**: seconds, bytes, ratios (0–1) | `http_request_duration_seconds`, `response_size_bytes` |
| Counters end with `_total` | `jobs_failed_total` |
| Unit as a suffix | `_seconds`, `_bytes`, `_ratio` |
| A name describes *what is measured*, never the label values | `http_requests_total`, not `http_requests_get_total` |

---

## Cardinality: the one thing that can kill Prometheus

**Every unique combination of label values is a separate time series**, stored and indexed in memory.

```
http_requests_total{route, method, status}
   50 routes × 5 methods × 10 statuses = 2,500 series      ✅ fine

http_requests_total{route, method, status, userId}
   × 1,000,000 users = 2.5 BILLION series                    💥 out-of-memory, crashed Prometheus
```

Rules:

| Rule | Why |
|---|---|
| **Labels must have a small, bounded set of values** | Series count = product of the label cardinalities |
| **Never** use as labels: user IDs, emails, order IDs, request IDs, session IDs, timestamps, raw URLs, error messages, IP addresses | Unbounded → cardinality explosion |
| Use the **route pattern**, not the actual URL | `/orders/:id`, not `/orders/42`, `/orders/43`, ... |
| Bucket numbers and statuses into classes where it helps | `status_class="5xx"` instead of every status code |
| Put high-cardinality detail in **logs and traces**, not metrics | That's what they're for |

### Getting the route label right in Express

```js
// ❌ req.path or req.url: "/orders/42", "/orders/43", ... → unbounded
// ✅ the matched route pattern, available AFTER routing, when the response finishes
const route = req.route?.path ? `${req.baseUrl}${req.route.path}` : "unmatched";
```

Use `"unmatched"` (not the raw URL) for 404s, otherwise a scanner hitting `/wp-admin/xyz-<random>` creates a new series per request.

---

## HTTP middleware: the RED metrics

```js
// src/middleware/httpMetrics.js
import { httpDuration, httpRequests, httpInFlight } from "../metrics/http.js";

export function httpMetrics(req, res, next) {
  if (req.path === "/metrics" || req.path === "/health") return next();      // don't measure the monitoring itself

  const start = process.hrtime.bigint();
  httpInFlight.inc();

  res.on("finish", () => {                                                   // 'finish' fires when the response has been sent
    const seconds = Number(process.hrtime.bigint() - start) / 1e9;
    const route = req.route?.path ? `${req.baseUrl}${req.route.path}` : "unmatched";
    const labels = {
      method: req.method,
      route,
      status_class: `${Math.floor(res.statusCode / 100)}xx`,
    };

    httpRequests.inc({ ...labels, status: String(res.statusCode) });
    httpDuration.observe(labels, seconds);
    httpInFlight.dec();
  });

  res.on("close", () => {                                                    // client aborted before 'finish'
    if (!res.writableFinished) httpInFlight.dec();
  });

  next();
}
```

```js
// src/metrics/http.js
export const httpRequests = new client.Counter({
  name: "http_requests_total", help: "HTTP requests", labelNames: ["method", "route", "status_class", "status"], registers: [register],
});
export const httpDuration = new client.Histogram({
  name: "http_request_duration_seconds", help: "HTTP request duration", labelNames: ["method", "route", "status_class"],
  buckets: [0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10], registers: [register],
});
export const httpInFlight = new client.Gauge({
  name: "http_requests_in_flight", help: "Requests currently being handled", registers: [register],
});
```

```js
app.use(httpMetrics);          // early, so it times the whole middleware chain
```

(Packages like `express-prom-bundle` do this for you; writing it once yourself shows what's happening and avoids surprises with route labels.)

---

## What to measure: RED, USE, and the golden signals

Don't measure everything. Measure what tells you whether users are happy and where the system is strained.

### RED: for request-driven services (your APIs)

| | Meaning | Metric |
|---|---|---|
| **R**ate | Requests per second | `rate(http_requests_total[5m])` |
| **E**rrors | Failed requests per second (or ratio) | requests with `status_class="5xx"` |
| **D**uration | How long requests take (distribution) | `http_request_duration_seconds` histogram |

### USE: for resources (CPU, memory, disk, connection pools, queues)

| | Meaning | Examples |
|---|---|---|
| **U**tilization | % of time/capacity in use | CPU %, pool connections in use ÷ max |
| **S**aturation | Work waiting because the resource is full | Event loop lag, queue depth, pool waiters |
| **E**rrors | Resource errors | Disk errors, connection failures, timeouts |

### Google's four golden signals

**Latency, Traffic, Errors, Saturation**: RED plus saturation. If you can only monitor four things, monitor these.

### Instrument these in a Node.js service

| Area | Metrics |
|---|---|
| **HTTP** | request count, errors, duration histogram, in-flight (above) |
| **Event loop & runtime** | default metrics: event loop lag, heap, GC, CPU, open handles |
| **Database** | pool size / in-use / waiting; query duration histogram; errors |
| **Cache** | hit/miss counters (`cache_requests_total{result="hit"}`), latency |
| **External calls** | duration and error counters **per dependency** (`outbound_requests_total{target="stripe",status_class}`) |
| **Queues & jobs** | waiting/active/failed/dead-letter counts, job duration, retries (`11-async-processing/`) |
| **WebSockets** | connected clients, connects/disconnects with reason, events in/out (`12-realtime/03-scaling-with-redis-adapter.md`) |
| **Business** | signups, orders placed, payments captured/failed, revenue. These catch the failures technical metrics miss |

Examples:

```js
// database pool (pg)
new client.Gauge({
  name: "db_pool_connections", help: "pg pool connections", labelNames: ["state"], registers: [register],
  collect() {
    this.set({ state: "total" }, pool.totalCount);
    this.set({ state: "idle" }, pool.idleCount);
    this.set({ state: "waiting" }, pool.waitingCount);     // > 0 for long = pool saturation
  },
});

// outbound dependency calls
export async function trackedFetch(target, url, options) {
  const end = outboundDuration.startTimer({ target });
  try {
    const res = await fetch(url, options);
    outboundRequests.inc({ target, status_class: `${Math.floor(res.status / 100)}xx` });
    return res;
  } catch (err) {
    outboundRequests.inc({ target, status_class: "error" });          // network failure / timeout
    throw err;
  } finally {
    end();
  }
}

// business metric: the failure that CPU graphs will never show
paymentsTotal.inc({ outcome: "failed", reason: err.code ?? "unknown" });   // reason: a SMALL fixed set of codes, never free text
```

---

## Securing `/metrics`

Metrics reveal your architecture, routes, versions, and traffic. **Don't expose `/metrics` to the internet.**

Options:

1. **Separate port**, bound to an internal interface, serving *only* metrics:

```js
import http from "node:http";

http.createServer(async (req, res) => {
  if (req.url === "/metrics") { res.setHeader("Content-Type", register.contentType); return res.end(await register.metrics()); }
  res.writeHead(404).end();
}).listen(9464, "0.0.0.0");                      // not routed by the public load balancer
```

2. **Network policy / security group:** only Prometheus can reach it.
3. **Block it at the reverse proxy** (`16-production/04-nginx.md`): `location /metrics { deny all; }` publicly, allow the internal scraper.
4. **Authentication** (a bearer token configured in Prometheus' scrape config) as an extra layer.

Also keep `/metrics` and `/health` **out of your access logs and your own metrics** so they don't distort the data.

---

## Scaling concerns

### Multiple instances and processes

Each process has its own in-memory metrics. Prometheus scrapes **each instance separately** and aggregates in queries. This is the standard model and works with containers/Kubernetes through service discovery.

With Node's `cluster` module (several worker processes behind one port), a scrape hits a random worker, which is wrong. Use `prom-client`'s `AggregatorRegistry` to aggregate across workers in the primary process, or prefer running one process per container.

### Short-lived jobs and scripts

A batch job that finishes before Prometheus scrapes it leaves no trace. Use the **Pushgateway**:

```js
const gateway = new client.Pushgateway("http://pushgateway:9091", {}, register);
await gateway.pushAdd({ jobName: "nightly-report" });
```

Use it sparingly (only for batch jobs): it holds the last pushed value forever unless you delete it, which hides crashes. Also expose `last_success_timestamp_seconds` so you can alert on staleness (`11-async-processing/03-scheduled-jobs.md`):

```js
lastSuccess.set({ job: "nightly-report" }, Date.now() / 1000);
```

### Counters reset on restart

That's expected: PromQL's `rate()`/`increase()` handle resets. Never graph the raw counter value; always use `rate()` or `increase()`.

---

## Prometheus configuration

```yaml
# prometheus.yml
global:
  scrape_interval: 15s
  evaluation_interval: 15s

rule_files:
  - /etc/prometheus/alerts.yml

alerting:
  alertmanagers:
    - static_configs:
        - targets: ["alertmanager:9093"]

scrape_configs:
  - job_name: orders-api
    metrics_path: /metrics
    static_configs:
      - targets: ["orders-api-1:3000", "orders-api-2:3000"]       # static list for simple setups

  # Kubernetes: discover pods automatically via annotations
  # - job_name: kubernetes-pods
  #   kubernetes_sd_configs: [{ role: pod }]
  #   relabel_configs: [...]
```

In Kubernetes, use the **Prometheus Operator** (`ServiceMonitor` / `PodMonitor` resources), and managed offerings (Amazon Managed Prometheus, Grafana Cloud, Google Managed Prometheus) handle storage for you. Prometheus automatically adds an `up{job, instance}` metric (1 = scrape succeeded, 0 = failed) for every target: **alert on it**.

---

## PromQL: the query language

You write PromQL in Grafana panels and alert rules. The essentials:

### Selecting and filtering

```promql
http_requests_total                                           # all series
http_requests_total{route="/orders/:id"}                       # label equals
http_requests_total{status_class=~"4xx|5xx"}                   # regex match
http_requests_total{route!="unmatched"}                        # not equals
```

### Rates (counters → per-second values)

```promql
rate(http_requests_total[5m])                                  # per-second rate, averaged over 5 minutes, PER series
sum(rate(http_requests_total[5m]))                             # total requests/second across everything
sum by (route) (rate(http_requests_total[5m]))                 # requests/second per route
increase(orders_placed_total[1h])                              # how many orders in the last hour
```

### Error ratio

```promql
sum(rate(http_requests_total{status_class="5xx"}[5m]))
  /
sum(rate(http_requests_total[5m]))                             # fraction of requests that are 5xx (0.02 = 2%)
```

### Latency percentiles from histograms

```promql
histogram_quantile(0.95, sum by (le) (rate(http_request_duration_seconds_bucket[5m])))               # fleet-wide p95
histogram_quantile(0.99, sum by (le, route) (rate(http_request_duration_seconds_bucket[5m])))        # p99 per route
```

Notes: always `sum by (le, ...)` before `histogram_quantile`; this is why histograms aggregate across instances. Percentiles are *estimates* limited by bucket boundaries. **Don't average percentiles** across instances (averaging p95s is meaningless); aggregate the buckets instead, as above.

### Averages

```promql
sum(rate(http_request_duration_seconds_sum[5m])) / sum(rate(http_request_duration_seconds_count[5m]))   # mean latency
```

Averages hide tail latency: **look at p95/p99, not the mean.** One slow request in 100 ruins that user's experience while the mean looks fine.

### Gauges

```promql
nodejs_eventloop_lag_p99_seconds                               # the current value
max_over_time(queue_jobs_waiting[10m])                         # peak over a window
avg_over_time(process_resident_memory_bytes[1h])
deriv(process_resident_memory_bytes[30m])                      # growth trend (a leak shows up as steady positive slope)
```

### Useful operators

```promql
sum by (instance) (...)          # aggregate, keep a label
topk(5, sum by (route) (rate(http_requests_total[5m])))        # the 5 busiest routes
up == 0                          # targets that are down
a / on(instance) b               # join two metrics by a label
```

---

## Alerting

### What to alert on

**Alert on symptoms users feel; investigate causes via dashboards.**

| ✅ Alert (page a human) | ❌ Don't page for |
|---|---|
| Error ratio above N% for M minutes | CPU at 80% (if latency is fine, so what?) |
| p95/p99 latency above the SLO | A single failed request |
| Service down (`up == 0`) / no traffic when there should be | One pod restarting once |
| Queue oldest-job age above the SLA; DLQ non-empty | Brief spikes that self-heal |
| Scheduled job hasn't succeeded in X hours | Disk 60% full (ticket it; page at 90% or on a *predicted fill*) |
| Payment/signup success rate drops (business symptom) | Anything nobody would act on at 3 a.m. |

Every page should be **actionable, urgent, and real.** Anything else trains people to ignore alerts. Send non-urgent warnings to a ticket queue or a chat channel instead.

### Alert rules

```yaml
# alerts.yml
groups:
  - name: orders-api
    rules:
      - alert: HighErrorRate
        expr: |
          sum(rate(http_requests_total{service="orders-api", status_class="5xx"}[5m]))
            /
          sum(rate(http_requests_total{service="orders-api"}[5m])) > 0.05
        for: 5m                                    # must stay true for 5 minutes: avoids flapping on blips
        labels: { severity: page }
        annotations:
          summary: "5xx rate above 5% for 5 minutes"
          description: "Error ratio is {{ $value | humanizePercentage }}"
          runbook_url: "https://wiki.example.com/runbooks/orders-api-errors"

      - alert: SlowRequests
        expr: |
          histogram_quantile(0.95, sum by (le) (rate(http_request_duration_seconds_bucket{service="orders-api"}[5m]))) > 0.5
        for: 10m
        labels: { severity: page }
        annotations: { summary: "p95 latency above 500 ms for 10 minutes" }

      - alert: InstanceDown
        expr: up{job="orders-api"} == 0
        for: 2m
        labels: { severity: page }
        annotations: { summary: "{{ $labels.instance }} is not responding to scrapes" }

      - alert: EventLoopLagHigh
        expr: nodejs_eventloop_lag_p99_seconds > 0.1
        for: 5m
        labels: { severity: warn }
        annotations: { summary: "Event loop p99 lag above 100 ms: something is blocking Node" }

      - alert: QueueBacklog
        expr: queue_jobs_waiting{queue="email"} > 5000
        for: 15m
        labels: { severity: warn }
        annotations: { summary: "Email queue backlog is growing" }

      - alert: NightlyJobStale
        expr: time() - scheduled_job_last_success_timestamp_seconds{job="nightly-report"} > 26 * 3600
        labels: { severity: page }
        annotations: { summary: "Nightly report hasn't succeeded in 26 hours" }
```

Alert design rules:

- **`for:`** prevents paging on a 30-second spike.
- Include **`runbook_url`** and a `summary` that says what's wrong and how bad.
- Use **severity labels** so Alertmanager routes pages vs. chat messages.
- **Alert on the absence of data** too (`absent(...)`, or `up == 0`): a dead service emits no errors.
- For fast-changing traffic, prefer **ratios** over absolute counts.

### Alertmanager

Prometheus evaluates the rules; **Alertmanager** deduplicates, groups, silences, and routes them (Slack, PagerDuty, email, Opsgenie) based on labels. Configure routes by `severity`/`team`, grouping to prevent alert storms, and inhibition rules (don't page about slow requests when the whole service is already down).

### SLOs and burn rate (a short taste)

An **SLI** is a measurable indicator ("fraction of requests that succeed in under 500 ms"); an **SLO** is the target ("99.5% over 30 days"); the **error budget** is the allowed failures (0.5%). Mature teams alert on **budget burn rate**: "at the current error rate, we'll exhaust the 30-day budget in 2 days," which catches both fast catastrophes and slow bleeds while ignoring harmless blips. Look into multi-window multi-burn-rate alerts (from Google's SRE workbook) once the basics work.

---

## OpenTelemetry and metrics

`prom-client` is simple and ubiquitous. OpenTelemetry also defines a metrics API/SDK that can export to Prometheus or any OTLP backend, and fits naturally if you're already using OTel for traces (`04-tracing-and-opentelemetry.md`). For a Node.js service that only needs Prometheus metrics, `prom-client` is perfectly fine and the most common choice.

---

## Testing metrics

```js
import { register } from "../src/metrics/registry.js";

beforeEach(() => register.resetMetrics());           // zero the counters between tests

test("counts requests by route pattern, not by URL", async () => {
  await request(app).get("/api/v1/orders/42").set(authHeader(user));
  await request(app).get("/api/v1/orders/43").set(authHeader(user));

  const metrics = await register.metrics();
  expect(metrics).toContain('route="/api/v1/orders/:id"');
  expect(metrics).not.toMatch(/route="\/api\/v1\/orders\/4[23]"/);        // no per-ID series
});

test("business counter increments when an order is placed", async () => {
  await placeOrder();
  const value = (await ordersPlaced.get()).values[0].value;
  expect(value).toBe(1);
});
```

Test that labels stay **low-cardinality**: it's the metrics bug that only hurts in production.

---

## Common mistakes

```js
// ❌ unbounded labels: userId, orderId, raw URL, error message → cardinality explosion
httpRequests.inc({ path: req.url, userId: req.user.id });

// ❌ using req.path/req.url instead of the route pattern
// ❌ latency as Summary or as a plain gauge/average → can't aggregate; hides the tail
// ❌ buckets that don't bracket your SLO → useless percentile estimates
// ❌ graphing raw counters instead of rate()/increase()
// ❌ averaging percentiles across instances
// ❌ units in milliseconds (the convention is seconds, so queries and dashboards line up)
// ❌ alerting on causes (CPU, memory) instead of symptoms (errors, latency)
// ❌ alerts without `for:`, severity, or a runbook → noisy, ignored pages
// ❌ exposing /metrics publicly
// ❌ measuring /metrics and /health requests (and logging them), skewing the data
// ❌ creating metrics inside request handlers (new Counter(...) per call) → duplicate registration errors / leaks
// ❌ using cluster mode without aggregation → each scrape sees a random worker
// ❌ pushing everything to a Pushgateway instead of exposing /metrics
// ❌ no business metrics: the app can be "healthy" and silently failing to take orders
```

## Checklist

- [ ] `prom-client` with a shared registry, default metrics enabled, default labels (`service`, `env`)
- [ ] `/metrics` served on an internal-only port/route, or protected; excluded from logs and its own metrics
- [ ] RED metrics for HTTP: request counter, duration **histogram** (seconds, sensible buckets), in-flight gauge
- [ ] Route **pattern** (never the raw URL) in labels; `"unmatched"` for 404s
- [ ] Dependency metrics: DB pool, cache hit/miss, each external call's rate/errors/duration
- [ ] Queue/job metrics and scheduled-job last-success timestamps
- [ ] At least a few **business metrics** (orders, signups, payments)
- [ ] Label values bounded and reviewed (no IDs, emails, free text)
- [ ] Prometheus scraping every instance; alerts on `up == 0`
- [ ] Alert rules on symptoms (error ratio, p95/p99, queue age, stale jobs) with `for:`, severity, and runbooks
- [ ] Alertmanager routing pages vs. chat; every alert actionable

## Next

**`04-tracing-and-opentelemetry.md`** adds the third pillar. Metrics told you *that* p99 latency is up; a distributed trace shows *which* downstream call, query, or service is responsible, for a single request, across every service it touched.
