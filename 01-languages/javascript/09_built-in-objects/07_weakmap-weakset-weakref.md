# JavaScript WeakMap, WeakSet, WeakRef & FinalizationRegistry: Ephemerons & GC Internals

> Deep technical guide to weak references in ECMAScript, `WeakMap` and `WeakSet` ephemerons, ES2021 `WeakRef` and `FinalizationRegistry`, non-deterministic GC timing, and memory leak prevention.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced / Memory Engineering |
| **Prerequisites** | [09_built-in-objects/06_map-and-set.md](06_map-and-set.md), [01_fundamentals/02_data-types.md](../01_fundamentals/02_data-types.md) |
| **Time to Read** | 26 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 (WeakMap, WeakSet) / ES2021 (WeakRef, FinalizationRegistry) / ES2023 (Symbols as WeakMap keys) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Strong References vs Ephemerons](#2-core-concepts-strong-references-vs-ephemerons)
3. [How It Works: Engine Ephemeron Architecture & GC Tracing](#3-how-it-works-engine-ephemeron-architecture--gc-tracing)
4. [Comprehensive Syntax & API Reference](#4-comprehensive-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics: Weak Collections vs Strong Collections](#8-performance-characteristics-weak-collections-vs-strong-collections)
9. [Engine Internals: V8 EphemeronHashTable & Microtask Liveness](#9-engine-internals-v8-ephemeronhashtable--microtask-liveness)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Reactive DOM Event Listener Cleanup & Asset Cache](#11-real-world-case-study-reactive-dom-event-listener-cleanup--asset-cache)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In garbage-collected runtimes like JavaScript, an object remains in memory as long as it is **reachable** via a chain of references originating from the root (the global object, active stack frames, or event listeners).

This poses a major architectural dilemma when attaching metadata to objects you do not own (such as DOM nodes, third-party library objects, or request contexts):
```javascript
// Memory leak with standard Map:
const metadata = new Map();
function track(domNode) {
  metadata.set(domNode, { clickCount: 0 }); // Strong reference!
}
// Even if domNode is removed from the DOM document, 
// the Map holds a strong reference, preventing GC!
```

To solve this, ECMAScript provides **Weak References**:
1. **`WeakMap` & `WeakSet` (ES2015):** Collections where keys/values are held as **ephemerons**. If the key has no other strong references outside the weak collection, the engine automatically garbage-collects both the key and its associated value.
2. **`WeakRef` & `FinalizationRegistry` (ES2021):** Low-level primitives providing direct access to weak pointers and asynchronous garbage collection lifecycle hooks.

---

## 2. Core Concepts: Strong References vs Ephemerons

### 2.1 The Ephemeron Model
An **ephemeron** is a key-value pair where the value is reachable **if and only if** the key is reachable through an independent path.

```
Strong Reference (Standard Map):
[Root / Global] ---> [Map] ===(Strong)===> [Key Object]
                                \===(Strong)===> [Value Data]
(Key cannot be collected even if application drops it)

Ephemeron Reference (WeakMap):
[Root / Global] ---> [WeakMap] - - -(Weak)- - - > [Key Object]
                                    \====(Reachable only if Key is alive)===> [Value Data]
```

### 2.2 Why Weak Collections Are Not Iterable
Neither `WeakMap` nor `WeakSet` exposes `.size`, `.keys()`, `.values()`, `.entries()`, or `[Symbol.iterator]`.
**Reason:** Garbage collection is non-deterministic. If iteration were permitted, the array of keys would change unpredictably depending on whether a V8 garbage collection cycle had executed between lines of code, violating deterministic JavaScript execution semantics.

---

## 3. How It Works: Engine Ephemeron Architecture & GC Tracing

During a V8 Mark-Sweep-Compact garbage collection cycle:

1. **Root Tracing:** The GC marks all objects directly reachable from the stack and global variables.
2. **Ephemeron Identification:** When the GC encounters a `WeakMap`, it does **not** trace into its values immediately. Instead, it queues the `WeakMap` in an ephemeron worklist.
3. **Iterative Ephemeron Resolution:**
   - If a key object has been marked alive by the root tracing phase, the GC marks its corresponding value as alive.
   - If a key object was **not** marked alive, the key and its value are left unmarked.
4. **Sweeping Phase:** Unmarked key objects and their associated values are reclaimed simultaneously, reclaiming memory without developer intervention.

---

## 4. Comprehensive Syntax & API Reference

### 4.1 WeakMap API
Keys must be **objects** or **registered/unregistered symbols** (ES2023). Primitives (`number`, `string`, `boolean`) are rejected with `TypeError`:
```javascript
const wm = new WeakMap();
const user = { name: "Grace" };

wm.set(user, { role: "admin" }); // Associates metadata
console.log(wm.has(user));       // true
console.log(wm.get(user));       // { role: 'admin' }
wm.delete(user);                 // Returns boolean
```

### 4.2 WeakSet API
Values must be objects or symbols:
```javascript
const ws = new WeakSet();
const activeSocket = { id: 101 };

ws.add(activeSocket);
console.log(ws.has(activeSocket)); // true
ws.delete(activeSocket);
```

### 4.3 WeakRef (ES2021)
Creates a direct weak reference to a target object:
```javascript
let heavyObject = { data: new Array(1_000_000) };
const weakRef = new WeakRef(heavyObject);

// Dereference to access object:
const target = weakRef.deref();
if (target) {
  console.log("Object is still alive in heap!");
} else {
  console.log("Object was collected by GC.");
}
```

### 4.4 FinalizationRegistry (ES2021)
Triggers a callback after a target object has been garbage-collected:
```javascript
const registry = new FinalizationRegistry((heldValue) => {
  console.log(`[GC Notification] Object with token '${heldValue}' was collected.`);
});

let cacheTarget = { id: "asset_99" };
// Register target object, pass metadata string as heldValue:
registry.register(cacheTarget, "asset_99");

cacheTarget = null; // Unreferenced; eligible for GC
```

---

## 5. Practical Examples

### Example 1: Truly Private Class State via WeakMap
Before private fields (`#field`), `WeakMap` was the gold standard for bulletproof private state encapsulation:
```javascript
const privateData = new WeakMap();

class BankAccount {
  constructor(initialBalance) {
    privateData.set(this, {
      balance: initialBalance,
      transactions: []
    });
  }

  deposit(amount) {
    if (amount <= 0) throw new Error("Invalid amount");
    const state = privateData.get(this);
    state.balance += amount;
    state.transactions.push({ type: "DEPOSIT", amount, date: Date.now() });
  }

  getBalance() {
    return privateData.get(this).balance;
  }
}

const account = new BankAccount(500);
account.deposit(200);
console.log(account.getBalance()); // 700

// Truly private: properties are not on the object instance!
console.log(Object.keys(account)); // []
console.log(account.balance);      // undefined
```

### Example 2: DOM Node Metadata Storage Without Memory Leaks
```javascript
// Safely associate complex state with DOM nodes
const nodeMetadata = new WeakMap();

function attachRippleEffect(buttonElement) {
  nodeMetadata.set(buttonElement, {
    activeRipples: 0,
    cleanupTimer: null
  });

  buttonElement.addEventListener("click", () => {
    const meta = nodeMetadata.get(buttonElement);
    meta.activeRipples++;
    console.log(`Triggered ripple #${meta.activeRipples}`);
  });
}

// When buttonElement is removed from document and dereferenced,
// nodeMetadata automatically releases the object without manual removeEventListener.
```

### Example 3: Instance Branding & Access Verification via WeakSet
```javascript
const validInstances = new WeakSet();

class SecurePlugin {
  constructor() {
    validInstances.add(this);
  }

  executeOperation() {
    // Brand check: verifies 'this' was constructed by SecurePlugin
    if (!validInstances.has(this)) {
      throw new TypeError("Illegal invocation: receiver is not a valid SecurePlugin instance.");
    }
    return "Operation successful";
  }
}

const plugin = new SecurePlugin();
console.log(plugin.executeOperation()); // "Operation successful"

// Attempting to hijack method via call():
const fake = {};
// Throws TypeError: Illegal invocation
// plugin.executeOperation.call(fake);
```

### Example 4: Memory-Sensitive Image / Buffer Cache via WeakRef
```javascript
class ImageCache {
  constructor() {
    this.cache = new Map(); // Maps url -> WeakRef<ArrayBuffer>
  }

  get(url) {
    const ref = this.cache.get(url);
    if (!ref) return null;

    const buffer = ref.deref();
    if (!buffer) {
      // Object was collected; remove stale key
      this.cache.delete(url);
      return null;
    }
    return buffer;
  }

  set(url, buffer) {
    this.cache.set(url, new WeakRef(buffer));
  }
}

const imgCache = new ImageCache();
let highResBuffer = new Uint8Array([10, 20, 30]).buffer;
imgCache.set("/assets/hero.png", highResBuffer);

console.log("Cached exists:", imgCache.get("/assets/hero.png") !== null); // true
```

### Example 5: Resource Cleanup Tracking with FinalizationRegistry
```javascript
class NativeResourceWrapper {
  static #registry = new FinalizationRegistry((handleId) => {
    console.log(`[Native Cleanup] Releasing OS file handle #${handleId}`);
    // Native OS resource close syscall emulator
  });

  constructor(handleId) {
    this.handleId = handleId;
    NativeResourceWrapper.#registry.register(this, handleId, this);
  }

  closeExplicitly() {
    console.log(`[Manual Close] Explicitly releasing handle #${this.handleId}`);
    NativeResourceWrapper.#registry.unregister(this);
  }
}

let handle = new NativeResourceWrapper(4092);
handle.closeExplicitly(); // Explicit cleanup cancels registry hook
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 Primitives Cannot Be Used as WeakMap Keys
Passing numbers, strings, or booleans to `WeakMap.prototype.set()` throws a `TypeError`. Primitives have no unique object identity and are never garbage-collected in the traditional sense:
```javascript
const wm = new WeakMap();
// Throws TypeError: Invalid value used as weak map key:
// wm.set("myKey", 100); 
```

### 6.2 Never Rely on FinalizationRegistry for Critical Actions
Garbage collection is unpredictable and not guaranteed to run:
- If a program exits normally, the engine may terminate without running finalizers.
- If memory pressure is low, the GC may not run for minutes or hours.
- **Rule:** Never use `FinalizationRegistry` to commit database transactions, flush write buffers, or close essential sockets. Always provide explicit `.close()` or `using` methods.

### 6.3 Microtask Liveness Pinning with `WeakRef.prototype.deref()`
Per the ECMAScript specification, if `weakRef.deref()` returns an object during an execution turn, that object is kept alive across the remainder of the current synchronous turn and any immediate microtasks. It cannot be collected until control returns to the event loop.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Using `Map` for DOM Node Caching** | `nodeCache.set(domElement, data)` | `weakNodeCache.set(domElement, data)` | Prevents removed DOM elements from leaking memory. |
| **Attempting to iterate `WeakMap`** | `for (const x of weakMap)` | Use `Map` if iteration is required | Weak collections are non-enumerable to preserve GC non-determinism. |
| **Relying on Finalizer for Cleanup** | Omitting explicit `dispose()` methods | Provide explicit cleanup; use finalizer as fallback | Finalizers might never execute before process termination. |
| **Storing Target in Held Value** | `registry.register(target, target)` | Pass unreferenced token: `registry.register(target, target.id)` | Holding target in the registry callback creates a strong reference leak! |
| **Passing primitives to `WeakMap`** | `wm.set(userIdNumber, data)` | Use `Map` or wrap in object `{ id: userId }` | WeakMap keys must be objects or non-registered symbols. |
| **Using `WeakRef` to save memory on numbers** | Wrapping small numbers in `WeakRef` | Store numbers directly as primitives | `WeakRef` itself is a heap object (~32 bytes), adding memory overhead. |

---

## 8. Performance Characteristics: Weak Collections vs Strong Collections

1. **Lookup & Mutation Speed:**
   - `WeakMap.prototype.get()` and `.set()` perform at nearly identical speeds to standard `Map` ($O(1)$ amortized lookups).
2. **GC Sweeping Overhead:**
   - During major garbage collection cycles, resolving ephemeron tables requires iterative passes over the ephemeron worklist. Having tens of thousands of active `WeakMap` instances slightly increases GC pause times.
3. **Memory Conservation:**
   - Weak collections provide massive net memory savings in long-running Single Page Applications (SPAs) and servers by eliminating accidental retainment of unmounted components and closed socket connections.

---

## 9. Engine Internals: V8 EphemeronHashTable & Microtask Liveness

Inside V8, a `WeakMap` is represented by `v8::internal::EphemeronHashTable`.

```
                  +-----------------------------------+
                  |   v8::internal::EphemeronHashTable|
                  +-----------------------------------+
                  | Capacity / Hash Seed              |
                  +-----------------------------------+
                  | Slot 0: [Weak Key*] ===> [Value*] |
                  | Slot 1: [Weak Key*] ===> [Value*] |
                  | Slot 2: [Weak Key*] ===> [Value*] |
                  +-----------------------------------+
```

### The Ephemeron Worklist Algorithm:
1. When V8's Mark-Compact collector begins, it treats all pointers in the key slots as **weak pointers**.
2. If the object pointed to by `Weak Key*` is marked as alive from elsewhere in the heap, V8 treats the `Value*` pointer as a strong root and traverses into it.
3. If the key is not marked alive when tracing completes, V8 clears both the key and value slots to `undefined` during the Sweep phase.

---

## 10. Best Practices & Design Guidelines

1. **Use `WeakMap` for Associating External Metadata:** Whenever caching metadata, event handlers, or calculation results for objects owned by callers or external frameworks, use `WeakMap`.
2. **Always Provide Explicit Disposal:** If wrapping OS resources, provide a manual `dispose()` or `close()` method. Register with `FinalizationRegistry` solely as a defensive diagnostic fallback.
3. **Never Pass Target as the Held Value:** In `FinalizationRegistry.register(target, heldValue)`, ensure `heldValue` contains zero references back to `target`.
4. **Use `WeakSet` for Security Tagging:** Use `WeakSet` to tag authorized class instances and verify them in sensitive methods.

---

## 11. Real-World Case Study: Reactive DOM Event Listener Cleanup & Asset Cache

### Problem
A dashboard framework dynamically created and destroyed thousands of data visualization widgets. Each widget attached resize observers and telemetry listeners to DOM container nodes. Because listeners were stored in a global observer dictionary, destroyed widgets remained pinned in memory, leading to an 800 MB memory leak over an 8-hour workday.

### Solution
Refactor the subscription engine to store widget listeners in a `WeakMap` keyed by the DOM container, coupled with a `WeakRef` asset texture cache:

```javascript
class ComponentLifecycleManager {
  // Keyed weakly by DOM container element:
  static #subscriptions = new WeakMap();

  static attachListener(domNode, eventName, handler) {
    let nodeListeners = this.#subscriptions.get(domNode);
    if (!nodeListeners) {
      nodeListeners = [];
      this.#subscriptions.set(domNode, nodeListeners);
    }

    domNode.addEventListener(eventName, handler);
    nodeListeners.push({ eventName, handler });
  }

  // Active check helper:
  static hasActiveSubscriptions(domNode) {
    return this.#subscriptions.has(domNode);
  }
}

// Widget instantiation:
let container = { id: "chart-container-1", addEventListener() {} };
ComponentLifecycleManager.attachListener(container, "resize", () => {
  console.log("Resized");
});

console.log("Subscriptions tracked:", ComponentLifecycleManager.hasActiveSubscriptions(container)); // true

// When container is dropped by UI router:
container = null; // Instantly eligible for GC along with all its attached event closures!
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What is an ephemeron, and how does WeakMap implement it?</strong></summary>

An ephemeron is a key-value relationship where the value is reachable if and only if the key is reachable. In a `WeakMap`, the key is held weakly; if no external strong references to the key remain, the key is collected, and its corresponding value becomes unreachable and is garbage collected simultaneously, even if the value itself was strongly held by the map.
</details>

<details>
<summary><strong>Q2: Why are <code>WeakMap</code> and <code>WeakSet</code> not iterable?</strong></summary>

Iterability requires a deterministic, countable set of elements. Because garbage collection timing is non-deterministic and varies between engines and memory loads, exposing iteration or `.size` would cause code behavior to vary unpredictably based on when the GC decided to sweep memory.
</details>

<details>
<summary><strong>Q3: What error is thrown if you pass a primitive value to <code>WeakMap.prototype.set()</code>?</strong></summary>

A `TypeError` is thrown (`TypeError: Invalid value used as weak map key`). WeakMap keys must have object identity so their lifecycle can be tracked by the garbage collector. (Note: Non-registered Symbols are also permitted as of ES2023).
</details>

<details>
<summary><strong>Q4: What is the main hazard of using <code>FinalizationRegistry</code> for resource cleanup?</strong></summary>

The engine makes no guarantee as to *when* or *if* a finalizer will run. If memory pressure is low, the GC might never sweep the object. If the Node.js process terminates or the browser tab closes, pending finalizers are dropped. Critical resources (like file locks or database transactions) must always be closed explicitly.
</details>

<details>
<summary><strong>Q5: What happens if you register an object with <code>FinalizationRegistry</code> and pass the object itself as the <code>heldValue</code>?</strong></summary>

It causes a permanent memory leak. The registry holds a strong reference to `heldValue`. If `heldValue` is the target object (or contains a reference to it), the target object remains strongly reachable and will **never** be garbage-collected, preventing the finalizer from ever running.
</details>

<details>
<summary><strong>Q6: How does <code>WeakRef.prototype.deref()</code> behave during an active execution turn?</strong></summary>

If `.deref()` successfully retrieves the target object, the engine guarantees that the returned object is kept alive (pinned) for the remainder of the current synchronous turn and any immediate microtasks. It cannot be garbage-collected until execution yields back to the macro-task event loop.
</details>

<details>
<summary><strong>Q7: Can Symbols be used as keys in a <code>WeakMap</code>?</strong></summary>

Yes, as standardized in ECMAScript 2023. However, only **locally created symbols** (e.g., `Symbol("id")`) can be used. Registered global symbols created via `Symbol.for("key")` cannot be used because they are held globally for the lifetime of the process and are never collected.
</details>

<details>
<summary><strong>Q8: How does private class state implemented with <code>WeakMap</code> compare to native private fields (<code>#private</code>)?</strong></summary>

Both provide hard private encapsulation that cannot be breached via `Object.keys()` or `Object.getOwnPropertySymbols()`. However, native private fields (`#field`) are stored in internal V8 in-object slots and optimize into direct constant offsets, making them faster and allocating zero external `WeakMap` heap structures.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Memoization Cache with Auto-Expiring Object Keys
Write a `memoizeWithWeakMap(fn)` decorator for single-argument functions where the argument is an object. Ensure the memoized cached values are automatically garbage-collected when the argument object is destroyed.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function memoizeWithWeakMap(fn) {
  const cache = new WeakMap();

  return function(obj) {
    if (typeof obj !== "object" || obj === null) {
      throw new TypeError("Memoized argument must be an object");
    }

    if (cache.has(obj)) {
      return cache.get(obj);
    }

    const result = fn(obj);
    cache.set(obj, result);
    return result;
  };
}

let user = { id: 101, name: "Alan" };
const computeStats = memoizeWithWeakMap((u) => {
  console.log("Computing expensive stats...");
  return { hash: u.id * 42 };
});

console.log(computeStats(user)); // Computes...
console.log(computeStats(user)); // Returns cached result immediately
```
</details>

### Exercise 2: Implementing Safe FinalizationRegistry Token Unregistration
Create a `ManagedSocket` class that registers a finalization callback when connected, but properly unregisters the callback when `.disconnect()` is called manually.

<details>
<summary><strong>Solution</strong></summary>

```javascript
class ManagedSocket {
  static #registry = new FinalizationRegistry((socketId) => {
    console.warn(`[Leak Alert] Socket #${socketId} was garbage-collected without being disconnected!`);
  });

  constructor(socketId) {
    this.socketId = socketId;
    this.connected = true;
    // Register passing 'this' as both target and unregister token:
    ManagedSocket.#registry.register(this, this.socketId, this);
  }

  disconnect() {
    if (!this.connected) return;
    this.connected = false;
    ManagedSocket.#registry.unregister(this);
    console.log(`Socket #${this.socketId} cleanly disconnected.`);
  }
}

const s1 = new ManagedSocket(101);
s1.disconnect(); // Clean disconnection; unregisters from finalizer
```
</details>

### Exercise 3: Weak Cache Map with Deref Fallback
Implement a `WeakValueMap` that holds strong keys but weak values, automatically deleting dead keys when queried if the value has been garbage collected.

<details>
<summary><strong>Solution</strong></summary>

```javascript
class WeakValueMap {
  constructor() {
    this.map = new Map();
  }

  set(key, value) {
    if (typeof value !== "object" || value === null) {
      throw new TypeError("Values must be objects");
    }
    this.map.set(key, new WeakRef(value));
  }

  get(key) {
    const ref = this.map.get(key);
    if (!ref) return undefined;

    const val = ref.deref();
    if (val === undefined) {
      this.map.delete(key); // Dead entry cleanup
      return undefined;
    }
    return val;
  }

  has(key) {
    return this.get(key) !== undefined;
  }
}

const wvm = new WeakValueMap();
let payload = { data: "large" };
wvm.set("job_1", payload);

console.log(wvm.get("job_1")); // { data: 'large' }
```
</details>
