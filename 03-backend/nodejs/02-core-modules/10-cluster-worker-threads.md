# `cluster` & `worker_threads`

Node's JavaScript execution is single-threaded — one process uses exactly one CPU core for running your code, no matter how many cores the machine has. These two modules are Node's built-in answers to using the rest of the machine's cores.

```
cluster         →  run multiple copies of your whole server, load-balanced across cores
worker_threads  →  run a piece of CPU-heavy work on another thread, within one process
```

---

## The problem

```js
// a single Node process, however many cores the machine has
http
  .createServer((req, res) => {
    /* ... */
  })
  .listen(3000);
```

This server only ever uses **one core**, regardless of whether the machine has 4 or 64. Node handles concurrent I/O extremely well on a single thread (thanks to its non-blocking, event-driven model), but it cannot use more than one core for actual JavaScript execution without help from one of these modules.

---

## `cluster` — multiple processes, one port

```js
import cluster from "node:cluster";
import os from "node:os";
import http from "node:http";

const numCPUs = os.cpus().length;

if (cluster.isPrimary) {
  console.log(`Primary ${process.pid} is running`);

  for (let i = 0; i < numCPUs; i++) {
    cluster.fork();
  }

  cluster.on("exit", (worker, code, signal) => {
    console.log(`Worker ${worker.process.pid} died, restarting...`);
    cluster.fork(); // replace the crashed worker
  });
} else {
  // this code runs in each worker process
  http
    .createServer((req, res) => {
      res.end(`Handled by worker ${process.pid}`);
    })
    .listen(3000);

  console.log(`Worker ${process.pid} started`);
}
```

- The **primary** process forks one **worker** process per CPU core
- Every worker runs the exact same server code, all listening on the same port
- Node's cluster module load-balances incoming connections across the workers automatically

### Why this matters

Each worker is a full, separate process with its own memory — a crash in one worker doesn't take down the others (and the `exit` handler above can automatically restart a crashed one). This is essentially what tools like **PM2** (`deployment/pm2.md`) do for you automatically, without writing the cluster logic by hand.

### The trade-off: no shared memory

Each worker has entirely separate memory — a variable in one worker isn't visible in another. Anything that needs to be shared across all workers (session data, a cache, rate-limit counters) has to live somewhere external, like Redis (`redis/redis-basics.md`), not in a plain in-memory JavaScript variable.

---

## `worker_threads` — one process, multiple threads

```js
// main.js
import { Worker } from "node:worker_threads";

const worker = new Worker("./heavy-task.js", {
  workerData: { numbers: [1, 2, 3, 4, 5] },
});

worker.on("message", (result) => {
  console.log("Result:", result);
});

worker.on("error", (err) => {
  console.error("Worker error:", err);
});
```

```js
// heavy-task.js
import { parentPort, workerData } from "node:worker_threads";

const sum = workerData.numbers.reduce((a, b) => a + b, 0);
parentPort.postMessage(sum);
```

Runs `heavy-task.js` on a separate thread, within the **same process** — lighter weight than spawning an entirely new process (`child_process`, `09-child-process.md`), and can optionally share memory directly via `SharedArrayBuffer` for advanced use cases.

### When to reach for this

Node's event loop is excellent for I/O-bound work (database queries, HTTP calls, file reads) — those don't block anything, since they're handled asynchronously outside the main thread already. `worker_threads` solves a different problem: genuinely **CPU-bound** synchronous work (image processing, complex calculations, parsing huge files) that would otherwise block the event loop and stall every other request the process is handling.

```js
// ❌ blocks the event loop — every other request waits
function heavySyncCalculation() {
  let result = 0;
  for (let i = 0; i < 10_000_000_000; i++) result += i;
  return result;
}

app.get("/compute", (req, res) => {
  res.json({ result: heavySyncCalculation() }); // freezes the whole server meanwhile
});
```

```js
// ✅ offloaded to a worker thread — the main thread stays responsive
app.get("/compute", (req, res) => {
  const worker = new Worker("./heavy-calc.js");
  worker.on("message", (result) => res.json({ result }));
});
```

---

## `cluster` vs `worker_threads`: different problems

|             | `cluster`                                              | `worker_threads`                                            |
| ----------- | ------------------------------------------------------ | ----------------------------------------------------------- |
| Solves      | Using all CPU cores to handle more concurrent requests | Preventing one CPU-heavy task from blocking everything else |
| Unit        | Separate processes                                     | Threads within one process                                  |
| Memory      | Fully isolated per worker                              | Can share memory                                            |
| Typical use | Scaling an HTTP server across cores                    | Image/video processing, heavy computation, parsing          |

They're not mutually exclusive — a clustered server, where each worker process also offloads occasional CPU-heavy tasks to its own worker threads, is a perfectly reasonable combination for a compute-heavy app.

---

## In practice: you often won't write this yourself

- **`cluster`** — most teams reach for a process manager like **PM2** (`deployment/pm2.md`), which wraps clustering behind a simple config flag, or let their container orchestrator (Kubernetes, ECS) run multiple container replicas instead — achieving the same "use all the cores" goal at the infrastructure level rather than inside the Node process itself.
- **`worker_threads`** — often used indirectly through libraries (e.g. image-processing libraries, some ORMs) rather than written by hand, though it's straightforward enough to reach for directly when you have a specific known CPU-bound bottleneck.

## Quick summary

- Node is single-threaded for JavaScript execution — I/O doesn't block it, but CPU-bound synchronous work does
- `cluster` runs multiple full processes (one per core), load-balancing connections — good for scaling overall request throughput; no shared memory between workers
- `worker_threads` runs a specific task on another thread within one process — good for offloading one CPU-heavy operation without blocking everything else
- In production, container/orchestrator-level scaling (multiple replicas) or a process manager (PM2) often replaces hand-written `cluster` code

## Next

**`11-module-system.md`** covers CommonJS vs ES Modules — the two module systems you'll encounter across different Node projects and versions.
