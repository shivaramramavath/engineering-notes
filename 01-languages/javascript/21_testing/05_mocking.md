# Mocking, Spies, and Stubs in JavaScript

> Mocking philosophy, spies vs stubs, network mocking with MSW, fake timers, and ESM module mocking mechanics.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Unit Testing](file:///e:/Learning/engineering-notes/01-languages/javascript/21_testing/02_unit-testing.md), [Modules](file:///e:/Learning/engineering-notes/01-languages/javascript/13_modules/01_module-systems-overview.md) |
| **Time to read** | 25 minutes |
| **Status** | Complete |
| **Standard / Runtime** | ECMAScript 2015+, Node.js `node:test` / Vitest |

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

In automated testing, our code frequently interacts with slow, non-deterministic, or hazardous external systems: sending real emails to users, charging credit cards via payment APIs, reading physical hard drives, or waiting on multi-second network timeouts. If our test suite hits real production infrastructure, tests become slow, flaky, costly, and dangerous.

**Mocking** is the practice of replacing real dependencies with simulated test doubles that mimic their behavior in a controlled, deterministic manner.

However, mocking is a double-edged sword. When overused, mocking creates tautological tests: tests that only verify the mocks themselves, providing a false sense of security while failing to detect critical production integration failures. Understanding the nuanced taxonomy of test doubles (spies, stubs, fakes, and mocks) and modern network mocking strategies (like Mock Service Worker) is essential for engineering resilient, high-confidence test suites.

---

## 2. Core Concepts

### The Test Double Spectrum
- **Dummy**: An object passed purely to satisfy parameter signatures; its methods are never invoked.
- **Stub**: Replaces a function with a predetermined canned response (e.g. `stub.mockReturnValue(42)`). It does not record or assert behavior.
- **Spy**: Wraps an existing function, allowing it to execute while recording invocations, arguments, and return values for subsequent assertions.
- **Mock**: An object pre-configured with strict behavioral expectations (e.g. "method X must be called exactly once with argument Y"), failing the test automatically if those expectations are violated.
- **Fake**: A fully working implementation that takes shortcuts unsuitable for production (e.g. an in-memory database using a `Map`).

### The Three Levels of Cleanup: Clear vs Reset vs Restore
Managing mock lifecycle is critical to avoid cross-test contamination:
1. **`mockClear()`**: Clears the recorded call history and arguments of the spy. The mock implementation remains in place.
2. **`mockReset()`**: Clears the call history **and** removes the custom mock implementation, resetting the spy to an empty function returning `undefined`.
3. **`mockRestore()`**: Completely removes the spy/mock and restores the original implementation of the method on the target object.

---

## 3. How It Works Internally

```
METHOD SPY INTERCEPTION:
Original Object: { calculate(x) { return x * 2; } }
                          |
                          v
mock.method(obj, 'calculate', customImpl)
                          |
                          v
+--------------------------------------------------------------------------+
| SPY WRAPPER FUNCTION                                                     |
| - records: calls.push({ args, time: Date.now(), this: this })            |
| - executes: const result = customImpl(...args)                           |
| - records: calls[last].result = result                                   |
| - returns result                                                         |
+--------------------------------------------------------------------------+
                          |
                          v
mock.restore() -> Restores original method descriptor to obj.calculate
```

---

## 4. Syntax & API Reference

### Function and Method Mocking with Vitest (`vi`)
```javascript
import { describe, it, expect, vi } from 'vitest';

// 1. Spying on an existing object method
const user = { getAge: () => 25 };
const ageSpy = vi.spyOn(user, 'getAge');

user.getAge();
expect(ageSpy).toHaveBeenCalledTimes(1);

// 2. Stubbing a return value
ageSpy.mockReturnValue(99);
expect(user.getAge()).toBe(99);

// 3. Stubbing an asynchronous promise resolution
const asyncSpy = vi.fn().mockResolvedValue({ status: 'ok' });
const res = await asyncSpy();
expect(res).toEqual({ status: 'ok' });

// 4. Restore original implementation
ageSpy.mockRestore();
expect(user.getAge()).toBe(25);
```

---

## 5. Practical Examples

### Example 1: Network Mocking with Mock Service Worker (MSW)
```javascript
// MSW intercepts requests at the network level (Service Worker or Node http hook),
// allowing application code to use standard fetch() without monkey-patching globals!
import { setupServer } from 'msw/node';
import { http, HttpResponse } from 'msw';
import { describe, it, expect, beforeAll, afterAll, afterEach } from 'vitest';

// Define request intercept handlers
const server = setupServer(
  http.get('https://api.example.com/users/:id', ({ params }) => {
    return HttpResponse.json({
      id: params.id,
      name: 'Ada Lovelace',
      email: 'ada@example.com',
    });
  }),
  http.post('https://api.example.com/orders', async ({ request }) => {
    const body = await request.json();
    return HttpResponse.json({ orderId: 'ord-123', ...body }, { status: 201 });
  })
);

describe('API Client with MSW', () => {
  beforeAll(() => server.listen({ onUnhandledRequest: 'error' }));
  afterEach(() => server.resetHandlers());
  afterAll(() => server.close());

  it('fetches user from intercepted network', async () => {
    const response = await fetch('https://api.example.com/users/42');
    const data = await response.json();

    expect(response.status).toBe(200);
    expect(data.name).toBe('Ada Lovelace');
  });

  it('handles server errors gracefully via runtime override', async () => {
    // Override handler for a single test case
    server.use(
      http.get('https://api.example.com/users/:id', () => {
        return new HttpResponse(null, { status: 500 });
      })
    );

    const response = await fetch('https://api.example.com/users/42');
    expect(response.status).toBe(500);
  });
});
```

### Example 2: Fake Timers and Interval Progression
```javascript
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';

class PollingService {
  constructor(callback, intervalMs = 1000) {
    this.callback = callback;
    this.intervalMs = intervalMs;
    this.timer = null;
  }

  start() {
    this.timer = setInterval(() => this.callback(), this.intervalMs);
  }

  stop() {
    clearInterval(this.timer);
  }
}

describe('PollingService with Fake Timers', () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it('executes callback exactly 3 times over 3000ms', () => {
    const spy = vi.fn();
    const service = new PollingService(spy, 1000);

    service.start();
    expect(spy).not.toHaveBeenCalled();

    // Advance 1000ms -> 1st call
    vi.advanceTimersByTime(1000);
    expect(spy).toHaveBeenCalledTimes(1);

    // Advance 2000ms more -> 3 total calls
    vi.advanceTimersByTime(2000);
    expect(spy).toHaveBeenCalledTimes(3);

    service.stop();
  });
});
```

### Example 3: Mocking System Clock / `Date.now()`
```javascript
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';

function isNewYear() {
  const now = new Date();
  return now.getMonth() === 0 && now.getDate() === 1;
}

describe('System Clock Mocking', () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it('detects New Years Day accurately when clock is pinned', () => {
    // Pin system time to Jan 1, 2026 00:00:00 UTC
    vi.setSystemTime(new Date('2026-01-01T00:00:00Z'));
    expect(isNewYear()).toBe(true);

    // Pin system time to July 4, 2026
    vi.setSystemTime(new Date('2026-07-04T12:00:00Z'));
    expect(isNewYear()).toBe(false);
  });
});
```

### Example 4: Testing Dependency Injection with Method Spies
```javascript
import { describe, it, expect, vi } from 'vitest';

class PaymentProcessor {
  constructor(emailGateway) {
    this.emailGateway = emailGateway;
  }

  async processTransaction(user, amount) {
    if (amount <= 0) throw new Error('Invalid amount');
    // Charge logic...
    await this.emailGateway.sendReceipt(user.email, amount);
    return { success: true };
  }
}

describe('PaymentProcessor', () => {
  it('calls sendReceipt with proper recipient and amount', async () => {
    const fakeEmailGateway = {
      sendReceipt: vi.fn().mockResolvedValue(true),
    };

    const processor = new PaymentProcessor(fakeEmailGateway);
    await processor.processTransaction({ email: 'bob@example.com' }, 150);

    expect(fakeEmailGateway.sendReceipt).toHaveBeenCalledTimes(1);
    expect(fakeEmailGateway.sendReceipt).toHaveBeenCalledWith('bob@example.com', 150);
  });
});
```

### Example 5: Native `node:test` Mocking Without Libraries
```javascript
import { describe, it, mock } from 'node:test';
import assert from 'node:assert/strict';

describe('Zero-Dependency Native Node Mocking', () => {
  it('stubs property and tracks calls natively', () => {
    const logger = {
      log: (msg) => console.log(msg),
    };

    // Native spy via node:test
    const spy = mock.method(logger, 'log', () => {});

    logger.log('First log');
    logger.log('Second log');

    assert.strictEqual(spy.mock.callCount(), 2);
    assert.strictEqual(spy.mock.calls[0].arguments[0], 'First log');
    assert.strictEqual(spy.mock.calls[1].arguments[0], 'Second log');

    mock.reset(); // Restores original logger.log
  });
});
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **ES Module Live Binding and Read-Only Exports**: In ECMAScript specifications, exported variables are live, immutable bindings. Attempting to directly overwrite an export (`import * as mod from './mod.js'; mod.myFn = mockFn`) throws a `TypeError: Cannot assign to read only property`. Mocking tools (like `vi.mock`) must intercept module loading at the Vite or loader pipeline level.
2. **Missing `mockRestore()` Contamination**: Calling `mockReturnValue()` modifies an object's method permanently unless `mockRestore()` is called. If Test A mocks `Date.now` and fails to restore it, Tests B through Z inherit the frozen timestamp, causing bewildering test failures.
3. **Mocking What You Don't Own**: Mocking external third-party SDK internals (e.g. mocking private methods inside the Stripe or AWS SDK) creates tests that pass against your mocks but fail catastrophically in production when the vendor releases a minor update. **Only mock interfaces you own** (wrap third-party SDKs in an Adapter, and mock the Adapter).
4. **Un-mocked Timers Hanging the Test Runner**: If code creates an un-refed `setInterval` or recursive `setTimeout` inside a test with fake timers enabled, calling `vi.runAllTimers()` will enter an infinite loop and freeze the process.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Mocking third-party SDK internal classes | Tests pass while production crashes on vendor changes | Wrap vendor SDK in an Adapter; mock your Adapter |
| Using `mockClear()` when `mockRestore()` is required | Leaves mock implementations active across test files | Use `mockRestore()` or `afterEach(() => vi.restoreAllMocks())` |
| Mocking every internal helper function in a unit test | Tautological testing; tests break during harmless refactors | Test the public unit of work; let helpers run naturally |
| Overwriting `global.fetch` manually with `vi.fn()` | Fails to simulate realistic headers, streams, and status | Use Mock Service Worker (MSW) |
| Running tests against real system clocks | Tests fail in CI runners in different timezones | Use `vi.setSystemTime()` or inject clock parameters |

---

## 8. Performance Characteristics

- **Spy Invocation Overhead**: An active spy adds $< 0.1$ microseconds per call to capture call metadata.
- **MSW Interception Speed**: Mock Service Worker intercepts HTTP requests in Node using an internal interceptor library (`@mswjs/interceptors`), executing simulated API responses in $< 1\text{ms}$ without traversing the OS network stack.

---

## 9. Engine Internals

1. **Property Descriptor Interception in Spies**:
   - `vi.spyOn(obj, 'method')` inspects `Object.getOwnPropertyDescriptor(obj, 'method')`.
   - It saves the original descriptor and replaces the property with a new descriptor whose `value` points to the spy wrapper function.
   - When `mockRestore()` is invoked, the engine reinstalls the original property descriptor via `Object.defineProperty()`.

---

## 10. Best Practices & Design Guidelines

- **Mock at the Architectural Boundaries**: Mock external network services, databases, and clocks. Do not mock internal domain logic.
- **Prefer Fakes Over Mocks**: Use simple in-memory fakes (like `FakeUserRepository`) whenever possible. Fakes behave like real systems, reducing mock setup boilerplate.
- **Automate Teardown in `afterEach`**: Configure test runners to automatically clean up all mocks after every test (`vi.restoreAllMocks()`).

---

## 11. Real-World Case Study

### Eliminating Flaky Network Tests by Adopting MSW
```javascript
// THE PROBLEM: A frontend test suite of 200 tests used manual `global.fetch = vi.fn()`.
// Tests were constantly breaking because different developers mocked fetch differently:
// some mocked `.json()`, some mocked `res.ok`, and one un-restored fetch broke 40 subsequent tests.

// THE SOLUTION: Replaced all manual fetch mocks with Mock Service Worker (MSW)
import { setupServer } from 'msw/node';
import { http, HttpResponse } from 'msw';
import { beforeAll, afterEach, afterAll, describe, it, expect } from 'vitest';

const server = setupServer(
  http.get('https://api.github.com/users/:username', ({ params }) => {
    return HttpResponse.json({
      login: params.username,
      public_repos: 42,
    });
  })
);

describe('GitHub Profile Card Component', () => {
  beforeAll(() => server.listen());
  afterEach(() => server.resetHandlers());
  afterAll(() => server.close());

  it('fetches and displays real HTTP response structures cleanly', async () => {
    // Application calls standard window.fetch() naturally!
    const res = await fetch('https://api.github.com/users/octocat');
    const user = await res.json();

    expect(res.ok).toBe(true);
    expect(user.login).toBe('octocat');
    expect(user.public_repos).toBe(42);
  });
});

// Result: Zero mock pollution, 100% test reliability, and zero fetch monkey-patching.
```

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental difference between a Spy and a Stub?</summary>
A <b>Spy</b> wraps an existing function to record its invocation history (call counts, passed arguments, return values) while allowing the original implementation to execute. A <b>Stub</b> replaces the original function entirely with a canned, predetermined response without executing the underlying logic.
</details>

<details>
<summary>2. What is the difference between mockClear(), mockReset(), and mockRestore()?</summary>
- <b>mockClear()</b>: Resets recorded call counts and argument histories. The mock implementation remains active.
- <b>mockReset()</b>: Clears call history and removes the custom mock implementation, resetting the spy to an empty function returning <code>undefined</code>.
- <b>mockRestore()</b>: Completely removes the spy and restores the original function implementation on the target object.
</details>

<details>
<summary>3. Why is Mock Service Worker (MSW) superior to monkey-patching global.fetch?</summary>
Monkey-patching <code>global.fetch = vi.fn()</code> bypasses browser and Node networking mechanics, forcing tests to manually mock <code>Response</code>, <code>Headers</code>, status codes, and stream bodies, which rarely matches real production behavior. MSW intercepts requests at the network transport layer, allowing code to execute real <code>fetch</code> calls while returning realistic mock HTTP responses.
</details>

<details>
<summary>4. Why is it dangerous to mock code you do not own?</summary>
Mocking external vendor libraries directly means your tests are asserting against your assumptions of how the vendor library works, rather than reality. When the vendor updates their library, your tests will continue to pass against your stale mocks while production code crashes. The solution is to wrap third-party code in an Adapter and mock your Adapter.
</details>

<details>
<summary>5. How do fake timers work in test runners?</summary>
Fake timers replace the global timer functions (<code>setTimeout</code>, <code>setInterval</code>, <code>Date.now</code>) with a simulated clock. When tests call <code>advanceTimersByTime(ms)</code>, the runner manually increments the simulated clock and synchronously executes all scheduled timer callbacks without burning real time.
</details>

<details>
<summary>6. What is a "Tautological Test"?</summary>
A tautological test is a test that only verifies its own mock configuration (e.g., configuring a mock to return 5 and asserting that it returned 5) without exercising real business logic. Tautological tests provide 100% code coverage on paper while catching zero real bugs.
</details>

<details>
<summary>7. How does Vitest hoist vi.mock() calls and why?</summary>
Vitest's compiler statically hoists <code>vi.mock()</code> calls to the very top of the compiled file before any <code>import</code> statements execute. This guarantees that modules imported by the test file receive the mocked version of dependencies rather than the real implementation.
</details>

<details>
<summary>8. How do you mock a read-only property on an object?</summary>
By using <code>vi.spyOn(object, 'propertyName', 'get')</code> to mock the property's getter descriptor, or by re-defining the property descriptor using <code>Object.defineProperty()</code>.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a Pure JavaScript Spy Implementation
Implement a zero-dependency function spy wrapper `createSpy(originalFn)` that records call counts and argument arrays, providing a `.restore()` method to detach itself.

<details>
<summary>View Solution</summary>

```javascript
export function createSpy(targetObj, methodName) {
  const originalMethod = targetObj[methodName];
  const calls = [];

  function spyWrapper(...args) {
    const record = { args, thisContext: this };
    calls.push(record);
    try {
      record.result = originalMethod.apply(this, args);
      return record.result;
    } catch (err) {
      record.error = err;
      throw err;
    }
  }

  spyWrapper.calls = calls;
  spyWrapper.callCount = () => calls.length;
  spyWrapper.restore = () => {
    targetObj[methodName] = originalMethod;
  };

  targetObj[methodName] = spyWrapper;
  return spyWrapper;
}
```
</details>

### Exercise 2: Mocking an Asynchronous Weather Service
Write a unit test that mocks an asynchronous weather service module using `vi.mock` to return `{ temp: 15, condition: 'Rain' }`, asserting that a weather alert function sends an umbrella reminder notification.

<details>
<summary>View Solution</summary>

```javascript
import { describe, it, expect, vi } from 'vitest';

// Assume weatherApi is imported from a module
const weatherApi = {
  getForecast: async (city) => ({ temp: 25, condition: 'Sunny' }),
};

function checkNeedUmbrella(forecast) {
  return forecast.condition.toLowerCase().includes('rain');
}

describe('Weather Alert Logic', () => {
  it('detects when an umbrella is required using a mock stub', async () => {
    const getForecastSpy = vi.spyOn(weatherApi, 'getForecast')
      .mockResolvedValue({ temp: 14, condition: 'Heavy Rain' });

    const forecast = await weatherApi.getForecast('Seattle');
    const needsUmbrella = checkNeedUmbrella(forecast);

    expect(needsUmbrella).toBe(true);
    expect(getForecastSpy).toHaveBeenCalledWith('Seattle');

    getForecastSpy.mockRestore();
  });
});
```
</details>
