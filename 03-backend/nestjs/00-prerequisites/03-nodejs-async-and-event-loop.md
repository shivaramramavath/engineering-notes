# Node.js Async and Event Loop

Node.js runs your JavaScript on a single thread and achieves high concurrency by never waiting: slow operations (network, disk, timers) are handed off, and their results are delivered later through callbacks, promises, and the event loop. Every NestJS route handler, database call, and queue consumer runs inside this model. Understanding it explains why one blocking line can stall an entire API, why `await` inside the wrong loop is slow, and why your logs appear in a surprising order.

---

## Overview

**What it is.** The Node.js runtime consists of the V8 JavaScript engine, **libuv** (the C library that provides the event loop, thread pool, and asynchronous I/O), and Node's built-in modules. The **event loop** is the mechanism that picks up completed work and runs the associated JavaScript callbacks, one at a time.

**Why it exists.** A thread-per-request server wastes memory and context-switch time while threads wait on I/O. Node uses one JavaScript thread and non-blocking I/O instead, which suits I/O-heavy workloads such as web APIs.

**Where it is used.** Every Node.js program, including all NestJS applications (Express and Fastify are both built on it).

**Why you should understand it.**

- A synchronous CPU-heavy task blocks **every** concurrent request.
- `async`/`await` is syntax over promises, which are scheduled through the microtask queue. Order of execution is deterministic once you know the queues.
- Performance tuning (clustering, worker threads, queues) only makes sense with this model in mind. See [Performance](../07-production/02-performance/README.md) and [Background Processing](../05-advanced/02-background-processing/README.md).

---

## Mental Model

One JavaScript thread. Many things happening in the background. A loop that delivers results.

```text
            ┌──────────────────────────────────────────┐
            │              Your JavaScript             │
            │        (single thread: V8 call stack)    │
            └───────────────┬──────────────────────────┘
                            │ starts async work
                            ▼
   ┌──────────────────────────────────────────────────────────┐
   │                   libuv / OS (background)                │
   │  ┌────────────┐  ┌──────────────┐  ┌──────────────────┐  │
   │  │  Timers    │  │ Network I/O  │  │  Thread pool     │  │
   │  │            │  │ (epoll,      │  │  (fs, crypto,    │  │
   │  │            │  │  kqueue,     │  │   zlib, dns)     │  │
   │  │            │  │  IOCP)       │  │  default 4 thrds │  │
   │  └─────┬──────┘  └──────┬───────┘  └────────┬─────────┘  │
   └────────┼────────────────┼───────────────────┼────────────┘
            │ completed      │ completed         │ completed
            ▼                ▼                   ▼
          ┌──────────────────────────────────────────┐
          │      Event loop picks up completions     │
          │   and runs their callbacks on the thread │
          └──────────────────────────────────────────┘
```

A restaurant analogy: one chef (the JS thread) takes an order, hands the long-running work to the oven (the OS or thread pool), and immediately starts on the next order. When the oven rings, the chef plates the result. If the chef stops to hand-knead dough for ten minutes (a synchronous CPU loop), no other order moves.

---

## Core Concepts

### Synchronous vs Asynchronous

```typescript
import { readFileSync, readFile } from 'node:fs';

const a = readFileSync('big.json');            // blocks the thread until done
readFile('big.json', (err, data) => { /* later */ });   // returns immediately
```

| | Synchronous | Asynchronous |
|---|---|---|
| Thread during the operation | Blocked | Free to run other code |
| Result delivered via | Return value | Callback, promise, event |
| Fine at startup? | Often yes | Yes |
| Fine in request handlers? | **No** | Yes |

### Concurrency vs Parallelism

- **Concurrency:** multiple tasks in progress, interleaved. Node's JavaScript thread is concurrent.
- **Parallelism:** multiple tasks executing at the same instant. Node achieves this only through the thread pool, worker threads, child processes, or multiple processes (cluster).

Your JavaScript is **never** executed in parallel on the main thread. Two callbacks never run at the same time, so you do not need locks for in-memory state within one process. You do need coordination across processes and across `await` points (see Common Mistakes).

### Callbacks

The original async pattern: pass a function to be called later.

```typescript
import { readFile } from 'node:fs';

readFile('config.json', 'utf8', (err, text) => {
  if (err) return console.error(err);
  console.log(text.length);
});
```

Convention: **error-first callbacks** (`(err, result)`). Nested callbacks ("callback hell") are hard to read and hard to error-handle, which is why promises replaced them.

### Promises

A promise is an object representing a value that will exist later.

```text
 pending ──► fulfilled (value)
    │
    └──────► rejected  (reason)
```

```typescript
const p = new Promise<number>((resolve, reject) => {
  setTimeout(() => resolve(42), 100);
});

p.then((v) => v + 1)
 .then((v) => console.log(v))        // 43
 .catch((e) => console.error(e))     // handles a rejection anywhere above
 .finally(() => console.log('done'));
```

Rules:

- A promise settles **once** (fulfilled or rejected) and never changes again.
- `.then()` returns a **new** promise, so you can chain.
- Returning a value from `.then` fulfills the next promise. Throwing rejects it. Returning a promise adopts its state.
- `.then` callbacks run on the **microtask queue**, never synchronously, even if the promise is already settled.

### `async` / `await`

Syntax sugar over promises.

```typescript
async function loadUser(id: number): Promise<User> {
  const row = await db.query('SELECT * FROM users WHERE id = $1', [id]);
  if (!row) throw new NotFoundError();    // becomes a rejected promise
  return row;                             // becomes the fulfilled value
}
```

- An `async` function **always** returns a promise.
- `await x` pauses **that function only** (not the thread) until `x` settles. The rest of the program continues. The continuation is scheduled as a microtask.
- `await` on a non-promise value still yields to the microtask queue.
- A `throw` or a rejected `await` can be caught with `try`/`catch`.

### Promise Combinators

| Combinator | Resolves when | Rejects when | Use for |
|---|---|---|---|
| `Promise.all([...])` | All fulfill (array of results, in input order) | **Any** rejects (fail fast) | Independent tasks that must all succeed |
| `Promise.allSettled([...])` | All settle (array of `{status, value\|reason}`) | Never | Collect every outcome, tolerate partial failure |
| `Promise.race([...])` | First settles (fulfilled **or** rejected) | First rejects | Timeouts |
| `Promise.any([...])` | First **fulfills** | All reject (`AggregateError`) | First successful of several sources |

```typescript
const [user, orders] = await Promise.all([getUser(id), getOrders(id)]);   // parallel
```

> `Promise.all` does not cancel the other promises when one rejects. They keep running.

### Sequential vs Parallel `await`

```typescript
// Sequential: ~ a + b
const a = await fetchA();
const b = await fetchB();

// Parallel: ~ max(a, b)
const [a2, b2] = await Promise.all([fetchA(), fetchB()]);
```

Use sequential execution only when `fetchB` depends on `fetchA`.

### The Event Loop Phases

Each iteration of the loop passes through these phases in order (simplified from the Node.js documentation):

```text
   ┌───────────────────────────┐
┌─►│          timers           │  setTimeout / setInterval callbacks whose time elapsed
│  └─────────────┬─────────────┘
│  ┌─────────────▼─────────────┐
│  │     pending callbacks     │  some deferred system-level callbacks (e.g. certain TCP errors)
│  └─────────────┬─────────────┘
│  ┌─────────────▼─────────────┐
│  │       idle, prepare       │  internal use only
│  └─────────────┬─────────────┘      ┌───────────────┐
│  ┌─────────────▼─────────────┐      │   incoming:   │
│  │           poll            │◄─────┤  connections, │  retrieve new I/O events,
│  └─────────────┬─────────────┘      │   data, etc.  │  run I/O callbacks;
│  ┌─────────────▼─────────────┐      └───────────────┘  may block here waiting for I/O
│  │           check           │  setImmediate callbacks
│  └─────────────┬─────────────┘
│  ┌─────────────▼─────────────┐
└──┤      close callbacks      │  e.g. socket.on('close')
   └───────────────────────────┘
```

### Microtasks and `process.nextTick`

Between phases, and between individual callbacks within a phase (since Node.js 11), Node drains two queues **completely** before continuing:

1. The **`process.nextTick` queue** (highest priority).
2. The **promise microtask queue** (`.then`, `await` continuations, `queueMicrotask`).

```text
 [macrotask: a timer callback, an I/O callback, a setImmediate callback, the main script]
        │
        ▼
 drain process.nextTick queue
        │
        ▼
 drain promise microtasks   (new microtasks added while draining also run now)
        │
        ▼
 next macrotask / next phase
```

Consequences:

- `nextTick` callbacks run before promise callbacks.
- A microtask that schedules another microtask forever **starves** the event loop (I/O never runs).
- Prefer `queueMicrotask` or promises. Use `process.nextTick` sparingly. It is mostly useful in library code that must emit events after a constructor returns.

### `setTimeout`, `setImmediate`, `setInterval`

| API | Runs in | Notes |
|---|---|---|
| `setTimeout(fn, ms)` | timers phase | `ms` is a **minimum** delay, not a guarantee |
| `setImmediate(fn)` | check phase | After the poll phase of the current iteration |
| `setInterval(fn, ms)` | timers phase | Does not wait for the previous run to finish |
| `process.nextTick(fn)` | After current operation | Before promise microtasks |
| `queueMicrotask(fn)` | Microtask queue | Same queue as promise callbacks |

### The Thread Pool

libuv has a pool of worker threads (default **4**, configurable with `UV_THREADPOOL_SIZE`, maximum 1024) for operations that have no non-blocking OS API:

- Most `fs` operations
- `dns.lookup`
- `crypto` CPU-heavy functions (`pbkdf2`, `scrypt`, `randomBytes`, `randomFill`)
- `zlib`
- Native addons such as `bcrypt` (the native one)

Network sockets use OS-level non-blocking I/O and **do not** use the thread pool.

If you start many password hashes at once, they queue behind four threads, and unrelated `fs` calls queue behind them. This is a real cause of latency spikes.

### Streams and Backpressure

Streams process data in chunks instead of loading it all into memory.

| Type | Role | Example |
|---|---|---|
| `Readable` | Source | `fs.createReadStream`, HTTP request body |
| `Writable` | Destination | `fs.createWriteStream`, HTTP response |
| `Duplex` | Both | TCP socket |
| `Transform` | Modify in the middle | `zlib.createGzip()` |

```typescript
import { createReadStream, createWriteStream } from 'node:fs';
import { pipeline } from 'node:stream/promises';
import { createGzip } from 'node:zlib';

await pipeline(
  createReadStream('big.log'),
  createGzip(),
  createWriteStream('big.log.gz'),
);
```

**Backpressure** means a slow consumer signals the producer to pause so memory does not grow without bound. `pipeline()` handles backpressure and cleanup (including error propagation and destroying streams). Prefer it over manual `.pipe()`.

### Errors in Async Code

| Style | How errors surface |
|---|---|
| Synchronous code | `throw` → `try`/`catch` |
| Callbacks | First argument `err` |
| Promises | Rejection → `.catch()` |
| `async`/`await` | `try`/`catch` around `await` |
| EventEmitter | `'error'` event (an unhandled `'error'` event throws) |

An **unhandled promise rejection** terminates the process by default in modern Node.js (see Version notes). Always handle or propagate rejections.

---

## How It Works

Putting the pieces together, a step-by-step trace of a Node process:

```text
1. Node starts, runs your main script top to bottom (synchronous).
        │   Async calls are registered with libuv; callbacks are queued for later.
        ▼
2. Main script finishes → drain process.nextTick queue → drain promise microtasks.
        │
        ▼
3. Event loop starts:
        │
        ├─ timers phase:   run due timer callbacks (microtasks drained after each)
        ├─ pending phase:  deferred system callbacks
        ├─ poll phase:     process completed I/O; if nothing is queued and no timers/immediates,
        │                   block here waiting for I/O (this is how an idle server sleeps)
        ├─ check phase:    run setImmediate callbacks
        └─ close phase:    run close callbacks
        │
        ▼
4. If there is still pending work (timers, open handles, open sockets), repeat from 3.
   Otherwise the process exits.
```

This last point explains why a script with an open HTTP server never exits, and why a stray `setInterval` or open database connection keeps a test run from finishing.

---

## Basic Example

```typescript
// order.ts
console.log('1: script start');

setTimeout(() => console.log('5: setTimeout'), 0);
setImmediate(() => console.log('6: setImmediate'));

Promise.resolve().then(() => console.log('4: promise microtask'));
process.nextTick(() => console.log('3: nextTick'));

console.log('2: script end');
```

```bash
npx ts-node order.ts
```

Typical output (CommonJS):

```text
1: script start
2: script end
3: nextTick
4: promise microtask
5: setTimeout        ← order of these two is NOT guaranteed
6: setImmediate         when scheduled from the main module
```

Step by step:

1. Synchronous code runs first: lines `1` and `2`.
2. Main script is done. Node drains `nextTick` (`3`), then promise microtasks (`4`).
3. The event loop starts. Whether the 0 ms timer is already "due" depends on process timing, so `setTimeout` vs `setImmediate` from the **main module** is nondeterministic.
4. Inside an **I/O callback**, `setImmediate` always fires before `setTimeout(…, 0)`, because the check phase directly follows the poll phase:

```typescript
import { readFile } from 'node:fs';

readFile(__filename, () => {
  setTimeout(() => console.log('timeout'), 0);
  setImmediate(() => console.log('immediate'));   // always first here
});
```

> In ES modules (`.mjs` or `"type": "module"`), the main module is itself evaluated asynchronously, so the relative order of `nextTick` and promise microtasks can differ from the CommonJS result above. Do not depend on that ordering. *Verify behavior on your Node.js version if you need it.*

---

## Practical Examples

### 1. Basic: Promisify a Callback API

```typescript
import { promisify } from 'node:util';
import { readFile } from 'node:fs';
import { readFile as readFileP } from 'node:fs/promises';   // built-in promise API (preferred)

const readFileAsync = promisify(readFile);
const text = await readFileP('config.json', 'utf8');
```

Prefer built-in `node:fs/promises`, `node:timers/promises`, and `node:stream/promises` over `promisify` where they exist.

### 2. Common: Parallel Fetch with Error Tolerance

```typescript
const results = await Promise.allSettled([getProfile(id), getOrders(id), getRecommendations(id)]);

const [profile, orders, recs] = results.map((r) => (r.status === 'fulfilled' ? r.value : null));
```

Use `allSettled` when a failing optional dependency should degrade the response rather than fail it.

### 3. Common: Timeout with `AbortSignal`

```typescript
async function fetchWithTimeout(url: string, ms = 3000) {
  const res = await fetch(url, { signal: AbortSignal.timeout(ms) });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.json();
}
```

`AbortSignal.timeout()` cancels the underlying request. A `Promise.race` timeout does not, because it only stops waiting.

### 4. Real-World: Bounded Concurrency

Doing 10,000 things with `Promise.all` at once exhausts connections and memory. Process in batches or use a limiter library (for example `p-limit`).

```typescript
async function mapLimit<T, R>(items: T[], limit: number, fn: (item: T) => Promise<R>): Promise<R[]> {
  const results: R[] = new Array(items.length);
  let next = 0;

  async function worker() {
    while (next < items.length) {
      const i = next++;          // safe: no await between read and increment
      results[i] = await fn(items[i]);
    }
  }

  await Promise.all(Array.from({ length: Math.min(limit, items.length) }, worker));
  return results;
}

await mapLimit(userIds, 10, (id) => sendEmail(id));
```

`next++` is safe because JavaScript on one thread cannot interleave between those two operations. Each `worker` pulls the next index only after finishing its previous item.

### 5. Real-World: Retry with Backoff

```typescript
import { setTimeout as sleep } from 'node:timers/promises';

async function retry<T>(fn: () => Promise<T>, attempts = 3, baseMs = 200): Promise<T> {
  let lastError: unknown;
  for (let i = 0; i < attempts; i++) {
    try {
      return await fn();
    } catch (err) {
      lastError = err;
      if (i < attempts - 1) await sleep(baseMs * 2 ** i);
    }
  }
  throw lastError;
}
```

### 6. Edge Case: `forEach` Does Not Await

```typescript
// BUG: forEach ignores the promises returned by the callback
items.forEach(async (item) => {
  await save(item);
});
console.log('done');   // prints BEFORE the saves finish

// Fix 1: sequential
for (const item of items) await save(item);

// Fix 2: parallel
await Promise.all(items.map((item) => save(item)));
```

### 7. Edge Case: Blocking the Event Loop

```typescript
import { createServer } from 'node:http';

createServer((req, res) => {
  if (req.url === '/block') {
    const end = Date.now() + 5000;
    while (Date.now() < end) {}          // 5 s of synchronous CPU work
  }
  res.end('ok');
}).listen(3000);
```

While `/block` runs, **every** other request, including `/`, waits. The server is single-threaded for JavaScript.

### 8. Edge Case: Async Race Across `await` Points

```typescript
let balance = 100;

async function withdraw(amount: number) {
  if (balance >= amount) {          // check
    await auditLog('withdraw');     // yields: other code can run here
    balance -= amount;              // act: balance may have changed
  }
}

await Promise.all([withdraw(80), withdraw(80)]);   // balance becomes -60
```

JavaScript does not run two statements at once, but `await` lets **other** code interleave. A check-then-act sequence that spans an `await` is a race condition. Fix it by making the check and act atomic (no `await` between), or by using a database transaction or lock. See [Transactions](../04-intermediate/02-database-foundations/04-transactions.md).

---

## Syntax / API / Commands

### Core APIs

| API | Purpose | Example |
|---|---|---|
| `Promise.all` / `allSettled` / `race` / `any` | Combine promises | `await Promise.all(tasks)` |
| `node:timers/promises` | Promise-based timers | `await setTimeout(1000)` |
| `node:fs/promises` | Promise-based file I/O | `await readFile(path, 'utf8')` |
| `node:stream/promises` `pipeline` | Safe stream composition | `await pipeline(a, b, c)` |
| `node:util` `promisify` | Wrap a callback API | `promisify(fn)` |
| `AbortController` / `AbortSignal` | Cancellation | `AbortSignal.timeout(3000)` |
| `queueMicrotask(fn)` | Schedule a microtask | |
| `process.nextTick(fn)` | Run before promise microtasks | |
| `node:events` `EventEmitter`, `once`, `on` | Event-driven code | `await once(emitter, 'ready')` |
| `node:async_hooks` `AsyncLocalStorage` | Context across async calls | request IDs, correlation IDs |
| `node:worker_threads` | Parallel JavaScript execution | CPU-bound work |
| `node:cluster` / `child_process` | Multiple processes | Use all CPU cores |
| `node:perf_hooks` `monitorEventLoopDelay` | Measure loop lag | |

### CLI and Environment

| Command / Variable | Purpose |
|---|---|
| `UV_THREADPOOL_SIZE=16 node app.js` | Resize the libuv thread pool (set before the pool is first used) |
| `node --inspect app.js` | Attach the debugger / Chrome DevTools |
| `node --cpu-prof app.js` | Write a CPU profile on exit |
| `node --heap-prof app.js` | Write a heap profile |
| `node --trace-warnings app.js` | Show stack for warnings |
| `node --unhandled-rejections=strict app.js` | Control unhandled-rejection behavior (see Version notes) |
| `node --enable-source-maps dist/main.js` | Map stack traces back to TypeScript |

### Measuring Event Loop Lag

```typescript
import { monitorEventLoopDelay } from 'node:perf_hooks';

const h = monitorEventLoopDelay({ resolution: 20 });
h.enable();

setInterval(() => {
  console.log(`p99 loop delay: ${(h.percentile(99) / 1e6).toFixed(1)} ms`);
  h.reset();
}, 5000);
```

A rising p99 means something is blocking the thread.

---

## Important Rules

1. **Never block the event loop in request-handling code.** No synchronous `fs`, no `*Sync` crypto, no long loops, no catastrophic regular expressions.
2. **Always handle rejections.** Use `try`/`catch` around `await`, or `.catch()`. An unhandled rejection can terminate the process.
3. **`await` does not block the thread.** It pauses only the current async function.
4. **`async` functions always return promises**, and a `throw` inside becomes a rejection.
5. **Run independent promises in parallel** with `Promise.all`. Do not `await` them one by one.
6. **`forEach` ignores returned promises.** Use `for...of` or `map` + `Promise.all`.
7. **Microtasks run before the next macrotask.** `nextTick` first, then promises.
8. **`setTimeout(fn, 0)` is not immediate.** The delay is a minimum, and timing depends on the loop.
9. **A promise settles once.** Later `resolve`/`reject` calls are ignored.
10. **Check-then-act across an `await` is a race.** Make it atomic or use external coordination.
11. **The thread pool is small (4 by default).** Heavy `fs` or `crypto` use can saturate it.
12. **Cancel what you start.** Use `AbortSignal` for requests, and clear timers and intervals on shutdown.

---

## Under the Hood

### Components

```text
┌──────────────────────────────── Node.js process ───────────────────────────────┐
│                                                                                │
│  ┌───────────┐   bindings   ┌──────────────┐   uv_* API   ┌─────────────────┐   │
│  │ V8 engine │◄────────────►│ Node core C++ │◄────────────►│ libuv           │   │
│  │ JS heap,  │              │ & JS modules  │              │ event loop,     │   │
│  │ call stack│              │ (fs, http,    │              │ thread pool,    │   │
│  │ microtask │              │  net, crypto) │              │ OS I/O polling  │   │
│  │ queue     │              └──────────────┘              └─────────────────┘   │
│  └───────────┘                                                                  │
└────────────────────────────────────────────────────────────────────────────────┘
```

- **V8** executes JavaScript and owns the microtask queue (promises).
- **Node core** bridges JavaScript to native operations and owns the `process.nextTick` queue.
- **libuv** owns the loop phases, timers, thread pool, and OS-level I/O polling (`epoll` on Linux, `kqueue` on macOS/BSD, `IOCP` on Windows).

### What `await` Compiles To (conceptually)

```typescript
async function f() {
  const x = await g();
  return x + 1;
}
```

Is semantically similar to:

```typescript
function f() {
  return Promise.resolve(g()).then((x) => x + 1);
}
```

The continuation after `await` is a microtask. That is why code after an `await` always runs **after** the current synchronous code finishes, even if the awaited value is already available.

### Timers

Timers are stored in a heap ordered by expiry time. The timers phase runs callbacks whose time has elapsed. A busy event loop delays them. Using `setInterval(fn, 1000)` does not guarantee a 1000 ms cadence. Under load it drifts.

### Memory

- Each pending promise, closure, and timer holds memory until it settles or fires.
- Unbounded `Promise.all` over a large array creates all the work at once.
- Event listeners that are never removed keep their closures alive. Node warns when more than 10 listeners are added to a single emitter event (`MaxListenersExceededWarning`).

### Cooperative Scheduling

JavaScript tasks are **run to completion**. Nothing preempts a running callback. The only way to yield is to finish the callback (or `await`, which splits your function into pieces). To process a huge array without freezing the server, yield periodically:

```typescript
import { setImmediate as yieldToLoop } from 'node:timers/promises';

for (let i = 0; i < items.length; i++) {
  heavy(items[i]);
  if (i % 1000 === 0) await yieldToLoop();   // let I/O callbacks run
}
```

For truly heavy work, prefer worker threads.

---

## Common Patterns

### Parallel Independent Calls

`await Promise.all([...])`. See above.

### Timeout + Cancellation

Use `AbortSignal.timeout(ms)` or an `AbortController` passed to the operation.

### Retry with Exponential Backoff and Jitter

Retry only idempotent operations. Add random jitter to avoid synchronized retry storms. See [Retry and Circuit Breaker](../08-architecture-and-patterns/04-real-world-patterns/05-retry-and-circuit-breaker.md).

### Worker Threads for CPU-Bound Work

```typescript
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';

if (isMainThread) {
  const worker = new Worker(__filename, { workerData: 40 });
  worker.on('message', (result) => console.log('fib =', result));
} else {
  const fib = (n: number): number => (n < 2 ? n : fib(n - 1) + fib(n - 2));
  parentPort!.postMessage(fib(workerData as number));
}
```

Workers run JavaScript in parallel in separate V8 isolates and communicate by message passing. For repeated tasks use a worker pool (for example `piscina`) instead of spawning per call.

### Offload to a Queue

Move slow or retryable work (emails, report generation, AI calls) out of the request path to a job queue. See [BullMQ](../05-advanced/02-background-processing/02-bullmq.md).

### Scale Across Cores with Processes

Run one Node.js process per CPU core (cluster module, PM2, or container replicas behind a load balancer). See [Clustering and Worker Processes](../07-production/02-performance/05-clustering-and-worker-processes.md).

### Request-Scoped Context with `AsyncLocalStorage`

```typescript
import { AsyncLocalStorage } from 'node:async_hooks';

const als = new AsyncLocalStorage<{ requestId: string }>();

function handle(requestId: string, work: () => Promise<void>) {
  return als.run({ requestId }, work);
}

function log(msg: string) {
  console.log(`[${als.getStore()?.requestId}] ${msg}`);
}
```

Context follows the async call chain without passing parameters. NestJS and logging libraries use this for correlation IDs. See [Request Logging and Correlation ID](../07-production/03-observability/03-request-logging-and-correlation-id.md).

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| Synchronous CPU work in a handler | All requests slow or hang during one request | JS is single-threaded. The loop is blocked | Move to a worker thread, queue, or separate service |
| Using `*Sync` APIs in requests (`readFileSync`, `pbkdf2Sync`) | Latency spikes under load | Blocks the loop | Use async versions |
| Missing `await` | Function returns before work completes. Errors lost | Promise is created but not awaited | Add `await`. Enable `@typescript-eslint/no-floating-promises` |
| `async` callback in `forEach` | Code after loop runs too early | `forEach` ignores returned promises | `for...of` or `Promise.all(map(...))` |
| Sequential `await` of independent calls | Response time is the sum, not the max | Needless serialization | `Promise.all` |
| Unbounded `Promise.all` over thousands of items | Memory spike, connection pool exhausted | All work starts at once | Batch, or limit concurrency |
| Unhandled promise rejection | Process crashes or logs `UnhandledPromiseRejection` | No `.catch`/`try` | Handle errors. Add a process-level handler for logging, then exit cleanly |
| `async` function in an event handler | Errors silently dropped | EventEmitter ignores returned promises | `try`/`catch` inside, or `events.on` with iteration |
| Check-then-act across `await` | Intermittent data corruption, negative counts | Other code runs at `await` | Transactions, locks, atomic operations |
| `Promise.race` used as a timeout | Operation continues after "timeout" | `race` does not cancel losers | `AbortSignal` |
| Forgetting to `return`/`await` inside `.then` | Next `.then` runs with `undefined` | Promise chain not linked | Return the inner promise |
| Leaving timers/intervals/connections open | Process (or test) never exits | Open handles keep the loop alive | Clear them on shutdown, `unref()` non-critical timers |
| Assuming `setTimeout(0)` runs "now" | Wrong ordering of logs | Macrotask after microtasks | Learn the queue order. Use `await` for ordering |
| Heavy `JSON.parse`/`stringify` on huge payloads | Event loop stalls | Synchronous and CPU-bound | Stream, paginate, or limit body size |
| Catastrophic regex (ReDoS) | One request pins CPU | Backtracking is synchronous | Safe regex, input length limits |

---

## Debugging

### Common Errors and Warnings

| Message | Meaning |
|---|---|
| `UnhandledPromiseRejection` / `[ERR_UNHANDLED_REJECTION]` | A promise rejected with no handler. Default is to crash the process on modern Node.js |
| `MaxListenersExceededWarning` | More than 10 listeners on one emitter event, often a leak |
| `Error: ECONNRESET`, `ETIMEDOUT` | Network connection closed or timed out. Handle and retry where safe |
| `Warning: Detected unsettled top-level await` | A top-level `await` never resolved and the process exited |
| `RangeError: Maximum call stack size exceeded` | Deep synchronous recursion. Async recursion via promises does not grow the stack |
| Process never exits | Open handles (timers, sockets, servers, DB pools) |

### Commands and Techniques

```bash
# Why won't the process exit?
node --trace-warnings app.js
# In code (diagnostic only):  process.getActiveResourcesInfo()

# Where is time going? (CPU profile; open the .cpuprofile in Chrome DevTools)
node --cpu-prof dist/main.js

# Attach a debugger
node --inspect dist/main.js

# Stack traces pointing at TypeScript
node --enable-source-maps dist/main.js

# Throttle/inspect load: see how loop delay changes
npx autocannon -c 100 -d 20 http://localhost:3000/
```

### What to Inspect

1. **Event loop delay** (`monitorEventLoopDelay`). If it is high while CPU is high, you have blocking code. If it is low while latency is high, you are waiting on I/O (database, network).
2. **CPU profile:** look for a single long synchronous function.
3. **Thread pool saturation:** if `fs`/`crypto` operations are slow while CPU is low, raise `UV_THREADPOOL_SIZE` or reduce concurrent hashing.
4. **Async stack traces:** use `async`/`await` (not raw callbacks) for better traces, and keep source maps enabled.
5. **Ordering bugs:** add temporary logs labeled with the phase (sync, `nextTick`, microtask, timer, immediate). Compare with the order in this document.

### Isolating the Problem

- Reproduce with the smallest script that shows the ordering or blocking.
- Replace suspect async calls with stubs that resolve after a fixed delay to see whether the issue is logic or timing.
- Use `Promise.allSettled` to see *every* failure instead of only the first.

---

## Performance

| Concern | Guidance |
|---|---|
| **Latency** | Keep handlers non-blocking. Parallelize independent I/O with `Promise.all` |
| **Throughput** | One process uses one core for JavaScript. Run multiple processes/replicas |
| **CPU-bound work** | Worker threads, a queue plus workers, or another service |
| **Thread pool** | Default 4. Tune `UV_THREADPOOL_SIZE` for heavy `fs`/`crypto`, and measure |
| **Memory** | Stream large payloads. Limit concurrency. Avoid retaining large closures |
| **Timers** | Avoid thousands of short intervals. Prefer one scheduler |
| **GC pauses** | Large heaps and allocation churn increase pauses. Profile with `--heap-prof` |
| **JSON** | Parsing and stringifying large objects is synchronous. Stream or paginate |

Rules of thumb:

- Measure first (event loop delay, CPU profile, p95/p99 latency), then optimize.
- Parallelism only helps up to the limits of downstream systems (database pool size, API rate limits).
- Compression (`zlib`) uses the thread pool. Offload it to a reverse proxy when possible.

---

## Security

- **Event loop denial of service.** One slow synchronous operation triggered by user input can stall the server. Sources: ReDoS, huge JSON bodies, expensive synchronous hashing, large synchronous loops over user-supplied arrays. Mitigate with input size limits, safe regex, rate limiting, and offloading.
- **Resource exhaustion through unbounded concurrency.** Accepting user-controlled batch sizes and running them with `Promise.all` can exhaust memory and connections. Cap batch sizes and concurrency.
- **Timeouts everywhere.** Outbound requests without timeouts allow slow upstreams to pile up pending promises. Set timeouts on every external call. Set server-level timeouts for slow clients.
- **Error leakage.** Do not send raw error objects (with stack traces and internals) from async handlers to clients.
- **Crashing on unhandled rejections is a feature.** Swallowing them hides corrupted state. Log, then restart under a process manager.
- **Randomness and hashing.** Use async, thread-pool-backed `crypto`/`bcrypt` for password hashing so the loop stays responsive. See [Password Hashing](../04-intermediate/06-authentication/03-password-hashing.md).

---

## Production Considerations

- **Graceful shutdown:** on `SIGTERM`, stop accepting new connections, finish in-flight work, close pools, then exit. See [Graceful Shutdown](../07-production/04-deployment/02-graceful-shutdown-and-process-management.md).
- **Process-level safety nets:** listen for `unhandledRejection` and `uncaughtException` to log, flush logs, and exit. Do not continue as if nothing happened.
- **Scaling:** one process per core (containers, PM2, or Kubernetes replicas). Keep application state out of process memory.
- **Observability:** export event loop delay, active handles, heap usage, and GC metrics. See [Metrics](../07-production/03-observability/05-metrics.md).
- **Health:** a stuck event loop cannot answer health checks. A failing readiness probe is an early warning of blocking code.
- **Resource limits:** set container memory limits and the Node.js heap size (`--max-old-space-size`) consistently with them.
- **Timeouts and backpressure:** set timeouts on database queries and HTTP clients. Limit concurrent work in queues.
- **Background work:** move retryable or slow tasks to a queue. See [Queues Fundamentals](../05-advanced/02-background-processing/01-queues-fundamentals.md).

---

## Best Practices

### Recommended

```typescript
// Parallel, with a timeout, with error handling
async function loadDashboard(userId: string) {
  const signal = AbortSignal.timeout(3000);
  try {
    const [profile, orders] = await Promise.all([
      fetchJson(`/users/${userId}`, signal),
      fetchJson(`/users/${userId}/orders`, signal),
    ]);
    return { profile, orders };
  } catch (err) {
    logger.error({ err, userId }, 'dashboard failed');
    throw err;
  }
}

// Async file read in a handler
const text = await readFile(path, 'utf8');
```

### Avoid

```typescript
// Sequential independent calls
const profile = await fetchJson(`/users/${id}`);
const orders = await fetchJson(`/users/${id}/orders`);

// Blocking in a request path
const text = readFileSync(path, 'utf8');

// Fire and forget with no error handling
sendEmail(user);      // floating promise: rejection becomes unhandled

// Async work in forEach
users.forEach(async (u) => await notify(u));
```

Why: the recommended versions keep the loop free, bound the waiting time, and make every failure observable. The avoided versions serialize, block, or lose errors.

Additional guidance:

- Enable the lint rules `@typescript-eslint/no-floating-promises` and `@typescript-eslint/no-misused-promises`.
- Prefer `node:`-prefixed imports (`node:fs`) for built-ins to make intent explicit.
- Use `async` functions consistently. Do not mix callbacks and promises in the same API.
- If a function must return a promise, always return a promise (even for early returns) to keep the contract uniform.
- For intentional fire-and-forget work, attach a `.catch()` that logs.

---

## Version / Compatibility Notes

| Version | Behavior |
|---|---|
| Node.js 11 | Microtasks (`nextTick` and promises) are drained **between individual timer/immediate callbacks**, matching browser behavior. Earlier versions drained only after a whole phase |
| Node.js 14.8 | Top-level `await` in ES modules (unflagged) |
| Node.js 15 | Default for unhandled promise rejections changed to **throw** (crash) instead of warn. Node 15 also added `node:timers/promises` |
| Node.js 18 | Global `fetch` and `AbortController`/`AbortSignal` available. *(Verify the stability status of `fetch` for your version.)* |
| `AbortSignal.timeout()` | Added in the Node.js 17.3 / 16.14 line. *Verify exact version.* |
| Even-numbered releases | Become LTS. Use an Active or Maintenance LTS in production. *Check the official Node.js release schedule for the current LTS lines and end-of-life dates.* |

- Behavior listed above is from the Node.js documentation and release notes as commonly cited. Treat exact version numbers as requiring verification against the official changelog for your runtime.
- **Specification vs implementation:** promises, `async`/`await`, and the microtask concept are defined by the ECMAScript and HTML-style job-queue models. The event loop **phases**, `process.nextTick`, `setImmediate`, the thread pool, and libuv are **Node.js implementation details**. Browsers, Deno, and Bun differ in details.
- NestJS supports specific Node.js versions per release. *Check the engine requirements of your installed `@nestjs/core`.*

---

## Real-World Use Cases

- **HTTP APIs:** handling thousands of concurrent connections with one thread per process, mostly waiting on databases.
- **Gateways and proxies:** streaming request/response bodies with backpressure.
- **File processing:** streaming CSV imports, log compression, uploads to object storage.
- **Real-time:** WebSockets and server-sent events with many idle connections.
- **Background workers:** BullMQ consumers processing jobs with controlled concurrency.
- **CLI tools:** parallel network calls, file watching, build tools.
- **Serverless functions:** short-lived handlers where cold start and async I/O dominate.
- **Failure scenarios in production:** an API that freezes under load because of one synchronous hash, JSON, or regex call. This is among the most common Node.js incident causes.

---

## Interview Questions

### Beginner

1. Is Node.js single-threaded?
   - Your JavaScript runs on one thread. Node itself uses additional threads (libuv thread pool, V8 helpers) and can create worker threads and processes.
2. What is the event loop?
   - The mechanism that waits for completed asynchronous operations and runs their callbacks on the JavaScript thread, one at a time.
3. What is the difference between a callback, a promise, and `async`/`await`?
   - Three styles for handling asynchronous results. Promises represent future values and chain. `async`/`await` is syntax over promises.
4. What does `await` do?
   - Pauses the current async function until the promise settles, without blocking the thread.

### Intermediate

1. What is the output order of `setTimeout(0)`, `setImmediate`, `Promise.then`, and `process.nextTick`?
   - Synchronous code, then `nextTick`, then promise microtasks, then timers/immediates (timers vs immediate order is not guaranteed from the main module).
2. What is the difference between `Promise.all`, `allSettled`, `race`, and `any`?
3. Why does `forEach(async ...)` not wait?
   - `forEach` ignores returned promises. Use `for...of` or `Promise.all(map())`.
4. What operations use the libuv thread pool?
   - Most `fs`, `dns.lookup`, some `crypto`, `zlib`, and native addons. Not network sockets.
5. How would you prevent a CPU-heavy task from blocking an API?
   - Worker threads, a queue with separate workers, or a separate service. Break up work with yields only as a stopgap.

### Advanced

1. Explain the phases of the event loop and where microtasks fit.
   - Timers, pending, idle/prepare, poll, check, close. Microtasks (`nextTick` first, then promises) drain between callbacks and phases.
2. How can microtasks starve the event loop?
   - A microtask that keeps scheduling microtasks prevents the loop from reaching I/O phases.
3. How would you diagnose high latency with low CPU in a Node.js service?
   - Likely waiting on I/O or saturated thread pool or connection pool. Check event loop delay, thread pool usage, and downstream latency.
4. What race conditions can exist in single-threaded JavaScript?
   - Check-then-act across `await` points, and cross-process races on shared resources.
5. When would you choose `cluster`, `worker_threads`, or container replicas?
   - Workers: parallel CPU-bound work sharing a process. Cluster/replicas: scale request handling across cores/hosts. Replicas are generally preferred in orchestrated environments.
6. What happens on an unhandled promise rejection, and how should production code respond?
   - Modern default is to throw and crash. Log, flush, exit, and let the supervisor restart. Fix the root cause.

---

## Quick Reference

```text
Execution order      sync code → nextTick queue → promise microtasks → timers → poll (I/O) → check (setImmediate) → close
Microtask priority   process.nextTick  >  promise .then / await / queueMicrotask
Thread pool          default 4 threads (UV_THREADPOOL_SIZE): fs, crypto, zlib, dns.lookup. Not sockets.
await                pauses the function, not the thread
async function       always returns a Promise; throw → rejection
Promise.all          all succeed or fail fast
Promise.allSettled   wait for all, never rejects
Promise.race         first to settle (does not cancel others)
Promise.any          first to fulfill
forEach(async)       does NOT await → use for...of or Promise.all(map)
Blocking             sync fs, *Sync crypto, long loops, JSON on huge data, ReDoS
CPU-bound work       worker_threads / queue / separate service
Cancellation         AbortController / AbortSignal.timeout(ms)
Context              AsyncLocalStorage
Measure              monitorEventLoopDelay, --cpu-prof, --heap-prof
```

---

## Key Takeaways

- Your JavaScript runs on one thread. Concurrency comes from non-blocking I/O, not from parallel JavaScript.
- A single blocking operation stalls every request. Keep request paths asynchronous and CPU-light.
- Order is deterministic: synchronous code, then `nextTick`, then promise microtasks, then event loop phases.
- `async`/`await` is promises underneath. `await` yields to other work but does not make code parallel. Use `Promise.all` for parallelism.
- `forEach(async)`, missing `await`, unbounded `Promise.all`, and check-then-act across `await` are the classic bugs.
- The libuv thread pool is small by default. Heavy `fs`, `crypto`, or `zlib` work can saturate it.
- Use streams (`pipeline`) for large data, `AbortSignal` for cancellation, and worker threads or queues for CPU-bound work.
- Handle every rejection. Treat unhandled rejections as bugs, and shut down gracefully.

---

## Related Topics

```text
JavaScript basics  →  01 TypeScript Essentials (Promise<T>, async types)
      ↓
[03 Node.js Async and Event Loop]
      ↓
04 HTTP and REST Basics  →  02-fundamentals (Controllers, Request Lifecycle)
      ↓
07-production/02-performance  →  05-advanced/02-background-processing
```

- [Prerequisites Overview](./README.md)
- [TypeScript Essentials](./01-typescript-essentials.md)
- [HTTP and REST Basics](./04-http-and-rest-basics.md)
- [Request Lifecycle](../02-fundamentals/09-request-lifecycle.md)
- [Performance Fundamentals](../07-production/02-performance/01-performance-fundamentals.md)
- [Request and Async Performance](../07-production/02-performance/02-request-and-async-performance.md)
- [Queues Fundamentals](../05-advanced/02-background-processing/01-queues-fundamentals.md)
- [Server-Sent Events and Streaming](../05-advanced/04-realtime/06-server-sent-events-and-streaming.md)
