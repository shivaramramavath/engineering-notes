# Real-World Engineering Projects (25_projects)

> A rigorous curriculum of 21 end-to-end, zero-dependency production engineering projects in modern JavaScript (ES2024+ / Node.js 22 LTS). Each project contains complete architectural specifications, zero placeholders, complete runnable source code, native `node:test` test suites, and verified execution logs.

---

## Curriculum Overview

This module bridges abstract language features and full-stack software architecture. Spanning browser applications, runtime systems, compilers, asynchronous concurrency tools, and reactive UI engines, every project is engineered to withstand production workloads while remaining completely self-contained.

```
                                    ENGINEERING CURRICULUM
                                              │
         ┌───────────────────┬────────────────┴───────────────────┬───────────────────┐
         ▼                   ▼                                    ▼                   ▼
    [DOM & SPA Apps]    [Runtimes & Libs]                   [CLI & Infra]      [Compilers & Reactivity]
    - Vanilla Todo      - Event Emitter                     - CLI Task Mgr     - Mini Virtual DOM
    - Weather Dashboard - Promise/A+                        - Web Scraper      - Mini Module Bundler
    - Markdown Editor   - Mini Lodash                       - Task Scheduler   - Mini Reactive Signals
    - IndexedDB Tracker - State Manager                     - PWA Notes App    - Mini Test Framework
    - Drag-Drop Kanban  - Client Router
    - WebSocket Chat    - DI Container
```

---

## Project Catalog & Specifications

| Code | Project Name | Complexity | Architecture Pattern | Key Language Concepts | File Link |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `PRJ-01` | Vanilla Todo App | Intermediate (2/5) | MVC / Event Delegation | DOM Mutation, LocalStorage, Custom Events | [01_vanilla-todo-app.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/01_vanilla-todo-app.md) |
| `PRJ-02` | Weather Dashboard | Intermediate (3/5) | Service Adapter / Async UI | Fetch API, AbortController, Geolocation, DOM | [02_weather-dashboard.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/02_weather-dashboard.md) |
| `PRJ-03` | Markdown Editor & Parser | Intermediate (3/5) | Lexer / Pipeline | Regex Tokenizer, Live Preview, Debounce | [03_markdown-editor.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/03_markdown-editor.md) |
| `PRJ-04` | Expense Tracker IndexedDB | Advanced (4/5) | Repository Pattern | IndexedDB Transactions, ObjectStores, Cursor | [04_expense-tracker-indexeddb.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/04_expense-tracker-indexeddb.md) |
| `PRJ-05` | Interactive Quiz Engine | Intermediate (2/5) | State Machine | Timers, Audio API, Local Persistence | [05_quiz-app.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/05_quiz-app.md) |
| `PRJ-06` | Kanban Drag & Drop Board | Intermediate (3/5) | HTML5 Drag & Drop API | DataTransfer, DOM Coordinates, Reordering | [06_kanban-drag-and-drop.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/06_kanban-drag-and-drop.md) |
| `PRJ-07` | Realtime WebSocket Chat | Advanced (4/5) | Client-Server WS Broker | WebSocket Framing, Heartbeats, Auto-Reconnect | [07_realtime-chat-websocket.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/07_realtime-chat-websocket.md) |
| `PRJ-08` | Tiny Event Emitter | Intermediate (3/5) | Publish-Subscribe | Memory Leak Guard, Once Listeners, Wildcards | [08_tiny-event-emitter.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/08_tiny-event-emitter.md) |
| `PRJ-09` | Promise/A+ From Scratch | Advanced (5/5) | Specification Compliance | Microtasks, State Invariants, Thenable Unwrapping | [09_promise-from-scratch.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/09_promise-from-scratch.md) |
| `PRJ-10` | Mini Virtual DOM | Advanced (4/5) | Tree Reconciliation | Hyperscript AST, Keyed Diffing, DOM Patches | [10_mini-virtual-dom.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/10_mini-virtual-dom.md) |
| `PRJ-11` | Mini State Manager | Intermediate (3/5) | Unidirectional Flow | Redux Pattern, Middleware Pipeline, Selectors | [11_mini-state-manager.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/11_mini-state-manager.md) |
| `PRJ-12` | Mini Client Router | Intermediate (3/5) | Declarative Routing | History API, Path-to-Regex, Route Guards | [12_mini-router.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/12_mini-router.md) |
| `PRJ-13` | Mini Utility Library | Intermediate (3/5) | Functional Utilities | Cycle-Safe Deep Clone, Debounce, Get/Set | [13_mini-lodash.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/13_mini-lodash.md) |
| `PRJ-14` | Dependency Injection Container | Advanced (4/5) | Inversion of Control | Singletons, Transients, Cycle Detection DAG | [14_dependency-injection-container.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/14_dependency-injection-container.md) |
| `PRJ-15` | CLI Task Manager | Intermediate (3/5) | CLI / Hexagonal | `util.parseArgs`, Atomic Filesystem, ANSI Colors | [15_cli-task-manager.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/15_cli-task-manager.md) |
| `PRJ-16` | Resilient Web Scraper | Advanced (4/5) | Worker Pool / Pipeline | Async Pool, Backoff Jitter, HTML Tokenizer | [16_web-scraper.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/16_web-scraper.md) |
| `PRJ-17` | In-Memory Task Scheduler | Advanced (4/5) | Priority Queue Dispatcher | Min-Heap, AbortSignal Timeouts, Recurring Jobs | [17_task-scheduler.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/17_task-scheduler.md) |
| `PRJ-18` | Mini JavaScript Bundler | Advanced (5/5) | Compiler Pipeline | Module AST, Dependency DAG, IIFE Packaging | [18_mini-bundler.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/18_mini-bundler.md) |
| `PRJ-19` | Offline PWA Notes App | Advanced (4/5) | Offline-First PWA | Service Worker Strategies, Cache API, Sync Queue | [19_pwa-notes-app.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/19_pwa-notes-app.md) |
| `PRJ-20` | Mini Test Framework | Advanced (4/5) | Testing BDD DSL | Assertions, Runner AST, Async Hooks, Reporters | [20_mini-test-framework.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/20_mini-test-framework.md) |
| `PRJ-21` | Mini Reactive Signals | Advanced (4/5) | Reactive Observer Graph | Signals, Effect Dependency Graph, Computed, Batch | [21_mini-reactive-signals.md](file:///e:/Learning/engineering-notes/01-languages/javascript/25_projects/21_mini-reactive-signals.md) |

---

## Test & Execution Harness

All projects are architected to execute natively using Node.js without compiling, bundlers, or third-party test runners:

```bash
# Run any individual project test suite
node --test tests/app.test.js

# Or test across all modules in headless mode
node --test --experimental-test-isolation=none tests/**/*.test.js
```
