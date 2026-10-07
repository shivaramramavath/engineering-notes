# Concurrency Patterns

JavaScript is single-threaded, so there are no data races on memory, but async code still has **logical** races: two operations interleave at `await` points and leave state inconsistent, stale responses overwrite fresh ones, or thousands of parallel calls overwhelm a service. This note collects the patterns that come up repeatedly: choosing sequential versus parallel, limiting concurrency, cancelling, deduplicating, and avoiding stale results.

**Prerequisites:**
- [The event loop](./00-event-loop.md)
- [Promises](./01-promises.md) and [async/await](./02-async-await.md)
- [Async generics](./03-async-generics.md)

---

## The mental model

Between two `await`s, your code runs without interruption. At each `await`, **other code can run**, including another call to the same function.

```ts
let balance = 100;

async function withdraw(amount: number) {
  if (balance >= amount) {          // check
    await logToServer(amount);      // other code can run here
    balance -= amount;              // act on a stale check
  }
}

await Promise.all([withdraw(80), withdraw(80)]);
// balance is -60: both passed the check before either deducted
```

The check and the update are separated by an `await`, so they are not atomic. Keep read-modify-write sequences free of `await`, or guard them with a lock (below).

## Sequential, parallel, or limited

| Goal | Pattern |
|---|---|
| Each step needs the previous result | `await` in order |
| Independent tasks, small count | `Promise.all` |
| Independent tasks, large count | concurrency limit |
| Want every outcome even if some fail | `Promise.allSettled` |
| First success wins | `Promise.any` |
| First result wins (including failure) | `Promise.race` |

## Limiting concurrency

`Promise.all(items.map(fn))` starts every call at once. For hundreds or thousands of items, that can exhaust sockets, trip rate limits, or overload a database. A small worker-pool helper starts only N at a time:

```ts
async function mapLimit<T, R>(
  items: readonly T[],
  limit: number,
  fn: (item: T, index: number) => Promise<R>,
): Promise<R[]> {
  const results = new Array<R>(items.length);
  let next = 0;

  async function worker() {
    while (true) {
      const i = next++;                    // safe: no await between read and increment
      if (i >= items.length) return;
      results[i] = await fn(items[i], i);
    }
  }

  const workers = Array.from({ length: Math.min(limit, items.length) }, worker);
  await Promise.all(workers);
  return results;
}

const users = await mapLimit(ids, 5, (id) => fetchUser(id));   // at most 5 in flight
```

How it works: `limit` workers each pull the next unprocessed index until none remain. Results are stored by index so order is preserved. If any call rejects, `Promise.all` rejects and the rest of the work in the other workers continues until it finishes naturally. If you need to stop early, check an `AbortSignal` inside the loop.

Libraries such as `p-limit` and `p-queue` offer tested versions with more features.

## A simple semaphore (limit around arbitrary code)

```ts
class Semaphore {
  private waiters: (() => void)[] = [];
  constructor(private permits: number) {}

  async acquire(): Promise<void> {
    if (this.permits > 0) {
      this.permits--;
      return;
    }
    await new Promise<void>((resolve) => this.waiters.push(resolve));
  }

  release(): void {
    const next = this.waiters.shift();
    if (next) next();             // hand the permit directly to a waiter
    else this.permits++;
  }
}

const dbLimit = new Semaphore(10);

async function query(sql: string) {
  await dbLimit.acquire();
  try {
    return await db.run(sql);
  } finally {
    dbLimit.release();            // always release
  }
}
```

A semaphore with one permit is a **mutex**, which fixes the `withdraw` race above: wrap the check and update in `acquire`/`release`.

## Cancellation and timeouts

Promises cannot be cancelled, but cooperative cancellation works through `AbortSignal`:

```ts
async function fetchJson<T>(url: string, signal?: AbortSignal): Promise<T> {
  const res = await fetch(url, { signal });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return (await res.json()) as T;
}

const controller = new AbortController();
const p = fetchJson("/api/slow", controller.signal);
controller.abort();                        // p rejects with an AbortError
```

Pass the signal down through your own async functions, and check `signal.aborted` (or call `signal.throwIfAborted()`) between steps in long-running work. `AbortSignal.timeout(ms)` gives a signal that aborts after a delay, where supported.

A bare timeout wrapper using `Promise.race` ([promises](./01-promises.md)) only stops *waiting*. The underlying work keeps going unless it also receives the signal.

## Avoiding stale results ("latest wins")

A classic bug: the user types quickly, several searches are in flight, and a slow earlier response arrives **after** a later one and overwrites it.

```ts
let latestRequest = 0;

async function search(query: string) {
  const id = ++latestRequest;
  const results = await api.search(query);
  if (id !== latestRequest) return;       // a newer search started: discard this
  render(results);
}
```

Or cancel the old request when starting a new one:

```ts
let controller: AbortController | undefined;

async function search(query: string) {
  controller?.abort();
  controller = new AbortController();
  try {
    render(await api.search(query, { signal: controller.signal }));
  } catch (e) {
    if (e instanceof DOMException && e.name === "AbortError") return;   // expected
    throw e;
  }
}
```

Data-fetching libraries such as TanStack Query handle this for you ([server state](../19-react-and-frontend/07-server-state-tanstack-query.md)).

## Deduplicating in-flight work

If several callers ask for the same thing at once, share one promise:

```ts
const inflight = new Map<string, Promise<User>>();

function getUser(id: string): Promise<User> {
  let p = inflight.get(id);
  if (!p) {
    p = fetchUser(id).finally(() => inflight.delete(id));
    inflight.set(id, p);
  }
  return p;
}
```

Ten simultaneous `getUser("1")` calls produce one request. The entry is removed when it settles, so later calls fetch fresh data (or keep it longer if you want a cache, see [async generics](./03-async-generics.md)).

## Retry with backoff

Retry transient failures with increasing delays, and only for idempotent operations. A full version with the rules of thumb lives in [error handling strategies](../11-error-handling/03-error-handling-strategies.md).

## Debounce and throttle

```ts
function debounce<A extends unknown[]>(fn: (...args: A) => void, ms: number) {
  let timer: ReturnType<typeof setTimeout> | undefined;
  return (...args: A) => {
    clearTimeout(timer);
    timer = setTimeout(() => fn(...args), ms);
  };
}

const onInput = debounce((text: string) => search(text), 300);
```

Generic rest parameters (`A extends unknown[]`) preserve the wrapped function's argument types ([variadic tuple types](../10-advanced-types/06-variadic-tuple-types.md)). Debounce delays until activity stops. Throttle limits how often something runs.

## Streams and queues

For a producer/consumer flow, async iteration gives natural backpressure: the consumer controls the pace.

```ts
for await (const job of queue) {
  await handle(job);         // the next job is not pulled until this finishes
}
```

To process several at once while still pulling lazily, run N workers that each loop over the same async iterator ([iterators and generators](./04-iterators-and-generators.md)).

## CPU-bound work

All of the above is about **waiting** concurrently. It does nothing for CPU-heavy work, which blocks the single thread. Move it off-thread:

- **Node:** `worker_threads`.
- **Browsers:** Web Workers.
- **Elsewhere:** a separate process or service.

Pass data by message, and keep the main thread free to handle I/O ([event loop](./00-event-loop.md)).

## Important rules and misconceptions

- **`Promise.all` starts nothing.** The promises already started when you created them. `all` only waits.
- **`map(async ...)` starts everything immediately,** so it is unbounded parallelism.
- **No parallel memory access, but interleaved logic.** Atomicity ends at every `await`.
- **A thrown error in one task does not stop the others.** They keep running unless you cancel them.
- **Ordering of completion is not ordering of start.** Use indices or `Promise.all` to keep results aligned with inputs.

## Common mistakes

- Using `forEach(async ...)` and losing both ordering and error handling.
- Unbounded `Promise.all` over large inputs.
- Read-modify-write across an `await` without a lock.
- Letting a stale response overwrite a newer one.
- Starting work and never handling its rejection when another task fails first.
- Forgetting to release a semaphore in `finally`.
- Retrying non-idempotent operations.
- Assuming a timeout cancels work.

## Debugging

- Add timestamps to logs at start and end of each task to see actual overlap.
- Log the number of in-flight operations to confirm a limit is working.
- Race conditions are timing-dependent: try inserting `await sleep(random)` in tests to shake out ordering bugs.
- If memory or sockets spike, check for unbounded `Promise.all`.
- If results are out of order, check that you index results rather than push in completion order.

## Quick summary

- Single thread, but `await` points allow interleaving. Keep read-modify-write sequences atomic or guard them.
- Choose: sequential for dependencies, `Promise.all` for small independent sets, a concurrency limit for large ones.
- Cancel cooperatively with `AbortSignal`. Race-based timeouts stop waiting but not the work.
- Prevent stale results with "latest wins" checks or abort-on-new-request.
- Deduplicate concurrent identical calls by sharing the in-flight promise.
- CPU-bound work needs workers, because async does not make computation parallel.

**Next:** [13 Compiler and tsconfig](../13-compiler-and-tsconfig/README.md)
