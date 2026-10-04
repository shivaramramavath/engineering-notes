# 17 · Concurrency and Parallelism

JavaScript runs your code on one thread, yet it handles thousands of simultaneous connections and can also use every CPU core. This chapter explains the difference between **concurrency** and **parallelism**, how to run code on other threads (Web Workers in browsers, `worker_threads` in Node), how threads share memory safely, and how to keep concurrent work under control.

## What you will learn

- The difference between concurrency (interleaving) and parallelism (simultaneous execution)
- Which workloads benefit from threads and which do not
- Browser workers: dedicated, shared, and service workers
- Node `worker_threads`: messaging, transfer, worker pools
- `SharedArrayBuffer` and `Atomics` for shared memory without data races
- Limiting concurrency: semaphores, queues, pools, and backpressure

## Contents

| # | File | Topic |
|---|------|-------|
| 01 | [Concurrency vs Parallelism](./01_concurrency-vs-parallelism.md) | Definitions, I/O-bound vs CPU-bound, choosing a tool |
| 02 | [Web Workers](./02_web-workers.md) | Browser workers, `postMessage`, transferables, module workers |
| 03 | [Worker Threads](./03_worker-threads.md) | Node `worker_threads`, `workerData`, `MessageChannel`, pools |
| 04 | [SharedArrayBuffer and Atomics](./04_sharedarraybuffer-and-atomics.md) | Shared memory, race conditions, locks, `Atomics.wait` |
| 05 | [Concurrency Control](./05_concurrency-control.md) | Semaphore, mutex, task queue, concurrency limit, backpressure |

## Prerequisites

- [Asynchronous JavaScript](../11_asynchronous-javascript/00_README.md)
- [Event Loop](../12_event-loop/00_README.md)
- [Node.js](../16_nodejs/00_README.md), especially [Child Process and Cluster](../16_nodejs/09_child-process-and-cluster.md)
- [Typed Arrays and ArrayBuffer](../09_built-in-objects/10_typed-arrays-and-arraybuffer.md)

## Quick decision guide

| Situation | Reach for |
|-----------|-----------|
| Many network or disk calls | `async`/`await` and `Promise.all` (concurrency, one thread) |
| Too many parallel calls overwhelming a service | A concurrency limit (file 05) |
| CPU-heavy work freezing the UI | Web Worker (file 02) |
| CPU-heavy work blocking a Node server | `worker_threads` (file 03) |
| Isolating untrusted or crash-prone code | Child process (see [Node chapter](../16_nodejs/09_child-process-and-cluster.md)) |
| Many threads touching the same numbers | `SharedArrayBuffer` plus `Atomics` (file 04) |

## Key takeaways

- Most JavaScript problems are I/O-bound, and `async` code already handles them concurrently
- Threads help only for CPU-bound work, and they cost startup time and message copying
- Workers share nothing by default; communication is by messages
- Shared memory is powerful and dangerous: use `Atomics`
- Always cap concurrency: unbounded parallel work is a common cause of outages

**Next:** [Concurrency vs Parallelism](./01_concurrency-vs-parallelism.md)
