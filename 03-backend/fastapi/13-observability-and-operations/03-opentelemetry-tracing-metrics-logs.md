---
title: "OpenTelemetry Tracing, Metrics, and Logs in FastAPI"
description: "Master distributed observability in FastAPI using OpenTelemetry (OTel): auto-instrumentation, custom trace spans, metric counters/histograms, OTLP exporters, and sampling strategies."
fastapi_version: "0.115.x+"
python_version: "3.12+"
pydantic_version: "v2.10+"
tested_with: "opentelemetry-api 1.28+, opentelemetry-sdk 1.28+, opentelemetry-instrumentation-fastapi 0.49+, fastapi 0.115+, pytest 8.3+"
free_threading_status: "Fully Compatible"
---

# OpenTelemetry Tracing, Metrics, and Logs in FastAPI

## 1. Conceptual Foundation & Mental Model

### The Three Pillars of Observability
Modern enterprise systems rely on three interrelated telemetry signals to understand system behavior from the outside:
1. **Metrics**: Aggregatable numeric data measuring health and throughput over time (e.g., Requests Per Second, Error Rate, p95 Latency, Memory Usage).
2. **Traces**: The complete chronological journey of a request as it navigates through distributed microservices, databases, queues, and caches. A trace is composed of one or more **Spans**.
3. **Logs**: Contextual, timestamped records of specific discrete events emitted during execution.

### The OpenTelemetry (OTel) Standard
**OpenTelemetry** is the Cloud Native Computing Foundation (CNCF) open-source vendor-neutral standard for collecting, processing, and exporting telemetry data. By instrumenting FastAPI with OpenTelemetry, applications avoid vendor lock-in: the exact same application code can export data to Jaeger, Prometheus, Datadog, Honeycomb, Dynatrace, or AWS X-Ray simply by changing the OTLP (OpenTelemetry Protocol) export destination.

```
+-------------------------------------------------------------------------------+
|                       OPENTELEMETRY TRACE STRUCTURE                           |
|                                                                               |
|  [Root Span: HTTP GET /orders/checkout] (Total: 120ms)                        |
|  |                                                                            |
|  +---> [Child Span 1: AuthService.validate_jwt] (Latency: 5ms)               |
|  |                                                                            |
|  +---> [Child Span 2: Database.query_user_account] (Latency: 12ms)            |
|  |     - Attributes: db.system="postgresql", db.statement="SELECT * FROM..." |
|  |                                                                            |
|  +---> [Child Span 3: HTTP POST https://api.stripe.com/charge] (Latency: 85ms)|
|  |     - Attributes: http.status_code=200, peer.service="stripe"             |
|  |                                                                            |
|  +---> [Child Span 4: Kafka.publish_order_event] (Latency: 4ms)               |
+-------------------------------------------------------------------------------+
```

---

## 2. Architecture & Internal Mechanics

Understanding how the OpenTelemetry SDK instruments FastAPI endpoints, propagates context, and batches export data via background workers.

```
+-----------------------------------------------------------------------------+
|                           FASTAPI APPLICATION                               |
|                                                                             |
|  [FastAPIInstrumentor] (Auto-instruments incoming HTTP routes)              |
|           |                                                                 |
|           v (Creates Root Span)                                             |
|  [Tracer: tracer.start_as_current_span("business_logic")]                   |
|           |                                                                 |
|           v (Attaches Attributes, Events, Status)                           |
|  [BatchSpanProcessor] (In-Memory Ring Buffer: 2048 spans)                   |
+-----------------------------------------------------------------------------+
                                     |
                         Non-blocking Background Thread
                                     | (Protobuf over gRPC / HTTP OTLP)
                                     v
+-----------------------------------------------------------------------------+
|                        OPENTELEMETRY COLLECTOR (Agent)                      |
|  - Ingestion -> Batching -> Filtering / Sampling -> Exporters               |
+-----------------------------------------------------------------------------+
                                     |
               +---------------------+---------------------+
               |                                           |
               v                                           v
      [Grafana Tempo / Jaeger]                   [Prometheus / Mimir]
      (Distributed Traces)                       (RED Metrics)
```

### Sampling Strategies
In high-throughput systems processing 50,000 RPS, exporting 100% of traces consumes massive network bandwidth and storage costs. OTel provides flexible sampling:
- **AlwaysOn / AlwaysOff**: Sample everything (dev) or nothing.
- **TraceIdRatioBased (Probabilistic)**: Deterministically samples a configured percentage (e.g., 5% of all traces) using consistent hashing on the `trace_id`.
- **ParentBased**: Respects upstream sampling decisions: if the client or API gateway sampled the request, downstream services sample it too.

---

## 3. Comprehensive Syntax & Core Mechanics

### Configuring the OpenTelemetry SDK in FastAPI

```python
from opentelemetry import trace, metrics
from opentelemetry.sdk.trace import TracerProvider
from opentelemetry.sdk.trace.export import BatchSpanProcessor, ConsoleSpanExporter
from opentelemetry.sdk.resources import Resource
from opentelemetry.instrumentation.fastapi import FastAPIInstrumentor
from fastapi import FastAPI

def setup_opentelemetry(app: FastAPI, service_name: str = "order-service"):
    # 1. Define immutable service resource metadata
    resource = Resource.create({"service.name": service_name, "service.version": "1.2.0"})

    # 2. Configure Tracer Provider & Batch Processor
    tracer_provider = TracerProvider(resource=resource)
    # In production: replace ConsoleSpanExporter with OTLPSpanExporter
    span_processor = BatchSpanProcessor(ConsoleSpanExporter())
    tracer_provider.add_span_processor(span_processor)
    trace.set_tracer_provider(tracer_provider)

    # 3. Auto-instrument FastAPI
    FastAPIInstrumentor.instrument_app(
        app,
        tracer_provider=tracer_provider,
        excluded_urls="healthz,metrics",  # Do not trace health probes!
    )
```

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: Full-Featured OpenTelemetry Production Setup with OTLP
Configuring OTel with standard environment variables (`OTEL_EXPORTER_OTLP_ENDPOINT`) and W3C trace propagation.

```python
# app/core/telemetry.py
import os
from fastapi import FastAPI
from opentelemetry import trace, metrics
from opentelemetry.sdk.resources import Resource
from opentelemetry.sdk.trace import TracerProvider
from opentelemetry.sdk.trace.export import BatchSpanProcessor
from opentelemetry.exporter.otlp.proto.grpc.trace_exporter import OTLPSpanExporter
from opentelemetry.instrumentation.fastapi import FastAPIInstrumentor
from opentelemetry.instrumentation.sqlalchemy import SQLAlchemyInstrumentor
from opentelemetry.instrumentation.httpx import HTTPXClientInstrumentor

def init_telemetry(app: FastAPI) -> None:
    service_name = os.getenv("OTEL_SERVICE_NAME", "fastapi-core-service")
    otlp_endpoint = os.getenv("OTEL_EXPORTER_OTLP_ENDPOINT", "http://localhost:4317")

    # Resource definition
    resource = Resource.create({
        "service.name": service_name,
        "deployment.environment": os.getenv("ENVIRONMENT", "production"),
    })

    # Tracer Setup
    provider = TracerProvider(resource=resource)
    otlp_exporter = OTLPSpanExporter(endpoint=otlp_endpoint, insecure=True)
    provider.add_span_processor(BatchSpanProcessor(otlp_exporter))
    trace.set_tracer_provider(provider)

    # Auto-instrumentations
    FastAPIInstrumentor.instrument_app(app, tracer_provider=provider, excluded_urls="/healthz,/metrics")
    HTTPXClientInstrumentor().instrument()
    # SQLAlchemyInstrumentor().instrument(engine=engine.sync_engine)
```

---

### Example 2: Manual Tracing and Custom Span Creation
Creating fine-grained child spans, recording domain exceptions, and setting business attributes.

```python
# app/services/inventory_service.py
from opentelemetry import trace
from opentelemetry.trace import Status, StatusCode

tracer = trace.get_tracer("app.inventory.service")

class InventoryService:
    async def reserve_stock(self, order_id: str, sku: str, quantity: int) -> bool:
        # Create a custom named child span within the current active request trace
        with tracer.start_as_current_span("inventory.reserve_stock") as span:
            # Attach structured business attributes to the span
            span.set_attribute("order.id", order_id)
            span.set_attribute("item.sku", sku)
            span.set_attribute("item.quantity", quantity)

            if sku == "OUT_OF_STOCK":
                # Record error and set span status explicitly
                span.set_status(Status(StatusCode.ERROR, description="SKU unavailable"))
                span.add_event(
                    name="inventory_check_failed",
                    attributes={"reason": "stock_depleted", "attempted_qty": quantity}
                )
                return False

            # Record in-span event (milestone)
            span.add_event("stock_allocated_in_warehouse")
            span.set_status(Status(StatusCode.OK))
            return True
```

---

### Example 3: Custom Metrics (Counters, Histograms, Gauges)
Tracking operational business metrics using the OpenTelemetry Metrics API.

```python
# app/core/metrics.py
from opentelemetry import metrics

meter = metrics.get_meter("app.orders.metrics")

# 1. Counter: Measures total orders placed over time
orders_counter = meter.create_counter(
    name="orders_total",
    description="Total count of completed order requests",
    unit="1",
)

# 2. Histogram: Measures distribution of order values (p50, p95, p99)
order_value_histogram = meter.create_histogram(
    name="order_value_usd",
    description="Distribution of order checkout values in USD",
    unit="USD",
)

# 3. Observable Gauge: Tracks active in-memory connections
active_connections = 0
def get_active_connections(options):
    yield metrics.Observation(active_connections, {"server.id": "worker_1"})

meter.create_observable_gauge(
    name="active_websocket_connections",
    callbacks=[get_active_connections],
    description="Current count of active WebSocket sessions",
)
```

---

### Example 4: Propagating OpenTelemetry Context (Baggage)
Passing cross-cutting user metadata (e.g., `user_tier`) across microservices using OpenTelemetry Baggage.

```python
# app/api/v1/checkout.py
from fastapi import APIRouter, Header, Depends
from opentelemetry import trace, baggage
from app.services.inventory_service import InventoryService
from app.core.metrics import orders_counter, order_value_histogram

router = APIRouter(prefix="/checkout", tags=["Checkout"])

@router.post("/{order_id}")
async def checkout_endpoint(
    order_id: str,
    sku: str,
    amount: float,
    x_user_tier: str = Header(default="standard"),
):
    # Set OpenTelemetry Baggage (propagates automatically via HTTP headers to downstream services)
    ctx = baggage.set_baggage("user.tier", x_user_tier)

    service = InventoryService()
    success = await service.reserve_stock(order_id, sku, quantity=1)

    if success:
        # Record metrics with dimensional labels
        orders_counter.add(1, {"user.tier": x_user_tier, "status": "success"})
        order_value_histogram.record(amount, {"currency": "USD"})
        return {"status": "SUCCESS", "order_id": order_id}

    orders_counter.add(1, {"user.tier": x_user_tier, "status": "failed"})
    return {"status": "FAILED", "reason": "out_of_stock"}
```

---

### Example 5: Testing Spans in Pytest with In-Memory Exporter
Validating trace spans, attributes, and statuses in unit tests without external collectors.

```python
# tests/test_telemetry.py
import pytest
from opentelemetry import trace
from opentelemetry.sdk.trace import TracerProvider
from opentelemetry.sdk.trace.export import SimpleSpanProcessor
from opentelemetry.sdk.trace.export.in_memory_span_exporter import InMemorySpanExporter
from app.services.inventory_service import InventoryService

@pytest.fixture
def in_memory_tracer():
    exporter = InMemorySpanExporter()
    provider = TracerProvider()
    provider.add_span_processor(SimpleSpanProcessor(exporter))
    trace.set_tracer_provider(provider)
    return exporter

@pytest.mark.asyncio
async def test_inventory_span_recorded(in_memory_tracer):
    service = InventoryService()
    result = await service.reserve_stock("ord_42", "SKU_VALID", 3)

    assert result is True
    spans = in_memory_tracer.get_finished_spans()
    assert len(spans) == 1

    span = spans[0]
    assert span.name == "inventory.reserve_stock"
    assert span.attributes["order.id"] == "ord_42"
    assert span.attributes["item.sku"] == "SKU_VALID"
    assert span.attributes["item.quantity"] == 3
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Tracing Health Probes (`/healthz`)**: Failing to exclude health check endpoints from auto-instrumentation generates thousands of useless spans per minute from Kubernetes probes, flooding storage and skewing latency metrics. Always pass `excluded_urls="healthz,readyz,metrics"` to `FastAPIInstrumentor`.
2. **Synchronous Exporter in Event Loop (`SimpleSpanProcessor`)**: Using `SimpleSpanProcessor` in production sends spans over HTTP/gRPC synchronously on the main thread during request execution. Always use `BatchSpanProcessor`, which buffers spans and exports them in the background.
3. **Cardinality Explosion in Metrics**: Adding high-cardinality attributes (such as `user_id`, `email`, or `uuid`) to Prometheus or OpenTelemetry metric labels. Storing 1,000,000 distinct user IDs in metric labels consumes gigabytes of memory on the collector and crashes Prometheus. Keep metric dimensions strictly bounded (e.g., `status_code`, `method`, `tier`).
4. **Context Loss in Manual Async Tasks**: Launching uninstrumented background tasks with `asyncio.create_task()` can detach the span context. Use `opentelemetry.context.attach()` or pass the active context explicitly.

---

## 6. Common Mistakes Table

| Mistake | Root Cause | Impact | Correct Approach |
| :--- | :--- | :--- | :--- |
| **Using `SimpleSpanProcessor` in Production** | Copying quickstart tutorial code | Serializes export I/O; adds 50ms latency to every API request. | Always use `BatchSpanProcessor` in production. |
| **Adding User IDs to Metric Labels** | High-cardinality label mistake | Metric database out-of-memory crash (Prometheus cardinality explosion). | Put `user_id` in Trace attributes and Logs, NEVER in Metric labels. |
| **Tracing Health Probes** | Forgetting `excluded_urls` | 80% of storage wasted on Kubernetes readiness probe traces. | Configure `excluded_urls="healthz,metrics"`. |
| **Swallowing Exceptions Without Setting Status** | Catching exceptions inside a span | Span appears green/successful in Jaeger despite failed operation. | Call `span.record_exception(exc)` and `span.set_status(Status(StatusCode.ERROR))`. |
| **Hardcoding Collector Endpoints** | Hardcoding `localhost:4317` | Fails when deployed across Kubernetes staging/production clusters. | Read from `OTEL_EXPORTER_OTLP_ENDPOINT` environment variable. |
| **Using Cryptographic Sampling on Every Request** | Over-complex sampling rules | CPU overhead on request parsing. | Use standard `TraceIdRatioBased` or `ParentBased` samplers. |

---

## 7. Performance & Scalability Considerations

- **BatchSpanProcessor Sizing**: Configure `max_queue_size=2048`, `max_export_batch_size=512`, and `schedule_delay_millis=5000` to prevent memory accumulation while avoiding frequent network calls.
- **Header Parsing Overhead**: Extracting W3C traceparent headers adds less than 5 microseconds to request execution.
- **Python 3.13+ Free-Threading**: The background batch span processor runs on a dedicated native OS thread, offloading telemetry serialization without competing with the main event loop for GIL locks.

---

## 8. Security & OWASP Guardrails

- **Sanitizing SQL Query Spans**: Auto-instrumentation libraries for databases (e.g., `opentelemetry-instrumentation-sqlalchemy`) should sanitize parameterized queries (`WHERE password = ?`) to ensure plain-text passwords or credit card data never appear in trace spans.
- **Data Redaction in Attributes**: Never set raw tokens, social security numbers, or cryptographic keys on span attributes. Spans are visible to all engineers with tracing access.

---

## 9. Testing Strategies & Pytest Recipes

```python
# tests/test_span_error_recording.py
import pytest
from opentelemetry.trace import StatusCode
from app.services.inventory_service import InventoryService

@pytest.mark.asyncio
async def test_inventory_failure_sets_error_status(in_memory_tracer):
    service = InventoryService()
    result = await service.reserve_stock("ord_99", "OUT_OF_STOCK", 1)

    assert result is False
    spans = in_memory_tracer.get_finished_spans()
    assert len(spans) == 1
    span = spans[0]
    assert span.status.status_code == StatusCode.ERROR
    assert span.status.description == "SKU unavailable"
    assert len(span.events) == 1
    assert span.events[0].name == "inventory_check_failed"
```

---

## 10. Real-World Case Study / Production Incident

An international food delivery service experienced an unexplained latency surge: every Friday night at 8 PM, p99 checkout response times jumped from 150ms to 12,000ms.

**The Post-Mortem Using OpenTelemetry**:
1. Engineers inspected the distributed trace for a 12-second request in Jaeger.
2. The root span (`POST /checkout`) contained 4 child spans.
3. Spans for payment (`stripe.charge`) and database insert were each under 30ms.
4. However, a child span named `promo_code.validate` was taking 11,800ms.
5. Drilling into the span attributes revealed:
   - `db.system`: `"redis"`
   - `redis.command`: `"KEYS promo:*"`
6. A newly hired developer had used the blocking `KEYS` command instead of `SCAN` to look up promotion rules. Because there were 800,000 active promotion keys, Redis paused for 11 seconds on every single checkout.

Without distributed tracing, isolating this sub-millisecond database command across 30 microservices would have taken days of speculation. With OpenTelemetry, the root cause was identified in 90 seconds.

---

## 11. Technical Interview Questions

<details>
<summary>1. What is the difference between OpenTelemetry Traces, Metrics, and Logs?</summary>
- **Metrics** are numerical measurements aggregated over time (e.g., requests per second, CPU usage, latency percentiles) used for alerting and dashboards.
- **Traces** record the complete end-to-end journey of a single request across distributed services, composed of timed, nested Spans showing where latency is spent.
- **Logs** are structured text/JSON records of specific discrete events occurring during execution.
</details>

<details>
<summary>2. Why should health checks (`/healthz`) and metric endpoints (`/metrics`) be excluded from OpenTelemetry tracing?</summary>
Kubernetes and load balancers ping health probes every 1–5 seconds. Tracing these requests generates massive volumes of identical, low-value spans that flood storage, consume network bandwidth, increase cloud ingestion costs, and artificially lower average latency metrics.
</details>

<details>
<summary>3. What is "Cardinality Explosion" in metrics, and how do you prevent it?</summary>
Cardinality refers to the total number of unique label-value combinations for a metric. Cardinality explosion occurs when high-entropy attributes (like user IDs, email addresses, or UUIDs) are used as metric labels. Each unique combination creates a new time series in memory. Storing 100,000 user IDs creates 100,000 time series, causing Prometheus or the OTel Collector to run out of memory and crash. High-entropy data belongs in Traces or Logs, never in Metric labels.
</details>

<details>
<summary>4. What is the difference between `SimpleSpanProcessor` and `BatchSpanProcessor`?</summary>
`SimpleSpanProcessor` exports each span immediately and synchronously as soon as it ends. In an API, this blocks the request thread on network I/O to the collector, adding tens of milliseconds to user latency. `BatchSpanProcessor` places finished spans into an in-memory buffer and exports them asynchronously in batches on a background thread, having virtually zero impact on request latency.
</details>

<details>
<summary>5. What is the W3C TraceContext standard?</summary>
The W3C TraceContext specification standardizes the HTTP headers used for distributed trace propagation across vendors. It defines the `traceparent` header (combining version, 128-bit trace ID, 64-bit parent span ID, and trace flags) and the `tracestate` header (for vendor-specific routing metadata), ensuring traces continue seamlessly across heterogeneous microservices.
</details>

<details>
<summary>6. How does OpenTelemetry Baggage differ from Span Attributes?</summary>
Span Attributes are local to the specific span where they are defined and are not passed downstream. Baggage is a key-value store that travels along with the distributed context across network boundaries (via the `baggage` HTTP header), making contextual metadata (like `user_tier: enterprise`) available to all downstream microservices.
</details>

<details>
<summary>7. What is Tail-Based Sampling?</summary>
In Head-Based sampling, the decision to sample a trace is made at the very beginning of the request (e.g., 5% random chance). If an error occurs on an unsampled request, the trace is lost. In Tail-Based sampling, an OpenTelemetry Collector buffers all spans until the trace completes; it samples 100% of traces that contain errors or latency > 1,000ms, and only 1% of normal healthy traces, maximizing diagnostic value while minimizing storage.
</details>

<details>
<summary>8. How do you manually mark a span as failed in OpenTelemetry?</summary>
By setting its status to `ERROR` and optionally recording the exception:
```python
span.record_exception(exc)
span.set_status(Status(StatusCode.ERROR, description="Database connection failed"))
```
</details>

---

## 12. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Basic Custom Span</summary>

**Task**: Write an async function `compute_hash(data: str) -> str` that wraps its execution in a custom trace span named `"crypto.hash"` and sets an attribute `"data.length"`.

**Solution**:
```python
import hashlib
from opentelemetry import trace

tracer = trace.get_tracer("test.crypto")

async def compute_hash(data: str) -> str:
    with tracer.start_as_current_span("crypto.hash") as span:
        span.set_attribute("data.length", len(data))
        return hashlib.sha256(data.encode()).hexdigest()
```
</details>

<details>
<summary>Exercise 2 (Easy): Create a Prometheus-Compatible Metric Counter</summary>

**Task**: Create an OpenTelemetry metric counter named `"login_attempts_total"` with description `"Total login attempts"`. Increment it with attribute `{"status": "success"}`.

**Solution**:
```python
from opentelemetry import metrics

meter = metrics.get_meter("auth.metrics")
login_counter = meter.create_counter(
    name="login_attempts_total",
    description="Total login attempts",
    unit="1",
)

# Increment
login_counter.add(1, {"status": "success"})
```
</details>

<details>
<summary>Exercise 3 (Medium): Add Custom Event to Active Span</summary>

**Task**: Write an endpoint that extracts the active span and logs an in-span event `"cache_miss"` with attribute `{"key": "user_42"}`.

**Solution**:
```python
from fastapi import APIRouter
from opentelemetry import trace

router = APIRouter()

@router.get("/user/{user_id}")
async def get_user(user_id: str):
    current_span = trace.get_current_span()
    if current_span.is_recording():
        current_span.add_event("cache_miss", {"key": f"user_{user_id}"})
    return {"user_id": user_id}
```
</details>

<details>
<summary>Exercise 4 (Medium): Instrumenting httpx Client</summary>

**Task**: Demonstrate how to instrument `httpx` with `HTTPXClientInstrumentor` so all outbound HTTP requests automatically inject W3C trace headers.

**Solution**:
```python
from opentelemetry.instrumentation.httpx import HTTPXClientInstrumentor
import httpx

# Instrument globally once at startup
HTTPXClientInstrumentor().instrument()

# All subsequent httpx calls automatically forward active trace headers!
async def call_external():
    async with httpx.AsyncClient() as client:
        return await client.get("https://httpbin.org/headers")
```
</details>

<details>
<summary>Exercise 5 (Hard): In-Memory Span Verification Test</summary>

**Task**: Write a complete test using `InMemorySpanExporter` verifying that an async function `process_order(order_id)` records a span with attribute `order_id` and status `OK`.

**Solution**:
```python
import pytest
from opentelemetry import trace
from opentelemetry.sdk.trace import TracerProvider
from opentelemetry.sdk.trace.export import SimpleSpanProcessor
from opentelemetry.sdk.trace.export.in_memory_span_exporter import InMemorySpanExporter
from opentelemetry.trace import Status, StatusCode

@pytest.mark.asyncio
async def test_order_span_status():
    exporter = InMemorySpanExporter()
    provider = TracerProvider()
    provider.add_span_processor(SimpleSpanProcessor(exporter))
    tracer = provider.get_tracer("test")

    with tracer.start_as_current_span("process_order") as span:
        span.set_attribute("order_id", "ord_100")
        span.set_status(Status(StatusCode.OK))

    spans = exporter.get_finished_spans()
    assert len(spans) == 1
    assert spans[0].name == "process_order"
    assert spans[0].attributes["order_id"] == "ord_100"
    assert spans[0].status.status_code == StatusCode.OK
```
</details>

<details>
<summary>Exercise 6 (Hard): Baggage Injection and Extraction</summary>

**Task**: Write a function that sets OpenTelemetry Baggage `tenant_id="tenant_alpha"`, and a downstream function that extracts and asserts the baggage value.

**Solution**:
```python
from opentelemetry import baggage

def set_tenant_baggage(tenant_id: str):
    return baggage.set_baggage("tenant_id", tenant_id)

def get_tenant_baggage() -> str | None:
    return baggage.get_baggage("tenant_id")

# Verification
token = set_tenant_baggage("tenant_alpha")
assert get_tenant_baggage() == "tenant_alpha"
```
</details>
