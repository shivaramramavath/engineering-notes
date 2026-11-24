# Server-Side Asynchronous Patterns & EventEmitter in Node.js

> Deep dive into concurrency control, `Promise.allSettled`, abort controllers, and production EventEmitter memory management.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **APIs** | `AbortController`, `EventEmitter`, `events.once`, `Promise.allSettled` |
| **Level** | Core Server-Side JS |

---

## 1. Concurrency Limiting & Batching

Running 10,000 promises simultaneously via `Promise.all()` will exhaust socket descriptors, database connections, and memory:

```javascript
// Idiomatic concurrency limiter (p-limit pattern)
export function pLimit(concurrency) {
  const queue = [];
  let activeCount = 0;

  const next = () => {
    activeCount--;
    if (queue.length > 0) {
      queue.shift()();
    }
  };

  return (fn, ...args) => new Promise((resolve, reject) => {
    const run = async () => {
      activeCount++;
      try {
        resolve(await fn(...args));
      } catch (err) {
        reject(err);
      } finally {
        next();
      }
    };

    if (activeCount < concurrency) {
      run();
    } else {
      queue.push(run);
    }
  });
}
```

---

## 2. Request Cancellation with `AbortController`

Modern Node.js APIs (such as `fetch`, `node:fs`, `http`, and `child_process`) natively accept an `AbortSignal`:

```javascript
import http from 'node:http';

const server = http.createServer(async (req, res) => {
  const controller = new AbortController();
  const { signal } = controller;

  // Abort ongoing upstream work if the client closes the HTTP connection
  req.on('close', () => {
    if (!res.writableEnded) {
      console.log('Client disconnected; aborting upstream queries');
      controller.abort();
    }
  });

  try {
    const response = await fetch('https://api.example.com/slow-data', { signal });
    const data = await response.json();
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify(data));
  } catch (err) {
    if (err.name === 'AbortError') {
      console.log('Upstream fetch aborted cleanly');
    } else {
      res.writeHead(500);
      res.end('Server Error');
    }
  }
});
```

---

## 3. EventEmitter Memory Leaks & Cleanup

When attaching listeners dynamically within request handlers, failing to remove them causes severe memory leaks:
```javascript
import { EventEmitter, on } from 'node:events';

class JobTracker extends EventEmitter {}
export const tracker = new JobTracker();

// ❌ ANTI-PATTERN: Adding listener inside request without removing
export function handleStatusRequest(req, res) {
  tracker.on('progress', (data) => {
    // Leaks 1 listener per request!
    res.write(JSON.stringify(data));
  });
}

// ✅ IDIOMATIC: Clean up with AbortSignal or req.on('close')
export function handleStatusRequestSafe(req, res) {
  const onProgress = (data) => res.write(JSON.stringify(data));
  tracker.on('progress', onProgress);

  req.on('close', () => {
    tracker.off('progress', onProgress); // Unbind!
  });
}
```
