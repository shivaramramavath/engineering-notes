# JUnit 5 and 6

**JUnit** is the standard testing framework for Java. Its modern programming model, **JUnit Jupiter**, arrived with JUnit 5 (2017) and is carried forward unchanged in **JUnit 6** (September 2025). If you know one, you know the other: the packages are still `org.junit.jupiter.*`, the annotations are the same, and the main difference is the baseline and version numbering ([section 1](#1-versions-and-setup)). This note uses "Jupiter" for the shared model.

**Prerequisites:** [Unit Testing Fundamentals](00_unit-testing-fundamentals.md), [Annotations](../13-advanced-language-features/00_annotations.md), [Lambda Expressions](../09-functional-java/00_lambda-expressions.md).

---

## 1. Versions and setup

JUnit has three parts:

| Part | Role |
|---|---|
| **JUnit Platform** | Launches test engines. IDEs and build tools talk to it |
| **JUnit Jupiter** | The modern annotations/API **and** the engine that runs them |
| **JUnit Vintage** | An engine that runs old JUnit 3/4 tests (deprecated in JUnit 6; use only while migrating) |

| | JUnit 5.x | JUnit 6.x |
|---|---|---|
| Java baseline | 8 | **17** |
| Version numbers | Platform 1.x, Jupiter 5.x, Vintage 5.x | **One version** for everything (6.x) |
| Packages/imports | `org.junit.jupiter.api.*` | **Unchanged** |
| Migration | | Mostly a version bump (if you're on Java 17+ and not using removed deprecated APIs) |

Choose by your Java baseline: **Java 17+ → JUnit 6**; older → JUnit 5.x. Spring Boot 4 manages JUnit 6. JUnit 6 also adds null-safety annotations (JSpecify), Kotlin coroutine support, Java Flight Recorder integration in the launcher, and a fail-fast option for the console launcher. See the official upgrade guide for the full list of removed APIs.

### Maven

```xml
<dependencyManagement>
  <dependencies>
    <dependency>
      <groupId>org.junit</groupId>
      <artifactId>junit-bom</artifactId>
      <version>6.x.y</version>            <!-- the current release; use the BOM so all JUnit artifacts match -->
      <type>pom</type>
      <scope>import</scope>
    </dependency>
  </dependencies>
</dependencyManagement>

<dependencies>
  <dependency>
    <groupId>org.junit.jupiter</groupId>
    <artifactId>junit-jupiter</artifactId>       <!-- API + params + engine -->
    <scope>test</scope>
  </dependency>
</dependencies>
```

Use a current `maven-surefire-plugin` (3.x), which runs the JUnit Platform natively.

### Gradle

```kotlin
dependencies {
    testImplementation(platform("org.junit:junit-bom:6.x.y"))
    testImplementation("org.junit.jupiter:junit-jupiter")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")   // required by Gradle 9
}
tasks.test { useJUnitPlatform() }
```

Mixing artifact versions (for example Jupiter 5 with Platform 6) causes `NoSuchMethodError`s, so always import the BOM ([Dependency Management](../18-build-and-dependencies/02_dependency-management.md#2-conflicts-how-they-are-resolved)).

---

## 2. Writing tests

```java
import org.junit.jupiter.api.*;
import static org.junit.jupiter.api.Assertions.*;

class AccountTest {

    private Account account;

    @BeforeAll
    static void initAll() { /* once before all tests: expensive shared setup */ }

    @BeforeEach
    void setUp() { account = new Account("A-1", new BigDecimal("100.00")); }       // fresh state per test

    @Test
    @DisplayName("withdrawing reduces the balance")
    void withdrawReducesBalance() {
        account.withdraw(new BigDecimal("30.00"));
        assertEquals(new BigDecimal("70.00"), account.balance());
    }

    @Test
    void withdrawingMoreThanBalanceFails() {
        var ex = assertThrows(InsufficientFundsException.class,
                              () -> account.withdraw(new BigDecimal("500.00")));
        assertTrue(ex.getMessage().contains("A-1"));
    }

    @AfterEach
    void tearDown() { /* release per-test resources */ }

    @AfterAll
    static void tearDownAll() { /* once after all tests */ }
}
```

Rules:

- Test classes and methods **don't need to be `public`**: package-private is idiomatic. They must **not** be `private` or `static`.
- **JUnit creates a new instance of the test class for every test method** (the default `PER_METHOD` lifecycle), so instance fields aren't shared between tests. That's what gives you isolation.
- `@BeforeAll`/`@AfterAll` must be **`static`**, unless you use `@TestInstance(TestInstance.Lifecycle.PER_CLASS)` (one instance for all tests, which lets them be non-static but makes shared state your responsibility).
- Order of tests is **deterministic but not obvious**. Never depend on it. (`@TestMethodOrder`/`@Order` exist, and mostly signal a design problem.)
- `@Disabled("reason")` skips a test. Always give a reason, and remove stale ones.

---

## 3. Assertions

### JUnit's built-in assertions

```java
assertEquals(expected, actual);                    // EXPECTED FIRST, actual second
assertEquals(3.14, result, 0.001);                 // doubles need a tolerance
assertNotEquals(a, b);
assertTrue(list.isEmpty());   assertFalse(flag);
assertNull(x);   assertNotNull(x);
assertSame(a, b);                                   // reference equality
assertArrayEquals(new int[]{1, 2}, actual);
assertIterableEquals(List.of(1, 2), actual);
assertInstanceOf(Circle.class, shape);              // returns the cast value

assertTrue(balance.signum() >= 0, "balance went negative: " + balance);       // message LAST
assertTrue(cond, () -> "built only on failure: " + expensiveDescription());    // lazy message

assertAll("customer",                                                         // run ALL, report every failure
        () -> assertEquals("Asha", c.name()),
        () -> assertEquals("asha@example.com", c.email()));

IllegalArgumentException ex = assertThrows(IllegalArgumentException.class, () -> parse("x"));   // exception + inspect it
assertDoesNotThrow(() -> parse("42"));
assertTimeout(Duration.ofMillis(200), () -> compute());     // fails if it takes too long (still runs to completion)
```

### AssertJ: fluent assertions (recommended companion)

AssertJ reads like a sentence, has rich assertions for collections, strings, optionals, and exceptions, and produces much better failure messages:

```java
import static org.assertj.core.api.Assertions.*;

assertThat(order.total()).isEqualByComparingTo("118.00");                    // BigDecimal ignoring scale
assertThat(users).extracting(User::name).containsExactlyInAnyOrder("asha", "ravi");
assertThat(name).isNotBlank().startsWith("A").hasSizeLessThan(50);
assertThat(result).isPresent().get().extracting(Order::id).isEqualTo(42L);

assertThatThrownBy(() -> service.cancel(99))
        .isInstanceOf(OrderNotFoundException.class)
        .hasMessageContaining("99");

assertThat(actualUser).usingRecursiveComparison().isEqualTo(expectedUser);   // compare field by field, no equals() needed

assertSoftly(s -> {                                                          // collect all failures
    s.assertThat(c.name()).isEqualTo("Asha");
    s.assertThat(c.age()).isGreaterThan(17);
});
```

JUnit's assertions are fine for simple cases. Many teams standardize on AssertJ for everything else. Hamcrest matchers are an older alternative.

---

## 4. Parameterized tests

Run one test body against many inputs, ideal for boundary tables ([Unit Testing Fundamentals](00_unit-testing-fundamentals.md#5-what-to-test-and-what-not-to)):

```java
@ParameterizedTest
@ValueSource(strings = {"", " ", "\t", "\n"})
void blankPasswordsAreRejected(String password) {
    assertFalse(validator.isValid(password));
}

@ParameterizedTest(name = "age {0} → adult = {1}")
@CsvSource({
    "17, false",
    "18, true",           // the boundary
    "19, true"
})
void adultFromEighteen(int age, boolean expected) {
    assertEquals(expected, AgeRules.isAdult(age));
}

@ParameterizedTest
@MethodSource("discountCases")
void appliesDiscount(BigDecimal total, BigDecimal expected) {
    assertEquals(expected, discounts.apply(total));
}
static Stream<Arguments> discountCases() {
    return Stream.of(
        Arguments.of(new BigDecimal("50.00"),  new BigDecimal("50.00")),
        Arguments.of(new BigDecimal("100.00"), new BigDecimal("90.00")),
        Arguments.of(new BigDecimal("500.00"), new BigDecimal("425.00")));
}

@ParameterizedTest
@EnumSource(value = Status.class, names = {"PAID", "SHIPPED"})
void paidOrShippedOrdersCannotBeEdited(Status status) { ... }

@ParameterizedTest
@NullAndEmptySource                         // null and ""
@ValueSource(strings = {"  "})              // plus blank
void rejectsMissingNames(String name) { ... }
```

| Source | Provides |
|---|---|
| `@ValueSource` | Literals of one type |
| `@CsvSource` / `@CsvFileSource` | Rows of mixed values (inline / from a file) |
| `@MethodSource` | A static method returning a `Stream`/collection of `Arguments`: any objects |
| `@EnumSource` | Enum constants (all, or selected/excluded) |
| `@NullSource`, `@EmptySource`, `@NullAndEmptySource` | `null` / empty values |
| `@ArgumentsSource` | A custom provider |

Each row appears as its own test in reports (name it with `name = "..."`). Put the data in the test, not hidden in distant files, so a reader sees the cases.

---

## 5. Organizing tests

```java
@Nested
@DisplayName("when the cart is empty")
class WhenEmpty {                                    // groups related tests; inherits the outer setup
    @Test void totalIsZero() { ... }
    @Test void checkoutIsRejected() { ... }
}

@Tag("slow")                                         // select with: mvn -Dgroups=slow / Gradle includeTags
@Test void importsLargeFile() { ... }

@Timeout(2)                                          // fail if it takes more than 2 seconds (default unit: seconds)
@Test void finishesQuickly() { ... }

@RepeatedTest(5)                                     // run repeatedly (to probe flakiness; not a fix)
void concurrentUpdate() { ... }

@Test void writesReport(@TempDir Path dir) throws IOException {    // fresh temporary directory, deleted afterwards
    Path file = dir.resolve("report.txt");
    Files.writeString(file, "ok");
    assertEquals("ok", Files.readString(file));
}
```

Conditional execution:

```java
@EnabledOnOs(OS.LINUX)
@EnabledForJreRange(min = JRE.JAVA_21)
@EnabledIfEnvironmentVariable(named = "CI", matches = "true")
@DisabledIf("isSlowMachine")                         // a method returning boolean
```

Assumptions abort (rather than fail) a test when preconditions aren't met: `assumeTrue(System.getenv("DB_URL") != null)`.

**Dynamic tests** (`@TestFactory`) generate tests at runtime from data, returning a stream of `DynamicTest`s. Parameterized tests cover most needs.

---

## 6. Extensions

Jupiter's extension model replaces JUnit 4's runners and rules. Extensions hook into the lifecycle and can **resolve parameters**, **manage resources**, **condition tests**, and **handle exceptions**.

```java
@ExtendWith(MockitoExtension.class)                  // initializes @Mock fields (see Mockito note)
class OrderServiceTest { ... }

@ExtendWith(SpringExtension.class)                   // Spring's test context (included in @SpringBootTest)
```

Programmatic registration (with configuration):

```java
@RegisterExtension
static final WireMockExtension wiremock = WireMockExtension.newInstance().build();
```

Writing your own is small:

```java
class TimingExtension implements BeforeEachCallback, AfterEachCallback {
    private static final ExtensionContext.Namespace NS = ExtensionContext.Namespace.create(TimingExtension.class);

    @Override public void beforeEach(ExtensionContext ctx) {
        ctx.getStore(NS).put("start", System.nanoTime());
    }
    @Override public void afterEach(ExtensionContext ctx) {
        long ms = (System.nanoTime() - ctx.getStore(NS).remove("start", long.class)) / 1_000_000;
        System.out.println(ctx.getDisplayName() + " took " + ms + " ms");
    }
}
```

Parameter injection into test methods (like `@TempDir Path dir`, `TestInfo`, `TestReporter`) uses the same mechanism. Most of what you need already ships as extensions: Mockito, Spring, Testcontainers, WireMock, and Awaitility integrations.

---

## 7. Configuration and parallel execution

`src/test/resources/junit-platform.properties`:

```properties
junit.jupiter.execution.parallel.enabled = true
junit.jupiter.execution.parallel.mode.default = concurrent
junit.jupiter.displayname.generator.default = org.junit.jupiter.api.DisplayNameGenerator$ReplaceUnderscores
```

Parallel execution speeds up large suites, but it turns every hidden shared-state bug into a flaky test. Before enabling it:

- Make sure tests don't share **mutable static state**, ports, files, or database rows.
- Use `@ResourceLock("name")` to serialize tests that touch the same resource, and `@Execution(ExecutionMode.SAME_THREAD)` to opt classes out.
- Remember that **thread-bound** things (Spring transactions, thread locals) behave per thread ([ThreadLocal](../14-concurrency/09_thread-local.md)).

Reports are written as JUnit XML (surefire/Gradle), which CI tools display. Use `@Tag` plus build config to split fast/slow suites ([CI/CD](../18-build-and-dependencies/03_ci-cd-pipelines.md#4-test-stages-in-the-pipeline)).

---

## 8. Migrating from JUnit 4

| JUnit 4 | JUnit Jupiter (5/6) |
|---|---|
| `org.junit.Test` | `org.junit.jupiter.api.Test` |
| `@Before` / `@After` | `@BeforeEach` / `@AfterEach` |
| `@BeforeClass` / `@AfterClass` | `@BeforeAll` / `@AfterAll` (static) |
| `@Ignore` | `@Disabled` |
| `@RunWith(X.class)` | `@ExtendWith(X.class)` |
| `@Rule` / `@ClassRule` | Extensions, `@TempDir`, `@RegisterExtension` |
| `@Test(expected = X.class)` | `assertThrows(X.class, ...)` |
| `@Test(timeout = ...)` | `@Timeout` |
| `@Category` | `@Tag` |
| `Assert.assertEquals(msg, expected, actual)` | `Assertions.assertEquals(expected, actual, msg)` (**message last**) |
| `MockitoJUnitRunner` | `MockitoExtension` |
| `SpringRunner` | `SpringExtension` (included in `@SpringBootTest`) |

Migration plan: add Jupiter and the **Vintage** engine so old and new tests run side by side, convert test classes incrementally (or with an automated tool such as OpenRewrite recipes), then remove Vintage. JUnit 6 deprecates Vintage, so don't plan to keep JUnit 4 tests long term.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Importing `org.junit.Test` (JUnit 4) in a Jupiter project, so the test silently doesn't run | Import `org.junit.jupiter.api.Test` |
| `assertEquals(actual, expected)` reversed | Expected first, so failure messages read correctly |
| Non-static `@BeforeAll` | `static`, or `@TestInstance(PER_CLASS)` |
| `private` test methods | Package-private or public |
| `assertEquals` on doubles with no delta | Use a tolerance (or `BigDecimal` with `compareTo`) |
| Depending on test execution order | Make each test independent |
| Mixing JUnit artifact versions | Import the BOM |
| `assertTimeout`/`@Timeout` as a performance test | Use JMH for performance ([Benchmarking](../20-performance/01_benchmarking-with-jmh.md)) |
| Enabling parallel execution without checking shared state | Audit statics, ports, files, DB rows |
| `@Disabled` tests left forever | Fix or delete |
| Parameterized data far from the test | Keep cases inline, readable |
| `Thread.sleep` to wait for async work | `Awaitility` or latches ([Integration Testing](03_integration-testing.md#5-async-code-and-messaging)) |

### Debugging

- **"No tests found" / 0 tests run** → the Jupiter engine isn't on the test classpath, surefire is too old, the Gradle launcher dependency is missing, tests use JUnit 4 imports without Vintage, or the class name doesn't match surefire's patterns.
- **`NoSuchMethodError`/`ClassNotFoundException` in the test runner** → mixed JUnit versions (use the BOM), or an IDE/plugin using an older platform.
- **`UnsupportedClassVersionError`** → JUnit 6 on Java below 17. Use JUnit 5.x or upgrade the JDK.
- **Test passes alone, fails in the suite** → shared static/mutable state, or order dependence.
- **Which test failed and why?** Check `target/surefire-reports` (or `build/reports/tests`), and run a single test: `-Dtest=Class#method` / `--tests`.
- **Parameterized test fails for one row** → the report names the row. Make sure `name = "..."` shows the arguments.

---

## Quick Summary

- **JUnit Jupiter** is the programming model for JUnit 5 and 6 (same `org.junit.jupiter.*` packages). **JUnit 6** needs Java 17+ and unifies version numbers. Use the **BOM**.
- Tests are package-private methods annotated `@Test`. A **new instance per test** gives isolation. Lifecycle: `@BeforeEach`/`@AfterEach` (and static `@BeforeAll`/`@AfterAll`).
- **Assertions:** JUnit's (`expected` first, message last), `assertThrows`, `assertAll`, plus **AssertJ** for fluent, readable checks.
- **Parameterized tests** (`@ValueSource`, `@CsvSource`, `@MethodSource`, `@EnumSource`) cover boundaries compactly.
- Organize with `@Nested`, `@Tag`, `@DisplayName`, `@TempDir`, `@Timeout`, and conditional annotations.
- **Extensions** (`@ExtendWith`) replace runners and rules. Parallel execution needs independent tests.
- Migrate JUnit 4 incrementally with Vintage, then remove it.

**Next:** [Mocking and Mockito](02_mocking-and-mockito.md)