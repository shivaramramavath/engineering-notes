# Event Loop Execution Order Cheatsheet

> Quick-reference guide outlining task execution precedence between the Call Stack, Microtasks, Render Stages, Macrotasks, and Node.js libuv loop phases.

| Metadata | Details |
| :--- | :--- |
| **Track** | Reference Cheatsheets (`27_cheatsheets`) |
| **Baseline** | ECMAScript 2024 / Node.js 22 LTS |
| **Level** | Reference / Quick Lookup |

---

## 1. Browser Event Loop Turn Precedence

```
 1. Call Stack (Synchronous Script Execution)
    │
    ▼
 2. Drain ALL Microtasks (Until Microtask Queue count == 0)
    │  ├─ Promise.then / catch / finally
    │  ├─ queueMicrotask(...)
    │  └─ MutationObserver callbacks
    │
    ▼
 3. Render Pipeline (If 60Hz/120Hz display refresh frame is due)
    │  ├─ requestAnimationFrame(...) callbacks
    │  ├─ Style Recalculation (CSSOM)
    │  ├─ Layout / Reflow (Geometry calculation)
    │  ├─ Paint (Rasterization)
    │  └─ Composite (GPU Layer assembly)
    │
    ▼
 4. Pick EXACTLY ONE Macrotask
    │  ├─ setTimeout / setInterval expired callbacks
    │  ├─ MessageChannel / postMessage
    │  └─ User I/O (Click, Pointer, Keyboard)
    │
    ▼ (Loop back to Step 2: Drain Microtasks after that one Macrotask)
```

---

## 2. Node.js Event Loop libuv Phases

| Phase Order | Name | Operations Executed |
| :--- | :--- | :--- |
| **Pre-Phase** | `process.nextTick` | Processed immediately after current sync execution before any libuv phase |
| **Microtasks**| `queueMicrotask` / Promises | Processed immediately after `nextTickQueue` empties |
| **Phase 1** | **Timers** | Expired callbacks scheduled via `setTimeout()` and `setInterval()` |
| **Phase 2** | **Pending Callbacks** | Deferred I/O callbacks (e.g. system socket errors `ECONNREFUSED`) |
| **Phase 3** | **Idle / Prepare** | Internal libuv housekeeping |
| **Phase 4** | **Poll** | Retrieves new I/O events (network connections, file I/O completions); blocks if empty |
| **Phase 5** | **Check** | Callbacks scheduled specifically via `setImmediate()` |
| **Phase 6** | **Close Callbacks** | Close notifications (e.g. `socket.on('close', ...)`) |

---

## 3. Asynchronous Timing Matrix Summary

| API Call | Environment | Queue Type | Execution Timing |
| :--- | :--- | :--- | :--- |
| `process.nextTick()` | Node.js only | `nextTickQueue` | Highest async priority; preempts promises |
| `queueMicrotask()` | Browser & Node.js | Microtask Queue | Drains before rendering and next macrotask |
| `Promise.resolve().then()` | Browser & Node.js | Microtask Queue | Drains alongside `queueMicrotask` |
| `requestAnimationFrame()` | Browser only | Animation Frame | Directly prior to next display refresh paint |
| `setImmediate()` | Node.js only | Check Phase | Runs right after the I/O poll phase |
| `setTimeout(fn, 0)` | Browser & Node.js | Macrotask / Timers | Minimum 1ms (or 4ms if nested $\ge 5$ in browser) |
| `requestIdleCallback()` | Browser only | Idle Period | Executes when main thread has spare time |
