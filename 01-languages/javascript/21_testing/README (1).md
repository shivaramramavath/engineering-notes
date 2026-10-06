# 21 · Testing

Automated testing for JavaScript: what to test, how to structure tests, and how to use Vitest/Jest effectively. The goal is to ship changes confidently, not to hit a coverage number.

## Learning Order

| # | Note | What you'll learn |
|---|---|---|
| 01 | [Testing Fundamentals](./01_testing-fundamentals.md) | Why test, test types, Arrange/Act/Assert, flaky tests, coverage myths |
| 02 | [Unit Testing](./02_unit-testing.md) | Testing functions and classes, matchers, async, edge cases, table-driven tests |
| 03 | [Integration Testing](./03_integration-testing.md) | Testing real collaboration: HTTP APIs, databases, faking external services |
| 04 | [Vitest and Jest](./04_vitest-and-jest.md) | Choosing a runner, config, shared API, snapshots, setup problems |
| 05 | [Mocking](./05_mocking.md) | Stubs, spies, mocks, fakes, module mocking, fake timers, network mocking |
| 06 | [Testing Patterns](./06_testing-patterns.md) | Factories, testable design, time-based and event testing, property-based tests |

## Prerequisites

- [Functions](../02_functions/README.md)
- [Error handling](../10_error-handling/README.md)
- [Asynchronous JavaScript](../11_asynchronous-javascript/README.md)
- [Modules](../13_modules/README.md): ESM vs CommonJS explains most runner setup issues

## Suggested Paths

- **Just starting:** 01 → 02 → 04 (then write tests for a small project)
- **Working on backend/API code:** 03 → 05 → 06
- **Interview prep:** 01 (types, pyramid, flakiness), 02 (edge cases, async), 05 (doubles, mock vs spy), 06 (testable design)

## Related Topics

- [Dependency injection](../20_design-patterns/09_dependency-injection.md): the main technique for making code testable
- [Real-world patterns](../23_real-world-patterns/README.md): debounce, throttle, retry, and timeout are good practice targets for fake timers
- [Security](../22_security/README.md): validate input, and test it

## Core Principles

1. Test **behavior**, not implementation.
2. Keep tests **fast, isolated, and deterministic**.
3. Mock at **boundaries**; avoid over-mocking.
4. Every fixed bug gets a **regression test**.
5. Coverage reveals untested code; it doesn't prove quality.
