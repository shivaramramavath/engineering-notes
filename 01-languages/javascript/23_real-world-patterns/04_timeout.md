# Timeout

A promise can stay pending forever: a server that accepts a connection and never answers, a lock that is never released, a dropped network. Without a timeout, one stuck call can hold a request, a UI spinner, or a worker indefinitely. A timeout turns "might hang" into "fails within N ms," which you can then handle.

## Prerequisites

- [Promises](../11_asynchronous-javascript/03_promises.md) and [Promise combinators](../11_asynchronous-javascript/04_promise-combinators.md) (`Promise.race`)
- [Cancellation and abort](../11_asynchronous-javascript/08_cancellation-and-abort.md)
- [Timers](../12_event-loop/03_timers.md)

---

## The Key Distinction

> **Timing out is not the same as cancelling.**

Racing a promise against a timer only makes *your code* stop waiting. The underlying operation (HTTP request, DB query, file read) keeps running in the background unless you also tell it to stop through an `AbortSignal` or its own cancellation API.

---

## Pattern 1: `Promise.race` with a Timer

Works for any promise, including ones you can't cancel.

```js
class TimeoutError extends Error {
  constructor(ms) {
    super(`Timed out after ${ms}ms`);
    this.name = 'TimeoutError';
  }
}

function withTimeout(promise, ms) {
  let timer;
  const timeout = new Promise((_, reject) => {
    timer = setTimeout(() => reject(new TimeoutError(ms)), ms);
  });
  return Promise.race([promise, timeout]).finally(() => clearTimeout(timer));
}

const data = await withTimeout(loadReport(), 3000);
```

Two details people miss:

- **Clear the timer** in `finally`. Otherwise a fast success leaves a live timer behind, which keeps a Node process alive until it fires and can leak in long-lived apps.
- If the original promise rejects *after* the timeout won the race, that rejection is already handled by `race`, so it won't cause an unhandled-rejection warning. But the work still ran.

---

## Pattern 2: `AbortSignal` (real cancellation)

For `fetch` and any API that accepts a signal, use it. The request is actually aborted.

```js
// Built-in timeout signal
const res = await fetch('/api/slow', { signal: AbortSignal.timeout(5000) });
```

When the time elapses, the promise rejects with a `DOMException` whose `name` is `'TimeoutError'`. A manual `controller.abort()` rejects with `'AbortError'` instead, so you can tell "too slow" from "cancelled":

```js
try {
  await fetch(url, { signal: AbortSignal.timeout(5000) });
} catch (err) {
  if (err.name === 'TimeoutError') showMessage('Server is taking too long');
  else if (err.name === 'AbortError') { /* cancelled on purpose */ }
  else throw err;
}
```

### Combining a timeout with a user cancel

```js
const controller = new AbortController();        // wired to a "Cancel" button

const signal = AbortSignal.any([controller.signal, AbortSignal.timeout(5000)]);
const res = await fetch(url, { signal });
```

`AbortSignal.timeout()` is available in modern browsers and Node 17.3+; `AbortSignal.any()` is newer (Node 20.3+, recent browsers). Check your target environments.

If you need a manual fallback:

```js
const controller = new AbortController();
const timer = setTimeout(() => controller.abort(new Error('timeout')), 5000);
try {
  await fetch(url, { signal: controller.signal });
} finally {
  clearTimeout(timer);
}
```

---

## Pattern 3: Timeouts in Node's Timer Promises

```js
import { setTimeout as sleep } from 'node:timers/promises';

const ac = new AbortController();
const op = doWork({ signal: ac.signal });

// Abort the work if it takes too long
const timer = sleep(2000, undefined, { signal: ac.signal }).then(() => ac.abort());
```

For simple waits, `await sleep(ms)` from `node:timers/promises` is cleaner than hand-rolling a promise around `setTimeout`. See [Node event loop](../16_nodejs/02_node-event-loop.md).

---

## Choosing Timeout Values

- Base them on the **expected latency distribution** (e.g. generous multiples of p99), not guesses. Too short causes false failures; too long defeats the purpose.
- Use **separate timeouts** where it matters: per-attempt vs total deadline. In a retry loop, each attempt should have a timeout, and the whole operation should have an overall deadline too.
- Make them **configurable** (env/config), not magic numbers.
- Timeouts should shrink as work flows downstream: if the user-facing request has a 10s budget, a backend call inside it shouldn't wait 30s.

---

## Common Mistakes

| Mistake | Fix |
|---|---|
| Using `Promise.race` and assuming the work stopped | Also pass an `AbortSignal` so the work cancels |
| Not clearing the timer | `clearTimeout` in `finally` |
| No timeout on outbound calls (fetch, DB, queue) | Set a default everywhere; `fetch` has none by default |
| Treating `TimeoutError` and `AbortError` the same | Check `err.name` |
| One giant timeout for a multi-step flow | Per-step timeouts plus an overall deadline |
| Retrying timed-out non-idempotent writes blindly | The first attempt may have succeeded; see [Retry](./03_retry.md) |
| Timeout shorter than the server's own processing time → constant failures | Measure real latency |

---

## Testing

```js
import { vi, it, expect, beforeEach, afterEach } from 'vitest';

beforeEach(() => vi.useFakeTimers());
afterEach(() => vi.useRealTimers());

it('rejects when the promise is too slow', async () => {
  const never = new Promise(() => {});
  const result = withTimeout(never, 1000);
  const assertion = expect(result).rejects.toThrow('Timed out after 1000ms');  // attach handler first

  await vi.advanceTimersByTimeAsync(1000);
  await assertion;
});

it('passes through the value when fast enough', async () => {
  await expect(withTimeout(Promise.resolve('ok'), 1000)).resolves.toBe('ok');
});
```

Attach the `expect(...).rejects` handler **before** advancing the timers, so the rejection never sits unhandled. See [Mocking](../21_testing/05_mocking.md).

---

## Quick Summary

- Every external call needs a timeout; otherwise it can hang forever.
- `Promise.race` + timer makes you **stop waiting**; `AbortSignal` makes the work **stop**. Prefer the signal when supported.
- `AbortSignal.timeout(ms)` gives a ready-made signal; its error name is `TimeoutError` (vs `AbortError` for manual aborts).
- Always clear manual timers; set per-step and overall deadlines.
- Combine with retry carefully, and test with fake timers.

**Next:** [Memoization](./05_memoization.md)
