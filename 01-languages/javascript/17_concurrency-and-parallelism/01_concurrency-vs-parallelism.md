# Concurrency vs Parallelism

These words are often used interchangeably, but they describe different things.

| | Concurrency | Parallelism |
|---|-------------|-------------|
| Meaning | Managing **many tasks at once** by interleaving them | **Executing** many tasks at the same instant |
| Needs multiple CPU cores | No | Yes |
| JavaScript tool | `async`/`await`, promises, event loop | Workers, child processes |
| Solves | Waiting (I/O latency) | Computing (CPU throughput) |
| Analogy | One cook switching between pots while waiting for water to boil | Several cooks, each with their own stove |

Concurrency is about **structure** (dealing with many things). Parallelism is about **execution** (doing many things).

## JavaScript is concurrent by default

One thread runs your code, but the event loop lets many operations be **in flight** at the same time. While one request waits for the network, other code runs.

```js
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function task(name, ms) {
  console.log('start', name);
  await sleep(ms);             // yields: the thread is free to do other work
  console.log('end', name);
}

console.time('total');
await Promise.all([task('A', 1000), task('B', 1000), task('C', 1000)]);
console.timeEnd('total');      // ~1000 ms, not 3000 ms
```

Three tasks overlapped, yet only one thread existed. No two lines of JavaScript ever ran at the same instant. See [Event Loop](../12_event-loop/01_event-loop.md).

## I/O-bound vs CPU-bound

| | I/O-bound | CPU-bound |
|---|-----------|-----------|
| Time is spent | Waiting (network, disk, database, timers) | Computing (loops, parsing, hashing, image processing) |
| Thread is | Idle most of the time | Busy |
| Fix with | Concurrency (`Promise.all`, streams) | Parallelism (workers, processes) |
| Adding threads | Little benefit | Scales with core count |

```js
// I/O-bound: concurrency is enough
const pages = await Promise.all(urls.map((u) => fetch(u).then((r) => r.text())));

// CPU-bound: this freezes everything while it runs
function fib(n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
fib(42);   // seconds of blocked thread: no timers, no requests, no UI updates
```

A blocked thread cannot run `async` callbacks, so `async` does **not** make CPU work non-blocking. `await` only helps while something else (the OS, the network) is doing the work.

```js
async function bad() {
  return fib(42);   // still blocks the thread, despite being "async"
}
```

## Seeing the difference

```js
// Concurrent, single thread: fine for waiting
const t0 = performance.now();
await Promise.all([sleep(500), sleep(500), sleep(500)]);
console.log(performance.now() - t0);        // ~500

// CPU work on one thread: runs back to back
const t1 = performance.now();
[fib(35), fib(35), fib(35)];
console.log(performance.now() - t1);        // ~3x one call

// CPU work in parallel: needs workers (see the next files)
```

## Ways to get parallelism

| Mechanism | Where | Memory | Cost to start | Typical use |
|-----------|-------|--------|---------------|-------------|
| **Web Worker** | Browser | Isolated (messages) | Moderate | Heavy computation off the UI thread |
| **Service Worker** | Browser | Isolated | Lifecycle-managed | Network proxy, offline cache |
| **`worker_threads`** | Node | Isolated; can share `SharedArrayBuffer` | Moderate (own V8 isolate) | CPU tasks in a server |
| **`child_process`** | Node | Fully separate process | Higher | Other programs, isolation |
| **`cluster`** | Node | Separate processes | Higher | Scale an I/O-bound server across cores |
| **GPU / WebAssembly** | Browser / Node | Varies | Varies | Numeric workloads (WASM threads use workers underneath) |

Each worker has its **own** event loop, heap, and globals. There are no shared variables between threads, which removes most data races but means data must be **copied or transferred**.

## Costs of parallelism

- **Startup**: creating a worker takes milliseconds and tens of MB of memory. Reuse workers via a pool.
- **Messaging**: data is copied with the structured clone algorithm unless transferred or shared.
- **Complexity**: debugging, error handling, and lifecycle (terminate, restart).
- **Diminishing returns**: speedup is limited by the serial part of the work (Amdahl's law) and by core count.

Rule of thumb: parallelize only when a task takes long enough (tens of milliseconds or more) that the overhead is small compared with the work.

## Concurrency hazards

Even without threads, concurrency creates bugs:

```js
let balance = 100;

async function withdraw(amount) {
  const current = balance;               // read
  await checkFraud();                    // yield: other code runs here
  balance = current - amount;            // write using stale data
}

await Promise.all([withdraw(80), withdraw(80)]);
console.log(balance);   // 20, not -60: both read 100 (a lost update)
```

This is a **race condition** between interleaved async functions, with a single thread. Fixes: avoid read-modify-write across an `await`, or serialize access with a mutex or queue (see [Concurrency Control](./05_concurrency-control.md)).

With real threads and shared memory, races can occur at any instruction; see [SharedArrayBuffer and Atomics](./04_sharedarraybuffer-and-atomics.md).

## Splitting long tasks without threads

If you cannot use a worker, yield to the event loop between chunks so the thread stays responsive:

```js
async function processLarge(items, handle, chunkSize = 1000) {
  for (let i = 0; i < items.length; i += chunkSize) {
    for (const item of items.slice(i, i + chunkSize)) handle(item);
    await new Promise((r) => setTimeout(r, 0));     // yield to the event loop (browser or Node)
  }
}
```

Alternatives: `scheduler.yield()` where available in browsers, `setImmediate` in Node, `requestIdleCallback` for low-priority work. The total time is not shorter; the page just stays responsive.

## Choosing a tool

```
Is the task mostly waiting (network, disk, DB)?
├─ Yes → async/await + Promise.all, with a concurrency limit
└─ No (CPU work)
   ├─ Short (a few ms)?           → just run it
   ├─ Long, in a browser?         → Web Worker
   ├─ Long, in Node?              → worker_threads (pool for many tasks)
   ├─ Needs another program?      → child_process
   └─ Need to use all cores for a server? → cluster, or several containers
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Using `async` for CPU work and expecting it not to block | `async` does not move work to another thread | Workers |
| Spawning a worker per tiny task | Startup overhead dominates | Pool and batch |
| Unbounded `Promise.all` over thousands of items | Floods APIs, sockets, memory | Limit concurrency |
| Read-modify-write across an `await` | Lost updates | Serialize, or update atomically |
| Assuming parallel always means faster | Overhead, serial portions, memory bandwidth | Measure |
| Sharing mutable objects between "threads" | Not possible: they are copied | Messages, or `SharedArrayBuffer` |
| Confusing `cluster` with threads | They are separate processes | Pick based on isolation and sharing needs |

## Key takeaways

- Concurrency = interleaving tasks; parallelism = running them simultaneously
- JavaScript is concurrent on one thread; parallelism requires workers or processes
- I/O-bound work needs concurrency; CPU-bound work needs parallelism
- `async`/`await` never makes synchronous computation non-blocking
- Async code can still have race conditions
- Measure before parallelizing: startup and messaging costs are real

**Next:** [Web Workers](./02_web-workers.md)
