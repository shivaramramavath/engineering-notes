# Coupling and Cohesion

These two ideas are the vocabulary for judging a design. Most design advice, including every SOLID principle, can be rephrased as "reduce coupling" or "increase cohesion".

- **Cohesion:** how closely the parts *inside* a module belong together.
- **Coupling:** how much a module depends on *other* modules.

Goal: **high cohesion, low coupling**.

```text
High cohesion:  one class, one clear job, everything in it relates
Low coupling:   change one class, few others notice
```

## Cohesion

A cohesive class has a single, nameable purpose. A quick test: can you describe it in one sentence without using "and"?

| Level | Description | Example |
|---|---|---|
| High (good) | All members work toward one purpose | `InvoiceCalculator` computes totals and taxes |
| Medium | Related by process or timing | `StartupTasks` runs several init steps |
| Low (bad) | Unrelated members grouped together | `Utils` with date parsing, email sending and file IO |

### Example

Low cohesion:

```java
class OrderService {
    void placeOrder(Order o) { ... }
    void sendConfirmationEmail(Order o) { ... }
    void generateInvoicePdf(Order o) { ... }
    void calculateShipping(Order o) { ... }
    void exportOrdersToCsv() { ... }
}
```

This has several reasons to change (email provider, PDF library, shipping rules, CSV format). Split it by responsibility. This is the [Single Responsibility Principle](../02-solid-principles/01-single-responsibility.md).

## Coupling

Coupling measures how much one module knows about another. Some coupling is necessary; the question is what you are coupled *to*.

From worst to best:

| Kind | What it looks like |
|---|---|
| Content | One class reads or modifies another's internals |
| Common | Classes share global mutable state |
| Control | One class passes flags that control another's logic |
| Data | Classes share only simple parameters |
| Message / interface | Classes interact only through a stable contract |

### Example

Tight coupling:

```java
class OrderService {
    private final MySqlOrderRepository repo = new MySqlOrderRepository();  // concrete + created inside
}
```

Looser:

```java
class OrderService {
    private final OrderRepository repo;                   // abstraction
    OrderService(OrderRepository repo) { this.repo = repo; }   // supplied from outside
}
```

Now `OrderService` can be tested with an in-memory fake and survives a database change. See [dependency injection](../03-patterns-intro/02-dependency-injection.md).

## Law of Demeter

A method should talk to its direct collaborators, not reach through them.

```java
// Reaching through objects: coupled to the whole chain
String city = order.getCustomer().getAddress().getCity();

// Ask the direct collaborator
String city = order.shippingCity();
```

If `Address` changes, only `Order` has to change, not every caller.

## Companion principles

| Principle | Meaning | Watch out for |
|---|---|---|
| DRY | One authoritative place for each piece of knowledge | Merging code that only *looks* similar but changes for different reasons |
| KISS | Prefer the simplest design that works | Clever abstractions nobody needs |
| YAGNI | Don't build for requirements you don't have | Using it as an excuse to ignore obvious extension points |

## Measuring coupling (optional)

For packages or modules:

- **Afferent coupling (Ca):** how many others depend on me.
- **Efferent coupling (Ce):** how many I depend on.
- **Instability:** `I = Ce / (Ca + Ce)`. Near 0 is stable (hard to change, many dependents); near 1 is unstable (easy to change, few dependents).

Dependencies should point **toward stability**: volatile code depends on stable abstractions, not the other way round. This idea returns in [Dependency Inversion](../02-solid-principles/05-dependency-inversion.md).

## Warning signs

| Symptom | Likely problem |
|---|---|
| One change touches many files ("shotgun surgery") | Tight coupling, scattered responsibility |
| A class has many unrelated imports | Low cohesion |
| Tests need heavy setup | Hidden or concrete dependencies |
| Name ends in `Manager`, `Helper`, `Utils` | Low cohesion |
| Method chains like `a.getB().getC().doX()` | Law of Demeter violation |

## Common mistakes

- **Chasing zero coupling.** Modules must interact. Aim for coupling through small, stable contracts.
- **Splitting too far.** Dozens of tiny classes with one method each lowers readability and may *raise* total coupling.
- **Confusing cohesion with size.** A big class can be cohesive; a small one can be a grab bag.

## Quick Summary

- High cohesion: a class does one thing well.
- Low coupling: depend on small, stable contracts, not concrete details.
- Use these two terms to describe *why* a design is good or bad.
- DRY, KISS and YAGNI help, but apply them with judgment.

## Next

[04-programming-to-interfaces.md](./04-programming-to-interfaces.md)
