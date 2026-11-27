# Gateway & Adapter for External Systems (Anti-Corruption Layer)

| Field | Value |
| :--- | :--- |
| **Category** | Enterprise Application Patterns (Integration Boundaries) |
| **Origin** | Martin Fowler (*PoEAA*, 2002) & Eric Evans (*DDD* ACL, 2003) |
| **Difficulty** | Beginner → Intermediate |
| **Prerequisite(s)** | [Adapter](file:///E:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/01-adapter.md), [Dependency Inversion Principle](file:///E:/Learning/engineering-notes/08-solid-design-patterns/02-solid-principles/05-dependency-inversion-principle.md) |
| **Status** | Complete |

> **TL;DR**: A **Gateway** encapsulates all HTTP/SDK communication with an external service (Stripe, Twilio, Legacy SOAP) behind a clean domain-language interface, translating external schemas and errors so third-party quirks never pollute your core domain.

---

## 1. The Problem: Why Do External Gateways Exist?

Calling Stripe, AWS S3, or a partner REST API directly from domain services creates three headaches:
1. **SDK/Schema Leakage**: External field names (`cust_acct_flg_v2`, cents vs dollars, Stripe `PaymentIntent` statuses) leak into every domain class.
2. **Painful Testing**: You can't run unit tests without mocking `HttpClient` / `axios` / `requests` and crafting raw HTTP JSON fixtures.
3. **Vendor Lock-in**: Switching payment providers or upgrading from SDK v2 to v3 requires touching 30 files.

---

## 2. What Is a Gateway / Anti-Corruption Layer?

- **Gateway (PoEAA)**: An object that encapsulates access to an external system or resource behind a simple, domain-oriented interface (`PaymentGateway.charge(orderId, money)`).
- **Anti-Corruption Layer (DDD)**: The translation boundary inside the Gateway that converts between your **clean domain model** and the **external system's messy model**, isolating timeouts, retries, auth headers, and vendor-specific error codes.

---

## 3. How It Works

```mermaid
flowchart LR
    Dom["OrderService (Domain)"] --> Port["interface PaymentGateway"]
    Port <|.. Stripe["StripePaymentGateway (ACL)<br/>Translates Money <-> Stripe cents,<br/>handles 429/500 & timeouts"]
    Port <|.. Fake["FakePaymentGateway (Tests)"]
    Stripe --> Ext([Stripe REST API])
```

---

## 4. What Goes Wrong Without It

```ts
// ❌ BROKEN: Domain service coupled directly to Stripe's raw SDK & error shapes
const pi = await stripe.paymentIntents.create({ amount: Math.round(total * 100), currency: 'usd' });
if (pi.status === 'requires_action') { ... }
```

---

## 5. Multi-Language Examples

### Java 25 LTS

```java
public class GatewayDemo {
    record Money(long cents, String currency) {}
    sealed interface ChargeResult permits ChargeOk, ChargeDeclined {}
    record ChargeOk(String txId) implements ChargeResult {}
    record ChargeDeclined(String reason) implements ChargeResult {}

    interface PaymentGateway {
        ChargeResult charge(String orderId, Money amount);
    }

    static class StripeGateway implements PaymentGateway {
        @Override
        public ChargeResult charge(String orderId, Money amount) {
            // Translates domain Money -> Stripe HTTP payload & maps Stripe errors to ChargeResult
            return new ChargeOk("stripe_ch_123");
        }
    }

    public static void main(String[] args) {
        PaymentGateway gw = new StripeGateway();
        System.out.println(gw.charge("ord-1", new Money(1999, "USD")));
    }
}
```

### TypeScript 5.8

```ts
interface PaymentGateway {
  charge(orderId: string, cents: number): Promise<{ ok: true; txId: string } | { ok: false; reason: string }>;
}

class StripeGateway implements PaymentGateway {
  async charge(orderId: string, cents: number) {
    // External HTTP call + schema translation isolated here
    return { ok: true as const, txId: `ch_${orderId}_${cents}` };
  }
}

new StripeGateway().charge('ord-1', 1999).then(console.log);
```

### Python 3.14

```python
from typing import Protocol

class PaymentGateway(Protocol):
    def charge(self, order_id: str, cents: int) -> str: ...

class StripeGateway:
    def charge(self, order_id: str, cents: int) -> str:
        # Isolates requests/httpx call, timeouts, and vendor mapping
        return f"ch_{order_id}_{cents}"

gw: PaymentGateway = StripeGateway()
print("Charged:", gw.charge("ord-1", 1999))
```

---

## 6. Real-World Examples

1. **Payment Gateways**: `PaymentGateway` interface backed by `StripeGateway`, `AdyenGateway`, or `SandboxFakeGateway`.
2. **Notification Gateways**: `EmailSender` interface backed by `SendGridAdapter` in prod and `InMemoryMailbox` in tests.
3. **Legacy System Migration (Strangler Fig)**: An ACL Gateway translates modern domain objects into legacy SOAP/XML calls while migrating module by module.

---

## 7. Related Patterns

- [Adapter](file:///E:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/01-adapter.md), [Ports & Adapters (Hexagonal)](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/02-clean-hexagonal-onion.md), [Circuit Breaker](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/01-circuit-breaker.md).

---

## 8. Advantages

- **Domain Purity**: External API breaking changes only touch one Gateway class.
- **Fast, Honest Tests**: Swap `StripeGateway` for `FakePaymentGateway` without HTTP mocking.
- **Centralized Resilience**: Timeouts, retries, circuit breakers, and idempotency keys live in one place.

---

## 9. Limitations & When NOT to Use

- **Over-Abstraction**: If your app is literally a thin Stripe webhook proxy, wrapping every single Stripe endpoint in a 1-to-1 duplicate interface adds boilerplate without protecting any domain model.

---

## 10. Anti-Patterns & Common Mistakes

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| Returning raw `Response` or vendor SDK types from the Gateway | Leaks vendor coupling to callers | Translate into domain types / `Result` inside the Gateway |
| Letting `HttpTimeoutException` or `StripeException` bubble into domain | Domain shouldn't catch vendor-specific exceptions | Map to domain errors (`PaymentDeclined`, `GatewayUnavailable`) |
| Calling external HTTP APIs without explicit timeouts | Default infinite timeouts hang worker threads forever | Always configure connect + read timeouts inside the Gateway |
| Mixing business logic into the Gateway | Gateway should translate, not decide pricing/discounts | Keep business rules in Domain/Service Layer |
| Skipping idempotency keys on payment/write gateways | Retries double-charge customers | Pass `idempotencyKey` (`orderId`) to external write calls |
| Mocking `fetch`/`axios` in unit tests instead of faking the Gateway interface | Tests break on URL/header refactors | Depend on the `PaymentGateway` interface and use an in-memory fake |

---

## 11. Comparison: Gateway vs Adapter vs Facade

| Pattern | Focus |
| :--- | :--- |
| **Gateway** | Boundary to an **external process/system** (HTTP, RPC, Queue) |
| **Adapter** | Translating an incompatible interface into an expected one |
| **Facade** | Simplifying a complex subsystem behind a coarse API |

---

## 12. Design Checklist

- [ ] Does the Gateway interface live in the domain/application layer using domain terms?
- [ ] Are vendor SDK types and HTTP errors translated inside the Gateway (ACL)?
- [ ] Are timeouts, retries, and idempotency keys configured at the Gateway?

---

## 13. Interview Questions

<details>
<summary>1. What is an Anti-Corruption Layer (ACL)?</summary>

A translation layer (often implemented via Gateways/Adapters) that prevents a legacy or third-party system's data model and quirks from leaking into and corrupting your clean domain model.

</details>

<details>
<summary>2. Why should a Gateway translate vendor exceptions into domain errors?</summary>

So callers don't have to `catch (StripeCardException | BraintreeException)` and remain decoupled from which vendor is active.

</details>

<details>
<summary>3. How does Gateway improve unit testing?</summary>

Domain services depend on a small `PaymentGateway` interface, allowing tests to pass a 5-line in-memory fake instead of mocking HTTP clients.

</details>

<details>
<summary>4. Where do timeouts, retries, and circuit breakers belong?</summary>

Inside or wrapping the Gateway implementation, never scattered across domain services.

</details>

<details>
<summary>5. How does Gateway relate to Hexagonal Architecture?</summary>

A Gateway interface is an **Outbound Port**, and the vendor implementation (`StripeGateway`) is an **Outbound Adapter**.

</details>

<details>
<summary>6. Why is passing an idempotency key critical in a `PaymentGateway`?</summary>

If a network timeout occurs after the provider charged the card, retrying with the same idempotency key prevents double-charging.

</details>

<details>
<summary>7. When is a Gateway unnecessary?</summary>

In thin pass-through scripts or BFFs whose entire purpose is proxying a single external API.

</details>

<details>
<summary>8. What is the difference between a Gateway and a Repository?</summary>

A Repository abstracts **persistence of your own domain aggregates** (collection semantics); a Gateway abstracts **communication with external systems** (RPC/HTTP/SDK).

</details>

---

## 14. Practice Exercises

<details>
<summary>Exercise 1: Multi-Provider SMS Gateway</summary>

Define `SmsGateway` and implement `TwilioSmsGateway` + `InMemorySmsGateway` for tests.

</details>

<details>
<summary>Exercise 2: Error Translation</summary>

Map HTTP `429` and `504` responses inside a Gateway into `RateLimited` and `TransientFailure` domain results.

</details>

<details>
<summary>Exercise 3: Legacy Field ACL</summary>

Translate `{ CUST_NM: "Alice", BAL_CENTS: 500 }` into a clean `CustomerBalance(name, balance)` domain record.

</details>

---

## 15. Cheat Sheet

- **Own the interface** in your domain; **isolate vendor SDKs** in the Gateway.
- **Translate** schemas & errors at the boundary (ACL).
- **Configure** timeouts, retries, and idempotency keys inside the Gateway.

---

## 16. In Your Language

| Track | Relevant Deep-Dive |
| :--- | :--- |
| **Java** | [Spring HTTP Clients](file:///E:/Learning/engineering-notes/03-backend/spring-boot/README.md) |
| **TypeScript** | [NestJS HTTP & Providers](file:///E:/Learning/engineering-notes/03-backend/nestjs/README.md) |
| **Python** | [Python Web & Networking](file:///E:/Learning/engineering-notes/01-languages/python/20-web-and-networking-basics/README.md) |

---

## 17. Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Lazy Loading](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/08-lazy-loading.md) | [Module 09](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/README.md) | [Domain Model vs Transaction Script](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/10-domain-model-vs-transaction-script.md) |
