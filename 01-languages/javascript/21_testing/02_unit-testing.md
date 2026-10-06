# Unit Testing

A unit test checks one small piece of logic in isolation: a function, a class, a module. It runs in milliseconds, needs no network or database, and points at exactly one place when it fails.

Examples here use Vitest; Jest's API is nearly identical (see [Vitest and Jest](./04_vitest-and-jest.md)).

## Prerequisites

- [Testing fundamentals](./01_testing-fundamentals.md)
- [Functions](../02_functions/README.md), [Error handling](../10_error-handling/README.md)

---

## What Is a "Unit"?

There is no official definition. In practice it's the smallest piece that has meaningful behavior from the caller's point of view: usually a function or a class's public API.

```text
 input ──► [ unit ] ──► output / side effect
              │
        (slow collaborators replaced with test doubles)
```

Pure functions are the easiest to unit test: same input, same output, no setup. This is a big reason to [separate logic from side effects](../07_functional-programming/01_pure-functions-and-side-effects.md).

---

## First Test

```js
// pricing.js
export function applyDiscount(price, percent) {
  if (price < 0) throw new RangeError('price must be >= 0');
  if (percent < 0 || percent > 100) throw new RangeError('percent must be 0-100');
  return Math.round(price * (1 - percent / 100) * 100) / 100;
}
```

```js
// pricing.test.js
import { describe, it, expect } from 'vitest';
import { applyDiscount } from './pricing.js';

describe('applyDiscount', () => {
  it('reduces the price by the given percentage', () => {
    expect(applyDiscount(200, 25)).toBe(150);
  });

  it('returns the original price for 0%', () => {
    expect(applyDiscount(99.99, 0)).toBe(99.99);
  });

  it('returns 0 for 100%', () => {
    expect(applyDiscount(50, 100)).toBe(0);
  });

  it('throws on a negative price', () => {
    expect(() => applyDiscount(-1, 10)).toThrow(RangeError);
  });
});
```

Note that for errors you pass **a function** to `expect`. If you call it directly, it throws before `expect` can catch it.

---

## What to Test: Think in Categories

For any function, walk through:

| Category | Examples |
|---|---|
| Happy path | Typical valid input |
| Boundaries | `0`, `1`, max, empty string, empty array, first/last element |
| Invalid input | `null`, `undefined`, wrong type, negative, `NaN` |
| Error paths | Does it throw/reject the right error? |
| Special values | `-0`, floating-point sums, Unicode, very large input |

You don't need all of them for every function, but the boundaries and error paths are where bugs hide.

---

## Choosing Matchers

```js
expect(2 + 2).toBe(4);                    // primitives, reference identity (Object.is)
expect({ a: 1 }).toEqual({ a: 1 });       // deep equality (ignores undefined props)
expect({ a: 1 }).toStrictEqual({ a: 1 }); // deep + checks types/undefined/sparse arrays
expect(0.1 + 0.2).toBeCloseTo(0.3);       // floats: never use toBe
expect([1, 2, 3]).toContain(2);
expect('hello world').toMatch(/world/);
expect(user).toMatchObject({ role: 'admin' }); // subset match
expect(fn).toThrow('message');
```

Common trap: `expect({}).toBe({})` fails because they're different objects. Use `toEqual` for structures.

---

## Table-Driven Tests

When the same logic runs over many inputs, parameterize instead of copy-pasting:

```js
import { it, expect } from 'vitest';

it.each([
  [100, 10, 90],
  [100, 50, 50],
  [19.99, 20, 15.99],
])('applyDiscount(%d, %d%%) → %d', (price, percent, expected) => {
  expect(applyDiscount(price, percent)).toBe(expected);
});
```

Each row becomes its own reported test, so failures show exactly which input broke.

---

## Async Code

Always `await` (or `return`) the promise; otherwise the test finishes before the assertion runs and passes falsely.

```js
// users.js
export async function fetchUserName(getUser, id) {
  const user = await getUser(id);
  if (!user) throw new Error(`User ${id} not found`);
  return user.name;
}
```

```js
import { it, expect } from 'vitest';

it('returns the name', async () => {
  const getUser = async () => ({ name: 'Asha' });
  await expect(fetchUserName(getUser, 1)).resolves.toBe('Asha');
});

it('rejects when the user is missing', async () => {
  const getUser = async () => null;
  await expect(fetchUserName(getUser, 9)).rejects.toThrow('User 9 not found');
});
```

Forgetting `await` before `expect(...).rejects` is one of the most common ways to write a test that can never fail.

Notice `getUser` is **passed in** rather than imported. That's what makes the unit testable without mocking the module system (see [Dependency injection](../20_design-patterns/09_dependency-injection.md)).

---

## Testing Classes and State

Test through the public API; set up state with the same methods users would call.

```js
class Counter {
  #count = 0;
  increment() { this.#count++; }
  get value() { return this.#count; }
}

it('increments from zero', () => {
  const c = new Counter();   // fresh instance per test
  c.increment();
  c.increment();
  expect(c.value).toBe(2);
});
```

Create a **new instance in each test** (or in `beforeEach`). Never share one mutable object across tests. Private fields (`#count`) can't be accessed from tests, which is a feature: it forces you to test behavior.

---

## Test Setup and Teardown

```js
import { beforeEach, afterEach } from 'vitest';

let cart;
beforeEach(() => { cart = new Cart(); });  // runs before every test
afterEach(() => { /* cleanup */ });
```

Keep setup visible. If a reader has to scroll to understand what a test starts with, inline the arrange step or use a small factory function (see [Testing patterns](./06_testing-patterns.md)).

---

## Common Mistakes

| Mistake | Why it hurts | Better |
|---|---|---|
| No assertion | Test always passes | Assert an outcome |
| Missing `await` on async | False pass | `await expect(...).resolves/rejects` |
| Testing private/internal details | Breaks on refactor | Assert public behavior |
| Re-implementing the logic in the test | Bug is duplicated | Use hard-coded expected values |
| Many unrelated asserts in one test | Unclear failures | One behavior per test |
| `toBe` on objects/floats | Wrong equality | `toEqual` / `toBeCloseTo` |
| Depending on test order | Fragile | Independent setup |

Re-implementation example:

```js
// Bad: same formula as the code, so it can't catch a wrong formula
expect(applyDiscount(200, 25)).toBe(200 * (1 - 25 / 100));

// Good: independently known answer
expect(applyDiscount(200, 25)).toBe(150);
```

---

## Debugging a Failing Unit Test

1. Run just that test: `npx vitest run -t "reduces the price"` or `npx vitest run pricing.test.js`.
2. Read the diff in the failure output; check expected vs received.
3. Add a `console.log` or run with a debugger (`node --inspect-brk` plus your test runner, or your editor's built-in test debugging).
4. Make the test smaller until the cause is obvious.
5. After fixing, keep the test as a regression test.

---

## Quick Summary

- A unit test targets one behavior with fast, isolated checks.
- Cover happy path, **boundaries**, invalid input, and error paths.
- Use `it.each` for tables of inputs; `toEqual` for structures; `toBeCloseTo` for floats.
- **Always await async assertions.**
- Test public behavior; inject dependencies to keep units easy to test.
- Expected values should be hard-coded, not recomputed.

**Next:** [Integration Testing](./03_integration-testing.md)
