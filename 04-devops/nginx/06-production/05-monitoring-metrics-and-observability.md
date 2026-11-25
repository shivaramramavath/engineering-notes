---
title: "Production Observability, Prometheus Metrics & OpenTelemetry Tracing"
description: "Comprehensive guide to instrumenting NGINX for production observability: stub_status, Prometheus exporter, OpenTelemetry distributed tracing, structured JSON logging, and Grafana dashboards."
module: "06-production"
topic: "05-monitoring-metrics-and-observability"
status: "completed"
tags: ["nginx", "observability", "metrics", "prometheus", "opentelemetry", "tracing", "grafana", "stub-status", "json-logging"]
prerequisites: ["02-core-features/01-logging.md", "06-production/04-troubleshooting-playbooks.md"]
estimated_read_time: "35 min"
---

# Production Observability, Prometheus Metrics & OpenTelemetry Tracing

```
+-------------------------------------------------------------------------------+
| NGINX Observability Pipeline (Metrics, Logs, Traces)                          |
|                                                                               |
|  [Incoming HTTP/HTTPS Traffic]                                                |
|        │                                                                      |
|        ▼                                                                      |
|  [NGINX Core Gateway (Instrumented)]                                          |
|        │                                                                      |
|        ├── 1. Metrics: `stub_status` on 127.0.0.1:8080/stub_status           |
|        │     │ Scraped by `nginx-prometheus-exporter`                         |
|        │     ▼                                                                |
|        │   [Prometheus Server] ---> [Grafana Dashboards & Alertmanager]       |
|        │                                                                      |
|        ├── 2. Logs: High-Precision Structured JSON Access Log                 |
|        │     │ Latency ($request_time, $upstream_response_time), Status       |
|        │     ▼                                                                |
|        │   [Fluentbit / Vector] ---> [Elasticsearch / OpenSearch / Loki]      |
|        │                                                                      |
|        └── 3. Traces: OpenTelemetry Module (`ngx_otel_module`)                |
|              │ Generates Root Spans; Injects `traceparent` (W3C TraceContext) |
|              ▼                                                                |
|            [OTel Collector] ---> [Jaeger / Tempo / Datadog APM]               |
+-------------------------------------------------------------------------------+
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [The Three Pillars of NGINX Observability](#2-the-three-pillars-of-nginx-observability)
3. [The `stub_status` Module & Counter Mechanics](#3-the-stub_status-module--counter-mechanics)
4. [Prometheus Monitoring with `nginx-prometheus-exporter`](#4-prometheus-monitoring-with-nginx-prometheus-exporter)
5. [Structured JSON Logging for Enterprise Ingestion](#5-structured-json-logging-for-enterprise-ingestion)
6. [Distributed Tracing with OpenTelemetry (`ngx_otel_module`)](#6-distributed-tracing-with-opentelemetry-ngx_otel_module)
7. [Alerting Rules for Production Gateways](#7-alerting-rules-for-production-gateways)
8. [Common Pitfalls & Anti-Patterns](#8-common-pitfalls--anti-patterns)
9. [Architectural Comparison: Open-Source vs NGINX Plus Telemetry](#9-architectural-comparison-open-source-vs-nginx-plus-telemetry)
10. [Technical Interview Questions & Scenarios](#10-technical-interview-questions--scenarios)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Operating NGINX in production without real-time observability is like flying blind. When an incident occurs, engineering teams must know instantly:
- **Is the problem in NGINX or the backend?** Comparing `$request_time` (total client duration) against `$upstream_response_time` (backend service duration) instantly isolates the fault domain.
- **Is the system approaching capacity limits?** Monitoring active connections, waiting sockets, and connection drops alerts engineers before packet loss occurs.
- **Where are requests getting stuck across microservices?** Without distributed tracing (`traceparent`), a slow user transaction cannot be correlated between NGINX, downstream API gateways, microservices, and databases.

Instrumenting NGINX with Prometheus, structured JSON logs, and OpenTelemetry turns the proxy into a rich source of telemetry.

---

## 2. The Three Pillars of NGINX Observability

| Pillar | Mechanism in NGINX | Data Emitted | Target Platform |
| :--- | :--- | :--- | :--- |
| **Metrics** | `ngx_http_stub_status_module` + Exporter | Active connections, req/sec, handshakes | Prometheus, Datadog |
| **Logs** | Structured JSON `log_format` | Per-transaction timing, status, IP, URI | Loki, Elasticsearch, OpenSearch |
| **Traces** | `ngx_otel_module` (OpenTelemetry) | W3C trace context, spans, child propagation | Jaeger, Grafana Tempo |

---

## 3. The `stub_status` Module & Counter Mechanics

The open-source `ngx_http_stub_status_module` provides real-time, in-memory basic connection counters.

### Configuration
```nginx
# /etc/nginx/conf.d/status.conf
server {
    listen 127.0.0.1:8080;
    server_name 127.0.0.1;

    location /stub_status {
        stub_status;
        access_log off;
        allow 127.0.0.1;
        deny all;
    }
}
```

### Raw Output Inspection
```bash
curl http://127.0.0.1:8080/stub_status
```

Output:
```text
Active connections: 291
server accepts handled requests
 16630948 16630948 31070651
Reading: 6 Writing: 179 Waiting: 106
```

### Detailed Field Definitions:
1. `Active connections`: Total current open connections, including clients currently in keepalive or actively transmitting.
2. `server accepts`: Total number of accepted client connections since NGINX started.
3. `server handled`: Total number of handled connections. If `handled < accepts`, NGINX dropped connections because resource limits (like `worker_connections`) were reached!
4. `server requests`: Total number of client HTTP requests processed. The ratio $\frac{\text{requests}}{\text{handled}}$ indicates HTTP keepalive efficiency.
5. `Reading`: Current connections where NGINX is actively reading client request headers.
6. `Writing`: Current connections where NGINX is actively reading the request body, processing a request, or streaming response bytes back to the client.
7. `Waiting`: Current idle client connections sitting in keepalive awaiting the next request (`keepalive_timeout`).

---

## 4. Prometheus Monitoring with `nginx-prometheus-exporter`

Prometheus cannot parse raw text from `stub_status` directly. The official `nginx-prometheus-exporter` acts as a sidecar daemon, converting `stub_status` metrics into standard Prometheus OpenMetrics format:

```
+-------------------------------------------------------------------------------+
| Prometheus Exporter Integration Architecture                                  |
|                                                                               |
|  [NGINX: localhost:8080/stub_status]                                          |
|        ▲                                                                      |
|        │ HTTP GET (Every 15s)                                                 |
|  [nginx-prometheus-exporter (Port 9113)]                                      |
|        ▲                                                                      |
|        │ Scraped via /metrics                                                 |
|  [Prometheus Server (scrape_interval: 15s)]                                   |
|        │                                                                      |
|        ▼                                                                      |
|  [Grafana Dashboard (ID: 12708)]                                              |
+-------------------------------------------------------------------------------+
```

### Running the Exporter via Docker
```bash
docker run -d \
  --name nginx-exporter \
  --net=host \
  nginx/nginx-prometheus-exporter:1.1.0 \
  -nginx.scrape-uri=http://127.0.0.1:8080/stub_status
```

### Prometheus Scrape Configuration (`prometheus.yml`)
```yaml
scrape_configs:
  - job_name: 'nginx-edge'
    scrape_interval: 10s
    static_configs:
      - targets: ['10.0.1.10:9113', '10.0.1.11:9113']
        labels:
          environment: 'production'
          role: 'edge-gateway'
```

---

## 5. Structured JSON Logging for Enterprise Ingestion

Traditional Common Log Format (`combined`) requires fragile regex parsers (Grok filters) that break whenever a custom header contains quotes or brackets. In production, always emit native structured JSON:

```nginx
# /etc/nginx/conf.d/logging.conf

http {
    log_format json_analytics escape=json '{'
        '"time_local":"$time_iso8601",'
        '"client_ip":"$remote_addr",'
        '"real_ip":"$realip_remote_addr",'
        '"request_method":"$request_method",'
        '"request_uri":"$request_uri",'
        '"server_protocol":"$server_protocol",'
        '"status":$status,'
        '"body_bytes_sent":$body_bytes_sent,'
        '"request_time":$request_time,'
        '"upstream_connect_time":"$upstream_connect_time",'
        '"upstream_header_time":"$upstream_header_time",'
        '"upstream_response_time":"$upstream_response_time",'
        '"upstream_addr":"$upstream_addr",'
        '"upstream_status":"$upstream_status",'
        '"http_referrer":"$http_referer",'
        '"http_user_agent":"$http_user_agent",'
        '"http_x_forwarded_for":"$http_x_forwarded_for",'
        '"ssl_protocol":"$ssl_protocol",'
        '"ssl_cipher":"$ssl_cipher"'
    '}';

    access_log /var/log/nginx/access.log json_analytics buffer=512k flush=5s;
}
```

*Key Benefit*: Tools like Vector, Fluentbit, and Logstash parse JSON lines in native $O(1)$ time, eliminating high CPU overhead in logging pipelines.

---

## 6. Distributed Tracing with OpenTelemetry (`ngx_otel_module`)

Starting with NGINX 1.23.4+, official OpenTelemetry support is provided via the native C dynamic module `ngx_otel_module`. It participates in distributed traces by generating spans and propagating W3C `traceparent` headers downstream:

```
+-------------------------------------------------------------------------------+
| OpenTelemetry Distributed Trace Flow                                         |
|                                                                               |
|  Client Request (No trace header)                                             |
|        │                                                                      |
|        ▼                                                                      |
|  [NGINX Gateway (ngx_otel_module)]                                            |
|        │  Creates Root Span: "nginx_proxy" (SpanID: e4a2, TraceID: 98f1)      |
|        │  Injects Header: `traceparent: 00-98f1-e4a2-01`                      |
|        │  Exports span via OTLP/gRPC to OTel Collector                        |
|        ▼                                                                      |
|  [Downstream Microservice (Node.js / Go / Java)]                              |
|        │  Reads incoming traceparent; creates Child Span (ParentID: e4a2)     |
|        ▼                                                                      |
|  [Database Query Span]                                                        |
|                                                                               |
|  Unified End-to-End Flame Chart Visible in Jaeger / Grafana Tempo!            |
+-------------------------------------------------------------------------------+
```

### Configuring OpenTelemetry in `nginx.conf`

```nginx
# Load module at top of nginx.conf
load_module modules/ngx_otel_module.so;

http {
    # 1. Configure OpenTelemetry Exporter Endpoint
    otel_exporter {
        endpoint otel-collector.monitoring.svc.cluster.local:4317;
    }

    # 2. Define Service Identity
    otel_service_name "nginx-ingress-gateway";

    # 3. Sampling Rate (1.0 = 100% of requests; 0.1 = 10% sample)
    otel_trace on;
    otel_trace_context inject;
    otel_span_name "http_request";

    server {
        listen 443 ssl;
        server_name api.example.com;

        location / {
            proxy_pass http://backend_api;
            # Propagate trace context to upstream automatically
            proxy_set_header traceparent $otel_traceparent;
        }
    }
}
```

---

## 7. Alerting Rules for Production Gateways

Configure these essential Prometheus Alertmanager rules to detect outages proactively:

```yaml
# /etc/prometheus/rules/nginx-alerts.yml
groups:
  - name: nginx_edge_alerts
    rules:
      # 1. Elevated 5xx Error Rate (> 2% for 3 minutes)
      - alert: NginxHighErrorRate
        expr: (sum(rate(nginx_http_requests_total{status=~"5.."}[3m])) / sum(rate(nginx_http_requests_total[3m]))) * 100 > 2
        for: 2m
        labels:
          severity: critical
        annotations:
          summary: "NGINX Edge experiencing high 5xx error rate (>2%)"
          description: "Current error rate is {{ $value | printf '%.2f' }}% on instance {{ $labels.instance }}."

      # 2. Connection Drops (accepts != handled)
      - alert: NginxDroppingConnections
        expr: rate(nginx_connections_accepted[1m]) - rate(nginx_connections_handled[1m]) > 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "NGINX is actively dropping connections!"
          description: "Handled connections is lower than accepted. Check worker_connections and system limits."

      # 3. High Latency Spike (p99 > 500ms)
      - alert: NginxElevatedLatency
        expr: histogram_quantile(0.99, sum(rate(nginx_http_request_duration_seconds_bucket[5m])) by (le)) > 0.5
        for: 3m
        labels:
          severity: warning
        annotations:
          summary: "NGINX p99 latency exceeds 500ms"
          description: "p99 latency is currently {{ $value | printf '%.3f' }}s on {{ $labels.instance }}."
```

---

## 8. Common Pitfalls & Anti-Patterns

| Anti-Pattern / Mistake | Root Cause | Observable Production Impact | Correct Solution |
| :--- | :--- | :--- | :--- |
| Exposing `/stub_status` to Public Internet | Missing access control on status location | Competitors/scanners view active connection volumes | Bind strictly to `127.0.0.1` and enforce `allow 127.0.0.1; deny all;` |
| Logging Unescaped Variables in JSON | Using standard string concatenation in `log_format` | Clients sending quotes (`"`) break JSON parsing in collectors | Always specify `escape=json` on the `log_format` line |
| Enabling 100% OTel Tracing at 50k req/sec | Sampling all spans (`otel_trace on` with ratio 1.0) | High network bandwidth and OTel collector buffer exhaustion | Tune sampling ratio (e.g. `0.05` for 5% probabilistic sampling) |
| Ignoring Connection Waiting Counters | Assuming high `Active connections` means high load | Unnecessary scaling when connections are merely idle keepalives | Compare `Writing` vs `Waiting` to determine real activity |
| Synchronous Unbuffered JSON Logging | Writing JSON records directly to disk synchronously | High disk I/O wait times; worker event loop blocked | Add `buffer=512k flush=5s` to `access_log` |

---

## 9. Architectural Comparison: Open-Source vs NGINX Plus Telemetry

| Capability / Metric | Open-Source NGINX (`stub_status`) | NGINX Plus (`api` module) |
| :--- | :--- | :--- |
| **Connection Metrics** | Basic (Active, Reading, Writing, Waiting) | Full live connection and worker breakdown |
| **Per-Virtual-Host Stats** | ❌ No (Single global cluster counter) |  Yes (`http/server_zones/`) |
| **Per-Upstream Health** | ❌ No (Requires external log scraping) |  Yes (Upstream node latency, state, fails) |
| **Cache Metrics** | ❌ No (Requires access log parsing) |  Yes (Cache hit, miss, bypass, expired) |
| **Dynamic Configuration** | ❌ No (Requires config reloads) |  Yes (Dynamic upstream draining via REST API) |
| **OpenTelemetry Tracing** |  Supported (`ngx_otel_module`) |  Supported (`ngx_otel_module`) |

---

## 10. Technical Interview Questions & Scenarios

<details>
<summary><strong>1. Explain the relationship between `accepts`, `handled`, and `requests` in `stub_status`.</strong></summary>

- `accepts`: The total number of TCP connection handshakes accepted by NGINX workers.
- `handled`: The total number of connections that NGINX successfully established without aborting. Under healthy operating conditions, `accepts == handled`. If `handled < accepts`, NGINX is actively dropping connections due to reaching `worker_connections`, OS file descriptor limits, or `somaxconn` queue overflows.
- `requests`: The total number of HTTP requests processed. Because HTTP keepalive permits multiple sequential requests over a single persistent TCP connection, `requests` is normally significantly higher than `handled`. The ratio $\frac{\text{requests}}{\text{handled}}$ indicates average keepalive reuse (e.g., a ratio of 5.0 means clients average 5 requests per TCP socket).
</details>

<details>
<summary><strong>2. What is the difference between `$request_time` and `$upstream_response_time` in NGINX logs?</strong></summary>

- `$request_time`: Measures the **total time** elapsed from when NGINX reads the first byte of the client's request to when NGINX finishes writing the last byte of the response back to the client. This includes client request transmission time, NGINX internal processing, backend upstream response time, and downstream client network transfer time.
- `$upstream_response_time`: Measures strictly the time elapsed between NGINX connecting to the backend upstream server and NGINX receiving the final response byte from that backend.
If `$request_time` is 5.0 seconds but `$upstream_response_time` is 0.05 seconds, the slowness is caused by a slow client network connection or client-side buffering, not the application backend!
</details>

<details>
<summary><strong>3. Why is `escape=json` mandatory when authoring JSON log formats in NGINX?</strong></summary>

Without `escape=json`, if a client issues a request with quotes or special control characters (e.g. `User-Agent: Mozilla/5.0 "Special"` or query strings with unencoded quotes), NGINX prints the literal characters into the log file. This invalidates the JSON syntax of the line. Downstream log forwarders (Fluentbit, Logstash, Vector) fail to parse the corrupted line, dropping telemetry or crashing parser pipelines. `escape=json` automatically escapes quotes, backslashes, and control characters into valid JSON strings.
</details>

<details>
<summary><strong>4. How does OpenTelemetry trace context propagation work in NGINX?</strong></summary>

NGINX adheres to the W3C TraceContext standard. When a client request arrives:
1. If the request already contains a `traceparent` header (e.g. from an upstream mobile app or CDN), NGINX extracts the `TraceID` and `ParentSpanID`.
2. If no header exists, NGINX generates a new 128-bit `TraceID`.
3. NGINX creates an internal span for its own routing and proxy processing.
4. Using `proxy_set_header traceparent $otel_traceparent;`, NGINX generates and forwards an updated `traceparent` header to the backend upstream, designating its own span as the parent. The downstream microservice reads this header to continue the distributed trace.
</details>

<details>
<summary><strong>5. What causes a high count in the `Waiting` connection metric in `stub_status`?</strong></summary>

Connections in the `Waiting` state are idle TCP connections kept open in the keepalive state waiting for subsequent client requests. A high `Waiting` count is normal and healthy for high-traffic sites using HTTP/1.1 or HTTP/2 persistent connections. However, if `Waiting` approaches `worker_connections`, it can exhaust available connection slots. It can be tuned by reducing `keepalive_timeout` (e.g. from 65s to 15s) or lowering `keepalive_requests`.
</details>

<details>
<summary><strong>6. How do you monitor per-upstream health in open-source NGINX without NGINX Plus?</strong></summary>

Open-source NGINX does not expose a real-time upstream status API. Teams achieve upstream monitoring by:
1. Emitting `$upstream_addr`, `$upstream_status`, and `$upstream_response_time` in structured JSON access logs.
2. Using a log forwarder (like Vector or Fluentbit) to aggregate metric counters by upstream address.
3. Or running a sidecar synthetic prober (such as Blackbox Exporter) that directly queries internal upstream service health check endpoints.
</details>

<details>
<summary><strong>7. Why should `/stub_status` always have `access_log off;` configured?</strong></summary>

Monitoring scrapers (Prometheus exporter, Datadog agent) poll `/stub_status` every 5 to 15 seconds. If access logging is enabled, these automated polling requests flood `/var/log/nginx/access.log`, inflating log volumes, consuming unnecessary disk I/O, and skewing application request rate metrics.
</details>

<details>
<summary><strong>8. What is the difference between `$time_local` and `$time_iso8601` in log formats?</strong></summary>

- `$time_local` emits timestamps in standard Common Log Format: `25/Nov/2026:14:30:00 +0000`. This format is difficult for modern analytics databases to index without custom timestamp parsing routines.
- `$time_iso8601` emits timestamps in standard ISO 8601 format: `2026-11-25T14:30:00+00:00`. This is natively recognized as a timestamp data type by Elasticsearch, BigQuery, ClickHouse, and Postgres.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure and Restrict `stub_status` Access</strong></summary>

**Goal**: Enable metrics endpoint restricted to localhost.
1. Add the `status.conf` block from Section 3 to `/etc/nginx/conf.d/`.
2. Reload NGINX: `sudo nginx -s reload`.
3. Query locally: `curl http://127.0.0.1:8080/stub_status` -> Confirm metrics returned.
4. Attempt to query using the host's public IP from another machine -> Confirm HTTP 403 Forbidden.
</details>

<details>
<summary><strong>Exercise 2: Deploy `nginx-prometheus-exporter` and Scrape Metrics</strong></summary>

**Goal**: Expose OpenMetrics format to Prometheus.
1. Run the exporter binary or Docker container targeting `http://127.0.0.1:8080/stub_status`.
2. Curl the exporter endpoint: `curl http://localhost:9113/metrics`.
3. Verify metrics such as `nginx_connections_active` and `nginx_http_requests_total` are present.
</details>

<details>
<summary><strong>Exercise 3: Implement Structured JSON Logging and Validate Syntax</strong></summary>

**Goal**: Replace default access logs with machine-readable JSON.
1. Configure `log_format json_analytics` from Section 5.
2. Reload NGINX and generate requests with curl.
3. Validate output with `jq`:
```bash
tail -n 1 /var/log/nginx/access.log | jq .
```
4. Confirm all JSON fields parse cleanly without escaping errors.
</details>

<details>
<summary><strong>Exercise 4: Correlate Client Latency vs Upstream Latency</strong></summary>

**Goal**: Analyze time spent in NGINX vs backend.
1. Create a dummy slow backend that sleeps 2 seconds.
2. Issue a request through NGINX: `curl http://localhost/slow`.
3. Inspect the JSON access log for that request.
4. Verify that `request_time` and `upstream_response_time` both report ~2.000s.
</details>

<details>
<summary><strong>Exercise 5: Build a Grafana Dashboard for Connection States</strong></summary>

**Goal**: Visualize NGINX connection health.
1. Import official Grafana Dashboard ID `12708` into Grafana.
2. Configure Prometheus data source.
3. Run a load test with `wrk` and observe active, reading, writing, and waiting connection graphs update in real-time.
</details>

<details>
<summary><strong>Exercise 6: Author an Alertmanager Connection Drop Rule</strong></summary>

**Goal**: Alert when NGINX drops connections due to resource exhaustion.
1. Implement the `NginxDroppingConnections` alert rule from Section 7.
2. Artificially lower `worker_connections` to 10 in `nginx.conf` and trigger load with `ab -n 100 -c 50 http://localhost/`.
3. Verify Prometheus evaluates the alert to `FIRING`.
</details>

---

## 12. Summary & Quick Reference

```
+--------------------------------------------------------------------------------+
| NGINX Observability Quick Reference                                            |
|                                                                                |
| Metric / Directive            | Source          | Meaning                      |
| :---------------------------- | :-------------- | :--------------------------- |
| stub_status;                  | ngx_http_stub.. | Enables metrics endpoint     |
| Active connections            | stub_status     | All currently open sockets   |
| Reading                       | stub_status     | Reading request headers      |
| Writing                       | stub_status     | Processing / writing response|
| Waiting                       | stub_status     | Idle keepalive connections   |
| $request_time                 | Core Variable   | Total transaction time (sec) |
| $upstream_response_time       | Proxy Variable  | Upstream service time (sec)   |
| $upstream_connect_time        | Proxy Variable  | TCP handshake time to backend|
| escape=json                   | log_format      | Sanitizes strings for JSON   |
| otel_trace on;                | ngx_otel_module | Enables OpenTelemetry tracing|
+--------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [NGINX Official Docs: Module ngx_http_stub_status_module](https://nginx.org/en/docs/http/ngx_http_stub_status_module.html)
- [Official NGINX Prometheus Exporter Repository](https://github.com/nginxinc/nginx-prometheus-exporter)
- [OpenTelemetry NGINX Dynamic Module Documentation](https://github.com/open-telemetry/opentelemetry-cpp-contrib/tree/main/instrumentation/nginx)
- [Next Concept: High Availability & Keepalived VRRP Architecture](06-high-availability-and-vrrp.md)

---

[← Previous Note: 04-troubleshooting-playbooks.md](04-troubleshooting-playbooks.md) | [Back to Module Index](README.md) | [Next Note: 06-high-availability-and-vrrp.md →](06-high-availability-and-vrrp.md)
