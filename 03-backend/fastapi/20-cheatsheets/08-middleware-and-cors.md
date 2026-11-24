# Quick Reference: Middleware, CORS & Security Headers Cheatsheet

## Production CORS Configuration

```python
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    # In production: explicitly declare domains (never use ["*"] with allow_credentials=True)
    allow_origins=[
        "https://app.mydomain.com",
        "https://admin.mydomain.com",
    ],
    allow_credentials=True,
    allow_methods=["GET", "POST", "PUT", "PATCH", "DELETE", "OPTIONS"],
    allow_headers=["Authorization", "Content-Type", "Idempotency-Key", "X-Request-ID"],
    expose_headers=["X-Request-ID", "X-RateLimit-Remaining"],
    max_age=86400,  # Cache preflight OPTIONS responses for 24 hours
)
```

---

## Custom Pure ASGI Middleware (High Performance)

```python
import time
from starlette.types import ASGIApp, Receive, Scope, Send

class RequestTimingASGIMiddleware:
    """Pure ASGI middleware avoids BaseHTTPMiddleware overhead."""
    def __init__(self, app: ASGIApp):
        self.app = app

    async def __call__(self, scope: Scope, receive: Receive, send: Send):
        if scope["type"] != "http":
            return await self.app(scope, receive, send)

        start_time = time.perf_counter()

        async def send_wrapper(message):
            if message["type"] == "http.response.start":
                duration = time.perf_counter() - start_time
                headers = list(message.get("headers", []))
                headers.append((b"x-process-time-ms", f"{duration * 1000:.2f}".encode()))
                message["headers"] = headers
            await send(message)

        await self.app(scope, receive, send_wrapper)

app.add_middleware(RequestTimingASGIMiddleware)
```

---

## Essential Production Security Headers

```python
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.requests import Request

class SecurityHeadersMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next):
        response = await call_next(request)
        response.headers["X-Content-Type-Options"] = "nosniff"
        response.headers["X-Frame-Options"] = "DENY"
        response.headers["X-XSS-Protection"] = "1; mode=block"
        response.headers["Strict-Transport-Security"] = "max-age=31536000; includeSubDomains"
        return response

app.add_middleware(SecurityHeadersMiddleware)
```
