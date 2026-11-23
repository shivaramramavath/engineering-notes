# Unit Testing in JavaScript

> Testing units of work, state vs behavior verification, test doubles taxonomy (stubs, spies, fakes, mocks), and refactor-proof tests.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [Testing Fundamentals](file:///e:/Learning/engineering-notes/01-languages/javascript/21_testing/01_testing-fundamentals.md), [Dependency Injection](file:///e:/Learning/engineering-notes/01-languages/javascript/20_design-patterns/09_dependency-injection.md) |
| **Time to read** | 24 minutes |
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

A **Unit Test** exercises the smallest testable piece of software in isolation from external infrastructure (such as databases, network sockets, file systems, or external microservices). In JavaScript, a "unit" is not rigidly defined as a single class or function; rather, it represents a cohesive **Unit of Work**: a behavior that accepts an input, performs a calculation or state transition, and yields an observable output.

Because unit tests run purely in memory without spinning up I/O or operating system sockets, they execute in sub-millisecond time. A suite of 1,000 unit tests can complete in less than 2 seconds, providing instant feedback during development and catching subtle algorithmic regressions before code leaves the local machine.

---

## 2. Core Concepts

### What Constitutes a "Unit"?
A unit can be:
- A single pure function (`calculateTax(price, rate)`).
- A stateful class instance (`new Cart()`).
- A cluster of small collaborating functions working together as an encapsulated module.

### State Verification vs Behavior Verification
- **State Verification**: Asserts that the system under test produces the correct return value or transitions to the expected internal state. Highly resilient to code refactoring.
- **Behavior Verification**: Asserts that specific internal collaborator methods were called with specific arguments (e.g. verifying `logger.info` was called 3 times). Fragile and prone to false-positive test failures when internal implementation changes.

### The Gerard Meszaros Test Doubles Taxonomy
When isolating a unit from external collaborators, developers use **Test Doubles**:
1. **Dummy**: Objects passed around but never actually used (e.g., satisfying a required parameter).
2. **Stub**: Provides predetermined canned answers to calls made during the test.
3. **Spy**: Wraps a real or stubbed function, recording execution metadata (how many times it was called, arguments passed).
4. **Mock**: Pre-programmed with expectations of which calls it should receive, failing the test if unexpected calls occur.
5. **Fake**: Has a working implementation, but takes shortcuts that make it unsuitable for production (e.g., an in-memory database).

---

## 3. How It Works Internally

```
+--------------------------------------------------------------------------+
| UNIT TEST EXECUTION ENVIRONMENT                                          |
+--------------------------------------------------------------------------+
                                    |
  1. Instantiates Fake/Stub         | const fakeRepo = new InMemoryUserRepo()
  2. Injects Double into SUT        | const sut = new OrderService(fakeRepo)
                                    |
  3. Executes Unit of Work          | const order = await sut.checkout(...)
                                    v
+--------------------------------------------------------------------------+
| SYSTEM UNDER TEST (SUT: OrderService)                                    |
| - Computes order total                                                   |
| - Validates credit limit                                                 |
| - Calls fakeRepo.saveOrder() [In-Memory fast execution: 0.1ms]           |
+--------------------------------------------------------------------------+
                                    |
                                    v
+--------------------------------------------------------------------------+
| ASSERTION STAGE (State Verification)                                     |
| assert.strictEqual(order.status, 'CONFIRMED')                            |
| assert.strictEqual(fakeRepo.count(), 1)                                  |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Native Mocking with `node:test`
Since Node.js v19+, `node:test` includes a built-in `mock` utility:
```javascript
import { describe, it, mock } from 'node:test';
import assert from 'node:assert/strict';

describe('Native Function Mocking', () => {
  it('spies on function calls and records calls', () => {
    // Create a spy
    const fnSpy = mock.fn((x) => x * 2);

    fnSpy(5);
    fnSpy(10);

    // Verify spy call count and arguments
    assert.strictEqual(fnSpy.mock.callCount(), 2);
    assert.deepStrictEqual(fnSpy.mock.calls[0].arguments, [5]);
    assert.strictEqual(fnSpy.mock.calls[0].result, 10);
  });

  it('stubs a method on an object', () => {
    const user = { getAge: () => 20 };

    // Stub getAge to return 42
    mock.method(user, 'getAge', () => 42);

    assert.strictEqual(user.getAge(), 42);

    // Restore original implementation
    mock.reset();
    assert.strictEqual(user.getAge(), 20);
  });
});
```

---

## 5. Practical Examples

### Example 1: Testing Pure Algorithmic Functions
```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

export function calculateLevenshteinDistance(a, b) {
  const matrix = Array.from({ length: b.length + 1 }, (_, i) => [i]);
  for (let j = 0; j <= a.length; j++) matrix[0][j] = j;

  for (let i = 1; i <= b.length; i++) {
    for (let j = 1; j <= a.length; j++) {
      if (b[i - 1] === a[j - 1]) {
        matrix[i][j] = matrix[i - 1][j - 1];
      } else {
        matrix[i][j] = Math.min(
          matrix[i - 1][j - 1] + 1, // substitution
          matrix[i][j - 1] + 1,     // insertion
          matrix[i - 1][j] + 1      // deletion
        );
      }
    }
  }
  return matrix[b.length][a.length];
}

describe('calculateLevenshteinDistance', () => {
  it('identifies identical strings', () => {
    assert.strictEqual(calculateLevenshteinDistance('kitten', 'kitten'), 0);
  });

  it('calculates single-character edits', () => {
    assert.strictEqual(calculateLevenshteinDistance('cat', 'hat'), 1);
    assert.strictEqual(calculateLevenshteinDistance('cat', 'cats'), 1);
  });

  it('handles empty strings', () => {
    assert.strictEqual(calculateLevenshteinDistance('', 'hello'), 5);
    assert.strictEqual(calculateLevenshteinDistance('', ''), 0);
  });
});
```

### Example 2: Using In-Memory Fakes Instead of Heavy Mocks
```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

// SUT
class TaskManager {
  constructor(repository) {
    this.repo = repository;
  }

  async addTask(title) {
    if (!title?.trim()) throw new Error('Title required');
    const task = { id: crypto.randomUUID(), title: title.trim(), completed: false };
    await this.repo.save(task);
    return task;
  }
}

// In-Memory Fake Repository (Fast, deterministic, real state)
class FakeTaskRepository {
  constructor() {
    this.tasks = new Map();
  }
  async save(task) {
    this.tasks.set(task.id, { ...task });
  }
  async findById(id) {
    return this.tasks.get(id) || null;
  }
}

describe('TaskManager', () => {
  it('creates and persists a task using an in-memory fake', async () => {
    const fakeRepo = new FakeTaskRepository();
    const manager = new TaskManager(fakeRepo);

    const created = await manager.addTask('Refactor core module');

    assert.strictEqual(created.title, 'Refactor core module');
    assert.strictEqual(created.completed, false);

    // Verify state persisted in fake
    const inDb = await fakeRepo.findById(created.id);
    assert.deepStrictEqual(inDb, created);
  });
});
```

### Example 3: Testing State Machine Transitions
```javascript
import { describe, it, beforeEach } from 'node:test';
import assert from 'node:assert/strict';

class OrderStateMachine {
  constructor() {
    this.state = 'PENDING';
  }

  pay() {
    if (this.state !== 'PENDING') throw new Error(`Cannot pay in state: ${this.state}`);
    this.state = 'PAID';
  }

  ship() {
    if (this.state !== 'PAID') throw new Error(`Cannot ship in state: ${this.state}`);
    this.state = 'SHIPPED';
  }

  cancel() {
    if (this.state === 'SHIPPED') throw new Error('Cannot cancel shipped order');
    this.state = 'CANCELLED';
  }
}

describe('OrderStateMachine', () => {
  let sm;
  beforeEach(() => { sm = new OrderStateMachine(); });

  it('follows happy path lifecycle', () => {
    sm.pay();
    assert.strictEqual(sm.state, 'PAID');
    sm.ship();
    assert.strictEqual(sm.state, 'SHIPPED');
  });

  it('rejects illegal state transitions', () => {
    assert.throws(() => sm.ship(), /Cannot ship in state: PENDING/);
    sm.pay();
    sm.ship();
    assert.throws(() => sm.cancel(), /Cannot cancel shipped order/);
  });
});
```

### Example 4: Testing Asynchronous Event Emitters
```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import EventEmitter from 'node:events';

class HeartbeatMonitor extends EventEmitter {
  constructor(timeoutMs = 50) {
    super();
    this.timeoutMs = timeoutMs;
    this.timer = null;
  }

  start() {
    this.timer = setTimeout(() => this.emit('timeout'), this.timeoutMs);
  }

  ping() {
    clearTimeout(this.timer);
    this.start();
  }

  stop() {
    clearTimeout(this.timer);
  }
}

describe('HeartbeatMonitor', () => {
  it('emits timeout event when ping is missing', async () => {
    const monitor = new HeartbeatMonitor(20);

    const timeoutPromise = new Promise(resolve => monitor.once('timeout', resolve));
    monitor.start();

    await timeoutPromise; // Await event emission
    assert.ok(true, 'Timeout event emitted successfully');
    monitor.stop();
  });
});
```

### Example 5: Spying on Collaborators with Method Restores
```javascript
import { describe, it, mock } from 'node:test';
import assert from 'node:assert/strict';

const notifier = {
  sendSlackAlert(msg) {
    // In production, makes HTTP POST to Slack webhook
    return true;
  },
};

function handleCriticalIncident(error, notifyService = notifier) {
  if (error.isCritical) {
    notifyService.sendSlackAlert(`CRITICAL: ${error.message}`);
  }
}

describe('handleCriticalIncident', () => {
  it('triggers slack notification only for critical errors', () => {
    const alertSpy = mock.method(notifier, 'sendSlackAlert', () => true);

    handleCriticalIncident({ isCritical: false, message: 'Minor warning' });
    assert.strictEqual(alertSpy.mock.callCount(), 0);

    handleCriticalIncident({ isCritical: true, message: 'DB connection lost' });
    assert.strictEqual(alertSpy.mock.callCount(), 1);
    assert.strictEqual(alertSpy.mock.calls[0].arguments[0], 'CRITICAL: DB connection lost');

    mock.reset(); // Restore original function
  });
});
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Over-Mocking (The Mockist Trap)**: Creating elaborate mock expectations on every collaborator method couples your tests to implementation details. When you refactor private helper methods without changing behavior, all your tests break. Prefer **State Verification** and **Fakes** over deep mocks.
2. **Floating Promises in Tests**: Initiating an asynchronous operation inside a test without returning or `await`ing it allows the test to finish while the promise is still executing in the background. If the promise fails later, it crashes the test suite runner.
3. **Leaked Mock State**: If a test stubs a global or module method (e.g. `mock.method(console, 'log')`) and fails to call `mock.reset()` in an `afterEach` hook, the stub remains active for all subsequent test suites.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Testing private methods directly (`service.#privateMethod()`) | Couples tests to internal implementation | Test private logic indirectly through public API |
| Mocking everything down to the bone | Tests test nothing except the mocks themselves | Use real collaborators or in-memory Fakes |
| Reusing global state across test cases | Tests become flaky and order-dependent | Re-instantiate SUT fresh in `beforeEach()` |
| Omitting `await` on asynchronous SUT methods | Test exits before code finishes; unhandled rejections | Always `await` promises returned by the SUT |
| Asserting only on method call counts, ignoring return data | Verifies code ran, but fails to verify correctness | Assert on resulting return values or state |

---

## 8. Performance Characteristics

- **Zero-I/O Execution**: Pure unit tests execute in roughly 0.1 to 1.0 milliseconds each.
- **In-Memory Fakes**: Using an in-memory `Map` fake instead of a SQLite or Docker database executes 1,000x faster ($< 0.1$ms vs $100$ms).

---

## 9. Engine Internals

1. **JIT Compilation of Test Loops**:
   - Running hundreds of unit tests through identical code paths triggers TurboFan optimization, speeding up subsequent test iterations within the same test suite process.

---

## 10. Best Practices & Design Guidelines

- **Write Refactor-Proof Tests**: A good test should pass before and after an internal refactor as long as the external observable behavior remains identical.
- **Follow DAMP over DRY**: While production code should be DRY (Don't Repeat Yourself), test code should be DAMP (Descriptive And Meaningful Phrases). Prioritize readability and self-contained clarity over eliminating duplication in tests.
- **Test Invariants and Boundaries**: Target edge cases: empty strings, zero, negative numbers, boundary limits, and malformed structures.

---

## 11. Real-World Case Study

### Unit Testing a Financial Token Bucket Rate Limiter
```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

export class TokenBucket {
  constructor(capacity, refillPerSecond) {
    this.capacity = capacity;
    this.tokens = capacity;
    this.refillRate = refillPerSecond;
    this.lastRefill = Date.now();
  }

  consume(tokens = 1, currentTime = Date.now()) {
    this.#refill(currentTime);
    if (this.tokens >= tokens) {
      this.tokens -= tokens;
      return true;
    }
    return false;
  }

  #refill(now) {
    const elapsedSeconds = (now - this.lastRefill) / 1000;
    this.tokens = Math.min(this.capacity, this.tokens + (elapsedSeconds * this.refillRate));
    this.lastRefill = now;
  }
}

describe('TokenBucket Unit Tests', () => {
  it('allows consumption within initial capacity', () => {
    const bucket = new TokenBucket(10, 1);
    assert.strictEqual(bucket.consume(5), true);
    assert.strictEqual(bucket.consume(5), true);
    assert.strictEqual(bucket.consume(1), false); // Capacity exhausted
  });

  it('refills tokens deterministically based on simulated time', () => {
    const t0 = 1_000_000;
    const bucket = new TokenBucket(10, 2); // 2 tokens per second
    bucket.consume(10, t0); // Drain completely

    // Simulate 3 seconds later
    const t1 = t0 + 3000;
    // Should have refilled 6 tokens
    assert.strictEqual(bucket.consume(6, t1), true);
    assert.strictEqual(bucket.consume(1, t1), false);
  });
});
```

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between a Stub, a Spy, and a Mock?</summary>
- <b>Stub</b>: Provides canned, predetermined return values to method calls during a test without performing verification.
- <b>Spy</b>: Wraps a real or stubbed function to record call metadata (call counts, passed arguments, returned values).
- <b>Mock</b>: An object pre-configured with strict expectations about which methods must be called and in what order, automatically failing the test if expectations are violated.
</details>

<details>
<summary>2. Why is State Verification generally preferred over Behavior Verification?</summary>
State Verification inspects the final output or state transitions of the system, allowing the internal implementation to change completely without breaking the test. Behavior verification inspects internal collaborator call sequences, coupling the test tightly to implementation details and causing tests to break during harmless refactoring.
</details>

<details>
<summary>3. What is a "Fake" in test double taxonomy and when should you use one?</summary>
A Fake is a test double with a working, functional implementation that takes shortcuts unsuitable for production (e.g. an in-memory repository using a <code>Map</code> instead of a SQL database). Fakes are ideal because they behave like real collaborators, eliminate fragile mock setups, and run blisteringly fast in memory.
</details>

<details>
<summary>4. Should you test private methods in a unit test?</summary>
No. Private methods are internal implementation details. You should test them indirectly through the public interface of the unit. Testing private methods directly couples tests to implementation details and discourages refactoring.
</details>

<details>
<summary>5. How can you test code that relies on Date.now() without introducing flakiness?</summary>
By designing the class or function to accept an optional clock or timestamp parameter (e.g. <code>consume(tokens, currentTime = Date.now())</code>), or by using mock timer utilities (like <code>mock.timers</code> in <code>node:test</code> or <code>vi.setSystemTime()</code>) to freeze and control simulated time.
</details>

<details>
<summary>6. What is the difference between DAMP and DRY in test suites?</summary>
<b>DRY</b> (Don't Repeat Yourself) aims to eliminate all code duplication. In tests, aggressive DRYness leads to complex shared helper functions and obscured test contexts. <b>DAMP</b> (Descriptive And Meaningful Phrases) prioritizes making each test self-contained, readable, and easy to understand from top to bottom, even if some setup code is repeated across tests.
</details>

<details>
<summary>7. How does Dependency Injection enable high-quality unit testing?</summary>
Dependency injection allows external collaborators (like databases, network APIs, or loggers) to be passed into a class via its constructor. During unit tests, you can inject fast in-memory Fakes or Spies instead of real infrastructure, keeping tests fast, isolated, and deterministic.
</details>

<details>
<summary>8. What is a "Unit of Work"?</summary>
A Unit of Work is a sequence of actions from an invocation to a noticeable result. It could be a simple mathematical calculation, or a method that changes internal state and notifies a collaborator. Defining units as behaviors rather than strictly individual functions leads to more resilient test suites.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: In-Memory Fake Key-Value Database
Implement a `FakeDatabase` class that implements a standard `{ get(k), set(k, v), delete(k), count() }` interface with simulated latency, and write a unit test demonstrating how to use it with a caching service.

<details>
<summary>View Solution</summary>

```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

export class FakeDatabase {
  constructor() {
    this.store = new Map();
  }
  async get(k) { return this.store.get(k) ?? null; }
  async set(k, v) { this.store.set(k, v); }
  async delete(k) { return this.store.delete(k); }
  count() { return this.store.size; }
}

class CacheService {
  constructor(db) { this.db = db; this.cache = new Map(); }
  async get(k) {
    if (this.cache.has(k)) return this.cache.get(k);
    const val = await this.db.get(k);
    if (val) this.cache.set(k, val);
    return val;
  }
}

describe('CacheService Unit Test', () => {
  it('reads from db on cache miss and caches subsequent reads', async () => {
    const fakeDb = new FakeDatabase();
    await fakeDb.set('user:1', { name: 'Ada' });

    const cacheService = new CacheService(fakeDb);

    // First read: cache miss -> queries fakeDb
    const user1 = await cacheService.get('user:1');
    assert.deepStrictEqual(user1, { name: 'Ada' });

    // Modify underlying DB to prove second read comes from memory cache
    await fakeDb.delete('user:1');
    const user2 = await cacheService.get('user:1');
    assert.deepStrictEqual(user2, { name: 'Ada' }); // Served from cache!
  });
});
```
</details>

### Exercise 2: Testing an Exponential Retry Wrapper with Spies
Write a unit test using `node:test`'s `mock.fn()` asserting that an exponential retry utility executes exactly 3 attempts when a simulated task rejects, before finally rejecting with the last error.

<details>
<summary>View Solution</summary>

```javascript
import { describe, it, mock } from 'node:test';
import assert from 'node:assert/strict';

async function retryTask(taskFn, maxAttempts = 3) {
  let attempt = 0;
  while (attempt < maxAttempts) {
    try {
      return await taskFn();
    } catch (err) {
      attempt++;
      if (attempt >= maxAttempts) throw err;
    }
  }
}

describe('retryTask Unit Test', () => {
  it('retries up to maxAttempts on continuous failure', async () => {
    const failingFn = mock.fn(async () => {
      throw new Error('Network timeout');
    });

    await assert.rejects(
      async () => retryTask(failingFn, 3),
      { name: 'Error', message: 'Network timeout' }
    );

    assert.strictEqual(failingFn.mock.callCount(), 3);
  });
});
```
</details>
