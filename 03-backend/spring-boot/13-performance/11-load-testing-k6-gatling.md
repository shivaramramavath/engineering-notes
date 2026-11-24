# Load Testing Spring Boot: k6 and Gatling

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 13-performance |
| **Status** | Complete |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Script Reference](#4-syntax--script-reference)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
A Spring Boot application may function flawlessly during unit, integration, and manual QA testing, yet collapse within minutes of being deployed to production under real-world concurrent traffic:
- Database connection pools saturate, causing thread starvation and cascading 504 Gateway Timeouts.
- Undetected memory leaks in Old Generation heap trigger Stop-The-World GC thrashing after 4 hours of sustained load.
- Autoscaling configurations in Kubernetes fail to react quickly enough to absorb sudden traffic surges (Spike Load).

Without structured performance testing, engineering teams deploy into the dark, relying on real customers to discover the system's breaking point.

### Historical Evolution
Legacy load testing tools like Apache JMeter relied on complex XML GUI configurations and heavy Java threads, requiring massive cluster infrastructure just to generate high load.

Modern engineering standardizes on **Developer-Centric Performance Testing as Code**:
1. **k6 (Grafana)**: Written in Go with an intuitive JavaScript/TypeScript scripting engine. Extremely lightweight, executes tens of thousands of Virtual Users (VUs) from a single machine, and integrates natively into CI/CD pipelines.
2. **Gatling**: High-performance asynchronous load testing framework using the Netty non-blocking network engine. Features a modern Java/Kotlin/Scala DSL, rich HTML reports, and tight assertion boundaries.

### Production Relevance
Structured load testing allows teams to:
- Establish empirical performance baselines (throughput, P95/P99 latency, error rates).
- Execute automated performance regression gates in CI/CD before releasing to production.
- Right-size Kubernetes CPU/RAM requests and limits based on empirical stress data.

---

## 2. Core Concepts

### Primary Load Testing Methodologies
1. **Load Testing (Baseline)**: Validates that the system meets its target Service Level Objectives (SLOs) under expected normal and peak traffic volumes (e.g. 5,000 requests/sec for 30 minutes).
2. **Stress Testing**: Gradually increases traffic beyond peak capacity to discover the system's **breaking point** and observe how gracefully it degrades (sheds load vs. hard crash).
3. **Spike Testing**: Simulates an instantaneous surge in traffic (e.g. $10\times$ traffic increase in 10 seconds) to verify Kubernetes autoscaling, connection pool elasticity, and circuit breaker resilience.
4. **Soak / Endurance Testing**: Runs moderate traffic continuously over extended durations (12 to 48 hours) to uncover slow memory leaks, connection pool leaks, and disk exhaustion.
5. **Virtual Users (VUs)**: Simulated client threads/coroutines executing user journeys in parallel.
6. **Thresholds & Assertions**: Automated pass/fail criteria (e.g. `p(99) < 200ms`, `errors < 0.1%`) that break CI/CD pipelines if breached.

---

## 3. How It Works Internally

### Load Generation Engine Architecture

```
LOAD GENERATOR (k6 / Gatling Runner)
+------------------------------------------------------------------------------------+
|  VIRTUAL USERS (VUs) SCHEDULER                                                     |
|  - Ramps VUs from 0 -> 500 -> 2,000 based on execution stages                      |
|  - Non-blocking I/O engine generates thousands of concurrent HTTP/gRPC requests    |
+------------------------------------------------------------------------------------+
                                         |
                                         | HTTP POST /api/orders (High Concurrency)
                                         v
+------------------------------------------------------------------------------------+
|  SPRING BOOT TARGET MICROSERVICE                                                   |
|                                                                                    |
|  - Ingress: Tomcat NIO Worker Threads / Virtual Threads                            |
|  - Persistence: HikariCP Connection Pool (16 connections)                          |
|  - Backend: PostgreSQL Database Cluster                                            |
+------------------------------------------------------------------------------------+
                                         |
                                         | Response Timestamps & Status Codes
                                         v
+------------------------------------------------------------------------------------+
|  METRICS AGGREGATION & REPORT ENGINE                                               |
|  - Calculates Quantiles: p50, p90, p95, p99                                        |
|  - Evaluates Threshold Assertions (e.g., http_req_duration{status:200} < 200ms)    |
|  - Outputs Exit Code 0 (Success) or Exit Code 99 (Threshold Breached -> Fail CI!)  |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Script Reference

### 1. k6 Load Testing Script (`load-test.js`)

```javascript
import http from 'k6/http';
import { check, sleep } from 'k6';

// 1. Test Configuration Stages & Performance Thresholds
export const options = {
  stages: [
    { duration: '1m', target: 100 },  // Ramp-up to 100 VUs over 1 minute
    { duration: '3m', target: 500 },  // Sustained load of 500 VUs for 3 minutes
    { duration: '30s', target: 0 },   // Ramp-down
  ],
  thresholds: {
    // 95% of requests must finish under 150ms; 99% under 300ms
    http_req_duration: ['p(95)<150', 'p(99)<300'],
    // HTTP failure rate must be less than 0.5%
    http_req_failed: ['rate<0.005'],
  },
};

const BASE_URL = 'http://localhost:8080/api';

export default function () {
  const payload = JSON.stringify({
    customerId: 'CUST-1001',
    amount: 149.99,
    items: ['SKU-A', 'SKU-B'],
  });

  const params = {
    headers: {
      'Content-Type': 'application/json',
      'X-Correlation-ID': `k6-${__VU}-${__ITER}`,
    },
  };

  // 2. Execute HTTP Request
  const res = http.post(`${BASE_URL}/orders`, payload, params);

  // 3. Assertions / Checks
  check(res, {
    'status is 201 Created': (r) => r.status === 201,
    'order id present': (r) => JSON.parse(r.body).orderId !== undefined,
  });

  // User think time between requests (100ms - 300ms)
  sleep(0.2);
}
```

### Running k6 CLI
```bash
# Run local script
k6 run load-test.js

# Run with custom environment variables
k6 run -e TARGET_URL=https://staging.internal/api load-test.js
```

---

## 5. Practical Examples

### Example 1: Gatling Simulation in Modern Java DSL (`pom.xml` & `OrderSimulation.java`)
Gatling 3.7+ natively supports pure Java without requiring Scala!

```xml
<!-- Gatling Maven Plugin in pom.xml -->
<plugin>
    <groupId>io.gatling</groupId>
    <artifactId>gatling-maven-plugin</artifactId>
    <version>3.10.5</version>
</plugin>
```

```java
package com.example.performance.gatling;

import io.gatling.javaapi.core.ScenarioBuilder;
import io.gatling.javaapi.core.Simulation;
import io.gatling.javaapi.http.HttpProtocolBuilder;

import java.time.Duration;

import static io.gatling.javaapi.core.CoreDsl.*;
import static io.gatling.javaapi.http.HttpDsl.*;

public class OrderCheckoutSimulation extends Simulation {

    // 1. HTTP Protocol Configuration
    private final HttpProtocolBuilder httpProtocol = http
        .baseUrl("http://localhost:8080/api")
        .acceptHeader("application/json")
        .contentTypeHeader("application/json");

    // 2. Feeder for dynamic test data
    private final FeederBuilder<String> feeder = csv("test-customers.csv").random();

    // 3. User Scenario Definition
    private final ScenarioBuilder userJourney = scenario("Customer Checkout Journey")
        .feed(feeder)
        .exec(
            http("Get Catalog")
                .get("/products")
                .check(status().is(200))
        )
        .pause(1) // Think time
        .exec(
            http("Create Order")
                .post("/orders")
                .body(StringBody("{\"customerId\": \"#{customerId}\", \"amount\": 99.50}"))
                .check(status().is(201))
                .check(jsonPath("$.orderId").saveAs("orderId"))
        )
        .pause(Duration.ofMillis(500))
        .exec(
            http("Get Order Details")
                .get("/orders/#{orderId}")
                .check(status().is(200))
        );

    // 4. Load Injection Profile & Assertions
    public OrderCheckoutSimulation() {
        setUp(
            userJourney.injectOpen(
                rampUsers(100).during(Duration.ofSeconds(30)),
                constantUsersPerSec(200).during(Duration.ofMinutes(2))
            )
        )
        .protocols(httpProtocol)
        .assertions(
            global().responseTime().percentile(95).lt(200), // P95 < 200ms
            global().failedRequests().percent().lt(1.0)     // Errors < 1%
        );
    }
}
```

### Example 2: Analyzing k6 Terminal Test Output
```text
          /\      |‾‾| /‾‾/   /‾‾/   
     /\  /  \     |  |/  /   /  /    
    /  \/    \    |     (   /   ‾‾\  
   /          \   |  |\  \ |  (‾)  | 
  / __________ \  |__| \__\ \_____/ .io

  execution: local
     script: load-test.js
     output: -

     scenarios: (100.00%) 1 scenario, 500 max VUs, 4m30s max duration:
              * default: 500 iterations for 4m30s

  ✓ status is 201 Created
  ✓ order id present

  checks.........................: 100.00% ✓ 48210      ✗ 0    
  data_received..................: 18 MB   67 kB/s
  data_sent......................: 8.2 MB  31 kB/s
  http_req_blocked...............: avg=12µs    min=1µs   med=4µs   max=4.2ms  p(90)=8µs    p(95)=12µs  
  http_req_connecting............: avg=2µs     min=0s    med=0s    max=1.8ms  p(90)=0s     p(95)=0s    
  http_req_duration..............: avg=18.42ms min=2.1ms med=12ms  max=342ms  p(95)=48.1ms p(99)=112ms 
    { expected_response:true }...: avg=18.42ms min=2.1ms med=12ms  max=342ms  p(95)=48.1ms p(99)=112ms 
  http_req_failed................: 0.00%   ✓ 0          ✗ 24105
  http_reqs......................: 24105   89.27/s
  vus............................: 1       min=1        max=500
```
*Analysis*: All thresholds passed! P95 was 48.1ms (target $< 150\text{ms}$), P99 was 112ms (target $< 300\text{ms}$), and 0% failures across 24,105 total requests.

### Example 3: Spike Test Scenario in k6
Spike tests test sudden traffic bursts to verify auto-scaling and rate-limiting resilience:

```javascript
export const options = {
  stages: [
    { duration: '30s', target: 50 },    // Normal baseline traffic
    { duration: '10s', target: 2000 },  // MASSIVE SPIKE! 2,000 VUs in 10 seconds!
    { duration: '1m', target: 2000 },   // Sustain spike
    { duration: '20s', target: 50 },    // Drop back to normal
    { duration: '1m', target: 50 },     // Verify recovery
  ],
  thresholds: {
    http_req_failed: ['rate<0.02'], // Allow max 2% errors during massive spike
  },
};
```

### Example 4: Anti-Pattern vs. Production Fix (No Think Time in Load Tests)
```javascript
// ANTI-PATTERN: Zero think time (Tight infinite while loop)
// Real humans do not send requests with 0ms delay!
// Result: 50 VUs generate 20,000 req/sec, behaving like a DDoS attack rather than real users!
export default function () {
  http.get('http://localhost:8080/api/products');
  // NO SLEEP: VUs instantly loop and hammer the socket!
}

// PRODUCTION FIX: Realistic pacing with random think time
export default function () {
  http.get('http://localhost:8080/api/products');
  // Simulates realistic user reading / hesitation time (1 to 3 seconds)
  sleep(Math.random() * 2 + 1);
}
```

### Example 5: Automating Performance Gates in GitHub Actions CI
```yaml
name: Performance Regression Gate

on:
  pull_request:
    branches: [ main ]

jobs:
  load-test:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout Code
        uses: actions/checkout@v4

      - name: Start Spring Boot Service Container
        run: |
          docker compose up -d order-service postgres redis
          # Wait for health probe
          timeout 60s sh -c 'until curl -s http://localhost:9090/actuator/health/readiness | grep UP; do sleep 2; done'

      - name: Install k6
        run: |
          sudo gpg -k
          sudo gpg --no-default-keyring --keyring /usr/share/keyrings/k6-archive-keyring.gpg --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys C5AD17C747E3415A3642D57D77C6C491D6AC1D69
          echo "deb [signed-by=/usr/share/keyrings/k6-archive-keyring.gpg] https://dl.k6.io/deb stable main" | sudo tee /etc/apt/sources.list.d/k6.list
          sudo apt-get update && sudo apt-get install k6

      - name: Execute k6 Performance Test
        run: |
          k6 run --exit-code 99 scripts/k6/ci-gate.js
```
*Result*: If `p(99) > 200ms` or errors exceed 0.5%, k6 exits with code 99, automatically blocking the PR from merging.

---

## 6. Best Practices & Production Guidelines

1. **Always Establish a Pre-Test Baseline**: Before running tests with 2,000 VUs, execute a single-user baseline test (1 VU for 30s) to measure pure latency under zero resource contention.
2. **Include Realistic Think Time (`sleep`)**: Model human behavior by including randomized delays between actions (e.g. 1–3s). Load tests without think time simulate synthetic microbenchmarks or DDoS attacks rather than user traffic.
3. **Parameterize Dynamic Test Data with Feeders**: Never query the same single `id=1` repeatedly. Querying a single ID hits the database cache 100% of the time, producing unrealistically optimistic results. Use CSV feeders to query random IDs across millions of records.
4. **Co-Locate Load Generators on Separate Network Infrastructure**: Never run the load generator (k6/Gatling) on the same machine hosting the Spring Boot service. The load generator's own CPU and network socket consumption will starve the Spring Boot process, corrupting test results.
5. **Monitor Server Metrics Concurrently**: Run Prometheus and Grafana during the load test. Correlate k6 latency spikes with HikariCP active connections, JVM GC pause durations, and CPU utilization.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Querying the same record in every request | All requests hit L1/L2 caches and database buffer pools, masking slow SQL execution. | Use diverse test datasets via CSV feeders or random generators. |
| Running load generator on the application host | Load generator competes for CPU cores and OS network sockets with the target JVM. | Run k6 from a dedicated runner node or external VM. |
| Ignoring OS file descriptor limits (`ulimit -n`) | High concurrency throws `java.net.SocketException: Too many open files` on the load generator. | Increase client ulimit: `ulimit -n 65535`. |
| Omitting error rate thresholds | All requests return `HTTP 500` in 2ms; test passes with "fast latency" despite total system failure! | Always assert `check(status().is(200))` and `http_req_failed: ['rate<0.01']`. |
| Testing against empty test databases | Queries against 50 rows take 1ms; production with 10,000,000 rows takes 500ms due to unindexed scans. | Seed test databases with realistic production-scale data volumes. |
| No warm-up phase | JVM JIT compiler is cold during initial seconds, falsely failing thresholds on first requests. | Include a 1-minute ramp-up phase before measuring SLA thresholds. |

---

## 8. Performance & Security Notes

### Coordinated Omission Problem
- Traditional load testers issue requests synchronously. If the target server pauses for 5 seconds during a GC pause, the load tester stops sending requests and waits.
- This artificially drops the request rate during the outage, hiding the true severity of the queue buildup.
- Modern tools (Gatling, k6 open models) generate requests on independent schedules (`constantUsersPerSec`), avoiding coordinated omission.

### Security & Denial of Service Protection
- Never run stress or spike tests against production systems without prior authorization and alerting the SRE team.
- Ensure test traffic includes synthetic headers (`X-Load-Test: true`) so that analytics databases can filter out test transactions.

---

## 9. Comparison / When to Use What

| Feature | k6 (Grafana) | Gatling | Apache JMeter |
|---|---|---|---|
| **Scripting Language** | JavaScript / TypeScript | Java / Kotlin / Scala DSL | GUI / XML |
| **Engine Architecture** | Go (Goroutines) | Java / Netty (Non-blocking) | Java (Thread-per-VU) |
| **CI/CD Integration** | **Native first-class CLI** | **Native Maven / Gradle plugin** | Complex plugins |
| **Max VUs per Machine** | **$\approx 30,000\text{--}50,000$** | **$\approx 30,000\text{--}50,000$** | $\approx 1,000\text{--}2,000$ |
| **HTML Reports** | Via plugins / Cloud dashboard | **Built-in interactive HTML report** | Basic HTML report |
| **Developer Experience** | **Highest (Modern JS code)** | **High (Type-safe Java code)** | Low (Heavy XML GUI) |

---

## 10. Interview Questions & Answers

### Q1: What is the difference between a Load Test and a Stress Test?
<details>
<summary>Click to view answer</summary>

- **Load Test**: Evaluates whether the system meets its target SLOs (latency, throughput, error rate) under expected normal and peak traffic loads.
- **Stress Test**: Continuously ramps traffic beyond normal peak capacity until the system breaks, identifying the maximum breaking point, the failure mode (graceful degradation vs. hard crash), and the root bottleneck (CPU, memory, database connection pool, thread pool).
</details>

### Q2: What is the "Coordinated Omission" problem in load testing?
<details>
<summary>Click to view answer</summary>

Coordinated omission occurs when a load testing tool waits for a response before scheduling the next request. If the server freezes for 5 seconds (e.g. during a Full GC pause), the tester stops sending requests during that 5-second window. In the real world, user traffic does not pause when a server freezes; requests queue up. Coordinated omission results in measuring only completed requests, severely under-reporting true tail latency (P99/P99.9).
</details>

### Q3: Why is k6 written in Go instead of NodeJS if tests are written in JavaScript?
<details>
<summary>Click to view answer</summary>

k6 uses an embedded JavaScript interpreter (goja) to parse test logic, but the actual networking, HTTP client, virtual user scheduling, and metrics aggregation run natively in compiled Go using lightweight goroutines and channels. This allows a single k6 process to simulate 30,000+ concurrent virtual users without the memory and single-threaded event loop constraints of NodeJS.
</details>

### Q4: What are "Thresholds" in k6 and how are they used in CI/CD pipelines?
<details>
<summary>Click to view answer</summary>

Thresholds are automated pass/fail criteria defined in test configuration options (e.g. `http_req_duration: ['p(95)<200']`, `http_req_failed: ['rate<0.01']`). During test execution, k6 tracks real-time metrics. If any threshold is violated, k6 returns a non-zero exit code upon completion, allowing CI/CD runners (GitHub Actions, GitLab CI) to automatically fail the build and block deployment.
</details>

### Q5: How do you prevent test data caching from skewing load test results?
<details>
<summary>Click to view answer</summary>

If the load test repeatedly requests the exact same entity ID (e.g. `/api/orders/1`), the database buffer pool and application L1/L2 caches will serve every request from memory, producing artificial sub-millisecond latencies. Use dynamic feeders (CSV files, random ID generators) containing thousands or millions of unique keys to ensure realistic cache hit/miss distributions that exercise actual database disk and index lookups.
</details>

### Q6: What is a Soak (Endurance) Test and what defects does it uncover?
<details>
<summary>Click to view answer</summary>

A soak test runs moderate traffic against an application continuously for 12 to 48 hours. It is designed to uncover slow, cumulative defects that do not appear during brief 5-minute tests:
- Slow memory leaks in JVM heap or native memory (Metaspace, Netty direct memory).
- HikariCP connection leaks (unclosed statements/connections).
- Disk space exhaustion from unrotated log files.
- Thread leaks in custom thread pools.
</details>

### Q7: What is the difference between an Open Model and a Closed Model in Gatling?
<details>
<summary>Click to view answer</summary>

- **Closed Model**: Controls the number of concurrent active users (e.g. maintain exactly 100 concurrent users). A new user only enters when an existing user finishes.
- **Open Model**: Controls the arrival rate of new users (e.g. inject 50 new users every second regardless of how long existing users take). Open models accurately reflect web traffic and avoid coordinated omission.
</details>

### Q8: How should you warm up a Spring Boot service before measuring load test results?
<details>
<summary>Click to view answer</summary>

During the first 30–60 seconds of a test, the HotSpot JVM is compiling bytecode to native assembly (JIT C1/C2 compilers), database connection pools are expanding, and caches are cold. Include an initial warm-up phase (e.g. 1 minute of low traffic) and exclude metrics captured during this period from the final SLA assertion calculations.
</details>

---

## 11. Practice Exercises

### Exercise 1: Write a Basic k6 Test Script with Thresholds (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a k6 script targeting `http://localhost:8080/actuator/health` with 50 VUs for 30 seconds, asserting that P95 latency is $< 50\text{ms}$ and error rate is $0\%$.

**Solution**:
```javascript
import http from 'k6/http';
import { check, sleep } from 'k6';

export const options = {
  vus: 50,
  duration: '30s',
  thresholds: {
    http_req_duration: ['p(95)<50'],
    http_req_failed: ['rate==0'],
  },
};

export default function () {
  const res = http.get('http://localhost:8080/actuator/health');
  check(res, {
    'status is 200': (r) => r.status === 200,
  });
  sleep(0.5);
}
```
</details>

### Exercise 2: Gatling Scenario with Dynamic CSV Feeder (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a Gatling Java simulation that feeds dynamic `orderId` values from a CSV file into a `GET /api/orders/#{orderId}` endpoint with 50 users injected per second for 1 minute.

**Solution**:
```java
package com.example.performance.gatling;

import io.gatling.javaapi.core.ScenarioBuilder;
import io.gatling.javaapi.core.Simulation;
import io.gatling.javaapi.http.HttpProtocolBuilder;

import java.time.Duration;

import static io.gatling.javaapi.core.CoreDsl.*;
import static io.gatling.javaapi.http.HttpDsl.*;

public class DynamicOrderSimulation extends Simulation {

    private final HttpProtocolBuilder httpProtocol = http
        .baseUrl("http://localhost:8080/api")
        .acceptHeader("application/json");

    private final ScenarioBuilder scn = scenario("Dynamic Order Lookups")
        .feed(csv("orders.csv").circular())
        .exec(
            http("Fetch Order")
                .get("/orders/#{orderId}")
                .check(status().is(200))
        );

    public DynamicOrderSimulation() {
        setUp(
            scn.injectOpen(
                constantUsersPerSec(50).during(Duration.ofMinutes(1))
            )
        )
        .protocols(httpProtocol)
        .assertions(
            global().responseTime().percentile(99).lt(150)
        );
    }
}
```
</details>

### Exercise 3: k6 Script Simulating an Authenticated OAuth2 User Journey (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a k6 script where each VU first acquires an OAuth2 Bearer token from `/oauth/token` using client credentials, and then uses that token to execute a POST `/api/transfers` request.

**Solution**:
```javascript
import http from 'k6/http';
import { check, sleep } from 'k6';

export const options = {
  stages: [
    { duration: '30s', target: 20 },
    { duration: '1m', target: 100 },
    { duration: '30s', target: 0 },
  ],
  thresholds: {
    http_req_duration: ['p(95)<250'],
    http_req_failed: ['rate<0.01'],
  },
};

const BASE_URL = 'http://localhost:8080';

export default function () {
  // Step 1: Obtain OAuth2 Bearer Token
  const tokenPayload = {
    client_id: 'test-client',
    client_secret: 'secret-key',
    grant_type: 'client_credentials',
  };

  const tokenRes = http.post(`${BASE_URL}/oauth/token`, tokenPayload);
  check(tokenRes, { 'token acquired': (r) => r.status === 200 });
  const token = JSON.parse(tokenRes.body).access_token;

  // Step 2: Authenticated Business API Call
  const transferPayload = JSON.stringify({
    sourceAccount: 'ACC-101',
    destAccount: 'ACC-202',
    amount: 50.00,
  });

  const headers = {
    headers: {
      'Content-Type': 'application/json',
      'Authorization': `Bearer ${token}`,
    },
  };

  const transferRes = http.post(`${BASE_URL}/api/transfers`, transferPayload, headers);
  check(transferRes, {
    'transfer success': (r) => r.status === 200,
  });

  sleep(1);
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Concept / Tool                              | Description & Best Practice                                 |
+---------------------------------------------+-------------------------------------------------------------+
| k6 (Grafana)                                | Go-based lightweight load testing engine with JS scripts    |
| Gatling                                     | Netty-based asynchronous load testing with Java/Scala DSL   |
| Thresholds (k6)                             | Automated SLO assertions (p(95)<150ms) that fail CI builds  |
| Coordinated Omission                        | Skew where testers wait on frozen servers; avoid with open  |
| Feeders                                     | Dynamic CSV/JSON data sources preventing cache skew         |
| Think Time (sleep)                          | Realistic delays between user actions (model human pacing)  |
| Spike Test                                  | Sudden traffic surge testing autoscaling and rate limits    |
| Soak Test                                   | Extended endurance run (12-48h) detecting slow memory leaks|
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- k6 Official Documentation & Performance Best Practices
- Gatling Official Documentation: Java DSL Reference
- Gil Tene: Understanding Latency and the Coordinated Omission Problem
- Google SRE Book: Testing for Reliability and Load Resistance