# Concurrency Patterns and Worker Threads in TypeScript

> Architecting concurrent systems in TypeScript: worker threads, message passing with structured clone, typed event buses, thread pools, and shared memory with `SharedArrayBuffer` and `Atomics`.

| Metadata | Details |
| :--- | :--- |
| **Category** | Async & Iteration |
| **Difficulty** | Advanced |
| **Prerequisites** | [12-async-and-iteration/00-event-loop.md](./00-event-loop.md), [12-async-and-iteration/01-promises.md](./01-promises.md) |
| **TypeScript Baseline** | TS 4.0+, Node.js 18+ (`worker_threads`) |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why
While JavaScript uses an asynchronous event loop for non-blocking I/O, CPU-intensive operations (cryptography, image compression, large JSON parsing, machine learning inference) will block the main thread, freezing the event loop and starving I/O.

To achieve true parallelism, Node.js provides the `worker_threads` module (and browsers provide Web Workers). In TypeScript, typing worker message passing (`postMessage`), managing thread lifecycles, and avoiding race conditions requires strict discriminated union protocols and typed worker pools.

---

## 2. Core Concepts
- **Worker Threads vs Processes**: Worker threads run in the same OS process and share memory via `SharedArrayBuffer`, but each has its own isolated V8 engine instance, heap, and event loop.
- **Message Passing Protocol**: Data sent via `worker.postMessage()` is cloned using the **HTML Structured Clone Algorithm** (functions and symbols cannot be cloned).
- **Discriminated Message Protocols**: Modeling worker communication using typed request/response messages with correlation IDs.
- **Worker Pools**: Managing a fixed pool of $N$ worker threads to avoid the heavy memory cost of repeatedly spawning and destroying threads.
- **Shared Memory with `Atomics`**: Direct zero-copy communication across threads using `SharedArrayBuffer` with atomic read/write synchronization.

---

## 3. How It Works Internally

```
+-------------------------------------------------------------+
|                 Main Thread (Event Loop 1)                  |
|     - HTTP Server / User Interactions                       |
|     - Dispatches CPU task to WorkerPool                     |
+------------------------------+------------------------------+
                               |
                   postMessage(TaskPayload)
                   (Structured Clone serialization)
                               |
                               v
+-------------------------------------------------------------+
|                 Worker Thread (Event Loop 2)                |
|     - Isolated V8 Heap & Call Stack                         |
|     - Executes heavy CPU loop without blocking Main         |
|     - postMessage(ResultPayload)                            |
+------------------------------+------------------------------+
                               |
                               v
               Main Thread receives MessageEvent
               Resolves pending Promise for caller
```

---

## 4. Syntax & API Reference

```typescript
// 1. Strictly Typed Message Protocol
export type WorkerRequest =
  | { id: string; type: 'HASH_PASSWORD'; password: string }
  | { id: string; type: 'PARSE_CSV'; data: string };

export type WorkerResponse =
  | { id: string; type: 'SUCCESS'; result: unknown }
  | { id: string; type: 'ERROR'; error: string };

// 2. Main Thread Dispatcher Type
export interface TypedWorkerBridge<Req, Res> {
  post(request: Req): Promise<Res>;
  terminate(): Promise<number>;
}
```

---

## 5. Practical Examples

### Example 1: Basic Typed Worker Message Protocol
```typescript
type ComputeTask = { id: string; num: number };
type ComputeResult = { id: string; prime: boolean };

// Main thread typed dispatcher
export class SimpleWorkerClient {
  private pending = new Map<string, (res: boolean) => void>();

  constructor(private readonly worker: { postMessage: (msg: ComputeTask) => void; on: (event: string, cb: (res: ComputeResult) => void) => void }) {
    this.worker.on('message', (res: ComputeResult) => {
      const resolver = this.pending.get(res.id);
      if (resolver) {
        resolver(res.prime);
        this.pending.delete(res.id);
      }
    });
  }

  checkPrime(num: number): Promise<boolean> {
    const id = Math.random().toString(36).substring(2);
    return new Promise<boolean>(resolve => {
      this.pending.set(id, resolve);
      this.worker.postMessage({ id, num });
    });
  }
}
```

### Example 2: Thread Pool with Worker Recycling
```typescript
export class WorkerPool<Req, Res extends { id: string }> {
  private workers: Array<{ worker: any; busy: boolean }> = [];
  private taskQueue: Array<{ req: Req; resolve: (val: Res) => void }> = [];

  constructor(private size: number, private workerFactory: () => any) {
    for (let i = 0; i < size; i++) {
      const worker = this.workerFactory();
      const entry = { worker, busy: false };
      worker.on('message', (res: Res) => {
        entry.busy = false;
        this.processNext();
      });
      this.workers.push(entry);
    }
  }

  dispatch(req: Req): Promise<Res> {
    return new Promise(resolve => {
      this.taskQueue.push({ req, resolve });
      this.processNext();
    });
  }

  private processNext(): void {
    const idleWorker = this.workers.find(w => !w.busy);
    if (!idleWorker || this.taskQueue.length === 0) return;

    const task = this.taskQueue.shift()!;
    idleWorker.busy = true;

    const onMessage = (res: Res) => {
      idleWorker.worker.off('message', onMessage);
      task.resolve(res);
    };
    idleWorker.worker.on('message', onMessage);
    idleWorker.worker.postMessage(task.req);
  }
}
```

### Example 3: Edge Case - Transferring ArrayBuffers (Zero-Copy)
```typescript
function dispatchTransferable(worker: any, data: Uint8Array): void {
  // Transfer buffer ownership: buffer is detached from main thread immediately
  worker.postMessage({ buffer: data.buffer }, [data.buffer]);
  console.log(data.byteLength); // 0 (detached!)
}
```

### Example 4: Compiler Failure Case - Non-Cloneable Values in postMessage
```typescript
interface SerializableMsg {
  type: string;
  callback?: () => void;
}

// In standard Web Workers / Node worker_threads, functions cannot be cloned
function sendTask(worker: any, msg: SerializableMsg) {
  // @ts-expect-error TypeScript does not natively forbid function properties in structural postMessage without a custom brand/filter
  worker.postMessage(msg);
}
```

### Example 5: Idiomatic SharedArrayBuffer Mutex with Atomics
```typescript
export class SimpleSpinLock {
  private lockState: Int32Array;

  constructor(sharedBuffer: SharedArrayBuffer, index = 0) {
    this.lockState = new Int32Array(sharedBuffer, index * 4, 1);
  }

  acquire(): void {
    while (Atomics.compareExchange(this.lockState, 0, 0, 1) !== 0) {
      // Spin wait
    }
  }

  release(): void {
    Atomics.store(this.lockState, 0, 0);
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Detached Buffers**: When passing an `ArrayBuffer` in the transfer list of `postMessage`, the buffer becomes detached in the sender thread; its `byteLength` becomes 0 and any attempt to read or write throws a `TypeError`.
2. **Heavy Worker Startup**: Starting a new Node.js worker thread takes ~20–50ms and consumes ~20MB of RAM. Spawning a worker per request degrades performance; always use a pool.
3. **Structured Clone Limits**: Functions, DOM elements, and Error objects with custom non-enumerable properties do not clone cleanly across thread boundaries.
4. **Cross-Origin Isolation for `SharedArrayBuffer`**: In browsers, `SharedArrayBuffer` is disabled by default due to Spectre vulnerabilities unless `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp` headers are set.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Spawning new worker per incoming HTTP request | Massive memory leak and latency spike | Use a persistent `WorkerPool` |
| Attempting to pass closures/callbacks to worker | `DataCloneError` at runtime | Pass serializable data and message action tags |
| Using `SharedArrayBuffer` without `Atomics` | Data races, memory corruption | Use `Atomics.load`, `Atomics.store`, `compareExchange` |
| Forgetting to call `worker.terminate()` | Dangling worker processes prevent exit | Terminate pool on application shutdown |
| CPU-heavy computation on main thread | Unresponsive API, dropped WebSocket connections | Offload CPU work to worker threads |

---

## 8. Performance & Type-Checker Complexity
- Worker threads allow scaling CPU work linearly with CPU core count ($N$ cores = $N\times$ throughput).
- Using `postMessage` with large objects involves serialization overhead ($\sim 1\text{ms}$ per megabyte). For high-frequency large datasets, use `ArrayBuffer` transfer lists (0ms zero-copy transfer).

---

## 9. Compiler Internals
In TypeScript, `@types/node` declares `Worker` inside `node:worker_threads`:
```typescript
class Worker extends EventEmitter {
  postMessage(value: any, transferList?: readonly TransferListItem[]): void;
}
```
Because the standard declaration types `value` as `any`, engineers must wrap it with custom generic interfaces to enforce strict message typing.

---

## 10. Best Practices & Design Guidelines
- Size your worker pool to `os.availableParallelism() - 1` to leave one CPU core free for the main event loop.
- Define communication protocols using strict discriminated unions with unique action types.
- Always attach request IDs to match responses asynchronously.
- Leverage `transferList` for binary payloads to eliminate cloning memory copies.
- Implement graceful pool shutdown hooks on `SIGTERM` / `SIGINT`.

---

## 11. Real-World Case Study
A PDF invoice generation endpoint in a Node.js microservice took 800ms of CPU time per document. When 10 customers generated invoices simultaneously, the entire API froze, causing health check probes to fail and Kubernetes to restart the pod in a crash loop.

**Refactored with Worker Thread Pool:**
The service allocated a pool of 4 worker threads. Invoice rendering was dispatched to the worker pool. The main thread's event loop remained responsive at $<5\text{ms}$ latency, health checks stayed green, and throughput scaled linearly across all CPU cores.

---

## 12. Interview Questions

<details>
<summary>1. How do Worker Threads in Node.js differ from Web Workers in browsers?</summary>

Both use message passing and structured cloning, but Node.js `worker_threads` runs inside the server environment with access to Node's internal C++ bindings and `SharedArrayBuffer`, while Web Workers run in a sandboxed browser context without DOM access.
</details>

<details>
<summary>2. What is the Structured Clone Algorithm?</summary>

It is the standard algorithm used by JavaScript to serialize and clone complex object graphs across realms. It supports circular references, Sets, Maps, Dates, and RegExps, but fails on functions, prototypes, and symbols.
</details>

<details>
<summary>3. What happens when an `ArrayBuffer` is included in a worker's `transferList`?</summary>

Ownership of the underlying memory buffer is transferred directly to the target thread with zero copying. The original thread's `ArrayBuffer` is immediately detached, resetting its byte length to 0.
</details>

<details>
<summary>4. Why should you avoid spawning a new worker thread per request?</summary>

Because creating a worker thread requires allocating a new V8 isolate and initializing its runtime environment, which incurs significant CPU latency (~30ms) and memory footprint (~20MB). A worker pool amortizes this cost over the process lifetime.
</details>

<details>
<summary>5. What role does `Atomics` play when working with `SharedArrayBuffer`?</summary>

`Atomics` provides synchronized, atomic read, write, and compare-and-swap operations on typed arrays backed by shared memory, preventing race conditions without traditional blocking locks.
</details>

<details>
<summary>6. How can you correlate request messages with response messages in a worker?</summary>

By including a unique `id` (correlation ID) in every request message and ensuring the worker echoes back the exact same `id` in its response message.
</details>

<details>
<summary>7. What happens if an uncaught exception is thrown inside a worker thread?</summary>

The worker emits an `'error'` event and terminates execution unless an `uncaughtException` handler is registered inside the worker.
</details>

<details>
<summary>8. How do you detect the optimal number of worker threads to spawn in Node.js?</summary>

Using `os.availableParallelism()` (introduced in Node 18.14+ / Node 19), which returns the number of logical CPU cores available to the current process.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Typed Message Creator**: Create a helper function `createWorkerTask<T>(type: string, payload: T): { id: string; type: string; payload: T }` that automatically attaches a UUID or random ID.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function createWorkerTask<T>(type: string, payload: T): { id: string; type: string; payload: T } {
     return {
       id: Math.random().toString(36).substring(2),
       type,
       payload,
     };
   }
   ```
   </details>

2. **Check Transferable Support**: Write a type guard `isTransferable(x: unknown): x is ArrayBuffer` that checks if an object can be transferred.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function isTransferable(x: unknown): x is ArrayBuffer {
     return x instanceof ArrayBuffer;
   }
   ```
   </details>

### Medium
3. **Promise-Based Worker Request Client**: Implement a class `TypedWorkerClient<Req, Res extends { id: string }>` that wraps a worker and exposes a clean `request(payload: Req): Promise<Res>` method.
   <details>
   <summary>Solution</summary>

   ```typescript
   export class TypedWorkerClient<Req extends { id: string }, Res extends { id: string }> {
     private pending = new Map<string, (res: Res) => void>();

     constructor(private worker: { postMessage: (msg: Req) => void; on: (e: string, cb: (res: Res) => void) => void }) {
       this.worker.on('message', (msg: Res) => {
         const resolver = this.pending.get(msg.id);
         if (resolver) {
           resolver(msg);
           this.pending.delete(msg.id);
         }
       });
     }

     request(req: Req): Promise<Res> {
       return new Promise<Res>(resolve => {
         this.pending.set(req.id, resolve);
         this.worker.postMessage(req);
       });
     }
   }
   ```
   </details>

4. **Task Timeout for Workers**: Extend the worker client to reject with a timeout error if a worker does not respond within $M$ milliseconds.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function requestWithTimeout<Req extends { id: string }, Res extends { id: string }>(
     client: { request: (req: Req) => Promise<Res> },
     req: Req,
     timeoutMs: number
   ): Promise<Res> {
     let timer: ReturnType<typeof setTimeout>;
     const timeout = new Promise<never>((_, reject) => {
       timer = setTimeout(() => reject(new Error(`Worker request timed out after ${timeoutMs}ms`)), timeoutMs);
     });
     return Promise.race([client.request(req), timeout]).finally(() => clearTimeout(timer));
   }
   ```
   </details>

### Hard
5. **Typed Zero-Copy Shared Ring Buffer**: Implement a fixed-capacity ring buffer over a `SharedArrayBuffer` that supports atomic thread-safe single-producer single-consumer message exchange.
   <details>
   <summary>Solution</summary>

   ```typescript
   export class SharedRingBuffer {
     private head: Int32Array;
     private tail: Int32Array;
     private data: Int32Array;

     constructor(buffer: SharedArrayBuffer, private capacity = 1024) {
       this.head = new Int32Array(buffer, 0, 1);
       this.tail = new Int32Array(buffer, 4, 1);
       this.data = new Int32Array(buffer, 8, capacity);
     }

     write(value: number): boolean {
       const currentTail = Atomics.load(this.tail, 0);
       const currentHead = Atomics.load(this.head, 0);
       if ((currentTail + 1) % this.capacity === currentHead) {
         return false; // Full
       }
       this.data[currentTail] = value;
       Atomics.store(this.tail, 0, (currentTail + 1) % this.capacity);
       return true;
     }

     read(): number | null {
       const currentHead = Atomics.load(this.head, 0);
       const currentTail = Atomics.load(this.tail, 0);
       if (currentHead === currentTail) {
         return null; // Empty
       }
       const val = this.data[currentHead];
       Atomics.store(this.head, 0, (currentHead + 1) % this.capacity);
       return val;
     }
   }
   ```
   </details>

6. **Worker Pool with Dynamic Autoscaling**: Build a worker pool class that maintains a minimum of 2 workers, dynamically scales up to 8 workers during queue backlog, and terminates idle workers after 30 seconds of inactivity.
   <details>
   <summary>Solution</summary>

   ```typescript
   export class DynamicWorkerPool<Req extends { id: string }, Res extends { id: string }> {
     private workers: Array<{ worker: any; busy: boolean; lastUsed: number }> = [];

     constructor(
       private minWorkers = 2,
       private maxWorkers = 8,
       private workerFactory: () => any
     ) {
       for (let i = 0; i < minWorkers; i++) {
         this.spawnWorker();
       }
     }

     private spawnWorker() {
       const worker = this.workerFactory();
       const entry = { worker, busy: false, lastUsed: Date.now() };
       this.workers.push(entry);
       return entry;
     }

     dispatch(req: Req): Promise<Res> {
       let idle = this.workers.find(w => !w.busy);
       if (!idle && this.workers.length < this.maxWorkers) {
         idle = this.spawnWorker();
       }

       if (!idle) {
         return Promise.reject(new Error('All workers busy and pool capacity reached'));
       }

       idle.busy = true;
       idle.lastUsed = Date.now();

       return new Promise(resolve => {
         const onMsg = (res: Res) => {
           if (res.id === req.id) {
             idle!.worker.off('message', onMsg);
             idle!.busy = false;
             idle!.lastUsed = Date.now();
             resolve(res);
           }
         };
         idle!.worker.on('message', onMsg);
         idle!.worker.postMessage(req);
       });
     }
   }
   ```
   </details>
