# The Event Loop

The mechanism that lets Node's single JavaScript thread (`01-fundamentals/01-node-runtime.md`) handle many concurrent operations without blocking. This file covers what it actually does, and why code ordering sometimes surprises people.

## The core idea

```js
console.log("1");
setTimeout(() => console.log("2"), 0);
console.log("3");
```

```
1
3
2
```

Even with a `0ms` delay, `"2"` logs last. Node runs all synchronous code to completion first, and only then processes queued callbacks — `setTimeout` schedules its callback for _later_, not _immediately_, no matter how small the delay.

---

## The phases of the event loop

Each pass through the event loop moves through several phases, in order, looping continuously as long as the process has pending work:

```
   ┌───────────────────────────┐
┌─>│           timers          │  ← setTimeout, setInterval callbacks
│  └─────────────┬─────────────┘
│  ┌─────────────┴─────────────┐
│  │     pending callbacks      │  ← some system-level callbacks
│  └─────────────┬─────────────┘
│  ┌─────────────┴─────────────┐
│  │        poll                │  ← retrieve new I/O events, run I/O callbacks
│  └─────────────┬─────────────┘
│  ┌─────────────┴─────────────┐
│  │        check                │  ← setImmediate callbacks
│  └─────────────┬─────────────┘
│  ┌─────────────┴─────────────┐
└──┤     close callbacks         │  ← e.g. socket.on('close', ...)
   └───────────────────────────┘
```

You rarely need to reason about every phase by name in application code — the practical takeaway is simpler: **Node processes work in a specific, repeating order, not just "whenever it feels like it."**

---

## Microtasks: they run _between_ phases, not as a phase themselves

Promise callbacks (`.then()`, `await` continuations) and `process.nextTick()` are **microtasks** — they don't wait for the next event loop phase; they run immediately after the current synchronous code finishes, before the event loop continues to its next phase.

```js
console.log("1. sync");

setTimeout(() => console.log("4. setTimeout (macrotask)"), 0);

Promise.resolve().then(() => console.log("3. promise (microtask)"));

console.log("2. sync");
```

```
1. sync
2. sync
3. promise (microtask)
4. setTimeout (macrotask)
```

**Microtasks (Promises) always run before the next macrotask (`setTimeout`, I/O callbacks), even if the macrotask was scheduled first** with a `0ms`/immediate delay. This is one of the most commonly tested "gotcha" behaviors in Node interviews, and matters in practice whenever you're reasoning about the exact order several async operations will actually run in.

### `process.nextTick()` runs before even other microtasks

```js
Promise.resolve().then(() => console.log("promise"));
process.nextTick(() => console.log("nextTick"));
```

```
nextTick
promise
```

`process.nextTick()` (`02-core-modules/07-process.md`) jumps the queue ahead of regular Promise microtasks — rarely needed directly in application code, but worth knowing when reading library internals that rely on this ordering guarantee.

---

## Why this matters for I/O-bound work

```js
app.get("/users/:id", async (req, res) => {
  const user = await db.findUser(req.params.id); // doesn't block other requests
  res.json(user);
});
```

While `db.findUser()` is waiting on the database, Node's event loop is completely free to handle other incoming requests — the wait isn't "blocking a thread," it's just a pending operation the event loop will come back to once the database responds. This is why Node can handle a large number of concurrent I/O-bound requests efficiently on a single thread, and it's the mechanism underneath everything in `01-callbacks-promises-async-await.md`.

---

## What actually blocks the event loop

```js
// ❌ blocks EVERYTHING else the process is doing, for the entire duration
app.get("/compute", (req, res) => {
  let result = 0;
  for (let i = 0; i < 10_000_000_000; i++) result += i;
  res.json({ result });
});
```

Synchronous, CPU-bound code doesn't yield to the event loop at all — while it runs, no other request, timer, or I/O callback can be processed, no matter how many are queued up. This is the specific problem `worker_threads` (`02-core-modules/10-cluster-and-worker-threads.md`) solves.

### Common accidental event-loop blockers

- `JSON.parse`/`JSON.stringify` on a very large object
- A synchronous regular expression against a long string (especially a poorly written, "catastrophically backtracking" regex)
- `*Sync` versions of `fs` functions (`02-core-modules/01-fs.md`) used inside a request handler
- A large, synchronous loop or data transformation

---

## Measuring event loop lag

```js
import { monitorEventLoopDelay } from "node:perf_hooks";

const histogram = monitorEventLoopDelay();
histogram.enable();

setInterval(() => {
  console.log("Mean event loop delay (ms):", histogram.mean / 1e6);
}, 5000);
```

A rising event loop delay is a strong signal that something synchronous and CPU-heavy is blocking the thread periodically — a useful diagnostic in a production app that feels intermittently sluggish (see `15-performance/01-event-loop-performance.md`).

## Common mistakes

- **Assuming `setTimeout(fn, 0)` runs "immediately"** — it runs after the current synchronous code and all pending microtasks, not instantly.
- **Assuming Promise callbacks and `setTimeout` callbacks interleave in the order they were written** — microtasks (Promises) always drain fully before the next macrotask phase runs.
- **Writing synchronous, CPU-heavy code in a request handler** — blocks every other concurrent request for the duration; needs `worker_threads` or breaking the work into smaller async chunks.
- **Confusing "asynchronous" with "runs on another thread"** — Node's async I/O doesn't use another JavaScript thread; it's the same single thread, just not blocked while waiting.

## Quick summary

- The event loop processes callbacks in phases (timers, I/O, `setImmediate`, etc.), looping continuously
- Microtasks (Promise callbacks, `process.nextTick`) run between phases, before the next macrotask — always before a `setTimeout`, however small its delay
- I/O doesn't block the single JS thread; synchronous CPU-bound code does, entirely, for its full duration
- `worker_threads` is the answer when you have real CPU-bound work that would otherwise stall everything else

## Next

**`03-closures.md`** covers a language feature used constantly in the async patterns and middleware you'll write throughout Node code.
