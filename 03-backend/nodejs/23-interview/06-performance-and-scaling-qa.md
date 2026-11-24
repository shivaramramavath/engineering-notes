# Node.js Technical Interview: Performance, Memory & Scaling Q&A

> Advanced questions and answers exploring V8 garbage collection internals, memory leak profiling, heap snapshots, CPU flamegraphs, and horizontal scaling patterns.

---

### Q1: Explain how the V8 Garbage Collector manages memory in Node.js. What are the Young and Old generations, and what causes "Stop-The-World" pauses?

#### In-Depth Answer
V8 divides its memory heap according to the **Generational Hypothesis**: *most objects die young* (e.g. temporary strings, request-scoped DTOs).

```
+----------------------------------------------------------------------------------------------------+
|                                           V8 Heap Layout                                           |
|                                                                                                    |
|  +──────────────────────────────────+  +────────────────────────────────────────────────────────+  |
|  |       Young Generation (New)     |  |                 Old Generation                         |  |
|  |  +─────────────+  +─────────────+|  |  +─────────────────────────+  +──────────────────────+ |  |
|  |  | From-Space  |  |  To-Space   ||  |  | Old Pointer Space       |  | Old Data Space       | |  |
|  |  | (16-32MB)   |  | (16-32MB)   ||  |  | (Long-lived closures)   |  | (Strings, raw bytes) | |  |
|  |  +─────────────+  +─────────────+|  |  +─────────────────────────+  +──────────────────────+ |  |
|  +──────────────────────────────────+  +────────────────────────────────────────────────────────+  |
+----------------------------------------------------------------------------------------------------+
```

1. **Young Generation (Minor GC / Scavenger)**:
   - Size: Small (16MB to 64MB).
   - Algorithm: Cheney's copying algorithm between two semi-spaces (`From-Space` and `To-Space`).
   - Execution: Extremely fast (1–3ms). Surviving objects are moved to the other semi-space. If an object survives two consecutive Scavenge cycles, it is **promoted to the Old Generation**.
2. **Old Generation (Major GC / Mark-Sweep-Compact)**:
   - Contains long-lived objects (database connection pools, singletons, session caches).
   - Algorithm:
     - **Marking**: V8 traverses roots (global variables, active call stack, DOM/handles) and marks reachable objects. Uses incremental and concurrent marking on background threads to minimize main-thread pauses.
     - **Sweeping**: Memory occupied by unmarked (unreachable) dead objects is reclaimed into free-lists.
     - **Compacting**: Relocates surviving fragmented objects into contiguous memory blocks to eliminate heap fragmentation.
3. **Stop-The-World (STW) Pauses**:
   - Compaction and final mark completion require pausing the main JavaScript thread so pointers do not move while code is executing. If the Old Generation grows to gigabytes (e.g. `--max-old-space-size=8192`), STW compaction pauses can spike to 100ms–500ms, starving incoming network traffic.

---

### Q2: What are the most common causes of Memory Leaks in Node.js, and how do you diagnose them using Heap Snapshots?

#### In-Depth Answer

#### The 4 Classic Node.js Memory Leak Antipatterns:
1. **Unbounded Global Variables / Caches**:
   ```javascript
   const cache = {}; // Module-level singleton
   app.get('/data', (req, res) => {
     cache[req.query.id] = req.body; // Never evicted! Heap grows forever until OOM.
   });
   // Fix: Use an LRU cache with max entries and TTL (e.g. lru-cache).
   ```
2. **Dangling Event Listeners**:
   Attaching listeners to long-lived objects (`process.on('message')`, DB pools) inside request handlers without cleanup. When the request finishes, the callback closure holds a reference to the `req` and `res` objects, preventing them from being garbage-collected.
3. **Forgotten Timers / Intervals**:
   `setInterval()` retains all variables referenced inside its closure until `clearInterval()` is explicitly invoked.
4. **Accidental Closures**:
   Inner functions retaining outer scopes containing large buffers or strings.

#### Diagnosing Memory Leaks via Heap Snapshots:
1. Take baseline snapshot: `node --inspect server.js` $\rightarrow$ Chrome DevTools `chrome://inspect` $\rightarrow$ Memory Tab $\rightarrow$ Take Snapshot 1.
2. Run a load test (e.g. `autocannon -c 50 -d 30s http://localhost:3000/api`).
3. Take Snapshot 2.
4. Run another load test and take Snapshot 3.
5. In Chrome DevTools, select Snapshot 3 and choose **"Comparison"** against Snapshot 1:
   - Sort by **# Delta** and **Size Delta**.
   - Inspect constructor names: look for `(closure)`, `Object`, or custom classes with monotonically increasing counts that never decrease after GC.

---

### Q3: Compare Node.js Clustering vs Worker Threads vs Container Scaling (Docker/Kubernetes). When should each be used?

#### In-Depth Answer

| Architecture | Mechanism | Memory Model | Best Use Case |
| :--- | :--- | :--- | :--- |
| **Cluster Module (`node:cluster`)** | Spawns multiple independent OS processes (forks) sharing a master server port via IPC/SO_REUSEPORT. | Completely isolated memory (no shared state). | Utilizing multi-core bare-metal VMs or single VM instances without container orchestrators. |
| **Worker Threads (`worker_threads`)** | Spawns multiple OS threads within the **same** Node.js process. | Shared memory via `SharedArrayBuffer` & `Atomics`, or structured cloning over `MessagePort`. | Offloading heavy CPU-bound tasks (image processing, video transcoding, PDF generation, cryptographic computation). |
| **Container Horizontal Scaling (Kubernetes / ECS)** | Deploys single-threaded Node.js containers scaled horizontally across a cluster behind an ingress load balancer. | Completely isolated container namespaces. | **Modern Production Standard**: Fault isolation, automated horizontal pod autoscaling (HPA), rolling zero-downtime deployments. |

#### Architectural Verdict:
In modern cloud-native environments (AWS/Kubernetes), **run single-threaded Node.js containers without the Cluster module**; let Kubernetes manage process lifecycles and horizontal scaling. Reserve `worker_threads` exclusively for parallel CPU calculations.
