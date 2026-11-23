# Mocking, Spying, and Test Doubles in TypeScript

> Comprehensive guide to test doubles in TypeScript: mocks, stubs, spies, fakes, Vitest `vi.fn()` and `vi.spyOn()`, MSW (Mock Service Worker) for network mocking, and type-safe mock builders.

| Metadata | Details |
| :--- | :--- |
| **Category** | Testing and Debugging |
| **Difficulty** | Intermediate |
| **Prerequisites** | [02-functions/00-function-types.md](../02-functions/00-function-types.md), [18-testing-and-debugging/00-unit-testing.md](./00-unit-testing.md) |
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

In unit testing, components often depend on external services: payment gateways, email providers, file systems, and timers. When testing a `NotificationService`, we do not want to send real SMS text messages or incur Twilio API billing charges on every test run.

We substitute real dependencies with **Test Doubles**:
- **Dummy**: Passed around but never actually used (e.g., filling required parameter slots).
- **Stub**: Provides predefined hardcoded answers to calls during the test.
- **Spy**: Records information about calls made to it (arguments, call count, return values).
- **Mock**: Pre-programmed with expectations regarding calls it should receive; verifies behavior.
- **Fake**: Has a working implementation, but takes a shortcut unsuitable for production (e.g., an in-memory database).

In TypeScript, untyped mocking (`jest.fn() as any`) destroys type safety. Modern TypeScript testing uses **Type-Safe Mocks** with Vitest (`vi.fn<[string], Promise<User>>()`) and **Network-Level Mocking** with **MSW (Mock Service Worker)**.

```
+-----------------------------------------------------------+
|                     MSW Network Interception              |
|                                                           |
|   App calls: fetch('https://api.stripe.com/v1/charges')   |
|                              |                            |
|                              v                            |
|                    Intercepted at OS / Node               |
|                    Network boundary by MSW                |
|                              |                            |
|                              v                            |
|   Returns mock JSON without touching the real internet!   |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. Vitest Spies and Mocks (`vi.fn` and `vi.spyOn`)
`vi.fn()` creates a typed mock function. It infers argument and return types automatically from function signatures:
```typescript
import { vi } from 'vitest';

const sendMailMock = vi.fn<(to: string, body: string) => Promise<boolean>>();
sendMailMock.mockResolvedValue(true);
```

### 2. Spying on Existing Methods
`vi.spyOn(object, 'methodName')` wraps an existing object method, tracking invocations while optionally delegating to the original implementation:
```typescript
const spy = vi.spyOn(Date, 'now').mockReturnValue(1700000000000);
```

### 3. Mock Service Worker (MSW)
MSW intercepts HTTP requests at the network layer (via Service Workers in browsers or `node:http` interception in Node.js), eliminating the need to mock `fetch` or `axios` instances directly:
```typescript
import { http, HttpResponse } from 'msw';
import { setupServer } from 'msw/node';

export const server = setupServer(
  http.get('https://api.example.com/user', () => {
    return HttpResponse.json({ id: '1', name: 'Alice' });
  })
);
```

---

## 3. How It Works Internally

1. **`vi.fn()` Internal Architecture**:
   Maintains a `.mock` property containing `calls: any[][]`, `results: { type: 'return' | 'throw', value: any }[]`, and `instances: any[]`. Every invocation records call arguments and returns the configured value or executes the mock implementation.
2. **MSW Node Interception**:
   MSW monkey-patches Node's underlying `http.ClientRequest` and global `fetch`. When a request matches a handler's path and method, MSW bypasses the network socket and synthesizes a `Response` object directly.

---

## 4. Syntax & API Reference

### Type-Safe Deep Partial Mock Helper
```typescript
export type DeepPartial<T> = T extends Function
  ? T
  : T extends Array<infer U>
  ? DeepPartial<U>[]
  : T extends object
  ? { [K in keyof T]?: DeepPartial<T[K]> }
  : T;

export function createMock<T extends object>(overrides: DeepPartial<T> = {}): T {
  return overrides as T;
}
```

---

## 5. Practical Examples

### Example 1: Basic Function Mocking and Argument Assertion
```typescript
import { vi, describe, it, expect } from 'vitest';

interface Notifier {
  send(to: string, message: string): boolean;
}

export function alertAdmins(notifier: Notifier, alertText: string): void {
  notifier.send('admin@corp.internal', `[ALERT] ${alertText}`);
}

describe('alertAdmins', () => {
  it('calls notifier with formatted admin email and alert text', () => {
    const mockNotifier: Notifier = {
      send: vi.fn().mockReturnValue(true)
    };

    alertAdmins(mockNotifier, 'Disk full');

    expect(mockNotifier.send).toHaveBeenCalledTimes(1);
    expect(mockNotifier.send).toHaveBeenCalledWith('admin@corp.internal', '[ALERT] Disk full');
  });
});
```

### Example 2: Real-World Network Mocking with MSW
```typescript
import { http, HttpResponse } from 'msw';
import { setupServer } from 'msw/node';
import { beforeAll, afterAll, afterEach, describe, it, expect } from 'vitest';

const server = setupServer(
  http.get('https://api.github.com/users/octocat', () => {
    return HttpResponse.json({ login: 'octocat', public_repos: 8 });
  })
);

beforeAll(() => server.listen());
afterEach(() => server.resetHandlers());
afterAll(() => server.close());

describe('GitHub Client', () => {
  it('fetches repo count correctly', async () => {
    const res = await fetch('https://api.github.com/users/octocat');
    const data = await res.json();
    expect(data.public_repos).toBe(8);
  });
});
```

### Example 3: Edge Case - Time and Timer Mocking
```typescript
import { vi, it, expect } from 'vitest';

export function runAfterDelay(ms: number, fn: () => void): void {
  setTimeout(fn, ms);
}

it('advances fake timers without waiting real time', () => {
  vi.useFakeTimers();
  const callback = vi.fn();

  runAfterDelay(10000, callback);

  expect(callback).not.toHaveBeenCalled();

  // Fast-forward 10 seconds instantly!
  vi.advanceTimersByTime(10000);

  expect(callback).toHaveBeenCalledTimes(1);
  vi.useRealTimers();
});
```

### Example 4: Failure Case - Type Checking on Mock Call Signatures
```typescript
interface AuthService {
  login(user: string, pass: string): Promise<boolean>;
}

const mockAuth: AuthService = {
  // @ts-expect-error TS2322: Type 'Mock<() => boolean>' is not assignable to '(user: string, pass: string) => Promise<boolean>'.
  login: vi.fn<() => boolean>().mockReturnValue(true)
};
```

### Example 5: Idiomatic Proxy-Based Auto-Mocking
```typescript
export function createAutoMock<T extends object>(): T {
  const mocks = new Map<string | symbol, ReturnType<typeof vi.fn>>();

  return new Proxy({} as T, {
    get: (_, prop) => {
      if (!mocks.has(prop)) {
        mocks.set(prop, vi.fn());
      }
      return mocks.get(prop);
    }
  });
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Pollution Across Tests**: Spies created with `vi.spyOn()` mutate the target object. If not restored, they alter behavior in subsequent tests. Always run `vi.restoreAllMocks()` or `afterEach(() => vi.clearAllMocks())`.
2. **`mockClear` vs `mockReset` vs `mockRestore`**:
   - `mockClear()`: Clears recorded calls and instances, keeps mock implementation.
   - `mockReset()`: Clears calls and resets mock implementation to return `undefined`.
   - `mockRestore()`: Completely restores original implementation (only works on spies).
3. **Module Mocking Hoisting**: `vi.mock('./path')` is hoisted to the top of the file before all imports. Variables defined in the test file cannot be used directly inside `vi.mock()` factory closures unless prefixed with `vi.hoisted()`.
4. **Mocking Native Globals**: Overwriting `globalThis.fetch` directly without restoration can break other tests or test runner internal reporting. Prefer MSW for HTTP.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Untyped Mock Assertion** | `(service as any).doStuff = jest.fn()` | `vi.spyOn(service, 'doStuff')` | Disables compiler checking |
| **Forgetting to Restore Spies** | Spying on `Date.now` without teardown | `afterEach(() => vi.restoreAllMocks())` | Cascading flaky test failures |
| **Over-Mocking Everything** | Mocking domain models and utility functions | Mock only external boundaries (I/O) | Tests test nothing real; fragile |
| **Manual `fetch` Mocking** | Monkey-patching `global.fetch = () => ...` | MSW (Mock Service Worker) | Misses headers, status codes, query strings |
| **Un-awaited Mock Resolutions** | `fn.mockReturnValue(Promise.resolve())` | `fn.mockResolvedValue()` | Shorter syntax, cleaner promises |
| **Closure Variable in Hoisted Mock** | Using local variable inside `vi.mock()` | Use `vi.hoisted(() => ...)` | `ReferenceError: Cannot access before initialization` |

---

## 8. Performance & Type-Checker Complexity

- **Overhead**: Creating a `vi.fn()` mock takes sub-microsecond time. MSW network interception in Node.js processes requests in under 1ms.
- **Type Checker**: Mock type signatures `vi.fn<[string], number>()` check parameter contravariance and return covariance accurately without performance drag.

---

## 9. Compiler Internals

When using `vi.spyOn(target, 'method')`:
TypeScript uses indexed access types `T[K]` and constraints `K extends keyof T`. The return type of `spyOn` is typed as `MockInstance<T[K]>`, extracting parameter types via `Parameters<T[K]>` and return type via `ReturnType<T[K]>`.

---

## 10. Best Practices & Design Guidelines

1. **Mock at Architectural Boundaries Only**: Never mock internal domain entities or calculation helpers; only mock external I/O (Stripe, S3, Email, SMS).
2. **Prefer MSW for Network APIs**: Keep `fetch` untouched and intercept network packets cleanly.
3. **Always Clean Up Spies**: Configure `restoreMocks: true` in `vitest.config.ts`.
4. **Use Fake Timers for Delays**: Avoid `setTimeout` sleeps in tests; advance fake timers instantaneously.

---

## 11. Real-World Case Study

### Resilient Payment Retry Engine with Clock Spying
Testing an exponential backoff payment retry service:

```typescript
export class PaymentRetrier {
  constructor(private gateway: { charge: () => Promise<boolean> }) {}

  public async chargeWithBackoff(maxAttempts: number): Promise<boolean> {
    for (let attempt = 1; attempt <= maxAttempts; attempt++) {
      try {
        return await this.gateway.charge();
      } catch (err) {
        if (attempt === maxAttempts) throw err;
        await new Promise((r) => setTimeout(r, Math.pow(2, attempt) * 1000));
      }
    }
    return false;
  }
}

describe('PaymentRetrier', () => {
  it('retries on failure with exponential delay', async () => {
    vi.useFakeTimers();
    const mockCharge = vi.fn()
      .mockRejectedValueOnce(new Error('Gateway error'))
      .mockResolvedValueOnce(true);

    const retrier = new PaymentRetrier({ charge: mockCharge });
    const promise = retrier.chargeWithBackoff(3);

    // Fast forward through backoff delay
    await vi.runAllTimersAsync();

    const success = await promise;
    expect(success).toBe(true);
    expect(mockCharge).toHaveBeenCalledTimes(2);
    vi.useRealTimers();
  });
});
```

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between a Mock and a Stub according to Martin Fowler?</summary>

A **Stub** provides canned answers to calls made during the test, usually not responding to anything outside what's programmed. A **Mock** is pre-programmed with expectations which form a specification of the calls they are expected to receive; it verifies behavior by asserting that expected methods were called with expected arguments.
</details>

<details>
<summary>2. Why is MSW (Mock Service Worker) superior to mocking `global.fetch` or `axios`?</summary>

Mocking `fetch` directly mocks the client implementation, making tests fragile if the codebase switches HTTP clients (e.g., from `fetch` to `axios` or `ky`). MSW intercepts at the network layer using standard `Request` and `Response` objects, allowing tests to test the real client configuration without changing test code.
</details>

<details>
<summary>3. What is the difference between `mockClear()`, `mockReset()`, and `mockRestore()`?</summary>

- `mockClear()`: Clears history of calls and results.
- `mockReset()`: Clears history and resets the mock implementation to an empty function returning `undefined`.
- `mockRestore()`: Completely removes the spy and restores the original function on the target object.
</details>

<details>
<summary>4. Why are `vi.mock()` calls hoisted to the top of the file?</summary>

Because ES module imports are evaluated before any top-level module code runs. For a mock to intercept an imported module, the mock must be registered before the target module is loaded by the module loader.
</details>

<details>
<summary>5. How do you test code that uses `Date.now()` or `new Date()` deterministically?</summary>

Use Vitest's `vi.setSystemTime(new Date('2026-01-01T00:00:00Z'))` in conjunction with `vi.useFakeTimers()`.
</details>

<details>
<summary>6. How can you ensure a mock function matches a TypeScript interface?</summary>

Declare the mock object explicitly conforming to the interface:
```typescript
const mockService: IService = {
  fetchData: vi.fn<(id: string) => Promise<Data>>()
};
```
</details>

<details>
<summary>7. What is "over-mocking" and why is it dangerous?</summary>

Over-mocking occurs when tests mock internal application details rather than external boundaries. Tests pass because mocks satisfy each other, but the real system fails in production because the real integrated components don't match mock assumptions.
</details>

<details>
<summary>8. How do you handle asynchronous timers with `vi.useFakeTimers()`?</summary>

Use `await vi.runAllTimersAsync()` or `await vi.advanceTimersByTimeAsync(ms)`, which ensures microtasks and promise callbacks triggered by timer expirations resolve properly before assertions run.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic Function Call Spy</summary>

**Problem:** Spy on a callback `onComplete(status: string)` and assert it is called with `'SUCCESS'`.

**Solution:**
```typescript
import { vi, it, expect } from 'vitest';

it('calls callback', () => {
  const spy = vi.fn();
  spy('SUCCESS');
  expect(spy).toHaveBeenCalledWith('SUCCESS');
});
```
</details>

<details>
<summary>Exercise 2 (Easy): Mock Return Value Once</summary>

**Problem:** Configure a mock function that returns `10` on the first call, `20` on the second call, and `0` thereafter.

**Solution:**
```typescript
const mockFn = vi.fn().mockReturnValueOnce(10).mockReturnValueOnce(20).mockReturnValue(0);
```
</details>

<details>
<summary>Exercise 3 (Medium): Spy on Console.warn</summary>

**Problem:** Spy on `console.warn` without printing to standard output during test execution, and restore it in cleanup.

**Solution:**
```typescript
import { vi, it, expect, afterEach } from 'vitest';

afterEach(() => { vi.restoreAllMocks(); });

it('suppresses and verifies warning', () => {
  const warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {});
  console.warn('Test warning');
  expect(warnSpy).toHaveBeenCalledWith('Test warning');
});
```
</details>

<details>
<summary>Exercise 4 (Medium): MSW 404 Handler Override</summary>

**Problem:** In an individual test, use MSW's `server.use()` to simulate an HTTP 404 Not Found response for an endpoint that normally succeeds.

**Solution:**
```typescript
import { http, HttpResponse } from 'msw';

it('handles 404 error from server', async () => {
  server.use(
    http.get('https://api.example.com/user', () => {
      return new HttpResponse(null, { status: 404 });
    })
  );

  const res = await fetch('https://api.example.com/user');
  expect(res.status).toBe(404);
});
```
</details>

<details>
<summary>Exercise 5 (Hard): Type-Safe Auto Mock Generator</summary>

**Problem:** Write a function `mockInterface<T extends object>(): { [K in keyof T]: any }` that returns an object where every method is automatically a typed `vi.fn()`.

**Solution:**
```typescript
export function mockInterface<T extends object>(): T {
  return new Proxy({} as T, {
    get: (target, prop) => {
      if (!(prop in target)) {
        (target as any)[prop] = vi.fn();
      }
      return (target as any)[prop];
    }
  });
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Deterministic Async Clock Advancer</summary>

**Problem:** Write a helper that advances fake timers in small increments while allowing pending microtasks (Promises) to flush between each increment.

**Solution:**
```typescript
export async function stepTimeAsync(totalMs: number, stepMs: number = 100): Promise<void> {
  let elapsed = 0;
  while (elapsed < totalMs) {
    await vi.advanceTimersByTimeAsync(stepMs);
    elapsed += stepMs;
  }
}
```
</details>
