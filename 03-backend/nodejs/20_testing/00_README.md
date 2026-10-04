# 21 · Testing

Tests are executable specifications: they describe what your code should do and tell you, within seconds, when a change breaks it. Good tests let you refactor with confidence, document behavior, and catch regressions before users do. Bad tests are slow, flaky, and break whenever you touch the implementation, so people stop trusting them.

This chapter teaches what to test, how to structure tests, how to use **Vitest** and **Jest**, how to replace dependencies with fakes and mocks, and the patterns that keep a test suite fast and maintainable.

## What you will learn

- Why we test, the test pyramid, and what makes a test good
- Writing unit tests: arrange-act-assert, edge cases, pure functions, async code
- Writing integration tests: real modules working together, HTTP APIs, databases
- Using Vitest and Jest: configuration, matchers, hooks, coverage, watch mode
- Test doubles: stubs, spies, mocks, fakes, fake timers, module mocking
- Patterns for readable, reliable suites: builders, parameterized tests, snapshots, contract tests

## Contents

| # | File | Topic |
|---|------|-------|
| 01 | [Testing Fundamentals](./01_testing-fundamentals.md) | Why test, test pyramid, anatomy of a test, TDD, coverage, what to test |
| 02 | [Unit Testing](./02_unit-testing.md) | Isolated tests, AAA, edge cases, async code, errors, `node:test` |
| 03 | [Integration Testing](./03_integration-testing.md) | Modules together, HTTP and database tests, test environments |
| 04 | [Vitest and Jest](./04_vitest-and-jest.md) | Setup, matchers, hooks, mocks API, coverage, CI |
| 05 | [Mocking](./05_mocking.md) | Test doubles, spies, fake timers, module mocks, network mocking |
| 06 | [Testing Patterns](./06_testing-patterns.md) | Builders, table-driven tests, snapshots, property tests, flaky tests |

## Prerequisites

- [Functions](../02_functions/00_README.md) and [Error Handling](../10_error-handling/00_README.md)
- [Asynchronous JavaScript](../11_asynchronous-javascript/00_README.md): promises and `async`/`await`
- [Modules](../13_modules/00_README.md): ES modules and CommonJS
- [Dependency Injection](../20_design-patterns/09_dependency-injection.md): makes code testable
- [Node.js](../16_nodejs/00_README.md) basics

## Quick start

```bash
mkdir testing-playground && cd testing-playground
npm init -y
npm pkg set type=module
npm install --save-dev vitest
```

```js
// sum.js
export const sum = (a, b) => a + b;
```

```js
// sum.test.js
import { test, expect } from 'vitest';
import { sum } from './sum.js';

test('adds two numbers', () => {
  expect(sum(2, 3)).toBe(5);
});
```

```bash
npx vitest          # watch mode
npx vitest run      # run once (CI)
```

Prefer no dependencies? Node has a built-in runner:

```js
import test from 'node:test';
import assert from 'node:assert/strict';
import { sum } from './sum.js';

test('adds two numbers', () => {
  assert.equal(sum(2, 3), 5);
});
```

```bash
node --test
```

## Which tool?

| Need | Choice |
|------|--------|
| Fast, modern, ESM-first, works with Vite | **Vitest** |
| Large existing project, React Native, huge ecosystem | **Jest** |
| Zero dependencies, simple libraries and scripts | **`node:test`** |
| Browser end-to-end tests | Playwright (or Cypress) |
| Component tests (DOM) | Testing Library with Vitest/Jest |

## Key takeaways

- Tests protect behavior; they should fail for real bugs and stay green during harmless refactors
- Favor many fast unit tests, a smaller number of integration tests, and few end-to-end tests
- Test **behavior through public interfaces**, not private implementation details
- Dependency injection and small pure functions make code easy to test
- Prefer fakes and real collaborators over heavy mocking; mock at the boundaries (network, time, randomness)
- Keep tests fast, deterministic, independent, and readable

**Next:** [Testing Fundamentals](./01_testing-fundamentals.md)
