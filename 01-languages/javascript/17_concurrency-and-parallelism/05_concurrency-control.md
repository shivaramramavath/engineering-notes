# Concurrency Control

Starting everything at once is easy: `Promise.all(items.map(fn))`. It is also how you exhaust sockets, hit API rate limits, run out of memory, and take down your own database. **Concurrency control** limits how much work is in flight and keeps the rest waiting in an orderly way.

Related patterns: [Concurrency Control (real-world)](../23_real-world-patterns/06_concurrency-control.md) and [Async Patterns](../11_asynchronous-javascript/09_async-patterns.md).

## The problem

```js
// 10,000 simultaneous requests
const results = await Promise.all(urls.map((u) => fetch(u)));
```

Possible outcomes: `ECONNRESET`, `EMFILE` (too many open files), `429 Too Many Requests`, memory spikes, or a slow down for everyone sharing the service.

Sequential is safe but slow:

```js
for (const u of urls) await fetch(u);       // one at a time
```

The target is **bounded concurrency**: at most *N* tasks running at once.

## Batching (simple but wasteful)

```js
async function inBatches(items, size, fn) {
  const results = [];
  for (let i = 0; i < items.length; i += size) {
    const batch = items.slice(i, i + size);
    results.push(...(await Promise.all(batch.map(fn))));
  }
  return results;
}
```

The weakness: each batch waits for its **slowest** item before the next starts, leaving capacity idle.

## A worker-style limiter (sliding window)

Start `limit` loops that each pull the next item until none remain. A slot is reused the moment a task finishes.

```js
async function mapLimit(items, limit, fn) {
  const results = new Array(items.length);
  let next = 0;

  async function runner() {
    while (true) {
      const i = next++;                       // safe: single-threaded, no await between read and increment
      if (i >= items.length) return;
      results[i] = await fn(items[i], i);
    }
  }

  const runners = Array.from({ length: Math.min(limit, items.length) }, runner);
  await Promise.all(runners);
  return results;                             // same order as the input
}

const pages = await mapLimit(urls, 5, (u) => fetch(u).then((r) => r.text()));
```

If one task rejects, `Promise.all` rejects immediately, but the other runners **keep going** until they finish their current task and drain the list. To stop them, check an `aborted` flag:

```js
async function mapLimitSafe(items, limit, fn) {
  const results = new Array(items.length);
  let next = 0;
  let failed = false;

  async function runner() {
    while (!failed) {
      const i = next++;
      if (i >= items.length) return;
      try {
        results[i] = await fn(items[i], i);
      } catch (err) {
        failed = true;
        throw err;
      }
    }
  }

  await Promise.all(Array.from({ length: Math.min(limit, items.length) }, runner));
  return results;
}
```

### Collecting successes and failures

```js
async function mapLimitSettled(items, limit, fn) {
  return mapLimit(items, limit, async (item, i) => {
    try {
      return { status: 'fulfilled', value: await fn(item, i) };
    } catch (reason) {
      return { status: 'rejected', reason };
    }
  });
}
```

## Semaphore

A **semaphore** holds a number of permits. `acquire()` takes one (waiting if none are free); `release()` returns it. A limiter is just a semaphore around a function.

```js
class Semaphore {
  #permits;
  #waiters = [];

  constructor(permits) {
    this.#permits = permits;
  }

  acquire() {
    if (this.#permits > 0) {
      this.#permits--;
      return Promise.resolve();
    }
    return new Promise((resolve) => this.#waiters.push(resolve));
  }

  release() {
    const next = this.#waiters.shift();
    if (next) next();                         // hand the permit directly to the next waiter
    else this.#permits++;
  }

  async run(fn) {
    await this.acquire();
    try {
      return await fn();
    } finally {
      this.release();                         // always release, even when fn throws
    }
  }
}

const sem = new Semaphore(3);
const results = await Promise.all(urls.map((u) => sem.run(() => fetch(u))));
```

`Promise.all` still creates all the promises up front, but only 3 `fetch` calls are active at a time.

## Mutex

A mutex is a semaphore with one permit: only one task in a critical section. It fixes **async race conditions** (read, `await`, write):

```js
const mutex = new Semaphore(1);
let balance = 100;

async function withdraw(amount) {
  await mutex.run(async () => {
    const current = balance;
    await checkFraud();                       // other withdrawals wait
    if (current < amount) throw new Error('insufficient funds');
    balance = current - amount;
  });
}
```

Per-key locks (one mutex per user id, file name, and so on) allow unrelated work to run in parallel:

```js
const locks = new Map();

function withLock(key, fn) {
  const prev = locks.get(key) ?? Promise.resolve();
  const next = prev.catch(() => {}).then(fn);
  locks.set(key, next);
  next.finally(() => {
    if (locks.get(key) === next) locks.delete(key);     // avoid unbounded growth
  }).catch(() => {});
  return next;
}

await withLock(`user:${id}`, () => updateUser(id));
```

## Task queue with concurrency

A queue accepts tasks over time (unlike `mapLimit`, which takes a fixed array).

```js
class TaskQueue {
  #concurrency;
  #running = 0;
  #queue = [];
  #idleResolvers = [];

  constructor(concurrency = 4) {
    this.#concurrency = concurrency;
  }

  add(task) {
    return new Promise((resolve, reject) => {
      this.#queue.push({ task, resolve, reject });
      this.#drain();
    });
  }

  #drain() {
    while (this.#running < this.#concurrency && this.#queue.length) {
      const { task, resolve, reject } = this.#queue.shift();
      this.#running++;
      Promise.resolve()
        .then(task)
        .then(resolve, reject)
        .finally(() => {
          this.#running--;
          this.#drain();
          if (this.#running === 0 && this.#queue.length === 0) {
            this.#idleResolvers.splice(0).forEach((r) => r());
          }
        });
    }
  }

  onIdle() {
    if (this.#running === 0 && this.#queue.length === 0) return Promise.resolve();
    return new Promise((r) => this.#idleResolvers.push(r));
  }

  get size() { return this.#queue.length; }       // waiting
  get pending() { return this.#running; }         // active
}

const queue = new TaskQueue(3);
queue.add(() => fetch('/a'));
queue.add(() => fetch('/b'));
await queue.onIdle();
```

Production libraries: **p-limit**, **p-queue**, **async** (`mapLimit`, `queue`), and Piscina for worker threads. See also [Task Queue project](../24_projects/04_task-queue/).

## Priorities

Insert higher-priority tasks earlier in the waiting list:

```js
add(task, priority = 0) {
  return new Promise((resolve, reject) => {
    const entry = { task, priority, resolve, reject };
    const i = this.#queue.findIndex((e) => e.priority < priority);   // after equals: FIFO among equals
    if (i === -1) this.#queue.push(entry);
    else this.#queue.splice(i, 0, entry);
    this.#drain();
  });
}
```

## Backpressure

Concurrency limits control **execution**. **Backpressure** controls the **producer**: if work arrives faster than it is processed, the waiting queue grows without bound.

Strategies:

| Strategy | Idea |
|----------|------|
| **Bounded queue, make the producer wait** | `await queue.add(...)` only resolves when there is room |
| **Reject** | Return an error (HTTP `429` / `503`) when the queue is full |
| **Drop** | Discard the newest or oldest items (metrics, telemetry) |
| **Pull instead of push** | The consumer asks for more when ready (streams, async iterators) |

```js
class BoundedQueue {
  #items = [];
  #max;
  #spaceWaiters = [];
  #itemWaiters = [];

  constructor(max) { this.#max = max; }

  async push(item) {
    while (this.#items.length >= this.#max) {
      await new Promise((r) => this.#spaceWaiters.push(r));       // producer blocks when full
    }
    this.#items.push(item);
    this.#itemWaiters.shift()?.();
  }

  async shift() {
    while (this.#items.length === 0) {
      await new Promise((r) => this.#itemWaiters.push(r));        // consumer blocks when empty
    }
    const item = this.#items.shift();
    this.#spaceWaiters.shift()?.();
    return item;
  }
}
```

Node streams implement backpressure for you through `write()` returning `false` and `'drain'`; see [Streams](../16_nodejs/06_streams.md). Async iterators and generators give pull-based flow naturally:

```js
async function* readPages(urls) {
  for (const u of urls) yield await fetch(u).then((r) => r.json());   // produce on demand
}

for await (const page of readPages(urls)) {
  await save(page);                            // the producer cannot run ahead of the consumer
}
```

## Timeouts and cancellation

Bound the waiting and the running time. Pass an `AbortSignal` so cancelled tasks release their slots:

```js
async function withTimeout(fn, ms) {
  const signal = AbortSignal.timeout(ms);
  return fn(signal);
}

await sem.run(() => fetch(url, { signal: AbortSignal.timeout(5000) }));
```

A limiter that supports cancellation of **queued** (not yet started) tasks:

```js
add(task, { signal } = {}) {
  return new Promise((resolve, reject) => {
    if (signal?.aborted) return reject(signal.reason);

    const entry = { task, resolve, reject };
    this.#queue.push(entry);

    signal?.addEventListener('abort', () => {
      const i = this.#queue.indexOf(entry);
      if (i !== -1) {                          // still waiting: remove it
        this.#queue.splice(i, 1);
        reject(signal.reason);
      }
    }, { once: true });

    this.#drain();
  });
}
```

See [Cancellation and Abort](../11_asynchronous-javascript/08_cancellation-and-abort.md) and [Timeout](../23_real-world-patterns/04_timeout.md).

## Retries with limits

Retrying amplifies load. Combine retries with the limiter, **exponential backoff**, and **jitter**, and cap attempts:

```js
async function retry(fn, { retries = 3, base = 200 } = {}) {
  for (let attempt = 0; ; attempt++) {
    try {
      return await fn();
    } catch (err) {
      if (attempt >= retries) throw err;
      const delay = base * 2 ** attempt * (0.5 + Math.random() / 2);   // jitter
      await new Promise((r) => setTimeout(r, delay));
    }
  }
}

await sem.run(() => retry(() => fetch(url)));
```

Acquire the permit around each attempt (or around the whole retry, depending on whether waiting for the backoff should hold the slot). See [Retry](../23_real-world-patterns/03_retry.md).

## Rate limiting vs concurrency limiting

| | Concurrency limit | Rate limit |
|---|-------------------|------------|
| Controls | Tasks **in flight** at once | Tasks **started per time window** |
| Example | At most 5 open requests | At most 10 requests per second |
| Protects against | Resource exhaustion, slow dependencies | Provider quotas, `429` errors |
| Tool | Semaphore, queue | Token bucket, leaky bucket, sliding window |

Use both when a service has a quota and a connection limit. See [Rate Limiting](../23_real-world-patterns/09_rate-limiting.md).

## Choosing a limit

- **Network calls to one host**: start at 5 to 20, then measure
- **Database connections**: match your connection pool size
- **CPU tasks in workers**: about the number of cores (`os.availableParallelism()`)
- **File operations**: keep it modest (tens) to avoid `EMFILE`
- Unknown downstreams: start low, raise gradually, watch latency and errors

More concurrency helps until a bottleneck saturates; past that point throughput stays flat and latency rises.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `Promise.all(items.map(fn))` over huge lists | Unbounded concurrency | `mapLimit`, semaphore, queue |
| Batching with `Promise.all` per chunk | Idle slots while waiting for the slowest | Sliding-window limiter |
| Not releasing the permit on errors | Permits leak, queue freezes | `try/finally` |
| Unbounded queues | Memory growth when producers outpace consumers | Backpressure: bounded queue, reject, or drop |
| Read-modify-write across an `await` without a lock | Lost updates | Mutex, or per-key lock |
| One global lock for everything | Needless serialization | Lock per resource key |
| Retrying without backoff or caps | Retry storms | Exponential backoff, jitter, max attempts |
| Ignoring cancellation | Slots held by abandoned work | `AbortSignal` and timeouts |
| Nested use of the same limiter (a task waits on tasks in the same limiter) | Deadlock | Separate limiters, or avoid nested waits |
| Using `forEach(async ...)` | Not awaited, no control | `for...of` with `await`, or `mapLimit` |
| Hard-coding a limit | Wrong for other environments | Make it configurable and measure |

## Key takeaways

- Bound concurrency: never fire unlimited parallel work at a finite resource
- A sliding-window runner or semaphore keeps exactly *N* tasks active and reuses slots immediately
- A mutex (semaphore of 1) removes async race conditions; use per-key locks for finer granularity
- Queues need backpressure: bound them, reject, drop, or pull instead of push
- Always release permits in `finally`, and support timeouts and cancellation
- Combine retries with backoff and jitter, and do not confuse concurrency limits with rate limits
- Pick limits by measuring the bottleneck (network, database pool, cores)

**Next:** [Memory and Garbage Collection](../18_memory-and-garbage-collection/00_README.md)
