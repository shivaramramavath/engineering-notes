# Strategy

**Intent:** define a family of algorithms, encapsulate each one, and make them interchangeable, so the algorithm can vary independently from the code that uses it.

This is the pattern behind most "remove the growing `if/else`" refactorings and the standard way to apply the [Open/Closed Principle](../02-solid-principles/02-open-closed.md).

## The problem

```java
public double shippingCost(Order order, String method) {
    if (method.equals("STANDARD")) return 5.0;
    else if (method.equals("EXPRESS")) return 15.0 + order.weightKg() * 1.5;
    else if (method.equals("PICKUP")) return 0.0;
    throw new IllegalArgumentException(method);
}
```

Each new shipping method edits this method, and every algorithm is mixed into one class.

## Structure

```text
Context ──► Strategy (interface)
  uses        + execute(...)
                  ▲
        ┌─────────┼─────────┐
 StrategyA    StrategyB   StrategyC
```

- **Strategy**: common interface for all algorithms.
- **Concrete strategies**: one class per algorithm.
- **Context**: holds a strategy and delegates the work to it.

## Example

```java
public interface ShippingStrategy {
    double cost(Order order);
}

public class StandardShipping implements ShippingStrategy {
    public double cost(Order order) { return 5.0; }
}

public class ExpressShipping implements ShippingStrategy {
    public double cost(Order order) { return 15.0 + order.weightKg() * 1.5; }
}

public class PickupShipping implements ShippingStrategy {
    public double cost(Order order) { return 0.0; }
}

public class Checkout {
    private ShippingStrategy shipping;

    public Checkout(ShippingStrategy shipping) { this.shipping = shipping; }

    public void setShipping(ShippingStrategy shipping) { this.shipping = shipping; }  // change at runtime

    public double total(Order order) {
        return order.subtotal() + shipping.cost(order);
    }
}

new Checkout(new ExpressShipping()).total(order);
```

A new method, say `DroneShipping`, is a new class. `Checkout` is untouched.

## With lambdas

When a strategy is one method, a functional interface and lambdas remove the boilerplate.

```java
@FunctionalInterface
public interface ShippingStrategy { double cost(Order order); }

ShippingStrategy standard = order -> 5.0;
ShippingStrategy express  = order -> 15.0 + order.weightKg() * 1.5;

new Checkout(express).total(order);
```

Or keep named strategies in a map:

```java
Map<String, ShippingStrategy> strategies = Map.of(
        "STANDARD", standard,
        "EXPRESS", express);

double cost = strategies.get(method).cost(order);
```

## Examples in the JDK

- `Comparator<T>` is a strategy for ordering: `list.sort(Comparator.comparing(User::name))`.
- `Collections.sort(list, comparator)`, `TreeMap(comparator)`.
- `ExecutorService` rejection policies (`RejectedExecutionHandler`).

## Choosing the strategy

The context shouldn't pick it. Select at the edge:

```java
ShippingStrategy strategy = switch (request.method()) {
    case STANDARD -> new StandardShipping();
    case EXPRESS  -> new ExpressShipping();
    case PICKUP   -> new PickupShipping();
};
```

This single `switch` (or a registry map) is the one acceptable place for it, usually in a factory or the composition root. See [Factory Method](../04-creational-patterns/02-factory-method.md).

## Strategy vs similar patterns

| Pattern | Difference |
|---|---|
| [State](./04-state.md) | Strategies are chosen by the client and are independent; states change themselves and usually know about the next state |
| [Template Method](./05-template-method.md) | Inheritance-based variation of *steps*; Strategy swaps the *whole* algorithm via composition |
| [Bridge](../05-structural-patterns/02-bridge.md) | Structural, designed to split two hierarchies; Strategy focuses on swapping behavior |
| [Decorator](../05-structural-patterns/04-decorator.md) | Wraps *around* behavior; Strategy replaces behavior *inside* |

## When to use it

- Several variants of an algorithm exist, or will (pricing, validation, sorting, routing, compression).
- A `switch` or `if/else` chain on type or mode keeps growing.
- You want to test each algorithm independently.
- The algorithm should be swappable at runtime.

Skip it for two stable variants that never change. A simple `if` is clearer.

## Common mistakes

- **Strategy interface shaped by one implementation**, so others need awkward parameters. Define it by what the context needs.
- **Context knowing which concrete strategy it has** (`instanceof` checks).
- **Passing too much data**, or the whole context, into the strategy, which couples them tightly.
- **Strategies holding mutable shared state**, which causes thread-safety bugs.
- **A class per trivial variation**, where a lambda or a parameter would do.

## Quick Summary

- Strategy encapsulates interchangeable algorithms behind one interface.
- The context delegates; the client (or composition root) chooses the strategy.
- In Java, lambdas often replace the concrete classes.
- Differs from State by who changes the behavior and why.

## Related

[State](./04-state.md) · [Template Method](./05-template-method.md) · [Factory Method](../04-creational-patterns/02-factory-method.md) · [Open/Closed](../02-solid-principles/02-open-closed.md)

## Next

[02-observer.md](./02-observer.md)
