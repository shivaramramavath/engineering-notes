# Testing

Nest is designed to be testable: constructor injection means any dependency can be swapped for a fake, and `@nestjs/testing` lets you build a miniature version of your module graph inside a test. This section covers what to test at each level, how to isolate things with mocks, how to test the pieces of the [request pipeline](../../03-core-concepts/01-request-pipeline/README.md), and how to run realistic integration and end-to-end tests.

```text
          fewer, slower, more realistic
                      ▲
            ┌─────────────────┐
            │       E2E       │  HTTP → app → real DB (supertest)
            ├─────────────────┤
            │   Integration   │  modules + real DB, no HTTP
            ├─────────────────┤
            │      Unit       │  one class, dependencies mocked
            └─────────────────┘
                      ▼
          more, faster, more isolated
```

> Applies to NestJS 10/11 with the default Jest + Supertest setup that `nest new` scaffolds.

## Reading order

| # | Note | Answers |
|---|------|---------|
| 01 | [Testing fundamentals](./01-testing-fundamentals.md) | Test types, project setup, `Test.createTestingModule`, what to test where |
| 02 | [Unit testing](./02-unit-testing.md) | Services and controllers in isolation |
| 03 | [Mocking](./03-mocking.md) | Test doubles, Jest mocks, `overrideProvider`, typed mocks, fakes |
| 04 | [Testing pipeline components](./04-testing-pipeline-components.md) | Guards, pipes, interceptors, filters, middleware |
| 05 | [Integration testing](./05-integration-testing.md) | Real database, cleanup, isolation, parallel runs |
| 06 | [E2E testing](./06-e2e-testing.md) | Supertest, mirroring `main.ts`, auth, debugging hangs |

## Prerequisites

- [Dependency injection](../../02-fundamentals/05-dependency-injection.md) and [custom providers](../../03-core-concepts/04-modules-and-di/05-custom-providers.md)
- [Pipeline overview](../../03-core-concepts/01-request-pipeline/01-pipeline-overview-and-execution-order.md)

## Related

- [Database foundations](../02-database-foundations/README.md) (repositories, transactions, migrations used in integration tests)
- [Testing quick reference](../../13-quick-reference/13-testing.md)
- [Interview questions: testing](../../12-interview-preparation/06-testing.md)