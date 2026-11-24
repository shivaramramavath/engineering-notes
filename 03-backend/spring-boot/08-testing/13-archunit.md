# 13. Architectural Testing & Fitness Functions with ArchUnit

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | Clean / Onion / Hexagonal Architecture, Java Reflection, JUnit 5 |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, ArchUnit 1.3 |

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
Software architecture diagrams and code conventions documented in wikis inevitably suffer from **architectural erosion** (code rot):
- Controllers start injecting `EntityManager` or calling repositories directly, bypassing security and transaction boundaries in the service layer.
- Cyclical package dependencies sneak in between domain modules, preventing clean modularization.
- Developers place `@Transactional` annotations on controllers or expose mutable internal JPA entities across public REST boundaries.
- Code reviews fail to catch these violations because manual human inspection cannot track thousands of class imports across large codebases.

**ArchUnit** is an extensible Java testing library that evaluates your compiled Java bytecode against declarative **Architectural Fitness Functions**. It allows engineering teams to enforce layered architectures, package dependencies, and coding conventions automatically as standard JUnit 5 tests in CI pipelines.

### Historical Evolution
- **Early Static Analysis (Checkstyle, PMD, SonarQube)**: Provided rule checking on source code, but lacked deep understanding of Java types, bytecode relationships, and cross-package call graphs.
- **ArchUnit (2017-Present)**: Revolutionized architectural governance by parsing compiled `.class` files into a rich Java object model and providing a fluent DSL (`classes().that()...should()...`).
- **ArchUnit 1.x & Spring Boot 3.x/4.x**: Full support for Java 21 records, sealed classes, pattern matching, and native Spring annotation analysis.

### Real-World Relevance
Leading engineering organizations treat architectural rules as automated unit tests: if a junior developer accidentally creates a circular dependency or accesses a repository from a controller, the build fails immediately in local development.

---

## 2. Core Concepts

### Architectural Contracts
- **`@AnalyzeClasses(packages = "com.example")`**: Instructs ArchUnit's JUnit 5 engine which root packages to import and analyze.
- **`@ArchTest`**: Marks a static `ArchRule` field or method for automated execution by the ArchUnit runner.
- **`ArchRule`**: The core assertion interface declaring architectural constraints (`classes().that()...should()...`).
- **Layered Architecture (`layeredArchitecture()`)**: Built-in DSL for defining and enforcing boundaries between layers (Web, Service, Persistence).
- **Onion / Hexagonal Architecture (`onionArchitecture()`)**: Built-in DSL validating Domain-Driven Design (DDD) boundaries (Domain Model, Domain Services, Application Services, Adapters).
- **Slices & Cycle Detection (`slices().matching().should().beFreeOfCycles()`)**: Enforces acyclic dependency structures across packages.

### The Mental Model
```
[ Compiled Java Bytecode (.class files) ]
                │
                ▼
[ ArchUnit Bytecode Parser (ASM Engine) ]
                │
                ▼
[ In-Memory Architectural Graph (JavaClasses) ]
                │
                ├── 1. Rule: Controllers must never touch Repositories!
                │      classes().that().resideInAPackage("..web..")
                │               .should().onlyAccessClassesThat()
                │               .resideOutsideOfPackage("..repository..")
                │
                └── 2. Evaluation:
                       Violation Found: OrderController.java:42
                       Calls: OrderRepository.deleteById()
                       FAILED: Architectural constraint violated!
```

### Invariants
1. ArchUnit tests analyze compiled bytecode (`target/classes`), not source files; compilation must precede test execution.
2. ArchUnit tests do not boot a Spring `ApplicationContext`; they execute in milliseconds using the in-memory ASM bytecode graph.
3. Architectural rules must apply universally across the codebase or define explicit, documented freeze exceptions.

---

## 3. How It Works Internally

### The ASM Bytecode Parsing Engine
1. ArchUnit uses the **ASM bytecode manipulation library** to scan all `.class` files in the specified packages.
2. It builds an in-memory graph of `JavaClass`, `JavaMethod`, `JavaField`, and `JavaAccess` instances representing all classes, calls, field accesses, and inheritance hierarchies.
3. When an `@ArchTest` runs, ArchUnit evaluates the rule against the graph:
   - Evaluates the **Predicate** (filters target classes, e.g., `resideInAPackage("..service..")`).
   - Evaluates the **Condition** (asserts requirements, e.g., `beAnnotatedWith(Service.class)`).
4. If violations exist, ArchUnit compiles a detailed report containing exact file names, line numbers, and violating method calls, throwing an `AssertionError`.

---

## 4. Syntax & Usage

### 1. Minimal Maven Dependency (`pom.xml`)
```xml
<dependency>
    <groupId>com.tngtech.archunit</groupId>
    <artifactId>archunit-junit5</artifactId>
    <version>1.3.0</version>
    <scope>test</scope>
</dependency>
```

### 2. Standard Layered Architecture Test
```java
package com.example.architecture;

import com.tngtech.archunit.junit.AnalyzeClasses;
import com.tngtech.archunit.junit.ArchTest;
import com.tngtech.archunit.lang.ArchRule;

import static com.tngtech.archunit.library.Architectures.layeredArchitecture;

@AnalyzeClasses(packages = "com.example")
class LayeredArchitectureTest {

    @ArchTest
    static final ArchRule layered_architecture_is_respected = layeredArchitecture()
        .consideringAllDependencies()
        .layer("Controller").definedBy("..web..")
        .layer("Service").definedBy("..service..")
        .layer("Repository").definedBy("..repository..")

        .whereLayer("Controller").mayNotBeAccessedByAnyLayer()
        .whereLayer("Service").mayOnlyBeAccessedByLayers("Controller")
        .whereLayer("Repository").mayOnlyBeAccessedByLayers("Service");
}
```

---

## 5. Practical Examples

### Example 1: Enforcing Package Cycle Prevention (Acyclic Dependencies)
Ensuring that domain slices and feature packages remain free of circular dependencies.

```java
package com.example.architecture;

import com.tngtech.archunit.junit.AnalyzeClasses;
import com.tngtech.archunit.junit.ArchTest;
import com.tngtech.archunit.lang.ArchRule;

import static com.tngtech.archunit.library.dependencies.SlicesRuleDefinition.slices;

@AnalyzeClasses(packages = "com.example")
class PackageCyclesTest {

    @ArchTest
    static final ArchRule packages_must_be_free_of_cycles = slices()
        .matching("com.example.(*)..")
        .should().beFreeOfCycles();
}
```

### Example 2: Naming Conventions & Annotation Rules
Enforcing that classes ending in `Controller` reside in `web` packages and are annotated with `@RestController`.

```java
package com.example.architecture;

import com.tngtech.archunit.junit.AnalyzeClasses;
import com.tngtech.archunit.junit.ArchTest;
import com.tngtech.archunit.lang.ArchRule;
import org.springframework.stereotype.Service;
import org.springframework.web.bind.annotation.RestController;

import static com.tngtech.archunit.lang.syntax.ArchRuleDefinition.classes;

@AnalyzeClasses(packages = "com.example")
class ConventionRulesTest {

    @ArchTest
    static final ArchRule controllers_must_reside_in_web_package = classes()
        .that().haveSimpleNameEndingWith("Controller")
        .should().resideInAPackage("..web..")
        .andShould().beAnnotatedWith(RestController.class);

    @ArchTest
    static final ArchRule services_must_be_annotated_with_service = classes()
        .that().haveSimpleNameEndingWith("ServiceImpl")
        .should().beAnnotatedWith(Service.class);
}
```

### Example 3: Edge Case – Forbidding `@Transactional` on Controllers
Preventing long-lived database transactions from holding database connections open during HTTP serialization.

```java
package com.example.architecture;

import com.tngtech.archunit.junit.AnalyzeClasses;
import com.tngtech.archunit.junit.ArchTest;
import com.tngtech.archunit.lang.ArchRule;
import org.springframework.transaction.annotation.Transactional;

import static com.tngtech.archunit.lang.syntax.ArchRuleDefinition.classes;
import static com.tngtech.archunit.lang.syntax.ArchRuleDefinition.methods;

@AnalyzeClasses(packages = "com.example")
class TransactionalRulesTest {

    @ArchTest
    static final ArchRule controllers_must_not_be_transactional = classes()
        .that().resideInAPackage("..web..")
        .should().notBeAnnotatedWith(Transactional.class);

    @ArchTest
    static final ArchRule controller_methods_must_not_be_transactional = methods()
        .that().areDeclaredInClassesThat().resideInAPackage("..web..")
        .should().notBeAnnotatedWith(Transactional.class);
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Using deprecated or banned APIs across the codebase (e.g., using `java.util.Date` instead of `java.time.Instant`, or using `System.out.println` instead of SLF4J).

```java
// BUGGY & UNGOVERNED: Developers use System.out.println and legacy Date classes!
System.out.println("Processing: " + new java.util.Date());
```

**Production Fix**:
Enforce coding standards via ArchUnit to fail builds whenever banned APIs are accessed.

```java
package com.example.architecture;

import com.tngtech.archunit.junit.AnalyzeClasses;
import com.tngtech.archunit.junit.ArchTest;
import com.tngtech.archunit.lang.ArchRule;

import static com.tngtech.archunit.library.GeneralCodingRules.*;

@AnalyzeClasses(packages = "com.example")
class CodingStandardsTest {

    @ArchTest
    static final ArchRule no_system_out_logging = NO_CLASSES_SHOULD_ACCESS_STANDARD_STREAMS;

    @ArchTest
    static final ArchRule no_generic_exceptions = NO_CLASSES_SHOULD_THROW_GENERIC_EXCEPTIONS;

    @ArchTest
    static final ArchRule no_java_util_logging = NO_CLASSES_SHOULD_USE_JAVA_UTIL_LOGGING;
}
```

### Example 5: Domain-Driven Design (Hexagonal / Onion) Architecture Rule
Enforcing that domain model entities and core logic depend on zero external frameworks or infrastructure adapters.

```java
package com.example.architecture;

import com.tngtech.archunit.junit.AnalyzeClasses;
import com.tngtech.archunit.junit.ArchTest;
import com.tngtech.archunit.lang.ArchRule;

import static com.tngtech.archunit.library.Architectures.onionArchitecture;

@AnalyzeClasses(packages = "com.example")
class HexagonalArchitectureTest {

    @ArchTest
    static final ArchRule hexagonal_architecture_is_strictly_enforced = onionArchitecture()
        .domainModels("com.example.domain.model..")
        .domainServices("com.example.domain.service..")
        .applicationServices("com.example.application..")
        .adapter("persistence", "com.example.adapter.persistence..")
        .adapter("rest", "com.example.adapter.rest..");
}
```

---

## 6. Best Practices

1. **Run ArchUnit in CI Pipelines**: Treat architectural rules as standard unit tests that must pass before PRs can be merged.
2. **Enforce Layer Separation Early**: Establish layer rules (`Controller -> Service -> Repository`) at project inception to prevent technical debt.
3. **Use Built-in General Coding Rules**: Leverage `GeneralCodingRules.NO_CLASSES_SHOULD_ACCESS_STANDARD_STREAMS` and `NO_CLASSES_SHOULD_THROW_GENERIC_EXCEPTIONS`.
4. **Use Freezing for Legacy Codebases**: If refactoring a legacy codebase, use ArchUnit's `FreezingArchRule` (`Freezer`) to record existing violations so builds pass while preventing new violations.
5. **Enforce Single Package Cycles**: Keep packages modular by prohibiting circular dependencies with `slices().beFreeOfCycles()`.
6. **Disallow Field Injection**: Use ArchUnit to mandate constructor injection by asserting that no fields are annotated with `@Autowired`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Analyzing wrong package root | Specifying `@AnalyzeClasses(packages = "com.wrong")` passes tests with 0 classes analyzed. | Verify the root package path carefully. |
| Forgetting `static` on `@ArchTest` | ArchUnit JUnit 5 runner ignores non-static rule fields. | Declare all `@ArchTest` fields `static final`. |
| Running against uncompiled code | Bytecode must exist in `target/classes` for ArchUnit to parse. | Ensure `mvn compile` or `gradle classes` runs before testing. |
| Overly restrictive rules | Blocking all access to utility classes creates developer frustration. | Use `resideOutsideOfPackage("..util..")` exclusions. |
| Bypassing rules via reflection | ArchUnit analyzes static bytecode; dynamic reflection calls require custom predicate checks. | Ban reflection libraries in domain packages. |
| Neglecting third-party imports | Dependencies on third-party libraries inside domain models pass unnoticed. | Enforce `onlyDependOnClassesThat().resideInAnyPackage("java..", "com.example..")`. |

---

## 8. Performance & Security Notes

- **Blazing Fast Execution**: Because ArchUnit analyzes compiled `.class` files via ASM without booting the Spring container, a suite of 30 architectural rules analyzes 500 classes in less than 500 milliseconds.
- **Zero Production Footprint**: ArchUnit is strictly a test dependency (`<scope>test</scope>`) with zero impact on production runtime or JAR size.
- **Security Guardrails**: ArchUnit can enforce that no class accesses unapproved cryptography algorithms (e.g., forbidding `MessageDigest.getInstance("MD5")`).

---

## 9. Comparison / When to Use What

| Tool | Focus | Analysis Level | Speed | Best Use Case |
|---|---|---|---|---|
| **ArchUnit** | Architectural Rules & Layering | **Java Bytecode (.class)** | **Very Fast (< 500ms)**| Layering, package cycles, naming conventions |
| **SonarQube** | Code Quality & Vulnerabilities | AST & Source Code | Slower (CI scan) | Security hotspots, duplication, test coverage |
| **Checkstyle** | Formatting & Linting | Source Code Text | Fast | Whitespace, indentation, line lengths |
| **SpotBugs** | Bug Patterns & Flaws | Java Bytecode | Medium | Null pointer bugs, resource leaks |

---

## 10. Interview Questions

<details>
<summary>1. What is an Architectural Fitness Function and how does ArchUnit implement it?</summary>

**Answer:**
An Architectural Fitness Function (a term popularized by Neal Ford and Rebecca Parsons) is an automated test that provides an objective, measurable assessment of whether an application's architecture conforms to defined design constraints. ArchUnit implements this by parsing compiled Java bytecode into an in-memory graph and providing a fluent assertion DSL to evaluate constraints (such as layer separation, acyclic package graphs, and naming conventions) as standard JUnit tests.
</details>

<details>
<summary>2. How does ArchUnit analyze Java classes without loading them into the JVM ClassLoader?</summary>

**Answer:**
ArchUnit uses the **ASM bytecode manipulation library** to read the raw `.class` binary files directly from the file system. It extracts metadata (class names, package names, method signatures, field accesses, annotations, and instructions) without invoking `ClassLoader.loadClass()`. This prevents static initializer side effects and allows ArchUnit to analyze code safely and rapidly.
</details>

<details>
<summary>3. Why should `@Transactional` be prohibited on Spring MVC controller classes?</summary>

**Answer:**
Placing `@Transactional` on a controller holds an open database connection from the moment the HTTP request reaches the controller until after the response body is completely serialized. If response serialization takes time (e.g., writing large JSON to a slow client), the database connection pool is needlessly exhausted. Transactions should start and end strictly within the service layer.
</details>

<details>
<summary>4. What is the ArchUnit "Freezing" feature?</summary>

**Answer:**
`FreezingArchRule` is a feature designed for legacy applications that already contain hundreds of architectural violations. Freezing takes a snapshot of existing violations and saves them to a text file. The rule passes as long as no **new** violations are introduced, allowing teams to enforce architectural governance on new code while incrementally refactoring legacy violations.
</details>

<details>
<summary>5. How do you prevent field injection (`@Autowired` on fields) using ArchUnit?</summary>

**Answer:**
```java
@ArchTest
static final ArchRule no_field_injection = NO_CLASSES_SHOULD_USE_FIELD_INJECTION;
// Or explicitly:
fields().should().notBeAnnotatedWith(Autowired.class);
```
This forces developers to use constructor injection across the entire project.
</details>

<details>
<summary>6. How do you detect and forbid circular package dependencies in ArchUnit?</summary>

**Answer:**
Using the `SlicesRuleDefinition` API:
```java
@ArchTest
static final ArchRule no_package_cycles = slices()
    .matching("com.example.(*)..")
    .should().beFreeOfCycles();
```
ArchUnit traverses the package dependency graph and fails if any cycle (e.g., Package $A \to B \to C \to A$) is discovered.
</details>

<details>
<summary>7. What is the difference between `layeredArchitecture()` and `onionArchitecture()` in ArchUnit?</summary>

**Answer:**
- `layeredArchitecture()` enforces strict top-down hierarchical layers (e.g., Controller can only access Service, Service can only access Repository).
- `onionArchitecture()` enforces Domain-Driven Design (DDD) circular layers where the core Domain Model and Domain Services have zero dependencies on outer application services or infrastructure adapters.
</details>

<details>
<summary>8. Does ArchUnit slow down build execution times?</summary>

**Answer:**
No. Because ArchUnit operates entirely in memory on compiled bytecode without launching Spring containers, embedded databases, or web servers, dozens of architectural rules evaluate hundreds of classes in less than 1 second.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Forbid Field Injection (Easy)</summary>

**Task**: Write an ArchUnit rule asserting that no field in the entire codebase is annotated with `@Autowired`.

**Solution**:
```java
@ArchTest
static final ArchRule no_field_injection = fields()
    .should().notBeAnnotatedWith(Autowired.class);
```
</details>

<details>
<summary>Exercise 2: Repository Naming Convention (Easy)</summary>

**Task**: Write a rule verifying that any class in a `..repository..` package must have a simple name ending with `"Repository"`.

**Solution**:
```java
@ArchTest
static final ArchRule repository_naming_convention = classes()
    .that().resideInAPackage("..repository..")
    .should().haveSimpleNameEndingWith("Repository");
```
</details>

<details>
<summary>Exercise 3: Forbid System.out and System.err (Medium)</summary>

**Task**: Write an ArchUnit test ensuring that no class in package `com.example` accesses standard output or error streams.

**Solution**:
```java
@ArchTest
static final ArchRule no_standard_streams = GeneralCodingRules.NO_CLASSES_SHOULD_ACCESS_STANDARD_STREAMS;
```
</details>

<details>
<summary>Exercise 4: Layer Access Isolation (Medium)</summary>

**Task**: Write an ArchRule ensuring that classes in `..web..` packages never access classes in `..repository..` packages directly.

**Solution**:
```java
@ArchTest
static final ArchRule controllers_must_not_access_repositories = noClasses()
    .that().resideInAPackage("..web..")
    .should().accessClassesThat().resideInAPackage("..repository..");
```
</details>

<details>
<summary>Exercise 5: Domain Model Purity (Hard)</summary>

**Task**: Write an ArchUnit test asserting that classes residing in `com.example.domain..` only depend on classes in `java..` packages and the domain itself (zero Spring or external framework dependencies).

**Solution**:
```java
@ArchTest
static final ArchRule domain_model_must_be_framework_independent = classes()
    .that().resideInAPackage("com.example.domain..")
    .should().onlyDependOnClassesThat()
    .resideInAnyPackage("com.example.domain..", "java..");
```
</details>

<details>
<summary>Exercise 6: Circular Dependency Prohibition (Hard)</summary>

**Task**: Write an ArchUnit test ensuring that all subpackages under `com.example.modules.(*)..` are completely free of cyclic dependencies.

**Solution**:
```java
@ArchTest
static final ArchRule modules_must_be_free_of_cycles = SlicesRuleDefinition.slices()
    .matching("com.example.modules.(*)..")
    .should().beFreeOfCycles();
```
</details>

---

## 12. Summary / Cheat Sheet

- **Automated Architecture Governance**: ArchUnit tests architectural rules as standard JUnit 5 tests.
- **Bytecode Inspection**: Uses ASM to evaluate compiled `.class` files in milliseconds without booting Spring.
- **Layer & Onion Enforcement**: Built-in DSLs enforce layered boundaries and DDD onion architectures.
- **Prevent Technical Debt**: Stop cyclic dependencies (`beFreeOfCycles()`) and field injection before PRs merge.
- **Fail Fast in CI**: Catches layer violations in milliseconds on every local build.

---

## 13. Further Reading
- [ArchUnit Official User Guide](https://www.archunit.org/userguide/html/000_Index.html)
- [Building Evolutionary Architectures by Neal Ford](https://www.oreilly.com/library/view/building-evolutionary-architectures/9781491986356/)
- [ArchUnit Examples Repository](https://github.com/TNG/ArchUnit-Examples)
- [Domain-Driven Design Reference by Eric Evans](https://www.domainlanguage.com/ddd/reference/)
- [ASM Bytecode Manipulation Framework](https://asm.ow2.io/)
