# Module 18: Testing and Debugging

> Comprehensive guide and curriculum for unit testing, integration testing, mocking, test runners, type testing, and debugging in TypeScript.

## Overview
This module covers modern testing and debugging strategies across the entire development lifecycle, from fast unit tests and containerized integration tests to compile-time type assertions and V8 debugger protocols.

## Module Structure

| Note | Title | Description | Baseline |
| :--- | :--- | :--- | :--- |
| **[00-unit-testing.md](./00-unit-testing.md)** | Unit Testing | Testing pure functions, AAA pattern, assertions, and parameterized tests with Vitest. | TS 4.5+ |
| **[01-integration-testing.md](./01-integration-testing.md)** | Integration Testing | Testing databases and APIs with Supertest, Testcontainers, and transaction rollbacks. | TS 4.5+ |
| **[02-mocking.md](./02-mocking.md)** | Mocking & Test Doubles | Vitest spies (`vi.fn`, `vi.spyOn`), MSW network mocking, and fake timers. | TS 4.5+ |
| **[03-test-runners.md](./03-test-runners.md)** | Test Runners | Comparing Vitest, Jest (@swc/jest), and Node.js native test runner (`node:test`). | TS 4.5+ |
| **[04-type-testing.md](./04-type-testing.md)** | Type Testing | Vitest `expectTypeOf`, `tsd`, testing complex generics, and `@ts-expect-error`. | TS 4.5+ |
| **[05-debugging.md](./05-debugging.md)** | Debugging Workflows | Source maps, VS Code launch configurations, Node inspector, and Chrome DevTools. | TS 4.5+ |
| **[06-reading-type-errors.md](./06-reading-type-errors.md)** | Reading Type Errors | Deconstructing compiler diagnostics, error code taxonomy (TS2322, TS2345, TS7053). | TS 4.5+ |

## Learning Path & Prerequisites
- Complete **[16-type-safe-apis](../16-type-safe-apis/README.md)** and **[17-design-patterns](../17-design-patterns/README.md)** before starting this module.
- After completing this module, proceed to **[19-react-and-frontend](../19-react-and-frontend/README.md)** and **[20-nodejs-backend](../20-nodejs-backend/README.md)**.
