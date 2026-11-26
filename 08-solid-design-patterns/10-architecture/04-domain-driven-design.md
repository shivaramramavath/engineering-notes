# Domain-Driven Design (DDD)

Domain-Driven Design, introduced by Eric Evans, is an approach to building software for **complex business domains** by centering the design on a rich model of that domain, built together with domain experts and expressed directly in code.

DDD is not a framework or a folder layout. It's a way of thinking, with two parts:

| Part | Focus | Question |
|---|---|---|
| **Strategic design** | The big picture | How do we divide the domain and keep models separate and clear? |
| **Tactical design** | The building blocks in code | How do we express the model with objects? |

## Strategic design

### Ubiquitous language

Developers and domain experts use **the same vocabulary**, in conversation *and* in code. If the business says "policy lapses", the code has `policy.lapse()`, not `policy.setStatus(3)`.

Benefits: fewer translation errors, and code that business people can read and challenge. When the language and the code disagree, fix the code or refine the language.

### Bounded context

A **bounded context** is a boundary inside which a model and its language are consistent. The same word can mean different things in different contexts, and that's fine.

```text
Sales context                 Shipping context              Support context
Customer: buyer, credit,      Recipient: address,           Customer: tickets,
pricing tier                  delivery window               satisfaction history
```

Trying to build *one* "Customer" model for everyone creates a bloated class that serves nobody well. Each context gets its own model.

Bounded contexts are the natural boundaries for [modules](./03-modular-monolith.md) or, if needed, services.

### Context map

Describes how bounded contexts relate and integrate.

| Relationship | Meaning |
|---|---|
| **Customer / Supplier** | Upstream context serves downstream's needs |
| **Conformist** | Downstream adopts upstream's model as is |
| **Anticorruption layer** | Downstream translates upstream's model so it doesn't pollute its own (an [Adapter](../05-structural-patterns/01-adapter.md)) |
| **Shared kernel** | Two contexts share a small common model (use sparingly) |
| **Published language** | A well-documented shared format for integration |

### Subdomains

Not all parts of the business deserve equal investment.

| Type | Meaning | Approach |
|---|---|---|
| **Core** | What differentiates the business | Best people, richest model, custom-built |
| **Supporting** | Needed, but not a differentiator | Simpler design |
| **Generic** | Solved problems (auth, billing, email) | Buy or use off-the-shelf |

## Tactical design

### Entity

An object defined by **identity**, which persists as its attributes change.

```java
public class Order {
    private final OrderId id;               // identity
    private OrderStatus status;
    private final List<OrderLine> lines = new ArrayList<>();

    public void addLine(ProductId product, int quantity, Money unitPrice) { ... }
}
```

Two orders with identical contents are still different orders.

### Value object

An object defined by its **values**, with no identity. Immutable and replaceable.

```java
public record Money(BigDecimal amount, Currency currency) {
    public Money {
        Objects.requireNonNull(amount);
        Objects.requireNonNull(currency);
    }

    public Money plus(Money other) {
        if (!currency.equals(other.currency)) throw new IllegalArgumentException("Currency mismatch");
        return new Money(amount.add(other.amount), currency);
    }
}
```

Replace primitives with value objects (`Money`, `EmailAddress`, `Quantity`) to carry meaning and validate on construction.

### Aggregate

A cluster of entities and value objects treated as **one consistency unit**, with a single entry point, the **aggregate root**. Outsiders interact only with the root.

```java
public class Order {                         // aggregate root
    private OrderStatus status = OrderStatus.OPEN;
    private final List<OrderLine> lines = new ArrayList<>();

    public void addLine(ProductId product, int quantity, Money unitPrice) {
        if (status != OrderStatus.OPEN) throw new IllegalStateException("Order is closed");
        if (quantity <= 0) throw new IllegalArgumentException("Quantity must be positive");
        lines.add(new OrderLine(product, quantity, unitPrice));
    }

    public Money total() {
        return lines.stream().map(OrderLine::subtotal).reduce(Money.zero(), Money::plus);
    }

    public void submit() {
        if (lines.isEmpty()) throw new IllegalStateException("Cannot submit an empty order");
        this.status = OrderStatus.SUBMITTED;
    }
}
```

The root **protects invariants**: the rules that must always hold ("a submitted order has at least one line", "a closed order can't change"). No one can bypass them by modifying `OrderLine` directly.

#### Aggregate design rules

| Rule | Why |
|---|---|
| **Keep aggregates small** | Large aggregates cause contention and slow loads |
| **Reference other aggregates by ID**, not by object | Keeps boundaries clear and loading cheap |
| **One transaction modifies one aggregate** | Consistency within, eventual consistency between |
| **Design around invariants** | If two things need immediate consistency, they belong together |
| **Access through the root only** | Preserves invariants |

### Domain service

Business logic that doesn't naturally belong to one entity or value object, expressed as a stateless operation in domain terms.

```java
public class TransferService {
    public void transfer(Account from, Account to, Money amount) {
        from.withdraw(amount);
        to.deposit(amount);
    }
}
```

### Domain event

A record that something meaningful **happened** in the domain, in the past tense, immutable.

```java
public record OrderSubmitted(OrderId orderId, Instant occurredAt) {}
```

Other parts react to events (see [event-driven](./05-event-driven.md)), which keeps contexts decoupled.

### Repository and factory

- [Repository](../07-application-patterns/01-repository.md): loads and saves aggregates, one per aggregate root.
- **Factory**: encapsulates complex creation so an aggregate is always created valid ([creational patterns](../04-creational-patterns/00-overview.md)).

## Where DDD fits in the code structure

DDD pairs naturally with [hexagonal architecture](./02-clean-hexagonal-onion.md): the domain model sits at the center, free of infrastructure, and application services orchestrate use cases around it.

```text
Application service  → loads aggregate via repository
                     → calls behavior on the aggregate root (invariants enforced)
                     → saves, publishes domain events
```

## Anemic vs rich domain model

```java
// Anemic: data only; rules live elsewhere
order.setStatus("SUBMITTED");
// Rich: behavior and invariants inside the model
order.submit();
```

DDD's tactical patterns push toward the rich model. See [service layer](../07-application-patterns/04-service-layer.md).

## When to use it

- The business domain is **complex**, with rules that matter and change.
- You have access to domain experts and can collaborate.
- The system will live a long time.

Don't use full DDD for simple CRUD, reporting apps or thin integrations. The cost of modeling exceeds the benefit. You can still borrow the cheap, high-value parts: ubiquitous language, value objects, and clear module boundaries.

## Common mistakes

- **Using the tactical patterns without the strategic part**: building entities and repositories everywhere, but no shared language or bounded contexts.
- **One giant model** for the whole company instead of bounded contexts.
- **Huge aggregates** loading half the database.
- **Aggregates referencing each other by object graph**, causing cascaded loads and transaction sprawl.
- **Anemic models** with DDD vocabulary on top.
- **Naming things after technology** (`OrderManager`, `OrderDTO`) instead of the business language.
- **Applying DDD everywhere**, even to generic or simple subdomains.
- **Developers modeling alone**, without domain experts.
- **Modifying several aggregates in one transaction** by default, instead of coordinating via events.

## Quick Summary

- DDD models complex domains with a shared language and bounded contexts (strategic design).
- Tactical building blocks: entities, value objects, aggregates, domain services, domain events, repositories.
- Aggregates protect invariants and define transaction boundaries.
- Invest in DDD for core, complex domains; keep simpler areas simple.

## Related

[Hexagonal architecture](./02-clean-hexagonal-onion.md) · [Modular monolith](./03-modular-monolith.md) · [Event-driven](./05-event-driven.md) · [Repository](../07-application-patterns/01-repository.md) · [Specification](../07-application-patterns/03-specification.md)

## Next

[05-event-driven.md](./05-event-driven.md)
