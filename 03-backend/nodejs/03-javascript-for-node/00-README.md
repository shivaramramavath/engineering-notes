# 03 — JavaScript for Node

Node runs plain JavaScript, but a few language features and mechanisms matter far more in a Node backend than they typically do in simple frontend scripts — asynchronous code above all. This section covers the JavaScript concepts that come up constantly once you're writing real Node applications.

## In this section

| File                                   | Covers                                                                                                           |
| -------------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| `01-callbacks-promises-async-await.md` | The evolution of async JavaScript — callbacks, Promises, and `async`/`await` — and how they relate               |
| `02-event-loop.md`                     | How Node schedules callbacks, microtasks, and I/O — the mechanism underneath everything in `01`                  |
| `03-closures.md`                       | Functions that remember the scope they were created in — used constantly in middleware, factories, and callbacks |
| `04-prototypes.md`                     | JavaScript's prototype-based inheritance, and how `class` relates to it                                          |
| `05-error-handling.md`                 | `try/catch`, error propagation through callbacks/promises, and custom error classes                              |
| `06-memory-management.md`              | How Node's garbage collector works, and the common causes of memory leaks                                        |

## Why this comes before frameworks

Express middleware, database drivers, and queue libraries all lean heavily on closures, Promises, and careful error handling. Debugging a confusing async bug, a memory leak, or an uncaught rejection in `06-express/` or `11-async-processing/` is far easier once the mechanisms here are second nature, rather than something to reverse-engineer under pressure.

## What you should be able to do after this section

- Convert between callback-style, Promise-based, and `async`/`await` code, and explain why `async`/`await` won out
- Explain what the event loop actually does, and why `setTimeout(fn, 0)` doesn't run immediately
- Use closures deliberately (e.g. to build configurable middleware) rather than by accident
- Explain how `class` in JavaScript relates to prototypes underneath
- Handle errors correctly across sync code, callbacks, and Promises — including the ones that are easy to accidentally swallow
- Recognize common memory leak patterns in a long-running Node process

## Next

**`04-npm-ecosystem`** covers using npm effectively day to day — versioning, scripts, and publishing.
