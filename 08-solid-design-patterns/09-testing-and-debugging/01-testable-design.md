# Testable Design

Testable code lets you check behavior **quickly, in isolation and deterministically**. This isn't a separate quality from good design. The same decisions that make code testable (explicit dependencies, small responsibilities, clear contracts) make it maintainable.

## The core idea: seams

A **seam** is a place where you can change behavior in a test without editing the code under test. Constructor parameters, interfaces and function arguments are seams. A direct `new`, a static call or a global lookup is a wall.

```text
Seam present:   OrderService(OrderRepository, Clock)   → pass fakes in tests
Wall:           new JdbcOrderRepository(); LocalDate.now(); Singleton.getInstance()
```

## What makes code hard to test

### 1. Dependencies created inside

```java
public class ReportService {
    private final ReportRepository repo = new MySqlReportRepository();   // can't replace
}
```

Fix: [inject](../03-patterns-intro/02-dependency-injection.md) an abstraction.

```java
public class ReportService {
    private final ReportRepository repo;
    public ReportService(ReportRepository repo) { this.repo = repo; }
}
```

### 2. Hidden inputs: time, randomness, environment

```java
public boolean isExpired(Coupon c) {
    return c.expiry().isBefore(LocalDate.now());      // result depends on the day the test runs
}
```

Fix: treat the clock as a dependency.

```java
public class CouponService {
    private final Clock clock;
    public CouponService(Clock clock) { this.clock = clock; }

    public boolean isExpired(Coupon c) {
        return c.expiry().isBefore(LocalDate.now(clock));
    }
}

@Test
void couponIsExpiredAfterItsExpiryDate() {
    Clock fixed = Clock.fixed(Instant.parse("2026-01-10T00:00:00Z"), ZoneOffset.UTC);
    var service = new CouponService(fixed);

    assertTrue(service.isExpired(new Coupon(LocalDate.of(2026, 1, 9))));
}
```

The same applies to `Random`, `UUID.randomUUID()`, environment variables, system properties and file paths. Pass them in.

### 3. Static and global state

```java
public void process(Order o) {
    AppConfig.INSTANCE.get("tax.rate");        // global
    Metrics.counter("orders").increment();      // static
}
```

Static calls can't be replaced, and global state leaks between tests. Wrap them behind an interface or pass values in. See why [Singleton](../04-creational-patterns/01-singleton.md) hurts testing.

### 4. Business logic tangled with side effects

```java
public void closeMonth() {
    List<Invoice> invoices = db.query("...");
    double total = 0;
    for (Invoice i : invoices) total += i.amount() * 1.2;     // the logic
    mailer.send("cfo@example.com", "Total: " + total);        // the side effect
}
```

To test the arithmetic you need a database and a mail server. Separate decisions from effects:

```java
// Pure: easy to test with plain values
static Money totalWithTax(List<Invoice> invoices, double taxRate) { ... }

// Thin orchestration: tested lightly with fakes
public void closeMonth() {
    var invoices = repo.findForMonth(month);
    mailer.send(cfo, "Total: " + totalWithTax(invoices, rate));
}
```

This is the **functional core, imperative shell** idea: keep decisions in pure functions that take values and return values, and push I/O to a thin outer layer.

### 5. Long chains and deep object graphs

```java
String city = order.getCustomer().getAddress().getCity();     // test must build all three objects
```

Follow the Law of Demeter ([coupling and cohesion](../01-foundations/03-coupling-and-cohesion.md)): ask `order.shippingCity()`, so tests set up less.

### 6. Large constructors and large classes

If a test needs eight collaborators, the class likely has too many responsibilities ([SRP](../02-solid-principles/01-single-responsibility.md)). Splitting it makes both the code and the tests smaller.

### 7. Logic hidden in constructors or private methods

Constructors that open connections or call services make instantiation itself a test hazard. Keep constructors simple: assign fields and validate arguments.

If a private method seems to need its own test, it's probably a separate concept. Extract it into its own class with a public method, rather than reaching for reflection.

## Designing for testability: a checklist

| Do | Avoid |
|---|---|
| Pass dependencies through constructors | `new` on collaborators inside business classes |
| Depend on small interfaces at boundaries | Depending on concrete infrastructure classes |
| Inject `Clock`, `Random`, ID generators | `now()`, `random()` buried in logic |
| Return values; keep functions pure where possible | Methods that only change hidden state |
| Keep I/O at the edges | Mixing SQL, HTTP and rules in one method |
| Prefer immutable objects | Shared mutable state |
| Keep classes small and focused | Large "manager" classes |

## Writing a good unit test

### Structure: Arrange, Act, Assert

```java
@Test
void goldCustomersGetTenPercentDiscount() {
    // Arrange
    var policy = new GoldDiscount();

    // Act
    double discount = policy.discount(200.0);

    // Assert
    assertEquals(20.0, discount, 0.001);
}
```

### Test behavior, not implementation

```java
// Brittle: tied to HOW it works
verify(repository).findById(id);
verify(repository, times(1)).save(any());

// Robust: tied to WHAT results
assertEquals(OrderStatus.CANCELLED, orders.findById(id).get().status());
```

Tests that mirror the implementation break whenever you refactor, even when behavior is unchanged, which discourages refactoring.

### Naming

Describe the behavior and the condition:

```text
placingAnEmptyOrderIsRejected
expiredCouponCannotBeApplied
transferFailsWhenBalanceIsInsufficient
```

A failing test name should tell you what broke without opening the file.

### One reason to fail

Each test checks one behavior. Many assertions about the same outcome are fine; several unrelated behaviors in one test are not.

### Avoid logic in tests

Loops, conditionals and calculations inside tests can hide bugs. Prefer literal expected values.

### Parameterized tests for variations

```java
@ParameterizedTest
@CsvSource({"REGULAR,0.0", "SILVER,10.0", "GOLD,20.0"})
void discountByCustomerType(CustomerType type, double expected) {
    assertEquals(expected, DiscountPolicies.forType(type).discount(200.0), 0.001);
}
```

## What to test where

| Code | Test with |
|---|---|
| Domain rules (entities, value objects, pure functions) | Plain unit tests, no doubles |
| Use cases / services | Unit tests with fakes for repositories and gateways |
| Adapters (database, HTTP clients) | Integration tests against real or containerized infrastructure |
| Wiring and configuration | A few smoke or slice tests |
| Whole flows | A small number of end-to-end tests |

## Common mistakes

- **Testing private methods via reflection**, instead of improving the design.
- **Slow unit tests** (starting a framework or database to test a calculation).
- **Order-dependent or shared-state tests** that pass alone and fail together.
- **Asserting on log output or internal calls** instead of outcomes.
- **Copying production logic into the test** to compute expected values.
- **Ignoring flaky tests.** They erode trust; fix or remove them.
- **Chasing a coverage number** with assertion-free tests.
- **Making things public only for tests.** Test through the public behavior instead.

## Quick Summary

- Testable code has seams: explicit dependencies and no hidden inputs.
- Inject time, randomness and infrastructure; separate pure logic from side effects.
- Write small, fast tests that check behavior with clear names.
- Difficulty testing is a design signal; fix the design, not the test.

## Related

[Test doubles](./02-test-doubles.md) · [Dependency injection](../03-patterns-intro/02-dependency-injection.md) · [Hexagonal architecture](../08-architecture/02-clean-hexagonal-onion.md)

## Next

[02-test-doubles.md](./02-test-doubles.md)
