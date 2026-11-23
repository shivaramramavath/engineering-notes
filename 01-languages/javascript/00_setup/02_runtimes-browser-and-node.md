# JavaScript Engines & Runtimes: Browser, Node.js, Deno, and Bun

> An in-depth architectural comparative analysis of JavaScript engines (V8, SpiderMonkey, JavaScriptCore) and major modern runtimes (Browsers, Node.js, Deno, and Bun).

| Metadata | Value |
| :--- | :--- |
| **Level** | Foundational / Systems Architecture |
| **Prerequisites** | [01_what-is-javascript.md](./01_what-is-javascript.md) |
| **Time to Read** | 40 minutes |
| **Status** | Complete |
| **Specification** | V8 / JSC / SpiderMonkey / Node.js 22 LTS / Bun 1.1+ |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
A JavaScript engine executes pure ECMAScript bytecode, but software requires access to physical computing resources: network sockets, storage filesystems, operating system processes, cryptographic hardware, and graphic display servers. A **runtime** binds an execution engine to host environment capabilities.

### Architectural Separation
An engine is not a runtime:
- **Engine (V8, JSC, SpiderMonkey):** Pure language parser, bytecode interpreter, JIT compiler, heap memory allocator, and garbage collector.
- **Runtime (Node.js, Deno, Bun, Chrome, Safari):** The encompassing application platform providing the event loop, thread pools, environment variables, I/O libraries, and native OS bindings.

Understanding the boundary between the engine and the host runtime is foundational to diagnosing asynchronous ordering differences, memory leak vectors, performance deoptimizations, and cross-platform compatibility bugs.

---

## 2. Core Concepts

### 1. Major JavaScript Engines
- **V8 (Google):** Written in C++. Powers Google Chrome, Chromium browsers, Node.js, and Deno. Uses the Ignition bytecode interpreter, Sparkplug baseline compiler, Maglev mid-tier compiler, and TurboFan optimizing JIT compiler.
- **JavaScriptCore / Nitro (Apple):** Written in C++. Powers Safari, iOS WebViews, and Bun. Features a 4-tier pipeline: LLInt (Low-Level Interpreter), Baseline JIT, DFG (Data Flow Graph) JIT, and FTL (Faster Than Light) JIT.
- **SpiderMonkey (Mozilla):** Written in C++ and Rust. Powers Firefox and SpiderNode. Features WarpMonkey JIT compilation with CacheIR architecture.

### 2. Major JavaScript Runtimes
- **The Browser (Evergreen Web):** Sandboxed, UI-driven, single-origin security model. Features the DOM, CSSOM, Fetch API, Web Workers, IndexedDB, and WebGL/WebGPU.
- **Node.js (Active LTS 22+):** Server-side, libuv-backed asynchronous event-driven I/O runtime. Broad package ecosystem (npm), CommonJS + ESM dual-module support, native Buffer, and filesystem access.
- **Deno:** Secure-by-default server runtime created by Ryan Dahl. Native TypeScript support, web-standard APIs (`fetch`, `Request`, `Response`), permissions-based security flags (`--allow-net`, `--allow-read`).
- **Bun:** Ultra-fast, all-in-one JavaScript/TypeScript runtime written in Zig, built on JavaScriptCore. Features built-in bundler, transpiler, test runner, and native SQLite driver.

---

## 3. How It Works Internally

### Runtime Architecture Topology: Node.js vs Browser

```
+-----------------------------------------------------------------------------+
|                               NODE.JS RUNTIME                               |
|                                                                             |
|  [ Your Application JS Code ]                                               |
|               |                                                             |
|               v                                                             |
|  [ Node.js Standard Library (fs, net, http, crypto, path) ]                 |
|               |                                                             |
|               v                                                             |
|  [ C++ Bindings (Node C++ Addons / N-API) ]                                 |
|         |                                 |                                 |
|         v                                 v                                 |
|  [ V8 Engine ]                      [ libuv ]                               |
|  - Ignition Interpreter             - Event Loop                            |
|  - TurboFan JIT                     - Thread Pool (default 4 threads)       |
|  - Heap & Garbage Collection        - Epoll (Linux) / Kqueue (macOS) / IOCP |
|         |                                 |                                 |
|         +-----------------+---------------+                                 |
|                           |                                                 |
|                           v                                                 |
|               [ Operating System Kernel ]                                   |
+-----------------------------------------------------------------------------+
```

### The libuv Thread Pool
While JavaScript mutator code executes single-threaded on the main V8 execution thread, Node.js delegates blocking operating system tasks to the **libuv thread pool** (configurable via `UV_THREADPOOL_SIZE`, default 4):
1. **Asynchronous Filesystem Operations (`fs.*`)**
2. **DNS Lookups (`dns.lookup()`)**
3. **Cryptographic Functions (`crypto.pbkdf2()`, `crypto.randomBytes()`)**
4. **Zlib Compression (`zlib.*`)**
Non-blocking network I/O (TCP/UDP sockets) does *not* consume libuv threads; it utilizes OS asynchronous multiplexing mechanisms (`epoll`, `kqueue`, or `IOCP`) directly.

---

## 4. Syntax & Usage

### Cross-Runtime Environment Sniffing & Polyfills
To detect runtime capabilities portably across modern runtimes:

```javascript
export function getRuntimeContext() {
  if (typeof Bun !== 'undefined') {
    return { name: 'bun', version: Bun.version };
  }
  if (typeof Deno !== 'undefined') {
    return { name: 'deno', version: Deno.version.deno };
  }
  if (typeof process !== 'undefined' && process.release?.name === 'node') {
    return { name: 'node', version: process.version };
  }
  if (typeof window !== 'undefined' && typeof document !== 'undefined') {
    return { name: 'browser', version: navigator.userAgent };
  }
  return { name: 'unknown' };
}
```

---

## 5. Practical Examples

### Example 1: Demonstrating Unified `globalThis` Access
- **Goal:** Access environment variables safely across runtimes without reference crashes.
- **Code:**
```javascript
function getEnvVariable(key) {
  // Check Node.js / Bun
  if (typeof process !== 'undefined' && process.env) {
    return process.env[key];
  }
  // Check Deno
  if (typeof Deno !== 'undefined' && typeof Deno.env?.get === 'function') {
    return Deno.env.get(key);
  }
  return undefined;
}

const nodeEnv = getEnvVariable('NODE_ENV') || 'development';
console.log('Detected environment:', nodeEnv);
```
- **Output:**
```
Detected environment: development
```
- **Explanation:** Demonstrates non-throwing defensive runtime checking across Node.js and modern alternatives.

### Example 2: Event Loop Task Ordering Differences
- **Goal:** Verify that microtask queue resolution (`Promise`) takes precedence over macrotasks across all runtimes.
- **Code:**
```javascript
const executionLog = [];

executionLog.push('1: Synchronous Script');

setTimeout(() => {
  executionLog.push('4: Macrotask (setTimeout)');
  console.log('Final Execution Log:\n' + executionLog.join('\n'));
}, 0);

Promise.resolve().then(() => {
  executionLog.push('2: Microtask (Promise.then)');
});

queueMicrotask(() => {
  executionLog.push('3: Microtask (queueMicrotask)');
});

console.log('Immediate Stack Finished');
```
- **Output:**
```
Immediate Stack Finished
Final Execution Log:
1: Synchronous Script
2: Microtask (Promise.then)
3: Microtask (queueMicrotask)
4: Macrotask (setTimeout)
```
- **Explanation:** ECMA-262 mandates that before the host runtime can dispatch the next macrotask, the entire microtask queue must be drained to completion.

### Example 3: Edge Case with `Buffer` vs `Uint8Array`
- **Goal:** Show that Node's `Buffer` is a subclass of `Uint8Array`, but behaves differently on subarray mutation.
- **Code:**
```javascript
const uint8 = new Uint8Array([65, 66, 67]);
const buf = Buffer.from([65, 66, 67]);

console.log('Buffer is Uint8Array instance:', buf instanceof Uint8Array);
console.log('Uint8Array is Buffer instance:', uint8 instanceof Buffer);

// Buffer.subarray references shared memory in both
const sub = buf.subarray(1, 2);
sub[0] = 90; // 'Z'
console.log('Mutated Buffer original:', buf.toString());
```
- **Output:**
```
Buffer is Uint8Array instance: true
Uint8Array is Buffer instance: false
Mutated Buffer original: AZC
```
- **Explanation:** `Buffer` inherits from `Uint8Array`. Calling `.subarray()` produces an array buffer view over the exact same underlying memory block, so mutating `sub` immediately mutates `buf`.

### Example 4: Failure Mode of Thread Pool Exhaustion
- **Goal:** Expose how heavy cryptographic computations can saturate the 4 default libuv worker threads.
- **Code:**
```javascript
import crypto from 'node:crypto';

const start = Date.now();
function hashPassword(id) {
  crypto.pbkdf2('secret-password', 'salt', 100000, 64, 'sha512', () => {
    console.log(`Task ${id} completed in ${Date.now() - start}ms`);
  });
}

// 4 tasks run in parallel; task 5 is queued until a thread frees up!
for (let i = 1; i <= 5; i++) {
  hashPassword(i);
}
```
- **Output:**
```
Task 1 completed in 32ms
Task 2 completed in 34ms
Task 3 completed in 35ms
Task 4 completed in 36ms
Task 5 completed in 68ms
```
- **Explanation:** Tasks 1–4 consume the entire default 4-thread pool of libuv. Task 5 stalls until one of the initial 4 worker threads finishes, taking roughly double the elapsed wall-clock time.

### Example 5: Idiomatic Modern Web Standards in Node.js 22+
- **Goal:** Use native Fetch, Request, and Response without external packages like `node-fetch`.
- **Code:**
```javascript
// Web Platform API in Node.js 18+ and Node 22 LTS
const req = new Request('https://httpbin.org/get', {
  headers: { 'Accept': 'application/json' }
});

console.log('Request Method:', req.method);
console.log('Request Headers:', req.headers.get('accept'));
console.log('AbortSignal available on Request:', req.signal instanceof AbortSignal);
```
- **Output:**
```
Request Method: GET
Request Headers: application/json
AbortSignal available on Request: true
```
- **Explanation:** Modern Node.js natively supports the WHATWG Fetch standard (`Request`, `Response`, `Headers`, `FormData`), enabling unified isomorphic networking code across browser and server.

---

## 6. Best Practices

1. **Avoid Runtime-Specific Globals in Shared Code:** Do not use `window` or `process` in utility libraries; use standard `globalThis` and WHATWG APIs.
2. **Tune `UV_THREADPOOL_SIZE` for Crypto/Filesystem Heavy Apps:** If your Node service performs frequent image resizing, hashing, or compression, set `UV_THREADPOOL_SIZE=8` or `16` before launching the process.
3. **Use Web Standards for Networking:** Migrate away from legacy `http.request` in Node.js in favor of standard `fetch()` and `AbortSignal.timeout()`.
4. **Be Mindful of Garbage Collection Across Engines:** V8 and JavaScriptCore have different allocation thresholds; avoid creating short-lived high-volume objects in hot paths.
5. **Enforce Strict Isolation in Multi-Tenant Environments:** Do not rely on JavaScript lexical closures for security barriers; use Node's `node:vm` with caution, or prefer lightweight subprocesses / WebAssembly / Web Workers.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| **Assuming Node.js is completely single-threaded** | The JavaScript thread is single-threaded, but libuv offloads I/O, crypto, and zlib to a background thread pool. | Understand when operations block the event loop vs pool threads. |
| **Using `window` in isomorphic code** | Fails with `ReferenceError: window is not defined` when executed in Node.js or Web Workers. | Use `globalThis` or guard with `typeof window !== 'undefined'`. |
| **Blocking the Event Loop with synchronous CPU loops** | Running a synchronous `while(true)` or heavy JSON parse freezes all user connections or UI frames. | Break long tasks with `scheduler.yield()`, `setImmediate()`, or Web Workers. |
| **Relying on `process.env` in the browser** | `process` does not exist in browsers without bundler polyfill hacks (like Webpack DefinePlugin). | Use standard configuration injection or build-time environment flags. |
| **Confusing `setImmediate` and `setTimeout(0)`** | `setImmediate` is specific to Node.js and executes in the check phase; `setTimeout(0)` has a 4ms minimum clamp in browsers. | Use standard `queueMicrotask()` for microtasks or `setTimeout()` portably. |
| **Assuming Bun or Deno have 100% Node compatibility** | Edge APIs, legacy C++ native addons (`node-gyp`), and private Node internals may differ or fail. | Verify compatibility tables before migrating production servers. |

---

## 8. Performance & Security Notes

### Performance Notes
- **JIT Warmup Trade-Offs:** V8 and JavaScriptCore spend initial time interpreting and profiling before compiling to machine code. Short-lived CLI commands may execute faster with baseline compilers (Sparkplug / LLInt).
- **Hidden Class Pollution Across Runtimes:** Both V8 and JSC use hidden shape transitions; consistent object field initialization order remains the single most important performance practice across all engines.

### Security Notes
- **Deno & Bun Permissions:** Deno executes with zero access to network, filesystem, or environment variables unless explicitly permitted via flags (`--allow-net`, `--allow-read`).
- **Node.js Permission Model:** Introduced in Node 20+ (`--permission`), allows restricting access to filesystem paths and child processes.

---

## 9. Comparison / When to Use What

| Runtime / Engine | Language Engine | Primary Focus | Best Use Case |
| :--- | :--- | :--- | :--- |
| **Node.js (LTS 22)** | V8 | Production Stability & Massive Ecosystem | Enterprise backend services, large npm dependency trees |
| **Deno** | V8 | Security & Web Standards Compliance | Edge microservices, modern TypeScript applications |
| **Bun** | JavaScriptCore | Maximum Execution Speed & DX | Fast development tooling, local scripts, SSR rendering |
| **Browser (Chrome/Safari/FF)** | V8 / JSC / SpiderMonkey | Client Interactivity & Rendering | User interfaces, web applications |

---

## 10. Interview Questions

### Q1: What is the architectural difference between a JavaScript engine and a JavaScript runtime?
<details>
<summary>Answer</summary>
A JavaScript engine (such as V8, JavaScriptCore, or SpiderMonkey) is an abstract language virtual machine responsible solely for parsing ECMAScript source code, compiling it to bytecode, executing it via an interpreter or JIT compiler, allocating heap memory, and performing garbage collection. A runtime (such as Node.js, Bun, or a web browser) embeds an engine and provides the execution environment: the event loop, thread pools, platform-specific host APIs (DOM, filesystem, network sockets), and process lifecycle management.
</details>

### Q2: Is Node.js truly single-threaded? Explain with reference to libuv.
<details>
<summary>Answer</summary>
The JavaScript execution thread where mutator code and event callbacks run is strictly single-threaded. However, Node.js as an entire operating system process is multi-threaded. Node delegates blocking I/O and CPU-intensive operations (asynchronous filesystem operations, DNS lookups, cryptographic hashing, and compression) to **libuv's background worker thread pool** (default size of 4 threads). Furthermore, network I/O is handled non-blockingly via operating system kernel event notification mechanisms (`epoll`, `kqueue`, `IOCP`).
</details>

### Q3: What is the difference between V8's Ignition interpreter and TurboFan JIT compiler?
<details>
<summary>Answer</summary>
**Ignition** is V8's register-based bytecode interpreter. It compiles the Abstract Syntax Tree (AST) into compact, memory-efficient bytecode, enabling fast startup and low memory footprint while executing code and collecting inline type feedback. **TurboFan** is V8's optimizing compiler. When Ignition detects that a function is "hot" (invoked frequently) and receives stable type feedback, TurboFan compiles that bytecode into highly optimized native machine instructions with speculative optimizations. If type assumptions are violated, TurboFan deoptimizes and bails back to Ignition.
</details>

### Q4: Why does `setTimeout(fn, 0)` not execute immediately?
<details>
<summary>Answer</summary>
`setTimeout()` schedules a callback into the macrotask (timer) queue. In the browser and Node.js event loops, macrotask callbacks can only execute when:
1. The current synchronous call stack is completely empty.
2. The microtask queue (containing Promise reactions and `queueMicrotask` callbacks) has been drained to exhaustion.
3. In browsers, HTML5 specifies a minimum clamping delay of 4ms for nested timers after 5 successive timeouts.
</details>

### Q5: How does JavaScriptCore's architecture differ from V8?
<details>
<summary>Answer</summary>
JavaScriptCore (used in WebKit/Safari and Bun) utilizes a 4-tier execution pipeline:
1. **LLInt (Low-Level Interpreter):** Interprets bytecode with hand-written assembly.
2. **Baseline JIT:** Compiles hot functions quickly to simple machine code.
3. **DFG (Data Flow Graph) JIT:** Applies mid-tier optimizations with control flow analysis.
4. **FTL (Faster Than Light) JIT:** Generates high-end optimized native code using B3 (Bare Bones Backend).
V8, by contrast, uses a 3-tier pipeline: Ignition (interpreter) $\to$ Sparkplug (baseline) $\to$ Maglev (mid-tier) $\to$ TurboFan (optimizing compiler).
</details>

### Q6: What happens when the libuv thread pool is exhausted in Node.js?
<details>
<summary>Answer</summary>
When more than 4 concurrent operations requiring libuv threads (e.g. 5 concurrent `fs.readFile()` calls or `crypto.pbkdf2()` executions) are scheduled, the excess tasks are placed in libuv's internal pending queue. They cannot start executing until an active worker thread finishes and returns to the pool, resulting in latency spikes for queued operations.
</details>

### Q7: What are the security benefits of Deno's permission architecture compared to traditional Node.js?
<details>
<summary>Answer</summary>
Traditional Node.js grants all code (including third-party npm packages) unrestricted access to disk, network sockets, environment variables, and child processes. Deno executes inside a secure sandbox by default: scripts cannot access the filesystem, make outbound network requests, or read environment variables without explicit user permission flags passed at runtime (e.g. `--allow-net=api.example.com`, `--allow-read=/tmp`).
</details>

### Q8: How does Bun achieve faster package installation and execution compared to Node.js?
<details>
<summary>Answer</summary>
Bun is written in Zig and built on JavaScriptCore (which has faster startup times and lower memory overhead than V8 for ephemeral scripts). For package management, Bun utilizes hard links, global shared caches, and kernel-level zero-copy system calls (`copy_file_range` on Linux, `clonefile` on macOS), drastically reducing disk I/O during installs.
</details>

---

## 11. Practice Exercises

### Exercise 1: Universal Platform Environment Detector
**Task:** Write an isomorphic function `detectEnvironment()` that returns an object containing: `isBrowser`, `isNode`, `isDeno`, `isBun`, and `isWorker`.
<details>
<summary>Verified Solution</summary>

```javascript
function detectEnvironment() {
  const isBun = typeof Bun !== 'undefined';
  const isDeno = typeof Deno !== 'undefined';
  const isNode = !isBun && !isDeno && typeof process !== 'undefined' && process.release?.name === 'node';
  const isBrowser = typeof window !== 'undefined' && typeof document !== 'undefined';
  const isWorker = typeof importScripts === 'function' && typeof navigator !== 'undefined';

  return { isBrowser, isNode, isDeno, isBun, isWorker };
}

console.log('Environment Detection:', detectEnvironment());
```
</details>

### Exercise 2: Benchmarking Microtask vs Macrotask Latency
**Task:** Write a script that measures the precise elapsed time (in microseconds) between dispatching a `queueMicrotask` versus a `setTimeout(..., 0)`.
<details>
<summary>Verified Solution</summary>

```javascript
function benchmarkTaskLatency() {
  const t0 = performance.now();

  queueMicrotask(() => {
    const microDuration = (performance.now() - t0).toFixed(4);
    console.log(`Microtask latency: ${microDuration} ms`);
  });

  setTimeout(() => {
    const macroDuration = (performance.now() - t0).toFixed(4);
    console.log(`Macrotask latency: ${macroDuration} ms`);
  }, 0);
}

benchmarkTaskLatency();
```
</details>

### Exercise 3: Cross-Runtime High-Resolution Timer
**Task:** Write a function `getMicroseconds()` that returns monotonic high-resolution elapsed time across all runtimes using `performance.now()`.
<details>
<summary>Verified Solution</summary>

```javascript
function getMicroseconds() {
  if (typeof performance !== 'undefined' && typeof performance.now === 'function') {
    return Math.round(performance.now() * 1000);
  }
  return Date.now() * 1000;
}

const start = getMicroseconds();
let sum = 0;
for (let i = 0; i < 100000; i++) sum += i;
const elapsed = getMicroseconds() - start;

console.log(`Elapsed execution time: ${elapsed} µs`);
```
</details>

### Exercise 4: Simulating Thread Pool Bottlenecks Programmatically
**Task:** Write a diagnostic script that measures execution times for 8 concurrent cryptographic hashing tasks to prove thread pool queue latency.
<details>
<summary>Verified Solution</summary>

```javascript
import crypto from 'node:crypto';

async function measurePoolSaturation() {
  const start = performance.now();
  const tasks = Array.from({ length: 8 }, (_, i) => {
    return new Promise(resolve => {
      crypto.pbkdf2('pass', 'salt', 80000, 32, 'sha512', () => {
        const time = (performance.now() - start).toFixed(1);
        resolve({ task: i + 1, timeMs: Number(time) });
      });
    });
  });

  const results = await Promise.all(tasks);
  console.table(results);
}

measurePoolSaturation();
```
</details>

### Exercise 5: Universal Memory Footprint Reporter
**Task:** Write a utility `getMemoryStats()` that returns heap usage metrics in megabytes for Node.js, Bun, or Browser environments.
<details>
<summary>Verified Solution</summary>

```javascript
function getMemoryStats() {
  if (typeof process !== 'undefined' && typeof process.memoryUsage === 'function') {
    const mem = process.memoryUsage();
    return {
      heapUsedMB: (mem.heapUsed / 1024 / 1024).toFixed(2),
      heapTotalMB: (mem.heapTotal / 1024 / 1024).toFixed(2),
      rssMB: (mem.rss / 1024 / 1024).toFixed(2),
    };
  }
  if (typeof performance !== 'undefined' && performance.memory) {
    return {
      heapUsedMB: (performance.memory.usedJSHeapSize / 1024 / 1024).toFixed(2),
      heapTotalMB: (performance.memory.totalJSHeapSize / 1024 / 1024).toFixed(2),
    };
  }
  return { message: 'Memory metrics unavailable in this realm' };
}

console.log('Memory Stats:', getMemoryStats());
```
</details>

### Exercise 6: Safe Isomorphic Base64 Converter
**Task:** Write an isomorphic function `toBase64(str)` and `fromBase64(b64)` that works across Node.js (`Buffer`), modern engines (`Uint8Array.fromBase64`), and Browsers (`btoa`/`atob`).
<details>
<summary>Verified Solution</summary>

```javascript
function toBase64(str) {
  if (typeof Buffer !== 'undefined') {
    return Buffer.from(str, 'utf-8').toString('base64');
  }
  if (typeof btoa === 'function') {
    return btoa(unescape(encodeURIComponent(str)));
  }
  throw new Error('No base64 encoding available in this environment');
}

function fromBase64(b64) {
  if (typeof Buffer !== 'undefined') {
    return Buffer.from(b64, 'base64').toString('utf-8');
  }
  if (typeof atob === 'function') {
    return decodeURIComponent(escape(atob(b64)));
  }
  throw new Error('No base64 decoding available in this environment');
}

const encoded = toBase64('Hello JavaScript World!');
console.log('Encoded:', encoded);
console.log('Decoded:', fromBase64(encoded));
```
</details>

---

## 12. Summary / Cheat Sheet

- **Engine vs Runtime:** Engine executes bytecode and manages heap; Runtime supplies the event loop, thread pool, and host APIs.
- **Node.js Concurrency:** JavaScript is single-threaded; libuv offloads filesystem, crypto, and zlib to a background thread pool (default 4 threads).
- **Task Scheduling:** Call Stack $\to$ Microtasks (`Promise`, `queueMicrotask`) $\to$ Browser Rendering (rAF) $\to$ Macrotasks (`setTimeout`, `I/O`).
- **Universal Standards:** Use `globalThis` and WHATWG Fetch instead of environment-specific globals or legacy libraries.

---

## 13. Further Reading
- [V8 Engine Architecture & Ignition](https://v8.dev/docs/ignition)
- [Node.js Official Documentation & Libuv Guide](https://nodejs.org/en/docs/guides/event-loop-timers-and-nexttick/)
- [WebKit JavaScriptCore Architecture](https://webkit.org/blog/category/javascript/)
- [MDN Web APIs Reference](https://developer.mozilla.org/en-US/docs/Web/API)
