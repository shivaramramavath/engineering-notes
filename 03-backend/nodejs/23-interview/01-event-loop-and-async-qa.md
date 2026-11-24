# Node.js Technical Interview: Event Loop & Asynchronous Mechanics Q&A

> In-depth questions and answers detailing the libuv event loop phases, microtask execution ordering, thread pool concurrency, and asynchronous execution subtleties.

---

### Q1: Detail the exact execution phases of the libuv Event Loop in Node.js. In what order do they execute, and what happens in each phase?

#### In-Depth Answer
The libuv event loop processes callbacks in six distinct sequential phases. Every cycle through these phases is called a **tick**:

```
   ┌───────────────────────────┐
┌─>│          timers           │ ──> Executes callbacks scheduled by setTimeout() & setInterval()
│  └─────────────┬─────────────┘
│  ┌─────────────┴─────────────┐
│  │     pending callbacks     │ ──> Executes I/O callbacks deferred from previous loop iteration (e.g. TCP errors)
│  └─────────────┬─────────────┘
│  ┌─────────────┴─────────────┐
│  │        idle, prepare      │ ──> Internal libuv use only (synchronization, housekeeping)
│  └─────────────┬─────────────┘
│  ┌─────────────┴─────────────┐
│  │            poll           │ ──> Retrieves new I/O events; executes I/O related callbacks; blocks if empty
│  └─────────────┬─────────────┘
│  ┌─────────────┴─────────────┐
│  │            check          │ ──> Executes setImmediate() callbacks
│  └─────────────┬─────────────┘
│  ┌─────────────┴─────────────┐
│  │       close callbacks     │ ──> Executes socket/handle close callbacks (e.g. socket.on('close', ...))
└──┴───────────────────────────┘
```

1. **Timers Phase**: Executes callbacks whose threshold has expired (`setTimeout`, `setInterval`). The event loop checks a min-heap of timers against the current loop timestamp (`uv_now`).
2. **Pending Callbacks (I/O Callbacks)**: Executes system operation errors or deferred callbacks (e.g. if a TCP socket received an `ECONNREFUSED` error during a previous iteration).
3. **Idle, Prepare Phase**: Used internally by libuv for housekeeping before entering the poll phase.
4. **Poll Phase**:
   - Calculates how long it should block and wait for I/O readiness (`epoll_wait` / `kqueue`).
   - Processes events in the poll queue (incoming network packets, file reads completed by OS).
   - If the poll queue is empty:
     - If `setImmediate()` callbacks are scheduled, it leaves the poll phase and proceeds immediately to the Check phase.
     - If timers are pending, it blocks only until the earliest timer threshold is reached.
5. **Check Phase**: Specifically designed to execute callbacks registered via `setImmediate()`.
6. **Close Callbacks Phase**: Executes cleanup handlers when sockets or handles are abruptly closed (e.g., `socket.destroy()` $\rightarrow$ `socket.on('close')`).

---

### Q2: How do Microtasks fit into the Event Loop? What is the execution priority between `process.nextTick()`, `queueMicrotask()`, and `Promise.then()`?

#### In-Depth Answer
**Microtasks are NOT managed by libuv.** They are managed directly by the V8 JavaScript engine and Node.js core.
There are two distinct microtask queues in Node.js:
1. **The `nextTick` Queue**: Callbacks scheduled via `process.nextTick()`.
2. **The Other Microtask Queue**: Promises (`Promise.then`, `catch`, `finally`, `async/await` resumptions) and explicit `queueMicrotask()`.

#### The Golden Rule of Execution:
Whenever the JavaScript call stack empties—**between ANY two operations or between ANY two phases of the libuv event loop**—Node.js drains the microtask queues completely before moving on to the next task.

#### Strict Priority Order:
1. All callbacks in the **`nextTick` queue** execute first until completely exhausted.
2. All callbacks in the **Promise / `queueMicrotask` queue** execute second.
3. If a Promise callback schedules a new `process.nextTick()`, that newly scheduled `nextTick` will execute **before** subsequent macrotask phases!

```javascript
// Demonstration of execution order:
setTimeout(() => console.log('1. Macrotask (Timers Phase)'), 0);
setImmediate(() => console.log('2. Macrotask (Check Phase)'));

Promise.resolve().then(() => console.log('3. Promise Microtask'));
queueMicrotask(() => console.log('4. queueMicrotask'));
process.nextTick(() => console.log('5. nextTick Priority Microtask'));

// Output order:
// 5. nextTick Priority Microtask
// 3. Promise Microtask
// 4. queueMicrotask
// 1. Macrotask (Timers Phase)
// 2. Macrotask (Check Phase)
```

---

### Q3: Why is the execution order of `setTimeout(fn, 0)` and `setImmediate(fn)` non-deterministic at the root level, but strictly deterministic inside an I/O callback?

#### In-Depth Answer

#### Scenario A: Root Top-Level Execution (Non-Deterministic)
```javascript
setTimeout(() => console.log('timeout'), 0);
setImmediate(() => console.log('immediate'));
```
**Why it is non-deterministic:**
- In Node.js, `setTimeout(fn, 0)` is normalized internally to `setTimeout(fn, 1)` (1ms).
- When the process initializes, libuv starts the loop and records `loop->time`.
- Depending on CPU load, thread scheduling, and machine speed, the process initialization might take less than 1ms (in which case the timer has *not* expired yet, so the loop skips Timers and reaches the Check phase first $\rightarrow$ prints `immediate`, then `timeout`), or it might take $\ge 1$ms (in which case the timer has expired $\rightarrow$ prints `timeout`, then `immediate`).

#### Scenario B: Inside an I/O Callback (Strictly Deterministic)
```javascript
import fs from 'node:fs';

fs.readFile('package.json', () => {
  setTimeout(() => console.log('timeout'), 0);
  setImmediate(() => console.log('immediate'));
});
```
**Why it is ALWAYS deterministic:**
- When `fs.readFile` finishes, its callback is executed in the **Poll Phase**.
- Inside the Poll Phase callback, both `setTimeout` and `setImmediate` are scheduled.
- The next sequential phase in the event loop after Poll is **Check** (where `setImmediate` lives).
- The Timers phase cannot run until the entire loop completes and cycles back around.
- **Therefore, `setImmediate` is guaranteed to ALWAYS print first!**

---

### Q4: What is the libuv Threadpool (`UV_THREADPOOL_SIZE`)? Which operations utilize it, and how do you prevent threadpool starvation?

#### In-Depth Answer
By default, the libuv threadpool has **4 worker threads**.

#### What uses the Threadpool:
1. `fs.*` (all synchronous and asynchronous file system APIs).
2. `crypto.*` (`pbkdf2`, `scrypt`, `randomBytes`, `generateKeyPair`).
3. `zlib.*` (gzip, brotli, deflate compression).
4. `dns.lookup()` (due to blocking system call `getaddrinfo`).

#### The Problem of Starvation:
If an incoming web request executes a CPU-heavy password hash using `crypto.pbkdf2` with high iterations, one thread in the pool is tied up for 100ms+. If 4 concurrent requests arrive, **all 4 threads in the threadpool are blocked simultaneously**.
During this period, any incoming file system read (`fs.readFile`) or static asset delivery is completely frozen, even though the main JavaScript thread is completely idle!

#### Mitigation Strategies:
1. **Increase Pool Size at Startup**:
   - `UV_THREADPOOL_SIZE=64 node server.js` (Must be set in the shell environment before the process boots; setting `process.env.UV_THREADPOOL_SIZE` inside JavaScript is ignored because libuv initializes its pool on C++ boot).
2. **Offload to Worker Threads (`worker_threads`)**:
   - Keep the libuv threadpool free for I/O; run custom CPU tasks in dedicated OS worker threads.
3. **Use Non-Blocking Alternatives**:
   - Use `dns.resolve4()` instead of `dns.lookup()` (c-ares library uses async network sockets, completely bypassing the threadpool).

---

### Q5: What is Event Loop Starvation, and how can you monitor loop latency (Event Loop Lag) in production?

#### In-Depth Answer
**Event Loop Starvation** occurs when synchronous JavaScript execution or an infinite recursive microtask loop prevents the event loop from advancing to subsequent phases or processing inbound network traffic.

#### Classic Starvation Example (Recursive `process.nextTick`):
```javascript
function starve() {
  process.nextTick(starve); // Recursively fills nextTick queue; event loop never ticks!
}
starve();
setTimeout(() => console.log('Never runs!'), 10);
```

#### Monitoring Event Loop Lag:
In production, latency is measured using the Node.js `perf_hooks` built-in `monitorEventLoopDelay`:

```javascript
import { monitorEventLoopDelay } from 'node:perf_hooks';

// Sample loop delay with 10ms resolution
const histogram = monitorEventLoopDelay({ resolution: 10 });
histogram.enable();

setInterval(() => {
  const p99LagMs = histogram.percentile(99) / 1e6; // Convert nanoseconds to milliseconds
  const maxLagMs = histogram.max / 1e6;
  console.log(`[Health] Event Loop P99 Lag: ${p99LagMs.toFixed(2)}ms | Max: ${maxLagMs.toFixed(2)}ms`);

  if (p99LagMs > 100) {
    console.error('CRITICAL: Event loop lag exceeds 100ms! Healthcheck failing.');
  }
  histogram.reset();
}, 5000);
```
If P99 lag exceeds 50-100ms, the process is experiencing synchronous blocking (e.g. large `JSON.parse` of 50MB payloads, heavy regex parsing, or unindexed array iterations).
