---
title: "Design Patterns in Modern FastAPI Applications"
description: "Implement Gang of Four (GoF) and enterprise software design patterns pythonically in FastAPI: Strategy, Factory, Adapter, Decorator, Facade, Unit of Work, and Chain of Responsibility."
fastapi_version: "0.115.x+"
python_version: "3.12+"
pydantic_version: "v2.10+"
tested_with: "pydantic 2.10+, fastapi 0.115+, pytest 8.3+"
free_threading_status: "Fully Compatible"
---

# Design Patterns in Modern FastAPI Applications

## 1. Conceptual Foundation & Mental Model

### Pythonic Design Patterns vs. Classic Object-Oriented Boilerplate
Classic design patterns—formalized by the Gang of Four (GoF)—originated in statically typed languages like C++ and Java. In those languages, patterns often required elaborate class hierarchies, verbose interfaces, and boilerplate factories. 

In modern Python 3.12+ and FastAPI, first-class functions, decorators, async context managers, and structural typing (`typing.Protocol`) allow us to implement these timeless architectural patterns **pythonically, cleanly, and with zero unnecessary inheritance**.

```
+-------------------------------------------------------------------------------+
|                       GOF PATTERNS IN FASTAPI ECOSYSTEMS                      |
|                                                                               |
|  1. Strategy Pattern        -> Swappable authentication, pricing, or payment  |
|  2. Factory Pattern         -> Dynamic adapter/service resolution via DI      |
|  3. Adapter Pattern         -> Encapsulating third-party APIs (Stripe, Twilio)|
|  4. Decorator Pattern       -> Endpoint auditing, metric tagging, memoization |
|  5. Facade Pattern          -> Simplifying multi-subsystem orchestration      |
|  6. Unit of Work            -> Managing atomic async database transactions    |
|  7. Chain of Responsibility -> Permission verification and ASGI middleware    |
+-------------------------------------------------------------------------------+
```

---

## 2. Architecture & Internal Mechanics

Understanding how core patterns integrate with FastAPI's request lifecycle and dependency injection container.

```
[Inbound Request]
       |
       v
[Chain of Responsibility] (FastAPI Dependency / Middleware Pipeline)
       | - Check API Key -> Check Rate Limit -> Validate Role
       v
[Factory Pattern] (Resolves Strategy based on Headers / Tenant)
       |
       +---> [Strategy A: StripePaymentStrategy]
       |
       +---> [Strategy B: PayPalPaymentStrategy]
       v
[Facade Pattern] (Coordinates Subsystems: Billing, Ledger, Notification)
       |
       v
[Unit of Work Pattern] (Atomically executes database mutations)
       |
       v
[Decorator Pattern] (Measures latency, logs structured metrics, caches)
       |
       v
[HTTP Response Outbound]
```

---

## 3. Comprehensive Syntax & Core Mechanics

### Pattern Mapping to Python Features
1. **Strategy**: Implemented via `typing.Protocol` or first-class callable functions. Selected at runtime using FastAPI dependencies (`Depends()`).
2. **Factory**: Implemented as Python module-level provider functions or class methods that inspect configuration settings or request context.
3. **Adapter**: Wraps non-standard external interfaces (e.g., synchronous libraries or varying third-party REST payloads) to match an application port.
4. **Decorator**: Implemented using `@functools.wraps` or Python callable classes wrapping endpoint handlers or service methods.
5. **Facade**: A unified high-level class orchestrating multiple lower-level repositories, microservices, and notification clients.
6. **Unit of Work**: Implemented using `@asynccontextmanager` wrapping SQLAlchemy `AsyncSession` transactions.
7. **Chain of Responsibility**: Implemented via chained FastAPI `Depends()` sub-dependencies or Starlette `BaseHTTPMiddleware`.

---

## 4. Practical Implementation: 5 Verified Examples

### Example 1: Strategy Pattern (Swappable Notification Engines)
Allowing customers to choose SMS, Email, or Slack notifications dynamically via strategy injection.

```python
# app/patterns/strategy.py
from typing import Protocol

class NotificationStrategy(Protocol):
    async def send(self, recipient: str, message: str) -> bool: ...

class EmailNotificationStrategy:
    async def send(self, recipient: str, message: str) -> bool:
        print(f"[EMAIL] Sent '{message}' to {recipient}")
        return True

class SmsNotificationStrategy:
    async def send(self, recipient: str, message: str) -> bool:
        print(f"[SMS] Sent '{message}' to {recipient}")
        return True

class SlackNotificationStrategy:
    async def send(self, recipient: str, message: str) -> bool:
        print(f"[SLACK] Posted '{message}' to channel {recipient}")
        return True
```

---

### Example 2: Factory Pattern with FastAPI Dependency Injection
Dynamically resolving the appropriate `NotificationStrategy` based on user preference or request headers.

```python
# app/patterns/factory.py
from fastapi import Header, HTTPException, status
from app.patterns.strategy import (
    NotificationStrategy,
    EmailNotificationStrategy,
    SmsNotificationStrategy,
    SlackNotificationStrategy,
)

class NotificationFactory:
    _strategies: dict[str, type[NotificationStrategy]] = {
        "email": EmailNotificationStrategy,
        "sms": SmsNotificationStrategy,
        "slack": SlackNotificationStrategy,
    }

    @classmethod
    def get_strategy(cls, channel: str) -> NotificationStrategy:
        strategy_cls = cls._strategies.get(channel.lower())
        if not strategy_cls:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail=f"Unsupported notification channel: {channel}. Supported: {list(cls._strategies.keys())}"
            )
        return strategy_cls()

def resolve_notification_strategy(
    x_channel: str = Header(default="email")
) -> NotificationStrategy:
    """FastAPI dependency injecting the dynamically selected strategy."""
    return NotificationFactory.get_strategy(x_channel)
```

---

### Example 3: Adapter Pattern (Standardizing Third-Party Geolocation APIs)
Adapting two external, incompatible geolocation services (MaxMind and IPInfo) into a uniform internal interface.

```python
# app/patterns/adapter.py
from typing import Protocol
from dataclasses import dataclass

@dataclass(frozen=True)
class GeoLocation:
    country_code: str
    city: str
    latitude: float
    longitude: float

class GeoLocationPort(Protocol):
    async def locate_ip(self, ip_address: str) -> GeoLocation: ...

# External Service A (Simulated third-party SDK)
class LegacyMaxMindClient:
    def lookup(self, ip: str) -> dict:
        return {"country": "US", "locality": "New York", "lat": 40.7128, "lon": -74.0060}

# External Service B (Simulated third-party SDK)
class IPInfoClient:
    async def fetch_details(self, ip_addr: str) -> dict:
        return {"country_iso": "US", "city_name": "New York", "coordinates": [40.7128, -74.0060]}

# Adapter A: Bridges LegacyMaxMindClient to GeoLocationPort
class MaxMindAdapter:
    def __init__(self, client: LegacyMaxMindClient):
        self._client = client

    async def locate_ip(self, ip_address: str) -> GeoLocation:
        # Translates synchronous SDK to async, and maps dictionary keys
        data = self._client.lookup(ip_address)
        return GeoLocation(
            country_code=data["country"],
            city=data["locality"],
            latitude=data["lat"],
            longitude=data["lon"],
        )

# Adapter B: Bridges IPInfoClient to GeoLocationPort
class IPInfoAdapter:
    def __init__(self, client: IPInfoClient):
        self._client = client

    async def locate_ip(self, ip_address: str) -> GeoLocation:
        data = await self._client.fetch_details(ip_address)
        return GeoLocation(
            country_code=data["country_iso"],
            city=data["city_name"],
            latitude=data["coordinates"][0],
            longitude=data["coordinates"][1],
        )
```

---

### Example 4: Decorator Pattern (Execution Timing & Resilience Retry)
Implementing an asynchronous retry decorator with exponential backoff for external API calls.

```python
# app/patterns/decorator.py
import asyncio
import functools
import logging

logger = logging.getLogger(__name__)

def async_retry(max_retries: int = 3, base_delay: float = 0.5):
    """Decorator retrying async calls on exception with exponential backoff."""
    def decorator(func):
        @functools.wraps(func)
        async def wrapper(*args, **kwargs):
            delay = base_delay
            last_exception = None
            for attempt in range(1, max_retries + 1):
                try:
                    return await func(*args, **kwargs)
                except Exception as exc:
                    last_exception = exc
                    logger.warning(
                        "Attempt %d/%d for %s failed: %s. Retrying in %.2fs...",
                        attempt, max_retries, func.__name__, exc, delay
                    )
                    if attempt == max_retries:
                        break
                    await asyncio.sleep(delay)
                    delay *= 2
            raise last_exception
        return wrapper
    return decorator
```

---

### Example 5: Chain of Responsibility (Multi-Tier Permission Guardrail)
Using FastAPI sub-dependencies to create an extensible authorization validation chain.

```python
# app/patterns/chain.py
from fastapi import APIRouter, Depends, HTTPException, Header, status

router = APIRouter(prefix="/enterprise", tags=["Enterprise"])

# Chain Link 1: IP Whitelist Check
async def verify_corporate_network(x_client_ip: str = Header(default="127.0.0.1")):
    if not (x_client_ip.startswith("10.") or x_client_ip == "127.0.0.1"):
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="Corporate VPN required")
    return x_client_ip

# Chain Link 2: Token Verification (Depends on Link 1)
async def verify_auth_token(
    ip: str = Depends(verify_corporate_network),
    authorization: str = Header(...)
):
    if not authorization.startswith("Bearer enterprise_"):
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Invalid enterprise token")
    return {"token": authorization, "client_ip": ip}

# Chain Link 3: Role Validation (Depends on Link 2)
def require_minimum_tier(required_tier: str):
    async def tier_checker(auth: dict = Depends(verify_auth_token)):
        # Simulates checking token claims
        token = auth["token"]
        if required_tier == "executive" and not token.endswith("_admin"):
            raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="Executive tier required")
        return {**auth, "tier": required_tier}
    return tier_checker

@router.get("/financials", dependencies=[Depends(require_minimum_tier("executive"))])
async def get_financial_reports():
    """Protected endpoint executing only after successfully passing all 3 chain links."""
    return {"status": "authorized", "report": "Q3 EBITDA Growth: +14.2%"}
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Decorator Signature Loss**: Forgetting `@functools.wraps(func)` when writing custom decorators strips function metadata, argument names, and docstrings. In FastAPI, this destroys OpenAPI schema generation and dependency parameter reflection.
2. **Stateful Singletons in Factories**: If a factory caches and returns mutable stateful instances across concurrent requests, requests from different tenants will overwrite each other's credentials and state. Ensure factory instances are stateless or scoped per-request.
3. **Chain of Responsibility Execution Order**: In FastAPI, dependencies without explicit parameters run in parallel where possible. To enforce a strict sequential chain of responsibility, each downstream dependency must explicitly take the upstream dependency as an argument (`Depends(upstream_func)`).
4. **Adapter Exception Leaks**: If an adapter lets vendor-specific exceptions (`stripe.error.CardError`, `botocore.exceptions.ClientError`) bubble up into the service layer, the service layer becomes coupled to the vendor library. Adapters must catch vendor exceptions and re-raise standard domain exceptions.

---

## 6. Common Mistakes Table

| Mistake | Root Cause | Impact | Correct Approach |
| :--- | :--- | :--- | :--- |
| **Omitting `@functools.wraps`** | Writing naive decorators | Destroys FastAPI dependency injection and Swagger docs. | Always decorate wrapper functions with `@functools.wraps(func)`. |
| **Hardcoding Vendor Exceptions** | Services catching `stripe.error.APIError` | Swapping payment providers requires rewriting service exception blocks. | Adapters must map vendor exceptions to application Domain exceptions. |
| **Over-Engineering with Classic Gang of Four** | Writing AbstractFactoryBuilderSingletons | Creates massive Java-style boilerplate in clean Python codebases. | Implement patterns pythonically using Protocols, functions, and decorators. |
| **Uncached Factory Lookups** | Reinstantiating heavy HTTP connection pools | High latency and socket exhaustion on every request. | Cache heavy adapter instances using `@lru_cache` or lifespan singletons. |
| **Unbounded Retries in Decorators** | Decorator retrying indefinitely | Starves worker threads and locks up event loops on outages. | Cap retries (`max_retries=3`) with exponential backoff and jitter. |
| **Mutating Global State in Strategies** | Reusing strategy instances with request state | Concurrency race conditions across multi-worker servers. | Strategies must remain completely stateless; pass state as method arguments. |

---

## 7. Performance & Scalability Considerations

- **Decorator Overhead**: Micro-benchmarks show that calling an async function wrapped with a clean `@functools.wraps` decorator adds less than 0.2 microseconds per call—completely negligible compared to network and database I/O.
- **Factory Caching**: Use `functools.lru_cache` for factories that instantiate clients with heavy setup costs (such as compiling regexes or establishing SSL contexts).
- **Free-Threading in Python 3.13t**: Stateless patterns (Strategy, Adapter, Decorator) are inherently thread-safe, running across multi-threaded workers without lock contention.

---

## 8. Security & OWASP Guardrails

- **OWASP A01 (Broken Access Control - Chain of Responsibility)**: When chaining security dependencies, ensure that failure in any single link short-circuits immediately with HTTP 401 or 403. Never allow an endpoint to execute if an upstream check raised an exception.
- **Secrets Isolation in Adapters**: Keep API keys, client secrets, and endpoint URLs encapsulated strictly inside concrete adapter classes. The strategy protocol and consumer services must never handle raw credentials.

---

## 9. Testing Strategies & Pytest Recipes

```python
# tests/test_patterns.py
import pytest
from app.patterns.factory import NotificationFactory
from app.patterns.strategy import EmailNotificationStrategy
from app.patterns.decorator import async_retry

def test_notification_factory_resolves_strategy():
    strategy = NotificationFactory.get_strategy("email")
    assert isinstance(strategy, EmailNotificationStrategy)

@pytest.mark.asyncio
async def test_async_retry_decorator_succeeds_on_transient_failure():
    attempts = 0

    @async_retry(max_retries=3, base_delay=0.01)
    async def flaky_network_call():
        nonlocal attempts
        attempts += 1
        if attempts < 3:
            raise ConnectionResetError("Transient network blip")
        return "success"

    result = await flaky_network_call()
    assert result == "success"
    assert attempts == 3
```

---

## 10. Real-World Case Study / Production Incident

An international e-commerce marketplace operated in 12 countries. In the US, taxes were computed using a third-party SaaS (TaxJar); in Europe, VAT rules were calculated in-house; and in Brazil, complex state-by-state ICMS calculations were handled by a local provider.

**The Anti-Pattern**:
The checkout endpoint contained a 600-line `if/elif/else` statement inspecting the customer's billing country:
```python
if country == "US":
    # 80 lines of TaxJar API logic
elif country in EU_COUNTRIES:
    # 150 lines of VAT logic
elif country == "BR":
    # 200 lines of ICMS logic
```
Whenever a developer modified Brazilian tax logic, a syntax error or unhandled edge case would crash US checkouts, costing the business \$40,000 in lost sales during an afternoon incident.

**The Solution**:
The team refactored the checkout engine using the **Strategy and Factory patterns**:
1. Defined a strict `TaxCalculationStrategy(Protocol)`.
2. Built three isolated adapters: `TaxJarStrategy`, `EuVatStrategy`, and `BrazilIcmsStrategy`.
3. Created a `TaxStrategyFactory` that selected the strategy based on the customer's shipping address.
4. Each strategy was covered by independent unit tests. A failure in the Brazilian tax calculation was completely isolated, allowing US and EU checkouts to proceed without disruption.

---

## 11. Technical Interview Questions

<details>
<summary>1. How does the Strategy pattern differ from the State pattern?</summary>
While both patterns share similar class diagrams (an interface with interchangeable concrete implementations), their intent is fundamentally different. The Strategy pattern is used to configure an algorithm or behavior from the outside (e.g., choosing between Stripe and PayPal, or selecting a sorting algorithm). The State pattern allows an object to alter its behavior internally when its internal state changes (e.g., an Order transitioning from Draft $\to$ Paid $\to$ Shipped).
</details>

<details>
<summary>2. Why is the Adapter pattern preferred over directly importing third-party SDKs in business services?</summary>
Third-party SDKs frequently change their API signatures, data structures, and exception hierarchies between major releases. If an SDK is imported across 50 service files, updating the SDK requires modifying 50 files. An Adapter wraps the SDK behind an application-owned interface (Port). When the SDK changes, only the single Adapter class needs updating, while the entire business domain remains untouched.
</details>

<details>
<summary>3. Why must custom Python decorators use `@functools.wraps` when applied to FastAPI route handlers?</summary>
FastAPI inspects route handlers using Python's `inspect` module to generate OpenAPI (Swagger) documentation and discover parameters for Dependency Injection (`Depends()`). Without `@functools.wraps`, the decorated function takes on the signature and name of the internal wrapper function (`*args, **kwargs`), causing FastAPI to lose all parameter types and query schema reflections.
</details>

<details>
<summary>4. How does FastAPI implement the Chain of Responsibility pattern?</summary>
FastAPI implements the Chain of Responsibility through its hierarchical Dependency Injection system and ASGI middleware. Route handlers can declare a list of dependencies (`dependencies=[Depends(check_ip), Depends(check_auth), Depends(check_role)]`). Each dependency acts as a handler in the chain, either passing control to the next dependency or halting the chain by raising an `HTTPException`.
</details>

<details>
<summary>5. What is the Facade pattern, and where should it be applied in a FastAPI service?</summary>
The Facade pattern provides a simplified, unified interface to a complex set of interfaces or subsystems. In FastAPI, a Service class often acts as a Facade: when placing an order, the `OrderService` coordinates inventory checks, payment processing, database persistence, and email notifications, exposing a clean single method (`place_order()`) to the router.
</details>

<details>
<summary>6. How can the Factory pattern be implemented pythonically without creating an abstract factory class?</summary>
In Python, functions are first-class objects, and dictionary lookups can replace verbose switch/case or class hierarchies. A simple dictionary mapping string keys to classes or constructor functions (`{"stripe": StripeAdapter, "paypal": PayPalAdapter}`) combined with a lookup function provides a clean, pythonic factory.
</details>

<details>
<summary>7. What is the difference between a Decorator and an Adapter?</summary>
An Adapter converts an incompatible interface into an interface that the client expects without adding new behavior. A Decorator implements the same interface as the wrapped object and dynamically adds new behavior or responsibilities (such as logging, caching, or retrying) to it.
</details>

<details>
<summary>8. How do you implement the Unit of Work pattern using Python context managers?</summary>
By writing an asynchronous class implementing `async def __aenter__(self)` and `async def __aexit__(self, exc_type, exc_val, exc_tb)`. On entry, it initiates a database transaction. If an exception occurs during the execution block (`exc_type is not None`), it automatically issues a rollback; otherwise, it allows explicit commits.
</details>

---

## 12. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): In-Memory Caching Decorator</summary>

**Task**: Implement a simple async caching decorator `@async_cache` using `@functools.wraps` that caches the result of a function call based on its string argument.

**Solution**:
```python
import functools

def async_cache(func):
    cache = {}

    @functools.wraps(func)
    async def wrapper(arg: str):
        if arg in cache:
            return cache[arg]
        result = await func(arg)
        cache[arg] = result
        return result

    return wrapper
```
</details>

<details>
<summary>Exercise 2 (Easy): Payment Strategy Protocol</summary>

**Task**: Define a `PaymentStrategy` Protocol with `async def pay(self, amount: float) -> str: ...`. Implement a `CreditCardStrategy` returning `"Paid {amount} with Credit Card"`.

**Solution**:
```python
from typing import Protocol

class PaymentStrategy(Protocol):
    async def pay(self, amount: float) -> str: ...

class CreditCardStrategy:
    async def pay(self, amount: float) -> str:
        return f"Paid {amount} with Credit Card"
```
</details>

<details>
<summary>Exercise 3 (Medium): Dynamic Strategy Factory with Error Handling</summary>

**Task**: Create a `PaymentFactory` with a method `create(method: str) -> PaymentStrategy`. Register `"credit_card"` and `"crypto"`. If an unknown method is requested, raise `ValueError("Invalid payment method")`.

**Solution**:
```python
class CryptoStrategy:
    async def pay(self, amount: float) -> str:
        return f"Paid {amount} with Bitcoin"

class PaymentFactory:
    _registry = {
        "credit_card": CreditCardStrategy,
        "crypto": CryptoStrategy,
    }

    @classmethod
    def create(cls, method: str) -> PaymentStrategy:
        strategy_cls = cls._registry.get(method.lower())
        if not strategy_cls:
            raise ValueError(f"Invalid payment method: {method}")
        return strategy_cls()
```
</details>

<details>
<summary>Exercise 4 (Medium): Weather API Adapter</summary>

**Task**: An external legacy SDK `OldWeatherLib` has a synchronous method `get_temp_f(zip_code: int) -> float`. Adapt it to an async port `WeatherPort(Protocol)` with `async def get_celsius(self, zip_code: int) -> float`.

**Solution**:
```python
from typing import Protocol

class WeatherPort(Protocol):
    async def get_celsius(self, zip_code: int) -> float: ...

class OldWeatherLib:
    def get_temp_f(self, zip_code: int) -> float:
        return 77.0  # 77 Fahrenheit = 25 Celsius

class WeatherAdapter:
    def __init__(self, legacy_lib: OldWeatherLib):
        self._legacy = legacy_lib

    async def get_celsius(self, zip_code: int) -> float:
        temp_f = self._legacy.get_temp_f(zip_code)
        temp_c = (temp_f - 32.0) * (5.0 / 9.0)
        return round(temp_c, 2)
```
</details>

<details>
<summary>Exercise 5 (Hard): Facade Pattern for Order Checkout</summary>

**Task**: Implement an `OrderCheckoutFacade` that coordinates three independent subsystems (`InventorySystem`, `PaymentSystem`, `ShippingSystem`) in a single method `async def checkout(user_id: str, item_id: str, price: float) -> str`. If inventory or payment fails, abort before shipping.

**Solution**:
```python
class InventorySystem:
    async def reserve(self, item_id: str) -> bool:
        return True

class PaymentSystem:
    async def charge(self, user_id: str, amount: float) -> bool:
        return True

class ShippingSystem:
    async def create_label(self, user_id: str, item_id: str) -> str:
        return f"TRACKING-{user_id}-{item_id}"

class OrderCheckoutFacade:
    def __init__(
        self,
        inventory: InventorySystem,
        payment: PaymentSystem,
        shipping: ShippingSystem,
    ):
        self._inventory = inventory
        self._payment = payment
        self._shipping = shipping

    async def checkout(self, user_id: str, item_id: str, price: float) -> str:
        if not await self._inventory.reserve(item_id):
            raise RuntimeError("Inventory unavailable")
            
        if not await self._payment.charge(user_id, price):
            raise RuntimeError("Payment declined")
            
        tracking_number = await self._shipping.create_label(user_id, item_id)
        return tracking_number
```
</details>

<details>
<summary>Exercise 6 (Hard): Execution Latency Tracking Decorator with FastAPI Integration</summary>

**Task**: Implement a decorator `@track_latency(metric_name: str)` that records execution time in milliseconds and attaches the timing to `request.state.timings`.

**Solution**:
```python
import time
import functools
from fastapi import Request

def track_latency(metric_name: str):
    def decorator(func):
        @functools.wraps(func)
        async def wrapper(*args, **kwargs):
            # Locate Request object from args or kwargs
            request: Request | None = kwargs.get("request")
            if not request:
                for arg in args:
                    if isinstance(arg, Request):
                        request = arg
                        break

            start = time.perf_counter()
            try:
                return await func(*args, **kwargs)
            finally:
                elapsed_ms = (time.perf_counter() - start) * 1000.0
                if request:
                    if not hasattr(request.state, "timings"):
                        request.state.timings = {}
                    request.state.timings[metric_name] = round(elapsed_ms, 2)
        return wrapper
    return decorator
```
</details>
