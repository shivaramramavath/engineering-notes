---
title: "Load and Performance Testing FastAPI with Locust and k6"
description: "Design realistic load, stress, spike, and soak testing scenarios for FastAPI using Python-native Locust and high-concurrency k6, analyzing p95/p99 latencies, concurrency bottlenecks, and ASGI event loop saturation."
fastapi_version: "0.115.x+"
python_version: "3.12+"
pydantic_version: "v2.10+"
tested_with: "locust 2.32+, k6 0.54+, uvicorn 0.32+"
free_threading_status: "Fully Compatible"
---

# Load and Performance Testing FastAPI with Locust and k6

## 1. Conceptual Foundation & Mental Model

### Testing Beyond Happy Paths Under Concurrency
Functional unit and integration tests evaluate whether an endpoint behaves correctly in isolation with 1 concurrent client. However, production systems fail under the weight of **concurrency, resource contention, pool exhaustion, and event loop starvation**. A FastAPI endpoint that responds in 12ms under single-user tests can degrade to 4,500ms or fail completely with HTTP 504/500 errors when subjected to 1,000 concurrent requests.

Load testing validates three critical system attributes:
1. **Throughput (Requests Per Second / RPS)**: The maximum sustainable request processing capacity before latencies degrade.
2. **Latency Distribution (p50, p95, p99)**: The tail latencies experienced by the slowest 5% and 1% of requests. A low average latency frequently masks catastrophic tail degradation caused by database connection pool waits or GIL contention.
3. **Saturation Point & Error Threshold**: The exact concurrency level at which memory consumption spikes, worker processes freeze, or socket timeouts occur.

```
       Load Profile Types
       ------------------
1. Constant Load:      |‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾| (Baseline benchmarking)
2. Ramp-Up (Stress):   |         /‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾| (Finding breakpoint)
3. Spike Testing:      |    /\           /\                  | (Flash sales / bursts)
4. Soak / Endurance:   |‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾| (Memory leaks over 24h)
```

### Locust vs. k6: The Testing Tool Dualism
- **Locust**: A Python-native distributed load testing tool. Test scenarios are written as standard Python classes, enabling direct code reuse (such as importing Pydantic models, auth helpers, or custom signing libraries). Ideal for complex stateful user workflows and rapid iteration within Python engineering teams.
- **k6 (by Grafana)**: A high-performance load testing engine written in Go, executing test scripts written in JavaScript. Because the core engine runs as native Go routines with minimal memory overhead, a single k6 instance can generate 30,000+ RPS without saturating the load injector's CPU, making it the industry standard for extreme volume and headless CI/CD performance gates.

---

## 2. Architecture & Internal Mechanics

Understanding how load injectors interact with FastAPI's ASGI event loop and worker process architecture is vital for accurate test diagnosis.

```
+-------------------------------------------------------------------------+
|                        LOAD INJECTOR TIERS                              |
|  [Locust Master] <---> [Locust Workers (Python/gevent)]                 |
|  OR [k6 Runner (Go Routines / JS V8 Contexts)]                          |
+-------------------------------------------------------------------------+
                                    |
                        HTTP/1.1 or HTTP/2 Pipelines
                                    v
+-------------------------------------------------------------------------+
|                     REVERSE PROXY / INGRESS                             |
|  [Nginx / Traefik / Envoy] (Keep-Alive Pools, SSL Termination)          |
+-------------------------------------------------------------------------+
                                    |
                    Unix Sockets or Local TCP Sockets
                                    v
+-------------------------------------------------------------------------+
|                  UVICORN ASGI SERVER ARCHITECTURE                       |
|  Worker 1 (PID 101)                Worker 2 (PID 102)                   |
|  +----------------------------+    +----------------------------+       |
|  | asyncio Event Loop (uvloop)|    | asyncio Event Loop (uvloop)|       |
|  | +------------------------+ |    | +------------------------+ |       |
|  | | Async Endpoints        | |    | | Async Endpoints        | |       |
|  | +------------------------+ |    | +------------------------+ |       |
|  | +------------------------+ |    | +------------------------+ |       |
|  | | anyio Threadpool (Sync)| |    | | anyio Threadpool (Sync)| |       |
|  | +------------------------+ |    | +------------------------+ |       |
|  | +------------------------+ |    | +------------------------+ |       |
|  | | DB Asyncpg Pool (Max 20)| |    | | DB Asyncpg Pool (Max 20)| |       |
|  | +------------------------+ |    | +------------------------+ |       |
|  +----------------------------+    +----------------------------+       |
+-------------------------------------------------------------------------+
```

### The Anatomy of an Event Loop Bottleneck Under Load
When load ramps up against a FastAPI application:
1. **Network I/O Saturation**: If keep-alive connections are disabled on the load generator, thousands of ephemeral TCP sockets enter `TIME_WAIT`, exhausting the operating system's port range.
2. **Database Connection Pool Starvation**: If Uvicorn runs 4 workers, each configured with an SQLAlchemy connection pool of `max_overflow=10, pool_size=20`, the application can open up to 120 concurrent connections. If 500 concurrent requests arrive demanding database access, 380 requests block in memory awaiting an available connection.
3. **Blocking Synchronous Calls**: If an endpoint defined with `async def` performs a synchronous file read or synchronous `requests.get()`, it halts the entire single-threaded `uvloop` for all other concurrent requests handled by that worker. p99 latencies skyrocket from milliseconds to multi-second timeouts.

---

## 3. Comprehensive Syntax & Core Mechanics

### Locust Core Abstractions
- `HttpUser`: Represents a simulated virtual user with an embedded HTTP client session (`self.client`).
- `task`: Decorator denoting a runnable user action. Supports task weightings (e.g., `@task(3)` runs 3 times more frequently than `@task(1)`).
- `between(min, max)`: Defines realistic user think times between tasks.
- `on_start()` / `on_stop()`: Lifecycle hooks for user login, session initialization, and logout cleanup.

```python
from locust import HttpUser, task, between

class StandardUser(HttpUser):
    wait_time = between(0.5, 2.0)
    
    def on_start(self):
        # Authenticate once per virtual user
        response = self.client.post("/auth/token", json={"username": "user", "password": "pwd"})
        self.client.headers["Authorization"] = f"Bearer {response.json()['token']}"

    @task(3)
    def view_feed(self):
        self.client.get("/api/v1/feed")

    @task(1)
    def post_comment(self):
        self.client.post("/api/v1/comments", json={"text": "Performance test payload"})
```

### k6 Core Abstractions
- `options`: Declares execution stages, targets, and latency threshold quality gates.
- `default function`: The main test iteration executed repeatedly by virtual users (VUs).
- `check`: Assertions on response status codes and body content without halting execution.
- `thresholds`: SLA enforcement rules (e.g., `http_req_duration: ['p(95)<250']`).

```javascript
import http from 'k6/http';
import { check, sleep } from 'k6';

export const options = {
  stages: [
    { duration: '30s', target: 50 },  // Ramp-up to 50 VUs
    { duration: '1m', target: 50 },   // Sustain 50 VUs
    { duration: '20s', target: 0 },   // Ramp-down
  ],
  thresholds: {
    http_req_failed: ['rate<0.01'],    // Error rate must be under 1%
    http_req_duration: ['p(95)<200'],  // 95% of requests must complete within 200ms
  },
};

export default function () {
  const res = http.get('http://localhost:8000/api/v1/items');
  check(res, { 'status is 200': (r) => r.status === 200 });
  sleep(1);
}
```

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: Target FastAPI Application Under Test
A realistic e-commerce API containing an async database-backed endpoint, a cacheable endpoint, and a background task.

```python
# app/main.py
import asyncio
from typing import Annotated
from fastapi import FastAPI, Depends, HTTPException, status, BackgroundTasks
from pydantic import BaseModel, Field

app = FastAPI(title="Load Test Target API")

class OrderCreate(BaseModel):
    item_id: int = Field(gt=0)
    quantity: int = Field(gt=0, le=100)

async def simulate_db_query():
    # Simulates a fast, indexed PostgreSQL query (5ms latency)
    await asyncio.sleep(0.005)
    return {"id": 1, "name": "Mechanical Keyboard", "stock": 42}

@app.get("/healthz")
async def health_check():
    """Minimal compute endpoint for testing baseline ASGI throughput."""
    return {"status": "healthy"}

@app.get("/items/{item_id}")
async def get_item(item_id: int):
    """Simulates a database read endpoint."""
    item = await simulate_db_query()
    return item

@app.post("/orders", status_code=status.HTTP_201_CREATED)
async def create_order(order: OrderCreate, background_tasks: BackgroundTasks):
    """Simulates an order submission with background processing."""
    background_tasks.add_task(asyncio.sleep, 0.05)  # Background analytics
    return {"order_id": 99912, "status": "processing", "quantity": order.quantity}
```

---

### Example 2: Production Locustfile with Custom Metrics and Sequential Workflows
Demonstrates realistic user behaviors, dynamic token injection, and custom failure reporting.

```python
# loadtests/locustfile.py
import random
from locust import HttpUser, SequentialTaskSet, task, between, events

class CheckoutWorkflow(SequentialTaskSet):
    """Simulates a sequential user browsing and purchasing flow."""
    
    @task
    def browse_catalog(self):
        with self.client.get("/items/1", catch_response=True) as resp:
            if resp.status_code == 200:
                resp.success()
            else:
                resp.failure(f"Failed to load item: status {resp.status_code}")

    @task
    def purchase_item(self):
        payload = {"item_id": 1, "quantity": random.randint(1, 3)}
        with self.client.post("/orders", json=payload, catch_response=True) as resp:
            if resp.status_code == 201:
                data = resp.json()
                if data.get("status") == "processing":
                    resp.success()
                else:
                    resp.failure("Invalid payload response structure")
            else:
                resp.failure(f"Order failed with HTTP {resp.status_code}")

class ECommerceShopper(HttpUser):
    wait_time = between(1.0, 3.0)
    tasks = [CheckoutWorkflow]

    def on_start(self):
        # Health check probe before starting workflow
        self.client.get("/healthz")
```

Run command for headless execution:
```bash
locust -f loadtests/locustfile.py --headless -u 200 -r 20 -t 5m --host http://localhost:8000 --html report.html
```

---

### Example 3: Production k6 Scenario with Strict SLAs and Spike Testing
A k6 test script modeling a sudden traffic spike to verify auto-scaling recovery and error rate limits.

```javascript
// loadtests/spike_test.js
import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate, Trend } from 'k6/metrics';

// Custom metrics
const errorRate = new Rate('custom_error_rate');
const orderDuration = new Trend('order_post_duration');

export const options = {
  scenarios: {
    spike_scenario: {
      executor: 'ramping-vus',
      startVUs: 10,
      stages: [
        { duration: '30s', target: 20 },   // Baseline steady state
        { duration: '10s', target: 500 },  // SUDDEN SPIKE: 500 concurrent users
        { duration: '1m', target: 500 },   // Maintain spike
        { duration: '20s', target: 20 },   // Scale down
        { duration: '30s', target: 0 },    // Cooldown
      ],
      gracefulRampDown: '10s',
    },
  },
  thresholds: {
    'http_req_failed': ['rate<0.02'],              // Error rate < 2% during spike
    'http_req_duration{status:200}': ['p(95)<300'], // 95% of item reads under 300ms
    'order_post_duration': ['p(99)<800'],           // 99% of orders under 800ms
  },
};

export default function () {
  const BASE_URL = __ENV.TARGET_URL || 'http://localhost:8000';

  // 1. Read item
  const readRes = http.get(`${BASE_URL}/items/1`);
  const readOk = check(readRes, {
    'item read is 200': (r) => r.status === 200,
  });
  errorRate.add(!readOk);

  // 2. Submit order
  const payload = JSON.stringify({ item_id: 1, quantity: 2 });
  const params = { headers: { 'Content-Type': 'application/json' } };
  
  const orderStart = Date.now();
  const orderRes = http.post(`${BASE_URL}/orders`, payload, params);
  orderDuration.add(Date.now() - orderStart);

  const orderOk = check(orderRes, {
    'order created is 201': (r) => r.status === 201,
  });
  errorRate.add(!orderOk);

  sleep(Math.random() * 2 + 1); // 1-3s think time
}
```

Run command:
```bash
k6 run -e TARGET_URL=http://localhost:8000 loadtests/spike_test.js
```

---

### Example 4: Diagnosing Event Loop Blocking During Load Testing
Demonstrating how a single synchronous blocking call destroys concurrency across all clients on the same worker process.

```python
# app/routes/unoptimized.py
import time
from fastapi import APIRouter

router = APIRouter()

@router.get("/bad-endpoint")
async def bad_endpoint():
    """FATAL: Using time.sleep inside async def blocks the entire Uvicorn event loop!"""
    time.sleep(0.5)  # 500ms synchronous block
    return {"status": "terrible"}

@router.get("/good-endpoint")
async def good_endpoint():
    """Correct: Offload CPU or synchronous I/O using anyio/asyncio or use non-blocking sleeps."""
    import anyio
    await anyio.to_thread.run_sync(time.sleep, 0.5)
    return {"status": "properly_offloaded"}
```

When 50 virtual users hit `/bad-endpoint` concurrently on a single Uvicorn worker:
- **Expected Throughput**: $\approx 2 \text{ RPS}$ (since 1 request takes 0.5s exclusively).
- **Latency**: Requests 40-50 experience 20–25 second timeouts.
- When switching to `/good-endpoint`, the default AnyIO threadpool (up to 40 threads) processes requests concurrently, scaling throughput to $\approx 80 \text{ RPS}$.

---

### Example 5: Automated Performance Regression Test in Pytest with k6
Integrating k6 CLI execution directly into an integration test suite to assert performance regressions in CI.

```python
# tests/test_performance_regression.py
import subprocess
import json
import pytest

@pytest.mark.performance
def test_k6_load_sla_compliance():
    """Executes k6 and validates that performance thresholds passed."""
    command = [
        "k6", "run",
        "--summary-export=k6_summary.json",
        "--quiet",
        "loadtests/spike_test.js"
    ]
    
    result = subprocess.run(command, capture_output=True, text=True)
    assert result.returncode == 0, f"k6 performance test failed: {result.stderr}"
    
    with open("k6_summary.json", "r") as f:
        summary = json.load(f)
        
    p95_duration = summary["metrics"]["http_req_duration"]["values"]["p(95)"]
    error_rate = summary["metrics"]["http_req_failed"]["values"]["rate"]
    
    print(f"\nFinal Performance Metrics: p95={p95_duration:.2f}ms, Error Rate={error_rate:.4f}")
    assert p95_duration < 300.0, f"SLA Violation: p95 latency was {p95_duration}ms (target: <300ms)"
    assert error_rate < 0.01, f"SLA Violation: Error rate was {error_rate * 100}% (target: <1%)"
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Ephemeral Port Exhaustion (TIME_WAIT)**: When load injectors open and close connections rapidly without HTTP Keep-Alive, the host OS exhausts available outbound ports (typically ~65,000). The test runner throws `Cannot assign requested address` errors, which is a test-harness failure, not a FastAPI bug.
2. **Coordinated Omission**: If a load testing tool pauses its request generator while waiting for slow responses from the server, it measures latency only for requests it successfully sent, completely omitting the requests that *would* have backed up in the queue. k6 and Locust mitigate this when configured with arrival-rate executors.
3. **Uvicorn Worker Starvation by CPU Workloads**: Running a CPU-intensive task (e.g., password hashing with bcrypt, image resizing) inside `async def` locks the GIL and the event loop. Always offload CPU-bound tasks to `concurrent.futures.ProcessPoolExecutor`.
4. **Database Connection Leaks Under Timeout**: When a client disconnects before a slow database query finishes, if the endpoint does not listen for `request.is_disconnected()`, the database query continues running to completion, keeping a pool connection locked unnecessarily.

---

## 6. Common Mistakes Table

| Mistake | Root Cause | Impact | Correct Approach |
| :--- | :--- | :--- | :--- |
| **Testing with a Single Uvicorn Worker** | Starting Uvicorn without `--workers` | Skews results; fails to measure inter-process connection pool limits and CPU saturation. | Run Uvicorn with `--workers $(nproc)` or match exact target production container spec. |
| **Running Load Injector on the Same Host as FastAPI** | Running `k6` or `locust` on localhost alongside the API | Load generator consumes 80% of CPU and socket buffers, artificially throttling FastAPI. | Run load injectors on dedicated machines or separate CI test worker nodes. |
| **Omitting Think Times (`sleep`)** | Virtual users sending back-to-back requests in a tight loop | Unrealistic traffic pattern; simulates a DoS attack rather than actual human or service behavior. | Introduce realistic randomized think times (`sleep(between(0.5, 2.0))`). |
| **Relying Exclusively on Average (Mean) Latency** | Looking only at the summary mean | Masks severe p95/p99 tail latency spikes caused by database lock contention or GC pauses. | Enforce explicit SLA thresholds on `p(95)` and `p(99)` latency metrics. |
| **Failing to Warm Up the Application** | Sending full peak load immediately at second 0 | Cold JIT, empty connection pools, and unprimed caches trigger false alarm initial failures. | Implement a 30–60 second gradual ramp-up stage to prime connection pools and caches. |
| **Using `time.sleep` in `async def` Endpoints** | Misunderstanding Python async mechanics | Blocks the entire event loop; serializes all concurrent requests to that worker process. | Use `await asyncio.sleep(...)` for async, or define endpoint as regular `def` so it runs in threadpool. |

---

## 7. Performance & Scalability Considerations

- **ASGI Event Loop Optimization (`uvloop`)**: Always verify that Uvicorn utilizes `uvloop` (standard on Linux/macOS) and `httptools`. `uvloop` provides a 2x-4x throughput improvement over the standard library asyncio event loop.
- **Connection Pooling (`pool_pre_ping` & Sizing)**: Ensure your async database engine pool size matches worker concurrency:
  $$\text{Total DB Connections} = \text{Uvicorn Workers} \times (\text{pool\_size} + \text{max\_overflow})$$
  Ensure this value does not exceed PostgreSQL's `max_connections`.
- **Python 3.13+ Free-Threading**: Under free-threaded Python (`python3.13t`), multi-threaded FastAPI workers can process pure CPU tasks across multiple cores without the GIL, significantly reducing p99 latency jitter under heavy load.

---

## 8. Security & OWASP Guardrails

- **OWASP A04 (Insecure Design & Denial of Service)**: Use load testing to determine the maximum request payload size and payload validation cost. Sending deeply nested 5MB JSON payloads can saturate CPU parsers; enforce `Content-Length` limits in middleware.
- **Rate Limiting Verification**: Validate that your Redis-backed rate limiting dependencies (e.g., `slowapi`) correctly reject excess traffic with HTTP 429 Too Many Requests under sustained spike loads without crashing Redis.

---

## 9. Testing Strategies & Pytest Recipes

```python
# tests/test_concurrency_stress.py
import asyncio
import httpx
import pytest

@pytest.mark.asyncio
async def test_concurrent_burst_integrity():
    """Direct async client burst test to verify race conditions in memory or DB."""
    async with httpx.AsyncClient(base_url="http://localhost:8000", timeout=10.0) as client:
        # Fire 50 concurrent requests simultaneously
        tasks = [client.get("/items/1") for _ in range(50)]
        responses = await asyncio.gather(*tasks, return_exceptions=True)
        
        # Verify no unhandled exceptions or 500s occurred
        status_codes = [r.status_code for r in responses if isinstance(r, httpx.Response)]
        assert len(status_codes) == 50
        assert all(code == 200 for code in status_codes), f"Encountered non-200 responses: {set(status_codes)}"
```

---

## 10. Real-World Case Study / Production Incident

During a promotional flash sale, an e-commerce FastAPI service deployed on Kubernetes experienced a cascading outage. Within 30 seconds of launch, p99 latencies jumped from 80ms to 32 seconds, followed by 502 Bad Gateway errors across the entire domain.

**Root Cause Analysis**:
1. The `/orders/checkout` endpoint was written as `async def checkout(...)`.
2. Inside the endpoint, a legacy order invoice generator was called: `generate_pdf_invoice(order)`. This was a CPU-bound, synchronous function taking 450ms.
3. Because the endpoint was `async def`, FastAPI did *not* offload it to the AnyIO worker threadpool. Instead, it ran directly on the main event loop thread.
4. Each 450ms CPU execution blocked Uvicorn from processing incoming I/O events, halting keep-alive acknowledgments and database reads across all concurrent users.
5. Ingress controllers timed out and terminated the connections.

**Remediation**:
The team moved the invoice generation to a background task queue (Celery/Redis) and load tested the fix using k6 with 1,000 virtual users. Peak throughput increased from 18 RPS to 1,450 RPS, and p99 latency stabilized at 120ms.

---

## 11. Technical Interview Questions

<details>
<summary>1. Why does an async endpoint using `time.sleep()` perform drastically worse than a synchronous `def` endpoint using `time.sleep()`?</summary>
In FastAPI, endpoints defined with standard `def` are automatically dispatched to an asynchronous threadpool (AnyIO worker pool), allowing other threads and the main event loop to continue running. When an endpoint is defined with `async def`, FastAPI executes it directly on the single-threaded asyncio event loop. Calling synchronous blocking functions like `time.sleep()` or synchronous database drivers halts the entire event loop, freezing all concurrent requests handled by that worker process.
</details>

<details>
<summary>2. What is "Coordinated Omission" in load testing, and how does it distort latency measurements?</summary>
Coordinated omission occurs when a load testing client waits for a slow response from the server before scheduling the next request. If the server stalls for 10 seconds, the load injector sends fewer requests during that window, artificially missing the queued requests that should have been recorded as suffering extreme latency. Tools with open-model or constant-arrival-rate executors generate requests according to an independent schedule, avoiding this distortion.
</details>

<details>
<summary>3. What is the difference between Locust and k6 in terms of resource utilization?</summary>
Locust runs on Python using gevent coroutines; each virtual user consumes Python memory and CPU time, meaning a single Locust master/worker node can typically drive between 1,000 and 3,000 virtual users before CPU saturation occurs. k6 is compiled in Go and executes lightweight goroutines with embedded JavaScript interpreters, allowing a single host to generate 30,000+ virtual users with significantly lower memory and CPU overhead.
</details>

<details>
<summary>4. What are the key metrics to evaluate in a load test report for a FastAPI service?</summary>
Key metrics include:
1. **p95 and p99 Latency**: Tail latencies indicating the worst-case user experience.
2. **Throughput (RPS)**: Sustainable requests per second.
3. **HTTP Error Rate**: Percentage of 5xx, 429, or network timeout responses.
4. **Host Saturation**: CPU utilization, memory consumption, open file descriptor counts, and database connection pool utilization.
</details>

<details>
<summary>5. How should database connection pools be configured relative to Uvicorn workers during load tests?</summary>
Each Uvicorn worker runs as an independent OS process with its own separate database connection pool. If you configure `pool_size=20` and `max_overflow=10` with 4 Uvicorn workers, your FastAPI service can open up to $4 \times (20 + 10) = 120$ connections to the database. The database server must be provisioned with a `max_connections` setting comfortably higher than this combined total.
</details>

<details>
<summary>6. How can you detect memory leaks in a FastAPI application using load testing?</summary>
Run a "Soak" or "Endurance" test: apply a moderate, constant load (e.g., 50% capacity) continuously for 8 to 24 hours. Monitor the resident set size (RSS) memory of the Uvicorn worker processes over time. In a healthy system, memory stabilizes after initial cache priming. If RSS memory continuously increases without leveling off, a memory leak (such as accumulating objects in global lists or unclosed connections) is present.
</details>

<details>
<summary>7. What is the impact of keep-alive connections on load test results?</summary>
HTTP Keep-Alive allows multiple HTTP requests to reuse an established TCP connection, eliminating the latency and overhead of the TCP 3-way handshake and TLS negotiation for every request. If a load test disables keep-alive, it tests connection setup throughput and risks OS ephemeral port exhaustion rather than the core application processing speed.
</details>

<details>
<summary>8. How do you define a performance threshold in k6 that causes the CLI to exit with code 1 if violated?</summary>
In the exported `options` object in the k6 script:
```javascript
export const options = {
  thresholds: {
    http_req_duration: ['p(95)<250'], // 95% of requests must complete under 250ms
    http_req_failed: ['rate<0.01'],    // Errors must be less than 1%
  },
};
```
If either condition is breached during execution, k6 marks the test as failed and returns a non-zero exit code.
</details>

---

## 12. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Basic Locustfile with Weighted Tasks</summary>

**Task**: Write a `locustfile.py` with an `HttpUser` that hits `/items` 4 times more often than `/items/special`, with a randomized wait time between 1 and 2 seconds.

**Solution**:
```python
from locust import HttpUser, task, between

class QuickShopper(HttpUser):
    wait_time = between(1.0, 2.0)
    
    @task(4)
    def get_items(self):
        self.client.get("/items")
        
    @task(1)
    def get_special_item(self):
        self.client.get("/items/special")
```
</details>

<details>
<summary>Exercise 2 (Easy): Minimal k6 Health Check Smoke Test</summary>

**Task**: Write a k6 script `smoke.js` that sends 5 concurrent requests to `http://localhost:8000/healthz` for 10 seconds and verifies that every response returns status code 200.

**Solution**:
```javascript
import http from 'k6/http';
import { check } from 'k6';

export const options = {
  vus: 5,
  duration: '10s',
};

export default function () {
  const res = http.get('http://localhost:8000/healthz');
  check(res, {
    'status is 200': (r) => r.status === 200,
  });
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Locust Authenticated User Session</summary>

**Task**: Write a Locust user class that logs in during `on_start` by POSTing credentials to `/auth/login`, extracts the JWT `access_token`, and attaches it as a Bearer token in the headers for all subsequent requests.

**Solution**:
```python
from locust import HttpUser, task, between

class AuthenticatedUser(HttpUser):
    wait_time = between(0.5, 1.5)
    
    def on_start(self):
        resp = self.client.post("/auth/login", json={"username": "alice", "password": "secret_password"})
        if resp.status_code == 200:
            token = resp.json().get("access_token")
            self.client.headers.update({"Authorization": f"Bearer {token}"})
        else:
            resp.failure("Authentication failed during user initialization")

    @task
    def access_protected_profile(self):
        self.client.get("/users/me")
```
</details>

<details>
<summary>Exercise 4 (Medium): k6 Ramping Stages with Threshold Enforcement</summary>

**Task**: Create a k6 test script with 3 stages: ramp up to 100 VUs in 20s, hold for 40s, ramp down in 10s. Enforce that p99 latency is below 400ms and failure rate is below 0.5%.

**Solution**:
```javascript
import http from 'k6/http';
import { check, sleep } from 'k6';

export const options = {
  stages: [
    { duration: '20s', target: 100 },
    { duration: '40s', target: 100 },
    { duration: '10s', target: 0 },
  ],
  thresholds: {
    http_req_duration: ['p(99)<400'],
    http_req_failed: ['rate<0.005'],
  },
};

export default function () {
  const res = http.get('http://localhost:8000/api/v1/resource');
  check(res, { 'is status 200': (r) => r.status === 200 });
  sleep(1);
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Distributed Locust Runner Bash Script</summary>

**Task**: Write a bash script `run_distributed_locust.sh` that spins up 1 Locust master process and 4 headless worker processes, runs a 10-minute test targeting 1,000 users against an environment URL, exports an HTML report, and gracefully terminates all worker processes upon completion.

**Solution**:
```bash
#!/usr/bin/env bash
set -e

TARGET_HOST=${1:-"http://localhost:8000"}
WORKERS=4

echo "Starting Locust Master targeting ${TARGET_HOST}..."
locust -f loadtests/locustfile.py --master --headless \
  --expect-workers ${WORKERS} \
  -u 1000 -r 50 -t 10m \
  --host "${TARGET_HOST}" \
  --html report_distributed.html &
MASTER_PID=$!

echo "Starting ${WORKERS} Locust Workers..."
WORKER_PIDS=()
for i in $(seq 1 ${WORKERS}); do
  locust -f loadtests/locustfile.py --worker &
  WORKER_PIDS+=($!)
done

# Wait for master to finish test duration
wait ${MASTER_PID}

echo "Test run complete. Cleaning up worker processes..."
for pid in "${WORKER_PIDS[@]}"; do
  kill -SIGTERM "${pid}" 2>/dev/null || true
done

echo "Distributed load test complete. Report saved to report_distributed.html"
```
</details>

<details>
<summary>Exercise 6 (Hard): k6 Webhook Payload Generator with Signature Calculation</summary>

**Task**: Write a k6 script that sends POST requests simulating an external payment webhook. In each iteration, generate a random JSON transaction ID, calculate an HMAC-SHA256 signature using a pre-shared secret, and send the signature in the `X-Signature` header.

**Solution**:
```javascript
import http from 'k6/http';
import crypto from 'k6/crypto';
import { check, sleep } from 'k6';

const SECRET_KEY = 'super_secret_webhook_key';

export const options = {
  vus: 20,
  duration: '30s',
  thresholds: {
    http_req_failed: ['rate<0.01'],
  },
};

export default function () {
  const txId = `tx_${Date.now()}_${Math.floor(Math.random() * 100000)}`;
  const payload = JSON.stringify({
    transaction_id: txId,
    amount: 99.50,
    status: 'COMPLETED',
  });

  // Calculate HMAC-SHA256 signature
  const signature = crypto.hmac('sha256', SECRET_KEY, payload, 'hex');

  const params = {
    headers: {
      'Content-Type': 'application/json',
      'X-Signature': signature,
    },
  };

  const res = http.post('http://localhost:8000/api/v1/webhooks/payment', payload, params);
  check(res, {
    'webhook accepted': (r) => r.status === 200,
  });

  sleep(0.5);
}
```
</details>
