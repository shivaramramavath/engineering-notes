# Code Coverage and Mutation Testing: JaCoCo and PIT (pitest)

> Measuring test suite effectiveness beyond false confidence: Line vs Branch Coverage, JaCoCo Bytecode Instrumentation, and Mutation Testing with PIT.

| Metadata | Details |
|---|---|
| **Level** | Core / Advanced |
| **Prerequisites** | Unit Testing, JUnit 5, Java Bytecode Basics |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **JDK Baseline** | Java 17 / 21 / 25 LTS (JaCoCo 0.8.12+ / PIT 1.16+) |

---

## Table of Contents
1. [1. What & Why](#1-what--why)
2. [2. Core Concepts](#2-core-concepts)
3. [3. How It Works Internally: Bytecode Probing & Mutation Operators](#3-how-it-works-internally-bytecode-probing--mutation-operators)
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
Software teams routinely track **Code Coverage** (often targeting 80% or 90%) as a proxy for software quality. However, traditional line coverage metrics suffer from a dangerous flaw: line coverage only measures which lines of code were *executed* by the JVM during a test run; it does **not** measure whether the tests actually *asserted* or verified anything! A test suite that executes 100% of the lines but contains zero assertions will proudly report 100% code coverage.

**Mutation Testing** solves this false sense of security. Instead of measuring what code was touched, mutation testing tests the quality of your tests. Tools like **PIT (pitest)** introduce small deliberate faults (mutants) into your compiled bytecode—such as changing `>` to `<`, negating conditionals, or removing void method calls. If your test suite fails when a mutant is introduced, the mutant is **killed** (good). If your test suite passes despite the corrupted code, the mutant **survives** (bad), exposing a blind spot in your test suite.

---

## 2. Core Concepts
- **Line Coverage vs Branch Coverage**:
  - *Line Coverage*: Percentage of executable source code lines executed during testing.
  - *Branch Coverage*: Percentage of boolean decision branches (e.g., both `true` and `false` evaluations in `if` statements) traversed.
- **JaCoCo (Java Code Coverage)**: The industry-standard coverage library using on-the-fly bytecode instrumentation.
- **Mutation Testing (PIT / pitest)**: A fault-based software testing technique that mutates bytecode to measure test suite detection power.
- **Mutant**: A deliberately corrupted version of the production bytecode (e.g., changing `a + b` to `a - b`).
- **Killed Mutant**: A mutant detected by the test suite (at least one test failed). Indicates high test quality.
- **Surviving Mutant**: A mutant that went undetected by the test suite (all tests passed). Exposes missing assertions or unverified edge cases.
- **Mutation Score**: Percentage of generated mutants killed by the test suite (`Killed Mutants / Total Mutants`).

---

## 3. How It Works Internally: Bytecode Probing & Mutation Operators

JaCoCo and PIT operate by modifying JVM bytecode in memory:

```
[ Production Class Bytecode: OrderCalculator.class ]
                           |
            +--------------+--------------+
            |                             |
            v                             v
+-----------------------+     +-----------------------+
|  JaCoCo Coverage      |     |  PIT Mutation Engine  |
|  - Injects boolean[]  |     |  - Mutates bytecode:  |
|    probe arrays       |     |    `if (a > b)` into  |
|  - Sets probe = true  |     |    `if (a >= b)`      |
|    on execution       |     |  - Runs tests against |
|  - Dumps jacoco.exec  |     |    mutated bytecode   |
+-----------------------+     +-----------------------+
```

### Common PIT Mutation Operators:
1. **Conditionals Boundary Mutator**: Changes `<` to `<=`, or `>` to `>=`.
2. **Negate Conditionals Mutator**: Inverts `if (x == y)` to `if (x != y)`.
3. **Math Mutator**: Replaces arithmetic operators (`+` $	o$ `-`, `*` $	o$ `/`).
4. **Void Method Call Mutator**: Completely removes invocations to void methods (e.g., removing `repository.save()` or `logger.info()`).
5. **Return Values Mutator**: Mutates return values to `null`, `0`, or `emptyOptional()`.

---

## 4. Syntax & Usage

### 4.1 Configuring JaCoCo in Maven (`pom.xml`)
```xml
<plugin>
    <groupId>org.jacoco</groupId>
    <artifactId>jacoco-maven-plugin</artifactId>
    <version>0.8.12</version>
    <executions>
        <!-- Prepares agent before tests run -->
        <execution>
            <id>prepare-agent</id>
            <goals><goal>prepare-agent</goal></goals>
        </execution>
        <!-- Generates HTML coverage report -->
        <execution>
            <id>report</id>
            <phase>test</phase>
            <goals><goal>report</goal></goals>
        </execution>
        <!-- Enforces minimum coverage thresholds -->
        <execution>
            <id>check</id>
            <goals><goal>check</goal></goals>
            <configuration>
                <rules>
                    <rule>
                        <element>BUNDLE</element>
                        <limits>
                            <limit>
                                <counter>LINE</counter>
                                <value>COVEREDRATIO</value>
                                <minimum>0.80</minimum>
                            </limit>
                            <limit>
                                <counter>BRANCH</counter>
                                <value>COVEREDRATIO</value>
                                <minimum>0.75</minimum>
                            </limit>
                        </limits>
                    </rule>
                </rules>
            </configuration>
        </execution>
    </executions>
</plugin>
```

### 4.2 Configuring PIT (pitest) in Maven
```xml
<plugin>
    <groupId>org.pitest</groupId>
    <artifactId>pitest-maven</artifactId>
    <version>1.16.1</version>
    <dependencies>
        <dependency>
            <groupId>org.pitest</groupId>
            <artifactId>pitest-junit5-plugin</artifactId>
            <version>1.2.1</version>
        </dependency>
    </dependencies>
    <configuration>
        <targetClasses>
            <param>com.example.service.*</param>
        </targetClasses>
        <targetTests>
            <param>com.example.service.*Test</param>
        </targetTests>
        <mutationThreshold>80</mutationThreshold> <!-- Fails build if mutation score < 80% -->
        <mutators>
            <mutator>STRONGER</mutator>
        </mutators>
        <threads>4</threads>
    </configuration>
</plugin>
```

---

## 5. Practical Examples

### Example 1: Surviving Mutant Exposing Missing Assertion
```java
// PRODUCTION CODE UNDER TEST
public class DiscountCalculator {
    public static double calculateDiscount(double price, boolean isVip) {
        if (price > 100.0 && isVip) { // Target of Conditionals Boundary Mutator
            return price * 0.20;
        }
        return 0.0;
    }
}

// WEAK TEST (Reports 100% Line Coverage, but Mutation Score is FAILING!)
class WeakDiscountTest {
    @Test
    void testVipDiscount() {
        // Runs lines, but does not assert on boundary condition price == 100!
        double discount = DiscountCalculator.calculateDiscount(200.0, true);
        assertThat(discount).isEqualTo(40.0);
    }
}
// PIT mutates `price > 100.0` to `price >= 100.0`.
// WeakDiscountTest still passes! MUTANT SURVIVES!
```

### Example 2: Killing the Mutant with Boundary Assertions
```java
// STRONG TEST (Kills the boundary mutant!)
class StrongDiscountTest {
    @Test
    void calculateDiscount_exactlyHundredDollars_receivesNoDiscount() {
        // Explicitly tests the boundary value: price == 100.0
        double discount = DiscountCalculator.calculateDiscount(100.0, true);
        assertThat(discount).isEqualTo(0.0);
    }

    @Test
    void calculateDiscount_aboveHundredDollars_receivesTwentyPercent() {
        double discount = DiscountCalculator.calculateDiscount(100.01, true);
        assertThat(discount).isCloseTo(20.002, within(0.001));
    }
}
// When PIT changes `>` to `>=`, StrongDiscountTest fails! MUTANT KILLED!
```

### Example 3: Edge Case - Excluding Auto-Generated Code from Coverage
```xml
<!-- Exclude Lombok, MapStruct, or Protobuf generated classes from JaCoCo -->
<configuration>
    <excludes>
        <exclude>**/dto/**</exclude>
        <exclude>**/config/**</exclude>
        <exclude>**/*MapperImpl.class</exclude>
    </excludes>
</configuration>
```

---

## 6. Best Practices
1. **Focus on Branch Coverage Over Line Coverage**: Line coverage is easily satisfied; branch coverage guarantees that both true and false paths of conditionals are tested.
2. **Run Mutation Testing in Nightly or PR Pipelines**: Mutation testing is CPU-intensive; run PIT on modified files during PR builds, or run full mutation sweeps nightly.
3. **Exclude DTOs, Boilerplate, and Configuration Classes**: Exclude auto-generated code, records, and configuration beans from mutation coverage metrics to avoid false alarms.
4. **Use Mutation Thresholds for Critical Domain Packages**: Enforce `<mutationThreshold>85</mutationThreshold>` on financial, cryptographic, and core business calculation modules.
5. **Never Game Line Coverage Metrics**: Writing assertion-free tests solely to meet a management coverage quota damages codebase integrity.
6. **Aim for Mutation Score > 80% on Core Modules**: A high mutation score guarantees that if a bug is introduced into business logic, at least one test will catch it.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Correction |
|---|---|---|
| Relying on line coverage alone | Tests execute code without asserting outputs; hides bugs | Adopt branch coverage and mutation testing |
| Running PIT across entire monolith on every commit | Build takes hours to complete due to thousands of mutants | Target PIT to modified packages or diffs |
| Ignoring surviving mutants | Blind spots in test suite remain unaddressed | Add boundary and negative test cases to kill mutants |
| Measuring coverage on generated code (Lombok/MapStruct) | Artificially deflates coverage scores | Exclude generated packages via configuration |
| Over-optimizing coverage on trivial getters/setters | Wastes engineering time on zero-risk code | Focus testing effort on complex domain logic |
| Tests relying on random values during PIT | Causes nondeterministic mutation test execution | Use deterministic inputs or fixed random seeds |
| Void method mutator survives | Indicates missing interaction verification (e.g., missing `verify(repo).save()`) | Verify side effects of void methods |
| Single-threaded PIT execution | CPU underutilized; mutation testing takes 10x longer | Configure `<threads>4</threads>` in PIT plugin |

---

## 8. Performance & Security Notes
- **JaCoCo Runtime Overhead**: JaCoCo's on-the-fly bytecode instrumentation adds less than 5% overhead to standard test runs, making it safe to run on every commit.
- **PIT Execution Optimization**: PIT uses intelligent test selection: for each mutant, it only executes tests that historically cover that specific line of code, cutting execution time by over 90%.
- **Security in Audit Logs**: Ensure test suites kill void-mutator mutants on audit logging and security authorization checks to verify that security side effects cannot be bypassed silently.

---

## 9. Comparison / When to Use What

| Metric / Tool | What It Measures | Speed | Blind Spot | Best Use Case |
|---|---|---|---|---|
| **Line Coverage (JaCoCo)** | Which lines ran | Ultra Fast (< 5% overhead)| Zero assertion detection | Minimum baseline sanity check |
| **Branch Coverage (JaCoCo)**| Which decisions ran | Fast | Misses subtle boundary logic | Standard CI pipeline quality gate |
| **Mutation Score (PIT)** | Test fault detection power | Slower (Runs tests per mutant)| Higher execution time | Core business logic, financial math |

---

## 10. Interview Questions

<details>
<summary>1. Why is 100% line coverage often a dangerous false sense of security?</summary>
Line coverage only tracks whether the JVM executed a given line of bytecode during test execution. It does not measure whether the test contains assertions, checks boundary conditions, or handles errors. A test can execute 100% of the lines in a method without an <code>assert</code> statement, achieving 100% line coverage while catching zero defects.
</details>

<details>
<summary>2. How does Mutation Testing evaluate the quality of a test suite?</summary>
Mutation testing introduces small, deliberate syntactic changes (mutants) into the compiled bytecode of the production code (such as changing <code>+</code> to <code>-</code>, or replacing a return value with <code>null</code>). It then executes the existing test suite against the mutated bytecode. If the test suite fails, the mutant is "killed" (proving the test suite detected the fault). If all tests pass, the mutant "survived", proving the test suite has a blind spot.
</details>

<details>
<summary>3. What is the difference between Line Coverage and Branch Coverage?</summary>
<b>Line Coverage</b> measures the percentage of physical lines containing executable instructions that were executed at least once.<br>
<b>Branch Coverage</b> measures whether every boolean decision branch (e.g., both the <code>true</code> and <code>false</code> paths of an <code>if</code> or <code>switch</code> statement) was traversed. A test can achieve 100% line coverage on a simple <code>if</code> block without ever testing the alternative branch.
</details>

<details>
<summary>4. What are three common mutation operators used by PIT?</summary>
1. <b>Conditionals Boundary Mutator</b>: Mutates relational operators (e.g., <code><</code> to <code><=</code>).<br>
2. <b>Negate Conditionals Mutator</b>: Inverts boolean conditions (e.g., <code>if (x == y)</code> to <code>if (x != y)</code>).<br>
3. <b>Void Method Call Mutator</b>: Completely removes calls to void methods (such as event publishing or database writes), checking if tests verify side effects.
</details>

<details>
<summary>5. How does PIT avoid running the entire test suite for every single generated mutant?</summary>
PIT uses <b>test prioritization and coverage mapping</b>. Before mutating code, it analyzes which tests cover which lines. When a mutant is injected into line 42 of <code>OrderService</code>, PIT only executes the specific tests that actually traverse line 42. Furthermore, as soon as the first test fails on that mutant, PIT stops running remaining tests and marks the mutant killed immediately.
</details>

<details>
<summary>6. What does a surviving Void Method Call mutant indicate in a test suite?</summary>
It indicates that a void method was removed from production code (e.g., <code>auditLogger.logEvent()</code> or <code>emailClient.send()</code>), but all tests continued to pass. This reveals that the test suite failed to verify that mandatory side-effects occurred, signaling the need for an interaction verification (e.g., <code>verify(auditLogger).logEvent(...)</code>).
</details>

<details>
<summary>7. How does JaCoCo measure coverage without modifying original source code?</summary>
JaCoCo uses Java bytecode instrumentation. When classes are loaded into the JVM by the class loader, the JaCoCo Java agent intercepts the class bytes and injects lightweight boolean probe arrays. When execution passes through a basic block of bytecode, the corresponding probe is set to <code>true</code>. At JVM termination, the agent dumps the probe execution data to <code>jacoco.exec</code>.
</details>

<details>
<summary>8. Why should mutation testing be targeted rather than executed on an entire enterprise codebase on every commit?</summary>
Mutation testing generates thousands of individual mutants, each requiring running a subset of tests. On a large monolithic codebase with 50,000 lines of code, running full mutation testing can take hours. Best practice is to run PIT incrementally on modified files in PR checks (e.g., via <code>pitest-git-plugin</code>) and schedule full sweeps nightly.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Write a test that kills a Conditionals Boundary Mutator on a youth discount rule.</summary>

```java
// Production rule: Must be strictly under 18 (age < 18)
public static boolean isEligibleForYouthDiscount(int age) {
    return age < 18;
}

// Tests killing the boundary mutant (age <= 18):
@Test
void isEligibleForYouthDiscount_boundaryTests() {
    assertThat(DiscountService.isEligibleForYouthDiscount(17)).isTrue();
    assertThat(DiscountService.isEligibleForYouthDiscount(18)).isFalse(); // Kills mutant changing < to <=
    assertThat(DiscountService.isEligibleForYouthDiscount(19)).isFalse();
}
```
</details>

<details>
<summary>Exercise 2: Write a Maven command running PIT exclusively on modified classes.</summary>

```bash
# Execute PIT mutation coverage using 4 concurrent threads on the service module
./mvnw org.pitest:pitest-maven:mutationCoverage -Dthreads=4 -DtargetClasses=com.example.service.*
```
</details>

<details>
<summary>Exercise 3: Configure a JaCoCo rule enforcing 100% branch coverage on a critical SecurityValidator class.</summary>

```xml
<rule>
    <element>CLASS</element>
    <includes>
        <include>com.example.security.SecurityValidator</include>
    </includes>
    <limits>
        <limit>
            <counter>BRANCH</counter>
            <value>COVEREDRATIO</value>
            <minimum>1.00</minimum>
        </limit>
    </limits>
</rule>
```
</details>

---


### 11.4 In-Depth Guide: Establishing Mutation Testing in CI/CD with Git Diff

Because running full mutation testing across an entire multi-module repository can take 20+ minutes, production pipelines use incremental Git-diff analysis:

1. **Integrating pitest-git-plugin**:
   Add the git-plugin dependency to `pitest-maven`:
   ```xml
   <dependency>
       <groupId>com.arcmutate</groupId>
       <artifactId>pitest-git-plugin</artifactId>
       <version>1.2.0</version>
   </dependency>
   ```

2. **Running Mutation Tests Strictly on Changed Files**:
   Execute mutation analysis against the baseline target branch (`origin/main`):
   ```bash
   ./mvnw org.pitest:pitest-maven:mutationCoverage -DdestinationBranch=origin/main
   ```
   PIT computes the exact list of modified lines in the pull request and generates mutants **only** for those modified lines, cutting execution time from 20 minutes down to 15 seconds while guaranteeing 100% mutation coverage on all newly merged code.

3. **Interpreting the HTML Mutation Report**:
   Open `target/pit-reports/index.html`. Green lines indicate code covered by tests where all generated mutants were killed. Red lines highlight surviving mutants, identifying exact unasserted edge cases.


## 12. Summary / Cheat Sheet
- **Line vs Branch**: Line coverage measures execution; branch coverage measures decision completeness.
- **Mutation Testing**: Injects bytecode faults to test the test suite; tests must kill mutants.
- **Mutant States**: Killed (test failed = good); Survived (test passed = blind spot in assertions).
- **JaCoCo**: Lightweight bytecode instrumentation (< 5% overhead); enforce rules in CI.
- **PIT (pitest)**: Generates mutants (boundary, math, void calls); target core business logic.
- **Void Calls**: Surviving void mutants indicate missing interaction verifications (`verify()`).

---

## 13. Further Reading
- JaCoCo Java Code Coverage Library Documentation
- PIT Mutation Testing (pitest) Official Documentation and User Guide
- State of Mutation Testing in the Software Industry (Google / Arcuri Research)
- *Refactoring: Improving the Design of Existing Code* by Martin Fowler
