# Web Workers in the Browser

> Multi-threaded browser execution, Dedicated and Shared Workers, offscreen graphics rendering, structured cloning, and zero-copy transfers.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Concurrency vs Parallelism](file:///e:/Learning/engineering-notes/01-languages/javascript/17_concurrency-and-parallelism/01_concurrency-vs-parallelism.md), [Event Loop](file:///e:/Learning/engineering-notes/01-languages/javascript/12_event-loop/01_event-loop.md) |
| **Time to read** | 24 minutes |
| **Status** | Complete |
| **Standard / Runtime** | HTML5 Web Workers Living Standard, Evergreen Browsers |

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

The browser's main execution thread is shared between JavaScript execution, layout calculation, style recalibration, and paint rendering (the 60 FPS / 120 FPS render pipeline). When a JavaScript task executes for longer than 16.6 milliseconds, frame budgets are violated, resulting in dropped frames, unresponsive user input, and sluggish user interfaces (jank).

**Web Workers** provide true background OS thread execution inside modern browsers. By delegating computationally expensive tasks—such as image processing, audio DSP, CSV/JSON parsing, cryptography, and physical simulation—to background workers, the main thread remains exclusively dedicated to user interactions and high-performance UI rendering.

---

## 2. Core Concepts

### Types of Web Workers
1. **Dedicated Workers (`Worker`)**: Owned exclusively by the script instance that created them. Terminated when the parent page or tab is closed.
2. **Shared Workers (`SharedWorker`)**: Accessible from multiple browsing contexts (multiple tabs, iframes, or windows belonging to the exact same origin). Ideal for cross-tab state synchronization, shared WebSockets, or background data stores.
3. **Service Workers**: Event-driven network proxies that intercept HTTP requests, manage offline caches, and handle push notifications.

### Execution Scope and Security Restrictions
Web Workers run inside a `DedicatedWorkerGlobalScope` (accessible as `self` or `globalThis`), which strictly prohibits access to:
- The DOM (`document`, `window`, direct element nodes).
- Certain `window` APIs (`localStorage`, `sessionStorage`, `alert`, `prompt`).

However, workers maintain full access to:
- `fetch()` and `XMLHttpRequest`
- `IndexedDB` and Cache Storage
- `WebSockets` and `WebRTC`
- WebAssembly (`WebAssembly.instantiate`)
- `OffscreenCanvas` for off-thread GPU rendering
- Timers (`setTimeout`, `setInterval`) and `crypto.subtle`

---

## 3. How It Works Internally

```
+-------------------------------------------------------------------------+
| MAIN BROWSER THREAD                                                     |
| - DOM Tree, CSSOM, Style Calculation, Layout, Paint                     |
| - Event Loop: User clicks, Keyboard, RAF                                |
|                                                                         |
| const worker = new Worker('worker.js', { type: 'module' });             |
| worker.postMessage(data, [transferList]);                               |
+-------------------------------------------------------------------------+
                                 |
        Structured Clone         | Zero-Copy Transfer
        (Deep copy in heap)      | (Detach & Move BackingStore pointer)
                                 v
+-------------------------------------------------------------------------+
| BACKGROUND WORKER THREAD                                                |
| - DedicatedWorkerGlobalScope (self)                                     |
| - Independent V8 Isolate & Heap                                         |
| - Independent Event Loop & Microtask Queue                              |
|                                                                         |
| self.onmessage = (e) => {                                               |
|   const result = executeHeavyTask(e.data);                              |
|   self.postMessage(result);                                             |
| };                                                                      |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Instantiating a Dedicated Worker
```javascript
// Classic script vs ES Module worker
const worker = new Worker('/workers/analyzer.js', {
  type: 'module', // Enables import/export syntax inside the worker
  name: 'data-analyzer', // Identifies worker in browser DevTools
});

// Outgoing communication
worker.postMessage({ action: 'PROCESS', data: payload });

// Incoming communication
worker.onmessage = (event) => {
  console.log('Result from worker:', event.data);
};

// Error handling
worker.onerror = (errorEvent) => {
  console.error('Worker error:', errorEvent.message, errorEvent.filename, errorEvent.lineno);
};

// Termination
worker.terminate(); // Abruptly stops background thread
```

### Worker Global Scope (`self`)
```javascript
// Inside worker.js
self.onmessage = (event) => {
  const { action, data } = event.data;
  if (action === 'PROCESS') {
    const result = heavyCalculation(data);
    self.postMessage(result);
  }
};

// Terminating from within
self.close();
```

---

## 5. Practical Examples

### Example 1: Offloading Heavy CSV/JSON Parsing
```javascript
// worker.js
self.onmessage = (e) => {
  const rawCsv = e.data;
  const lines = rawCsv.split('\n');
  const headers = lines[0].split(',');
  const records = [];

  for (let i = 1; i < lines.length; i++) {
    const line = lines[i].trim();
    if (!line) continue;
    const values = line.split(',');
    const record = {};
    for (let j = 0; j < headers.length; j++) {
      record[headers[j]] = values[j];
    }
    records.push(record);
  }

  self.postMessage({ count: records.length, records });
};

// main.js
const worker = new Worker(new URL('./csv-worker.js', import.meta.url), { type: 'module' });

function parseLargeCsvAsync(csvText) {
  return new Promise((resolve, reject) => {
    worker.onmessage = (e) => resolve(e.data);
    worker.onerror = reject;
    worker.postMessage(csvText);
  });
}
```

### Example 2: Parallel Image Filtering with Transferable `ImageData`
```javascript
// main.js
const canvas = document.getElementById('myCanvas');
const ctx = canvas.getContext('2d');
const imgData = ctx.getImageData(0, 0, canvas.width, canvas.height);

const filterWorker = new Worker(new URL('./filter-worker.js', import.meta.url), { type: 'module' });

// Transfer the underlying ArrayBuffer without copying
filterWorker.postMessage(
  { width: imgData.width, height: imgData.height, buffer: imgData.data.buffer },
  [imgData.data.buffer] // Ownership transferred!
);

filterWorker.onmessage = (e) => {
  const { width, height, buffer } = e.data;
  const processedData = new ImageData(new Uint8ClampedArray(buffer), width, height);
  ctx.putImageData(processedData, 0, 0);
};

// filter-worker.js
self.onmessage = (e) => {
  const { width, height, buffer } = e.data;
  const pixels = new Uint8ClampedArray(buffer);

  // Invert grayscale filter
  for (let i = 0; i < pixels.length; i += 4) {
    const avg = (pixels[i] + pixels[i + 1] + pixels[i + 2]) / 3;
    pixels[i] = 255 - avg;     // Red
    pixels[i + 1] = 255 - avg; // Green
    pixels[i + 2] = 255 - avg; // Blue
  }

  // Transfer back to main thread
  self.postMessage({ width, height, buffer }, [buffer]);
};
```

### Example 3: Multi-Tab Communication with `SharedWorker`
```javascript
// shared-worker.js
const connections = new Set();
let sharedState = 0;

self.onconnect = (event) => {
  const port = event.ports[0];
  connections.add(port);

  port.onmessage = (e) => {
    if (e.data.type === 'INCREMENT') {
      sharedState++;
      // Broadcast new state to all connected tabs
      for (const client of connections) {
        client.postMessage({ type: 'UPDATE', value: sharedState });
      }
    }
  };

  // Send current state upon new tab connection
  port.postMessage({ type: 'UPDATE', value: sharedState });
};

// tab.js (in any open browser tab on the same origin)
const shared = new SharedWorker('/workers/counter-shared.js');
shared.port.start();

shared.port.onmessage = (e) => {
  console.log('Shared state across tabs:', e.data.value);
};

function incrementAcrossAllTabs() {
  shared.port.postMessage({ type: 'INCREMENT' });
}
```

### Example 4: Rendering High-FPS Animations via `OffscreenCanvas`
```javascript
// main.js
const canvas = document.querySelector('#renderTarget');
const offscreen = canvas.transferControlToOffscreen();

const renderWorker = new Worker(new URL('./canvas-worker.js', import.meta.url), { type: 'module' });
renderWorker.postMessage({ canvas: offscreen }, [offscreen]);

// canvas-worker.js
self.onmessage = (e) => {
  const { canvas } = e.data;
  const ctx = canvas.getContext('2d');
  let x = 0;

  function renderFrame() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.fillStyle = '#4f46e5';
    ctx.fillRect(x % canvas.width, 50, 40, 40);
    x += 2;
    requestAnimationFrame(renderFrame); // Workers have their own RAF!
  }

  renderFrame();
};
```

### Example 5: Inline Web Worker via Blob URL (Zero-Build Setup)
```javascript
function createInlineWorker(workerFunction) {
  const code = `(${workerFunction.toString()})();`;
  const blob = new Blob([code], { type: 'application/javascript' });
  const url = URL.createObjectURL(blob);
  const worker = new Worker(url);

  // Revoke object URL after worker starts to free memory
  URL.revokeObjectURL(url);
  return worker;
}

// Usage
const inlineWorker = createInlineWorker(() => {
  self.onmessage = (e) => {
    const sum = e.data.reduce((acc, n) => acc + n, 0);
    self.postMessage(sum);
  };
});

inlineWorker.postMessage([10, 20, 30, 40]);
inlineWorker.onmessage = (e) => console.log('Sum from inline worker:', e.data);
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Same-Origin Security Policy**: Loading a worker script from a Cross-Origin CDN (e.g., `https://cdn.example.com/worker.js`) throws a `SecurityError`. Workaround: Fetch the code via CORS, create a `Blob`, and pass `URL.createObjectURL(blob)`.
2. **Accessing Transferred Buffers**: Attempting to read `imgData.data.buffer` after transferring it throws `TypeError: Cannot perform operation on a detached ArrayBuffer`.
3. **Worker Memory Leaks**: Dedicated workers do not get garbage collected if references are kept on the main thread or if the worker is executing an infinite loop. Always call `worker.terminate()` or `self.close()` when tasks finish.
4. **`SharedWorker` Port Starting**: In `SharedWorker`, if you use `port.addEventListener('message', ...)` instead of `port.onmessage = ...`, you **must** explicitly call `port.start()`, otherwise incoming messages are buffered silently and never delivered.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Passing functions or DOM elements via `postMessage` | Fails with `DOMException: could not be cloned` | Send pure serializable state or action identifiers |
| Creating a worker per user keystroke | Creates hundreds of OS threads, crashing browser tab | Reuse a singleton worker or debounce inputs |
| Forgetting `port.start()` with `addEventListener` | Messages are silently buffered and never received | Call `sharedWorker.port.start()` |
| Polling workers with rapid `postMessage` calls | Floods the worker's message queue, introducing latency | Use batching or `SharedArrayBuffer` with Atomics |
| Omitting `worker.onerror` | Unhandled exceptions in worker fail silently without logs | Always attach an `onerror` listener |
| Expecting `localStorage` inside worker scope | `localStorage` is synchronous and prohibited in workers | Use asynchronous `IndexedDB` in worker scopes |

---

## 8. Performance Characteristics

- **Worker Instantiation Cost**: Instantiating a Web Worker typically requires 10–30ms of thread creation time and 2–5MB of heap allocation.
- **Transferable Objects Performance**: Transferring a 100MB `ArrayBuffer` takes $< 0.1$ms ($O(1)$ pointer transfer), whereas structured cloning requires $\approx 50$ms.
- **Rendering Throughput with `OffscreenCanvas`**: Decouples GPU draw calls from main-thread layout thrashing, maintaining 60/120 FPS even when the UI thread experiences garbage collection pauses.

---

## 9. Engine Internals

1. **V8 Isolate Separation**: Each Web Worker runs in its own native POSIX/Windows thread with its own independent V8 Isolate. They share no pointer addresses, eliminating thread data corruption without requiring global interpreter locks (GIL).
2. **Structured Clone Implementation**: Handled by V8's `ValueSerializer` and `ValueDeserializer`. It traverses the object graph, identifies cyclic references via an internal identity hash map, and encodes data into a packed binary stream.

---

## 10. Best Practices & Design Guidelines

- **RPC Pattern Abstraction**: Wrap raw `postMessage` calls in a promise-based RPC abstraction (e.g., matching request/response correlation IDs) to prevent callback spaghetti.
- **Resource Cleanup**: Always ensure workers call `self.close()` or the main thread calls `worker.terminate()` upon unmounting single-page application (SPA) components.
- **Modular Workers**: Use `{ type: 'module' }` to allow workers to use standard ES imports (`import { calculate } from './math.js'`).

---

## 11. Real-World Case Study

### Promise-Based Worker RPC Client
```javascript
class WorkerRPC {
  constructor(workerUrl) {
    this.worker = new Worker(workerUrl, { type: 'module' });
    this.pending = new Map();

    this.worker.onmessage = (event) => {
      const { id, result, error } = event.data;
      const deferred = this.pending.get(id);
      if (deferred) {
        this.pending.delete(id);
        if (error) deferred.reject(new Error(error));
        else deferred.resolve(result);
      }
    };

    this.worker.onerror = (err) => {
      console.error('Fatal Worker Error:', err);
    };
  }

  invoke(method, params = [], transferList = []) {
    return new Promise((resolve, reject) => {
      const id = crypto.randomUUID();
      this.pending.set(id, { resolve, reject });
      this.worker.postMessage({ id, method, params }, transferList);
    });
  }

  destroy() {
    this.worker.terminate();
    this.pending.forEach(d => d.reject(new Error('Worker terminated')));
    this.pending.clear();
  }
}

// Client usage
const rpc = new WorkerRPC(new URL('./rpc-worker.js', import.meta.url));
const result = await rpc.invoke('computeHash', ['hello world']);
```

---

## 12. Interview Questions

<details>
<summary>1. What browser APIs are inaccessible inside a Web Worker and why?</summary>
The DOM (<code>window</code>, <code>document</code>, elements) and UI storage mechanisms (<code>localStorage</code>) are inaccessible. This restriction prevents race conditions and eliminates the need for complex DOM concurrency locking mechanisms across multiple threads.
</details>

<details>
<summary>2. What is the difference between structured cloning and transferring an object?</summary>
Structured cloning creates an identical deep copy of an object in the destination thread's heap, consuming double the memory and serialization time. Transferring transfers memory ownership directly by detaching the memory pointer from the sender and attaching it to the receiver in $O(1)$ time with zero memory duplication.
</details>

<details>
<summary>3. How does a SharedWorker differ from a Dedicated Worker?</summary>
A Dedicated Worker is private to the single tab or script that created it. A <code>SharedWorker</code> can be accessed and shared simultaneously by multiple browser tabs, windows, or iframes running on the exact same origin, communicating via <code>MessagePort</code> objects.
</details>

<details>
<summary>4. What is OffscreenCanvas and why is it important?</summary>
<code>OffscreenCanvas</code> is a canvas interface that can be transferred to and controlled from inside a Web Worker. It enables complex graphics, WebGL rendering, and physics animations to run on a background thread without blocking the browser's UI thread.
</details>

<details>
<summary>5. Can Web Workers communicate with each other directly?</summary>
Yes, by creating a <code>MessageChannel</code> on the main thread and transferring <code>port1</code> to Worker A and <code>port2</code> to Worker B, the two workers can communicate directly via <code>postMessage</code> without routing through the main thread.
</details>

<details>
<summary>6. How do you handle unhandled errors occurring inside a Web Worker?</summary>
By attaching an <code>onerror</code> event listener to the worker instance on the main thread (inspecting <code>event.message</code>, <code>event.filename</code>, and <code>event.lineno</code>), or by wrapping worker task execution in a global <code>self.onerror</code> or <code>try/catch</code> block inside the worker.
</details>

<details>
<summary>7. What is the Cross-Origin Worker restriction and how can it be bypassed?</summary>
Browsers enforce that worker scripts must be hosted on the same origin as the parent page. To load a script from a CDN or different origin, you can fetch the script as text via CORS, create a Blob (<code>new Blob([code], { type: 'text/javascript' })</code>), and pass <code>URL.createObjectURL(blob)</code> to the Worker constructor.
</details>

<details>
<summary>8. Does self.close() immediately abort synchronous execution in a worker?</summary>
No. <code>self.close()</code> stops the worker from processing further events in its event queue, but synchronous JavaScript code currently executing in the active stack frame continues to run to completion.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Resilient Worker Watchdog (Heartbeat Monitor)
Implement a client-side watchdog class that pings a Web Worker every 2 seconds. If the worker fails to reply within 4 seconds (e.g., due to an infinite loop or locked thread), automatically terminate the stuck worker and spawn a replacement.

<details>
<summary>View Solution</summary>

```javascript
class ResilientWorker {
  constructor(workerUrl) {
    this.workerUrl = workerUrl;
    this.spawn();
  }

  spawn() {
    this.worker = new Worker(this.workerUrl, { type: 'module' });
    this.lastSeen = Date.now();

    this.worker.onmessage = (e) => {
      if (e.data?.type === 'PONG') {
        this.lastSeen = Date.now();
      }
    };

    this.monitor = setInterval(() => {
      if (Date.now() - this.lastSeen > 4000) {
        console.warn('Worker unresponsive! Terminating and respawning...');
        this.worker.terminate();
        clearInterval(this.monitor);
        this.spawn();
        return;
      }
      this.worker.postMessage({ type: 'PING' });
    }, 2000);
  }

  terminate() {
    clearInterval(this.monitor);
    this.worker.terminate();
  }
}
```
</details>

### Exercise 2: Direct Peer-to-Peer Worker Channel
Create two dedicated Web Workers (Worker A and Worker B) and establish a direct communication channel between them using `MessageChannel`, ensuring no message routing passes through the main thread.

<details>
<summary>View Solution</summary>

```javascript
// main.js
const workerA = new Worker('./workerA.js', { type: 'module' });
const workerB = new Worker('./workerB.js', { type: 'module' });

const channel = new MessageChannel();

// Transfer port1 to Worker A, port2 to Worker B
workerA.postMessage({ type: 'INIT_PORT', port: channel.port1 }, [channel.port1]);
workerB.postMessage({ type: 'INIT_PORT', port: channel.port2 }, [channel.port2]);

// workerA.js
self.onmessage = (e) => {
  if (e.data.type === 'INIT_PORT') {
    const peerPort = e.data.port;
    peerPort.onmessage = (msg) => {
      console.log('Worker A received directly from Worker B:', msg.data);
    };
    peerPort.postMessage('Hello directly from Worker A!');
  }
};

// workerB.js
self.onmessage = (e) => {
  if (e.data.type === 'INIT_PORT') {
    const peerPort = e.data.port;
    peerPort.onmessage = (msg) => {
      console.log('Worker B received directly from Worker A:', msg.data);
      peerPort.postMessage('Acknowledgment from Worker B');
    };
  }
};
```
</details>
