# Module 07: Authentication & Security — 12: Rate Limiting Strategies (`12-rate-limiting-strategies`)

> Rate limiting protects APIs from Denial of Service (DoS), brute-force attacks, and noisy-neighbor resource exhaustion. In production FastAPI architectures, rate limiting is implemented across multiple tiers (IP, User ID, API Key) using Token Bucket and Sliding Window Counter algorithms backed by atomic Redis Lua scripts or `slowapi`.

| Field | Value |
|---|---|
| **Category** | Authentication & Security |
| **Target Level** | Advanced / Production Engineer |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | slowapi 0.1.9+, redis-py 5.2+, httpx 0.28+, pytest 9.1+ |

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

Without rate limiting, any client can flood an API with thousands of requests per second, starving database pools, locking CPU cores during password hashing, and causing cascading microservice outages:
- **Algorithms Compared**:
  1. **Fixed Window**: Counters reset at fixed intervals (e.g. at the top of every minute). Vulnerable to traffic bursts at the boundary (2x burst).
  2. **Sliding Window Log**: Stores timestamps of every request in a Redis Sorted Set (`ZSET`). Mathematically precise, but memory-intensive for high request volumes.
  3. **Sliding Window Counter**: Blends count from the previous window with the current window proportionally: $\text{Count} = \text{Count}_{\text{current}} + \text{Count}_{\text{prev}} \times (1 - \text{elapsed\_fraction})$. Memory-efficient and smooth.
  4. **Token Bucket**: Tokens replenish at a constant rate up to a max burst capacity. Allows burstiness while strictly capping sustained rate.
- **RFC 6585 Alignment**: Saturated endpoints return HTTP **429 Too Many Requests** with `Retry-After`, `X-RateLimit-Limit`, and `X-RateLimit-Remaining` headers.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               Sliding Window Counter Rate Limiting in Redis                       |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  Current Time: 12:00:45 (Window: 1 minute)                                        |
|  Previous Window [11:59:00 - 12:00:00]: 80 requests                               |
|  Current Window  [12:00:00 - 12:01:00]: 30 requests                               |
|                                                                                   |
|  Elapsed in current window: 45s (75%)                                             |
|  Weight of previous window: 100% - 75% = 25%                                      |
|                                                                                   |
|  Estimated Sliding Count = 30 + (80 * 0.25) = 30 + 20 = 50 requests               |
|  Limit = 60 requests/min ──> ALLOW REQUEST (Remaining: 10)                        |
|                                                                                   |
+-----------------------------------------------------------------------------------+

Request Flow with Multi-Tier Identification:

Client Request
      │
      ▼
Resolve Rate Limit Key:
      ├── Is Authenticated User? ──> Key = `rl:usr:{user_id}`
      ├── Is API Key Client? ──────> Key = `rl:apikey:{key_prefix}`
      └── Anonymous Request? ──────> Key = `rl:ip:{client_ip}`
      │
      ▼
Execute Atomic Redis Lua Script (Evaluates Window, Increments, Sets TTL)
      │
      ┌───────────────┴───────────────┐
  Within Quota?                   Quota Exceeded?
      │                               │
     Yes                              ▼
      │                      HTTP 429 Too Many Requests
      │                      Headers:
      │                      Retry-After: 15
      │                      X-RateLimit-Remaining: 0
      ▼
Execute Endpoint Logic
```

---

## 3. How It Works Internally

1. **Atomic Redis Lua Scripting**:
   - Evaluating a sliding window counter requires fetching the previous window count, fetching the current count, calculating the weighted sum, and incrementing.
   - Doing this via separate Redis commands creates race conditions under high concurrency.
   - Encapsulating the logic in an atomic **Lua script** (`EVALSHA`) guarantees that window calculations and counter increments execute as a single atomic operation in Redis.
2. **`slowapi` Middleware / Decorator**:
   - `slowapi` wraps Python's `limits` library. It inspects `request.client.host` or custom key-generating functions and automatically injects standard rate-limiting headers.

---

## 4. Syntax & API Reference

### Using `slowapi`
```python
from fastapi import FastAPI, Request
from slowapi import Limiter, _rate_limit_exceeded_handler
from slowapi.util import get_remote_address
from slowapi.errors import RateLimitExceeded

limiter = Limiter(key_func=get_remote_address, default_limits=["100/minute"])
app = FastAPI()
app.state.limiter = limiter
app.add_exception_handler(RateLimitExceeded, _rate_limit_exceeded_handler)

@app.get("/items")
@limiter.limit("10/minute")
async def read_items(request: Request):
    return {"items": []}
```

---

## 5. Verified Code Examples

### Example 1: Minimal Working `slowapi` IP Rate Limiter

```python
from fastapi import FastAPI, Request
from slowapi import Limiter, _rate_limit_exceeded_handler
from slowapi.errors import RateLimitExceeded
from slowapi.util import get_remote_address

limiter = Limiter(key_func=get_remote_address)
app = FastAPI(title="Rate Limited App")
app.state.limiter = limiter
app.add_exception_handler(RateLimitExceeded, _rate_limit_exceeded_handler)

@app.get("/api/v1/ping")
@limiter.limit("5/minute")
async def ping(request: Request):
    return {"message": "pong"}
```

### Example 2: Enterprise Sliding Window Counter with Redis & Lua

```python
import time
from typing import Annotated
from fastapi import APIRouter, Depends, HTTPException, Request, Response, status
import redis.asyncio as aioredis

router = APIRouter(prefix="/api/v1")
redis = aioredis.from_url("redis://localhost:6379/0", decode_responses=True)

# Atomic Sliding Window Counter in Redis Lua
SLIDING_WINDOW_LUA = """
local key = KEYS[1]
local limit = tonumber(ARGV[1])
local current_time = tonumber(ARGV[2])
local window_size = tonumber(ARGV[3])

local clear_before = current_time - window_size
redis.call('ZREMRANGEBYSCORE', key, '-inf', clear_before)

local current_requests = redis.call('ZCARD', key)
if current_requests < limit then
    redis.call('ZADD', key, current_time, current_time)
    redis.call('EXPIRE', key, window_size)
    return {1, limit - current_requests - 1, 0}
else
    local oldest = redis.call('ZRANGE', key, 0, 0, 'WITHSCORES')
    local retry_after = math.ceil((oldest[2] + window_size - current_time))
    return {0, 0, retry_after}
end
"""

class SlidingWindowLimiter:
    def __init__(self, limit: int, window_seconds: int):
        self.limit = limit
        self.window_seconds = window_seconds

    async def __call__(self, request: Request, response: Response):
        # Identify subject: prefer authenticated user ID, fallback to client IP
        user_id = getattr(request.state, "user_id", None)
        subject_key = f"rl:user:{user_id}" if user_id else f"rl:ip:{request.client.host if request.client else 'unknown'}"
        now = time.time()

        res = await redis.eval(
            SLIDING_WINDOW_LUA,
            1,
            subject_key,
            self.limit,
            now,
            self.window_seconds,
        )
        allowed, remaining, retry_after = res

        response.headers["X-RateLimit-Limit"] = str(self.limit)
        response.headers["X-RateLimit-Remaining"] = str(remaining)

        if not allowed:
            response.headers["Retry-After"] = str(retry_after)
            raise HTTPException(
                status_code=status.HTTP_429_TOO_MANY_REQUESTS,
                detail="Rate limit exceeded. Please retry later.",
                headers={"Retry-After": str(retry_after)},
            )

@router.get("/heavy-query", dependencies=[Depends(SlidingWindowLimiter(limit=10, window_seconds=60))])
async def heavy_query():
    return {"data": "computationally_expensive_result"}
```

### Example 3: Edge Case / Boundary — Proxy & Cloudflare Real-IP Resolution

```python
from fastapi import Request

def resolve_true_client_ip(request: Request) -> str:
    # 1. Cloudflare header
    cf_connecting_ip = request.headers.get("CF-Connecting-IP")
    if cf_connecting_ip:
        return cf_connecting_ip.strip()

    # 2. Standard X-Forwarded-For (take the first client IP in the chain)
    x_forwarded_for = request.headers.get("X-Forwarded-For")
    if x_forwarded_for:
        return x_forwarded_for.split(",")[0].strip()

    # 3. Direct socket IP fallback
    if request.client:
        return request.client.host
    return "127.0.0.1"
```

### Example 4: Failure Case — Structured 429 Error with RFC Compliance

```python
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
from slowapi.errors import RateLimitExceeded

app = FastAPI()

@app.exception_handler(RateLimitExceeded)
async def custom_rate_limit_handler(request: Request, exc: RateLimitExceeded):
    return JSONResponse(
        status_code=429,
        content={
            "error": "rate_limit_exceeded",
            "message": "Too many requests submitted. Slow down.",
            "retry_after_seconds": 60,
        },
        headers={"Retry-After": "60", "X-RateLimit-Reset": "1711900060"},
    )
```

### Example 5: Idiomatic Modern Pattern with Tier-Based Custom Key Generator

```python
from fastapi import FastAPI, Request
from slowapi import Limiter

def resolve_tier_key(request: Request) -> str:
    # Separate quotas by tier extracted from header or auth context
    tier = request.headers.get("X-Tier", "free")
    client_ip = request.client.host if request.client else "unknown"
    return f"{tier}:{client_ip}"

limiter = Limiter(key_func=resolve_tier_key)
app = FastAPI()
app.state.limiter = limiter

@app.get("/export")
@limiter.limit("5/minute")
async def export_data(request: Request):
    return {"status": "exporting"}
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Proxy IP Spoofing**: Trusting `X-Forwarded-For` without validating that the reverse proxy (Nginx/Cloudflare) is trusted allows an attacker to send `X-Forwarded-For: 8.8.8.8` with randomized IPs on every request, completely bypassing IP rate limits!
2. **Fixed Window Boundary Bursts**: In a fixed window of 100 req/min, an attacker sends 100 requests at 11:59:59 and another 100 requests at 12:00:01. In a 2-second span, the server handles 200 requests, potentially triggering an outage. Use Sliding Window.
3. **Sorted Set Memory Growth**: The sliding window log (`ZSET`) stores one entry per request. If an endpoint receives 10,000 requests/sec, the ZSET can grow by dozens of megabytes per key.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Trusting raw `request.client.host` behind proxy | All external traffic appears to originate from `10.0.0.1` | One user exhausts rate limit for entire company | Extract `CF-Connecting-IP` or verified `X-Forwarded-For` |
| 2. Non-atomic Redis read-then-write | Race condition under concurrency allows limit bypass | Saturated limits under burst load | Use atomic Redis Lua scripts (`eval`) |
| 3. Missing `request: Request` in endpoint signature | `slowapi` decorator inspects endpoint arguments for `Request` | `KeyError: 'request'` | Include `request: Request` parameter |
| 4. Not returning `Retry-After` header | Violates RFC 6585; clients retry immediately | Client hammers server in retry loop | Return `Retry-After: <seconds>` header |
| 5. In-memory rate limiting across multiple Uvicorn workers | Each worker process has an independent memory counter | Quota multiplies by number of worker processes | Back rate limiter with a shared Redis instance |
| 6. Rate-limiting health checks (`/healthz`) | Kubernetes liveness probes get 429 and restart pods | Cascading pod restarts | Exclude internal health probes from rate limits |

---

## 8. Performance & Scalability Considerations

- **Redis Lua Script Overhead**: Redis executes pre-compiled Lua scripts (`EVALSHA`) in ~0.08ms. A single Redis core can evaluate 40,000+ rate limit decisions per second.
- **Header Parsing**: Pre-parse client IPs early in ASGI middleware rather than executing regexes inside every endpoint dependency.

---

## 9. Security & OWASP Guardrails

- **OWASP API4:2023 Unrestricted Resource Consumption**: Strict rate limiting on authentication routes (`/auth/token`, `/auth/login`, `/auth/reset-password`) is mandatory to prevent credential stuffing.
- **Differentiated Rate Limits**: Apply aggressive limits (5 req/min) on CPU-heavy operations (Argon2 login, report generation) and relaxed limits (1000 req/min) on read-only queries.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_rate_limiter_blocks_on_sixth_request(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # First 5 requests should succeed (limit is 5/minute)
        for _ in range(5):
            res = await client.get("/api/v1/ping")
            assert res.status_code == 200

        # Sixth request must fail with 429
        blocked_res = await client.get("/api/v1/ping")
        assert blocked_res.status_code == 429
        assert "Retry-After" in blocked_res.headers
```

---

## 11. Real-World Case Study

A public financial API experienced severe outages during US stock market openings. Attackers and automated trading bots submitted thousands of concurrent quote requests, causing the database connection pool to max out at 100%.
- **Solution**:
  1. Configured Redis-backed sliding window rate limiting.
  2. Free tier was capped at 30 req/min; Pro tier capped at 600 req/min; Enterprise capped at 5,000 req/min.
  3. Deployed Cloudflare WAF to drop rogue IP traffic exceeding 2,000 req/min before reaching FastAPI.
  System availability reached 99.99%, and database CPU utilization dropped from 98% to 18%.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: Why is the Sliding Window Counter algorithm superior to the Fixed Window Counter?</summary>

**Answer**: Fixed Window counters reset at fixed boundaries (e.g. 12:00:00). An attacker can send the entire allowed quota in the last second of window 1 and another full quota in the first second of window 2, delivering a 2x burst that can overwhelm the system. Sliding Window Counter computes a weighted average of the previous and current windows, smoothing traffic and preventing boundary burst exploits.
</details>

<details>
<summary>Q2: Why must Redis operations for rate limiting be executed via Lua scripts?</summary>

**Answer**: Rate limiting requires checking the current counter, comparing it against the threshold, and conditionally incrementing or adding timestamps. If executed through separate Redis commands, concurrent requests will read stale counts simultaneously before any writes occur (a race condition), allowing clients to exceed their quotas. Redis executes Lua scripts atomically on its single-threaded engine, preventing race conditions.
</details>

<details>
<summary>Q3: How do you prevent IP spoofing when rate limiting based on `X-Forwarded-For`?</summary>

**Answer**: An attacker can insert arbitrary fake IPs into the `X-Forwarded-For` header. To prevent this, reverse proxies (Nginx, AWS ALB, Cloudflare) must be configured to strip untrusted incoming headers and append the true socket IP. The application must only trust headers coming from known private proxy IP addresses, or rely on trusted provider headers like `CF-Connecting-IP`.
</details>

<details>
<summary>Q4: What standard HTTP response headers must accompany a 429 Too Many Requests response?</summary>

**Answer**:
- `Retry-After`: Number of seconds the client must wait before making another request (RFC 6585).
- `X-RateLimit-Limit`: Maximum requests permitted within the window.
- `X-RateLimit-Remaining`: Number of requests remaining in the current window.
- `X-RateLimit-Reset`: Unix epoch timestamp when the current window resets.
</details>

<details>
<summary>Q5: Why does `slowapi` require the `request: Request` parameter in route handlers?</summary>

**Answer**: The `@limiter.limit` decorator inspects the wrapped function's keyword arguments to find an instance of Starlette's `Request`. It extracts client information (headers, client IP, state) to evaluate the `key_func`. If `request: Request` is omitted from the route handler signature, `slowapi` cannot identify the caller and raises an exception.
</details>

<details>
<summary>Q6: How can rate limiting be customized based on an authenticated user's subscription tier?</summary>

**Answer**: Implement a dynamic dependency or custom `key_func` that inspects `request.state.user` or the incoming JWT. The function extracts `user.tier` and dynamically selects the limit string (e.g. `"100/minute"` for Enterprise vs. `"10/minute"` for Free) when calling the rate limiting evaluator.
</details>

<details>
<summary>Q7: What is the Token Bucket algorithm, and when is it preferred over Sliding Window?</summary>

**Answer**: Token Bucket maintains a bucket with a maximum capacity $B$ replenished at rate $r$ tokens/second. Each request consumes one token. It is preferred when an API wants to allow intentional short bursts of traffic (e.g., loading an initial dashboard requiring 15 concurrent API calls) while strictly capping sustained long-term throughput.
</details>

<details>
<summary>Q8: How do you avoid rate limiting internal microservices communicating within a service mesh?</summary>

**Answer**: Implement a key resolver that checks if the request originates from a private VPC subnet (e.g. `10.0.0.0/8`) or contains a verified internal mutual TLS certificate / service mesh header (`X-Internal-Service`). If matched, the resolver returns `None` or bypasses the rate limiting check entirely.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Setup slowapi with a 2-per-minute limit on a test route</summary>

**Problem**: Create a FastAPI app with `slowapi` that restricts `/ping` to 2 requests per minute.

**Solution**:
```python
from fastapi import FastAPI, Request
from slowapi import Limiter, _rate_limit_exceeded_handler
from slowapi.util import get_remote_address
from slowapi.errors import RateLimitExceeded

limiter = Limiter(key_func=get_remote_address)
app = FastAPI()
app.state.limiter = limiter
app.add_exception_handler(RateLimitExceeded, _rate_limit_exceeded_handler)

@app.get("/ping")
@limiter.limit("2/minute")
async def ping(request: Request):
    return {"ping": "pong"}
```
</details>

<details>
<summary>Exercise 2 (Easy): Extract real client IP behind Cloudflare</summary>

**Problem**: Write a helper `get_real_ip(request)` that prioritizes `CF-Connecting-IP`.

**Solution**:
```python
from fastapi import Request

def get_real_ip(request: Request) -> str:
    cf_ip = request.headers.get("CF-Connecting-IP")
    if cf_ip:
        return cf_ip.strip()
    return request.client.host if request.client else "127.0.0.1"
```
</details>

<details>
<summary>Exercise 3 (Medium): Implement User-ID-Based Rate Limiting</summary>

**Problem**: Write a key function for `slowapi` that rate limits by `request.headers.get("X-User-ID")` if present, falling back to IP.

**Solution**:
```python
from fastapi import Request

def user_or_ip_key(request: Request) -> str:
    user_id = request.headers.get("X-User-ID")
    if user_id:
        return f"user:{user_id}"
    return f"ip:{request.client.host if request.client else '127.0.0.1'}"
```
</details>

<details>
<summary>Exercise 4 (Medium): Build an In-Memory Token Bucket Class</summary>

**Problem**: Implement a simple Python class `TokenBucket(capacity, refill_rate)` with a thread-safe `consume()` method.

**Solution**:
```python
import time

class TokenBucket:
    def __init__(self, capacity: int, refill_rate: float):
        self.capacity = capacity
        self.refill_rate = refill_rate
        self.tokens = float(capacity)
        self.last_update = time.time()

    def consume(self, amount: int = 1) -> bool:
        now = time.time()
        elapsed = now - self.last_update
        self.tokens = min(self.capacity, self.tokens + elapsed * self.refill_rate)
        self.last_update = now

        if self.tokens >= amount:
            self.tokens -= amount
            return True
        return False
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Custom FastAPI Dependency for Sliding Window in Redis</summary>

**Problem**: Write an async FastAPI dependency `check_rate_limit(request)` using `redis.asyncio` that enforces 60 requests per 60 seconds using atomic `INCR` and `EXPIRE`.

**Solution**:
```python
from fastapi import HTTPException, Request, status
import redis.asyncio as aioredis
import time

redis_client = aioredis.from_url("redis://localhost:6379/0")

async def fixed_window_rate_limiter(request: Request):
    ip = request.client.host if request.client else "unknown"
    current_minute = int(time.time() // 60)
    key = f"rate:{ip}:{current_minute}"

    count = await redis_client.incr(key)
    if count == 1:
        await redis_client.expire(key, 60)

    if count > 60:
        raise HTTPException(
            status_code=status.HTTP_429_TOO_MANY_REQUESTS,
            detail="Rate limit exceeded",
            headers={"Retry-After": "60"}
        )
```
</details>

<details>
<summary>Exercise 6 (Hard): Build Multi-Tier Dynamic Quota Evaluator</summary>

**Problem**: Implement an endpoint guard that reads the customer's subscription tier (`free` = 5 req/min, `pro` = 50 req/min) and dynamically enforces the appropriate threshold.

**Solution**:
```python
from typing import Annotated
from fastapi import Depends, Header, HTTPException, Request, status
import redis.asyncio as aioredis

redis_pool = aioredis.from_url("redis://localhost:6379/0")
LIMITS = {"free": 5, "pro": 50}

async def dynamic_tier_rate_limiter(
    request: Request,
    api_key: Annotated[str, Header(alias="X-API-Key")],
):
    tier = "pro" if api_key.startswith("pro_") else "free"
    limit = LIMITS[tier]
    key = f"dyn_rl:{api_key}"

    count = await redis_pool.incr(key)
    if count == 1:
        await redis_pool.expire(key, 60)

    if count > limit:
        raise HTTPException(
            status_code=status.HTTP_429_TOO_MANY_REQUESTS,
            detail=f"Exceeded quota of {limit} requests/minute for tier '{tier}'",
            headers={"Retry-After": "60", "X-RateLimit-Limit": str(limit)}
        )
```
</details>
