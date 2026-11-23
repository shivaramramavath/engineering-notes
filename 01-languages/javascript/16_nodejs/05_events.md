# Event Emitter & Event-Driven Architecture

> Node's `node:events` module provides the core `EventEmitter` class, powering asynchronous event-driven coordination, streams, sockets, and inter-service messaging across the runtime.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [The Node.js Runtime](./01_node-runtime.md), [The Node.js Event Loop](./02_node-event-loop.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **Standard** | Node.js Core API (`node:events`) / Baseline Widely Available |

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

Much of the Node.js core API is architected around an idiomatic event-driven design: `net.Server` emits an event whenever a peer connects; `fs.ReadStream` emits an event when a chunk of file bytes is ready; HTTP request objects emit events when headers arrive.

The foundation of this architecture is the **`EventEmitter`** class from the `node:events` module. Unlike the browser's DOM `EventTarget` (which involves complex capturing, bubbling, and DOM tree propagation), `EventEmitter` is a lightweight, synchronous pub/sub dispatcher built for speed. It allows independent modules to communicate asynchronously without direct coupling, providing lifecycle hooks, error propagation boundaries, and streaming backpressure coordination.

---

## 2. Core Concepts

### Synchronous Listener Execution
A critical invariant of `EventEmitter` is that **listeners execute synchronously in the order they were registered**:
- When `emitter.emit('event')` is called, all attached listener functions are invoked immediately on the current call stack before `emit()` returns.
- If a listener must execute asynchronously, the developer must explicitly schedule it via `setImmediate()` or `process.nextTick()`.

```text
Caller invokes: emitter.emit('user:registered', user)
                      │
                      ▼
 [ Synchronous Execution on Current Call Stack ]
   ├── Listener 1: Send Welcome Email (Synchronous call)
   ├── Listener 2: Initialize Analytics Record
   └── Listener 3: Trigger Billing Check
                      │
                      ▼
 emitter.emit() returns true/false
```

### The Special `'error'` Event
The `'error'` event has special semantics in Node.js:
- If an `EventEmitter` emits `'error'` and **no listener is registered for `'error'`**, Node.js treats the unhandled error as a fatal exception.
- It logs the stack trace to `process.stderr` and crashes the entire Node process!
- Always attach an `.on('error', ...)` handler to every persistent `EventEmitter`.

### Capture Rejections (`events.captureRejections`)
Historically, if an `async` function passed to `.on('event', async () => ...)` rejected, the error became an unhandled promise rejection.
Since Node.js v13.4.0+, enabling `captureRejections: true` automatically intercepts promise rejections inside async listeners and routes them to the emitter's `'error'` handler!

---

## 3. How It Works Internally

### Listener Storage Structure

An `EventEmitter` stores its listeners in an internal dictionary (`this._events`):

```
 EventEmitter Instance
   │
   └── _events: Object (Prototype: null)
         ├── 'data': [ Function: onData1, Function: onData2 ]
         ├── 'error': Function: onError (Single listener stored directly without array!)
         └── 'close': [ Function: onClose ]
```

When an event has only 1 listener, Node stores the raw function reference directly to avoid allocating an Array object. When a 2nd listener is added, Node converts the property into an Array.

---

## 4. Syntax & API Reference

### Core Methods

```javascript
import { EventEmitter } from 'node:events';

const emitter = new EventEmitter({ captureRejections: true });

// 1. Registering listeners
emitter.on('data', (chunk) => console.log('Chunk:', chunk));
emitter.once('close', () => console.log('Closed once'));
emitter.prependListener('data', (c) => console.log('Runs first!'));

// 2. Emitting events (returns true if listeners existed, false otherwise)
const hadListeners = emitter.emit('data', 'sample_data');

// 3. Removing listeners
const callback = () => {};
emitter.on('event', callback);
emitter.removeListener('event', callback); // or emitter.off()
emitter.removeAllListeners('event');

// 4. Memory Leak Limits
emitter.setMaxListeners(20); // Default is 10
emitter.getMaxListeners();   // 20
```

### Static Utility Methods

```javascript
import { once, on } from 'node:events';

// 1. Convert single event to Promise
const [connection] = await once(server, 'connection');

// 2. Consume continuous events as an Async Iterable
for await (const [chunk] of on(readableStream, 'data')) {
  console.log('Stream chunk:', chunk);
}
```

---

## 5. Practical Examples

### Example 1: Domain Event Broker with Capture Rejections
```javascript
import { EventEmitter } from 'node:events';

class UserService extends EventEmitter {
  constructor() {
    super({ captureRejections: true }); // Catch async listener rejections!
  }

  async registerUser(email) {
    const user = { id: Date.now(), email };

    // Emit business domain event
    this.emit('user:registered', user);
    return user;
  }
}

const users = new UserService();

// Async listener that fails
users.on('user:registered', async (user) => {
  throw new Error(`Failed to send welcome email to ${user.email}`);
});

// Centralized error handler captures the rejected promise automatically!
users.on('error', (err) => {
  console.error('[Error Guard] Caught async listener failure:', err.message);
});

await users.registerUser('alice@example.com');
```

### Example 2: Promise-Based Waiting with `events.once()`
```javascript
import { once } from 'node:events';
import net from 'node:net';

async function connectToDatabase(port, host) {
  const socket = net.createConnection(port, host);

  // Await socket connection or error in a clean promise race!
  try {
    await once(socket, 'connect');
    console.log('Successfully connected to database socket');
    return socket;
  } catch (err) {
    socket.destroy();
    throw new Error(`Socket connection failed: ${err.message}`);
  }
}
```

### Example 3: Consuming Events via `for await...of` (`events.on`)
```javascript
import { on } from 'node:events';
import readline from 'node:readline';

async function processLogLines(stream) {
  const rl = readline.createInterface({ input: stream });

  // Stream each 'line' event asynchronously with natural backpressure
  for await (const [line] of on(rl, 'line')) {
    if (line.includes('FATAL')) {
      console.error('Fatal log entry detected:', line);
      break; // Automatically unregisters listener from readline interface!
    }
  }
}
```

### Example 4: Preventing Memory Leaks in Multi-Listener Architecture
```javascript
import { EventEmitter } from 'node:events';

class JobWorker extends EventEmitter {
  constructor() {
    super();
    // If a service legitimately attaches 50 listeners, explicitly raise the ceiling
    this.setMaxListeners(50);
  }
}

// Memory Leak Diagnostic Hook
process.on('warning', (warning) => {
  if (warning.name === 'MaxListenersExceededWarning') {
    console.error('POTENTIAL MEMORY LEAK DETECTED:', warning.message);
    console.error('Stack trace:', warning.stack);
  }
});
```

### Example 5: Custom Event Emitter with Prepend and Once Cleanup
```javascript
import { EventEmitter } from 'node:events';

class PaymentGateway extends EventEmitter {
  pay(amount) {
    this.emit('payment:start', amount);

    // Business payment processing...
    const success = true;

    if (success) {
      this.emit('payment:success', { amount, txId: 'tx_99' });
    } else {
      this.emit('error', new Error('Payment processing declined'));
    }
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Unhandled `'error'` Events Crash the Process**: Emitting `'error'` when no listener exists throws an uncatchable exception that crashes the Node.js process with exit code 1.
2. **Anonymous Listeners Cannot Be Removed**: Calling `emitter.off('event', () => {})` does nothing because function references do not match. Always retain named references if removal is required.
3. **The 10-Listener Memory Leak Warning**: By default, adding $> 10$ listeners to a single event type triggers `MaxListenersExceededWarning`. This is not a hard limit; it is a diagnostic safety guard designed to catch memory leaks caused by forgetting to detach listeners.
4. **Synchronous Execution Blocks the Emitter**: Because listeners run synchronously, if Listener #1 takes 2 seconds to compute, Listener #2 and any code following `emitter.emit()` are blocked for 2 seconds.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Omitting `'error'` Listener** | `emitter.emit('error', err)` without handler | Attach `.on('error', ...)` handler | Node crashes the entire process immediately. |
| **Silent Async Rejections** | `emitter.on('e', async () => throw err)` | Enable `captureRejections: true` | Async listener rejection bypasses standard error handlers. |
| **Ignoring MaxListeners Warnings** | Bumping `setMaxListeners(1000)` blindly | Fix leaking listener attachments | Masks real memory leaks where listeners accumulate on every request. |
| **Assuming Asynchronous Emit** | Expecting code after `emit()` to run first | Defer work using `process.nextTick` | `emit()` runs all attached callbacks synchronously. |
| **Anonymous Listener Leaks** | Adding `emitter.on('msg', () => ...)` in route | Detach on close or use `emitter.once` | Retains closures in memory on every incoming HTTP request. |
| **Mutating Listener Array during Emit**| Modifying listener list inside a listener | Handled safely by Node, but order shifts | Listeners added inside an active emit are deferred to next emit. |

---

## 8. Performance Characteristics

- **Direct In-Memory Dispatch**: Emitting an event with 1 listener takes $\approx 15\text{ns}$ (comparable to a direct JavaScript function call).
- **Single-Listener Optimization**: Node optimizes single-listener events by avoiding Array object allocations, reducing heap pressure in high-throughput streams.

---

## 9. Engine Internals

1. **Array Cloning During Emit**: When `emit()` is called, Node creates a shallow slice copy of the listeners array (`this._events[type].slice()`) before invoking them. This ensures that if a listener calls `removeListener()` or `on()`, the currently running dispatch iteration is not mutated mid-flight.
2. **`captureRejections` Symbol Hook**: When enabled, Node wraps listener execution in an internal promise check (`Promise.resolve(result).catch(...)`), redirecting rejections to `emitter[Symbol.for('nodejs.rejection')]()`.

---

## 10. Best Practices & Design Guidelines

1. **Always Register an `'error'` Handler**: Treat `.on('error', handler)` as mandatory for any `EventEmitter` instance.
2. **Enable `captureRejections: true` in Modern Classes**: Protect against unhandled promise rejections in async listeners.
3. **Use `events.once()` for Sequential Async Steps**: Simplify event waiting with `await once(emitter, 'ready')`.
4. **Investigate `MaxListenersExceededWarning` Immediately**: Never blindly raise limits without auditing why multiple listeners are attaching.

---

## 11. Real-World Case Study

### Resilient Order Processing Event Pipeline with Error Guard

An e-commerce order fulfillment service emits domain events across inventory, payment, and notification micro-subsystems. Async listener errors must be contained without crashing the server process.

```javascript
import { EventEmitter } from 'node:events';

class OrderPipeline extends EventEmitter {
  constructor() {
    super({ captureRejections: true });

    // Mandatory centralized error guard
    this.on('error', (err) => {
      console.error(`[OrderPipeline Error Guard] Caught failure:`, err.message);
      this.#logToTelemetry(err);
    });
  }

  async placeOrder(orderData) {
    console.log(`Processing order: ${orderData.id}`);

    // Synchronously dispatch to all listeners
    this.emit('order:created', orderData);
  }

  #logToTelemetry(err) {
    // Send to Datadog/Sentry
  }
}

// Subsystem 1: Inventory (Synchronous)
const pipeline = new OrderPipeline();
pipeline.on('order:created', (order) => {
  console.log(`[Inventory] Reserved stock for order ${order.id}`);
});

// Subsystem 2: Billing (Async - Fails intentionally)
pipeline.on('order:created', async (order) => {
  // Simulate network glitch during credit card charge
  throw new Error(`Payment Gateway unreachable for order ${order.id}`);
});

// Subsystem 3: Analytics (Async - Succeeds)
pipeline.on('order:created', async (order) => {
  console.log(`[Analytics] Tracked purchase for order ${order.id}`);
});

// Execution
await pipeline.placeOrder({ id: 'ORD-9823', amount: 150 });
// Output:
// Processing order: ORD-9823
// [Inventory] Reserved stock for order ORD-9823
// [Analytics] Tracked purchase for order ORD-9823
// [OrderPipeline Error Guard] Caught failure: Payment Gateway unreachable for order ORD-9823
// Node process does NOT crash! Error was safely intercepted!
```

---

## 12. Interview Questions

<details>
<summary><strong>1. Is listener execution in an <code>EventEmitter</code> synchronous or asynchronous?</strong></summary>

It is **synchronous**. When `emitter.emit('event')` is called, all attached listeners are invoked immediately on the current call stack in the order they were registered, before `emit()` returns.
</details>

<details>
<summary><strong>2. What happens if an <code>EventEmitter</code> emits an <code>'error'</code> event with no listeners attached?</strong></summary>

Node.js treats unhandled `'error'` events as fatal exceptions. It prints the error stack trace to `process.stderr` and immediately terminates the Node process with a non-zero exit code.
</details>

<details>
<summary><strong>3. What causes a <code>MaxListenersExceededWarning</code> and how should it be addressed?</strong></summary>

It occurs when more than 10 listeners (the default limit) are attached to a single event type on an emitter. It is designed to catch memory leaks caused by repeatedly adding listeners inside recurring loops or request handlers without detaching them. If the high number of listeners is intentional, it can be raised via `emitter.setMaxListeners(n)`.
</details>

<details>
<summary><strong>4. What does the <code>captureRejections: true</code> option do?</strong></summary>

Introduced in Node.js v13.4.0+, it automatically catches rejected promises from `async` event listeners and routes them directly to the emitter's `'error'` event handler, preventing unhandled promise rejections.
</details>

<details>
<summary><strong>5. How does <code>events.once(emitter, eventName)</code> work?</strong></summary>

It returns a Promise that resolves with an array of the arguments emitted by the event when it first fires, or rejects if an `'error'` event is emitted first.
</details>

<details>
<summary><strong>6. What is the difference between <code>emitter.on()</code> and <code>emitter.prependListener()</code>?</strong></summary>

`emitter.on()` appends the listener to the end of the internal listeners array, executing it after all previously registered listeners. `emitter.prependListener()` adds the listener to the beginning of the array, ensuring it executes first.
</details>

<details>
<summary><strong>7. How does Node optimize memory when an event has only one listener?</strong></summary>

When an event has only one listener, Node stores the function reference directly on `_events[type]` instead of allocating an Array. It only allocates an Array if a second listener is attached.
</details>

<details>
<summary><strong>8. What is the return value of <code>emitter.emit()</code>?</strong></summary>

It returns `true` if there were listeners registered for that event name, and `false` if there were no listeners.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement an Async Priority Event Emitter
Create an `EventEmitter` subclass `PriorityEmitter` that allows registering listeners with a numeric priority (higher numbers execute first) and executes them in prioritized order.

<details>
<summary>View Solution</summary>

```javascript
import { EventEmitter } from 'node:events';

export class PriorityEmitter extends EventEmitter {
  onPriority(event, listener, priority = 0) {
    listener._priority = priority;
    super.on(event, listener);

    // Re-sort listeners array by priority descending
    const listeners = this.rawListeners(event);
    listeners.sort((a, b) => (b._priority || 0) - (a._priority || 0));

    // Reset internal array
    this.removeAllListeners(event);
    listeners.forEach(fn => super.on(event, fn));
  }
}

// Verification:
const pe = new PriorityEmitter();
pe.onPriority('data', () => console.log('Low Priority'), 10);
pe.onPriority('data', () => console.log('High Priority'), 100);
pe.emit('data');
// Output: "High Priority", then "Low Priority"
```
</details>

### Challenge 2: Implement an Event-Driven Timeout Guard
Write a function `waitForEventWithTimeout(emitter, event, timeoutMs)` that returns a Promise that resolves when the event fires or rejects with a `TimeoutError` if `timeoutMs` elapses first, cleanly removing all listeners in both cases.

<details>
<summary>View Solution</summary>

```javascript
import { once } from 'node:events';

export async function waitForEventWithTimeout(emitter, event, timeoutMs) {
  const ac = new AbortController();
  const timer = setTimeout(() => {
    ac.abort(new Error(`Timeout: Event "${event}" did not fire within ${timeoutMs}ms`));
  }, timeoutMs);

  try {
    const result = await once(emitter, event, { signal: ac.signal });
    return result;
  } finally {
    clearTimeout(timer);
  }
}
```
</details>
