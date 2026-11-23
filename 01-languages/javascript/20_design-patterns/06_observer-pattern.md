# The Observer and Publish-Subscribe Patterns in JavaScript

> Subject-Observer direct coupling vs event brokers, synchronous vs asynchronous dispatch, unsubscription teardown, and modern Observable models.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [Events](file:///e:/Learning/engineering-notes/01-languages/javascript/14_dom-and-browser/04_events.md), [Node Events](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs/05_events.md) |
| **Time to read** | 24 minutes |
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

In modern software applications, components rarely operate in isolation. When a user updates their profile picture, multiple subsystems must react simultaneously: the UI navigation bar must update its avatar, the local storage cache must refresh, analytics telemetry must log the event, and background sync must notify the server.

Hardcoding direct method calls from the profile component into every dependent service introduces rigid coupling and makes modular testing impossible.

The **Observer Pattern** (and its decoupled sibling, the **Publish-Subscribe Pattern**) establishes a one-to-many dependency between objects. When the primary object (the **Subject** or **Publisher**) undergoes a state change, all registered dependents (**Observers** or **Subscribers**) are notified and updated automatically.

---

## 2. Core Concepts

### Observer vs Publish-Subscribe: The Crucial Distinction
Although frequently used interchangeably, the patterns differ architecturally:

| Feature | Observer Pattern | Publish-Subscribe (Pub-Sub) |
| :--- | :--- | :--- |
| **Coupling** | Subject maintains a direct reference list of Observers | Publishers and Subscribers share zero knowledge of each other |
| **Broker** | No broker; Subject calls Observers directly | Relies on an intermediary Event Broker / Message Bus |
| **Execution** | Typically synchronous | Can be synchronous or asynchronous (microtasks/queues) |
| **Scoping** | Scoped to individual Subject instances | Application-wide or topic-based channels |

### The Unsubscription Contract
Every robust observer implementation must return a clean unsubscription mechanism (such as an `unsubscribe()` function or `Subscription` object). Failing to remove observers when components unmount causes the notorious **Lapsed Listener Memory Leak**, where the Subject keeps dead components alive in memory.

---

## 3. How It Works Internally

```
OBSERVER PATTERN (Direct Coupling):
+------------------------------------+
| SUBJECT (Observable)               |
| - observers: Set<Observer>         |
| - notify(data) {                   |
|     observers.forEach(o => o.update(data)) |
|   }                                |
+------------------+-----------------+
                   |
         +---------+---------+
         |                   |
         v                   v
+------------------+ +------------------+
| Observer 1       | | Observer 2       |
| - update(data)   | | - update(data)   |
+------------------+ +------------------+

PUBLISH-SUBSCRIBE PATTERN (Decoupled via Broker):
+-------------+                     +-------------+
| PUBLISHER   |                     | SUBSCRIBER  |
| emit('foo') |                     | on('foo')   |
+------+------+                     +------+------+
       |                                   ^
       | publish()                         | notify()
       v                                   |
+-------------------------------------------------+
| EVENT BROKER / MESSAGE BUS (Mediator)           |
| Topics: Map<EventName, Set<HandlerFunction>>    |
+-------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Classic Subject-Observer Implementation
```javascript
export class Subject {
  constructor() {
    this.observers = new Set();
  }

  subscribe(observer) {
    if (typeof observer?.update !== 'function') {
      throw new TypeError('Observer must implement an update() method');
    }
    this.observers.add(observer);

    // Return unsubscription teardown
    return () => this.observers.delete(observer);
  }

  notify(data) {
    for (const observer of this.observers) {
      observer.update(data);
    }
  }
}
```

### Modern Functional Pub-Sub Bus
```javascript
export class EventBus {
  #channels = new Map();

  on(event, handler) {
    if (!this.#channels.has(event)) {
      this.#channels.set(event, new Set());
    }
    this.#channels.get(event).add(handler);
    return () => this.off(event, handler);
  }

  off(event, handler) {
    const handlers = this.#channels.get(event);
    if (handlers) {
      handlers.delete(handler);
      if (handlers.size === 0) this.#channels.delete(event);
    }
  }

  emit(event, payload) {
    const handlers = this.#channels.get(event);
    if (handlers) {
      // Defensive snapshot prevents mutation issues during execution
      Array.from(handlers).forEach(fn => fn(payload));
    }
  }
}
```

---

## 5. Practical Examples

### Example 1: Reactive State Store (Redux-Lite Model)
```javascript
export function createReactiveStore(reducer, initialState = {}) {
  let state = initialState;
  const subscribers = new Set();

  function getState() {
    return state;
  }

  function dispatch(action) {
    state = reducer(state, action);
    // Notify all observers synchronously
    for (const sub of subscribers) {
      sub(state);
    }
  }

  function subscribe(listener) {
    subscribers.add(listener);
    return () => subscribers.delete(listener);
  }

  return { getState, dispatch, subscribe };
}
```

### Example 2: DOM-Native EventTarget Implementation
```javascript
// Modern browsers and Node.js v16+ provide native EventTarget
export class DownloadTracker extends EventTarget {
  #progress = 0;

  updateProgress(percent) {
    this.#progress = percent;
    this.dispatchEvent(new CustomEvent('progress', {
      detail: { percent: this.#progress, timestamp: Date.now() },
    }));

    if (this.#progress >= 100) {
      this.dispatchEvent(new CustomEvent('complete'));
    }
  }
}

// Client usage
const tracker = new DownloadTracker();
tracker.addEventListener('progress', (e) => {
  console.log(`Progress: ${e.detail.percent}%`);
});
```

### Example 3: Asynchronous Pub-Sub with Isolated Error Handling
```javascript
export class AsyncEventBus {
  #listeners = new Map();

  subscribe(topic, asyncCallback) {
    if (!this.#listeners.has(topic)) this.#listeners.set(topic, new Set());
    this.#listeners.get(topic).add(asyncCallback);
    return () => this.#listeners.get(topic)?.delete(asyncCallback);
  }

  async publish(topic, data) {
    const list = this.#listeners.get(topic);
    if (!list) return;

    // Run all subscribers in parallel, capturing errors without aborting others
    const promises = Array.from(list).map(async (fn) => {
      try {
        await fn(data);
      } catch (err) {
        console.error(`Error in subscriber on [${topic}]:`, err);
      }
    });

    await Promise.all(promises);
  }
}
```

### Example 4: Minimal RxJS-Style Observable Implementation
```javascript
export class SimpleObservable {
  constructor(subscribeFn) {
    this._subscribe = subscribeFn;
  }

  subscribe(observerOrNext) {
    const observer = typeof observerOrNext === 'function'
      ? { next: observerOrNext, error: (e) => console.error(e), complete: () => {} }
      : observerOrNext;

    return this._subscribe(observer);
  }

  pipe(transformFn) {
    return new SimpleObservable((observer) => {
      return this.subscribe({
        next: (val) => observer.next(transformFn(val)),
        error: (err) => observer.error(err),
        complete: () => observer.complete(),
      });
    });
  }
}

// Usage
const numberStream = new SimpleObservable((observer) => {
  let count = 0;
  const interval = setInterval(() => observer.next(count++), 500);
  return () => clearInterval(interval); // Teardown logic
});

const doubledStream = numberStream.pipe(x => x * 2);
const unsubscribe = doubledStream.subscribe((val) => console.log('Doubled:', val));
```

### Example 5: One-Time Observer (`once` listener)
```javascript
export class OnceEventBus {
  #bus = new EventBus();

  once(event, callback) {
    const unsubscribe = this.#bus.on(event, (data) => {
      unsubscribe(); // Auto-unsubscribe upon first invocation
      callback(data);
    });
    return unsubscribe;
  }

  emit(event, data) {
    this.#bus.emit(event, data);
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Mutation During Notification**: If an observer calls `unsubscribe()` or adds a new subscriber *while* the Subject is looping through its observer set, standard `for...of` loops can skip observers or enter infinite loops. Always iterate over a shallow copy (e.g. `Array.from(this.observers)`).
2. **Synchronous Exceptions Blocking Other Observers**: If Observer 1 throws an unhandled error inside its callback, execution halts immediately and Observers 2 through 10 never receive the notification. Wrap individual observer notifications in `try...catch`.
3. **Memory Leaks via Anonymous Functions**: Calling `bus.on('event', () => doSomething())` prevents subsequent unsubscription if the unsubscription function is not saved. The Subject keeps the callback closure alive forever.
4. **Ordering Guarantees**: Standard JavaScript `Set` guarantees insertion-order iteration. However, client code should never rely on one observer executing strictly before another.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Forgetting to unsubscribe on unmount | Lapsed listener memory leak; dead components retained in RAM | Store unsubscription teardown and call on destroy |
| Unhandled exception in one subscriber | Stops subsequent subscribers from executing | Wrap subscriber notifications in `try...catch` |
| Mutating subscriber set during active dispatch | Skips subscribers or causes concurrent modification bugs | Snapshot subscribers via `Array.from()` before looping |
| Passing internal mutable objects to subscribers | Subscribers can mutate publisher's internal state | Pass frozen objects or shallow clones |
| Using Pub-Sub for simple 1-to-1 method delegation | Unnecessary indirection makes control flow hard to trace | Use direct function injection or callbacks |

---

## 8. Performance Characteristics

- **Notification Overhead**: Iterating over 1,000 subscribers in a `Set` and calling lightweight functions takes $< 0.1$ms in V8.
- **Memory Footprint**: A `Set` holding 100 function pointers consumes $< 4$ KB of heap memory.

---

## 9. Engine Internals

1. **V8 Set Iteration Performance**:
   - `Set.prototype.forEach` and `for...of` on Sets are heavily optimized in TurboFan.
   - V8 allocates an internal `OrderedHashSet` backing store that allows insertion-ordered traversal without allocating intermediate array buffers.

---

## 10. Best Practices & Design Guidelines

- **Always Return Teardown Functions**: Make unsubscription effortless by returning a cleanup function directly from `.subscribe()` / `.on()`.
- **Isolate Errors**: Ensure that a crash in an analytics subscriber does not break a payment completion subscriber.
- **Support `AbortSignal`**: Allow modern callers to pass `{ signal: controller.signal }` to automatically deregister subscribers.

---

## 11. Real-World Case Study

### Production WebSocket Broadcast Channel
```javascript
export class LivePriceFeed {
  #subscribers = new Map(); // symbol -> Set<callback>
  #socket = null;

  constructor(wsUrl) {
    this.wsUrl = wsUrl;
    this.#initSocket();
  }

  #initSocket() {
    this.#socket = new WebSocket(this.wsUrl);
    this.#socket.onmessage = (event) => {
      const { symbol, price, timestamp } = JSON.parse(event.data);
      this.#notifySubscribers(symbol, { price, timestamp });
    };
  }

  subscribe(symbol, callback) {
    if (!this.#subscribers.has(symbol)) {
      this.#subscribers.set(symbol, new Set());
    }
    this.#subscribers.get(symbol).add(callback);

    // Return unsubscription function
    return () => {
      const set = this.#subscribers.get(symbol);
      if (set) {
        set.delete(callback);
        if (set.size === 0) this.#subscribers.delete(symbol);
      }
    };
  }

  #notifySubscribers(symbol, data) {
    const listeners = this.#subscribers.get(symbol);
    if (!listeners) return;

    for (const fn of Array.from(listeners)) {
      try {
        fn(data);
      } catch (err) {
        console.error(`Error notifying subscriber for ${symbol}:`, err);
      }
    }
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the key difference between the Observer Pattern and the Publish-Subscribe Pattern?</summary>
In the <b>Observer Pattern</b>, the Subject maintains a direct reference to its observers and notifies them directly. In the <b>Publish-Subscribe Pattern</b>, publishers and subscribers are completely decoupled and share no knowledge of each other; they communicate exclusively through an intermediary message broker or event bus.
</details>

<details>
<summary>2. What is a "Lapsed Listener" memory leak?</summary>
A lapsed listener leak occurs when an observer subscribes to a long-lived subject (such as a global event bus or window object) but fails to unsubscribe when the observer's lifecycle ends. Because the subject maintains a strong reference to the observer's callback closure, the garbage collector cannot reclaim the observer or any variables captured in its lexical scope.
</details>

<details>
<summary>3. Why should you iterate over a shallow copy of subscribers during dispatch?</summary>
Because a subscriber might call <code>unsubscribe()</code> or register a new subscriber during the execution of its callback. Modifying the subscriber collection while actively iterating over it can cause skipped notifications, duplicate notifications, or runtime iteration errors.
</details>

<details>
<summary>4. What happens if an observer throws a synchronous error during notification?</summary>
If the loop in the Subject's <code>notify()</code> method does not wrap subscriber invocations in a <code>try...catch</code> block, the thrown exception will terminate the loop, preventing all subsequent registered observers from receiving the event.
</details>

<details>
<summary>5. How does EventTarget in modern JavaScript compare to a custom Subject class?</summary>
<code>EventTarget</code> is the native standard interface implemented by DOM elements, <code>window</code>, and modern Node.js environments. It supports event bubbling, capturing, <code>preventDefault()</code>, <code>stopPropagation()</code>, and cancellation via <code>AbortSignal</code>, whereas custom Subject classes are lightweight and simpler.
</details>

<details>
<summary>6. How can AbortController be integrated into an Observer pattern?</summary>
By accepting an optional <code>{ signal }</code> option in the <code>subscribe()</code> method. The subject listens for <code>signal.addEventListener('abort', ...)</code> and automatically removes the subscriber when the controller is aborted.
</details>

<details>
<summary>7. What is the difference between push-based and pull-based observer updates?</summary>
In a <b>push-based</b> model, the Subject passes detailed state information directly as arguments to the observer's update method. In a <b>pull-based</b> model, the Subject simply notifies observers that a change occurred, and the observers query the Subject for the specific data they need.
</details>

<details>
<summary>8. How do RxJS Observables differ from standard EventEmitters?</summary>
EventEmitters are multi-cast and push events continuously regardless of whether anyone is listening. RxJS Observables are cold by default (they do not produce values until subscribed to), can represent single or multiple values over time, and provide functional pipeline operators (like <code>map</code>, <code>filter</code>, <code>switchMap</code>).
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a Wildcard Event Bus
Implement an `EventBus` that supports subscribing to wildcard topic patterns (e.g. `bus.on('user:*', handler)` matches `'user:login'` and `'user:logout'`).

<details>
<summary>View Solution</summary>

```javascript
export class WildcardEventBus {
  constructor() {
    this.exact = new Map();
    this.wildcards = [];
  }

  on(pattern, callback) {
    if (pattern.includes('*')) {
      const regex = new RegExp(`^${pattern.replace('*', '.*')}$`);
      const entry = { pattern, regex, callback };
      this.wildcards.push(entry);
      return () => {
        const idx = this.wildcards.indexOf(entry);
        if (idx !== -1) this.wildcards.splice(idx, 1);
      };
    }

    if (!this.exact.has(pattern)) this.exact.set(pattern, new Set());
    this.exact.get(pattern).add(callback);
    return () => this.exact.get(pattern)?.delete(callback);
  }

  emit(event, data) {
    // 1. Exact matches
    if (this.exact.has(event)) {
      this.exact.get(event).forEach(fn => fn(data, event));
    }

    // 2. Wildcard matches
    for (const { regex, callback } of this.wildcards) {
      if (regex.test(event)) {
        callback(data, event);
      }
    }
  }
}
```
</details>

### Exercise 2: State Change History Tracker Observer
Create an observable `HistorySubject` that retains the last 5 emitted states, allowing newly registered observers to optionally receive the historical states upon subscription.

<details>
<summary>View Solution</summary>

```javascript
export class HistorySubject {
  constructor(bufferSize = 5) {
    this.bufferSize = bufferSize;
    this.history = [];
    this.subscribers = new Set();
  }

  emit(value) {
    this.history.push(value);
    if (this.history.length > this.bufferSize) {
      this.history.shift();
    }
    for (const sub of Array.from(this.subscribers)) {
      sub(value);
    }
  }

  subscribe(callback, replayHistory = false) {
    if (replayHistory) {
      this.history.forEach(item => callback(item));
    }
    this.subscribers.add(callback);
    return () => this.subscribers.delete(callback);
  }
}
```
</details>
