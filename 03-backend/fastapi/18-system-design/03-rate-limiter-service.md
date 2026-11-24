# System Design Blueprint: Distributed API Rate Limiter & Abuse Prevention

## Metadata

- **Topic**: Distributed High-Performance Rate Limiting & DoS Protection
- **Scale Requirements**: 500,000 requests per second across 100 API gateway nodes
- **Target SLA**: Rate limit decision latency < 1.5ms p99; zero false-positive drops on paid tiers; fail-open on cache partitions
- **Tech Stack**: FastAPI Middleware, Redis Cluster (In-Memory Lua Scripts), Sliding Window Counter, Token Bucket

---

## 1. Requirements & System Estimation

### 1.1 Functional Requirements
1. **Multi-Tier Throttling**: Rate limit based on IP address, authenticated User ID, API Key, or endpoint tier (e.g. Free: 60 req/min; Pro: 1,000 req/min).
2. **Standard HTTP Compliance**: Return `429 Too Many Requests` with RFC 6585 headers: `X-RateLimit-Limit`, `X-RateLimit-Remaining`, `X-RateLimit-Reset`, `Retry-After`.
3. **Graceful Degradation**: If Redis rate limiting node fails, configure fail-open vs. fail-closed based on endpoint sensitivity.

### 1.2 Rate Limiting Algorithms Comparison

| Algorithm | Pros | Cons | Ideal Use Case |
|:---|:---|:---|:---|
| **Fixed Window Counter** | Minimal memory (1 integer per key) | Traffic bursts at window boundaries allow 2x limit | Simple, low-contention services |
| **Token Bucket** | Accommodates short bursts while maintaining steady-state rate | Requires tracking token counts and timestamp | General API rate limiting |
| **Leaky Bucket** | Enforces completely smooth outflow rate | Drops bursts abruptly | Processing queues, webhook dispatchers |
| **Sliding Window Counter** | Accurate, low memory footprint, eliminates boundary double-spending | Slightly more complex Lua logic | Modern enterprise production APIs |

---

## 2. High-Level Architecture Diagram

```
                       +---------------------------------------+
                       |       Client / External Consumer      |
                       +-------------------+-------------------+
                                           |
                                           | HTTP Request (Header: Authorization / X-API-Key)
                                           v
                       +-------------------+-------------------+
                       |    API Gateway / FastAPI Middleware   |
                       +-------------------+-------------------+
                                           |
                                           | 1. Extract Rate Limit Key (e.g. "rl:user:123")
                                           | 2. Execute Atomic Redis Lua Script (EVALSHA)
                                           v
                       +-------------------+-------------------+
                       |  Redis Cluster (Master Nodes)         |
                       |  - Atomic Sliding Window / Token      |
                       |  - Latency: < 0.8ms (Single roundtrip)|
                       +-------------------+-------------------+
                                           |
                              +------------+------------+
                              | Allowed?                |
                              v                         v
                       [YES: Allowed]            [NO: Rate Limited]
                              |                         |
                              v                         v
               +--------------+--------------+   +------+----------------------+
               | Pass to FastAPI Route Handler|  | Return HTTP 429 Too Many Req|
               | & Inject Rate Limit Headers |  | Header: Retry-After: 30     |
               +-----------------------------+   +-----------------------------+
```

---

## 3. High-Performance Redis Lua Script (Sliding Window Counter)

Executing the sliding window algorithm via a Redis Lua script guarantees atomicity without distributed race conditions:

```lua
-- KEYS[1]: rate limit key, e.g. "rl:user:99"
-- ARGV[1]: current timestamp in milliseconds
-- ARGV[2]: window size in milliseconds (e.g. 60000 for 1 minute)
-- ARGV[3]: max allowed requests in window (e.g. 100)

local key = KEYS[1]
local now = tonumber(ARGV[1])
local window = tonumber(ARGV[2])
local limit = tonumber(ARGV[3])
local clear_before = now - window

-- 1. Remove old requests outside current window
redis.call('ZREMRANGEBYSCORE', key, 0, clear_before)

-- 2. Count current entries in window
local current_requests = redis.call('ZCARD', key)

if current_requests < limit then
    -- 3. Add current request to sorted set
    redis.call('ZADD', key, now, now)
    redis.call('PEXPIRE', key, window)
    return {1, limit - current_requests - 1, 0} -- {allowed, remaining, retry_after}
else
    -- 4. Rate limited: calculate retry_after from oldest element
    local oldest = redis.call('ZRANGE', key, 0, 0, 'WITHSCORES')
    local retry_after = math.ceil((oldest[2] + window - now) / 1000)
    return {0, 0, retry_after} -- {allowed, remaining, retry_after}
end
```

---

## 4. FastAPI Production Middleware Implementation

```python
import time
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
import redis.asyncio as aioredis
from starlette.middleware.base import BaseHTTPMiddleware

app = FastAPI(title="Rate Limited Gateway")
redis_client = aioredis.from_url("redis://localhost:6379/0", decode_responses=True)

# Preloaded Lua script SHA
SLIDING_WINDOW_LUA = """
local key = KEYS[1]
local now = tonumber(ARGV[1])
local window = tonumber(ARGV[2])
local limit = tonumber(ARGV[3])
local clear_before = now - window
redis.call('ZREMRANGEBYSCORE', key, 0, clear_before)
local current = redis.call('ZCARD', key)
if current < limit then
    redis.call('ZADD', key, now, now)
    redis.call('PEXPIRE', key, window)
    return {1, limit - current - 1, 0}
else
    local oldest = redis.call('ZRANGE', key, 0, 0, 'WITHSCORES')
    local retry_after = math.ceil((oldest[2] + window - now) / 1000)
    return {0, 0, retry_after}
end
"""


class DistributedRateLimitMiddleware(BaseHTTPMiddleware):
    def __init__(self, app, max_requests: int = 100, window_seconds: int = 60):
        super().__init__(app)
        self.max_requests = max_requests
        self.window_ms = window_seconds * 1000
        self.script_sha = None

    async def dispatch(self, request: Request, call_next):
        # Identify caller by API key or Client IP
        client_id = request.headers.get("X-API-Key") or request.client.host
        rate_key = f"ratelimit:{client_id}"
        now_ms = int(time.time() * 1000)

        try:
            # Atomic evaluation in Redis
            res = await redis_client.eval(
                SLIDING_WINDOW_LUA, 1, rate_key, now_ms, self.window_ms, self.max_requests
            )
            allowed, remaining, retry_after = res[0], res[1], res[2]
        except Exception:
            # Fail-Open strategy: allow request if rate limiter is temporarily unreachable
            allowed, remaining, retry_after = 1, self.max_requests, 0

        if not allowed:
            return JSONResponse(
                status_code=429,
                content={"error": "Too Many Requests", "retry_after": retry_after},
                headers={
                    "X-RateLimit-Limit": str(self.max_requests),
                    "X-RateLimit-Remaining": "0",
                    "Retry-After": str(retry_after),
                },
            )

        response = await call_next(request)
        response.headers["X-RateLimit-Limit"] = str(self.max_requests)
        response.headers["X-RateLimit-Remaining"] = str(remaining)
        return response


app.add_middleware(DistributedRateLimitMiddleware, max_requests=100, window_seconds=60)
```

---

## 5. Failure Modes, Scalability & Bottlenecks

| Failure Scenario | Impact | Mitigation Strategy |
|:---|:---|:---|
| **Redis Memory Exhaustion (ZSET footprint)** | Millions of distinct IPs cause sorted sets to balloon in memory. | Switch to **Sliding Window Log with Count estimation** (HyperLogLog or 2-integer counter) for non-authenticated public IPs. |
| **Hot Key Contention in Redis** | Extreme traffic spikes to single API key saturate single Redis CPU core. | Apply **Local In-Memory Cache (L1 Cache with Cachetools)** with 1-second TTL inside FastAPI process to absorb bursts before querying Redis (L2). |
| **Redis Network Partition / Outage** | Requests fail or block indefinitely. | Implement strict socket timeout (20ms) and **Fail-Open mode** for standard traffic; Fail-Closed only for payment endpoints. |
| **Clock Drift across Distributed Nodes** | Servers disagree on current time, distorting window calculations. | Sync all ASGI hosts and Redis clusters via Network Time Protocol (NTP / AWS Chrony). |
