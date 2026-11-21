# JUnit 5 and 6: Architecture, Lifecycle, and Parameterized Tests

> The modern standard testing framework: JUnit Platform, Jupiter Engine, Lifecycle Hooks, Nested Tests, Parameterized Suites, and Parallel Execution.

| Metadata | Details |
|---|---|
| **Level** | Core / Intermediate |
| **Prerequisites** | Java Unit Testing Fundamentals, Annotations |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **JDK Baseline** | Java 17 / 21 / 25 LTS (JUnit 5.10+ / 6.0) |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally: Platform, Jupiter, & TestEngine SPI](#3-how-it-works-internally-platform-jupiter--testengine-spi)
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
For over a decade, JUnit 4 was the ubiquitous testing framework of Java. However, JUnit 4 suffered from severe architectural limitations: a monolithic codebase coupling the test runner directly to IDEs and build tools, rigid inheritance-based extension rules (`@Rule` and `@ClassRule` confusion), and zero support for modern Java 8+ features like lambdas and stream-based parameterization.

**JUnit 5 (and JUnit 6)** completely redesigned the framework from the ground up. It introduced a decoupled three-tier architecture, an extensible `Extension` model, hierarchical `@Nested` test contexts, powerful `@ParameterizedTest` capabilities, and native support for concurrent, multi-threaded test execution. Understanding JUnit 5/6 architecture is fundamental for authoring expressive, modular, and lightning-fast test suites.

---

## 2. Core Concepts
- **Three-Tier Architecture**:
  1. `JUnit Platform`: Foundation for launching testing frameworks on the JVM. Defines the `TestEngine` API used by build tools and IDEs.
  2. `JUnit Jupiter`: The modern programming and extension model containing annotations (`@Test`, `@BeforeEach`, `@ParameterizedTest`).
  3. `JUnit Vintage`: Legacy test engine providing backward compatibility to run JUnit 3 and JUnit 4 tests on the modern Platform.
- **Lifecycle Annotations**:
  - `@BeforeAll` / `@AfterAll`: Static setup/teardown methods executed once per test class.
  - `@BeforeEach` / `@AfterEach`: Executed before and after every individual test method.
- **Hierarchical Tests (`@Nested`)**: Expressive, grouped test suites modeling state trees and behavioral specifications.
- **Parameterized Tests (`@ParameterizedTest`)**: Running the same test logic across multiple input datasets (`@ValueSource`, `@CsvSource`, `@MethodSource`).
- **Dynamic Tests (`@TestFactory`)**: Generating tests programmatically at runtime using Java Streams.
- **Extension Model (`@ExtendWith`)**: Replaces JUnit 4's brittle Runners and Rules with composable lifecycle callback interfaces (`BeforeEachCallback`, `ParameterResolver`).

---

## 3. How It Works Internally: Platform, Jupiter, & TestEngine SPI

JUnit 5 decouples test discovery and execution from IDEs and build tools via the Service Provider Interface:

```
+-------------------------------------------------------+
|             Build Tools & IDEs (Maven, Gradle, IntelliJ) |
+-------------------------------------------------------+
                           |
                           v
+-------------------------------------------------------+
|                    JUnit Platform                     |
|           LauncherDiscoveryRequestBuilder             |
+-------------------------------------------------------+
                           |
                TestEngine Discovery (SPI)
        (META-INF/services/org.junit.platform.engine.TestEngine)
                           |
            +--------------+--------------+
            |                             |
            v                             v
+-----------------------+     +-----------------------+
|  junit-jupiter-engine |     |  junit-vintage-engine |
| (Modern JUnit 5/6)    |     | (Legacy JUnit 3/4)    |
+-----------------------+     +-----------------------+
```

### The Extension Pipeline Lifecycle:
When a test method executes, Jupiter invokes registered extensions in deterministic order:
1. `BeforeAllCallback` $ightarrow$ `@BeforeAll`
2. `BeforeEachCallback` $ightarrow$ `@BeforeEach`
3. `BeforeTestExecutionCallback`
4. `@Test` method invocation (with `ParameterResolver` injecting arguments)
5. `AfterTestExecutionCallback`
6. `@AfterEach` $ightarrow$ `AfterEachCallback`
7. `@AfterAll` $ightarrow$ `AfterAllCallback`

---

## 4. Syntax & Usage

### 4.1 Hierarchical Nested Test Suite
```java
package com.example.testing;

import org.junit.jupiter.api.*;
import java.util.EmptyStackException;
import java.util.Stack;
import static org.assertj.core.api.Assertions.*;

@DisplayName("A Stack")
class StackTest {

    private Stack<String> stack;

    @Nested
    @DisplayName("when newly instantiated")
    class WhenNew {

        @BeforeEach
        void init() {
            stack = new Stack<>();
        }

        @Test
        @DisplayName("is empty")
        void isEmpty() {
            assertThat(stack.isEmpty()).isTrue();
        }

        @Test
        @DisplayName("throws EmptyStackException on pop")
        void throwsOnPop() {
            assertThatThrownBy(stack::pop)
                    .isInstanceOf(EmptyStackException.class);
        }

        @Nested
        @DisplayName("after pushing an element")
        class AfterPushing {
            private final String element = "Java 21";

            @BeforeEach
            void pushElement() {
                stack.push(element);
            }

            @Test
            @DisplayName("is no longer empty")
            void isNotEmpty() {
                assertThat(stack.isEmpty()).isFalse();
            }

            @Test
            @DisplayName("returns the element on peek")
            void peekReturnsElement() {
                assertThat(stack.peek()).isEqualTo(element);
            }
        }
    }
}
```

### 4.2 Parameterized Tests with Diverse Sources
```java
package com.example.testing;

import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.*;
import java.util.stream.Stream;
import static org.assertj.core.api.Assertions.*;

class ValidationTest {

    // 1. ValueSource for single primitive or string arguments
    @ParameterizedTest(name = "Validating username: {0}")
    @ValueSource(strings = {"admin", "developer", "qa_engineer"})
    void username_validStrings_returnsTrue(String username) {
        assertThat(Validator.isValidUsername(username)).isTrue();
    }

    // 2. CsvSource for multi-argument test matrices
    @ParameterizedTest(name = "{0} + {1} should equal {2}")
    @CsvSource({
        "1,   2,   3",
        "10,  20,  30",
        "-5,  15,  10"
    })
    void add_variousInputs_computesSum(int a, int b, int expectedSum) {
        assertThat(Calculator.add(a, b)).isEqualTo(expectedSum);
    }

    // 3. MethodSource for complex objects and dynamic Streams
    @ParameterizedTest
    @MethodSource("provideDiscountScenarios")
    void discountPolicy_computesCorrectRate(CustomerType type, double spend, double expectedRate) {
        assertThat(DiscountPolicy.getRate(type, spend)).isEqualTo(expectedRate);
    }

    static Stream<Arguments> provideDiscountScenarios() {
        return Stream.of(
            Arguments.of(CustomerType.STANDARD, 50.0, 0.0),
            Arguments.of(CustomerType.VIP, 50.0, 0.10),
            Arguments.of(CustomerType.VIP, 500.0, 0.20)
        );
    }
}
```

---

## 5. Practical Examples

### Example 1: Custom Benchmark Execution Extension
```java
package com.example.testing;

import org.junit.jupiter.api.extension.*;

public final class TimingExtension implements BeforeTestExecutionCallback, AfterTestExecutionCallback {
    private static final ExtensionContext.Namespace NAMESPACE = ExtensionContext.Namespace.create("TimingExtension");

    @Override
    public void beforeTestExecution(ExtensionContext context) {
        context.getStore(NAMESPACE).put(context.getRequiredTestMethod(), System.nanoTime());
    }

    @Override
    public void afterTestExecution(ExtensionContext context) {
        long start = context.getStore(NAMESPACE).remove(context.getRequiredTestMethod(), long.class);
        long durationMs = (System.nanoTime() - start) / 1_000_000;
        System.out.printf("[BENCHMARK] Test %s completed in %d ms%n", context.getDisplayName(), durationMs);
    }
}

// Usage in test class:
// @ExtendWith(TimingExtension.class)
// class MyFastTest { ... }
```

### Example 2: Concurrent Parallel Test Execution Configuration
```properties
# src/test/resources/junit-platform.properties
# Enable parallel test execution across CPU cores
junit.jupiter.execution.parallel.enabled=true
junit.jupiter.execution.parallel.mode.default=concurrent
junit.jupiter.execution.parallel.mode.classes.default=concurrent
junit.jupiter.execution.parallel.config.strategy=dynamic
junit.jupiter.execution.parallel.config.dynamic.factor=1.0
```

### Example 3: Edge Case - Synchronizing Shared Resources in Parallel Tests
```java
package com.example.testing;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.parallel.ResourceAccessMode;
import org.junit.jupiter.api.parallel.ResourceLock;
import org.junit.jupiter.api.parallel.Resources;

class SystemPropertyTest {
    @Test
    @ResourceLock(value = Resources.SYSTEM_PROPERTIES, mode = ResourceAccessMode.READ_WRITE)
    void modifySystemProperty() {
        System.setProperty("app.env", "staging");
        // Safe from concurrent interference from other parallel tests!
    }
}
```

---

## 6. Best Practices
1. **Use `@Nested` to Model State Trees**: Group tests logically by context (`whenEmpty`, `afterLoggedIn`) to produce expressive, self-documenting test reports.
2. **Leverage `@MethodSource` for Complex Matrix Tests**: Avoid duplicating test logic across dozens of test methods; parameterize inputs via Streams.
3. **Use Composable Extensions instead of Base Test Classes**: Favor `@ExtendWith` over deep class inheritance hierarchies (`abstract class BaseIntegrationTest`).
4. **Isolate Parallel Tests with `@ResourceLock`**: When tests must touch shared resources (e.g., system properties, shared files), declare explicit resource locks to prevent concurrency race conditions.
5. **Use AssertJ `assertAll` or Soft Assertions**: Group independent assertions so that a failure in assertion 1 does not suppress reporting for assertion 2.
6. **Set `@TestInstance(Lifecycle.PER_CLASS)` Only When Truly Necessary**: Keep default `PER_METHOD` lifecycle to guarantee complete test isolation.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
|---|---|---|
| Non-static `@BeforeAll` without `PER_CLASS` | Jupiter throws exception because instance does not exist yet | Make method `static` or set `@TestInstance(Lifecycle.PER_CLASS)` |
| Forgetting to enable parallel execution in properties | Tests execute sequentially despite `@Execution(CONCURRENT)` | Set `junit.jupiter.execution.parallel.enabled=true` |
| MethodSource method not static | Jupiter cannot invoke method without an existing instance | Make `@MethodSource` factory methods `static` |
| Modifying shared static variables in parallel tests | Causes intermittent, non-reproducible test failures | Use `@ResourceLock` or avoid shared mutable state |
| Mixing JUnit 4 and JUnit 5 annotations | `@org.junit.Test` ignored by Jupiter engine without Vintage | Ensure imports use `org.junit.jupiter.api.*` |
| Swallowing assertions in async threads | Assertion error thrown on worker thread is missed by test runner | Use CompletableFuture assertions or Awaitility |
| Hardcoding machine-dependent paths | Tests fail when executed on CI runners or Windows/Linux | Use `@TempDir` to generate temporary test directories |
| Huge multi-parameter `@CsvSource` matrices | Hard to read and debug when tests fail | Extract into structured `@MethodSource` with named arguments |

---

## 8. Performance & Security Notes
- **Dynamic Parallel Forking**: In multi-core CPU architectures, enabling parallel execution cuts test suite duration by up to 70%. Ensure test isolation is verified before enabling globally.
- **Temporary Directory Injection (`@TempDir`)**: Using `@TempDir Path tempDir` ensures Jupiter creates an isolated temporary directory and automatically deletes all files upon test conclusion, preventing disk bloat and file lock issues.
- **Extension Store Isolation**: Extensions share state via `ExtensionContext.Store`. Scoping keys to `context.getRequiredTestMethod()` prevents state leakage across test methods.

---

## 9. Comparison / When to Use What

| Feature | JUnit 4 | JUnit 5 / 6 (Jupiter) |
|---|---|---|
| **Architecture** | Monolithic (`junit.jar`) | Decoupled (Platform, Jupiter, Vintage) |
| **Java Baseline** | Java 5+ | Java 8+ (JUnit 5) / Java 17+ (JUnit 6) |
| **Extension Mechanism** | `@Rule`, `@ClassRule`, `@RunWith` | `@ExtendWith` (Unified Callback SPI) |
| **Nested Contexts** | Not natively supported | Native `@Nested` inner classes |
| **Parameterized Tests** | Brittle (`@RunWith(Parameterized.class)`) | Native `@ParameterizedTest` with multiple sources |
| **Parallel Execution** | Experimental / Brittle | Native, First-Class (`junit-platform.properties`) |

---

## 10. Interview Questions

<details>
<summary>1. Describe the three major components of the JUnit 5 architecture.</summary>
1. <b>JUnit Platform</b>: The underlying foundation and test-launching API. It defines the <code>TestEngine</code> SPI that build tools (Maven, Gradle) and IDEs (IntelliJ, Eclipse) connect to for test discovery and reporting.<br>
2. <b>JUnit Jupiter</b>: The modern programming and extension model. Contains modern annotations (<code>@Test</code>, <code>@Nested</code>, <code>@ExtendWith</code>) and the <code>junit-jupiter-engine</code> implementation.<br>
3. <b>JUnit Vintage</b>: A backward-compatibility engine that runs JUnit 3 and JUnit 4 tests on the modern JUnit Platform, allowing gradual codebase migration.
</details>

<details>
<summary>2. What is the difference between @TestInstance(Lifecycle.PER_METHOD) and Lifecycle.PER_CLASS?</summary>
By default (<code>PER_METHOD</code>), Jupiter instantiates a brand-new instance of the test class before running every single <code>@Test</code> method, guaranteeing total isolation between tests. Consequently, <code>@BeforeAll</code> and <code>@AfterAll</code> methods must be <code>static</code>.<br>
With <code>PER_CLASS</code>, Jupiter creates exactly one instance of the test class and executes all test methods on that single instance. This allows non-static <code>@BeforeAll</code> methods, but requires careful cleanup of mutable instance state.
</details>

<details>
<summary>3. How does the JUnit 5 Extension model improve upon JUnit 4 Rules and Runners?</summary>
In JUnit 4, a test class could only declare a single <code>@RunWith</code> runner, creating severe friction when combining frameworks (e.g., Spring Runner vs Mockito Runner). JUnit 5 eliminates runners and rules in favor of a single unified <code>Extension</code> model. Multiple extensions can be composed on the same class using <code>@ExtendWith</code>, and extensions implement specific granular lifecycle callback interfaces.
</details>

<details>
<summary>4. How does @MethodSource provide dynamic arguments to @ParameterizedTest?</summary>
<code>@MethodSource</code> references a factory method (by name) that returns a <code>Stream</code>, <code>Iterable</code>, or array of arguments. If the test accepts multiple parameters, the factory method emits <code>Arguments.of(arg1, arg2, arg3)</code>. The factory method must be <code>static</code> unless <code>@TestInstance(Lifecycle.PER_CLASS)</code> is active.
</details>

<details>
<summary>5. How does JUnit 5 support safe parallel test execution with shared resources?</summary>
JUnit 5 provides the <code>@ResourceLock</code> annotation. Tests that read or mutate shared global resources (such as system properties, static state, or singletons) declare a resource key and lock mode (<code>ResourceAccessMode.READ</code> or <code>READ_WRITE</code>). Jupiter uses read-write lock semantics to run concurrent reads in parallel while serializing write operations.
</details>

<details>
<summary>6. What is the purpose of @TempDir in JUnit 5?</summary>
<code>@TempDir</code> is a built-in parameter resolver and field injector that creates an isolated temporary directory on the host filesystem before test execution, injects it as a <code>Path</code> or <code>File</code>, and guarantees recursive deletion after the test concludes, eliminating manual directory creation and cleanup code.
</details>

<details>
<summary>7. What are Dynamic Tests (@TestFactory) in JUnit 5?</summary>
Unlike standard <code>@Test</code> methods which are defined statically at compile time, a method annotated with <code>@TestFactory</code> generates test cases dynamically at runtime. It returns a <code>Stream<DynamicNode></code> (using <code>DynamicTest.dynamicTest()</code>), allowing tests to be generated from external configuration files, database tables, or dynamic data streams.
</details>

<details>
<summary>8. How do you assert that multiple conditions are checked even if the first condition fails?</summary>
In standard assertions, the first failure aborts the test, hiding subsequent failures. In JUnit 5, use <code>assertAll()</code>, which accepts an array of executable lambdas and executes all of them, reporting all accumulated assertion failures simultaneously. In AssertJ, use <code>SoftAssertions</code>.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Create a Parameterized test testing an email validator against a CSV dataset.</summary>

```java
@ParameterizedTest(name = "Email "{0}" validity should be {1}")
@CsvSource({
    "user@example.com,    true",
    "john.doe@sub.org,    true",
    "invalid-email,       false",
    "@missing-user.com,   false",
    "spaces in@mail.com,  false"
})
void emailValidation_variousInputs_matchesExpectedResult(String email, boolean expectedValid) {
    assertThat(EmailValidator.isValid(email)).isEqualTo(expectedValid);
}
```
</details>

<details>
<summary>Exercise 2: Write a custom Extension resolving an authenticated test User Session.</summary>

```java
public final class UserSessionParameterResolver implements ParameterResolver {
    @Override
    public boolean supportsParameter(ParameterContext parameterContext, ExtensionContext extensionContext) {
        return parameterContext.getParameter().getType().equals(UserSession.class);
    }

    @Override
    public Object resolveParameter(ParameterContext parameterContext, ExtensionContext extensionContext) {
        return new UserSession(101L, "test_admin", Set.of("ROLE_ADMIN"));
    }
}

// In test class:
// @ExtendWith(UserSessionParameterResolver.class)
// @Test void mySecuredTest(UserSession session) { ... }
```
</details>

<details>
<summary>Exercise 3: Write an assertAll block verifying multiple properties of a generated Order.</summary>

```java
@Test
void createOrder_validInput_setsAllPropertiesCorrectly() {
    Order order = OrderService.createOrder("CUST-1", 150.00);

    org.junit.jupiter.api.Assertions.assertAll("Order State Validation",
        () -> assertThat(order.getId()).isNotNull(),
        () -> assertThat(order.getCustomerId()).isEqualTo("CUST-1"),
        () -> assertThat(order.getTotalAmount()).isEqualTo(150.00),
        () -> assertThat(order.getStatus()).isEqualTo(OrderStatus.NEW)
    );
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Architecture**: JUnit Platform (SPI) $ightarrow$ Jupiter (Modern) + Vintage (Legacy).
- **Hierarchy**: Group contexts with `@Nested` to model state branches.
- **Parameterization**: `@ParameterizedTest` with `@ValueSource`, `@CsvSource`, `@MethodSource`.
- **Parallelism**: Enable `junit.jupiter.execution.parallel.enabled=true`; guard with `@ResourceLock`.
- **Extensions**: Implement `BeforeEachCallback`, `ParameterResolver`, compose via `@ExtendWith`.
- **Soft Assertions**: Group checks with `assertAll()` to inspect all errors at once.

---

## 13. Further Reading
- JUnit 5 User Guide (Official Documentation)
- JUnit 6 Release Notes and Migration Guide
- AssertJ Core Fluent Assertions Documentation
- Baeldung: Guide to JUnit 5 Extensions and Parameterized Tests
