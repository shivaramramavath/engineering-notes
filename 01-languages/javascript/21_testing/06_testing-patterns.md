# Testing Patterns

Recurring techniques that keep test suites readable, reliable, and cheap to maintain. Most of them are about reducing noise so a failing test tells you something immediately.

## Prerequisites

- [Unit testing](./02_unit-testing.md), [Integration testing](./03_integration-testing.md), [Mocking](./05_mocking.md)

---

## 1. Design for Testability

The biggest "pattern" happens in the production code, not the test. Make dependencies explicit and keep logic separate from I/O.

```js
// Hard to test: hidden clock and network
export async function isTokenExpired() {
  const token = await fetch('/token').then(r => r.json());
  return Date.now() > token.expiresAt;
}

// Easy to test: pure core, injected boundaries
export const isExpired = (expiresAt, now) => now > expiresAt;

export async function isTokenExpired({ fetchToken, now = Date.now } = {}) {
  const token = await fetchToken();
  return isExpired(token.expiresAt, now());
}
```

Now `isExpired` is a trivial unit test, and the wrapper needs no module mocks. See [Dependency injection](../20_design-patterns/09_dependency-injection.md) and [Pure functions](../07_functional-programming/01_pure-functions-and-side-effects.md).

---

## 2. Test Data Factories and Builders

Inline giant object literals bury what matters. A factory supplies valid defaults; each test overrides only the field it cares about.

```js
let seq = 0;
export const makeOrder = (overrides = {}) => ({
  id: `order-${++seq}`,
  status: 'pending',
  items: [{ sku: 'A1', qty: 1, price: 10 }],
  ...overrides,
});

it('does not ship cancelled orders', () => {
  const order = makeOrder({ status: 'cancelled' });   // the relevant detail is obvious
  expect(canShip(order)).toBe(false);
});
```

For nested structures, a builder with chainable methods (`anOrder().withItem(...).cancelled().build()`) reads well, but a plain factory with overrides covers most needs.

---

## 3. Table-Driven Tests

When many inputs go through the same logic, list them as data:

```js
it.each([
  { input: '',          expected: false },
  { input: 'a@b.co',    expected: true  },
  { input: 'a@b',       expected: false },
  { input: ' a@b.co ',  expected: false },
])('isEmail($input) → $expected', ({ input, expected }) => {
  expect(isEmail(input)).toBe(expected);
});
```

Adding a case is one line, and the failure message names the failing input.

---

## 4. Testing Time-Based Utilities

Debounce, throttle, retry, and timeout logic can be tested instantly with fake timers; no real waiting.

```js
import { vi, it, expect, beforeEach, afterEach } from 'vitest';
import { debounce } from './debounce.js';

beforeEach(() => vi.useFakeTimers());
afterEach(() => vi.useRealTimers());

it('fires once after the quiet period', () => {
  const fn = vi.fn();
  const debounced = debounce(fn, 300);

  debounced('a'); debounced('b'); debounced('c');
  expect(fn).not.toHaveBeenCalled();

  vi.advanceTimersByTime(300);
  expect(fn).toHaveBeenCalledOnce();
  expect(fn).toHaveBeenCalledWith('c');
});
```

Related implementations: [Debounce](../23_real-world-patterns/01_debounce.md), [Throttle](../23_real-world-patterns/02_throttle.md), [Retry](../23_real-world-patterns/03_retry.md), [Timeout](../23_real-world-patterns/04_timeout.md).

Retry logic: use a stub that fails N times and then succeeds, and assert both the final result and the number of attempts.

```js
it('retries until success', async () => {
  const op = vi.fn()
    .mockRejectedValueOnce(new Error('fail'))
    .mockRejectedValueOnce(new Error('fail'))
    .mockResolvedValue('ok');

  await expect(retry(op, { retries: 3, delay: 0 })).resolves.toBe('ok');
  expect(op).toHaveBeenCalledTimes(3);
});
```

---

## 5. Testing Events and Callbacks

Use a mock function as the listener and assert on the calls:

```js
import { EventEmitter } from 'node:events';

it('emits "done" with the result', () => {
  const emitter = new EventEmitter();
  const onDone = vi.fn();
  emitter.on('done', onDone);

  runJob(emitter);

  expect(onDone).toHaveBeenCalledWith({ ok: true });
});
```

For something that emits asynchronously, wrap it in a promise:

```js
import { once } from 'node:events';

const [payload] = await once(emitter, 'done');   // resolves on the first 'done'
expect(payload).toEqual({ ok: true });
```

See [Events](../16_nodejs/05_events.md).

---

## 6. Testing Errors Precisely

Assert on the **type or code**, not only the message text (messages change; error classes are contracts).

```js
class ValidationError extends Error {
  constructor(field) { super(`${field} is invalid`); this.name = 'ValidationError'; this.field = field; }
}

expect(() => validate({})).toThrow(ValidationError);

await expect(save({})).rejects.toMatchObject({ name: 'ValidationError', field: 'email' });
```

Also confirm that the function does **not** swallow errors it shouldn't and that cleanup (`finally`) still runs. See [Custom errors](../10_error-handling/03_custom-errors.md).

---

## 7. Fixtures and Shared Setup

Keep setup close to the test that needs it.

- Use `beforeEach` for **reset**, not for hiding the important arrangement.
- Reach for a helper function (`setupCart({ items: 2 })`) over a deep `describe` nest of `beforeEach` hooks.
- Share fixtures only when they are **immutable**; never mutate a shared object across tests.

```js
function setup(overrides = {}) {
  const repo = createMemoryRepo();
  const mailer = { send: vi.fn() };
  const service = createUserService({ repo, mailer, ...overrides });
  return { service, repo, mailer };
}

it('sends a welcome email', async () => {
  const { service, mailer } = setup();
  await service.register({ email: 'a@x.com' });
  expect(mailer.send).toHaveBeenCalledOnce();
});
```

---

## 8. Custom Matchers

If you repeat the same multi-line assertion, give it a name:

```js
import { expect } from 'vitest';

expect.extend({
  toBeValidId(received) {
    const pass = typeof received === 'string' && /^[a-f0-9]{24}$/.test(received);
    return {
      pass,
      message: () => `expected ${received} ${pass ? 'not ' : ''}to be a valid id`,
    };
  },
});

expect(user.id).toBeValidId();
```

Register matchers in a setup file; add type declarations if you use TypeScript.

---

## 9. Snapshots, Used Sparingly

Good fits: small serialized outputs that you review in diffs (error messages, generated config). Poor fits: large objects, data containing timestamps/IDs (unstable), or anything nobody reads. A snapshot you update blindly is not a test.

Prefer explicit assertions when you know what matters; they document intent where a snapshot only records current output.

---

## 10. Property-Based Testing (Advanced)

Instead of hand-picked examples, state a property that must hold for *any* input and let a library generate cases. The `fast-check` library does this for JavaScript.

```js
import fc from 'fast-check';

it('reverse twice returns the original', () => {
  fc.assert(fc.property(fc.array(fc.integer()), (arr) => {
    expect([...arr].reverse().reverse()).toEqual(arr);
  }));
});
```

It's very effective for parsers, serializers, sorting, and round-trip logic (`decode(encode(x)) === x`). When a property fails, the library shrinks the input to a minimal counterexample.

---

## 11. Regression Tests for Bugs

When a bug is reported:

1. Write a test that **fails** and reproduces it.
2. Fix the code until it passes.
3. Keep the test, named after the behavior (`'does not double-charge on retry'`), not the ticket number.

This is the cheapest way a suite grows in value over time.

---

## Common Anti-Patterns

| Anti-pattern | Problem | Instead |
|---|---|---|
| Logic (`if`/loops) in tests | Tests need their own tests | Use table-driven cases |
| Giant "does everything" test | Failure location unclear | One behavior each |
| Tests that depend on each other | Order-sensitive, flaky | Independent setup |
| Real `setTimeout` waits | Slow, flaky | Fake timers |
| Excessive `beforeEach` nesting | Reader must chase context | Local `setup()` helper |
| Asserting on mock call internals | Brittle | Assert on results |
| Commented-out or skipped tests left forever | Hidden rot | Fix or delete |
| Testing the framework/library | Wasted effort | Test your own behavior |

---

## Organizing Tests

- **Colocate** unit tests next to the source (`pricing.js`, `pricing.test.js`) or mirror the tree in a `tests/` folder; choose one convention and keep it.
- Keep integration tests in their own directory or with a naming suffix (`*.int.test.js`) so you can run the fast suite separately.
- Run unit tests on every save, the full suite in CI, and E2E on merge/release.

---

## Quick Summary

- The best testing pattern is **testable design**: inject boundaries, separate pure logic.
- Use **factories** with overrides, **table-driven** tests, and a local `setup()` helper to keep tests short.
- Control **time** with fake timers; control **async** by awaiting everything.
- Assert on **error types**, not message strings.
- Use snapshots and property-based tests where they fit, not everywhere.
- Every fixed bug gets a **regression test**.

**Next:** Back to the [Testing README](./README.md), or continue to [Security](../22_security/README.md).
