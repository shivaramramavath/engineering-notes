# Module 02: Core Standard Modules In-Depth

> Deep dive into the Node.js standard library: streams, buffers, file operations, processes, concurrency, built-in SQLite, and the native permission model.

---

## Module Overview

Node.js includes a rich, highly optimized standard library designed for asynchronous networking, file handling, binary data manipulation, and multi-core process clustering. This module explores both established core modules and cutting-edge additions like `node:sqlite`, single-executable packaging (SEA), and the runtime permission model.

---

## Contents

| Index | Note | Core Focus |
| :---: | :--- | :--- |
| **01** | [01-fs.md](./01-fs.md) | File system operations, `fs/promises`, streaming file I/O, file watching. |
| **02** | [02-path.md](./02-path.md) | Path manipulation, normalization, POSIX vs Windows paths. |
| **03** | [03-http.md](./03-http.md) | Low-level HTTP/HTTPS servers, socket pooling, agent configurations. |
| **04** | [04-events.md](./04-events.md) | `EventEmitter`, event listeners, memory leak detection, event-driven architecture. |
| **05** | [05-streams.md](./05-streams.md) | Readable, Writable, Duplex, Transform streams, backpressure, `stream.pipeline()`. |
| **06** | [06-buffer.md](./06-buffer.md) | Raw binary allocation, encoding conversions, memory safety with `Buffer.alloc()`. |
| **07** | [07-process.md](./07-process.md) | Process metadata, memory usage (`process.memoryUsage()`), stdout/stderr. |
| **08** | [08-os.md](./08-os.md) | Operating system information, CPU architectures, network interfaces. |
| **09** | [09-child-process.md](./09-child-process.md) | Spawning subprocesses (`spawn`, `exec`, `fork`), IPC communication. |
| **10** | [10-cluster-worker-threads.md](./10-cluster-worker-threads.md) | Multi-core parallelism, `cluster` module, `worker_threads` for CPU-bound tasks. |
| **11** | [11-node-sqlite-and-test-runner.md](./11-node-sqlite-and-test-runner.md) | Built-in `node:sqlite` relational database, zero-dependency `node:test` runner. |
| **12** | [12-permissions-model.md](./12-permissions-model.md) | Granular runtime permissions (`--permission`, `--allow-fs-read`, `process.permission`). |
| **13** | [13-single-executable-applications.md](./13-single-executable-applications.md) | Standalone binary generation via `--build-sea`, embedding assets with `node:sea`. |
| **14** | [14-diagnostics-channel-and-asynclocalstorage.md](./14-diagnostics-channel-and-asynclocalstorage.md) | Request-scoped context with `AsyncLocalStorage`, decoupled APM tracing. |

---

## Next Steps
Proceed to [03-javascript-for-node](../03-javascript-for-node/README.md) to examine server-side async execution patterns, EventEmitter idioms, and V8 garbage collection behavior.
