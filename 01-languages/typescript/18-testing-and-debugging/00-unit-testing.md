# Unit Testing in TypeScript with Vitest and Jest

> Authoring robust, isolated, and type-safe unit tests in TypeScript using Vitest, Jest, ts-jest, assertions, and pure function testing.

| Metadata | Details |
| :--- | :--- |
| **Category** | Testing and Debugging |
| **Difficulty** | Intermediate |
| **Prerequisites** | [02-functions/00-function-types.md](../02-functions/00-function-types.md), [11-error-handling/00-catching-and-narrowing-errors.md](../11-error-handling/00-catching-and-narrowing-errors.md) |
| **TypeScript Baseline** | TS 4.5+, TS 5.0+ |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

**Unit Testing** is the discipline of testing the smallest testable parts of an application—individual functions, methods, and classes—in strict isolation from external infrastructure (networks, databases, file systems).

In TypeScript, unit tests provide a dual guarantee:
1. **Compile-Time Contract Verification**: The TypeScript compiler verifies that test fixtures, arguments, and expected return types conform to domain interfaces.
2. **Runtime Behavioral Verification**: The test runner executes the code against varied operational inputs, asserting that edge cases, business invariants, and branch conditions behave as expected.

In modern TypeScript ecosystems, **Vitest** has largely superseded Jest due to its native Vite pipeline, out-of-the-box ESM and TypeScript support via esbuild, multi-threaded worker pools, and instant HMR test execution.

```
+-------------------------------------------------------------+
|                         Vitest Runner                       |
|                                                             |
|   my-service.ts  <--(Transforms via esbuild)                |
|   my-service.test.ts                                        |
|         |                                                   |
|         v                                                   |
|   Executes in isolated Worker Pool (threads)                |
|   Reports: Pass / Fail, Execution Times, Coverage           |
+-------------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. Test Structure (AAA Pattern)
Every unit test should follow the **Arrange-Act-Assert** pattern:
- **Arrange**: Set up test inputs, mock dependencies, and fixtures.
- **Act**: Invoke the unit under test.
- **Assert**: Verify that the output matches expected values and no unexpected side-effects occurred.

### 2. Matchers and Type Inference
Vitest and Jest matchers (`toBe`, `toEqual`, `toThrowError`) infer target types to prevent nonsensical comparisons (e.g. asserting that a string equals a number):
```typescript
import { describe, it, expect } from 'vitest';

describe('Calculator', () => {
  it('adds two numbers correctly', () => {
    const sum = 2 + 3;
    expect(sum).toBe(5);
  });
});
```

### 3. Asynchronous Testing
Testing Promise-based code using `async/await` and `.rejects`:
```typescript
it('rejects with error on negative input', async () => {
  await expect(calculateSquareRoot(-1)).rejects.toThrow('Negative input not allowed');
});
```

---

## 3. How It Works Internally

When running `vitest run`:
1. **Discovery**: Glob scanners find files matching `**/*.{test,spec}.ts`.
2. **Transpilation**: Vitest transforms TypeScript files via esbuild or Vite plugins on-the-fly, stripping types without emitting separate `.js` files to disk.
3. **Execution**: Tests run in parallel worker threads (using Node.js `worker_threads` or `tinypool`).
4. **Assertion Evaluation**: `expect()` assertions evaluate expressions. If an assertion fails, an `AssertionError` is thrown with an informative diff.

---

## 4. Syntax & API Reference

### Vitest Core API
```typescript
import { describe, it, test, expect, beforeEach, afterEach, beforeAll, afterAll } from 'vitest';

describe('suite', () => {
  beforeEach(() => { /* setup */ });
  afterEach(() => { /* teardown */ });

  it('test description', () => {
    expect(actual).toEqual(expected);
  });
});
```

---

## 5. Practical Examples

### Example 1: Basic Pure Function Unit Testing
```typescript
export function formatCurrency(cents: number, currency: 'USD' | 'EUR' = 'USD'): string {
  if (!Number.isInteger(cents) || cents < 0) {
    throw new TypeError('Cents must be a non-negative integer');
  }
  const symbol = currency === 'USD' ? '$' : '€';
  return `${symbol}${(cents / 100).toFixed(2)}`;
}

// Test Suite:
import { describe, it, expect } from 'vitest';

describe('formatCurrency', () => {
  it('formats standard dollar amounts', () => {
    expect(formatCurrency(1250, 'USD')).toBe('$12.50');
  });

  it('handles zero cents', () => {
    expect(formatCurrency(0, 'USD')).toBe('$0.00');
  });

  it('throws on negative input', () => {
    expect(() => formatCurrency(-100)).toThrow(TypeError);
  });
});
```

### Example 2: Real-World In-Memory Business Domain Service Test
```typescript
interface CartItem { id: string; priceCents: number; qty: number; }

export class ShoppingCart {
  private items: CartItem[] = [];

  public addItem(item: CartItem): void {
    if (item.qty <= 0) throw new Error('Quantity must be greater than zero');
    this.items.push(item);
  }

  public calculateSubtotal(): number {
    return this.items.reduce((acc, item) => acc + item.priceCents * item.qty, 0);
  }
}

describe('ShoppingCart', () => {
  it('calculates total for multiple items', () => {
    const cart = new ShoppingCart();
    cart.addItem({ id: '1', priceCents: 500, qty: 2 });
    cart.addItem({ id: '2', priceCents: 1000, qty: 1 });

    expect(cart.calculateSubtotal()).toBe(2000);
  });
});
```

### Example 3: Edge Case - Parameterized / Table-Driven Tests
```typescript
import { test, expect } from 'vitest';

export function isLeapYear(year: number): boolean {
  return (year % 4 === 0 && year % 100 !== 0) || year % 400 === 0;
}

test.each([
  [2000, true],
  [2024, true],
  [1900, false],
  [2023, false],
  [2400, true]
])('isLeapYear(%i) should return %s', (year, expected) => {
  expect(isLeapYear(year)).toBe(expected);
});
```

### Example 4: Failure Case - Type Mismatch in Assertions
```typescript
interface UserProfile { id: string; role: 'admin' | 'user'; }

const user: UserProfile = { id: 'u_1', role: 'admin' };

// @ts-expect-error TS2367: This condition will always return 'false' since the types '"admin" | "user"' and '"superuser"' have no overlap.
if (user.role === 'superuser') {
  // Compiler detects impossible branch!
}
```

### Example 5: Idiomatic Snapshot Testing for Complex Data Shapes
```typescript
import { it, expect } from 'vitest';

export function generateReport(title: string, metrics: Record<string, number>) {
  return {
    meta: { title, generatedAt: '2026-01-01T00:00:00.000Z' },
    summary: metrics
  };
}

it('matches snapshot structure', () => {
  const result = generateReport('Monthly Billing', { totalOrders: 42, revenueCents: 500000 });
  expect(result).toMatchInlineSnapshot(`
    {
      "meta": {
        "generatedAt": "2026-01-01T00:00:00.000Z",
        "title": "Monthly Billing",
      },
      "summary": {
        "revenueCents": 500000,
        "totalOrders": 42,
      },
    }
  `);
});
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Floating Point Precision**: In JavaScript, `0.1 + 0.2 === 0.30000000000000004`. Using `expect(0.1 + 0.2).toBe(0.3)` will fail! Always use `toBeCloseTo()` for floating point arithmetic: `expect(0.1 + 0.2).toBeCloseTo(0.3, 5)`.
2. **Missing `await` on Async Assertions**: Forgetting `await` on `expect(promise).rejects.toThrow()` causes the test to pass prematurely without verifying the rejection.
3. **Shared State Between Tests**: Mutating a shared module-level variable across tests makes test outcomes dependent on execution order. Always reset state in `beforeEach()`.
4. **`toBe` vs `toEqual`**: `toBe` checks referential identity (`Object.is`), whereas `toEqual` performs deep structural equality. Comparing two distinct object literals `{ a: 1 }` with `toBe` always fails.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Float Equality with `toBe`** | `expect(price * 0.1).toBe(1.05)` | `expect(price * 0.1).toBeCloseTo(1.05)` | Fails due to IEEE 754 precision |
| **Forgotten `await` on Rejects** | `expect(fn()).rejects.toThrow()` | `await expect(fn()).rejects.toThrow()` | Unhandled promise rejection / false pass |
| **Testing Implementation Details** | Checking private class properties | Testing public interface outputs | Fragile tests that break on refactoring |
| **Global State Contamination** | Module-level cache not cleared | Reset cache in `beforeEach()` | Flaky tests when run concurrently |
| **Multiple Unrelated Asserts** | 30 assertions in one 200-line test | Focused single-responsibility tests | Failure obscures which invariant broke |
| **Type Assertion Bypass in Tests** | `const fake = {} as CompleteUser` | Use complete fixtures or factories | Bypasses compiler protection in tests |

---

## 8. Performance & Type-Checker Complexity

- **Type Stripping vs Type Checking**: Fast test runners (Vitest, tsx, esbuild) strip types without type-checking tests during execution. To ensure tests themselves contain no type errors, run `tsc --noEmit` or `vitest typecheck` in your CI pipeline.
- **Worker Concurrency**: Vitest isolates test files across worker threads. Ensure tests do not bind to the same hardcoded localhost port or touch the same file path.

---

## 9. Compiler Internals

When using Vitest's `expect<T>(val: T)`:
The compiler types `expect` using generic parameter inference. If you pass `expect(user.role)`, the matcher methods (`toBe`, `toEqual`) constrain their arguments to `user.role`'s inferred type union, flagging invalid values at compile time.

---

## 10. Best Practices & Design Guidelines

1. **Follow the AAA Pattern**: Keep Arrange, Act, and Assert clearly demarcated.
2. **Test Invariants, Not Private Methods**: Test units through their public APIs.
3. **Run Typechecks in CI**: Add `tsc --noEmit` alongside `vitest run`.
4. **Use `toBeCloseTo` for Decimals**: Prevent IEEE-754 precision bugs.

---

## 11. Real-World Case Study

### Validating Invariant Business Rules in a Banking Ledger
Testing that balance debit operations cannot exceed available funds and produce correct audit entries:

```typescript
export interface Account { id: string; balanceCents: bigint; }

export function debitAccount(acc: Account, amountCents: bigint): Account {
  if (amountCents <= 0n) throw new RangeError('Debit amount must be positive');
  if (acc.balanceCents < amountCents) throw new Error('Insufficient funds');
  return { ...acc, balanceCents: acc.balanceCents - amountCents };
}

describe('debitAccount', () => {
  it('deducts funds correctly when balance is sufficient', () => {
    const acc: Account = { id: 'acc_1', balanceCents: 10000n };
    const updated = debitAccount(acc, 3000n);
    expect(updated.balanceCents).toBe(7000n);
  });

  it('throws Insufficient funds when balance is exceeded', () => {
    const acc: Account = { id: 'acc_1', balanceCents: 500n };
    expect(() => debitAccount(acc, 600n)).toThrow('Insufficient funds');
  });
});
```

---

## 12. Interview Questions

<details>
<summary>1. Why has Vitest largely replaced Jest in modern TypeScript development?</summary>

Vitest shares its configuration with Vite, leverages esbuild for instant sub-millisecond TypeScript transpilation, natively supports modern ESM without experimental flags, runs tests concurrently in worker pools, and provides built-in type testing (`expectTypeOf`).
</details>

<details>
<summary>2. What is the difference between `toBe()` and `toEqual()` in test assertions?</summary>

`toBe()` checks strict referential equality using `Object.is` (suitable for primitives and exact object references). `toEqual()` performs a deep recursive structural comparison of all properties on objects and arrays.
</details>

<details>
<summary>3. Why do fast TypeScript test runners (Vitest/esbuild) skip type checking by default, and how should CI handle this?</summary>

Skipping full type checking allows tests to run 10x-50x faster during development by merely stripping type syntax. CI pipelines must run a separate `tsc --noEmit` step to catch type errors before or alongside test runs.
</details>

<details>
<summary>4. How do you properly test that an async function rejects with an error?</summary>

```typescript
await expect(asyncFunction()).rejects.toThrow('Expected message');
```
The `await` is required because `.rejects` returns a Promise that must settle before the test completes.
</details>

<details>
<summary>5. What is the AAA (Arrange-Act-Assert) pattern and why is it important?</summary>

AAA is a testing convention that structures tests into three distinct phases: arranging test data, acting upon the system under test, and asserting expectations. It improves readability, standardizes test structure, and simplifies debugging.
</details>

<details>
<summary>6. How do you test floating-point numbers in Jest/Vitest?</summary>

Use `expect(val).toBeCloseTo(expected, numDigits)` to avoid test failures caused by binary IEEE 754 precision artifacts like `0.1 + 0.2 === 0.30000000000000004`.
</details>

<details>
<summary>7. What are table-driven / parameterized tests and when should they be used?</summary>

Table-driven tests use `test.each(table)` to run the identical assertion logic across an array of varied inputs and expected outputs. They are ideal for validating mathematical functions, parsers, and validation rules.
</details>

<details>
<summary>8. Why should you avoid using `as any` when creating test fixtures?</summary>

Casting test fixtures with `as any` disables TypeScript's type checker, allowing tests to pass invalid object shapes that real code would reject, creating a false sense of test security.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): String Reversal Unit Test</summary>

**Problem:** Write a pure function `reverseWords(str: string): string` and a Vitest test suite asserting that words are reversed while preserving whitespace.

**Solution:**
```typescript
export function reverseWords(str: string): string {
  return str.split(' ').reverse().join(' ');
}

import { it, expect } from 'vitest';
it('reverses words', () => {
  expect(reverseWords('hello world')).toBe('world hello');
});
```
</details>

<details>
<summary>Exercise 2 (Easy): Async Error Assertion</summary>

**Problem:** Write a test verifying that `fetchUser('-1')` rejects with `'Invalid user ID'`.

**Solution:**
```typescript
import { it, expect } from 'vitest';

async function fetchUser(id: string) {
  if (Number(id) < 0) throw new Error('Invalid user ID');
  return { id };
}

it('rejects on negative id', async () => {
  await expect(fetchUser('-1')).rejects.toThrow('Invalid user ID');
});
```
</details>

<details>
<summary>Exercise 3 (Medium): Table-Driven Email Validator Test</summary>

**Problem:** Use `test.each` to test an `isValidEmail(email: string): boolean` function against 5 valid and 5 invalid email formats.

**Solution:**
```typescript
import { test, expect } from 'vitest';

function isValidEmail(e: string) { return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(e); }

test.each([
  ['alice@example.com', true],
  ['test.user+tag@domain.co.uk', true],
  ['invalid-email', false],
  ['@missing-user.com', false],
  ['no-tld@domain', false]
])('isValidEmail(%s) -> %s', (email, expected) => {
  expect(isValidEmail(email)).toBe(expected);
});
```
</details>

<details>
<summary>Exercise 4 (Medium): Custom Matcher Implementation</summary>

**Problem:** Extend Vitest with a custom matcher `toBeWithinRange(actual: number, floor: number, ceiling: number)`.

**Solution:**
```typescript
import { expect } from 'vitest';

expect.extend({
  toBeWithinRange(received: number, floor: number, ceiling: number) {
    const pass = received >= floor && received <= ceiling;
    return {
      pass,
      message: () => `expected ${received} to be within range [${floor}, ${ceiling}]`
    };
  }
});
```
</details>

<details>
<summary>Exercise 5 (Hard): High-Precision Decimal Assertion Helper</summary>

**Problem:** Implement an assertion helper `assertBigIntEqual(actual: bigint, expected: bigint)` that formats clear BigInt diffs on failure.

**Solution:**
```typescript
export function assertBigIntEqual(actual: bigint, expected: bigint) {
  if (actual !== expected) {
    throw new Error(`BigInt assertion failed:\n  Expected: ${expected}n\n  Received: ${actual}n`);
  }
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Concurrent Invariant Fuzz Tester</summary>

**Problem:** Write a test that fires 1,000 randomized state updates concurrently on a `BankVault` class, asserting that the total ledger balance remains invariant.

**Solution:**
```typescript
import { it, expect } from 'vitest';

class BankVault {
  public balance = 10000;
  deposit(n: number) { this.balance += n; }
  withdraw(n: number) { this.balance -= n; }
}

it('preserves ledger balance invariant under concurrent ops', async () => {
  const vault = new BankVault();
  const ops = Array.from({ length: 500 }, () => [
    () => vault.deposit(50),
    () => vault.withdraw(50)
  ]).flat();

  await Promise.all(ops.map((fn) => Promise.resolve().then(fn)));
  expect(vault.balance).toBe(10000);
});
```
</details>
