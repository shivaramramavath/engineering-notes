# Module 06: Closures

> Architectural guide to JavaScript closures, lexical environment retention on the heap, function factories, the Revealing Module Pattern, memory leak mitigation, and heap snapshot diagnostics.

## Overview & Curriculum Map

A closure is the combination of a function bundled together with references to its surrounding lexical environment (`[[Environment]]`). In JavaScript, functions outlive their parent execution contexts, causing the engine to persist captured variables inside heap-allocated `Context` structures.

This module explores the mechanics of lexical retention, production design patterns (memoization, currying, rate limiters, debounce/throttle), and hazardous pitfalls (the Meteor shared-context leak, stale closures, and Chrome DevTools heap profiling).

---

## Complete Note Index

| Order | Note | Level | Topics Covered | Status |
| :---: | :--- | :---: | :--- | :---: |
| 01 | [01_closures.md](./01_closures.md) | Core / Inter | Lexical closure definition, `[[Environment]]` internal slot, live variable references vs value snapshots, V8 Context allocation, `var` vs `let` loop mechanics | Complete |
| 02 | [02_closure-use-cases.md](./02_closure-use-cases.md) | Intermediate | Revealing Module Pattern, private state factories, currying, parametric decorators, memoization caches, production debounce & throttle | Complete |
| 03 | [03_closure-pitfalls.md](./03_closure-pitfalls.md) | Advanced | Memory leaks, the Meteor shared-context bug, stale closures in timers and React hooks, Chrome DevTools heap snapshots, lifecycle disposal | Complete |

---

## Learning Objectives & Mastery Checklist

By completing this module, you will be able to:
- [x] Trace how the V8 engine transitions stack-allocated local variables into heap-allocated `Context` objects when captured by escaping closures.
- [x] Implement resilient data encapsulation using factory closures and the Revealing Module Pattern without relying on class keywords.
- [x] Design high-performance memoization wrappers with LRU eviction policies to prevent unbounded memory growth.
- [x] Prevent and resolve stale closure bugs in asynchronous timers (`setInterval`) and event subscriptions using functional state updates.
- [x] Identify and remediate shared-context memory leaks (the Meteor bug) and profile heap allocations using Chrome DevTools memory snapshots.
- [x] Formulate clean lifecycle teardown patterns (`dispose()`) to sever closure references and prevent GC leaks in long-running Node.js processes.

---

## Verification & Execution Baseline

All code examples, edge cases, and performance benchmarks in this module are tested and verified against:
- **Node.js**: `v24.11.1` / `v22.x LTS`
- **ECMAScript Standards**: ES1 through ES2024
- **Engine Targets**: Google V8 (Chromium / Node.js), JavaScriptCore (Safari), SpiderMonkey (Firefox)
