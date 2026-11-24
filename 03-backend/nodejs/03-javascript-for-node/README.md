# Module 03: JavaScript for Node.js Backends

> Server-side JavaScript mechanics, async coordination, EventEmitter architectures, and V8 memory management.

---

## Module Overview

While general JavaScript language concepts reside in [01-languages/javascript/](../../01-languages/javascript/README.md), server-side Node.js engineering requires deep understanding of how asynchronous tasks, closures, and object references interact with the single-threaded V8 runtime and multi-tenant memory constraints.

---

## Contents

| Index | Note | Core Focus |
| :---: | :--- | :--- |
| **01** | [01-callbacks-promises-async-await.md](./01-callbacks-promises-async-await.md) | Asynchronous evolution, promise chaining, async/await microtasks. |
| **02** | [02-event-loop.md](./02-event-loop.md) | Macrotasks vs microtasks (`process.nextTick`, `queueMicrotask`). |
| **03** | [03-closures.md](./03-closures.md) | Lexical scoping, factory functions, memory retention in closures. |
| **04** | [04-prototypes.md](./04-prototypes.md) | Prototype inheritance, `Object.create(null)`, prototype pollution defenses. |
| **05** | [05-error-handling.md](./05-error-handling.md) | Try/catch, error bubbling, asynchronous stack traces. |
| **06** | [06-memory-management.md](./06-memory-management.md) | V8 garbage collector (Scavenge vs Mark-Sweep-Compact), heap spaces. |
| **07** | [07-server-async-patterns-and-event-emitter.md](./07-server-async-patterns-and-event-emitter.md) | Concurrency limiting (`p-limit`), `AbortController`, EventEmitter leaks. |
| **08** | [08-memory-leaks-and-heap-profiling.md](./08-memory-leaks-and-heap-profiling.md) | Heap snapshots (`v8.getHeapSnapshot`), Chrome DevTools memory profiling. |

---

## Next Steps
Proceed to [04-npm-ecosystem](../04-npm-ecosystem/README.md) to explore modern package management, exports fields, and monorepo workspaces.
