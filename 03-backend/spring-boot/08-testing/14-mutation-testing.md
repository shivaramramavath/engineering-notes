# 14. Mutation Testing with Pitest (Evaluating Test Quality)

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | `01-testing-fundamentals-in-spring.md`, `02-unit-testing-without-spring.md`, Code Coverage Metrics (JaCoCo) |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, Pitest 1.17 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
Standard line and branch code coverage (measured via tools like JaCoCo) is notoriously a **vanity metric**:
- A test suite can achieve **100% line coverage** while asserting **nothing**. If an integration test executes a service method but omits assertions on the returned payload, JaCoCo marks every executed line green.
- Developers write superficial tests that execute code paths to satisfy management coverage mandates without verifying boundary logic (e.g., `<` vs `<=`, `+` vs `-`, or null checks).

**Mutation Testing** evaluates the **actual quality and effectiveness** of your tests. It introduces deliberate artificial bugs (mutants) into your compiled bytecode and runs your test suite against them.
- If a test fails $\implies$ The mutant was **KILLED** (Good! Your tests detected the bug).
- If all tests pass $\implies$ The mutant **SURVIVED** (Bad! Your test suite failed to detect a logic change).

### Historical Evolution
- **1970s Academic Research**: Concept originated by Richard Lipton and Frederick Sayward, but remained computationally impractical due to slow CPU hardware.
- **PIT / Pitest (2011-Present)**: Revolutionized the field by operating directly on Java bytecode in memory using Byte Buddy, introducing incremental analysis and making mutation testing practical for CI pipelines.
- **Modern Spring Boot 3.x/4.x**: Full compatibility with Java 21 bytecode, record classes, and parallel multi-core execution.

### Real-World Relevance
High-reliability industries (fintech, aviation, medical devices) rely on Pitest to guarantee that critical financial calculations, pricing logic, and authorization guards are thoroughly verified by real assertions.

---

## 2. Core Concepts

### Architectural Contracts
- **Mutator**: An automated rule that alters bytecode to create a mutant.
  - *Conditionals Boundary Mutator*: Inverts `<` to `<=`, or `>` to `>=`.
  - *Math Mutator*: Replaces `+` with `-`, or `*` with `/`.
  - *Invert Negatives Mutator*: Replaces `-x` with `x`.
  - *Void Method Call Mutator*: Completely deletes calls to `void` methods (e.g., deletes `repository.save()`).
  - *Empty Returns Mutator*: Replaces returned collections with empty lists, or returns `null`.
  - *True/False Returns Mutator*: Replaces boolean returns with their inverse (`true` becomes `false`).
- **Killed Mutant**: An altered bytecode variant that causes at least one test in the suite to fail.
- **Survived Mutant**: A mutated bytecode variant where all tests still pass, proving an assertion gap in your test suite.
- **Mutation Score Indicator (MSI)**: The percentage of created mutants that were killed:
  $$\text{Mutation Score} = \frac{\text{Killed Mutants}}{\text{Total Mutants}} \times 100\%$$
- **Incremental Analysis**: Caching mutation results between builds using a history file to only analyze changed classes and affected tests.

### The Mental Model
```
[ Original Production Java Code ]
int finalPrice = basePrice - discount;

                 │
                 ▼
[ Pitest Generates Mutant Bytecode ]
int finalPrice = basePrice + discount;  // Math Mutator applied!

                 │
                 ▼
[ Pitest Runs Existing Unit Test Suite ]
                 │
                 ├── Test Suite FAILS (Expected 80, Got 120)
                 │     └── MUTANT KILLED! (Test suite is effective)
                 │
                 └── Test Suite PASSES (No assertion checked price!)
                       └── MUTANT SURVIVED! (Critical test gap exposed!)
```

### Invariants
1. A mutant is only considered killed if an existing automated test fails with an assertion failure or exception.
2. Mutation testing must only run against **pure unit tests**; running Pitest against slow `@SpringBootTest` suites causes catastrophic build execution delays.
3. Tests that run into infinite loops because of a mutation are stopped by Pitest's timeout watchdog and marked as killed.

---

## 3. How It Works Internally

### The Pitest Bytecode Mutation Pipeline
```
Target Bytecode (.class) & Test Classes
   │
   ├── 1. Test Coverage Pre-Scan:
   │      └─ Runs test suite once to identify which tests cover which production lines
   │
   ├── 2. Mutant Generation:
   │      └─ Applies configured mutators to compiled bytecode in memory (via ASM)
   │
   ├── 3. Targeted Test Execution:
   │      ├─ For each mutant: Runs ONLY the specific tests that cover that line of code!
   │      ├─ Submits tasks to isolated child worker JVMs
   │      └─ Enforces timeout threshold (default: min(3s, 1.25 * normal execution time))
   │
   └── 4. Report Generation:
          ├─ Computes Mutation Score Indicator (MSI)
          ├─ Emits interactive HTML report showing surviving mutant code lines
          └─ Updates incremental analysis history cache file
```

---

## 4. Syntax & Usage

### 1. Maven Plugin Configuration (`pom.xml`)
```xml
<plugin>
    <groupId>org.pitest</groupId>
    <artifactId>pitest-maven</artifactId>
    <version>1.17.0</version>
    <dependencies>
        <dependency>
            <groupId>org.pitest</groupId>
            <artifactId>pitest-junit5-plugin</artifactId>
            <version>1.2.1</version>
        </dependency>
    </dependencies>
    <configuration>
        <targetClasses>
            <!-- Restrict mutation strictly to domain logic and services -->
            <param>com.example.domain.*</param>
            <param>com.example.service.*</param>
        </targetClasses>
        <targetTests>
            <param>com.example.*Test</param>
        </targetTests>
        <excludedClasses>
            <param>com.example.config.*</param>
            <param>com.example.dto.*</param>
        </excludedClasses>
        <mutators>
            <mutator>STRONGER</mutator> <!-- Modern comprehensive mutator group -->
        </mutators>
        <mutationThreshold>80</mutationThreshold> <!-- Fail build if score < 80% -->
        <threads>4</threads> <!-- Multi-core parallel execution -->
        <withHistory>true</withHistory> <!-- Incremental analysis cache -->
    </configuration>
</plugin>
```

### 2. Executing Mutation Analysis
```bash
mvn test-compile pitest:mutationCoverage
```

---

## 5. Practical Examples

### Example 1: Exposing a Missing Boundary Assertion
Demonstrating how a surviving mutant catches an off-by-one boundary bug that passed 100% JaCoCo coverage.

```java
// Production Code:
public class DiscountPolicy {
    public boolean isEligibleForDiscount(int customerAge) {
        // Pitest applies ConditionalsBoundaryMutator: changes >= 65 to > 65
        return customerAge >= 65;
    }
}
```

```java
// Weak Unit Test (100% JaCoCo Line Coverage, but Fails Mutation Testing!):
@Test
void testDiscount() {
    DiscountPolicy policy = new DiscountPolicy();
    assertThat(policy.isEligibleForDiscount(70)).isTrue();  // Tests 70
    assertThat(policy.isEligibleForDiscount(25)).isFalse(); // Tests 25
    // Notice: Age 65 was NEVER tested!
    // Mutant (customerAge > 65) SURVIVED!
}
```

**Production Fix**:
Add the boundary test case to kill the mutant:

```java
// Comprehensive Test (Mutant Killed!):
@Test
void testExactBoundary() {
    DiscountPolicy policy = new DiscountPolicy();
    assertThat(policy.isEligibleForDiscount(65)).isTrue(); // Kills the mutant!
}
```

### Example 2: Exposing Missing Void Method Assertions
Detecting that a critical audit logger call was never verified.

```java
// Production Code:
public class OrderService {
    private final OrderRepository repository;
    private final AuditLogger auditLogger;

    public void cancelOrder(Long id) {
        Order order = repository.findById(id).orElseThrow();
        order.setStatus("CANCELLED");
        repository.save(order);
        auditLogger.logCancellation(id); // Pitest deletes this line!
    }
}
```

```java
// Weak Test:
@Test
void shouldCancelOrder() {
    service.cancelOrder(101L);
    verify(repository).save(any());
    // Missing verification for auditLogger!
    // Mutant "removed call to AuditLogger::logCancellation" SURVIVED!
}
```

**Production Fix**:
Verify the void method call to kill the mutant:

```java
// Robust Test:
@Test
void shouldCancelOrderAndAudit() {
    service.cancelOrder(101L);
    verify(repository).save(any());
    verify(auditLogger).logCancellation(101L); // Kills the mutant!
}
```

### Example 3: Edge Case – Excluding Auto-Generated Records & Boilerplate
Configuring Pitest to avoid wasting time mutating immutable Java records, Lombok getters/setters, or configuration classes.

```xml
<configuration>
    <excludedClasses>
        <param>com.example.domain.*Record</param>
        <param>com.example.config.*</param>
    </excludedClasses>
    <avoidCallsTo>
        <avoidCallTo>java.util.logging</avoidCallTo>
        <avoidCallTo>org.slf4j</avoidCallTo>
    </avoidCallsTo>
</configuration>
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Running Pitest across the entire project including `@SpringBootTest` and Testcontainers integration tests, causing build times to explode to 2 hours.

```xml
<!-- BUGGY: Includes heavy integration tests in mutation analysis! -->
<targetTests>
    <param>com.example.**.*IT</param> <!-- Integration tests with Spring Context! -->
</targetTests>
```

**Production Fix**:
Strictly target fast, isolated pure unit tests (`*Test.java`), excluding slow integration tests (`*IT.java`).

```xml
<!-- FAST & ACCURATE: Mutation completes in 15 seconds! -->
<targetTests>
    <param>com.example.unit.**.*Test</param>
</targetTests>
```

### Example 5: Incremental Analysis with Git and CI Gates
Configuring Pitest to analyze only code modified in the current Git branch.

```xml
<configuration>
    <withHistory>true</withHistory>
    <historyInputFile>${project.build.directory}/pitest-history</historyInputFile>
    <historyOutputFile>${project.build.directory}/pitest-history</historyOutputFile>
</configuration>
```

---

## 6. Best Practices

1. **Target Pure Unit Tests Only**: Never run Pitest against `@SpringBootTest` or test slices; restrict mutation testing to domain logic and service unit tests.
2. **Use the `STRONGER` Mutator Group**: Enables comprehensive mutators (conditionals, math, return values, void calls) for thorough test evaluation.
3. **Enable Incremental Analysis (`withHistory = true`)**: Dramatically accelerates local and CI runs by only analyzing classes that changed since the last build.
4. **Enforce Mutation Score Thresholds**: Configure `<mutationThreshold>80</mutationThreshold>` in Maven/Gradle to fail builds if test quality drops below 80%.
5. **Exclude Logging Calls**: Use `<avoidCallsTo>` for `org.slf4j` to prevent generating surviving mutants on logger statements.
6. **Focus on High-Value Business Logic**: Prioritize mutation testing on financial engines, billing calculators, security evaluators, and state machines.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Running Pitest on `@SpringBootTest` | Build runs for hours because every mutant rebooted the Spring container. | Target only pure unit tests (`targetTests=*Test`). |
| Relying on line coverage alone | High JaCoCo coverage masks untested edge cases and missing assertions. | Track Mutation Score Indicator (MSI). |
| Forgetting to exclude DTOs/Records | Generates hundreds of useless mutants on equals/hashCode/toString boilerplate. | Exclude data classes via `<excludedClasses>`. |
| Unbounded mutation timeouts | Infinite loops caused by mutations hang the build. | Configure `<timeoutConstant>3000</timeoutConstant>`. |
| Single-threaded execution | CPU cores sit idle during bytecode analysis. | Configure `<threads>4</threads>` or use dynamic core counts. |
| Over-optimizing to 100% mutation score | Striving for 100% leads to testing trivial getters or equivalent mutants. | Aim for an 80–85% mutation threshold on core business packages. |

---

## 8. Performance & Security Notes

- **Multi-Process Architecture**: Pitest forks isolated child JVM worker processes to test mutants, ensuring an infinite loop or JVM crash caused by a mutant never kills the parent Maven/Gradle build.
- **Equivalent Mutants**: Approximately 5–10% of surviving mutants are "equivalent mutants"—code modifications that alter bytecode but produce identical semantic behavior. Striving for 100% mutation score is mathematically impractical; set thresholds between 80% and 85%.
- **Virtual Threads Integration**: Pitest child runners execute tests with minimal memory overhead, leveraging multi-core parallel execution smoothly on modern JDK 21 systems.

---

## 9. Comparison / When to Use What

| Metric / Tool | What It Measures | Vulnerability to "Assertion-Free" Tests | Speed | Best Use Case |
|---|---|---|---|---|
| **Line Coverage (JaCoCo)**| Lines executed during test run | **High (Passes with zero assertions!)**| Real-Time (Fast) | Baseline sanity check in CI |
| **Branch Coverage (JaCoCo)**| Decision branches executed | High | Fast | Baseline conditional check |
| **Mutation Testing (Pitest)**| **Assertion strength & test quality**| **Immune (Detects missing assertions)**| Moderate (10–30s) | Core domain logic, pricing, security |

---

## 10. Interview Questions

<details>
<summary>1. Why is 100% code coverage often described as a vanity metric?</summary>

**Answer:**
Standard code coverage tools (like JaCoCo) only measure which bytecode lines or branches were **executed** during a test run. They do not verify whether any meaningful **assertions** were made on the results. A developer could write a test that invokes every method in a service without a single `assertThat()` or `assert` statement, achieving 100% code coverage while providing zero defect detection capability.
</details>

<details>
<summary>2. How does Mutation Testing determine if a test suite is effective?</summary>

**Answer:**
Mutation testing introduces artificial modifications (mutations) into the compiled bytecode (such as inverting boundary comparisons, changing mathematical operators, or deleting method calls). It then runs the existing test suite against each mutated version. If the test suite fails, the mutant is "killed," proving the tests actively verify that behavior. If all tests pass, the mutant "survived," proving the test suite has an assertion gap.
</details>

<details>
<summary>3. What is an "Equivalent Mutant"?</summary>

**Answer:**
An equivalent mutant is a mutation that changes the bytecode syntax but produces the exact same semantic behavior as the original code. For example, changing a loop optimization from `for (int i=0; i < 10; i++)` to `for (int i=0; i != 10; i++)` in certain contexts produces identical program output. Because the behavior is identical, no test can ever fail, causing the mutant to survive indefinitely. This is why 100% mutation coverage is rarely achievable.
</details>

<details>
<summary>4. Why should mutation testing never be run on Spring integration tests?</summary>

**Answer:**
Pitest works by repeatedly executing tests for each generated mutant. If a class has 30 mutants and each test boots a Spring `@SpringBootTest` context taking 2 seconds, analyzing that single class would take over a minute. Multiplied across a real project, the build would take hours. Mutation testing must strictly target fast, pure unit tests (< 5ms) that execute in memory without containers.
</details>

<details>
<summary>5. What does the "Conditionals Boundary Mutator" do?</summary>

**Answer:**
The Conditionals Boundary Mutator alters boundary checks in relational expressions:
- `<` is mutated to `<=`
- `<=` is mutated to `<`
- `>` is mutated to `>=`
- `>=` is mutated to `>`
If tests only check values well within bounds (e.g., testing age 25 and age 70 for a `age >= 65` check) without testing the exact boundary (age 65), the mutant will survive.
</details>

<details>
<summary>6. How does Pitest's "Incremental Analysis" work?</summary>

**Answer:**
Pitest tracks code hashes and test results in an internal history file (`withHistory = true`). In subsequent builds, Pitest compares current bytecode hashes with the history file and only generates mutants for classes that have changed, and only runs the specific tests that cover those modified lines, reducing mutation run times by up to 90%.
</details>

<details>
<summary>7. What happens if a mutant causes an infinite loop in the production code?</summary>

**Answer:**
Pitest uses an internal watchdog timer. Before mutating, it records the normal execution time of the test suite. When running against a mutant, if the test exceeds a configured threshold (by default, `max(3000ms, 1.25 * normal_time)`), Pitest forcibly kills the worker process, records the mutant as **Timed Out** (which counts as a killed mutant), and proceeds to the next mutant.
</details>

<details>
<summary>8. What is the difference between Mutation Coverage and Mutation Score Indicator (MSI)?</summary>

**Answer:**
- **Mutation Coverage**: The ratio of killed mutants to total mutants generated: $\frac{\text{Killed}}{\text{Total}}$.
- **Mutation Score Indicator (MSI)**: Adjusts the metric by accounting for uncovered mutants, timed-out mutants, and memory errors, providing a standardized quality score used in automated build gates.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Spotting the Surviving Mutant (Easy)</summary>

**Task**: Identify the surviving mutant in this method when tested with `canAccess(18)` returning `true` and `canAccess(10)` returning `false`.
```java
public boolean canAccess(int age) {
    return age >= 18;
}
```

**Solution**:
The test does not assert the exact boundary value `18` vs `17`.
The Conditionals Boundary Mutator mutates `age >= 18` to `age > 18`.
Under the mutant, `canAccess(18)` returns `false`. Since the test expected `true`, this mutant is killed!
However, if the developer only tested `canAccess(21)` (returns true) and `canAccess(10)` (returns false), the mutant `age > 18` returns `true` for 21 and `false` for 10, allowing the mutant to **survive**.
</details>

<details>
<summary>Exercise 2: Minimal Pitest Maven Setup (Easy)</summary>

**Task**: Write the Maven XML configuration snippet to add `pitest-maven` targeting package `com.example.service.*` with 4 worker threads.

**Solution**:
```xml
<plugin>
    <groupId>org.pitest</groupId>
    <artifactId>pitest-maven</artifactId>
    <version>1.17.0</version>
    <configuration>
        <targetClasses>
            <param>com.example.service.*</param>
        </targetClasses>
        <threads>4</threads>
    </configuration>
</plugin>
```
</details>

<details>
<summary>Exercise 3: Killing a Void Call Mutator (Medium)</summary>

**Task**: Write a unit test that kills the mutant: `removed call to EmailService::sendAlert`.
```java
public void notifyUser(User user) {
    if (user.isVip()) {
        emailService.sendAlert(user.getEmail());
    }
}
```

**Solution**:
```java
@Test
void shouldKillVoidMethodMutator() {
    EmailService emailService = mock(EmailService.class);
    UserManager manager = new UserManager(emailService);

    manager.notifyUser(new User("vip@example.com", true));

    // Explicit verification kills the mutant!
    verify(emailService).sendAlert("vip@example.com");
}
```
</details>

<details>
<summary>Exercise 4: Math Mutator Detection (Medium)</summary>

**Task**: Write unit tests to kill mutants on:
```java
public int calculateTotal(int subtotal, int tax, int discount) {
    return subtotal + tax - discount;
}
```

**Solution**:
```java
@Test
void shouldKillMathMutators() {
    Calculator calc = new Calculator();
    // Use non-zero, asymmetric values so subtotal + tax - discount != subtotal - tax + discount
    int result = calc.calculateTotal(100, 20, 15);
    assertThat(result).isEqualTo(105);
}
```
</details>

<details>
<summary>Exercise 5: Configuring Build Failure Thresholds (Hard)</summary>

**Task**: Configure Pitest in Maven to fail the build if the overall mutation score drops below 85% or if line coverage drops below 90%.

**Solution**:
```xml
<configuration>
    <mutationThreshold>85</mutationThreshold>
    <coverageThreshold>90</coverageThreshold>
</configuration>
```
</details>

<details>
<summary>Exercise 6: Incremental History Configuration for CI (Hard)</summary>

**Task**: Configure Pitest to use an incremental history file stored in `target/pitest-history` so subsequent runs only analyze modified classes.

**Solution**:
```xml
<configuration>
    <withHistory>true</withHistory>
    <historyInputFile>${project.build.directory}/pitest-history.bin</historyInputFile>
    <historyOutputFile>${project.build.directory}/pitest-history.bin</historyOutputFile>
</configuration>
```
</details>

---

## 12. Summary / Cheat Sheet

- **Line Coverage $\ne$ Quality**: Tests can execute 100% of lines without asserting anything.
- **Mutants Test the Tests**: Pitest mutates bytecode; killed mutants prove tests are effective.
- **Target Pure Unit Tests**: Restrict Pitest to domain services; never run on slow Spring context tests.
- **Incremental Analysis**: Use `withHistory=true` to make mutation analysis practical in continuous delivery.
- **Aim for 80–85%**: Don't waste time on equivalent mutants; enforce realistic 80% thresholds in CI.

---

## 13. Further Reading
- [Pitest Official Documentation](https://pitest.org/)
- [Pitest Maven Plugin Configuration Guide](https://pitest.org/quickstart/maven/)
- [Martin Fowler: Mutation Testing](https://martinfowler.com/bliki/MutationTesting.html)
- [State of the Art in Mutation Testing](https://ieeexplore.ieee.org/document/8643837)
- [JaCoCo vs Pitest Comparison Guide](https://www.baeldung.com/java-mutation-testing-with-pitest)
