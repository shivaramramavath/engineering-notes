# Sync vs Async

## Synchronous code

Each statement finishes before the next begins. The thread is **blocked** while a statement runs.

```js
console.log("A");
const data = readHugeFileSync("big.csv");   // nothing else can run for the whole read
console.log("B");
```

In a browser, blocking the main thread freezes clicks, scrolling and rendering. On a server, it blocks **every** request.

## Asynchronous code

Start an operation, return immediately, and be notified later.

```js
console.log("A");
setTimeout(() => console.log("timer"), 0);
console.log("B");
// A, B, timer
```

The timer callback runs **after** the current code finishes, even with a `0` delay.

## Why one thread can do many things

```
┌─────────────┐   delegates    ┌──────────────────────────┐
│  JS thread  │ ─────────────► │ host: network, timers,   │
│ (call stack)│                │ file I/O, OS, thread pool│
└─────▲───────┘                └────────────┬─────────────┘
      │  callbacks / promise reactions       │ result ready
      └────────── event loop ◄───────────────┘
```

1. Your JS starts an operation and hands it to the host (browser or Node)
2. The host does the waiting **outside** the JS thread
3. When done, a callback is queued
4. The **event loop** runs it when the call stack is empty

Details in [Event Loop](../12_event-loop/01_event-loop.md).

## Async is not parallel

| | Async (concurrency) | Parallel |
|---|---------------------|----------|
| JS code runs | one piece at a time | simultaneously on several threads |
| Good for | **waiting** (I/O, timers) | **computing** (CPU-heavy work) |
| Tools | callbacks, promises, `async`/`await` | Web Workers, `worker_threads` |

```js
async function crunch() {
  for (let i = 0; i < 1e10; i++) {}      // async function, but still blocks the thread
}
```

`async` does not make CPU work faster or non-blocking.

## What is asynchronous

| Source | Examples |
|--------|----------|
| Timers | `setTimeout`, `setInterval` |
| Network | `fetch`, XHR, WebSocket |
| Files / OS | `fs.readFile`, `child_process` |
| Events | clicks, messages, `load` |
| Promises | `.then` reactions, `await` continuations |
| Scheduling | `queueMicrotask`, `requestAnimationFrame` |
| Workers | `postMessage` replies |

## Order of execution (working model)

1. Run the current script / task to completion (the **call stack**)
2. Run all queued **microtasks** (promise reactions, `queueMicrotask`)
3. Render (browser)
4. Take the next **macrotask** (timer, I/O, event) and repeat

```js
console.log("1 sync");
setTimeout(() => console.log("4 timeout"), 0);
Promise.resolve().then(() => console.log("3 microtask"));
queueMicrotask(() => console.log("3b microtask"));
console.log("2 sync");
// 1 sync, 2 sync, 3 microtask, 3b microtask, 4 timeout
```

## Async operations and data flow

You cannot return an async result directly:

```js
function getUser() {
  let user;
  fetch("/user").then((r) => r.json()).then((u) => { user = u; });
  return user;                               // undefined: the fetch has not finished
}
```

Instead return a promise (or accept a callback):

```js
const getUser = () => fetch("/user").then((r) => r.json());
const user = await getUser();
```

## Sync vs async APIs in Node

```js
import fs from "node:fs";
fs.readFileSync(path);                        // blocks (OK in startup scripts and CLIs)
fs.readFile(path, cb);                        // callback
import { readFile } from "node:fs/promises";
await readFile(path);                         // promise (preferred)
```

Use sync variants only at startup or in short scripts, never inside request handlers.

## Timing guarantees

- `setTimeout(fn, ms)` means "no sooner than `ms`", not "exactly at `ms`"
- Timers wait if the stack is busy
- Browsers clamp nested timers to ≥ 4 ms and throttle background tabs
- Do not use timers for precise timing; compare timestamps

```js
const start = performance.now();
setTimeout(() => console.log(performance.now() - start), 100);   // 100.x, possibly much more if busy
```

## Keeping the UI/server responsive

| Technique | Use |
|-----------|-----|
| Split long loops into chunks | `await scheduler.yield?.()` / `setTimeout(0)` between chunks |
| Move CPU work to a worker | Web Workers, `worker_threads` |
| Use async APIs for I/O | `fs/promises`, `fetch` |
| Stream large data | avoid loading everything in memory |
| Debounce/throttle events | fewer handler calls |

```js
async function processInChunks(items, size = 500) {
  for (let i = 0; i < items.length; i += size) {
    items.slice(i, i + size).forEach(handle);
    await new Promise((r) => setTimeout(r));      // let the event loop breathe
  }
}
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Expecting `setTimeout(fn, 0)` to run immediately | Runs after the current task and microtasks | `queueMicrotask` for "soon, same turn" |
| Returning data from a callback to the caller | Nothing to return to | Promises / `await` |
| Using sync APIs in servers | Blocks all requests | Async APIs |
| Assuming `async` means parallel or off-thread | CPU work still blocks | Workers |
| Relying on exact timer precision | Timers are minimums | Timestamps |
| Mixing sync and async completion in one API | Unpredictable order ("Zalgo") | Always async or always sync |

## Key takeaways

- JavaScript has one thread; async hands waiting to the host and resumes via the event loop
- Microtasks (promises) run before the next macrotask (timers, I/O)
- Async helps with waiting, not computing: use workers for CPU-bound work
- You cannot `return` an async result: return a promise instead

**Next:** [Callbacks](./02_callbacks.md)
