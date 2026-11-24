---
title: "Error Tracking and Alerting with Sentry in FastAPI"
description: "Implement real-time error tracking and alerting in FastAPI using Sentry: exception grouping, breadcrumb trails, user context binding, PII scrubbing with before_send, and PagerDuty/Slack routing."
fastapi_version: "0.115.x+"
python_version: "3.12+"
pydantic_version: "v2.10+"
tested_with: "sentry-sdk 2.19+, fastapi 0.115+, pytest 8.3+"
free_threading_status: "Fully Compatible"
---

# Error Tracking and Alerting with Sentry in FastAPI

## 1. Conceptual Foundation & Mental Model

### Log Files vs. Real-Time Error Tracking
While structured logs record chronological events, relying on logs alone to detect production outages is notoriously inefficient. Searching through millions of log lines to identify a newly introduced regression requires manual query construction and alert rule maintenance.

**Error Tracking Platforms** (such as Sentry, GlitchTip, or Honeybadger) provide real-time exception intelligence:
1. **Automated Error Aggregation & Deduplication**: Instead of creating 10,000 separate alerts for 10,000 occurrences of a bug, Sentry analyzes stack traces and generates a single **Issue**, tracking occurrence frequency, affected users, and regression releases.
2. **Breadcrumb Forensics**: Sentry automatically records the sequence of actions that occurred *prior* to a crash (recent SQL queries, outgoing HTTP requests, console logs, user clicks), providing instant root-cause diagnostics.
3. **Release & Commit Tracking**: Directly links crashes to the specific Git commit and pull request that introduced the bug.

```
+-------------------------------------------------------------------------------+
|                       SENTRY ERROR REPORTING LIFECYCLE                        |
|                                                                               |
|  [HTTP Request Ingress: POST /checkout]                                       |
|         |                                                                     |
|         +---> [Breadcrumb 1: User navigated to /checkout]                     |
|         +---> [Breadcrumb 2: SQL SELECT * FROM cart WHERE id = 'c1']          |
|         +---> [Breadcrumb 3: HTTP POST https://api.stripe.com/v1/charge]     |
|         |                                                                     |
|         v (Crash: ZeroDivisionError or StripeCardError)                       |
|  [Sentry FastApiIntegration captures unhandled exception]                    |
|         |                                                                     |
|         v                                                                     |
|  [before_send hook: Scrubs passwords, bearer tokens, and credit cards]       |
|         |                                                                     |
|         v (Asynchronously sent to Sentry Ingestion Gateway)                   |
|  [Sentry Grouping Engine: Matches fingerprint -> Updates Issue #402]          |
|         |                                                                     |
|         v (Triggers Alert Rule)                                               |
|  [Slack Notification / PagerDuty Call to On-Call Engineer]                    |
+-------------------------------------------------------------------------------+
```

---

## 2. Architecture & Internal Mechanics

Understanding how the Sentry SDK hooks into Starlette's ASGI middleware pipeline to capture exceptions and record breadcrumbs without blocking the event loop.

```
+-----------------------------------------------------------------------------+
|                            FASTAPI APPLICATION                              |
|                                                                             |
|  [SentryAsgiMiddleware] (Intercepts all ASGI scope calls)                   |
|            |                                                                |
|            +---> [Records HTTP Breadcrumbs: Method, Path, Status]           |
|            +---> [Binds Scope Context: Headers, Client IP, Query Params]    |
|            |                                                                |
|            v                                                                |
|  [Route Handler / Service Layer]                                            |
|  - Developer adds custom breadcrumbs: sentry_sdk.add_breadcrumb(...)        |
|  - Attaches user context: sentry_sdk.set_user({"id": "usr_42"})             |
|            |                                                                |
|            v (Unhandled Exception Raised)                                   |
|  [Sentry Exception Hook]                                                    |
|  1. Captures full stack frame with local variables                          |
|  2. Collects preceding breadcrumbs                                          |
|  3. Runs `before_send` PII scrubber                                         |
+-----------------------------------------------------------------------------+
                                     |
                       Non-blocking Background Thread
                                     v
                        [Sentry Cloud / Self-Hosted]
```

### Automatic Framework Integrations
The modern `sentry-sdk` automatically configures integrations for the entire FastAPI ecosystem:
- `FastApiIntegration`: Captures unhandled route exceptions and maps HTTP metadata.
- `StarletteIntegration`: Injects request headers and query parameters.
- `SqlalchemyIntegration`: Records all database queries as breadcrumbs, sanitizing SQL parameters automatically.
- `HttpxIntegration` / `AioHttpIntegration`: Records all outgoing external API calls as network breadcrumbs.

---

## 3. Comprehensive Syntax & Core Mechanics

### Standard Sentry Initialization in FastAPI

```python
import os
import sentry_sdk
from sentry_sdk.integrations.fastapi import FastApiIntegration
from sentry_sdk.integrations.starlette import StarletteIntegration
from sentry_sdk.integrations.sqlalchemy import SqlalchemyIntegration

def setup_sentry() -> None:
    sentry_sdk.init(
        dsn=os.getenv("SENTRY_DSN"),
        environment=os.getenv("ENVIRONMENT", "production"),
        release=os.getenv("APP_VERSION", "v1.0.0"),
        integrations=[
            FastApiIntegration(),
            StarletteIntegration(transaction_style="endpoint"),
            SqlalchemyIntegration(),
        ],
        # Performance Monitoring (Traces)
        traces_sample_rate=0.10,  # Sample 10% of transactions for performance
        profiles_sample_rate=0.05, # Sample 5% for continuous profiling
        send_default_pii=False,    # NEVER send plain-text PII automatically!
    )
```

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: Production PII Scrubbing and Filtering Hook (`before_send`)
Sanitizing sensitive user information (tokens, authorization headers, credit cards) before sending reports to Sentry.

```python
# app/core/sentry/filters.py
import re
from typing import Any, Dict, Optional

SENSITIVE_KEYS = {"password", "token", "secret", "authorization", "cvv", "credit_card"}
BEARER_REGEX = re.compile(r"Bearer\s+[\w\.\-]+", re.IGNORECASE)

def scrub_sensitive_data(event: Dict[str, Any], hint: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    """Cleanse sensitive credentials and ignore benign expected exceptions."""
    # 1. Ignore benign, expected business exceptions (e.g. 404 EntityNotFound)
    if "exc_info" in hint:
        exc_type, exc_value, _ = hint["exc_info"]
        if exc_type.__name__ in ("EntityNotFoundError", "InvalidCredentialsError"):
            return None  # Drop event; do not log to Sentry!

    # 2. Scrub Request Headers
    request_data = event.get("request", {})
    headers = request_data.get("headers", {})
    if "authorization" in headers:
        headers["authorization"] = "[REDACTED_BEARER_TOKEN]"

    # 3. Scrub Local Stack Frame Variables
    for thread in event.get("threads", {}).get("values", []):
        for frame in thread.get("stacktrace", {}).get("frames", []):
            vars_dict = frame.get("vars", {})
            for var_key in list(vars_dict.keys()):
                if any(s in var_key.lower() for s in SENSITIVE_KEYS):
                    vars_dict[var_key] = "[REDACTED_VARIABLE]"

    return event
```

---

### Example 2: Binding User Context and Custom Tags in Middleware
Ensuring that when an error occurs, Sentry records which customer and organization experienced it.

```python
# app/core/sentry/middleware.py
import sentry_sdk
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.requests import Request

class SentryContextMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next):
        # Configure a fresh Sentry scope for this async request
        with sentry_sdk.configure_scope() as scope:
            # Set request tags
            scope.set_tag("http.method", request.method)
            scope.set_tag("http.route", request.url.path)

            # Extract user token or session (if authenticated)
            user_id = request.headers.get("X-User-ID")
            tenant_id = request.headers.get("X-Tenant-ID")

            if user_id:
                scope.set_user({
                    "id": user_id,
                    "ip_address": request.client.host if request.client else "unknown",
                })

            if tenant_id:
                scope.set_tag("tenant.id", tenant_id)

            response = await call_next(request)
            return response
```

---

### Example 3: Adding Custom Breadcrumbs in Business Workflows
Adding breadcrumbs to track critical steps in a payment process before a failure.

```python
# app/services/checkout_service.py
import sentry_sdk

class CheckoutService:
    async def execute_checkout(self, user_id: str, cart_id: str, amount: float):
        # 1. Record milestone breadcrumb
        sentry_sdk.add_breadcrumb(
            category="checkout",
            message=f"Starting checkout for user {user_id}",
            level="info",
            data={"cart_id": cart_id, "amount": amount},
        )

        # 2. Inventory check
        sentry_sdk.add_breadcrumb(
            category="inventory",
            message="Reserving inventory in warehouse",
            level="debug",
        )

        # Simulated unexpected crash
        if amount > 10000.0:
            raise RuntimeError("Payment gateway rejected unverified high-value wire")

        return "SUCCESS"
```

---

### Example 4: Manual Exception Capture with Custom Fingerprinting
Grouping similar errors together or capturing handled exceptions without letting them crash the request.

```python
# app/api/v1/integrations.py
from fastapi import APIRouter
import sentry_sdk

router = APIRouter(prefix="/integrations", tags=["Integrations"])

@router.post("/sync-crm")
async def sync_crm_contact(contact_id: str):
    try:
        # Simulated external API call
        raise TimeoutError("Salesforce API connection timeout")
    except TimeoutError as exc:
        # Manually capture exception to Sentry without returning a 500 to the client
        with sentry_sdk.push_scope() as scope:
            # Custom fingerprint: groups all Salesforce timeouts into a single issue
            scope.fingerprint = ["salesforce-sync", "timeout"]
            scope.set_extra("contact_id", contact_id)
            sentry_sdk.capture_exception(exc)

        # Gracefully handle error for the user
        return {"status": "QUEUED_FOR_RETRY", "detail": "CRM sync delayed"}
```

---

### Example 5: Testing Sentry Integration with In-Memory Transport
Validating that exceptions are properly captured and scrubbed in pytest.

```python
# tests/test_sentry_capture.py
import pytest
import sentry_sdk
from sentry_sdk.transport import Transport

class InMemoryTransport(Transport):
    def __init__(self):
        self.events = []

    def capture_event(self, event):
        self.events.append(event)

@pytest.fixture
def mock_sentry():
    transport = InMemoryTransport()
    sentry_sdk.init(
        dsn="https://public@sentry.example.com/1",
        transport=transport,
    )
    yield transport
    sentry_sdk.init()  # Reset

def test_sentry_captures_unhandled_error(mock_sentry):
    try:
        1 / 0
    except ZeroDivisionError as exc:
        sentry_sdk.capture_exception(exc)

    sentry_sdk.flush()
    assert len(mock_sentry.events) == 1
    event = mock_sentry.events[0]
    assert event["exception"]["values"][0]["type"] == "ZeroDivisionError"
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Reporting Expected 4xx Errors to Sentry**: Tracking `HTTPException(404)` or `HTTPException(401)` in Sentry floods the dashboard with user input errors, burying critical 500 crashes. Filter out 4xx exceptions using `before_send`.
2. **Synchronous Sentry Network Calls (`flush()`)**: Calling `sentry_sdk.flush()` inside an endpoint halts the request thread until Sentry's cloud servers acknowledge the HTTP write. Only call `flush()` during application lifespan shutdown (`app.state.sentry.flush()`).
3. **Leaking Database Passwords in SQL Breadcrumbs**: Default SQL string formatting can leak plain-text parameters into breadcrumbs. Ensure `SqlalchemyIntegration` is enabled with parameter sanitization active.
4. **Infinite Exception Recursion**: If the `before_send` hook itself raises an unhandled exception, Sentry's event processor crashes silently and drops the error report. Keep `before_send` hooks strictly defensive.

---

## 6. Common Mistakes Table

| Mistake | Root Cause | Impact | Correct Approach |
| :--- | :--- | :--- | :--- |
| **Sending Passwords/Tokens to Sentry** | `send_default_pii=True` with no scrubbing | Data compliance breach (GDPR, PCI-DSS, HIPAA). | Implement a strict `before_send` scrubbing function. |
| **Logging 404s and 422s to Sentry** | Unfiltered exception handling | Alert fatigue; quota exhaustion; Sentry bill spikes. | Filter out client-side 4xx errors in `before_send`. |
| **Calling `sentry_sdk.flush()` in Routes** | Misunderstanding SDK mechanics | Adds 100–300ms network latency to API responses. | Let the SDK buffer and export events asynchronously. |
| **Omitting Git Commit Releases** | Not setting `release=...` in init | Sentry cannot associate errors with specific deployments or PRs. | Pass Git SHA via `release=os.getenv("GIT_COMMIT_SHA")`. |
| **Missing User Context** | Forgetting `scope.set_user()` | Cannot determine how many customers are affected by a crash. | Bind `user_id` and `tenant_id` in authentication middleware. |
| **Random Exception Grouping** | Variable error messages (e.g. including timestamps) | Sentry creates 5,000 separate issues for the same bug. | Use custom `scope.fingerprint` to group related exceptions. |

---

## 7. Performance & Scalability Considerations

- **Sampling Strategy (`traces_sample_rate`)**: In high-throughput services (10,000+ RPS), set `traces_sample_rate` to `0.01` or `0.05` (1% to 5%). Errors (`capture_exception`) are always captured at 100%, while performance spans are sampled to conserve bandwidth.
- **Asynchronous Background Worker**: The Sentry SDK runs an internal non-blocking background queue that batches and flushes events over HTTPS, introducing less than 0.5ms of overhead to the calling request.
- **Python 3.13+ Free-Threading**: The Sentry background transport thread executes concurrently across native OS threads without GIL serialization.

---

## 8. Security & OWASP Guardrails

- **OWASP A09 (Security Logging Failures - Alerting Routing)**: Critical security events (e.g., repeated authentication brute-force attacks, secret rotation failures) should trigger high-priority alerts routed directly to PagerDuty or security Slack channels.
- **Zero-PII Storage**: Configure Sentry data retention policies to automatically delete error logs older than 30 or 90 days for compliance.

---

## 9. Testing Strategies & Pytest Recipes

```python
# tests/test_sentry_filter_rules.py
from app.core.sentry.filters import scrub_sensitive_data

def test_scrub_sensitive_data_redacts_auth_header():
    mock_event = {
        "request": {
            "headers": {"authorization": "Bearer secret_jwt_token_12345"}
        }
    }
    cleaned = scrub_sensitive_data(mock_event, hint={})
    assert cleaned["request"]["headers"]["authorization"] == "[REDACTED_BEARER_TOKEN]"

def test_scrub_sensitive_data_drops_404_errors():
    class EntityNotFoundError(Exception): pass
    
    mock_event = {"message": "User not found"}
    hint = {"exc_info": (EntityNotFoundError, EntityNotFoundError("404"), None)}
    
    # Must return None to drop event completely
    assert scrub_sensitive_data(mock_event, hint) is None
```

---

## 10. Real-World Case Study / Production Incident

An international SaaS accounting tool deployed a minor database migration on a Friday afternoon. Within 5 minutes of release:
- A foreign key constraint failed on a newly introduced `currency_code` column.
- Over 2,500 active invoices failed to generate.

**How Sentry Saved the Deployment**:
1. Sentry's automated alert rule fired into the `#engineering-incidents` Slack channel: `"High Issue Frequency: ForeignKeyViolation in InvoiceService"`.
2. The alert directly tagged the release version `v2.4.1` and linked the exact GitHub commit and author.
3. Sentry's breadcrumb trail showed:
   - `User usr_99 clicked 'Generate Invoice'`
   - `SQL Query: INSERT INTO invoices (currency_code) VALUES ('EUR')`
   - `DB Error: Key (currency_code)=(EUR) is not present in table currencies`
4. The on-call engineer identified that the `currencies` seed table had not been updated in production.
5. The seed script was executed, resolving the issue within 6 minutes of the initial alert.

---

## 11. Technical Interview Questions

<details>
<summary>1. What is the difference between a log aggregator (like ELK/Loki) and an error tracker (like Sentry)?</summary>
A log aggregator collects raw, unstructured or structured streams of text lines generated by all services, useful for general system querying and auditing. An error tracking platform specifically monitors exceptions and application crashes, automatically grouping duplicate occurrences into actionable "Issues", capturing pre-crash breadcrumbs, attaching local variable stack frames, tracking affected users, and alerting on-call engineers in real time.
</details>

<details>
<summary>2. What is a "Breadcrumb" in Sentry, and why is it useful?</summary>
A breadcrumb is a timestamped record of an event or action that occurred shortly *before* an exception was raised. Breadcrumbs automatically record recent database queries, outbound HTTP requests, navigation events, and custom logs. When a crash occurs, breadcrumbs provide a chronological timeline of user and system behavior leading up to the failure.
</details>

<details>
<summary>3. How does Sentry's `before_send` hook protect user privacy and sensitive data?</summary>
`before_send` is a callback function executed immediately before an error payload is sent to Sentry's servers. It allows developers to programmatically inspect, modify, or drop the event payload. Developers use `before_send` to strip sensitive data (passwords, JWT tokens, credit card numbers, PII) or completely discard benign business exceptions (e.g., 404 Not Found).
</details>

<details>
<summary>4. Why should you avoid tracking expected 4xx HTTP exceptions in Sentry?</summary>
HTTP 4xx errors (such as 401 Unauthorized, 404 Not Found, and 422 Validation Error) represent client-side errors and expected business validation failures, not application code crashes. Sending 4xx errors to Sentry causes alert fatigue, pollutes issue trackers, and rapidly exhausts quota limits on commercial SaaS monitoring plans.
</details>

<details>
<summary>5. What is Sentry "Fingerprinting", and how can it be customized?</summary>
Fingerprinting is the algorithm Sentry uses to group related exception events into a single Issue based on stack trace similarity. When exceptions contain dynamic strings (e.g., `"Timeout on worker PID 102"`), Sentry might mistakenly group them into separate issues. Setting `scope.fingerprint = ["custom-tag"]` forces Sentry to group all matching events into a single, cohesive issue.
</details>

<details>
<summary>6. How do you associate Sentry errors with specific users?</summary>
By calling `sentry_sdk.set_user({"id": user_id, "email": email})` in authentication middleware. When an exception occurs, Sentry records the user metadata, allowing engineers to filter issues by specific customer accounts and determine the blast radius of a bug.
</details>

<details>
<summary>7. What is the impact of calling `sentry_sdk.flush()` inside an API route handler?</summary>
`flush()` is a synchronous blocking operation that forces the SDK to wait for confirmation that all buffered events have been delivered over the network to Sentry. Calling `flush()` inside an endpoint adds 100–500ms of unnecessary latency to the user's response. The SDK automatically manages background asynchronous delivery.
</details>

<details>
<summary>8. How do you track application performance (APM) using Sentry in FastAPI?</summary>
By configuring `traces_sample_rate` (e.g., `traces_sample_rate=0.10`) and using `FastApiIntegration`. Sentry automatically measures endpoint transaction durations, database query latency, and external HTTP call timings, presenting distributed waterfall charts similar to OpenTelemetry.
</details>

---

## 12. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Basic Sentry Initialization</summary>

**Task**: Write a function `init_test_sentry(dsn: str)` that initializes Sentry with `FastApiIntegration` and an environment tag `"staging"`.

**Solution**:
```python
import sentry_sdk
from sentry_sdk.integrations.fastapi import FastApiIntegration

def init_test_sentry(dsn: str):
    sentry_sdk.init(
        dsn=dsn,
        environment="staging",
        integrations=[FastApiIntegration()],
        traces_sample_rate=0.0,
    )
```
</details>

<details>
<summary>Exercise 2 (Easy): Manual Exception Capture</summary>

**Task**: Write an async function that catches a `ValueError`, manually reports it to Sentry with `sentry_sdk.capture_exception()`, and re-raises it.

**Solution**:
```python
import sentry_sdk

async def process_data(value: int):
    try:
        if value < 0:
            raise ValueError("Value cannot be negative")
    except ValueError as exc:
        sentry_sdk.capture_exception(exc)
        raise exc
```
</details>

<details>
<summary>Exercise 3 (Medium): Add Custom Sentry Breadcrumb</summary>

**Task**: Write a function `record_payment_step(step_name: str, tx_id: str)` that logs a Sentry breadcrumb under category `"billing"` with level `"info"`.

**Solution**:
```python
import sentry_sdk

def record_payment_step(step_name: str, tx_id: str):
    sentry_sdk.add_breadcrumb(
        category="billing",
        message=f"Payment step: {step_name}",
        level="info",
        data={"transaction_id": tx_id},
    )
```
</details>

<details>
<summary>Exercise 4 (Medium): Custom Error Fingerprinting</summary>

**Task**: Catch an external API timeout and send it to Sentry with a custom fingerprint `["external-api", "stripe-timeout"]` so all Stripe timeouts are grouped together.

**Solution**:
```python
import sentry_sdk

def report_stripe_timeout(exc: TimeoutError):
    with sentry_sdk.push_scope() as scope:
        scope.fingerprint = ["external-api", "stripe-timeout"]
        sentry_sdk.capture_exception(exc)
```
</details>

<details>
<summary>Exercise 5 (Hard): PII Header Redaction in `before_send`</summary>

**Task**: Write a `before_send(event, hint)` function that scrubs the `"x-api-key"` header from the request object, replacing it with `"[MASKED]"`.

**Solution**:
```python
def before_send_scrub_api_key(event, hint):
    request_data = event.get("request", {})
    headers = request_data.get("headers", {})
    if "x-api-key" in headers:
        headers["x-api-key"] = "[MASKED]"
    return event
```
</details>

<details>
<summary>Exercise 6 (Hard): FastAPI User Context Binding Middleware</summary>

**Task**: Write a Starlette middleware that extracts `user_id` from `request.headers.get("X-User-ID")` and binds it to Sentry scope using `sentry_sdk.set_user()`.

**Solution**:
```python
import sentry_sdk
from starlette.middleware.base import BaseHTTPMiddleware

class SentryUserMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request, call_next):
        user_id = request.headers.get("X-User-ID")
        with sentry_sdk.configure_scope() as scope:
            if user_id:
                scope.set_user({"id": user_id})
            else:
                scope.set_user(None)
            return await call_next(request)
```
</details>
