# Concurrency Control

`Promise.all(items.map(task))` starts **everything at once**. For 10 items that's fine. For 10,000 URLs, files, or database writes, it exhausts sockets, memory, file descriptors, or the remote server's patience.

Concurrency control keeps the number of **in-flight async operations** under a limit while still using that parallelism, rather than running one at a time.

This note covers the practical patterns. For the underlying model (event loop, workers, shared memory), see [Concurrency vs parallelism](../17_concurrency-and-parallelism/01_concurrency-vs-parallelism.md) and [Concurrency control](../17_concurrency-and-parallelism/05_concurrency-control.md).

## Prerequisites

- [Promises](../11_asynchronous-javascript/03_promises.md), [Promise combinators](../11_asynchronous-javascript/04_promise-combinators.md), [Async/await](../11_asynchronous-javascript/05_async-await.md)

---

## The Spectrum

```text
sequential         limited concurrency         unbounded
for...await   ─────►   pool of N workers   ─────►   Promise.all(map)
 slow, safe            usually what you want         fast, can overload
```

| Approach | Behavior | Problem |
|---|---|---|
| `for (const x of xs) await task(x)` | One at a time | Slow when tasks are I/O-bound |
| `await Promise.all(xs.map(task))` | All at once | Overload; one rejection fails the whole batch |
| **Pool with limit N** | At most N at once | Slightly more code |

Limiting concurrency only helps **async I/O**. CPU-bound work blocks the single thread no matter how you schedule promises; use [worker threads](../17_concurrency-and-parallelism/03_worker-threads.md) for that.

---

## Pattern 1: `mapLimit` (workers pulling from a shared cursor)

Start N "workers"; each loops, taking the next item until none remain.

```js
async function mapLimit(items, limit, mapper) {
  const results = new Array(items.length);
  let next = 0;
  let failed = false;

  async function worker() {
    while (!failed) {
      const i = next++;
      if (i >= items.length) return;
      try {
        results[i] = await mapper(items[i], i);
      } catch (err) {
        failed = true;          // stop other workers from picking up new items
        throw err;
      }
    }
  }

  const workers = Array.from({ length: Math.min(limit, items.length) }, worker);
  await Promise.all(workers);
  return results;               // same order as the input
}

const pages = await mapLimit(urls, 5, async (url) => {
  const res = await fetch(url);
  return res.text();
});
```

Why it works: JavaScript is single-threaded, so `next++` is atomic between `await`s. No locks are needed. Results are written by index, so **order is preserved** even though completion order varies.

On failure this version **fails fast**: the first error rejects the whole call and no new items start. (Already-running tasks still finish in the background.)

### Collect failures instead of failing fast

```js
async function mapLimitSettled(items, limit, mapper) {
  return mapLimit(items, limit, async (item, i) => {
    try { return { status: 'fulfilled', value: await mapper(item, i) }; }
    catch (reason) { return { status: 'rejected', reason }; }
  });
}
```

The result shape matches `Promise.allSettled`. Choose per use case: fail-fast for "all or nothing," settled for "process as much as possible and report."

---

## Pattern 2: A Reusable Limiter

Wrap any async function call; extra calls queue until a slot frees up. This is the idea behind the popular `p-limit` library.

```js
function createLimit(concurrency) {
  let active = 0;
  const queue = [];

  const next = () => {
    if (active >= concurrency || queue.length === 0) return;
    active++;
    queue.shift()();
  };

  return (fn) =>
    new Promise((resolve, reject) => {
      queue.push(() => {
        Promise.resolve()
          .then(fn)
          .then(resolve, reject)
          .finally(() => { active--; next(); });
      });
      next();
    });
}

const limit = createLimit(3);
const results = await Promise.all(
  ids.map((id) => limit(() => fetchUser(id)))
);
```

This shape is useful when calls come from many places (a shared limiter for an API client) rather than one array. Note it accepts a **function**, not a promise: an already-created promise has already started running.

```js
// Wrong: all requests already started before the limiter sees them
ids.map((id) => limit(fetchUser(id)));

// Right: the limiter decides when to start
ids.map((id) => limit(() => fetchUser(id)));
```

For production, `p-limit` and `p-queue` (priorities, pause/resume, intervals) are well tested and handle edge cases.

---

## Choosing the Limit

- Start from the **bottleneck**: the downstream API's allowed concurrency, DB connection pool size, file-descriptor limits, or memory per task.
- Browsers cap concurrent connections per host (commonly around 6 on HTTP/1.1), so a limit far above that gains little for same-origin fetches.
- Tune by measurement. More concurrency stops helping (and starts hurting) once the downstream saturates.
- If the API publishes a **rate** limit (requests per second) rather than a concurrency limit, you also need [rate limiting](./09_rate-limiting.md); they're different controls.

---

## Related Controls

| Need | Tool |
|---|---|
| Max N tasks at once | Pool / `p-limit` (this note) |
| Max N tasks per second | [Rate limiter](./09_rate-limiting.md) |
| Process in fixed-size batches | Chunk the array, `await Promise.all(chunk)` per batch (simpler, but a slow item stalls its batch) |
| Stop work early | Pass an `AbortSignal` ([Cancellation](../11_asynchronous-javascript/08_cancellation-and-abort.md)) |
| Streaming data with backpressure | [Streams](../16_nodejs/06_streams.md) |

---

## Common Mistakes

| Mistake | Fix |
|---|---|
| `Promise.all(hugeArray.map(asyncFn))` | Use a limiter |
| Passing promises, not functions, to a limiter | Pass `() => task()` |
| Assuming results arrive in input order from a queue | Write by index (as `mapLimit` does) |
| Unhandled rejections from background tasks after a fail-fast | Settle/handle errors deliberately; use `allSettled`-style when appropriate |
| Using `forEach(async ...)` | `forEach` doesn't await; use `for...of` or the patterns above |
| Raising concurrency to "go faster" without measuring | Find the real bottleneck first |
| Using promise limiting for CPU-heavy work | Offload to worker threads |

---

## Testing

Assert that the maximum number of simultaneously running tasks never exceeds the limit.

```js
import { it, expect } from 'vitest';

it('never runs more than `limit` tasks at once', async () => {
  let running = 0;
  let maxRunning = 0;

  await mapLimit([...Array(20).keys()], 3, async () => {
    running++;
    maxRunning = Math.max(maxRunning, running);
    await new Promise((r) => setTimeout(r, 5));
    running--;
  });

  expect(maxRunning).toBeLessThanOrEqual(3);
});

it('preserves input order in results', async () => {
  const out = await mapLimit([30, 10, 20], 3, async (ms) => {
    await new Promise((r) => setTimeout(r, ms));
    return ms;
  });
  expect(out).toEqual([30, 10, 20]);
});
```

(The tiny real delays are fine here; for larger suites use fake timers, see [Mocking](../21_testing/05_mocking.md).)

---

## Quick Summary

- `Promise.all(map)` is **unbounded concurrency**, which is dangerous for large inputs.
- Use a worker pool (`mapLimit`) or a reusable limiter (`p-limit` style): at most N in flight, results in input order.
- Pass **functions** to limiters so work starts only when a slot is free.
- Decide fail-fast vs collect-all-errors up front.
- Concurrency limits ≠ rate limits; CPU-bound work needs workers, not promise tricks.

**Next:** [Event Emitter](./07_event-emitter.md)
