# Retry

Networks drop packets, servers restart, rate limiters push back. Many failures are **transient**: trying again a moment later works. A retry wrapper makes operations resilient to those, but a naive one can also turn a small outage into a big one. The details (what to retry, how long to wait, when to stop) matter more than the loop.

## Prerequisites

- [Async/await](../11_asynchronous-javascript/05_async-await.md) and [Async error handling](../11_asynchronous-javascript/06_async-error-handling.md)
- [Fetch](../15_networking/02_fetch.md)
- [Cancellation and abort](../11_asynchronous-javascript/08_cancellation-and-abort.md)

---

## Decide First: Is It Safe to Retry?

Retry only when **both** are true:

1. **The error is transient.** Network failure, timeout, `502/503/504`, `429` (after the server's requested delay). *Not* `400`, `401`, `403`, `404`, validation errors, or programmer bugs; those will fail the same way every time.
2. **The operation is idempotent**, or protected by an idempotency key. Retrying `GET` is safe. Retrying "charge the card" after a timeout can charge twice if the first attempt actually succeeded. Use idempotency keys (a unique ID the server uses to deduplicate) for such operations.

---

## Backoff and Jitter

Retrying immediately in a tight loop hammers a struggling server. Wait between attempts, and increase the wait each time (**exponential backoff**):

```text
attempt:  1     2      3       4
delay:   ~200  ~400   ~800   ~1600 ms   (base × 2^attempt, capped)
```

If a thousand clients all fail at the same moment and use identical delays, they all retry at the same moment too (a **retry storm** / thundering herd). **Jitter**, a random component, spreads them out. A widely used form is "full jitter": wait a random time between 0 and the exponential cap.

---

## Implementation

```js
const sleep = (ms, signal) =>
  new Promise((resolve, reject) => {
    if (signal?.aborted) return reject(signal.reason);
    const timer = setTimeout(() => { signal?.removeEventListener('abort', onAbort); resolve(); }, ms);
    const onAbort = () => { clearTimeout(timer); reject(signal.reason); };
    signal?.addEventListener('abort', onAbort, { once: true });
  });

async function retry(fn, {
  retries = 3,                 // retries after the first attempt
  delay = 200,                 // base delay in ms
  maxDelay = 5000,
  shouldRetry = () => true,    // (error, attempt) => boolean
  signal,
} = {}) {
  for (let attempt = 0; ; attempt++) {
    signal?.throwIfAborted();
    try {
      return await fn({ attempt, signal });
    } catch (err) {
      if (attempt >= retries || !shouldRetry(err, attempt)) throw err;
      const cap = Math.min(maxDelay, delay * 2 ** attempt);
      await sleep(Math.random() * cap, signal);   // full jitter
    }
  }
}
```

Design notes:

- `retries = 3` means up to **4 total attempts**. Be explicit in naming and docs.
- The **last error is rethrown** unchanged, so callers see the real failure.
- `signal` lets callers stop retrying (navigation, user cancel, shutdown) even during the wait.
- `shouldRetry` keeps policy out of the loop.

### Using it with `fetch`

`fetch` only rejects on network errors; HTTP error statuses resolve normally, so convert them to errors yourself.

```js
class HttpError extends Error {
  constructor(status, retryAfterMs) {
    super(`HTTP ${status}`);
    this.name = 'HttpError';
    this.status = status;
    this.retryAfterMs = retryAfterMs;
  }
}

const RETRYABLE = new Set([408, 429, 500, 502, 503, 504]);

const isTransient = (err) =>
  err instanceof TypeError ||                          // fetch network failure
  (err instanceof HttpError && RETRYABLE.has(err.status));

async function getJson(url, { signal } = {}) {
  return retry(async () => {
    const res = await fetch(url, { signal: AbortSignal.any([...(signal ? [signal] : []), AbortSignal.timeout(5000)]) });
    if (!res.ok) throw new HttpError(res.status);
    return res.json();
  }, { retries: 3, shouldRetry: isTransient, signal });
}
```

A few things to note:

- A per-attempt timeout (see [Timeout](./04_timeout.md)) is essential: without one, a hung request never fails, so it never retries. `AbortSignal.any` needs a reasonably recent runtime; check support for your target environments, or combine signals manually.
- Respect the server: if a `429`/`503` includes a `Retry-After` header, honor it rather than your own schedule.
- `AbortError` (user cancelled) must **not** be retried. The `isTransient` above excludes it because it isn't a `TypeError`/`HttpError`.

---

## Beyond the Basics

| Technique | What it adds |
|---|---|
| **Retry budget / cap on total time** | Prevents a request from retrying "forever" from the user's perspective |
| **Circuit breaker** | After repeated failures, stop calling the dependency for a cooldown period and fail fast, so you don't pile onto a dead service |
| **Idempotency keys** | Make retried writes safe |
| **Retry at one layer only** | If client, SDK, and gateway each retry 3×, one failure becomes 27 requests |
| **Metrics/logging per retry** | Retries hide problems unless you observe them |

---

## Common Mistakes

| Mistake | Fix |
|---|---|
| Retrying every error, including `4xx` | Classify with `shouldRetry` |
| No delay, or fixed delay with many clients | Exponential backoff **with jitter** |
| Unbounded retries | Cap attempts and/or total elapsed time |
| Retrying non-idempotent writes blindly | Idempotency keys, or don't retry |
| No timeout on each attempt | Add per-attempt timeouts |
| Swallowing the final error | Rethrow so callers can handle it |
| Retry stacked on retry across layers | Decide which layer owns retrying |
| Ignoring cancellation | Pass an `AbortSignal` through |

---

## Testing

A stub that fails N times, plus zero delay, makes it fast and deterministic ([Testing patterns](../21_testing/06_testing-patterns.md)).

```js
import { vi, it, expect } from 'vitest';

it('retries until success', async () => {
  const op = vi.fn()
    .mockRejectedValueOnce(new Error('fail'))
    .mockRejectedValueOnce(new Error('fail'))
    .mockResolvedValue('ok');

  await expect(retry(op, { retries: 3, delay: 0 })).resolves.toBe('ok');
  expect(op).toHaveBeenCalledTimes(3);
});

it('gives up after the limit and rethrows the last error', async () => {
  const op = vi.fn().mockRejectedValue(new Error('down'));
  await expect(retry(op, { retries: 2, delay: 0 })).rejects.toThrow('down');
  expect(op).toHaveBeenCalledTimes(3);   // 1 initial + 2 retries
});

it('does not retry non-transient errors', async () => {
  const op = vi.fn().mockRejectedValue(new Error('bad request'));
  await expect(retry(op, { retries: 3, delay: 0, shouldRetry: () => false })).rejects.toThrow();
  expect(op).toHaveBeenCalledTimes(1);
});
```

With `delay: 0` the jittered wait is `0`, so no fake timers are needed.

---

## Quick Summary

- Retry **transient** failures of **idempotent** operations; never retry bugs or `4xx`.
- Use **exponential backoff + jitter**, a **max attempts/time** limit, and a **per-attempt timeout**.
- Support `AbortSignal`, honor `Retry-After`, and rethrow the final error.
- Retry in one layer only; add a circuit breaker for dependencies that stay down.
- Test with a stub that fails N times.

**Next:** [Timeout](./04_timeout.md)
