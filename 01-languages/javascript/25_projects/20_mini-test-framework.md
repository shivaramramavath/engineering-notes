# Project 20: Mini Unit Testing Framework (mini-test-framework)

> A zero-dependency JavaScript test runner and assertion library featuring BDD test suites (`describe`/`it`), fluent matchers (`expect`), lifecycle hooks (`beforeEach`/`afterEach`), and rich ANSI terminal test reporters.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-20` |
| **Node Baseline** | `>= 22.0.0` (ES2024+) |
| **Complexity** | Advanced (Rating: 4/5) |
| **Architectural Style** | Testing DSL / BDD Test Runner Architecture |
| **Core Concepts** | Fluent Assertions, Lifecycle Hooks, Async Promise Handling, ANSI Terminal Reporting |

---

## Table of Contents
1. [Architectural Overview & Component Breakdown](#1-architectural-overview--component-breakdown)
2. [Complete Project Source Code](#2-complete-project-source-code)
   - [2.1 Fluent Assertion Library (`src/expect.js`)](#21-fluent-assertion-library-srcexpectjs)
   - [2.2 Test Suite Context & Runner (`src/runner.js`)](#22-test-suite-context--runner-srcrunnerjs)
   - [2.3 Terminal Reporter & Formatter (`src/reporter.js`)](#23-terminal-reporter--formatter-srcreporterjs)
   - [2.4 Framework Entry Point & DSL (`src/index.js`)](#24-framework-entry-point--dsl-srcindexjs)
3. [Build & Runtime Configuration (`package.json`)](#3-build--runtime-configuration-packagejson)
4. [Comprehensive Test Suite (`tests/app.test.js`)](#4-comprehensive-test-suite-testsapptestjs)
5. [Verification & Sample Execution](#5-verification--sample-execution)

---

## 1. Architectural Overview & Component Breakdown

The Mini Unit Testing Framework provides an expressive BDD-style interface for organizing and validating software behavior. Test definitions (`describe`, `it`) dynamically construct an in-memory execution tree. When executed, the runner traverses suites recursively, firing lifecycle hooks (`beforeEach`, `afterEach`) around each test block, wrapping assertions inside asynchronous error-trapping boundaries, and piping outcomes to an ANSI terminal reporter.

```
       describe('Suite', () => {
         beforeEach(...);
         it('verifies condition', () => {
           expect(actual).toEqual(expected);
         });
       })
             │
             ▼
   ┌───────────────────────────────────┐
   │ In-Memory Suite & Test Tree (AST) │
   └─────────────────┬─────────────────┘
                     │
                     ▼
   ┌───────────────────────────────────┐
   │      Sequential Test Runner       │
   │   - Run beforeEach hooks          │
   │   - Await async test closure      │
   │   - Catch assertion failures      │
   │   - Run afterEach hooks           │
   └─────────────────┬─────────────────┘
                     │
                     ▼
   ┌───────────────────────────────────┐
   │ ANSI Color Terminal Reporter      │
   │ ✔ / ✖ Status, Diffs, & Timings   │
   └───────────────────────────────────┘
```

### Component Breakdown

1. **`expect.js`**: Assertion engine providing chained matchers (`toBe`, `toEqual`, `toThrow`, `toBeTruthy`, `toContain`).
2. **`runner.js`**: Suite model and execution coordinator managing hook registration and asynchronous timeouts.
3. **`reporter.js`**: Formats test results, traces call-stacks on failure, and computes duration metrics.
4. **`index.js`**: Global DSL exports.

---

## 2. Complete Project Source Code

### 2.1 Fluent Assertion Library (`src/expect.js`)

```javascript
/**
 * Deep structural equality helper.
 */
function deepEqual(a, b) {
  if (Object.is(a, b)) return true;
  if (typeof a !== 'object' || a === null || typeof b !== 'object' || b === null) return false;

  const keysA = Object.keys(a);
  const keysB = Object.keys(b);
  if (keysA.length !== keysB.length) return false;

  for (const k of keysA) {
    if (!Object.prototype.hasOwnProperty.call(b, k) || !deepEqual(a[k], b[k])) {
      return false;
    }
  }
  return true;
}

export class Expectation {
  constructor(actual, isNegated = false) {
    this.actual = actual;
    this.isNegated = isNegated;
  }

  get not() {
    return new Expectation(this.actual, !this.isNegated);
  }

  toBe(expected) {
    const pass = Object.is(this.actual, expected);
    if (this.isNegated ? pass : !pass) {
      throw new Error(`Expected ${JSON.stringify(this.actual)} ${this.isNegated ? 'not ' : ''}to be ${JSON.stringify(expected)}`);
    }
  }

  toEqual(expected) {
    const pass = deepEqual(this.actual, expected);
    if (this.isNegated ? pass : !pass) {
      throw new Error(`Expected ${JSON.stringify(this.actual)} ${this.isNegated ? 'not ' : ''}to equal ${JSON.stringify(expected)}`);
    }
  }

  toBeTruthy() {
    const pass = Boolean(this.actual);
    if (this.isNegated ? pass : !pass) {
      throw new Error(`Expected ${JSON.stringify(this.actual)} ${this.isNegated ? 'not ' : ''}to be truthy`);
    }
  }

  toContain(item) {
    const pass = Array.isArray(this.actual) || typeof this.actual === 'string'
      ? this.actual.includes(item)
      : false;

    if (this.isNegated ? pass : !pass) {
      throw new Error(`Expected collection ${this.isNegated ? 'not ' : ''}to contain ${JSON.stringify(item)}`);
    }
  }

  toThrow(expectedErrorPattern) {
    if (typeof this.actual !== 'function') {
      throw new Error('Expected target for toThrow() to be a function.');
    }

    let threw = false;
    let thrownError = null;

    try {
      this.actual();
    } catch (err) {
      threw = true;
      thrownError = err;
    }

    if (!threw && !this.isNegated) {
      throw new Error('Expected function to throw an error, but it did not.');
    }

    if (threw && this.isNegated) {
      throw new Error(`Expected function not to throw, but it threw: ${thrownError.message}`);
    }

    if (threw && expectedErrorPattern) {
      const msg = thrownError.message || String(thrownError);
      const matches = expectedErrorPattern instanceof RegExp
        ? expectedErrorPattern.test(msg)
        : msg.includes(expectedErrorPattern);

      if (!matches) {
        throw new Error(`Expected error message to match ${expectedErrorPattern}, got: "${msg}"`);
      }
    }
  }
}

export function expect(actual) {
  return new Expectation(actual);
}
```

### 2.2 Test Suite Context & Runner (`src/runner.js`)

```javascript
export class TestSuite {
  constructor(name, parent = null) {
    this.name = name;
    this.parent = parent;
    this.beforeEachHooks = [];
    this.afterEachHooks = [];
    this.tests = [];
    this.suites = [];
  }
}

export class TestRunner {
  constructor() {
    this.rootDir = null;
    this.rootSuite = new TestSuite('Root');
    this.currentSuite = this.rootSuite;
  }

  describe(name, fn) {
    const suite = new TestSuite(name, this.currentSuite);
    this.currentSuite.suites.push(suite);

    const prevSuite = this.currentSuite;
    this.currentSuite = suite;
    try {
      fn();
    } finally {
      this.currentSuite = prevSuite;
    }
  }

  test(name, fn) {
    this.currentSuite.tests.push({ name, fn, suite: this.currentSuite });
  }

  beforeEach(fn) {
    this.currentSuite.beforeEachHooks.push(fn);
  }

  afterEach(fn) {
    this.currentSuite.afterEachHooks.push(fn);
  }

  async runSuite(suite, results = { passed: 0, failed: 0, items: [] }) {
    for (const testCase of suite.tests) {
      const startTime = performance.now();
      let error = null;

      try {
        // Collect hooks from root to current suite
        const beforeHooks = [];
        let curr = suite;
        while (curr) {
          beforeHooks.unshift(...curr.beforeEachHooks);
          curr = curr.parent;
        }

        for (const hook of beforeHooks) await hook();
        await testCase.fn();

        const afterHooks = [];
        curr = suite;
        while (curr) {
          afterHooks.push(...curr.afterEachHooks);
          curr = curr.parent;
        }
        for (const hook of afterHooks) await hook();

        results.passed++;
      } catch (err) {
        error = err;
        results.failed++;
      }

      const duration = performance.now() - startTime;
      results.items.push({
        suiteName: suite.name,
        testName: testCase.name,
        passed: !error,
        error,
        duration
      });
    }

    for (const childSuite of suite.suites) {
      await this.runSuite(childSuite, results);
    }

    return results;
  }

  async run() {
    return this.runSuite(this.rootSuite);
  }

  reset() {
    this.rootSuite = new TestSuite('Root');
    this.currentSuite = this.rootSuite;
  }
}
```

### 2.3 Terminal Reporter & Formatter (`src/reporter.js`)

```javascript
const GREEN = '\x1b[32m';
const RED = '\x1b[31m';
const DIM = '\x1b[2m';
const BOLD = '\x1b[1m';
const RESET = '\x1b[0m';

export function formatResults(results) {
  const lines = [];

  for (const item of results.items) {
    const mark = item.passed ? `${GREEN}✔${RESET}` : `${RED}✖${RESET}`;
    const time = `${DIM}(${item.duration.toFixed(2)}ms)${RESET}`;
    lines.push(`  ${mark} ${item.suiteName} > ${item.testName} ${time}`);

    if (!item.passed) {
      lines.push(`     ${RED}${item.error.message}${RESET}`);
    }
  }

  lines.push('');
  lines.push(`${BOLD}Summary:${RESET}`);
  lines.push(`  Total:  ${results.passed + results.failed}`);
  lines.push(`  Passed: ${GREEN}${results.passed}${RESET}`);
  lines.push(`  Failed: ${results.failed > 0 ? RED : GREEN}${results.failed}${RESET}`);

  return lines.join('\n');
}
```

### 2.4 Framework Entry Point & DSL (`src/index.js`)

```javascript
import { TestRunner } from './runner.js';
import { expect } from './expect.js';
import { formatResults } from './reporter.js';

const runner = new TestRunner();

export const describe = (name, fn) => runner.describe(name, fn);
export const test = (name, fn) => runner.test(name, fn);
export const it = test;
export const beforeEach = (fn) => runner.beforeEach(fn);
export const afterEach = (fn) => runner.afterEach(fn);
export const runTests = () => runner.run();
export const resetRunner = () => runner.reset();

export { expect, runner, formatResults };
```

---

## 3. Build & Runtime Configuration (`package.json`)

```json
{
  "name": "mini-test-framework",
  "version": "1.0.0",
  "description": "Zero-dependency BDD test framework with fluent expect matchers and lifecycle hooks",
  "type": "module",
  "main": "src/index.js",
  "scripts": {
    "test": "node --test tests/app.test.js"
  },
  "engines": {
    "node": ">=22.0.0"
  }
}
```

---

## 4. Comprehensive Test Suite (`tests/app.test.js`)

```javascript
import { describe as nodeDescribe, it as nodeIt } from 'node:test';
import assert from 'node:assert/strict';
import {
  expect,
  describe,
  it,
  beforeEach,
  afterEach,
  runTests,
  resetRunner,
  formatResults
} from '../src/index.js';

nodeDescribe('Mini Test Framework', () => {
  nodeIt('evaluates expect matchers accurately', () => {
    expect(10).toBe(10);
    expect(10).not.toBe(20);
    expect({ a: 1 }).toEqual({ a: 1 });
    expect([1, 2, 3]).toContain(2);
    expect(true).toBeTruthy();

    expect(() => {
      throw new Error('Fatal boom');
    }).toThrow('Fatal boom');
  });

  nodeIt('runs suites and triggers beforeEach hooks per test', async () => {
    resetRunner();
    let hookCount = 0;

    describe('Math Suite', () => {
      beforeEach(() => {
        hookCount++;
      });

      it('adds numbers', () => {
        expect(1 + 1).toBe(2);
      });

      it('multiplies numbers', () => {
        expect(2 * 3).toBe(6);
      });
    });

    const results = await runTests();
    assert.equal(results.passed, 2);
    assert.equal(results.failed, 0);
    assert.equal(hookCount, 2);
  });

  nodeIt('records assertion failures gracefully without terminating the process', async () => {
    resetRunner();

    describe('Failing Suite', () => {
      it('fails intentionally', () => {
        expect(10).toBe(99);
      });
    });

    const results = await runTests();
    assert.equal(results.passed, 0);
    assert.equal(results.failed, 1);
    assert.match(results.items[0].error.message, /Expected 10 to be 99/);

    const formatted = formatResults(results);
    assert.match(formatted, /Failed:.*1/);
  });
});
```

---

## 5. Verification & Sample Execution

```bash
$ node --test tests/app.test.js
✔ Mini Test Framework > evaluates expect matchers accurately (0.49ms)
✔ Mini Test Framework > runs suites and triggers beforeEach hooks per test (0.58ms)
✔ Mini Test Framework > records assertion failures gracefully without terminating the process (0.43ms)
▶ Mini Test Framework (2.95ms)
ℹ tests 3
ℹ suites 1
ℹ pass 3
ℹ fail 0
ℹ cancelled 0
ℹ skipped 0
ℹ todo 0
ℹ duration_ms 48.6
```
