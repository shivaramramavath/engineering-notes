# Event Loop Performance & Lag Monitoring in Node.js

> Measuring event loop delay, mitigating starvation, partitioning CPU-intensive workloads, and understanding `UV_THREADPOOL_SIZE`.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **APIs** | `perf_hooks.monitorEventLoopDelay()`, `process.hrtime` |
| **Level** | Advanced Performance Engineering |

---

## 1. What is Event Loop Lag?

Event loop lag (or delay) is the time difference between when an asynchronous task or timer was scheduled to execute and when V8 actually begins executing its callback.
- **Normal Lag**: 1ms – 5ms.
- **Degraded Lag**: 50ms – 500ms (requests queue up, TTFB spikes).
- **Event Loop Starvation**: 1000ms+ (health checks fail, Kubernetes restarts container).

---

## 2. Monitoring Event Loop Delay with `perf_hooks`

Modern Node.js provides a native C++ histogram that tracks microsecond-level event loop delay without overhead:

```javascript
import { monitorEventLoopDelay } from 'node:perf_hooks';

// Resolution: 10ms histogram sampling
export const h = monitorEventLoopDelay({ resolution: 10 });
h.enable();

setInterval(() => {
  const p50 = (h.percentile(50) / 1e6).toFixed(2);
  const p90 = (h.percentile(90) / 1e6).toFixed(2);
  const p99 = (h.percentile(99) / 1e6).toFixed(2);
  const max = (h.max / 1e6).toFixed(2);

  console.log(`[Event Loop Delay] p50: ${p50}ms | p90: ${p90}ms | p99: ${p99}ms | max: ${max}ms`);

  if (h.percentile(99) / 1e6 > 50) {
    console.warn('WARNING: High event loop lag detected (>50ms p99)!');
  }

  h.reset();
}, 5000).unref();
```

---

## 3. Techniques to Prevent Starvation

1. **Partition Long Synchronous Loops**: Use `setImmediate()` to yield back to the event loop:
   ```javascript
   export async function processLargeArrayInChunks(items, chunkSize = 1000) {
     for (let i = 0; i < items.length; i += chunkSize) {
       const chunk = items.slice(i, i + chunkSize);
       processChunk(chunk);
       // Yield to allow pending I/O and HTTP requests to process:
       await new Promise(resolve => setImmediate(resolve));
     }
   }
   ```
2. **Offload Heavy CPU Work**: Move cryptographic hashing, image resizing, or large JSON parsing to `worker_threads`.
3. **Configure `UV_THREADPOOL_SIZE`**: By default, libuv allocates 4 threads for asynchronous filesystem calls, `crypto.pbkdf2`, and DNS resolution. For high-concurrency file or crypto services, set:
   ```bash
   UV_THREADPOOL_SIZE=16 node server.js
   ```
