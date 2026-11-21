# Unit Testing Fundamentals and Testable Architecture

> The foundational philosophy of software verification: Arrange-Act-Assert, Gerard Meszaros's Test Double Taxonomy, Testable Code Design, and Test Smells.

| Metadata | Details |
|---|---|
| **Level** | Core |
| **Prerequisites** | Java OOP, Exceptions, Interfaces |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **JDK Baseline** | Java 17 / 21 / 25 LTS |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally: Test Runners, ClassLoaders, & Assertions](#3-how-it-works-internally-test-runners-classloaders--assertions)
4. [4. Syntax & Usage](#4-syntax--usage)
5. [5. Practical Examples](#5-practical-examples)
6. [6. Best Practices](#6-best-practices)
7. [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [8. Performance & Security Notes](#8-performance--security-notes)
9. [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [10. Interview Questions](#10-interview-questions)
11. [11. Practice Exercises](#11-practice-exercises)
12. [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [13. Further Reading](#13-further-reading)

---

## 1. What & Why
In professional software engineering, writing code without automated unit tests is technical negligence. Unit testing is the systematic practice of verifying that individual units of source code—typically single classes or methods—operate correctly in total isolation from external infrastructure, databases, network sockets, or file systems.

Good unit tests are not merely regression safety nets; they are living documentation, design feedback instruments, and refactoring enablers. When code is difficult to test, it is almost always due to architectural flaws: tight coupling, hidden state, hardcoded singletons, or violation of the Single Responsibility Principle. Mastering unit testing fundamentals, the AAA pattern, test double taxonomy, and smell diagnosis enables engineers to create trustworthy, millisecond-fast test suites that run continuously.

---

## 2. Core Concepts
- **Unit of Work**: The smallest testable piece of application software. In object-oriented Java, this is typically a method or an aggregate class.
- **Arrange-Act-Assert (AAA)**: The universal structural pattern for unit test design:
  1. *Arrange*: Set up test inputs, mock expectations, and the system under test (SUT).
  2. *Act*: Invoke the target method or operation under verification.
  3. *Assert*: Verify that expected return values or state mutations occurred.
- **Gerard Meszaros's Test Double Taxonomy**:
  - **Dummy**: An object passed around solely to satisfy method parameters (e.g., logger or filler argument), never actually used or inspected.
  - **Stub**: Provides hardcoded canned responses to method calls during the test.
  - **Spy**: Wraps a real object or stub to record runtime interactions (number of calls, arguments passed) for post-execution verification.
  - **Mock**: An object pre-programmed with explicit expectations (which calls must occur in what order) that fails the test if expectations are violated.
  - **Fake**: A working software implementation with a simplified shortcut (e.g., in-memory `ConcurrentHashMap` repository instead of a real SQL database).
- **Test Smells**: Anti-patterns in test code (e.g., Flaky Tests, Mystery Guest, Fragile Test, Mocking the SUT).
- **Fast, Independent, Repeatable, Self-Validating, Timely (FIRST)**: The classic principles of effective unit tests.

---

## 3. How It Works Internally: Test Runners, ClassLoaders, & Assertions

When a build tool executes `mvn test` or `./gradlew test`:

```
+-------------------------------------------------------+
|             Surefire / Gradle Test Runner             |
|  - Spawns isolated worker JVM process                 |
|  - Scans test classpath for @Test annotated classes   |
+-------------------------------------------------------+
                           |
                           v
+-------------------------------------------------------+
|             JUnit Platform / Jupiter Engine           |
|  - Instantiates Test Class (new per method by default)|
|  - Executes @BeforeEach -> @Test method -> @AfterEach |
+-------------------------------------------------------+
                           |
                           v
+-------------------------------------------------------+
|               Assertion Engine (AssertJ)              |
|  - Evaluates expected vs actual boolean condition     |
|  - On failure: Throws org.opentest4j.AssertionFailedError |
|  - Caught by Jupiter engine; logs failure stack trace |
+-------------------------------------------------------+
```

### The Per-Method Instance Lifecycle:
By default, JUnit Jupiter creates a **fresh instance of the test class** before invoking each `@Test` method. This guarantees that instance fields modified in Test A do not leak into Test B, maintaining strict test isolation and preventing ordering dependencies.

---

## 4. Syntax & Usage

### 4.1 Clean Arrange-Act-Assert Structure with AssertJ
```java
package com.example.testing;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import java.math.BigDecimal;
import static org.assertj.core.api.Assertions.*;

@DisplayName("BankAccount State Transitions")
class BankAccountTest {

    private BankAccount account;

    @BeforeEach
    void setUp() {
        // Arrange: Shared fresh instance for every test
        account = new BankAccount("ACC-101", new BigDecimal("100.00"));
    }

    @Test
    @DisplayName("Depositing positive amount increases balance accurately")
    void deposit_positiveAmount_increasesBalance() {
        // Arrange
        BigDecimal depositAmount = new BigDecimal("50.00");

        // Act
        account.deposit(depositAmount);

        // Assert: Fluent, readable assertion via AssertJ
        assertThat(account.getBalance())
                .isEqualByComparingTo(new BigDecimal("150.00"));
    }

    @Test
    @DisplayName("Withdrawing amount exceeding balance throws InsufficientFundsException")
    void withdraw_exceedingBalance_throwsException() {
        // Arrange
        BigDecimal excessiveAmount = new BigDecimal("150.00");

        // Act & Assert combined: Type-safe exception verification
        assertThatThrownBy(() -> account.withdraw(excessiveAmount))
                .isInstanceOf(InsufficientFundsException.class)
                .hasMessageContaining("Insufficient balance")
                .hasMessageContaining("ACC-101");

        // Verify balance remained unchanged
        assertThat(account.getBalance())
                .isEqualByComparingTo(new BigDecimal("100.00"));
    }
}
```

---

## 5. Practical Examples

### Example 1: Hand-Crafted In-Memory Fake Repository
```java
package com.example.testing;

import java.util.*;
import java.util.concurrent.ConcurrentHashMap;

// A Fake implementation: Fully functional in-memory storage, zero database overhead!
public final class InMemoryUserRepository implements UserRepository {
    private final Map<Long, User> storage = new ConcurrentHashMap<>();
    private long idSequence = 1;

    @Override
    public Optional<User> findById(long id) {
        return Optional.ofNullable(storage.get(id));
    }

    @Override
    public User save(User user) {
        long id = user.id() <= 0 ? idSequence++ : user.id();
        User saved = new User(id, user.email(), user.name());
        storage.put(id, saved);
        return saved;
    }

    @Override
    public void deleteById(long id) {
        storage.remove(id);
    }
}
```

### Example 2: Testing Business Logic with Fake instead of Mock
```java
package com.example.testing;

import org.junit.jupiter.api.Test;
import static org.assertj.core.api.Assertions.*;

class RegistrationServiceTest {

    @Test
    void registerUser_validRequest_persistsUserInRepository() {
        // Arrange: Using in-memory fake instead of heavy Mockito mocks!
        UserRepository fakeRepo = new InMemoryUserRepository();
        RegistrationService service = new RegistrationService(fakeRepo);

        // Act
        User registered = service.register("engineer@example.com", "Alice");

        // Assert
        assertThat(registered.id()).isPositive();
        assertThat(fakeRepo.findById(registered.id()))
                .isPresent()
                .hasValueSatisfying(u -> assertThat(u.email()).isEqualTo("engineer@example.com"));
    }
}
```

### Example 3: Edge Case - Testing Floating Point Precision and Rounding
```java
package com.example.testing;

import org.junit.jupiter.api.Test;
import static org.assertj.core.api.Assertions.*;

class FinancialCalculatorTest {
    @Test
    void calculateTax_withRounding_matchesFinancialPrecision() {
        double subtotal = 19.99;
        double taxRate = 0.0825; // 8.25%

        double tax = FinancialCalculator.computeTax(subtotal, taxRate);

        // In floating-point tests, ALWAYS provide an explicit offset/epsilon tolerance!
        assertThat(tax).isCloseTo(1.65, within(0.001));
    }
}
```

### Example 4: Failure Case - The "Mystery Guest" Anti-Pattern
```java
// ANTI-PATTERN: Test relies on external file "test-data.json" on disk.
// Readers cannot see inputs in test code; if file is moved or modified, test breaks mysteriously!
@Test
void brokenMysteryGuestTest() {
    // Hidden dependency: Where did test-data.json come from? What are its contents?
    Order order = DataLoader.loadOrderFromFile("test-data.json");
    assertThat(order.getTotal()).isEqualTo(150.0);
}
```

---

## 6. Best Practices
1. **Follow the Arrange-Act-Assert Pattern Strictly**: Separate each test logically into three distinct visual blocks with single-line whitespace.
2. **One Logical Assertion Concept per Test**: A test should verify one specific business rule or outcome; multiple assertions are acceptable only if they verify aspects of the same single state transition.
3. **Use Descriptive Test Names**: Name tests after business behavior: `methodUnderTest_stateUnderTest_expectedBehavior` or use JUnit 5 `@DisplayName`.
4. **Prefer In-Memory Fakes over Deep Mocking**: When testing business services, in-memory fakes provide higher fidelity and resilience against refactoring compared to brittle mock method verifications.
5. **Never Sleep in Unit Tests**: Never use `Thread.sleep()` to wait for asynchronous behavior; use Awaitility or explicit latch synchronizers.
6. **Eliminate Test Interdependence**: Tests must be 100% independent; test suite execution order should be completely randomized without affecting pass/fail outcomes.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
|---|---|---|
| Sharing mutable state across tests | Test A pollutes state, causing Test B to fail intermittently | Use `@BeforeEach` or immutable fakes |
| Using `Thread.sleep()` for async tests | Introduces flaky failures on slow CI runners | Use Awaitility or `CountDownLatch` |
| Mocking everything blindly | Tests verify mock wiring rather than real business behavior | Use real objects or in-memory Fakes |
| Testing private methods directly | Couples test to implementation details, breaking refactoring | Test public API behavior; private methods are verified transitively |
| Comparing floats with `==` | Floating point representation introduces tiny precision errors | Use `isCloseTo(expected, within(epsilon))` |
| Asserting on exception without type check | Passes if completely unrelated exception is thrown | Use `assertThatThrownBy().isInstanceOf(...)` |
| "The Liar" test smell | Test passes but contains zero assertions | Ensure every test contains at least one meaningful assertion |
| Huge multi-screen test setup | Indicates class violates Single Responsibility Principle | Refactor production class into smaller components |

---

## 8. Performance & Security Notes
- **Sub-Second Execution Target**: A healthy unit test suite should execute 1,000 tests in under 2 seconds. Any individual unit test taking $> 50	ext{ ms}$ is either doing I/O or indicates architectural coupling.
- **Randomized Execution Order**: Configure `junit.jupiter.testclass.order.default = org.junit.jupiter.api.ClassOrderer$Random` to expose hidden cross-test state pollution immediately.
- **Security in Test Data**: Never use production credentials, real API tokens, or real customer Personally Identifiable Information (PII) in unit test fixture files.

---

## 9. Comparison / When to Use What

| Test Double Type | Has Implementation? | Returns Canned Data? | Records Calls? | Best Use Case |
|---|---|---|---|---|
| **Dummy** | No | No | No | Parameter filler (e.g., unused logger) |
| **Stub** | Partial | Yes (Fixed) | No | Providing fixed query data (e.g., pricing table) |
| **Spy** | Real or partial | Optional | Yes | Verifying method call counts on real object |
| **Mock** | Dynamic proxy | Yes | Yes (Strict) | Verifying mandatory interaction contracts |
| **Fake** | Yes (Simplified) | Dynamic | No | In-memory repositories, simulated auth services |

---

## 10. Interview Questions

<details>
<summary>1. What are the five types of Test Doubles defined in Gerard Meszaros's taxonomy?</summary>
1. <b>Dummy</b>: Objects passed to satisfy signatures, never actually used.<br>
2. <b>Stub</b>: Provides hardcoded canned responses to method calls.<br>
3. <b>Spy</b>: A stub or real wrapper that records invocations and arguments for later inspection.<br>
4. <b>Mock</b>: Pre-programmed with expected calls; fails execution if expectations are not met.<br>
5. <b>Fake</b>: A working implementation with a simplified in-memory shortcut (e.g., H2 or ConcurrentHashMap).
</details>

<details>
<summary>2. Why does JUnit Jupiter instantiate a new test class instance for every @Test method?</summary>
To enforce strict test isolation. If a single instance were shared across all tests, any modification to instance fields made by Test A would persist into Test B. Re-instantiating the class guarantees a pristine, empty heap environment for every test, allowing tests to run in parallel or in random order without state leakage.
</details>

<details>
<summary>3. Why is testing private methods directly considered an architectural anti-pattern?</summary>
Private methods represent internal implementation details, not contracts. Tests should verify public behavior (the <i>what</i>, not the <i>how</i>). Testing private methods couples the test suite to internal code structure; any internal refactoring breaks the test suite even if the public behavior remains 100% correct. If a private method is so complex that it demands isolated testing, it should be extracted into a separate, focused class.
</details>

<details>
<summary>4. What is a "Flaky Test" and what are its primary root causes?</summary>
A flaky test passes or fails nondeterministically without any code changes. Primary root causes include:
- Relying on system clock or timezones (`new Date()`).
- Using fixed `Thread.sleep()` rather than event-driven synchronizers.
- Shared mutable static state across tests.
- Undocumented test execution order dependencies.
- Network or file system I/O inside unit tests.
</details>

<details>
<summary>5. What are the key differences between state-based verification and interaction-based verification?</summary>
<b>State-Based Verification</b>: Verifies the final state of the system or return value after an action (e.g., checking that account balance is 150). Resilient to refactoring.<br>
<b>Interaction-Based Verification</b>: Verifies which methods were called on collaborating dependencies with what arguments (e.g., verifying `auditLogger.log()` was invoked once). Prone to brittleness if internal calls change.
</details>

<details>
<summary>6. What is the Arrange-Act-Assert (AAA) pattern and how does it map to Given-When-Then?</summary>
Arrange maps to <b>Given</b> (pre-conditions and inputs). Act maps to <b>When</b> (the behavior or command under test). Assert maps to <b>Then</b> (verifying expected outcomes and observable side effects). Both patterns enforce clean separation between setup, execution, and verification.
</details>

<details>
<summary>7. What is the "Mystery Guest" test smell?</summary>
The Mystery Guest smell occurs when a test relies on external data sources (e.g., a file on disk or database pre-loaded data) that is not visible inside the test method. When reading the test, the engineer cannot understand why certain assertions are expected without hunting down external files.
</details>

<details>
<summary>8. How do you design classes to make them easily testable without mocks?</summary>
Apply Dependency Injection: accept dependencies via constructors rather than instantiating them internally (`new`). Program to interfaces so in-memory fakes can be substituted. Separate pure domain logic (calculations, rules) from I/O infrastructure (databases, HTTP).
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Write a test verifying that an Order accurately calculates discounts using AssertJ.</summary>

```java
@Test
void calculateTotal_withTenPercentDiscount_appliesDiscountAccurately() {
    // Arrange
    Order order = new Order("ORD-1", new BigDecimal("100.00"));
    DiscountPolicy tenPercent = new PercentageDiscountPolicy(10);

    // Act
    BigDecimal finalTotal = order.applyDiscount(tenPercent);

    // Assert
    assertThat(finalTotal).isEqualByComparingTo(new BigDecimal("90.00"));
}
```
</details>

<details>
<summary>Exercise 2: Implement an in-memory FakeClock to test time-dependent expiration logic deterministically.</summary>

```java
public final class FakeClock extends Clock {
    private Instant currentInstant;
    private final ZoneId zone;

    public FakeClock(Instant initial) {
        this.currentInstant = initial;
        this.zone = ZoneId.of("UTC");
    }

    public void advanceTime(Duration duration) {
        this.currentInstant = this.currentInstant.plus(duration);
    }

    @Override public ZoneId getZone() { return zone; }
    @Override public Clock withZone(ZoneId zone) { return this; }
    @Override public Instant instant() { return currentInstant; }
}
```
</details>

<details>
<summary>Exercise 3: Write a test verifying that expired authentication tokens are rejected.</summary>

```java
@Test
void isTokenExpired_afterOneHour_returnsTrue() {
    // Arrange
    Instant now = Instant.parse("2026-11-20T12:00:00Z");
    FakeClock clock = new FakeClock(now);
    AuthToken token = new AuthToken("secret", now.plus(Duration.ofMinutes(30)));

    // Act: Advance clock by 31 minutes
    clock.advanceTime(Duration.ofMinutes(31));

    // Assert
    assertThat(token.isExpired(clock.instant())).isTrue();
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Structure**: Arrange $ightarrow$ Act $ightarrow$ Assert (AAA) with clear visual separation.
- **Test Doubles**: Dummy (filler), Stub (canned data), Spy (recorder), Mock (expectations), Fake (in-memory shortcut).
- **Isolation**: JUnit creates a fresh test class instance per method; never share mutable static state.
- **AssertJ**: Use fluent, readable assertions (`assertThat(x).isEqualByComparingTo(y)`).
- **Fast Feedback**: Unit tests must run in milliseconds with zero disk/network I/O.
- **Smells**: Eliminate Flaky Tests, Mystery Guests, and testing private methods directly.

---

## 13. Further Reading
- *xUnit Test Patterns: Refactoring Test Code* by Gerard Meszaros
- *Growing Object-Oriented Software, Guided by Tests* by Steve Freeman and Nat Pryce
- AssertJ Core Documentation and Best Practices
- Martin Fowler: Mocks Aren't Stubs
