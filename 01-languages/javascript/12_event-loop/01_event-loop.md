# The Event Loop

JavaScript engines run your code on **one thread**. The runtime around the engine (the browser or Node.js) supplies the waiting, the queues, and the loop that feeds callbacks back to that thread.

## The pieces

| Piece | Role |
|-------|------|
| **Call stack** | where synchronous code runs, one frame per active function call |
| **Heap** | memory for objects |
| **Host APIs** | timers, network, file system, DOM events: do their waiting **outside** the JS thread |
| **Task (macrotask) queues** | callbacks from timers, I/O, UI events, messages |
| **Microtask queue** | promise reactions, `queueMicrotask`, `MutationObserver` callbacks |
| **Event loop** | repeatedly picks the next job when the stack is empty |

```
         JS thread                         Host (browser / Node)
   ┌──────────────────┐   start work   ┌──────────────────────────┐
   │ call stack       │ ─────────────► │ timers, network, disk,   │
   │                  │                │ DOM events, workers      │
   └────────▲─────────┘                └───────────┬──────────────┘
            │ run next job                         │ completion
      ┌─────┴──────┐ ◄──── queues ◄────────────────┘
      │ event loop │
      └────────────┘
```

## One turn of the loop (simplified)

```text
loop forever:
  1. pick a macrotask from a queue → run it to completion
  2. run ALL microtasks until the microtask queue is empty
  3. (browser) if it is time to render: run rAF callbacks, style, layout, paint
```

Key properties:

- **Run to completion**: a running task is never interrupted by another task
- Microtasks run **after every task and after every callback** that leaves the stack empty
- Timers only guarantee a **minimum** delay: they wait for the stack and queues

## Walk-through

```js
console.log("start");

setTimeout(() => console.log("timeout"), 0);

Promise.resolve().then(() => console.log("promise"));

console.log("end");
// start, end, promise, timeout
```

| Step | Stack | Queues | Output |
|------|-------|--------|--------|
| 1 | script | | `start` |
| 2 | script | macrotask: timeout callback (after host timer fires) | |
| 3 | script | microtask: promise reaction | |
| 4 | script | | `end` |
| 5 | (empty) | run microtasks | `promise` |
| 6 | (empty) | run next macrotask | `timeout` |

## Browser event loop

A browser has several task queues (timers, DOM events, networking, UI) and chooses among them, but the model is the same. After each task it performs a **microtask checkpoint**, and at suitable moments (typically every ~16.7 ms on a 60 Hz display) a **rendering opportunity**:

```text
task → microtasks → [rendering: rAF callbacks → style → layout → paint] → next task
```

| Concept | Detail |
|---------|--------|
| Long task | any task over ~50 ms: blocks input and rendering |
| Rendering opportunity | skipped if nothing changed or the tab is hidden |
| `requestAnimationFrame` | runs right before the next paint |
| `requestIdleCallback` | runs when the loop is idle (not all browsers) |
| Input events | queued as tasks; blocked by long tasks (poor INP) |
| Background tabs | timers throttled (≥ 1 s, even once per minute under intensive throttling) |

## Node.js event loop (libuv phases)

Node's loop cycles through ordered **phases**, each with its own queue of callbacks:

```text
   ┌──────────────────────┐
┌─►│ timers               │  setTimeout / setInterval callbacks that are due
│  ├──────────────────────┤
│  │ pending callbacks    │  some deferred system/network errors
│  ├──────────────────────┤
│  │ idle, prepare        │  internal
│  ├──────────────────────┤
│  │ poll                 │  retrieve new I/O events, run I/O callbacks (may wait here)
│  ├──────────────────────┤
│  │ check                │  setImmediate callbacks
│  ├──────────────────────┤
└──┤ close callbacks      │  socket.on("close") etc.
   └──────────────────────┘
```

Between **every callback** (since Node 11), Node drains:

1. the `process.nextTick` queue, then
2. the promise microtask queue

and repeats until both are empty.

| API | Queue / phase |
|-----|---------------|
| `process.nextTick(fn)` | nextTick queue, before promise microtasks |
| `Promise.then`, `queueMicrotask` | microtask queue |
| `setTimeout`, `setInterval` | timers phase |
| `fs`, `net`, `http` callbacks | poll phase |
| `setImmediate(fn)` | check phase (right after poll) |
| `socket.on("close")` | close callbacks |

```js
// Inside an I/O callback, setImmediate always beats setTimeout(0)
fs.readFile(__filename, () => {
  setTimeout(() => console.log("timeout"), 0);
  setImmediate(() => console.log("immediate"));
});
// immediate, timeout

// From the main module the order is NOT guaranteed (depends on process performance)
setTimeout(() => console.log("timeout"), 0);
setImmediate(() => console.log("immediate"));
```

## Where the thread pool fits (Node)

JavaScript callbacks always run on the main thread. libuv's **thread pool** (default 4 threads) performs some blocking work off-thread: `fs` operations, `dns.lookup`, `crypto.pbkdf2`/`scrypt`, `zlib`. Network sockets use OS-level async I/O instead.

```js
// CPU-bound JS blocks the loop; offload with worker_threads
```

## Single-threaded does not mean slow

The loop excels at **I/O-bound** workloads: while waiting, the thread serves other work. It performs badly when **CPU-bound** code occupies the stack.

```js
// blocks every request for ~1 s
app.get("/hash", (req, res) => { res.send(syncHeavyWork()); });
```

## Seeing the loop in action

| Tool | Use |
|------|-----|
| DevTools **Performance** panel | tasks, long tasks (red corners), microtasks, rendering |
| `PerformanceObserver` with `"longtask"` | detect long tasks in the browser |
| Node `perf_hooks.monitorEventLoopDelay()` | event loop lag histogram |
| `node --cpu-prof`, `--inspect` | CPU profiles |
| `console.time` around sync sections | quick checks |

## Web Workers, Node worker threads

They have **their own event loops and stacks**, communicating via `postMessage`. They provide true parallelism, but not shared mutable objects (except `SharedArrayBuffer`).

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Long synchronous work | Freezes UI or server | Chunk it, or use workers |
| Assuming timers are exact | They are minimums | Compare timestamps |
| Believing `async` moves code to another thread | Same thread | Workers for CPU work |
| Mixing up `nextTick`, microtasks, `setImmediate` | Wrong order assumptions | Learn the queues (next file) |
| Blocking the loop in Node servers (sync fs, JSON of huge data, regex backtracking) | Every request waits | Async APIs, streaming, limits |
| Ignoring event loop lag in production | Latency spikes unexplained | Monitor delay metrics |

## Key takeaways

- One thread runs a task to completion; the loop then drains microtasks and picks the next task
- Browsers add rendering opportunities between tasks; Node cycles through libuv phases
- Callbacks in Node drain `nextTick` first, then promise microtasks
- The loop is great for I/O, terrible for CPU-bound code on the main thread

**Next:** [Microtasks and Macrotasks](./02_microtasks-and-macrotasks.md)
