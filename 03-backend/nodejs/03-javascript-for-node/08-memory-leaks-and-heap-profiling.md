# Memory Leaks & Heap Profiling in Node.js

> Diagnosing, reproducing, and eliminating memory leaks in production Node.js applications using heap snapshots, V8 flags, and Chrome DevTools.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Tools** | `v8.getHeapSnapshot()`, `--max-old-space-size`, Chrome DevTools Memory Inspector |
| **Level** | Advanced Server JS |

---

## 1. Common Memory Leak Sources in Node.js

1. **Global Variables & Unbounded Caches**:
   - Storing user sessions or API responses in a plain JavaScript `Map` without TTL or maximum size eviction.
   - Solution: Use `lru-cache` or Redis for externalized caching.
2. **Dangling Event Listeners**:
   - Attaching listeners to global EventEmitters inside transient HTTP request handlers without cleanup.
3. **Accidental Closure Captures**:
   - Creating nested functions that retain large parent scope objects in memory long after the request completes.
4. **Buffer Allocations Outside V8 Heap**:
   - Allocating uncollected native buffers that fail to release OS memory descriptors.

---

## 2. Generating Programmatic Heap Snapshots

You can take a heap snapshot on-demand when memory crosses an alert threshold:

```javascript
import v8 from 'node:v8';
import fs from 'node:fs';
import path from 'node:path';

export function takeHeapSnapshot(prefix = 'heap') {
  const fileName = `${prefix}-${Date.now()}.heapsnapshot`;
  const snapshotStream = v8.getHeapSnapshot();
  const fileStream = fs.createWriteStream(path.join(process.cwd(), fileName));

  snapshotStream.pipe(fileStream);
  console.log(`Heap snapshot written to: ${fileName}`);
}

// Monitor heap usage
setInterval(() => {
  const { heapUsed, heapTotal } = process.memoryUsage();
  const usedMB = (heapUsed / 1024 / 1024).toFixed(2);
  const totalMB = (heapTotal / 1024 / 1024).toFixed(2);

  if (heapUsed / heapTotal > 0.9) {
    console.warn(`CRITICAL: Heap usage at 90%! (${usedMB}MB / ${totalMB}MB)`);
    takeHeapSnapshot('oom-warning');
  }
}, 30000).unref();
```

---

## 3. Analyzing Snapshots in Chrome DevTools

1. Open Chrome DevTools $\rightarrow$ **Memory** tab.
2. Click **Load** and select the `.heapsnapshot` file.
3. Use the **Comparison View** between two snapshots taken before and after a load test:
   - Sort by **# Delta** and **Size Delta**.
   - Inspect retained objects (`Retainers` pane) to identify the root reference preventing garbage collection.
