# Memory Leaks in JavaScript

> Unintentional object retention patterns, closure context entrapment, detached DOM nodes, EventEmitter leaks, and diagnostic workflows.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Memory Model](file:///e:/Learning/engineering-notes/01-languages/javascript/18_memory-and-garbage-collection/01_memory-model.md), [Garbage Collection](file:///e:/Learning/engineering-notes/01-languages/javascript/18_memory-and-garbage-collection/02_garbage-collection.md) |
| **Time to read** | 25 minutes |
| **Status** | Complete |
| **Standard / Runtime** | ECMAScript 2015+, Node.js & Evergreen Browsers |

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

In languages with automated garbage collection, true memory leaks (in the C/C++ sense of lost pointers to allocated memory) do not occur. Instead, JavaScript memory leaks are caused by **unintentional object retention**: objects that the application no longer requires, yet remain connected to the GC Root reference graph.

Because the V8 Garbage Collector can only reclaim objects that are strictly unreachable, any lingering reference—whether trapped in an uncleared timer, captured in a closure context, stored in an unbounded map, or attached to a detached DOM subtree—forces the engine to keep that entire object graph alive on the heap. Over time, heap usage grows monotonically until the application experiences severe GC thrashing and eventually crashes with an Out-of-Memory (OOM) error.

---

## 2. Core Concepts

### Reachability and Retained Size
When diagnosing memory, two metrics define every heap object:
- **Shallow Size**: The byte size directly occupied by the object's own structure (its internal header, primitive properties, and array pointer slots).
- **Retained Size**: The total byte size of memory that would be freed if this specific object was deleted. This includes the object's shallow size plus the size of all child objects reachable exclusively through it.

### The 5 Classic JavaScript Memory Leaks
1. **Accidental Global Variables**: Undeclared variables (`foo = 'bar'`) or assignments to `window` / `globalThis` remain referenced indefinitely because the global execution context never dies.
2. **Forgotten Timers and Callbacks**: `setInterval` and `setTimeout` keep their callback closures (and all variables captured in their lexical scope) alive until explicitly cancelled via `clearInterval` / `clearTimeout`.
3. **Retained Closures (The Meteor Leak Pattern)**: When multiple closures share the same enclosing scope, V8 merges captured variables into a shared context. A short-lived closure can keep a massive object alive because a sibling long-lived closure shares that scope context.
4. **Detached DOM Nodes**: A DOM node removed from the active document tree via `element.remove()` will not be garbage collected if any JavaScript variable or collection retains a reference to it or any of its descendants.
5. **Accumulated Event Listeners**: Adding listeners to long-lived targets (`window`, `document`, or Node.js singletons like `process`) without removing them retains the listener closure and everything it references.

---

## 3. How It Works Internally

```
+-------------------------------------------------------------------------+
| GC ROOTS (Global Window / Process / Active Call Stack)                  |
+-------------------------------------------------------------------------+
       |                                                    |
       | Retains                                            | Retains
       v                                                    v
+-----------------------------+             +-----------------------------+
| Uncleared setInterval()     |             | Global Event Bus (Emitter)  |
| - Callback Function Context |             | - Listeners: Array<Fn>      |
+-----------------------------+             +-----------------------------+
       |                                                    |
       | Captures                                           | Captures
       v                                                    v
+-----------------------------+             +-----------------------------+
| Lexical Closure Context     |             | Closure Context             |
| - userAccount: Object       |             | - largeComponentInstance    |
+-----------------------------+             +-----------------------------+
       |                                                    |
       | Retains                                            | Retains
       v                                                    v
+-------------------------------------------------------------------------+
| UNINTENTIONALLY RETAINED HEAP GRAPHS (Cannot be reclaimed by GC)        |
| - 50MB of cached data, database rows, or detached DOM elements           |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Taking Heap Snapshots in Node.js
```javascript
import v8 from 'node:v8';
import fs from 'node:fs';

function captureHeapSnapshot(filename) {
  const snapshotStream = v8.getHeapSnapshot();
  const fileStream = fs.createWriteStream(filename);
  snapshotStream.pipe(fileStream);
  console.log(`Heap snapshot written to ${filename}`);
}

// Write snapshot before and after load test to compare retained sizes
captureHeapSnapshot('baseline.heapsnapshot');
```

### AbortSignal for Automatic Listener Teardown
```javascript
const controller = new AbortController();

// Automatically unregisters listener when controller.abort() is called
window.addEventListener('resize', handleResize, { signal: controller.signal });

// Later, during component unmount:
controller.abort(); // Listener removed cleanly!
```

---

## 5. Practical Examples

### Example 1: The Meteor Lexical Closure Leak
```javascript
let theThing = null;

function replaceThing() {
  const originalThing = theThing;

  // unusedFunc captures `originalThing`
  const unusedFunc = function () {
    if (originalThing) console.log('hi');
  };

  // theThing captures a new closure `someMethod`
  // Because unusedFunc and someMethod share the SAME parent lexical context,
  // V8 allocates a shared Context holding originalThing.
  // Every call links a new closure context retaining the previous `theThing`!
  theThing = {
    longStr: new Array(1_000_000).join('*'),
    someMethod: function () {
      console.log('active');
    },
  };
}

// Repeated execution creates an infinite linked-chain of 1MB closures in the heap!
setInterval(replaceThing, 100);
```

### Example 2: Detached DOM Node Leak and Remedy
```javascript
// LEAK: Removing from DOM but keeping reference in JS
function createDetachedDomLeak() {
  const button = document.createElement('button');
  button.innerHTML = 'Click me';
  document.body.appendChild(button);

  // Storing reference in module-scoped or window-scoped array
  window.leakedButtons = window.leakedButtons || [];
  window.leakedButtons.push(button);

  // Removing from visible DOM
  button.remove();
  // Button is detached from DOM tree, but remains 100% alive in V8 heap!
}

// REMEDY: Clean up references upon element removal
function createCleanDomRemoval() {
  const button = document.createElement('button');
  document.body.appendChild(button);

  const tracker = new WeakSet();
  tracker.add(button);

  button.remove();
  // No strong references remain; GC reclaims button memory on next pass
}
```

### Example 3: Unbounded In-Memory Cache vs WeakMap / LRU Cache
```javascript
// BAD: Unbounded Map grows forever
const leakyCache = new Map();
function cacheUserProfile(user) {
  leakyCache.set(user.id, user); // User objects never garbage collected!
}

// GOOD: Associating metadata with object lifecycle using WeakMap
const userMetadata = new WeakMap();
function setMetadata(user, meta) {
  userMetadata.set(user, meta);
  // When `user` object is dereferenced by the app, its metadata in WeakMap
  // is automatically reclaimed by the GC with zero manual cleanup!
}

// GOOD: LRU Cache for ID-based lookups with fixed upper capacity
class SimpleLRUCache {
  constructor(maxSize = 1000) {
    this.maxSize = maxSize;
    this.cache = new Map();
  }

  get(key) {
    if (!this.cache.has(key)) return null;
    const val = this.cache.get(key);
    this.cache.delete(key);
    this.cache.set(key, val); // Refresh recency
    return val;
  }

  set(key, val) {
    if (this.cache.has(key)) {
      this.cache.delete(key);
    } else if (this.cache.size >= this.maxSize) {
      // Evict oldest (first inserted) item
      const oldestKey = this.cache.keys().next().value;
      this.cache.delete(oldestKey);
    }
    this.cache.set(key, val);
  }
}
```

### Example 4: Node.js EventEmitter Listener Leak
```javascript
import EventEmitter from 'node:events';

class GlobalDataBus extends EventEmitter {}
const bus = new GlobalDataBus();

class RequestHandler {
  constructor() {
    this.requestData = new Array(100_000).fill('payload');

    // BAD: Adding listener without removing it when request completes
    this.listener = () => this.handleUpdate();
    bus.on('update', this.listener);
  }

  handleUpdate() {
    console.log('Update received');
  }

  // MUST be called to prevent memory leak
  cleanup() {
    bus.removeListener('update', this.listener);
  }
}
```

### Example 5: Detecting Memory Leaks via Heap Growth Profiler
```javascript
import process from 'node:process';

export class LeakDetector {
  constructor(thresholdPercent = 20, checkIntervalMs = 5000) {
    this.threshold = thresholdPercent;
    this.interval = checkIntervalMs;
    this.baseline = null;
    this.timer = null;
  }

  start() {
    this.baseline = process.memoryUsage().heapUsed;

    this.timer = setInterval(() => {
      const current = process.memoryUsage().heapUsed;
      const growthMB = ((current - this.baseline) / 1024 / 1024).toFixed(2);
      const percentIncrease = ((current - this.baseline) / this.baseline) * 100;

      if (percentIncrease > this.threshold) {
        console.warn(`[LEAK WARNING] Heap has grown by ${growthMB}MB (+${percentIncrease.toFixed(1)}%)`);
      }
    }, this.interval);
  }

  stop() {
    clearInterval(this.timer);
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`EventEmitter.defaultMaxListeners` Warning**: When more than 10 listeners are added to an `EventEmitter`, Node.js logs `MaxListenersExceededWarning`. While intended to alert you to leaks, developers often silence it with `emitter.setMaxListeners(100)` rather than diagnosing the underlying retained callbacks.
2. **Console Object Retention**: Passing complex objects to `console.log(largeObject)` in browser environments keeps them alive in the browser DevTools memory context until the console is cleared.
3. **`URL.createObjectURL` Memory Retention**: Blobs registered via `URL.createObjectURL(blob)` remain alive in browser memory until `URL.revokeObjectURL(url)` is explicitly called or the document is unloaded.
4. **WeakSet / WeakMap Primitives Restriction**: Weak collections only accept object references as keys, not primitive strings or numbers. For caching keyed by string UUIDs, you cannot use `WeakMap`; you must use an LRU Cache.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| `setInterval` without `clearInterval` on unmount | Retains callback closure and captured variables indefinitely | Clear timer in teardown/unmount hooks |
| Removing DOM elements while retaining JS array pointers | Creates detached DOM trees that consume memory | Nullify JS references to removed elements |
| Accumulating listeners on `window` or `process` | Retains listener closures permanently | Detach listeners or use `{ signal }` with `AbortController` |
| Unbounded `Map` as an in-memory cache | Cache entries grow forever without eviction | Use LRU cache with fixed capacity |
| Silencing `MaxListenersExceededWarning` | Masks listener accumulation leaks | Fix the lifecycle teardown of listener subscriptions |
| Creating closures inside recurring timers that capture parent scope | Retains entire outer scope context | Extract callback to standalone function |

---

## 8. Performance Characteristics

- **Memory Bloat Acceleration**: A leak of 1,000 small 50-byte objects that capture a parent scope containing a 10MB buffer will consume 10GB of RAM. Retained size vastly outstrips shallow size.
- **GC Thrashing Cost**: As heap usage approaches `--max-old-space-size`, V8 triggers frequent, emergency Mark-Sweep-Compact cycles, jumping from 1% CPU to 90%+ CPU spent exclusively in GC pauses.

---

## 9. Engine Internals

1. **V8 Heap Profiler Object Retention Trees**:
   - V8's heap profiler computes the **Dominator Tree** of the heap graph.
   - Node A *dominates* Node B if every path from the GC Roots to B must pass through A.
   - An object's **Retained Size** is calculated as the sum of shallow sizes of all nodes in the dominator subtree rooted at that object.
2. **Context Merging Optimization**:
   - V8 allocates a single `Context` object for a lexical scope if any closure inside it captures variables. All closures in that scope hold a pointer to this same context, creating implicit retention dependencies.

---

## 10. Best Practices & Design Guidelines

- **Pair Subscriptions with Teardown**: Every `addEventListener`, `on('event')`, or `subscribe()` must have an unambiguous, co-located teardown (`removeEventListener`, `off`, `unsubscribe`).
- **Use WeakRefs for Non-Owning Caches**: For secondary indexing, use `WeakRef` to reference objects without preventing their garbage collection.
- **Automate Leak Detection in CI/CD**: Run end-to-end integration test suites under `node --inspect` and compare heap snapshots before and after 1,000 requests.

---

## 11. Real-World Case Study

### Diagnosing and Resolving an Express Middleware Token Leak
```javascript
// THE LEAK: A custom authentication middleware attached request audit metadata
// to a module-scoped Map using req.headers['authorization'] as the key.
// Over 3 days of production traffic, 4,000,000 unique bearer tokens were cached,
// consuming 3.2 GB of heap until Node crashed with OOM.

// THE RESOLUTION: Replace unbounded Map with an LRU cache with a 5-minute TTL
class TTLCache {
  constructor(ttlMs = 300_000, maxEntries = 50_000) {
    this.ttlMs = ttlMs;
    this.maxEntries = maxEntries;
    this.store = new Map();
  }

  set(key, value) {
    if (this.store.size >= this.maxEntries) {
      const oldestKey = this.store.keys().next().value;
      this.store.delete(oldestKey);
    }
    this.store.set(key, { value, expiresAt: Date.now() + this.ttlMs });
  }

  get(key) {
    const entry = this.store.get(key);
    if (!entry) return null;
    if (Date.now() > entry.expiresAt) {
      this.store.delete(key);
      return null;
    }
    return entry.value;
  }
}

// In middleware:
const tokenAuditCache = new TTLCache(300_000, 10_000);
```

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between Shallow Size and Retained Size in a heap snapshot?</summary>
Shallow size is the memory allocated directly for the object itself (its struct header and immediate properties). Retained size is the total memory that would be freed if the object was collected, including all downstream objects whose only path from GC Roots passes through that object.
</details>

<details>
<summary>2. How can two sibling closures in the same function cause a memory leak?</summary>
V8 allocates a shared lexical <code>Context</code> object on the heap for the enclosing function scope. If Closure A captures a massive object, and Closure B is long-lived (e.g., an event listener), Closure B keeps the shared Context object alive, which in turn keeps the massive object alive even if Closure B never references it.
</details>

<details>
<summary>3. What is a detached DOM node?</summary>
A detached DOM node is an element that has been removed from the document tree (e.g. via <code>node.remove()</code> or <code>innerHTML = ''</code>), but is still referenced by a JavaScript variable, array, or closure. Because a path from GC Roots exists, the browser engine cannot free the node or its child subtree.
</details>

<details>
<summary>4. Why does WeakMap prevent memory leaks in object caches?</summary>
In a standard <code>Map</code>, holding an object as a key creates a strong reference that prevents garbage collection. In a <code>WeakMap</code>, keys are held "weakly"—if no other strong references to the key object exist, the entry is eligible for garbage collection, and its memory is reclaimed automatically.
</details>

<details>
<summary>5. How does AbortController simplify event listener cleanup?</summary>
By passing <code>{ signal: controller.signal }</code> to <code>addEventListener</code>, you can remove multiple listeners across different elements and events simultaneously with a single call to <code>controller.abort()</code>, eliminating the need to maintain manual references to listener functions.
</details>

<details>
<summary>6. What causes a Node.js process to emit MaxListenersExceededWarning?</summary>
When more than 10 event listeners are attached to a single event on an <code>EventEmitter</code> without increasing <code>maxListeners</code>. It is a built-in diagnostic warning to alert developers that code may be repeatedly attaching listeners in a request loop without unregistering them.
</details>

<details>
<summary>7. How do you identify a memory leak using Chrome DevTools Heap Snapshots?</summary>
Take Snapshot 1 as a baseline. Perform the suspected action repeatedly (e.g., open and close a modal 10 times). Take Snapshot 2. Select the "Comparison" view in DevTools, filter by objects allocated between Snapshot 1 and Snapshot 2, and inspect the "# Delta" column for constructors whose count continuously increases.
</details>

<details>
<summary>8. Why can console.log cause memory retention in browser applications?</summary>
Browsers keep references to objects passed to <code>console.log()</code> in the DevTools console evaluation context so users can expand and inspect their properties later. Until the console is cleared (or closed), those objects remain reachable and cannot be garbage collected.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Identify and Fix the Retained Closure Leak
Identify the memory leak in the following code snippet and provide the corrected implementation.

```javascript
// BUGGY CODE:
function setupUserSession(user) {
  const hugeSessionLog = new Array(1_000_000).fill(user.id);

  function logAction(action) {
    console.log(`User ${user.id} did ${action}`);
  }

  function getSessionLength() {
    return hugeSessionLog.length;
  }

  // Only logAction is returned and stored globally
  return logAction;
}
```

<details>
<summary>View Solution</summary>

```javascript
// EXPLANATION:
// Because `logAction` and `getSessionLength` share the same lexical scope,
// V8 creates a single shared Context object containing both `user` and `hugeSessionLog`.
// Returning `logAction` retains `hugeSessionLog` in heap memory indefinitely!

// CORRECTED CODE:
function setupUserSessionFixed(user) {
  const userId = user.id; // Extract only what is needed

  // Isolate hugeSessionLog in its own separate block or scope
  {
    const hugeSessionLog = new Array(1_000_000).fill(userId);
    // Use hugeSessionLog here if needed...
  }

  // Now logAction only captures `userId`, leaving no path to hugeSessionLog
  return function logAction(action) {
    console.log(`User ${userId} did ${action}`);
  };
}
```
</details>

### Exercise 2: Auto-Expiring WeakRef Cache
Implement an in-memory cache `EphemeralCache` that stores values using `WeakRef`, allowing objects to be garbage collected when memory is low, while providing a clean `get()` API that handles dereferenced values.

<details>
<summary>View Solution</summary>

```javascript
export class EphemeralCache {
  constructor() {
    this.map = new Map();
    this.registry = new FinalizationRegistry((key) => {
      // Clean up map key when the object is reclaimed
      this.map.delete(key);
    });
  }

  set(key, objectValue) {
    if (typeof objectValue !== 'object' || objectValue === null) {
      throw new TypeError('EphemeralCache only accepts object values');
    }
    this.map.set(key, new WeakRef(objectValue));
    this.registry.register(objectValue, key);
  }

  get(key) {
    const ref = this.map.get(key);
    if (!ref) return undefined;

    const value = ref.deref();
    if (!value) {
      // Object has been garbage collected!
      this.map.delete(key);
      return undefined;
    }
    return value;
  }
}
```
</details>
