# Technical Interview Mastery: Live Coding Challenges & Real-Time Implementations

## Overview & Evaluation Criteria

Live coding rounds evaluate speed, syntactic precision, edge-case coverage, and idiomatic modern FastAPI and Python 3.12+ features.

---

## Challenge 1: Distributed Sliding Window Rate Limiter Dependency

**Task**: Write a self-contained, reusable FastAPI dependency `RateLimiter(times=5, seconds=60)` using Redis and async operations that injects standard rate-limit headers.

### Live Coding Solution:

```python
import time
from fastapi import Depends, HTTPException, Request, Response, status
import redis.asyncio as aioredis

redis_client = aioredis.from_url("redis://localhost:6379/0", decode_responses=True)


class RateLimiter:
    def __init__(self, times: int = 60, seconds: int = 60):
        self.times = times
        self.window_ms = seconds * 1000

    async def __call__(self, request: Request, response: Response):
        client_ip = request.client.host if request.client else "unknown"
        key = f"rl:{request.url.path}:{client_ip}"
        now_ms = int(time.time() * 1000)
        clear_before = now_ms - self.window_ms

        # Pipeline atomic execution
        pipe = redis_client.pipeline()
        pipe.zremrangebyscore(key, 0, clear_before)
        pipe.zcard(key)
        pipe.zadd(key, {str(now_ms): now_ms})
        pipe.pexpire(key, self.window_ms)
        results = await pipe.execute()

        current_count = results[1]
        remaining = max(0, self.times - current_count - 1)

        response.headers["X-RateLimit-Limit"] = str(self.times)
        response.headers["X-RateLimit-Remaining"] = str(remaining)

        if current_count >= self.times:
            raise HTTPException(
                status_code=status.HTTP_429_TOO_MANY_REQUESTS,
                detail="Rate limit exceeded",
                headers={"Retry-After": str(self.window_ms // 1000)},
            )
```

---

## Challenge 2: Dynamic JWT Authentication & Role-Check Dependency

**Task**: Implement a clean, parameterized `has_role(required_role: str)` security dependency that validates a Bearer JWT, decodes user claims, and enforces role access.

### Live Coding Solution:

```python
from typing import Annotated
from fastapi import Depends, HTTPException, Security, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
import jwt

SECRET_KEY = "super-secret-jwt-signing-key"
ALGORITHM = "HS256"
security_scheme = HTTPBearer()


def get_current_user_claims(
    creds: Annotated[HTTPAuthorizationCredentials, Security(security_scheme)]
) -> dict:
    try:
        payload = jwt.decode(creds.credentials, SECRET_KEY, algorithms=[ALGORITHM])
        return payload
    except jwt.ExpiredSignatureError:
        raise HTTPException(status_code=401, detail="Token has expired")
    except Exception:
        raise HTTPException(status_code=401, detail="Invalid token credentials")


def require_role(required_role: str):
    def role_checker(claims: dict = Depends(get_current_user_claims)) -> dict:
        roles: list[str] = claims.get("roles", [])
        if required_role not in roles:
            raise HTTPException(
                status_code=status.HTTP_403_FORBIDDEN,
                detail=f"Access denied: Required role '{required_role}' missing",
            )
        return claims

    return role_checker
```

---

## Challenge 3: Idempotent POST Endpoint with Redis Mutex

**Task**: Write an endpoint `POST /orders` that requires an `Idempotency-Key` header. If called twice with the same key, it must not execute the order logic twice.

### Live Coding Solution:

```python
from fastapi import APIRouter, Header, HTTPException, status
from pydantic import BaseModel
import redis.asyncio as aioredis

router = APIRouter()
redis_client = aioredis.from_url("redis://localhost:6379/0", decode_responses=True)


class OrderIn(BaseModel):
    item_id: str
    quantity: int


@router.post("/orders", status_code=status.HTTP_201_CREATED)
async def create_order(
    order: OrderIn,
    idempotency_key: str = Header(..., alias="Idempotency-Key"),
):
    lock_key = f"lock:order:{idempotency_key}"
    result_key = f"result:order:{idempotency_key}"

    # 1. Check if already computed
    cached_response = await redis_client.get(result_key)
    if cached_response:
        return {"order_id": cached_response, "status": "REPLAYED"}

    # 2. Acquire Redis distributed lock
    acquired = await redis_client.set(lock_key, "PROCESSING", nx=True, ex=30)
    if not acquired:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail="Order with this Idempotency-Key is currently being processed",
        )

    try:
        # Simulate business logic & DB creation
        order_id = f"ord_{order.item_id}_9921"

        # Cache result for 24 hours
        await redis_client.set(result_key, order_id, ex=86400)
        return {"order_id": order_id, "status": "CREATED"}
    finally:
        await redis_client.delete(lock_key)
```
