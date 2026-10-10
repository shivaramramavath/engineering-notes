# 19 · Testing

Tests are how you change code without being afraid of it. Good tests catch regressions in seconds, document behavior, and make refactoring and upgrades routine. Bad tests are slow, flaky, tied to implementation details, and cost more to maintain than the bugs they catch. This module is about writing the first kind.

It covers the mindset and structure of unit tests, the tools (JUnit, Mockito, AssertJ, Testcontainers), and the techniques that make a test suite trustworthy: integration tests against real infrastructure, reusable patterns, and honest measurement with coverage and mutation testing.

## Contents

| # | Note | Focus |
|---|------|-------|
| 00 | [Unit Testing Fundamentals](00_unit-testing-fundamentals.md) | What a good unit test is, Arrange-Act-Assert, test doubles, testable design, edge cases, smells |
| 01 | [JUnit 5 and 6](01_junit-5.md) | Jupiter programming model: lifecycle, assertions, parameterized tests, extensions, parallelism, migration |
| 02 | [Mocking and Mockito](02_mocking-and-mockito.md) | Stubbing, verification, captors, strictness, when *not* to mock |
| 03 | [Integration Testing](03_integration-testing.md) | Testcontainers, Spring Boot test slices, databases, HTTP, async, flakiness |
| 04 | [Testing Patterns](04_testing-patterns.md) | Builders, fakes and contract tests, time/randomness/concurrency, legacy code, property-based testing |
| 05 | [Code Coverage and Mutation Testing](05_code-coverage-and-mutation-testing.md) | JaCoCo, what coverage can and can't tell you, PIT |

## The shape of a healthy suite

```text
                  ▲  few, slow, broad
                 ╱ ╲   end-to-end / system tests    (a handful: critical user journeys)
                ╱───╲
               ╱     ╲  integration tests           (real DB / HTTP / broker: Testcontainers)
              ╱───────╲
             ╱         ╲ unit tests                 (many, in-memory, milliseconds)
            ╱───────────╲
                  ▼  many, fast, narrow
```

Most behavior is verified by **fast unit tests**. **Integration tests** cover what unit tests can't (SQL, serialization, wiring, configuration). A **few end-to-end tests** guard the critical paths. Inverting the pyramid (mostly slow end-to-end tests) produces suites nobody wants to run.

## Tool snapshot (October 2026)

- **JUnit 6** (released September 2025) is the current major version of the JUnit framework. It requires **Java 17+**, uses one version number for the Platform and Jupiter, and keeps the same `org.junit.jupiter.*` packages as JUnit 5, so JUnit 5 knowledge and code carry over. Projects on older Java stay on **JUnit 5.x**. Spring Boot 4 manages JUnit 6.
- **Mockito 5.x** (inline mock maker by default), **AssertJ** for fluent assertions.
- **Testcontainers 2.x**: JUnit 4 support removed, artifacts renamed with a `testcontainers-` prefix, container classes moved to `org.testcontainers.<module>` packages.
- Library versions change quickly. Check each project's current documentation.

## Running tests

```bash
./mvnw test                      # unit tests (surefire)
./mvnw verify                    # + integration tests (failsafe, *IT classes)
./mvnw -Dtest=OrderServiceTest test
./gradlew test
./gradlew test --tests "*OrderService*"
```

See [Maven](../18-build-and-dependencies/00_maven.md#5-plugins) and [Gradle](../18-build-and-dependencies/01_gradle.md) for how these plugins are configured, and [CI/CD](../18-build-and-dependencies/03_ci-cd-pipelines.md#4-test-stages-in-the-pipeline) for how the stages fit into a pipeline.

## Prerequisites

[Exceptions](../06-exceptions-and-debugging/README.md), [Interfaces](../04-oop/09_interfaces.md) and [Polymorphism](../04-oop/07_polymorphism.md) (the basis of test doubles), [Lambda Expressions](../09-functional-java/00_lambda-expressions.md), [Annotations](../13-advanced-language-features/00_annotations.md), and [Maven](../18-build-and-dependencies/00_maven.md) or [Gradle](../18-build-and-dependencies/01_gradle.md).

## Related

[Immutability](../23-design-and-clean-code/04_immutability.md) · [Defensive Programming](../23-design-and-clean-code/05_defensive-programming.md) · [Refactoring and Code Smells](../23-design-and-clean-code/03_refactoring-and-code-smells.md) · [JDBC Patterns](../16-jdbc-and-databases/06_jdbc-patterns.md#8-testing-database-code)

**Next module:** [Performance](../20-performance/README.md)