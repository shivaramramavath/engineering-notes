---
title: "Incident Runbooks for FastAPI in Production"
description: "Comprehensive emergency runbooks: high CPU triage, event loop deadlocks, connection pool starvation, memory leaks, 502/504 errors, and post-mortem procedures."
fastapi_version: "0.115+"
python_version: "3.12+"
free_threading_compatible: true
tested_with: "Python 3.12.8, FastAPI 0.115.6, py-spy 0.4.0, Kubernetes 1.31"
---

# Incident Runbooks for FastAPI in Production

## 1. Conceptual Foundation & Mental Model

When a production incident strikes (high latency, cascading HTTP 500/502/504 errors, pod crashloops, or database exhaustion), panic and ad-hoc troubleshooting exacerbate the outage. **Incident Runbooks** provide structured, deterministic, step-by-step procedures designed to mitigate user impact rapidly, stabilize the service, identify root causes, and conduct blameless post-mortem analysis.

The mental model for incident response follows the **Triage $\to$ Mitigate $\to$ Diagnose $\to$ Resolve $\to$ Prevent** lifecycle:
1. **Triage (Assess Severity)**: Determine impact level (Sev-1: Complete service outage; Sev-2: Core feature degraded; Sev-3: Minor bug with workaround).
2. **Mitigate (Stop the Bleeding)**: Roll back recent deployments, scale horizontal replicas, enable circuit breakers, or shed non-critical traffic. **Mitigation takes priority over root-cause investigation!**
3. **Diagnose & Capture State**: Collect heap dumps, py-spy flamegraphs, database lock telemetry, and correlation IDs before restarting failing containers.
4. **Resolve**: Apply verified hotfixes or infrastructure configuration changes.
5. **Prevent**: Document incident timeline and implement defensive guardrails in a blameless post-mortem.

```
[ ALERT FIRES: PagerDuty / Opsgenie ]
                 │
                 ▼
     [ Step 1: Rapid Triage ]
     Assess user impact (Sev-1 vs Sev-2)
                 │
                 ▼
     [ Step 2: Immediate Mitigation ]
     ├── Has a new release occurred in the last 60 minutes?
     │    └── YES: Execute IMMEDIATE ROLLBACK to previous stable image!
     └── NO: Shed non-critical traffic / Scale HPA replicas
                 │
                 ▼
     [ Step 3: Diagnostic Snapshot ]
     Capture live stack traces (py-spy) and DB lock metrics
                 │
                 ▼
     [ Step 4: Root Cause Resolution ]
     Apply targeted hotfix or infrastructure repair
                 │
                 ▼
     [ Step 5: Blameless Post-Mortem ]
     Document timeline, root cause, and 5 Whys action items
```

---

## 2. Architecture & Emergency Tooling

```
+---------------------------------------------------------------+
|                      Production Incident Toolkit              |
+---------------------------------------------------------------+
|  1. py-spy              -> Live non-invasive CPython profiler |
|  2. kubectl / ecs-cli   -> Orchestrator inspection & scaling  |
|  3. pg_stat_activity    -> PostgreSQL live query / lock audit |
|  4. redis-cli           -> Redis slowlog and memory inspection|
|  5. Prometheus / Grafana-> Request rate, error%, and p99 trace|
+---------------------------------------------------------------+
```

---

## 3. Comprehensive Syntax & Diagnostic Commands

### Capturing Live Stack Traces with `py-spy`
```bash
# Capture live stack traces of all running threads in container (PID 1)
py-spy dump --pid 1

# Generate a 30-second flamegraph without restarting or attaching GDB
py-spy record -o /tmp/incident_flamegraph.svg --pid 1 --duration 30
```

### PostgreSQL Emergency Lock & Slow Query Inspection
```sql
-- Find all queries running longer than 10 seconds holding locks
SELECT 
    pid, 
    now() - pg_stat_activity.query_start AS duration, 
    query, 
    state,
    wait_event_type,
    wait_event
FROM pg_stat_activity
WHERE (now() - pg_stat_activity.query_start) > interval '10 seconds'
AND state != 'idle'
ORDER BY duration DESC;

-- Terminate a rogue query blocking other transactions
SELECT pg_terminate_backend(12345);
```

---

## 4. Practical Implementation: 6 Emergency Runbooks

---

### RUNBOOK 1: High CPU Utilization & Event Loop Freezes

**Symptoms**: Pod CPU pins at 100%; p99 latency spikes above 5,000ms; Prometheus reports elevated `event_loop_lag_seconds`.

#### Immediate Mitigation:
```bash
# 1. Scale deployment horizontally by 50% to distribute traffic
kubectl scale deployment/fastapi-service --replicas=12 -n production
```

#### Diagnostic Steps:
```bash
# 2. Exec into one struggling pod and capture thread stack traces
kubectl exec -it <pod-name> -n production -- py-spy dump

# Look for threads executing:
# - Synchronous requests or time.sleep inside async def routes
# - Unbounded regular expressions (ReDoS backtracking)
# - CPU-heavy serialization or unindexed database sorting
```

#### Resolution:
If caused by a recent commit, roll back immediately:
```bash
kubectl rollout undo deployment/fastapi-service -n production
```

---

### RUNBOOK 2: Database Connection Pool Starvation

**Symptoms**: HTTP 504 Gateway Timeouts; logs display `TimeoutError: QueuePool limit of size 20 overflow 10 reached, connection timed out`.

#### Immediate Mitigation:
```bash
# 1. Check PostgreSQL active connections
psql -h db-cluster.internal -U admin -c "SELECT count(*), state FROM pg_stat_activity GROUP BY state;"

# 2. Identify long-running idle-in-transaction connections
psql -h db-cluster.internal -U admin -c "
SELECT pid, now() - state_change as idle_duration, query 
FROM pg_stat_activity 
WHERE state = 'idle in transaction' 
ORDER BY idle_duration DESC LIMIT 10;
"
```

#### Remediation:
1. Terminate orphaned idle-in-transaction connections:
```sql
SELECT pg_terminate_backend(pid) 
FROM pg_stat_activity 
WHERE state = 'idle in transaction' 
AND now() - state_change > interval '1 minute';
```
2. Verify application code is not holding DB sessions open across outbound HTTP calls.

---

### RUNBOOK 3: Memory Leak & Container OOM-Kills

**Symptoms**: Pods restart intermittently with exit code 137 (`OOMKilled`); memory usage steadily increases without leveling off.

#### Diagnostic Steps:
1. Identify the leaking route using process inspection:
```bash
# Check memory allocation growth in container
kubectl top pods -n production --sort-by=memory
```
2. Inspect application heap using Python's `tracemalloc` endpoint (if enabled):
```python
import tracemalloc
from fastapi import FastAPI

app = FastAPI()

@app.get("/debug/memory-top")
async def get_memory_snapshot():
    snapshot = tracemalloc.take_snapshot()
    top_stats = snapshot.statistics("lineno")
    return [{"file": str(stat.traceback), "size_kb": stat.size / 1024} for stat in top_stats[:10]]
```

#### Mitigation:
1. Temporarily increase pod memory limit in Kubernetes manifest:
```yaml
resources:
  limits:
    memory: "2048Mi" # Doubled from 1024Mi to prevent immediate OOM
```
2. Configure Gunicorn worker recycling (`max_requests = 5000`) to periodically refresh workers.

---

### RUNBOOK 4: 502 Bad Gateway Cascades Behind Reverse Proxy / Nginx

**Symptoms**: Ingress/Nginx returns HTTP 502 Bad Gateway; Uvicorn appears unresponsive.

#### Diagnostic Checklist:
1. Check if Uvicorn processes have crashed:
```bash
kubectl get pods -n production -l app=fastapi-service
```
2. Verify Uvicorn listen backlog: If requests arrive faster than Uvicorn can accept them, the OS socket backlog fills up, causing Nginx to receive `Connection Refused`.
3. Check Nginx error logs:
```bash
# Look for upstream errors:
# "connect() failed (111: Connection refused) while connecting to upstream"
# "upstream prematurely closed connection while reading response header"
tail -n 100 /var/log/nginx/error.log
```

#### Remediation:
1. Increase Uvicorn socket backlog: `--backlog 4096`.
2. Ensure `preStop: exec: command: ["sleep", "5"]` is configured in Kubernetes deployment.

---

### RUNBOOK 5: Redis Outage & Cache Saturation

**Symptoms**: Cache timeouts; error logs show `ConnectionError: Error connecting to Redis`.

#### Immediate Mitigation:
1. Enable Cache-Bypass Mode (Circuit Breaker) via Feature Flag:
```bash
# Flip feature flag to skip Redis lookups and query DB directly
curl -X POST https://api.internal/admin/flags/bypass-cache -H "Authorization: Bearer $ADMIN_TOKEN"
```
2. Restart or failover Redis cluster instance via AWS ElastiCache / Redis Enterprise console.

---

### RUNBOOK 6: Post-Mortem Incident Template (Blameless)

```markdown
# Blameless Post-Mortem: [Service Name] - [Incident Date]

## Incident Summary
- **Date**: YYYY-MM-DD
- **Duration**: XX minutes (from detection to mitigation)
- **Severity**: Sev-1 / Sev-2
- **Impact**: XX,XXX requests failed; $XX estimated revenue impact.
- **Incident Commander**: [Name]

## Timeline (All times in UTC)
- **14:02** - Deployment v2.4.1 rolled out to production.
- **14:06** - Datadog alert: p99 latency spiked from 35ms to 4,200ms.
- **14:08** - On-call engineer acknowledged incident; war room opened.
- **14:12** - Identified connection pool starvation on PostgreSQL cluster.
- **14:15** - Decision made to execute immediate rollback to v2.4.0.
- **14:18** - Rollback completed; latency normalized to 35ms; incident resolved.

## Root Cause Analysis (The 5 Whys)
1. **Why did latency spike?** All database connections were exhausted.
2. **Why were connections exhausted?** Each request held a database connection for 4.5 seconds.
3. **Why did requests hold connections for 4.5 seconds?** A new geocoding HTTP call was invoked inside the active database transaction context.
4. **Why was an HTTP call made inside a DB transaction?** A repository method combined data fetching and external HTTP validation in one function.
5. **Why was this not caught in CI?** Integration tests mocked the external geocoder with an instantaneous 1ms response, masking the concurrency holding pattern.

## Action Items & Preventative Measures
- [ ] Refactor geocoding service to execute before opening the DB session (Owner: @dev, Priority: P0, Due: Tomorrow).
- [ ] Add query duration alerts in Prometheus for queries exceeding 1,000ms (Owner: @sre, Priority: P1, Due: Friday).
- [ ] Implement maximum transaction timeout (`statement_timeout = 3000`) in PostgreSQL (Owner: @dba, Priority: P1, Due: Next Week).
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Restarting Containers Before Capturing Diagnostic Data**:
   The most common mistake during Sev-1 incidents is immediately deleting or restarting failing pods. Once the pod terminates, volatile memory, py-spy stack traces, and unwritten logs are destroyed forever, preventing root-cause determination. Always capture a `py-spy dump` or heap snapshot on at least one pod before restarting.

2. **Cascading Failure from Blindly Increasing Connection Pools**:
   When connection pool starvation occurs, the instinctive response is often to increase `pool_size` from 20 to 100. This compounds the failure: PostgreSQL's CPU becomes overwhelmed by lock contention, and the entire database cluster crashes.

3. **Rollback Blocked by Non-Backward-Compatible Database Migrations**:
   If an incident was caused by a release that included an irreversible breaking database migration (e.g., dropping a column), executing `kubectl rollout undo` will cause the previous code version to crash immediately. Follow the Expand-Contract pattern strictly.

---

## 6. Anti-Patterns & Common Mistakes

| Anti-Pattern | Operational Impact | Correct Pattern |
| :--- | :--- | :--- |
| Investigating root cause before mitigating user impact | Outage duration prolonged; customer impact multiplies | Mitigate first (rollback/scale), diagnose second |
| Restarting all pods without capturing thread stacks | Evidence destroyed; incident recurs on next traffic spike | Capture `py-spy` dump on 1 pod before restart |
| Increasing connection pool size during DB saturation | Overwhelms database CPU with thread contention | Throttle admission and terminate slow transactions |
| Blaming individuals during post-mortems | Promotes fear, culture of concealment, and poor reporting | Conduct blameless post-mortems focusing on systemic safeguards |
| Omitting automated rollbacks in CI/CD pipelines | Human delay in initiating rollback extends outage | Configure automated canary rollback on error rate spikes |

---

## 7. Performance & Scalability Considerations

- **Traffic Shedding**: Implement a shedding middleware that rejects non-critical requests (e.g. recommendations, telemetry ingestion) with HTTP 503 during high-load incidents, preserving capacity for core checkout/payment flows.
- **Failover Testing (Chaos Engineering)**: Periodically test incident runbooks using Chaos Mesh or AWS Fault Injection Simulator in staging environments to verify team preparedness.

---

## 8. Security & OWASP Guardrails

- **Emergency Access Management**: Restrict production container `kubectl exec` access to authorized incident commanders via ephemeral Privileged Access Management (PAM) or AWS IAM temporary credentials.
- **Sanitizing Incident Dumps**: Ensure memory snapshots and stack traces generated during incidents do not contain customer PII or plaintext secret tokens before uploading to ticket systems.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import AsyncClient, ASGITransport
from main import app

@pytest.mark.asyncio
async def test_circuit_breaker_activates_on_failure():
    # Verify that circuit breaker returns HTTP 503 instead of hanging
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Simulate circuit trip
        response = await client.get("/critical-flow?simulate_outage=true")
        assert response.status_code in [503, 504]
        assert "Service Unavailable" in response.text or "timeout" in response.text
```

---

## 10. Real-World Production Incident

**Incident**: A global digital ticketing platform experienced a 55-minute complete service blackout during a major concert tour on-sale. Over 400,000 fans received HTTP 504 errors.

**Root Cause**: The application encountered an unexpected surge of 15,000 RPS. The PostgreSQL database connection pool saturated within 10 seconds. In response, an on-call engineer doubled the connection pool size on all 30 Kubernetes pods from 20 to 50. This immediately generated 1,500 simultaneous connections to the database, exhausting PostgreSQL's shared memory buffers and causing the Linux kernel OOM killer to terminate the PostgreSQL primary instance.

**Resolution**: The database administrator manually restarted PostgreSQL, scaled down the FastAPI deployment to 5 pods to throttle incoming traffic, cleared deadlocks, and gradually scaled pods back up while monitoring connection queues. The team adopted the emergency runbooks documented here, prohibiting ad-hoc connection pool increases during active database incidents.

---

## 11. Technical Interview Questions

<details>
<summary>1. In an active production incident, what is the golden rule regarding mitigation vs root-cause diagnosis?</summary>

The golden rule is: **Mitigate first to stop user impact, diagnose second to identify root cause.** When an incident is active, the primary objective is to restore service stability (via rollback, scaling, traffic shedding, or feature flags). Spending hours investigating code while users experience an outage is unacceptable. Once service is stabilized, detailed root-cause investigation can proceed.
</details>

<details>
<summary>2. How does <code>py-spy</code> capture diagnostic information from a struggling Python process without stopping it?</summary>

`py-spy` is an external sampling profiler written in Rust. It utilizes the Linux `process_vm_readv` system call to read the memory space of the target CPython process directly from the outside. It inspects the CPython interpreter's internal thread state structures and call stacks at a fixed frequency (e.g. 100Hz). Because it does not modify Python bytecode, run inside the event loop, or hold locks, its overhead is negligible (<1%), making it safe for live production diagnosis.
</details>

<details>
<summary>3. Why is increasing database connection pool size during a pool exhaustion incident often harmful?</summary>

Connection pool exhaustion is usually a symptom of slow database queries or lock contention, not insufficient pool size. Increasing pool size allows more concurrent queries to hit the database simultaneously. Relational databases experience severe CPU context-switching and lock thrashing when handling hundreds of concurrent active queries. Increasing the pool often causes the database CPU to pin at 100% or trigger an OOM crash.
</details>

<details>
<summary>4. What is the purpose of a "Blameless Post-Mortem"?</summary>

A blameless post-mortem assumes that engineers act in good faith based on the information available to them at the time. Instead of blaming individuals for mistakes, it investigates systemic vulnerabilities, missing automated guardrails, flawed tooling, inadequate monitoring, and organizational processes. Blameless culture encourages transparent reporting of errors and leads to more resilient systems.
</details>

<details>
<summary>5. What does the "5 Whys" methodology accomplish in root-cause analysis?</summary>

The "5 Whys" technique is an iterative interrogative drill-down used to explore the cause-and-effect relationships underlying a failure. By repeatedly asking "Why?" (typically five times), engineers move beyond immediate technical symptoms (e.g., "The server timed out") to uncover systemic root causes (e.g., "CI lacked integration tests against real databases, allowing a non-performant locking pattern to reach production").
</details>

<details>
<summary>6. How do you identify a thread executing synchronous blocking code inside an async FastAPI app?</summary>

Run `py-spy dump --pid <uvicorn-pid>`. Look at the main thread's call stack. In a healthy async application, the main thread will be inside `uvloop` or `asyncio.selector_events.select()`. If the call stack shows standard synchronous functions—such as `time.sleep()`, `requests.adapters.send()`, or `socket.recv()`—that synchronous call is actively blocking the event loop and freezing all concurrent coroutines.
</details>

<details>
<summary>7. What is traffic shedding and when should it be activated?</summary>

Traffic shedding (load shedding) is an emergency defensive mechanism where an overloaded service deliberately drops or rejects non-critical requests (e.g. analytics tracking, recommendations, background notifications) with HTTP 503 Service Unavailable. Shedding non-essential load preserves CPU, memory, and database connections so that core business transactions (e.g. logins, checkout, payments) can continue functioning normally.
</details>

<details>
<summary>8. Why should you capture a thread dump or memory snapshot before restarting a failing container?</summary>

Restarting a container terminates the running process and clears volatile memory. Once restarted, all diagnostic evidence—including which thread was deadlocked, which queries were queued, and which objects leaked in memory—is lost. Capturing a 5-second `py-spy dump` or heap snapshot preserves the evidence required to conduct a thorough post-mortem and prevent the bug from recurring.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Fast Pod Log Tail Script
**Task**: Write the `kubectl` command to tail the last 100 log lines of all pods with label `app=fastapi-service` in namespace `production`.
<details>
<summary>Solution</summary>

```bash
kubectl logs -l app=fastapi-service -n production --tail=100 -f
```
</details>

### Exercise 2 (Easy): Instant Rollback Command
**Task**: Write the `kubectl` command to undo the most recent deployment rollout for `deployment/fastapi-api` in namespace `production`.
<details>
<summary>Solution</summary>

```bash
kubectl rollout undo deployment/fastapi-api -n production
```
</details>

### Exercise 3 (Medium): Emergency Circuit Breaker Endpoint
**Task**: Implement an administrative route `/admin/circuit-breaker` that sets an in-memory boolean flag to drop incoming non-critical requests with HTTP 503.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, HTTPException, status

app = FastAPI()
CIRCUIT_OPEN = False

@app.post("/admin/circuit-breaker")
async def toggle_circuit(open_state: bool):
    global CIRCUIT_OPEN
    CIRCUIT_OPEN = open_state
    return {"circuit_open": CIRCUIT_OPEN}

@app.get("/recommendations")
async def get_recommendations():
    if CIRCUIT_OPEN:
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="Emergency load shedding active, recommendations temporarily disabled"
        )
    return {"items": [1, 2, 3]}
```
</details>

### Exercise 4 (Medium): Identify Long-Running PostgreSQL Queries
**Task**: Write an async Python function that queries `pg_stat_activity` and prints any query running longer than 5 seconds.
<details>
<summary>Solution</summary>

```python
from sqlalchemy.ext.asyncio import create_async_engine
from sqlalchemy import text

async def check_slow_queries(engine):
    sql = text("""
        SELECT pid, now() - query_start AS duration, query 
        FROM pg_stat_activity 
        WHERE (now() - query_start) > interval '5 seconds'
        AND state != 'idle';
    """)
    async with engine.connect() as conn:
        result = await conn.execute(sql)
        for row in result.fetchall():
            print(f"PID {row.pid} running for {row.duration}: {row.query[:100]}...")
```
</details>

### Exercise 5 (Hard): In-Memory Tracemalloc Profiling Endpoint
**Task**: Build a secured endpoint `/admin/debug/heap` that starts `tracemalloc`, takes a snapshot, and returns the top 5 memory allocation lines in the Python process.
<details>
<summary>Solution</summary>

```python
import tracemalloc
from fastapi import FastAPI, Header, HTTPException, status

app = FastAPI()
tracemalloc.start()

@app.get("/admin/debug/heap")
async def get_heap_allocations(x_admin_secret: str = Header(...)):
    if x_admin_secret != "super-admin-emergency-token":
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN)

    snapshot = tracemalloc.take_snapshot()
    top_stats = snapshot.statistics("lineno")

    results = []
    for stat in top_stats[:5]:
        results.append({
            "traceback": str(stat.traceback),
            "size_kb": round(stat.size / 1024, 2),
            "count": stat.count
        })

    return {"top_allocations": results}
```
</details>

### Exercise 6 (Hard): Automated Canary Rollback Watcher Script
**Task**: Write a Python script that polls a canary service's error rate every 2 seconds for 1 minute; if error rate exceeds 3%, automatically invoke `kubectl rollout undo`.
<details>
<summary>Solution</summary>

```python
import time
import subprocess
import httpx

CANARY_URL = "https://canary.api.enterprise.internal/health"
DEPLOYMENT = "deployment/fastapi-service"
NAMESPACE = "production"

def monitor_and_guard_canary():
    errors = 0
    total = 0
    start_time = time.time()
    
    with httpx.Client(timeout=1.5) as client:
        while time.time() - start_time < 60:
            total += 1
            try:
                resp = client.get(CANARY_URL)
                if resp.status_code >= 500:
                    errors += 1
            except Exception:
                errors += 1

            error_rate = errors / total
            if total > 10 and error_rate > 0.03:
                print(f"CRITICAL: Canary error rate {error_rate:.2%} exceeded 3%! Initiating rollback...")
                subprocess.run(["kubectl", "rollout", "undo", DEPLOYMENT, "-n", NAMESPACE], check=True)
                print("Rollback triggered successfully!")
                return False

            time.sleep(2.0)

    print("Canary validation passed with acceptable error rate.")
    return True

if __name__ == "__main__":
    monitor_and_guard_canary()
```
</details>
