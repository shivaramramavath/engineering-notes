# Node Event Loop

Node runs your JavaScript on a **single thread**, and uses **libuv** to hand slow work (network, disk, timers) to the operating system or a thread pool. The event loop picks up results and runs the callbacks. For the general model see [Event Loop](../12_event-loop/01_event-loop.md); this file covers what is specific to Node.

## The phases

Each trip around the loop is a **tick** that visits these phases in order:

```
   ┌───────────────────────────┐
┌─▶│          timers           │  setTimeout / setInterval callbacks that are due
│  └─────────────┬─────────────┘
│  ┌─────────────▼─────────────┐
│  │     pending callbacks     │  some deferred system-level I/O callbacks
│  └─────────────┬─────────────┘
│  ┌─────────────▼─────────────┐
│  │       idle, prepare       │  internal use
│  └─────────────┬─────────────┘
│  ┌─────────────▼─────────────┐
│  │           poll            │  wait for and run I/O callbacks (sockets, files)
│  └─────────────┬─────────────┘
│  ┌─────────────▼─────────────┐
│  │           check           │  setImmediate callbacks
│  └─────────────┬─────────────┘
│  ┌─────────────▼─────────────┐
└──┤      close callbacks      │  socket.on('close'), ...
   └───────────────────────────┘
```

| Phase | Runs |
|-------|------|
| **timers** | Expired `setTimeout` / `setInterval` callbacks |
| **pending callbacks** | Certain system errors (for example some TCP errors) |
| **poll** | New I/O events; blocks here waiting for I/O if nothing else is scheduled |
| **check** | `setImmediate` callbacks |
| **close callbacks** | `'close'` events such as `socket.destroy()` |

Between **every callback** (and between phases), Node drains two queues:

1. `process.nextTick` queue
2. Promise microtask queue (`.then`, `await`, `queueMicrotask`)

## nextTick, microtasks, timers, immediates

```js
setTimeout(() => console.log('timeout'), 0);
setImmediate(() => console.log('immediate'));
process.nextTick(() => console.log('nextTick'));
Promise.resolve().then(() => console.log('promise'));
queueMicrotask(() => console.log('microtask'));
console.log('sync');
```

Typical output in a CommonJS file:

```
sync
nextTick
promise
microtask
timeout      // order of these two can vary, see below
immediate
```

| API | Queue | Runs |
|-----|-------|------|
| `process.nextTick(fn)` | nextTick queue | After the current operation, before promise microtasks |
| `Promise.then`, `queueMicrotask` | Microtask queue | After nextTick queue drains |
| `setTimeout(fn, 0)` | Timers phase | Next loop iteration (minimum ~1 ms) |
| `setImmediate(fn)` | Check phase | After the poll phase of the current iteration |

In an **ES module** the top-level code itself runs inside a promise job, so promise microtasks can run **before** `nextTick` callbacks. Do not depend on the relative order of `nextTick` and promises across module types.

### `setTimeout(0)` vs `setImmediate`

From the main module the order is **not deterministic** (it depends on process performance). Inside an I/O callback, `setImmediate` always runs first:

```js
import fs from 'node:fs';

fs.readFile(import.meta.filename, () => {
  setTimeout(() => console.log('timeout'), 0);
  setImmediate(() => console.log('immediate'));
});
// always: immediate, then timeout
```

## Starvation

`nextTick` and microtasks are drained completely before the loop moves on. A callback that keeps scheduling more of them **starves** I/O:

```js
function loop() {
  process.nextTick(loop);   // I/O callbacks never get a turn
}
loop();
```

Use `setImmediate` to yield to the loop when you split up long work.

## The thread pool

Not everything is non-blocking at the OS level. libuv keeps a **thread pool** (default **4 threads**) for work that has no async OS API:

| Uses the thread pool | Uses OS async I/O (no pool) |
|----------------------|-----------------------------|
| Most `fs` operations | TCP / UDP / HTTP sockets |
| `dns.lookup` (and `getaddrinfo`) | Pipes, TTY |
| `crypto.pbkdf2`, `scrypt`, `randomBytes` (async) | Timers |
| `zlib` (async) | |

```bash
UV_THREADPOOL_SIZE=8 node app.js   # raise the pool size (set before the process starts)
```

```js
import crypto from 'node:crypto';

// Five hashes at once: with 4 pool threads, the fifth waits
for (let i = 0; i < 5; i++) {
  crypto.pbkdf2('pw', 'salt', 1_000_000, 64, 'sha512', () => console.log('done', i));
}
```

## Blocking the loop

Your JavaScript runs on one thread. While it runs, **nothing else does**: no requests are handled, no timers fire.

```js
import http from 'node:http';

http.createServer((req, res) => {
  if (req.url === '/slow') {
    const end = Date.now() + 5000;
    while (Date.now() < end) {}          // blocks every client for 5 s
  }
  res.end('ok');
}).listen(3000);
```

Common blockers:

- `fs.readFileSync`, `execSync` and other `*Sync` calls in a server's request path
- Large `JSON.parse` / `JSON.stringify`
- CPU-heavy loops, regexes with catastrophic backtracking
- Synchronous crypto (`pbkdf2Sync`, `scryptSync`)

Fixes: use async APIs, stream large data, move CPU work to [worker threads](../17_concurrency-and-parallelism/03_worker-threads.md) or a child process, or break work into chunks with `setImmediate`.

## Measuring loop health

```js
import { monitorEventLoopDelay } from 'node:perf_hooks';

const h = monitorEventLoopDelay({ resolution: 20 });
h.enable();

setInterval(() => {
  console.log('p99 delay (ms):', h.percentile(99) / 1e6);
  h.reset();
}, 5000).unref();
```

A rising p99 means something is blocking the loop.

## Keeping the process alive (and not)

Node exits when the loop has nothing left to do. Pending timers, open sockets, and servers keep it alive.

```js
const t = setInterval(poll, 1000);
t.unref();   // this timer alone will not keep the process alive
t.ref();     // undo
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `*Sync` APIs in request handlers | Blocks all clients | Async versions |
| Recursive `process.nextTick` | Starves I/O | `setImmediate` |
| Assuming `setTimeout(0)` runs before `setImmediate` | Not guaranteed from the main module | Do not depend on it |
| Expecting more than 4 parallel `fs` / `crypto` jobs | Thread pool queues the rest | Raise `UV_THREADPOOL_SIZE` or redesign |
| CPU-heavy work on the main thread | Latency spikes | Worker threads or child processes |
| Forgetting `unref()` on background timers | Process never exits | `timer.unref()` |

## Key takeaways

- Node has one JavaScript thread plus libuv's pool and OS async I/O
- Phases: timers → pending → poll → check → close; nextTick and microtasks drain between callbacks
- `setImmediate` runs in the check phase, right after poll
- Default thread pool size is 4; it affects `fs`, `dns.lookup`, `crypto`, `zlib`
- Never block the loop in a server: async APIs, streams, or move CPU work off-thread

**Next:** [Process and Env](./03_process-and-env.md)
