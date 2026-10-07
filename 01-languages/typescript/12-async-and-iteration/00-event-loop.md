# The Event Loop

JavaScript runs your code on a **single thread**, yet handles timers, network calls, and user input without freezing. The mechanism that makes this work is the **event loop**. TypeScript does not change any of it: types are erased before the code runs. But every promise, `async`/`await`, and callback you write is scheduled by the event loop, so understanding it explains ordering bugs, "why did my timeout fire late", and why a long loop freezes an entire server.

**Prerequisites:**
- [Callbacks](../02-functions/02-callbacks.md)
- [Type erasure and runtime](../14-type-system-internals/00-type-erasure-and-runtime.md)

---

## The pieces

```text
        +-------------------+
        |    Call stack     |  <- runs your code, one frame at a time
        +---------+---------+
                  ^
                  | picks next job only when the stack is empty
        +---------+-----------------------------+
        |  Microtask queue                      |  promise callbacks (.then/await),
        |  (drained completely first)           |  queueMicrotask
        +---------+-----------------------------+
        |  Macrotask (task) queue               |  setTimeout, setInterval, I/O callbacks,
        |  (one task per loop turn)             |  UI events, setImmediate (Node)
        +---------------------------------------+
```

The loop, simplified:

1. Run the current script or task until the **call stack is empty**.
2. Run **all** microtasks, including any new ones queued while draining.
3. (Browsers) maybe render.
4. Take **one** macrotask from the queue, go to step 1.

Slow operations such as network and file I/O are handled by the runtime (browser or Node's native layer), outside your JavaScript thread. When they finish, they queue a callback for the event loop. That is why JavaScript is **concurrent but not parallel**: many operations can be in flight, but your code runs one piece at a time.

## Ordering in practice

```ts
console.log("1: sync");

setTimeout(() => console.log("5: timeout"), 0);

Promise.resolve().then(() => console.log("3: promise"));
queueMicrotask(() => console.log("4: microtask"));

console.log("2: sync");

// Output order: 1, 2, 3, 4, 5
```

- Synchronous code finishes first (1, 2).
- Then microtasks, in the order they were queued (3, 4).
- Then the timer, a macrotask (5), even with a delay of `0`.

**Rule:** promise continuations always run before the next timer or I/O callback.

### `await` is a microtask boundary

```ts
async function main() {
  console.log("a");
  await null;            // everything after this runs as a microtask
  console.log("c");
}

main();
console.log("b");
// a, b, c
```

Code before the first `await` runs synchronously. Code after it resumes later. See [async/await](./02-async-await.md).

## Node.js specifics

Node's loop has phases (timers, pending callbacks, poll for I/O, check, close). The parts you will notice:

- **`setImmediate`** runs in the "check" phase, right after I/O polling. From the main module, its order relative to `setTimeout(fn, 0)` is not guaranteed. Inside an I/O callback, `setImmediate` always runs first.
- **`process.nextTick`** runs **before** promise microtasks, after the current operation finishes. It is a separate, higher-priority queue. Overusing it can starve I/O.

```ts
Promise.resolve().then(() => console.log("promise"));
process.nextTick(() => console.log("nextTick"));
// nextTick, promise
```

## Blocking the loop

Because everything shares one thread, a long synchronous task blocks **all** other work: timers, requests, rendering, click handlers.

```ts
function sumTo(n: number) {
  let s = 0;
  for (let i = 0; i < n; i++) s += i;
  return s;
}

setTimeout(() => console.log("timer"), 10);
sumTo(5_000_000_000);      // timer cannot fire until this returns
```

Putting code in an `async` function does **not** move it off the thread. `async` changes how results are delivered, not where work runs.

Ways to avoid blocking:

- **Break work into chunks** and yield between them (`await new Promise(r => setTimeout(r))`, or `setImmediate` in Node).
- **Move CPU-heavy work to a thread:** Web Workers in browsers, `worker_threads` in Node.
- **Use native async I/O** (which is already off-thread) rather than sync variants like `fs.readFileSync` in a server.

### Microtasks can starve the loop

A microtask that keeps queueing another microtask prevents macrotasks from ever running:

```ts
function loop() {
  Promise.resolve().then(loop);   // timers and I/O never get a turn
}
```

## TypeScript angles

**Timer return types differ by environment.** In Node, `setTimeout` returns a `NodeJS.Timeout` object. In browsers it returns a number. With both `dom` and `@types/node` in scope, you can hit confusing errors. A portable annotation:

```ts
let timer: ReturnType<typeof setTimeout> | undefined;
timer = setTimeout(() => {}, 100);
clearTimeout(timer);
```

The `lib` option controls which globals exist (`dom`, `es2022`, and so on). See [target, module, and lib](../13-compiler-and-tsconfig/02-target-module-and-lib.md).

**`await` on non-promises is allowed** but still yields to the microtask queue, so ordering changes even when the value is plain.

**Promise-returning callbacks passed where `void` is expected** compile fine and cause floating promises. The ESLint rule `@typescript-eslint/no-misused-promises` catches this.

## Important rules and misconceptions

- **`setTimeout(fn, 0)` is not immediate.** It runs after the current task and all microtasks. Timers also have minimum delays (browsers clamp nested timers to about 4ms), and a delay is a *minimum*, not a guarantee.
- **Async does not mean parallel.** Two `await`ed network calls overlap because the waiting happens outside JavaScript. Two CPU-heavy functions do not.
- **Promises start immediately.** Creating a promise runs its executor synchronously. Only the continuation is deferred.
- **One thread does not mean no race conditions.** Between two `await`s, other code can run and change shared state ([concurrency patterns](./05-concurrency-patterns.md)).
- **Event loop details differ between browsers and Node.** The microtask-before-macrotask rule is common to both. Phases and `nextTick` are Node specifics.

## Common mistakes

- Running CPU-heavy loops in a request handler and freezing the server.
- Assuming `setTimeout(..., 0)` runs before a resolved promise callback.
- Using synchronous file or crypto APIs in server hot paths.
- Using `forEach(async ...)` and assuming it waits.
- Mixing `process.nextTick` and promises and being surprised by ordering.
- Using `number` as the type of a timer handle and breaking in Node.

## Debugging

- Log with labels and compare against the order rules above.
- **Measure event loop delay in Node** with `perf_hooks.monitorEventLoopDelay()`. Sustained high delay means something is blocking.
- **Profile:** `node --cpu-prof` or the browser Performance panel shows long tasks.
- **Browsers:** tasks over about 50ms are "long tasks" that hurt responsiveness.
- If a timer fires late, look for synchronous work or microtask floods before blaming the timer.
- More on tooling in [debugging](../18-testing-and-debugging/05-debugging.md) and [runtime performance](../22-performance/02-runtime-performance.md).

## Quick summary

- One thread, a call stack, a microtask queue, and a macrotask queue.
- Order: finish sync code, drain **all** microtasks (promises, `await`), then take **one** macrotask (timers, I/O, events).
- `await` and `.then` continuations are microtasks. `setTimeout` and `setImmediate` are macrotasks. In Node, `process.nextTick` runs before promise microtasks.
- Async is concurrent, not parallel. CPU-heavy work blocks everything unless chunked or moved to a worker.
- TypeScript types are erased and do not affect scheduling, but timer types and `lib` settings are environment-sensitive.

**Next:** [Promises](./01-promises.md)
