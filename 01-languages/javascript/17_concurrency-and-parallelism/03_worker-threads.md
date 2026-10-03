# Worker Threads

`node:worker_threads` runs JavaScript in **separate threads inside one Node process**. Each worker has its own V8 isolate, event loop, and heap, so CPU-heavy work no longer blocks the main thread's event loop. The API mirrors Web Workers but is Node-specific.

## When to use them

| Use `worker_threads` for | Do not use them for |
|--------------------------|---------------------|
| CPU-bound work: hashing, compression, image processing, parsing big files, number crunching | I/O-bound work (Node's async I/O is already non-blocking) |
| Keeping a server responsive during heavy computation | Isolating untrusted code (use a process or sandbox) |
| Sharing memory through `SharedArrayBuffer` | Scaling a plain HTTP server across cores (use [`cluster`](../16_nodejs/09_child-process-and-cluster.md) or multiple instances) |

## A first worker (single file)

```js
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';

if (isMainThread) {
  const worker = new Worker(new URL(import.meta.url), { workerData: { n: 40 } });

  worker.on('message', (result) => console.log('fib =', result));
  worker.on('error', (err) => console.error('worker failed:', err));
  worker.on('exit', (code) => console.log('worker exited with', code));
} else {
  const fib = (n) => (n < 2 ? n : fib(n - 1) + fib(n - 2));
  parentPort.postMessage(fib(workerData.n));
}
```

Many projects use separate files, which is clearer:

```js
// main.mjs
import { Worker } from 'node:worker_threads';

const worker = new Worker(new URL('./worker.mjs', import.meta.url), {
  workerData: { n: 40 },
});
worker.on('message', (v) => console.log(v));
```

```js
// worker.mjs
import { parentPort, workerData } from 'node:worker_threads';
parentPort.postMessage(workerData.n * 2);
```

## Key pieces

| API | Where | Purpose |
|-----|-------|---------|
| `new Worker(filename \| URL, options)` | Main | Start a worker |
| `isMainThread` | Both | `true` in the main thread |
| `workerData` | Worker | Data copied from `options.workerData` at startup |
| `parentPort` | Worker | `MessagePort` to the parent |
| `worker.postMessage(value, [transfer])` | Main | Send to the worker |
| `parentPort.postMessage(value, [transfer])` | Worker | Send to the parent |
| `worker.on('message' / 'error' / 'exit' / 'online')` | Main | Events |
| `worker.terminate()` | Main | Stop the thread; returns a promise |
| `threadId` | Both | Numeric id of this thread |
| `resourceLimits` | Option | Cap heap sizes and stack |

You can also pass code as a string with `{ eval: true }`, but files are easier to debug and lint.

## Worker options

```js
new Worker('./worker.mjs', {
  workerData: { config: 'x' },                // structured-clone at startup
  argv: ['--flag'],                           // appended to process.argv in the worker
  env: { ...process.env, MODE: 'fast' },      // or SHARE_ENV to share the parent's env
  resourceLimits: {
    maxOldGenerationSizeMb: 256,
    maxYoungGenerationSizeMb: 32,
    stackSizeMb: 4,
  },
  stdout: false,                              // true: do not auto-pipe worker stdout to the parent
  trackUnmanagedFds: true,
});
```

## Messaging

Like Web Workers, messages are copied with the **structured clone algorithm** (no functions, no class methods; `Map`, `Set`, `Date`, typed arrays, and circular references are fine).

```js
// main.mjs
worker.postMessage({ type: 'resize', width: 800, buffer });
worker.on('message', (msg) => { /* ... */ });
```

```js
// worker.mjs
import { parentPort } from 'node:worker_threads';

parentPort.on('message', (msg) => {
  if (msg.type === 'resize') {
    parentPort.postMessage({ ok: true });
  }
});
```

### Transferring instead of copying

```js
const buf = new ArrayBuffer(64 * 1024 * 1024);
worker.postMessage({ buf }, [buf]);          // moves ownership; buf is now detached (length 0)
```

Transferable: `ArrayBuffer`, `MessagePort`, `FileHandle`, and a few others. Node `Buffer` objects cannot be transferred directly; transfer their underlying `ArrayBuffer`, being careful about the pooled-buffer offset (`buf.buffer`, `byteOffset`, `byteLength`).

### Sharing memory

`SharedArrayBuffer` is visible to all threads at once, with no copy and no transfer. Always combine it with `Atomics` (see [SharedArrayBuffer and Atomics](./04_sharedarraybuffer-and-atomics.md)).

```js
const shared = new SharedArrayBuffer(4);
const counter = new Int32Array(shared);
new Worker('./worker.mjs', { workerData: { shared } });     // the same memory in both threads
```

## Promise wrapper for one-off tasks

```js
import { Worker } from 'node:worker_threads';

function runWorker(file, data) {
  return new Promise((resolve, reject) => {
    const worker = new Worker(file, { workerData: data });
    worker.once('message', resolve);
    worker.once('error', reject);
    worker.once('exit', (code) => {
      if (code !== 0) reject(new Error(`worker stopped with exit code ${code}`));
    });
  });
}

const result = await runWorker(new URL('./worker.mjs', import.meta.url), { n: 38 });
```

This starts a new thread per call. Fine for rare, long jobs; wasteful for many small ones.

## A worker pool

Starting a worker costs milliseconds and several MB. Keep a fixed set and queue tasks.

```js
// pool.mjs
import { Worker } from 'node:worker_threads';
import os from 'node:os';

export class WorkerPool {
  #file;
  #idle = [];
  #queue = [];
  #all = new Set();

  constructor(file, size = Math.max(1, os.availableParallelism() - 1)) {
    this.#file = file;
    for (let i = 0; i < size; i++) this.#spawn();
  }

  #spawn() {
    const worker = new Worker(this.#file);
    this.#all.add(worker);

    worker.on('error', (err) => {
      worker.currentTask?.reject(err);
      worker.currentTask = null;
    });

    worker.on('exit', () => {
      this.#all.delete(worker);
      this.#idle = this.#idle.filter((w) => w !== worker);
      if (worker.currentTask) worker.currentTask.reject(new Error('worker exited'));
      if (!this.closed) this.#spawn();                 // replace crashed workers
    });

    this.#idle.push(worker);
    this.#next();
  }

  run(data, transfer = []) {
    return new Promise((resolve, reject) => {
      this.#queue.push({ data, transfer, resolve, reject });
      this.#next();
    });
  }

  #next() {
    while (this.#idle.length && this.#queue.length) {
      const worker = this.#idle.pop();
      const task = this.#queue.shift();
      worker.currentTask = task;

      worker.once('message', (result) => {
        worker.currentTask = null;
        this.#idle.push(worker);
        task.resolve(result);
        this.#next();
      });

      worker.postMessage(task.data, task.transfer);
    }
  }

  async close() {
    this.closed = true;
    await Promise.all([...this.#all].map((w) => w.terminate()));
  }
}
```

```js
// task-worker.mjs
import { parentPort } from 'node:worker_threads';
import crypto from 'node:crypto';

parentPort.on('message', ({ password }) => {
  const hash = crypto.scryptSync(password, 'salt', 64).toString('hex');
  parentPort.postMessage(hash);
});
```

```js
// app.mjs
import { WorkerPool } from './pool.mjs';

const pool = new WorkerPool(new URL('./task-worker.mjs', import.meta.url));
const hashes = await Promise.all(['a', 'b', 'c', 'd'].map((password) => pool.run({ password })));
await pool.close();
```

For production, consider a battle-tested pool such as **Piscina**, which adds task cancellation, resource limits, and queue management.

## MessageChannel

Create a direct channel between two threads (for example between two workers), bypassing the parent:

```js
import { MessageChannel, Worker } from 'node:worker_threads';

const { port1, port2 } = new MessageChannel();

const a = new Worker('./a.mjs', { workerData: { port: port1 }, transferList: [port1] });
const b = new Worker('./b.mjs', { workerData: { port: port2 }, transferList: [port2] });
// a and b can now talk to each other with port.postMessage / port.on('message')
```

`BroadcastChannel` offers publish/subscribe messaging among all threads that open the same name.

## Errors and exit

```js
worker.on('error', (err) => {
  // uncaught exception thrown in the worker; the worker then exits
});

worker.on('exit', (code) => {
  // 0 = clean; non-zero = failure or terminate()
});

worker.on('messageerror', (err) => {
  // a message could not be deserialized
});
```

An uncaught exception terminates **that worker only**, not the whole process. Inside the worker, `process.exit()` ends the worker thread, not the process.

```js
await worker.terminate();                        // returns a promise; stops JS execution ASAP
```

A running worker (or one with active handles) keeps the main process alive. Call `worker.unref()` if it should not.

## What differs from the main thread

- Each worker has its own globals and module cache: a module imported in two threads is evaluated twice
- Environment variables are copied (unless `SHARE_ENV`); `process.chdir()` is not available
- Some `process` methods and signal handling are unavailable or limited in workers
- Native addons must be thread-safe (context-aware) to load in workers
- `console.log` from a worker is forwarded asynchronously to the parent's stdout

## worker_threads vs child_process vs cluster

| | `worker_threads` | `child_process` | `cluster` |
|---|------------------|-----------------|-----------|
| Isolation | Shared process, separate isolate | Separate OS process | Separate OS processes |
| Startup cost | Lower | Higher | Higher |
| Memory sharing | `SharedArrayBuffer`, transferables | None (IPC only) | None (IPC only) |
| Crash impact | The worker dies (process survives unless native crash) | Contained | Contained |
| Runs other programs | No | Yes | No |
| Typical use | CPU-bound tasks in an app | External tools, scripts | Multi-core I/O servers |

## Measuring whether it helps

```js
console.time('single');
for (let i = 0; i < 8; i++) fib(35);
console.timeEnd('single');

console.time('pool');
await Promise.all(Array.from({ length: 8 }, () => pool.run({ n: 35 })));
console.timeEnd('pool');
```

If the task is short or the data is large, messaging and startup overhead can make workers slower. Always benchmark with realistic data.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Using workers for I/O-bound work | Added complexity, no speedup | `async` APIs |
| New worker per request | Startup cost, memory growth | Pool |
| Passing functions or class instances in messages | `DataCloneError` or lost prototypes | Plain data |
| Cloning huge buffers every time | Slow | `transferList` or `SharedArrayBuffer` |
| No `error` / `exit` handlers | Silent failures, hung promises | Handle both, reject pending tasks |
| Using a transferred buffer afterwards | It is detached | Treat it as moved |
| Sharing memory without `Atomics` | Data races, torn reads | `Atomics` operations |
| Creating more workers than cores for CPU tasks | Context-switch overhead | About `availableParallelism()` |
| Forgetting to close the pool | Process never exits | `close()` / `terminate()` or `unref()` |
| Expecting shared module state | Each thread has its own copy | Pass data explicitly |

## Key takeaways

- `worker_threads` gives true parallel JavaScript inside one Node process
- Use them for CPU-bound work; async I/O already scales without threads
- Communicate with `postMessage`; copy by default, transfer or share for large data
- Pool workers rather than spawning one per task, and size the pool near the core count
- Handle `error` and `exit`, and clean up with `terminate()`
- Use `SharedArrayBuffer` with `Atomics` when threads must share mutable state

**Next:** [SharedArrayBuffer and Atomics](./04_sharedarraybuffer-and-atomics.md)
