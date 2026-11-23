# Node.js Server Performance and High Throughput

> Event loop delay monitoring, microtask queue starvation, schema-based JSON serialization, radix tree routing, and streaming architecture.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Node Event Loop](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs/02_node-event-loop.md), [HTTP Server](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs/08_http-server.md) |
| **Time to read** | 25 minutes |
| **Status** | Complete |
| **Standard / Runtime** | Node.js v18 LTS - v24+, POSIX & Windows |

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

In backend engineering, Node.js is widely celebrated for its event-driven non-blocking I/O model. It excels at handling tens of thousands of concurrent network connections using a single operating system thread. However, this same architecture makes Node.js applications uniquely vulnerable to single-point performance collapse.

If a request handler executes a synchronous CPU-heavy loop, performs synchronous JSON parsing of massive payloads, or starves the event loop with recursive microtasks, **the entire server freezes**. Every other concurrent HTTP request, database callback, and health check stops progressing. Achieving ultra-high throughput (50,000+ requests/sec per server) requires measuring **Event Loop Delay**, eliminating serialization overhead, utilizing Radix Tree routing, and designing streaming pipelines with backpressure.

---

## 2. Core Concepts

### Event Loop Lag (Delay)
Event loop delay is the measure of time that elapses between when an event or timer is scheduled to run and when the event loop actually gets around to executing it.
- In a healthy server, event loop delay is $< 2\text{ms}$.
- If event loop delay climbs to $50\text{ms}$ or $500\text{ms}$, incoming client requests experience extreme tail latency (p99), connection timeouts, and health-check failures.

### Microtask Queue Starvation
Microtasks (`process.nextTick` and resolved Promise callbacks) execute immediately after the current synchronous stack frame unwinds, **before** the event loop advances to the next phase (Timers, Poll, Check). Recursively scheduling microtasks causes infinite loop starvation: the event loop never processes I/O, timers, or network packets.

### Schema-Based Serialization (Why Fastify Beats Express)
- **Express**: Uses native `JSON.stringify()`, which uses dynamic reflection to traverse object properties at runtime. It also uses regex-based routing (`path-to-regexp`), which performs $O(N)$ regex evaluations across every registered route.
- **Fastify**: Uses compiled schema serialization (`fast-json-stringify`) which generates specialized JIT string concatenation code based on JSON Schema, running up to 2x faster than `JSON.stringify()`. It routes requests using a **Radix Tree (Prefix Tree)**, matching URLs in deterministic $O(K)$ time (where $K$ is URL length).

---

## 3. How It Works Internally

```
+--------------------------------------------------------------------------+
| INCOMING HIGH-CONCURRENCY HTTP TRAFFIC (50,000 req/s)                    |
+--------------------------------------------------------------------------+
                                    |
                                    v
+--------------------------------------------------------------------------+
| RADIX TREE ROUTER (O(K) Path Match via Trie traversal)                   |
| Matches /api/users/:id in deterministic microsecond lookup              |
+--------------------------------------------------------------------------+
                                    |
                                    v
+--------------------------------------------------------------------------+
| EVENT LOOP HEALTH MONITOR (monitorEventLoopDelay)                        |
| - High-resolution libuv timer records loop latency histogram             |
| - If p99 delay > 50ms -> Automatically trigger load-shedding (HTTP 503)  |
+--------------------------------------------------------------------------+
                                    |
                                    v
+--------------------------------------------------------------------------+
| SCHEMA-BASED SERIALIZER (Pre-compiled string concatenation)              |
| Avoids JSON.stringify dynamic traversal; writes directly to TCP socket   |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Monitoring Event Loop Delay via `perf_hooks`
```javascript
import { monitorEventLoopDelay } from 'node:perf_hooks';

// Resolution in milliseconds; samples loop delay continuously
const histogram = monitorEventLoopDelay({ resolution: 20 });
histogram.enable();

setInterval(() => {
  console.log({
    min: (histogram.min / 1e6).toFixed(2) + 'ms',
    mean: (histogram.mean / 1e6).toFixed(2) + 'ms',
    p50: (histogram.percentile(50) / 1e6).toFixed(2) + 'ms',
    p95: (histogram.percentile(95) / 1e6).toFixed(2) + 'ms',
    p99: (histogram.percentile(99) / 1e6).toFixed(2) + 'ms',
    max: (histogram.max / 1e6).toFixed(2) + 'ms',
  });
  histogram.reset();
}, 2000);
```

### Measuring Route Latency via `process.hrtime.bigint()`
```javascript
const start = process.hrtime.bigint();
// Execute route logic...
const end = process.hrtime.bigint();
const elapsedMicroseconds = Number(end - start) / 1000;
```

---

## 5. Practical Examples

### Example 1: Demonstrating and Preventing Microtask Starvation
```javascript
// DANGEROUS: Recursively calling process.nextTick starves all I/O!
function starveEventLoop() {
  process.nextTick(starveEventLoop); // Event loop never moves to Poll or Check phase!
}

// SAFE: Cooperative scheduling using setImmediate yields back to the event loop
function cooperativeLoop(iterations, onComplete) {
  let count = 0;

  function chunk() {
    const end = Math.min(count + 1000, iterations);
    for (; count < end; count++) {
      // Execute piece of work
    }

    if (count < iterations) {
      // Yield to Check phase: allows timers and network packets to be serviced
      setImmediate(chunk);
    } else {
      onComplete();
    }
  }

  chunk();
}
```

### Example 2: Automatic Load Shedding on Event Loop Lag
```javascript
import http from 'node:http';
import { monitorEventLoopDelay } from 'node:perf_hooks';

const histogram = monitorEventLoopDelay({ resolution: 20 });
histogram.enable();

const MAX_P99_DELAY_MS = 50;

const server = http.createServer((req, res) => {
  const p99LagMs = histogram.percentile(99) / 1e6;

  // Protect server health: shed load if event loop is severely delayed
  if (p99LagMs > MAX_P99_DELAY_MS && req.url !== '/health') {
    res.writeHead(503, {
      'Content-Type': 'application/json',
      'Retry-After': '2',
    });
    res.end(JSON.stringify({ error: 'Server Overloaded', currentLagMs: p99LagMs }));
    return;
  }

  res.writeHead(200, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify({ status: 'ok', lagMs: p99LagMs }));
});

server.listen(3000);
```

### Example 3: Schema-Based JSON Serializer (Fast String Concatenation)
```javascript
// Demonstrating the core principle of fast-json-stringify
// Instead of reflective JSON.stringify(), compile a dedicated string builder
function compileUserSerializer() {
  return function serialize(user) {
    return `{"id":${user.id},"name":"${user.name}","active":${user.active ? 'true' : 'false'}}`;
  };
}

const fastSerialize = compileUserSerializer();

const user = { id: 101, name: 'Grace Hopper', active: true };
console.log(fastSerialize(user)); // ~2x faster than JSON.stringify(user)
```

### Example 4: Deterministic Radix Tree Routing Primitive
```javascript
class RadixNode {
  constructor(part = '', isEnd = false) {
    this.part = part;
    this.isEnd = isEnd;
    this.children = new Map();
    this.handler = null;
  }
}

export class RadixRouter {
  constructor() {
    this.root = new RadixNode();
  }

  insert(path, handler) {
    const segments = path.split('/').filter(Boolean);
    let current = this.root;

    for (const seg of segments) {
      if (!current.children.has(seg)) {
        current.children.set(seg, new RadixNode(seg));
      }
      current = current.children.get(seg);
    }
    current.isEnd = true;
    current.handler = handler;
  }

  lookup(path) {
    const segments = path.split('/').filter(Boolean);
    let current = this.root;

    for (const seg of segments) {
      if (!current.children.has(seg)) {
        return null; // Fast miss
      }
      current = current.children.get(seg);
    }
    return current.isEnd ? current.handler : null;
  }
}
```

### Example 5: High-Performance Streaming File Server with Backpressure
```javascript
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { pipeline } from 'node:stream/promises';

const server = http.createServer(async (req, res) => {
  if (req.method === 'GET' && req.url.startsWith('/static/')) {
    const filename = path.basename(req.url);
    const filepath = path.resolve('./public', filename);

    try {
      const stat = await fs.promises.stat(filepath);
      res.writeHead(200, {
        'Content-Type': 'application/octet-stream',
        'Content-Length': stat.size,
      });

      // Stream with automatic kernel backpressure and fd cleanup
      const readStream = fs.createReadStream(filepath);
      await pipeline(readStream, res);
    } catch (err) {
      if (!res.headersSent) {
        res.writeHead(err.code === 'ENOENT' ? 404 : 500);
        res.end();
      }
    }
  }
});

server.listen(3000);
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`JSON.parse()` Blocking on Large Payloads**: Parsing a 10MB JSON string synchronously blocks the V8 thread for 50–150ms. If your API accepts massive payloads, parse them in a Worker Thread or stream them using an incremental JSON parser (`stream-json`).
2. **`crypto.pbkdf2Sync` and `bcrypt.hashSync`**: Using synchronous cryptographic functions in request handlers completely paralyzes the server. Always use their asynchronous promise-based variants or worker threads.
3. **Regex Catastrophic Backtracking (ReDoS)**: Evaluating an exponential regex (e.g., `/(a+)+$/`) against malicious client input locks the event loop at 100% CPU indefinitely. Use safe regex linters (`safe-regex`) or RE2 C++ bindings.
4. **V8 String Garbage Creation in Concatenation**: Generating dynamic HTML or CSV templates via raw `str += ...` in tight loops creates millions of ConsStrings. Stream output chunks directly to `res.write()` instead of accumulating in memory.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| `fs.readFileSync()` inside route handlers | Freezes the event loop for all concurrent users | Use `fs.promises.readFile()` or `createReadStream()` |
| Synchronous JSON parsing of multi-MB payloads | Blocks event loop for 100ms+ | Stream with `stream-json` or parse in a Worker |
| Unbounded `process.nextTick` recursions | Microtasks starve I/O, timers, and sockets | Yield cooperatively via `setImmediate()` |
| Monolithic Express regex routing tables | $O(N)$ linear regex scans degrade routing latency | Use Radix Tree routing (Fastify) |
| Omitting Event Loop Delay monitoring | Latency spikes go undetected until complete outage | Monitor via `monitorEventLoopDelay` |
| Loading entire database tables into RAM | Causes massive V8 heap bloat and Major GC pauses | Stream DB rows with database cursors |

---

## 8. Performance Characteristics

- **Throughput Scalability**: A native Node.js HTTP server or Fastify application can sustain 40,000–70,000 requests/sec per core for lightweight I/O endpoints.
- **Event Loop Overhead**: Checking timers and polling sockets in libuv consumes $< 0.5$ microseconds per turn when unblocked.

---

## 9. Engine Internals

1. **`uv_prepare_t` and `uv_check_t` in Event Loop Delay**:
   - `monitorEventLoopDelay` utilizes libuv's `uv_prepare_t` (fires right before epoll wait) and `uv_check_t` (fires immediately after epoll wait).
   - The engine measures the delta between timer target expirations and actual execution timestamps, logging the duration into a native C++ HDR Histogram.
2. **HTTP Parsing with `llhttp`**:
   - Node.js parses incoming HTTP request lines and headers using `llhttp`, a state machine written in C and compiled into native assembly. It streams tokens into JavaScript without copying byte buffers until required.

---

## 10. Best Practices & Design Guidelines

- **Implement Load Shedding**: Monitor event loop delay. When p99 exceeds 50ms, return HTTP 503 early to maintain stability for existing in-flight connections.
- **Use Streaming Endpoints**: Stream uploads and downloads directly through `pipeline()` to maintain a flat memory profile regardless of file size.
- **Cluster Across Available Cores**: Deploy with `cluster` or a process manager (PM2) to utilize `os.availableParallelism()` cores.

---

## 11. Real-World Case Study

### Rebuilding a Degraded Express Gateway to Achieve 5x Throughput
```javascript
// PROBLEM: An API Gateway built with Express degraded under load:
// At 5,000 req/sec, p99 latency surged to 850ms, and event loop delay exceeded 200ms.
// Profiling identified two bottlenecks:
// 1. `path-to-regexp` routing overhead over 120 registered routes.
// 2. Heavy `JSON.stringify()` overhead on large catalog response bodies.

// SOLUTION:
// 1. Migrated routing to a Radix Tree router (`find-my-way`).
// 2. Replaced `JSON.stringify` with `fast-json-stringify` schema compiler.
// 3. Implemented `monitorEventLoopDelay` load shedding.

// Result:
// - P99 latency dropped from 850ms to 18ms.
// - Maximum throughput surged from 5,000 req/sec to 28,000 req/sec on the same VM.
// - CPU utilization dropped by 45%.
```

---

## 12. Interview Questions

<details>
<summary>1. What is Event Loop Delay and why is it the most critical metric in Node.js backend services?</summary>
Event loop delay (or lag) measures the delay between when an event, timer, or I/O callback was scheduled to run and when the event loop actually gets around to executing it. Because Node.js is single-threaded, any event loop lag directly delays all other concurrent requests, leading to tail latency surges and connection timeouts.
</details>

<details>
<summary>2. How does microtask execution differ from macrotasks, and how can microtasks cause starvation?</summary>
Microtasks (<code>process.nextTick</code>, resolved promises) execute immediately at the completion of the current stack frame before the event loop advances to the next phase. Macrotasks (timers, I/O events, <code>setImmediate</code>) execute during their specific phase. If microtasks repeatedly schedule further microtasks, the microtask queue never drains, starving all subsequent event loop phases and freezing I/O.
</details>

<details>
<summary>3. Why does Fastify outperform Express in routing and serialization?</summary>
Fastify uses a Radix Tree (Prefix Tree) router which matches URL paths in deterministic $O(K)$ time (where $K$ is path length), unlike Express which scans an array of routes linearly using regex ($O(N)$). Fastify also uses schema-based serialization (<code>fast-json-stringify</code>) which compiles specialized string concatenation functions upfront, outperforming dynamic reflection in <code>JSON.stringify()</code>.
</details>

<details>
<summary>4. What is Load Shedding and how should a Node.js server implement it?</summary>
Load shedding is the architectural pattern of intentionally dropping excess incoming traffic (e.g., returning HTTP 503 Service Unavailable) when a server is overloaded, preserving system stability for in-flight requests. In Node.js, load shedding is typically triggered when <code>monitorEventLoopDelay</code> detects p99 event loop delay exceeding a safety threshold (e.g. 50ms).
</details>

<details>
<summary>5. What is the difference between setImmediate() and setTimeout(fn, 0)?</summary>
<code>setImmediate()</code> is scheduled to execute in the <b>Check</b> phase of the libuv event loop, right after the I/O Poll phase. <code>setTimeout(fn, 0)</code> executes in the <b>Timers</b> phase at the start of the next loop iteration, and incurs a minimum ~1ms timer resolution overhead.
</details>

<details>
<summary>6. How can synchronous JSON parsing cause a Denial of Service (DoS)?</summary>
If an endpoint accepts arbitrary JSON payloads without size validation, an attacker can transmit a 20MB deeply nested JSON body. Synchronously calling <code>JSON.parse()</code> on this payload freezes the single execution thread for hundreds of milliseconds, denying service to all other concurrent clients.
</details>

<details>
<summary>7. Why should you avoid process.nextTick in favor of queueMicrotask or setImmediate?</summary>
<code>process.nextTick</code> executes ahead of all other microtasks, including promise resolutions. Overusing it can bypass standard promise chaining mechanics and heighten the risk of event loop starvation. <code>queueMicrotask</code> is standard ECMAScript, while <code>setImmediate</code> yields to the event loop properly.
</details>

<details>
<summary>8. How do database cursors improve Node.js memory performance?</summary>
Instead of fetching 100,000 rows into memory as a giant JavaScript array (which causes heap bloat and Major GC pauses), a database cursor streams rows in small chunks over the network. Combined with Node.js streams and backpressure, memory usage remains flat regardless of total query size.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build an Event Loop Delay Health-Check Endpoint
Implement a native Node.js HTTP server that tracks event loop delay using `perf_hooks.monitorEventLoopDelay` and returns HTTP 200 on `/health` if p95 delay is under 20ms, or HTTP 503 if p95 delay exceeds 20ms.

<details>
<summary>View Solution</summary>

```javascript
import http from 'node:http';
import { monitorEventLoopDelay } from 'node:perf_hooks';

const histogram = monitorEventLoopDelay({ resolution: 20 });
histogram.enable();

const server = http.createServer((req, res) => {
  if (req.url === '/health') {
    const p95Ms = histogram.percentile(95) / 1e6;
    const isHealthy = p95Ms < 20;

    res.writeHead(isHealthy ? 200 : 503, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({
      status: isHealthy ? 'healthy' : 'degraded',
      p95EventLoopDelayMs: p95Ms.toFixed(2),
    }));
    return;
  }

  res.writeHead(200);
  res.end('Hello World\n');
});

server.listen(3000);
```
</details>

### Exercise 2: Cooperative Array Slicer
Write a function `cooperativeProcess(items, processItem, chunkSize = 500)` that processes a 100,000 item array without blocking the event loop for more than 5ms at a time, using `setImmediate` to interleave work.

<details>
<summary>View Solution</summary>

```javascript
export function cooperativeProcess(items, processItem, chunkSize = 500) {
  return new Promise((resolve) => {
    let index = 0;

    function nextChunk() {
      const end = Math.min(index + chunkSize, items.length);

      for (; index < end; index++) {
        processItem(items[index], index);
      }

      if (index < items.length) {
        setImmediate(nextChunk); // Yield to event loop
      } else {
        resolve(); // Completed all items
      }
    }

    nextChunk();
  });
}
```
</details>
