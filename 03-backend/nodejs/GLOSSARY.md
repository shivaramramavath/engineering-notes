# Node.js Engineering Glossary

> Authoritative definitions and technical mechanics of core Node.js runtime concepts, event loop phases, concurrency models, and backend patterns.

---

### A
- **AsyncLocalStorage**: A built-in class (`node:async_hooks`) providing ambient request-scoped state storage across asynchronous execution chains (commonly used for request IDs and distributed tracing spans).
- **Autocannon**: A fast HTTP/1.1 benchmarking tool written in Node.js capable of generating high-throughput load to measure API latency percentiles and requests per second.

### B
- **Backpressure**: The resistance or force opposing the desired flow of data in a stream when the consumer is slower than the producer. Handled natively in Node streams when `.write()` returns `false` and waits for `'drain'`.
- **Buffer**: A low-level Node.js class allocating fixed-size raw memory outside the V8 JavaScript heap to handle binary data (TCP streams, file I/O).
- **BullMQ**: Modern Redis-backed message queue and distributed job scheduler for Node.js featuring parent-child job flows, rate-limiting, and exponential backoff retries.

### C
- **Cluster**: A built-in core module (`node:cluster`) allowing Node.js to spawn child worker processes that share server ports across multi-core CPUs via OS socket sharing.
- **CommonJS (CJS)**: The legacy synchronous module format in Node.js characterized by `require()` and `module.exports`.

### D
- **Drizzle ORM**: A lightweight, TypeScript-first SQL query builder and ORM that compiles directly to SQL without runtime overhead or external query engine binaries.

### E
- **ES Modules (ESM)**: The official ECMAScript standard module system characterized by static `import` and `export` statements and asynchronous module loading.
- **Event Loop**: The libuv-powered single-threaded scheduling mechanism that orchestrates the execution of callbacks across distinct phases: Timers, Pending I/O, Idle/Prepare, Poll, Check (`setImmediate`), and Close callbacks.
- **EventEmitter**: Core Node.js class (`node:events`) implementing the Observer pattern for event-driven asynchronous communication.

### F
- **Fastify**: A high-performance web framework for Node.js focused on low overhead, JSON Schema compilation, and robust plugin encapsulation.

### H
- **Hono**: An ultra-fast, lightweight web framework built on standard Web Fetch APIs that runs natively across Node.js, Deno, Bun, and edge workers.

### I
- **Idempotency Key**: A unique client-provided token sent in HTTP request headers (`Idempotency-Key`) preventing duplicate mutations during network retries.

### L
- **libuv**: A multi-platform C library providing Node.js with its event loop, non-blocking asynchronous I/O abstractions, and default 4-thread worker pool (`UV_THREADPOOL_SIZE`).

### N
- **`node:sqlite`**: The built-in synchronous SQLite engine in Node.js providing in-memory or file-backed relational storage without external npm dependencies.
- **`node:test`**: The built-in test runner module in Node.js supporting suites, assertions, mocks, and code coverage.

### P
- **Permission Model**: Built-in runtime security feature in Node.js (`--permission`) restricting access to the file system, child processes, and native addons.
- **Pino**: An ultra-fast, zero-overhead JSON logger for Node.js that offloads string serialization to worker threads.

### S
- **Single Executable Applications (SEA)**: Feature allowing packaging of a Node.js application, its dependencies, and assets into a single standalone binary.
- **Stream**: An abstract interface in Node.js for working with streaming data. Four base types: `Readable`, `Writable`, `Duplex`, and `Transform`.

### T
- **Turbopack**: Incremental bundler written in Rust for high-speed module compilation.
- **Type Stripping**: Native Node.js capability (`--experimental-strip-types` / default in Node 22.18+) allowing direct execution of `.ts` files by removing type syntax without compilation.
