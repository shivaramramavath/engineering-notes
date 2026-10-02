# Grafana

One place to see metrics, logs, and traces together — dashboards for the big picture, drill-down links to the details, and alerts that tell you when to look.

## What Grafana is (and isn't)

Grafana is a **visualization and alerting layer**. It doesn't store your telemetry; it **queries** other systems (called *data sources*) and draws the results.

```
                              ┌──────────────┐
   metrics (PromQL) ◀──────── │              │ ────────▶ dashboards
   Prometheus                 │              │
                              │   Grafana    │ ────────▶ Explore (ad-hoc queries)
   logs (LogQL)     ◀──────── │              │
   Loki                       │              │ ────────▶ alerts → Slack / PagerDuty / email
                              │              │
   traces (TraceQL) ◀──────── │              │ ────────▶ links between all three
   Tempo                      └──────────────┘
```

| Data source | Holds | Query language | Covered in |
|---|---|---|---|
| **Prometheus** | Metrics | PromQL | `03-metrics-and-prometheus.md` |
| **Loki** | Logs | LogQL | `01-pino-and-structured-logging.md` |
| **Tempo** | Traces | TraceQL | `04-tracing-and-opentelemetry.md` |

Grafana also reads from PostgreSQL, MySQL, Elasticsearch, CloudWatch, InfluxDB, and dozens more, so one dashboard can combine *"orders per minute"* (Prometheus), *"revenue today"* (PostgreSQL), and *"error logs"* (Loki). The same stack's pieces are commonly called the **LGTM** stack (Loki, Grafana, Tempo, Mimir for long-term metrics storage); Grafana Cloud offers it hosted.

---

## A complete local stack with Docker Compose

Run the whole observability pipeline on your laptop and see all three signals from your own Node.js app. (Image tags and configuration keys evolve; pin versions you've tested and check each project's docs when something doesn't start.)

```
observability/
├── docker-compose.yml
├── prometheus/
│   └── prometheus.yml
├── loki/
│   └── loki-config.yml
├── promtail/
│   └── promtail-config.yml
├── tempo/
│   └── tempo.yml
├── otel-collector/
│   └── config.yml
└── grafana/
    └── provisioning/
        ├── datasources/datasources.yml
        └── dashboards/
            ├── dashboards.yml
            └── orders-api.json
```

```yaml
# docker-compose.yml
services:
  prometheus:
    image: prom/prometheus:latest
    volumes:
      - ./prometheus/prometheus.yml:/etc/prometheus/prometheus.yml:ro
    ports: ["9090:9090"]
    extra_hosts: ["host.docker.internal:host-gateway"]     # lets Prometheus reach an app running on your host

  loki:
    image: grafana/loki:latest
    command: -config.file=/etc/loki/loki-config.yml
    volumes: ["./loki/loki-config.yml:/etc/loki/loki-config.yml:ro"]
    ports: ["3100:3100"]

  promtail:                                                  # ships container logs to Loki (see the Alloy note below)
    image: grafana/promtail:latest
    command: -config.file=/etc/promtail/promtail-config.yml
    volumes:
      - ./promtail/promtail-config.yml:/etc/promtail/promtail-config.yml:ro
      - /var/run/docker.sock:/var/run/docker.sock:ro
    depends_on: [loki]

  tempo:
    image: grafana/tempo:latest
    command: -config.file=/etc/tempo/tempo.yml
    volumes: ["./tempo/tempo.yml:/etc/tempo/tempo.yml:ro"]
    ports: ["3200:3200"]

  otel-collector:
    image: otel/opentelemetry-collector-contrib:latest
    command: ["--config=/etc/otelcol/config.yml"]
    volumes: ["./otel-collector/config.yml:/etc/otelcol/config.yml:ro"]
    ports: ["4317:4317", "4318:4318"]                        # OTLP gRPC and HTTP: your app sends here
    depends_on: [tempo]

  grafana:
    image: grafana/grafana:latest
    environment:
      GF_AUTH_ANONYMOUS_ENABLED: "true"                       # LOCAL DEVELOPMENT ONLY
      GF_AUTH_ANONYMOUS_ORG_ROLE: Admin
    volumes:
      - ./grafana/provisioning:/etc/grafana/provisioning:ro
    ports: ["3001:3000"]                                      # 3001 so it doesn't collide with your app on 3000
    depends_on: [prometheus, loki, tempo]
```

```yaml
# prometheus/prometheus.yml
global:
  scrape_interval: 15s
scrape_configs:
  - job_name: orders-api
    static_configs:
      - targets: ["host.docker.internal:3000"]               # the Node app running on the host (or its compose service name)
```

```yaml
# loki/loki-config.yml: minimal single-process setup for local development
auth_enabled: false
server:
  http_listen_port: 3100
common:
  path_prefix: /tmp/loki
  replication_factor: 1
  ring:
    kvstore: { store: inmemory }
  storage:
    filesystem:
      chunks_directory: /tmp/loki/chunks
      rules_directory: /tmp/loki/rules
schema_config:
  configs:
    - from: 2024-01-01
      store: tsdb
      object_store: filesystem
      schema: v13
      index: { prefix: index_, period: 24h }
```

```yaml
# promtail/promtail-config.yml: scrape Docker container logs and parse the JSON
server: { http_listen_port: 9080 }
positions: { filename: /tmp/positions.yaml }
clients:
  - url: http://loki:3100/loki/api/v1/push
scrape_configs:
  - job_name: docker
    docker_sd_configs:
      - host: unix:///var/run/docker.sock
        refresh_interval: 5s
    relabel_configs:
      - source_labels: ["__meta_docker_container_name"]
        regex: "/(.*)"
        target_label: container
    pipeline_stages:
      - json:
          expressions: { level: level, service: service }     # pull a FEW low-cardinality fields out as labels
      - labels: { level: , service: }
```

> **Promtail is being superseded by Grafana Alloy**, Grafana's OpenTelemetry-compatible collector/agent. Promtail still works; for new production setups, look at Alloy (or the OpenTelemetry Collector's Loki exporter). The idea is the same: an agent reads container stdout and pushes it to Loki.

```yaml
# tempo/tempo.yml: minimal local trace storage
server: { http_listen_port: 3200 }
distributor:
  receivers:
    otlp:
      protocols:
        grpc: { endpoint: 0.0.0.0:4317 }                      # Tempo's own OTLP port (inside its container)
storage:
  trace:
    backend: local
    local: { path: /var/tempo/blocks }
    wal: { path: /var/tempo/wal }
```

```yaml
# otel-collector/config.yml: receive from the app, forward traces to Tempo
receivers:
  otlp:
    protocols:
      grpc: { endpoint: 0.0.0.0:4317 }
      http: { endpoint: 0.0.0.0:4318 }
processors:
  batch: {}
exporters:
  otlp/tempo:
    endpoint: tempo:4317
    tls: { insecure: true }
service:
  pipelines:
    traces:
      receivers: [otlp]
      processors: [batch]
      exporters: [otlp/tempo]
```

```yaml
# grafana/provisioning/datasources/datasources.yml: data sources defined AS CODE, so no clicking through the UI
apiVersion: 1
datasources:
  - name: Prometheus
    type: prometheus
    uid: prometheus
    access: proxy
    url: http://prometheus:9090
    isDefault: true

  - name: Loki
    type: loki
    uid: loki
    access: proxy
    url: http://loki:3100
    jsonData:
      derivedFields:                                           # turns "trace_id":"abc..." in a log line into a clickable link to Tempo
        - name: TraceID
          matcherRegex: '"trace_id":"(\w+)"'
          url: '$${__value.raw}'                               # $$ escapes the $ in provisioning files
          datasourceUid: tempo

  - name: Tempo
    type: tempo
    uid: tempo
    access: proxy
    url: http://tempo:3200
    jsonData:
      tracesToLogsV2:                                          # from a span, jump to that trace's logs in Loki
        datasourceUid: loki
        filterByTraceID: true
      serviceMap:
        datasourceUid: prometheus
```

Start it and point your app at the Collector:

```bash
cd observability && docker compose up -d

# your app (running on the host)
OTEL_SERVICE_NAME=orders-api OTEL_EXPORTER_OTLP_ENDPOINT=http://localhost:4318 npm start
# open Grafana: http://localhost:3001  → Explore → pick Prometheus / Loki / Tempo
```

Generate some traffic (`curl` in a loop, or `autocannon`), then explore. The app's Pino logs must include `"trace_id":"..."` (`04-tracing-and-opentelemetry.md`) for the Loki → Tempo link to light up.

---

## Dashboards

A dashboard is a collection of **panels**, each running one query and rendering it (time series, stat, gauge, table, heatmap, logs, ...).

### Design principle: answer questions, don't display data

The most common dashboard failure is a wall of 40 unrelated charts. A good dashboard answers a specific question for a specific audience, in a deliberate reading order.

| Dashboard | Audience | Question |
|---|---|---|
| **Service overview** (RED) | On-call engineer | "Is this service healthy right now, and if not, where do I look?" |
| **Node.js runtime** | Developers | "Is it the event loop, memory, or GC?" |
| **Dependencies** (DB, Redis, queues, external APIs) | On-call | "Which dependency is hurting us?" |
| **Business** | Product, leadership | "Are users succeeding? Orders, signups, revenue" |
| **SLO** | Team, management | "Are we meeting our reliability targets? How much error budget is left?" |

### The service overview layout

Top to bottom, from "is it OK?" to "why not?":

```
┌─────────────────────────────────────────────────────────────────────────┐
│ ROW 1: At a glance (stat panels, with thresholds → green/yellow/red)    │
│  [Request rate]  [Error ratio]  [p95 latency]  [Instances up]  [Burn]   │
├─────────────────────────────────────────────────────────────────────────┤
│ ROW 2: RED over time                                                    │
│  [Requests/s by route]   [Error ratio by route]   [p50/p95/p99 latency] │
├─────────────────────────────────────────────────────────────────────────┤
│ ROW 3: Saturation / runtime                                             │
│  [Event loop lag p99]  [Heap used]  [GC pause]  [CPU]  [In-flight reqs] │
├─────────────────────────────────────────────────────────────────────────┤
│ ROW 4: Dependencies                                                     │
│  [DB pool in use/waiting] [Query time] [Cache hit ratio] [Outbound errors│
│   by target] [Queue depth / oldest job]                                  │
├─────────────────────────────────────────────────────────────────────────┤
│ ROW 5: Logs (Loki panel filtered to level=error for this service)       │
└─────────────────────────────────────────────────────────────────────────┘
```

### Panel queries (from the metrics you built in `03`)

**Stat panels**

```promql
# Request rate (req/s)
sum(rate(http_requests_total{service="$service"}[$__rate_interval]))

# Error ratio (set unit = Percent (0.0-1.0); thresholds: green < 1%, yellow < 5%, red ≥ 5%)
sum(rate(http_requests_total{service="$service", status_class="5xx"}[$__rate_interval]))
  / sum(rate(http_requests_total{service="$service"}[$__rate_interval]))

# p95 latency (unit = seconds)
histogram_quantile(0.95, sum by (le) (rate(http_request_duration_seconds_bucket{service="$service"}[$__rate_interval])))

# Instances up
sum(up{job="$service"})
```

**Time series panels**

```promql
# Requests per second by route (legend: {{route}})
sum by (route) (rate(http_requests_total{service="$service"}[$__rate_interval]))

# Latency percentiles: three queries on one panel
histogram_quantile(0.50, sum by (le) (rate(http_request_duration_seconds_bucket{service="$service"}[$__rate_interval])))
histogram_quantile(0.95, sum by (le) (rate(http_request_duration_seconds_bucket{service="$service"}[$__rate_interval])))
histogram_quantile(0.99, sum by (le) (rate(http_request_duration_seconds_bucket{service="$service"}[$__rate_interval])))

# Slowest routes (table or bar gauge)
topk(10, histogram_quantile(0.95, sum by (le, route) (rate(http_request_duration_seconds_bucket{service="$service"}[$__rate_interval]))))

# Event loop lag
nodejs_eventloop_lag_p99_seconds{service="$service"}

# Heap and RSS memory (unit = bytes)
nodejs_heap_size_used_bytes{service="$service"}
process_resident_memory_bytes{service="$service"}

# DB pool saturation: waiting > 0 for long means connections are the bottleneck
db_pool_connections{service="$service", state="waiting"}
```

**Heatmap** of `http_request_duration_seconds_bucket` (format: *Heatmap*, a rate query without `histogram_quantile`) shows the *distribution* over time, which is far more informative than a single percentile line: you see bimodal latency (cache hits vs misses) and tail growth.

### Variables: one dashboard, many services

Variables (the dropdowns at the top) let a single dashboard serve every service and environment, instead of copy-pasting dashboards.

| Variable | Type | Query |
|---|---|---|
| `$service` | Query (Prometheus) | `label_values(http_requests_total, service)` |
| `$env` | Query | `label_values(http_requests_total{service="$service"}, env)` |
| `$route` | Query (multi-value, "All") | `label_values(http_requests_total{service="$service"}, route)` |

Then use `{service="$service", env="$env", route=~"$route"}` in every query. Note `$__rate_interval`, Grafana's built-in variable that picks a sensible `rate()` window from the scrape interval and the dashboard zoom. Prefer it over hard-coded `[5m]`.

### Dashboard craft

- **Units and axis labels everywhere** (seconds, bytes, percent, req/s). Unitless numbers are a puzzle.
- **Thresholds and colors** on stat panels, so a glance tells good from bad. Add a **threshold line** on latency panels at your SLO.
- **Limit series per panel** (`topk`, or "Top 5 routes") so legends stay readable.
- **Consistent time ranges and shared crosshair** (Dashboard settings → Graph tooltip: *Shared crosshair*) so you can line up a latency spike with an error spike across panels.
- **Descriptions** on panels ("What this shows and what bad looks like") and **links** to runbooks. A panel nobody understands isn't used.
- **Annotate deploys:** push a Grafana annotation (or a Prometheus `deployment_info` metric) from CI so a regression is visibly "after v1.8.2." Deploy markers answer "did the release cause this?" instantly (`16-production/05-ci-cd.md`).
- **Less is more:** 10–20 panels beat 80. Move deep-dive panels to a second dashboard linked from the first.
- **Dashboards as code** (below), not hand-edited in production.

---

## Provisioning dashboards as code

Dashboards edited by hand in the UI drift, disappear when Grafana's volume is lost, and can't be reviewed. Store dashboard JSON in Git and provision it, like any other config.

```yaml
# grafana/provisioning/dashboards/dashboards.yml
apiVersion: 1
providers:
  - name: default
    folder: Services
    type: file
    allowUiUpdates: true                       # while iterating; set false in production
    options:
      path: /etc/grafana/provisioning/dashboards
```

Workflow: build the dashboard in the UI → **Share → Export → Export for sharing externally** → save the JSON into `grafana/provisioning/dashboards/` → commit. The file provider reloads it automatically.

For larger estates, generate dashboards programmatically (**Grafonnet**/Jsonnet, the **Grafana Foundation SDK**, Terraform's `grafana_dashboard` resource, or Grafana's **Git Sync**). Review dashboard changes in pull requests like code.

---

## Logs in Grafana: Loki and LogQL

LogQL has two parts: a **stream selector** (labels, indexed and cheap), then a **pipeline** (filter and parse text, scanned at query time).

```logql
{service="orders-api"}                                          # all logs from the service
{service="orders-api"} |= "timeout"                             # contains the text
{service="orders-api"} | json | level="error"                   # parse JSON, filter on a field
{service="orders-api"} | json | requestId="req_8f3a2c1d"        # the whole story of one request
{service="orders-api"} | json | err_code="card_declined" | line_format "{{.orderId}} {{.msg}}"
```

Turn logs into metrics (for graphs and alerts):

```logql
sum by (level) (count_over_time({service="orders-api"} | json [5m]))            # log volume by level
sum(rate({service="orders-api"} | json | level="error" [5m]))                    # errors per second from logs
topk(5, sum by (err_code) (count_over_time({service="orders-api"} | json | level="error" [1h])))
```

**Label discipline, same cardinality lesson as Prometheus:** Loki indexes only the labels, so keep them **few and low-cardinality** (`service`, `env`, `level`, `container`). Do **not** make `requestId`, `userId`, or `traceId` labels. Keep them in the JSON body and filter with `| json | requestId="..."`. Putting IDs in labels is the main way people destroy Loki's performance.

The **Logs panel** in a dashboard (filtered to `level="error"`) next to the RED graphs lets you go from "error rate rose" to the actual error messages without leaving the page.

---

## Traces in Grafana: Tempo and TraceQL

In **Explore → Tempo**, you can search for traces or look one up by ID. **TraceQL** queries spans by their attributes:

```traceql
{ resource.service.name = "orders-api" && duration > 1s }                    # slow traces
{ resource.service.name = "orders-api" && status = error }                   # failed traces
{ span.http.route = "/api/v1/orders" && span.http.response.status_code >= 500 }
{ span.app.order.id = "ord_42" }                                              # find the trace for a given order (attributes allow high cardinality)
```

Attribute names follow what your instrumentation emits (semantic conventions have been renamed across versions, for example `http.status_code` → `http.response.status_code`), so check which names your traces actually carry.

Tempo also offers:

- **Service graph:** an auto-generated map of services and their calls, with rates, errors, and latency (needs the Collector/Tempo metrics-generator feeding Prometheus).
- **Trace → logs** and **trace → metrics** links from any span.

---

## Connecting the three signals

This is where Grafana earns its keep. The links you provisioned above give you these workflows:

```
 1. ALERT or dashboard: "error ratio 8%"          (metrics)
        │ click the spike / use an exemplar
        ▼
 2. TRACE: one failing request, span tree        (traces)   → "HTTP POST stripe.com took 3.8 s"
        │ "Logs for this span" button
        ▼
 3. LOGS: the exact lines for that trace_id      (logs)     → err.code = "provider_timeout"
```

| Link | How it's configured |
|---|---|
| **Log line → trace** | Loki data source **derived field** matching `"trace_id":"(\w+)"` and linking to Tempo (provisioned above) |
| **Trace/span → logs** | Tempo data source `tracesToLogsV2` pointing at Loki, filtered by trace ID |
| **Metric point → trace** | **Exemplars**: Prometheus stores a sample trace ID alongside histogram observations; Grafana shows them as dots on the graph and links them to Tempo. Requires the app to attach exemplars (OpenMetrics format, with an exemplar-capable client/SDK) and Prometheus' exemplar storage enabled |
| **Dashboard → logs/traces** | Panel and dashboard **data links**, passing variables through (`${__field.labels.route}`) |

All of it depends on **consistent identifiers**: `trace_id` in your logs, the same `service` name across metrics, logs, and traces. This is why `02-correlation-id.md` and `04-tracing-and-opentelemetry.md` insisted on consistent field names.

---

## Alerting

Grafana has **unified alerting**: rules are evaluated against any data source, routed through **notification policies** to **contact points** (Slack, PagerDuty, email, Opsgenie, webhooks, Microsoft Teams, ...).

### Two places to define alerts

| | Prometheus rules + Alertmanager (`03`) | Grafana-managed alerts |
|---|---|---|
| Defined in | YAML files in Git, next to your service | The Grafana UI/API/Terraform/provisioning files |
| Data sources | Prometheus only | Prometheus, Loki, Tempo metrics, SQL, CloudWatch, and more, even in one rule |
| Evaluation | In Prometheus | In Grafana |
| Good for | Core service alerts, GitOps, closeness to the metrics | Cross-source alerts (log-based, business SQL), teams that live in Grafana |

Either is fine. Pick a primary home so alerts aren't scattered, and keep them in version control (provision Grafana alerts or use Terraform/Prometheus rule files).

### A good Grafana alert rule

1. **Query:** the same PromQL as your dashboard panel.
2. **Condition:** a threshold (`> 0.05`) with a **pending period** (`for 5m`) to avoid flapping.
3. **Labels:** `severity=page|warn`, `service`, `team` (drive routing).
4. **Annotations:** `summary`, a link to the **dashboard**, and the **runbook**.
5. **No-data / error handling:** decide explicitly whether missing data means *alerting* (a dead service sends no data), *OK*, or *no data*.

### Notification policies (routing)

```
severity=page  → PagerDuty (wakes someone)           group by [service, alertname]
severity=warn  → #alerts-warn Slack channel          group & batch to reduce noise
team=payments  → #payments-oncall
```

Group related alerts, set repeat intervals so a long-running issue doesn't spam, and add **mute timings** for planned maintenance windows.

### Alert on what matters (reminder)

Symptoms users feel (error ratio, latency vs SLO, queue age, stale scheduled jobs, business success rates), each with a runbook link, as listed in `03`. A Grafana alert for "CPU > 80%" is as unhelpful as a Prometheus one.

### Alert annotations on dashboards

Show firing alerts and thresholds on the relevant panels so the dashboard, alert, and runbook tell one consistent story.

---

## Managing Grafana in production

| Concern | Guidance |
|---|---|
| **Authentication** | Never leave anonymous admin on (the local stack above does it for convenience). Use SSO (OAuth/OIDC/SAML/LDAP) and map groups to roles |
| **Authorization** | Folders and teams with Viewer/Editor/Admin roles; restrict who can edit production dashboards and data sources |
| **Secrets** | Data source credentials via provisioning with environment variables or a secret store; never committed |
| **Persistence** | Use an external database (PostgreSQL/MySQL) instead of the default SQLite for HA setups; provision config as code so Grafana is disposable |
| **Data source access** | Use `access: proxy` (the Grafana server queries the data source), so browsers never talk to Prometheus/Loki directly |
| **Network** | Put Grafana behind HTTPS and a reverse proxy; keep Prometheus, Loki, and Tempo on private networks |
| **Sizing** | Expensive dashboards (many panels × long ranges × high-cardinality queries) load the backends. Use recording rules, sensible default time ranges, and query limits |
| **Retention & cost** | Metrics 15–90 days at full resolution (longer downsampled in Mimir/Thanos); logs days–weeks; traces days. Decide per signal |
| **Backups** | Dashboards in Git; back up the Grafana DB only if you rely on UI-created content |
| **Upgrades** | Pin versions; read release notes. Provisioning formats and plugin APIs change over time |

### Recording rules: make heavy queries cheap

If a dashboard computes an expensive query on every refresh (p95 over all routes for 30 days), precompute it in Prometheus:

```yaml
# recording-rules.yml
groups:
  - name: orders-api-recording
    rules:
      - record: service:http_request_duration_seconds:p95_5m
        expr: histogram_quantile(0.95, sum by (le, service) (rate(http_request_duration_seconds_bucket[5m])))
      - record: service:http_requests:error_ratio_5m
        expr: sum by (service) (rate(http_requests_total{status_class="5xx"}[5m])) / sum by (service) (rate(http_requests_total[5m]))
```

Dashboards and alerts then query the cheap `service:...` series instead.

---

## Grafana vs the alternatives

| Option | Notes |
|---|---|
| **Grafana + Prometheus/Loki/Tempo** (self-hosted) | Open source, flexible, no vendor lock-in; you operate it |
| **Grafana Cloud** | The same stack managed for you, with a free tier; OTel-native |
| **Datadog, New Relic, Dynatrace, Honeycomb** | Fully managed all-in-one platforms; excellent UX, priced by usage (cost control becomes a job) |
| **Elastic / Kibana** | Strong for logs and search; APM and metrics available |
| **Cloud-native** (CloudWatch, Cloud Monitoring, Azure Monitor) | Zero infrastructure on that cloud; fewer cross-signal conveniences |
| **SigNoz, Uptrace, OpenObserve** | Open-source all-in-one, OTel-first |

Because your instrumentation is **OpenTelemetry** and **Prometheus-format metrics + structured JSON logs**, you can move between these with Collector and exporter configuration rather than rewriting application code.

---

## Practice: use it before you need it

Observability tooling is only valuable if people can use it during an incident. A few habits:

- **Run a game day:** break something in staging (kill the DB, add latency to a dependency, deploy a slow query) and have someone diagnose it using only the dashboards, logs, and traces.
- **Walk through a real past incident** and ask: "Which dashboard/alert would have caught this? How long to find the cause?" Fix the gaps.
- **Put dashboard and runbook links in every alert**, so the person woken at 3 a.m. isn't searching.
- **Review alerts regularly:** delete the ones that never fire or always fire and get ignored.
- **Review dashboards after each incident:** if you needed a panel that didn't exist, add it.
- **Teach new teammates** to follow a request from alert → trace → log in the first week.

---

## Common mistakes

```
❌ Dashboards that are walls of unrelated charts: no reading order, no units, no thresholds
❌ Hard-coded service names and 5m windows instead of variables and $__rate_interval
❌ Average latency as the headline number (hides the tail): show p95/p99 and the heatmap
❌ High-cardinality Loki labels (requestId, userId, traceId): destroys Loki; filter on JSON fields instead
❌ No trace_id in logs, or inconsistent service names across signals: the cross-links never work
❌ Dashboards edited by hand in production, never exported, lost with the container
❌ Anonymous admin access left on outside local development
❌ Alerts defined in five places with no owner; alerts with no runbook; alerts nobody acts on
❌ Alerting on causes (CPU, memory) instead of user-visible symptoms
❌ "No data" alert state left default: a dead service sends no data, so nothing fires
❌ Expensive queries on every refresh instead of recording rules
❌ Treating Grafana as storage: it only displays what the backends keep
❌ Never rehearsing an incident, so the first time the dashboards are used is during a real outage
```

## Checklist

- [ ] Data sources provisioned as code: Prometheus, Loki, Tempo, with UIDs and the cross-links (derived fields, trace-to-logs)
- [ ] Logs carry `trace_id`; `service` names are identical across metrics, logs, and traces
- [ ] A service overview dashboard: stat row → RED over time → runtime saturation → dependencies → error logs
- [ ] Variables (`$service`, `$env`, `$route`) and `$__rate_interval`; units and thresholds on every panel
- [ ] Latency shown as p50/p95/p99 plus a heatmap; SLO line on latency panels
- [ ] Deploy annotations from CI
- [ ] Dashboards and alert rules stored in Git and provisioned, not hand-edited
- [ ] Alerts on symptoms, with `for`, severity, routing, a dashboard link, a runbook link, and deliberate no-data handling
- [ ] Loki labels few and low-cardinality; IDs filtered from the JSON body
- [ ] SSO/roles configured; no anonymous admin; backends on private networks
- [ ] Heavy queries precomputed with recording rules
- [ ] An incident rehearsal done at least once

## Wrap-up

This completes `14-logging-observability/`. The through-line: **emit structured, redacted JSON logs to stdout** (`01`); **give every request one ID that follows it everywhere** (`02`); **measure rates, errors, and durations with bounded labels and alert on symptoms** (`03`); **trace a request through every service to see where time and failures come from** (`04`); and **bring the three signals together, linked by shared identifiers, in dashboards and alerts someone has actually practiced using** (`05`).

## Next

Section **`15-performance/`**: event loop performance, profiling and memory leaks, database optimization, and load balancing and load testing. You now have the instruments (metrics, traces, logs) to *find* where an application is slow; this section teaches how to *fix* it.
