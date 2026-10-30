# SOLID in Practice

Knowing the five principles is the easy part. The hard part is applying them where they help, and *not* applying them where they don't. This note covers how SOLID shows up in real code, how it gets misused, and how to decide how far to go.

## Start from the pain, not the principle

Don't open a class and ask "which principle can I apply here?". Look for friction, then pick the matching principle.

| Pain you feel | Principle to reach for |
|---|---|
| Fixes in one feature break another | SRP |
| Every new variant means editing the same `switch` | OCP |
| Subclass surprises callers at runtime | LSP |
| Implementations stub out methods | ISP |
| Tests need a database or network | DIP |

If you feel no pain and the code rarely changes, leave it alone.

## A worked example

A first version of an order processor:

```java
public class OrderProcessor {
    public void process(Order order) {
        if (order.items().isEmpty()) throw new IllegalArgumentException("Empty order");

        double total = 0;
        for (Item i : order.items()) total += i.price() * i.quantity();
        if (order.customerType().equals("GOLD")) total *= 0.9;
        else if (order.customerType().equals("SILVER")) total *= 0.95;

        try (Connection c = DriverManager.getConnection(DB_URL)) {
            // INSERT order and total
        } catch (SQLException e) { throw new RuntimeException(e); }

        new SmtpClient().send(order.email(), "Order confirmed: " + total);
    }
}
```

Problems by principle:

| Principle | Problem |
|---|---|
| SRP | Validation, pricing, persistence and email are in one method |
| OCP | A new customer type means editing the `if/else` |
| DIP | Concrete database and SMTP created inside |
| ISP / LSP | Not yet visible: there are no abstractions yet |

### After

```java
public interface DiscountPolicy { double apply(double total); }
public interface OrderRepository { void save(Order order, double total); }
public interface Notifier { void notify(String to, String message); }

public class OrderProcessor {
    private final DiscountPolicy discount;
    private final OrderRepository repository;
    private final Notifier notifier;

    public OrderProcessor(DiscountPolicy discount, OrderRepository repository, Notifier notifier) {
        this.discount = discount;
        this.repository = repository;
        this.notifier = notifier;
    }

    public void process(Order order) {
        order.validate();                                    // behavior stays with the entity
        double total = discount.apply(order.subtotal());
        repository.save(order, total);
        notifier.notify(order.email(), "Order confirmed: " + total);
    }
}
```

The discount rules are chosen when the processor is constructed, so `OrderProcessor` stays unchanged when a new rule is added. The class now *coordinates*; the details live behind interfaces. Notice what we did **not** do: we didn't split validation or subtotal into separate classes, because they don't change independently of `Order`.

## Misusing SOLID (anti-patterns)

| Anti-pattern | What it looks like | Why it hurts |
|---|---|---|
| **Interface per class** | `UserService` + `UserServiceImpl`, no second implementation | Extra files, no flexibility gained |
| **Class explosion** | A class for every tiny step | Logic is scattered; hard to follow |
| **Speculative OCP** | Plugin system for something that never varies | Complexity with no payoff |
| **God abstraction** | One interface to "future-proof" everything | Becomes a fat interface; violates ISP |
| **Leaky abstraction** | Interface exposes SQL, HTTP or provider-specific details | DIP in name only |
| **Pattern worship** | Forcing a Factory/Strategy where an `if` is clear | Harder to read than the problem |
| **Inheritance for reuse** | Subclassing to share a helper | Breaks LSP, couples to internals |
| **DI framework magic everywhere** | Every object injected and proxied | Hidden wiring, hard to debug |

These are covered further in [avoiding overengineering](../10-production/05-avoiding-overengineering.md).

## How far to go

A practical rule of thumb:

1. **Write the simple version.** Make it work and keep it readable.
2. **Watch for change.** When the same kind of change happens a second time, refactor toward the principle that addresses it.
3. **Add abstractions at boundaries first.** Database, network, clock, filesystem, third-party services, and places where behavior truly varies.
4. **Keep stable internal code concrete.** Not everything needs an interface.

```text
Rarely changes + one implementation        → keep it simple and concrete
Varies today or will clearly vary          → abstraction (OCP, DIP)
Crosses a boundary (I/O, third-party)      → abstraction (DIP), for testability
```

## SOLID and patterns

SOLID principles describe the goals; design patterns are common ways to reach them.

| Principle | Patterns that help |
|---|---|
| SRP | Facade (hide complexity behind one entry), Decorator (separate cross-cutting concerns) |
| OCP | Strategy, Decorator, Template Method, Observer |
| LSP | Constrains how you use inheritance in all patterns |
| ISP | Adapter, Facade, role interfaces |
| DIP | Factory Method, Abstract Factory, Dependency Injection |

## Code review checklist

- Can I describe each class in one sentence without "and"?
- Does adding a new variant require editing existing logic?
- Are there empty overrides, `instanceof` checks or `UnsupportedOperationException`s?
- Do implementers stub methods they don't need?
- Can the class be unit-tested without real I/O?
- Is each interface used by more than one implementation, or does it guard a boundary?
- Would a new teammate find the flow easy to follow?

## When it's fine to break a principle

- **Prototypes and scripts** you will throw away.
- **Small, stable code** that won't change.
- **Performance-critical paths** where indirection has a measured cost.
- **Simplicity wins** over an abstraction the team doesn't need yet.

Break a principle on purpose, and leave a short comment explaining why, so the next person knows it was a choice.

## Quick Summary

- Apply SOLID in response to real friction, not as a checklist.
- Put abstractions at volatile boundaries; keep stable internals simple.
- Interface-per-class, class explosion and speculative abstraction are the common misuses.
- Principles are guidelines with trade-offs: the goal is cheaper change, not more classes.

## Next

On to design patterns: [../03-patterns-intro/00-overview.md](../03-patterns-intro/00-overview.md)
