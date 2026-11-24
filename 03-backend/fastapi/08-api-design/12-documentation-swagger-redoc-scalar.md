# Module 08: API Design — 12: Documentation: Swagger UI, ReDoc, and Scalar (`12-documentation-swagger-redoc-scalar`)

> Automated interactive documentation is one of FastAPI's most celebrated features. Beyond default CDN-hosted Swagger UI and ReDoc, production environments demand air-gapped self-hosted assets, modern UI alternatives like Scalar, tuned UI parameters (`persistAuthorization`), and authenticated doc access gates.

| Field | Value |
|---|---|
| **Category** | API Design & Standards |
| **Target Level** | Intermediate / Production Engineer |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | httpx 0.28+, pytest 9.1+, starlette 1.10+ |

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

FastAPI provides out-of-the-box HTML interfaces to visualize and interact with the OpenAPI schema:
- **Swagger UI (`/docs`)**: The ubiquitous industry standard. Supports interactive "Try it out" HTTP requests directly from the browser, authorization modals, and curl code snippets.
- **ReDoc (`/redoc`)**: A three-panel, read-only layout popular for public API reference documentation. Highly readable with nested schema folding and code samples.
- **Scalar (`/scalar`)**: A modern, blazing-fast API client and documentation interface written in Vue/TypeScript that combines Swagger's interactive testing with ReDoc's three-panel elegance and instant theme customization.
- **Air-Gapped & Offline Deployments**: Default FastAPI documentation loads JavaScript and CSS bundles from `cdn.jsdelivr.net`. In secure enterprise VPCs without external internet access, CDNs fail, leaving documentation blank unless self-hosted via `StaticFiles`.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               FastAPI Documentation UI Architecture                               |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|                      [ GET /openapi.json ]                                        |
|                                │                                                  |
|        ┌───────────────────────┼───────────────────────┐                          |
|        ▼                       ▼                       ▼                          |
|  [ Swagger UI ]         [ ReDoc UI ]             [ Scalar UI ]                    |
|  Route: `/docs`         Route: `/redoc`          Route: `/scalar`                 |
|  - Interactive tests    - 3-panel layout         - Modern aesthetics              |
|  - "Authorize" modal    - Read-only reference    - Built-in API Client            |
|  - CDN / Local assets   - Search bar             - Offline / Air-gapped           |
|                                                                                   |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **`fastapi.openapi.docs`**:
   - Contains factory functions: `get_swagger_ui_html()`, `get_redoc_html()`.
   - Generates minimal HTML shells containing script and link tags referencing external or local JS/CSS files and configuring the viewer's `url="/openapi.json"`.
2. **`swagger_ui_parameters`**:
   - Passed directly to Swagger UI's initialization object. Allows customizing behavior like `persistAuthorization: True` (retains bearer tokens across page refreshes) and `docExpansion: "none"`.

---

## 4. Syntax & API Reference

```python
from fastapi import FastAPI
from fastapi.openapi.docs import get_swagger_ui_html, get_redoc_html

# 1. Customizing Swagger parameters on app instantiation
app = FastAPI(
    docs_url="/docs",
    redoc_url="/redoc",
    swagger_ui_parameters={
        "persistAuthorization": True,     # Retain token on refresh
        "defaultModelsExpandDepth": -1,   # Hide "Schemas" section at bottom
        "displayRequestDuration": True,   # Show request duration in ms
        "filter": True,                   # Enable search filter bar
    },
)

# 2. Modern Scalar HTML generation
def get_scalar_html(openapi_url: str, title: str) -> str:
    return f"""
    <!doctype html>
    <html>
      <head>
        <title>{title}</title>
        <meta charset="utf-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1" />
      </head>
      <body>
        <script id="api-reference" data-url="{openapi_url}"></script>
        <script src="https://cdn.jsdelivr.net/npm/@scalar/api-reference"></script>
      </body>
    </html>
    """
```

---

## 5. Verified Code Examples

### Example 1: Minimal Scalar Integration alongside Swagger & ReDoc

```python
from fastapi import FastAPI
from fastapi.responses import HTMLResponse

app = FastAPI(title="Triple-Engine Documentation API")

@app.get("/scalar", response_class=HTMLResponse, include_in_schema=False)
async def scalar_html():
    return """
    <!doctype html>
    <html>
      <head>
        <title>Scalar API Reference</title>
        <meta charset="utf-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1" />
      </head>
      <body>
        <script id="api-reference" data-url="/openapi.json"></script>
        <script src="https://cdn.jsdelivr.net/npm/@scalar/api-reference"></script>
      </body>
    </html>
    """

@app.get("/items")
def list_items():
    return [{"id": 1, "name": "Standard item"}]
```

### Example 2: Enterprise Air-Gapped / Offline Documentation with StaticFiles

```python
from fastapi import FastAPI
from fastapi.openapi.docs import get_swagger_ui_html, get_redoc_html
from fastapi.staticfiles import StaticFiles

# Disable default CDN routes
app = FastAPI(docs_url=None, redoc_url=None)

# Mount local assets directory (e.g. static/swagger-ui/*)
# In real application: app.mount("/static", StaticFiles(directory="static"), name="static")

@app.get("/docs", include_in_schema=False)
async def custom_swagger_ui():
    return get_swagger_ui_html(
        openapi_url=app.openapi_url or "/openapi.json",
        title=f"{app.title} - Offline Docs",
        swagger_js_url="/static/swagger-ui-bundle.js",
        swagger_css_url="/static/swagger-ui.css",
        swagger_favicon_url="/static/favicon.png",
    )

@app.get("/redoc", include_in_schema=False)
async def custom_redoc():
    return get_redoc_html(
        openapi_url=app.openapi_url or "/openapi.json",
        title=f"{app.title} - Offline ReDoc",
        redoc_js_url="/static/redoc.standalone.js",
        redoc_favicon_url="/static/favicon.png",
    )
```

### Example 3: Edge Case / Boundary — Securing Documentation behind HTTP Basic Auth

```python
from typing import Annotated
import secrets
from fastapi import Depends, FastAPI, HTTPException, status
from fastapi.openapi.docs import get_swagger_ui_html
from fastapi.security import HTTPBasic, HTTPBasicCredentials

app = FastAPI(docs_url=None)  # Disable public /docs
security = HTTPBasic()

def require_doc_access(creds: Annotated[HTTPBasicCredentials, Depends(security)]):
    is_user = secrets.compare_digest(creds.username, "internal_dev")
    is_pass = secrets.compare_digest(creds.password, "StagingSecret2026!")
    if not (is_user and is_pass):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Authentication required to view documentation",
            headers={"WWW-Authenticate": "Basic"},
        )
    return creds.username

@app.get("/docs", include_in_schema=False)
def protected_swagger_docs(username: Annotated[str, Depends(require_doc_access)]):
    return get_swagger_ui_html(openapi_url="/openapi.json", title="Internal Documentation")
```

### Example 4: Failure Case — Rejecting Missing Static Files in Air-Gapped Mode

```python
# If custom swagger_js_url points to non-existent local file:
# GET /static/swagger-ui-bundle.js -> 404 Not Found
#
# Result: Browser renders blank white page with console error:
# "Uncaught ReferenceError: SwaggerUIBundle is not defined"
```

### Example 5: Idiomatic Modern Pattern with Advanced Swagger UI Configuration

```python
from fastapi import FastAPI

app = FastAPI(
    title="Optimized Swagger API",
    swagger_ui_parameters={
        # Retain auth token in browser localStorage across page reloads
        "persistAuthorization": True,
        # Collapse all tags by default to keep long API specifications readable
        "docExpansion": "none",
        # Enable live text filter box at top of Swagger page
        "filter": True,
        # Syntax highlighting theme
        "syntaxHighlight.theme": "monokai",
        # Hide the redundant Schemas section at the bottom of the page
        "defaultModelsExpandDepth": -1,
    },
)

@app.get("/data")
def read_data():
    return {"message": "Swagger UI tuned"}
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **CDN Outages and Air-Gapped Networks**: Default FastAPI documentation loads from `cdn.jsdelivr.net`. If the CDN has an outage or your server runs in a PCI-compliant air-gapped private subnet, the documentation fails to render completely. Always host assets locally in air-gapped systems.
2. **`persistAuthorization` Security Risks on Shared Machines**: Setting `persistAuthorization: True` saves authentication tokens in the browser's `localStorage`. On shared developer workstations, previous developers' tokens remain active.
3. **CSP Blocking CDN Scripts**: If you apply a strict `Content-Security-Policy: default-src 'self'` without exempting `cdn.jsdelivr.net`, the browser blocks Swagger and ReDoc scripts, displaying a blank screen.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Relying on CDNs in offline/secure VPCs | Network timeouts loading `cdn.jsdelivr.net` | Blank screen at `/docs` | Use `get_swagger_ui_html` with local `/static` files |
| 2. Leaving `/docs` unprotected in public staging | Competitors and attackers inspect private APIs | Information disclosure | Guard `/docs` with HTTP Basic Authentication |
| 3. Re-authenticating on every page reload | Swagger UI resets token on refresh by default | Developer annoyance | Set `swagger_ui_parameters={"persistAuthorization": True}` |
| 4. Including `/docs` in OpenAPI schema | Causes recursive schema documentation | Cluttered API documentation | Set `include_in_schema=False` on custom doc routes |
| 5. Not disabling `docs_url` when using custom doc routes | Mounts two conflicting `/docs` endpoints | Routing ambiguity | Set `FastAPI(docs_url=None)` |
| 6. Allowing all tags to be expanded by default | A 200-endpoint API creates a massive 50,000px page | Slow browser rendering | Set `"docExpansion": "none"` |

---

## 8. Performance & Scalability Considerations

- **Browser DOM Load**: Setting `"docExpansion": "none"` prevents Swagger UI from mounting thousands of DOM elements on initial load, reducing initial page load time from 4s to 0.3s for large APIs.
- **Static Asset Caching**: Cache local Swagger JS/CSS files with `Cache-Control: public, max-age=31536000, immutable`.

---

## 9. Security & OWASP Guardrails

- **OWASP API8:2023 Security Misconfiguration**: Disable public access to documentation on public-facing production APIs. Protect internal staging docs with authentication or VPN/IP whitelisting.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_swagger_and_scalar_endpoints(app: FastAPI):
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Check Swagger UI HTML
        res_swagger = await client.get("/docs")
        assert res_swagger.status_code == 200
        assert "swagger-ui" in res_swagger.text.lower()

        # Check Scalar HTML
        res_scalar = await client.get("/scalar")
        assert res_scalar.status_code == 200
        assert "@scalar/api-reference" in res_scalar.text
```

---

## 11. Real-World Case Study

A healthcare analytics platform deployed their FastAPI service into an isolated AWS VPC with zero outbound internet egress for HIPAA compliance. Upon deployment, QA reported that `/docs` and `/redoc` were completely broken, rendering white screens because the browser could not reach `cdn.jsdelivr.net`.
- **The Solution**:
  1. Downloaded `swagger-ui-bundle.js`, `swagger-ui.css`, and `redoc.standalone.js` into the repository's `static/` directory.
  2. Mounted `StaticFiles(directory="static")`.
  3. Overrode `/docs` and `/redoc` using `get_swagger_ui_html()` pointing to local static assets.
  Documentation loaded instantaneously with 100% network isolation.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: How does FastAPI generate Swagger UI and ReDoc pages without requiring an external server?</summary>

**Answer**: FastAPI includes built-in Starlette HTML responses in `fastapi.openapi.docs`. When a user requests `/docs` or `/redoc`, FastAPI serves a small HTML template string containing JavaScript and CSS dependencies that fetch `/openapi.json` and render the interactive interface directly inside the client's browser.
</details>

<details>
<summary>Q2: What is the benefit of using Scalar over Swagger UI?</summary>

**Answer**: Scalar provides a modern, fast, and visually polished alternative to Swagger UI. It combines ReDoc's popular three-panel layout with Swagger UI's interactive testing capabilities, provides built-in code snippets in a dozen languages, supports instant dark/light themes, and has a smaller JavaScript footprint.
</details>

<details>
<summary>Q3: How do you implement offline / air-gapped documentation in FastAPI?</summary>

**Answer**:
1. Disable default docs on instantiation: `FastAPI(docs_url=None, redoc_url=None)`.
2. Serve local JS and CSS assets via `app.mount("/static", StaticFiles(...))`.
3. Create custom endpoints returning `get_swagger_ui_html()` with `swagger_js_url` and `swagger_css_url` pointing to the local `/static` paths.
</details>

<details>
<summary>Q4: What does the `persistAuthorization: True` parameter do in Swagger UI?</summary>

**Answer**: By default, refreshing the browser page in Swagger UI clears any Bearer token entered in the "Authorize" modal. Setting `persistAuthorization: True` stores the token in the browser's `localStorage`, retaining the authenticated state across page reloads and browser sessions.
</details>

<details>
<summary>Q5: How can you protect `/docs` with authentication while keeping the actual API endpoints accessible to public clients?</summary>

**Answer**: Disable default docs (`docs_url=None`) and create a dedicated `@app.get("/docs")` endpoint guarded by an authentication dependency (such as HTTP Basic Auth). Clients must provide valid admin credentials to access the documentation HTML, while public API routes execute without documentation credentials.
</details>

<details>
<summary>Q6: Why should `include_in_schema=False` be passed to custom documentation routes?</summary>

**Answer**: If `include_in_schema=False` is omitted, the `/docs` route will recursively document itself inside the OpenAPI specification, polluting the API reference with internal documentation viewing routes.
</details>

<details>
<summary>Q7: How do you hide the large "Schemas" section at the bottom of the Swagger UI interface?</summary>

**Answer**: Set `"defaultModelsExpandDepth": -1` inside the `swagger_ui_parameters` dictionary passed to `FastAPI()`. This cleans up the interface, showing only the endpoints and tags.
</details>

<details>
<summary>Q8: How does Content Security Policy (CSP) interact with CDN-hosted documentation?</summary>

**Answer**: A strict `Content-Security-Policy: default-src 'self'` blocks external scripts and stylesheets from loading. If using default FastAPI documentation, the CSP must explicitly whitelist `script-src 'self' 'unsafe-inline' https://cdn.jsdelivr.net; style-src 'self' 'unsafe-inline' https://cdn.jsdelivr.net;`.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Enable persistAuthorization in Swagger UI</summary>

**Problem**: Instantiate FastAPI with Swagger UI configured to retain authorization on reload.

**Solution**:
```python
from fastapi import FastAPI

app = FastAPI(swagger_ui_parameters={"persistAuthorization": True})
```
</details>

<details>
<summary>Exercise 2 (Easy): Disable ReDoc while keeping Swagger UI</summary>

**Problem**: Disable the `/redoc` route on FastAPI startup.

**Solution**:
```python
from fastapi import FastAPI

app = FastAPI(redoc_url=None)
```
</details>

<details>
<summary>Exercise 3 (Medium): Add Scalar documentation route</summary>

**Problem**: Add a custom `/scalar` route returning the Scalar HTML shell.

**Solution**:
```python
from fastapi import FastAPI
from fastapi.responses import HTMLResponse

app = FastAPI()

@app.get("/scalar", response_class=HTMLResponse, include_in_schema=False)
def scalar_docs():
    return """
    <!doctype html><html><head><title>Scalar</title></head><body>
    <script id="api-reference" data-url="/openapi.json"></script>
    <script src="https://cdn.jsdelivr.net/npm/@scalar/api-reference"></script>
    </body></html>
    """
```
</details>

<details>
<summary>Exercise 4 (Medium): Collapse Swagger UI tags by default</summary>

**Problem**: Configure Swagger UI to collapse all route groups on initial load.

**Solution**:
```python
from fastapi import FastAPI

app = FastAPI(swagger_ui_parameters={"docExpansion": "none"})
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Air-Gapped Swagger Route with Static Files</summary>

**Problem**: Construct a self-hosted documentation router that serves Swagger UI from a local `/vendor` directory without external CDN access.

**Solution**:
```python
from fastapi import FastAPI
from fastapi.openapi.docs import get_swagger_ui_html
from fastapi.staticfiles import StaticFiles

app = FastAPI(docs_url=None)
app.mount("/vendor", StaticFiles(directory="static"), name="vendor")

@app.get("/docs", include_in_schema=False)
def local_swagger():
    return get_swagger_ui_html(
        openapi_url="/openapi.json",
        title="Local Air-Gapped Docs",
        swagger_js_url="/vendor/swagger-ui-bundle.js",
        swagger_css_url="/vendor/swagger-ui.css",
    )
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Basic-Auth Protected Swagger and OpenAPI Router</summary>

**Problem**: Build a setup where both `/docs` and `/openapi.json` require HTTP Basic Authentication, ensuring unauthorized users cannot read the API specification.

**Solution**:
```python
import secrets
from typing import Annotated
from fastapi import Depends, FastAPI, HTTPException, Response, status
from fastapi.openapi.docs import get_swagger_ui_html
from fastapi.openapi.utils import get_openapi
from fastapi.security import HTTPBasic, HTTPBasicCredentials

app = FastAPI(docs_url=None, openapi_url=None)
basic_auth = HTTPBasic()

def require_admin(creds: Annotated[HTTPBasicCredentials, Depends(basic_auth)]):
    if not (secrets.compare_digest(creds.username, "admin") and secrets.compare_digest(creds.password, "pass")):
        raise HTTPException(status_code=401, headers={"WWW-Authenticate": "Basic"})
    return creds.username

@app.get("/openapi.json", include_in_schema=False)
def get_protected_openapi(_: Annotated[str, Depends(require_admin)]):
    return get_openapi(title="Protected API", version="1.0.0", routes=app.routes)

@app.get("/docs", include_in_schema=False)
def get_protected_docs(_: Annotated[str, Depends(require_admin)]):
    return get_swagger_ui_html(openapi_url="/openapi.json", title="Secure Docs")
```
</details>
