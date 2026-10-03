# 12 · Event Loop

The **event loop** is the mechanism that lets single-threaded JavaScript handle timers, network responses, user input and promises without blocking. Understanding it explains output order puzzles, frozen UIs, laggy servers and most "why did that run first?" bugs.

```
┌───────────────────────────┐
│ call stack (runs your JS) │◄──── picks the next job when empty
└─────────────▲─────────────┘
              │
        ┌─────┴─────┐       microtask queue  (promises, queueMicrotask)
        │ event loop│◄───── macrotask queues (timers, I/O, events, messages)
        └───────────┘       rendering (browser)
```

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 1 | [Event Loop](./01_event-loop.md) | Call stack, queues, browser loop vs Node.js phases |
| 2 | [Microtasks and Macrotasks](./02_microtasks-and-macrotasks.md) | What goes where, draining rules, starvation |
| 3 | [Timers](./03_timers.md) | `setTimeout`, `setInterval`, clamping, drift, `setImmediate`, `requestAnimationFrame` |
| 4 | [Task Ordering](./04_task-ordering.md) | Predicting output, async/await ordering, worked puzzles |
| 5 | [Event Loop Patterns](./05_event-loop-patterns.md) | Yielding, chunking, batching, measuring lag, offloading work |

## The one-paragraph model

1. Run the current task on the **call stack** until it is empty
2. Run **all microtasks** (promise reactions, `queueMicrotask`), including ones queued while draining
3. In a browser, possibly **render** (animation frames, style, layout, paint)
4. Take the **next macrotask** (timer, I/O, UI event, message) and repeat

## Goal

By the end you can predict the exact order of `console.log` output across timers, promises and `async`/`await`, keep apps responsive, and find event-loop blocking problems.

## Prerequisites

- [Execution Context and Call Stack](../04_scope-and-execution/03_execution-context-and-call-stack.md)
- [Sync vs Async](../11_asynchronous-javascript/01_sync-vs-async.md) and [Promises](../11_asynchronous-javascript/03_promises.md)

**Next:** [Event Loop](./01_event-loop.md)
