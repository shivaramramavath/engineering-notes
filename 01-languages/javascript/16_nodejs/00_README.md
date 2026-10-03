# 16 · Node.js

Node.js is a JavaScript runtime built on V8 that runs outside the browser. This chapter covers the parts of Node you need to build real tools and servers: how the runtime is put together, how its event loop differs from the browser's, and the core modules (`process`, `fs`, `events`, `stream`, `buffer`, `http`, `child_process`, `cluster`).

## What you will learn

- How Node is built (V8 + libuv + C++ bindings) and what that means in practice
- The Node event loop phases, `process.nextTick`, `setImmediate`, and the thread pool
- Reading arguments, environment variables, signals, and exit codes
- Working with files and paths safely
- Building on `EventEmitter`
- Streaming data with backpressure instead of loading everything into memory
- Handling binary data with `Buffer`
- Writing an HTTP server with only the standard library
- Running other programs and using multiple CPU cores

## Contents

| # | File | Topic |
|---|------|-------|
| 01 | [Node Runtime](./01_node-runtime.md) | V8, libuv, globals, built-in modules, CLI, versions |
| 02 | [Node Event Loop](./02_node-event-loop.md) | Phases, `nextTick`, `setImmediate`, thread pool |
| 03 | [Process and Env](./03_process-and-env.md) | `process`, argv, env vars, signals, exit codes |
| 04 | [Filesystem and Path](./04_filesystem-and-path.md) | `fs/promises`, `path`, `URL`, path safety |
| 05 | [Events](./05_events.md) | `EventEmitter`, `once`, `on`, error events |
| 06 | [Streams](./06_streams.md) | Readable, Writable, Transform, `pipeline`, backpressure |
| 07 | [Buffers](./07_buffers.md) | `Buffer`, encodings, binary parsing |
| 08 | [HTTP Server](./08_http-server.md) | `node:http`, routing, bodies, graceful shutdown |
| 09 | [Child Process and Cluster](./09_child-process-and-cluster.md) | `spawn`, `fork`, `cluster`, multi-core servers |

## Prerequisites

- [Asynchronous JavaScript](../11_asynchronous-javascript/00_README.md): promises and `async`/`await`
- [Event Loop](../12_event-loop/00_README.md): microtasks vs macrotasks
- [Modules](../13_modules/00_README.md): ES modules and CommonJS
- [Networking](../15_networking/00_README.md): HTTP basics

## How to practice

Every file has runnable snippets. Create a scratch folder, add `"type": "module"` to `package.json` (or use `.mjs` files), and run examples with `node file.mjs`.

```bash
mkdir node-playground && cd node-playground
npm init -y
npm pkg set type=module
```

## Key takeaways

- Node is a runtime plus a standard library, not just "JavaScript on a server"
- Most Node code is I/O bound: understand the event loop before reaching for threads
- Prefer streams and `async` APIs over blocking calls in servers
- The standard library is enough to build a working server, CLI, or process manager

**Next:** [Node Runtime](./01_node-runtime.md)
