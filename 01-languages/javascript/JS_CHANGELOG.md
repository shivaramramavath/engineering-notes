# JavaScript Engineering Curriculum Changelog

> Detailed chronological record of all structural modifications, corrections, note expansions, and audit remediations across the JavaScript curriculum.

---

## 2026-11-23

### Batch 11: Real-World Engineering Projects (Module 25)
- **Module 25 (`25_projects`):**
  - Created `01_vanilla-todo-app.md` (350 lines): MVC todo application with localStorage persistence, filter states, and event delegation.
  - Created `02_weather-dashboard.md` (330 lines): Live asynchronous weather client with Open-Meteo integration, AbortController timeouts, and DOM hydration.
  - Created `03_markdown-editor.md` (325 lines): Split-pane live Markdown preview editor with custom regex lexing and debounced DOM updates.
  - Created `04_expense-tracker-indexeddb.md` (340 lines): Local-first transaction ledger utilizing native IndexedDB schema migrations, transactions, and cursors.
  - Created `05_quiz-app.md` (335 lines): Timed quiz application with countdown intervals, scoring state machine, and keyboard interaction.
  - Created `06_kanban-drag-and-drop.md` (345 lines): Trello-style board utilizing native HTML5 Drag and Drop API with transfer data and live column reordering.
  - Created `07_realtime-chat-websocket.md` (355 lines): Full-stack chat application with client-server WebSocket transport, ping/pong heartbeats, and reconnection backoff.
  - Created `08_tiny-event-emitter.md` (310 lines): Zero-dependency EventEmitter supporting once-listeners, regex pattern matching, and memory leak warnings.
  - Created `09_promise-from-scratch.md` (360 lines): Complete Promises/A+ compliant Promise implementation with state transitions, chaining, and microtask scheduling.
  - Created `10_mini-virtual-dom.md` (350 lines): Virtual DOM engine featuring `h()` hyperscript factory, keyed reconciliation, and surgical DOM patch operations.
  - Created `11_mini-state-manager.md` (340 lines): Unidirectional Redux-style state container with selector memoization, async thunk middleware, and subscription dispatchers.
  - Created `12_mini-router.md` (340 lines): Client-side SPA router featuring path-to-regex compilation, dynamic URL params, query string parsing, and asynchronous navigation guards.
  - Created `13_mini-lodash.md` (350 lines): Production utility toolkit with circular-reference-safe deep cloning, debounce/throttle, get/set path traversal, and collection grouping.
  - Created `14_dependency-injection-container.md` (340 lines): Inversion of Control container supporting singleton and transient lifecycles, class instantiations, and circular dependency graph cycle detection.
  - Created `15_cli-task-manager.md` (350 lines): Command-line productivity tool using `util.parseArgs`, atomic filesystem operations, and ANSI terminal color formatting.
  - Created `16_web-scraper.md` (360 lines): Resilient scraper engine with bounded concurrency pool, exponential backoff retries with jitter, and HTML tokenization.
  - Created `17_task-scheduler.md` (360 lines): In-memory task scheduler utilizing a Binary Min-Heap Priority Queue, AbortSignal timeouts, and recurring interval execution.
  - Created `18_mini-bundler.md` (350 lines): Static module bundler with ES module AST parsing, dependency graph traversal, CommonJS shimming, and single-file IIFE bundle emission.
  - Created `19_pwa-notes-app.md` (350 lines): Offline-first Progressive Web App with Cache Storage API strategies (`Cache-First`, `Stale-While-Revalidate`), Service Worker lifecycle, and sync outbox.
  - Created `20_mini-test-framework.md` (350 lines): BDD testing DSL with `describe`/`it`, fluent `expect` assertions, nested `beforeEach`/`afterEach` hooks, and ANSI terminal reporter.
  - Created `21_mini-reactive-signals.md` (350 lines): Fine-grained reactivity primitives library featuring signals, automatic effect tracking, memoized computeds, and batched updates.
  - Created `25_projects/README.md` indexing all 21 engineering projects with metadata, complexity ratings, and execution instructions.

### Batch 10: Data Structures & Algorithms in JavaScript (Module 24)
- **Module 24 (`24_dsa-in-javascript`):**
  - Created `01_complexity-and-big-o.md` (470 lines) covering asymptotic bounds ($O, \Omega, \Theta$), Call Stack auxiliary space in recursion, hidden $O(N)$ operations in loops, V8 ElementsKinds, and empirical benchmarking.
  - Created `02_arrays-and-strings.md` (470 lines) covering opposite-end and fast/slow two-pointer patterns, sliding window, prefix sums, and UTF-16 surrogate pairs / Intl.Segmenter graphemes.
  - Created `03_hash-tables.md` (470 lines) covering hash functions (djb2), collision resolution (separate chaining vs open addressing), load factor rehashing, and V8 OrderedHashTable.
  - Created `04_linked-lists.md` (470 lines) covering Singly and Doubly Linked Lists, Sentinel node patterns, in-place reversal, and Floyd's Cycle Detection.
  - Created `05_stacks-and-queues.md` (470 lines) covering $O(1)$ Linked-List Queues, Monotonic Stacks, Reverse Polish Notation, and Dual-Stack queues.
  - Created `06_trees-and-binary-trees.md` (470 lines) covering Full/Complete/Balanced binary trees, iterative DFS stack traversals, BFS level-order queues, LCA, and Virtual DOM diffing.
  - Created `07_binary-search-trees.md` (470 lines) covering BST ordering invariants, in-order successor deletion, global boundary validation ($-\infty < x < \infty$), and sorted array balancing.
  - Created `08_heaps-and-priority-queues.md` (470 lines) covering Min/Max Heaps, complete binary tree array mappings ($2i+1, 2i+2$), sift-up/down, $O(N)$ linear-time heapify, and Dual-Heap median tracking.
  - Created `09_graphs-and-graph-algorithms.md` (475 lines) covering Adjacency Lists vs Matrices, BFS/DFS traversals, Kahn's Topological Sorting, Dijkstra's shortest path, and circular dependency detection.
  - Created `10_sorting-algorithms.md` (470 lines) covering Merge Sort, 3-Way QuickSort (Dutch National Flag), Counting Sort, QuickSelect, and V8's native TimSort.
  - Created `11_searching-algorithms.md` (470 lines) covering Classic Binary Search, Lower/Upper Bounds, Rotated Sorted Array searches, 2D matrices, and Binary Search on Answer spaces.
  - Created `12_recursion-and-backtracking.md` (470 lines) covering the Choose-Explore-Unchoose template, state-space trees, constraint pruning, N-Queens, permutations, and subsets.
  - Created `13_dynamic-programming.md` (470 lines) covering Overlapping subproblems, Memoization vs Tabulation, 1D/2D state transitions, rolling space compression, and LCS text diffing.
  - Created `14_greedy-algorithms.md` (470 lines) covering the Greedy Choice Property, exchange arguments, interval scheduling, Jump Game, and Fractional Knapsack.
  - Created `15_bitwise-algorithms.md` (470 lines) covering 32-bit two's complement coercion, bitmasking flags, Brian Kernighan's trick, XOR properties, and RBAC permissions.
  - Created `16_trie.md` (470 lines) covering Prefix trees, $O(L)$ lookups, autocomplete suggestion engines, wildcard search (`.`), and Boggle board pruning.
  - Created `17_disjoint-set-union.md` (470 lines) covering Path compression, Union by Rank, Inverse Ackermann bounds, Kruskal's MST, and connected components.
  - Created `18_algorithmic-patterns.md` (470 lines) covering the 14 core algorithmic patterns, problem recognition decision flowchart, Cyclic Sort, and K-Way Merge.
  - Created `24_dsa-in-javascript/README.md` with complete module roadmap and curriculum cross-references.

### Batch 9: Patterns, Testing, Security & Real-World Architecture (Modules 20, 21, 22, 23)
- **Module 20 (`20_design-patterns`):**
  - Expanded `01_module-pattern.md` (430 lines) covering IIFE closures, Revealing Module Pattern, ESM static import/export trees, and live binding mechanics.
  - Expanded `02_factory-pattern.md` (430 lines) covering Simple Factory, Factory Method, Abstract Factory, polymorphism, and dynamic provider instantiation.
  - Expanded `03_singleton-pattern.md` (435 lines) covering ESM module caching as singletons, class private static fields, multi-realm/worker boundaries, and thread-safe synchronization.
  - Expanded `04_builder-pattern.md` (430 lines) covering Fluent Interfaces, immutable builders, query generators, and monomorphic V8 method chaining.
  - Expanded `05_strategy-pattern.md` (430 lines) covering First-Class Function strategies, dynamic runtime swapping, payment processors, and sorting algorithms.
  - Expanded `06_observer-pattern.md` (440 lines) covering Subject/Observer vs Pub/Sub, `Set` subscriber registries, copy-on-write event loops, and leak prevention.
  - Expanded `07_adapter-pattern.md` (430 lines) covering Third-party API compatibility wrappers, modernizing legacy callbacks to Promises, and cross-framework components.
  - Expanded `08_decorator-pattern.md` (440 lines) covering ECMAScript Stage 3 Decorators, method wrapping, caching/timing interceptors, and TC39 context metadata.
  - Expanded `09_dependency-injection.md` (445 lines) covering Inversion of Control (IoC), Constructor vs Property Injection, lightweight custom DI containers, and mock testing.
  - Updated `20_design-patterns/README.md` with complete curriculum roadmap and reading guide.
- **Module 21 (`21_testing`):**
  - Expanded `01_testing-fundamentals.md` (435 lines) covering The Testing Pyramid vs Trophy, Unit vs Integration vs E2E, deterministic test suites, and assertions.
  - Expanded `02_unit-testing.md` (440 lines) covering AAA (Arrange, Act, Assert) structure, testing pure functions, state-based vs interaction testing, and coverage metrics.
  - Expanded `03_integration-testing.md` (445 lines) covering API route testing with Supertest, database test containers, isolated test fixtures, and transactional rollbacks.
  - Expanded `04_vitest-and-jest.md` (440 lines) covering Vitest vs Jest architectures, ESM native execution, worker pools, multi-realm VM context differences, and snapshot testing.
  - Expanded `05_mocking.md` (445 lines) covering Dummies, Stubs, Spies, and Mocks, `vi.fn()` / `jest.fn()`, module mocking, time manipulation (`useFakeTimers`), and unmocking.
  - Expanded `06_testing-patterns.md` (435 lines) covering Page Object Model (POM), Test Data Builders, Given-When-Then, and parameterized table-driven tests.
  - Updated `21_testing/README.md` with complete curriculum roadmap and reading guide.
- **Module 22 (`22_security`):**
  - Expanded `01_xss.md` (440 lines) covering Stored, Reflected, and DOM-based XSS, context-aware output encoding, DOMPurify sanitization, and strict CSP nonces.
  - Expanded `02_csrf.md` (435 lines) covering Cross-Site Request Forgery, Synchronizer Token Pattern, Double-Submit Cookies, and `SameSite` cookie policies (Strict vs Lax).
  - Expanded `03_prototype-pollution.md` (440 lines) covering `__proto__` and `constructor.prototype` tampering, deep merge exploits, `Object.freeze()`, null-prototype maps, and V8 Hidden Class deoptimizations.
  - Expanded `04_input-validation.md` (445 lines) covering Schema validation with Zod, Regular Expression Denial of Service (ReDoS) prevention, SQL injection, and NoSQL operator injection.
  - Expanded `05_dependency-security.md` (440 lines) covering Supply chain attacks, lockfile poisoning, `npm audit`, Dependabot, typosquatting defense, and reproducible builds.
  - Expanded `06_security-checklist.md` (440 lines) covering OWASP ASVS production verification, Helmet security headers, SSRF guards, cryptographic PRNG, and constant-time comparisons.
  - Updated `22_security/README.md` with complete curriculum roadmap and reading guide.
- **Module 23 (`23_real-world-patterns`):**
  - Expanded `01_debounce.md` (470 lines) covering Leading vs trailing debounce, closure-backed timer management, `cancel()` and `flush()` methods, and search autocompletes.
  - Expanded `02_throttle.md` (470 lines) covering Leading/trailing rate limits, scroll and canvas rendering, and `requestAnimationFrame` monitor refresh sync.
  - Expanded `03_retry.md` (475 lines) covering Exponential backoff, Full & Decorrelated Jitter, HTTP `Retry-After` header parsing, and `AbortSignal` cooperative cancellation.
  - Expanded `04_timeout.md` (465 lines) covering Promise deadlines, avoiding `Promise.race` memory leaks, `AbortSignal.timeout(ms)`, and Node process `timer.unref()`.
  - Expanded `05_memoization.md` (470 lines) covering Pure function caching, $O(1)$ LRU eviction via Map, WeakMap leak prevention, and async SingleFlight deduplication.
  - Expanded `06_concurrency-control.md` (475 lines) covering Asynchronous promise pools (`pLimit`), backpressure, priority task queues, and socket descriptor limits.
  - Expanded `07_event-emitter.md` (470 lines) covering Universal EventEmitter, copy-on-write iteration loops, wildcard namespaces, and `MaxListenersExceededWarning` defense.
  - Expanded `08_caching.md` (470 lines) covering Cache-Aside, Stale-While-Revalidate (SWR), cache stampede mitigation, and multi-tier L1 Memory / L2 Redis caching.
  - Expanded `09_rate-limiting.md` (470 lines) covering Token Bucket, Leaky Bucket, Sliding Window Counter, atomic Redis Lua scripts, and IETF rate limit headers.
  - Updated `23_real-world-patterns/README.md` with complete curriculum roadmap and reading guide.

### Batch 8: Node.js, Concurrency, Memory & Performance (Modules 16, 17, 18, 19)
- **Module 16 (`16_nodejs`):**
  - Expanded `01_node-runtime.md` (470 lines) covering V8 + libuv + C++ bindings architecture, Node.js globals vs window, module execution wrappers, CLI flags, LTS release cadence, and cross-platform paths.
  - Expanded `02_node-event-loop.md` (460 lines) covering libuv phases (Timers, Pending I/O, Idle/Prepare, Poll, Check, Close), `process.nextTick` microtask prioritization, `setImmediate` vs `setTimeout`, and thread pool work dispatch.
  - Expanded `03_process-and-env.md` (445 lines) covering `process.argv` parsing, environment variables (`process.env`), POSIX signal handling (`SIGINT`, `SIGTERM`), exit codes, and high-resolution timing via `process.hrtime.bigint()`.
  - Expanded `04_filesystem-and-path.md` (465 lines) covering `node:fs/promises`, atomic file writes via temp renames, path traversal security (`path.resolve` directory jail), and recursive directory crawling.
  - Expanded `05_events.md` (440 lines) covering `EventEmitter` memory management, synchronous dispatch, `once`, `events.captureRejections`, custom event emitters, and avoiding max listener warnings.
  - Expanded `06_streams.md` (470 lines) covering Readable, Writable, Duplex, and Transform streams, `stream.pipeline()` vs `stream.pipe()`, backpressure handling (`highWaterMark`), and async iteration (`for await`).
  - Expanded `07_buffers.md` (455 lines) covering raw binary memory, `Buffer.alloc` vs `Buffer.allocUnsafe` security hazards, multi-byte UTF-8 encoding traps (`Buffer.byteLength`), and zero-copy slicing.
  - Expanded `08_http-server.md` (430 lines) covering `node:http`, `IncomingMessage` and `ServerResponse` streams, keep-alive connection pooling, chunked transfer encoding, and graceful shutdown pipelines.
  - Expanded `09_child-process-and-cluster.md` (440 lines) covering `spawn`, `exec`, `execFile`, `fork`, IPC message channels, `node:cluster` primary/worker port sharing (`SCM_RIGHTS`), and rolling reloads.
  - Updated `16_nodejs/README.md` with complete curriculum roadmap and reading guide.
- **Module 17 (`17_concurrency-and-parallelism`):**
  - Expanded `01_concurrency-vs-parallelism.md` (400 lines) covering single-threaded I/O interleaving vs multi-core CPU parallelism, Amdahl's Law, CPU-bound vs I/O-bound bottlenecks, and structured cloning costs.
  - Expanded `02_web-workers.md` (415 lines) covering Dedicated, Shared, and Service Workers, `postMessage`, Transferable Objects (`ArrayBuffer`, `ImageBitmap`), `OffscreenCanvas` rendering, and promise-based RPC abstractions.
  - Expanded `03_worker-threads.md` (430 lines) covering Node.js `worker_threads`, V8 isolate lifecycles, `parentPort`, `workerData`, direct thread communication via `MessageChannel`, and production worker pools.
  - Expanded `04_sharedarraybuffer-and-atomics.md` (445 lines) covering true zero-copy shared memory, `Atomics` operations (`load`, `store`, `add`, `compareExchange`), `Atomics.wait` and `notify`, and Cross-Origin Isolation (`COOP`/`COEP`).
  - Expanded `05_concurrency-control.md` (450 lines) covering asynchronous mutexes, counting semaphores, read-write locks, check-then-act (TOCTOU) race condition prevention, and the Actor model.
  - Updated `17_concurrency-and-parallelism/README.md` with complete curriculum roadmap and reading guide.
- **Module 18 (`18_memory-and-garbage-collection`):**
  - Expanded `01_memory-model.md` (440 lines) covering Call Stack execution frames vs V8 Heap graph, primitive values vs reference types, V8 pointer tagging (Smi vs HeapObject), heap spaces, and pointer compression.
  - Expanded `02_garbage-collection.md` (450 lines) covering the Weak Generational Hypothesis, Scavenger Minor GC (From/To semi-spaces, Cheney's copying algorithm), Full Major GC (Mark-Sweep-Compact), Orinoco concurrent marking, and write barriers.
  - Expanded `03_memory-leaks.md` (460 lines) covering shallow size vs retained size, the 5 classic leaks (accidental globals, forgotten timers, retained closures, detached DOM nodes, accumulated listeners), and heap snapshots.
  - Expanded `04_memory-optimization.md` (470 lines) covering Array of Structs (AoS) vs Struct of Arrays (SoA), generic object pools, string interning, array pre-allocation, and sampling heap profiling.
  - Updated `18_memory-and-garbage-collection/README.md` with complete curriculum roadmap and reading guide.
- **Module 19 (`19_performance`):**
  - Expanded `01_performance-fundamentals.md` (435 lines) covering latency vs throughput, percentile distributions (p50/p95/p99), V8 compilation pipeline (Ignition, Sparkplug, TurboFan), speculative type feedback, and performance budgets.
  - Expanded `02_profiling.md` (445 lines) covering sampling profilers vs instrumentation, `node --prof` and `node --prof-process`, Chrome DevTools performance traces, flame graph interpretation, and Clinic.js.
  - Expanded `03_browser-performance.md` (450 lines) covering Critical Rendering Path, Layout Thrashing (Forced Synchronous Layout), GPU compositor layers (`transform`/`opacity`), Core Web Vitals (LCP, INP, CLS), and Long Animation Frames (LoAF).
  - Expanded `04_node-performance.md` (440 lines) covering event loop delay monitoring (`monitorEventLoopDelay`), microtask queue starvation, schema-based JSON serialization, and Radix Tree routing.
  - Expanded `05_optimization-patterns.md` (450 lines) covering Hidden Class stability and property ordering, monomorphism vs megamorphism, packed vs holey arrays, hardware branch prediction, and loop unrolling.
  - Updated `19_performance/README.md` with complete curriculum roadmap and reading guide.

### Batch 7: DOM, Browser & Networking (Modules 14, 15)
- **Module 14 (`14_dom-and-browser`):**
  - Expanded 10 concept notes (`01_dom.md` through `10_web-components.md`) covering DOM tree mechanics, selectors, element manipulation, event capturing/bubbling, event delegation, form validation, modern observers (`IntersectionObserver`, `ResizeObserver`, `MutationObserver`), browser storage (`IndexedDB`, `localStorage`, Cookies), Web APIs, and Shadow DOM custom elements.
  - Updated `14_dom-and-browser/README.md`.
- **Module 15 (`15_networking`):**
  - Expanded 6 concept notes (`01_http-fundamentals.md` through `06_networking-patterns.md`) covering HTTP/1.1 vs HTTP/2 vs HTTP/3, `fetch` API, headers and CORS preflights, Request/Response streaming, WebSockets and Server-Sent Events, and resilience patterns (retry with exponential backoff, circuit breaker).
  - Updated `15_networking/README.md`.

### Batch 6: Asynchronous JavaScript, Event Loop & Modules (Modules 11, 12, 13)
- **Module 11 (`11_asynchronous-javascript`):**
  - Expanded 9 concept notes (`01_asynchronous-model.md` through `09_async-patterns.md`) covering the asynchronous execution model, callback contracts, Promise state machines and combinators (`Promise.all`, `allSettled`, `race`, `any`), `async`/`await` desugaring, unhandled rejection lifecycles, and cancellation via `AbortController`.
  - Updated `11_asynchronous-javascript/README.md`.
- **Module 12 (`12_event-loop`):**
  - Expanded 5 concept notes (`01_event-loop.md` through `05_macrotasks.md`) covering the HTML5 Event Loop specification, microtasks vs macrotasks, `queueMicrotask`, `requestAnimationFrame` vs `requestIdleCallback`, and task scheduling.
  - Updated `12_event-loop/README.md`.
- **Module 13 (`13_modules`):**
  - Expanded 5 concept notes (`01_module-systems-overview.md` through `05_module-patterns.md`) covering module history (IIFE, AMD, CJS, UMD, ESM), CommonJS synchronous loading, ES Modules static analysis and live bindings, CJS/ESM interop dual-package hazards, dynamic `import()`, import maps, and module bundlers.
  - Updated `13_modules/README.md`.

### Batch 5: Modern JavaScript, Built-in Objects & Error Handling (Modules 08, 09, 10)
- **Module 08 (`08_modern-javascript`):**
  - Expanded `01_template-literals.md` (393 lines) covering backticks, multiline formatting, embedded expressions `${}`, tagged template functions, `String.raw`, AST sanitization, and SQL injection defense.
  - Expanded `02_enhanced-object-literals.md` (425 lines) covering property shorthands, method definitions, computed property keys `[key]`, `[[HomeObject]]`, `super` in object literals, and object spread semantics.
  - Expanded `03_symbols.md` (512 lines) covering primitive uniqueness, the Global Symbol Registry (`Symbol.for`, `Symbol.keyFor`), well-known symbols (`Symbol.iterator`, `Symbol.toPrimitive`, `Symbol.hasInstance`, `Symbol.toStringTag`), and V8 `Name` representation.
  - Expanded `04_iterators-and-iterables.md` (578 lines) covering the Iterable Protocol (`[Symbol.iterator]`), the Iterator Protocol (`{ next() }`), early termination cleanup via `.return()`, consumer constructs (`for...of`, spread, destructuring), and ES2024 Iterator Helpers (`map`, `filter`, `take`).
  - Expanded `05_generators.md` (496 lines) covering `function*`, `yield`, `yield*` delegation, bidirectional message passing via `.next(val)`, exception injection via `.throw()`, early return via `.return()`, coroutine state machines, and Redux-Saga cooperative scheduling.
  - Expanded `06_es-features-by-version.md` (437 lines) covering the TC39 5-stage proposal pipeline, comprehensive edition-by-edition feature breakdowns from ES2015 to ES2025+, V8 harmony flags, and enterprise migration strategies.
  - Updated `08_modern-javascript/README.md` with complete curriculum roadmap and reading guide.
- **Module 09 (`09_built-in-objects`):**
  - Expanded `01_string.md` (459 lines) covering UTF-16 code units vs Unicode code points, surrogate pairs, normalization forms (NFC, NFD), grapheme cluster segmentation, and V8 internal string representations (`ConsString`, `SlicedString`, `ThinString`).
  - Expanded `02_number-and-bigint.md` (413 lines) covering IEEE-754 64-bit double precision binary layout, floating-point drift ($0.1 + 0.2 \neq 0.3$), `Number.EPSILON`, safe integer boundaries ($2^{53}-1$), `BigInt` arbitrary precision, and V8 Smi tagging.
  - Expanded `03_math.md` (419 lines) covering mathematical constants (`PI`, `E`), rounding algorithms (`floor`, `ceil`, `trunc`, `round`), trigonometry vectors, `xorshift128+` PRNG mechanics, and TurboFan hardware assembly intrinsics.
  - Expanded `04_date-and-temporal.md` (412 lines) covering legacy `Date` flaws (0-indexed months, mutability, timezone confusion), Unix epoch milliseconds, UTC vs local timezone resolution, ISO-8601 parsing, and the modern TC39 Stage 3 `Temporal` API (`Instant`, `ZonedDateTime`, `PlainDate`, `Duration`).
  - Expanded `05_regexp.md` (425 lines) covering modern flags (`d`, `v`, `y`, `s`), named capture groups, lookaround assertions (lookahead/lookbehind), V8 Irregexp engine architecture, and Regular Expression Denial of Service (ReDoS) defense.
  - Expanded `06_map-and-set.md` (454 lines) covering `Map` and `Set`, `SameValueZero` equality, ES2024 mathematical set algebra (`union`, `intersection`, `difference`, `symmetricDifference`, `isSubsetOf`), V8 `OrderedHashTable` layout, and high-throughput LRU cache implementation.
  - Expanded `07_weakmap-weakset-weakref.md` (441 lines) covering ephemeron tables, `WeakMap` and `WeakSet`, memory leak prevention, non-enumerable design rationale, and ES2021 `WeakRef` / `FinalizationRegistry` with microtask pinning.
  - Expanded `08_json.md` (447 lines) covering RFC 8259 invariants, `JSON.stringify` array/function replacers, `JSON.parse` bottom-up revivers, the `toJSON()` protocol, V8 SIMD parsing optimizations, and circular reference handling.
  - Expanded `09_intl.md` (408 lines) covering ECMA-402, BCP 47 locale tags, `DateTimeFormat`, `NumberFormat`, `Collator`, `ListFormat`, `Segmenter`, ICU C++ bridge, and formatter instance caching patterns.
  - Expanded `10_typed-arrays-and-arraybuffer.md` (422 lines) covering raw binary memory, `ArrayBuffer`, TypedArray views, `DataView` explicit endianness handling, `Uint8ClampedArray` canvas pixels, ES2024 `ArrayBuffer.prototype.transfer()`, and `SharedArrayBuffer` Atomics.
  - Expanded `11_proxy-and-reflect.md` (502 lines) covering metaprogramming, the 13 fundamental language traps, 1:1 `Reflect` parity, engine invariants, the `receiver` parameter, private class field (`#`) gotchas, and building a reactive state engine (Vue 3 model).
  - Expanded `12_global-objects.md` (410 lines) covering `globalThis` universal standard, `encodeURI` vs `encodeURIComponent` delimiter rules, Unicode-safe Base64 (`btoa`/`atob`), `structuredClone` memory transfers, and V8 `JSGlobalProxy` iframe isolation.
  - Updated `09_built-in-objects/README.md` with complete curriculum roadmap and reading guide.
- **Module 10 (`10_error-handling`):**
  - Expanded `01_errors.md` (476 lines) covering the 7 native error subclasses (`TypeError`, `RangeError`, `ReferenceError`, etc.), causal error chaining via `Error.cause` (ES2022), `AggregateError` multi-error bundling (ES2021), V8 stack trace allocation semantics, and APM JSON serialization.
  - Expanded `02_try-catch-finally.md` (463 lines) covering call stack unwinding, optional catch binding (ES2019), `try...finally` resource guarantees, the fatal `finally` return override trap, and V8 `HandlerTable` zero-cost try blocks.
  - Expanded `03_custom-errors.md` (527 lines) covering ES6 class inheritance from `Error`, `this.name = this.constructor.name`, V8 `Error.captureStackTrace` trimming, ES5 prototype transpilation fixes, and RFC 7807 Problem Details for REST APIs.
  - Expanded `04_error-propagation.md` (490 lines) covering synchronous stack bubbling vs asynchronous event loop disconnections, Promise rejection chains, `unhandledRejection` lifecycle, Node.js process crash guards (`uncaughtException`), and graceful shutdown pipelines.
  - Expanded `05_production-error-handling.md` (511 lines) covering operational vs programmer errors, centralized Express/Fastify error middleware, PII and secret redaction, the Circuit Breaker resilience pattern, and 12-factor JSON logging to stdout.
  - Updated `10_error-handling/README.md` with complete curriculum roadmap and reading guide.

### Batch 4: Scopes, OOP, Closures & Functional Programming (Modules 04, 05, 06, 07)
- **Module 04 (`04_scope-and-execution`):**
  - Expanded `01_scope.md` (490 lines) covering the 4 scopes (Global, Module, Function, Block), lexical vs dynamic scope, identifier resolution algorithm, and multi-tenant sandboxing.
  - Expanded `02_lexical-environment.md` (515 lines) covering ECMAScript §9.1 Lexical Environments, Declarative vs Object vs Module Environment Records, `[[OuterEnv]]` pointers, and V8 heap `Context` allocation.
  - Expanded `03_execution-context-and-call-stack.md` (510 lines) covering Creation Phase vs Execution Phase, LIFO Call Stack mechanics, VariableEnvironment vs LexicalEnvironment, stack overflows, and stack trace sanitization.
  - Expanded `04_hoisting-and-tdz.md` (520 lines) covering declaration hoisting, Temporal Dead Zone boundaries, `typeof` TDZ semantics, parameter TDZ, and V8 `the_hole` internal sentinel checks.
  - Expanded `05_strict-mode.md` (495 lines) covering `"use strict"`, automatic strict mode in ES modules and classes, silent failures converted to exceptions, `eval` scope isolation, and undefined `this`.
  - Updated `04_scope-and-execution/README.md` with complete curriculum roadmap and reading guide.
- **Module 05 (`05_this-and-oop`):**
  - Expanded `01_this.md` (530 lines) covering the 4 binding rules (new, explicit, implicit, default), call-site precedence, strict mode differences, lexical arrow `this`, and detached method bugs.
  - Expanded `02_call-apply-bind.md` (510 lines) covering `call`, `apply`, `bind`, method borrowing patterns, partial application, polyfill implementations, and V8 `JSBoundFunction` exotic objects.
  - Expanded `03_prototypes-and-prototype-chain.md` (525 lines) covering prototypal delegation, `[[Prototype]]` vs `prototype` vs `__proto__`, `Object.create()`, property shadowing, and Prototype Pollution security defenses.
  - Expanded `04_constructor-functions.md` (515 lines) covering pre-ES6 constructor mechanics, 5-step `new` operator execution algorithm, return overrides, `new.target` meta-property, and prototype wiring.
  - Expanded `05_classes.md` (535 lines) covering ES6 class declarations vs expressions, constructor desugaring, non-enumerable prototype methods, public class fields, and ES2022 static initialization blocks.
  - Expanded `06_inheritance.md` (520 lines) covering `extends`, mandatory derived constructor `super()` requirement, method delegation via `[[HomeObject]]`, dual prototype chains, and subclassing built-ins.
  - Expanded `07_private-fields.md` (510 lines) covering hard privacy via `#field`, private methods, private static members, realm-safe brand checking with `#field in obj`, and V8 `PrivateName` internals.
  - Expanded `08_mixins-and-composition.md` (525 lines) covering composition over inheritance, Fragile Base Class problem, `Object.assign` mixins vs subclass factory mixins, and Entity-Component-System (ECS) architecture.
  - Updated `05_this-and-oop/README.md` with complete curriculum roadmap and reading guide.
- **Module 06 (`06_closures`):**
  - Expanded `01_closures.md` (505 lines) covering lexical closure definition, `[[Environment]]` internal slot, live variable references vs value snapshots, V8 Context allocation, and `var` vs `let` loop mechanics.
  - Expanded `02_closure-use-cases.md` (520 lines) covering the Revealing Module Pattern, private state factories, currying, parametric decorators, memoization caches, and production debounce & throttle engines.
  - Expanded `03_closure-pitfalls.md` (530 lines) covering memory leaks, the Meteor shared-context leak, stale closures in timers and React hooks, Chrome DevTools heap snapshots, and lifecycle disposal.
  - Updated `06_closures/README.md` with complete curriculum roadmap and reading guide.
- **Module 07 (`07_functional-programming`):**
  - Expanded `01_pure-functions-and-side-effects.md` (515 lines) covering the two axioms of purity, referential transparency, substitution model, side effect cataloging, and "Pure Core, Imperative Shell" architecture.
  - Expanded `02_immutability.md` (520 lines) covering primitive immutability vs object mutability, shallow `Object.freeze()` vs recursive `deepFreeze()`, structural sharing, and Immer copy-on-write proxies.
  - Expanded `03_map-filter-reduce.md` (530 lines) covering declarative iteration, catamorphism via `reduce`, intermediate array allocation hazards, $O(n^2)$ accumulator trap, and single-pass Transducers.
  - Expanded `04_composition-and-pipe.md` (510 lines) covering right-to-left `compose` vs left-to-right `pipe`, associativity law, unary pipelines, async pipelines (`pipeAsync`), and debugging taps.
  - Expanded `05_currying-and-partial-application.md` (525 lines) covering mathematical currying vs partial application, auto-currying utilities with placeholders, `fn.length` arity traps, and data-last architecture.
  - Expanded `06_point-free.md` (495 lines) covering tacit programming, Eta reduction ($\eta$-reduction), point-free composition, the multi-arg `parseInt` trap, and readability thresholds.
  - Expanded `07_functional-patterns.md` (545 lines) covering category theory, Functor & Monad laws, the Maybe Monad, Either Monad (Railway-Oriented Programming), IO Monad, and Applicative Validation.
  - Updated `07_functional-programming/README.md` with complete curriculum roadmap and reading guide.

### Batch 3: Functions & Objects/Arrays (Modules 02, 03)
- **Module 02 (`02_functions`):**
  - Expanded `01_declarations-and-expressions.md` (520 lines) covering hoisting differences, function name inference (`fn.name`), and V8 compilation pipelines.
  - Expanded `02_arrow-functions.md` (501 lines) covering lexical `this`, absence of `arguments`/`super`/`new.target`, non-constructability, and method pitfalls.
  - Expanded `03_parameters-and-arguments.md` (534 lines) covering default parameters, TDZ in parameter lists, rest parameters (`...args`), and `fn.length` arity calculation rules.
  - Expanded `04_return-values.md` (530 lines) covering ASI return traps, constructor return overrides (primitive vs object returns), and early returns.
  - Expanded `05_callbacks.md` (610 lines) covering callback contracts, inversion of control, error-first conventions in Node.js, and synchronization patterns.
  - Expanded `06_higher-order-functions.md` (562 lines) covering function composition, currying, partial application, transducers, and memoization.
  - Expanded `07_iife-and-function-properties.md` (523 lines) covering modern IIFE use cases (top-level await, scope isolation), `fn.caller`/`fn.arguments` deprecation in strict mode, and custom function metadata.
  - Expanded `08_recursion.md` (541 lines) covering base case guarantees, call stack frame limits, trampolining, and recursion-to-loop transformations.
  - Authored new note `09_function-internals-and-tail-calls.md` (500 lines) covering V8 `JSFunction` heap layout, Context allocation for closures, and Proper Tail Calls (PTC) status.
  - Updated `02_functions/README.md` with complete curriculum roadmap and reading guide.
- **Module 03 (`03_objects-and-arrays`):**
  - Expanded `01_objects.md` (497 lines) covering object literals, computed keys, `delete` performance traps, prototype chain basics, and V8 Hidden Class (Shape) transitions.
  - Expanded `02_property-descriptors.md` (551 lines) covering `Object.defineProperty`, data vs accessor descriptors, `Object.freeze()`, `Object.seal()`, and `Object.preventExtensions()`.
  - Expanded `03_getters-and-setters.md` (597 lines) covering accessor properties, reactive computed state, recursion traps with self-assignment, and prototype inheritance of accessors.
  - Expanded `04_object-static-methods.md` (540 lines) covering `keys`, `values`, `entries`, `assign`, `fromEntries`, `Object.hasOwn()`, and ES2024 `Object.groupBy()` / `Map.groupBy()`.
  - Expanded `05_copying-and-cloning.md` (562 lines) covering reference equality, shallow copy vs deep clone, native `structuredClone()` capabilities/limitations, and JSON serialization loss.
  - Expanded `06_arrays.md` (515 lines) covering dense vs sparse arrays, holes vs undefined, length manipulation, and V8 `PACKED` vs `HOLEY` ElementsKind transitions.
  - Expanded `07_array-methods.md` (530 lines) covering mutating vs pure transformations, ES2023 `toSorted`, `toReversed`, `toSpliced`, `with`, `flatMap`, and `reduce` deep dive.
  - Expanded `08_sorting-and-searching.md` (520 lines) covering V8 Timsort architecture, comparator mathematical contracts, ES2019 stability guarantees, `localeCompare`, and binary search.
  - Expanded `09_destructuring.md` (510 lines) covering positional array destructuring, nominal object destructuring, default value evaluation semantics (`=== undefined`), nested patterns, and aliasing.
  - Expanded `10_spread-and-rest.md` (505 lines) covering rest parameter collection, iterable array spread, ES2018 object spread, shallow copy mechanics, and call stack overflow limits.
  - Expanded `11_optional-chaining-and-nullish.md` (495 lines) covering optional chaining `?.`, nullish coalescing `??`, logical nullish assignment `??=`, and V8 jump bytecode optimization.
  - Authored new note `12_property-enumeration-and-symbols.md` (525 lines) covering ECMAScript §9.1.11 `[[OwnPropertyKeys]]` deterministic 3-phase ordering, Symbol keys, well-known symbols, and V8 Elements vs Properties backing stores.
  - Updated `03_objects-and-arrays/README.md` with complete curriculum roadmap and reading guide.

### Batch 2: Setup & Fundamentals (Modules 00, 01)
- **Module 00 (`00_setup`):**
  - Expanded `01_what-is-javascript.md` to 473 lines covering TC39 stage processes, ES2015-ES2025 evolution, and V8 Ignition/TurboFan execution pipelines.
  - Expanded `02_runtimes-browser-and-node.md` to 441 lines with deep-dives on browser Web APIs vs Node.js C++ bindings, `libuv`, and isomorphic execution via `globalThis`.
  - Expanded `03_npm-and-package-managers.md` to 511 lines with package.json anatomy, semantic versioning algorithms, lockfile mechanics, and pnpm content-addressable storage.
  - Expanded `04_devtools-and-debugging.md` to 492 lines with Chrome DevTools heap snapshot profiling, memory leak detection, conditional breakpoints, and `node --inspect`.
  - Expanded `05_tooling.md` to 621 lines covering ESLint 9+ flat config (`eslint.config.js`), Prettier 3+, Biome Rust linter/formatter, Husky, and `lint-staged`.
  - Authored new note `06_jsdoc-and-type-checking.md` (655 lines) covering zero-build type safety with `checkJs`, `@type`, `@typedef`, `@template`, and `tsc --noEmit`.
  - Updated `00_setup/README.md` with environment verification instructions and module roadmap.
- **Module 01 (`01_fundamentals`):**
  - Expanded `01_variables.md` to 562 lines covering `var`, `let`, `const`, lexical scope, hoisting, and Temporal Dead Zone (TDZ) mechanics.
  - Expanded `02_data-types.md` to 569 lines covering the 7 primitives + Object, auto-boxing wrappers, `Symbol`, `BigInt`, and V8 SMI tagged pointer internals.
  - Expanded `03_type-conversion-and-equality.md` to 550 lines covering ECMAScript abstract operations (`ToPrimitive`, `ToNumber`, `ToBoolean`), loose `==`, strict `===`, `Object.is`, and `SameValueZero`.
  - Expanded `04_operators.md` to 511 lines covering arithmetic, bitwise 32-bit integer casting, short-circuiting, `??`, `?.`, logical assignment, and operator precedence.
  - Expanded `05_expressions-and-statements.md` to 512 lines covering the dual grammar, Automatic Semicolon Insertion (ASI) specification rules, and restricted productions.


### Batch 5: Modern JavaScript, Built-in Objects & Error Handling (Modules 08, 09, 10)
- **Module 08 (`08_modern-javascript`):**
  - Expanded `01_template-literals.md` (393 lines) covering tagged templates, String.raw, cooked vs raw strings, and DSL creation.
  - Expanded `02_enhanced-object-literals.md` (425 lines) covering shorthand methods, computed property keys, prototype assignment via `__proto__`, and super calls.
  - Expanded `03_symbols.md` (512 lines) covering primitive uniqueness, global registry (`Symbol.for`), and well-known symbols (`Symbol.iterator`, `Symbol.toPrimitive`, `Symbol.hasInstance`).
  - Expanded `04_iterators-and-iterables.md` (578 lines) covering iteration protocols, `Symbol.iterator`, `for...of`, infinite sequences, and consumer protocol hooks (`return()`).
  - Expanded `05_generators.md` (496 lines) covering generator suspension, two-way message passing via `next(value)`, error injection via `throw()`, and coroutine design.
  - Expanded `06_es-features-by-version.md` (437 lines) covering detailed release history from ES2016 through ES2024/ES2025 proposals.
  - Updated `08_modern-javascript/README.md` with complete curriculum roadmap.
- **Module 09 (`09_built-in-objects`):**
  - Expanded `01_string.md` (459 lines) covering UTF-16 surrogate pairs, normalization forms (NFC/NFD), code point iteration, and string algorithms.
  - Expanded `02_number-and-bigint.md` (413 lines) covering IEEE 754 double precision floats, subnormal numbers, EPSILON comparisons, and arbitrary precision `BigInt`.
  - Expanded `03_math.md` (419 lines) covering numeric manipulation, clamping, trigonometry, pseudo-randomness, and cryptographic random numbers.
  - Expanded `04_date-and-temporal.md` (412 lines) covering legacy `Date` UTC bugs, month-indexing pitfalls, and the Stage 3/4 `Temporal` API architecture.
  - Expanded `05_regexp.md` (425 lines) covering regex engine automata (NFA), lookarounds, named capture groups, the `d` indices flag, and ReDoS prevention.
  - Expanded `06_map-and-set.md` (454 lines) covering hash tables, `SameValueZero` key equality, insertion-order preservation, and ES2024 Set composition methods.
  - Expanded `07_weakmap-weakset-weakref.md` (441 lines) covering ephemeron tables, garbage collection semantics, private data hiding, `WeakRef`, and `FinalizationRegistry`.
  - Expanded `08_json.md` (447 lines) covering JSON grammar, replacer/reviver functions, circular reference serialization, and JSON injection prevention.
  - Expanded `09_intl.md` (408 lines) covering internationalization standard: `Collator`, `DateTimeFormat`, `NumberFormat`, `RelativeTimeFormat`, `PluralRules`, and `Segmenter`.
  - Expanded `10_typed-arrays-and-arraybuffer.md` (422 lines) covering binary memory buffers, `ArrayBuffer`, `DataView` endianness, `TypedArray` views, and `SharedArrayBuffer`.
  - Expanded `11_proxy-and-reflect.md` (502 lines) covering meta-programming, the 13 internal `Proxy` traps, `Reflect` 1:1 mirroring, and transparent reactive observation.
  - Expanded `12_global-objects.md` (410 lines) covering `globalThis` resolution across environments, global lexical scope vs global object properties, and sandbox security.
  - Updated `09_built-in-objects/README.md` with complete curriculum roadmap.
- **Module 10 (`10_error-handling`):**
  - Expanded `01_errors.md` (476 lines) covering the `Error` prototype hierarchy, native error types, stack frame structure, and `Error.captureStackTrace`.
  - Expanded `02_try-catch-finally.md` (463 lines) covering exception handling semantics, optional catch binding (ES2019), `finally` override traps, and synchronous rethrows.
  - Expanded `03_custom-errors.md` (527 lines) covering subclassing `Error`, prototype chain repair, serialization, error codes, and operational metadata.
  - Expanded `04_error-propagation.md` (490 lines) covering operational vs programmer errors, centralized domain handlers, error classification, and ES2022 `error.cause`.
  - Expanded `05_production-error-handling.md` (511 lines) covering defensive logging, correlation IDs, graceful shutdown (`SIGTERM`), unhandled rejection crashes, and Sentry/Datadog reporting.
  - Updated `10_error-handling/README.md` with complete curriculum roadmap.

### Batch 6: Asynchronous JavaScript, Event Loop & Modules (Modules 11, 12, 13)
- **Module 11 (`11_asynchronous-javascript`):**
  - Expanded `01_sync-vs-async.md` (460 lines) covering execution models, single-threaded concurrency, run-to-completion guarantees, and non-blocking I/O.
  - Expanded `02_callbacks.md` (467 lines) covering CPS, error-first callback conventions, callback hell, inversion of control, and Zalgo hazards.
  - Expanded `03_promises.md` (453 lines) covering Promise state machine, chaining, microtask scheduling, unhandled rejections, and zero-cost async stack stitching.
  - Expanded `04_promise-combinators.md` (473 lines) covering `all`, `allSettled`, `race`, `any`, empty array edge cases, and AggregateError.
  - Expanded `05_async-await.md` (491 lines) covering generator/promise synthesis, return await catch-block traps, and zero-cost async stack traces.
  - Expanded `06_async-error-handling.md` (467 lines) covering rejection tracking, process/window rejection events, and async boundaries.
  - Expanded `07_async-iterators-and-generators.md` (426 lines) covering `Symbol.asyncIterator`, `for await...of`, `async function*`, pull-based backpressure, and resource cleanup.
  - Expanded `08_cancellation-and-abort.md` (390 lines) covering `AbortController`, `AbortSignal`, `AbortSignal.timeout()`, `AbortSignal.any()`, and cooperative cancellation.
  - Expanded `09_async-patterns.md` (444 lines) covering `p-limit` concurrency pools, async queues, in-flight request deduplication (single-flight), and retries with jitter.
  - Updated `11_asynchronous-javascript/README.md` with complete curriculum roadmap.
- **Module 12 (`12_event-loop`):**
  - Expanded `01_event-loop.md` (400 lines) covering call stack, heap, Web APIs, libuv, macrotask & microtask queues, and phase sequence.
  - Expanded `02_microtasks-and-macrotasks.md` (405 lines) covering queue differences, microtask checkpoints, `queueMicrotask`, starvation hazards, and rendering timing.
  - Expanded `03_timers.md` (378 lines) covering `setTimeout`, `setInterval`, 4ms HTML clamping rule, timer drift, monotonic clocks, and Node `unref()`.
  - Expanded `04_task-ordering.md` (388 lines) covering execution puzzles, Node `process.nextTick` vs `setImmediate`, browser vs Node discrepancies, and microtask checkpoints.
  - Expanded `05_event-loop-patterns.md` (372 lines) covering long tasks (>50ms), INP optimization, `scheduler.yield()`, `isInputPending()`, and progressive chunking.
  - Updated `12_event-loop/README.md` with complete curriculum roadmap.
- **Module 13 (`13_modules`):**
  - Expanded `01_es-modules.md` (352 lines) covering static dependency graphs, live read-only bindings, 3-phase loading (Construction, Instantiation, Evaluation), dynamic `import()`, top-level await, and `import.meta`.
  - Expanded `02_commonjs.md` (403 lines) covering synchronous runtime resolution, module wrapper function, `module.exports` vs `exports`, `require.cache`, and circular dependency handling.
  - Expanded `03_esm-vs-commonjs.md` (386 lines) covering loading differences, Node interop rules, synthetic module records, dual-package hazard, and `package.json` conditional exports.
  - Expanded `04_bundlers-and-tree-shaking.md` (362 lines) covering Rollup, Webpack, esbuild, live code inclusion, pure annotations `/*#__PURE__*/`, and `"sideEffects": false`.
  - Expanded `05_module-patterns.md` (404 lines) covering barrel file hazards, dependency injection, breaking circular dependencies, and subpath exports encapsulation.
  - Updated `13_modules/README.md` with complete curriculum roadmap.


### Batch 7: DOM, Browser & Networking (Modules 14, 15)
- **Module 14 (`14_dom-and-browser`):**
  - Expanded `01_dom.md` (385 lines) covering DOM inheritance tree (`EventTarget` -> `Node` -> `Element`), live vs static collections, `DocumentFragment`, and parsing pipelines.
  - Expanded `02_selectors.md` (330 lines) covering `querySelector`, CSS Level 4 selectors (`:has()`, `:is()`, `:where()`), `:scope`, `element.closest()`, and `CSS.escape()`.
  - Expanded `03_element-manipulation.md` (373 lines) covering attributes vs properties, boolean attributes, `classList`, inline styles vs `getComputedStyle`, and layout thrashing prevention.
  - Expanded `04_events.md` (380 lines) covering event lifecycle (Capture, Target, Bubble), passive listeners, `isTrusted`, and cancellation via `AbortSignal`.
  - Expanded `05_event-delegation.md` (375 lines) covering delegated bubbling, `element.closest()` matching, boundary checking, and delegating non-bubbling events (`focusin`/`focusout`).
  - Expanded `06_forms.md` (466 lines) covering Constraint Validation API (`validity`, `setCustomValidity`), `FormData` serialization, `form.requestSubmit()`, and File API.
  - Expanded `07_observers.md` (394 lines) covering `IntersectionObserver` (lazy loading, scroll spy), `ResizeObserver` (box models, loop limits), and `MutationObserver` (microtask batching).
  - Expanded `08_browser-storage.md` (455 lines) covering `localStorage`, `sessionStorage`, HTTP-only security cookies, `IndexedDB` transactions, and `BroadcastChannel`.
  - Expanded `09_web-apis.md` (419 lines) covering Page Visibility API, Clipboard API, Geolocation, Fullscreen, Web Share, and Permissions API.
  - Expanded `10_web-components.md` (506 lines) covering Custom Elements lifecycle (`connectedCallback`, `attributeChangedCallback`), Shadow DOM encapsulation, and `<template>`/`<slot>`.
  - Updated `14_dom-and-browser/README.md` with complete curriculum roadmap.
- **Module 15 (`15_networking`):**
  - Expanded `01_http-fundamentals.md` (403 lines) covering HTTP/1.1 vs HTTP/2 vs HTTP/3 (QUIC), idempotency, status code semantics, Keep-Alive connection pooling, and Head-of-Line blocking.
  - Expanded `02_fetch.md` (397 lines) covering two-step async resolution, body stream consumption (`json`, `blob`, `arrayBuffer`), `AbortSignal.timeout()`, and credentials.
  - Expanded `03_headers-and-cors.md` (375 lines) covering Same-Origin Policy, simple vs preflighted requests (`OPTIONS`), wildcard credentials trap, and `Access-Control-Expose-Headers`.
  - Expanded `04_request-and-response.md` (419 lines) covering constructible `Request` and `Response`, Service Worker caching, stream teeing/cloning, and `Response.json()`.
  - Expanded `05_websockets-and-sse.md` (448 lines) covering full-duplex WebSockets (`ws://`/`wss://`), Server-Sent Events (`EventSource`), heartbeats, auto-reconnection, and binary data.
  - Expanded `06_networking-patterns.md` (482 lines) covering Circuit Breaker state machines, Request Preemption, Single-Flight deduplication, Full Jitter, and Offline Mutation Queues.
  - Updated `15_networking/README.md` with complete curriculum roadmap.
