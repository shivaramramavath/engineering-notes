# Maintainability and Extensibility

Most of a system's life is spent being **read and changed**, not written. Maintainability is how cheaply you can understand and modify existing code; extensibility is how cheaply you can add new behavior. They overlap, but they aren't the same thing, and optimizing one can hurt the other.

| | Maintainability | Extensibility |
|---|---|---|
| Question | "Can I understand and safely change this?" | "Can I add a new capability without rewriting?" |
| Helped by | Clarity, small units, tests, consistency | Abstractions at real variation points |
| Hurt by | Cleverness, hidden dependencies, big classes | Rigid structure, hard-coded variants |
| Over-investment looks like | Endless refactoring of stable code | Frameworks, plugin systems nobody uses |

## Maintainability

### Readability first

Code is read far more often than written. Optimize for the next reader (often you, six months later).

| Practice | Example |
|---|---|
| **Intention-revealing names** | `daysUntilExpiry` rather than `d` |
| **Small functions at one level of abstraction** | A method either coordinates steps or does detail work, not both |
| **Explain *why*, not *what*, in comments** | The code says what; comments record non-obvious reasons |
| **Avoid surprises** | A method named `getX` shouldn't change state |
| **Consistent conventions** | Same naming, structure and error style across the codebase |
| **Make illegal states unrepresentable** | Use types (enums, value objects) instead of loose strings and flags |

```java
// Hard to read
if (u.getS() == 2 && u.getD() > 30) { ... }

// Self-explanatory
if (user.status() == UserStatus.SUSPENDED && user.daysSuspended() > MAX_SUSPENSION_DAYS) { ... }
```

### Structure that supports change

- **High cohesion, low coupling** ([foundations](../01-foundations/03-coupling-and-cohesion.md)): a change touches few places.
- **Clear boundaries**: [modules](../08-architecture/03-modular-monolith.md) with small public APIs and hidden internals.
- **One place per rule**: a business rule lives in one named location, not copied.
- **Minimal public surface**: the less you expose, the less you must preserve. Default to package-private and make things public deliberately.
- **Dependencies pointing to stable things** ([DIP](../02-solid-principles/05-dependency-inversion.md)).

### Tests as maintenance insurance

Tests let people change code without fear. Fast, behavior-focused tests ([testable design](../09-testing-and-debugging/01-testable-design.md)) are one of the strongest maintainability tools. Tests tied to implementation details do the opposite.

### Record decisions

Code shows *what* the system does, not *why* it was built that way. Keep short **Architecture Decision Records (ADRs)**:

```text
# ADR-007: Use outbox for publishing order events
Status: accepted
Context: Saving the order and publishing to the broker can't be atomic.
Decision: Write events to an outbox table in the same transaction; a relay publishes them.
Consequences: At-least-once delivery, so consumers must be idempotent.
```

A paragraph per significant decision saves enormous time later.

### Make the system observable

Maintainers diagnose problems from the outside, so the system must tell them what it is doing:

- Structured logs with correlation IDs.
- Metrics for key operations (rate, errors, duration).
- Meaningful error messages and health checks.

### Manage technical debt deliberately

Debt is a conscious trade of future cost for present speed. It's only dangerous when it's invisible.

| Practice | Why |
|---|---|
| Record known shortcuts (a ticket, a `TODO` linked to one) | Debt you can see can be scheduled |
| Fix as you touch (boy scout rule) | Gradual improvement in active areas |
| Prioritize by **change frequency** | Messy code nobody changes costs little; messy hotspots cost a lot |
| Reserve capacity for cleanup | Otherwise it never happens |

## Extensibility

### Extend where change is real

Make code extensible along the axes that **have actually varied or clearly will**. See [Open/Closed](../02-solid-principles/02-open-closed.md): wait for the first or second variation, then introduce the abstraction.

### Common extension mechanisms

| Mechanism | Use when | Example |
|---|---|---|
| **Polymorphism / Strategy** | Behavior varies by type or policy | Discount rules, shipping methods |
| **Registry of handlers** | New types are added over time | Map of `EventType → Handler` |
| **Decorator** | Optional cross-cutting behavior | Logging, caching, retries |
| **Events** ([event-driven](../08-architecture/05-event-driven.md)) | New reactions to something that happened | Add an analytics listener |
| **Plugin discovery** | Third parties or separate modules add implementations | Java's `ServiceLoader` |
| **Configuration / feature flags** | Behavior toggles without redeploying | Gradual rollouts |

### Example: handler registry

```java
public interface PaymentMethodHandler {
    String type();                                   // "CARD", "BANK_TRANSFER", ...
    PaymentResult pay(PaymentRequest request);
}

public class PaymentRouter {
    private final Map<String, PaymentMethodHandler> handlers;

    public PaymentRouter(List<PaymentMethodHandler> handlerList) {
        this.handlers = handlerList.stream()
                .collect(Collectors.toUnmodifiableMap(PaymentMethodHandler::type, h -> h));
    }

    public PaymentResult pay(PaymentRequest request) {
        PaymentMethodHandler handler = handlers.get(request.method());
        if (handler == null) throw new UnsupportedPaymentMethod(request.method());
        return handler.pay(request);
    }
}
```

Adding a payment method means adding a handler. `PaymentRouter` doesn't change. With a DI container the list is assembled automatically.

### Example: plugin discovery with `ServiceLoader`

```java
public interface ExportFormat { String name(); byte[] export(Report report); }

// Anywhere on the classpath, registered in META-INF/services/com.shop.ExportFormat
for (ExportFormat format : ServiceLoader.load(ExportFormat.class)) {
    registry.put(format.name(), format);
}
```

Use plugin discovery only when independent parties really need to add implementations. For internal variation, a registry is simpler.

## Public APIs and compatibility

Anything others depend on (a library API, REST endpoints, event schemas, database tables read by other teams) becomes a **contract**. Changing it costs coordination.

| Practice | Detail |
|---|---|
| **Expose less** | What you never expose, you never have to maintain |
| **Add, don't change** | Adding optional fields and methods is usually safe; removing or renaming is breaking |
| **Deprecate, then remove** | Announce, provide a migration path, keep it for a period |
| **Version deliberately** | Semantic versioning for libraries; URL or schema versions for APIs and events |
| **Be liberal in what you accept, conservative in what you send** | Tolerate unknown fields on input |
| **Test compatibility** | Contract tests between producers and consumers |

In practice, consumers come to rely on every observable behavior (ordering, timing, quirks), so changing "internal" behavior can still break someone. Treat observable behavior carefully.

## Balancing the two

```text
Too rigid:        hard-coded variants, change = edit many places
Sweet spot:       simple code + abstractions only at proven variation points
Too flexible:     config-driven everything, plugin frameworks, indirection nobody follows
```

Over-flexibility harms maintainability: the more generic the machinery, the harder it is to see what the system actually does. Extensibility is paid for in readability, so buy only what you need ([avoiding overengineering](./05-avoiding-overengineering.md)).

## Measuring (use lightly)

| Signal | Indicates |
|---|---|
| Time to onboard a new developer | Overall clarity |
| Lead time for a typical change | Cost of change |
| Frequently changed + frequently buggy files | Maintainability hotspots |
| Cycles and high coupling in dependency graphs | Structural rot |
| Cyclomatic complexity of methods | Hard-to-test, hard-to-read logic |
| Change failure rate | Weak tests or fragile design |

Numbers are hints. Use them to find where to look, not as targets.

## Common mistakes

- **Cleverness over clarity**: dense one-liners, unusual idioms, "smart" frameworks.
- **Inconsistent styles** within one codebase.
- **Premature generalization**: configurable everything, with a single use case.
- **Leaky public APIs** that expose internals, so internals can never change.
- **Breaking consumers silently**: renaming a field or changing an event without versioning.
- **Undocumented decisions**, so later teams reverse them by accident.
- **No tests around critical code**, so nobody dares to improve it.
- **Postponing cleanup indefinitely**, until the code is untouchable.
- **Copy-paste extension** (clone a class and tweak it) instead of a proper variation point.
- **Magic**: behavior hidden in annotations, reflection or global configuration that readers can't follow.

## Quick Summary

- Maintainability is about readability, clear structure and tests; extensibility is about abstractions at real variation points.
- Record decisions, make the system observable, and manage technical debt openly.
- Treat anything others depend on as a contract: expose less, add rather than change, deprecate before removing.
- Don't buy flexibility you don't need; it costs readability.

## Related

[Coupling and cohesion](../01-foundations/03-coupling-and-cohesion.md) · [Open/Closed](../02-solid-principles/02-open-closed.md) · [Modular monolith](../08-architecture/03-modular-monolith.md) · [Refactoring to patterns](../09-testing-and-debugging/03-refactoring-to-patterns.md)

## Next

[02-error-handling-design.md](./02-error-handling-design.md)
