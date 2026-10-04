# Unit Testing

A **unit test** checks one small piece of code (a function, a class, a module) **in isolation**, quickly, and without external systems. Unit tests are the base of the pyramid: cheap to write, instant to run, and precise when they fail.

See also: [Testing Fundamentals](./01_testing-fundamentals.md), [Mocking](./05_mocking.md), [Dependency Injection](../20_design-patterns/09_dependency-injection.md), [Pure Functions](../07_functional-programming/01_pure-functions-and-side-effects.md).

## What counts as a "unit"

A unit is the smallest piece of **behavior** worth testing alone. Often that is a function or a class, but it may be a small group of collaborating pieces behind one public interface.

A good unit test:

- Runs in **milliseconds**
- Touches **no network, database, file system, or real clock**
- Does not depend on other tests
- Tests **one behavior** and has one reason to fail

## Your first unit tests

```js
// math.js
export function clamp(n, min, max) {
  if (min > max) throw new RangeError('min must be <= max');
  return Math.min(Math.max(n, min), max);
}
```

```js
// math.test.js
import { describe, it, expect } from 'vitest';
import { clamp } from './math.js';

describe('clamp', () => {
  it('returns the number when it is within range', () => {
    expect(clamp(5, 0, 10)).toBe(5);
  });

  it('returns the minimum when the number is too small', () => {
    expect(clamp(-5, 0, 10)).toBe(0);
  });

  it('returns the maximum when the number is too large', () => {
    expect(clamp(50, 0, 10)).toBe(10);
  });

  it('accepts values exactly on the boundaries', () => {
    expect(clamp(0, 0, 10)).toBe(0);
    expect(clamp(10, 0, 10)).toBe(10);
  });

  it('throws when min is greater than max', () => {
    expect(() => clamp(1, 10, 0)).toThrow(RangeError);
  });
});
```

Each test is tiny, named for a behavior, and independent. Together they cover the happy path, both ends, the exact boundaries, and the error case.

## Arrange, Act, Assert

Keep the three phases visible, separated by blank lines:

```js
it('marks an order as shipped and records the date', () => {
  // Arrange
  const order = createOrder({ id: 1, status: 'paid' });
  const now = new Date('2026-05-01T10:00:00Z');

  // Act
  const shipped = markShipped(order, now);

  // Assert
  expect(shipped.status).toBe('shipped');
  expect(shipped.shippedAt).toEqual(now);
});
```

If the **Act** step has more than one call, the test probably checks more than one behavior.

## Matchers you will use most

Examples use Vitest/Jest's `expect`; the same ideas exist in every framework (see [Vitest and Jest](./04_vitest-and-jest.md)).

```js
// Equality
expect(2 + 2).toBe(4);                         // primitives and identity (Object.is)
expect({ a: 1 }).toEqual({ a: 1 });            // deep equality (ignores undefined props)
expect({ a: 1 }).toStrictEqual({ a: 1 });      // deep equality, also checks types and undefined props

// Truthiness and null checks
expect(value).toBeTruthy();
expect(value).toBeFalsy();
expect(value).toBeNull();
expect(value).toBeUndefined();
expect(value).toBeDefined();

// Numbers
expect(0.1 + 0.2).toBeCloseTo(0.3);            // floating point: never use toBe for decimals
expect(score).toBeGreaterThan(10);
expect(score).toBeLessThanOrEqual(100);

// Strings
expect('hello world').toContain('world');
expect('hello world').toMatch(/^hello/);

// Arrays and objects
expect([1, 2, 3]).toContain(2);
expect([{ id: 1 }, { id: 2 }]).toContainEqual({ id: 2 });
expect([1, 2, 3]).toHaveLength(3);
expect({ a: 1, b: 2 }).toHaveProperty('a', 1);
expect({ a: 1, b: 2 }).toMatchObject({ a: 1 });     // subset match

// Exceptions
expect(() => risky()).toThrow();
expect(() => risky()).toThrow('exact or partial message');
expect(() => risky()).toThrow(TypeError);

// Negation
expect(list).not.toContain('x');
```

### `toBe` vs `toEqual`

```js
expect({ a: 1 }).toBe({ a: 1 });         // FAILS: two different objects
expect({ a: 1 }).toEqual({ a: 1 });      // passes: same structure
```

Use `toBe` for primitives and reference identity; `toEqual` or `toStrictEqual` for objects and arrays.

## Testing pure functions

Pure functions (same input, same output, no side effects) are the easiest to test: call them and check the result.

```js
export function formatPrice(cents, currency = 'USD') {
  return new Intl.NumberFormat('en-US', { style: 'currency', currency }).format(cents / 100);
}

it.each([
  [0, '$0.00'],
  [5, '$0.05'],
  [1999, '$19.99'],
  [100000, '$1,000.00'],
])('formats %i cents as %s', (cents, expected) => {
  expect(formatPrice(cents)).toBe(expected);
});
```

Design tip: push logic into pure functions and keep the code that talks to the outside world thin. The pure core gets thorough unit tests; the thin shell gets a few integration tests.

## Testing classes and state

```js
class Stack {
  #items = [];
  push(x) { this.#items.push(x); }
  pop() {
    if (this.#items.length === 0) throw new Error('Stack is empty');
    return this.#items.pop();
  }
  peek() { return this.#items.at(-1); }
  get size() { return this.#items.length; }
}

describe('Stack', () => {
  let stack;
  beforeEach(() => { stack = new Stack(); });      // fresh instance every test

  it('starts empty', () => {
    expect(stack.size).toBe(0);
  });

  it('pops items in last-in-first-out order', () => {
    stack.push(1);
    stack.push(2);
    expect(stack.pop()).toBe(2);
    expect(stack.pop()).toBe(1);
  });

  it('throws when popping an empty stack', () => {
    expect(() => stack.pop()).toThrow('Stack is empty');
  });
});
```

Test through the **public interface** (`push`, `pop`, `size`); never reach for `#items`. If behavior is hard to observe from the outside, that is a design signal, not a reason to expose internals.

## Testing async code

Always **return or await** the promise so the runner waits for it. Forgetting this makes tests pass without ever running their assertions.

```js
// async/await (preferred)
it('loads a user', async () => {
  const user = await getUser(1);
  expect(user.name).toBe('Ada');
});

// Rejections
it('rejects when the user does not exist', async () => {
  await expect(getUser(-1)).rejects.toThrow('not found');
});

// Resolves helper
it('resolves with the id', async () => {
  await expect(createUser({ name: 'Ada' })).resolves.toMatchObject({ name: 'Ada' });
});
```

Common mistake:

```js
// The promise is not awaited: the test finishes before the assertion runs
it('loads a user', () => {
  getUser(1).then((user) => {
    expect(user.name).toBe('Nobody');          // never reported
  });
});
```

Assert that the assertions ran when the check is inside a callback:

```js
it('calls the callback with data', async () => {
  expect.assertions(1);
  await new Promise((resolve) => {
    load((data) => { expect(data).toBe('ok'); resolve(); });
  });
});
```

### Testing timers and delays

Do not use real `setTimeout` waits in unit tests. Use **fake timers** (details in [Mocking](./05_mocking.md)):

```js
import { vi } from 'vitest';

it('debounces calls', () => {
  vi.useFakeTimers();
  const fn = vi.fn();
  const debounced = debounce(fn, 200);

  debounced(); debounced(); debounced();
  expect(fn).not.toHaveBeenCalled();

  vi.advanceTimersByTime(200);
  expect(fn).toHaveBeenCalledTimes(1);

  vi.useRealTimers();
});
```

## Testing for errors

```js
function parseAge(input) {
  const n = Number(input);
  if (!Number.isInteger(n)) throw new TypeError(`Invalid age: ${input}`);
  if (n < 0 || n > 150) throw new RangeError(`Age out of range: ${n}`);
  return n;
}

describe('parseAge', () => {
  it('parses a valid integer string', () => {
    expect(parseAge('42')).toBe(42);
  });

  it('throws TypeError for non-numeric input', () => {
    expect(() => parseAge('abc')).toThrow(TypeError);
    expect(() => parseAge('abc')).toThrow('Invalid age: abc');
  });

  it('throws RangeError for values out of range', () => {
    expect(() => parseAge('-1')).toThrow(RangeError);
    expect(() => parseAge('200')).toThrow(RangeError);
  });
});
```

Wrap the call in an arrow function: `expect(parseAge('abc')).toThrow()` would throw **before** `expect` runs. Prefer checking the error **type or a stable message fragment** rather than an entire long message that may change.

## Isolation: handling dependencies

A unit that talks to a database, the network, or the clock cannot be tested quickly and reliably unless you replace those dependencies. The simplest way is **dependency injection**: pass them in.

```js
// Hard to test: reaches out directly
import { db } from './db.js';
export async function getActiveUsers() {
  const users = await db.query('SELECT * FROM users');
  return users.filter((u) => u.active);
}

// Easy to test: dependency is a parameter
export function createUserQueries({ db }) {
  return {
    async getActiveUsers() {
      const users = await db.query('SELECT * FROM users');
      return users.filter((u) => u.active);
    },
  };
}
```

```js
it('returns only active users', async () => {
  const db = { query: async () => [{ id: 1, active: true }, { id: 2, active: false }] };   // a stub
  const { getActiveUsers } = createUserQueries({ db });

  await expect(getActiveUsers()).resolves.toEqual([{ id: 1, active: true }]);
});
```

No mocking library needed. When code is not designed for injection, module mocking (`vi.mock`, `jest.mock`) can substitute imports; see [Mocking](./05_mocking.md).

## What to isolate (and what not to)

| Replace with a double | Keep real |
|-----------------------|-----------|
| Network calls, databases, queues | Pure functions and simple value objects |
| File system, environment | The module under test and its tiny helpers |
| Current time, randomness | Standard library and language features |
| Slow or non-deterministic services | Collaborators that are fast and deterministic |
| Code with irreversible side effects (emails, payments) | Data structures the unit owns |

Over-isolating (mocking every collaborator) produces tests that mirror the implementation and miss real bugs. If a collaborator is fast and deterministic, **use the real one**.

## Edge cases checklist

For each function, ask:

```js
// A function that sums prices: what could go wrong?
sumPrices([]);                       // empty
sumPrices([5]);                      // single element
sumPrices([0.1, 0.2]);               // floating point
sumPrices([-5, 10]);                 // negatives
sumPrices([Number.MAX_SAFE_INTEGER, 1]);   // large numbers
sumPrices(null);                     // missing input
sumPrices(['5', 5]);                 // wrong types
```

| Input kind | Try |
|-----------|-----|
| Numbers | `0`, `-1`, `1`, `NaN`, `Infinity`, decimals, very large values |
| Strings | `''`, whitespace only, very long, Unicode, emoji, newlines, special characters |
| Arrays | `[]`, one item, duplicates, already sorted, unsorted, nested |
| Objects | `{}`, missing keys, extra keys, `null` prototype, circular references |
| Dates | Leap day, month ends, daylight saving changes, different time zones, invalid date |
| Collections of IDs | Duplicates, unknown ID, many items |
| Booleans/flags | Every combination for small sets |

## Table-driven (parameterized) tests

When many cases share a shape, list them in a table:

```js
describe('isPalindrome', () => {
  it.each([
    ['racecar', true],
    ['A man, a plan, a canal: Panama', true],
    ['hello', false],
    ['', true],
    ['a', true],
  ])('isPalindrome(%j) → %s', (input, expected) => {
    expect(isPalindrome(input)).toBe(expected);
  });
});

// With named objects
it.each([
  { name: 'empty cart', items: [], total: 0 },
  { name: 'one item', items: [{ price: 5 }], total: 5 },
  { name: 'two items', items: [{ price: 5 }, { price: 7 }], total: 12 },
])('total for $name is $total', ({ items, total }) => {
  expect(calcTotal(items)).toBe(total);
});
```

Adding a case is one line. See [Testing Patterns](./06_testing-patterns.md).

## Testing without a framework: `node:test`

Node ships a runner and assertion library, so small projects need no dependencies:

```js
import { describe, it, beforeEach, mock } from 'node:test';
import assert from 'node:assert/strict';
import { clamp } from './math.js';

describe('clamp', () => {
  it('clamps below the minimum', () => {
    assert.equal(clamp(-5, 0, 10), 0);
  });

  it('throws for an invalid range', () => {
    assert.throws(() => clamp(1, 10, 0), RangeError);
  });

  it('rejects asynchronously', async () => {
    await assert.rejects(() => loadThing('missing'), { message: /not found/ });
  });

  it('deep equality', () => {
    assert.deepEqual({ a: [1, 2] }, { a: [1, 2] });
  });

  it('spies with the built-in mock', () => {
    const fn = mock.fn((x) => x * 2);
    fn(2);
    assert.equal(fn.mock.callCount(), 1);
    assert.deepEqual(fn.mock.calls[0].arguments, [2]);
  });
});
```

```bash
node --test                    # run all *.test.js, *.spec.js and files under test/ directories
node --test --watch            # re-run on changes
node --test --experimental-test-coverage
```

`assert/strict` makes `assert.equal` behave like `===` and `deepEqual` like `deepStrictEqual`. Always import from `node:assert/strict`.

## Organizing unit tests

| Convention | Example |
|------------|---------|
| **Co-located** | `user.js` next to `user.test.js` (easy to find and move together) |
| **Mirrored folder** | `src/user.js` and `test/user.test.js` |
| **Naming** | `*.test.js` or `*.spec.js` (stay consistent) |
| **One file per module** | Tests for `cart.js` live in `cart.test.js` |
| **Shared helpers** | `test/helpers/` for builders and fixtures |

Keep the **public behavior** as the organizing principle: group by feature (`describe('checkout')`), then by scenario.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Not awaiting async assertions | Test passes without running them | `await` or `return` the promise; use `expect.assertions(n)` |
| `expect(fn()).toThrow()` without wrapping | The error escapes before `expect` | `expect(() => fn()).toThrow()` |
| `toBe` on objects or arrays | Fails due to reference comparison | `toEqual` / `toStrictEqual` |
| `toBe` on floats | `0.1 + 0.2 !== 0.3` | `toBeCloseTo` |
| Testing private state | Breaks on refactors | Test public behavior |
| Shared mutable fixtures across tests | Order-dependent failures | Create fresh state in `beforeEach` |
| Real timers and sleeps | Slow and flaky | Fake timers |
| Real `Date.now()` / `Math.random()` | Nondeterministic | Inject or fake them |
| Asserting on huge objects or entire error messages | Brittle | Assert on the fields that matter |
| Loops and branches inside tests | The test can have bugs | Table-driven tests with literal data |
| One test checking five behaviors | Failures are ambiguous | Split into focused tests |
| Over-mocking collaborators | Tests mirror the code and miss real bugs | Use real fast collaborators or in-memory fakes |
| Testing the framework or language | Wasted effort | Test your logic |
| Copying the implementation into the test to compute the expected value | The test repeats the bug | Hard-code expected results |

## Key takeaways

- A unit test checks one behavior of one piece of code, fast, and in isolation
- Structure tests as Arrange, Act, Assert, with names that describe behavior and condition
- Use `toBe` for primitives, `toEqual`/`toStrictEqual` for objects, `toBeCloseTo` for floats, and wrap throwing calls in a function
- Always await async assertions; use fake timers instead of real delays
- Make code testable by injecting dependencies and keeping logic in pure functions
- Cover edge cases: empty, boundary, invalid, and error paths
- Use table-driven tests for many similar cases; keep tests simple and literal
- Node's built-in `node:test` is enough for small projects

**Next:** [Integration Testing](./03_integration-testing.md)
