# Web Workers

A **Web Worker** runs JavaScript on a separate thread in the browser, so heavy computation does not freeze the page. Workers have their own global scope and event loop, and they communicate with the main thread by **passing messages**.

## Kinds of workers

| Type | Scope | Lifetime | Purpose |
|------|-------|----------|---------|
| **Dedicated Worker** | One owner page or script | Until terminated or the owner goes away | Offload computation (this file's focus) |
| **Shared Worker** | Several tabs or windows of the same origin | While any connected context exists | Shared state or one connection across tabs |
| **Service Worker** | The origin (event-driven) | Started and stopped by the browser | Offline caching, network interception, push |
| **Worklets** | Rendering/audio pipelines | Managed by the browser | Audio and paint customization |

## A dedicated worker

```js
// main.js
const worker = new Worker('worker.js');

worker.postMessage({ numbers: [1, 2, 3, 4, 5] });

worker.onmessage = (event) => {
  console.log('sum:', event.data);        // sum: 15
};

worker.onerror = (event) => {
  console.error('worker error:', event.message);
};
```

```js
// worker.js
self.onmessage = (event) => {
  const { numbers } = event.data;
  const sum = numbers.reduce((a, b) => a + b, 0);
  self.postMessage(sum);
};
```

Inside a worker, `self` (or `globalThis`) is the `DedicatedWorkerGlobalScope`. There is no `window` and no DOM.

## Module workers

Use `import` inside a worker by creating it as a module:

```js
const worker = new Worker(new URL('./worker.js', import.meta.url), { type: 'module' });
```

The `new URL(..., import.meta.url)` form is understood by bundlers (Vite, webpack, esbuild) so the worker file is bundled correctly.

```js
// worker.js (module)
import { heavyCompute } from './compute.js';

self.onmessage = ({ data }) => {
  self.postMessage(heavyCompute(data));
};
```

## What workers can and cannot access

| Available in workers | Not available |
|----------------------|---------------|
| `fetch`, `XMLHttpRequest`, `WebSocket` | `window`, `document`, any DOM API |
| `setTimeout`, `setInterval`, `queueMicrotask` | Direct access to main-thread variables |
| `IndexedDB`, Cache API, `crypto` | `localStorage`, `sessionStorage` |
| `importScripts()` (classic workers), `import` (module workers) | Direct access to the page's JS objects |
| `OffscreenCanvas`, `WebAssembly`, `TextEncoder`, `URL` | Alerts and dialogs |
| `navigator` (limited), `location` (read-only) | |

To change the page, the worker must send a message and let the main thread update the DOM.

## Messaging and structured clone

`postMessage` copies data using the **structured clone algorithm**:

| Supported | Not supported |
|-----------|---------------|
| Primitives, plain objects, arrays | Functions |
| `Date`, `RegExp`, `Map`, `Set` | DOM nodes |
| `ArrayBuffer`, typed arrays, `Blob`, `File` | Class instances keep their data but **lose methods and prototype** |
| `Error` objects (basic), `BigInt` | Objects with getters/setters keep values, not accessors |
| Circular references | Symbols as values |

```js
worker.postMessage({ id: 1, when: new Date(), tags: new Set(['a']) });    // works
worker.postMessage({ fn: () => 1 });                                       // DataCloneError
```

Copying large data repeatedly is expensive. Two ways to avoid copying:

### Transferable objects

Ownership **moves** to the receiver; the sender can no longer use it. Transfer is near-instant regardless of size.

```js
const buffer = new ArrayBuffer(100 * 1024 * 1024);          // 100 MB
console.log(buffer.byteLength);                              // 104857600

worker.postMessage({ buffer }, [buffer]);                    // second argument lists transferables
console.log(buffer.byteLength);                              // 0 (detached on this side)
```

Transferable types include `ArrayBuffer`, `MessagePort`, `ImageBitmap`, `OffscreenCanvas`, `ReadableStream`, `WritableStream`, and `TransformStream`.

### SharedArrayBuffer

Both sides see the **same memory**. See [SharedArrayBuffer and Atomics](./04_sharedarraybuffer-and-atomics.md).

## Request and response pattern

Because messaging is event-based, match responses to requests with an id and wrap it in a promise:

```js
function createWorkerClient(url) {
  const worker = new Worker(url, { type: 'module' });
  const pending = new Map();
  let nextId = 0;

  worker.onmessage = ({ data }) => {
    const { id, result, error } = data;
    const { resolve, reject } = pending.get(id);
    pending.delete(id);
    error ? reject(new Error(error)) : resolve(result);
  };

  worker.onerror = (e) => {
    for (const { reject } of pending.values()) reject(e.error ?? new Error(e.message));
    pending.clear();
  };

  return {
    call(method, params) {
      return new Promise((resolve, reject) => {
        const id = nextId++;
        pending.set(id, { resolve, reject });
        worker.postMessage({ id, method, params });
      });
    },
    terminate: () => worker.terminate(),
  };
}

const client = createWorkerClient(new URL('./worker.js', import.meta.url));
const result = await client.call('hash', { text: 'hello' });
```

```js
// worker.js
const methods = {
  hash: async ({ text }) => {
    const bytes = new TextEncoder().encode(text);
    const digest = await crypto.subtle.digest('SHA-256', bytes);
    return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, '0')).join('');
  },
};

self.onmessage = async ({ data: { id, method, params } }) => {
  try {
    self.postMessage({ id, result: await methods[method](params) });
  } catch (err) {
    self.postMessage({ id, error: err.message });
  }
};
```

Libraries such as **Comlink** automate this by turning a worker into an object with async methods.

## Error handling

```js
worker.addEventListener('error', (e) => {
  console.error(`${e.filename}:${e.lineno} ${e.message}`);       // uncaught error inside the worker
});

worker.addEventListener('messageerror', (e) => {
  console.error('message could not be deserialized');
});
```

Inside the worker, catch errors in async code and send them back; unhandled rejections do not reach the main thread automatically (they fire `unhandledrejection` in the worker's scope).

## Lifecycle

```js
worker.terminate();      // stops immediately from outside, no cleanup
```

```js
// inside the worker
self.close();            // the worker ends itself
```

Workers keep running (and holding memory) until terminated, so always clean up, for example when a component unmounts.

## Progress reporting

```js
// worker.js
self.onmessage = ({ data: items }) => {
  const results = [];
  for (let i = 0; i < items.length; i++) {
    results.push(process(items[i]));
    if (i % 1000 === 0) self.postMessage({ type: 'progress', done: i, total: items.length });
  }
  self.postMessage({ type: 'done', results });
};
```

```js
// main.js
worker.onmessage = ({ data }) => {
  if (data.type === 'progress') bar.value = data.done / data.total;
  else if (data.type === 'done') show(data.results);
};
```

## Cancellation

Workers process messages one at a time, so a long synchronous task cannot see a "cancel" message until it finishes. Options:

- Check a flag in a `SharedArrayBuffer` periodically
- Process in chunks and yield between them so `onmessage` can run
- Call `worker.terminate()` and start a fresh worker

## Shared Workers

One worker instance serves multiple tabs on the same origin through **ports**.

```js
// in each tab
const shared = new SharedWorker(new URL('./shared.js', import.meta.url), { type: 'module' });
shared.port.onmessage = (e) => console.log(e.data);
shared.port.postMessage('hello');
```

```js
// shared.js
const ports = new Set();

self.onconnect = (event) => {
  const port = event.ports[0];
  ports.add(port);
  port.onmessage = (e) => {
    for (const p of ports) p.postMessage(e.data);       // broadcast to all tabs
  };
};
```

Browser support differs (notably on some mobile browsers), so check compatibility. `BroadcastChannel` is a simpler option when you only need to message between tabs.

## Service Workers (overview)

Service workers sit between the page and the network. They are event-driven, can run when no page is open, and require HTTPS (except on `localhost`).

```js
// register from the page
if ('serviceWorker' in navigator) {
  await navigator.serviceWorker.register('/sw.js');
}
```

```js
// sw.js
self.addEventListener('install', (e) => {
  e.waitUntil(caches.open('v1').then((c) => c.addAll(['/', '/app.js', '/style.css'])));
});

self.addEventListener('fetch', (e) => {
  e.respondWith(caches.match(e.request).then((hit) => hit ?? fetch(e.request)));
});
```

They are **not** for CPU offloading: they can be shut down at any time and have no persistent state. Use dedicated workers for computation.

## Worker pools

Creating a worker is costly. For many tasks, keep a fixed number of workers (usually `navigator.hardwareConcurrency - 1`) and queue tasks:

```js
class WorkerPool {
  #idle = [];
  #queue = [];

  constructor(url, size = Math.max(1, (navigator.hardwareConcurrency ?? 4) - 1)) {
    for (let i = 0; i < size; i++) {
      this.#idle.push(new Worker(url, { type: 'module' }));
    }
  }

  run(message, transfer = []) {
    return new Promise((resolve, reject) => {
      this.#queue.push({ message, transfer, resolve, reject });
      this.#next();
    });
  }

  #next() {
    if (!this.#idle.length || !this.#queue.length) return;
    const worker = this.#idle.pop();
    const { message, transfer, resolve, reject } = this.#queue.shift();

    const done = () => {
      worker.onmessage = worker.onerror = null;
      this.#idle.push(worker);
      this.#next();
    };
    worker.onmessage = (e) => { done(); resolve(e.data); };
    worker.onerror = (e) => { done(); reject(e.error ?? new Error(e.message)); };
    worker.postMessage(message, transfer);
  }

  terminate() {
    for (const w of this.#idle) w.terminate();
  }
}
```

## OffscreenCanvas

Render graphics in a worker so the UI thread stays free:

```js
const canvas = document.querySelector('canvas');
const offscreen = canvas.transferControlToOffscreen();
worker.postMessage({ canvas: offscreen }, [offscreen]);
```

```js
// worker.js
self.onmessage = ({ data: { canvas } }) => {
  const ctx = canvas.getContext('2d');
  (function draw() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    // ... draw ...
    requestAnimationFrame(draw);
  })();
};
```

## Cross-origin isolation

`SharedArrayBuffer` (and some high-precision timers) require the page to be **cross-origin isolated**, enabled by two response headers:

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

Check with `self.crossOriginIsolated`. Isolation restricts embedding third-party resources unless they opt in.

## When to use a worker

| Good fit | Poor fit |
|----------|----------|
| Parsing or generating large files | Quick operations (a few milliseconds) |
| Image or video processing | Anything that must touch the DOM |
| Encryption and hashing of large data | Tasks dominated by network waiting |
| Search indexing, diffing, compression | Small data with expensive transfer back and forth |
| Running WebAssembly modules | |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Trying to touch the DOM in a worker | `document is not defined` | Send results to the main thread |
| Posting functions or class instances | `DataCloneError` or lost methods | Send plain data |
| Cloning huge buffers every message | Slow, doubles memory | Transfer or share |
| Using a buffer after transferring it | It is detached (length 0) | Treat as moved |
| Creating a worker per task | Startup overhead | Worker pool |
| Never terminating workers | Memory and CPU leaks | `terminate()` on cleanup |
| No `error` handler | Failures vanish silently | Handle `error` and `messageerror` |
| Relying on cancel messages during sync loops | They are not processed until the loop ends | Chunk, or use shared flag |
| Using `SharedArrayBuffer` without isolation headers | `ReferenceError` or unavailable | Set COOP and COEP |
| Using service workers for computation | Can be killed any time | Dedicated workers |

## Key takeaways

- Dedicated workers run code on another thread with their own global scope and no DOM
- Communication is by `postMessage`; data is cloned unless transferred or shared
- Use transferables (`ArrayBuffer`, `OffscreenCanvas`, ...) to avoid copying large data
- Wrap messaging in promises with request ids, or use a library like Comlink
- Pool workers instead of creating one per task, and terminate them when done
- Service workers and shared workers solve different problems (network proxy, cross-tab sharing)

**Next:** [Worker Threads](./03_worker-threads.md)
