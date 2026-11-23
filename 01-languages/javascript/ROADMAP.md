# JavaScript Engineering Curriculum Roadmap

> A phased, structured learning pathway taking an engineer from core language primitives to senior-level runtime architecture, systems engineering, and staff interview mastery.

---

## 12-Week Mastery Roadmap

```
  [Stage 1: Foundations] ──► [Stage 2: Async & Concurrency] ──► [Stage 3: DSA & Architecture] ──► [Stage 4: Systems & Projects]
      Weeks 1–3                   Weeks 4–6                         Weeks 7–9                         Weeks 10–12
  - Types & Coercion          - Event Loop (Browser & Node)     - DSA in JS (18 Patterns)         - 21 Engineering Projects
  - Scope & Closures          - Microtasks vs Macrotasks        - GoF Design Patterns             - V8 JIT & Hidden Classes
  - this & Prototypes         - Streams & WebSockets            - Security & Defensive Coding     - Senior Interview Mastery
```

---

## Stage 1: Core Fundamentals & Runtime Mechanics (Weeks 1–3)

### Goal
Develop an uncompromising, specification-accurate mental model of JavaScript execution contexts, memory allocations, and type coercion.

### Modules to Complete:
- [x] **Week 1**: Tooling & Type Foundations
  - Complete [`00_setup`](file:///e:/Learning/engineering-notes/01-languages/javascript/00_setup) (Node 22 LTS, Biome, ESLint flat config).
  - Complete [`01_fundamentals`](file:///e:/Learning/engineering-notes/01-languages/javascript/01_fundamentals) (Tagged Smis, `ToPrimitive`, bitwise operations).
  - Complete [`02_functions`](file:///e:/Learning/engineering-notes/01-languages/javascript/02_functions) (First-class functions, pure functions, currying).
- [x] **Week 2**: Objects, Scope & Execution Pipeline
  - Complete [`03_objects-and-arrays`](file:///e:/Learning/engineering-notes/01-languages/javascript/03_objects-and-arrays) (Property descriptors, Proxies, TypedArrays).
  - Complete [`04_scope-and-execution`](file:///e:/Learning/engineering-notes/01-languages/javascript/04_scope-and-execution) (Call stack, lexical environments, TDZ).
- [x] **Week 3**: Object Models, Closures & Functional Paradigms
  - Complete [`05_this-and-oop`](file:///e:/Learning/engineering-notes/01-languages/javascript/05_this-and-oop) (4 binding rules, prototype delegation, private fields).
  - Complete [`06_closures`](file:///e:/Learning/engineering-notes/01-languages/javascript/06_closures) (Heap context promotion, Meteor memory leak).
  - Complete [`07_functional-programming`](file:///e:/Learning/engineering-notes/01-languages/javascript/07_functional-programming) (Immutability, pipe/compose, monads).

### Milestone Checkpoint 1:
- Can you explain why `0.1 + 0.2 !== 0.3` down to IEEE 754 mantissa truncation?
- Can you trace the exact prototype lookup receiver when getters are inherited?

---

## Stage 2: Asynchrony, Concurrency & Browser Internals (Weeks 4–6)

### Goal
Master asynchronous event scheduling, microtask queues, libuv phases, and browser rendering pipelines.

### Modules to Complete:
- [x] **Week 4**: Modern Evolution & Built-Ins
  - Complete [`08_modern-javascript`](file:///e:/Learning/engineering-notes/01-languages/javascript/08_modern-javascript) (Iterators, Generators, Well-Known Symbols).
  - Complete [`09_built-in-objects`](file:///e:/Learning/engineering-notes/01-languages/javascript/09_built-in-objects) (Map/Set, WeakMap, Atomics, SharedArrayBuffer).
  - Complete [`10_error-handling`](file:///e:/Learning/engineering-notes/01-languages/javascript/10_error-handling) (Custom error subclasses, `Error.cause`).
- [x] **Week 5**: Promises & The Event Loop
  - Complete [`11_asynchronous-javascript`](file:///e:/Learning/engineering-notes/01-languages/javascript/11_asynchronous-javascript) (Promises/A+, `async/await`, AbortSignal).
  - Complete [`12_event-loop`](file:///e:/Learning/engineering-notes/01-languages/javascript/12_event-loop) (Browser Microtask turn vs Node.js 6 libuv phases).
  - Complete [`13_modules`](file:///e:/Learning/engineering-notes/01-languages/javascript/13_modules) (ESM static graphs, dynamic imports).
- [x] **Week 6**: Browser DOM & Web Networking
  - Complete [`14_dom-and-browser`](file:///e:/Learning/engineering-notes/01-languages/javascript/14_dom-and-browser) (Critical Rendering Path, Layout Thrashing, Observers).
  - Complete [`15_networking`](file:///e:/Learning/engineering-notes/01-languages/javascript/15_networking) (Fetch, Streams, WebSockets, CORS).
  - Complete [`16_nodejs`](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs) (Native streams, pipeline, process).

### Milestone Checkpoint 2:
- Can you write the exact execution sequence of `process.nextTick`, `queueMicrotask`, `setTimeout`, and `setImmediate`?
- Can you eliminate Layout Thrashing in high-frequency animations?

---

## Stage 3: Architecture, Testing & Algorithms (Weeks 7–9)

### Goal
Implement classical and modern software architecture patterns, test harnesses, application security protocols, and algorithm problem-solving patterns in JavaScript.

### Modules to Complete:
- [x] **Week 7**: Advanced Concurrency & Engine Profiling
  - Complete [`17_concurrency-and-parallelism`](file:///e:/Learning/engineering-notes/01-languages/javascript/17_concurrency-and-parallelism) (Web Workers, Worker Threads).
  - Complete [`18_memory-and-garbage-collection`](file:///e:/Learning/engineering-notes/01-languages/javascript/18_memory-and-garbage-collection) (Scavenger Minor GC, Major GC Mark-Sweep).
  - Complete [`19_performance`](file:///e:/Learning/engineering-notes/01-languages/javascript/19_performance) (Ignition, Turbofan, Hidden Classes, Monomorphic ICs).
- [x] **Week 8**: Design Patterns, Testing & Security
  - Complete [`20_design-patterns`](file:///e:/Learning/engineering-notes/01-languages/javascript/20_design-patterns) (IoC, Factory, Strategy, Observer).
  - Complete [`21_testing`](file:///e:/Learning/engineering-notes/01-languages/javascript/21_testing) (Node native test runner, Vitest, mocking).
  - Complete [`22_security`](file:///e:/Learning/engineering-notes/01-languages/javascript/22_security) (XSS, CSRF, Prototype Pollution prevention).
  - Complete [`23_real-world-patterns`](file:///e:/Learning/engineering-notes/01-languages/javascript/23_real-world-patterns) (Debounce, Virtualization, Token Bucket Rate Limiting).
- [x] **Week 9**: Data Structures & Algorithms
  - Complete all 18 notes in [`24_dsa-in-javascript`](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript):
    - Trees, BSTs, Min-Heaps, Graphs, Trie, Disjoint Set Union (DSU), Dynamic Programming, Backtracking.

### Milestone Checkpoint 3:
- Can you implement an $O(\log N)$ Min-Heap priority queue from scratch in 15 minutes?
- Can you identify and patch an XSS injection and a Prototype Pollution payload?

---

## Stage 4: Systems Engineering & Staff Interview Mastery (Weeks 10–12)

### Goal
Author complex production libraries and tools without third-party frameworks, and master senior engineering coding rounds.

### Modules to Complete:
- [x] **Weeks 10–11**: Production Engineering Projects ([`25_projects`](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects))
  - Build Mini Virtual DOM (`PRJ-10`), Unidirectional State Manager (`PRJ-11`), SPA Client Router (`PRJ-12`).
  - Build IoC DI Container (`PRJ-14`), Resilient Web Scraper (`PRJ-16`), In-Memory Task Scheduler (`PRJ-17`).
  - Build Mini JS Bundler (`PRJ-18`), Offline PWA (`PRJ-19`), Mini Test Framework (`PRJ-20`), Reactive Signals (`PRJ-21`).
- [x] **Week 12**: Interview Mastery & Reference Memorization
  - Complete all 9 technical guides in [`26_interview`](file:///e:/Learning/engineering-notes/01-languages/javascript/26_interview).
  - Master all 13 reference sheets in [`27_cheatsheets`](file:///e:/Learning/engineering-notes/01-languages/javascript/27_cheatsheets).
  - Solve all 15 code prediction riddles in `04_output-based-questions.md`.
  - Author all specification polyfills in `08_polyfills-from-scratch.md`.

---

## Self-Assessment Rubric

| Level | Expected Capability |
| :--- | :--- |
| **Mid-Level Engineer** | Fluency in ES2024 syntax, async/await, DOM event listeners, writing unit tests with `node:test`. |
| **Senior Engineer** | Deep command of microtask event loop timing, V8 Hidden Class transitions, diagnosing memory leaks via Heap Snapshots, building zero-dependency architectures. |
| **Staff / Principal** | Designing compiler pipelines, bundler dependency DAGs, fine-grained reactive dependency graphs, and authoring specification-compliant polyfills. |
