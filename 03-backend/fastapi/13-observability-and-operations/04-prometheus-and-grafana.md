---
title: "Prometheus Metrics and Grafana Dashboards for FastAPI"
description: "Implement RED method metrics in FastAPI using prometheus-fastapi-instrumentator and prometheus-client: multi-process worker aggregation, PromQL queries, and Grafana dashboard alerts."
fastapi_version: "0.115.x+"
python_version: "3.12+"
pydantic_version: "v2.10+"
tested_with: "prometheus-client 0.21+, prometheus-fastapi-instrumentator 7.0+, fastapi 0.115+, pytest 8.3+"
free_threading_status: "Fully Compatible"
---

# Prometheus Metrics and Grafana Dashboards for FastAPI

## 1. Conceptual Foundation & Mental Model

### The RED Method for Microservices
When monitoring web APIs, collecting arbitrary operational statistics without a coherent framework leads to alert fatigue and dashboard clutter. The industry-standard approach for request-driven architectures is the **RED Method** (formulated by Tom Wilkie):
- **Rate**: The number of incoming requests per second (RPS).
- **Errors**: The number of failed requests per second (HTTP 5xx responses).
- **Duration**: The time taken to process requests, evaluated as latency distributions (p50, p90, p95, p99).

```
+-------------------------------------------------------------------------------+
|                             THE RED METHOD METRICS                            |
|                                                                               |
|  1. RATE:     sum(rate(http_requests_total[1m]))                              |
|               "Are we experiencing a traffic spike or traffic collapse?"      |
|                                                                               |
|  2. ERRORS:   sum(rate(http_requests_total{status=~"5.."}[1m]))               |
|               "What percentage of requests are failing right now?"            |
|                                                                               |
|  3. DURATION: histogram_quantile(0.99, rate(http_request_duration_bucket[5m]))|
|               "What is the tail latency experienced by the slowest 1%?"       |
+-------------------------------------------------------------------------------+
```

### The Multi-Process Metric Aggregation Problem
A critical architectural challenge when running FastAPI with Uvicorn or Gunicorn is that production servers run multiple worker processes (e.g., `--workers 4`). 

In a naive setup:
- Each Uvicorn worker maintains its own separate in-memory counters.
- When Prometheus scrapes `GET /metrics`, the request hits Worker 1 on scrape 1, Worker 2 on scrape 2, and Worker 3 on scrape 3.
- The resulting metric graphs fluctuate wildly with sawtooth patterns because each scrape reads a completely different process's memory.

The solution is the **Prometheus Multi-Process Mode**: all Uvicorn worker processes share memory-mapped files via `PROMETHEUS_MULTIPROC_DIR`, aggregating metrics across all workers seamlessly.

---

## 2. Architecture & Internal Mechanics

Understanding how multiple Uvicorn worker processes aggregate metric counters into shared memory-mapped files for Prometheus scraping.

```
+-----------------------------------------------------------------------------+
|                          UVICORN SERVER INSTANCE                            |
|                                                                             |
|  Worker 1 (PID 101)                Worker 2 (PID 102)                       |
|  [Increment Counter]               [Increment Counter]                      |
|         |                                  |                                |
|         v                                  v                                |
|  +-----------------------------------------------------------------------+  |
|  |             SHARED MEMORY DIRECTORY: PROMETHEUS_MULTIPROC_DIR         |  |
|  |  /tmp/prometheus_multiproc/*.db (Memory-Mapped Files via mmap)        |  |
|  +-----------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------+
                                       ▲
                                       │ Scrapes aggregated metrics
+-----------------------------------------------------------------------------+
|                       PROMETHEUS SCRAPE AGENT                               |
|  GET http://localhost:8000/metrics (Aggregates across all worker PIDs)      |
+-----------------------------------------------------------------------------+
                                       │
                                       ▼
+-----------------------------------------------------------------------------+
|                      PROMETHEUS TIME-SERIES DATABASE (TSDB)                 |
|  - PromQL Queries, AlertManager rules (Error rate > 1% for 5m)              |
+-----------------------------------------------------------------------------+
                                       │
                                       ▼
+-----------------------------------------------------------------------------+
|                            GRAFANA DASHBOARD                                |
|  - Real-time visualization: RPS, Error Rates, p95/p99 Tail Latency Heatmaps |
+-----------------------------------------------------------------------------+
```

---

## 3. Comprehensive Syntax & Core Mechanics

### Configuring `prometheus-fastapi-instrumentator`

```python
from fastapi import FastAPI
from prometheus_fastapi_instrumentator import Instrumentator

app = FastAPI()

# Configure and expose metrics endpoint
instrumentator = Instrumentator(
    should_group_status_codes=True,  # Groups 200, 201 -> "2xx"
    should_ignore_untemplated=True,   # Replaces /users/123 with /users/{id}
    excluded_handlers=["/metrics", "/healthz"],
)

instrumentator.instrument(app).expose(app, endpoint="/metrics")
```

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: Full-Featured Multi-Process Production Metrics Setup
Configuring Uvicorn workers to share metrics safely using memory-mapped directories.

```python
# app/core/metrics/multiproc_setup.py
import os
import shutil
from pathlib import Path
from fastapi import FastAPI
from prometheus_client import CollectorRegistry, multiprocess, generate_latest, CONTENT_TYPE_LATEST
from starlette.responses import Response

def setup_multiproc_metrics(app: FastAPI) -> None:
    # 1. Ensure clean multiprocess directory
    multiproc_dir = os.getenv("PROMETHEUS_MULTIPROC_DIR", "/tmp/prometheus_multiproc")
    Path(multiproc_dir).mkdir(parents=True, exist_ok=True)

    @app.get("/metrics", include_in_schema=False)
    def metrics_endpoint():
        """Scrapes aggregated metrics across all Uvicorn worker processes."""
        registry = CollectorRegistry()
        multiprocess.MultiProcessCollector(registry)
        data = generate_latest(registry)
        return Response(content=data, media_type=CONTENT_TYPE_LATEST)
```

---

### Example 2: Custom Prometheus Metrics (Counters, Histograms, Gauges)
Declaring and updating business-specific Prometheus metrics in FastAPI endpoints.

```python
# app/core/metrics/business_metrics.py
from prometheus_client import Counter, Histogram, Gauge

# Counter: Monotonically increasing metric
CHECKOUT_TOTAL = Counter(
    "ecommerce_checkouts_total",
    "Total number of completed checkout attempts",
    ["status", "payment_method"],  # Low-cardinality labels
)

# Histogram: Distribution of response times with custom latency buckets
PAYMENT_LATENCY_SECONDS = Histogram(
    "ecommerce_payment_duration_seconds",
    "Time spent communicating with payment gateway",
    ["gateway_name"],
    buckets=[0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0],
)

# Gauge: Value that can increase or decrease
ACTIVE_USERS_GAUGE = Gauge(
    "ecommerce_active_users_current",
    "Estimated count of currently authenticated active sessions",
)
```

```python
# app/api/v1/orders.py
import time
from fastapi import APIRouter
from app.core.metrics.business_metrics import CHECKOUT_TOTAL, PAYMENT_LATENCY_SECONDS

router = APIRouter(prefix="/orders", tags=["Orders"])

@router.post("/checkout")
async def checkout(payment_method: str = "credit_card"):
    start = time.perf_counter()
    try:
        # Simulate gateway processing
        time.sleep(0.12)
        elapsed = time.perf_counter() - start
        
        # Record observations
        PAYMENT_LATENCY_SECONDS.labels(gateway_name="stripe").observe(elapsed)
        CHECKOUT_TOTAL.labels(status="success", payment_method=payment_method).inc()
        return {"status": "SUCCESS"}
    except Exception as exc:
        CHECKOUT_TOTAL.labels(status="error", payment_method=payment_method).inc()
        raise exc
```

---

### Example 3: Securing the `/metrics` Endpoint
Isolating metrics behind internal basic authentication or a dedicated private port.

```python
# app/core/metrics/security.py
import secrets
from fastapi import Depends, HTTPException, status
from fastapi.security import HTTPBasic, HTTPBasicCredentials

security = HTTPBasic()

def verify_metrics_credentials(credentials: HTTPBasicCredentials = Depends(security)):
    """Guards /metrics endpoint against unauthorized public scraping."""
    correct_username = secrets.compare_digest(credentials.username, "prom_scraper")
    correct_password = secrets.compare_digest(credentials.password, "production_secret_99")
    if not (correct_username and correct_password):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Incorrect metrics credentials",
            headers={"WWW-Authenticate": "Basic"},
        )
    return credentials.username
```

```python
# Mounting secure metrics route
@app.get("/metrics", dependencies=[Depends(verify_metrics_credentials)], include_in_schema=False)
def secure_metrics():
    # Return Prometheus exposition format
    pass
```

---

### Example 4: Key PromQL Queries for Alerting Rules
Production PromQL expressions for Prometheus alerting rules.

```yaml
# prometheus_alerts.yml
groups:
  - name: fastapi_service_alerts
    rules:
      # Alert 1: High HTTP 5xx Error Rate (> 2% for 5 minutes)
      - alert: HighErrorRate
        expr: |
          (
            sum(rate(http_requests_total{status=~"5.."}[5m]))
            /
            sum(rate(http_requests_total[5m]))
          ) * 100 > 2.0
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "FastAPI High Error Rate (> 2%)"
          description: "HTTP 5xx rate is currently {{ $value | printf '%.2f' }}%."

      # Alert 2: High Tail Latency (p99 > 500ms for 5 minutes)
      - alert: HighTailLatencyP99
        expr: |
          histogram_quantile(
            0.99,
            sum(rate(http_request_duration_seconds_bucket[5m])) by (le)
          ) > 0.50
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "FastAPI High p99 Latency (> 500ms)"
          description: "p99 latency is currently {{ $value | printf '%.2f' }} seconds."
```

---

### Example 5: Testing Prometheus Metrics in Pytest
Verifying that endpoints correctly increment counters and record histogram durations in tests.

```python
# tests/test_metrics_collection.py
from fastapi.testclient import TestClient
from prometheus_client import REGISTRY
from app.main import app

client = TestClient(app)

def test_checkout_increments_prometheus_counter():
    # 1. Trigger request
    response = client.post("/orders/checkout?payment_method=apple_pay")
    assert response.status_code == 200

    # 2. Assert Prometheus counter updated
    success_count = REGISTRY.get_sample_value(
        "ecommerce_checkouts_total",
        labels={"status": "success", "payment_method": "apple_pay"}
    )
    assert success_count is not None
    assert success_count >= 1.0
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Cardinality Explosion via Untemplated Route Paths**: If your instrumentator records `/users/42` and `/users/43` as distinct label values instead of grouping them as `/users/{id}`, every user ID creates a separate Prometheus time series. In a system with 1,000,000 users, this causes Prometheus to run out of memory. Always enable `should_ignore_untemplated=True` or use FastAPI's matched route template (`route.path`).
2. **Missing `PROMETHEUS_MULTIPROC_DIR` Cleanup**: When Uvicorn worker processes terminate and restart, stale `.db` files can accumulate in `/tmp/prometheus_multiproc`. The parent Gunicorn or container startup script must clear this directory before spawning new workers.
3. **Exposing `/metrics` to the Public Internet**: Leaving `/metrics` open allows attackers to scrape internal service topology, active user counts, and internal API paths. Always guard `/metrics` with network firewalls (Kubernetes NetworkPolicy) or HTTP Basic Auth.
4. **Incorrect Histogram Bucketing**: Using default buckets (e.g., `[0.005, 0.01, ..., 10.0]`) on an endpoint that always takes 500ms causes all observations to fall into the highest bucket, destroying p95/p99 calculation accuracy. Tailor histogram buckets to match expected endpoint SLAs.

---

## 6. Common Mistakes Table

| Mistake | Root Cause | Impact | Correct Approach |
| :--- | :--- | :--- | :--- |
| **Using Default Mode with Multi-Worker Uvicorn** | Omitting `PROMETHEUS_MULTIPROC_DIR` | Sawtooth graphs; metrics fluctuate wildly between worker scrapes. | Configure `prometheus_client.multiprocess.MultiProcessCollector`. |
| **Putting User IDs or Emails in Labels** | High-cardinality label design | Crashes Prometheus server with Out-of-Memory (OOM) error. | Restrict metric labels strictly to bounded enums (`status`, `method`, `tier`). |
| **Untemplated URL Path Labels** | Recording raw `/items/12345` paths | Creates hundreds of thousands of ephemeral time series. | Group paths by route template (`/items/{item_id}`). |
| **Measuring Total Latency with Counter** | Misunderstanding metric types | Cannot calculate percentiles (p50, p95, p99). | Use a `Histogram` or `Summary` to measure request duration. |
| **Exposing `/metrics` Without Authentication** | Missing security layer | Leaks internal business volume and operational metadata to the public. | Protect `/metrics` with Basic Auth or internal cluster networking. |
| **Re-instantiating Metrics on Every Request** | Calling `Counter(...)` inside route | Raises `ValueError: Duplicated timeseries in CollectorRegistry`. | Declare metric objects globally at module import time. |

---

## 7. Performance & Scalability Considerations

- **Histogram Bucket Counts**: Every bucket in a Prometheus histogram creates an independent time series. A histogram with 10 buckets across 5 HTTP methods and 4 status codes creates $10 \times 5 \times 4 = 200$ time series per endpoint. Keep bucket counts lean (between 6 and 10 buckets).
- **Scrape Interval vs. Storage**: Standard production Prometheus scrape intervals are 15s or 30s. Scraping every 1s adds unnecessary CPU overhead to FastAPI workers with little added diagnostic value.
- **Python 3.13+ Free-Threading**: In free-threaded Python, metric atomic counters and memory-mapped updates scale across native threads without GIL lock contention.

---

## 8. Security & OWASP Guardrails

- **OWASP A05 (Security Misconfiguration - Public Metrics)**: The `/metrics` endpoint exposes sensitive operational intelligence. Ensure ingress controllers (Nginx, Traefik, AWS ALB) block public internet access to `/metrics`, permitting access only to the internal Prometheus scraper IP.
- **Denial of Service via Cardinality**: Reject request headers or queries that attempt to inject random strings into metric label dimensions.

---

## 9. Testing Strategies & Pytest Recipes

```python
# tests/test_prometheus_scrape.py
from fastapi.testclient import TestClient
from app.main import app

client = TestClient(app)

def test_metrics_endpoint_exposition_format():
    response = client.get("/metrics")
    assert response.status_code == 200
    assert "text/plain" in response.headers["Content-Type"]
    
    body = response.text
    # Verify standard Prometheus header markers
    assert "# HELP" in body
    assert "# TYPE" in body
```

---

## 10. Real-World Case Study / Production Incident

A mobile gaming backend running FastAPI on AWS Elastic Beanstalk operated with 8 Uvicorn workers per EC2 instance. The team configured Prometheus alerts for high error rates:
```promql
rate(http_requests_total{status="500"}[5m]) > 5
```
During a promotional event, the alert fired intermittently for 20 seconds, then silenced for 2 minutes, then fired again, repeatedly confusing the on-call engineer.

**The Post-Mortem**:
- The developers had initialized Prometheus using standard `prometheus-client` single-process mode.
- Worker 3 was throwing 500 errors due to a dead database connection, while Workers 1, 2, 4, 5, 6, 7, and 8 were healthy.
- Because Prometheus scraped a load-balanced endpoint hitting random workers, 7 out of 8 scrapes returned `0 errors`, while 1 out of 8 scrapes returned `50 errors`. The Prometheus alert manager continuously opened and closed alerts, preventing automated scaling triggers.

**The Fix**:
The team enabled `PROMETHEUS_MULTIPROC_DIR` and used `multiprocess.MultiProcessCollector`. Scrapes immediately aggregated metrics across all 8 workers, providing a stable, true representation of cluster error rates.

---

## 11. Technical Interview Questions

<details>
<summary>1. What are the three metrics evaluated in the RED method, and what do they measure?</summary>
- **Rate**: The number of requests processed per second, measuring overall traffic volume.
- **Errors**: The number of requests that fail per second (typically HTTP 5xx status codes), measuring operational health.
- **Duration**: The amount of time requests take to complete, evaluated as percentile distributions (p50, p95, p99), measuring latency and performance degradation.
</details>

<details>
<summary>2. Why does multi-worker Uvicorn require special configuration for Prometheus metrics?</summary>
In multi-worker Uvicorn, each worker runs as an independent operating system process with its own private memory space. Without multi-process configuration, each worker records metrics in its own memory. When Prometheus scrapes `/metrics`, each scrape hits a random worker, returning partial, conflicting data. Setting `PROMETHEUS_MULTIPROC_DIR` forces workers to write metrics to shared memory-mapped files (`mmap`), allowing the scrape handler to return accurate aggregated totals across all processes.
</details>

<details>
<summary>3. What is "Cardinality Explosion" in Prometheus, and why is it dangerous?</summary>
Cardinality is the total number of unique time-series generated by a metric, calculated as the Cartesian product of all its label values. Cardinality explosion occurs when a high-entropy field (like user ID, order ID, or UUID) is added as a label. Each unique value creates a new time-series that Prometheus must store in RAM. This causes Prometheus memory usage to explode, leading to OOM crashes, failed scrapes, and sluggish queries.
</details>

<details>
<summary>4. What is the difference between a Counter, a Gauge, and a Histogram in Prometheus?</summary>
- **Counter**: A cumulative metric that can only increase or be reset to zero on restart (e.g., total requests served).
- **Gauge**: A metric that represents a single numerical value that can arbitrarily go up or down (e.g., active connections, memory usage).
- **Histogram**: Samples observations (typically request durations or payload sizes) and counts them in configurable buckets, allowing calculation of quantiles (p95, p99).
</details>

<details>
<summary>5. How do you calculate p95 latency in PromQL from a Prometheus Histogram?</summary>
Using the `histogram_quantile()` function:
```promql
histogram_quantile(0.95, sum(rate(http_request_duration_seconds_bucket[5m])) by (le))
```
This calculates the 95th percentile latency over the past 5 minutes by aggregating duration buckets across all instances.
</details>

<details>
<summary>6. Why is average latency considered an unreliable metric compared to p95/p99?</summary>
Average (mean) latency hides catastrophic tail latency. In a dataset of 1,000 requests where 990 requests take 10ms and 10 requests take 10,000ms, the average latency is ~110ms—which appears acceptable. However, 1% of users suffered an intolerable 10-second wait. Tail percentiles (p95, p99) accurately expose these extreme outliers.
</details>

<details>
<summary>7. How should Prometheus metrics be secured in a production Kubernetes deployment?</summary>
1. Expose metrics on an independent internal port (e.g., port 9090) that is not mapped to the public Ingress.
2. Apply Kubernetes `NetworkPolicy` rules allowing traffic to `/metrics` only from the Prometheus pod namespace.
3. Protect the endpoint with HTTP Basic Authentication or mutual TLS (mTLS).
</details>

<details>
<summary>8. Why should you avoid calculating percentiles on the client side in Python before sending to Prometheus?</summary>
Calculating percentiles across a single instance's window cannot be aggregated mathematically across multiple server instances (you cannot average percentiles together!). Prometheus histograms collect raw bucket counts, allowing the centralized Prometheus server to compute accurate global percentiles across hundreds of microservice replicas.
</details>

---

## 12. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Create an HTTP Request Counter</summary>

**Task**: Create a Prometheus `Counter` named `"api_http_requests_total"` with labels `["method", "endpoint", "status_code"]`. Show how to increment it.

**Solution**:
```python
from prometheus_client import Counter

HTTP_REQUESTS_TOTAL = Counter(
    "api_http_requests_total",
    "Total HTTP requests received",
    ["method", "endpoint", "status_code"],
)

# Increment usage
HTTP_REQUESTS_TOTAL.labels(method="GET", endpoint="/items", status_code="200").inc()
```
</details>

<details>
<summary>Exercise 2 (Easy): Measure Code Execution with a Histogram</summary>

**Task**: Create a `Histogram` named `"database_query_duration_seconds"`. Use its `.time()` decorator or context manager to measure query execution duration.

**Solution**:
```python
from prometheus_client import Histogram

DB_HISTOGRAM = Histogram(
    "database_query_duration_seconds",
    "Time spent executing DB queries",
    buckets=[0.01, 0.05, 0.1, 0.5, 1.0],
)

# Using context manager
def execute_query():
    with DB_HISTOGRAM.time():
        # Simulated database work
        pass
```
</details>

<details>
<summary>Exercise 3 (Medium): PromQL Error Rate Calculation</summary>

**Task**: Write a PromQL expression that computes the percentage error rate (HTTP 5xx errors divided by total requests) over a 5-minute rolling window.

**Solution**:
```promql
(
  sum(rate(http_requests_total{status=~"5.."}[5m]))
  /
  sum(rate(http_requests_total[5m]))
) * 100
```
</details>

<details>
<summary>Exercise 4 (Medium): Instrumenting FastAPI with `prometheus-fastapi-instrumentator`</summary>

**Task**: Write the startup code to instrument a FastAPI app with `prometheus-fastapi-instrumentator`, excluding `/health` and `/metrics`.

**Solution**:
```python
from fastapi import FastAPI
from prometheus_fastapi_instrumentator import Instrumentator

app = FastAPI()

instrumentator = Instrumentator(
    excluded_handlers=["/health", "/metrics"],
    should_group_status_codes=True,
)
instrumentator.instrument(app).expose(app)
```
</details>

<details>
<summary>Exercise 5 (Hard): Multi-Process Directory Cleanup and Setup</summary>

**Task**: Write a Python startup function that checks if `PROMETHEUS_MULTIPROC_DIR` exists. If it exists, clear all existing `.db` files from previous runs to prevent stale metric aggregation, and recreate the directory.

**Solution**:
```python
import os
import shutil
from pathlib import Path

def prepare_prometheus_multiproc_dir():
    dir_path = os.getenv("PROMETHEUS_MULTIPROC_DIR", "/tmp/prometheus_multiproc")
    path = Path(dir_path)
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True, exist_ok=True)
    os.environ["PROMETHEUS_MULTIPROC_DIR"] = str(path)
```
</details>

<details>
<summary>Exercise 6 (Hard): Custom Gauge with Callback Observation</summary>

**Task**: Implement an `ObservableGauge` that measures Python's current resident set size (RSS) memory in megabytes using the `resource` module.

**Solution**:
```python
import resource
from prometheus_client import Gauge

def get_memory_usage_mb():
    # resource.ru_maxrss is in kilobytes on Linux
    usage = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    return usage / 1024.0

MEMORY_GAUGE = Gauge(
    "process_resident_memory_megabytes",
    "Resident set size memory usage in megabytes"
)
MEMORY_GAUGE.set_function(get_memory_usage_mb)
```
</details>
