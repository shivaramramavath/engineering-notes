# Module 01: Node.js Runtime Fundamentals

> Core architectural principles, the libuv event loop, module systems, environment configuration, and process lifecycle mechanics.

---

## Module Overview

This module establishes the foundational mental model for server-side Node.js engineering: how V8 executes JavaScript, how libuv coordinates non-blocking asynchronous I/O across OS threads, how ES Modules and CommonJS interoperate natively, and how production processes manage their lifecycle and exit codes.

---

## Contents

| Index | Note | Core Focus |
| :---: | :--- | :--- |
| **01** | [01-node-runtime.md](./01-node-runtime.md) | V8 engine, libuv event loop phases, threadpool (`UV_THREADPOOL_SIZE`). |
| **02** | [02-global-objects.md](./02-global-objects.md) | Globals (`process`, `Buffer`), Web Platform APIs (`fetch`, Web Streams, SubtleCrypto). |
| **03** | [03-esm-vs-commonjs-in-node.md](./03-esm-vs-commonjs-in-node.md) | ESM vs CommonJS, native `require(esm)`, package exports. |
| **04** | [04-environment-variables.md](./04-environment-variables.md) | Built-in `--env-file`, `process.loadEnvFile`, Zod configuration validation. |
| **05** | [05-process-lifecycle-and-exit-codes.md](./05-process-lifecycle-and-exit-codes.md) | Process lifecycle, POSIX signals (`SIGTERM`, `SIGINT`), graceful shutdown, exit codes. |
| **06** | [06-error-handling-fundamentals.md](./06-error-handling-fundamentals.md) | Operational vs programmer errors, `uncaughtException`, `unhandledRejection`. |

---

## Next Steps
Proceed to [02-core-modules](../02-core-modules/README.md) to dive deep into Node's standard library: streams, buffers, SQLite, worker threads, and single-executable applications.
