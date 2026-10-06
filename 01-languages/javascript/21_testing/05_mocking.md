# Mocking

Mocking replaces a real dependency with a controllable stand-in so a test can be fast, deterministic, and focused. It's powerful and easy to overuse: every mock is a place where your test can disagree with reality.

Examples use Vitest's `vi`; Jest's `jest` object works the same way unless noted.

## Prerequisites

- [Unit testing](./02_unit-testing.md)
- [Closures](../06_closures/README.md) (mock functions are closures that record calls)
- [Dependency injection](../20_design-patterns/09_dependency-injection.md)

---

## Test Doubles

"Mock" is often used for everything, but these are different tools:

| Double | Purpose | Example |
|---|---|---|
| **Stub** | Returns canned answers | `getUser` always returns `{ name: 'Asha' }` |
| **Spy** | Wraps the real thing and records calls | Did `logger.warn` get called? |
| **Mock** | Pre-programmed with expectations about calls | Verify `sendEmail` called once with X |
| **Fake** | Simplified working implementation | In-memory repository |

Prefer **stubs and fakes** that let you assert on *results*. Asserting on *how* something was called ties the test to implementation.

---

## Mock Functions

```js
import { vi, it, expect } from 'vitest';

const fn = vi.fn();
fn('a', 1);
fn('b');

expect(fn).toHaveBeenCalledTimes(2);
expect(fn).toHaveBeenCalledWith('a', 1);
expect(fn).toHaveBeenLastCalledWith('b');
expect(fn.mock.calls[0]).toEqual(['a', 1]);   // raw call data

// Controlling return values
const getPrice = vi.fn()
  .mockReturnValueOnce(100)
  .mockReturnValue(200);
getPrice(); // 100
getPrice(); // 200

const load = vi.fn().mockResolvedValue({ ok: true });  // returns a resolved promise
const fail = vi.fn().mockRejectedValue(new Error('boom'));

const impl = vi.fn((x) => x * 2);                      // inline implementation
```

---

## Spying on Existing Methods

`vi.spyOn(obj, 'method')` wraps a real method. By default it still calls the original; you can override it.

```js
import { vi, it, expect, afterEach } from 'vitest';

afterEach(() => vi.restoreAllMocks());   // put originals back

it('logs a warning on bad input', () => {
  const warn = vi.spyOn(console, 'warn').mockImplementation(() => {});  // silence output too
  parse('???');
  expect(warn).toHaveBeenCalledWith(expect.stringContaining('invalid'));
});
```

Forgetting to restore leaks the mock into later tests, a classic source of "passes alone, fails together".

### clear vs reset vs restore

| Call | Effect (typical) |
|---|---|
| `mockClear()` | Clears call history only |
| `mockReset()` | Clears history **and** resets the implementation |
| `mockRestore()` | Resets and restores the original for spies |

Exact `mockReset` behavior has differed between Jest and Vitest versions, so check the docs for yours. A safe habit: enable `restoreMocks: true` (or `clearMocks`) in config so each test starts clean.

---

## Mocking Modules

Replace an entire import:

```js
// notify.js
import { sendEmail } from './mailer.js';
export async function notifyUser(user, msg) {
  if (!user.email) return false;
  await sendEmail(user.email, msg);
  return true;
}
```

```js
// notify.test.js
import { vi, it, expect } from 'vitest';
import { notifyUser } from './notify.js';
import { sendEmail } from './mailer.js';

vi.mock('./mailer.js', () => ({
  sendEmail: vi.fn().mockResolvedValue(undefined),
}));

it('sends an email when the user has an address', async () => {
  await expect(notifyUser({ email: 'a@x.com' }, 'hi')).resolves.toBe(true);
  expect(sendEmail).toHaveBeenCalledWith('a@x.com', 'hi');
});
```

Things to know:

- `vi.mock` / `jest.mock` calls are **hoisted** above imports, so the mock is in place before the module loads. This also means the factory can't reference variables declared in the test file unless they're created with `vi.hoisted()` (Vitest) or prefixed `mock` (Jest's rule).
- Partially mock when you only need to replace one export:

```js
vi.mock('./utils.js', async (importOriginal) => ({
  ...(await importOriginal()),
  now: vi.fn(() => 1_700_000_000_000),
}));
```

- Module mocks are **global to the test file**, which is a heavy tool. If you can pass the dependency as an argument instead, do that and skip module mocking entirely:

```js
export async function notifyUser(user, msg, { sendEmail }) { /* ... */ }
// test: notifyUser(user, 'hi', { sendEmail: vi.fn() })
```

---

## Fake Timers and Dates

Real clocks make tests slow and flaky. Control them.

```js
import { vi, it, expect, beforeEach, afterEach } from 'vitest';

beforeEach(() => vi.useFakeTimers());
afterEach(() => vi.useRealTimers());

it('calls the callback after the delay', () => {
  const cb = vi.fn();
  setTimeout(cb, 1000);

  vi.advanceTimersByTime(999);
  expect(cb).not.toHaveBeenCalled();

  vi.advanceTimersByTime(1);
  expect(cb).toHaveBeenCalledOnce();
});

it('uses a fixed date', () => {
  vi.setSystemTime(new Date('2026-01-15T10:00:00Z'));
  expect(new Date().toISOString()).toBe('2026-01-15T10:00:00.000Z');
});
```

When timers and promises mix, use the async variants (`vi.advanceTimersByTimeAsync`, `vi.runAllTimersAsync`) so microtasks get a chance to run. See [Event loop: microtasks and macrotasks](../12_event-loop/02_microtasks-and-macrotasks.md) for why ordering matters.

---

## Mocking Network Calls

| Approach | Pros | Cons |
|---|---|---|
| `vi.stubGlobal('fetch', vi.fn(...))` | Quick, tiny | Doesn't exercise your real request/response handling; easy to mock something unrealistic |
| **MSW** (network-level interception) | Your real `fetch` code runs; reusable handlers; realistic failures | Extra dependency and setup |
| Inject an API client | Simple, explicit | Requires designing for it |

For quick unit tests, stubbing `fetch` is fine:

```js
vi.stubGlobal('fetch', vi.fn().mockResolvedValue({
  ok: true,
  json: async () => ({ id: 1 }),
}));
// afterEach: vi.unstubAllGlobals()
```

For anything more than a trivial case, prefer MSW (shown in [Integration testing](./03_integration-testing.md)).

---

## What to Mock, and What Not To

**Good candidates** (non-deterministic, slow, or external):
- Network requests, email, payments
- Clock, timers, randomness
- Filesystem or process env (when needed)
- Anything with real-world side effects

**Avoid mocking:**
- Your own pure functions and simple helpers; just call them
- The thing you're testing
- Everything between two of your own modules "for isolation"; this leads to tests that pass while the real wiring is broken
- Types/data shapes you don't control, unless you keep them realistic

A useful check: *if I changed the implementation but kept behavior the same, would this test break?* If yes, you're over-mocking.

---

## Common Mistakes

| Mistake | Fix |
|---|---|
| Mock leaks into other tests | `restoreAllMocks` / config `restoreMocks`; reset in `afterEach` |
| `vi.mock` factory references an outer variable → "not defined" | Use `vi.hoisted()` or create the mock inside the factory |
| Mocking after the import evaluated | Module mocks must be declared at the top level so they hoist |
| Asserting only that a mock was called | Also assert on observable output |
| Mock returns an unrealistic shape | Build mocks from real responses; share fixtures |
| Forgot to restore fake timers | `useRealTimers()` in `afterEach` |
| `mockResolvedValue` on a function the code awaits synchronously | Match sync vs async to the real signature |

---

## Debugging Mocks

- Inspect `fn.mock.calls`, `fn.mock.results`, and `fn.mock.invocationCallOrder`.
- `console.log(fn.mock.calls)` shows exactly what the code under test passed.
- If a module mock "does nothing", confirm the **path/specifier matches exactly** what the code under test imports (relative path or package name).
- If a spy never fires, check you spied on the same object the code uses (e.g. you spied on a copy, or the module namespace is immutable in ESM, so mock the module instead).

---

## Quick Summary

- Know the doubles: **stub, spy, mock, fake**. Prefer stubs/fakes and assert on outcomes.
- `vi.fn()` creates controllable functions; `vi.spyOn()` wraps real methods; `vi.mock()` replaces modules (hoisted).
- Always **restore** mocks and timers between tests.
- Use **fake timers** for time and `setSystemTime` for dates.
- For HTTP, prefer network-level interception (MSW) for non-trivial cases.
- Mock at boundaries. Injecting dependencies often beats module mocking.

**Next:** [Testing Patterns](./06_testing-patterns.md)
