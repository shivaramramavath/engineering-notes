# Garbage Collection in V8 and JavaScript

> V8 generational garbage collection, Scavenger semi-spaces, Mark-Sweep-Compact, Orinoco concurrent marking, and GC pause mitigation.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Memory Model](file:///e:/Learning/engineering-notes/01-languages/javascript/18_memory-and-garbage-collection/01_memory-model.md), [Event Loop](file:///e:/Learning/engineering-notes/01-languages/javascript/12_event-loop/01_event-loop.md) |
| **Time to read** | 25 minutes |
| **Status** | Complete |
| **Standard / Runtime** | V8 Engine (Node.js & Chromium) |

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

In JavaScript, developers do not explicitly deallocate memory. The JavaScript engine automatically reclaims memory occupied by objects that are no longer reachable by the application. This automated reclamation is managed by the **Garbage Collector (GC)**.

In high-throughput services and 60/120 FPS browser applications, garbage collection is often the primary source of latency spikes, dropped frames (jank), and tail-latency outliers (p99/p999). When the GC stops execution to trace heap graphs—a "Stop-the-World" (STW) pause—the event loop freezes. Understanding V8's internal garbage collection architecture—the **Generational Hypothesis**, **Minor GC (Scavenger)**, **Major GC (Mark-Sweep-Compact)**, and **Orinoco concurrent engine**—enables engineers to write code that minimizes GC overhead and eliminates latency stutter.

---

## 2. Core Concepts

### The Weak Generational Hypothesis
V8's memory management is founded on an empirical observation known as the **Weak Generational Hypothesis**: *most objects die young*. In typical JavaScript programs, over 80–90% of allocated objects (temporary iteration variables, intermediate string substrings, short-lived promises) become unreachable almost immediately after creation.

To exploit this, V8 splits the heap into two generations:
1. **Young Generation (New Space)**: Small (typically 16–64 MB), fast allocation, collected frequently via **Minor GC (Scavenger)**.
2. **Old Generation (Old Space)**: Large (hundreds of MB to several GB), contains objects that have survived multiple young generation scavenge cycles. Collected infrequently via **Major GC (Full Mark-Sweep-Compact)**.

### Reachability and Root Objects
An object is "alive" (retained) if it is reachable from a set of **GC Roots**:
- Global variables (`globalThis`, `window`, Node.js `global`)
- Active local variables and execution contexts on the Call Stack
- Built-in internal V8 objects and DOM wrappers
- Active event listeners and timer references

If an object graph cannot be reached via any path starting from the GC Roots, it is dead and eligible for reclamation.

---

## 3. How It Works Internally

```
+--------------------------------------------------------------------------+
| NEW SPACE (Young Generation)                                             |
|                                                                          |
|  [ From-Space (Active Allocations) ]        [ To-Space (Idle Receiver) ] |
|  +---------------------------------+        +--------------------------+ |
|  | Obj A (Dead)                    |        |                          | |
|  | Obj B (Survives 1st Scavenge)  | =====> | Obj B (Copied to To-Space| |
|  | Obj C (Dead)                    |        +--------------------------+ |
|  | Obj D (Survives 2nd Scavenge)  | =========================+           |
|  +---------------------------------+                         |           |
|                                                              | Promoted  |
+--------------------------------------------------------------|-----------+
                                                               v
+--------------------------------------------------------------------------+
| OLD SPACE (Old Generation)                                               |
|                                                                          |
|  Phase 1: Concurrent Mark (Tri-color algorithm: White/Grey/Black)        |
|  Phase 2: Concurrent/Parallel Sweep (Reclaim memory into Free-Lists)     |
|  Phase 3: Parallel Compact (Defragment scattered heap chunks)            |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Observing GC Activity via Node CLI Flags
```bash
# Basic GC logs (displays duration and space reclaimed)
node --trace-gc app.js

# Detailed GC logs with memory breakdowns per space
node --trace-gc-verbose app.js

# Expose manual GC function for benchmarking
node --expose-gc app.js
```

### Programmatic Memory & GC Observation
```javascript
import { PerformanceObserver } from 'node:perf_hooks';
import v8 from 'node:v8';

// Monitor GC pauses programmatically
const obs = new PerformanceObserver((list) => {
  for (const entry of list.getEntries()) {
    console.log(`GC Type: ${entry.name}, Duration: ${entry.duration.toFixed(2)}ms`);
    // entry.detail contains V8 GC kind: 1 = Scavenge (Minor), 2 = Mark-Sweep (Major)
  }
});
obs.observe({ entryTypes: ['gc'] });

// Inspect heap space breakdown
const spaces = v8.getHeapSpaceStatistics();
console.table(spaces.map(s => ({
  space: s.space_name,
  sizeMB: (s.space_size / 1024 / 1024).toFixed(2),
  usedMB: (s.space_used_size / 1024 / 1024).toFixed(2),
})));
```

---

## 5. Practical Examples

### Example 1: Measuring Scavenge vs Full GC Latency Under Load
```javascript
import { PerformanceObserver, performance } from 'node:perf_hooks';

const obs = new PerformanceObserver((list) => {
  for (const entry of list.getEntries()) {
    const isMinor = entry.detail?.kind === 1;
    console.log(`[${isMinor ? 'Minor GC' : 'Major GC'}] ${entry.duration.toFixed(2)}ms`);
  }
});
obs.observe({ entryTypes: ['gc'] });

// Churning short-lived objects (Triggers Minor GC Scavenges)
function generateYoungGenChurn() {
  for (let i = 0; i < 500_000; i++) {
    const temp = { id: i, payload: `item-${i}` };
  }
}

// Retaining long-lived objects (Triggers Major GC Mark-Sweep)
const retained = [];
function generateOldGenRetention() {
  for (let i = 0; i < 200_000; i++) {
    retained.push(new Array(50).fill(i));
  }
}

generateYoungGenChurn();
generateOldGenRetention();
```

### Example 2: Tri-Color Marking Simulation
```javascript
// Conceptual simulation of V8's Tri-Color Marking Algorithm
const WHITE = 0; // Unvisited (Candidate for collection)
const GREY = 1;  // Visited, but child references not yet scanned
const BLACK = 2; // Visited and all child references scanned

function simulateTriColorMarking(roots, allObjects) {
  const colorMap = new Map();
  allObjects.forEach(obj => colorMap.set(obj, WHITE));

  const greyStack = [];

  // Step 1: Mark roots as Grey
  for (const root of roots) {
    colorMap.set(root, GREY);
    greyStack.push(root);
  }

  // Step 2: Process Grey objects
  while (greyStack.length > 0) {
    const current = greyStack.pop();
    for (const child of current.references) {
      if (colorMap.get(child) === WHITE) {
        colorMap.set(child, GREY);
        greyStack.push(child);
      }
    }
    colorMap.set(current, BLACK);
  }

  // Step 3: Sweep White objects
  const deadObjects = allObjects.filter(obj => colorMap.get(obj) === WHITE);
  console.log(`Reclaimed ${deadObjects.length} dead objects`);
  return deadObjects;
}
```

### Example 3: Mitigating GC Churn with Object Pooling
```javascript
// POOR: Allocating 10,000 vectors per animation frame triggers constant GC pauses
function badRenderLoop() {
  for (let i = 0; i < 10000; i++) {
    const vec = { x: i, y: i * 2 }; // Churns New Space
  }
}

// EXCELLENT: Object Pool reuses existing heap allocations with zero GC churn
class VectorPool {
  constructor(size) {
    this.pool = Array.from({ length: size }, () => ({ x: 0, y: 0 }));
    this.index = 0;
  }

  obtain(x, y) {
    const vec = this.pool[this.index++];
    vec.x = x;
    vec.y = y;
    return vec;
  }

  reset() {
    this.index = 0; // Free all pooled objects without triggering GC!
  }
}

const pool = new VectorPool(10000);
function goodRenderLoop() {
  pool.reset();
  for (let i = 0; i < 10000; i++) {
    const vec = pool.obtain(i, i * 2);
  }
}
```

### Example 4: Preventing Unintentional Old Gen Promotion
```javascript
// Premature promotion happens when objects live just long enough to survive 2 scavenge cycles
async function processBatchWithCleanup(items) {
  for (const item of items) {
    // Process item within isolated scope so it becomes dead before next scavenge
    processItem(item);
  }
}

function processItem(item) {
  const transformed = JSON.stringify(item);
  // `transformed` dies immediately when processItem returns
}
```

### Example 5: Benchmarking GC Overhead with `--expose-gc`
```javascript
// Run with: node --expose-gc benchmark.js
import { performance } from 'node:perf_hooks';

if (typeof globalThis.gc !== 'function') {
  console.error('Run with --expose-gc to run this benchmark');
  process.exit(1);
}

// Flush heap before test
globalThis.gc();

const startMemory = process.memoryUsage().heapUsed;
const startTime = performance.now();

// Allocation benchmark
const array = [];
for (let i = 0; i < 1_000_000; i++) {
  array.push({ i });
}

console.log(`Allocated in: ${(performance.now() - startTime).toFixed(2)}ms`);

// Force Major GC and measure pause duration
const gcStartTime = performance.now();
globalThis.gc();
const gcDuration = performance.now() - gcStartTime;

console.log(`Full Major GC Pause: ${gcDuration.toFixed(2)}ms`);
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Stop-the-World Latency Spikes**: While V8's Orinoco project performs most marking concurrently on background threads, final atomic marking and sweeping require brief Stop-the-World pauses. On multi-gigabyte heaps with millions of interconnected objects, these pauses can exceed 50–100ms.
2. **Write Barriers**: In concurrent marking, background threads scan the object graph while the main JavaScript thread continues running. If the main thread assigns a White object to a Black object, a **Write Barrier** must intercept the mutation and color the White object Grey, otherwise it would be mistakenly collected. Write barriers add slight CPU overhead to object property mutations during marking phases.
3. **Fragmentation in Old Space**: Unlike New Space (which evacuates and packs objects contiguously during scavenging), Old Space can become fragmented with gaps between live objects. When large allocations fail to find contiguous space, V8 is forced to execute an expensive **Compaction** cycle.
4. **WeakRef and FinalizationRegistry Timing**: ECMAScript specifications make zero guarantees about when or whether `WeakRef` targets will be cleared or `FinalizationRegistry` callbacks executed. Never rely on them for essential business logic or deterministic resource cleanup.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Creating short-lived objects inside tight render/request loops | Saturates New Space, triggering frequent Scavenge pauses | Use Object Pools or flat TypedArrays |
| Manually calling `global.gc()` in production | Freezes the event loop with an uncoordinated full Major GC | Rely on V8's automated idle-time GC scheduling |
| Keeping references in global arrays/maps | Prevents GC reachability graph from freeing dead objects | Use `WeakMap` or `WeakSet` for metadata associations |
| Accumulating event listeners without `removeListener` | Retains listener closures and target objects permanently | Detach listeners or use `AbortController` signals |
| Exceeding heap limits without backpressure | Triggers catastrophic V8 Out-Of-Memory termination | Apply backpressure to incoming streams |
| Allocating massive objects ($> 512\text{KB}$) in tight loops | Bypasses New Space into Large Object Space, bloating Old Gen | Chunk large allocations or stream buffers |

---

## 8. Performance Characteristics

- **Minor GC (Scavenge) Cost**: Proportional to the number of *surviving* objects, NOT dead objects. If 95% of objects are dead, Scavenging is blisteringly fast (typically 0.5–2ms).
- **Major GC (Mark-Sweep) Cost**: Proportional to the total size of the Old Generation heap and graph complexity (typically 10–100ms on large heaps).
- **Orinoco Concurrency Gains**: Concurrent marking reduces main-thread STW pause times by up to 80% compared to legacy synchronous V8 engines.

---

## 9. Engine Internals

1. **Cheney's Copying Algorithm (Scavenger)**:
   - New Space is split into **From-Space** and **To-Space**.
   - Live objects in From-Space are copied sequentially into To-Space, automatically packing them contiguously without memory fragmentation.
   - Pointers are updated. Then, From-Space and To-Space swap roles.
   - If an object survives two Scavenge passes (tracked by an age bit in its header), it is **promoted** to Old Space.
2. **Orinoco Architecture**:
   - **Parallel**: Main thread and helper threads execute GC work simultaneously during a pause.
   - **Incremental**: The main thread breaks full marking into smaller slices interleaved between event loop turns.
   - **Concurrent**: Helper threads perform marking and sweeping in the background while the main JavaScript thread continues running uninterrupted.

---

## 10. Best Practices & Design Guidelines

- **Keep Objects Short-Lived or Permanent**: Avoid "medium-lived" objects (objects that live just long enough to be promoted to Old Space, only to die immediately after).
- **Use Weak Collections for Ephemeral Metadata**: Associate data with object lifecycles using `WeakMap` so metadata is automatically collected when the key object dies.
- **Tune Heap Size Realistically**: Avoid artificially huge heaps (`--max-old-space-size=16384`) if not needed, as larger heaps increase the duration of unavoidable full GC compaction phases.

---

## 11. Real-World Case Study

### High-Frequency Trading WebSocket Ingestion: Eliminating GC Jitter
```javascript
// PROBLEM: Parsing 50,000 tick updates per second using JSON.parse()
// produced 50,000 ephemeral objects per second, causing 15ms GC pauses every 2 seconds.

// SOLUTION: Binary protocol parser reusing a single pre-allocated record instance
class ReusableTickParser {
  constructor() {
    this.currentTick = { symbol: '', price: 0, volume: 0, timestamp: 0 };
  }

  parseBuffer(buffer, onTick) {
    let offset = 0;
    while (offset < buffer.length) {
      // Decode into the SAME in-place record (zero new heap allocations)
      this.currentTick.price = buffer.readFloatLE(offset);
      this.currentTick.volume = buffer.readInt32LE(offset + 4);
      this.currentTick.timestamp = buffer.readBigInt64LE(offset + 8);
      offset += 16;

      onTick(this.currentTick);
    }
  }
}

// Result: New Space churn dropped by 99.8%; P99 latency dropped from 18ms to 0.4ms.
```

---

## 12. Interview Questions

<details>
<summary>1. What is the Weak Generational Hypothesis and how does V8 use it?</summary>
The Weak Generational Hypothesis states that the vast majority of allocated objects die very shortly after creation. V8 exploits this by dividing the heap into New Space (young generation) and Old Space (old generation). New Space is small and collected frequently using a fast copying scavenger algorithm, while long-lived objects are promoted to Old Space and collected much less frequently.
</details>

<details>
<summary>2. How does Cheney's copying algorithm work during a Minor GC?</summary>
New Space is divided into two semi-spaces: From-Space and To-Space. When From-Space fills up, the scavenger traces reachable live objects and copies them contiguously into To-Space, compacting them and eliminating memory fragmentation. Unreachable objects are left behind. The spaces then swap roles.
</details>

<details>
<summary>3. What is the Tri-Color Marking algorithm used in Major GC?</summary>
Objects are classified into three colors: <b>White</b> (unvisited, initial state, candidate for collection), <b>Grey</b> (visited, but child pointers not yet examined), and <b>Black</b> (visited, and all referenced children examined). The GC pops objects from the Grey worklist, marks referenced White children as Grey, and turns the current object Black. Once no Grey objects remain, all remaining White objects are reclaimed.
</details>

<details>
<summary>4. What is a Write Barrier and why is it necessary during concurrent marking?</summary>
During concurrent marking, background GC threads color objects while the main thread continues running JavaScript. If the main thread assigns a White (unmarked) object as a property of a Black (scanned) object, the GC would fail to traverse that White object, causing it to be erroneously deleted. A write barrier intercepts property mutations and repaints the White object Grey.
</details>

<details>
<summary>5. What are the three phases of V8's Major GC?</summary>
1. <b>Marking</b>: Identifies all live reachable objects starting from GC roots using tri-color marking.
2. <b>Sweeping</b>: Reclaims dead object memory and records free memory gaps in free-lists for future allocations.
3. <b>Compacting</b>: Evacuates live objects from heavily fragmented memory pages to new contiguous pages, updating all pointer references.
</details>

<details>
<summary>6. How does V8's Orinoco project achieve low-latency garbage collection?</summary>
Orinoco replaces synchronous Stop-the-World pauses with a hybrid combination of: <i>Concurrent</i> marking and sweeping (running on background threads while JS executes), <i>Parallel</i> scavenging and compaction (multiple worker threads running during pauses), and <i>Incremental</i> marking (interleaving marking work in small chunks across event loop ticks).
</details>

<details>
<summary>7. What causes an object to be promoted from New Space to Old Space?</summary>
An object is promoted to Old Space if it survives two consecutive Minor GC scavenge cycles (tracked by an age bit in its header) or if To-Space becomes more than 25% full during a scavenge to prevent out-of-memory errors in New Space.
</details>

<details>
<summary>8. Why should you avoid calling global.gc() in production code?</summary>
Calling <code>global.gc()</code> forces an uncoordinated, synchronous Major GC Stop-the-World pause, disrupting V8's adaptive scheduling heuristics and introducing massive latency spikes into high-throughput servers.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Real-Time GC Pause Watchdog
Implement an automated performance observer that monitors GC pauses and emits an alert if any individual Major GC pause exceeds 30ms or if more than 5 Minor GC pauses occur within a 1-second rolling window.

<details>
<summary>View Solution</summary>

```javascript
import { PerformanceObserver } from 'node:perf_hooks';

class GCWatchdog {
  constructor(alertCallback) {
    this.alertCallback = alertCallback;
    this.minorGCTimestamps = [];

    this.obs = new PerformanceObserver((list) => {
      for (const entry of list.getEntries()) {
        const isMinor = entry.detail?.kind === 1;

        if (!isMinor && entry.duration > 30) {
          this.alertCallback(`MAJOR_PAUSE_ALERT: Major GC took ${entry.duration.toFixed(2)}ms`);
        }

        if (isMinor) {
          const now = Date.now();
          this.minorGCTimestamps.push(now);
          this.minorGCTimestamps = this.minorGCTimestamps.filter(t => now - t <= 1000);

          if (this.minorGCTimestamps.length > 5) {
            this.alertCallback(`SCAVENGE_CHURN_ALERT: ${this.minorGCTimestamps.length} Minor GCs in 1 second!`);
          }
        }
      }
    });

    this.obs.observe({ entryTypes: ['gc'] });
  }

  stop() {
    this.obs.disconnect();
  }
}
```
</details>

### Exercise 2: Reusable Buffer Slice Allocator
Create a memory-efficient slice allocator that allocates a single 1MB `ArrayBuffer` upfront and doles out sub-slices using `Uint8Array(buffer, offset, length)` to completely eliminate GC allocations when decoding small binary messages.

<details>
<summary>View Solution</summary>

```javascript
export class SlabAllocator {
  constructor(slabSize = 1024 * 1024) {
    this.slab = new ArrayBuffer(slabSize);
    this.offset = 0;
    this.slabSize = slabSize;
  }

  allocate(bytes) {
    if (this.offset + bytes > this.slabSize) {
      // Allocate fresh slab when current is exhausted
      this.slab = new ArrayBuffer(this.slabSize);
      this.offset = 0;
    }

    const view = new Uint8Array(this.slab, this.offset, bytes);
    this.offset += bytes;
    return view;
  }
}
```
</details>
