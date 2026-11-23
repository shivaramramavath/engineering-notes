# Event Emitter & Publish-Subscribe in JavaScript

> A production-grade implementation and architectural guide to EventEmitters in JavaScript, featuring copy-on-write dispatch, wildcard namespaces, asynchronous pipelines, and memory leak detection.

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate / Production-Grade |
| **Prerequisites** | [06_closures/02_closure-use-cases.md](file:///e:/Learning/engineering-notes/01-languages/javascript/06_closures/02_closure-use-cases.md), [16_nodejs/03_events.md](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs/03_events.md) |
| **Standard Baseline** | ECMAScript 2024 / Node.js 20+ LTS / Browser Evergreen |
| **Status** | Production Ready |

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

In complex web applications, micro-frontends, and backend microservices, components frequently need to communicate without maintaining tight compile-time coupling. Directly importing and calling methods across disparate modules introduces rigid dependencies, complicates testing, and creates circular reference hazards.

The **Event Emitter (Publish-Subscribe)** pattern decouples the emitter of an event from its consumers:
- **Loose Coupling**: Emitters broadcast named events without knowing which—or how many—subscribers are listening.
- **Asynchronous & Synchronous Pipelines**: Subscribers react to notifications independently.
- **Cross-Boundary Messaging**: Provides a shared communication bus between decoupled application modules, plugins, or micro-frontends.

---

## 2. Core Concepts

### Core Methods
- **`on(event, listener)`**: Registers a persistent listener callback.
- **`once(event, listener)`**: Registers a single-use listener that automatically deregisters after its first invocation.
- **`emit(event, ...args)`**: Synchronously invokes all listeners registered for `event` in insertion order.
- **`off(event, listener)`**: Deregisters a specific listener callback.
- **`removeAllListeners(event)`**: Removes all listeners for a specific event or all events entirely.

```
       [ Emitter Module ]
               │
               ▼  emit('order:created', orderData)
     ┌────────────────────────────────────────────────────────┐
     │                   Event Emitter Bus                    │
     └────────────────────────────────────────────────────────┘
         │                          │                         │
         ▼                          ▼                         ▼
 [ Email Service ]         [ Inventory Service ]      [ Analytics Service ]
  (Sends receipt)           (Deducts stock)            (Logs metric)
```

### The Special `'error'` Event Convention
In standard JavaScript and Node.js EventEmitters, the `'error'` event holds special status. If an emitter triggers `emit('error', err)` and no listener has been registered for `'error'`, the emitter **throws the error as an uncaught exception**, crashing the Node.js process. Always register error handlers.

---

## 3. How It Works Internally

The emitter stores events in a `Map<string, Set<Function>>` or `Map<string, Array<Function>>`.

### The Copy-on-Write Dispatch Loop
A critical hazard occurs when a listener deregisters itself or registers another listener *during* the execution of `emit()`:
```javascript
emitter.on('data', function handler() {
  emitter.off('data', handler); // Mutates listener collection while iterating!
});
```
To avoid skipping subsequent listeners or encountering index offset corruptions, a production emitter **copies the listener collection** before iterating:
```javascript
const listeners = [...(this._events.get(eventName) || [])];
for (const listener of listeners) {
  listener.apply(this, args);
}
```

---

## 4. Syntax & API Reference

### Production-Grade Universal EventEmitter
```javascript
/**
 * Production-ready universal EventEmitter with leak detection,
 * once-listeners, copy-on-write dispatch, and async support.
 */
export class EventEmitter {
  constructor(options = {}) {
    this._events = new Map();
    this._maxListeners = options.maxListeners ?? 10;
  }

  setMaxListeners(n) {
    if (typeof n !== 'number' || n < 0) {
      throw new TypeError('maxListeners must be a non-negative number');
    }
    this._maxListeners = n;
    return this;
  }

  getMaxListeners() {
    return this._maxListeners;
  }

  on(eventName, listener) {
    if (typeof listener !== 'function') {
      throw new TypeError('Listener must be a function');
    }

    let listeners = this._events.get(eventName);
    if (!listeners) {
      listeners = [];
      this._events.set(eventName, listeners);
    }

    listeners.push(listener);

    // Leak detection check
    if (this._maxListeners > 0 && listeners.length > this._maxListeners) {
      console.warn(
        `[MaxListenersExceededWarning] Possible EventEmitter memory leak detected. ` +
        `${listeners.length} '${String(eventName)}' listeners added. Use setMaxListeners() to increase limit.`
      );
    }

    return this;
  }

  once(eventName, listener) {
    if (typeof listener !== 'function') {
      throw new TypeError('Listener must be a function');
    }

    const wrapper = (...args) => {
      this.off(eventName, wrapper);
      listener.apply(this, args);
    };

    // Store reference to original function so off(eventName, listener) works
    wrapper.originalListener = listener;
    this.on(eventName, wrapper);
    return this;
  }

  off(eventName, listener) {
    if (typeof listener !== 'function') {
      throw new TypeError('Listener must be a function');
    }

    const listeners = this._events.get(eventName);
    if (!listeners) return this;

    const index = listeners.findIndex(
      (l) => l === listener || l.originalListener === listener
    );

    if (index !== -1) {
      listeners.splice(index, 1);
      if (listeners.length === 0) {
        this._events.delete(eventName);
      }
    }

    return this;
  }

  emit(eventName, ...args) {
    // Special error handling convention
    if (eventName === 'error') {
      const listeners = this._events.get('error');
      if (!listeners || listeners.length === 0) {
        const error = args[0] instanceof Error ? args[0] : new Error(`Unhandled error: ${args[0]}`);
        throw error;
      }
    }

    const listeners = this._events.get(eventName);
    if (!listeners || listeners.length === 0) return false;

    // Copy-on-write snapshot to prevent iteration mutation bugs
    const snapshot = [...listeners];
    for (let i = 0; i < snapshot.length; i++) {
      snapshot[i].apply(this, args);
    }

    return true;
  }

  async emitAsync(eventName, ...args) {
    const listeners = this._events.get(eventName);
    if (!listeners || listeners.length === 0) return [];

    const snapshot = [...listeners];
    return Promise.all(snapshot.map((fn) => Promise.resolve().then(() => fn.apply(this, args))));
  }

  removeAllListeners(eventName) {
    if (eventName) {
      this._events.delete(eventName);
    } else {
      this._events.clear();
    }
    return this;
  }

  listenerCount(eventName) {
    const listeners = this._events.get(eventName);
    return listeners ? listeners.length : 0;
  }
}
```

---

## 5. Practical Examples

### Example 1: Type-Safe Event Bus with TypeScript-Style Contracts
```javascript
import { EventEmitter } from './EventEmitter.js';

export class ApplicationBus extends EventEmitter {
  notifyUserLoggedIn(user) {
    this.emit('user:login', user);
  }

  notifyOrderPlaced(order) {
    this.emit('order:placed', order);
  }
}

const bus = new ApplicationBus();

bus.on('user:login', (user) => {
  console.log(`Analytics logged user login: ${user.id}`);
});

bus.once('user:login', (user) => {
  console.log(`Sending welcome email to ${user.email} (Once only)`);
});

bus.notifyUserLoggedIn({ id: 'u_101', email: 'user@example.com' });
bus.notifyUserLoggedIn({ id: 'u_101', email: 'user@example.com' }); // Second call does not send email
```

### Example 2: Wildcard Topic Hierarchy Event Bus
```javascript
import { EventEmitter } from './EventEmitter.js';

export class WildcardEventEmitter extends EventEmitter {
  emit(eventName, ...args) {
    super.emit(eventName, ...args);

    // Support wildcard matching: 'order.created' triggers 'order.*' and '*'
    const parts = eventName.split('.');
    if (parts.length > 1) {
      const wildcardNamespace = `${parts[0]}.*`;
      super.emit(wildcardNamespace, eventName, ...args);
    }

    super.emit('*', eventName, ...args);
  }
}

// Usage
const bus = new WildcardEventEmitter();
bus.on('order.*', (originalEvent, payload) => {
  console.log(`[Order Subsystem] Event: ${originalEvent}, Order ID: ${payload.orderId}`);
});

bus.emit('order.created', { orderId: 'ord_99' });
bus.emit('order.shipped', { orderId: 'ord_99' });
```

### Example 3: Asynchronous Sequential Pipeline with Interceptors
```javascript
export class AsyncPipelineEmitter {
  constructor() {
    this.middleware = [];
  }

  use(fn) {
    this.middleware.push(fn);
  }

  async execute(context) {
    for (const fn of this.middleware) {
      // Execute each async middleware sequentially, passing mutable context
      await fn(context);
    }
    return context;
  }
}

// Usage in request processing
const pipeline = new AsyncPipelineEmitter();
pipeline.use(async (ctx) => {
  ctx.startTime = Date.now();
});
pipeline.use(async (ctx) => {
  ctx.user = { id: 'admin' };
});

const result = await pipeline.execute({ url: '/api/v1/dashboard' });
console.log(result);
```

### Example 4: Unhandled Error Safety Barrier
```javascript
import { EventEmitter } from './EventEmitter.js';

const workerEmitter = new EventEmitter();

// Without this listener, workerEmitter.emit('error', err) will crash the entire Node process!
workerEmitter.on('error', (err) => {
  console.error(`[CRITICAL] Captured unhandled worker failure: ${err.message}`);
  // Log to external error monitor (Sentry / Datadog)
});

workerEmitter.emit('error', new Error('Database connection timed out'));
console.log('Application continues running safely.');
```

### Example 5: DOM Event Unbinding Lifecycle Cleanup
```javascript
import { EventEmitter } from './EventEmitter.js';

export class ResilientWebSocketClient extends EventEmitter {
  constructor(url) {
    super();
    this.url = url;
    this.ws = null;
  }

  connect() {
    this.ws = new WebSocket(this.url);

    this.ws.onopen = () => this.emit('connected');
    this.ws.onmessage = (e) => this.emit('message', JSON.parse(e.data));
    this.ws.onerror = (e) => this.emit('error', e);
    this.ws.onclose = () => this.emit('disconnected');
  }

  destroy() {
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
    this.removeAllListeners();
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Listener Mutation During Dispatch**:
   If listener 1 removes listener 2 using `emitter.off()`, a naive loop iterating directly over `this._events.get(event)` will skip a listener due to array index shifting. Always use a snapshot copy (`[...listeners]`).
2. **Missing `'error'` Listener Crash**:
   In standard EventEmitters, emitting an `'error'` event with no registered listeners throws an uncaught exception. In Node.js, this crashes the process immediately.
3. **Memory Leaks from Anonymous Closures**:
   ```javascript
   // MEMORY LEAK: Cannot be removed via emitter.off()
   emitter.on('tick', () => doWork());
   // Calling emitter.off('tick', () => doWork()) fails because the arrow function is a new reference!
   ```
   Always store a reference to the named listener function if you plan to deregister it later.
4. **Synchronous Execution Freezing the Loop**:
   By default, `emitter.emit()` is completely synchronous. If a registered listener performs an expensive synchronous calculation, it blocks the emitter and all other subscribers.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Risk | Production Fix |
| :--- | :--- | :--- |
| Unnamed arrow function listeners | Impossible to remove with `off()`, causing memory leaks. | Bind named methods or store listener references in variables. |
| Forgetting to listen to `'error'` | Unhandled error event crashes the entire Node.js server. | Always register at least one default `'error'` handler. |
| Mutating listener arrays during dispatch | Callbacks are skipped or invoked with wrong arguments. | Use copy-on-write (`[...listeners]`) before dispatching. |
| Silently raising `maxListeners` to 1000 | Masks legitimate memory leaks in components. | Investigate why listeners accumulate before increasing threshold. |
| Assuming `emit()` is asynchronous | Heavy listeners block the main thread and downstream code. | Use `emitAsync` or schedule listeners with `queueMicrotask`. |
| Cross-realm object references | Memory retained across unmounted component trees. | Call `removeAllListeners()` or `off()` during component teardown. |

---

## 8. Performance Characteristics

- **Invocation Latency**: Synchronous iteration over a pre-allocated array of listeners takes less than $10\text{ns}$ per listener in V8.
- **Copy-on-Write Allocation**: Creating a shallow copy via `[...listeners]` allocates a small array slice (~48 bytes), which is rapidly reclaimed in V8's young generation (nursery) garbage collector.
- **Set vs Array**: Using an `Array` preserves insertion order deterministically; using a `Set` offers $O(1)$ removal at the expense of slightly higher allocation overhead during iteration.

---

## 9. Engine Internals

### V8 Hidden Classes & Monomorphic Dispatch
- **Call-Site Monomorphism**: When an emitter iterates over listeners and invokes `listener(arg)`, V8's optimizing compiler (TurboFan) profiles the listener call site. If listeners have identical parameter signatures, TurboFan optimizes the loop into direct branch table dispatches.
- **Closure Retention**: Listeners capture their outer lexical scope. If a listener captures a large object or DOM element, that object cannot be freed as long as the listener resides in the emitter's `Map`.

---

## 10. Best Practices & Design Guidelines

1. **Always Implement Cleanup**: Pair every `emitter.on()` with a corresponding `emitter.off()` during teardown.
2. **Namespace Event Names**: Use colon or dot separators (`user:created`, `payment:success`) to prevent naming collisions.
3. **Respect Leak Warnings**: If you see `MaxListenersExceededWarning`, do not simply call `setMaxListeners(Infinity)`. Profile your code to verify that listeners are being removed on unmount.
4. **Isolate Failures**: Wrap listener invocations in `try...catch` blocks within `emit()` if you want one failing subscriber not to abort execution of subsequent subscribers.

---

## 11. Real-World Case Study

### Micro-Frontend Communication Bus
**Architecture**: An enterprise portal integrates three micro-frontend applications (React header, Angular product catalog, Vue shopping cart) on a single web page.
**Implementation**:
- The parent container injects a global `window.__appBus = new EventEmitter()` instance into each micro-frontend sandbox.
- When a user clicks "Add to Cart" in the Angular catalog, it emits `bus.emit('cart:item_added', item)`.
- The Vue shopping cart listens via `bus.on('cart:item_added', updateBadge)`.
- When the Angular catalog unmounts during route navigation, its destroy lifecycle calls `bus.off('cart:item_added', updateBadge)` to prevent memory leaks.

**Result**:
- Micro-frontends maintain zero code imports or dependencies on each other.
- Teams deploy, test, and release each application independently.

---

## 12. Interview Questions

<details>
<summary>1. Why is <code>emitter.emit()</code> synchronous by default rather than asynchronous?</summary>

Synchronous execution guarantees deterministic ordering: listeners execute in the exact order they were registered, and any state mutations performed by a listener are immediately visible to the code that called `emit()`. If `emit()` were asynchronous, race conditions could arise where subsequent operations execute before listeners have had a chance to update their state.
</details>

<details>
<summary>2. What is the copy-on-write pattern in EventEmitters, and what bug does it solve?</summary>

Copy-on-write takes a shallow copy of the listener array (`[...listeners]`) before starting the invocation loop. Without this copy, if a listener removes itself (`off()`) or registers a new listener (`on()`) during execution, the underlying array is modified while being iterated. This causes elements to shift index positions, leading to subsequent listeners being skipped or invoked multiple times.
</details>

<details>
<summary>3. Why does Node.js throw an uncaught exception if an <code>'error'</code> event has no listeners?</summary>

In Node.js, errors are treated as exceptional conditions requiring handling. If an error is emitted and no code is listening for it, swallowing it silently would leave the application in an indeterminate, corrupted state. To avoid silent failures, Node.js escalates unhandled `'error'` events to the process level as fatal uncaught exceptions.
</details>

<details>
<summary>4. What causes a <code>MaxListenersExceededWarning</code> and how should you address it?</summary>

By default, an emitter warns if more than 10 listeners are added to a single event. This threshold exists to alert developers to potential memory leaks—most commonly caused by repeatedly adding listeners inside loops, render functions, or reconnect cycles without removing previous ones. To address it, verify that cleanup logic (`off()`) is functioning correctly before raising `setMaxListeners()`.
</details>

<details>
<summary>5. How does <code>emitter.once()</code> ensure the callback runs only once while still allowing manual removal via <code>emitter.off()</code>?</summary>

`once()` creates an internal wrapper function that calls `this.off(event, wrapper)` and then invokes the original callback. To allow `emitter.off(event, originalCallback)` to work, the wrapper stores a reference to the original function on a property (e.g. `wrapper.originalListener = originalCallback`). The `off()` implementation checks both `l === listener || l.originalListener === listener`.
</details>

<details>
<summary>6. How can you implement an asynchronous EventEmitter where callers can await all listeners?</summary>

By adding an `emitAsync(event, ...args)` method that maps the listener array to promises using `Promise.resolve(fn.apply(this, args))` and awaits their settlement using `Promise.all()`. This allows all listeners to execute concurrently and returns an array of their resolved outputs.
</details>

<details>
<summary>7. What is the difference between the Observer pattern and the Publish-Subscribe pattern?</summary>

In the classic Observer pattern, the Subject maintains a direct reference to its observers and notifies them directly (tight coupling between subject and observer interfaces). In Publish-Subscribe, an intermediary Event Bus/Channel decouples publishers from subscribers completely: publishers do not know who is subscribed, and subscribers do not know who published the event.
</details>

<details>
<summary>8. Why is removing listeners by passing an inline arrow function ineffective?</summary>

`emitter.off('event', () => {})` compares functions by reference (`===`). An inline arrow function creates a new function object in memory on every invocation. Because the reference of the arrow function passed to `off()` does not match the reference passed to `on()`, the listener is never found or removed, causing a silent leak.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Implement a Basic EventEmitter
**Specification**: Implement a minimal `EventEmitter` with `on(event, fn)`, `emit(event, ...args)`, and `off(event, fn)`.

<details>
<summary>Solution</summary>

```javascript
export class SimpleEventEmitter {
  constructor() {
    this.events = {};
  }

  on(event, fn) {
    if (!this.events[event]) this.events[event] = [];
    this.events[event].push(fn);
  }

  emit(event, ...args) {
    const fns = this.events[event];
    if (fns) {
      const copy = [...fns];
      for (const fn of copy) fn(...args);
    }
  }

  off(event, fn) {
    const fns = this.events[event];
    if (fns) {
      this.events[event] = fns.filter((cb) => cb !== fn);
    }
  }
}
```
</details>

### Exercise 2: Add `once()` and Return Cleanup Function
**Specification**: Enhance `SimpleEventEmitter` so that `on(event, fn)` returns an unbind function `() => void`, and implement `once(event, fn)`.

<details>
<summary>Solution</summary>

```javascript
export class EnhancedEventEmitter {
  constructor() {
    this.events = new Map();
  }

  on(event, fn) {
    if (!this.events.has(event)) {
      this.events.set(event, []);
    }
    this.events.get(event).push(fn);

    // Return cleanup function
    return () => this.off(event, fn);
  }

  once(event, fn) {
    const unbind = this.on(event, (...args) => {
      unbind();
      fn(...args);
    });
    return unbind;
  }

  emit(event, ...args) {
    const listeners = this.events.get(event);
    if (listeners) {
      const copy = [...listeners];
      for (const listener of copy) listener(...args);
    }
  }

  off(event, fn) {
    const listeners = this.events.get(event);
    if (listeners) {
      const filtered = listeners.filter((cb) => cb !== fn);
      if (filtered.length > 0) {
        this.events.set(event, filtered);
      } else {
        this.events.delete(event);
      }
    }
  }
}
```
</details>
