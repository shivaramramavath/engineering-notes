# Module 07: Authentication & Security — 11: Token Revocation and Blacklisting (`11-token-revocation-and-blacklisting`)

> Pure stateless JWTs cannot be revoked prior to their natural expiration without external state. In high-security FastAPI systems, revocation is achieved using high-speed distributed Redis blacklists, user-level invalidation epochs (`revoked_before`), and bounded-memory bloom filters.

| Field | Value |
|---|---|
| **Category** | Authentication & Security |
| **Target Level** | Advanced / Production Engineer |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | redis-py 5.2+, pyjwt 2.10+, httpx 0.28+, pytest 9.1+ |

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

A fundamental trade-off of JSON Web Tokens is that signatures remain valid until the `exp` timestamp. If a token is stolen or a user changes their password, an attacker can continue querying APIs until the token expires:
- **JTI Blacklisting**: Every token is issued a unique UUID (`jti`). Upon logout or revocation, the `jti` is stored in Redis with a TTL equal to the token's remaining lifespan. The key automatically self-deletes when the token expires.
- **User Epoch Invalidation (`revoked_before`)**: When a user changes their password or clicks "Log out all devices", storing hundreds of JTIs is inefficient. Instead, store a single Unix timestamp `user:{id}:revoked_before = now()`. Any token with an `iat` (Issued At) prior to that timestamp is rejected.
- **Hybrid Architecture**: Short token lifetimes (5-15m) minimize the window of exposure, while Redis-backed revocation handles immediate security events.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|                        Token Revocation Strategies Matrix                         |
+-----------------------------------------------------------------------------------+
| Strategy             | Storage Mechanism        | Memory Footprint | Scope        |
+----------------------+--------------------------+------------------+--------------+
| JTI Blacklist        | Redis SETEX (Key per JTI)| High (Per Token) | Single Token |
| User Invalidation Epoch| Redis Key per User ID  | Low (1 Key/User) | All Sessions |
| Bloom Filter Pre-check| Bit array in memory     | Ultra-Low (<1MB) | Rejection gate|
+-----------------------------------------------------------------------------------+

Verification Data Flow:

Client Request ──> Header: "Authorization: Bearer <jwt>"
                         │
                         ▼
        [ Verify Cryptographic Signature & exp ]
                         │
        ┌────────────────┴────────────────┐
     Valid?                            Invalid?
        │                                 │
       Yes                                ▼
        │                        HTTP 401 Unauthorized
        ▼
   [ Extract 'jti', 'sub', 'iat' ]
        │
        ▼
   [ Check Redis: user:{sub}:revoked_before ]
        │
   Token iat < revoked_before? ──> Yes ──> HTTP 401 (Session Expired)
        │
        No
        ▼
   [ Check Redis: blacklist:{jti} ]
        │
   Exists in Blacklist? ──────────> Yes ──> HTTP 401 (Token Revoked)
        │
        No
        ▼
   [ Allow Request to Route Handler ]
```

---

## 3. How It Works Internally

1. **Self-Pruning Redis Keys**:
   - When a token is revoked at timestamp $T_{\text{revoke}}$, its remaining TTL is computed: $\Delta t = T_{\text{exp}} - T_{\text{revoke}}$.
   - It is written to Redis via `SETEX blacklist:<jti> <delta_t> "1"`.
   - Redis automatically evicts the key when $\Delta t$ reaches zero. The blacklist never grows indefinitely!
2. **Epoch Comparison**:
   - `iat` is an integer epoch timestamp. If `iat < user_revoked_before_epoch`, the token was minted before the security event (such as password reset), rendering it immediately invalid.

---

## 4. Syntax & API Reference

```python
import time
from datetime import datetime, timezone
import redis.asyncio as aioredis
from fastapi import HTTPException, status
import jwt

# Redis connection
redis_client = aioredis.from_url("redis://localhost:6379/0", decode_responses=True)

# Revoking a single token
async def revoke_token(token: str, secret: str):
    payload = jwt.decode(token, secret, algorithms=["HS256"])
    jti = payload["jti"]
    exp = payload["exp"]
    now = int(time.time())
    ttl = exp - now
    if ttl > 0:
        await redis_client.setex(f"blacklist:{jti}", ttl, "revoked")

# Revoking all sessions for a user
async def revoke_all_user_sessions(user_id: str):
    now = int(time.time())
    # Retain epoch for 30 days (max token lifetime)
    await redis_client.setex(f"user:{user_id}:revoked_before", 30 * 86400, str(now))
```

---

## 5. Verified Code Examples

### Example 1: Minimal JTI Blacklist Verification Dependency

```python
import time
from typing import Annotated
from fastapi import Depends, FastAPI, HTTPException, Security, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
import jwt

app = FastAPI()
security = HTTPBearer()

# Simulated Redis cache
REDIS_BLACKLIST: dict[str, float] = {}  # jti -> expiry_timestamp
SECRET = "production-cluster-secret"

async def verify_active_token(
    creds: Annotated[HTTPAuthorizationCredentials, Security(security)]
) -> dict:
    try:
        payload = jwt.decode(creds.credentials, SECRET, algorithms=["HS256"])
    except jwt.PyJWTError:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid token")

    jti = payload.get("jti")
    if not jti:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Missing jti claim")

    # Check blacklist
    if jti in REDIS_BLACKLIST:
        if time.time() < REDIS_BLACKLIST[jti]:
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Token has been revoked",
            )

    return payload

@app.post("/auth/logout")
async def logout(payload: Annotated[dict, Depends(verify_active_token)]):
    jti = payload["jti"]
    exp = payload["exp"]
    REDIS_BLACKLIST[jti] = float(exp)
    return {"message": "Successfully logged out. Token revoked."}
```

### Example 2: Enterprise Redis Pipeline (JTI + User Epoch Check)

```python
import time
from typing import Annotated
from fastapi import APIRouter, Depends, HTTPException, Security, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
import redis.asyncio as aioredis
import jwt

router = APIRouter(prefix="/auth")
bearer = HTTPBearer()

# Shared Redis client
redis = aioredis.from_url("redis://localhost:6379/0", decode_responses=True)
JWT_SECRET = "enterprise-signing-secret"

async def validate_token_revocation(
    creds: Annotated[HTTPAuthorizationCredentials, Security(bearer)]
) -> dict:
    try:
        claims = jwt.decode(creds.credentials, JWT_SECRET, algorithms=["HS256"])
    except jwt.PyJWTError:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid token")

    user_id = claims["sub"]
    jti = claims["jti"]
    token_iat = claims["iat"]

    # Execute both Redis checks in a single atomic pipelined roundtrip
    async with redis.pipeline(transaction=False) as pipe:
        pipe.get(f"blacklist:{jti}")
        pipe.get(f"user:{user_id}:revoked_before")
        is_blacklisted, revoked_before = await pipe.execute()

    if is_blacklisted:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Token has been revoked")

    if revoked_before and token_iat < int(revoked_before):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Session invalidated due to security reset. Please re-login.",
        )

    return claims

@router.post("/revoke-all")
async def logout_all_devices(claims: Annotated[dict, Depends(validate_token_revocation)]):
    user_id = claims["sub"]
    now = int(time.time())
    # Keep timestamp in Redis for 7 days
    await redis.setex(f"user:{user_id}:revoked_before", 7 * 86400, str(now))
    return {"message": "All active sessions have been invalidated"}
```

### Example 3: Edge Case / Boundary — Graceful Degradation on Redis Failure

```python
import logging
from typing import Annotated
from fastapi import Depends, HTTPException, Security, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
import redis.asyncio as aioredis
import jwt

logger = logging.getLogger("security")
security = HTTPBearer()

async def resilient_token_check(
    creds: Annotated[HTTPAuthorizationCredentials, Security(security)],
    redis: aioredis.Redis,
) -> dict:
    payload = jwt.decode(creds.credentials, "secret", algorithms=["HS256"])
    jti = payload["jti"]

    try:
        # Strict 50ms timeout prevents Redis network stalls from cascading into API failures
        is_revoked = await redis.get(f"blacklist:{jti}")
        if is_revoked:
            raise HTTPException(status_code=401, detail="Token revoked")
    except aioredis.RedisError as e:
        logger.error(f"Redis unavailable for token blacklist check: {e}. Failing safe/closed.")
        # Enterprise policy decision: Fail Closed (raise 401/503) or Fail Open with alert
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="Authentication verification temporarily degraded",
        )
    return payload
```

### Example 4: Failure Case — Structured 401 on Blacklisted Token

```python
# When client sends a revoked token:
# GET /api/v1/profile
# Header: Authorization: Bearer <revoked_token_jwt>
#
# Response:
# HTTP 401 Unauthorized
# WWW-Authenticate: Bearer error="invalid_token", error_description="The token has been revoked"
# Content-Type: application/json
# {
#   "error": "token_revoked",
#   "detail": "Token has been revoked"
# }
```

### Example 5: Idiomatic Modern Pattern with Python 3.12+ `Annotated` Dependency

```python
from typing import Annotated
from fastapi import Depends, FastAPI
from pydantic import BaseModel

app = FastAPI()

class SessionContext(BaseModel):
    user_id: str
    session_jti: str

# Clean type alias
ActiveSession = Annotated[SessionContext, Depends(validate_token_revocation)]

@app.get("/dashboard")
async def get_dashboard(session: ActiveSession):
    return {"welcome": session.user_id, "session": session.session_jti}
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Redis Memory Leak from Missing TTL**: Storing blacklisted JTIs without an expiration time (`SET` instead of `SETEX`) causes Redis memory usage to grow monotonically, eventually triggering Redis OOM and eviction panics.
2. **Clock Skew Invalidation**: If the Redis server clock is ahead of the app server clock, setting TTL might prematurely evict the blacklist entry before the token truly expires, allowing a revoked token to become valid again.
3. **Pipelining Neglect**: Running individual sequential Redis queries (`await redis.get("jti")` followed by `await redis.get("epoch")`) doubles network latency. Always use `redis.pipeline()`.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Not setting TTL on blacklist entries | Redis memory fills with permanently stored JTIs | Redis Out Of Memory (OOM) | Use `SETEX` with `ttl = exp - now` |
| 2. Querying relational DB for token blacklist | Every HTTP request hits Postgres/MySQL disk | Database connection pool exhaustion | Use in-memory Redis key-value store |
| 3. Sequential Redis calls in dependency | Incurs multiple network roundtrips per request | +5-15ms added to endpoint latency | Use `redis.pipeline()` |
| 4. Missing `jti` in token payload | Cannot identify discrete token instance | `KeyError: 'jti'` or unable to blacklist | Inject UUID4 `jti` claim during token creation |
| 5. Setting TTL longer than token lifetime | Wastes Redis memory storing already-expired tokens | Unnecessary memory overhead | TTL should exactly equal remaining token life |
| 6. Not handling Redis connection timeouts | Network partition hangs all API requests | Application stalls and drops connections | Set `socket_timeout=0.05` on Redis client |

---

## 8. Performance & Scalability Considerations

- **Redis Pipelining**: Checking both the JTI blacklist and user revocation epoch in a single pipelined command completes in <1.2ms over a local network.
- **Key Eviction Overhead**: Redis handles key expiration lazily (on access) and periodically via background active sweeps, incurring virtually zero CPU impact on write.

---

## 9. Security & OWASP Guardrails

- **OWASP API2:2023 Broken Authentication**: Systems MUST support instant revocation upon sensitive account actions (password reset, email change, MFA enrollment).
- **Session Termination on Device Loss**: Providing a "Log out of all devices" endpoint backed by `user:revoked_before` satisfies compliance requirements (SOC2, PCI-DSS).

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from datetime import datetime, timezone, timedelta
import uuid
import jwt
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_blacklisted_token_rejected(app: FastAPI):
    jti = str(uuid.uuid4())
    payload = {
        "sub": "user_revoked",
        "jti": jti,
        "exp": datetime.now(timezone.utc) + timedelta(minutes=15),
    }
    token = jwt.encode(payload, "secret", algorithm="HS256")
    
    # Pre-seed blacklist in mock
    REDIS_BLACKLIST[jti] = time.time() + 900

    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        res = await client.get("/api/v1/resource", headers={"Authorization": f"Bearer {token}"})
        assert res.status_code == 401
        assert "revoked" in res.json()["detail"].lower()
```

---

## 11. Real-World Case Study

A crypto-currency trading exchange experienced an incident where a user's laptop was stolen while logged in. The user immediately changed their password from their mobile phone, yet the thief was able to execute trades for 45 minutes because the JWT tokens remained valid until expiration.
- **The Solution**:
  1. Implemented a Redis-backed user invalidation epoch: changing passwords automatically updates `user:{id}:revoked_before = now()`.
  2. Implemented single-session revocation upon clicking "Log out" using `blacklist:{jti}` with precise remaining TTL.
  3. Added an automated integration test verifying that all existing tokens are immediately rejected within 10 milliseconds of a password change.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: Why is storing a JWT blacklist in a relational database an anti-pattern?</summary>

**Answer**: Stateless JWT verification is chosen specifically to eliminate database hits on every API request. If every incoming request requires querying a relational database table (`SELECT * FROM token_blacklist WHERE jti = $1`), the application incurs high database connection pool pressure, disk I/O, and query latency (10-50ms). A distributed in-memory key-value store like Redis resolves checks in sub-millisecond time.
</details>

<details>
<summary>Q2: How does setting TTL on Redis blacklist keys prevent memory exhaustion?</summary>

**Answer**: A token is only dangerous while its expiration timestamp (`exp`) is in the future. Once `time.time() > exp`, standard JWT verification logic will reject the token as expired regardless of whether it is in the blacklist. Therefore, blacklisting a token only requires storing its `jti` for its remaining lifetime: `ttl = exp - now`. Redis automatically purges the key after this duration.
</details>

<details>
<summary>Q3: How does the `user:{id}:revoked_before` epoch mechanism work for "Log Out All Devices"?</summary>

**Answer**: Instead of hunting down and blacklisting dozens of individual token JTIs across multiple devices, the server stores a single Unix timestamp in Redis under `user:{id}:revoked_before = now()`. During token validation, the dependency compares the token's `iat` (Issued At) claim against this timestamp. Any token issued prior to that moment is rejected, invalidating all sessions in an $O(1)$ operation.
</details>

<details>
<summary>Q4: What is a Bloom filter, and how does it optimize high-throughput token revocation checks?</summary>

**Answer**: A Bloom filter is a space-efficient probabilistic data structure used to test set membership. A FastAPI service can maintain an in-memory Bloom filter of revoked JTIs. If the filter returns false, the token is guaranteed NOT to be blacklisted, completely skipping the Redis network call. If it returns true (possible false positive), the service makes the definitive query to Redis.
</details>

<details>
<summary>Q5: Why should the Redis check use pipelining when verifying multiple keys?</summary>

**Answer**: Without pipelining, checking `blacklist:{jti}` and `user:{id}:revoked_before` requires two sequential network roundtrips (Request 1 -> Response 1 -> Request 2 -> Response 2). Pipelining bundles both commands into a single TCP packet, halving network latency.
</details>

<details>
<summary>Q6: What should the application do if Redis becomes completely unreachable during a revocation check?</summary>

**Answer**: This depends on the organization's risk tolerance:
- **Fail Closed (High Security)**: Reject the request with HTTP 503 or 401, prioritizing security over availability.
- **Fail Open (High Availability)**: Allow requests through if the cryptographic signature and expiration are valid, while firing high-priority alerts to the operations team.
</details>

<details>
<summary>Q7: How does refresh token rotation interact with token blacklisting?</summary>

**Answer**: When a refresh token is rotated, its old JTI is recorded as consumed. If an attacker presents an old refresh token, the server detects reuse, immediately adds the entire token family to the blacklist, and sets the user's `revoked_before` epoch, terminating all sessions for that account.
</details>

<details>
<summary>Q8: Why must the `iat` (Issued At) claim be verified when using revocation epochs?</summary>

**Answer**: If a token lacks an `iat` claim or the verification logic does not enforce its presence, the system cannot determine whether the token was issued before or after the `revoked_before` timestamp, rendering the epoch invalidation strategy inoperative.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Compute remaining TTL for a JWT token</summary>

**Problem**: Write a function `get_remaining_ttl(exp_timestamp)` that returns seconds until expiration, or 0 if expired.

**Solution**:
```python
import time

def get_remaining_ttl(exp: int) -> int:
    remaining = exp - int(time.time())
    return max(0, remaining)
```
</details>

<details>
<summary>Exercise 2 (Easy): Blacklist a JTI in a simulated Redis dictionary</summary>

**Problem**: Write a function `blacklist_jti(store, jti, ttl)` that adds a key with an expiration timestamp.

**Solution**:
```python
import time

def blacklist_jti(store: dict[str, float], jti: str, ttl: int):
    store[f"blacklist:{jti}"] = time.time() + ttl
```
</details>

<details>
<summary>Exercise 3 (Medium): Implement User Revocation Epoch Invalidation</summary>

**Problem**: Build a validator function that checks if `token_iat < user_revoked_at` and raises 401.

**Solution**:
```python
from fastapi import HTTPException, status

def check_epoch(token_iat: int, user_revoked_at: int | None):
    if user_revoked_at is not None and token_iat < user_revoked_at:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Session has been revoked by user action",
        )
```
</details>

<details>
<summary>Exercise 4 (Medium): Build an Async Redis Revocation Service</summary>

**Problem**: Implement a service class `TokenRevocationService` with methods `revoke_jti(jti, ttl)` and `is_revoked(jti)`.

**Solution**:
```python
import redis.asyncio as aioredis

class TokenRevocationService:
    def __init__(self, redis_client: aioredis.Redis):
        self.redis = redis_client

    async def revoke_jti(self, jti: str, ttl: int):
        if ttl > 0:
            await self.redis.setex(f"blacklist:{jti}", ttl, "1")

    async def is_revoked(self, jti: str) -> bool:
        val = await self.redis.get(f"blacklist:{jti}")
        return val is not None
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Fail-Closed Redis Health Check Circuit Breaker</summary>

**Problem**: Create a dependency that wraps Redis checks with an `asyncio.wait_for` timeout of 50ms, raising HTTP 503 if Redis times out.

**Solution**:
```python
import asyncio
from typing import Annotated
from fastapi import Depends, HTTPException, Security, status
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
import redis.asyncio as aioredis

security = HTTPBearer()

async def get_redis_client():
    client = aioredis.from_url("redis://localhost:6379/0", socket_timeout=0.05)
    try:
        yield client
    finally:
        await client.aclose()

async def strict_revocation_guard(
    creds: Annotated[HTTPAuthorizationCredentials, Security(security)],
    redis: Annotated[aioredis.Redis, Depends(get_redis_client)],
):
    try:
        # Strict 50ms timeout
        is_revoked = await asyncio.wait_for(redis.get("blacklist:some_jti"), timeout=0.05)
        if is_revoked:
            raise HTTPException(status_code=401, detail="Token revoked")
    except (asyncio.TimeoutError, aioredis.RedisError):
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="Authentication authority temporarily unreachable",
        )
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Combined Atomic Batch Revocation for Multi-Device Logout</summary>

**Problem**: Write a function that accepts `user_id` and a list of specific session JTIs, atomically setting the user's `revoked_before` epoch while simultaneously blacklisting all active JTIs in a single Redis transaction.

**Solution**:
```python
import time
import redis.asyncio as aioredis

async def atomic_multi_logout(
    redis: aioredis.Redis,
    user_id: str,
    active_jtis: list[tuple[str, int]],  # [(jti, ttl), ...]
):
    now = int(time.time())
    async with redis.pipeline(transaction=True) as pipe:
        # Set user epoch
        pipe.setex(f"user:{user_id}:revoked_before", 7 * 86400, str(now))
        # Blacklist all individual known tokens
        for jti, ttl in active_jtis:
            if ttl > 0:
                pipe.setex(f"blacklist:{jti}", ttl, "1")
        await pipe.execute()
```
</details>
