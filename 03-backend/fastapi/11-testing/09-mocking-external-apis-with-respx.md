---
title: "Mocking External APIs with RESPX in FastAPI Testing"
category: "11-testing"
order: 9
tags: ["fastapi", "testing", "respx", "httpx", "mocking", "external-apis", "pytest"]
py_version: "3.12+"
fastapi_version: "0.115+"
starlette_version: "0.38+"
pydantic_version: "2.8+"
tested_with: "Python 3.12, 3.13"
---

# Mocking External APIs with RESPX in FastAPI Testing

## 1. Conceptual Foundation & Mental Model

Modern FastAPI applications constantly communicate with third-party HTTP APIs: charging customer cards via Stripe, verifying identities via Auth0, sending transactional emails via SendGrid, or fetching exchange rates.

When writing automated integration tests for these services, making real outbound HTTP calls to third-party endpoints is an absolute anti-pattern:
- Real network calls introduce non-deterministic latency (500ms–2,000ms per test).
- Test runs fail if the third-party sandbox is down or if CI environments lack internet access.
- Tests can accidentally exhaust sandbox API rate limits or charge real money.

In the synchronous Python world, `responses` was the standard library for mocking `requests`. In modern asynchronous FastAPI development built on `httpx`, the gold standard is **`RESPX`**.

**RESPX** is an ultra-fast, mock library designed specifically for `httpx`. It intercepts outbound HTTP requests at the transport level, matching requests by URL, method, headers, or JSON body, and returning mocked responses without a single packet touching the operating system network stack.

### The Mental Model: The Truman Show Fake Telephone Line
- In *The Truman Show*, whenever Truman dials an international phone number on his telephone, the call does not connect to an overseas satellite or undersea fiber cable.
- The phone wire runs directly down into a hidden control room in the basement. An actor picks up the receiver, pretends to be the operator in Fiji, and speaks the scripted response (**RESPX Mock Transport**).
- Truman's telephone (**`httpx.AsyncClient`**) believes it made a real phone call to Fiji; it receives dial tones, status codes, and voice audio, but the entire exchange happened inside the building with zero cost and zero delays.

---

## 2. Architecture & Transport Interception

```
   FastAPI Endpoint (e.g. POST /checkout)
              |
              v
   `httpx.AsyncClient().post("https://api.stripe.com/v1/charges")`
              |
              v
   +-------------------------------------------------------------+
   | RESPX Mock Transport Layer                                  |
   |                                                             |
   | Intercepts request BEFORE opening OS TCP socket!            |
   | Checks route matchers:                                      |
   | `respx.post("https://api.stripe.com/v1/charges")`           |
   |                                                             |
   | Matches?                                                    |
   |    YES -> Returns Mock `httpx.Response(201, json={...})`   |
   |    NO  -> Raises `respx.errors.PassThroughNotAllowed`!      |
   +-------------------------------------------------------------+
              |
              v
   FastAPI receives response instantly in <0.1ms!
```

### The Zero-Leak Guarantee
RESPX defaults to strict mode (`assert_all_called=False` and pass-through disabled). If your test makes an unexpected outbound HTTP call to an unmocked URL, RESPX blocks the call and raises `PassThroughNotAllowed`. This guarantees that your test suite can never accidentally make live external calls in CI!

---

## 3. Comprehensive Syntax & Core Mechanics

### 1. Basic RESPX Decorator Pattern
```python
import respx
import httpx
from httpx import Response

@respx.mock
async def test_mocked_http_request():
    # Define mock route
    route = respx.get("https://api.github.com/users/octocat").mock(
        return_value=Response(200, json={"login": "octocat", "id": 1})
    )
    
    # Make real httpx call
    async with httpx.AsyncClient() as client:
        resp = await client.get("https://api.github.com/users/octocat")
        
    assert resp.status_code == 200
    assert resp.json()["login"] == "octocat"
    assert route.called is True
    assert route.call_count == 1
```

### 2. Context Manager Pattern with Assertion Checks
```python
async def test_with_context_manager():
    with respx.mock(assert_all_called=True) as respx_mock:
        route = respx_mock.post("https://api.sendgrid.com/v3/mail/send").mock(
            return_value=Response(202)
        )
        # Outbound call happens here...
```

---

## 4. Practical Implementation

### Example 1: Mocking an External Weather Service in a FastAPI Route
```python
import pytest
import respx
from httpx import AsyncClient, ASGITransport, Response
from fastapi import FastAPI, HTTPException

app = FastAPI()

@app.get("/weather/{city}")
async def get_city_weather(city: str):
    async with httpx.AsyncClient() as client:
        try:
            resp = await client.get(f"https://api.weather.com/v1/{city}", timeout=3.0)
            resp.raise_for_status()
            data = resp.json()
            return {"city": city, "temp": data["temperature"], "condition": data["condition"]}
        except httpx.HTTPStatusError as exc:
            if exc.response.status_code == 404:
                raise HTTPException(status_code=404, detail="City not found in weather index")
            raise HTTPException(status_code=502, detail="Upstream weather API failed")

@pytest.mark.anyio
@respx.mock
async def test_weather_endpoint_success():
    # Mock external weather API
    respx.get("https://api.weather.com/v1/Paris").mock(
        return_value=Response(200, json={"temperature": 22.5, "condition": "Sunny"})
    )
    
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        resp = await client.get("/weather/Paris")
        assert resp.status_code == 200
        assert resp.json() == {"city": "Paris", "temp": 22.5, "condition": "Sunny"}

@pytest.mark.anyio
@respx.mock
async def test_weather_endpoint_upstream_404():
    # Mock 404 from external service
    respx.get("https://api.weather.com/v1/Atlantis").mock(
        return_value=Response(404, json={"error": "Not Found"})
    )
    
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        resp = await client.get("/weather/Atlantis")
        assert resp.status_code == 404
        assert resp.json()["detail"] == "City not found in weather index"
```

### Example 2: Simulating Network Timeouts and Connection Drops
Testing how your FastAPI error handling behaves when external services experience network drops:
```python
@pytest.mark.anyio
@respx.mock
async def test_weather_upstream_timeout_mapped_to_502():
    # Simulate a network timeout exception during socket read
    respx.get("https://api.weather.com/v1/Tokyo").mock(
        side_effect=httpx.ConnectTimeout("Connection to weather.com timed out")
    )
    
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        with pytest.raises(httpx.ConnectTimeout):
            # Endpoint will experience timeout
            await client.get("/weather/Tokyo")
```

### Example 3: Edge Case — Dynamic Response Callables (Pattern Matching on Body)
Generating different mock responses based on request payload parameters:
```python
@pytest.mark.anyio
@respx.mock
async def test_dynamic_payment_mock_routing():
    def dynamic_payment_callback(request: httpx.Request):
        import json
        payload = json.loads(request.content.decode())
        amount = payload.get("amount", 0)
        
        if amount > 10000:
            # Reject high amounts
            return Response(402, json={"error": "Card declined: Exceeds limit"})
        return Response(200, json={"status": "approved", "tx_id": "tx_9988"})

    # Bind dynamic callback function
    respx.post("https://api.stripe.com/v1/charges").mock(side_effect=dynamic_payment_callback)
    
    async with httpx.AsyncClient() as client:
        # 1. Under limit -> Approved
        res_ok = await client.post("https://api.stripe.com/v1/charges", json={"amount": 5000})
        assert res_ok.status_code == 200
        assert res_ok.json()["status"] == "approved"
        
        # 2. Over limit -> Declined
        res_fail = await client.post("https://api.stripe.com/v1/charges", json={"amount": 15000})
        assert res_fail.status_code == 402
        assert res_fail.json()["error"] == "Card declined: Exceeds limit"
```

### Example 4: Failure Case — Accidental Live Network Request
```python
# CATASTROPHIC ANTI-PATTERN
@pytest.mark.anyio
async def test_unmocked_live_call():
    # Missing @respx.mock or mocking configuration!
    # This executes a REAL outbound network call across the public internet!
    # In CI without internet, this crashes! If rate-limited, it crashes!
    async with httpx.AsyncClient() as client:
        resp = await client.get("https://api.github.com/rate_limit")
        assert resp.status_code == 200
```

### Example 5: Idiomatic Modern — Reusable Pytest Fixture for Shared Mocks
```python
import pytest
import respx

@pytest.fixture
def mock_auth0():
    with respx.mock(base_url="https://auth.company.com") as respx_mock:
        # Mock JWKS public key endpoint
        respx_mock.get("/.well-known/jwks.json").mock(
            return_value=Response(200, json={"keys": [{"kid": "k1", "kty": "RSA"}]})
        )
        # Mock token introspection
        respx_mock.post("/oauth/token").mock(
            return_value=Response(200, json={"access_token": "mocked-token-abc"})
        )
        yield respx_mock
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Base URL Trailing Slashes**: In RESPX, matching `base_url="https://api.example.com"` vs `"https://api.example.com/"` can lead to unmatched URL errors if paths do not align. Use `respx.route(url__startswith="https://api.example.com")` for robust prefix matching.
2. **Order of Mock Registration**: RESPX evaluates routes in the order they were defined. If you register a broad catch-all route first (`respx.get("https://api.example.com/")`), it will intercept requests before specific sub-routes (`https://api.example.com/special`) can evaluate! Always define specific routes before generic routes.
3. **Assert All Called Failures**: Setting `assert_all_called=True` causes pytest to fail if *any* registered mock route was not invoked during the test. If your test only covers an error condition that skips a secondary network call, the assertion will fail.
4. **Shared Client State across Tests**: If an `httpx.AsyncClient` is stored as a global module singleton, RESPX attaches its transport hook cleanly, but connection pool caches can retain stale headers. Always use fresh clients or close connections.

---

## 6. Common Mistakes & Anti-Patterns

| Anti-Pattern | Root Cause | Consequence | Correct Practice |
|:---|:---|:---|:---|
| Making live network calls in CI tests | Forgetting to mock with RESPX | Flaky tests, network timeouts, rate limiting | Intercept with `@respx.mock` |
| Using string regex with unescaped dots | Writing `https://api.example.com` in regex | Matches unexpected domains | Use exact string matching or `url__regex` |
| Registering generic routes before specific routes | Misunderstanding route evaluation order | Generic route absorbs all calls; specific ignored | Register specific routes first |
| Mocking JSON with invalid strings | Passing string to `json=` parameter | Deserialization failure in test | Pass Python `dict` or `list` to `json=` |
| Modifying global `requests` instead of `httpx` | Using `responses` library for async httpx code | Mocks fail to intercept httpx calls | Use `respx` for `httpx` clients |
| Forgetting side effects for network exceptions | Only testing 200 OK responses | Unhandled `ConnectTimeout` crashes production | Test `side_effect=httpx.ConnectTimeout` |

---

## 7. Performance & Scalability Considerations

- **Transport Overhead**: Intercepting requests with RESPX avoids DNS resolution, TCP handshakes, TLS negotiation, and socket I/O. Outbound HTTP calls that take 300ms in production complete in under $0.05\text{ms}$ in tests.
- **Memory Efficiency**: RESPX creates lightweight in-memory response buffers without disk or network caching.

---

## 8. Security & OWASP Guardrails

- **Zero Data Leakage**: By default, RESPX disables pass-through, preventing proprietary application data, authorization tokens, or internal test payloads from leaking to external third-party hosts during automated testing.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
import respx
import httpx
from httpx import Response

@pytest.mark.anyio
@respx.mock
async def test_respx_request_verification():
    # 1. Setup mock
    route = respx.post("https://api.acme.com/v1/notify").mock(return_value=Response(204))
    
    # 2. Execute call
    async with httpx.AsyncClient() as client:
        await client.post(
            "https://api.acme.com/v1/notify",
            json={"alert": "disk_full"},
            headers={"X-Service-ID": "backend-01"}
        )
        
    # 3. Assert on call metadata
    assert route.called is True
    last_request = route.calls.last.request
    assert last_request.headers["X-Service-ID"] == "backend-01"
    assert b"disk_full" in last_request.content
```

---

## 10. Real-World Case Study / Production Incident

**The Incident**: A high-volume logistics dispatch API ran an automated integration test suite containing 300 tests on every GitHub commit. One morning, the team woke up to a $14,000 AWS bill and account suspension from Twilio.

**Root Cause**: A newly added test for a package delivery notification endpoint had omitted the mock decorator:
```python
# test_notifications.py:
async def test_send_delivery_sms():
    # BUG: Forgot @respx.mock!
    # Outbound call hit REAL Twilio API in a loop across 50 pull requests!
    await notification_service.send_sms("+15551234567", "Package Delivered!")
```
Every pull request and CI retry had dispatched thousands of real international SMS text messages to unallocated test phone numbers!

**The Fix**:
1. Added a global pytest fixture in `conftest.py` setting `respx.mock(assert_all_called=False)` to run automatically for **all** tests.
2. Configured strict pass-through rejection: any attempt to make an outbound network call to a non-local URL raises `respx.errors.PassThroughNotAllowed`, making accidental external requests mathematically impossible.

---

## 11. Technical Interview Questions & Answers

<details>
<summary>1. What is RESPX and how does it differ from the legacy <code>responses</code> library?</summary>
<code>RESPX</code> is an asynchronous mock library designed specifically for <code>httpx</code>. The legacy <code>responses</code> library was built to intercept synchronous <code>requests</code> calls using socket monkey-patching. RESPX integrates directly into <code>httpx</code>'s ASGI and custom transport architecture, supporting both synchronous and asynchronous HTTP requests seamlessly.
</details>

<details>
<summary>2. How does RESPX prevent tests from accidentally making real network calls to production APIs?</summary>
By default, RESPX operates in strict mode with network pass-through disabled. If an application makes an HTTP request to a URL that has not been explicitly mocked, RESPX intercepts the call and raises a <code>respx.errors.PassThroughNotAllowed</code> exception, preventing any data from leaving the local process.
</details>

<details>
<summary>3. How do you simulate an upstream HTTP timeout in RESPX?</summary>
Configure the mock route with the <code>side_effect</code> parameter: <code>respx.get(url).mock(side_effect=httpx.TimeoutException("Read timed out"))</code>. When the client invokes the route, RESPX immediately raises the specified exception.
</details>

<details>
<summary>4. How can you inspect the request payload sent by your application to a mocked external endpoint?</summary>
Inspect the route's call history: <code>route.calls.last.request</code>. You can inspect headers (<code>request.headers</code>), query parameters (<code>request.url.params</code>), and the request body (<code>request.content</code> or <code>json.loads(request.content)</code>).
</details>

<details>
<summary>5. What does the <code>assert_all_called = True</code> parameter do in RESPX?</summary>
It enforces that every mock route declared in the test must have been invoked at least once before the test context exits. If a declared route was never called, RESPX raises an assertion error, flagging obsolete or unreached mock configurations.
</details>

<details>
<summary>6. How can you dynamically return different responses based on the request body in RESPX?</summary>
Pass a callback function to <code>side_effect</code>: <code>respx.post(url).mock(side_effect=my_callback)</code>. The callback receives the incoming <code>httpx.Request</code> object and dynamically returns an <code>httpx.Response</code> based on headers or body contents.
</details>

<details>
<summary>7. How does route evaluation ordering work in RESPX?</summary>
RESPX evaluates routes in the order they were defined (first-registered, first-checked). If a broad pattern matches a request, it consumes the call. Therefore, specific URL patterns must always be declared before generic catch-all patterns.
</details>

<details>
<summary>8. Can RESPX mock HTTP/2 requests?</summary>
Yes. Because RESPX operates at the transport interface layer of <code>httpx</code>, it seamlessly mocks HTTP/1.1 and HTTP/2 requests identically without requiring a live TLS connection.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Basic GitHub User Mock
**Task**: Use `@respx.mock` to mock `GET https://api.github.com/users/octocat` returning HTTP 200 with `{"name": "The Octocat"}`. Make a call with `httpx.AsyncClient` and assert on the JSON response.
<details>
<summary>Solution</summary>

```python
import pytest
import respx
import httpx
from httpx import Response

@pytest.mark.anyio
@respx.mock
async def test_github_octocat_mock():
    respx.get("https://api.github.com/users/octocat").mock(
        return_value=Response(200, json={"name": "The Octocat"})
    )
    
    async with httpx.AsyncClient() as client:
        resp = await client.get("https://api.github.com/users/octocat")
        assert resp.status_code == 200
        assert resp.json()["name"] == "The Octocat"
```
</details>

### Exercise 2 (Easy): HTTP 500 Upstream Error Simulation
**Task**: Mock a third-party payment gateway endpoint returning `HTTP 500 Internal Server Error`. Assert that the client catches the error.
<details>
<summary>Solution</summary>

```python
import pytest
import respx
import httpx
from httpx import Response

@pytest.mark.anyio
@respx.mock
async def test_upstream_500_error():
    respx.post("https://api.payments.com/charge").mock(
        return_value=Response(500, json={"error": "Database down"})
    )
    
    async with httpx.AsyncClient() as client:
        resp = await client.post("https://api.payments.com/charge")
        assert resp.status_code == 500
        assert resp.json()["error"] == "Database down"
```
</details>

### Exercise 3 (Medium): Outbound Request Payload Inspection
**Task**: Write a test verifying that when an application calls an external webhook `POST https://hooks.slack.com/services/123`, it sends a JSON body containing `{"text": "Alert: Low Memory"}`.
<details>
<summary>Solution</summary>

```python
import pytest
import respx
import httpx
import json
from httpx import Response

@pytest.mark.anyio
@respx.mock
async def test_slack_webhook_payload():
    route = respx.post("https://hooks.slack.com/services/123").mock(return_value=Response(200, text="ok"))
    
    async with httpx.AsyncClient() as client:
        await client.post("https://hooks.slack.com/services/123", json={"text": "Alert: Low Memory"})
        
    assert route.called is True
    sent_payload = json.loads(route.calls.last.request.content)
    assert sent_payload["text"] == "Alert: Low Memory"
```
</details>

### Exercise 4 (Medium): Route Matching by Query Parameters
**Task**: Configure RESPX to return different responses for `GET https://api.rates.com/convert?from=USD&to=EUR` vs `?from=USD&to=GBP`.
<details>
<summary>Solution</summary>

```python
import pytest
import respx
import httpx
from httpx import Response

@pytest.mark.anyio
@respx.mock
async def test_query_parameter_matching():
    respx.get("https://api.rates.com/convert", params={"from": "USD", "to": "EUR"}).mock(
        return_value=Response(200, json={"rate": 0.92})
    )
    respx.get("https://api.rates.com/convert", params={"from": "USD", "to": "GBP"}).mock(
        return_value=Response(200, json={"rate": 0.79})
    )
    
    async with httpx.AsyncClient() as client:
        res_eur = await client.get("https://api.rates.com/convert?from=USD&to=EUR")
        res_gbp = await client.get("https://api.rates.com/convert?from=USD&to=GBP")
        
        assert res_eur.json()["rate"] == 0.92
        assert res_gbp.json()["rate"] == 0.79
```
</details>

### Exercise 5 (Hard): Network Connection Drop Simulation
**Task**: Use RESPX `side_effect` to simulate a broken network connection (`httpx.NetworkError`) when calling an SMS gateway, and test that a retry wrapper attempts 3 calls before failing.
<details>
<summary>Solution</summary>

```python
import pytest
import respx
import httpx

async def call_with_retry(url: str, max_retries: int = 3):
    async with httpx.AsyncClient() as client:
        for attempt in range(max_retries):
            try:
                return await client.get(url)
            except httpx.NetworkError:
                if attempt == max_retries - 1:
                    raise

@pytest.mark.anyio
@respx.mock
async def test_retry_on_network_drop():
    route = respx.get("https://sms.gateway.internal/send").mock(
        side_effect=httpx.NetworkError("Peer reset connection")
    )
    
    with pytest.raises(httpx.NetworkError):
        await call_with_retry("https://sms.gateway.internal/send", max_retries=3)
        
    # Verify exactly 3 attempts were made
    assert route.call_count == 3
```
</details>

### Exercise 6 (Hard): Multi-Host Shared Mock Fixture
**Task**: Build a reusable pytest fixture `mock_microservices` that sets up mock routes for both an external Billing Service and an Auth Service, verifying both were called during a checkout workflow.
<details>
<summary>Solution</summary>

```python
import pytest
import respx
from httpx import Response, AsyncClient

@pytest.fixture
def mock_microservices():
    with respx.mock(assert_all_called=True) as respx_mock:
        auth_route = respx_mock.post("https://auth.internal/verify").mock(
            return_value=Response(200, json={"valid": True})
        )
        billing_route = respx_mock.post("https://billing.internal/charge").mock(
            return_value=Response(200, json={"status": "paid"})
        )
        yield {"auth": auth_route, "billing": billing_route}

@pytest.mark.anyio
async def test_checkout_workflow(mock_microservices):
    async with AsyncClient() as client:
        await client.post("https://auth.internal/verify")
        await client.post("https://billing.internal/charge")
        
    assert mock_microservices["auth"].called is True
    assert mock_microservices["billing"].called is True
```
</details>
