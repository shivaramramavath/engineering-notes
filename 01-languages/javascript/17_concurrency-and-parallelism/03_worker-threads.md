# Worker Threads in Node.js

> Multi-threaded CPU execution in Node.js, V8 isolate lifecycles, MessageChannel synchronization, and production-grade worker pools.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Concurrency vs Parallelism](file:///e:/Learning/engineering-notes/01-languages/javascript/17_concurrency-and-parallelism/01_concurrency-vs-parallelism.md), [Node Runtime](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs/01_node-runtime.md) |
| **Time to read** | 25 minutes |
| **Status** | Complete |
| **Standard / Runtime** | Node.js v12+ (Stable v18 LTS - v24+) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Historically, when a Node.js server needed to perform computationally intensive calculations—such as image compression, password hashing (`bcrypt`), syntax highlighting, or machine learning inference—developers had to spawn separate operating system processes using `child_process.fork()`. While process isolation is robust, each child process consumes substantial system memory (30–60 MB) and introduces noticeable process spawn latency.

The `node:worker_threads` module introduces true multi-threading inside a single Node.js process. Each worker thread runs its own V8 isolate and event loop while sharing the same system process ID and memory space. This drastically reduces RAM overhead (~1–2 MB per thread) and enables true zero-copy shared memory concurrency via `SharedArrayBuffer` and `Atomics`.

---

## 2. Core Concepts

### Worker Threads Architecture
Unlike POSIX threads in C/C++ or threads in Java, Node.js worker threads do **not** share arbitrary JavaScript object references. Memory safety is strictly preserved:
- Each worker possesses an isolated **V8 Isolate**.
- Each worker runs its own **libuv Event Loop**.
- Communication takes place over bidirectional `MessagePort` channels using the Structured Clone Algorithm or Transferable Objects.
- Raw binary memory can be shared across isolates using `SharedArrayBuffer`.

### Core Exports of `node:worker_threads`
1. `Worker`: Represents the worker thread from the parent side.
2. `parentPort`: An instance of `MessagePort` accessible inside the worker to talk to the parent.
3. `isMainThread`: Boolean indicating whether the script is running in the main process thread or inside a worker.
4. `workerData`: Arbitrary cloneable data passed synchronously to the worker at construction time.
5. `MessageChannel`: A pair of connected `MessagePort` instances (`port1` and `port2`).

---

## 3. How It Works Internally

```
+--------------------------------------------------------------------------+
| NODE.JS PROCESS (PID 4000)                                               |
|                                                                          |
|  +---------------------------+          +-----------------------------+  |
|  | MAIN THREAD (Isolate 0)   |          | WORKER THREAD 1 (Isolate 1) |  |
|  | - Main V8 Heap            |          | - Worker V8 Heap            |  |
|  | - Main Libuv Event Loop   |          | - Dedicated Event Loop      |  |
|  | - workerData Initializer  |          | - parentPort MessagePort    |  |
|  +---------------------------+          +-----------------------------+  |
|                |                                       ^                 |
|                |  uv_async_t / MessageChannel Queue    |                 |
|                +---------------------------------------+                 |
|                                                                          |
|  +--------------------------------------------------------------------+  |
|  | SHARED PROCESS MEMORY                                              |  |
|  | - SharedArrayBuffer (Raw memory accessed by both threads)          |  |
|  | - Atomics (Lock-free synchronization & thread sleeping)             |  |
|  +--------------------------------------------------------------------+  |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Instantiating a Worker Thread
```javascript
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';

if (isMainThread) {
  const worker = new Worker(new URL(import.meta.url), {
    workerData: { initialValue: 42 },
  });

  worker.on('message', (result) => console.log('From worker:', result));
  worker.on('error', (err) => console.error('Worker failed:', err));
  worker.on('exit', (code) => console.log(`Worker exited with code ${code}`));

  worker.postMessage({ command: 'EXECUTE' });
} else {
  // Inside worker thread
  console.log('Worker initialized with:', workerData.initialValue);

  parentPort.on('message', (msg) => {
    if (msg.command === 'EXECUTE') {
      parentPort.postMessage({ status: 'DONE', result: workerData.initialValue * 2 });
    }
  });
}
```

### Key Worker Thread Methods
- `worker.postMessage(value[, transferList])`: Posts a message to the worker.
- `worker.terminate()`: Forcefully terminates the worker thread immediately. Returns a promise resolving to the exit code.
- `worker.getHeapSnapshot()`: Returns a readable stream of a V8 heap snapshot taken from the worker thread.
- `worker.threadId`: Unique integer identifier for the thread inside the current process.

---

## 5. Practical Examples

### Example 1: Offloading CPU-Bound Cryptographic Work
```javascript
// crypto-worker.js
import { parentPort, workerData } from 'node:worker_threads';
import crypto from 'node:crypto';

const { password, salt, iterations, keylen, digest } = workerData;

try {
  const derivedKey = crypto.pbkdf2Sync(password, salt, iterations, keylen, digest);
  parentPort.postMessage({ success: true, key: derivedKey.toString('hex') });
} catch (error) {
  parentPort.postMessage({ success: false, error: error.message });
}

// main.js
import { Worker } from 'node:worker_threads';

function hashPasswordParallel(password, salt) {
  return new Promise((resolve, reject) => {
    const worker = new Worker(new URL('./crypto-worker.js', import.meta.url), {
      workerData: {
        password,
        salt,
        iterations: 200_000,
        keylen: 64,
        digest: 'sha512',
      },
    });

    worker.on('message', (msg) => {
      if (msg.success) resolve(msg.key);
      else reject(new Error(msg.error));
    });

    worker.on('error', reject);
    worker.on('exit', (code) => {
      if (code !== 0) reject(new Error(`Worker stopped with exit code ${code}`));
    });
  });
}
```

### Example 2: Implementing a Fixed-Size Worker Pool
```javascript
import { Worker } from 'node:worker_threads';
import os from 'node:os';

export class WorkerPool {
  constructor(workerPath, poolSize = os.availableParallelism()) {
    this.workerPath = workerPath;
    this.poolSize = poolSize;
    this.workers = [];
    this.freeWorkers = [];
    this.taskQueue = [];

    this._initialize();
  }

  _initialize() {
    for (let i = 0; i < this.poolSize; i++) {
      const worker = new Worker(this.workerPath);
      this.workers.push(worker);
      this.freeWorkers.push(worker);
    }
  }

  runTask(taskData) {
    return new Promise((resolve, reject) => {
      this.taskQueue.push({ taskData, resolve, reject });
      this._dispatch();
    });
  }

  _dispatch() {
    if (this.freeWorkers.length === 0 || this.taskQueue.length === 0) {
      return;
    }

    const worker = this.freeWorkers.pop();
    const task = this.taskQueue.shift();

    const onMessage = (result) => {
      cleanup();
      task.resolve(result);
    };

    const onError = (error) => {
      cleanup();
      task.reject(error);
    };

    const cleanup = () => {
      worker.off('message', onMessage);
      worker.off('error', onError);
      this.freeWorkers.push(worker);
      this._dispatch(); // Process next queued task
    };

    worker.once('message', onMessage);
    worker.once('error', onError);
    worker.postMessage(task.taskData);
  }

  async destroy() {
    await Promise.all(this.workers.map((w) => w.terminate()));
    this.workers = [];
    this.freeWorkers = [];
  }
}
```

### Example 3: Sharing Memory via `SharedArrayBuffer`
```javascript
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';

if (isMainThread) {
  // Allocate 4 bytes of shared process memory
  const sharedBuffer = new SharedArrayBuffer(4);
  const sharedView = new Int32Array(sharedBuffer);
  sharedView[0] = 100;

  const worker = new Worker(new URL(import.meta.url), {
    workerData: { buffer: sharedBuffer },
  });

  worker.on('message', () => {
    console.log('Value modified by worker thread:', sharedView[0]); // Logs 200
  });
} else {
  const sharedView = new Int32Array(workerData.buffer);
  // Safely modify shared memory
  Atomics.add(sharedView, 0, 100);
  parentPort.postMessage('MODIFIED');
}
```

### Example 4: Transferring a MessageChannel Port Between Workers
```javascript
import { Worker, MessageChannel } from 'node:worker_threads';

const worker1 = new Worker('./worker-producer.js');
const worker2 = new Worker('./worker-consumer.js');

const { port1, port2 } = new MessageChannel();

// Transfer port1 to worker1 and port2 to worker2
worker1.postMessage({ type: 'PORT', port: port1 }, [port1]);
worker2.postMessage({ type: 'PORT', port: port2 }, [port2]);
// Workers now communicate directly without routing through the main thread!
```

### Example 5: Capturing Heap Snapshots of Running Workers
```javascript
import { Worker } from 'node:worker_threads';
import fs from 'node:fs';

const worker = new Worker('./long-running-worker.js');

// Inspect worker heap without stopping execution
async function dumpWorkerMemory(outputPath) {
  const snapshotStream = await worker.getHeapSnapshot();
  const fileStream = fs.createWriteStream(outputPath);
  snapshotStream.pipe(fileStream);

  fileStream.on('finish', () => {
    console.log(`Worker snapshot saved to ${outputPath}`);
  });
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Deadlock on Unhandled Promise Rejections**: If a worker thread encounters an uncaught exception or rejected promise without an `'error'` handler on the parent `Worker` instance, the worker terminates abruptly and the parent's awaiting promise hangs forever.
2. **Transferring Already-Detached Buffers**: Attempting to transfer an `ArrayBuffer` twice or referencing it after transfer throws `TypeError: Cannot perform operation on a detached ArrayBuffer`.
3. **CWD and Process Context Differences**: Inside a worker thread, modifying `process.env` alters environment variables for the *entire* Node.js process (all threads). However, calling `process.chdir()` is strictly disallowed inside workers and throws an error.
4. **Native C++ Addons**: Not all native addons (`.node` files) are thread-safe. Addons built without Context-Aware Native Addon support (`N-API` or `Node-API`) can crash the process when loaded inside multiple workers.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Creating a new `Worker` per incoming HTTP request | Spawning threads incurs 10–20ms latency and high RAM churn | Maintain a pre-warmed `WorkerPool` |
| Calling `process.exit()` inside a worker | Terminates the entire Node.js server process | Use `process.exitCode = ...` or `worker.terminate()` |
| Passing functions in `postMessage()` | Functions cannot be cloned across isolates | Send messages or action descriptors |
| Using Worker Threads for I/O-bound requests | Unnecessary thread overhead for simple async tasks | Rely on the standard event loop and promises |
| Over-allocating worker threads ($> 2 \times \text{CPUs}$) | Context-switch thrashing degrades total throughput | Set max workers to `os.availableParallelism()` |
| Forgetting `worker.unref()` on persistent workers | Prevents the Node.js process from exiting cleanly | Call `worker.unref()` if worker is optional |

---

## 8. Performance Characteristics

- **Memory Overhead**: Each `Worker` instance consumes approximately 1–3 MB of resident memory (compared to ~35 MB for `child_process.fork()`).
- **Startup Latency**: Spawning a worker thread takes 5–15 milliseconds.
- **Message Transfer Speed**: Structured cloning of small objects takes $< 0.1$ms. Zero-copy `ArrayBuffer` transfer takes $< 0.01$ms regardless of size (up to multiple gigabytes).

---

## 9. Engine Internals

1. **Isolate Instantiation**:
   - `Worker` calls Node.js C++ layer `Worker::StartThread()`.
   - Node spawns a native thread via `uv_thread_create()`.
   - A new `v8::Isolate` and `v8::Context` are created on that thread.
2. **Cross-Thread Event Notification**:
   - When `postMessage` is called, the serialized message is queued in the target thread's thread-safe message queue.
   - libuv signals the target thread's event loop using an internal `uv_async_t` handle, waking up its poll phase.

---

## 10. Best Practices & Design Guidelines

- **Limit Pool Size**: Bound your worker pool to `os.availableParallelism()`.
- **Prefer Worker Threads for Pure CPU Work**: Image manipulation, cryptography, compression, parsing, and heavy math calculations.
- **Always Handle Both Error and Exit Events**: A worker can exit with code 0 (clean) or non-zero (crash). Your pool must recover and recreate failed workers.

---

## 11. Real-World Case Study

### High-Throughput PDF Rendering Worker Pool
```javascript
import http from 'node:http';
import { WorkerPool } from './WorkerPool.js';

const pdfPool = new WorkerPool('./pdf-generator-worker.js', 4);

const server = http.createServer(async (req, res) => {
  if (req.method === 'POST' && req.url === '/generate-pdf') {
    let body = '';
    req.on('data', chunk => body += chunk);
    req.on('end', async () => {
      try {
        const invoiceData = JSON.parse(body);
        // Offload PDF byte generation to worker thread pool
        const pdfBuffer = await pdfPool.runTask(invoiceData);

        res.writeHead(200, {
          'Content-Type': 'application/pdf',
          'Content-Length': pdfBuffer.byteLength,
        });
        res.end(pdfBuffer);
      } catch (err) {
        res.writeHead(500, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: err.message }));
      }
    });
  }
});

server.listen(3000);
```

---

## 12. Interview Questions

<details>
<summary>1. How do Worker Threads differ from child processes in Node.js?</summary>
Worker threads execute within the same operating system process and address space, consuming far less RAM (~2MB vs ~35MB), spawning much faster, and allowing shared memory via <code>SharedArrayBuffer</code>. Child processes are completely separate OS processes with dedicated process IDs and separate memory maps.
</details>

<details>
<summary>2. What happens when process.exit() is called inside a Worker Thread?</summary>
Calling <code>process.exit()</code> inside a worker terminates the <b>entire</b> Node.js process, including the main thread and all other workers. To terminate only the current worker, the worker should call <code>process.exitCode = ...</code> or the parent should invoke <code>worker.terminate()</code>.
</details>

<details>
<summary>3. Can Worker Threads share JavaScript objects directly without serialization?</summary>
No. Each worker runs in an independent V8 Isolate with its own heap. Direct JavaScript object references cannot cross isolate boundaries. Shared memory is only possible through raw binary buffers like <code>SharedArrayBuffer</code> accessed via typed array views.
</details>

<details>
<summary>4. What is workerData and how does it differ from postMessage?</summary>
<code>workerData</code> is passed synchronously to the worker at construction time before its script begins executing. <code>postMessage()</code> sends asynchronous messages through the event loop while the worker is actively running.
</details>

<details>
<summary>5. How does Node.js wake up an idle Worker Thread when a message arrives?</summary>
It uses libuv's <code>uv_async_t</code> mechanism. Calling <code>postMessage</code> pushes data into a thread-safe mutex-protected queue and triggers <code>uv_async_send()</code>, which wakes up the worker thread's event loop from its epoll/kqueue/IOCP wait state.
</details>

<details>
<summary>6. Why is process.chdir() disallowed inside Worker Threads?</summary>
In modern operating systems, the Current Working Directory (CWD) is a process-wide resource shared by all threads. Changing CWD in one worker thread would unpredictably alter file paths for every other worker and the main thread simultaneously.
</details>

<details>
<summary>7. What is the role of worker.unref()?</summary>
Calling <code>worker.unref()</code> allows the main process event loop to exit even if the worker thread is still running, provided there are no other active handles keeping the main thread alive.
</details>

<details>
<summary>8. How do you pass open network sockets to a Worker Thread?</summary>
Network sockets or server handles cannot be transferred directly to worker threads using <code>postMessage</code> in the same way <code>child_process.fork</code> does with <code>SCM_RIGHTS</code>. Worker threads are intended for CPU parallelism rather than socket distribution.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Multi-Threaded Array Sorter
Implement a function `parallelQuickSort(array)` that divides an array of 1,000,000 integers into halves, sorts each half on a separate `worker_thread`, and merges the sorted arrays on the main thread.

<details>
<summary>View Solution</summary>

```javascript
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';

if (isMainThread) {
  function sortChunk(data) {
    return new Promise((resolve, reject) => {
      const worker = new Worker(new URL(import.meta.url), { workerData: data });
      worker.on('message', resolve);
      worker.on('error', reject);
    });
  }

  function merge(left, right) {
    const result = [];
    let i = 0, j = 0;
    while (i < left.length && j < right.length) {
      if (left[i] < right[j]) result.push(left[i++]);
      else result.push(right[j++]);
    }
    return result.concat(left.slice(i)).concat(right.slice(j));
  }

  export async function parallelQuickSort(arr) {
    const mid = Math.floor(arr.length / 2);
    const leftChunk = arr.slice(0, mid);
    const rightChunk = arr.slice(mid);

    const [sortedLeft, sortedRight] = await Promise.all([
      sortChunk(leftChunk),
      sortChunk(rightChunk),
    ]);

    return merge(sortedLeft, sortedRight);
  }
} else {
  const data = workerData;
  data.sort((a, b) => a - b);
  parentPort.postMessage(data);
}
```
</details>

### Exercise 2: Worker Thread Ping-Pong with MessageChannel
Create two Worker Threads that exchange 1,000 incrementing counter messages back and forth directly across a dedicated `MessageChannel` without routing through the main thread.

<details>
<summary>View Solution</summary>

```javascript
import { Worker, MessageChannel } from 'node:worker_threads';

const { port1, port2 } = new MessageChannel();

const worker1 = new Worker(`
  const { parentPort } = require('node:worker_threads');
  parentPort.on('message', ({ port }) => {
    port.on('message', (count) => {
      if (count < 1000) {
        port.postMessage(count + 1);
      } else {
        console.log('Worker 1 finished ping-pong at:', count);
      }
    });
    port.postMessage(1); // Start ping
  });
`, { eval: true });

const worker2 = new Worker(`
  const { parentPort } = require('node:worker_threads');
  parentPort.on('message', ({ port }) => {
    port.on('message', (count) => {
      if (count < 1000) {
        port.postMessage(count + 1);
      }
    });
  });
`, { eval: true });

worker1.postMessage({ port: port1 }, [port1]);
worker2.postMessage({ port: port2 }, [port2]);
```
</details>
