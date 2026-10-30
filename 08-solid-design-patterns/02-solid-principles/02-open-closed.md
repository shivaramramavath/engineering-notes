# Open/Closed Principle (OCP)

> Software entities should be open for extension, but closed for modification.

You should be able to **add new behavior by writing new code**, not by editing code that already works and is tested. Editing working code risks breaking it; adding new code beside it doesn't.

## The typical violation

A type check that grows with every new case.

```java
public class DiscountCalculator {
    public double discount(Customer c, double total) {
        switch (c.type()) {
            case REGULAR: return total * 0.00;
            case SILVER:  return total * 0.05;
            case GOLD:    return total * 0.10;
            default: throw new IllegalArgumentException();
        }
    }
}
```

Every new customer type means editing this method, retesting all existing cases, and risking a regression in the old ones.

## The fix: depend on an abstraction

```java
public interface DiscountPolicy {
    double discount(double total);
}

public class RegularDiscount implements DiscountPolicy {
    public double discount(double total) { return 0; }
}
public class SilverDiscount implements DiscountPolicy {
    public double discount(double total) { return total * 0.05; }
}
public class GoldDiscount implements DiscountPolicy {
    public double discount(double total) { return total * 0.10; }
}

public class Checkout {
    private final DiscountPolicy policy;
    public Checkout(DiscountPolicy policy) { this.policy = policy; }

    public double finalPrice(double total) {
        return total - policy.discount(total);
    }
}
```

Adding `PlatinumDiscount` means a new class. `Checkout` doesn't change.

```text
Before:   DiscountCalculator ── switch(type) ── edit for every new type

After:    Checkout ──► DiscountPolicy ◄── Regular / Silver / Gold / Platinum (new class)
          (closed)       (stable)             (open: add here)
```

This is the Strategy pattern; see [strategy](../06-behavioral-patterns/01-strategy.md).

## Ways to achieve OCP

| Technique | Use when | Pattern |
|---|---|---|
| Polymorphism via interface | Behavior varies by type | [Strategy](../06-behavioral-patterns/01-strategy.md) |
| Wrapping to add behavior | Adding cross-cutting behavior (logging, caching) | [Decorator](../05-structural-patterns/04-decorator.md) |
| Fixed algorithm skeleton with replaceable steps | Same flow, varying steps | [Template Method](../06-behavioral-patterns/05-template-method.md) |
| Registering handlers | New event types or plugins | [Observer](../06-behavioral-patterns/02-observer.md), registry |

## "Closed" is relative

No design can be closed against every kind of change. `Checkout` above is closed against *new discount types*, but adding a new input (say, a coupon code) still requires editing it.

Practical approach:

1. Write the simple version first.
2. When a change of a given kind happens, make the code resistant to *that* kind of change.
3. If the same kind of change happens again, that's the signal to introduce the abstraction.

Predicting every future extension leads to speculative abstractions nobody needs (see [avoiding overengineering](../10-production/05-avoiding-overengineering.md)).

## When a `switch` is fine

A `switch` is not automatically a violation. It's fine when:

- The set of cases is closed and stable (days of the week, enum of protocol states fixed by a spec).
- It sits at a single creation point, such as a factory that maps a type to an object.
- Adding a case would be rare and caught by the compiler (Java `switch` expressions over `sealed` types or enums are checked for exhaustiveness).

## Common mistakes

- **Abstracting before there is any variation**, leaving a single implementation behind an interface.
- **Interface designed around one implementation**, so the next implementation doesn't fit.
- **Using inheritance to extend** and then breaking [Liskov](./03-liskov-substitution.md).
- **Treating "closed" as "never edit"**. Bugs and requirement changes still require edits; OCP reduces them for *anticipated* kinds of change.

## Quick Summary

- Add behavior by adding code, not by editing existing, tested code.
- The usual mechanism is an abstraction with multiple implementations.
- Apply it after you see change, not before.
- Growing `if/else` or `switch` chains on type are the main smell.

## Next

[03-liskov-substitution.md](./03-liskov-substitution.md)
