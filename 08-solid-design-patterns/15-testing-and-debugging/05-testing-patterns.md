# Testing Patterns

| Field | Value |
| :--- | :--- |
| **Category** | Testing & Quality |

> **TL;DR**: Beyond basic unit tests, **Object Mother / Test Data Builders**, **Contract Tests**, and **Property-Based Testing** keep test suites readable, decoupled from constructors, and capable of catching subtle edge cases.

## Key Testing Patterns
1. **Arrange–Act–Assert (AAA)**: Clear 3-part structure in every test.
2. **Test Data Builder**: `anOrder().withTotal(500).paid().build()` prevents 20 tests from breaking when `Order` adds a new constructor field.
3. **Consumer-Driven Contract Testing (Pact)**: Verifies API request/response schemas between microservices without spinning up all services in E2E tests.
4. **Property-Based Testing (`jqwik`, `fast-check`, `hypothesis`)**: Generates hundreds of random/edge inputs to test invariants (`decode(encode(x)) == x`).
