# Module 08: API Design — 13: Webhooks and Callbacks (`13-hubs-webhooks-and-callbacks`)

> In modern event-driven architectures, webhooks push asynchronous state changes to subscribers in real time, eliminating inefficient polling loops. FastAPI natively supports OpenAPI 3.1 `webhooks` declarations, route-level `callbacks`, cryptographic HMAC-SHA256 payload signing, and timestamped replay-attack prevention.

| Field | Value |
|---|---|
| **Category** | API Design & Standards |
| **Target Level** | Advanced / Production Architect |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | httpx 0.28+, pytest 9.1+, hmac/hashlib |

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

While standard REST operates via request-response, event-driven architectures require inverted communication where the server issues HTTP requests to client-registered endpoints:
- **Webhooks**: Outbound HTTP POST requests dispatched when an asynchronous event occurs (e.g. `payment.succeeded`, `invoice.created`).
- **Callbacks in OpenAPI**: OpenAPI 3.1 differentiates between:
  1. **Callbacks**: Outbound requests tied directly to a specific incoming API call (e.g., subscribing to a transaction and receiving a callback on completion).
  2. **Webhooks (`webhooks` object)**: Top-level event definitions completely decoupled from specific API requests, documenting events an external system might receive.
- **Security Guarantees**: Webhook receivers must verify that messages originate from the legitimate provider using **HMAC-SHA256 signatures** and prevent replay attacks using **timestamped headers**.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               Enterprise Webhook Delivery & Verification Engine                   |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  FastAPI Dispatcher Service                                                       |
|  - Event Occurs: `charge.completed`                                               |
|  - Constructs Payload: `{"event": "charge.completed", "amount": 100.0}`           |
|  - Timestamp: `t = 1711900000`                                                    |
|  - Computes HMAC: `sig = hmac_sha256(secret, t + "." + payload)`                 |
|  - Dispatches HTTP POST to subscriber URL with exponential retry queue            |
|  - Header: `X-Signature: t=1711900000,v1=a1b2c3d4...`                             |
|                                                                                   |
|                         │                                                         |
|                         ▼                                                         |
|  Client Webhook Receiver (FastAPI Endpoint)                                       |
|  1. Extract timestamp `t` from header. Assert: `now - t < 300` (Replay Defense)   |
|  2. Read raw unparsed body bytes.                                                 |
|  3. Recompute `expected = hmac_sha256(secret, t + "." + raw_bytes)`               |
|  4. Assert: `hmac.compare_digest(sig, expected)`                                  |
|  5. Return HTTP 200 OK immediately; enqueue event processing asynchronously        |
|                                                                                   |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **`FastAPI.webhooks` (OpenAPI 3.1)**:
   - FastAPI allows declaring an `APIRouter()` specifically dedicated to webhooks via `app.webhooks.add_api_route()`.
   - These routes do not mount HTTP listeners; instead, they populate the top-level `webhooks` section of `openapi.json`, generating documentation for event consumers.
2. **Signature Verification Mechanics**:
   - Signature checks MUST be performed on the raw incoming byte stream (`await request.body()`).
   - If JSON is deserialized into a Pydantic model and re-serialized, key ordering and whitespace changes will invalidate the cryptographic signature!

---

## 4. Syntax & API Reference

```python
from fastapi import APIRouter, FastAPI
from pydantic import BaseModel, HttpUrl

app = FastAPI()
# Dedicated router for OpenAPI 3.1 webhooks documentation
webhooks_router = APIRouter()

class InvoiceEvent(BaseModel):
    id: str
    amount: float
    status: str

@webhooks_router.post("new-invoice")
def new_invoice_event(event: InvoiceEvent):
    """Event emitted when a new customer invoice is generated."""

app.webhooks.include_router(webhooks_router)
```

---

## 5. Verified Code Examples

### Example 1: Documenting Top-Level Webhooks in OpenAPI 3.1

```python
from fastapi import APIRouter, FastAPI
from pydantic import BaseModel

app = FastAPI(title="SaaS Platform with Webhooks")
webhooks = APIRouter()

class UserSubscribedEvent(BaseModel):
    user_id: str
    plan: str
    timestamp: float

# This documents the event in openapi.json under "webhooks"
@webhooks.post("user-subscribed")
def user_subscribed_webhook(body: UserSubscribedEvent):
    """Sent to subscriber URL when a customer activates a new subscription."""

app.webhooks.include_router(webhooks)

@app.get("/status")
def get_status():
    return {"operational": True}
```

### Example 2: Enterprise Cryptographic Webhook Receiver with Replay Protection

```python
import hashlib
import hmac
import time
from typing import Annotated
from fastapi import Header, HTTPException, Request, status
from pydantic import BaseModel

WEBHOOK_SIGNING_SECRET = b"whsec_production_secret_key_32_bytes"

class WebhookPayload(BaseModel):
    event: str
    data: dict

async def verify_signed_webhook(
    request: Request,
    x_signature: Annotated[str, Header(alias="X-Signature")],
) -> bytes:
    # 1. Parse signature header format: "t=1711900000,v1=abcdef..."
    parts = dict(item.split("=", 1) for item in x_signature.split(",") if "=" in item)
    timestamp_str = parts.get("t")
    signature_hash = parts.get("v1")

    if not timestamp_str or not signature_hash:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Malformed signature header")

    # 2. Defend against replay attacks (reject if older than 5 minutes)
    event_timestamp = int(timestamp_str)
    if abs(time.time() - event_timestamp) > 300:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Webhook timestamp outside tolerance window")

    # 3. Read RAW unparsed request body bytes
    raw_body = await request.body()

    # 4. Recompute HMAC on: f"{timestamp}.{raw_body}"
    signed_payload = f"{timestamp_str}.".encode("utf-8") + raw_body
    expected_sig = hmac.new(WEBHOOK_SIGNING_SECRET, signed_payload, hashlib.sha256).hexdigest()

    # 5. Constant-time comparison
    if not hmac.compare_digest(signature_hash, expected_sig):
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid webhook signature")

    return raw_body
```

### Example 3: Edge Case / Boundary — Handling Webhook Retries with Idempotency

```python
import json
from typing import Annotated
from fastapi import APIRouter, Depends, status
import redis.asyncio as aioredis

router = APIRouter(prefix="/webhooks")
redis = aioredis.from_url("redis://localhost:6379/0", decode_responses=True)

@router.post("/stripe", status_code=status.HTTP_200_OK)
async def handle_stripe_webhook(raw_bytes: Annotated[bytes, Depends(verify_signed_webhook)]):
    data = json.loads(raw_bytes.decode("utf-8"))
    event_id = data.get("id")

    # Idempotency check: Providers retry webhooks until 200 OK is received
    is_new = await redis.set(f"processed_event:{event_id}", "1", nx=True, ex=86400 * 3)
    if not is_new:
        # Already processed; return 200 OK immediately to stop retries!
        return {"status": "already_processed"}

    # Process event asynchronously (offload to background queue)
    return {"status": "received"}
```

### Example 4: Failure Case — Rejecting Tampered Webhook Body

```python
# Attacker intercepts valid webhook, changes {"amount": 10.0} to {"amount": 1000.0}
# POST /webhooks/stripe
# Header: X-Signature: t=1711900000,v1=valid_hash_of_original_body
#
# Server verification fails because sha256(t + "." + modified_body) != v1
# Response:
# HTTP 401 Unauthorized
# {
#   "detail": "Invalid webhook signature"
# }
```

### Example 5: Idiomatic Modern Pattern with Route Callbacks in OpenAPI

```python
from fastapi import APIRouter, FastAPI
from pydantic import BaseModel, HttpUrl

app = FastAPI()
callback_router = APIRouter()

class InvoiceStatusUpdate(BaseModel):
    invoice_id: str
    status: str

@callback_router.post("{$request.body.callback_url}")
def invoice_notification(body: InvoiceStatusUpdate):
    """Callback triggered on subscriber URL when payment processes."""

class InvoiceRequest(BaseModel):
    amount: float
    callback_url: HttpUrl

@app.post("/invoices", callbacks=callback_router.routes)
def create_invoice(invoice: InvoiceRequest):
    return {"status": "created", "callback_registered": str(invoice.callback_url)}
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Deserializing JSON Before Signature Verification**: Deserializing JSON into a dictionary or Pydantic model and re-encoding it alters key ordering and whitespace, causing valid HMAC signatures to fail validation. Always compute HMAC on the exact raw byte stream (`await request.body()`).
2. **Synchronous Webhook Processing**: Running heavy database operations or external API calls inside the webhook handler causes the HTTP connection to time out. Most providers (Stripe, GitHub) enforce strict 5-10 second timeouts. Always return `200 OK` immediately and offload work to a background queue.
3. **Replay Attacks on Un-timestamped Webhooks**: If webhooks only sign the body without a timestamp, an attacker can capture a valid payload and re-transmit it 1,000 times to trigger duplicate actions.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Verifying signature on re-serialized JSON | Whitespace and key order changes break HMAC | False-positive 401 Signature Mismatch | Verify on raw bytes: `await request.body()` |
| 2. Synchronous heavy processing in receiver | Webhook connection times out; provider triggers retries | Duplicate event storm / webhook failures | Return 200 OK; enqueue to background queue |
| 3. No timestamp validation | Vulnerable to network replay attacks | Duplicate event execution | Enforce `abs(now - t) < 300` seconds |
| 4. Plain `==` signature comparison | Side-channel timing attack reveals secret hash | Security vulnerability | Use `hmac.compare_digest` |
| 5. Returning non-2xx status on already-processed events | Provider keeps retrying old events indefinitely | Wasteful retry traffic | Return 200 OK if event was previously handled |
| 6. Not setting timeouts on outbound webhooks | Target endpoint hangs; stalls dispatcher workers | Worker pool exhaustion | Set `timeout=5.0` on outbound `httpx` client |

---

## 8. Performance & Scalability Considerations

- **Outbound Dispatch Architecture**: Never dispatch outbound webhooks synchronously from web request threads. Push webhook events to an asynchronous message broker (Redis Streams, RabbitMQ, SQS) consumed by dedicated worker pools with exponential backoff and jitter.
- **Circuit Breakers for Broken Subscribers**: If a subscriber endpoint fails consistently (500s or timeouts), trip a circuit breaker and disable the webhook subscription after 20 consecutive failures.

---

## 9. Security & OWASP Guardrails

- **OWASP API7:2023 Server Side Request Forgery (SSRF)**: Validate subscriber callback URLs during registration to prevent users from registering internal IPs (`http://169.254.169.254`).
- **Secret Separation**: Generate unique, high-entropy HMAC signing secrets per subscriber (`secrets.token_hex(32)`).

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
import hmac
import hashlib
import time
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_webhook_signature_verification(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        body = b'{"event": "charge.success", "id": "ch_1"}'
        t = str(int(time.time()))
        secret = b"whsec_production_secret_key_32_bytes"
        sig = hmac.new(secret, f"{t}.".encode() + body, hashlib.sha256).hexdigest()
        header_val = f"t={t},v1={sig}"

        res = await client.post(
            "/webhooks/stripe",
            content=body,
            headers={"X-Signature": header_val, "Content-Type": "application/json"}
        )
        assert res.status_code == 200
```

---

## 11. Real-World Case Study

An online billing platform delivered webhooks directly inside route handlers: `await httpx.post(customer_webhook_url)`. When a popular customer's server went down and hung for 30 seconds on each connection, the billing platform's FastAPI worker threads backed up and exhausted memory, causing a total platform outage.
- **The Redesign**:
  1. Decoupled webhook dispatch from API execution using Celery and Redis.
  2. Implemented exponential backoff: retry after 1m, 5m, 15m, 1h, 6h.
  3. Added HMAC-SHA256 signatures and timestamp verification headers.
  API latency remained flat, and webhook delivery succeeded with 99.999% reliability.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: How do you prevent replay attacks when designing a webhook delivery system?</summary>

**Answer**: Include a Unix epoch timestamp `t` in the webhook header alongside the cryptographic signature (e.g. `X-Signature: t=1711900000,v1=abcdef...`). The signature is computed over both the timestamp and the payload: `HMAC(secret, t + "." + payload)`. The receiver verifies that the timestamp is within a narrow tolerance window (e.g. within 5 minutes of current time) before verifying the signature, preventing attackers from replaying captured packets later.
</details>

<details>
<summary>Q2: What is the difference between OpenAPI 3.1 `webhooks` and route `callbacks`?</summary>

**Answer**:
- **Callbacks**: Outbound requests that are initiated as a direct consequence of a specific incoming API call (e.g., an endpoint `POST /streams` accepts a `callback_url` parameter and calls it when processing completes).
- **Webhooks**: Top-level definitions in OpenAPI 3.1 representing asynchronous event notifications that are independent of any single incoming API request (e.g., a customer subscribing to account-wide `user.created` events).
</details>

<details>
<summary>Q3: Why must webhook receivers be strictly idempotent?</summary>

**Answer**: Webhook delivery protocols guarantee **at-least-once** delivery. Network timeouts, dropped TCP packets, or slow receiver acknowledgments cause the dispatch server to retry sending the event. A receiver must track processed event IDs (e.g. in Redis) so duplicate deliveries do not result in duplicate transactions.
</details>

<details>
<summary>Q4: Why does signature verification fail if executed on parsed JSON instead of raw body bytes?</summary>

**Answer**: JSON has no canonical serialization specification; differences in key sorting (`{"a":1,"b":2}` vs `{"b":2,"a":1}`), whitespace formatting, and Unicode escaping will produce completely different byte streams. Because cryptographic hashes are computed over exact byte sequences, parsing and re-serializing JSON breaks the hash. Verification must execute on `await request.body()`.
</details>

<details>
<summary>Q5: What HTTP status code should a webhook receiver return if it has already processed an event?</summary>

**Answer**: It should return **HTTP 200 OK**. Returning 4xx or 5xx causes the sending server to assume the delivery failed and schedule further retries, unnecessarily saturating network buffers.
</details>

<details>
<summary>Q6: How should an API platform protect itself against Server-Side Request Forgery (SSRF) when allowing users to register webhook URLs?</summary>

**Answer**: When a user registers a webhook URL, the server must parse the URL and resolve its hostname to an IP address. If the resolved IP belongs to a private network (RFC 1918: `10.0.0.0/8`, `192.168.0.0/16`), loopback (`127.0.0.1`), or cloud metadata services (`169.254.169.254`), the registration must be rejected.
</details>

<details>
<summary>Q7: What is the recommended timeout duration for webhook dispatch requests?</summary>

**Answer**: Outbound webhook calls should enforce a strict timeout between **3 and 5 seconds**. A slow or hanging subscriber server must not tie up worker processes.
</details>

<details>
<summary>Q8: How does exponential backoff with jitter improve webhook retry delivery?</summary>

**Answer**: Exponential backoff doubles the delay between successive retry attempts (1s, 2s, 4s, 8s...). Adding randomized jitter (e.g. $\pm 20\%$) prevents "thundering herd" problems where thousands of retried events hit a recovering subscriber server simultaneously at the exact same second.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Generate an HMAC-SHA256 signature for a payload</summary>

**Problem**: Write a function `sign_payload(secret, body_bytes)` returning a hex digest.

**Solution**:
```python
import hmac
import hashlib

def sign_payload(secret: bytes, body: bytes) -> str:
    return hmac.new(secret, body, hashlib.sha256).hexdigest()
```
</details>

<details>
<summary>Exercise 2 (Easy): Register a webhook in OpenAPI 3.1</summary>

**Problem**: Create a FastAPI app with a documented `payment-received` webhook.

**Solution**:
```python
from fastapi import APIRouter, FastAPI
from pydantic import BaseModel

app = FastAPI()
wh = APIRouter()

class PaymentEvent(BaseModel):
    amount: float

@wh.post("payment-received")
def payment_event(p: PaymentEvent):
    """Fired when payment completes"""

app.webhooks.include_router(wh)
```
</details>

<details>
<summary>Exercise 3 (Medium): Validate timestamp tolerance</summary>

**Problem**: Write a function checking if a Unix timestamp is within 300 seconds of `time.time()`.

**Solution**:
```python
import time
from fastapi import HTTPException, status

def assert_fresh_timestamp(ts: int):
    if abs(time.time() - ts) > 300:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Timestamp out of tolerance window"
        )
```
</details>

<details>
<summary>Exercise 4 (Medium): Build an Outbound Webhook Header Formatter</summary>

**Problem**: Write a function that accepts `body_bytes` and `secret`, returning headers dictionary with `X-Signature` containing `t` and `v1`.

**Solution**:
```python
import hashlib
import hmac
import time

def make_webhook_headers(body_bytes: bytes, secret: bytes) -> dict[str, str]:
    t = str(int(time.time()))
    payload_to_sign = f"{t}.".encode() + body_bytes
    sig = hmac.new(secret, payload_to_sign, hashlib.sha256).hexdigest()
    return {
        "Content-Type": "application/json",
        "X-Signature": f"t={t},v1={sig}",
    }
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Webhook Event Deduplication Middleware</summary>

**Problem**: Build a dependency that checks Redis for `event:{event_id}`, returning HTTP 200 with an empty body early if already processed.

**Solution**:
```python
from fastapi import HTTPException, status
import redis.asyncio as aioredis

redis_pool = aioredis.from_url("redis://localhost:6379/0")

async def deduplicate_event(event_id: str):
    is_new = await redis_pool.set(f"event:{event_id}", "1", nx=True, ex=86400)
    if not is_new:
        # Halt execution and return 200 early
        raise HTTPException(status_code=status.HTTP_200_OK, detail="Duplicate event acknowledged")
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Complete End-to-End Webhook Receiver Dependency</summary>

**Problem**: Build a FastAPI dependency that verifies signature, checks timestamp freshness, parses JSON body into a Pydantic model, and returns the validated payload.

**Solution**:
```python
import hashlib
import hmac
import json
import time
from typing import Annotated
from fastapi import Header, HTTPException, Request, status
from pydantic import BaseModel

SECRET = b"production_webhook_secret_key"

class GenericEvent(BaseModel):
    event_id: str
    event_type: str
    payload: dict

async def verified_webhook_payload(
    request: Request,
    x_signature: Annotated[str, Header(alias="X-Signature")],
) -> GenericEvent:
    # 1. Parse header
    parts = dict(p.split("=", 1) for p in x_signature.split(",") if "=" in p)
    t = parts.get("t")
    v1 = parts.get("v1")
    if not t or not v1 or abs(time.time() - int(t)) > 300:
        raise HTTPException(status_code=400, detail="Invalid signature header or expired timestamp")

    # 2. Verify raw bytes
    raw_body = await request.body()
    expected = hmac.new(SECRET, f"{t}.".encode() + raw_body, hashlib.sha256).hexdigest()
    if not hmac.compare_digest(v1, expected):
        raise HTTPException(status_code=401, detail="Signature mismatch")

    # 3. Parse JSON
    try:
        data = json.loads(raw_body.decode("utf-8"))
        return GenericEvent(**data)
    except Exception as e:
        raise HTTPException(status_code=422, detail=f"Invalid payload: {e}")
```
</details>
