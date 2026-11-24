# Module 07: Authentication & Security — 14: Content Security Policy and Security Headers (`14-content-security-policy-and-security-headers`)

> Security headers instruct modern browsers to enforce strict client-side sandboxing, preventing clickjacking, MIME-sniffing, XSS script injection, and protocol downgrade attacks. In FastAPI, security headers are applied uniformly using ASGI middleware while accounting for interactive documentation tools (Swagger UI and ReDoc).

| Field | Value |
|---|---|
| **Category** | Authentication & Security |
| **Target Level** | Intermediate / Production Engineer |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | starlette 1.10+, httpx 0.28+, pytest 9.1+ |

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

While backend logic validates input and authenticates users, the user's browser executes rendering engines that can be exploited by malicious HTML, rogue iframes, and MIME confusion:
- **HSTS (HTTP Strict Transport Security)**: Forces browsers to communicate exclusively over TLS, preventing SSL stripping attacks.
- **X-Content-Type-Options: nosniff**: Prevents the browser from executing non-executable MIME types (e.g. executing an uploaded image as JavaScript).
- **X-Frame-Options: DENY / SAMEORIGIN**: Protects against clickjacking by disallowing the page from being embedded in foreign iframes.
- **Content-Security-Policy (CSP)**: The definitive defense against Cross-Site Scripting (XSS). Dictates which origins can execute scripts, load stylesheets, or connect websockets.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|                       Essential HTTP Security Headers                             |
+-----------------------------------------------------------------------------------+
| Header Name                     | Recommended Production Value                    |
+---------------------------------+-------------------------------------------------+
| Strict-Transport-Security       | max-age=63072000; includeSubDomains; preload    |
| X-Content-Type-Options          | nosniff                                         |
| X-Frame-Options                 | DENY                                            |
| Referrer-Policy                 | strict-origin-when-cross-origin                 |
| Permissions-Policy              | camera=(), microphone=(), geolocation=()        |
| Content-Security-Policy         | default-src 'self'; frame-ancestors 'none';     |
+-----------------------------------------------------------------------------------+

ASGI Middleware Header Pipeline:

Client Request
      │
      ▼
Uvicorn ASGI Engine
      │
      ▼
Security Headers Middleware (Receives Downstream Response)
      │
      ├── Injects HSTS, nosniff, DENY, Referrer-Policy, Permissions-Policy
      │
      ├── Inspects path: Is it `/docs` or `/redoc`?
      │     ├── Yes: Apply relaxed CSP (allow CDN scripts & inline styles)
      │     └── No:  Apply strict default-src 'self' CSP
      │
      ▼
Client Browser Enforces Sandbox
```

---

## 3. How It Works Internally

1. **ASGI Response Header Modification**:
   - Headers in raw ASGI are lists of two-item byte tuples: `[(b"content-type", b"application/json"), ...]`.
   - Starlette middleware intercepts `http.response.start` and appends security headers before the first response body bytes are streamed to Uvicorn.
2. **CSP Evaluation in Browsers**:
   - The browser parses the `Content-Security-Policy` header before compiling inline `<script>` tags or fetching external CDN resources. Any violation halts execution and fires a telemetry payload to `report-to`.

---

## 4. Syntax & API Reference

```python
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.requests import Request
from starlette.responses import Response

class SecurityHeadersMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next) -> Response:
        response = await call_next(request)
        response.headers["Strict-Transport-Security"] = "max-age=63072000; includeSubDomains; preload"
        response.headers["X-Content-Type-Options"] = "nosniff"
        response.headers["X-Frame-Options"] = "DENY"
        response.headers["Referrer-Policy"] = "strict-origin-when-cross-origin"
        response.headers["Permissions-Policy"] = "camera=(), microphone=(), geolocation=()"
        return response
```

---

## 5. Verified Code Examples

### Example 1: Minimal Production Security Headers Middleware

```python
from fastapi import FastAPI
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.requests import Request

app = FastAPI()

class ProductionSecurityHeaders(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next):
        response = await call_next(request)
        response.headers["X-Content-Type-Options"] = "nosniff"
        response.headers["X-Frame-Options"] = "DENY"
        response.headers["Strict-Transport-Security"] = "max-age=31536000; includeSubDomains"
        return response

app.add_middleware(ProductionSecurityHeaders)

@app.get("/health")
def health():
    return {"status": "ok"}
```

### Example 2: Enterprise Adaptive CSP Middleware (Supporting Swagger UI & ReDoc)

```python
from fastapi import FastAPI
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.requests import Request
from starlette.responses import Response

app = FastAPI(docs_url="/docs", redoc_url="/redoc")

STRICT_CSP = (
    "default-src 'self'; "
    "img-src 'self' data:; "
    "font-src 'self'; "
    "frame-ancestors 'none'; "
    "object-src 'none'; "
    "base-uri 'self';"
)

# Swagger UI requires CDN assets and inline scripts
DOCS_CSP = (
    "default-src 'self'; "
    "script-src 'self' 'unsafe-inline' https://cdn.jsdelivr.net; "
    "style-src 'self' 'unsafe-inline' https://cdn.jsdelivr.net; "
    "img-src 'self' data: https://fastapi.tiangolo.com; "
    "frame-ancestors 'none';"
)

class EnterpriseSecurityHeadersMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next) -> Response:
        response = await call_next(request)
        
        # Apply strict baseline headers
        response.headers["X-Content-Type-Options"] = "nosniff"
        response.headers["X-Frame-Options"] = "DENY"
        response.headers["Strict-Transport-Security"] = "max-age=63072000; includeSubDomains; preload"
        response.headers["Referrer-Policy"] = "strict-origin-when-cross-origin"
        response.headers["Permissions-Policy"] = "camera=(), microphone=(), geolocation=(), payment=()"

        # Adapt CSP depending on route
        if request.url.path in ("/docs", "/redoc", "/openapi.json"):
            response.headers["Content-Security-Policy"] = DOCS_CSP
        else:
            response.headers["Content-Security-Policy"] = STRICT_CSP

        return response

app.add_middleware(EnterpriseSecurityHeadersMiddleware)
```

### Example 3: Edge Case / Boundary — Nonce-Based CSP for Dynamic Templates

```python
import secrets
from fastapi import FastAPI, Request
from fastapi.responses import HTMLResponse

app = FastAPI()

@app.get("/dashboard", response_class=HTMLResponse)
async def dashboard(request: Request):
    # Generate cryptographic one-time nonce
    nonce = secrets.token_urlsafe(16)
    
    html_content = f"""
    <!DOCTYPE html>
    <html>
      <head>
        <title>Secured Dashboard</title>
      </head>
      <body>
        <h1>Hello Secured World</h1>
        <script nonce="{nonce}">
          console.log("Legitimate script executed with nonce!");
        </script>
      </body>
    </html>
    """
    
    response = HTMLResponse(content=html_content)
    # Browsers will reject any inline script lacking this exact nonce
    response.headers["Content-Security-Policy"] = f"default-src 'self'; script-src 'self' 'nonce-{nonce}';"
    return response
```

### Example 4: Failure Case — Browser Rejection of CSP Violation

```python
# When an attacker injects <script src="http://evil.com/leak.js"></script>
# into a page guarded by:
# Content-Security-Policy: default-src 'self'
#
# Browser Console Error:
# Refused to load the script 'http://evil.com/leak.js' because it violates
# the following Content Security Policy directive: "default-src 'self'".
# Note that 'script-src' was not explicitly set, so 'default-src' is used as a fallback.
```

### Example 5: Idiomatic Modern Pure ASGI Middleware (Zero Overhead)

```python
class PureASGISecurityHeaders:
    def __init__(self, app):
        self.app = app

    async def __call__(self, scope, receive, send):
        if scope["type"] != "http":
            return await self.app(scope, receive, send)

        async def send_with_security_headers(message):
            if message["type"] == "http.response.start":
                headers = list(message.get("headers", []))
                headers.extend([
                    (b"x-content-type-options", b"nosniff"),
                    (b"x-frame-options", b"DENY"),
                    (b"strict-transport-security", b"max-age=31536000; includeSubDomains"),
                ])
                message["headers"] = headers
            await send(message)

        await self.app(scope, receive, send_with_security_headers)
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Breaking Swagger UI with Strict CSP**: Applying `Content-Security-Policy: default-src 'self'` globally without exemptions renders `/docs` completely blank because Swagger UI relies on `cdn.jsdelivr.net` and inline JavaScript.
2. **Missing `includeSubDomains` in HSTS**: If HSTS omits `includeSubDomains`, an attacker can execute SSL stripping against subdomains (`api.company.com` vs `company.com`).
3. **MIME Sniffing Vulnerabilities**: When serving user-uploaded files, omitting `X-Content-Type-Options: nosniff` allows Internet Explorer and older browsers to interpret an uploaded `.jpg` containing HTML/JS as an active script, leading to stored XSS.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Setting global strict CSP without exempting `/docs` | Swagger UI scripts and styles blocked by browser | Blank screen at `/docs` with console CSP errors | Apply relaxed CSP or host Swagger assets locally |
| 2. Hardcoding `X-Frame-Options: ALLOW-FROM` | Deprecated in modern browsers; ignored by Chrome | Warning in browser console; clickjacking vulnerability | Use CSP `frame-ancestors 'none'` or `'self'` |
| 3. Using HSTS over plain HTTP | Browsers ignore HSTS headers delivered over insecure HTTP | HSTS not recognized | Terminate TLS and redirect HTTP to HTTPS first |
| 4. Omission of `object-src 'none'` in CSP | Leaves browser vulnerable to malicious Flash/Java applets | Security scanner alert | Explicitly add `object-src 'none'` |
| 5. Setting `max-age=0` on HSTS | Disables HSTS protection immediately | SSL-stripping attack vector | Set `max-age=31536000` (1 year) or greater |
| 6. Modifying response headers after body streaming began | ASGI protocol error | `RuntimeError: Response already started` | Append headers in `http.response.start` |

---

## 8. Performance & Scalability Considerations

- **Header Injection Overhead**: Adding 5-6 byte headers to an ASGI response takes <0.0005ms per request.
- **Pure ASGI vs `BaseHTTPMiddleware`**: Using pure ASGI middleware (`__call__(scope, receive, send)`) avoids the task-spawning overhead of `BaseHTTPMiddleware`, improving RPS by ~15-20% under high concurrency.

---

## 9. Security & OWASP Guardrails

- **OWASP ASVS v4.0.3 Chapter 14.4**: Verify that all responses contain recommended security headers (`nosniff`, `HSTS`, `CSP`, `Referrer-Policy`).
- **HSTS Preload**: Submitting domains to the Chrome HSTS Preload list hardcodes HTTPS requirements into browser binary updates, eliminating the initial unencrypted connection.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_security_headers_present(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        res = await client.get("/health")
        assert res.status_code == 200
        assert res.headers["x-content-type-options"] == "nosniff"
        assert res.headers["x-frame-options"] == "DENY"
        assert "max-age=" in res.headers["strict-transport-security"]
```

---

## 11. Real-World Case Study

A banking portal hosted its marketing content and API on the same domain. Attackers embedded the money transfer interface in a transparent iframe on a gaming website, overlaying a "Click to win prize" button directly on top of the "Confirm Transfer" button (Clickjacking).
- **Remediation**:
  1. Deployed `X-Frame-Options: DENY` and `Content-Security-Policy: frame-ancestors 'none'`.
  2. The browser immediately refused to render the application inside any iframe, instantly eliminating the clickjacking exploit across all customer accounts.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: Why has `Content-Security-Policy: frame-ancestors` largely superseded `X-Frame-Options`?</summary>

**Answer**: `X-Frame-Options` only supports binary directives (`DENY`, `SAMEORIGIN`) or the deprecated `ALLOW-FROM uri` (which modern browsers ignore). `frame-ancestors` in CSP Level 2 allows granular whitelisting of multiple parent origins (e.g., `frame-ancestors 'self' https://partner.com https://dashboard.com`) and is supported across all modern evergreen browsers.
</details>

<details>
<summary>Q2: What is the purpose of `X-Content-Type-Options: nosniff`?</summary>

**Answer**: Historically, browsers tried to be "helpful" by inspecting the contents of downloaded files to determine their type (MIME-sniffing), ignoring the server's `Content-Type` header. If an attacker uploaded a text or image file containing JavaScript (`<script>alert(1)</script>`), the browser would execute it. `nosniff` forces the browser to adhere strictly to the declared `Content-Type`.
</details>

<details>
<summary>Q3: How does HSTS protect against SSL-Stripping attacks?</summary>

**Answer**: When a user types `example.com` into a browser, the browser initially attempts an insecure HTTP request before being redirected to HTTPS. An attacker on the local network (e.g. coffee shop Wi-Fi) can intercept this initial HTTP request and proxy it, serving plain HTTP to the victim while communicating via HTTPS with the server (SSL stripping). HSTS tells the browser to never make unencrypted requests to that domain for the specified `max-age`.
</details>

<details>
<summary>Q4: Why does FastAPI's default `/docs` page fail to load if a strict `default-src 'self'` CSP is applied?</summary>

**Answer**: FastAPI's default Swagger UI documentation loads JavaScript and CSS bundles from third-party CDNs (`cdn.jsdelivr.net`) and executes inline initialization scripts. A strict `default-src 'self'` policy forbids loading external scripts and executing inline code, causing the browser to block Swagger assets.
</details>

<details>
<summary>Q5: What is the difference between a hash-based CSP and a nonce-based CSP?</summary>

**Answer**:
- **Nonce-based**: The server generates a unique cryptographic random token per request (e.g., `nonce-XYZ`), attaches it to the CSP header, and includes the matching `nonce="XYZ"` attribute in permitted `<script>` tags.
- **Hash-based**: The server computes the cryptographic SHA-256 hash of the exact static inline script body (e.g. `'sha256-abcdef...'`) and places that hash in the CSP header.
</details>

<details>
<summary>Q6: What is the purpose of the `Permissions-Policy` header?</summary>

**Answer**: `Permissions-Policy` (formerly `Feature-Policy`) allows a web server to restrict or disable browser hardware APIs and sensitive features (such as `camera`, `microphone`, `geolocation`, `accelerometer`, and `payment`) for the page and any embedded third-party iframes.
</details>

<details>
<summary>Q7: Why should pure ASGI middleware be preferred over `BaseHTTPMiddleware` for header injection?</summary>

**Answer**: `BaseHTTPMiddleware` creates Python asyncio tasks and wraps request/response streams in memory-buffered generators, adding ~0.2ms latency and reducing maximum RPS. Pure ASGI middleware simply wraps the `send` callable, appending headers in the existing ASGI protocol dictionary with zero task-spawning overhead.
</details>

<details>
<summary>Q8: What is the purpose of `Referrer-Policy: strict-origin-when-cross-origin`?</summary>

**Answer**: It controls how much referrer information is sent when navigating away from the page. For same-origin requests, the full URL path is sent. For cross-origin requests, only the domain origin is sent (preventing URL query parameter leaks). If downgrading from HTTPS to HTTP, no referrer header is sent.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Add `X-Content-Type-Options` and `X-Frame-Options` to all responses</summary>

**Problem**: Create a Starlette middleware that attaches `nosniff` and `DENY` to every HTTP response.

**Solution**:
```python
from fastapi import FastAPI
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.requests import Request

app = FastAPI()

class BasicSecurityHeaders(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next):
        res = await call_next(request)
        res.headers["X-Content-Type-Options"] = "nosniff"
        res.headers["X-Frame-Options"] = "DENY"
        return res

app.add_middleware(BasicSecurityHeaders)
```
</details>

<details>
<summary>Exercise 2 (Easy): Verify security headers using pytest and httpx</summary>

**Problem**: Write a test verifying that `Strict-Transport-Security` contains `max-age` and `includeSubDomains`.

**Solution**:
```python
import pytest
from httpx import ASGITransport, AsyncClient

@pytest.mark.asyncio
async def test_hsts_header(app):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        res = await client.get("/health")
        hsts = res.headers.get("strict-transport-security", "")
        assert "max-age=" in hsts
        assert "includesubdomains" in hsts.lower()
```
</details>

<details>
<summary>Exercise 3 (Medium): Build Pure ASGI Security Headers Middleware</summary>

**Problem**: Write a class implementing `__call__(scope, receive, send)` that injects `Referrer-Policy: no-referrer`.

**Solution**:
```python
class AsgiReferrerPolicy:
    def __init__(self, app):
        self.app = app

    async def __call__(self, scope, receive, send):
        if scope["type"] != "http":
            return await self.app(scope, receive, send)

        async def send_wrapper(message):
            if message["type"] == "http.response.start":
                headers = list(message.get("headers", []))
                headers.append((b"referrer-policy", b"no-referrer"))
                message["headers"] = headers
            await send(message)

        await self.app(scope, receive, send_wrapper)
```
</details>

<details>
<summary>Exercise 4 (Medium): Implement Path-Conditional CSP for Docs vs API</summary>

**Problem**: Middleware that attaches `default-src 'self'` for `/api/*` and permits `cdn.jsdelivr.net` on `/docs`.

**Solution**:
```python
from starlette.middleware.base import BaseHTTPMiddleware

class RouteSpecificCSP(BaseHTTPMiddleware):
    async def dispatch(self, request, call_next):
        res = await call_next(request)
        if request.url.path.startswith("/docs"):
            res.headers["Content-Security-Policy"] = "default-src 'self' 'unsafe-inline' https://cdn.jsdelivr.net;"
        else:
            res.headers["Content-Security-Policy"] = "default-src 'self';"
        return res
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Nonce-Injected HTML Response Dependency</summary>

**Problem**: Write a dependency that creates a random nonce, sets it in the CSP response header, and returns the nonce string for template injection.

**Solution**:
```python
import secrets
from fastapi import FastAPI, Request, Response

app = FastAPI()

def inject_csp_nonce(response: Response) -> str:
    nonce = secrets.token_urlsafe(16)
    response.headers["Content-Security-Policy"] = f"default-src 'self'; script-src 'self' 'nonce-{nonce}';"
    return nonce

@app.get("/view")
def view_page(response: Response):
    nonce = inject_csp_nonce(response)
    return {"nonce": nonce, "html_template": f'<script nonce="{nonce}">console.log(1)</script>'}
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Complete OWASP Recommended Header Stack</summary>

**Problem**: Build a comprehensive middleware enforcing HSTS, nosniff, DENY, Permissions-Policy, Referrer-Policy, and Cross-Origin policies (COOP and COEP).

**Solution**:
```python
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.requests import Request
from starlette.responses import Response

class OWASPFullSecurityHeadersMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next) -> Response:
        response = await call_next(request)
        h = response.headers
        h["Strict-Transport-Security"] = "max-age=63072000; includeSubDomains; preload"
        h["X-Content-Type-Options"] = "nosniff"
        h["X-Frame-Options"] = "DENY"
        h["Referrer-Policy"] = "strict-origin-when-cross-origin"
        h["Permissions-Policy"] = "accelerometer=(), camera=(), geolocation=(), gyroscope=(), microphone=()"
        h["Cross-Origin-Opener-Policy"] = "same-origin"
        h["Cross-Origin-Embedder-Policy"] = "require-corp"
        h["Content-Security-Policy"] = "default-src 'self'; frame-ancestors 'none'; object-src 'none';"
        return response
```
</details>
