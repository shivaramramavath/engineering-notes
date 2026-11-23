# JavaScript Engineering Curriculum

> An exhaustive, production-grade, zero-dependency masterclass in modern JavaScript (ECMAScript 2024 / ECMAScript 2025, Node.js 22 LTS). Covers language foundations, runtime execution, asynchronous concurrency, V8 compiler internals, data structures, full-stack projects, and senior interview mastery.

---

## Curriculum Overview

This curriculum provides a definitive, engineering-first education in the JavaScript programming language. Moving beyond surface-level syntax tutorials, it bridges formal ECMAScript specifications with empirical V8 engine internals, memory models, hardware-accelerated browser rendering, and full-stack software architecture.

```
                              JAVASCRIPT ENGINEERING TRACK
                                           │
         ┌───────────────────┬─────────────┴─────────────┬───────────────────┐
         ▼                   ▼                           ▼                   ▼
    [Core & Runtime]    [Async & Concurrency]       [DSA & Architecture]  [Applied Systems]
    - 00_setup          - 11_asynchronous-js        - 20_design-patterns  - 25_projects (21)
    - 01_fundamentals   - 12_event-loop             - 21_testing          - 26_interview
    - 02_functions      - 13_modules                - 22_security         - 27_cheatsheets
    - 03_objects-arrays - 14_dom-and-browser        - 23_real-world-pat   - _templates
    - 04_scope-exec     - 15_networking             - 24_dsa-in-js (18)
    - 05_this-and-oop   - 16_nodejs
    - 06_closures       - 17_concurrency-parallel
    - 07_functional     - 18_memory-gc
    - 08_modern-js      - 19_performance
    - 09_built-ins
    - 10_error-handling
```

---

## Specification & Runtime Baselines

All curriculum materials, code examples, test harnesses, and architectural specifications are calibrated to the following baselines:

- **Language Specification**: ECMAScript 2024 (15th Edition) / ECMAScript 2025 Stage 4.
  - Tagged features indicate precise standardization: `"Since ES2015"`, `"Since ES2020"`, `"Since ES2024"`.
- **Runtime Environment**: Node.js v22.x LTS / Node.js v24 Current.
  - Native ES module execution (`"type": "module"`).
  - Native test runner (`node:test` & `node:assert/strict`).
  - Zero external npm runtime dependencies.
- **Browser Standard**: Baseline Widely Available across all major evergreen engines (Chromium / Blink, Firefox / Gecko, Safari / WebKit).

---

## Curriculum Modules Directory

| Module | Title | Scope & Core Topics | Notes Count |
| :--- | :--- | :--- | :--- |
| [`00_setup`](file:///e:/Learning/engineering-notes/01-languages/javascript/00_setup) | Environment & Tooling | Node.js, npm, nvm/fnm, pnpm, Biome, ESLint, TypeScript integration | 7 notes |
| [`01_fundamentals`](file:///e:/Learning/engineering-notes/01-languages/javascript/01_fundamentals) | Language Fundamentals | Types, Variables, Coercion, Control Flow, Operators, Bitwise, Loops | 10 notes |
| [`02_functions`](file:///e:/Learning/engineering-notes/01-languages/javascript/02_functions) | Functions & Invocations | Declarations, Expressions, Arrows, Parameters, Rest/Spread, Currying | 10 notes |
| [`03_objects-and-arrays`](file:///e:/Learning/engineering-notes/01-languages/javascript/03_objects-and-arrays) | Objects & Collections | Object Literals, Methods, Arrays, Iteration, Descriptors, Proxies, Reflect | 13 notes |
| [`04_scope-and-execution`](file:///e:/Learning/engineering-notes/01-languages/javascript/04_scope-and-execution) | Execution & Scopes | Execution Context, Lexical Scope, Hoisting, TDZ, Scope Chain | 6 notes |
| [`05_this-and-oop`](file:///e:/Learning/engineering-notes/01-languages/javascript/05_this-and-oop) | `this` & Object Models | 4 Binding Rules, Prototypes, ES6 Classes, Private Fields, Mixins | 9 notes |
| [`06_closures`](file:///e:/Learning/engineering-notes/01-languages/javascript/06_closures) | Closure Mechanics | Heap Context Allocation, Private State, Factory Functions, Leaks | 4 notes |
| [`07_functional-programming`](file:///e:/Learning/engineering-notes/01-languages/javascript/07_functional-programming) | Functional JavaScript | Pure Functions, Immutability, Composition, Monads, Functors | 8 notes |
| [`08_modern-javascript`](file:///e:/Learning/engineering-notes/01-languages/javascript/08_modern-javascript) | ES6+ Modern Evolution | Destructuring, Template Literals, Symbols, Iterators, Optional Chaining | 7 notes |
| [`09_built-in-objects`](file:///e:/Learning/engineering-notes/01-languages/javascript/09_built-in-objects) | Standard Built-Ins | Math, Date, RegExp, Map, Set, WeakMap, WeakSet, JSON, Intl | 13 notes |
| [`10_error-handling`](file:///e:/Learning/engineering-notes/01-languages/javascript/10_error-handling) | Error Handling & Auditing | Error Objects, Custom Errors, Asynchronous Trapping, Error Boundaries | 6 notes |
| [`11_asynchronous-javascript`](file:///e:/Learning/engineering-notes/01-languages/javascript/11_asynchronous-javascript) | Asynchronous Architecture | Callbacks, Promises/A+, Async/Await, Combinators, AbortSignal | 10 notes |
| [`12_event-loop`](file:///e:/Learning/engineering-notes/01-languages/javascript/12_event-loop) | Event Loop Internals | Call Stack, Microtasks, Task Queue, Render Steps, libuv Phases | 6 notes |
| [`13_modules`](file:///e:/Learning/engineering-notes/01-languages/javascript/13_modules) | Module Architectures | ESM Static Trees, CommonJS Runtime Shim, Dynamic Imports, Bundling | 6 notes |
| [`14_dom-and-browser`](file:///e:/Learning/engineering-notes/01-languages/javascript/14_dom-and-browser) | Browser DOM & APIs | Critical Rendering Path, Layout Thrashing, Event Propagation, Observers | 11 notes |
| [`15_networking`](file:///e:/Learning/engineering-notes/01-languages/javascript/15_networking) | Web Networking | Fetch API, Streams, WebSockets, Server-Sent Events, CORS, Headers | 7 notes |
| [`16_nodejs`](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs) | Node.js Runtime Core | Global Objects, Streams, Buffer, Process Lifecycle, native `fs`/`path` | 10 notes |
| [`17_concurrency-and-parallelism`](file:///e:/Learning/engineering-notes/01-languages/javascript/17_concurrency-and-parallelism) | Parallelism & Workers | Web Workers, Node Worker Threads, SharedArrayBuffer, Atomics | 6 notes |
| [`18_memory-and-garbage-collection`](file:///e:/Learning/engineering-notes/01-languages/javascript/18_memory-and-garbage-collection) | Memory Management | V8 Managed Heap, Scavenger Minor GC, Major GC Mark-Sweep, Heap Dumps | 5 notes |
| [`19_performance`](file:///e:/Learning/engineering-notes/01-languages/javascript/19_performance) | Engine Optimization | Ignition, Turbofan, Hidden Classes (Shapes), Monomorphic ICs, Profiling | 6 notes |
| [`20_design-patterns`](file:///e:/Learning/engineering-notes/01-languages/javascript/20_design-patterns) | Software Design Patterns | Factory, Singleton, Strategy, Observer, Decorator, Dependency Injection | 9 notes |
| [`21_testing`](file:///e:/Learning/engineering-notes/01-languages/javascript/21_testing) | Testing & Verification | Unit, Integration, Mocking, Test Runners (Node Native, Vitest, Jest) | 6 notes |
| [`22_security`](file:///e:/Learning/engineering-notes/01-languages/javascript/22_security) | Application Security | XSS, CSRF, Prototype Pollution Prevention, CSP, Input Sanitization | 6 notes |
| [`23_real-world-patterns`](file:///e:/Learning/engineering-notes/01-languages/javascript/23_real-world-patterns) | Real-World Engineering | Debounce, Throttle, Polling, Retry Backoff, Virtualization, Rate Limiting | 9 notes |
| [`24_dsa-in-javascript`](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript) | Data Structures & Algorithms | Big-O, Hash Tables, Linked Lists, Trees, Graphs, DP, Trie, DSU | 18 notes |
| [`25_projects`](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects) | Real-World Projects | 21 complete projects: VDOM, Redux Store, SPA Router, Bundler, Scraper | 21 projects |
| [`26_interview`](file:///e:/Learning/engineering-notes/01-languages/javascript/26_interview) | Interview Mastery | Senior Q&A, Event Loop, Output Puzzles, Machine Coding, Polyfills | 9 guides |
| [`27_cheatsheets`](file:///e:/Learning/engineering-notes/01-languages/javascript/27_cheatsheets) | Quick-Reference Sheets | Syntax, Arrays/Objects, Async, Regex, DevTools, Node Modules, Gotchas | 13 sheets |

---

## Pedagogical Standard & Note Structure

Every conceptual note in modules `00` through `24` strictly follows the authoritative 13-section template:
1. **What & Why**: Historical context, design rationale, and exact problems solved.
2. **Core Concepts**: Foundational principles and terminologies.
3. **How It Works Internally**: Step-by-step engine execution mechanics with ASCII/mermaid architecture diagrams.
4. **Syntax & API Reference**: Exhaustive parameter descriptions, return types, and failure states.
5. **Practical Examples**: 5 verified, progressive, idiomatic code examples.
6. **Edge Cases, Pitfalls & Gotchas**: Unintuitive behaviors and type boundary failures.
7. **Common Mistakes & Anti-Patterns**: Comparison table of wrong vs corrected idiomatic patterns.
8. **Performance Characteristics**: Big-O time and space complexity, memory allocations, cache locality.
9. **Engine Internals**: V8 Hidden Classes, IC transitions, bytecode opcodes, and garbage collection.
10. **Best Practices & Design Guidelines**: Production architectural standards.
11. **Real-World Case Study**: Production architecture teardown from enterprise systems.
12. **Interview Questions**: 8 deep technical questions with complete answers in collapsible `<details>`.
13. **Exercises & Challenges**: 2–6 practical challenges with full solutions in collapsible `<details>`.

---

## Verification & Test Harness

Every engineering project and code sample is verified against Node.js:

```bash
# Verify all project suites across the track
node --test 01-languages/javascript/25_projects/tests/*.test.js

# Run native test runner in watch mode
node --test --watch
```

---

## Cross-Curriculum Navigation

- **TypeScript Track**: [`01-languages/typescript/`](file:///e:/Learning/engineering-notes/01-languages/typescript) — Type systems, generics, compiler flags, AST transformers.
- **Frontend Frameworks**:
  - React: [`02-frontend/reactjs/`](file:///e:/Learning/engineering-notes/02-frontend/reactjs)
  - Next.js: [`02-frontend/nextjs/`](file:///e:/Learning/engineering-notes/02-frontend/nextjs)
- **Backend Architecture**:
  - Node.js & Express: [`03-backend/nodejs/`](file:///e:/Learning/engineering-notes/03-backend/nodejs)
  - NestJS Enterprise: [`03-backend/nestjs/`](file:///e:/Learning/engineering-notes/03-backend/nestjs)
