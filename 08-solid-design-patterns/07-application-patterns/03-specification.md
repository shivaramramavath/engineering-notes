# Specification

**Intent:** encapsulate a business rule (a criterion an object must satisfy) in a reusable object that can be combined with other rules using boolean logic.

A specification answers one question: **"does this object satisfy this rule?"** Because each rule is an object, rules can be named, tested, reused and composed with `and`, `or` and `not`.

## The problem

Business rules end up as conditions scattered across the code:

```java
// In a service
if (customer.age() >= 18 && customer.isVerified() && !customer.isBlocked()) { ... }

// In a report, slightly different
customers.stream().filter(c -> c.age() > 18 && c.isVerified()).toList();   // is >18 the same rule?

// In a query
"WHERE age >= 18 AND verified = true AND blocked = false"
```

The same rule is duplicated, drifts out of sync, and has no name ("eligible customer"). Combining or reusing parts is clumsy, and repository interfaces grow methods like `findVerifiedAdultsNotBlockedWithOverdueInvoices`.

## Structure

```text
Specification<T> (interface)
  + isSatisfiedBy(T): boolean
  + and(other), or(other), not()
        ▲
   ┌────┴─────────────┬──────────────────┐
IsAdult         IsVerified          AndSpecification(left, right) ...
```

## Example

```java
@FunctionalInterface
public interface Specification<T> {
    boolean isSatisfiedBy(T candidate);

    default Specification<T> and(Specification<T> other) {
        return c -> this.isSatisfiedBy(c) && other.isSatisfiedBy(c);
    }

    default Specification<T> or(Specification<T> other) {
        return c -> this.isSatisfiedBy(c) || other.isSatisfiedBy(c);
    }

    default Specification<T> not() {
        return c -> !this.isSatisfiedBy(c);
    }
}
```

Named, single-purpose rules:

```java
public class IsAdult implements Specification<Customer> {
    public boolean isSatisfiedBy(Customer c) { return c.age() >= 18; }
}

public class IsVerified implements Specification<Customer> {
    public boolean isSatisfiedBy(Customer c) { return c.isVerified(); }
}

public class IsBlocked implements Specification<Customer> {
    public boolean isSatisfiedBy(Customer c) { return c.isBlocked(); }
}
```

Composing and reusing:

```java
Specification<Customer> eligible =
        new IsAdult().and(new IsVerified()).and(new IsBlocked().not());

if (eligible.isSatisfiedBy(customer)) { /* allow */ }

List<Customer> eligibleCustomers = customers.stream()
        .filter(eligible::isSatisfiedBy)
        .toList();
```

Now "eligible customer" is one named definition, used for validation, filtering and reporting.

## Three uses of specifications

| Use | Example |
|---|---|
| **Validation** | Does this object meet the rule? (`order` can be shipped) |
| **Selection** | Which objects from a collection match? (filter) |
| **Construction to order** | Describe what an object must satisfy to be created |

## Specifications and repositories

In-memory filtering works for small sets, but loading everything to filter is wasteful. Options:

### 1. Let the repository evaluate the specification in memory

```java
List<Customer> findMatching(Specification<Customer> spec);   // loads and filters in Java
```

Simple, but only suitable for small collections.

### 2. Make the specification translatable to a query

The specification can expose its criteria so the repository converts it into SQL or a query API call.

```java
public interface QuerySpecification<T> extends Specification<T> {
    // e.g. returns a JPA Predicate or SQL fragment
}
```

Spring Data JPA has its own `Specification<T>` type that builds Criteria API predicates executed in the database. It follows the same idea: composable rules with `and()` and `or()`, executed as SQL.

```java
Specification<Customer> verified = (root, query, cb) -> cb.isTrue(root.get("verified"));
Specification<Customer> adult    = (root, query, cb) -> cb.ge(root.get("age"), 18);

List<Customer> result = customerRepository.findAll(verified.and(adult));
```

The trade-off: the rule now exists twice if you want both an in-memory check and a database query (and they must agree). Some teams keep the in-memory version for domain validation and a query version for fetching.

## Parameterized specifications

```java
public class HasOverdueInvoices implements Specification<Customer> {
    private final Clock clock;
    private final int minDays;

    public HasOverdueInvoices(Clock clock, int minDays) { this.clock = clock; this.minDays = minDays; }

    public boolean isSatisfiedBy(Customer c) {
        return c.invoices().stream().anyMatch(i -> i.daysOverdue(clock) >= minDays);
    }
}
```

Inject dependencies such as a `Clock` rather than reading the system time inside, so rules stay testable.

## Specification vs simple alternatives

| Option | Use when |
|---|---|
| Boolean method on the entity (`customer.isEligible()`) | The rule belongs naturally to one object and is rarely combined |
| `Predicate<T>` (Java's built-in) | A lightweight rule, and you don't need names or extra structure |
| **Specification** | Rules are named business concepts, combined in varied ways, shared across validation and queries |

`java.util.function.Predicate` already has `and`, `or` and `negate`. Often you can implement the idea with predicates and only introduce a dedicated `Specification` type when you need explanations, query translation or richer behavior.

```java
Predicate<Customer> isAdult = c -> c.age() >= 18;
Predicate<Customer> eligible = isAdult.and(Customer::isVerified).and(Predicate.not(Customer::isBlocked));
```

## When to use it

- The domain has several named business rules reused in more than one place.
- Rules are combined in many ways (reports, eligibility, search filters).
- Repository interfaces are growing one method per criteria combination.

Skip it for one-off conditions or simple CRUD filtering.

## Common mistakes

- **Specifications for trivial conditions**, adding classes where an `if` is clearer.
- **Business logic duplicated** between in-memory and query versions, drifting apart.
- **Loading everything then filtering** in memory for large datasets.
- **Hidden performance traps**: a specification that loads related data per object (N+1 queries).
- **Specifications with side effects** or hidden state. They should be pure checks.
- **Reading the clock or other globals directly**, which makes tests flaky.
- **Over-abstracting** with a full specification framework for a small rule set.

## Quick Summary

- A specification is a named, combinable business rule that answers "does this satisfy it?".
- `and`, `or` and `not` let you build complex rules from small ones.
- Use it for validation and selection; translate to queries when working with large data.
- `Predicate` often suffices; add a dedicated type when rules need names, explanations or query support.

## Related

[Repository](./01-repository.md) · [Strategy](../06-behavioral-patterns/01-strategy.md) · [Composite](../05-structural-patterns/03-composite.md) · [Domain-driven design](../08-architecture/04-domain-driven-design.md)

## Next

[04-service-layer.md](./04-service-layer.md)
