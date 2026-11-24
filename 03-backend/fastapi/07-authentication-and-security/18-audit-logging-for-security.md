# Module 07: Authentication & Security — 18: Audit Logging for Security (`18-audit-logging-for-security`)

> Security audit logging creates an immutable, tamper-evident forensic record of security-relevant events across an application lifecycle. In FastAPI, structured JSON audit logs are generated using ASGI middleware, Python `contextvars` for request correlation IDs, and asynchronous background pipelines, satisfying SOC2, HIPAA, and PCI-DSS compliance standards.

| Field | Value |
|---|---|
| **Category** | Authentication & Security |
| **Target Level** | Advanced / Production Engineer |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | structlog 24.4+, httpx 0.28+, pytest 9.1+ |

---

## Table of Contents
1. [Conceptual Overview](#1-conceptual-overview)
2. [Core Concepts & Architecture](#2-core-concepts--architecture)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Verified Code Examples](#5-verified-code-examples)
6. [Edge Cases, Pitfalls & Failure Modes](#6-edge-cases-pitfalls--failure-modes)
7. [Common Mistakes Table](#7-common-mistakes-table)
8. [Performance & Scalability Considerations](#8-performance--scalability-considerations)
9. [Security & OWASP Guardrails](#9-security--owasp-guardrails)
10. [Testing Strategies & Pytest Recipes](#10-testing-strategies--pytest-recipes)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Technical Interview Questions](#12-technical-interview-questions)
13. [Practical Exercises & Solutions](#13-practical-exercises--solutions)

---

## 1. Conceptual Overview

Operational logging tracks application health (e.g. CPU spikes, database timeouts). In contrast, **Security Audit Logging** answers five fundamental forensic questions:
1. **Who**: The authenticated subject (`user_id`, `client_id`, or `anonymous`).
2. **What**: The specific action attempted (`auth.login`, `user.delete`, `billing.refund`).
3. **When**: Precise UTC ISO 8601 timestamp.
4. **Where**: Client IP, User-Agent, endpoint path, and geo-location.
5. **Outcome**: `SUCCESS`, `DENIED`, or `ERROR` with failure reasons.

Audit logs must be structured (JSON), correlated across distributed microservices via `correlation_id`, and strictly scrubbed of Personally Identifiable Information (PII) and credentials.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               Structured Audit Logging Pipeline Architecture                      |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  Client Request ──> Header: "X-Correlation-ID: 7a8b-9c0d"                         |
|                            │                                                      |
|                            ▼                                                      |
|              [ Correlation ID Middleware ]                                        |
|              - Extracts or generates UUID4 correlation ID                         |
|              - Binds to thread/task local `contextvars`                           |
|                            │                                                      |
|                            ▼                                                      |
|              [ Security Dependencies / Handlers ]                                 |
|              - Execute authentication / authorization                             |
|              - Dispatch structured audit event:                                   |
|                audit_logger.info("auth.login", outcome="SUCCESS", user="u1")       |
|                            │                                                      |
|                            ▼                                                      |
|              [ JSON Formatter / Log Sanitizer ]                                   |
|              - Redacts passwords, tokens, and PII                                 |
|              - Emits newline-delimited JSON to stdout / SIEM                      |
|                                                                                   |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **Python `contextvars`**:
   - ASGI request handling is asynchronous and cooperative. Traditional thread-local storage (`threading.local`) fails because multiple concurrent async requests share the same thread.
   - Python's `contextvars.ContextVar` provides task-local isolation, ensuring `correlation_id` and `client_ip` remain bound to the specific coroutine task throughout its lifecycle.
2. **Zero-Blocking Log Emission**:
   - Audit logging must not block request completion. Writing logs to disk synchronously halts the asyncio event loop. Production systems stream logs to `stdout` in JSON format, collected by container daemons (FluentBit, Vector) and shipped asynchronously to a SIEM (Elasticsearch, OpenSearch, Datadog).

---

## 4. Syntax & API Reference

```python
import json
import logging
from contextvars import ContextVar
from datetime import datetime, timezone
from fastapi import Request

correlation_id_ctx: ContextVar[str] = ContextVar("correlation_id", default="")

class AuditLogger:
    def __init__(self, name: str = "security.audit"):
        self.logger = logging.getLogger(name)

    def log_event(self, action: str, outcome: str, actor_id: str, request: Request, **extra):
        event = {
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "event_type": "audit",
            "action": action,
            "outcome": outcome,
            "actor": {"id": actor_id, "ip": request.client.host if request.client else "unknown"},
            "correlation_id": correlation_id_ctx.get(),
            "path": request.url.path,
            "method": request.method,
            "details": extra,
        }
        self.logger.info(json.dumps(event))
```

---

## 5. Verified Code Examples

### Example 1: Minimal Audit Logger with ContextVar Correlation ID

```python
from contextvars import ContextVar
import json
import logging
import uuid
from fastapi import FastAPI, Request
from starlette.middleware.base import BaseHTTPMiddleware

correlation_id_var: ContextVar[str] = ContextVar("correlation_id", default="")
logger = logging.getLogger("audit")
logger.setLevel(logging.INFO)

app = FastAPI()

class CorrelationMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next):
        corr_id = request.headers.get("X-Correlation-ID") or str(uuid.uuid4())
        correlation_id_var.set(corr_id)
        response = await call_next(request)
        response.headers["X-Correlation-ID"] = corr_id
        return response

app.add_middleware(CorrelationMiddleware)

@app.post("/auth/login")
async def login(request: Request):
    # Simulated audit log emission
    audit_record = {
        "action": "auth.login",
        "status": "SUCCESS",
        "user_id": "usr_99",
        "correlation_id": correlation_id_var.get(),
    }
    logger.info(json.dumps(audit_record))
    return {"status": "authenticated"}
```

### Example 2: Enterprise Structured Audit Event Dispatcher

```python
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
import json
import logging
from typing import Annotated, Any
from fastapi import APIRouter, Depends, HTTPException, Request, status
from pydantic import BaseModel

audit_log = logging.getLogger("enterprise.audit")

@dataclass(frozen=True, slots=True)
class AuditEvent:
    timestamp: str
    action: str
    outcome: str  # SUCCESS, FAILURE, DENIED
    actor_id: str
    target_id: str | None
    client_ip: str
    user_agent: str
    metadata: dict[str, Any]

def record_audit(event: AuditEvent):
    audit_log.info(json.dumps(asdict(event)))

router = APIRouter(prefix="/admin")

class UserRoleUpdate(BaseModel):
    new_role: str

@router.put("/users/{target_id}/role")
async def update_user_role(
    target_id: str,
    update: UserRoleUpdate,
    request: Request,
):
    operator_id = "admin_alice"  # extracted from auth dependency
    
    # Audit log the privilege elevation event
    event = AuditEvent(
        timestamp=datetime.now(timezone.utc).isoformat(),
        action="rbac.role_change",
        outcome="SUCCESS",
        actor_id=operator_id,
        target_id=target_id,
        client_ip=request.client.host if request.client else "unknown",
        user_agent=request.headers.get("user-agent", "unknown"),
        metadata={"new_role": update.new_role},
    )
    record_audit(event)

    return {"status": "role_updated", "target": target_id, "role": update.new_role}
```

### Example 3: Edge Case / Boundary — Automated Redaction of Sensitive Fields

```python
SENSITIVE_KEYS = {"password", "token", "secret", "cvv", "credit_card", "ssn", "authorization"}

def scrub_sensitive_dict(data: dict) -> dict:
    cleaned = {}
    for k, v in data.items():
        if any(sens in k.lower() for sens in SENSITIVE_KEYS):
            cleaned[k] = "[REDACTED]"
        elif isinstance(v, dict):
            cleaned[k] = scrub_sensitive_dict(v)
        else:
            cleaned[k] = v
    return cleaned
```

### Example 4: Failure Case — Logging Authentication Failures with Threat Telemetry

```python
# When an attacker fails authentication 5 times:
# Emitted Structured Log:
# {
#   "timestamp": "2026-03-31T12:00:00Z",
#   "event_type": "security_alert",
#   "action": "auth.login.failed",
#   "outcome": "DENIED",
#   "actor_id": "unknown",
#   "attempted_username": "admin",
#   "client_ip": "198.51.100.42",
#   "reason": "invalid_credentials",
#   "threat_indicator": "potential_credential_stuffing"
# }
```

### Example 5: Idiomatic Modern Pattern with Python 3.12+ `structlog`

```python
import structlog
from fastapi import FastAPI, Request

structlog.configure(
    processors=[
        structlog.processors.TimeStamper(fmt="iso"),
        structlog.processors.JSONRenderer(),
    ]
)
log = structlog.get_logger("security")

app = FastAPI()

@app.post("/vault/access")
async def access_vault(request: Request):
    log.info(
        "secret_vault.access",
        outcome="SUCCESS",
        vault_id="vlt_01",
        ip=request.client.host if request.client else "127.0.0.1",
    )
    return {"vault": "unlocked"}
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Logging Plaintext Passwords during Failed Logins**: Developers logging `f"Failed login attempt with password: {pw}"` commit a critical compliance violation. Passwords must NEVER appear in logs under any circumstances.
2. **Blocking Log Transports**: Using HTTP webhooks inside request handlers to synchronously POST audit logs to third-party APIs destroys application throughput. If the log collector slows down, API endpoints time out. Always write to stdout or use async queues.
3. **Log Injection Attacks**: If user input contains raw newline characters (`\n`), attackers can forge fake log lines in text logs. Writing strictly serialized JSON (`json.dumps()`) neutralizes log injection because newlines are automatically escaped as `\n`.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Logging raw request body without redaction | Captures passwords, credit cards, and PII | GDPR/HIPAA/PCI compliance violation | Scrub sensitive keys via recursive sanitizer |
| 2. Using `threading.local` for correlation IDs | Async tasks share threads; IDs leak between users | Corrupted request correlation | Use `contextvars.ContextVar` |
| 3. Unstructured text logs (`logger.info("User logged in")`) | Difficult to query and alert on in SIEM platforms | Cannot parse or aggregate security metrics | Emit structured JSON objects |
| 4. Writing logs synchronously over network | Network latency stalls FastAPI event loop | Severe endpoint latency spikes | Log to stdout; collect via FluentBit/Vector |
| 5. Omitting client IP and User-Agent | Forensic investigations cannot track attacker origin | Incomplete security audit trail | Include `client_ip` and `user-agent` in metadata |
| 6. Not logging failed authorization attempts (403) | Attackers probe endpoints undetected | Blind to privilege escalation attempts | Log every HTTP 403 Forbidden event |

---

## 8. Performance & Scalability Considerations

- **Stdout Serialization**: Serializing a compact JSON dictionary with `json.dumps()` takes <0.005ms per log line.
- **Log Aggregator Offloading**: Writing to standard output delegates all disk I/O, buffering, compression, and network transport to native sidecar daemons (e.g. Vector written in Rust), keeping Python memory footprints tiny.

---

## 9. Security & OWASP Guardrails

- **OWASP Top 10 A09:2021 Security Logging and Monitoring Failures**: Ensure all security-critical operations are logged with sufficient context to detect suspicious activity.
- **Tamper-Evident Storage**: Audit logs shipped to cloud storage (AWS S3) must be configured with Object Lock (WORM — Write Once, Read Many) to prevent compromised accounts from erasing forensic evidence.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
import json
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_audit_log_emitted_on_role_change(app: FastAPI, caplog):
    caplog.set_level("INFO")
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        res = await client.put("/admin/users/usr_42/role", json={"new_role": "admin"})
        assert res.status_code == 200

        # Verify audit log JSON was captured
        audit_records = [json.loads(record.message) for record in caplog.records if "rbac.role_change" in record.message]
        assert len(audit_records) == 1
        assert audit_records[0]["target_id"] == "usr_42"
        assert audit_records[0]["outcome"] == "SUCCESS"
```

---

## 11. Real-World Case Study

A healthcare API was breached by a rogue internal contractor who extracted 10,000 patient medical records over three months. When federal investigators audited the system, they discovered that while the database recorded patient reads, the application logs did not capture the user identity or IP address associated with the reads.
- **Remediation**:
  1. Mandated structured JSON audit events on every record access: `patient.record.read`.
  2. Captured `user_id`, `patient_id`, `role`, and `purpose_of_use`.
  3. Streamed logs in real time to an immutable, append-only OpenSearch cluster, meeting full HIPAA compliance.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: Why must asynchronous Python applications use `contextvars` instead of `threading.local` for tracking request correlation IDs?</summary>

**Answer**: In an async ASGI framework like FastAPI, hundreds of concurrent requests are multiplexed cooperatively across a single OS thread via the asyncio event loop. `threading.local` associates data with the operating system thread, meaning all concurrent coroutines running on that thread would share and overwrite each other's state. `contextvars.ContextVar` associates data with the specific coroutine execution task, guaranteeing complete isolation across concurrent async requests.
</details>

<details>
<summary>Q2: What is "Log Injection" (CWE-117), and how is it prevented?</summary>

**Answer**: Log injection occurs when untrusted user input containing newline characters (`\n` or `\r\n`) is concatenated into plain-text log files. An attacker can inject fake log entries (e.g. `\n2026-03-31 INFO: User admin authenticated`) to deceive security analysts. Emitting logs as structured JSON (`json.dumps()`) prevents log injection because newlines and control characters inside strings are automatically escaped as literal `\n`.
</details>

<details>
<summary>Q3: What are the key differences between application operational logs and security audit logs?</summary>

**Answer**:
- **Operational Logs**: Track system performance, stack traces, warnings, and latency metrics; intended for developers and SREs; typically retained for 7 to 30 days.
- **Security Audit Logs**: Track authentication, authorization, access to sensitive data, and privilege modifications; answer "Who did what, when, and where"; intended for compliance, security teams, and legal forensics; strictly scrubbed of credentials; retained for 1 to 7 years in immutable storage.
</details>

<details>
<summary>Q4: What data must NEVER appear in security audit logs?</summary>

**Answer**: Plaintext passwords, password reset tokens, JWT signatures, API secrets, private cryptographic keys, full credit card numbers (PAN), CVVs, and unnecessary PII (Social Security numbers, national IDs).
</details>

<details>
<summary>Q5: Why is writing audit logs to `stdout` preferred over writing directly to database tables or files inside container environments?</summary>

**Answer**: In Kubernetes and cloud-native architectures, containers are ephemeral and should follow the 12-Factor App rule for logs (treating logs as event streams to stdout). Logging to a database adds transactional overhead, connection pool pressure, and latency to every request. Container runtimes redirect stdout to log collectors (FluentBit, Vector) that handle batching, compression, and network delivery out-of-band without impacting application throughput.
</details>

<details>
<summary>Q6: How do you track a single user interaction that traverses five distributed microservices?</summary>

**Answer**: Use an incoming `X-Correlation-ID` (or `traceparent` via OpenTelemetry). The edge gateway generates a unique UUID if absent. Each microservice extracts this header, binds it to its local `ContextVar`, injects it into all emitted audit logs, and forwards the identical header on all outbound HTTP/gRPC requests.
</details>

<details>
<summary>Q7: What is the WORM (Write Once, Read Many) storage model, and why is it required for audit logs?</summary>

**Answer**: WORM prevents stored data from being deleted, overwritten, or modified for a retention period. If an attacker gains root access to an infrastructure, their first action is often deleting logs to conceal their tracks. WORM policies (such as AWS S3 Object Lock) prevent even root administrators from deleting audit logs during the retention window.
</details>

<details>
<summary>Q8: How should authorization failures (HTTP 403) be logged compared to normal endpoint errors?</summary>

**Answer**: HTTP 403 Forbidden events should be logged with dedicated `event_type="security_alert"` containing the actor ID, the required permission, the actor's actual permissions, the requested URI, and client IP. A sudden spike in 403 events from a single IP or user account indicates active endpoint enumeration or privilege escalation attacks, triggering SIEM alerts.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Setup a ContextVar correlation ID middleware</summary>

**Problem**: Create a middleware setting a unique UUID in a `ContextVar` and attaching it to response headers.

**Solution**:
```python
from contextvars import ContextVar
import uuid
from fastapi import FastAPI, Request
from starlette.middleware.base import BaseHTTPMiddleware

corr_var: ContextVar[str] = ContextVar("corr_id", default="")
app = FastAPI()

class CorrMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next):
        corr_id = request.headers.get("X-Correlation-ID") or str(uuid.uuid4())
        corr_var.set(corr_id)
        res = await call_next(request)
        res.headers["X-Correlation-ID"] = corr_id
        return res

app.add_middleware(CorrMiddleware)
```
</details>

<details>
<summary>Exercise 2 (Easy): Build a dictionary scrubber for sensitive keys</summary>

**Problem**: Write a function that replaces values of `"password"` and `"token"` with `"***"`.

**Solution**:
```python
def scrub_secrets(data: dict) -> dict:
    return {
        k: ("***" if k.lower() in ("password", "token") else v)
        for k, v in data.items()
    }
```
</details>

<details>
<summary>Exercise 3 (Medium): Log failed authentication attempts</summary>

**Problem**: Write a helper function `log_auth_failure(username, ip, reason)` that outputs formatted JSON.

**Solution**:
```python
import json
import logging
from datetime import datetime, timezone

audit_logger = logging.getLogger("audit")

def log_auth_failure(username: str, ip: str, reason: str):
    record = {
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "action": "auth.failed",
        "username": username,
        "ip": ip,
        "reason": reason,
    }
    audit_logger.warning(json.dumps(record))
```
</details>

<details>
<summary>Exercise 4 (Medium): Build an Audit Event Decorator</summary>

**Problem**: Create a decorator `@audit_action("document.delete")` that logs start, outcome, and duration.

**Solution**:
```python
from functools import wraps
import time
import json
import logging

audit_logger = logging.getLogger("audit")

def audit_action(action_name: str):
    def decorator(func):
        @wraps(func)
        async def wrapper(*args, **kwargs):
            t0 = time.perf_counter()
            try:
                result = await func(*args, **kwargs)
                duration = round(time.perf_counter() - t0, 4)
                audit_logger.info(json.dumps({"action": action_name, "outcome": "SUCCESS", "duration_sec": duration}))
                return result
            except Exception as e:
                duration = round(time.perf_counter() - t0, 4)
                audit_logger.error(json.dumps({"action": action_name, "outcome": "ERROR", "error": str(e), "duration_sec": duration}))
                raise
        return wrapper
    return decorator
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Global 403/401 Audit Exception Handler</summary>

**Problem**: Attach an exception handler in FastAPI that automatically captures any `HTTPException` with 401 or 403 and writes a structured audit log before returning the response.

**Solution**:
```python
import json
import logging
from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import JSONResponse

audit_logger = logging.getLogger("security.audit")
app = FastAPI()

@app.exception_handler(HTTPException)
async def audit_http_exceptions(request: Request, exc: HTTPException):
    if exc.status_code in (401, 403):
        audit_record = {
            "event": "security_violation",
            "status_code": exc.status_code,
            "detail": exc.detail,
            "path": request.url.path,
            "ip": request.client.host if request.client else "unknown",
            "method": request.method,
        }
        audit_logger.warning(json.dumps(audit_record))
    return JSONResponse(status_code=exc.status_code, content={"detail": exc.detail}, headers=exc.headers)
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct an Asynchronous Audit Buffer with Background Flushing</summary>

**Problem**: Build an in-memory queue that buffers audit events and flushes them to stdout in batches of 100 or every 2 seconds via an async background worker.

**Solution**:
```python
import asyncio
import json

class AsyncAuditBuffer:
    def __init__(self, batch_size: int = 100, flush_interval: float = 2.0):
        self.queue: asyncio.Queue = asyncio.Queue()
        self.batch_size = batch_size
        self.flush_interval = flush_interval

    async def emit(self, event: dict):
        await self.queue.put(event)

    async def run_worker(self):
        while True:
            batch = []
            try:
                # Wait for at least one item
                item = await asyncio.wait_for(self.queue.get(), timeout=self.flush_interval)
                batch.append(item)
                # Drain up to batch_size
                while len(batch) < self.batch_size and not self.queue.empty():
                    batch.append(self.queue.get_nowait())
            except asyncio.TimeoutError:
                pass

            if batch:
                for record in batch:
                    print(json.dumps(record))
```
</details>
