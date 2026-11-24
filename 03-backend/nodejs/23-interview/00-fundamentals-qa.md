# Node.js Technical Interview: Runtime & Fundamentals Q&A

> Rigorous technical questions and in-depth architectural answers covering Node.js runtime fundamentals, the V8 JavaScript engine, process lifecycles, module systems, and native features in Node.js 22 LTS / 24+.

---

### Q1: Is Node.js truly single-threaded? Explain what executes on the main thread and what does not.

#### In-Depth Answer
Node.js is **single-threaded from the perspective of user JavaScript execution**, but **multi-threaded under the hood**:
1. **The Main Thread (V8 Event Loop)**: All JavaScript code, event loop phases (Timers, Poll, Check), microtask queues, and callback executions run strictly on a single thread (the main V8 thread). If you block this thread with a heavy synchronous calculation (`while(true)` or cryptographic computation), no other callbacks or network I/O can be processed.
2. **The libuv Threadpool**: By default, libuv initializes a background worker threadpool with 4 threads (`UV_THREADPOOL_SIZE=4`, expandable up to 1024). This pool handles operations that OS kernels cannot execute asynchronously through non-blocking I/O event notifications:
   - File system I/O (`fs.readFile`, `fs.writeFile`, `fs.stat`).
   - DNS resolution (`dns.lookup` uses `getaddrinfo(3)` which is blocking at the OS level; note that `dns.resolve*` uses non-blocking c-ares sockets and does *not* use the thread pool).
   - CPU-bound crypto operations (`crypto.pbkdf2`, `crypto.scrypt`, `crypto.generateKeyPair`).
   - Compression algorithms (`zlib.*`).
3. **OS Kernel Asynchronous I/O (epoll / kqueue / IOCP)**: Network sockets (TCP, UDP, HTTP, TLS) do **not** use the libuv threadpool. Modern operating systems provide asynchronous readiness notifications (`epoll` on Linux, `kqueue` on macOS/BSD, and `IOCP` on Windows). A single thread can monitor tens of thousands of open network sockets without creating OS threads.
4. **V8 Background Auxiliary Threads**: V8 spawns background threads for concurrent and incremental Garbage Collection (GC mark-sweep tasks) and JIT compilation (TurboFan / Maglev).
5. **Worker Threads (`node:worker_threads`)**: Applications can explicitly spawn true parallel OS threads sharing memory via `SharedArrayBuffer` for CPU-intensive tasks.

---

### Q2: What are the fundamental differences between CommonJS (CJS) and ECMAScript Modules (ESM) in Node.js? How does `require(esm)` change the landscape in Node.js 22+?

#### In-Depth Answer

| Feature | CommonJS (CJS) | ECMAScript Modules (ESM) |
| :--- | :--- | :--- |
| **Specification** | Community standard (early Node.js) | Official ECMA-262 Language Standard |
| **Loading Mechanism** | **Synchronous & Dynamic**: files are read, parsed, and evaluated synchronously at runtime. | **Asynchronous & Static**: 3-phase loading: Construction (fetching/parsing), Instantiation, and Evaluation. |
| **Top-Level `await`** | Not supported (Syntax error) | Fully supported natively |
| **Tree-Shaking** | Extremely difficult (exports are mutable runtime objects: `module.exports`) | First-class support via static `import { foo } from 'bar'` analysis |
| **Scope Globals** | Injected function wrapper parameters: `__dirname`, `__filename`, `exports`, `module`, `require` | Standard globals: `import.meta.url`, `import.meta.dirname` (added in Node 20.11+/22+) |
| **Circular Dependencies**| Returns partially evaluated export object | Returns live uninitialized binding; access before evaluation throws `ReferenceError: Cannot access 'X' before initialization` (TDZ) |

#### Modern Shift: `require(esm)` in Node.js 22 LTS / 23+
Historically, CommonJS could not load ESM synchronously; attempting `require('esm-package')` threw `ERR_REQUIRE_ESM`, forcing packages into dual-publishing or awkward dynamic `await import()`.
Starting in **Node.js 22.12.0 and Node.js 23+ (enabled by default)**, synchronous `require(esm)` is supported provided the target ESM module does **not** contain top-level `await`. If the ESM file contains top-level `await`, `require()` throws `ERR_REQUIRE_ASYNC_MODULE`.

```javascript
// Valid in Node.js 22.12+ / 24+ without experimental flags:
const { add } = require('./math.mjs'); // Loads synchronous ESM file cleanly!
```

---

### Q3: Explain the Node.js Process Lifecycle and Exit Codes. What is the difference between `process.on('exit')` and `process.on('beforeExit')`?

#### In-Depth Answer
The Node.js process exits when:
1. The libuv event loop has no further work scheduled (no active timers, no open network sockets, no active child processes, and no registered event listeners).
2. Code explicitly calls `process.exit(code)`.
3. An unhandled exception or termination signal (`SIGTERM`, `SIGINT`, `SIGHUP`) is received.

#### `beforeExit` vs `exit` Lifecycle Hooks:
- **`beforeExit`**: Emitted when Node.js empties its event loop and is about to exit. Callbacks registered to `beforeExit` **can make asynchronous calls** (e.g. scheduling a timer or dispatching an HTTP metric). Doing so adds work to the event loop, causing the process to continue running! `beforeExit` is **not** called if `process.exit()` is invoked explicitly or for uncaught exceptions.
- **`exit`**: Emitted when the process has definitively decided to terminate. The event loop is torn down. **Only synchronous operations are permitted**. Any asynchronous callback (e.g. `setTimeout`, database queries, promise resolutions) scheduled inside `process.on('exit')` will be discarded silently and will never run.

```javascript
process.on('beforeExit', (code) => {
  console.log('Event loop empty! We can still do async work:', code);
  setTimeout(() => console.log('This will execute and delay exit!'), 100);
});

process.on('exit', (code) => {
  // Synchronous cleanup only!
  console.log(`Process exited with code: ${code}`);
  setTimeout(() => console.log('NEVER PRINTED!'), 100);
});
```

#### Standard Exit Codes to Know in Interviews:
- `0`: Success (clean exit).
- `1`: Uncaught fatal exception.
- `13`: Unfinished Top-Level Await promise rejection.
- `128 + N`: Terminated by OS signal (e.g., `143` = `128 + 15` for `SIGTERM`; `130` = `128 + 2` for `SIGINT` / Ctrl+C).

---

### Q4: How does Node.js 22 natively support TypeScript? What does `--experimental-strip-types` do, and what are its constraints?

#### In-Depth Answer
Starting in Node.js 22.6.0+ and active in Node.js 22 LTS, Node includes native type stripping via the Amaro library (a WebAssembly build of the SWC Rust compiler):
- **Mechanism**: Node.js does not perform type checking. Instead, before feeding the source code into V8, it replaces all TypeScript type annotations, interfaces, type aliases, and type assertions with blank whitespace (preserving identical line and column numbers for accurate source maps and debugging stack traces).
- **Execution**: Can be executed directly: `node app.ts` (or `node --experimental-strip-types app.ts` in earlier 22.x builds).
- **Supported Features**: Standard TypeScript type syntax, type imports (`import type { User } from './user.js'`), generics, and type parameters.
- **Unsupported Features (Intentional Constraints)**: Features that require code generation or runtime JavaScript emissions are **not** supported:
  1. `enum` (must use `const` objects with `as const` or string unions).
  2. `namespaces` / `module` declarations.
  3. Legacy TypeScript parameter properties in class constructors (e.g. `constructor(public name: string)`).
  4. Non-standard legacy experimental decorators (standard Stage 3 ECMAScript decorators *are* supported).

---

### Q5: What is the Node.js Permission Model (`--permission`), and how does it safeguard production servers from malicious npm supply-chain dependencies?

#### In-Depth Answer
Node.js introduced an experimental Permission Model inspired by Deno/browsers to restrict process capabilities without relying on external OS sandboxes (like Docker or seccomp).
When activated with `node --permission`, Node.js blocks unauthorized access to:
1. **File System**:
   - `node --permission --allow-fs-read="/app/*" --allow-fs-write="/tmp/*"`
   - Prevents compromised npm dependencies from reading `/etc/passwd`, `~/.ssh/id_rsa`, or environment variable files.
2. **Child Process Execution**:
   - Spawning arbitrary shell commands (`child_process.exec`, `child_process.spawn`) is blocked unless explicitly granted via `--allow-child-process`.
3. **Worker Threads**:
   - Spawning threads is blocked unless granted via `--allow-worker`.

```javascript
// Runtime permission checking API:
if (process.permission.has('fs.read', '/var/log/app.log')) {
  // Allowed to read
} else {
  console.warn('Read permission denied by security policy');
}
```
In a senior interview, emphasize that while the Permission Model protects file and process boundaries, network access control (`--allow-net`) is still in development; outbound egress controls should still be reinforced with OS firewalls and container network policies.
