# JavaScript Testing Fundamentals

> Testing Pyramid vs Testing Trophy, the AAA pattern (Arrange-Act-Assert), Node.js native test runner, assertions, and test hygiene.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [Asynchronous JavaScript](file:///e:/Learning/engineering-notes/01-languages/javascript/11_asynchronous-javascript/01_asynchronous-model.md), [Error Handling](file:///e:/Learning/engineering-notes/01-languages/javascript/10_error-handling/01_errors.md) |
| **Time to read** | 24 minutes |
| **Status** | Complete |
| **Standard / Runtime** | ECMAScript 2015+, Node.js `node:test` (v18+ LTS - v24+) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In modern software development, automated testing is the foundational safeguard of codebase stability. Without automated test suites, every refactor, dependency upgrade, performance optimization, or bug fix requires manual regression testing, which is slow, expensive, and error-prone.

Automated testing is not an attempt to prove that software is mathematically free of bugs; rather, it is an empirical verification harness that **cheaply detects regressions**, guarantees contract invariants, and serves as executable, living documentation for the system.

Understanding testing fundamentals—the **Testing Pyramid** vs **Testing Trophy**, the **Arrange-Act-Assert (AAA)** structure, deterministic test environments, and native assertion mechanics—empowers teams to build fast, reliable CI/CD pipelines that deploy code to production with high confidence.

---

## 2. Core Concepts

### The Testing Pyramid vs The Testing Trophy
- **The Classic Testing Pyramid (Mike Cohn)**: Advocates for a massive base of fast, isolated **Unit Tests**, a smaller layer of **Integration Tests**, and a very thin top layer of slow, flaky **End-to-End (E2E) UI Tests**.
- **The Testing Trophy (Kent C. Dodds)**: Argues that in modern web applications, unit tests often over-focus on trivial implementation details. Instead, it prioritizes **Integration Tests** as the sweet spot of maximum confidence per development effort:
  ```text
            [ E2E ]          -> Few, high confidence, slow
       [ Integration ]       -> MAJORITY OF TESTS: verifies modules work together
         [ Unit ]            -> Edge cases, complex algorithms, pure math
        [ Static ]           -> TypeScript, ESLint, Prettier (type & syntax bugs)
  ```

### The AAA Pattern (Arrange-Act-Assert)
Every well-structured test follows three distinct, sequential phases:
1. **Arrange**: Set up test preconditions, seed inputs, initialize fakes, and configure state.
2. **Act**: Invoke the single method or function under test.
3. **Assert**: Verify that the actual output matches expected return values or state mutations.

### Node.js Native Test Runner (`node:test`)
Since Node.js v18 (stabilized in v20+), Node provides a native, zero-dependency test runner (`node:test`) and assertion library (`node:assert`). It supports suites (`describe`), tests (`it`/`test`), lifecycle hooks (`before`, `after`, `beforeEach`, `afterEach`), mocking, and native TAP/spec reporting without requiring third-party npm packages like Jest.

---

## 3. How It Works Internally

```
+--------------------------------------------------------------------------+
| TEST RUNNER LIFECYCLE (node --test / Vitest / Jest)                      |
+--------------------------------------------------------------------------+
                                    |
  1. Discovery Phase                | Scans filesystem for *.test.js, *.spec.js
                                    v
+--------------------------------------------------------------------------+
| ISOLATED TEST WORKER (Worker Thread / Sub-process)                       |
| - Executes `beforeAll` / `before` hooks                                  |
|                                                                          |
|   +------------------------------------------------------------------+   |
|   | TEST CASE 1: it('should calculate tax')                          |   |
|   | - 1. Arrange: const order = { amount: 100 }                      |   |
|   | - 2. Act:     const tax = calculateTax(order)                    |   |
|   | - 3. Assert:  assert.strictEqual(tax, 10)                        |   |
|   |      (Throws AssertionError if actual !== expected)              |   |
|   +------------------------------------------------------------------+   |
|                                                                          |
| - Executes `afterAll` / `after` cleanup hooks                            |
+--------------------------------------------------------------------------+
                                    |
                                    v
+--------------------------------------------------------------------------+
| REPORTER (TAP / Spec Reporter)                                           |
| ✔ should calculate tax (1.2ms)                                           |
| Tests: 1 passed, 0 failed, 1 total                                       |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Built-in Node.js Test Runner API (`node:test` & `node:assert`)
```javascript
import { describe, it, before, after, beforeEach, afterEach } from 'node:test';
import assert from 'node:assert/strict'; // Always use strict assertions!

describe('Math Operations', () => {
  beforeEach(() => {
    // Runs before every test in this suite
  });

  it('correctly sums two numbers', () => {
    // Arrange
    const a = 10;
    const b = 20;

    // Act
    const result = a + b;

    // Assert
    assert.strictEqual(result, 30);
  });

  it('rejects division by zero', () => {
    assert.throws(
      () => divide(10, 0),
      { name: 'RangeError', message: 'Division by zero' }
    );
  });

  it('resolves asynchronous promises', async () => {
    const data = await fetchUserData(101);
    assert.deepStrictEqual(data, { id: 101, name: 'Alice' });
  });
});
```

### Running Native Node.js Tests
```bash
# Execute all *.test.js files in the project
node --test

# Execute with watch mode (re-runs on file save)
node --test --watch

# Filter tests by name regex
node --test --test-name-pattern="Math Operations"
```

---

## 5. Practical Examples

### Example 1: Testing Edge Cases and Invariants
```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

function calculateDiscount(price, percentage) {
  if (typeof price !== 'number' || price < 0) throw new TypeError('Invalid price');
  if (typeof percentage !== 'number' || percentage < 0 || percentage > 100) {
    throw new RangeError('Percentage must be 0-100');
  }
  return Math.round((price * (1 - percentage / 100)) * 100) / 100;
}

describe('calculateDiscount', () => {
  it('calculates standard discount correctly', () => {
    assert.strictEqual(calculateDiscount(100, 20), 80);
    assert.strictEqual(calculateDiscount(49.99, 10), 44.99);
  });

  it('handles 0% and 100% boundary conditions', () => {
    assert.strictEqual(calculateDiscount(100, 0), 100);
    assert.strictEqual(calculateDiscount(100, 100), 0);
  });

  it('throws on invalid inputs', () => {
    assert.throws(() => calculateDiscount(-10, 20), TypeError);
    assert.throws(() => calculateDiscount(100, -5), RangeError);
    assert.throws(() => calculateDiscount(100, 105), RangeError);
  });
});
```

### Example 2: Testing Asynchronous Operations with Promises and Rejections
```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

async function fetchUserById(id) {
  if (id <= 0) throw new Error('User not found');
  return { id, username: `user_${id}` };
}

describe('fetchUserById', () => {
  it('resolves valid user data', async () => {
    const user = await fetchUserById(42);
    assert.deepStrictEqual(user, { id: 42, username: 'user_42' });
  });

  it('rejects with proper error when user ID is invalid', async () => {
    await assert.rejects(
      async () => fetchUserById(-1),
      { name: 'Error', message: 'User not found' }
    );
  });
});
```

### Example 3: Deep Object Comparison (`deepStrictEqual` vs `strictEqual`)
```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

describe('Object Equality Checks', () => {
  it('demonstrates reference equality vs structural equality', () => {
    const objA = { name: 'Ada', roles: ['admin'] };
    const objB = { name: 'Ada', roles: ['admin'] };

    // strictEqual checks memory reference (objA === objB) -> FAILS!
    assert.notStrictEqual(objA, objB);

    // deepStrictEqual recursively compares prototype, properties, and values -> PASSES!
    assert.deepStrictEqual(objA, objB);
  });
});
```

### Example 4: Testing State Transitions in a Shopping Cart
```javascript
import { describe, it, beforeEach } from 'node:test';
import assert from 'node:assert/strict';

class ShoppingCart {
  constructor() {
    this.items = new Map();
  }

  add(id, qty, price) {
    const existing = this.items.get(id) || { qty: 0, price };
    this.items.set(id, { qty: existing.qty + qty, price });
  }

  get total() {
    let sum = 0;
    for (const { qty, price } of this.items.values()) {
      sum += qty * price;
    }
    return sum;
  }
}

describe('ShoppingCart State Transitions', () => {
  let cart;

  beforeEach(() => {
    cart = new ShoppingCart(); // Fresh isolated instance per test
  });

  it('starts with total 0', () => {
    assert.strictEqual(cart.total, 0);
  });

  it('aggregates multiple quantities of same item', () => {
    cart.add('item-1', 2, 10);
    cart.add('item-1', 3, 10);
    assert.strictEqual(cart.total, 50);
  });
});
```

### Example 5: Testing with Custom Asserts
```javascript
import { it } from 'node:test';
import assert from 'node:assert/strict';

// Helper assertion function
function assertIsoDate(str) {
  const isIso = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?Z$/.test(str);
  assert.ok(isIso, `Expected "${str}" to be a valid UTC ISO-8601 string`);
}

it('emits timestamps in valid ISO-8601 UTC format', () => {
  const timestamp = new Date().toISOString();
  assertIsoDate(timestamp);
});
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Legacy `assert` vs `assert/strict`**: In Node.js, `import assert from 'node:assert'` uses loose equality (`==`), which allows `assert.equal(0, false)` and `assert.equal('1', 1)` to pass silently. **Always import `node:assert/strict`**, which enforces strict equality (`===`).
2. **Missing `await` in Asynchronous Tests**: If an `it('...', async () => { ... })` function omits `await` on a promise assertion (`assert.rejects(...)`), the test runner exits before the assertion runs. If the promise subsequently rejects, Node emits an `unhandledRejection` outside the test lifecycle, resulting in silent test passes or hard process crashes.
3. **Flaky Tests from Time Dependency**: Code that calls `Date.now()` or `new Date()` inside logic will produce flaky tests that pass locally but fail in CI environments running in different timezones (UTC vs Local). Always inject clocks or mock system time.
4. **Order-Dependent Tests**: If Test B relies on state created by Test A (e.g. data in a global array or persistent database), running tests in parallel or isolated mode causes random failures. Every test must be completely self-contained.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Using loose `assert.equal()` | Coerces types (`0 == false`), masking type bugs | Always use `node:assert/strict` (`strictEqual`) |
| Forgetting `await` on `assert.rejects()` | Promise executes unhandled after test finishes | Always `await assert.rejects(asyncFn)` |
| Sharing mutable state across tests | Tests become order-dependent and flaky | Re-initialize state in `beforeEach()` |
| Testing implementation details rather than behavior | Refactoring private variables breaks passing tests | Test public API inputs and outputs |
| Catching errors inside tests with manual `try/catch` | Swallowing errors or missing assert expectations | Use `assert.throws()` or `assert.rejects()` |
| Over-mocking everything in unit tests | Code passes tests but crashes in production integration | Focus tests on behavior and contracts |

---

## 8. Performance Characteristics

- **Node Native Runner Speed**: `node:test` starts up in $< 50\text{ms}$ (zero Babel/bundling overhead), running hundreds of unit tests in milliseconds.
- **Worker Parallelism**: By default, `node:test` executes test files concurrently across worker threads, utilizing multi-core parallelism.

---

## 9. Engine Internals

1. **`AssertionError` Stack Trimming**:
   - When an assertion fails, `node:assert` instantiates an internal `AssertionError` class.
   - It utilizes V8's `Error.captureStackTrace(err, sliceFunction)` to trim away internal assertion library frames, ensuring the stack trace points directly to the failing line of your test code.

---

## 10. Best Practices & Design Guidelines

- **Keep Tests Deterministic**: Tests must produce identical results regardless of whether they run on macOS, Linux, Windows, or in CI, and regardless of timezone or network connectivity.
- **One Conceptual Assertion Per Test**: While a test can have multiple `assert` statements validating a returned object's fields, it should test only one distinct logical behavior or state transition.
- **Fast Execution**: Unit test suites should execute in under 5 seconds. If tests are slow, developers stop running them before committing.

---

## 11. Real-World Case Study

### Regression Testing a Critical Tax Calculation Bug
```javascript
// THE BUG: When calculating taxes on items priced at $19.99 with 8.25% sales tax,
// floating-point precision error produced 1.649175 -> rounded down incorrectly to $1.64 instead of $1.65.

// REGRESSION TEST SUITE (Locks in expected financial rounding invariant):
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

function calculateExactTax(cents, taxRatePercent) {
  // Using integer cents to avoid floating point drift
  return Math.round((cents * taxRatePercent) / 100);
}

describe('Financial Tax Calculation Regression Guard', () => {
  it('correctly rounds half-up on fractional cents', () => {
    const itemPriceInCents = 1999; // $19.99
    const taxRate = 8.25;          // 8.25%

    // 1999 * 8.25 = 16491.75 / 100 = 164.9175 cents -> rounds to 165 cents ($1.65)
    const taxInCents = calculateExactTax(itemPriceInCents, taxRate);

    assert.strictEqual(taxInCents, 165);
    assert.strictEqual(taxInCents / 100, 1.65);
  });
});
```

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between the Testing Pyramid and the Testing Trophy?</summary>
The <b>Testing Pyramid</b> advocates for a broad foundation of unit tests, a medium tier of integration tests, and very few end-to-end tests. The <b>Testing Trophy</b> (popularized by Kent C. Dodds) emphasizes that integration tests offer the highest return on investment, advocating for a large integration testing middle, a targeted unit testing tier for complex algorithms, and a static analysis base (TypeScript/ESLint).
</details>

<details>
<summary>2. What is the Arrange-Act-Assert (AAA) pattern?</summary>
It is an established convention for structuring test cases:
1. <b>Arrange</b>: Set up test data, configure dependencies, and establish preconditions.
2. <b>Act</b>: Execute the function or method being tested.
3. <b>Assert</b>: Verify that the expected outcome, return value, or side effect occurred.
</details>

<details>
<summary>3. Why should you always import node:assert/strict instead of node:assert in Node.js?</summary>
The default <code>node:assert</code> uses loose equality (<code>==</code>), which coerces types and can cause tests to pass erroneously (e.g. <code>assert.equal(0, '')</code> passes). <code>node:assert/strict</code> enforces strict equality (<code>===</code>), guaranteeing type and value accuracy.
</details>

<details>
<summary>4. What is the difference between strictEqual and deepStrictEqual?</summary>
<code>strictEqual</code> checks referential equality (<code>actual === expected</code>); if two distinct objects have identical content, <code>strictEqual</code> fails. <code>deepStrictEqual</code> recursively traverses object prototypes, enumerable properties, arrays, Maps, Sets, and Buffers to verify deep structural equality.
</details>

<details>
<summary>5. How does the native Node.js test runner (node:test) eliminate the need for Jest in many projects?</summary>
<code>node:test</code> is built directly into Node core, requiring zero installation, zero configuration, and zero transpilation overhead. It natively supports suites (<code>describe</code>), tests (<code>it</code>), lifecycle hooks, mocking, coverage reporting, and watch mode directly using standard ES modules.
</details>

<details>
<summary>6. What causes a test to be "flaky"?</summary>
A flaky test produces inconsistent results (sometimes passes, sometimes fails) without code changes. Common causes include: race conditions in asynchronous code, reliance on real-world system time or timezones, shared mutable state between tests, and un-mocked external network dependencies.
</details>

<details>
<summary>7. How do you test that a function throws an asynchronous error?</summary>
Using <code>await assert.rejects(asyncFn, expectedError)</code>. The test runner awaits the execution of the promise and asserts that the promise rejects with an error matching the expected type or message.
</details>

<details>
<summary>8. Why is testing implementation details considered an anti-pattern?</summary>
Testing implementation details (e.g. asserting on private variable names or internal method call sequences) makes tests fragile. When internal code is refactored without altering public behavior, implementation-focused tests fail, discouraging healthy code cleanup. Tests should focus on inputs and observable outputs.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a Mini Test Suite and Assertion Runner
Implement a zero-dependency mini test runner `miniTest(name, fn)` and assertion helper `expect(val).toBe(expected)` that runs tests and prints green/red results to the terminal.

<details>
<summary>View Solution</summary>

```javascript
export function expect(actual) {
  return {
    toBe(expected) {
      if (actual !== expected) {
        throw new Error(`Expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
      }
    },
    toEqual(expected) {
      if (JSON.stringify(actual) !== JSON.stringify(expected)) {
        throw new Error(`Expected deep match with ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
      }
    },
  };
}

export async function miniTest(name, testFn) {
  try {
    await testFn();
    console.log(`\x1b[32m✔ PASS:\x1b[0m ${name}`);
  } catch (err) {
    console.error(`\x1b[31m✖ FAIL:\x1b[0m ${name}`);
    console.error(`  ${err.message}`);
  }
}

// Usage:
miniTest('adds numbers correctly', () => {
  expect(2 + 2).toBe(4);
});
```
</details>

### Exercise 2: Testing an Event-Driven Asynchronous Pipeline
Write a test using `node:test` that asserts a custom `EventEmitter` emits an `'audit'` event with a valid payload within 100ms of calling `service.doAction()`.

<details>
<summary>View Solution</summary>

```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import EventEmitter from 'node:events';

class AuditService extends EventEmitter {
  doAction(userId) {
    setTimeout(() => {
      this.emit('audit', { userId, action: 'LOGIN', timestamp: Date.now() });
    }, 20);
  }
}

describe('AuditService Event Dispatch', () => {
  it('emits audit event with valid payload', async () => {
    const service = new AuditService();

    const auditPromise = new Promise((resolve) => {
      service.once('audit', resolve);
    });

    service.doAction(101);

    const eventPayload = await auditPromise;
    assert.strictEqual(eventPayload.userId, 101);
    assert.strictEqual(eventPayload.action, 'LOGIN');
    assert.ok(typeof eventPayload.timestamp === 'number');
  });
});
```
</details>
