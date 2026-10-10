# Unit Testing Fundamentals

A **unit test** checks one small piece of behavior (a method, a class, a tight cluster of collaborating objects) in isolation, quickly, and automatically. The value isn't the test itself. It's what the suite gives you: the freedom to refactor, upgrade, and ship without re-checking everything by hand.

This note is tool-independent: the habits that make tests valuable rather than burdensome. The next notes cover [JUnit](01_junit-5.md) and [Mockito](02_mocking-and-mockito.md) syntax.

**Prerequisites:** [Interfaces](../04-oop/09_interfaces.md), [Exceptions](../06-exceptions-and-debugging/README.md).

---

## 1. A first test

```java
public class PriceCalculator {
    public BigDecimal totalWithTax(BigDecimal net, BigDecimal taxRate) {
        return net.add(net.multiply(taxRate)).setScale(2, RoundingMode.HALF_UP);
    }
}
```

```java
class PriceCalculatorTest {

    @Test
    void addsTaxToNetAmount() {
        // Arrange
        var calculator = new PriceCalculator();

        // Act
        BigDecimal total = calculator.totalWithTax(new BigDecimal("100.00"), new BigDecimal("0.18"));

        // Assert
        assertEquals(new BigDecimal("118.00"), total);
    }
}
```

### Arrange-Act-Assert (Given-When-Then)

Every good test has three visible parts:

1. **Arrange (Given):** set up the object under test and its inputs.
2. **Act (When):** do **one** thing: call the method you're testing.
3. **Assert (Then):** check the outcome.

If you can't separate them (an Act in the middle of Arranging, several Acts per test), the test is trying to verify too much. Split it.

---

## 2. What makes a good unit test: F.I.R.S.T.

| Property | Meaning | Violated by |
|---|---|---|
| **Fast** | Milliseconds. You'll run thousands often | Network, disk, databases, `Thread.sleep` |
| **Isolated / Independent** | No test depends on another or on run order | Shared mutable static state, order-dependent data |
| **Repeatable** | Same result every run, on any machine | Current time, randomness, time zones, environment |
| **Self-validating** | Pass or fail automatically: no human inspection | Tests that only print output |
| **Timely** | Written close to the code (ideally just before or with it) | Tests added months later, if ever |

Add two more that matter in practice: **readable** (a test is documentation; a failure message should tell you what broke) and **focused** (one behavior per test, so failures point at one cause).

---

## 3. Test doubles

A unit under test usually collaborates with others (a repository, a payment gateway, the clock). To test it in isolation, you replace collaborators with **test doubles**:

| Double | What it does | Example |
|---|---|---|
| **Dummy** | Passed around, never used | `null`-like filler for a required parameter |
| **Stub** | Returns canned answers | `findUser(42)` always returns a fixed user |
| **Fake** | A *working*, simplified implementation | An in-memory `UserRepository` backed by a `HashMap` |
| **Spy** | Wraps a real object and records calls | Real object plus "was `send` called?" |
| **Mock** | Pre-programmed with *expectations*, verifies interactions | "`gateway.charge(...)` must be called once with 50.00" |

Prefer **state-based verification** (call the method, assert on the result or resulting state), using stubs and fakes, over **interaction-based verification** (asserting that specific calls happened). Interaction checks couple the test to *how* the code works instead of *what* it achieves, and break on harmless refactoring. Reserve mocks for **side effects that are the observable outcome** (an email was sent, a payment was requested). See [Mocking and Mockito](02_mocking-and-mockito.md).

---

## 4. Design for testability

Hard-to-test code is usually badly designed code. Testability comes from a few habits:

- **Inject collaborators** through the constructor instead of creating them inside (`new PaymentGateway()`) or reaching for static singletons.

```java
// Hard to test: hidden dependencies, real clock, real network
class Invoicing {
    void send(long orderId) {
        Order o = Database.INSTANCE.find(orderId);
        if (LocalDate.now().isAfter(o.dueDate())) { new SmtpClient().send(o.email(), "Overdue!"); }
    }
}

// Testable: dependencies are explicit, time is injected
class Invoicing {
    private final OrderRepository orders;
    private final Notifier notifier;
    private final Clock clock;

    Invoicing(OrderRepository orders, Notifier notifier, Clock clock) { ... }

    void send(long orderId) {
        Order o = orders.find(orderId);
        if (LocalDate.now(clock).isAfter(o.dueDate())) notifier.send(o.email(), "Overdue!");
    }
}
```

- **Inject time** (`Clock`) and **randomness** (a `Random`/ID supplier) so tests are deterministic ([Date-Time Best Practices](../10-date-and-time/05_date-time-best-practices.md#4-make-time-injectable-clock)).
- **Prefer pure functions** (output depends only on input, no side effects). They need no setup at all ([Functional Patterns](../09-functional-java/08_functional-patterns.md)).
- **Separate decisions from I/O.** Put business logic in plain objects, and keep the thin I/O shell (HTTP, SQL, files) separate and tested by integration tests.
- **Depend on interfaces at boundaries** (repositories, gateways), so you can substitute fakes.
- **Avoid hidden global state.** Static mutable fields and singletons leak between tests.
- Don't make things `public` or add test-only hooks just to test them. If a private method seems to need its own test, it's often a **separate class** waiting to be extracted ([Common Java Mistakes](../23-design-and-clean-code/06_common-java-mistakes.md)).

---

## 5. What to test (and what not to)

### Test behavior, not implementation

Assert **what** the unit promises (its observable results), not **how** it does it. A test that survives a correct refactoring is a good test:

```java
// Brittle: tied to internals (which helper is called, in what order)
verify(helper).normalize(input);  verify(helper).validate(input);

// Robust: observable behavior
assertEquals("asha@example.com", service.register("  ASHA@Example.com ").email());
```

### Cases to cover

For each unit, think in **equivalence classes and boundaries**:

| Category | Examples |
|---|---|
| **Typical** ("happy path") | A normal valid input |
| **Boundaries** | `0`, `1`, `-1`, `max`, `max+1`; the exact threshold (age 17/18/19); empty vs one vs many |
| **Empty/null/blank** | `null`, `""`, `"   "`, empty list, empty `Optional` |
| **Invalid input** | Out of range, wrong format → expected exception or error result |
| **State transitions** | Calling in the wrong order, calling twice |
| **Error paths** | Collaborator throws, returns nothing, times out |
| **Special values** | Duplicates, unicode, very long strings, large numbers and overflow, negative zero |

Boundaries are where bugs live. `age >= 18` and `age > 18` differ at exactly one value, and only a test at 18 can tell them apart ([Mutation Testing](05_code-coverage-and-mutation-testing.md#3-mutation-testing)).

### What not to test

- **Trivial code** (getters, setters, simple records): the compiler checks it, and coverage targets shouldn't push you there.
- **The framework or library:** don't test that Jackson serializes a field. Test *your* configuration and contracts.
- **Private methods directly:** test through the public API. If that's impossible, reconsider the design.
- **Implementation details that may legitimately change.**

---

## 6. Writing readable tests

### Names say what and when

```java
@Test void rejectsRegistrationWhenEmailAlreadyExists() { ... }
@Test void appliesTenPercentDiscountToOrdersOverOneHundred() { ... }
@Test void withdraw_throwsInsufficientFunds_whenBalanceTooLow() { ... }   // another common style
```

A failing test's name should read like the bug report. Avoid `test1`, `testRegister`, and `shouldWork`.

### One behavior per test, with focused assertions

Several asserts about one *outcome* (a returned object's fields) are fine. Asserting several unrelated behaviors in one test is not, because the first failure hides the rest and the name can't describe them.

### No logic in tests

No `if`, loops, or computing the expected value with the same algorithm as the code under test (which just repeats its bug). Use literal expected values:

```java
assertEquals(new BigDecimal("118.00"), calculator.totalWithTax(new BigDecimal("100.00"), new BigDecimal("0.18")));   // literal
```

### DAMP, not only DRY

Production code values DRY. In tests, readability beats deduplication: a reader should understand a test **without scrolling** to a base class or shared setup. Extract *helpers* (builders, factory methods) for noisy setup, but keep each test's essential data visible.

### Use test data builders for objects with many fields

```java
Order order = anOrder().withCustomer("asha").withItem("book", 2).paid().build();   // intent is obvious; irrelevant fields default
```

See [Testing Patterns](04_testing-patterns.md#1-test-data-builders-and-object-mothers).

---

## 7. Test-driven development (TDD)

TDD flips the order: **write a failing test first**, make it pass with the simplest code, then clean up.

```text
 RED      write a small test that fails (for the right reason)
 GREEN    write the simplest code to make it pass
 REFACTOR improve the design with the safety net of passing tests
 (repeat in small steps)
```

Benefits: you design the API from the caller's side, you only write code a test demands, and every line has a test by construction. It's especially effective for algorithmic and business-rule code. It's less natural for exploratory work, UI layout, or heavily framework-driven glue code, where integration tests and fast feedback matter more. You don't have to be a purist: "write tests alongside the code, in small steps" captures most of the value.

---

## 8. Test smells

| Smell | Why it hurts | Fix |
|---|---|---|
| **Fragile tests** (break on any refactoring) | Coupled to implementation, over-mocked | Assert on behavior and state, use fakes |
| **Over-mocking** (everything mocked, nothing real) | The test verifies the mocks, not the code | Use real objects/fakes for in-process collaborators |
| **Huge setup** | The unit has too many dependencies | Simplify the design, use builders |
| **Shared mutable state / order dependence** | Passes alone, fails in a suite (or vice versa) | Fresh state per test, no statics |
| **Conditional logic or loops in tests** | The test may be wrong | Literal expectations, parameterized tests |
| **Assertion-free tests** | Pass without checking anything (and inflate coverage) | Always assert something meaningful |
| **Testing the framework / getters** | Noise | Delete |
| **Flaky tests** (sometimes fail) | Destroys trust and CI value | Find the nondeterminism (time, threads, order, network) and remove it. Don't normalize "re-run" |
| **`Thread.sleep` in tests** | Slow *and* flaky | Latches, `Awaitility`, injected schedulers/clocks |
| **Commented-out or `@Disabled` tests that rot** | Dead weight | Fix or delete |
| **Mystery guest** (data hidden in files/DB) | Can't see why it passes | Keep key data in the test |
| **Giant "god" tests** | Failures are undiagnosable | Split by behavior |

---

## 9. Beyond example-based tests

- **Parameterized tests:** one test body, many inputs/expectations ([JUnit](01_junit-5.md#4-parameterized-tests)). Great for boundary tables.
- **Property-based testing** (for example the jqwik library): state a *property* that must hold for all inputs ("decoding an encoded value returns the original", "sorting is idempotent") and let the framework generate hundreds of inputs and shrink failures to a minimal case ([Testing Patterns](04_testing-patterns.md#8-property-based-testing)).
- **Mutation testing:** checks that your tests would actually notice bugs ([Code Coverage and Mutation Testing](05_code-coverage-and-mutation-testing.md)).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Testing implementation details (private calls, order of internal calls) | Test observable behavior |
| Mocking everything | Fakes/real objects for in-process collaborators. Mock only boundaries and side effects |
| Using the real clock, randomness, or environment | Inject `Clock`/seeded `Random`/config |
| Tests that depend on each other or on execution order | Independent, fresh state |
| Expected values computed with the code's own algorithm | Literal expected values |
| One giant test per class | One behavior per test, descriptive names |
| Chasing 100% coverage with assertion-free tests | Meaningful assertions, mutation testing |
| Ignoring edge cases and error paths | Boundaries, empties, failures |
| Writing code that's untestable, then calling testing "hard" | Inject dependencies, separate logic from I/O |
| Tolerating flaky tests | Eliminate the nondeterminism |
| Slow unit tests (I/O, sleeps) | Move I/O to integration tests, fake time |

### Debugging failing tests

- **Read the assertion message first.** Expected vs actual usually points at the cause. Improve failure messages (AssertJ descriptions) if they don't.
- **Reproduce in isolation:** run the single test. Passes alone but fails in a suite → shared state or order dependence.
- **Fails only on CI** → time zone, locale, charset, file system case sensitivity, concurrency, environment ([CI debugging](../18-build-and-dependencies/03_ci-cd-pipelines.md#debugging-works-locally-fails-in-ci-or-vice-versa)).
- **Intermittent failure** → timing, threads, random data, unordered collections (`HashMap` iteration order), or leaked state.
- **Use the debugger and stack traces** ([Stack Traces and Debugging](../06-exceptions-and-debugging/06_stack-traces-and-debugging.md)).

---

## Quick Summary

- A unit test verifies one behavior quickly and deterministically. Structure it as **Arrange-Act-Assert**, and follow **F.I.R.S.T.** (fast, independent, repeatable, self-validating, timely).
- **Test doubles:** stubs/fakes for inputs, mocks only for outbound side effects. Prefer **state-based** over interaction-based assertions.
- **Design for testability:** inject dependencies (including `Clock`), prefer pure functions, separate logic from I/O, avoid hidden global state.
- Test **behavior, not implementation**. Cover typical, **boundary**, empty/null, invalid, and error cases.
- Keep tests **readable**: descriptive names, literal expectations, no logic, builders for setup, DAMP over DRY.
- Watch for **smells**: over-mocking, shared state, `sleep`, flaky tests, assertion-free tests.
- Use **TDD** where it fits, parameterized and property-based tests for breadth, and mutation testing to check test quality.

**Next:** [JUnit 5 and 6](01_junit-5.md)