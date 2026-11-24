# Module 13: Testing & Quality Assurance in Node.js

> Complete guide to unit testing, integration testing, `node:test`, Vitest, Supertest, Testcontainers, and mocking.

---

## Module Overview

Testing backend services requires verifying individual pure units in isolation, testing database queries against real Docker engines, asserting HTTP endpoints via Supertest, and eliminating flaky tests.

---

## Contents

| Index | Note | Core Focus |
| :---: | :--- | :--- |
| **01** | [01-testing-fundamentals.md](./01-testing-fundamentals.md) | Testing pyramid, regression defense, test doubles (mocks, stubs, fakes). |
| **02** | [02-unit-testing.md](./02-unit-testing.md) | Unit test principles, isolating domain units, test speed. |
| **03** | [03-integration-testing.md](./03-integration-testing.md) | Testing multiple collaborating components, boundary integration. |
| **04** | [04-vitest-and-jest.md](./04-vitest-and-jest.md) | Modern testing with Vitest: ESM native, fast watch mode, configuration. |
| **05** | [05-api-testing-with-supertest.md](./05-api-testing-with-supertest.md) | End-to-end HTTP API testing with Supertest against Express/Fastify. |
| **06** | [06-test-databases-and-coverage.md](./06-test-databases-and-coverage.md) | Managing test databases, transaction rollbacks, code coverage thresholds. |
| **07** | [07-mocking-and-test-doubles.md](./07-mocking-and-test-doubles.md) | Mocking timers, HTTP calls (MSW), and module boundaries. |
| **08** | [08-testing-patterns.md](./08-testing-patterns.md) | Testing patterns: AAA (Arrange, Act, Assert), test factories, fixtures. |
| **09** | [09-unit-and-integration-guide.md](./09-unit-and-integration-guide.md) | Deep technical handbook on mock doubles, async testing, and ESM mocks. |
| **10** | [10-node-test-runner.md](./10-node-test-runner.md) | Zero-dependency native `node:test` runner, watch mode, coverage. |

---

## Next Steps
Proceed to [14-logging-observability](../14-logging-observability/README.md) to explore Pino, Prometheus, and OpenTelemetry distributed tracing.
