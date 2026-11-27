# Anemic vs Rich Domain Model

| Field | Value |
| :--- | :--- |
| **Category** | Enterprise Application Patterns (Domain Modeling) |
| **Origin** | Martin Fowler (2003, *"AnemicDomainModel"*) |
| **Difficulty** | Intermediate |
| **Prerequisite(s)** | [Domain Model vs Transaction Script](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/10-domain-model-vs-transaction-script.md), [Tell, Don't Ask](file:///E:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/09-tell-dont-ask.md) |
| **Status** | Complete |

> **TL;DR**: An **Anemic Domain Model** turns entities into passive getter/setter data bags while dumping all rules into giant `Service` classes. A **Rich Domain Model** puts state transitions and invariants (`order.cancel()`, `account.withdraw(amount)`) directly on the domain entity so invalid states cannot be created.

---

## 1. The Problem: Why Is the Anemic Domain Model an Anti-Pattern?

In many enterprise codebases, every entity looks like this:
```java
class Order {
    private String status;
    private double total;
    public String getStatus() { return status; }
    public void setStatus(String s) { this.status = s; }
    public double getTotal() { return total; }
    public void setTotal(double t) { this.total = t; }
}
```
Because any caller can call `order.setStatus("SHIPPED")` without checking whether the order was paid, **every Service class has to remember all business rules**. Over time, rules get forgotten in one endpoint, creating corrupted database rows.

---

## 2. Definitions

- **Anemic Domain Model**: Domain classes contain *only* fields + getters/setters; all validation and state logic lives externally in `OrderService`, `BillingService`, etc.
- **Rich Domain Model**: Domain entities hide raw setters, expose **intention-revealing domain methods** (`pay()`, `cancel()`, `addItem()`), and enforce their own invariants—supported by Value Objects (`Money`, `Email`).

---

## 3. How It Works

```mermaid
flowchart LR
    subgraph Anemic["Anemic (Fragile)"]
        S1["Service A"] -->|setStatus('PAID')| E1["Order (public setters)"]
        S2["Service B"] -->|setTotal(-50)| E1
    end
    subgraph Rich["Rich (Protected)"]
        S3["Service"] -->|order.markPaid(txId)| E2["Order (private state + invariants)"]
    end
```

---

## 4. What Goes Wrong With Anemic Models

```ts
// ❌ ANEMIC: Any service can put Order in an illegal state
order.status = 'SHIPPED'; // Even though order.items is empty and order.paid === false!
```

---

## 5. Multi-Language Examples

### Java 25 LTS (Rich Domain Entity)

```java
public class RichDomainDemo {
    static class Order {
        enum Status { DRAFT, PAID, SHIPPED }
        private Status status = Status.DRAFT;
        private long totalCents;

        Order(long totalCents) {
            if (totalCents <= 0) throw new IllegalArgumentException("Total must be > 0");
            this.totalCents = totalCents;
        }

        public void pay() {
            if (status != Status.DRAFT) throw new IllegalStateException("Only draft orders can be paid");
            status = Status.PAID;
        }

        public void ship() {
            if (status != Status.PAID) throw new IllegalStateException("Cannot ship unpaid order");
            status = Status.SHIPPED;
        }

        public Status status() { return status; }
    }

    public static void main(String[] args) {
        var o = new Order(2500);
        o.pay();
        o.ship();
        System.out.println("Order status: " + o.status());
    }
}
```

### TypeScript 5.8

```ts
class Order {
  #status: 'DRAFT' | 'PAID' | 'SHIPPED' = 'DRAFT';
  readonly totalCents: number;

  constructor(totalCents: number) {
    if (totalCents <= 0) throw new Error('Total must be > 0');
    this.totalCents = totalCents;
  }
  pay(): void {
    if (this.#status !== 'DRAFT') throw new Error('Already paid/shipped');
    this.#status = 'PAID';
  }
  ship(): void {
    if (this.#status !== 'PAID') throw new Error('Must pay before shipping');
    this.#status = 'SHIPPED';
  }
  get status() { return this.#status; }
}

const o = new Order(2500);
o.pay();
o.ship();
console.log('Status:', o.status);
```

### Python 3.14

```python
class Order:
    def __init__(self, total_cents: int) -> None:
        if total_cents <= 0:
            raise ValueError("Total must be > 0")
        self._status = "DRAFT"
        self._total_cents = total_cents

    def pay(self) -> None:
        if self._status != "DRAFT":
            raise ValueError("Only draft orders can be paid")
        self._status = "PAID"

    def ship(self) -> None:
        if self._status != "PAID":
            raise ValueError("Cannot ship unpaid order")
        self._status = "SHIPPED"

    @property
    def status(self) -> str:
        return self._status

o = Order(2500)
o.pay()
o.ship()
print("Status:", o.status)
```

---

## 6. Real-World Examples

1. **DDD Aggregates**: `BankAccount.withdraw(money)` checks overdraft limits inside the entity rather than in `WithdrawalController`.
2. **Functional Rich Domain**: Immutable `record Order` + pure module functions `Order.pay(order)` enforcing invariants via `Result<Order, DomainError>`.

---

## 7. Related Patterns

- [Tell, Don't Ask](file:///E:/Learning/engineering-notes/08-solid-design-patterns/03-design-principles/09-tell-dont-ask.md), [Value Object, Entity & Aggregate](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/15-value-object-entity-aggregate.md).

---

## 8. Advantages of Rich Domain Models

- **Always-Valid Entities**: Impossible to construct or transition into an illegal state.
- **Single Source of Truth**: Rules live in one place, not scattered across 10 services.
- **Trivial Unit Testing**: Test `order.ship()` directly in 3 lines with zero mocks.

---

## 9. Limitations & When Anemic Is Fine

- **DTOs & Read Models**: DTOs and CQRS view projections *should* be passive data bags (anemic by design).
- **Simple CRUD**: If an entity literally has no invariants beyond DB column types, a Rich Domain Model adds unnecessary wrapping.

---

## 10. Anti-Patterns & Common Mistakes

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| Auto-generating public `setX()` for every field | Allows any caller to bypass invariants | Remove setters; expose domain methods (`rename()`, `cancel()`) |
| Putting `repo.save()` inside a Rich Domain entity | Couples domain entity to persistence | Return state / let Service Layer call `repo.save(order)` |
| Confusing DTOs with Anemic Domain Entities | DTOs *should* be passive; Domain Entities should enforce rules | Keep DTOs anemic, Entities rich |
| Creating "God Entities" with 40 methods | Violates SRP | Split into smaller Value Objects & focused Aggregates |

---

## 11. Comparison Table

| Aspect | Anemic Domain Model | Rich Domain Model |
| :--- | :--- | :--- |
| **Invariants** | Scattered in Services | Encapsulated in Entities |
| **Setters** | Public everywhere | Hidden / replaced by domain verbs |
| **Testability** | Requires mocking Services/Repos | Direct pure unit tests on Entities |

---

## 12. Design Checklist

- [ ] Are public setters removed from core domain entities?
- [ ] Do state transitions (`pay`, `cancel`, `ship`) enforce preconditions inside the entity?
- [ ] Are entities free of `Repository` / HTTP / SQL dependencies?

---

## 13. Interview Questions

<details>
<summary>1. Why does Martin Fowler call the Anemic Domain Model an anti-pattern?</summary>

Because it uses OOP classes purely as procedural structs (data bags), forfeiting encapsulation and forcing business invariants to be duplicated across Service classes.

</details>

<details>
<summary>2. How do you fix an Anemic Domain Model?</summary>

Remove public setters, move validation and state-transition logic into intention-revealing methods on the entity (`order.cancel()`), and extract Value Objects (`Money`, `Email`).

</details>

<details>
<summary>3. Should a Rich Domain Entity inject a `Repository`?</summary>

Usually **no**—keep Entities persistence-ignorant. Let the Service Layer load entities from Repositories, call domain methods, and save them.

</details>

<details>
<summary>4. Can Functional Programming have a Rich Domain Model?</summary>

Yes: immutable `record`/`sealed` types with smart constructors and pure state-transition functions (`pay(order) -> Result<Order, Err>`) achieve the exact same invariant protection without mutable classes.

</details>

<details>
<summary>5. When is an anemic data structure completely appropriate?</summary>

For DTOs, API payloads, events, and CQRS Read Models—because they are pure data contracts, not domain entities.

</details>

<details>
<summary>6. How does "Tell, Don't Ask" relate to Rich Domain Models?</summary>

Instead of asking `if (order.getStatus() == PAID) order.setStatus(SHIPPED)`, you tell the object `order.ship()`.

</details>

<details>
<summary>7. How do you handle ORM hydration (JPA/SQLAlchemy) without public setters?</summary>

Use package-private/protected no-arg constructors for JPA or Data Mapper translation between DB rows and domain constructors.

</details>

<details>
<summary>8. What is a "never-invalid" entity?</summary>

An entity whose constructor and methods validate every invariant so an invalid instance can never exist in memory.

</details>

---

## 14. Practice Exercises

<details>
<summary>Exercise 1: Cure an Anemic `BankAccount`</summary>

Remove `setBalance()` and `setFrozen()` from `BankAccount`, replacing them with `deposit()`, `withdraw()`, and `freeze()`.

</details>

<details>
<summary>Exercise 2: Functional Rich Domain</summary>

Rewrite the Java `Order` state machine using a `sealed interface Order permits DraftOrder, PaidOrder, ShippedOrder`.

</details>

<details>
<summary>Exercise 3: Spot Invariant Leaks</summary>

Audit a sample `UserService` and move email validation and password-change rules onto `User` / `Email`.

</details>

---

## 15. Cheat Sheet

- **Anemic**: Getters/setters + logic in Services.
- **Rich**: Private state + domain verbs (`pay()`, `ship()`) + Value Objects.
- **Keep DTOs anemic, Entities rich.**

---

## 16. In Your Language

| Track | Relevant Deep-Dive |
| :--- | :--- |
| **Java** | [OOP Encapsulation & Records](file:///E:/Learning/engineering-notes/01-languages/java/07-oop-advanced/README.md) |
| **TypeScript** | [Classes & Private Fields](file:///E:/Learning/engineering-notes/01-languages/typescript/06-classes/README.md) |
| **Python** | [Python OOP & Properties](file:///E:/Learning/engineering-notes/01-languages/python/05-oop/README.md) |

---

## 17. Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Domain Model vs Transaction Script](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/10-domain-model-vs-transaction-script.md) | [Module 09](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/README.md) | [MVC, MVP & MVVM](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/12-mvc-mvp-mvvm.md) |
